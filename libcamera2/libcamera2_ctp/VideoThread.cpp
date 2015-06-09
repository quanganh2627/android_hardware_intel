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

namespace android {

VideoThread::VideoThread(sp<CallbacksThread> callbacksThread) :
    Thread(true) // callbacks may call into java
    ,mMessageQueue("VideoThread", MESSAGE_ID_MAX)
    ,mThreadRunning(false)
    ,mCallbacksThread(callbacksThread)
    ,mSlowMotionRate(1)
    ,mFirstFrameTimestamp(0)
{
    LOG1("@%s", __FUNCTION__);
#ifdef GRAPHIC_IS_GEN
    mVpp = new VideoVPPBase();
    if (!mVpp) {
        ALOGE("Fail to construct VPP");
    }
#endif
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
}

status_t VideoThread::video(AtomBuffer *buff)
{
    LOG2("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_VIDEO;
    msg.data.video.buff = *buff;
    return mMessageQueue.send(&msg);
}

status_t VideoThread::flushBuffers()
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_FLUSH;
    mMessageQueue.remove(MESSAGE_ID_VIDEO);
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

status_t VideoThread::handleMessageVideo(MessageVideo *msg)
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    nsecs_t timestamp = (msg->buff.capture_timestamp.tv_sec)*1000000000LL
                        + (msg->buff.capture_timestamp.tv_usec)*1000LL;
    if(mSlowMotionRate > 1)
    {
        if(mFirstFrameTimestamp == 0)
            mFirstFrameTimestamp = timestamp;
        timestamp = (timestamp - mFirstFrameTimestamp) * mSlowMotionRate + mFirstFrameTimestamp;
    }

    if (convertNV12Linear2Tiled(msg->buff)) {
        // Print err and do nothing here
        ALOGE("Fail to convertNV12Linear2Tiled");
    }

    mCallbacksThread->videoFrameDone(&msg->buff, timestamp);

    return status;
}

status_t VideoThread::handleMessageFlush()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    mFirstFrameTimestamp = 0;
#ifdef GRAPHIC_IS_GEN
    // vpp also needs to flush the buffers that were registered internally, stop will do it.
    if (mVpp)
        mVpp->stop();
#endif
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

        case MESSAGE_ID_VIDEO:
            status = handleMessageVideo(&msg.data.video);
            break;

        case MESSAGE_ID_FLUSH:
            status = handleMessageFlush();
            break;

        case MESSAGE_ID_SET_SLOWMOTION_RATE:
            status = handleMessageSetSlowMotionRate(&msg.data.setSlowMotionRate);
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

} // namespace android
