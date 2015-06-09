/*
 * Copyright (C) 2011 The Android Open Source Project
 * Copyright (C) 2011,2012,2013,2014 Intel Corporation
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

#define LOG_TAG "Camera_WarperService"

#include "WarperService.h"
#include "LogHelper.h"

namespace android {

WarperService::WarperService() :
        Thread(false)
        ,mMessageQueue("WarperService", MESSAGE_ID_MAX)
        ,mThreadRunning(false)
        ,mGPUWarper(NULL)
        ,mWidth(0)
        ,mHeight(0)
        ,mFrameDimChanged(false)
        ,mGPUWarperActive(false)
{
    LOG1("@%s", __FUNCTION__);
}

WarperService::~WarperService() {
    LOG1("@%s", __FUNCTION__);
    delete mGPUWarper;
    mGPUWarper = NULL;
}

bool WarperService::threadLoop() {
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    mThreadRunning = true;
    while (mThreadRunning) {
        status = waitForAndExecuteMessage();
    }

    return false;
}

status_t WarperService::updateFrameDimensions(GLuint width, GLuint height) {
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_UPDATE_FRAME_DIMENSIONS;
    msg.data.messageUpdateFrameDimensions.width = width;
    msg.data.messageUpdateFrameDimensions.height = height;

    return mMessageQueue.send(&msg, MESSAGE_ID_UPDATE_FRAME_DIMENSIONS);
}

status_t WarperService::updateStatus(bool active) {
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_UPDATE_STATUS;
    msg.data.messageUpdateStatus.active = active;

    return mMessageQueue.send(&msg, MESSAGE_ID_UPDATE_STATUS);
}

status_t WarperService::warpBackFrame(AtomBuffer *frame, double projective[PROJ_MTRX_DIM][PROJ_MTRX_DIM]) {
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_WARP_BACK_FRAME;
    if (frame) {
        msg.data.messageWarpBackFrame.frame = frame;
    } else {
        ALOGE("Can not access frame data.");
        return INVALID_OPERATION;
    }
    if (projective) {
        for (int i = 0; i < PROJ_MTRX_DIM; i++) {
            for (int j = 0; j < PROJ_MTRX_DIM; j++) {
                msg.data.messageWarpBackFrame.projective[i][j] = projective[i][j];
            }
        }
    } else {
        ALOGE("Projective matrix is not initialized.");
        return INVALID_OPERATION;
    }
    return mMessageQueue.send(&msg, MESSAGE_ID_WARP_BACK_FRAME);
}

status_t WarperService::handleMessageUpdateFrameDimensions(MessageUpdateFrameDimensions &msg) {
    LOG2("@%s", __FUNCTION__);

    status_t status = NO_ERROR;

    if (msg.width != mWidth || msg.height != mHeight) {
        mWidth = msg.width;
        mHeight = msg.height;
        mFrameDimChanged = true;

        if (mGPUWarperActive) {
            if (mGPUWarper == NULL) {
                ALOGE("GPUWarper is not initialized.");
                mMessageQueue.reply(MESSAGE_ID_UPDATE_FRAME_DIMENSIONS, INVALID_OPERATION);
                return INVALID_OPERATION;
            }

            status = mGPUWarper->updateFrameDimensions(mWidth, mHeight);
            if (status != NO_ERROR) {
                mMessageQueue.reply(MESSAGE_ID_UPDATE_FRAME_DIMENSIONS, status);
                return status;
            }
            mFrameDimChanged = false;
        }
    }

    mMessageQueue.reply(MESSAGE_ID_UPDATE_FRAME_DIMENSIONS, OK);
    return OK;
}

status_t WarperService::handleMessageUpdateStatus(MessageUpdateStatus &msg) {
    LOG2("@%s", __FUNCTION__);

    status_t status = NO_ERROR;

    mGPUWarperActive = msg.active;

    if (mGPUWarperActive) {

        if (mGPUWarper == NULL) {
            mGPUWarper = new GPUWarper(mWidth, mHeight, 64);
            if (mGPUWarper == NULL) {
                ALOGE("Failed to create GPUWarper");
                mGPUWarperActive = false;
                mMessageQueue.reply(MESSAGE_ID_UPDATE_STATUS, NO_MEMORY);
                return NO_MEMORY;
            }

            status = mGPUWarper->init();
            if (status != NO_ERROR) {
                mGPUWarperActive = false;
                mMessageQueue.reply(MESSAGE_ID_UPDATE_STATUS, status);
                return status;
            }

            LOG1("GPUWarper initialized.");

        } else {
            if (mFrameDimChanged) {
                status = mGPUWarper->updateFrameDimensions(mWidth, mHeight);
                if (status != NO_ERROR) {
                    mMessageQueue.reply(MESSAGE_ID_UPDATE_STATUS, status);
                    return status;
                }

                mFrameDimChanged = false;
            }
        }
    }

    mMessageQueue.reply(MESSAGE_ID_UPDATE_STATUS, OK);
    return OK;
}

status_t WarperService::handleMessageWarpBackFrame(MessageWarpBackFrame &msg) {
    LOG2("@%s", __FUNCTION__);

    status_t status;

    if (mGPUWarper == NULL) {
        // GPUWarper should be initialized before entering this function
        ALOGE("GPUWarper is not initialized.");
        mMessageQueue.reply(MESSAGE_ID_WARP_BACK_FRAME, INVALID_OPERATION);
        return INVALID_OPERATION;
    }

    status = mGPUWarper->warpBackFrame(msg.frame, msg.projective);
    if (status != NO_ERROR) {
        mMessageQueue.reply(MESSAGE_ID_WARP_BACK_FRAME, status);
        return status;
    }

    mMessageQueue.reply(MESSAGE_ID_WARP_BACK_FRAME, OK);
    return OK;
}

status_t WarperService::waitForAndExecuteMessage() {
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    Message msg;
    mMessageQueue.receive(&msg);

    switch (msg.id) {
    case MESSAGE_ID_EXIT:
        status = handleMessageExit();
        break;
    case MESSAGE_ID_WARP_BACK_FRAME:
        status = handleMessageWarpBackFrame(msg.data.messageWarpBackFrame);
        break;
    case MESSAGE_ID_UPDATE_FRAME_DIMENSIONS:
        status = handleMessageUpdateFrameDimensions(msg.data.messageUpdateFrameDimensions);
        break;
    case MESSAGE_ID_UPDATE_STATUS:
        status = handleMessageUpdateStatus(msg.data.messageUpdateStatus);
        break;
    default:
        status = BAD_VALUE;
        break;
    };
    return status;
}

status_t WarperService::handleMessageExit() {
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    mThreadRunning = false;

    delete mGPUWarper;
    mGPUWarper = NULL;

    return status;
}

status_t WarperService::requestExitAndWait() {
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
