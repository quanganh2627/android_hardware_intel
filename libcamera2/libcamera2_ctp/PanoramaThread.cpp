/*
 * Copyright (C) 2012 The Android Open Source Project
 * Copyright (c) 2012 Intel Corporation
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
#define LOG_TAG "Camera_Panorama"
//#define LOG_NDEBUG 0

#include "ImageScaler.h"
#include <math.h>
#include <assert.h>

#include "PanoramaThread.h"
#include "IntelParameters.h"
#include "LogHelper.h"
#include "AtomCommon.h"
#include "ICameraHwControls.h"
#include "PlatformData.h"
#include "AtomCP.h"

namespace android {

#ifdef ENABLE_INTEL_EXTRAS

// The interval for polling stitch status in case of failure:
static const useconds_t STITCH_CHECK_INTERVAL_USEC = 500000; // 0.5 secs
// Max count of stitch check tries:
static const int STITCH_CHECK_LIMIT = 10;

PanoramaThread::PanoramaThread(ICallbackPanorama *panoramaCallback, I3AControls *aaaControls,
                               sp<CallbacksThread> callbacksThread, Callbacks *callbacks, int cameraId) :
    Thread(false)
    ,mPanoramaCallback(panoramaCallback)
    ,mContext(NULL)
    ,mMessageQueue("Panorama", (int) MESSAGE_ID_MAX)
    ,mPanoramaTotalCount(0)
    ,mThreadRunning(false)
    ,mPanoramaWaitingForImage(false)
    ,mCallbacksThread(callbacksThread)
    ,mCallbacks(callbacks) // for memory allocation
    ,mState(PANORAMA_STOPPED)
    ,mPreviewWidth(0)
    ,mPreviewHeight(0)
    ,mThumbnailWidth(0)
    ,mThumbnailHeight(0)
    ,mPanoramaStitchThread(NULL)
    ,mStopInProgress(false)
    ,mCameraId(cameraId)
    ,m3AControls(aaaControls)
{
    LOG1("@%s", __FUNCTION__);
    mCurrentMetadata.direction = 0;
    mCurrentMetadata.motion_blur = false;
    mCurrentMetadata.horizontal_displacement = 0;
    mCurrentMetadata.vertical_displacement = 0;
    mPanoramaMaxSnapshotCount = PlatformData::getMaxPanoramaSnapshotCount();
}

PanoramaThread::~PanoramaThread()
{
    LOG1("@%s", __FUNCTION__);
}

void PanoramaThread::getDefaultParameters(CameraParameters *intel_params, int cameraId)
{
    LOG1("@%s", __FUNCTION__);
    if (!intel_params) {
        ALOGE("params is null!");
        assert(false);
        return;
    }

    // Set if Panorama is available or not.
    // panorama
    intel_params->set(IntelCameraParameters::KEY_PANORAMA_LIVE_PREVIEW_SIZE, CAM_RESO_STR(PANORAMA_DEF_PREV_WIDTH,PANORAMA_DEF_PREV_HEIGHT));
    intel_params->set(IntelCameraParameters::KEY_SUPPORTED_PANORAMA, PlatformData::supportedPanorama(cameraId));
    intel_params->set(IntelCameraParameters::KEY_PANORAMA_MAX_SNAPSHOT_COUNT, mPanoramaMaxSnapshotCount);
    intel_params->set(IntelCameraParameters::KEY_SUPPORTED_PANORAMA_LIVE_PREVIEW_SIZES,
            CAM_RESO_STR(PANORAMA_MAX_LIVE_PREV_WIDTH,PANORAMA_MAX_LIVE_PREV_HEIGHT)
            ",800x600,720x480,640x480,640x360,416x312,352x288,320x240,320x180,176x144,160x120");
}

void PanoramaThread::startPanorama(void)
{
    LOG1("@%s", __FUNCTION__);
    if (mState == PANORAMA_STOPPED) {
        Message msg;
        msg.id = MESSAGE_ID_START_PANORAMA;
        mMessageQueue.send(&msg);
        mState = PANORAMA_STARTED;
    }
}

status_t PanoramaThread::handleMessageStartPanorama(void)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    Mutex::Autolock lock(mStitchLock);

    mContext = ia_panorama_init(NULL);
    if (mContext == NULL) {
        ALOGE("fatal - error initializing panorama");
        assert(false);
        return UNKNOWN_ERROR;
    }

    mPanoramaStitchThread = new PanoramaStitchThread(mCallbacks);
    if (mPanoramaStitchThread == NULL) {
        ALOGE("error creating PanoramaThread");
        assert(false);
        return NO_MEMORY;
    }

    status = mPanoramaStitchThread->run("CamHAL_PANOSTITCH");
    if (status != NO_ERROR) {
        ALOGE("Error starting PanoramaStitchThread!");
    }

    return status;
}

void PanoramaThread::stopPanorama(bool synchronous)
{
    LOG1("@%s", __FUNCTION__);
    Message msg;

    // cancel stitching to make stop faster
    mStitchLock.lock();
    if (mPanoramaStitchThread != NULL && mContext != NULL) {
        mStopInProgress = true;
        mPanoramaStitchThread->cancel(mContext);
    }
    mStitchLock.unlock();

    msg.id = MESSAGE_ID_STOP_PANORAMA;
    msg.data.stop.synchronous = synchronous;
    mMessageQueue.send(&msg, synchronous ? MESSAGE_ID_STOP_PANORAMA : (MessageId) -1);
}

void PanoramaThread::flush()
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_FLUSH;
    mMessageQueue.send(&msg, MESSAGE_ID_FLUSH);
}

status_t PanoramaThread::handleMessageFlush()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = mPanoramaStitchThread->flush();
    mMessageQueue.reply(MESSAGE_ID_FLUSH, status);
    return OK;
}

status_t PanoramaThread::handleMessageStopPanorama(const MessageStopPanorama &stop)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    Mutex::Autolock lock(mStitchLock);

    if (mContext) {
        if (mPanoramaTotalCount > 0 && mPanoramaStitchThread != NULL)
            cancelStitch();

        ia_panorama_uninit(mContext);

        mPanoramaTotalCount = 0;
        mCurrentMetadata.direction = 0;
        mCurrentMetadata.motion_blur = false;
        mCurrentMetadata.horizontal_displacement = 0;
        mCurrentMetadata.vertical_displacement = 0;

        mContext = NULL;
    }

    if (mPanoramaStitchThread != NULL) {
        mPanoramaStitchThread->requestExitAndWait();
        mPanoramaStitchThread.clear();
    }

    mState = PANORAMA_STOPPED;
    mStopInProgress = false;
    if (stop.synchronous)
        mMessageQueue.reply(MESSAGE_ID_STOP_PANORAMA, status);

    return status;
}

void PanoramaThread::startPanoramaCapture()
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_START_PANORAMA_CAPTURE;
    mMessageQueue.send(&msg);
}

status_t PanoramaThread::handleMessageStartPanoramaCapture()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    if (mState == PANORAMA_STARTED) {
        reInit();
        mState = PANORAMA_WAITING_FOR_SNAPSHOT;
    }
    else
        status = INVALID_OPERATION;

    return status;
}
void PanoramaThread::stopPanoramaCapture()
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_STOP_PANORAMA_CAPTURE;
    mMessageQueue.send(&msg);
}

status_t PanoramaThread::handleMessageStopPanoramaCapture()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    if (mState == PANORAMA_DETECTING_OVERLAP || mState == PANORAMA_WAITING_FOR_SNAPSHOT)
        mState = PANORAMA_STARTED;
    else
        status = INVALID_OPERATION;

    return status;
}

status_t PanoramaThread::reInit()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    ia_panorama_reinit(mContext);

    return status;
}

bool PanoramaThread::isBlurred(int width, int dx, int dy) const
{
    LOG1("@%s", __FUNCTION__);
    SensorAeConfig config;
    m3AControls->getExposureInfo(config);

    float speed = sqrtf(dx * dx + dy * dy);
    float percentage = speed / width; // assuming square pixels
    float blurvalue = percentage * config.expTime;

    return blurvalue > PANORAMA_MAX_BLURVALUE;
}

bool PanoramaThread::detectOverlap(ia_frame *frame)
{
    LOG2("@%s", __FUNCTION__);

    if (mPanoramaTotalCount < mPanoramaMaxSnapshotCount) {
        int ret = ia_panorama_detect_overlap(mContext, frame);
        LOG2("@%s: direction: %d, H-displacement: %d, V-displacement: %d", __FUNCTION__,
            mContext->direction, mContext->horizontal_displacement, mContext->vertical_displacement);

        // calculate motion blur (based on movement compared to previous displacement)
        int dxPrev = mContext->horizontal_displacement - mCurrentMetadata.horizontal_displacement;
        int dyPrev = mContext->vertical_displacement - mCurrentMetadata.vertical_displacement;
        mCurrentMetadata.motion_blur = isBlurred(frame->width, dxPrev, dyPrev);
        // store values, do displacement callback
        mCurrentMetadata.horizontal_displacement = mContext->horizontal_displacement;
        mCurrentMetadata.vertical_displacement = mContext->vertical_displacement;
        mCurrentMetadata.direction = mContext->direction;
        mCurrentMetadata.finalization_started = false;
        mCallbacksThread->panoramaDisplUpdate(mCurrentMetadata);

        // capture triggering, after first capture, if panorama engine so suggests
        if (mPanoramaTotalCount > 0 && ret) {
            return true;
        }
    }

    return false;
}

status_t PanoramaThread::stitch(AtomBuffer *img, AtomBuffer *pv)
{
    LOG1("@%s", __FUNCTION__);
    if (mState != PANORAMA_WAITING_FOR_SNAPSHOT) {
        ALOGE("Panorama stitch called in wrong state (%d)", mState);
        return INVALID_OPERATION;
    }

    Message msg;
    msg.id = MESSAGE_ID_STITCH;

    msg.data.stitch.img = *img;
    msg.data.stitch.pv = *pv;
    msg.data.stitch.stitchId = ia_panorama_prepare_stitch(mContext);
    LOG2("stitchId = %d", msg.data.stitch.stitchId);

    return mMessageQueue.send(&msg, MESSAGE_ID_STITCH);
}

status_t PanoramaThread::cancelStitch()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    mPanoramaStitchThread->cancel(mContext);

    return status;
}

void PanoramaThread::finalize(void)
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_FINALIZE;
    mMessageQueue.send(&msg);
}

/**
 * fullHeightSrcForThumb sets srcWidth, srcHeight and startPixel such that the panorama thumbnail will be
 * created by using the full source image height. Width which is used from src image is calculated according
 * to the thumbnail aspect ratio, and centered to the src image with the startPixel. The srcWidth is also made
 * dividable by four so that the scaler works.
 */
void PanoramaThread::fullHeightSrcForThumb(AtomBuffer &img, int &srcWidth, int &srcHeight, int &startPixel)
{
    startPixel = img.width / 2 - mThumbnailWidth / 2;
    srcHeight = img.height;
    srcWidth = ceil((float) img.height * mThumbnailWidth / mThumbnailHeight);
    srcWidth = (srcWidth + 2) & ~0x3; // hack for scaler
}

/**
 * fullWidthSrcForThumb sets srcWidth, srcHeight, skipLinesTop and skipLinesBottom such that the panorama
 * thumbnail will be created by using the full source image width. Height which is used from src image is calculated
 * according to the thumbnail aspect ratio, and centered to the src image with the skipLinesTop and skipLinesBottom.
 */
void PanoramaThread::fullWidthSrcForThumb(AtomBuffer &img, int &srcWidth, int &srcHeight, int &skipLinesTop, int &skipLinesBottom)
{
    srcWidth = img.width;
    srcHeight = img.width * mThumbnailHeight / mThumbnailWidth;
    skipLinesTop = img.height / 2 - srcHeight / 2;
    skipLinesBottom = img.height - srcHeight - skipLinesTop;
}

status_t PanoramaThread::handleMessageFinalize()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    if (mState == PANORAMA_STARTED) {
        LOG1("@%s: nothing to finalize", __FUNCTION__);
        return status;
    }

    mPanoramaStitchThread->flush();

    int checkCnt = 0;
    // ia_panorama_check_stitch_status() should always return 0, as all remained stiching
    // is processed in flush()
    while (ia_panorama_check_stitch_status(mContext) != 0 && checkCnt < STITCH_CHECK_LIMIT) {
        LOG2("Polling ia_panorama_check_stitch_status()");
        usleep(STITCH_CHECK_INTERVAL_USEC);
        ++checkCnt;
    }

    if (checkCnt == STITCH_CHECK_LIMIT) {
        ALOGE("Panorama stitching error: stitch check retries exceeded");
        return UNKNOWN_ERROR;
    }

    ia_frame *pFrame = ia_panorama_finalize(mContext);
    if (!pFrame) {
        Mutex::Autolock lock(mStitchLock);

        if (mStopInProgress) {
            ALOGD("ia_panorama_finalize() aborted, because of stop panorama in progress");
            return NO_ERROR;
        } else {
            ALOGE("ia_panorama_finalize() failed");
            return UNKNOWN_ERROR;
        }
    }

    mPanoramaTotalCount = 0;
    mCurrentMetadata.direction = 0;
    mCurrentMetadata.motion_blur = false;
    mCurrentMetadata.horizontal_displacement = 0;
    mCurrentMetadata.vertical_displacement = 0;

    // create AtomBuffer descriptor for panorama engine memory (ia_frame)
    AtomBuffer img = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_PANORAMA);
    img.width = pFrame->width;
    img.height = pFrame->height;
    img.bpl = pFrame->stride;
    img.fourcc = V4L2_PIX_FMT_NV12;
    img.size = frameSize(V4L2_PIX_FMT_NV12, img.bpl, img.height); // because pFrame->size from panorama is currently incorrectly zero
    img.buff = NULL;
    img.owner = this;
    img.dataPtr = pFrame->data;
    // return panorama image via callback to PostProcThread, which passes it onwards

    if (mThumbnailWidth > 0 && mThumbnailHeight > 0) {
        AtomBuffer pvImg = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_POSTVIEW);
        pvImg.width = mThumbnailWidth;
        pvImg.height = mThumbnailHeight;
        pvImg.bpl = mThumbnailWidth;
        pvImg.fourcc = V4L2_PIX_FMT_NV12;
        pvImg.size = frameSize(V4L2_PIX_FMT_NV12, pvImg.bpl, pvImg.height);
        pvImg.owner = this;
        mCallbacks->allocateMemory(&pvImg, pvImg.size);
        if (pvImg.dataPtr == NULL) {
            ALOGE("Failed to allocate panorama snapshot memory.");
            return NO_MEMORY;
        }

        float thumbRatio = (float) mThumbnailWidth / mThumbnailHeight;
        float imgRatio = (float) img.width / img.height;
        int startPixel = 0; // used to skip startPixel amount of left side of image
        int skipLinesTop = 0; // used to skip top of the image
        int skipLinesBottom = 0; // used to skip bottom of the image
        int srcWidth, srcHeight;

        if (imgRatio > thumbRatio) {
            fullHeightSrcForThumb(img, srcWidth, srcHeight, startPixel);
        } else if (1 / imgRatio > thumbRatio) {
            fullWidthSrcForThumb(img, srcWidth, srcHeight, skipLinesTop, skipLinesBottom);
        } else {
            // image is rather square, choose method based on which thumbnail dimension is bigger
            if (mThumbnailWidth > mThumbnailHeight) {
                fullWidthSrcForThumb(img, srcWidth, srcHeight, skipLinesTop, skipLinesBottom);
            } else {
                fullHeightSrcForThumb(img, srcWidth, srcHeight, startPixel);
            }
        }

        ImageScaler::downScaleImage(((char *)img.dataPtr) + startPixel, pvImg.dataPtr, mThumbnailWidth,
                mThumbnailHeight, mThumbnailWidth, srcWidth, srcHeight, img.bpl, V4L2_PIX_FMT_NV12,
                skipLinesTop, skipLinesBottom);

        mPanoramaCallback->panoramaFinalized(&img, &pvImg);
    } else
        mPanoramaCallback->panoramaFinalized(&img, NULL);

    if (mState == PANORAMA_DETECTING_OVERLAP || mState == PANORAMA_WAITING_FOR_SNAPSHOT)
        handleMessageStopPanoramaCapture(); // drops state to PANORAMA_STARTED

    return status;
}

void PanoramaThread::setThumbnailSize(int width, int height)
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_THUMBNAILSIZE;
    msg.data.thumbnail.width = width;
    msg.data.thumbnail.height = height;
    mMessageQueue.send(&msg);
}

status_t PanoramaThread::handleMessageThumbnailSize(const MessageThumbnailSize &size)
{
    LOG1("@%s", __FUNCTION__);
    mThumbnailWidth = size.width;
    mThumbnailHeight = size.height;
    return OK;
}

/**
 * returnBuffer is used for returning the finalized buffer after jpeg has been delivered
 */
void PanoramaThread::returnBuffer(AtomBuffer *atomBuffer) {
    LOG1("@%s", __FUNCTION__);
    // for postview buffer type we release the memory we allocated
    if (atomBuffer->buff) {
        atomBuffer->buff->release(atomBuffer->buff);
        atomBuffer->buff = NULL;
    }
    // for panorama buffer type dataPtr was overwritten with ia_frame data
    // panorama engine releases its memory either at reinit (handleMessageStartPanoramaCapture)
    // or uninit (handleMessageStopPanorama)
}

void PanoramaThread::sendFrame(AtomBuffer &buf)
{
    LOG2("@%s", __FUNCTION__);
    ia_frame frame;
    frame.data = (unsigned char*) buf.dataPtr;
    frame.width = buf.width;
    frame.stride = buf.bpl;
    frame.height = buf.height;
    frame.size = buf.size;
    if (AtomCP::setIaFrameFormat(&frame, buf.fourcc) != NO_ERROR) {
        ALOGE("@%s: setting ia_frame format failed", __FUNCTION__);
    }
    Message msg;
    msg.id = MESSAGE_ID_FRAME;
    msg.data.frame.frame = frame;
    mMessageQueue.send(&msg, MESSAGE_ID_FRAME);
}

status_t PanoramaThread::handleFrame(MessageFrame &frame)
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    mPreviewWidth = frame.frame.width;
    mPreviewHeight = frame.frame.height;
    if (mState == PANORAMA_DETECTING_OVERLAP) {
        if (detectOverlap(&frame.frame)) {
            mState = PANORAMA_WAITING_FOR_SNAPSHOT;
            mPanoramaCallback->panoramaCaptureTrigger();
        }
    }
    mMessageQueue.reply(MESSAGE_ID_FRAME, status);
    return status;
}

PanoramaState PanoramaThread::getState(void)
{
    return mState;
}

status_t PanoramaThread::handleStitch(const MessageStitch &stitch)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    AtomBuffer postviewBuf(AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_POSTVIEW));

    int ret = mPanoramaStitchThread->stitch(mContext, stitch.img, stitch.stitchId, mCameraId);

    if (ret < 0) {
        ALOGE("ia_panorama_stitch failed, error = %d", ret);
        return UNKNOWN_ERROR;
    }

    mPanoramaTotalCount++;

    // convert displacement to reflect PV image size
    camera_panorama_metadata metadata = mCurrentMetadata;
    if (mPreviewWidth != 0 && mPreviewHeight != 0)  {
        metadata.horizontal_displacement = roundf((float) metadata.horizontal_displacement / mPreviewWidth * stitch.pv.width);
        metadata.vertical_displacement = roundf((float) metadata.vertical_displacement / mPreviewHeight * stitch.pv.height);
    }
    metadata.finalization_started = (mPanoramaTotalCount == mPanoramaMaxSnapshotCount);

    // allocate memory for the live preview callback.
    mCallbacks->allocateMemory(&postviewBuf, stitch.pv.size + sizeof(camera_panorama_metadata));
    if (postviewBuf.dataPtr == NULL) {
        ALOGE("fatal - out of memory for live preview callback");
        status =  NO_MEMORY;
    } else {
        // space for the metadata is reserved in the beginning of the buffer, copy it there
        memcpy(postviewBuf.dataPtr, &metadata, sizeof(camera_panorama_metadata));
        // copy PV image
        memcpy((char *)postviewBuf.dataPtr + sizeof(camera_panorama_metadata), stitch.pv.dataPtr, stitch.pv.size);

        // set rest of PV fields
        postviewBuf.width = stitch.pv.width;
        postviewBuf.height = stitch.pv.height;
        postviewBuf.size = stitch.pv.size;
        postviewBuf.bpl = stitch.pv.bpl;

        mCallbacksThread->panoramaSnapshot(postviewBuf);
        postviewBuf.buff = NULL; // callbacks thread responsible memory release
        postviewBuf.dataPtr = NULL;
    }

    //panorama engine resets displacement values after stitching, so we reset the current values here, too
    mCurrentMetadata.horizontal_displacement = 0;
    mCurrentMetadata.vertical_displacement = 0;

    mState = PANORAMA_DETECTING_OVERLAP;
    if (mPanoramaTotalCount == mPanoramaMaxSnapshotCount) {
        finalize();
    }

    mMessageQueue.reply(MESSAGE_ID_STITCH, status);

    return status;
}

bool PanoramaThread::threadLoop()
{
    LOG2("@%s", __FUNCTION__);
    mThreadRunning = true;
    while(mThreadRunning)
        waitForAndExecuteMessage();

    return false;
}

PanoramaThread::PanoramaStitchThread::PanoramaStitchThread(Callbacks *callbacks) :
     mMessageQueue("PanoramaStitch", (int) MESSAGE_ID_MAX)
    ,mCallbacks(callbacks)
{
    LOG1("@%s", __FUNCTION__);
}

bool PanoramaThread::PanoramaStitchThread::threadLoop()
{
    LOG2("@%s", __FUNCTION__);
    mThreadRunning = true;
    while(mThreadRunning)
        waitForAndExecuteMessage();

    return false;
}

status_t PanoramaThread::PanoramaStitchThread::flush()
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_FLUSH;
    return mMessageQueue.send(&msg, MESSAGE_ID_FLUSH);
}

status_t PanoramaThread::PanoramaStitchThread::cancel(ia_panorama_state* mContext)
{
    LOG1("@%s", __FUNCTION__);
    Vector<Message> stitches;
    // remove messages from queue
    mMessageQueue.remove(MESSAGE_ID_STITCH, &stitches);

    // release memory in messages
    Vector<Message>::iterator it;
    for (it = stitches.begin(); it != stitches.end(); ++it) {
       camera_memory_t* b = it->data.stitch.img.buff;
       b->release(b);
       b = NULL;
    }

    // cancel last stitch
    ia_panorama_cancel_stitching(mContext);

    // flush (releases memory from last stitch, too)
    return flush();
}

status_t PanoramaThread::PanoramaStitchThread::requestExitAndWait()
{
    LOG2("@%s", __FUNCTION__);

    Message msg;
    msg.id = MESSAGE_ID_EXIT;
    // tell thread to exit
    // send message asynchronously
    mMessageQueue.send(&msg);

    // propagate call to base class
    return Thread::requestExitAndWait();
}

status_t PanoramaThread::PanoramaStitchThread::stitch(ia_panorama_state* mContext, AtomBuffer frame, int stitchId, int cameraId)
{
    LOG1("@%s", __FUNCTION__);

    AtomBuffer copy = frame;
    size_t size = frameSize(V4L2_PIX_FMT_NV12, frame.bpl, frame.height);

    int retryTimeMillis = 5000;
    int retrySleepMillis = 50;

    do {
        mCallbacks->allocateMemory(&copy, size);
        if (!copy.dataPtr) {
            ALOGW("Failed to allocate panorama snapshot memory, sleeping %d milliseconds and retrying!", retrySleepMillis);
            usleep(1000 * retrySleepMillis);
            retryTimeMillis -= retrySleepMillis;
        }
    } while (!copy.dataPtr && retryTimeMillis);

    if (!copy.dataPtr) {
        // could not allocate at all
        ALOGE("Failed to allocate panorama snapshot memory - aborting!");
        // since capturing and stitching is done without user interaction,
        // and the device is unable to release resources, it is time to give up
        abort();
    }

    memcpy(copy.dataPtr, frame.dataPtr, size);

    Message msg;
    msg.id = MESSAGE_ID_STITCH;
    msg.data.stitch.img = copy;
    msg.data.stitch.mContext = mContext;
    msg.data.stitch.stitchId = stitchId;
    mMessageQueue.send(&msg);
    return OK;
}

status_t PanoramaThread::PanoramaStitchThread::handleMessageStitch(MessageStitch &stitch)
{
    LOG1("@%s", __FUNCTION__);
    int stitchId = stitch.stitchId;

    ia_frame iaFrame;
    iaFrame.data = stitch.img.dataPtr;
    iaFrame.size = stitch.img.size;
    iaFrame.width = stitch.img.width;
    iaFrame.height = stitch.img.height;
    iaFrame.stride = stitch.img.bpl;
    iaFrame.format = ia_frame_format_nv12;

    if (iaFrame.stride == 0) {
        ALOGW("panorama stitch hack - snapshot frame stride zero, replacing with width %d", iaFrame.width);
        iaFrame.stride = iaFrame.width;
    }
    int ret = OK;

    ret = ia_panorama_stitch(stitch.mContext, &iaFrame, stitchId);

    stitch.img.buff->release(stitch.img.buff);
    stitch.img.buff = NULL;

    if (ret >= 0)
        return OK;
    else
        return ret;
}

status_t PanoramaThread::PanoramaStitchThread::handleExit()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    mThreadRunning = false;
    return status;
}

status_t PanoramaThread::PanoramaStitchThread::handleFlush()
{
    LOG1("@%s", __FUNCTION__);
    // this intentionally does nothing. When this returns, all previously queued stitches are done.
    mMessageQueue.reply(MESSAGE_ID_FLUSH, OK);
    return OK;
}


status_t PanoramaThread::PanoramaStitchThread::waitForAndExecuteMessage()
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    Message msg;
    mMessageQueue.receive(&msg);

    switch (msg.id)
    {
        case MESSAGE_ID_STITCH:
            status = handleMessageStitch(msg.data.stitch);
            break;
        case MESSAGE_ID_EXIT:
            status = handleExit();
            break;
        case MESSAGE_ID_FLUSH:
            status = handleFlush();
            break;
        default:
            status = INVALID_OPERATION;
            break;
    }
    if (status != NO_ERROR) {
        ALOGE("operation failed, ID = %d, status = %d", msg.id, status);
    }
    return status;
}

status_t PanoramaThread::waitForAndExecuteMessage()
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    Message msg;
    mMessageQueue.receive(&msg);

    switch (msg.id)
    {
        case MESSAGE_ID_STITCH:
            status = handleStitch(msg.data.stitch);
            break;
        case MESSAGE_ID_EXIT:
            status = handleExit();
            break;
        case MESSAGE_ID_FRAME:
            status = handleFrame(msg.data.frame);
            break;
        case MESSAGE_ID_FINALIZE:
            status = handleMessageFinalize();
            break;
        case MESSAGE_ID_START_PANORAMA:
            status = handleMessageStartPanorama();
            break;
        case MESSAGE_ID_STOP_PANORAMA:
            status = handleMessageStopPanorama(msg.data.stop);
            break;
        case MESSAGE_ID_START_PANORAMA_CAPTURE:
            status = handleMessageStartPanoramaCapture();
            break;
        case MESSAGE_ID_STOP_PANORAMA_CAPTURE:
            status = handleMessageStopPanoramaCapture();
            break;
        case MESSAGE_ID_THUMBNAILSIZE:
            status = handleMessageThumbnailSize(msg.data.thumbnail);
            break;
        case MESSAGE_ID_FLUSH:
            status = handleMessageFlush();
            break;
        default:
            status = INVALID_OPERATION;
            break;
    }
    if (status != NO_ERROR) {
        ALOGE("operation failed, ID = %d, status = %d", msg.id, status);
    }
    return status;
}

status_t PanoramaThread::requestExitAndWait()
{
    LOG2("@%s", __FUNCTION__);
    // first stop synchronously, it cleans up panorama engine etc
    stopPanorama(true);

    Message msg;
    msg.id = MESSAGE_ID_EXIT;
    // tell thread to exit
    // send message asynchronously
    mMessageQueue.send(&msg);

    // propagate call to base class
    return Thread::requestExitAndWait();
}

status_t PanoramaThread::handleExit()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    mThreadRunning = false;
    return status;
}

#endif // ENABLE_INTEL_EXTRAS

}; // namespace android

