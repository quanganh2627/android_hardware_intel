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

#define LOG_TAG "Camera_Callbacks"

#include "LogHelper.h"
#include "Callbacks.h"
#include "intel_camera_extensions.h"
#include "PerformanceTraces.h"
#include "cutils/atomic.h"

namespace android {

Callbacks::Callbacks() :
    mNotifyCB(NULL)
    ,mDataCB(NULL)
    ,mDataCBTimestamp(NULL)
    ,mGetMemoryCB(NULL)
    ,mUserToken(NULL)
    ,mMessageFlags(0)
    ,mDummyByte(NULL)
    ,mPanoramaMetadata(NULL)
    ,mSceneDetectionMetadata(NULL)
    ,mStoreMetaDataInBuffers(false)
{
    LOG1("@%s", __FUNCTION__);
}

Callbacks::~Callbacks()
{
    LOG1("@%s", __FUNCTION__);
    if (mDummyByte != NULL) {
        mDummyByte->release(mDummyByte);
        mDummyByte = NULL;
    }
    if (mPanoramaMetadata != NULL) {
        mPanoramaMetadata->release(mPanoramaMetadata);
        mPanoramaMetadata = NULL;
    }
}

void Callbacks::setCallbacks(camera_notify_callback notify_cb,
                             camera_data_callback data_cb,
                             camera_data_timestamp_callback data_cb_timestamp,
                             camera_request_memory get_memory,
                             void* user)
{
    LOG1("@%s: Notify = %p, Data = %p, DataTimestamp = %p, GetMemory = %p",
            __FUNCTION__,
            notify_cb,
            data_cb,
            data_cb_timestamp,
            get_memory);
    mNotifyCB = notify_cb;
    mDataCB = data_cb;
    mDataCBTimestamp = data_cb_timestamp;
    mGetMemoryCB = get_memory;
    mUserToken = user;
}

void Callbacks::enableMsgType(int32_t msgType)
{
    LOG1("@%s: msgType = 0x%08x", __FUNCTION__, msgType);
    android_atomic_or(msgType, (int32_t*)&mMessageFlags);
    LOG1("@%s: mMessageFlags = 0x%08x", __FUNCTION__,  mMessageFlags);
}

void Callbacks::disableMsgType(int32_t msgType)
{
    LOG1("@%s: msgType = 0x%08x", __FUNCTION__, msgType);
    android_atomic_and(~msgType, (int32_t*)&mMessageFlags);
    LOG1("@%s: mMessageFlags = 0x%08x", __FUNCTION__,  mMessageFlags);
}

bool Callbacks::msgTypeEnabled(int32_t msgType)
{
    return (mMessageFlags & msgType) != 0;
}

void Callbacks::panoramaSnapshot(const AtomBuffer &livePreview)
{
    LOG2("@%s", __FUNCTION__);
    mDataCB(CAMERA_MSG_PANORAMA_SNAPSHOT, livePreview.buff, 0, NULL, mUserToken);
}

void Callbacks::panoramaDisplUpdate(camera_panorama_metadata &metadata)
{
    LOG2("@%s", __FUNCTION__);
    if (mPanoramaMetadata == NULL)
        mPanoramaMetadata = mGetMemoryCB(-1, sizeof(camera_panorama_metadata), 1, NULL);
    memcpy(mPanoramaMetadata->data, &metadata, sizeof(camera_panorama_metadata));
    mDataCB(CAMERA_MSG_PANORAMA_METADATA, mPanoramaMetadata, 0, NULL, mUserToken);
}

void Callbacks::previewFrameDone(AtomBuffer *buff)
{
    LOG2("@%s", __FUNCTION__);
    if ((mMessageFlags & CAMERA_MSG_PREVIEW_FRAME) && mDataCB != NULL) {
        LOG2("Sending message: CAMERA_MSG_PREVIEW_FRAME, buff id = %d, size = %zu", buff->id,  buff->buff->size);
        mDataCB(CAMERA_MSG_PREVIEW_FRAME, buff->buff, 0, NULL, mUserToken);
    }
}

void Callbacks::videoFrameDone(AtomBuffer *buff, nsecs_t timestamp)
{
    LOG2("@%s", __FUNCTION__);
    if ((mMessageFlags & CAMERA_MSG_VIDEO_FRAME) && mDataCBTimestamp != NULL) {
        LOG2("Sending message: CAMERA_MSG_VIDEO_FRAME, buff id = %d", buff->id);
        if(mStoreMetaDataInBuffers) {
            mDataCBTimestamp(timestamp, CAMERA_MSG_VIDEO_FRAME, buff->metadata_buff, 0, mUserToken);
        } else {
            mDataCBTimestamp(timestamp, CAMERA_MSG_VIDEO_FRAME, buff->buff, 0, mUserToken);
        }
    }
}

void Callbacks::compressedFrameDone(AtomBuffer *buff)
{
    LOG1("@%s", __FUNCTION__);
    if ((mMessageFlags & CAMERA_MSG_COMPRESSED_IMAGE) && mDataCB != NULL) {
        LOG1("Sending message: CAMERA_MSG_COMPRESSED_IMAGE, buff id = %d, size = %zu", buff->id, buff->buff->size);
        mDataCB(CAMERA_MSG_COMPRESSED_IMAGE, buff->buff, 0, NULL, mUserToken);
    }
}

void Callbacks::postviewFrameDone(AtomBuffer *buff)
{
    LOG1("@%s", __FUNCTION__);
    if ((mMessageFlags & CAMERA_MSG_POSTVIEW_FRAME) && mDataCB != NULL) {
        if (buff && buff->buff) {
            LOG1("Sending message: CAMERA_MSG_POSTVIEW_FRAME, buff id = %d, dataPtr: %p, size = %zu", buff->id, buff->dataPtr, buff->buff->size);
            mDataCB(CAMERA_MSG_POSTVIEW_FRAME, buff->buff, 0, NULL, mUserToken);
        } else {
            if (buff && buff->dataPtr && buff->size && buff->gfxInfo.gfxBufferHandle) {
                // allocated from graphics (HAL ZSL use case, usually)
                // callback memory allocation is deferred to here to conserve memory
                allocateMemory(&buff->buff, buff->size);
                if (buff->buff) {
                    memcpy(buff->buff->data, buff->dataPtr, buff->size);
                    LOG1("Sending message: CAMERA_MSG_POSTVIEW_FRAME, buff id = %d, size = %zu", buff->id,  buff->buff->size);
                    mDataCB(CAMERA_MSG_POSTVIEW_FRAME, buff->buff, 0, NULL, mUserToken);
                    buff->buff->release(buff->buff);
                    buff->buff = 0;
                } else {
                    ALOGE("@%s, Not enough memory for postview callback.", __FUNCTION__);
                }
            } else {
                ALOGE("@%s, unusable postview buffer", __FUNCTION__);
            }
        }
    }
}

void Callbacks::rawFrameDone(AtomBuffer *buff)
{
    LOG1("@%s", __FUNCTION__);
    if ((mMessageFlags & CAMERA_MSG_RAW_IMAGE_NOTIFY) && mNotifyCB != NULL) {
        LOG1("Sending message: CAMERA_MSG_RAW_IMAGE_NOTIFY, buff id = %d", buff->id);
        mNotifyCB(CAMERA_MSG_RAW_IMAGE_NOTIFY, 0, 0, mUserToken);
    }

    if ((mMessageFlags & CAMERA_MSG_RAW_IMAGE) && mNotifyCB != NULL) {
        LOG1("Sending message: CAMERA_MSG_RAW_IMAGE, buff id = %d, size = %zu", buff->id, buff->buff->size);
        mDataCB(CAMERA_MSG_RAW_IMAGE, buff->buff, 0, NULL, mUserToken);
    }
}

void Callbacks::cameraError(int err)
{
    LOG1("@%s", __FUNCTION__);
    if ((mMessageFlags & CAMERA_MSG_ERROR) && mNotifyCB != NULL) {
        ALOGD("Sending message: CAMERA_MSG_ERROR, err # = %d", err);
        mNotifyCB(CAMERA_MSG_ERROR, err, 0, mUserToken);
    }
}

void Callbacks::facesDetected(camera_frame_metadata_t &face_metadata)
{
    /* If the Call back is enabled for meta data and face detection is
    * active, inform about faces.*/
    if ((mMessageFlags & CAMERA_MSG_PREVIEW_METADATA)){
        // We can't pass NULL to camera service, otherwise it
        // will handle it as notification callback. So we need a dummy.
        // Another bad design from Google.
        if (mDummyByte == NULL) mDummyByte = mGetMemoryCB(-1, 1, 1, NULL);
        mDataCB(CAMERA_MSG_PREVIEW_METADATA,
             mDummyByte,
             0,
             &face_metadata,
             mUserToken);
    }
}

void Callbacks::sceneDetected(camera_scene_detection_metadata &metadata)
{
    LOG1("@%s", __FUNCTION__);
    if (mSceneDetectionMetadata == NULL)
        mSceneDetectionMetadata = mGetMemoryCB(-1, sizeof(camera_scene_detection_metadata), 1, NULL);
    memcpy(mSceneDetectionMetadata->data, &metadata, sizeof(camera_scene_detection_metadata));
    LOG1("Sending message: CAMERA_MSG_SCENE_DETECT, scene = %s, HDR = %d", metadata.scene ,metadata.hdr);
    mDataCB(CAMERA_MSG_SCENE_DETECT, mSceneDetectionMetadata,0, NULL, mUserToken);
}

void Callbacks::allocateMemory(AtomBuffer *buff, int size)
{
    LOG1("@%s: size %d", __FUNCTION__, size);
    buff->buff = NULL;
    if (mGetMemoryCB != NULL) {
        buff->buff = mGetMemoryCB(-1, size, 1, mUserToken);
        if (buff->buff != NULL) {
            buff->dataPtr = buff->buff->data;
            buff->size = buff->buff->size;
        } else {
            ALOGE("Memory allocation failed (get memory callback return null)");
            buff->dataPtr = NULL;
            buff->size = 0;
        }
    } else {
        ALOGE("Memory allocation failed (missing get memory callback)");
        buff->buff = NULL;
        buff->dataPtr = NULL;
        buff->size = 0;
    }
}

void Callbacks::allocateMemory(camera_memory_t **buff, size_t size)
{
    LOG1("@%s", __FUNCTION__);
    *buff = NULL;
    if (mGetMemoryCB != NULL) {
          *buff = mGetMemoryCB(-1, size, 1, mUserToken);
    }
}

void Callbacks::autoFocusDone(bool status)
{
    LOG1("@%s", __FUNCTION__);
    if (mMessageFlags & CAMERA_MSG_FOCUS) {
        LOG1("Sending message: CAMERA_MSG_FOCUS");
        mNotifyCB(CAMERA_MSG_FOCUS, status, 0, mUserToken);
    }
}

void Callbacks::focusMove(bool start)
{
    LOG1("@%s", __FUNCTION__);
    if (mMessageFlags & CAMERA_MSG_FOCUS_MOVE) {
        LOG2("Sending message: CAMERA_MSG_FOCUS_MOVE");
        mNotifyCB(CAMERA_MSG_FOCUS_MOVE, start, 0, mUserToken);
    }
}

void Callbacks::shutterSound()
{
    LOG1("@%s", __FUNCTION__);
    if (mMessageFlags & CAMERA_MSG_SHUTTER) {
        LOG1("Sending message: CAMERA_MSG_SHUTTER");
        mNotifyCB(CAMERA_MSG_SHUTTER, 1, 0, mUserToken);
    }
}

status_t Callbacks::storeMetaDataInBuffers(bool enabled)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    mStoreMetaDataInBuffers = enabled;
    return status;
}

void Callbacks::ullPictureDone(AtomBuffer *buff)
{
    LOG1("@%s", __FUNCTION__);
    if (mDataCB != NULL) {
        LOG1("Sending message: CAMERA_MSG_ULL_SNAPSHOT, buff id = %d, size = %zu", buff->id, buff->buff->size);
        mDataCB(CAMERA_MSG_ULL_SNAPSHOT, buff->buff, 0, NULL, mUserToken);
    }
}

void Callbacks::ullTriggered(int id)
{
    LOG1("@%s", __FUNCTION__);

    if (mNotifyCB != NULL) {
        LOG1("Sending message: CAMERA_MSG_ULL_TRIGGERED, id = %d", id);
        mNotifyCB(CAMERA_MSG_ULL_TRIGGERED, id, 0, mUserToken);
    }

}

void Callbacks::lowBattery()
{
    LOG1("@%s", __FUNCTION__);

    if (mNotifyCB != NULL) {
        LOG1("Sending message: CAMERA_MSG_LOW_BATTERY");
        mNotifyCB(CAMERA_MSG_LOW_BATTERY, 1, 0, mUserToken);
    }
}

void Callbacks::accManagerPointer(int isp_ptr, int idx)
{
    LOG1("@%s", __FUNCTION__);

    if (mNotifyCB != NULL) {
        LOG1("Sending message: CAMERA_MSG_ACC_POINTER, isp_ptr = %d, idx = %d", isp_ptr, idx);
        mNotifyCB(CAMERA_MSG_ACC_POINTER, isp_ptr, idx, mUserToken);
    }
}

void Callbacks::accManagerFinished()
{
    LOG1("@%s", __FUNCTION__);

    if (mNotifyCB != NULL) {
        LOG1("Sending message: CAMERA_MSG_ACC_FINISHED");
        mNotifyCB(CAMERA_MSG_ACC_FINISHED, 0, 0, mUserToken);
    }
}

void Callbacks::accManagerPreviewBuffer(camera_memory_t *buffer)
{
    LOG2("@%s", __FUNCTION__);

    if (mDataCB != NULL) {
        LOG2("Sending message: CAMERA_MSG_ACC_PREVIEW_BUFFER");
        mDataCB(CAMERA_MSG_ACC_PREVIEW_BUFFER, buffer, 0, NULL, mUserToken);
    }
}

void Callbacks::accManagerArgumentBuffer(camera_memory_t *buffer)
{
    LOG1("@%s", __FUNCTION__);

    if (mDataCB != NULL) {
        LOG1("Sending message: CAMERA_MSG_ACC_ARGUMENT_BUFFER");
        mDataCB(CAMERA_MSG_ACC_ARGUMENT_BUFFER, buffer, 0, NULL, mUserToken);
    }
}

void Callbacks::accManagerMetadataBuffer(camera_memory_t *buffer)
{
    LOG1("@%s", __FUNCTION__);

    if (mDataCB != NULL) {
        LOG1("Sending message: CAMERA_MSG_ACC_METADATA_BUFFER");
        mDataCB(CAMERA_MSG_ACC_METADATA_BUFFER, buffer, 0, NULL, mUserToken);
    }
}

};
