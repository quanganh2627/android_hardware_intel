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
#include "Callbacks.h"
#include "ColorConverter.h"
#include "DumpImage.h"

namespace android {

VideoThread::VideoThread() :
    Thread(true) // callbacks may call into java
    ,mMessageQueue("VideoThread", MESSAGE_ID_MAX)
    ,mThreadRunning(false)
    ,mCallbacks(NULL)
    ,mInputFormat(V4L2_PIX_FMT_NV21)
    ,mOutputFormat(V4L2_PIX_FMT_NV21)
    ,mWidth(640)  // VGA
    ,mHeight(480) // VGA
    ,mVaConvertor(new VAConvertor())
{
    LOG1("@%s", __FUNCTION__);
}

VideoThread::~VideoThread()
{
    LOG1("@%s", __FUNCTION__);
    if(mVaConvertor !=NULL)
       delete mVaConvertor;
    if (mCallbacks.get())
        mCallbacks.clear();
}

status_t VideoThread::setConfig(int inputFormat, int outputFormat, int width, int height)
{
    mInputFormat = inputFormat;
    mOutputFormat = outputFormat;
    mWidth = width;
    mHeight = height;

    return NO_ERROR;
}

status_t VideoThread::video(CameraBuffer *buff, CameraBuffer *interbuff,nsecs_t timestamp)
{
    LOG2("@%s", __FUNCTION__);
    Message msg;
    status_t ret = INVALID_OPERATION;
    msg.id = MESSAGE_ID_VIDEO;
    msg.data.video.yuv422hbuff= buff;
    msg.data.video.nv12buff= interbuff;
    msg.data.video.timestamp = timestamp;
    if(buff != 0)
       buff->incrementProcessor();
    if(interbuff !=0)
       interbuff->incrementProcessor();
    if ((ret = mMessageQueue.send(&msg)) != NO_ERROR) {
       if(buff != 0)
          buff->decrementProcessor();
       if(interbuff !=0)
          interbuff->decrementProcessor();
    }
    return ret;
}

status_t VideoThread::flushBuffers()
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_FLUSH;
    mMessageQueue.remove(MESSAGE_ID_VIDEO);
    return mMessageQueue.send(&msg, MESSAGE_ID_FLUSH);
}

status_t VideoThread::handleMessageExit()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    mThreadRunning = false;
    return status;
}

status_t VideoThread::handleMessageVideo(MessageVideo *msg)
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    if((msg->yuv422hbuff == NULL) || (msg->nv12buff == NULL))
    {
        ALOGE("yuv422hbuff or nv12buff is NULL!");
        return UNKNOWN_ERROR;
    }
    mVaConvertor->VPPBitBlit(msg->yuv422hbuff->GetRenderTargetHandle(),msg->nv12buff->GetRenderTargetHandle());
    mCallbacks->videoFrameDone(msg->nv12buff, msg->timestamp);
    if (msg->yuv422hbuff != 0)
        msg->yuv422hbuff->decrementProcessor();
    if (msg->nv12buff != 0)
        msg->nv12buff->decrementProcessor();

    return status;
}

status_t VideoThread::handleMessageFlush()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    if(mVaConvertor)
       mVaConvertor->stop();

    mMessageQueue.reply(MESSAGE_ID_FLUSH, status);
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
