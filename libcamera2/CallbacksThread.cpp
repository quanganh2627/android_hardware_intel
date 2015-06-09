/*
 * Copyright (C) 2011 The Android Open Source Project
 * Copyright (c) 2012-2014 Intel Corporation
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
#define LOG_TAG "Camera_CallbacksThread"

#include "CallbacksThread.h"
#include "LogHelper.h"
#include "Callbacks.h"
#include "FaceDetector.h"
#include "MemoryUtils.h"
#include "PlatformData.h"
#include "CameraDump.h"
#include "PerformanceTraces.h"

namespace android {

CallbacksThread::CallbacksThread(Callbacks *callbacks, int cameraId, ICallbackPicture *pictureDone) :
    Thread(true) // callbacks may call into java
    ,mMessageQueue("CallbacksThread", MESSAGE_ID_MAX)
    ,mThreadRunning(false)
    ,mCallbacks(callbacks)
    ,mJpegRequested(0)
    ,mPostviewRequested(0)
    ,mRawRequested(0)
    ,mULLRequested(0)
    ,mULLid(UINT_MAX)
    ,mFocusActive(false)
    ,mWaitRendering(false)
    ,mLastReportedNumberOfFaces(0)
    ,mLastReportedNeedLLS(0)
    ,mFaceCbCount(0)
    ,mFaceCbFreqDivider(1)
    ,mPictureDoneCallback(pictureDone)
    ,mCameraId(cameraId)
    ,mPausePreviewCallbacks(false)
{
    LOG1("@%s", __FUNCTION__);
    mPostponedJpegReady.id = (MessageId) -1;

    // Trying to slighly optimize here, instead of calling this for each face callback
    mFaceCbFreqDivider =  PlatformData::faceCallbackDivider();
}

CallbacksThread::~CallbacksThread()
{
    LOG1("@%s", __FUNCTION__);
}

status_t CallbacksThread::shutterSound()
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_CALLBACK_SHUTTER;
    return mMessageQueue.send(&msg);
}

void CallbacksThread::panoramaDisplUpdate(camera_panorama_metadata_t &metadata)
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_PANORAMA_DISPL_UPDATE;
    msg.data.panoramaDisplUpdate.metadata = metadata;
    mMessageQueue.send(&msg);
}

status_t CallbacksThread::handleMessagePanoramaDisplUpdate(MessagePanoramaDisplUpdate *msg)
{
    LOG1("@%s", __FUNCTION__);
    mCallbacks->panoramaDisplUpdate(msg->metadata);
    return OK;
}

void CallbacksThread::panoramaSnapshot(const AtomBuffer &livePreview)
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_PANORAMA_SNAPSHOT;
    msg.data.panoramaSnapshot.snapshot = livePreview;
    mMessageQueue.send(&msg);
}

status_t CallbacksThread::handleMessagePanoramaSnapshot(MessagePanoramaSnapshot *msg)
{
    LOG1("@%s", __FUNCTION__);
    mCallbacks->panoramaSnapshot(msg->snapshot);
    MemoryUtils::freeAtomBuffer(msg->snapshot);
    return OK;
}

status_t CallbacksThread::compressedFrameDone(AtomBuffer* jpegBuf, AtomBuffer* snapshotBuf, AtomBuffer* postviewBuf)
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_JPEG_DATA_READY;

    if (jpegBuf != NULL) {
        LOG1("@%s: ID = %d", __FUNCTION__, jpegBuf->id);
        msg.data.compressedFrame.jpegBuff = *jpegBuf;
    } else {
        msg.data.compressedFrame.jpegBuff = AtomBufferFactory::createAtomBuffer();
        msg.data.compressedFrame.jpegBuff.id = -1;
    }

    if (snapshotBuf != NULL) {
        msg.data.compressedFrame.snapshotBuff = *snapshotBuf;
    } else {
        msg.data.compressedFrame.snapshotBuff = AtomBufferFactory::createAtomBuffer();
        msg.data.compressedFrame.snapshotBuff.status = FRAME_STATUS_SKIPPED;
        msg.data.compressedFrame.snapshotBuff.id = -1;
    }

    if (postviewBuf != NULL) {
        msg.data.compressedFrame.postviewBuff = *postviewBuf;
    } else {
        msg.data.compressedFrame.postviewBuff = AtomBufferFactory::createAtomBuffer();
        msg.data.compressedFrame.postviewBuff.status = FRAME_STATUS_SKIPPED;
        msg.data.compressedFrame.postviewBuff.id = -1;
    }

    return mMessageQueue.send(&msg);
}

/**
 * Sends an "ULL triggered"-callback to the application
 * \param id ID of the post-processed ULL snapshot that will be provided to the
 * application after the post-processing is done.
 */
status_t CallbacksThread::ullTriggered(int id)
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_ULL_TRIGGERED;
    msg.data.ull.id = id;

    return mMessageQueue.send(&msg);
}

/**
 * Requests a ULL capture to be sent to client
 * the next JPEG image done received by the CallbackThread will be returned to
 * the client via  a custom callback rather than the normal JPEG data callabck
 * \param id [in] Running number identifying the ULL capture. It matches the
 * number provided to the application when ULL starts
 */
status_t CallbacksThread::requestULLPicture(int id)
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_ULL_JPEG_DATA_REQUEST;
    msg.data.ull.id = id;

    return mMessageQueue.send(&msg);
}

status_t  CallbacksThread::previewFrameDone(AtomBuffer *aPreviewFrame)
{
    if(aPreviewFrame == NULL) return BAD_VALUE;

    LOG2("@%s: ID = %d", __FUNCTION__, aPreviewFrame->id);
    Message msg;
    msg.id = MESSAGE_ID_PREVIEW_DONE;
    msg.data.preview.frame = *aPreviewFrame;

    return mMessageQueue.send(&msg);

}

status_t CallbacksThread::postviewRendered()
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_POSTVIEW_RENDERED;
    return mMessageQueue.send(&msg);
}

status_t CallbacksThread::lowBattery()
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_LOW_BATTERY;

    return mMessageQueue.send(&msg);
}

status_t CallbacksThread::sendFrameId(int id)
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.data.frameId.id = id;
    msg.id = MESSAGE_ID_SEND_FRAME_ID;

    return mMessageQueue.send(&msg);
}

status_t CallbacksThread::rawFrameDone(AtomBuffer* snapshotBuf)
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_RAW_FRAME_DONE;
    msg.data.rawFrame.frame.buff = NULL;

    if (snapshotBuf != NULL) {
        msg.data.rawFrame.frame = *snapshotBuf;
    }

    return mMessageQueue.send(&msg);
}

status_t CallbacksThread::smartStabilizationFrameDone(const AtomBuffer &yuvBuf)
{
    LOG1("@%s",__FUNCTION__);
    status_t status;
    Message msg;
    msg.id = MESSAGE_ID_SS_FRAME_DONE;
    msg.data.smartStabilizationFrame.frame = yuvBuf;

    status = mMessageQueue.send(&msg);
    return status;
}

status_t CallbacksThread::handleMessageSSFrameDone(MessageFrame *msg)
{
    LOG1("@%s",__FUNCTION__);

    mCallbacks->smartStabilizationFrameDone(&msg->frame);

    return NO_ERROR;
}

status_t CallbacksThread::postviewFrameDone(AtomBuffer* postviewBuf)
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_POSTVIEW_FRAME_DONE;
    msg.data.postviewFrame.frame.buff = NULL;

   if (postviewBuf != NULL) {
        msg.data.postviewFrame.frame = *postviewBuf;
    }

    return mMessageQueue.send(&msg);
}

status_t CallbacksThread::handleMessagePostviewRendered()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    if (mWaitRendering) {
        mWaitRendering = false;
        // check if handling of jpeg data ready was postponed for this
        if (mPostponedJpegReady.id == MESSAGE_ID_JPEG_DATA_READY) {
           status = handleMessageJpegDataReady(&mPostponedJpegReady.data.compressedFrame);
           mPostponedJpegReady.id = (MessageId) -1;
        }
    }
    return status;
}


/**
 * Allocate memory for callbacks needed in takePicture()
 *
 * \param postviewCallback allocate for postview callback
 * \param rawCallback      allocate for raw callback
 * \param waitRendering    synchronize compressed frame callback with
 *                         postviewRendered()
 */
status_t CallbacksThread::requestTakePicture(bool postviewCallback, bool rawCallback, bool waitRendering)
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_JPEG_DATA_REQUEST;
    msg.data.dataRequest.postviewCallback = postviewCallback;
    msg.data.dataRequest.rawCallback = rawCallback;
    msg.data.dataRequest.waitRendering = waitRendering;
    return mMessageQueue.send(&msg);
}

status_t CallbacksThread::flushPictures()
{
    LOG1("@%s", __FUNCTION__);
    // we own the dynamically allocated jpegbuffer. Free that buffer for all
    // the pending messages before flushing them
    Vector<Message> pending;
    mMessageQueue.remove(MESSAGE_ID_JPEG_DATA_READY, &pending);
    Vector<Message>::iterator it;
    for (it = pending.begin(); it != pending.end(); ++it) {
       camera_memory_t* b = it->data.compressedFrame.jpegBuff.buff;
       b->release(b);
       b = NULL;
       mPictureDoneCallback->pictureDone(&it->data.compressedFrame.snapshotBuff,
               &it->data.compressedFrame.postviewBuff);
    }

    if (mWaitRendering) {
        mWaitRendering = false;
        // check if handling of jpeg data was postponed
        if (mPostponedJpegReady.id == MESSAGE_ID_JPEG_DATA_READY) {
            camera_memory_t* b = mPostponedJpegReady.data.compressedFrame.jpegBuff.buff;
            b->release(b);
            b = NULL;
            mPostponedJpegReady.id = (MessageId) -1;
            mPictureDoneCallback->pictureDone(&it->data.compressedFrame.snapshotBuff,
               &it->data.compressedFrame.postviewBuff);

        }
    }

    /* Remove also any requests that may be queued  */
    mMessageQueue.remove(MESSAGE_ID_JPEG_DATA_REQUEST, NULL);

    Message msg;
    msg.id = MESSAGE_ID_FLUSH;
    return mMessageQueue.send(&msg);
}

/**
 * Inform CallbacksThread from auto-focus activation
 *
 * Android API contains a specific rule:
 *
 * "If the apps call autoFocus(AutoFocusCallback), the camera will stop sending face callbacks.
 * The last face callback indicates the areas used to do autofocus. After focus completes, face
 * detection will resume sending face callbacks. If the apps call cancelAutoFocus(), the face
 * callbacks will also resume."
 *
 * While focusing sequence only might be triggered to focus into faces,
 * we do commonly prevent the face callbacks in this level, while focusing is
 * active.
 */
void CallbacksThread::autoFocusActive(bool focusActive)
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.data.autoFocusActive.value = focusActive;
    msg.id = MESSAGE_ID_AUTO_FOCUS_ACTIVE;
    mMessageQueue.send(&msg);
}

void CallbacksThread::autoFocusDone(bool status)
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.data.autoFocusDone.status = status;
    msg.id = MESSAGE_ID_AUTO_FOCUS_DONE;
    mMessageQueue.send(&msg);
}

void CallbacksThread::focusMove(bool start)
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_FOCUS_MOVE;
    msg.data.focusMove.start = start;
    mMessageQueue.send(&msg);
}

status_t CallbacksThread::handleMessageAutoFocusActive(MessageAutoFocusActive *msg)
{
    LOG1("@%s", __FUNCTION__);
    mFocusActive = msg->value;
    return NO_ERROR;
}

status_t CallbacksThread::handleMessageAutoFocusDone(MessageAutoFocusDone *msg)
{
    LOG1("@%s", __FUNCTION__);
    mCallbacks->autoFocusDone(msg->status);
    mFocusActive = false;
    return NO_ERROR;
}

status_t CallbacksThread::handleMessageFocusMove(CallbacksThread::MessageFocusMove *msg)
{
    LOG1("@%s", __FUNCTION__);
    mCallbacks->focusMove(msg->start);
    return NO_ERROR;
}


status_t CallbacksThread::sendError(int id)
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.data.error.id = id;
    msg.id = MESSAGE_ID_ERROR_CALLBACK;

    return mMessageQueue.send(&msg);
}

status_t CallbacksThread::handleMessageSendError(MessageError *msg)
{
    ALOGE("@%s: id %d", __FUNCTION__,msg->id);
    mCallbacks->cameraError(msg->id);
    return NO_ERROR;
}

void CallbacksThread::facesDetected(extended_frame_metadata_t *extended_face_metadata)
{
    LOG2("@%s: face_metadata ptr = %p", __FUNCTION__, extended_face_metadata);

    Mutex::Autolock lock(mFaceReportingLock); // protecting member variable accesses during the filtering below

    // Null face data -> reset IFaceDetectionListener
    if (extended_face_metadata == NULL) {
        mLastReportedNumberOfFaces = 0;
        mFaceCbCount = 0;
        return;
    }

    // needLLS needs to be sent always when it changes, so don't adjust sending frequency, if it changes
    if (mLastReportedNeedLLS != extended_face_metadata->needLLS) {
        mLastReportedNeedLLS = extended_face_metadata->needLLS;
    } else {
        // Count the callbacks to adjust sending frequency.
        // We want to do this to relieve the application face indicator rendering load,
        // as it seems to get heavy when large number of faces are presented.
        // TODO: Dynamic adjustment of frequency, depending on number of faces
        ++mFaceCbCount; // Ok to wrap around

        if (extended_face_metadata->number_of_faces > 0 || mLastReportedNumberOfFaces != 0) {
            mLastReportedNumberOfFaces = extended_face_metadata->number_of_faces;
            // Not the time to send cb -> do nothing
            if (!(mFaceCbCount % mFaceCbFreqDivider == 0 || mLastReportedNumberOfFaces == 0)) {
                return;
            }
        } else if (extended_face_metadata->number_of_faces == 0 && mLastReportedNumberOfFaces == 0) {
            // For subsequent zero faces, after the first zero-face cb sent to application
            // -> do nothing
            return;
        }
    }

    int num_faces;
    if (extended_face_metadata->number_of_faces > MAX_FACES_DETECTABLE) {
        ALOGW("@%s: %d faces detected, limiting to %d", __FUNCTION__,
                extended_face_metadata->number_of_faces, MAX_FACES_DETECTABLE);
        num_faces = MAX_FACES_DETECTABLE;
    } else {
        num_faces = extended_face_metadata->number_of_faces;
    }

    if (num_faces > 0)
        PerformanceTraces::FaceLock::stop(num_faces);

    Message msg;
    msg.id = MESSAGE_ID_FACES;
    msg.data.faces.needLLS = extended_face_metadata->needLLS;
    msg.data.faces.numFaces = num_faces;
    memcpy(msg.data.faces.faces, extended_face_metadata->faces,
           num_faces * sizeof(camera_face_t));

    mMessageQueue.send(&msg);
}

status_t CallbacksThread::sceneDetected(camera_scene_detection_metadata_t &metadata)
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_SCENE_DETECTED;
    strlcpy(msg.data.sceneDetected.sceneMode, metadata.scene, (size_t)SCENE_STRING_LENGTH);
    msg.data.sceneDetected.sceneHdr = metadata.hdr;
    return mMessageQueue.send(&msg);
}

status_t CallbacksThread::handleMessageExit()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    mThreadRunning = false;
    return status;
}

status_t CallbacksThread::handleMessageCallbackShutter()
{
    LOG1("@%s", __FUNCTION__);
    mCallbacks->shutterSound();
    return NO_ERROR;
}

status_t CallbacksThread::videoFrameDone(AtomBuffer *buff, nsecs_t timestamp)
{
    LOG2("@%s: ID = %d", __FUNCTION__, buff->id);
    Message msg;
    msg.id = MESSAGE_ID_VIDEO_DONE;
    msg.data.video.frame = *buff;
    msg.data.video.timestamp = timestamp;
    return mMessageQueue.send(&msg);
}

status_t CallbacksThread::accManagerPointer(int isp_ptr, int idx)
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_ACC_POINTER;
    msg.data.accManager.isp_ptr = isp_ptr;
    msg.data.accManager.idx = idx;

    return mMessageQueue.send(&msg);
}

status_t CallbacksThread::accManagerFinished()
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_ACC_FINISHED;

    return mMessageQueue.send(&msg);
}

status_t CallbacksThread::accManagerPreviewBuffer(camera_memory_t *buffer)
{
    LOG2("@%s", __FUNCTION__);

    if(buffer == NULL) return BAD_VALUE;

    Message msg;
    msg.id = MESSAGE_ID_ACC_PREVIEW_BUFFER;
    msg.data.accManager.buffer = *buffer;

    return mMessageQueue.send(&msg);
}

status_t CallbacksThread::accManagerArgumentBuffer(camera_memory_t *buffer)
{
    LOG1("@%s", __FUNCTION__);

    if(buffer == NULL) return BAD_VALUE;

    Message msg;
    msg.id = MESSAGE_ID_ACC_ARGUMENT_BUFFER;
    msg.data.accManager.buffer = *buffer;

    return mMessageQueue.send(&msg);
}

status_t CallbacksThread::accManagerMetadataBuffer(camera_memory_t *buffer)
{
    LOG1("@%s", __FUNCTION__);

    if(buffer == NULL) return BAD_VALUE;

    Message msg;
    msg.id = MESSAGE_ID_ACC_METADATA_BUFFER;
    msg.data.accManager.buffer = *buffer;

    return mMessageQueue.send(&msg);
}

/**
 * Process message received from Picture Thread when a the image compression
 * has completed.
 */
status_t CallbacksThread::handleMessageJpegDataReady(MessageCompressed *msg)
{
    LOG1("@%s: JPEG buffers queued: %d, mJpegRequested = %u, mPostviewRequested = %u, mRawRequested = %u, mULLRequested = %u",
            __FUNCTION__,
            mBuffers.size(),
            mJpegRequested,
            mPostviewRequested,
            mRawRequested,
            mULLRequested);
    AtomBuffer jpegBuf = msg->jpegBuff;
    AtomBuffer snapshotBuf = msg->snapshotBuff;
    AtomBuffer postviewBuf= msg->postviewBuff;

    mPictureDoneCallback->encodingDone(&snapshotBuf, &postviewBuf);

    if (jpegBuf.dataPtr == NULL) {
        ALOGW("@%s: returning raw frames used in failed encoding", __FUNCTION__);
        mPictureDoneCallback->pictureDone(&snapshotBuf, &postviewBuf);
        return NO_ERROR;
    }

    if ((msg->snapshotBuff.type == ATOM_BUFFER_ULL) && (mULLRequested > 0)) {
        return handleMessageUllJpegDataReady(msg);
    }

    if (mJpegRequested > 0) {
        if (gLogLevel & CAMERA_DEBUG_JPEG_DUMP) {
            String8 jpegDumpName("/data/cam_hal_jpeg_dump.jpeg");
            CameraDump::dumpAtom2File(&jpegBuf, jpegDumpName.string());
        }

        // For depth mode, it don't pause preview callback.
        if (!PlatformData::isExtendedCamera(mCameraId))
            mPausePreviewCallbacks = true;

        mCallbacks->compressedFrameDone(&jpegBuf);
        if (jpegBuf.buff == NULL) {
            ALOGW("CallbacksThread received NULL jpegBuf.buff, which should not happen");
        } else {
            LOG1("Releasing jpegBuf @%p", jpegBuf.dataPtr);
            MemoryUtils::freeAtomBuffer(jpegBuf);
        }
        mJpegRequested--;

        // Return the raw buffers back to ControlThread
        mPictureDoneCallback->pictureDone(&snapshotBuf, &postviewBuf);
    } else {
        // Insert the buffer on the top
        mBuffers.push(*msg);
    }

    return NO_ERROR;
}

status_t CallbacksThread::handleMessageJpegDataRequest(MessageDataRequest *msg)
{
    LOG1("@%s: JPEG buffers queued: %d, mJpegRequested = %u, mPostviewRequested = %u, mRawRequested = %u",
            __FUNCTION__,
            mBuffers.size(),
            mJpegRequested,
            mPostviewRequested,
            mRawRequested);
    if (!mBuffers.isEmpty()) {
        AtomBuffer jpegBuf = mBuffers[0].jpegBuff;
        AtomBuffer snapshotBuf = mBuffers[0].snapshotBuff;
        AtomBuffer postviewBuf = mBuffers[0].postviewBuff;

        if (msg->postviewCallback) {
            mCallbacks->postviewFrameDone(&postviewBuf);
        }
        if (msg->rawCallback) {
            mCallbacks->rawFrameDone(&snapshotBuf);
        }

        // For depth mode, it don't pause preview callback.
        if (!PlatformData::isExtendedCamera(mCameraId))
            mPausePreviewCallbacks = true;

        mCallbacks->compressedFrameDone(&jpegBuf);

        LOG1("Releasing jpegBuf.buff %p, dataPtr %p", jpegBuf.buff, jpegBuf.dataPtr);
        MemoryUtils::freeAtomBuffer(jpegBuf);

        // Return the raw buffers back
        mPictureDoneCallback->pictureDone(&snapshotBuf, &postviewBuf);

        mBuffers.removeAt(0);
    } else {
        mJpegRequested++;
        if (msg->postviewCallback) {
            mPostviewRequested++;
        }
        if (msg->rawCallback) {
            mRawRequested++;
        }
        mWaitRendering = msg->waitRendering;
    }

    return NO_ERROR;
}

status_t CallbacksThread::handleMessageUllTriggered(MessageULLSnapshot *msg)
{
    LOG1("@%s Done id:%d",__FUNCTION__,msg->id);
    int id = msg->id;
    mCallbacks->ullTriggered(id);
    return NO_ERROR;
}

status_t CallbacksThread::handleMessageLowBattery()
{
    LOG1("@%s Done",__FUNCTION__);
    mCallbacks->lowBattery();
    return NO_ERROR;
}

status_t CallbacksThread::handleMessageSendFrameId(MessageFrameId *msg)
{
    LOG1("@%s Done",__FUNCTION__);
    mCallbacks->sendFrameId(msg->id);
    return NO_ERROR;
}

status_t CallbacksThread::handleMessageUllJpegDataRequest(MessageULLSnapshot *msg)
{
    LOG1("@%s Done",__FUNCTION__);
    mULLRequested++;
    mULLid = msg->id;
    return NO_ERROR;
}

status_t CallbacksThread::handleMessageUllJpegDataReady(MessageCompressed *msg)
{
    LOG1("@%s",__FUNCTION__);
    AtomBuffer jpegBuf = msg->jpegBuff;
    AtomBuffer snapshotBuf = msg->snapshotBuff;
    AtomBuffer postviewBuf= msg->postviewBuff;

    --mULLRequested;

    if (jpegBuf.dataPtr == NULL) {
        ALOGW("@%s: returning raw frames used in failed encoding", __FUNCTION__);
        mPictureDoneCallback->pictureDone(&snapshotBuf, &postviewBuf);
        return NO_ERROR;
    }

    if (gLogLevel & CAMERA_DEBUG_ULL_DUMP) {
        String8 jpegName("/data/ull_jpeg_dump_");
        jpegName.appendFormat("id_%d.jpg",mULLid);
        CameraDump::dumpAtom2File(&jpegBuf, jpegName.string());
    }

    // Put the metadata in place to the ULL image buffer. This will be
    // split into separate JPEG buffer and ULL metadata in the service (JNI) layer
    // before passing to application via the Java callback
    camera_ull_metadata_t metadata;
    metadata.id = mULLid;

    AtomBuffer jpegAndMeta = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_SNAPSHOT);
    mCallbacks->allocateMemory(&jpegAndMeta, jpegBuf.size + sizeof(camera_ull_metadata_t));

    if (jpegAndMeta.buff == NULL) {
        ALOGE("Failed to allocate memory for buffer jpegAndMeta");
        return UNKNOWN_ERROR;
    }

    // space for the metadata is reserved in the beginning of the buffer, copy it there
    memcpy(jpegAndMeta.dataPtr, &metadata, sizeof(camera_ull_metadata_t));

    // copy the image data in place, it goes after the metadata in the buffer
    memcpy((char*)jpegAndMeta.dataPtr + sizeof(camera_ull_metadata_t), jpegBuf.dataPtr, jpegBuf.size);

    mCallbacks->ullPictureDone(&jpegAndMeta);

    LOG1("Releasing jpegBuf.buff %p, dataPtr %p", jpegBuf.buff, jpegBuf.dataPtr);
    MemoryUtils::freeAtomBuffer(jpegBuf);

    if (jpegAndMeta.buff == NULL) {
        ALOGW("NULL jpegAndMeta buffer, while reaching freeAtomBuffer().");
        return UNKNOWN_ERROR;
    } else {
        LOG1("Releasing jpegAndMeta.buff %p, dataPtr %p", jpegAndMeta.buff, jpegAndMeta.dataPtr);
        MemoryUtils::freeAtomBuffer(jpegAndMeta);
    }

    /**
     *  TODO at the moment ULL does not process postview. Should we process it as well?
     */

    if (snapshotBuf.dataPtr != NULL) {
        // Return the raw buffers back to ISP
        LOG1("Returning ULL raw image now");
        snapshotBuf.type = ATOM_BUFFER_SNAPSHOT;  // reset the buffer type
    }
    mPictureDoneCallback->pictureDone(&snapshotBuf, &postviewBuf);

    return NO_ERROR;

}

status_t CallbacksThread::handleMessageFlush()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    mJpegRequested = 0;
    mPostviewRequested = 0;
    mRawRequested = 0;
    mWaitRendering = false;
    mPostponedJpegReady.id = (MessageId) -1;
    for (size_t i = 0; i < mBuffers.size(); i++) {
        AtomBuffer jpegBuf = mBuffers[i].jpegBuff;
        LOG1("Releasing jpegBuf.buff %p, dataPtr %p", jpegBuf.buff, jpegBuf.dataPtr);
        MemoryUtils::freeAtomBuffer(jpegBuf);
    }
    mBuffers.clear();
    return status;
}

status_t CallbacksThread::handleMessageFaces(MessageFaces *msg)
{
    LOG2("@%s", __FUNCTION__);
    if (!mFocusActive) {
        extended_frame_metadata_t face_metadata;
        face_metadata.number_of_faces = msg->numFaces;
        face_metadata.faces = msg->faces;
        face_metadata.needLLS = msg->needLLS;
        mCallbacks->facesDetected((camera_frame_metadata_t *)&face_metadata);
    }
    else
        LOG1("Faces metadata dropped during focusing.");
    return NO_ERROR;
}

status_t CallbacksThread::handleMessageSceneDetected(MessageSceneDetected *msg)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    camera_scene_detection_metadata_t metadata;
    strlcpy(metadata.scene, msg->sceneMode, (size_t)SCENE_STRING_LENGTH);
    metadata.hdr = msg->sceneHdr;
    mCallbacks->sceneDetected(metadata);
    return status;
}

status_t CallbacksThread::handleMessagePreviewDone(MessageFrame *msg)
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    if (!mPausePreviewCallbacks) {
        mCallbacks->previewFrameDone(&(msg->frame));
    } else if (msg->frame.owner != NULL && msg->frame.returnAfterCB) {
        msg->frame.owner->returnBuffer(&msg->frame);
    }

    return status;
}

status_t CallbacksThread::handleMessageVideoDone(MessageVideo *msg)
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    mCallbacks->videoFrameDone(&(msg->frame), msg->timestamp);
    return status;
}

status_t CallbacksThread::handleMessageRawFrameDone(MessageFrame *msg)
{
    LOG1("@%s",__FUNCTION__);
    status_t status = NO_ERROR;
    bool    releaseTmp = false;
    AtomBuffer tmpCopy = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_SNAPSHOT);
    AtomBuffer snapshotBuf = msg->frame;
    if (mRawRequested > 0) {
        if (snapshotBuf.gfxInfo.gfxBufferHandle && mCallbacks->msgTypeEnabled(CAMERA_MSG_RAW_IMAGE)) {
            LOG1("snapshotBuf.size:%d", snapshotBuf.size);
            mCallbacks->allocateMemory(&tmpCopy.buff, snapshotBuf.size);
            if (tmpCopy.buff != NULL) {
                memcpy(tmpCopy.buff->data, snapshotBuf.dataPtr, snapshotBuf.size);
                releaseTmp = true;
            }
        } else {
            tmpCopy = snapshotBuf;
        }
        mCallbacks->rawFrameDone(&tmpCopy);
        mRawRequested--;
    }

    if (tmpCopy.buff != NULL && releaseTmp) {
        MemoryUtils::freeAtomBuffer(tmpCopy);
    }

    return status;
}

void CallbacksThread::extispFrame(const AtomBuffer &yuvBuf, int offset, int size)
{
    LOG1("@%s",__FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_EXTISP_FRAME;
    msg.data.extIspFrame.frame = yuvBuf;
    msg.data.extIspFrame.size = size;
    msg.data.extIspFrame.offset = offset;

    mMessageQueue.send(&msg);
}

status_t CallbacksThread::handleMessageExtIspFrame(MessageExtIspFrame *msg)
{
    LOG1("@%s",__FUNCTION__);
    status_t status = NO_ERROR;
    mCallbacks->extIspFrameDone(&msg->frame, msg->offset, msg->size);
    msg->frame.owner->returnBuffer(&msg->frame);
    return status;
}

status_t CallbacksThread::handleMessagePostviewFrameDone(MessageFrame *msg)
{
    LOG1("@%s",__FUNCTION__);
    status_t status = NO_ERROR;
    AtomBuffer tmpCopy = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_POSTVIEW);
    if (mPostviewRequested > 0) {
        tmpCopy = msg->frame;
        mCallbacks->postviewFrameDone(&tmpCopy);
        mPostviewRequested--;
    }
    return status;
}

status_t CallbacksThread::handleMessageAccManagerPointer(MessageAccManager *msg)
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    mCallbacks->accManagerPointer(msg->isp_ptr, msg->idx);
    return status;
}

status_t CallbacksThread::handleMessageAccManagerFinished()
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    mCallbacks->accManagerFinished();
    return status;
}

status_t CallbacksThread::handleMessageAccManagerPreviewBuffer(MessageAccManager *msg)
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    mCallbacks->accManagerPreviewBuffer(&(msg->buffer));
    return status;
}

status_t CallbacksThread::handleMessageAccManagerArgumentBuffer(MessageAccManager *msg)
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    mCallbacks->accManagerArgumentBuffer(&(msg->buffer));
    return status;
}

status_t CallbacksThread::handleMessageAccManagerMetadataBuffer(MessageAccManager *msg)
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    mCallbacks->accManagerMetadataBuffer(&(msg->buffer));
    return status;
}

void CallbacksThread::resumePreviewCallbacks()
{
    LOG1("@%s",__FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_RESUME_PREVIEW_CALLBACKS;
    mMessageQueue.send(&msg);
}

status_t CallbacksThread::handleMessageResumePreviewCallbacks()
{
    LOG1("@%s",__FUNCTION__);
    mPausePreviewCallbacks = false;
    return OK;
}

status_t CallbacksThread::waitForAndExecuteMessage()
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    Message msg;
    mMessageQueue.receive(&msg);

    switch (msg.id) {

        case MESSAGE_ID_EXIT:
            status = handleMessageExit();
            break;

        case MESSAGE_ID_PREVIEW_DONE:
            status = handleMessagePreviewDone(&msg.data.preview);
            break;

        case MESSAGE_ID_VIDEO_DONE:
            status = handleMessageVideoDone(&msg.data.video);
            break;

        case MESSAGE_ID_CALLBACK_SHUTTER:
            status = handleMessageCallbackShutter();
            break;

        case MESSAGE_ID_RESUME_PREVIEW_CALLBACKS:
            status = handleMessageResumePreviewCallbacks();
            break;

        case MESSAGE_ID_JPEG_DATA_READY:
            if (mWaitRendering) {
                LOG1("Postponed Jpeg callbacks due rendering");
                mPostponedJpegReady = msg;
                status = NO_ERROR;
            } else {
                status = handleMessageJpegDataReady(&msg.data.compressedFrame);
            }
            break;

        case MESSAGE_ID_POSTVIEW_RENDERED:
            status = handleMessagePostviewRendered();
            break;

        case MESSAGE_ID_JPEG_DATA_REQUEST:
            status = handleMessageJpegDataRequest(&msg.data.dataRequest);
            break;

        case MESSAGE_ID_AUTO_FOCUS_ACTIVE:
            status = handleMessageAutoFocusActive(&msg.data.autoFocusActive);
            break;

        case MESSAGE_ID_AUTO_FOCUS_DONE:
            status = handleMessageAutoFocusDone(&msg.data.autoFocusDone);
            break;

        case MESSAGE_ID_FOCUS_MOVE:
            status = handleMessageFocusMove(&msg.data.focusMove);
            break;

        case MESSAGE_ID_FLUSH:
            status = handleMessageFlush();
            break;

        case MESSAGE_ID_FACES:
            status = handleMessageFaces(&msg.data.faces);
            break;

        case MESSAGE_ID_SCENE_DETECTED:
            status = handleMessageSceneDetected(&msg.data.sceneDetected);
            break;

        case MESSAGE_ID_PANORAMA_DISPL_UPDATE:
            status = handleMessagePanoramaDisplUpdate(&msg.data.panoramaDisplUpdate);
            break;

        case MESSAGE_ID_PANORAMA_SNAPSHOT:
            status = handleMessagePanoramaSnapshot(&msg.data.panoramaSnapshot);
            break;

        case MESSAGE_ID_ULL_JPEG_DATA_REQUEST:
            status = handleMessageUllJpegDataRequest(&msg.data.ull);
            break;

        case MESSAGE_ID_ULL_TRIGGERED:
            status = handleMessageUllTriggered(&msg.data.ull);
            break;

        case MESSAGE_ID_ERROR_CALLBACK:
            status = handleMessageSendError(&msg.data.error);
            break;

        case MESSAGE_ID_LOW_BATTERY:
            status = handleMessageLowBattery();
            break;

        case MESSAGE_ID_RAW_FRAME_DONE:
            status = handleMessageRawFrameDone(&msg.data.rawFrame);
            break;

        case MESSAGE_ID_SS_FRAME_DONE:
            status = handleMessageSSFrameDone(&msg.data.smartStabilizationFrame);
            break;

        case MESSAGE_ID_POSTVIEW_FRAME_DONE:
            status = handleMessagePostviewFrameDone(&msg.data.postviewFrame);
            break;

        case MESSAGE_ID_EXTISP_FRAME:
            status = handleMessageExtIspFrame(&msg.data.extIspFrame);
            break;

        case MESSAGE_ID_ACC_POINTER:
            status = handleMessageAccManagerPointer(&msg.data.accManager);
            break;

        case MESSAGE_ID_ACC_FINISHED:
            status = handleMessageAccManagerFinished();
            break;

        case MESSAGE_ID_ACC_PREVIEW_BUFFER:
            status = handleMessageAccManagerPreviewBuffer(&msg.data.accManager);
            break;

        case MESSAGE_ID_ACC_ARGUMENT_BUFFER:
            status = handleMessageAccManagerArgumentBuffer(&msg.data.accManager);
            break;

        case MESSAGE_ID_ACC_METADATA_BUFFER:
            status = handleMessageAccManagerMetadataBuffer(&msg.data.accManager);
            break;

        case MESSAGE_ID_SEND_FRAME_ID:
            status = handleMessageSendFrameId(&msg.data.frameId);
            break;

        default:
            status = BAD_VALUE;
            break;
    };
    return status;
}

bool CallbacksThread::threadLoop()
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    mThreadRunning = true;
    while (mThreadRunning)
        status = waitForAndExecuteMessage();

    return false;
}

status_t CallbacksThread::requestExitAndWait()
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
 * Converts a Preview Gfx buffer into a regular buffer to be given to the user
 * The caller is responsible of freeing the memory allocated to the
 * regular buffer.
 * This is actually only allocating the struct camera_memory_t.
 * The actual image memory is re-used from the Gfx buffer.
 * Please remember that this memory is own by the native window and not the HAL
 * so we cannot de-allocate it.
 * Here we just present it to the client like any other buffer.
 *
 */
void CallbacksThread::convertGfx2Regular(AtomBuffer* aGfxBuf, AtomBuffer* aRegularBuf)
{
    LOG1("%s", __FUNCTION__);

    mCallbacks->allocateMemory(aRegularBuf, 0);
    aRegularBuf->buff->data = aGfxBuf->dataPtr;
    aRegularBuf->dataPtr = aRegularBuf->buff->data; // Keep the dataPtr in sync
    aRegularBuf->buff->size = aGfxBuf->size;
}
} // namespace android
