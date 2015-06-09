/*
 * Copyright (C) 2011 The Android Open Source Project
 * Copyright (c) 2014 Intel Corporation
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
#define LOG_TAG "Camera_AAAThread"

#include <time.h>
#include "LogHelper.h"
#include "CallbacksThread.h"
#include "AAAThread.h"
#include "FaceDetector.h"
#include "CameraDump.h"
#include "PerformanceTraces.h"
#include "PlatformData.h"

namespace android {

const size_t FLASH_FRAME_TIMEOUT = 5;
// TODO: use values relative to real sensor timings or fps
const unsigned int SKIP_PARTIALLY_EXPOSED = 1;

AAAThread::AAAThread(ICallbackAAA *aaaDone, UltraLowLight *ull, I3AControls *aaaControls, sp<CallbacksThread> callbacksThread, int cameraId, bool extIsp) :
    Thread(false)
    ,mMessageQueue("AAAThread", (int) MESSAGE_ID_MAX)
    ,mThreadRunning(false)
    ,m3AControls(aaaControls)
    ,mAAADoneCallback(aaaDone)
    ,mCallbacksThread(callbacksThread)
    ,mULL(ull)
    ,mCameraId(cameraId)
    ,m3ARunning(false)
    ,mStartAfSeqInMode(CAM_AF_MODE_NOT_SET)
    ,mWaitForScanStart(true)
    ,mStopAF(false)
    ,mPreviousCafStatus(CAM_AF_STATUS_IDLE)
    ,mPublicAeLock(false)
    ,mPublicAwbLock(false)
    ,mSmartSceneHdr(false)
    ,mPreviousFaceCount(0)
    ,mFlashStage(FLASH_STAGE_NA)
    ,mFramesTillExposed(0)
    ,mBlockForStage(FLASH_STAGE_NA)
    ,mSkipStatistics(0)
    ,mSkipForEv(0)
    ,mSensorEmbeddedMetaDataEnabled(false)
    ,mTrigger3A(0)
    ,mExtIsp(extIsp)
    ,mOrientation(0)
{
    LOG1("@%s", __FUNCTION__);
    mFaceState.faces = new ia_face[MAX_FACES_DETECTABLE];
    if (mFaceState.faces == NULL) {
        ALOGE("Error allocation memory for face state");
    } else {
        memset(mFaceState.faces, 0, MAX_FACES_DETECTABLE * sizeof(ia_face));
    }
    mFaceState.num_faces = 0;
    CLEAR(mCachedStatsEventMsg);
}

AAAThread::~AAAThread()
{
    LOG1("@%s", __FUNCTION__);
    delete [] mFaceState.faces;
    mFaceState.faces = NULL;
    mFaceState.num_faces = 0;
    CLEAR(mCachedStatsEventMsg);
}

status_t AAAThread::enable3A()
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_ENABLE_AAA;
    return mMessageQueue.send(&msg, MESSAGE_ID_ENABLE_AAA);
}

/**
 * Enters to FlashStage-machine by setting the initial stage.
 * Following newFrame()'s are tracked by handleFlashSequence().
 *
 * @param blockForStage runFlashSequence() is left blocked until
 *                      FlashStage-machine enters the requested state or
 *                      there's failure in sequence.
 */
status_t AAAThread::enterFlashSequence(FlashStage blockForStage)
{
    LOG1("@%s", __FUNCTION__);
    if (blockForStage == FLASH_STAGE_EXIT)
        return BAD_VALUE;
    Message msg;
    msg.id = MESSAGE_ID_FLASH_STAGE;
    msg.data.flashStage.value = blockForStage;
    return mMessageQueue.send(&msg,
            (blockForStage != FLASH_STAGE_NA) ? MESSAGE_ID_FLASH_STAGE : (MessageId) -1);
}

/**
 * Exits or interrupts the current flash sequence
 *
 * Must always be called after runFlashSequence() since sequence
 * continues until exposed or failure and remains the state until
 * this function is called.
 *
 * e.g for normal pre-flash sequence client calls:
 *
 * 1. runFlashSequence(FLASH_STAGE_PRE_FLASH_EXPOSED);
 *  - blocks until exposed frame is received or a failure
 *
 * 2. preview keeps running in exposed state keeping 3A not processed
 *
 * 3. endFlashSequence()
 *  - client wants to exit flash sequence and let normal 3A to continue
 */
status_t AAAThread::exitFlashSequence()
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_FLASH_STAGE;
    msg.data.flashStage.value = FLASH_STAGE_EXIT;
    return mMessageQueue.send(&msg);
}

/**
 * Sets AE lock status.
 *
 * This lock status is maintained also outside the autofocus
 * sequence.
 */
status_t AAAThread::lockAe(bool en)
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_ENABLE_AE_LOCK;
    msg.data.enable.enable = en;
    return mMessageQueue.send(&msg, MESSAGE_ID_ENABLE_AE_LOCK);
}

/**
 * Sets AWB lock status.
 *
 * This lock status is maintained also outside the autofocus
 * sequence.
 */
status_t AAAThread::lockAwb(bool en)
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_ENABLE_AWB_LOCK;
    msg.data.enable.enable = en;
    return mMessageQueue.send(&msg, MESSAGE_ID_ENABLE_AWB_LOCK);
}

status_t AAAThread::autoFocus()
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_AUTO_FOCUS;
    return mMessageQueue.send(&msg);
}

status_t AAAThread::cancelAutoFocus()
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_CANCEL_AUTO_FOCUS;
    return mMessageQueue.send(&msg);
}

/**
 * override for IAtomIspObserver::atomIspNotify()
 *
 * signal start of 3A processing based on 3A statistics available event
 * store SOF event information for future use
 */
bool AAAThread::atomIspNotify(IAtomIspObserver::Message *msg, const ObserverState state)
{
    if(msg && msg->id == IAtomIspObserver::MESSAGE_ID_EVENT) {
        if (mSensorEmbeddedMetaDataEnabled || msg->data.event.type == EVENT_TYPE_METADATA_READY) {
            mSensorEmbeddedMetaDataEnabled = true;
            // When both sensor metadata event and statistics event are ready, then triggers 3A run
            if (msg->data.event.type == EVENT_TYPE_METADATA_READY) {
                mTrigger3A |= EVENT_TYPE_METADATA_READY;
                if (mTrigger3A & EVENT_TYPE_STATISTICS_READY) {
                    mTrigger3A = 0;
                    newStats(mCachedStatsEventMsg.data.event.timestamp, mCachedStatsEventMsg.data.event.sequence);
                    CLEAR(mCachedStatsEventMsg);
                }
                return false;
            }
            if (msg->data.event.type == EVENT_TYPE_STATISTICS_READY) {
                mTrigger3A |= EVENT_TYPE_STATISTICS_READY;
                if (mTrigger3A & EVENT_TYPE_METADATA_READY) {
                    mTrigger3A = 0;
                    newStats(msg->data.event.timestamp, msg->data.event.sequence);
                } else {
                    mCachedStatsEventMsg = *msg;
                }
                return false;
            }
        }else if (msg->data.event.type == EVENT_TYPE_STATISTICS_READY) {
            LOG2("-- STATS READY, seq %d, ts %lldus, systemTime %lldms ---",
                                   msg->data.event.sequence,
                                     nsecs_t(msg->data.event.timestamp.tv_sec)*1000000LL
                                   + nsecs_t(msg->data.event.timestamp.tv_usec),
                                   systemTime()/1000/1000);
            newStats(msg->data.event.timestamp, msg->data.event.sequence);
        }
    } else if (msg && msg->id == IAtomIspObserver::MESSAGE_ID_FRAME) {
        LOG2("--- FRAME, seq %d, ts %lldms, systemTime %lldms ---",
                msg->data.frameBuffer.buff.frameSequenceNbr,
                  nsecs_t(msg->data.frameBuffer.buff.capture_timestamp.tv_sec)*1000000LL
                + nsecs_t(msg->data.frameBuffer.buff.capture_timestamp.tv_usec),
                systemTime()/1000/1000);
        newFrame(&msg->data.frameBuffer.buff);
    }
    return false;
}

status_t AAAThread::newStats(timeval &t, unsigned int seqNo)
{
    LOG2("@%s", __FUNCTION__);
    Message msg;

    msg.id = MESSAGE_ID_NEW_STATS_READY;
    msg.data.stats.capture_timestamp = t;
    msg.data.stats.sequence_number = seqNo;

    return mMessageQueue.send(&msg);
}

status_t AAAThread::newFrame(AtomBuffer *b)
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    Message msg;
    msg.id = MESSAGE_ID_NEW_FRAME;
    msg.data.frame.buff = b;

    status = mMessageQueue.send(&msg);
    return status;
}

status_t AAAThread::switchModeAndRate(AtomMode mode, float fps)
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    Message msg;
    msg.id = MESSAGE_ID_SWITCH_MODE_AND_RATE;
    msg.data.switchInfo.mode = mode;
    msg.data.switchInfo.fps = fps;
    status = mMessageQueue.send(&msg, MESSAGE_ID_SWITCH_MODE_AND_RATE);
    return status;
}

status_t AAAThread::setFaces(const ia_face_state& faceState)
{
    LOG2("@%s", __FUNCTION__);
    status_t status(NO_ERROR);

    if (mFaceState.faces == NULL) {
        ALOGE("face state not allocated");
        return NO_INIT;
    }

    if (faceState.num_faces > MAX_FACES_DETECTABLE) {
        ALOGW("@%s: %d faces detected, limiting to %d", __FUNCTION__,
            faceState.num_faces, MAX_FACES_DETECTABLE);
         mFaceState.num_faces = MAX_FACES_DETECTABLE;
    } else {
         mFaceState.num_faces = faceState.num_faces;
    }

    memcpy(mFaceState.faces, faceState.faces, mFaceState.num_faces * sizeof(ia_face));

    return status;
}

int32_t AAAThread::getFaceNum(void) const
{
    LOG1("@%s", __FUNCTION__);
    return mFaceState.num_faces;
}

status_t AAAThread::getFaces(ia_face_state &faceState) const
{
    LOG1("@%s", __FUNCTION__);

    if (mFaceState.faces == NULL) {
        ALOGE("face state not allocated");
        return NO_INIT;
    }

    if (mFaceState.num_faces == 0) {
        LOG1("No face detection information can be gotten");
        return INVALID_OPERATION;
    }

    faceState.num_faces = mFaceState.num_faces;
    if (faceState.num_faces > MAX_FACES_DETECTABLE) {
        ALOGW("@%s: %d faces detected, limiting to %d", __FUNCTION__,
                faceState.num_faces, MAX_FACES_DETECTABLE);
        faceState.num_faces = MAX_FACES_DETECTABLE;
    }

    memcpy(faceState.faces, mFaceState.faces, faceState.num_faces * sizeof(ia_face));

    return NO_ERROR;
}

status_t AAAThread::handleMessageExit()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    mThreadRunning = false;
    m3ARunning = false;
    SensorThread::getInstance()->unRegisterOrientationListener(this);

    return status;
}

status_t AAAThread::handleMessageEnable3A()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    m3ARunning = true;
    mOrientation = SensorThread::getInstance()->registerOrientationListener(this);
    mMessageQueue.reply(MESSAGE_ID_ENABLE_AAA, status);
    return status;
}

status_t AAAThread::handleMessageEnableAeLock(MessageEnable* msg)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    mPublicAeLock = msg->enable;

    // during AF, AE lock is controlled by AF, otherwise
    // set the value here
    if (mStartAfSeqInMode == CAM_AF_MODE_NOT_SET)
        m3AControls->setAeLock(msg->enable);

    mMessageQueue.reply(MESSAGE_ID_ENABLE_AE_LOCK, status);
    return status;
}

status_t AAAThread::handleMessageEnableAwbLock(MessageEnable* msg)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    mPublicAwbLock = msg->enable;

    // during AF, AWB lock is controlled by AF, otherwise
    // set the value here
    if (mStartAfSeqInMode == CAM_AF_MODE_NOT_SET)
        m3AControls->setAwbLock(msg->enable);

    mMessageQueue.reply(MESSAGE_ID_ENABLE_AWB_LOCK, status);
    return status;
}

status_t AAAThread::handleMessageAutoFocus()
{
    LOG1("@%s", __FUNCTION__);
    status_t status(NO_ERROR);
    AfMode currAfMode(m3AControls->getAfMode());

    // Run the "normal" still AF sequence
    if (currAfMode == CAM_AF_MODE_AUTO ||
        currAfMode == CAM_AF_MODE_MACRO ||
        currAfMode == CAM_AF_MODE_CONTINUOUS ||
        currAfMode == CAM_AF_MODE_CONTINUOUS_VIDEO) {

        if (currAfMode == CAM_AF_MODE_CONTINUOUS ||
            currAfMode == CAM_AF_MODE_CONTINUOUS_VIDEO) {
            // If we are not busy in continuous-focus mode, we should
            // return immediately with the current status
            AfStatus cafStatus = m3AControls->getCAFStatus();
            if (cafStatus != CAM_AF_STATUS_BUSY) {
                m3AControls->setAfLock(true);
                mCallbacksThread->autoFocusDone(cafStatus == CAM_AF_STATUS_SUCCESS);
                return status;
            }
        }

        if (m3AControls->isIntel3A()) {
            m3AControls->setAfEnabled(true);
            // state of client requested 3A locks is kept, so it
            // is safe to override the values here
            m3AControls->setAwbLock(true);

            m3AControls->startStillAf();

            // Turning on the flash as AF assist light doesn't support
            // proper synchronisation with frames. We skip fixed amount
            // of statistics in order to prevent partially exposed frames
            // to get processed.
            if (m3AControls->getAfNeedAssistLight())
                mSkipStatistics = SKIP_PARTIALLY_EXPOSED;

            mFramesTillAfComplete = 0;
        } else if (mExtIsp) {
            m3AControls->setAfEnabled(true);
            m3AControls->startStillAf();
        }

        mStartAfSeqInMode = currAfMode;
        mStopAF = false;
    } else {
        // FIXME: Should be called immediately for SOC/fixed focus cameras
        mStartAfSeqInMode = CAM_AF_MODE_NOT_SET;
        mCallbacksThread->autoFocusDone(true);
    }

    return status;
}

status_t AAAThread::handleAutoFocusExtIsp(const AtomBuffer *buff)
{
    LOG2("@%s", __FUNCTION__);
    bool stopStillAf = false;
    AfStatus afStatus = CAM_AF_STATUS_FAIL;

    // TODO: Optimize the AF meta parsing to when we actually need the
    // status data...
    afStatus = parseAfMeta(buff);

    AfMode currAfMode = m3AControls->getAfMode();
    if (currAfMode == CAM_AF_MODE_CONTINUOUS) {
        // Check CAF status:
        bool focusMoving = false;

        if (afStatus == CAM_AF_STATUS_BUSY || afStatus == CAM_AF_STATUS_IDLE) {
            focusMoving = true;
        }

        LOG1("CAF move (ext-isp): %d", focusMoving);

        if (afStatus != mPreviousCafStatus) {
            // Send the callback to upper layer to inform about the CAF status.
            mCallbacksThread->focusMove(focusMoving);
        }
        mPreviousCafStatus = afStatus;
        return NO_ERROR;
    }

    if (mStartAfSeqInMode == CAM_AF_MODE_AUTO ||
        mStartAfSeqInMode == CAM_AF_MODE_MACRO ||
        mStartAfSeqInMode == CAM_AF_MODE_CONTINUOUS ||
        mStartAfSeqInMode == CAM_AF_MODE_CONTINUOUS_VIDEO) {
        // TODO: Check AF for ext-ISP, that it still works..
        // Status for normal AF sequence:
        if (afStatus == CAM_AF_STATUS_BUSY) {
            // metadata has indicated that AF is running:
            mWaitForScanStart = false;
            LOG1("StillAFExtIsp BUSY    (continuing...)");
        } else if (afStatus == CAM_AF_STATUS_SUCCESS) {
            LOG1("StillAFExtIsp SUCCESS (stopping...)");
            stopStillAf = true;
        } else if (afStatus == CAM_AF_STATUS_FAIL) {
            LOG1("StillAFExtIsp FAIL    (stopping...)");
            stopStillAf = true;
        }

        if (mWaitForScanStart) {
            // We are still waiting for the NV12 metadata to indicate that AF is actually running
            // a new sequence on ext-ISP. We need to wait AF start, as metadata still indicates previous AF run
            // status.
            // Do nothing for this frame:
            return NO_ERROR;
        } else if (stopStillAf) {
            m3AControls->stopStillAf();
            // Stop the AF once we are done.
            m3AControls->setAfEnabled(false);
            mCallbacksThread->autoFocusDone(afStatus == CAM_AF_STATUS_SUCCESS);
            // Some AF status was reached, stop the sequence:
            mStartAfSeqInMode = CAM_AF_MODE_NOT_SET;
            mWaitForScanStart = true;
            m3AControls->setAfLock(true);
        }
    }

    return NO_ERROR;
}

/**
 * External ISP specific (m10mo) metadata parser for getting AF/CAF status
 */
AfStatus AAAThread::parseAfMeta(const AtomBuffer *buff)
{
    LOG2("@%s", __FUNCTION__);
    unsigned char *nv12Meta = ((unsigned char*)buff->auxBuf->dataPtr) + NV12_META_START;

    // metadata is big-endian. getU16FromFrame() converts to host format:
    uint16_t afState = getU16fromFrame(nv12Meta, NV12_META_AF_STATE_ADDR);
    AfStatus status = CAM_AF_STATUS_FAIL;

    LOG2("Ext-isp AF state: 0x%x", afState);

    switch (afState) {
        case CAF_RESTART_CHECK:
        case AF_INVALID:
            // TODO: Check: Is this OK for CAF restart?
            status = CAM_AF_STATUS_IDLE;
            break;
        case CAF_FOCUSING:
        case AF_FOCUSING:
            status = CAM_AF_STATUS_BUSY;
            break;
        case CAF_SUCCESS:
        case AF_SUCCESS:
            status = CAM_AF_STATUS_SUCCESS;
            break;
        case CAF_FAIL:
        case AF_FAIL:
            status = CAM_AF_STATUS_FAIL;
            break;
        default:
            status = CAM_AF_STATUS_FAIL;
            ALOGW("Unknown AF/CAF state 0x%x, using CAM_AF_STATUS_FAIL", afState);
            break;
    }

    return status;
}

status_t AAAThread::handleMessageCancelAutoFocus()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    if (mStartAfSeqInMode != CAM_AF_MODE_NOT_SET) {
        // For ext-isp, only stop the AF
        if (mExtIsp) {
            m3AControls->setAfEnabled(false);
        }

        mStopAF = true;
        mWaitForScanStart = true;
    }

    // Set AF lock off to enable continuous AF again
    m3AControls->setAfLock(false);

    return status;
}

status_t AAAThread::handleMessageFlashStage(MessageFlashStage *msg)
{
    status_t status = NO_ERROR;
    LOG1("@%s", __FUNCTION__);

    // handle exitFlashSequence()
    if (msg->value == FLASH_STAGE_EXIT) {
        if (mBlockForStage != FLASH_STAGE_PRE_EXPOSED &&
            mBlockForStage != FLASH_STAGE_SHOT_EXPOSED &&
            mBlockForStage != FLASH_STAGE_NA) {
            LOG1("Flash sequence interrupted");
        }
        if (mBlockForStage != FLASH_STAGE_NA) {
            mMessageQueue.reply(MESSAGE_ID_FLASH_STAGE, status);
            mBlockForStage = FLASH_STAGE_NA;
        }
        mFlashStage = FLASH_STAGE_NA;
        return NO_ERROR;
    }

    // handle enterFlashSequence()
    if (mFlashStage != FLASH_STAGE_NA) {
        status = ALREADY_EXISTS;
        ALOGE("Flash sequence already started");
        if (msg->value != FLASH_STAGE_NA)
            mMessageQueue.reply(MESSAGE_ID_FLASH_STAGE, status);
        return status;
    }

    mBlockForStage = msg->value;
    if (mBlockForStage == FLASH_STAGE_SHOT_EXPOSED) {
        // TODO: Not receiving expose statuses for snapshot frames
        // ControlThread does snapshot capturing atm for this purpose.
        ALOGD("Not Implemented! its a deadlock");
        mFlashStage = FLASH_STAGE_SHOT_WAITING;
    } else {
        // Enter pre-flash sequence by default
        mFlashStage = FLASH_STAGE_PRE_START;
    }
    return NO_ERROR;
}

/**
 * handles FlashStage-machine states based on newFrame()'s
 *
 * returns true if sequence is running and normal 3A should
 * not be executed
 */
bool AAAThread::handleFlashSequence(FrameBufferStatus frameStatus, struct timeval capture_timestamp, uint32_t expId)
{
    // TODO: Make aware of frame sync and changes in exposure to
    //       reduce unneccesary skipping and consider processing for
    //       every frame with AtomAIQ
    status_t status = NO_ERROR;

    if (mFlashStage == FLASH_STAGE_NA) {
        if (mBlockForStage != FLASH_STAGE_NA) {
            // should never occur
            LOG1("Releasing runFlashSequence(), sequence not started");
            mMessageQueue.reply(MESSAGE_ID_FLASH_STAGE, UNKNOWN_ERROR);
            mBlockForStage = FLASH_STAGE_NA;
        }
        return false;
    }

    if (mStartAfSeqInMode != CAM_AF_MODE_NOT_SET) {
        LOG1("AF running while entering flash sequence, stopping AF");
        // Stop the AF sequence in case we are running AF and are entering
        // pre-flash. We are skipping the statistics
        // event handling int the pre-flash sequence (see: handleMessageNewStats()),
        // which causes the still AF sequence to get stuck, when entering pre-flash.
        m3AControls->stopStillAf();
        mStartAfSeqInMode = CAM_AF_MODE_NOT_SET;
        mStopAF = false;
    }

    LOG2("@%s : mFlashStage %d, FrameStatus %d", __FUNCTION__, mFlashStage, frameStatus);

    switch (mFlashStage) {
        case FLASH_STAGE_PRE_START:
            // hued images fix (BZ: 72908)
            if (mSkipForEv++ < m3AControls->getExposureDelay()) {
                LOG2("@%s : Frame skipped to wait correct exposure", __FUNCTION__);
                break;
            }
            // Enter Stage 1
            mFramesTillExposed = 0;
            mSkipForEv = m3AControls->getExposureDelay();
            status = m3AControls->applyPreFlashProcess(CAM_FLASH_STAGE_NONE, capture_timestamp, mOrientation, expId);
            mFlashStage = FLASH_STAGE_PRE_PHASE1;
            break;
        case FLASH_STAGE_PRE_PHASE1:
            // Stage 1.5: Skip frames to get exposure from Stage 1.
            //            First frame is for sensor to pick up the new value
            //            and second for sensor to apply it.
            if (mSkipForEv-- > 0) {
                LOG2("@%s : Frame skipped to wait correct exposure", __FUNCTION__);
                break;
            }
            // Enter Stage 2
            mSkipForEv = m3AControls->getExposureDelay();
            status = m3AControls->applyPreFlashProcess(CAM_FLASH_STAGE_PRE, capture_timestamp, mOrientation, expId);
            mFlashStage = FLASH_STAGE_PRE_PHASE2;
            break;
        case FLASH_STAGE_PRE_PHASE2:
            // Stage 2.5: Same as above, but for Stage 2.
            if (mSkipForEv-- > 0) {
                LOG2("@%s : Frame skipped to wait correct exposure", __FUNCTION__);
                break;
            }
            // Enter Stage 3: get the flash-exposed preview frame
            // and let the 3A library calculate the exposure
            // settings for the flash-exposed still capture.
            // We check the frame status to make sure we use
            // the flash-exposed frame.
            mSkipForEv = 0;
            status = m3AControls->setFlash(1);
            mFlashStage = FLASH_STAGE_PRE_WAITING;
            break;
        case FLASH_STAGE_SHOT_WAITING:
        case FLASH_STAGE_PRE_WAITING:
            mFramesTillExposed++;
            if (frameStatus == FRAME_STATUS_FLASH_EXPOSED) {
                m3AControls->setFlash(0);
                m3AControls->applyPreFlashProcess(CAM_FLASH_STAGE_MAIN, capture_timestamp, mOrientation, expId);
                if (mFlashStage == FLASH_STAGE_SHOT_WAITING) {
                    LOG1("ShotFlash@Frame %d: SUCCESS    (stopping...)", mFramesTillExposed);
                    mFlashStage = FLASH_STAGE_SHOT_EXPOSED;
                } else if (m3AControls->getAeFlashNecessity() == CAM_FLASH_STAGE_PRE) {
                    LOG1("PreFlash@Frame %d: SUCCESS     (repeat...)", mFramesTillExposed);
                    mFramesTillExposed = 0;
                    mSkipForEv = m3AControls->getExposureDelay();
                    mFlashStage = FLASH_STAGE_PRE_PHASE2;
                } else {
                    LOG1("PreFlash@Frame %d: SUCCESS    (stopping...)", mFramesTillExposed);
                    mFlashStage = FLASH_STAGE_PRE_EXPOSED;
                }
            } else if(mFramesTillExposed > FLASH_FRAME_TIMEOUT
                   || ( frameStatus != FRAME_STATUS_OK
                        && frameStatus != FRAME_STATUS_FLASH_PARTIAL) ) {
                ALOGW("PreFlash@Frame %d: FAILED     (stopping...)", mFramesTillExposed);
                status = UNKNOWN_ERROR;
                m3AControls->setFlash(0);
                break;
            }
            status = NO_ERROR;
            break;
        case FLASH_STAGE_PRE_EXPOSED:
        case FLASH_STAGE_SHOT_EXPOSED:
            // staying in exposed state and not processing 3A until
            status = NO_ERROR;
            break;
        default:
        case FLASH_STAGE_EXIT:
        case FLASH_STAGE_NA:
            status = UNKNOWN_ERROR;
            break;
    }

    if (mBlockForStage == mFlashStage) {
        LOG2("Releasing runFlashSequence()");
        mMessageQueue.reply(MESSAGE_ID_FLASH_STAGE, status);
        mBlockForStage = FLASH_STAGE_NA;
    }

    if (status != NO_ERROR) {
        ALOGD("Flash sequence failed!");
        mFramesTillExposed = 0;
        mSkipForEv = 0;
        mFlashStage = FLASH_STAGE_NA;
        if (mBlockForStage != FLASH_STAGE_NA) {
            LOG2("Releasing runFlashSequence()");
            mMessageQueue.reply(MESSAGE_ID_FLASH_STAGE, status);
            mBlockForStage = FLASH_STAGE_NA;
        }
        return false;
    }

    return true;
}

/**
 * New preview frame available
 * 3A thread needs this message only during the pre-flash sequence
 * For normal 3A operation it runs from the 3A statistics ready event
 */
status_t AAAThread::handleMessageNewFrame(MessageNewFrame *msg)
{
    LOG1("@%s: status: %d", __FUNCTION__, msg->buff->status);

    // TODO: Refactor this to be more general, instead of having ext-ISP specific
    // code path for autofocus and flash sequence. Ideally both could be called and check
    // for applying given operation is in the function itself. This of course depends on who
    // is responsible for runnig the flash sequence, us as camera HAL. Or, someone else, like ext-ISP firmware

    // At this point this makes sense due to the fact that Intel AIQ needs
    // 'new frame'-event for flash sequence only. and ext-ISP for AF status reporting only.
    if (mExtIsp) {
        // external ISP autoFocus
        handleAutoFocusExtIsp(msg->buff);
    } else {
        handleFlashSequence(msg->buff->status, msg->buff->capture_timestamp, msg->buff->expId);
    }

    return NO_ERROR;
}

/**
 * Run 3A and DVS processing
 * We received message that new 3A statistics are ready
 */
status_t AAAThread::handleMessageNewStats(MessageNewStats *msgFrame)
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    String8 sceneMode;
    bool sceneHdr = false;
    Vector<Message> messages;
    struct timeval capture_timestamp = msgFrame->capture_timestamp;
    unsigned int capture_sequence_number = msgFrame->sequence_number;

    if (!m3ARunning)
        return status;

    /* Do not run 3A if we are in the pre-flash sequence */
    if (mFlashStage != FLASH_STAGE_NA) {
        // NOTE: For flash sequence the 3A is driven by preview frames,
        // but unread stats will be left queued in the driver, since we skip the stats reading here
        // For up-to-date statistics in flash sequence, the old stats will be dequeued in
        // AtomAIQ::getStatistics()
        return status;
    }

    if (mSkipStatistics > 0) {
        LOG1("3A statistics skipped. Partially exposed or wait for exposure. (%d)", mSkipStatistics);
        --mSkipStatistics;
        return status;
    }
    // 3A & DVS stats are read with proprietary ioctl that returns the
    // statistics of most recent frame done.
    // Multiple newFrames indicates we are late and 3A process is going
    // to read the statistics of the most recent frame.
    // We flush the queue and use the most recent timestamp.
    mMessageQueue.remove(MESSAGE_ID_NEW_STATS_READY, &messages);
    if(!messages.isEmpty()) {
        Vector<Message>::iterator it = messages.begin();
        ALOGW("%d frames in 3A process queue, handling timestamp "
             "%lld instead of %lld\n", messages.size(),
        ((long long)(it->data.stats.capture_timestamp.tv_sec)*1000000LL +
         (long long)(it->data.stats.capture_timestamp.tv_usec)),
        ((long long)(capture_timestamp.tv_sec)*1000000LL +
         (long long)(capture_timestamp.tv_usec)));
        capture_timestamp = it->data.stats.capture_timestamp;
        capture_sequence_number = it->data.stats.sequence_number;
    }

    if(m3ARunning){
        // Run 3A statistics
        status = m3AControls->apply3AProcess(true, &capture_timestamp, mOrientation);

        // Flag for not sendig CAF move callbacks during AF sequence.
        // This is needed as AF sequence may be run, even when AF mode is CAF
        bool afDuringCaf = false;

        // If normal auto-focus was requested, run auto-focus sequence
        if (status == NO_ERROR &&
            (mStartAfSeqInMode == CAM_AF_MODE_AUTO ||
             mStartAfSeqInMode == CAM_AF_MODE_MACRO ||
             mStartAfSeqInMode == CAM_AF_MODE_CONTINUOUS ||
             mStartAfSeqInMode == CAM_AF_MODE_CONTINUOUS_VIDEO)) {

            if (mStartAfSeqInMode == CAM_AF_MODE_CONTINUOUS ||
                mStartAfSeqInMode == CAM_AF_MODE_CONTINUOUS_VIDEO) {
                // Enable the flag to skip focusMove() callback during AF sequence.
                afDuringCaf = true;
            }

            // Check for cancel-focus
            AfStatus afStatus = CAM_AF_STATUS_FAIL;
            if (mStopAF) {
                afStatus = CAM_AF_STATUS_FAIL;
            } else {
                afStatus = m3AControls->isStillAfComplete();
                mFramesTillAfComplete++;
            }
            bool stopStillAf = false;

            if (afStatus == CAM_AF_STATUS_BUSY) {
                LOG1("StillAF@Frame %d: BUSY    (continuing...)", mFramesTillAfComplete);
            } else if (afStatus == CAM_AF_STATUS_SUCCESS) {
                LOG1("StillAF@Frame %d: SUCCESS (stopping...)", mFramesTillAfComplete);
                stopStillAf = true;
            } else if (afStatus == CAM_AF_STATUS_FAIL) {
                LOG1("StillAF@Frame %d: FAIL    (stopping...)", mFramesTillAfComplete);
                stopStillAf = true;
            }

            if (stopStillAf) {
                if (m3AControls->getAfNeedAssistLight()) {
                    // skip frames until we get result with orginal exposure settings
                    unsigned int exposureDelay = m3AControls->getExposureDelay();
                    mSkipStatistics = MAX(SKIP_PARTIALLY_EXPOSED, exposureDelay);
                }
                m3AControls->stopStillAf();
                m3AControls->setAeLock(mPublicAeLock);
                m3AControls->setAwbLock(mPublicAwbLock);
                m3AControls->setAfEnabled(false);

                if (mStartAfSeqInMode == CAM_AF_MODE_CONTINUOUS ||
                    mStartAfSeqInMode == CAM_AF_MODE_CONTINUOUS_VIDEO) {
                    m3AControls->setAfLock(true);
                }

                mStartAfSeqInMode = CAM_AF_MODE_NOT_SET;
                mStopAF = false;
                mFramesTillAfComplete = 0;
                mCallbacksThread->autoFocusDone(afStatus == CAM_AF_STATUS_SUCCESS);
            }

        }

        AfMode currAfMode = m3AControls->getAfMode();
        if (!m3AControls->getAfLock() && !afDuringCaf &&
            (currAfMode == CAM_AF_MODE_CONTINUOUS ||
             currAfMode == CAM_AF_MODE_CONTINUOUS_VIDEO)) {
            // Inform app about the CAF move when the focus is *not* locked.
            AfStatus cafStatus = m3AControls->getCAFStatus();
            LOG2("CAF move lens status: %d", cafStatus);

            if (cafStatus != mPreviousCafStatus) {
                bool focusMoving = false;
                if (cafStatus == CAM_AF_STATUS_BUSY) {
                    focusMoving = true;
                }
                LOG2("CAF move: %d", focusMoving);
                // Send the callback to upper layer and inform about the CAF status.
                mCallbacksThread->focusMove(focusMoving);
                mPreviousCafStatus = cafStatus;
            }

            if (cafStatus == CAM_AF_STATUS_SUCCESS)
                PerformanceTraces::Launch2FocusLock::stop();
        }

        // Set face data to 3A only if there were detected faces and avoid unnecessary
        // setting with consecutive zero face count.
        if (!(mFaceState.num_faces == 0 && mPreviousFaceCount == 0)) {
            m3AControls->setFaces(mFaceState);
            mPreviousFaceCount = mFaceState.num_faces;
        }

        // Query the detected scene and notify the application
        if (m3AControls->getSmartSceneDetection()) {
            m3AControls->getSmartSceneMode(sceneMode, sceneHdr);
            if (sceneMode != mSmartSceneMode || sceneHdr != mSmartSceneHdr) {
                LOG1("SmartScene: new scene detected: %s, HDR: %d", sceneMode.string(), sceneHdr);
                mSmartSceneMode = sceneMode;
                mSmartSceneHdr = sceneHdr;
                mAAADoneCallback->sceneDetected(sceneMode, sceneHdr);
            }
        }
        /**
         * query 3A and update ULL trigger
         */
        updateULLTrigger();
    }

    return status;
}

status_t AAAThread::handleMessageSwitchModeAndRate(MessageSwitchInfo *msg)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    status = m3AControls->switchModeAndRate(msg->mode, msg->fps);
    mMessageQueue.reply(MESSAGE_ID_SWITCH_MODE_AND_RATE, status);
    return status;
}

void AAAThread::getCurrentSmartScene(String8 &sceneMode, bool &sceneHdr)
{
    LOG1("@%s", __FUNCTION__);
    sceneMode = mSmartSceneMode;
    sceneHdr = mSmartSceneHdr;
}

/**
 *  updateULLTrigger
 *
 *  query 3A gain and exposure and update the ULL trigger
 *
 */
void AAAThread::updateULLTrigger()
{
    LOG2("%s",__FUNCTION__);

    if (mULL) {
        bool trigger = m3AControls->getAeUllTrigger();
        mULL->updateTrigger(trigger);
    }
}

void AAAThread::resetSmartSceneValues()
{
    LOG1("@%s", __FUNCTION__);
    mSmartSceneMode.clear();
    mSmartSceneHdr = false;
}

void AAAThread::orientationChanged(int orientation)
{
    LOG1("@%s: orientation = %d", __FUNCTION__, orientation);
    Message msg;
    msg.id = MESSAGE_ID_SET_ORIENTATION;
    msg.data.orientation.orientation = orientation;
    mMessageQueue.send(&msg);
}

status_t AAAThread::handleMessageSetOrientation(MessageOrientation *msg)
{
    LOG1("@%s: orientation = %d", __FUNCTION__, msg->orientation);
    mOrientation = msg->orientation;
    return NO_ERROR;
}

status_t AAAThread::waitForAndExecuteMessage()
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    Message msg;
    mMessageQueue.receive(&msg);

    if (msg.id != MESSAGE_ID_EXIT && PlatformData::isDisable3A(mCameraId)) {
        if (msg.id == MESSAGE_ID_AUTO_FOCUS)
            mCallbacksThread->autoFocusDone(true);
        mMessageQueue.reply(msg.id, status);
        return status;
    }

    switch (msg.id) {

        case MESSAGE_ID_EXIT:
            status = handleMessageExit();
            break;

        case MESSAGE_ID_NEW_STATS_READY:
            status = handleMessageNewStats(&msg.data.stats);
            break;

        case MESSAGE_ID_ENABLE_AAA:
            status = handleMessageEnable3A();
            break;

        case MESSAGE_ID_AUTO_FOCUS:
            status = handleMessageAutoFocus();
            break;

        case MESSAGE_ID_CANCEL_AUTO_FOCUS:
            status = handleMessageCancelAutoFocus();
            break;

        case MESSAGE_ID_NEW_FRAME:
            status = handleMessageNewFrame(&msg.data.frame);
            break;

        case MESSAGE_ID_ENABLE_AE_LOCK:
            status = handleMessageEnableAeLock(&msg.data.enable);
            break;

        case MESSAGE_ID_ENABLE_AWB_LOCK:
            status = handleMessageEnableAwbLock(&msg.data.enable);
            break;

        case MESSAGE_ID_FLASH_STAGE:
            status = handleMessageFlashStage(&msg.data.flashStage);
            break;

        case MESSAGE_ID_SWITCH_MODE_AND_RATE:
            status = handleMessageSwitchModeAndRate(&msg.data.switchInfo);
            break;

        case MESSAGE_ID_SET_ORIENTATION:
            status = handleMessageSetOrientation(&msg.data.orientation);
            break;

        default:
            status = BAD_VALUE;
            break;
    };
    return status;
}

bool AAAThread::threadLoop()
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    mThreadRunning = true;
    while (mThreadRunning)
        status = waitForAndExecuteMessage();

    return false;
}

status_t AAAThread::requestExitAndWait()
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
