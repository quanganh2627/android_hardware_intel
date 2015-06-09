/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#define LOG_TAG "Camera_PreviewThread"

#include "PreviewThread.h"
#include "LogHelper.h"
#include "DebugFrameRate.h"
#include "Callbacks.h"
#include "CallbacksThread.h"
#include "ColorConverter.h"
#include <gui/Surface.h>
#include "PerformanceTraces.h"
#include <ui/GraphicBuffer.h>
#include <ui/GraphicBufferMapper.h>
#include "AtomCommon.h"
#include "nv12rotation.h"
#include "PlatformData.h"
#include "MemoryUtils.h"

namespace android {

PreviewThread::PreviewThread(sp<CallbacksThread> callbacksThread, Callbacks* callbacks) :
    Thread(true) // callbacks may call into java
    ,mMessageQueue("PreviewThread", (int) MESSAGE_ID_MAX)
    ,mThreadRunning(false)
    ,mState(STATE_STOPPED)
    ,mLastFrameTs(0)
    ,mFramesDone(0)
    ,mCallbacksThread(callbacksThread)
    ,mPreviewWindow(NULL)
    ,mPreviewBuf(AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_PREVIEW))
    ,mCallbacks(callbacks)
    ,mBuffersInWindow(0)
    ,mNumOfPreviewBuffers(0)
    ,mFetchDone(false)
    ,mDebugFPS(new DebugFrameRate())
    ,mPreviewWidth(0)
    ,mPreviewHeight(0)
    ,mPreviewBpl(0)
    ,mPreviewFourcc(PlatformData::getPreviewPixelFormat())
    ,mPreviewCbFormat(V4L2_PIX_FMT_NV21)
    ,mGfxBpl(640)
    ,mOverlayEnabled(false)
    ,mRotation(0)
{
    LOG1("@%s", __FUNCTION__);
    mPreviewBuffers.setCapacity(MAX_NUMBER_PREVIEW_GFX_BUFFERS);
}

PreviewThread::~PreviewThread()
{
    LOG1("@%s", __FUNCTION__);
    mDebugFPS.clear();
    freeGfxPreviewBuffers();
    freeLocalPreviewBuf();
}

status_t PreviewThread::setCallback(ICallbackPreview *cb, ICallbackPreview::CallbackType t)
{
    LOG2("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_SET_CALLBACK;
    msg.data.setCallback.icallback = cb;
    msg.data.setCallback.type = t;
    return mMessageQueue.send(&msg);
}

status_t PreviewThread::handleMessageSetCallback(MessageSetCallback *msg)
{
    CallbackVector *cbVector =
        (msg->type == ICallbackPreview::INPUT
      || msg->type == ICallbackPreview::INPUT_ONCE)?
         &mInputBufferCb : &mOutputBufferCb;

    CallbackVector::iterator it = cbVector->begin();
    for (;it != cbVector->end(); ++it) {
        if (it->value == msg->icallback)
            return ALREADY_EXISTS;
        if (msg->type == ICallbackPreview::OUTPUT_WITH_DATA &&
            it->key == ICallbackPreview::OUTPUT_WITH_DATA) {
            return ALREADY_EXISTS;
        }
    }

    cbVector->push(callback_pair_t(msg->type, msg->icallback));
    return NO_ERROR;
}

void PreviewThread::inputBufferCallback()
{
    if (mInputBufferCb.empty())
        return;
    Vector<CallbackVector::iterator> toDrop;
    CallbackVector::iterator it = mInputBufferCb.begin();
    for (;it != mInputBufferCb.end(); ++it) {
        it->value->previewBufferCallback(NULL, it->key);
        if (it->key == ICallbackPreview::INPUT_ONCE)
            toDrop.push(it);
    }
    while (!toDrop.empty()) {
        mInputBufferCb.erase(toDrop.top());
        toDrop.pop();
    }
}

bool PreviewThread::outputBufferCallback(AtomBuffer *buff)
{
    bool ownership_passed = false;
    if (mOutputBufferCb.empty())
        return ownership_passed;
    Vector<CallbackVector::iterator> toDrop;
    CallbackVector::iterator it = mOutputBufferCb.begin();
    for (;it != mOutputBufferCb.end(); ++it) {
        if (it->key == ICallbackPreview::OUTPUT_WITH_DATA) {
            ownership_passed = true;
            it->value->previewBufferCallback(buff, it->key);
        } else {
            it->value->previewBufferCallback(buff, it->key);
        }
        if (it->key == ICallbackPreview::OUTPUT_ONCE)
            toDrop.push(it);
    }
    while (!toDrop.empty()) {
        mInputBufferCb.erase(toDrop.top());
        toDrop.pop();
    }
    return ownership_passed;
}

status_t PreviewThread::enableOverlay(bool set, int rotation)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    if ((mState != STATE_STOPPED) && (mState != STATE_NO_WINDOW)) {
        ALOGE("Cannot set overlay once Preview is configured");
        return INVALID_OPERATION;
    }
    mOverlayEnabled = set;
    mRotation = rotation;

    return status;
}

void PreviewThread::getDefaultParameters(CameraParameters *params)
{
    LOG2("@%s", __FUNCTION__);
    if (!params) {
        ALOGE("params is null!");
        return;
    }

    /**
     * PREVIEW
     */
    params->setPreviewFormat(cameraParametersFormat(mPreviewCbFormat));

    char previewFormats[100] = {0};
    if (snprintf(previewFormats, sizeof(previewFormats), "%s,%s",
                 CameraParameters::PIXEL_FORMAT_YUV420SP,
                 CameraParameters::PIXEL_FORMAT_YUV420P) < 0) {
        ALOGE("Could not generate %s string: %s", CameraParameters::KEY_SUPPORTED_PREVIEW_FORMATS, strerror(errno));
        return;
    }
    else {
        LOG1("supported preview cb formats %s\n", previewFormats);
    }
    params->set(CameraParameters::KEY_SUPPORTED_PREVIEW_FORMATS, previewFormats);
}

status_t PreviewThread::setPreviewWindow(struct preview_stream_ops *window)
{
    LOG1("@%s: preview_window = %p", __FUNCTION__, window);
    Message msg;
    msg.id = MESSAGE_ID_SET_PREVIEW_WINDOW;
    msg.data.setPreviewWindow.window = window;
    msg.data.setPreviewWindow.synchronous = false;
    if (window == NULL) {
        // When the window is set to NULL, we should release all Graphic buffer handles synchronously.
        msg.data.setPreviewWindow.synchronous = true;
        return mMessageQueue.send(&msg, MESSAGE_ID_SET_PREVIEW_WINDOW);
    } else {
        return mMessageQueue.send(&msg);
    }
}

/**
 * Configure PreviewThread
 *
 * \param preview_width     preview buffer width
 * \param preview_height    preview buffer height
 * \param preview_cb_format preview callback buffer format (fourcc)
 * \param shared_mode       allocate buffers for shared mode (0-copy)
 * \param buffer_count      amount of buffers to allocate
 */
status_t PreviewThread::setPreviewConfig(int preview_width, int preview_height,
                                         int preview_cb_format, bool shared_mode, int buffer_count)
{
    LOG1("@%s", __FUNCTION__);

    // for non-shared mode, PreviewThread's client doesn't need to set the buffer count
    if (shared_mode && buffer_count < 1)
        return BAD_VALUE;

    Message msg;
    msg.id = MESSAGE_ID_SET_PREVIEW_CONFIG;
    msg.data.setPreviewConfig.width = preview_width;
    msg.data.setPreviewConfig.height = preview_height;
    msg.data.setPreviewConfig.cb_format = preview_cb_format;
    msg.data.setPreviewConfig.bufferCount = buffer_count;
    msg.data.setPreviewConfig.sharedMode = shared_mode;
    setState(STATE_CONFIGURED);
    return mMessageQueue.send(&msg);
}

/**
 * Retrieve the GFx Preview buffers
 *
 * This is done sending a synchronous message to make sure
 * that the previewThread has processed all previous messages
 */
status_t PreviewThread::fetchPreviewBuffers(Vector<AtomBuffer> &pvBufs)
{
    LOG1("@%s", __FUNCTION__);

    Message msg;
    msg.id = MESSAGE_ID_FETCH_PREVIEW_BUFS;

    status_t status;
    status = mMessageQueue.send(&msg, MESSAGE_ID_FETCH_PREVIEW_BUFS);

    if (status != NO_ERROR) {
        ALOGE("Failed to fetch preview buffers (error: %d)", status);
        return status;
    }

    if(mSharedMode && mPreviewBuffers.size() > 0) {
        Vector<GfxAtomBuffer>::iterator it = mPreviewBuffers.begin();
        for (;it != mPreviewBuffers.end(); ++it) {
            it->owner = OWNER_CLIENT;
            pvBufs.push(it->buffer);
        }
    } else {
        status = INVALID_OPERATION;
    }
    LOG1("@%s: got [%d] buffers", __FUNCTION__, pvBufs.size());
    return status;
}

/**
 * Retrieve the Preview buffers geometry
 *
 * synchronous message used by ControlThread to retrieve any bytes-per-line
 * requirements imposed by the Gfx subsystem.
 * ControlThread first configures the window, this will get the Gfx buffer
 * requirements. Then it calls this method to pass it to the ISP
 */
status_t PreviewThread::fetchPreviewBufferGeometry(int *w, int *h, int *bpl)
{
    LOG1("@%s", __FUNCTION__);

    if ((w == NULL) || (h == NULL) || (bpl == NULL))
        return BAD_VALUE;

    Message msg;
    msg.id = MESSAGE_ID_FETCH_BUF_GEOMETRY;

    status_t status;
    status = mMessageQueue.send(&msg, MESSAGE_ID_FETCH_BUF_GEOMETRY);

    if (status == NO_ERROR) {
        *w = mPreviewWidth;
        *h = mPreviewHeight;
        *bpl = mPreviewBpl;
        LOG1("@%s: got WxH (bpl): %dx%d (%d)", __FUNCTION__,*w,*h,*bpl);
    } else {
        ALOGE("@%s: could not fetch buffer geometry ret=%d", __FUNCTION__,status);
    }

    return status;
}

/**
 * Returns the GFx Preview buffers to the window
 * There is no need for parameters since the PreviewThread
 * keeps track of the buffers already
 *
 */
status_t PreviewThread::returnPreviewBuffers()
{
    LOG1("@%s", __FUNCTION__);

    Message msg;
    msg.id = MESSAGE_ID_RETURN_PREVIEW_BUFS;
    return mMessageQueue.send(&msg, MESSAGE_ID_RETURN_PREVIEW_BUFS);
}

status_t PreviewThread::preview(AtomBuffer *buff)
{
    LOG2("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_PREVIEW;
    msg.data.preview.buff = *buff;
    return mMessageQueue.send(&msg);
}

/**
 * override for IAtomIspObserver::atomIspNotify()
 *
 * PreviewThread gets attached to receive preview stream here.
 *
 *
 */
bool PreviewThread::atomIspNotify(IAtomIspObserver::Message *msg, const ObserverState state)
{
    LOG2("@%s", __FUNCTION__);
    if (!msg) {
        LOG1("Received observer state change");
        // We are currently not receiving MESSAGE_ID_END_OF_STREAM when stream
        // stops. Observer gets paused when device is about to be stopped and
        // after pausing, we no longer receive new frames for the same session.
        // Reset frame counter based on any observer state change
        mFramesDone = 0;
        return false;
    }

    AtomBuffer *buff = &msg->data.frameBuffer.buff;
    if (msg->id == MESSAGE_ID_FRAME) {
        if (buff->status == FRAME_STATUS_SKIPPED) {
            buff->owner->returnBuffer(buff);
        } else if(buff->status == FRAME_STATUS_CORRUPTED) {
            buff->owner->returnBuffer(buff);
        } else {
            PerformanceTraces::FaceLock::getCurFrameNum(buff->frameCounter);
            preview(buff);
        }
    } else {
        LOG1("Received unexpected notify message id %d!", msg->id);
    }

    return false;
}

status_t PreviewThread::postview(AtomBuffer *buff, bool hidePreview, bool synchronous)
{
    LOG2("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_POSTVIEW;
    if (buff)
        msg.data.preview.buff = *buff;
    else
        msg.data.preview.buff.status = FRAME_STATUS_SKIPPED;
    msg.data.preview.hide = hidePreview;
    msg.data.preview.synchronous = synchronous;
    if (!synchronous)
        return mMessageQueue.send(&msg);
    else
        return mMessageQueue.send(&msg, MESSAGE_ID_POSTVIEW);
}

status_t PreviewThread::flushBuffers()
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_FLUSH;
    mMessageQueue.remove(MESSAGE_ID_PREVIEW);
    mMessageQueue.remove(MESSAGE_ID_POSTVIEW);
    return mMessageQueue.send(&msg, MESSAGE_ID_FLUSH);
}

status_t PreviewThread::handleMessageExit()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    mThreadRunning = false;
    return status;
}

/**
 * Public preview state checker
 *
 * State transitions do not synchronize with stream.
 * State distinctly serves the client to deside what
 * to do with preview frame buffers.
 */
PreviewThread::PreviewState PreviewThread::getPreviewState() const
{
    Mutex::Autolock lock(&mStateMutex);
    return mState;
}

/**
 * Public state setter for allowed transitions
 *
 * Note: only internally handled transition is initially
 *       STATE_CONFIGURED - which requires the client to
 *       call setPreviewConfig()
 *
 * Allowed transitions:
 * _STOPPED -> _NO_WINDOW:
 *   - Preview is started without window handle
 * _NO_WINDOW -> _STOPPED:
 * _ENABLED -> _STOPPED:
 * _ENABLED_HIDDEN -> _STOPPED:
 *  - Preview is stopped with one of the supported transition
 * _CONFIGURED -> _ENABLED:
 *  - preview gets enabled normally through supported transition
 * _ENABLED_HIDDEN -> _ENABLED:
 *  - preview gets restored visible (currently no-op internally)
 * _ENABLED -> _HIDDEN:
 *  - preview stream active, but do not send buffers to display
 * _ENABLED -> _NO_WINDOW:
 *  - preview stream active, but without window handle
 */
status_t PreviewThread::setPreviewState(PreviewState state)
{
    LOG1("@%s: state request %d", __FUNCTION__, state);
    status_t status = INVALID_OPERATION;
    Mutex::Autolock lock(&mStateMutex);

    if (mState == state)
        return NO_ERROR;

    switch (state) {
        case STATE_NO_WINDOW:
            if (mState == STATE_STOPPED
             || mState == STATE_ENABLED)
                status = NO_ERROR;
            break;
        case STATE_STOPPED:
            if (mState == STATE_NO_WINDOW
             || mState == STATE_ENABLED
             || mState == STATE_ENABLED_HIDDEN)
                status = NO_ERROR;
            break;
        case STATE_ENABLED:
            if (mState == STATE_CONFIGURED
             || mState == STATE_ENABLED_HIDDEN)
                status = NO_ERROR;
            break;
        case STATE_ENABLED_HIDDEN:
            if (mState == STATE_ENABLED)
                status = NO_ERROR;
            break;
        case STATE_CONFIGURED:
        default:
            break;
    }

    if (status != NO_ERROR) {
        LOG1("Invalid preview state transition request %d => %d", mState, state);
    } else {
        mState = state;
    }

    return status;
}

/**
 * Protected state setter for internal transitions
 */
status_t PreviewThread::setState(PreviewState state)
{
    LOG1("@%s: state %d => %d", __FUNCTION__, mState, state);
    Mutex::Autolock lock(&mStateMutex);
    mState = state;
    return NO_ERROR;
}

/**
 * Synchronous query to check if a valid native window
 * has been received.
 *
 * First we send a synchronous message (handler does nothing)
 * when it is processed we are sure that all previous commands have
 * been processed so we can check the mPreviewWindow variable.
 **/
bool PreviewThread::isWindowConfigured()
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_WINDOW_QUERY;
    mMessageQueue.send(&msg, MESSAGE_ID_WINDOW_QUERY);
    return (mPreviewWindow != NULL);
}

status_t PreviewThread::handleMessageIsWindowConfigured()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    mMessageQueue.reply(MESSAGE_ID_WINDOW_QUERY, status);
    return status;
}

/**
 * helper function to update per-frame locally tracked timestamps and counters
 */
void PreviewThread::frameDone(AtomBuffer &buff)
{
    LOG2("@%s", __FUNCTION__);
    mLastFrameTs = nsecs_t(buff.capture_timestamp.tv_sec) * 1000000LL
                 + nsecs_t(buff.capture_timestamp.tv_usec);
    mFramesDone++;
}

status_t PreviewThread::waitForAndExecuteMessage()
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    Message msg;
    mMessageQueue.receive(&msg);

    switch (msg.id) {

        case MESSAGE_ID_EXIT:
            status = handleMessageExit();
            break;

        case MESSAGE_ID_PREVIEW:
            status = handlePreview(&msg.data.preview);
            frameDone(msg.data.preview.buff);
            break;

        case MESSAGE_ID_POSTVIEW:
            status = handlePostview(&msg.data.preview);
            if (msg.data.preview.hide)
                setPreviewState(STATE_ENABLED_HIDDEN);
            mCallbacksThread->postviewRendered();
            break;

        case MESSAGE_ID_SET_PREVIEW_WINDOW:
            status = handleSetPreviewWindow(&msg.data.setPreviewWindow);
            break;

        case MESSAGE_ID_WINDOW_QUERY:
            status = handleMessageIsWindowConfigured();
            break;

        case MESSAGE_ID_SET_PREVIEW_CONFIG:
            status = handleSetPreviewConfig(&msg.data.setPreviewConfig);
            break;

        case MESSAGE_ID_FLUSH:
            status = handleMessageFlush();
            break;

        case MESSAGE_ID_FETCH_PREVIEW_BUFS:
            status = handleFetchPreviewBuffers();
            break;

        case MESSAGE_ID_RETURN_PREVIEW_BUFS:
            status = handleReturnPreviewBuffers();
            break;

        case MESSAGE_ID_SET_CALLBACK:
            status = handleMessageSetCallback(&msg.data.setCallback);
            break;

        case MESSAGE_ID_FETCH_BUF_GEOMETRY:
            status = handleMessageFetchBufferGeometry();
            break;

        default:
            ALOGE("Invalid message");
            status = BAD_VALUE;
            break;
    };
    return status;
}

bool PreviewThread::threadLoop()
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    // start gathering frame rate stats
    mDebugFPS->run();

    mThreadRunning = true;
    while (mThreadRunning)
        status = waitForAndExecuteMessage();

    // stop gathering frame rate stats
    mDebugFPS->requestExitAndWait();

    return false;
}

status_t PreviewThread::requestExitAndWait()
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_EXIT;
    // tell thread to exit
    // send message asynchronously
    mMessageQueue.send(&msg);

    // propagate call to base class
    return Thread::requestExitAndWait();
}

status_t PreviewThread::handleMessageFlush()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    mMessageQueue.reply(MESSAGE_ID_FLUSH, status);
    return status;
}

void PreviewThread::freeLocalPreviewBuf(void)
{
    LOG1("releasing existing preview buffer\n");
    MemoryUtils::freeAtomBuffer(mPreviewBuf);
}

void PreviewThread::allocateLocalPreviewBuf(void)
{
    size_t size(0);
    int bpl(0);
    size_t ySize(0);
    int cBpl(0);
    size_t cSize(0);

    LOG1("allocating the preview buffer\n");
    freeLocalPreviewBuf();

    switch(mPreviewCbFormat) {
    case V4L2_PIX_FMT_YVU420:
        bpl = ALIGN16(mPreviewWidth);
        ySize = bpl * mPreviewHeight;
        cBpl = ALIGN16(bpl/2);
        cSize = cBpl * mPreviewHeight/2;
        size = ySize + cSize * 2;
        break;

    case V4L2_PIX_FMT_NV21:
        size = mPreviewWidth*mPreviewHeight*3/2;
        break;

    case V4L2_PIX_FMT_RGB565:
        size = mPreviewWidth*mPreviewHeight*2;
        break;

    default:
        ALOGE("invalid preview format: %d", mPreviewCbFormat);
        break;
    }

    if (size > 0) {
        mCallbacks->allocateMemory(&mPreviewBuf, size);
        if(!mPreviewBuf.buff) {
            ALOGE("getting memory failed\n");
        }
    }
}

status_t PreviewThread::handleMessageFetchBufferGeometry()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    if (mState < STATE_CONFIGURED) {
        ALOGE("Trying to fetch buffer geometry when PreviewThread is not initialized");
        status = INVALID_OPERATION;
    }

    mMessageQueue.reply(MESSAGE_ID_FETCH_BUF_GEOMETRY, status);
    return status;
}

/**
 * stream-time dequeuing of buffers from preview_window_ops
 *
 */
PreviewThread::GfxAtomBuffer* PreviewThread::dequeueFromWindow()
{
    GfxAtomBuffer *ret = NULL;
    int dq_retries = GFX_DEQUEUE_RETRY_COUNT;
    MapperPointer mapperPointer;
    mapperPointer.ptr = NULL;
    int w,h;
    int lockMode;

    /**
     * Note: selected lock mode relies that if buffers are shared or not
     *
     * if they are shared with ControlThread then the ISP writes into the buffers
     * and Previewthread and PostprocThread read from it.
     *
     * if they are not shared then CPU is copying into the buffers in the PreviewThread
     * and nobody is reading, since these buffers are not passed to PostProcThread
     */
    if (mSharedMode)
        lockMode = GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_NEVER;
    else
        lockMode = GRALLOC_USAGE_SW_READ_NEVER | GRALLOC_USAGE_SW_WRITE_OFTEN;

    GraphicBufferMapper &mapper = GraphicBufferMapper::get();

    for (;dq_retries > 0 && ret == NULL; dq_retries--) {
        // mMinUndequeued is a constraint set by native window and
        // it controls when we can dequeue a frame and call previewDone.
        // Typically at least two frames must be kept in native window
        // when streaming.
        if (mBuffersInWindow > mMinUndequeued) {
            int err(-1), bpl(0), pixel_stride(0);
            buffer_handle_t *buf(NULL);
            err = mPreviewWindow->dequeue_buffer(mPreviewWindow, &buf, &pixel_stride);
            if (err != 0 || buf == NULL) {
                ALOGW("Error dequeuing preview buffer");
            } else {
                bpl = pixelsToBytes(mPreviewFourcc, pixel_stride);
                // If Gfx frame buffer stride is greater than ISP buffer stride, we can
                // repad the shared buffer between Gfx and ISP to meet Gfx alignmnet
                // requirement, so we should change the lockMode as CPU writable.
                if (bpl > mPreviewBpl)
                    lockMode |= GRALLOC_USAGE_SW_WRITE_OFTEN;
                ret = lookForGfxBufferHandle(buf);
                if (ret == NULL) {
                    if (mFetchDone) {
                        ALOGW("unknown gfx buffer dequeued, handle=%p", buf);
                        mPreviewWindow->cancel_buffer(mPreviewWindow, buf);
                    } else {
                        // stream-time fetching until target buffer count
                        AtomBuffer tmpBuf;

                        getEffectiveDimensions(&w,&h);
                        const Rect bounds(w, h);
                        tmpBuf.buff = NULL;     // We do not allocate a normal camera_memory_t
                        tmpBuf.id = mPreviewBuffers.size();
                        tmpBuf.type = ATOM_BUFFER_PREVIEW_GFX;
                        tmpBuf.bpl = bpl;
                        tmpBuf.width = w;
                        tmpBuf.height = h;
                        tmpBuf.size = frameSize(mPreviewFourcc, pixel_stride, h);
                        tmpBuf.fourcc = mPreviewFourcc;
                        tmpBuf.status = FRAME_STATUS_NA;
                        if(mapper.lock(*buf, lockMode, bounds, &mapperPointer.ptr) != NO_ERROR) {
                            ALOGE("Failed to lock GraphicBufferMapper!");
                            mPreviewWindow->cancel_buffer(mPreviewWindow, buf);
                        } else {
                            tmpBuf.dataPtr = mapperPointer.ptr;
                            GfxAtomBuffer newBuf;
                            newBuf.buffer = tmpBuf;
                            newBuf.owner = OWNER_PREVIEWTHREAD;
                            newBuf.buffer.gfxInfo.gfxBufferHandle = buf;
                            mPreviewBuffers.push(newBuf);
                            mBuffersInWindow--;
                            ret = &(mPreviewBuffers.editItemAt(tmpBuf.id));
                            if (mPreviewBuffers.size() == mNumOfPreviewBuffers)
                                mFetchDone = true;
                        }
                    }
                } else {
                    mBuffersInWindow--;
                    getEffectiveDimensions(&w,&h);
                    const Rect bounds(w, h);
                    if (mapper.lock(*buf, lockMode, bounds, &mapperPointer.ptr) != NO_ERROR) {
                        ALOGE("Failed to lock GraphicBufferMapper!");
                        mPreviewWindow->cancel_buffer(mPreviewWindow, buf);
                        ret = NULL;
                    } else {
                        ret->owner = OWNER_PREVIEWTHREAD;
                        if (ret->buffer.id >= MAX_NUMBER_PREVIEW_GFX_BUFFERS) {
                            LOG2("Received one of reserved buffers from Gfx, dequeueing another one");
                            ret = NULL;
                        }
                    }
                }
            }
        }
        else {
            LOG2("@%s: %d buffers in window, not enough, need %d",
                 __FUNCTION__, mBuffersInWindow, mMinUndequeued);
            break;
        }
    }

    return ret;
}

/**
 * Dequeue Gfx buffers for mReservedBuffers
 */
status_t
PreviewThread::fetchReservedBuffers(int reservedBufferCount)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    if (reservedBufferCount > 0 && mReservedBuffers.isEmpty()) {
        GraphicBufferMapper &mapper = GraphicBufferMapper::get();
        AtomBuffer tmpBuf;
        int err(-1), pixel_stride(0);
        buffer_handle_t *buf(NULL);
        MapperPointer mapperPointer;
        mapperPointer.ptr = NULL;
        int lockMode = GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_SW_READ_NEVER;
        const Rect bounds(mPreviewWidth, mPreviewHeight);
        for (int i = 0; i < reservedBufferCount; i++) {
            err = mPreviewWindow->dequeue_buffer(mPreviewWindow, &buf, &pixel_stride);
            if(err != 0 || buf == NULL) {
                ALOGE("Surface::dequeueBuffer returned error %d (buf=%p)", err, buf);
                status = UNKNOWN_ERROR;
                goto freeDeQueued;
            }
            tmpBuf.buff = NULL;     // We do not allocate a normal camera_memory_t
            // NOTE: distinquish reserved from normal stream buffers by id
            tmpBuf.id = i + MAX_NUMBER_PREVIEW_GFX_BUFFERS;
            tmpBuf.type = ATOM_BUFFER_PREVIEW_GFX;
            tmpBuf.bpl = pixelsToBytes(mPreviewFourcc, pixel_stride);
            tmpBuf.width = mPreviewWidth;
            tmpBuf.height = mPreviewHeight;
            tmpBuf.status = FRAME_STATUS_NA;
            tmpBuf.size = frameSize(mPreviewFourcc, pixel_stride, mPreviewHeight);
            tmpBuf.fourcc = mPreviewFourcc;

            status = mapper.lock(*buf, lockMode, bounds, &mapperPointer.ptr);
            if(status != NO_ERROR) {
                ALOGE("Failed to lock GraphicBufferMapper!");
                goto freeDeQueued;
            }

            tmpBuf.dataPtr = mapperPointer.ptr;
            GfxAtomBuffer newBuf;
            newBuf.buffer = tmpBuf;
            newBuf.owner = OWNER_PREVIEWTHREAD;
            newBuf.buffer.gfxInfo.gfxBufferHandle = buf;
            mReservedBuffers.push(newBuf);
            LOG1("%s: got Gfx Buffer (reserved): native_ptr %p, size:(%dx%d), bpl: %d ", __FUNCTION__,
                 buf, mPreviewWidth, mPreviewHeight, tmpBuf.bpl);
        } // for
    }
    return status;
freeDeQueued:
    freeGfxPreviewBuffers();
    return status;
}

PreviewThread::GfxAtomBuffer*
PreviewThread::lookForGfxBufferHandle(buffer_handle_t *handle)
{
    Vector<GfxAtomBuffer>::iterator it = mPreviewBuffers.begin();

    for (;it != mPreviewBuffers.end(); ++it) {
        if (it->buffer.gfxInfo.gfxBufferHandle == handle) {
            return it;
        }
    }

    it = mReservedBuffers.begin();
    for (;it != mReservedBuffers.end(); ++it) {
        if (it->buffer.gfxInfo.gfxBufferHandle == handle) {
            return it;
        }
    }

    if (mFetchDone)
        ALOGE("%s: Unknown buffer handle(%p)!", __FUNCTION__, handle);

    return NULL;
}

PreviewThread::GfxAtomBuffer*
PreviewThread::lookForAtomBuffer(AtomBuffer *buffer)
{
    Vector<GfxAtomBuffer>::iterator it = mPreviewBuffers.begin();

    for (;it != mPreviewBuffers.end(); ++it) {
        if (it->buffer.dataPtr == buffer->dataPtr) {
            return it;
        }
    }

    it = mReservedBuffers.begin();
    for (;it != mReservedBuffers.end(); ++it) {
        if (it->buffer.dataPtr == buffer->dataPtr) {
            return it;
        }
    }

    ALOGE("%s: Unknown buffer(%p)!", __FUNCTION__, buffer);

    return NULL;
}

/**
 *  This is a quick fix due to BayTrial Gfx is 128 bytes aligned while ISP may be 64/48/32/16 bytes
 *  aligned, in case of the Gfx buffer queued into ISP is not 128 bytes aligned, when the buffer is
 *  queued back to Gfx, we have to add padding to make this buffer 128 bytes aligned to meet the Gfx
 *  weird requirement.
 *  When doing the Gfx buffer padding We should take lockMode into account or we may have cache line
 *  issues as we have had, more details please refer to:
 *  http://android.intel.com:8080/#/c/102603/
 */
void PreviewThread::padPreviewBuffer(GfxAtomBuffer* &gfxBuf, MessagePreview* &msg)
{
    if (mPreviewBpl != 0 && msg->buff.bpl < mGfxBpl && msg->buff.type == ATOM_BUFFER_PREVIEW_GFX) {
        LOG2("@%s bpl-gfx=%d, bpl-msg=%d", __FUNCTION__, mGfxBpl, msg->buff.bpl);
        repadYUV420(gfxBuf->buffer.width, gfxBuf->buffer.height, gfxBuf->buffer.bpl,
                    mGfxBpl, gfxBuf->buffer.dataPtr, gfxBuf->buffer.dataPtr);
        msg->buff.bpl = mGfxBpl;
    }
}

/**
 *  This method gets executed for each preview frames that the Thread
 *  receives.
 *  The message is sent by the observer thread that polls the preview
 *  stream
 */
status_t PreviewThread::handlePreview(MessagePreview *msg)
{
    status_t status = NO_ERROR;
    bool passedToGfx = false;
    GraphicBufferMapper &mapper = GraphicBufferMapper::get();
    LOG2("@%s: id = %d, width = %d, height = %d, fourcc = %s, bpl = %d", __FUNCTION__,
            msg->buff.id, msg->buff.width, msg->buff.height, v4l2Fmt2Str(msg->buff.fourcc), msg->buff.bpl);

    PreviewState state = getPreviewState();
    if (state != STATE_ENABLED) {
        LOG2("Received preview frame in state %d, skipping", state);
        goto skip_displaying;
    }

    if (mPreviewWindow != 0) {
        int err;
        GfxAtomBuffer *bufToEnqueue = NULL;
        if (msg->buff.type != ATOM_BUFFER_PREVIEW_GFX) {
            // client not passing our buffers, not in 0-copy path
            // do basic checks that configuration matches for a frame copy
            // Note: ignoring format, as we seem to use fixed NV12
            // while PreviewThread is configured according to public
            // parameter for callback conversions
            if (msg->buff.width != mPreviewWidth ||
                msg->buff.height != mPreviewHeight ||
                (msg->buff.bpl != mPreviewBpl && mPreviewBpl != 0)) {
                LOG1("%s: not passing buffer to window, conflicting format", __FUNCTION__);
                LOG1(", input : %dx%d(%d:%x:%s)",
                     msg->buff.width, msg->buff.height, msg->buff.bpl, msg->buff.fourcc,
                    v4l2Fmt2Str(msg->buff.fourcc));
                LOG1(", preview : %dx%d(%d:%x:%s)",
                     mPreviewWidth, mPreviewHeight,
                     mPreviewBpl, mPreviewFourcc, v4l2Fmt2Str(mPreviewFourcc));
            } else {
                bufToEnqueue = dequeueFromWindow();
                if (bufToEnqueue) {
                    LOG2("copying frame %p -> %p : size %d", msg->buff.dataPtr, bufToEnqueue->buffer.dataPtr, msg->buff.size);
                    LOG2("src frame  %dx%d bpl %d ", msg->buff.width, msg->buff.height,msg->buff.bpl);
                    LOG2("dst frame  %dx%d bpl %d ", bufToEnqueue->buffer.width, bufToEnqueue->buffer.height, bufToEnqueue->buffer.bpl);
                    // If mPreviewBpl is not equal with 0, it had set to Gfx bpl when window configuration.
                    // If Gfx current bpl is not equal with window configuration setting, this frame should be dropped.
                    if ((mPreviewBpl != 0) && (bufToEnqueue->buffer.bpl != mPreviewBpl)) {
                        ALOGE("msg->buff.bpl=%d, bufToEnqueue->buffer.bpl=%d, bpl don't match for a frame copy",msg->buff.bpl,bufToEnqueue->buffer.bpl);
                        mapper.unlock(*(bufToEnqueue->buffer.gfxInfo.gfxBufferHandle));
                        mPreviewWindow->cancel_buffer(mPreviewWindow, bufToEnqueue->buffer.gfxInfo.gfxBufferHandle);
                        bufToEnqueue->owner = OWNER_WINDOW;
                        bufToEnqueue = NULL;
                    } else {
                        copyPreviewBuffer(&msg->buff, &bufToEnqueue->buffer);
                    }
                } else {
                    ALOGE("failed to dequeue from window");
                }
            }
        } else {
            bufToEnqueue = lookForAtomBuffer(&msg->buff);
            if (bufToEnqueue) {
                passedToGfx = true;
            }
        }

        if (bufToEnqueue != NULL) {
            // TODO: If ISP can be configured to match Gfx buffer stride alignment, please delete below line.
            padPreviewBuffer(bufToEnqueue, msg);
            bufToEnqueue->buffer.gfxInfo.scalerId = msg->buff.gfxInfo.scalerId;
            bufToEnqueue->buffer.shared = msg->buff.shared;
            bufToEnqueue->buffer.capture_timestamp = msg->buff.capture_timestamp;
            bufToEnqueue->buffer.frameCounter = msg->buff.frameCounter;
            mapper.unlock(*(bufToEnqueue->buffer.gfxInfo.gfxBufferHandle));
            if ((err = mPreviewWindow->enqueue_buffer(mPreviewWindow,
                            bufToEnqueue->buffer.gfxInfo.gfxBufferHandle)) != 0) {
                ALOGE("Surface::queueBuffer returned error %d", err);
                passedToGfx = false;
            } else {
                bufToEnqueue->owner = OWNER_WINDOW;
                mBuffersInWindow++;
                // preview frame shown, update perf traces
                PERFORMANCE_TRACES_PREVIEW_SHOWN(msg->buff.frameCounter);
            }
        }
    }

    if(!mPreviewBuf.buff) {
        allocateLocalPreviewBuf();
    }

    if (mCallbacks->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME) && mPreviewBuf.dataPtr) {
        void *src = msg->buff.dataPtr;
        switch(mPreviewCbFormat) {
                                  // Android definition: PIXEL_FORMAT_YUV420P-->YV12, please refer to
        case V4L2_PIX_FMT_YVU420: // header file: frameworks/av/include/camera/CameraParameters.h
            convertBuftoYV12(mPreviewFourcc, mPreviewWidth,
                             mPreviewHeight, msg->buff.bpl,
                             mPreviewWidth, src, mPreviewBuf.dataPtr);
            break;
        case V4L2_PIX_FMT_NV21: // you need to do this for the first time
            convertBuftoNV21(mPreviewFourcc, mPreviewWidth,
                             mPreviewHeight, msg->buff.bpl,
                             mPreviewWidth, src, mPreviewBuf.dataPtr);
            break;
        case V4L2_PIX_FMT_RGB565:
            if (mPreviewFourcc == V4L2_PIX_FMT_NV12)
                trimConvertNV12ToRGB565(mPreviewWidth, mPreviewHeight, msg->buff.bpl, src, mPreviewBuf.dataPtr);
            //TBD for other preview format, not supported yet
            break;
        default:
            ALOGE("invalid preview callback format: %d", mPreviewCbFormat);
            status = -1;
            break;
        }
        if (status == NO_ERROR)
            mCallbacksThread->previewFrameDone(&mPreviewBuf);
    }

skip_displaying:
    inputBufferCallback();

    mDebugFPS->update(); // update fps counter

    if (!passedToGfx) {
        // passing the input buffer as output
        if (!outputBufferCallback(&(msg->buff)))
            msg->buff.owner->returnBuffer(&msg->buff);
    } else {
        // input buffer was passed to Gfx queue, now try
        // dequeueing to replace output callback buffer
        GfxAtomBuffer *outputBuffer = dequeueFromWindow();
        if (outputBuffer) {
            outputBuffer->owner = OWNER_CLIENT;
            // restore the owner and state of input AtomBuffer to output
            outputBuffer->buffer.owner = msg->buff.owner;
            outputBuffer->buffer.status = msg->buff.status;
            if (!outputBufferCallback(&outputBuffer->buffer))
                msg->buff.owner->returnBuffer(&outputBuffer->buffer);
        }
    }

    return status;
}

status_t PreviewThread::handleSetPreviewWindow(MessageSetPreviewWindow *msg)
{
    LOG1("@%s: preview_window = %p", __FUNCTION__, msg->window);
    status_t status = NO_ERROR;

    if (mPreviewWindow != msg->window) {
        LOG1("Received the different window handle, update window setting.");

        if (mPreviewWindow != NULL) {
            freeGfxPreviewBuffers();
        }

        mPreviewWindow = msg->window;

        if (mPreviewWindow != NULL) {
            int w = mPreviewWidth;
            int h = mPreviewHeight;
            int usage;

            getEffectiveDimensions(&w,&h);

            if (mOverlayEnabled) {
                // write-often: overlay copy into the buffer
                // read-never: we do not use this buffer for callbacks. We never read from it
                usage = GRALLOC_USAGE_SW_WRITE_OFTEN |
                        GRALLOC_USAGE_SW_READ_NEVER  |
                        GRALLOC_USAGE_HW_COMPOSER    |
                        GRALLOC_USAGE_HW_RENDER      |
                        GRALLOC_USAGE_HW_TEXTURE;

            } else {
                // write-never: main use-case, stream image data to window by ISP only
                // read-rarely: 2nd use-case, memcpy to application data callback
                usage = GRALLOC_USAGE_SW_READ_RARELY |
                        GRALLOC_USAGE_SW_WRITE_NEVER |
                        GRALLOC_USAGE_HW_COMPOSER    |
                        GRALLOC_USAGE_HW_RENDER      |
                        GRALLOC_USAGE_HW_TEXTURE;
            }

            LOG1("Setting new preview window %p (%dx%d)", mPreviewWindow,w,h);
            mPreviewWindow->set_usage(mPreviewWindow, usage);
            /**
             * In case we do the rotation in CPU (like in overlay case) the width and
             * height do not need to be restricted by ISP bpl. PreviewThread does not
             * request any bpl and therefore sets its mPreviewBpl to zero. The rotation
             * routine will take care of arbitrary bytes per line sizes of input
             * frames.
             */
            if (mRotation == 90 || mRotation == 270) {
                mPreviewWindow->set_buffers_geometry(mPreviewWindow, w, h, getGFXHALPixelFormatFromV4L2Format(mPreviewFourcc));
            } else {
                /**
                 * For 0-copy path we need to configure the window with the stride required
                 * by ISP, and then set the crop rectangle accordingly
                 */
                mPreviewWindow->set_buffers_geometry(mPreviewWindow, bytesToPixels(mPreviewFourcc, mPreviewBpl), h, getGFXHALPixelFormatFromV4L2Format(mPreviewFourcc));
                mPreviewWindow->set_crop(mPreviewWindow, 0, 0, w, h);
            }
        }
    }

    if (msg->synchronous)
        mMessageQueue.reply(MESSAGE_ID_SET_PREVIEW_WINDOW, status);

    return status;
}

status_t PreviewThread::handleSetPreviewConfig(MessageSetPreviewConfig *msg)
{
    LOG1("@%s: width = %d, height = %d, callback format = %s", __FUNCTION__,
         msg->width, msg->height, v4l2Fmt2Str(msg->cb_format));
    status_t status = NO_ERROR;
    int w = msg->width;
    int h = msg->height;
    // Note: ANativeWindowBuffer defines stride with pixels
    int pixel_stride =  w;
    int bufferCount = 0;
    int reservedBufferCount = 0;

    mSharedMode = msg->sharedMode;

    if ((w != 0 && h != 0)) {
        LOG1("Setting new preview size: %dx%d", w, h);
        if (mPreviewWindow != NULL) {

            /**
             *  if preview size changed, update the preview window
             *  but account for the rotation when setting the geometry
             */
            if (mRotation == 90 || mRotation == 270) {
                // we swap width and height
                w = msg->height;
                h = msg->width;
                pixel_stride = msg->height;
            }

            /**
             * Overlay buffer width needs to be aligned to 64 in saltbay. Suppose gralloc
             * should cover this work, no need to consider it in camera HAL. But it doesn't work
             * because now preview is also using graphic buffer while ISP wrongly takes it as 32
             * byte aligned (css2.0)
             * So, it's a workaround here to make only overlay graphic buffer align to 64.
             * TODO: remove it if ISP is able use the bpl passed from HAL.
             */
            if (mOverlayEnabled && (strcmp(PlatformData::getBoardName(), "saltbay") == 0)) {
                pixel_stride = ALIGN64(pixel_stride);
                LOG1("Buffer width is aligned to %d for saltbay overlay", pixel_stride);
            }
            mPreviewWindow->set_buffers_geometry(mPreviewWindow, pixel_stride, h, getGFXHALPixelFormatFromV4L2Format(mPreviewFourcc));
            mPreviewWindow->set_crop(mPreviewWindow, 0, 0, w, h);
        }

        /**
         * we keep in our internal fields the resolution provided by CtrlThread
         * in order to get the effective resolution taking into account the
         * rotation use \sa getEffectiveDimensions
         */
        mPreviewWidth = msg->width;
        mPreviewHeight = msg->height;
        // If the preview needs to be rotated, let AtomISP decide the bpl
        // since nv12rotateBy90() supports arbitrary line-badding.
        // Otherwise use the bpl of buffers dequeued from the preview window.
        if (mRotation == 90 || mRotation == 270 || mPreviewWindow == NULL)
            mPreviewBpl = 0;
        else
            mPreviewBpl = getGfxBufferBytesPerLine();
    }

    mPreviewCbFormat = msg->cb_format;

    LOG1("%s: preview callback format %s", __FUNCTION__, v4l2Fmt2Str(mPreviewCbFormat));

    // allocate local buffer used with preview callbacks
    allocateLocalPreviewBuf();

    if (mPreviewWindow == NULL) {
        LOG1("No window, not trying to allocate graphic buffers");
        return NO_ERROR;
    }

    // Decide the count of Gfx buffers to allocate
    mPreviewWindow->get_min_undequeued_buffer_count(mPreviewWindow, &mMinUndequeued);
    LOG1("Surface::get_min_undequeued_buffer_count buffers %d", mMinUndequeued);
    if (mOverlayEnabled) {
        // overlay needs buffer transformations, force shared mode off and
        // reconfigure buffer count
        bufferCount = GFX_BUFFERS_DURING_OVERLAY_USE;
        mSharedMode = false;
    } else if (mSharedMode) {
        if (msg->bufferCount > mMinUndequeued)
            bufferCount = msg->bufferCount;
        else
            bufferCount = GFX_BUFFERS_DURING_SHARED_TEXTURE_USE;
    } else {
        // shared mode disabled, allocate minimum amount of buffers to
        // circulate
        bufferCount = mMinUndequeued + 1;
    }

    if ( mMinUndequeued < 0 || mMinUndequeued > bufferCount - 1) {
        ALOGE("unexpected min undeueued requirement %d", mMinUndequeued);
        return INVALID_OPERATION;
    }

    // Allocate gfx buffers
    // In shared mode, add reserved buffers count to the total amount
    // allocated but fetch them into separate vector.
    reservedBufferCount = (mSharedMode) ? GFX_BUFFERS_FOR_RESERVED_USE : 0;
    status = allocateGfxPreviewBuffers(bufferCount + reservedBufferCount);

    if (status == NO_ERROR) {
        mNumOfPreviewBuffers = bufferCount;
        status = fetchReservedBuffers(reservedBufferCount);
    }

    return status;
}

/**
 * handle fetchPreviewBuffers()
 *
 * By fetching all our external buffers at once, we provide an
 * array of loose pointers to buffers acquired from NativeWindow
 * ops. Pre-fetching is typical operation when ISP is fed with
 * graphic-buffers to attain 0-copy preview loop.
 *
 * If buffers are not fetched in the beginning of streaming,
 * buffers allocated by AtomISP are expected.
 */
status_t PreviewThread::handleFetchPreviewBuffers()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    if (mSharedMode && mPreviewBuffers.isEmpty()) {

        GraphicBufferMapper &mapper = GraphicBufferMapper::get();
        AtomBuffer tmpBuf = AtomBufferFactory::createAtomBuffer();
        int err(-1);
        buffer_handle_t *buf(NULL);
        MapperPointer mapperPointer;
        mapperPointer.ptr = NULL;
        int lockMode = GRALLOC_USAGE_SW_READ_OFTEN |
                       GRALLOC_USAGE_SW_WRITE_NEVER |
                       GRALLOC_USAGE_HW_COMPOSER;
        const Rect bounds(mPreviewWidth, mPreviewHeight);
        int pixel_stride(0);

        if (mPreviewWindow == NULL) {
            ALOGE("no PreviewWindow set, could not fetch previewBuffers");
            status = INVALID_OPERATION;
            goto freeDeQueued;
        }

        for (size_t i = 0; i < mNumOfPreviewBuffers; i++) {
            err = mPreviewWindow->dequeue_buffer(mPreviewWindow, &buf, &pixel_stride);
            if(err != 0 || buf == NULL) {
                ALOGE("Surface::dequeueBuffer returned error %d (buf=%p)", err, buf);
                status = UNKNOWN_ERROR;
                goto freeDeQueued;
            }
            tmpBuf.buff = NULL;     // We do not allocate a normal camera_memory_t
            tmpBuf.id = i;
            tmpBuf.type = ATOM_BUFFER_PREVIEW_GFX;
            /*
              There may be alignment mismatch between Gfx and ISP. For BayTrial case:
              Gfx is 128 bytes aligned, while ISP may be 64/48/32/16 bytes aligned, we
              forcibly set stride value with mPreviewBpl, when buffer is dequeued
              from ISP, we can repad this buffer then enqueue it to Gfx for displaying.
              Please notice, we could not support Gfx stride is less than ISP stride
              case, this should not happen.
            */
            mGfxBpl = pixelsToBytes(mPreviewFourcc, pixel_stride);
            LOG1("%s bpl-gfx=%d bpl-preview=%d", __FUNCTION__, mGfxBpl, mPreviewBpl);
            if (mGfxBpl >= mPreviewBpl) {
                tmpBuf.bpl = mPreviewBpl;
            } else {
                ALOGE("Gfx buffer size is to small to save ISP preview output");
                status = UNKNOWN_ERROR;
                goto freeDeQueued;
            }

            tmpBuf.width = mPreviewWidth;
            tmpBuf.height = mPreviewHeight;
            tmpBuf.size = frameSize(mPreviewFourcc, pixel_stride, mPreviewHeight);
            tmpBuf.fourcc = mPreviewFourcc;
            tmpBuf.status = FRAME_STATUS_NA;

            status = mapper.lock(*buf, lockMode, bounds, &mapperPointer.ptr);
            if(status != NO_ERROR) {
                ALOGE("Failed to lock GraphicBufferMapper!");
                goto freeDeQueued;
            }

            tmpBuf.dataPtr = mapperPointer.ptr;

            GfxAtomBuffer newBuf;
            newBuf.buffer = tmpBuf;
            newBuf.owner = OWNER_PREVIEWTHREAD;
            newBuf.buffer.gfxInfo.gfxBufferHandle = buf;
            mPreviewBuffers.push(newBuf);
            LOG1("%s: got Gfx Buffer: native_ptr %p, size:(%dx%d), bpl: %d ", __FUNCTION__,
                 buf, mPreviewWidth, mPreviewHeight, tmpBuf.bpl);
        } // for

        mBuffersInWindow = 0;
        mFetchDone = true;
    }
    mMessageQueue.reply(MESSAGE_ID_FETCH_PREVIEW_BUFS, status);
    return status;
freeDeQueued:
    freeGfxPreviewBuffers();
    mMessageQueue.reply(MESSAGE_ID_FETCH_PREVIEW_BUFS, status);
    return status;
}

status_t PreviewThread::handleReturnPreviewBuffers()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    status = freeGfxPreviewBuffers();
    mMessageQueue.reply(MESSAGE_ID_RETURN_PREVIEW_BUFS, status);
    return status;
}


/**
 * Allocates preview buffers from native window.
 *
 * \param numberOfBuffers:[IN]: Number of requested buffers to allocate
 *
 * @return NO_MEMORY: If it could not allocate or dequeue the required buffers
 * @return INVALID_OPERATION: if it couldn't allocate the buffers because lack of preview window
 */
status_t PreviewThread::allocateGfxPreviewBuffers(int numberOfBuffers) {
    LOG1("@%s: num buf: %d", __FUNCTION__, numberOfBuffers);
    status_t status = NO_ERROR;
    if(!mPreviewBuffers.isEmpty()) {
        ALOGW("Preview buffers already allocated size=[%d] -- this should not happen",mPreviewBuffers.size());
        freeGfxPreviewBuffers();
    }

    if (mPreviewWindow != 0) {

        if(numberOfBuffers > MAX_NUMBER_PREVIEW_GFX_BUFFERS)
            return NO_MEMORY;

        int res = mPreviewWindow->set_buffer_count(mPreviewWindow, numberOfBuffers);
        if (res != 0) {
            ALOGW("Surface::set_buffer_count returned %d", res);
            return NO_MEMORY;
        }

        mBuffersInWindow = numberOfBuffers;
        mFetchDone = false;
    } else {
        status = INVALID_OPERATION;
    }

    return status;
}

/**
 * Frees the  preview buffers taken from native window.
 * Goes through the list of GFx preview buffers and unlocks them all
 * using the Graphic Buffer Mapper
 * it does cancel only the ones currently not used by the window
 *
 *
 * @return NO_ERROR
 *
 */
status_t PreviewThread::freeGfxPreviewBuffers() {
    LOG1("@%s: preview buffer: %d", __FUNCTION__, mPreviewBuffers.size());
    size_t i;
    int res;
    GraphicBufferMapper &mapper = GraphicBufferMapper::get();

    if ((mPreviewWindow != NULL) && (!mPreviewBuffers.isEmpty())) {

        for( i = 0; i < mPreviewBuffers.size(); i++) {
            if (mPreviewBuffers[i].owner != OWNER_WINDOW) {

                res = mapper.unlock(*(mPreviewBuffers[i].buffer.gfxInfo.gfxBufferHandle));
                if (res != 0) {
                    ALOGW("%s: unlocking gfx buffer %d failed!", __FUNCTION__, i);
                }
                buffer_handle_t *bufHandle = mPreviewBuffers[i].buffer.gfxInfo.gfxBufferHandle;
                LOG1("%s: canceling gfx buffer[%d]: %p (value = %p)",
                     __FUNCTION__, i, bufHandle, *bufHandle);
                res = mPreviewWindow->cancel_buffer(mPreviewWindow, bufHandle);
                if (res != 0)
                    ALOGW("%s: canceling gfx buffer %d failed!", __FUNCTION__, i);
            }
        }
        mPreviewBuffers.clear();
    }

    if ((mPreviewWindow != NULL) && (!mReservedBuffers.isEmpty())) {

        for( i = 0; i < mReservedBuffers.size(); i++) {
            if (mReservedBuffers[i].owner != OWNER_WINDOW) {

                res = mapper.unlock(*(mReservedBuffers[i].buffer.gfxInfo.gfxBufferHandle));
                if (res != 0) {
                    ALOGW("%s: unlocking gfx (reserved) buffer %d failed!", __FUNCTION__, i);
                }
                buffer_handle_t *bufHandle = mReservedBuffers[i].buffer.gfxInfo.gfxBufferHandle;
                LOG1("%s: canceling gfx buffer[%d] (reserved): %p (value = %p)",
                     __FUNCTION__, i, bufHandle, *bufHandle);
                res = mPreviewWindow->cancel_buffer(mPreviewWindow, bufHandle);
                if (res != 0)
                    ALOGW("%s: canceling gfx buffer %d failed!", __FUNCTION__, i);
            }

        }
        mReservedBuffers.clear();
    }

    LOG1("%s: clearing vectors !",__FUNCTION__);
    mBuffersInWindow = 0;

    return NO_ERROR;
}

/**
 * getGfxBufferBytesPerLine
 * returns the bpl of the buffers dequeued by the current window
 *
 * Please NOTE:
 *  It is the caller responsibility to ensure mPreviewWindow is initialized
 */
int PreviewThread::getGfxBufferBytesPerLine(void)
{
    int pixel_stride = 0;
    buffer_handle_t *buf(NULL);
    int err(-1);
    err = mPreviewWindow->dequeue_buffer(mPreviewWindow, &buf, &pixel_stride);
    if (!err || buf != NULL)
        mPreviewWindow->cancel_buffer(mPreviewWindow, buf);
    else
        ALOGE("Surface::dequeueBuffer returned error %d (buf=%p)", err, buf);

    return pixelsToBytes(mPreviewFourcc, pixel_stride);
}

/**
 * returns one of mReservedBuffer if available (not in Gfx queue)
 */
PreviewThread::GfxAtomBuffer* PreviewThread::pickReservedBuffer()
{
    Vector<GfxAtomBuffer>::iterator it = mReservedBuffers.begin();
    for (;it != mReservedBuffers.end(); ++it) {
        if (it->owner == OWNER_PREVIEWTHREAD) {
            return it;
        }
    }
    return NULL;
}

/**
 * Copies snapshot-postview buffer to preview window for displaying
 *
 * TODO: This is temporary solution to update preview surface with postview
 * frames. Buffers coupling (indexes mapping in AtomISP & ControlThread)
 * technique need to be revisited to properly avoid copy done here and
 * to allow using shared gfx buffers also for postview purpose. Drawing
 * postview should use generic preview() and this method is to be removed.
 *
 * Note: expects the buffers to be of correct size with configuration
 * left from preview ran before snapshot.
 */
status_t PreviewThread::handlePostview(MessagePreview *msg)
{
    int err;
    status_t status = NO_ERROR;
    GraphicBufferMapper &mapper = GraphicBufferMapper::get();

    if (msg->buff.status == FRAME_STATUS_SKIPPED)
        return NO_ERROR;

    LOG2("@%s: id = %d, width = %d, height = %d, format = %s, bpl = %d", __FUNCTION__,
            msg->buff.id, msg->buff.width, msg->buff.height, v4l2Fmt2Str(msg->buff.fourcc), msg->buff.bpl);

    if (!mPreviewWindow) {
        ALOGW("Unable to display postview frame, no window!");
        return NO_ERROR;
    }

    if (msg->buff.type != ATOM_BUFFER_POSTVIEW) {
        // support implemented for using AtomISP postview type only
        LOG1("Unable to display postview frame, input buffer type unexpected");
        return UNKNOWN_ERROR;
    }

    if (msg->buff.width != mPreviewWidth ||
        msg->buff.height != mPreviewHeight) {
        ALOGD("Unable to display postview frame, postview %dx%d -> preview %dx%d ",
                msg->buff.width, msg->buff.height, mPreviewWidth, mPreviewHeight);
        return UNKNOWN_ERROR;
    }

    PreviewState currentState = getPreviewState();
    if (currentState == STATE_ENABLED ||
        currentState == STATE_ENABLED_HIDDEN) {
        // in continuous-mode, preview stream active
        // first try dequeueing from window
        GfxAtomBuffer *buf = dequeueFromWindow();
        if (buf == NULL)
            buf = pickReservedBuffer();
        if (buf) {
            // succeeded
            copyPreviewBuffer(&msg->buff, &buf->buffer);
            mapper.unlock(*buf->buffer.gfxInfo.gfxBufferHandle);
            if ((err = mPreviewWindow->enqueue_buffer(mPreviewWindow,
                        buf->buffer.gfxInfo.gfxBufferHandle)) != 0) {
                ALOGE("Surface::queueBuffer returned error %d", err);
            } else {
                buf->owner = OWNER_WINDOW;
                mBuffersInWindow++;
            }
        } else {
            LOG1("Postview not rendered, all buffers in use");
        }
    } else if (currentState == STATE_STOPPED) {
        // TODO: properly leave buffers not freed for postview rendering
        // to do things commonly.
        //
        // We were in non-continuous mode and stopPreviewCore() was
        // done before switching to snapshot mode and PreviewThread's buffers
        // were freed. Configuration still applies, so we dequeue one back to
        // copy our postview.
        AtomBuffer tmpBuf = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_POSTVIEW);
        getEffectiveDimensions(&tmpBuf.width,&tmpBuf.height);
        buffer_handle_t *buf(NULL);
        int pixel_stride(0);

        const Rect bounds(tmpBuf.width, tmpBuf.height);

        // queue one from the window
        err = mPreviewWindow->dequeue_buffer(mPreviewWindow, &buf, &pixel_stride);
        if (err != 0 || buf == NULL) {
            ALOGW("Error dequeuing preview buffer for postview. error=%d (buf=%p)", err, buf);
            return UNKNOWN_ERROR;
        }
        tmpBuf.bpl = pixelsToBytes(mPreviewFourcc, pixel_stride);
        tmpBuf.size = frameSize(mPreviewFourcc, pixel_stride, mPreviewHeight);
        MapperPointer mapperPointer;
        mapperPointer.ptr = NULL;

        err = mapper.lock(*buf,
            GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_SW_READ_NEVER,
            bounds, &mapperPointer.ptr);
        if (err != 0) {
            ALOGE("Error locking buffer for postview rendering");
            mPreviewWindow->cancel_buffer(mPreviewWindow, buf);
            return UNKNOWN_ERROR;
        }

        tmpBuf.dataPtr = mapperPointer.ptr;

        copyPreviewBuffer(&msg->buff,&tmpBuf);

        mapper.unlock(*buf);

        err = mPreviewWindow->enqueue_buffer(mPreviewWindow, buf);
        if (err != 0)
            ALOGE("Surface::queueBuffer returned error %d", err);
    } else {
        LOG1("Postview not rendered, preview enabled!");
        return UNKNOWN_ERROR;
    }

    LOG1("@%s: done", __FUNCTION__);

    if (msg->synchronous)
        mMessageQueue.reply(MESSAGE_ID_POSTVIEW, status);

    return status;
}

/**
 * Copies or rotates the buffer given by the ControlThread.
 *
 * Usually the src is a buffer from the ControlThread and the dst is a Gfx buffer
 * dequeued from the preview window
 *
 * The rotation is passed when the overlay is enabled in cases where the scan
 * order of the display and camera are different
 */
void PreviewThread::copyPreviewBuffer(AtomBuffer* src, AtomBuffer* dst)
{
    switch (mRotation) {
    case 90:
        nv12rotateBy90(src->width,       // width of the source image
                       src->height,      // height of the source image
                       src->bpl,      // scanline bpl of the source image
                       dst->bpl,      // scanline bpl of the target image
                       (const char*)src->dataPtr,               // source image
                       (char *)dst->dataPtr);                 // target image
        break;
    case 270:
        // TODO: Not handled, waiting for Semi
        ALOGE("@%s: 270 case not handled", __FUNCTION__);
        break;
    case 0:
        memcpy((char *)dst->dataPtr, (const char*)src->dataPtr, dst->size);
        break;
    }

}

/**
 * Returns the effective dimensions of the preview
 * we store only the original request from the client in mPreviewWidth and
 * mPreviewHeight. When we use these values we need to take into account any
 * rotation that we need to do to the buffers in case we are using overlay.
 * \param w [out] pointer to the effective width
 * \param h [out] pointer to the effective height
 */
void PreviewThread::getEffectiveDimensions(int *w, int *h)
{
    if (mRotation == 90 || mRotation == 270) {
        // we swap width and height
        *w = mPreviewHeight;
        *h = mPreviewWidth;
    } else {
        *w = mPreviewWidth;
        *h = mPreviewHeight;
    }
}
} // namespace android
