/*
 * Copyright (C) 2013 The Android Open Source Project
 * Copyright (c) 2013 Intel Corporation. All Rights Reserved.
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

#define LOG_TAG "Camera_AccManagerThread"
//#define LOG_NDEBUG 0

const unsigned int MAX_NUMBER_ARGUMENT_BUFFERS = 50;
enum {
    STANDALONE_START = 1,
    STANDALONE_WAIT = 2,
    STANDALONE_ABORT = 3,
    STANDALONE_UNLOAD = 4
};

#include "LogHelper.h"
#include "Callbacks.h"
#include "CallbacksThread.h"
#include "AccManagerThread.h"
#include "AtomCommon.h"
#include "ICameraHwControls.h"

namespace android {

AccManagerThread::AccManagerThread(HWControlGroup &hwcg, sp<CallbacksThread> callbacksThread, Callbacks *callbacks, int cameraId) :
    Thread(true) // callbacks may call into java
    ,mMessageQueue("AccManagerThread", (int) MESSAGE_ID_MAX)
    ,mCallbacks(callbacks)
    ,mCallbacksThread(callbacksThread)
    ,mThreadRunning(false)
    ,mFirmwareLoaded(false)
    ,mFirmwareBuffer(NULL)
    ,mFramesGrabbed(0)
    ,mFrameMetadata(NULL)
    ,mCameraId(cameraId)
{
    LOG1("@%s", __FUNCTION__);

    mFrameMetadataBuffer.data = NULL;

    mFrameOne.inUse = false;
    mFrameOne.mem.data = NULL;
    mFrameTwo.inUse = false;
    mFrameTwo.mem.data = NULL;

    // Set up list of argument buffers
    mArgumentBuffers.setCapacity(MAX_NUMBER_ARGUMENT_BUFFERS);

    // Setting ISP handle
    mIspHandle = hwcg.mIspCI;
}

AccManagerThread::~AccManagerThread()
{
    LOG1("@%s", __FUNCTION__);

    // Free metadata buffer
    if (mFrameMetadataBuffer.data != NULL) {
        mFrameMetadataBuffer.release(&mFrameMetadataBuffer);
    }

    // Free argument buffers
    for(unsigned int i = 0; i < mArgumentBuffers.size(); i++) {
        if (mArgumentBuffers[i].mem.data != NULL) {
            mArgumentBuffers[i].mem.release(&mArgumentBuffers.editItemAt(i).mem);
        }
    }

    // Free frame buffers
    if (mFrameOne.mem.data != NULL) {
        mFrameOne.mem.release(&mFrameOne.mem);
    }
    if (mFrameTwo.mem.data != NULL) {
        mFrameTwo.mem.release(&mFrameTwo.mem);
    }
}

int AccManagerThread::getCameraID()
{
    LOG1("@%s", __FUNCTION__);

    return mCameraId;
}

/**
 * override for ICallbackPreview::previewBufferCallback()
 *
 * ControlThread assigns AccManagerThread to PreviewThreads output data callback.
 */
void AccManagerThread::previewBufferCallback(AtomBuffer* frame, ICallbackPreview::CallbackType t)
{
    LOG2("@%s", __FUNCTION__);

    if (t != ICallbackPreview::OUTPUT_WITH_DATA) {
        ALOGE("Unexpected preview buffer callback type!");
        return;
    }

    if (frame == NULL) {
        ALOGW("@%s: NULL AtomBuffer frame", __FUNCTION__);
        return;
    }

    if (!mMessageQueue.isEmpty()) {
        LOG2("@%s: skipping frame", __FUNCTION__);
        frame->owner->returnBuffer(frame);
        return;
    }

    Message msg;
    msg.id = MESSAGE_ID_FRAME;
    msg.data.frame.img = *frame;

    if (mMessageQueue.send(&msg) != NO_ERROR)
        frame->owner->returnBuffer(frame);
}

status_t AccManagerThread::handleMessageFrame(MessageFrame msg)
{
    LOG2("@%s", __FUNCTION__);

    MyAtomBuffer* free;

    if (mFramesGrabbed > 1) {
        LOG2("@%s: skipping frame", __FUNCTION__);
        msg.img.owner->returnBuffer(&msg.img);
    } else {
        LOG2("Grabbed preview buffer of size %d, frameCounter=%d", msg.img.size, msg.img.frameCounter);

        if (mFrameOne.inUse) {
            if (mFrameTwo.inUse) {
                ALOGE("Too many frames grabbed!");
                msg.img.owner->returnBuffer(&msg.img);
                return UNKNOWN_ERROR;
            }
            free = &mFrameTwo;
        } else {
            free = &mFrameOne;
        }

        if (free->mem.data == NULL) {
            LOG1("Allocating buffer for copying preview frames (size=%d)", msg.img.size);
            AtomBuffer tmpBuf;
            mCallbacks->allocateMemory(&tmpBuf, msg.img.size);
            if (!tmpBuf.buff) {
                ALOGE("getting memory failed\n");
                msg.img.owner->returnBuffer(&msg.img);
                return UNKNOWN_ERROR;
            }
            free->mem = *tmpBuf.buff;
        }

        free->inUse = true;
        free->frameCounter = msg.img.frameCounter;
        memcpy(free->mem.data, msg.img.dataPtr, free->mem.size); // Copy frame

        mFramesGrabbed++;

        mFrameMetadata->img_data = NULL; // to be filled in by upper layers
        mFrameMetadata->id = msg.img.id;
        mFrameMetadata->frameCounter = msg.img.frameCounter;
        mFrameMetadata->width = msg.img.width;
        mFrameMetadata->height = msg.img.height;
        mFrameMetadata->fourcc = msg.img.fourcc;
        mFrameMetadata->bpl = msg.img.bpl;
        mFrameMetadata->size = msg.img.size;

        // copy finished, return frame
        msg.img.owner->returnBuffer(&msg.img);

        // notify grabbed frame to upper layers
        mCallbacksThread->accManagerPreviewBuffer(&free->mem);
    }

    return NO_ERROR;
}

status_t AccManagerThread::returnBuffer(int idx)
{
    LOG2("@%s", __FUNCTION__);

    Message msg;
    msg.id = MESSAGE_ID_RETURN_BUFFER;
    msg.data.ret.idx = idx;
    mMessageQueue.send(&msg);

    return NO_ERROR;
}

status_t AccManagerThread::handleMessageReturnBuffer(const MessageReturn& msg)
{
    LOG2("@%s idx=%d", __FUNCTION__, msg.idx);

    if (msg.idx == mFrameOne.frameCounter) {
        mFrameOne.inUse = false;
        mFramesGrabbed--;
    } else if (msg.idx == mFrameTwo.frameCounter) {
        mFrameTwo.inUse = false;
        mFramesGrabbed--;
    } else {
        ALOGE("No frame with that frame counter grabbed: %d", msg.idx);
        return UNKNOWN_ERROR;
    }

    return NO_ERROR;
}

status_t AccManagerThread::alloc(int size)
{
    LOG1("@%s", __FUNCTION__);

    Message msg;
    msg.id = MESSAGE_ID_ALLOC;
    msg.data.alloc.size = size;
    mMessageQueue.send(&msg);

    return NO_ERROR;
}

status_t AccManagerThread::handleMessageAlloc(const MessageAlloc& msg)
{
    LOG1("@%s: size = %d", __FUNCTION__, msg.size);

    if (mArgumentBuffers.size() >= MAX_NUMBER_ARGUMENT_BUFFERS) {
        ALOGE("Cannot allocate more buffers!");
        return UNKNOWN_ERROR;
    }

    AtomBuffer tmpBuf;

    mCallbacks->allocateMemory(&tmpBuf, msg.size);
    if (!tmpBuf.buff) {
        ALOGE("getting memory failed\n");
        return UNKNOWN_ERROR;
    }

    ArgumentBuffer tmpArg;
    tmpArg.mem = *tmpBuf.buff;
    tmpArg.ptr = NULL;
    tmpArg.toBeSend = false;

    unsigned int idx = mArgumentBuffers.add(tmpArg);
    LOG1("Created new buffer of size %d at index %d", tmpBuf.buff->size, idx);

    // notify allocated buffer to upper layers
    mCallbacksThread->accManagerArgumentBuffer(tmpBuf.buff);

    return NO_ERROR;
}

status_t AccManagerThread::free(unsigned int idx)
{
    LOG1("@%s", __FUNCTION__);

    Message msg;
    msg.id = MESSAGE_ID_FREE;
    msg.data.buffer.idx = idx;
    mMessageQueue.send(&msg);

    return NO_ERROR;
}

status_t AccManagerThread::handleMessageFree(const MessageBuffer& msg)
{
    LOG1("@%s: idx = %d", __FUNCTION__, msg.idx);

    if (mArgumentBuffers.size() <= msg.idx) {
        ALOGE("Argument buffer has not been allocated");
        return UNKNOWN_ERROR;
    }

    if (mArgumentBuffers[msg.idx].ptr != NULL) {
        ALOGE("Argument is still mapped!");
        return UNKNOWN_ERROR;
    }

    camera_memory_t tmp = mArgumentBuffers[msg.idx].mem;
    mArgumentBuffers.removeAt(msg.idx);
    tmp.release(&tmp);

    return NO_ERROR;
}

status_t AccManagerThread::map(unsigned int idx)
{
    LOG1("@%s", __FUNCTION__);

    Message msg;
    msg.id = MESSAGE_ID_MAP;
    msg.data.map.idx = idx;
    mMessageQueue.send(&msg);

    return NO_ERROR;
}

status_t AccManagerThread::handleMessageMap(const MessageMap& msg)
{
    LOG1("@%s: idx = %d", __FUNCTION__, msg.idx);

    status_t status = NO_ERROR;

    if (mArgumentBuffers.size() <= msg.idx) {
        ALOGE("Argument buffer has not been allocated");
        return UNKNOWN_ERROR;
    }

    if (mArgumentBuffers[msg.idx].ptr != NULL) {
        ALOGW("Argument is already mapped!");

        // send ISP pointer to upper layers
        mCallbacksThread->accManagerPointer((int) mArgumentBuffers[msg.idx].ptr, msg.idx);
        return NO_ERROR;
    }

    void* ispPtr;

    status = mIspHandle->mapFirmwareArgument(mArgumentBuffers[msg.idx].mem.data,
                                             mArgumentBuffers[msg.idx].mem.size,
                                             (unsigned long*) &ispPtr);

    if (status == NO_ERROR) {
        // store ISP pointer
        mArgumentBuffers.editItemAt(msg.idx).ptr = ispPtr;

        // send ISP pointer to upper layers
        mCallbacksThread->accManagerPointer((int) ispPtr, msg.idx);
    } else {
        ALOGE("Could not map buffer, status: %d", status);
    }

    return status;
}

status_t AccManagerThread::unmap(unsigned int idx)
{
    LOG1("@%s", __FUNCTION__);

    Message msg;
    msg.id = MESSAGE_ID_UNMAP;
    msg.data.map.idx = idx;
    mMessageQueue.send(&msg);

    return NO_ERROR;
}

status_t AccManagerThread::handleMessageUnmap(const MessageMap& msg)
{
    LOG1("@%s: idx = %d", __FUNCTION__, msg.idx);

    status_t status = NO_ERROR;

    if (mArgumentBuffers[msg.idx].ptr == NULL) {
        ALOGE("Argument not mapped!");
        return UNKNOWN_ERROR;
    }

    status = mIspHandle->unmapFirmwareArgument((unsigned long) mArgumentBuffers[msg.idx].ptr,
                                               mArgumentBuffers[msg.idx].mem.size);

    if (status == NO_ERROR) {
        // delete ISP pointer
        mArgumentBuffers.editItemAt(msg.idx).ptr = NULL;
    } else {
        ALOGE("Could not unmap buffer, status: %d", status);
    }

    return status;
}

status_t AccManagerThread::setArgToBeSend(unsigned int idx)
{
    LOG1("@%s", __FUNCTION__);

    Message msg;
    msg.id = MESSAGE_ID_SEND_ARG;
    msg.data.map.idx = idx;
    mMessageQueue.send(&msg);

    return NO_ERROR;
}

status_t AccManagerThread::handleMessageSetArgToBeSend(const MessageMap& msg)
{
    LOG1("@%s: idx = %d", __FUNCTION__, msg.idx);

    if (mArgumentBuffers[msg.idx].ptr == NULL) {
        ALOGE("Argument not mapped!");
        return UNKNOWN_ERROR;
    }

    mArgumentBuffers.editItemAt(msg.idx).toBeSend = true;

    return NO_ERROR;
}

status_t AccManagerThread::sendFirmwareArguments()
{
    LOG1("@%s", __FUNCTION__);

    status_t status = NO_ERROR;

    for(unsigned int i = 0; i < mArgumentBuffers.size(); i++) {
        if (mArgumentBuffers[i].toBeSend)
            status = mIspHandle->setMappedFirmwareArgument(mFirmwareHandle,
                                                           ATOMISP_ACC_MEMORY_DMEM,
                                                           (unsigned long) mArgumentBuffers[i].ptr,
                                                           mArgumentBuffers[i].mem.size);
        if (status != NO_ERROR) {
            ALOGE("Could not sendargument, status:%d", status);
            return status;
        }
    }

    return NO_ERROR;
}

status_t AccManagerThread::configureIspStandalone(int mode)
{
    LOG1("@%s", __FUNCTION__);

    Message msg;
    msg.id = MESSAGE_ID_CONFIGURE_ISP_STANDALONE;
    msg.data.configureIspStandalone.mode = mode;
    mMessageQueue.send(&msg);

    return NO_ERROR;
}

status_t AccManagerThread::handleMessageConfigureIspStandalone(const MessageConfigureIspStandalone& msg)
{
    LOG1("@%s: mode = %d", __FUNCTION__, msg.mode);

    unsigned int fw_handle;

    switch (msg.mode) {
        case STANDALONE_START:
            if (mFirmwareBuffer == NULL) {
                ALOGE("No firmware loaded!");
                return UNKNOWN_ERROR;
            }
            if (mIspHandle->loadAccFirmware(mFirmwareBuffer, mFirmwareBufferSize, &fw_handle) != NO_ERROR) {
                ALOGE("Error loading standalone firmware!");
                return UNKNOWN_ERROR;
            }
            mFirmwareLoaded = true;
            mFirmwareHandle = fw_handle;
            if (sendFirmwareArguments() != NO_ERROR) {
                ALOGE("Error sending arguments to standalone firmware!");
                return UNKNOWN_ERROR;
            }
            if (mIspHandle->startFirmware(mFirmwareHandle) != NO_ERROR) {
                ALOGE("Error starting standalone firmware");
                return UNKNOWN_ERROR;
            }
            break;
        case STANDALONE_WAIT:
            mIspHandle->waitForFirmware(mFirmwareHandle);
            mCallbacksThread->accManagerFinished();
            break;
        case STANDALONE_ABORT:
            if (mIspHandle->abortFirmware(mFirmwareHandle, 0) != NO_ERROR) {
                ALOGE("Could not abort standalone firmware");
                return UNKNOWN_ERROR;
            }
            break;
        case STANDALONE_UNLOAD:
            if (mIspHandle->unloadAccFirmware(mFirmwareHandle) != NO_ERROR) {
                ALOGE("Could not unload standalone firmware");
                return UNKNOWN_ERROR;
            } else {
                mFirmwareLoaded = false;
            }
            break;
        default:
            ALOGE("Invalid operation %d", msg.mode);
            return UNKNOWN_ERROR;
    }

    return NO_ERROR;
}

status_t AccManagerThread::load(unsigned int idx)
{
    LOG1("@%s", __FUNCTION__);

    Message msg;
    msg.id = MESSAGE_ID_LOAD;
    msg.data.fw.idx = idx;
    mMessageQueue.send(&msg);

    return NO_ERROR;
}

status_t AccManagerThread::handleMessageLoad(const MessageFirmware& msg)
{
    LOG1("@%s", __FUNCTION__);

    if (mArgumentBuffers.size() <= msg.idx) {
        ALOGE("Firmware data not in buffer allocated by us!");
        return UNKNOWN_ERROR;
    }

    mFirmwareBuffer = mArgumentBuffers[msg.idx].mem.data;
    mFirmwareBufferSize = mArgumentBuffers[msg.idx].mem.size;

    return NO_ERROR;
}

status_t AccManagerThread::loadIspExtensions()
{
    LOG1("@%s", __FUNCTION__);

    Message msg;
    msg.id = MESSAGE_ID_LOAD_ISP_EXTENSIONS;
    return mMessageQueue.send(&msg, MESSAGE_ID_LOAD_ISP_EXTENSIONS);
}

status_t AccManagerThread::handleMessageLoadIspExtensions()
{
    LOG1("@%s", __FUNCTION__);

    status_t status = NO_ERROR;

    unsigned int fw_handle;

    if (mFirmwareBuffer == NULL) {
        ALOGE("No firmware loaded!");
        status = UNKNOWN_ERROR;
    } else {
        if (mIspHandle->loadAccPipeFirmware(mFirmwareBuffer, mFirmwareBufferSize, &fw_handle) == NO_ERROR) {
            mFirmwareLoaded = true;
            mFirmwareHandle = fw_handle;
            sendFirmwareArguments();
        } else {
            ALOGE("Error loading firmware to pipeline!");
            status = UNKNOWN_ERROR;
        }
    }

    if (mFrameMetadata == NULL) {
        // Set up buffer for sharing the metadata
        AtomBuffer tmpBuf;
        mCallbacks->allocateMemory(&tmpBuf, sizeof(Frame));
        mFrameMetadataBuffer = *tmpBuf.buff;
        mFrameMetadata = (Frame*) mFrameMetadataBuffer.data;
        memset(mFrameMetadata, 0, sizeof(mFrameMetadata));

        // notify buffer for metadata sharing to upper layers
        mCallbacksThread->accManagerMetadataBuffer(tmpBuf.buff);
    }

    mMessageQueue.reply(MESSAGE_ID_LOAD_ISP_EXTENSIONS, status);
    return status;
}

status_t AccManagerThread::unloadIspExtensions()
{
    LOG1("@%s", __FUNCTION__);

    Message msg;
    msg.id = MESSAGE_ID_UNLOAD_ISP_EXTENSIONS;
    return mMessageQueue.send(&msg, MESSAGE_ID_UNLOAD_ISP_EXTENSIONS);
}

status_t AccManagerThread::handleMessageUnloadIspExtensions()
{
    LOG1("@%s", __FUNCTION__);

    status_t status = NO_ERROR;

    if (mFirmwareLoaded) {
        if (mIspHandle->unloadAccFirmware(mFirmwareHandle) == NO_ERROR) {
            mFirmwareLoaded = false;
        } else {
            ALOGE("Error unloading firmware!");
            status = UNKNOWN_ERROR;
        }
    } else {
        ALOGW("No firmware extension loaded!");
    }

    mMessageQueue.reply(MESSAGE_ID_UNLOAD_ISP_EXTENSIONS, status);
    return status;
}

bool AccManagerThread::threadLoop()
{
    LOG2("@%s", __FUNCTION__);

    mThreadRunning = true;
    while(mThreadRunning)
        waitForAndExecuteMessage();

    return false;
}

status_t AccManagerThread::waitForAndExecuteMessage()
{
    LOG2("@%s", __FUNCTION__);

    status_t status = NO_ERROR;
    Message msg;
    mMessageQueue.receive(&msg);

    switch (msg.id) {
        case MESSAGE_ID_EXIT:
            status = handleExit();
            break;
        case MESSAGE_ID_LOAD:
            status = handleMessageLoad(msg.data.fw);
            break;
        case MESSAGE_ID_ALLOC:
            status = handleMessageAlloc(msg.data.alloc);
            break;
        case MESSAGE_ID_FREE:
            status = handleMessageFree(msg.data.buffer);
            break;
        case MESSAGE_ID_MAP:
            status = handleMessageMap(msg.data.map);
            break;
        case MESSAGE_ID_UNMAP:
            status = handleMessageUnmap(msg.data.map);
            break;
        case MESSAGE_ID_SEND_ARG:
            status = handleMessageSetArgToBeSend(msg.data.map);
            break;
        case MESSAGE_ID_CONFIGURE_ISP_STANDALONE:
            status = handleMessageConfigureIspStandalone(msg.data.configureIspStandalone);
            break;
        case MESSAGE_ID_RETURN_BUFFER:
            status = handleMessageReturnBuffer(msg.data.ret);
            break;
        case MESSAGE_ID_LOAD_ISP_EXTENSIONS:
            status = handleMessageLoadIspExtensions();
            break;
        case MESSAGE_ID_UNLOAD_ISP_EXTENSIONS:
            status = handleMessageUnloadIspExtensions();
            break;
        case MESSAGE_ID_FRAME:
            status = handleMessageFrame(msg.data.frame);
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

status_t AccManagerThread::requestExitAndWait()
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

status_t AccManagerThread::handleExit()
{
    LOG1("@%s", __FUNCTION__);

    status_t status = NO_ERROR;

    mThreadRunning = false;
    return status;
}

}
