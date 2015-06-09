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
#define LOG_TAG "Camera_BracketManager"
//#define LOG_NDEBUG 0

#include "LogHelper.h"
#include "PlatformData.h"
#include "BracketManager.h"
#include "PerformanceTraces.h"

namespace android {

BracketManager::BracketManager(HWControlGroup &hwcg, I3AControls *aaaControls) :
    Thread(false)
    ,m3AControls(aaaControls)
    ,mISP(hwcg.mIspCI)
    ,mSensorCI(hwcg.mSensorCI)
    ,mFpsAdaptSkip(-1)
    ,mBurstLength(-1)
    ,mBurstCaptureNum(-1)
    ,mSnapshotReqNum(-1)
    ,mBracketNum(-1)
    ,mLastFrameSequenceNbr(-1)
    ,mState(STATE_STOPPED)
    ,mMessageQueue("BracketManager", (int) MESSAGE_ID_MAX)
    ,mThreadRunning(false)
{
    LOG1("@%s", __FUNCTION__);
    mBracketing.mode = BRACKET_NONE;
}

BracketManager::~BracketManager()
{
    LOG1("@%s", __FUNCTION__);
}

/**
 *  For Exposure Bracketing, the applied exposure value will be available in
 *  current frame + CPF Exposure.Lag (2 by default). Therefore, in order to do
 *  a correct exposure bracketing we need to skip this amount of frames
 *  initially. When burst-skip-frames parameter is set (>0) we need to skip also
 *  for the target fps. Bracketing itself follows the same logic but more than one
 *  frames are given with the same exposure and skipped.
*
 *  We start applying bracketing for the initially skipped frames, so the
 *  desired result will be available in the real needed frame.
 *  Below is the explanation:
 *  (S stands for skipped frame)
 *  (F stands for forced skipped frame, needed initially for Exposure.Lag)
 *
 *  Detailed descriptions for 3 different slow, medium and fast use cases
 *  below (Where exposure.lag = 2)
 *
 *  For burst-skip-frames=0
 *  Applied exposure value   EV0 EV1 EV2 EV3 EV4 EV5
 *  Frame number             FS0 FS1   2   3   4   5   6   7
 *  Output exposure value            EV0 EV1 EV2 EV3 EV4 EV5
 *  Explanation: in the beginning, we need to force two frame skipping, so
 *  that the applied exposure will be available in frame 2. Continuing the
 *  burst, we don't need to skip, because we will apply the bracketing exposure
 *  in burst sequence (see the timeline above).
 *
 *  For burst-skip-frames=1
 *  Applied exposure value   EV0     EV1     EV2     EV3     EV4     EV5
 *  Frame number             FS0  S1   2  S3   4  S5   6  S7   8  S9  10 S11
 *  Output exposure value            EV0 EV0 EV1 EV1 EV2 EV2 EV3 EV3 EV4 EV4
 *  Explanation: in the beginning, we need to force one frame skipping, so
 *  that the applied exposure will be available in frame 2. Continuing the
 *  burst, we don't need to force skip frames, because we will apply the
 *  bracketing exposure in burst sequence (see the timeline above).
 *
 *  For burst-skip-frames=3
 *  Applied exposure value   EV0             EV1             EV2
 *  Frame number              S0  S1  S2   3  S4  S5  S6   7  S8  S9 S10  11
 *  Output exposure value            EV0 EV0 EV0 EV0 EV1 EV1 EV1 EV1 EV2 EV2
 *  Explanation: for burst-skip-frames >= 2, it's enough to apply the exposure
 *  bracketing in the first skipped frame in order to get the applied exposure
 *  in the next real frame (see the timeline above).
 *
 *  Exposure Bracketing and HDR:
 *  Currently there is an assumption in the HDR firmware in the ISP
 *  that the order how the frames are presented to the algorithm have the following
 *  exposures: MIN,0,MAX
 *  If the order of the exposure bracketing changes HDR firmware needs to be
 *  modified.
 *  This was noticed when this changed from libcamera to libcamera2.
 */

status_t BracketManager::skipFrames(int numFrames, int doBracket)
{
    LOG1("@%s: numFrames=%d, doBracket=%d", __FUNCTION__, numFrames, doBracket);
    status_t status = NO_ERROR;
    AtomBuffer snapshotBuffer, postviewBuffer;
    int retryCount = 0;
    bool recoveryNeeded = false;
    int numLost = 0;

    do {
        recoveryNeeded = false;

        for (int i = 0; i < numFrames; i++) {
            if (i < doBracket) {
                status = applyBracketingParams();
                if (status != NO_ERROR) {
                    ALOGE("@%s: Error applying bracketing params for frame %d!", __FUNCTION__, i);
                    return status;
                }
            }
            if ((status = mISP->getSnapshot(&snapshotBuffer, &postviewBuffer)) != NO_ERROR) {
                ALOGE("@%s: Error in grabbing warm-up frame %d!", __FUNCTION__, i);
                return status;
            }

            // Check if frame loss recovery is needed.
            // Note: Does not work with CSS2 due buffered sensor mode. Driver
            //       does not receive interrupts to increment sequence counter
            //       for skipped frames.
            numLost = getNumLostFrames(snapshotBuffer.frameSequenceNbr);

            status = mISP->putSnapshot(&snapshotBuffer, &postviewBuffer);
            if (status == DEAD_OBJECT) {
                LOG1("@%s: Stale snapshot buffer returned to ISP", __FUNCTION__);
            } else if (status != NO_ERROR) {
                ALOGE("@%s: Error in putting skip frame %d!", __FUNCTION__, i);
                return status;
            }

            // Frame loss recovery. Currently only supported for exposure bracketing.
            if (numLost > 0 && mBracketing.mode == BRACKET_EXPOSURE) {
                if (retryCount == MAX_RETRY_COUNT) {
                    ALOGE("@%s: Frames lost and can't recover.",__FUNCTION__);
                    break;
                }

                // If only skip-frame was lost, then just skip less frames
                if (i + numLost < numFrames) {
                    ALOGI("@%s: Recovering, skip %d frames less", __FUNCTION__, numLost);
                    i += numLost;
                } else {
                    ALOGI("@%s: Lost a snapshot frame, trying to recover", __FUNCTION__);
                    // Restart bracketing from the last successfully captured frame.
                    getRecoveryParams(numFrames, doBracket);
                    retryCount++;
                    recoveryNeeded = true;
                    break;
                }
            }
        }
    } while (recoveryNeeded);

    return status;
}

/**
 * This function returns the number of lost frames. Number of lost
 * frames is calculated based on frame sequence numbering.
 */
int BracketManager::getNumLostFrames(int frameSequenceNbr)
{
    LOG1("@%s", __FUNCTION__);
    int numLost = 0;

    if ((mLastFrameSequenceNbr != -1) && (frameSequenceNbr != (mLastFrameSequenceNbr + 1))) {
        // Frame loss detected, check how many frames were lost.
        numLost = frameSequenceNbr - mLastFrameSequenceNbr - 1;
        ALOGE("@%s: %d frame(s) lost. Current sequence number: %d, previous received: %d",__FUNCTION__, numLost,
            frameSequenceNbr, mLastFrameSequenceNbr);
    }
    mLastFrameSequenceNbr = frameSequenceNbr;

    return numLost;
}

/**
 * When recovery is needed, the bracketing sequence is re-started from the last
 * successfully captured frame. This function updates the next bracketing value,
 * and returns how many frames need to be skipped and how many bracketing values
 * need to be pushed.
 */
void BracketManager::getRecoveryParams(int &skipNum, int &bracketNum)
{
    LOG1("@%s", __FUNCTION__);

    skipNum = 2; // in case of exposure bracketing, need to skip 2 frames to re-fill the pipeline
    bracketNum = (mFpsAdaptSkip > 0) ? 1 : 2; // push at least 1 value, 2 if capturing @15fps

    mBracketNum = mSnapshotReqNum; // rewind to the last succesful capture
    mBracketing.currentValue = mBracketing.values[mBracketNum];
}

status_t BracketManager::initBracketing(int length, int skip, float *bracketValues)
{
    LOG1("@%s: mode = %d", __FUNCTION__, mBracketing.mode);
    status_t status = NO_ERROR;

    mBurstLength = length;
    mFpsAdaptSkip = skip;
    mBurstCaptureNum = 0;
    mSnapshotReqNum = 0;
    mBracketNum = 0;
    mBracketingParams.clear();
    mLastFrameSequenceNbr = -1;

    switch (mBracketing.mode) {
    case BRACKET_EXPOSURE:
        if (mBurstLength > 1) {
            m3AControls->initAeBracketing();
            m3AControls->setAeMode(CAM_AE_MODE_MANUAL);

            mBracketing.values.reset(new float[length]);
            if (bracketValues != NULL) {
                // Using custom bracketing sequence
                for (int i = 0; i < length; i++) {
                    if (bracketValues[i] <= EV_MAX && bracketValues[i] >= EV_MIN) {
                        mBracketing.values[i] = bracketValues[i];
                    } else if (bracketValues[i] > EV_MAX) {
                        ALOGW("Too high exposure value: %.2f", bracketValues[i]);
                        mBracketing.values[i] = EV_MAX;
                    } else if (bracketValues[i] < EV_MIN) {
                        ALOGW("Too low exposure value: %.2f", bracketValues[i]);
                        mBracketing.values[i] = EV_MIN;
                    }
                    LOG1("Setting exposure bracketing parameter %d EV value: %.2f", i, mBracketing.values[i]);
                }
            } else {
                // Using standard bracketing sequence: EV_MIN ---> EV_MAX
                float bracketingStep = ((float)(EV_MAX - EV_MIN)) / (mBurstLength - 1);
                float exposureValue = EV_MIN;
                for (int i = 0; i < length; i++) {
                    mBracketing.values[i] = exposureValue;
                    exposureValue += bracketingStep;
                    LOG1("Setting exposure bracketing parameter %d EV value: %.2f", i, mBracketing.values[i]);
                }
            }
            mBracketing.currentValue = mBracketing.values[mBracketNum];
        } else {
            LOG1("Can't do bracketing with only one capture, disable bracketing!");
            mBracketing.mode = BRACKET_NONE;
        }
        break;
    case BRACKET_FOCUS:
        if (mBurstLength > 1) {
            mBracketing.step = mBurstLength;
            mBracketing.currentValue = 0;
            m3AControls->initAfBracketing(mBracketing.step, CAM_AF_BRACKETING_MODE_SYMMETRIC);
        } else {
            LOG1("Can't do bracketing with only one capture, disable bracketing!");
            mBracketing.mode = BRACKET_NONE;
        }
        break;
    case BRACKET_NONE:
        // Do nothing here
        break;
    }

    // Allocate internal buffers for captured frames
    mSnapshotBufs.reset(new AtomBuffer[mBurstLength]);
    mPostviewBufs.reset(new AtomBuffer[mBurstLength]);

    return status;
}

status_t BracketManager::applyBracketing()
{
    LOG1("@%s: mode = %d", __FUNCTION__, mBracketing.mode);
    status_t status = NO_ERROR;
    int retryCount = 0;
    int numLost = 0;
    bool recoveryNeeded = false;

    if  (mFpsAdaptSkip > 0) {
        LOG1("Skipping %d burst frames", mFpsAdaptSkip);
        int doBracketNum = 0;
        int skipNum = 0;
        skipNum += mFpsAdaptSkip;
        if (mBracketing.mode == BRACKET_EXPOSURE && mFpsAdaptSkip >= 2) {
            // In Exposure Bracket, if mFpsAdaptSkip >= 2 apply bracketing every first skipped frame
            // This is because, exposure needs 2 frames for the exposure value to take effect
            doBracketNum += 1;
        } else if (mBracketing.mode == BRACKET_FOCUS && mFpsAdaptSkip >= 1) {
            // In Focus Bracket, if mFpsAdaptSkip >= 1 apply bracketing every first skipped frame
            // This is because focus needs only 1 frame for the focus position to take effect
            doBracketNum += 1;
        }
        if (skipFrames(skipNum, doBracketNum) != NO_ERROR) {
            ALOGE("Error skipping burst frames!");
        }
    }

    // If mFpsAdaptSkip < 2, apply exposure bracketing every real frame
    // If mFpsAdaptSkip < 1, apply focus bracketing every real frame
    if ((mFpsAdaptSkip < 2 && mBracketing.mode == BRACKET_EXPOSURE) ||
        (mFpsAdaptSkip < 1 && mBracketing.mode == BRACKET_FOCUS)) {

        if (applyBracketingParams() != NO_ERROR) {
            ALOGE("Error applying bracketing params!");
        }
    }

    do {
        recoveryNeeded = false;

        status = mISP->getSnapshot(&mSnapshotBufs[mBurstCaptureNum], &mPostviewBufs[mBurstCaptureNum]);

        // Check number of lost frames
        // Note: Does not work with CSS2 due buffered sensor mode. Driver
        //       does not receive interrupts to increment sequence counter
        //       for skipped frames.
        numLost = getNumLostFrames(mSnapshotBufs[mBurstCaptureNum].frameSequenceNbr);

        // Frame loss recovery. Currently only supported for exposure bracketing.
        if (numLost > 0 && mBracketing.mode == BRACKET_EXPOSURE) {
            if (retryCount == MAX_RETRY_COUNT) {
                ALOGE("@%s: Frames lost and can't recover.",__FUNCTION__);
                status = UNKNOWN_ERROR;
                break;
            }
            // Return the buffer to ISP
            status = mISP->putSnapshot(&mSnapshotBufs[mBurstCaptureNum], &mPostviewBufs[mBurstCaptureNum]);

            // Restart bracketing from the last successfully captured frame.
            int skip, doBracket;
            getRecoveryParams(skip, doBracket);
            skipFrames(skip, doBracket);
            retryCount++;
            recoveryNeeded = true;
        }
    } while (recoveryNeeded);

    LOG1("@%s: Captured frame %d, sequence number: %d", __FUNCTION__, mBurstCaptureNum + 1, mSnapshotBufs[mBurstCaptureNum].frameSequenceNbr);
    mLastFrameSequenceNbr = mSnapshotBufs[mBurstCaptureNum].frameSequenceNbr;
    mBurstCaptureNum++;

    if (mBurstCaptureNum == mBurstLength) {
        LOG1("@%s: All frames captured", __FUNCTION__);
        mState = STATE_CAPTURE;
    }

    return status;
}

status_t BracketManager::applyBracketingParams()
{
    LOG1("@%s: mode = %d", __FUNCTION__, mBracketing.mode);
    status_t status = NO_ERROR;
    SensorAeConfig aeConfig;
    memset(&aeConfig, 0, sizeof(aeConfig));

    switch (mBracketing.mode) {
    case BRACKET_EXPOSURE:
        if (mBracketNum < mBurstLength) {
            LOG1("Applying Exposure Bracketing: %.2f", mBracketing.currentValue);
            status = m3AControls->applyEv(mBracketing.currentValue);
            if (status != NO_ERROR) {
                ALOGE("Error applying exposure bracketing value EV = %.2f", mBracketing.currentValue);
                return status;
            }
            m3AControls->getExposureInfo(aeConfig);
            aeConfig.evBias = mBracketing.currentValue;

            LOG1("Adding aeConfig to list (size=%d+1)", mBracketingParams.size());
            mBracketingParams.push_front(aeConfig);

            mBracketNum++;
            if (mBracketNum < mBurstLength) {
                mBracketing.currentValue = mBracketing.values[mBracketNum];
                LOG1("@%s: setting next exposure value = %.2f", __FUNCTION__, mBracketing.values[mBracketNum]);
            }
        }
        break;
    case BRACKET_FOCUS:
        if(mBracketing.currentValue < mBracketing.step) {
            m3AControls->setManualFocusIncrement(mBracketing.currentValue);
            mBracketing.currentValue++;
        }
        break;
    case BRACKET_NONE:
        // Do nothing here
        break;
    }

    return status;
}

void BracketManager::setBracketMode(BracketingMode mode)
{
    if (mState == STATE_STOPPED)
        mBracketing.mode = mode;
    else
        ALOGW("%s: attempt to change bracketing mode during capture", __FUNCTION__);
}

BracketingMode BracketManager::getBracketMode()
{
    LOG1("@%s", __FUNCTION__);
    return mBracketing.mode;
}

void BracketManager::getNextAeConfig(SensorAeConfig *aeConfig) {
    LOG1("@%s", __FUNCTION__);

    if (!mBracketingParams.empty()) {
        LOG1("Popping sensorAeConfig from list (size=%d-1)", mBracketingParams.size());
        if (aeConfig)
            *aeConfig = *(--mBracketingParams.end());
        mBracketingParams.erase(--mBracketingParams.end());
    }
}

status_t BracketManager::startBracketing()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    Message msg;
    msg.id = MESSAGE_ID_START_BRACKETING;

    // skip initial frames
    int doBracketNum = 0;
    int skipNum = 0;
    if (mBracketing.mode == BRACKET_EXPOSURE) {
        /*
         *  Because integration time and gain can not become effective immediately
         *  and delays depend on sensors, we need to skip first several frames
         *  See documentation for BracketManager::skipFrames()
         */
        int exposureLag = 0;
        PlatformData::HalConfig.getValue(exposureLag, CPF::Exposure, CPF::Lag);
        if (exposureLag == 0) {
            LOG1("Exposure latency (CPF Exposure.Lag) zero, using static default in bracketing");
            exposureLag = 2;
        }

        /*
         * CSS2.0 frame sync is triggered at EOF, so we miss the first
         * valid apply time here.
         * TODO: apply initial already before start, or implement async
         *       parameter queue
         */
        if (mISP->getCssMajorVersion() == 2) {
            exposureLag++;
        }

        /*
         *  If we are in Exposure Bracketing, and we need to skip frames for
         *  mFpsAdaptSkip (target fps) we can count these out from initial
         *  skips done at start.
         */
        if (mFpsAdaptSkip < exposureLag) {
            skipNum += exposureLag - mFpsAdaptSkip;
            doBracketNum += exposureLag - mFpsAdaptSkip;
        }
    } else if (mBracketing.mode == BRACKET_FOCUS && mFpsAdaptSkip < 1) {
        /*
         *  If we are in Focus Bracketing, and mFpsAdaptSkip < 1, we need to
         *  skip one initial frame w/o apply bracketing so that the focus will be
         *  positioned in the initial position.
         */
        skipNum += 1;
    }
    if (skipNum > 0) {
        if (skipFrames(skipNum, doBracketNum) != NO_ERROR) {
            ALOGE("@%s: Error skipping initial frames!", __FUNCTION__);
        }
        PERFORMANCE_TRACES_BREAKDOWN_STEP_PARAM("Skip", skipNum);
    }
    status = mMessageQueue.send(&msg, MESSAGE_ID_START_BRACKETING);
    return status;
}

status_t BracketManager::handleMessageStartBracketing()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    mState = STATE_BRACKETING;
    mMessageQueue.reply(MESSAGE_ID_START_BRACKETING, status);
    return status;
}

status_t BracketManager::stopBracketing()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    Message msg;
    msg.id = MESSAGE_ID_STOP_BRACKETING;
    status = mMessageQueue.send(&msg, MESSAGE_ID_STOP_BRACKETING);
    PERFORMANCE_TRACES_BREAKDOWN_STEP_NOPARAM();
    return status;
}

status_t BracketManager::handleMessageStopBracketing()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    mState = STATE_STOPPED;
    mSnapshotBufs.reset();
    mPostviewBufs.reset();
    mBracketing.values.reset();
    mMessageQueue.reply(MESSAGE_ID_STOP_BRACKETING, status);
    return status;
}

status_t BracketManager::getSnapshot(AtomBuffer &snapshotBuf, AtomBuffer &postviewBuf)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    Message msg;

    // If we already have enough buffers to use, no need to block and wait
    // This will improve Bracketing performance.
    if (mSnapshotReqNum >= mBurstCaptureNum) {
        LOG1("@%s, no buffer is ready, try to wait...", __FUNCTION__);
        msg.id = MESSAGE_ID_GET_SNAPSHOT;
        if ((status = mMessageQueue.send(&msg, MESSAGE_ID_GET_SNAPSHOT)) != NO_ERROR) {
            return status;
        }
    }

    snapshotBuf = mSnapshotBufs[mSnapshotReqNum];
    postviewBuf = mPostviewBufs[mSnapshotReqNum];
    mSnapshotReqNum++;
    LOG1("@%s: grabbing snapshot %d / %d (%d frames captured)", __FUNCTION__, mSnapshotReqNum, mBurstLength, mBurstCaptureNum);

    return status;
}

/*
 * Bracketing get snapshot buffer via BracketManager:getSnapshot
 * It's better to return the buffer via BracketManager too.
 */
status_t BracketManager::putSnapshot(AtomBuffer &snapshotBuf, AtomBuffer &postviewBuf)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    Message msg;

    msg.id = MESSAGE_ID_PUT_SNAPSHOT;
    msg.data.capture.snapshotBuf = snapshotBuf;
    msg.data.capture.postviewBuf = postviewBuf;

    if ((status = mMessageQueue.send(&msg, MESSAGE_ID_PUT_SNAPSHOT)) != NO_ERROR) {
        ALOGE("@%s: put snapshot error:%d", __FUNCTION__, status);
    }

    return status;
}

status_t BracketManager::handleMessageGetSnapshot()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    if (mState != STATE_CAPTURE && mState != STATE_BRACKETING) {
        ALOGE("@%s: wrong state (%d)", __FUNCTION__, mState);
        status = INVALID_OPERATION;
    }

    mMessageQueue.reply(MESSAGE_ID_GET_SNAPSHOT, status);
    return status;
}

status_t BracketManager::handleMessagePutSnapshot(MessageCapture capture)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    if (mState != STATE_CAPTURE && mState != STATE_BRACKETING) {
        ALOGE("@%s: wrong state (%d)", __FUNCTION__, mState);
        status = INVALID_OPERATION;
    }

    status = mISP->putSnapshot(&capture.snapshotBuf, &capture.postviewBuf);

    mMessageQueue.reply(MESSAGE_ID_PUT_SNAPSHOT, status);
    return status;
}

bool BracketManager::threadLoop()
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    mThreadRunning = true;

    while (mThreadRunning) {
        switch (mState) {

        case STATE_STOPPED:
        case STATE_CAPTURE:
            LOG2("In %s...", (mState == STATE_STOPPED) ? "STATE_STOPPED" : "STATE_CAPTURE");
            // in the stop/capture state all we do is wait for messages
            status = waitForAndExecuteMessage();
            break;

        case STATE_BRACKETING:
            LOG2("In STATE_BRACKETING...");
            // Check if snapshot is requested and if we already have some available
            if (!mMessageQueue.isEmpty() && mBurstCaptureNum > mSnapshotReqNum) {
                status = waitForAndExecuteMessage();
            } else {
                status = applyBracketing();
            }
            break;

        default:
            break;
        }

        if (status != NO_ERROR) {
            ALOGE("operation failed, state = %d, status = %d", mState, status);
        }
    }

    return false;
}

status_t BracketManager::waitForAndExecuteMessage()
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    Message msg;
    mMessageQueue.receive(&msg);

    switch (msg.id)
    {
        case MESSAGE_ID_EXIT:
            status = handleExit();
            break;
        case MESSAGE_ID_START_BRACKETING:
            status = handleMessageStartBracketing();
            break;
        case MESSAGE_ID_STOP_BRACKETING:
            status = handleMessageStopBracketing();
            break;
        case MESSAGE_ID_GET_SNAPSHOT:
            status = handleMessageGetSnapshot();
            break;
        case MESSAGE_ID_PUT_SNAPSHOT:
            status = handleMessagePutSnapshot(msg.data.capture);
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

status_t BracketManager::handleExit()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    mThreadRunning = false;
    return status;
}

status_t BracketManager::requestExitAndWait()
{
    LOG2("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_EXIT;
    // tell thread to exit
    // send message asynchronously
    mMessageQueue.send(&msg);

    // propagate call to base class
    return Thread::requestExitAndWait();
}


} // namespace android
