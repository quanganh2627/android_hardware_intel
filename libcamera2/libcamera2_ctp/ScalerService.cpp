/*
 * Copyright (C) 2011 The Android Open Source Project
 * Copyright (C) 2011,2012,2013 Intel Corporation
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

#define LOG_TAG "Camera_ScalerService"

#include "ScalerService.h"
#include "LogHelper.h"
#include <ui/GraphicBuffer.h>
#include <ui/GraphicBufferMapper.h>
namespace android {

ScalerService::ScalerService() :
    Thread(false)
    ,mMessageQueue("ScalerService", MESSAGE_ID_MAX)
    ,mThreadRunning(false)
    ,mGPUScaler(NULL)
    ,mFrameCounter(0)
{
    LOG1("@%s", __FUNCTION__);
}

ScalerService::~ScalerService()
{
    LOG1("@%s", __FUNCTION__);
    if (mGPUScaler) {
        delete mGPUScaler;
        mGPUScaler = NULL;
    }
}

bool ScalerService::threadLoop()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    mThreadRunning = true;
    while (mThreadRunning) {
        status = waitForAndExecuteMessage();
    }

    return false;
}

status_t ScalerService::scaleAndZoom(const AtomBuffer *input, AtomBuffer *output, float zoomFactor)
{
    LOG2("@%s iw %d ih %d ow %d oh %d", __FUNCTION__, input->width, input->height, output->width, output->height);
    Message msg;
    msg.id = MESSAGE_ID_SCALE_AND_ZOOM;
    msg.data.messageScaleAndZoom.input = input;
    msg.data.messageScaleAndZoom.output = output;
    msg.data.messageScaleAndZoom.zoomFactor = zoomFactor;
    return mMessageQueue.send(&msg, MESSAGE_ID_SCALE_AND_ZOOM);
}

status_t ScalerService::unRegisterBuffer(AtomBuffer &buffer, BufferDirection dir)
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_UNREGISTER_BUFFER;
    msg.data.messageUnregister.buffer = &buffer;
    msg.data.messageUnregister.dir = dir;
    return mMessageQueue.send(&msg, MESSAGE_ID_UNREGISTER_BUFFER);
}

status_t ScalerService::registerBuffer(AtomBuffer &buffer, BufferDirection dir)
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_REGISTER_BUFFER;
    msg.data.messageRegister.buffer = &buffer;
    msg.data.messageRegister.dir = dir;
    return mMessageQueue.send(&msg, MESSAGE_ID_REGISTER_BUFFER);
}

status_t ScalerService::handleMessageRegisterBuffer(MessageRegister &msg)
{
    LOG1("@%s", __FUNCTION__);

    if (mGPUScaler == NULL) {
        mGPUScaler = new GPUScaler();
        mFrameCounter = 0;
    }

    int id = -1;
    GraphicBufferMapper &mapper = GraphicBufferMapper::get();
    mapper.unlock(*msg.buffer->gfxInfo.gfxBufferHandle);  // gpuscaler wants unlocked bufs

    if (msg.dir == SCALER_INPUT) {
        id = mGPUScaler->addInputBuffer(msg.buffer->gfxInfo.gfxBufferHandle, msg.buffer->width, msg.buffer->height, msg.buffer->bpl);
    } else { // BufferDirection == SCALER_OUTPUT
        id = mGPUScaler->addOutputBuffer(msg.buffer->gfxInfo.gfxBufferHandle, msg.buffer->width, msg.buffer->height, msg.buffer->bpl);
    }

    if (id < 0)
        mMessageQueue.reply(MESSAGE_ID_REGISTER_BUFFER, NO_MEMORY);

    msg.buffer->gfxInfo.scalerId = id;
    // restore buffers to the locked state since they enter this function
    // locked
    int lockMode = GRALLOC_USAGE_SW_READ_OFTEN |
                   GRALLOC_USAGE_SW_WRITE_NEVER |
                   GRALLOC_USAGE_HW_COMPOSER;
    const Rect bounds(msg.buffer->width, msg.buffer->height);
    MapperPointer mapperPointer;
    mapper.lock(*msg.buffer->gfxInfo.gfxBufferHandle, lockMode, bounds, &mapperPointer.ptr); // lock
    msg.buffer->gfxInfo.locked = true;

    mMessageQueue.reply(MESSAGE_ID_REGISTER_BUFFER, OK);
    return OK;
}

status_t ScalerService::handleMessageUnregisterBuffer(MessageRegister &msg)
{
    LOG1("@%s", __FUNCTION__);
    if (msg.dir == SCALER_INPUT) {
        mGPUScaler->removeInputBuffer(msg.buffer->gfxInfo.scalerId);
    } else { // BufferDirection == SCALER_OUTPUT
        mGPUScaler->removeOutputBuffer(msg.buffer->gfxInfo.scalerId);
    }
    mMessageQueue.reply(MESSAGE_ID_UNREGISTER_BUFFER, OK);
    return OK;
}

status_t ScalerService::handleMessageScaleAndZoom(MessageScaleAndZoom &msg)
{
    LOG2("@%s", __FUNCTION__);

    if (mGPUScaler == NULL) {
        mGPUScaler = new GPUScaler();
        mFrameCounter = 0;
    }

    mGPUScaler->setZoomFactor(msg.zoomFactor);
    mGPUScaler->processFrame(msg.input->gfxInfo.scalerId, msg.output->gfxInfo.scalerId);

    // handle locking for non shared case (in shared case, PreviewThread handles)
    if (!msg.output->shared) {
        // flushes the buffer from cache

        GraphicBufferMapper &mapper = GraphicBufferMapper::get();
        int lockMode = GRALLOC_USAGE_SW_READ_OFTEN |
                       GRALLOC_USAGE_SW_WRITE_NEVER |
                       GRALLOC_USAGE_HW_COMPOSER;
        const Rect bounds(msg.output->width, msg.output->height);
        MapperPointer mapperPointer;

        mapper.unlock(*msg.output->gfxInfo.gfxBufferHandle);
        mapper.lock(*msg.output->gfxInfo.gfxBufferHandle, lockMode, bounds, &mapperPointer.ptr); // restore to locked state
        msg.output->gfxInfo.locked = true;
    }

    mFrameCounter++;
    mMessageQueue.reply(MESSAGE_ID_SCALE_AND_ZOOM, OK);
    return OK;
}

status_t ScalerService::waitForAndExecuteMessage()
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    Message msg;
    mMessageQueue.receive(&msg);

    switch (msg.id) {
        case MESSAGE_ID_EXIT:
            status = handleMessageExit();
            break;
        case MESSAGE_ID_SCALE_AND_ZOOM:
            status = handleMessageScaleAndZoom(msg.data.messageScaleAndZoom);
            break;
        case MESSAGE_ID_REGISTER_BUFFER:
            status = handleMessageRegisterBuffer(msg.data.messageRegister);
            break;
        case MESSAGE_ID_UNREGISTER_BUFFER:
            status = handleMessageUnregisterBuffer(msg.data.messageUnregister);
            break;
        default:
            status = BAD_VALUE;
            break;
    };
    return status;
}

status_t ScalerService::handleMessageExit()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    mThreadRunning = false;

    if (mGPUScaler) {
        delete mGPUScaler;
        mGPUScaler = NULL;
    }
    LOG1("@%s returning..", __FUNCTION__);
    return status;
}

status_t ScalerService::requestExitAndWait()
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

} /* namespace android */
