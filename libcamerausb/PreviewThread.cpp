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
#include "ColorConverter.h"
#include <ui/GraphicBuffer.h>
#include <ui/GraphicBufferMapper.h>
#include "CameraCommon.h"
#include "DumpImage.h"


namespace android {

PreviewThread::PreviewThread() :
    Thread(true) // callbacks may call into java
    ,mMessageQueue("PreviewThread", (int) MESSAGE_ID_MAX)
    ,mThreadRunning(false)
    ,mDebugFPS(new DebugFrameRate())
    ,mCallbacks(NULL)
    ,mPreviewWindow(NULL)
    ,mPreviewWidth(640)
    ,mPreviewHeight(480)
    ,mInputFormat(0)
    ,mOutputFormat(0)
    ,mGFXHALPixelFormat(HAL_PIXEL_FORMAT_YCbCr_422_I)
    ,mVaConvertor(new VAConvertor())
{
    LOG1("@%s", __FUNCTION__);
}

PreviewThread::~PreviewThread()
{
    LOG1("@%s", __FUNCTION__);
    mDebugFPS.clear();
    if(mVaConvertor !=NULL)
       delete mVaConvertor;
    if (mCallbacks.get())
        mCallbacks.clear();
}

status_t PreviewThread::setPreviewWindow(struct preview_stream_ops *window)
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_SET_PREVIEW_WINDOW;
    msg.data.setPreviewWindow.window = window;
    return mMessageQueue.send(&msg);
}
status_t PreviewThread::setPreviewConfig(int preview_width, int preview_height,
        int input_format, int output_format)
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_SET_PREVIEW_CONFIG;
    msg.data.setPreviewConfig.width = preview_width;
    msg.data.setPreviewConfig.height = preview_height;
    msg.data.setPreviewConfig.inputFormat = input_format;
    msg.data.setPreviewConfig.outputFormat = output_format;
    return mMessageQueue.send(&msg);
}

status_t PreviewThread::preview(CameraBuffer *inputBuff, CameraBuffer *outputBuff,CameraBuffer *midConvert)
{
    LOG2("@%s", __FUNCTION__);
    Message msg;
    status_t ret = INVALID_OPERATION;
    msg.id = MESSAGE_ID_PREVIEW;
    msg.data.preview.inputBuff = inputBuff;
    msg.data.preview.outputBuff = outputBuff;
    msg.data.preview.midConvert = midConvert;
    if (inputBuff != 0)
        inputBuff->incrementProcessor();
    if (outputBuff != 0)
        outputBuff->incrementProcessor();
    if ((ret = mMessageQueue.send(&msg)) != NO_ERROR) {
        if (inputBuff != 0)
            inputBuff->decrementProcessor();
        if (outputBuff != 0)
            outputBuff->decrementProcessor();
    }
    return ret;
}

status_t PreviewThread::flushBuffers()
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_FLUSH;
    mMessageQueue.remove(MESSAGE_ID_PREVIEW);
    return mMessageQueue.send(&msg, MESSAGE_ID_FLUSH);
}

status_t PreviewThread::handleMessageExit()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    mThreadRunning = false;
    return status;
}

status_t PreviewThread::handleMessagePreview(MessagePreview *msg)
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    LOG2("Buff: id = %d, data = %p",
            msg->inputBuff->getID(),
            msg->inputBuff->getData());

    if (mPreviewWindow != 0) {
        buffer_handle_t *buf = NULL;
        int err;
        int stride;
        if ((err = mPreviewWindow->dequeue_buffer(mPreviewWindow, &buf, &stride)) != 0) {
            ALOGE("Surface::dequeueBuffer returned error %d", err);
        } else {

            if (mPreviewWindow->lock_buffer(mPreviewWindow, buf) != NO_ERROR) {
                ALOGE("Failed to lock preview buffer!");
                mPreviewWindow->cancel_buffer(mPreviewWindow, buf);
                status = NO_MEMORY;
                goto exit;
            }
            GraphicBufferMapper &mapper = GraphicBufferMapper::get();
            const Rect bounds(mPreviewWidth, mPreviewHeight);
            void *dst;

            if (mapper.lock(*buf, GRALLOC_USAGE_SW_WRITE_OFTEN, bounds, &dst) != NO_ERROR) {
                ALOGE("Failed to lock GraphicBufferMapper!");
                mPreviewWindow->cancel_buffer(mPreviewWindow, buf);
                status = NO_MEMORY;
                goto exit;
            }
            LOG1("Preview Color Conversion to YUY2, stride: %d height: %d", stride, mPreviewHeight);
            RenderTarget previewRT;
            memset((void*)&previewRT,0,sizeof(RenderTarget));
            mVaConvertor->ConfigBuffer(&previewRT,*buf,mPreviewWidth,mPreviewHeight,mGFXHALPixelFormat);
            mVaConvertor->VPPBitBlit(msg->inputBuff->GetRenderTargetHandle(),&previewRT);
            if ((err = mPreviewWindow->enqueue_buffer(mPreviewWindow, buf)) != 0) {
                ALOGE("Surface::queueBuffer returned error %d", err);
            }
            mapper.unlock(*buf);
        }
    }

    mDebugFPS->update(); // update fps counter
exit:

    if (mCallbacks->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME)&& (msg->outputBuff))
    {
        void *srcaddr[3];
        int size = 0;
        int alignHeight = 0;

        alignHeight = msg->midConvert->GetRenderTargetHandle()->height;

        if(mOutputFormat == V4L2_PIX_FMT_NV21)
        {
           //currently, vpp don't support colorconvert from yuv422h to nv21, so convert to yv12 with vpp, and then convert from yv12 to NV21
           mVaConvertor->VPPBitBlit(msg->inputBuff->GetRenderTargetHandle(),msg->midConvert->GetRenderTargetHandle());
           status = msg->midConvert->LockGrallocData((void**)&srcaddr,&size);
           if (status != NO_ERROR) {
              LOGE("lock data failed,ret=%d, in line %d",status, __LINE__);
           }
           colorConvertwithStride(V4L2_PIX_FMT_YUV420,mOutputFormat,msg->midConvert->GetGraStride(),mPreviewWidth,alignHeight,mPreviewHeight,srcaddr[0],msg->outputBuff->getData());
           msg->midConvert->UnLockGrallocData();
        }
        else
        {
           mVaConvertor->VPPBitBlit(msg->inputBuff->GetRenderTargetHandle(),msg->midConvert->GetRenderTargetHandle());
           status  = msg->midConvert->LockGrallocData((void**)&srcaddr,&size);
           if (status != NO_ERROR) {
               LOGE("lock data failed,ret=%d, in line %d",status, __LINE__);
           }
           colorConvertwithStride(mOutputFormat,mOutputFormat,msg->midConvert->GetGraStride(),mPreviewWidth,alignHeight,mPreviewHeight,srcaddr[0],msg->outputBuff->getData());
           msg->midConvert->UnLockGrallocData();
        }
        mCallbacks->previewFrameDone(msg->outputBuff);
    }
    if(msg->inputBuff)
    {
       msg->inputBuff->decrementProcessor();
    }
    if (msg->outputBuff)
        msg->outputBuff->decrementProcessor();
    return status;
}



status_t PreviewThread::handleMessageSetPreviewWindow(MessageSetPreviewWindow *msg)
{
    LOG1("@%s: window = %p", __FUNCTION__, msg->window);

    mPreviewWindow = msg->window;

    if (mPreviewWindow != NULL) {
        LOG1("Setting new preview window %p", mPreviewWindow);
        int previewWidthPadded =  paddingWidth(V4L2_PIX_FMT_YUYV,mPreviewWidth,mPreviewHeight);
        mPreviewWindow->set_usage(mPreviewWindow, GRALLOC_USAGE_SW_WRITE_OFTEN);
        mPreviewWindow->set_buffer_count(mPreviewWindow, 4);
        mPreviewWindow->set_buffers_geometry(
                mPreviewWindow,
                previewWidthPadded,
                mPreviewHeight,
                mGFXHALPixelFormat);
    }

    return NO_ERROR;
}

status_t PreviewThread::handleMessageSetPreviewConfig(MessageSetPreviewConfig *msg)
{
    LOG1("@%s: width = %d, height = %d", __FUNCTION__,
         msg->width, msg->height);

    if ((msg->width != 0 && msg->height != 0) &&
            (mPreviewWidth != msg->width || mPreviewHeight != msg->height)) {
        LOG1("Setting old preview size: %dx%d", mPreviewWidth, mPreviewHeight);
        if (mPreviewWindow != NULL) {
            int previewWidthPadded = paddingWidth(V4L2_PIX_FMT_YUYV, msg->width, msg->height);
            // if preview size changed, update the preview window
            mPreviewWindow->set_buffers_geometry(
                    mPreviewWindow,
                    previewWidthPadded,
                    msg->height,
                    mGFXHALPixelFormat);
        }
        mPreviewWidth = msg->width;
        mPreviewHeight = msg->height;
    }

    mInputFormat = msg->inputFormat;
    mOutputFormat = msg->outputFormat;

    return NO_ERROR;
}

status_t PreviewThread::handleMessageFlush()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    if(mVaConvertor)
       mVaConvertor->stop();

    mMessageQueue.reply(MESSAGE_ID_FLUSH, status);
    return status;
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
            status = handleMessagePreview(&msg.data.preview);
            break;

        case MESSAGE_ID_SET_PREVIEW_WINDOW:
            status = handleMessageSetPreviewWindow(&msg.data.setPreviewWindow);
            break;

        case MESSAGE_ID_SET_PREVIEW_CONFIG:
            status = handleMessageSetPreviewConfig(&msg.data.setPreviewConfig);
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

    return status;
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

} // namespace android
