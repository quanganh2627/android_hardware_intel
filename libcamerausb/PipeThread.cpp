/*
 * Copyright (C) 2012 The Android Open Source Project
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
#define LOG_TAG "Camera_PipeThread"

#include "PipeThread.h"
#include "ColorConverter.h"
#include "LogHelper.h"
#include "VideoThread.h"
#include "PreviewThread.h"
#include "VAConvertor.h"
#include "DumpImage.h"

namespace android {

PipeThread::PipeThread() :
    Thread(true) // callbacks may call into java
    ,mInputFormat(0)
    ,mOutputFormat(0)
    ,mWidth(0)
    ,mHeight(0)
    ,mPreviewThread(NULL)
    ,mVideoThread(NULL)
    ,mMessageQueue("PipeThread", MESSAGE_ID_MAX)
    ,mThreadRunning(false)
{
    LOG1("@%s", __FUNCTION__);
}

PipeThread::~PipeThread()
{
    LOG1("@%s", __FUNCTION__);
    if (mPreviewThread.get())
        mPreviewThread.clear();
    if (mVideoThread.get())
        mVideoThread.clear();
}

void PipeThread::setThreads(sp<PreviewThread> &previewThread, sp<VideoThread> &videoThread)
{
    mPreviewThread = previewThread;
    mVideoThread = videoThread;
}

void PipeThread::setConfig(int inputFormat, int outputFormat, int width, int height)
{
    mInputFormat = inputFormat;
    mOutputFormat = outputFormat;
    mWidth = width;
    mHeight = height;
}

status_t PipeThread::preview(CameraBuffer *input, CameraBuffer *output,CameraBuffer *midConvert)
{
    LOG2("@%s", __FUNCTION__);
    Message msg;
    status_t ret = INVALID_OPERATION;
    msg.id = MESSAGE_ID_PREVIEW;
    msg.data.preview.input = input;
    msg.data.preview.output = output;
    msg.data.preview.midConvert = midConvert;
    if (input != 0)
        input->incrementProcessor();
    if (output != 0)
        output->incrementProcessor();
    if ((ret = mMessageQueue.send(&msg)) != NO_ERROR) {
        if (input != 0)
            input->decrementProcessor();
        if (output != 0)
            output->decrementProcessor();
    }
    return ret;
}

status_t PipeThread::previewVideo(CameraBuffer *input, CameraBuffer *output,CameraBuffer *toAndroid,CameraBuffer *midConvert, nsecs_t timestamp)
{
    LOG2("@%s", __FUNCTION__);
    Message msg;
    status_t ret = INVALID_OPERATION;
    msg.id = MESSAGE_ID_PREVIEW_VIDEO;
    msg.data.previewVideo.input = input;
    msg.data.previewVideo.output = output;
    msg.data.previewVideo.toAndroid = toAndroid;
    msg.data.previewVideo.midConvert = midConvert;
    msg.data.previewVideo.timestamp = timestamp;
    if (input != 0)
        input->incrementProcessor();
    if (output != 0)
        output->incrementProcessor();
    if (toAndroid != 0)
        toAndroid->incrementProcessor();
    if ((ret = mMessageQueue.send(&msg)) != NO_ERROR) {
        if (input != 0)
            input->decrementProcessor();
        if (output != 0)
            output->decrementProcessor();
        if (toAndroid != 0)
            toAndroid->decrementProcessor();
    }
    return ret;
}

status_t PipeThread::flushBuffers()
{
    LOG2("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_FLUSH;
    mMessageQueue.remove(MESSAGE_ID_PREVIEW);
    mMessageQueue.remove(MESSAGE_ID_PREVIEW_VIDEO);
    return mMessageQueue.send(&msg, MESSAGE_ID_FLUSH);
}

status_t PipeThread::handleMessageExit()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    mThreadRunning = false;
    return status;
}

status_t PipeThread::handleMessagePreview(MessagePreview *msg)
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    CameraBuffer *previewIn = msg->input;
    CameraBuffer *previewOut = msg->output;
    CameraBuffer *midConvert = msg->midConvert;
    status = mPreviewThread->preview(previewIn, previewOut,midConvert);
    if (status != NO_ERROR) {
        ALOGE("failed to send preview buffer");
    }
    msg->input->decrementProcessor();
    msg->output->decrementProcessor();
    return status;
}

status_t PipeThread::handleMessagePreviewVideo(MessagePreviewVideo *msg)
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    CameraBuffer *previewIn = msg->input;//yuv422h
    CameraBuffer *previewOut = msg->toAndroid;//memory heap
    CameraBuffer *midConvert = msg->midConvert;
    CameraBuffer *video = msg->output;

    status = mPreviewThread->preview(previewIn, previewOut,midConvert);
    if (status == NO_ERROR) {
        status = mVideoThread->video(previewIn,video,msg->timestamp);
        if (status != NO_ERROR) {
            ALOGE("failed to send preview buffer");
        }
    } else {
        ALOGE("failed to send preview buffer");
    }
    //we are done with the buffer
    msg->input->decrementProcessor();
    msg->output->decrementProcessor();
    msg->toAndroid->decrementProcessor();
    return status;
}

status_t PipeThread::handleMessageFlush()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    mMessageQueue.reply(MESSAGE_ID_FLUSH, status);
    return status;
}

status_t PipeThread::waitForAndExecuteMessage()
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
            status = handleMessagePreview(&msg.data.preview);
            break;

        case MESSAGE_ID_PREVIEW_VIDEO:
            status = handleMessagePreviewVideo(&msg.data.previewVideo);
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

bool PipeThread::threadLoop()
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    mThreadRunning = true;
    while (mThreadRunning)
        status = waitForAndExecuteMessage();

    return status;
}

status_t PipeThread::requestExitAndWait()
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
