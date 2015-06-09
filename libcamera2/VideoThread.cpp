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
#define LOG_TAG "Camera_VideoThread"

#include "VideoThread.h"
#include "LogHelper.h"
#include "CallbacksThread.h"
#include "IntelParameters.h"
#include "PlatformData.h"
#include "AtomISP.h"

namespace android {

VideoThread::VideoThread(AtomISP *atomIsp, sp<CallbacksThread> callbacksThread) :
    Thread(true) // callbacks may call into java
    ,mIsp(atomIsp)
    ,mMessageQueue("VideoThread", MESSAGE_ID_MAX)
    ,mThreadRunning(false)
    ,mCallbacksThread(callbacksThread)
    ,mSlowMotionRate(1)
    ,mState(STATE_IDLE)
    ,mMirror(false)
    ,mRecOrientation(0)
    ,mCamOrientation(0)
    ,mCameraId(-1)
{
    LOG1("@%s", __FUNCTION__);
#ifdef GRAPHIC_IS_GEN
    mVpp = new VideoVPPBase();
    if (!mVpp) {
        ALOGE("Fail to construct VPP");
    }
#endif
    IHWSensorControl *hwSensorCtl = mIsp->getSensorControlInterface();

    if (hwSensorCtl != NULL)
        mCameraId = hwSensorCtl->getCurrentCameraId();
    else
        ALOGW("Sensor HW not initialized");

    hwSensorCtl = NULL;
    reset();
}

VideoThread::~VideoThread()
{
    LOG1("@%s", __FUNCTION__);
#ifdef GRAPHIC_IS_GEN
    if (mVpp) {
        delete mVpp;
        mVpp = NULL;
    }
#endif

    reset();
}

/**
 * override for IAtomIspObserver::atomIspNotify()
 *
 * VideoThread is attached to receive preview stream notifications
 * to handle dequeueing of recording frames in video mode.
 * NOTE: not touching Preview buffer here and ignoring state changes
 */
bool VideoThread::atomIspNotify(IAtomIspObserver::Message *msg, const ObserverState state)
{
    LOG2("@%s", __FUNCTION__);

    if (msg) {
        AtomBuffer *buff = &msg->data.frameBuffer.buff;
        if (msg->id != IAtomIspObserver::MESSAGE_ID_FRAME) {
            LOG1("Received unexpected notify message id %d!", msg->id);
            if (msg->id == IAtomIspObserver::MESSAGE_ID_ERROR) {
                ALOGE("Error in preview stream");
            }
            return false;
        }

        if (mIsp->getMode() == MODE_VIDEO || mIsp->getMode() == MODE_CONTINUOUS_VIDEO ||
            mIsp->getMode() == MODE_CONTINUOUS_JPEG_VIDEO) {
            bool skipFrame = mIsp->checkSkipFrameRecording(msg->data.frameBuffer.buff.frameCounter);

            Message local_msg;
            local_msg.id = MESSAGE_ID_DEQUEUE_RECORDING;
            local_msg.data.dequeueRecording.skipFrame =
               (buff->status == FRAME_STATUS_CORRUPTED) || skipFrame;
            mMessageQueue.send(&local_msg);
        }
    }

    return false;
}

status_t VideoThread::startRecording()
{
    LOG1("@%s", __FUNCTION__);
    if (mState == STATE_RECORDING) {
        ALOGW("Already in recording state");
        return NO_ERROR;
    }

    Message msg;
    msg.id = MESSAGE_ID_START_RECORDING;
    return mMessageQueue.send(&msg, MESSAGE_ID_START_RECORDING);
}

status_t VideoThread::stopRecording()
{
    LOG1("@%s", __FUNCTION__);
    if (mState != STATE_RECORDING) {
        ALOGW("Invalid stopRecording request for state:%d", mState);
        return NO_ERROR;
    }

    Message msg;
    msg.id = MESSAGE_ID_STOP_RECORDING;
    return mMessageQueue.send(&msg, MESSAGE_ID_STOP_RECORDING);
}

status_t VideoThread::setRecordingMirror(bool mirror, int recOrientation, int camOrientation)
{
    // TODO: should return error if we don't allow to set mirror in some cases
    LOG1("@%s :%d recOrientation:%d, camOrientation:%d", __FUNCTION__,
           mirror, recOrientation, camOrientation);
    Mutex::Autolock lock(mLock);
    if (mMirror != mirror) {
        mMirror = mirror;
    }
    // orientation is valid only if mMirror is true
    if (mMirror) {
        mRecOrientation = recOrientation;
        mCamOrientation = camOrientation;
    }

    return NO_ERROR;
}

// currently we just reset some variable here
void VideoThread::reset()
{
    mFirstFrameTimestamp = 0;
    // clear reserved lists
    mSnapshotBuffers.clear();
    mRecordingBuffers.clear();
}

status_t VideoThread::handleMessageStartRecording()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    reset();

    mState = STATE_RECORDING;
    mMessageQueue.reply(MESSAGE_ID_START_RECORDING, status);
    return status;
}

status_t VideoThread::handleMessageStopRecording()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    mState = STATE_PREVIEW;
#ifdef GRAPHIC_IS_GEN
    // to flush vpp buffers inside
    if (mVpp)
        mVpp->stop();
#endif
    reset();
    mMessageQueue.reply(MESSAGE_ID_STOP_RECORDING, status);
    return status;
}

status_t VideoThread::handleMessageDequeueRecording(MessageDequeueRecording *msg)
{
    LOG2("@%s", __FUNCTION__);
    AtomBuffer buff = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_VIDEO);

    status_t status = NO_ERROR;

    // after ISP timeout, we will get a burst of notifications without really that
    // many recording buffers, so we need to skip the unnecessary notifications
    status = mIsp->getRecordingFrame(&buff);
    if (status == NOT_ENOUGH_DATA) {
        ALOGW("@%s - recording frame was not ready. Maybe there was an ISP timeout?", __FUNCTION__);
        return NO_ERROR;
    }

    if (status == NO_ERROR) {
       if (buff.status != FRAME_STATUS_CORRUPTED) {
            // Check whether driver has run out of buffers
            if (!mIsp->dataAvailable()) {
                ALOGE("Video frame dropped, buffers reserved : %d video encoder, %d video snapshot",
                        mRecordingBuffers.size(), mSnapshotBuffers.size());
                msg->skipFrame = true;
            }
            // See if recording has started (state).
            // If it has, process the buffer, unless frame is to be dropped.
            // If recording hasn't started or frame is dropped, return the buffer to the driver
            if (mState == STATE_RECORDING && !msg->skipFrame) {
                mLock.lock();
                // Mirror the recording buffer if mirroring is enabled
                if (mMirror) {
                    mirrorBuffer(&buff, mRecOrientation, mCamOrientation);
                }

                mRecordingBuffers.push(buff);
                mFrameCondition.signal();
                mLock.unlock();
                // process video buffers
                processVideoBuffer(buff);
            } else {
                if (mState == STATE_IDLE) {
                    LOG1("VideoThread switch to preview mode");
                    mState = STATE_PREVIEW;
                }
                mIsp->putRecordingFrame(&buff);
            }
        } else {
            ALOGD("Recording frame %d corrupted, ignoring", buff.id);
            mIsp->putRecordingFrame(&buff);
        }
    } else {
        ALOGE("Error: getting recording from isp\n");
    }

    return status;
}

status_t VideoThread::getVideoSnapshot(AtomBuffer &buff)
{
    LOG1("@%s", __FUNCTION__);
    if (mState != STATE_RECORDING)
        return INVALID_OPERATION;

    // lock to wait until mRecordingBuffers size > 0
    Mutex::Autolock lock(mLock);
    while (mRecordingBuffers.empty()) {
        LOG1("%s empty, to wait", __FUNCTION__);
        mFrameCondition.wait(mLock);
    }

    buff = mRecordingBuffers.top();
    LOG1("%s get buffer id:%d", __FUNCTION__, buff.id);
    mSnapshotBuffers.push(buff);
    return NO_ERROR;
}

status_t VideoThread::putVideoSnapshot(AtomBuffer *buff)
{
    Mutex::Autolock lock(mLock);

    LOG1("@%s", __FUNCTION__);
    AtomBuffer *videoBuffer = findVideoSnapshotBuffer(buff->id);

    if (videoBuffer) {
        // check if also reserved by encoder
        if (!mRecordingBuffers.empty()) {
            AtomBuffer *recBuffer = findRecordingBuffer(buff->id);
            if (recBuffer) {
                LOG1("Snapshot buffer found reserved for video encoding");
                // drop from reserved list
                mSnapshotBuffers.erase(videoBuffer);
                return NO_ERROR;
            }
        }

        videoBuffer->owner->returnBuffer(videoBuffer);
        mSnapshotBuffers.erase(videoBuffer);
    }
    return NO_ERROR;
}

status_t VideoThread::releaseRecordingFrame(void *buff)
{
    LOG2("@%s: buff = %p", __FUNCTION__, buff);
    Message msg;
    msg.id = MESSAGE_ID_RELEASE_RECORDING_FRAME;
    msg.data.releaseRecordingFrame.buff = buff;
    return mMessageQueue.send(&msg);
}

status_t VideoThread::handleMessageReleaseRecordingFrame(MessageReleaseRecordingFrame *msg)
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    Mutex::Autolock lock(mLock);

    if (mState == STATE_RECORDING) {
        AtomBuffer *recBuff = findRecordingBuffer(msg->buff);
        if (recBuff == NULL) {
            // This may happen with buffer sharing. When the omx component is stopped
            // it disables buffer sharing and deallocates its buffers. Internally we check
            // to see if sharing was disabled then we restart the ISP with new buffers. In
            // the mean time, the app is returning us shared buffers when we are no longer
            // using them.
            ALOGE("Could not find recording buffer: %p", msg->buff);
            return DEAD_OBJECT;
        }
        LOG2("Recording buffer released from encoder, buff id = %d", recBuff->id);
        // check if also reserved by snapshot
        if (!mSnapshotBuffers.empty()) {
            AtomBuffer *videoBuffer = findVideoSnapshotBuffer(recBuff->id);
            if (videoBuffer) {
                LOG1("Recording buffer found reserved for video snapshot");
                // drop from reserved list
                mRecordingBuffers.erase(recBuff);
                return NO_ERROR;
            }
        }

        // Do we need to detect errors here before erase()?
        recBuff->owner->returnBuffer(recBuff);
        mRecordingBuffers.erase(recBuff);
    }

    return status;
}

AtomBuffer* VideoThread::findVideoSnapshotBuffer(int index)
{
    Vector<AtomBuffer>::iterator it = mSnapshotBuffers.begin();
    for (;it != mSnapshotBuffers.end(); ++it) {
        if (it->id == index)
            return it;
    }

    return NULL;
}

AtomBuffer* VideoThread::findRecordingBuffer(int index)
{
    Vector<AtomBuffer>::iterator it = mRecordingBuffers.begin();
    for (;it != mRecordingBuffers.end(); ++it) {
        if (it->id == index)
            return it;
    }

    return NULL;
}

AtomBuffer* VideoThread::findRecordingBuffer(void *ptr)
{
    Vector<AtomBuffer>::iterator it = mRecordingBuffers.begin();
    for (;it != mRecordingBuffers.end(); ++it) {
        if(it->metadata_buff != NULL && it->metadata_buff->size > 0) {
            if (it->metadata_buff->data == ptr)
                return it;
        } else {
             if (it->dataPtr == ptr)
                return it;
        }
    }

    ALOGD("@%s ptr:%p buffer is not found", __FUNCTION__, ptr);
    return NULL;
}

status_t VideoThread::flushBuffers()
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_FLUSH;
    mMessageQueue.remove(MESSAGE_ID_DEQUEUE_RECORDING);
    return mMessageQueue.send(&msg, MESSAGE_ID_FLUSH);
}

status_t VideoThread::setSlowMotionRate(int rate)
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_SET_SLOWMOTION_RATE;
    msg.data.setSlowMotionRate.rate = rate;
    return mMessageQueue.send(&msg);
}

void VideoThread::getDefaultParameters(CameraParameters *intel_params, int cameraId)
{
    LOG1("@%s", __FUNCTION__);
    if (!intel_params) {
        ALOGE("params is null!");
        return;
    }
    // Set slow motion rate in high speed mode
    if (PlatformData::supportsSlowMotion(cameraId)) {
        intel_params->set(IntelCameraParameters::KEY_SLOW_MOTION_RATE, IntelCameraParameters::SLOW_MOTION_RATE_1X);
        intel_params->set(IntelCameraParameters::KEY_SUPPORTED_SLOW_MOTION_RATE, "1x,2x,3x,4x");
    }

    intel_params->set(IntelCameraParameters::REC_BUFFER_SHARING_SESSION_ID, "0");
}

status_t VideoThread::handleMessageExit()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    mThreadRunning = false;
    return status;
}

status_t VideoThread::convertNV12Linear2Tiled(const AtomBuffer &buff)
{
#ifdef GRAPHIC_IS_GEN
    VAStatus ret;
    RenderTarget Src, Dst;
    ANativeWindowBuffer *nativeBuffer = buff.gfxInfo_rec.gfxBuffer->getNativeBuffer();

    if (mVpp == NULL) {
        ALOGE("@%s vpp is not valid", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    Src.width  = buff.bpl;
    Src.height = buff.height;
    Src.stride = buff.bpl;
    Src.format = VA_RT_FORMAT_YUV420;
    Src.pixel_format = VA_FOURCC_NV12;
    Src.type   = RenderTarget::ANDROID_GRALLOC;
    Src.handle = (int)*buff.gfxInfo.gfxBufferHandle;
    Src.rect.x = Src.rect.y = 0;
    Src.rect.width = buff.width;
    Src.rect.height = buff.height;

    // Specific format information is in graphic handle.
    Dst.width  = nativeBuffer->stride;
    Dst.height = ALIGN32(buff.height);
    Dst.stride = nativeBuffer->stride;
    Dst.format = VA_RT_FORMAT_YUV420;
    Dst.pixel_format = VA_FOURCC_NV12;
    Dst.type   = RenderTarget::ANDROID_GRALLOC;
    Dst.handle = (int)*buff.gfxInfo_rec.gfxBufferHandle;
    Dst.rect.x = Dst.rect.y = 0;
    Dst.rect.width = buff.width;
    Dst.rect.height = buff.height;

    LOG2("@%s Src %dx%d s:%d handle:%x", __FUNCTION__, Src.width, Src.height, Src.stride, Src.handle);
    LOG2("@%s Dst %dx%d s:%d handle:%x", __FUNCTION__, Dst.width, Dst.height, Dst.stride, Dst.handle);

    /**
     * vpp start will be called automatically inside perform
     * perform asynchronously to speed up recording process.
     */
    ret = mVpp->perform(Src, Dst, NULL, true);
    if (ret != VA_STATUS_SUCCESS) {
        ALOGE("@%s error:%x", __FUNCTION__, ret);
        return UNKNOWN_ERROR;
    }
#endif //GRAPHIC_IS_GEN
    return OK;
}

status_t VideoThread::processVideoBuffer(AtomBuffer &buff)
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    nsecs_t timestamp = (buff.capture_timestamp.tv_sec)*1000000000LL
                        + (buff.capture_timestamp.tv_usec)*1000LL;

    if(mSlowMotionRate > 1)
    {
        if(mFirstFrameTimestamp == 0)
            mFirstFrameTimestamp = timestamp;
        timestamp = (timestamp - mFirstFrameTimestamp) * mSlowMotionRate + mFirstFrameTimestamp;
    }

    if (convertNV12Linear2Tiled(buff)) {
        // Print err and do nothing here
        ALOGE("Fail to convertNV12Linear2Tiled");
    }

    mCallbacksThread->videoFrameDone(&buff, timestamp);

    return status;
}

/**
 * Video thread flush buffer is not going to return all buffers that were
 * held in encoder. They will be recycled in AtomISP.Nothing need to do now
 * Maybe we need to do something in the future
 */
status_t VideoThread::handleMessageFlush()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    mMessageQueue.remove(MESSAGE_ID_DEQUEUE_RECORDING);
    mMessageQueue.reply(MESSAGE_ID_FLUSH, status);
    return status;
}

status_t VideoThread::handleMessageSetSlowMotionRate(MessageSetSlowMotionRate* msg)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    mSlowMotionRate = msg->rate;
    return status;
}

status_t VideoThread::waitForAndExecuteMessage()
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    Message msg;
    mMessageQueue.receive(&msg);

    switch (msg.id) {

        case MESSAGE_ID_EXIT:
            status = handleMessageExit();
            break;

        case MESSAGE_ID_FLUSH:
            status = handleMessageFlush();
            break;

        case MESSAGE_ID_RELEASE_RECORDING_FRAME:
            status = handleMessageReleaseRecordingFrame(&msg.data.releaseRecordingFrame);
            break;

        case MESSAGE_ID_SET_SLOWMOTION_RATE:
            status = handleMessageSetSlowMotionRate(&msg.data.setSlowMotionRate);
            break;

        case MESSAGE_ID_DEQUEUE_RECORDING:
            status = handleMessageDequeueRecording(&msg.data.dequeueRecording);
            break;

        case MESSAGE_ID_START_RECORDING:
            status = handleMessageStartRecording();
            break;

        case MESSAGE_ID_PUSH_FRAME:
            status = handleMessagePushFrame(&msg.data.pushFrame);
            break;

        case MESSAGE_ID_STOP_RECORDING:
            status = handleMessageStopRecording();
            break;

        default:
            ALOGE("Invalid message");
            status = BAD_VALUE;
            break;
    };
    return status;
}

bool VideoThread::threadLoop()
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    mThreadRunning = true;
    while (mThreadRunning)
        status = waitForAndExecuteMessage();

    return false;
}

status_t VideoThread::requestExitAndWait()
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

/**
 * override for ICallbackPreview::previewBufferCallback()
 *
 * For now this callback is used for implementing single stream preview/video
 * 0-copy buffer flow to be used with HAL video stabilization component.
 */
void VideoThread::previewBufferCallback(AtomBuffer *buff, ICallbackPreview::CallbackType t)
{
    LOG2("@%s", __FUNCTION__);
    if (t != ICallbackPreview::OUTPUT_WITH_DATA) {
        ALOGE("Unexpected preview/video buffer callback type!");
        return;
    }

    // If we are in recording mode, pass to encoder. Otherwise we're in preview,
    // and no need to encode -> ignore frame

    // this is done synchronously so that the possibly on-the-fly-allocated
    // metadata buffer is stored straight into the caller's atombuffer
    // pointer.
    Message msg;
    msg.id = MESSAGE_ID_PUSH_FRAME;
    msg.data.pushFrame.buf = buff;
    mMessageQueue.send(&msg, MESSAGE_ID_PUSH_FRAME);

}

status_t VideoThread::handleMessagePushFrame(MessagePushFrame *msg)
{
    LOG2("@%s", __FUNCTION__);

    if (mState == STATE_RECORDING) {
        mLock.lock();
        // Mirror the recording buffer if mirroring is enabled
        if (mMirror) {
            mirrorBuffer(msg->buf, mRecOrientation, mCamOrientation);
        }

        // process video buffers
        // if there is no metadata, allocate it now
        if (msg->buf->metadata_buff == NULL) {
            mIsp->allocateMetaDataBuffers(msg->buf, 1);
        }

        mRecordingBuffers.push(*msg->buf);

        mFrameCondition.signal();
        mLock.unlock();
        processVideoBuffer(*msg->buf);
    } else {
        if (mState == STATE_IDLE) {
            LOG1("VideoThread switch to preview mode");
            mState = STATE_PREVIEW;
        }
        // return buffer from video thread, since it is idling
        msg->buf->owner->returnBuffer(msg->buf);
    }

    mMessageQueue.reply(MESSAGE_ID_PUSH_FRAME, OK);

    return OK;
}

int VideoThread::getCameraID()
{
    return mCameraId;
}

} // namespace android
