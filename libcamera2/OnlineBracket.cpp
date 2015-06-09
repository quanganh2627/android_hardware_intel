/*
 * Copyright (C) 2014 The Android Open Source Project
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
#define LOG_TAG "Camera_OnlineBracket"
//#define LOG_NDEBUG 0

#include "LogHelper.h"
#include "PlatformData.h"
#include "OnlineBracket.h"
#include "PerformanceTraces.h"
#include "AtomISP.h"

namespace android {

OnlineBracket::OnlineBracket(AtomISP *atomISP, I3AControls *aaaControls, BracketManager *manager, int cameraId) :
    Thread(false)
    ,mManager(manager)
    ,m3AControls(aaaControls)
    ,mIspCI(atomISP)
    ,mISP(atomISP)
    ,mState(STATE_STOPPED)
    ,mFpsAdaptSkip(-1)
    ,mBurstLength(-1)
    ,mBurstCaptureNum(-1)
    ,mSnapshotReqNum(-1)
    ,mBracketNum(-1)
    ,mLastFrameSequenceNbr(-1)
    ,mMessageQueue("OnlineBracket", (int) MESSAGE_ID_MAX)
    ,mThreadRunning(false)
    ,mCameraId(cameraId)
{
    LOG1("@%s", __FUNCTION__);
    mBracketing = &mManager->mBracketing;
    mBracketingParams = &mManager->mBracketingParams;
}

OnlineBracket::~OnlineBracket()
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

status_t OnlineBracket::skipFrames(int numFrames, int doBracket)
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
            if (numLost > 0 && mBracketing->mode == BRACKET_EXPOSURE) {
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
int OnlineBracket::getNumLostFrames(int frameSequenceNbr)
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
void OnlineBracket::getRecoveryParams(int &skipNum, int &bracketNum)
{
    LOG1("@%s", __FUNCTION__);

    skipNum = 2; // in case of exposure bracketing, need to skip 2 frames to re-fill the pipeline
    bracketNum = (mFpsAdaptSkip > 0) ? 1 : 2; // push at least 1 value, 2 if capturing @15fps

    mBracketNum = mSnapshotReqNum; // rewind to the last succesful capture
    mBracketing->currentValue = mBracketing->values[mBracketNum];
}

status_t OnlineBracket::init(int length, int skip)
{
    LOG1("@%s: mode = %d", __FUNCTION__, mBracketing->mode);

    mBurstLength = length;
    mFpsAdaptSkip = skip;
    mBurstCaptureNum = 0;
    mSnapshotReqNum = 0;
    mBracketNum = 0;
    mLastFrameSequenceNbr = -1;

    // Allocate internal buffers for captured frames
    mSnapshotBufs.reset(new AtomBuffer[mBurstLength]);
    mPostviewBufs.reset(new AtomBuffer[mBurstLength]);

    return NO_ERROR;
}

status_t OnlineBracket::applyBracketing()
{
    LOG1("@%s: mode = %d", __FUNCTION__, mBracketing->mode);
    status_t status = NO_ERROR;
    int retryCount = 0;
    int numLost = 0;
    bool recoveryNeeded = false;

    if  (mFpsAdaptSkip > 0) {
        LOG1("Skipping %d burst frames", mFpsAdaptSkip);
        int doBracketNum = 0;
        int skipNum = 0;
        skipNum += mFpsAdaptSkip;
        if (mBracketing->mode == BRACKET_EXPOSURE && mFpsAdaptSkip >= 2) {
            // In Exposure Bracket, if mFpsAdaptSkip >= 2 apply bracketing every first skipped frame
            // This is because, exposure needs 2 frames for the exposure value to take effect
            doBracketNum += 1;
        } else if (mBracketing->mode == BRACKET_FOCUS && mFpsAdaptSkip >= 1) {
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
    if ((mFpsAdaptSkip < 2 && mBracketing->mode == BRACKET_EXPOSURE) ||
        (mFpsAdaptSkip < 1 && mBracketing->mode == BRACKET_FOCUS)) {

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
        if (numLost > 0 && mBracketing->mode == BRACKET_EXPOSURE) {
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

status_t OnlineBracket::applyBracketingParams()
{
    LOG1("@%s: mode = %d", __FUNCTION__, mBracketing->mode);
    status_t status = NO_ERROR;
    SensorAeConfig aeConfig;
    memset(&aeConfig, 0, sizeof(aeConfig));

    switch (mBracketing->mode) {
    case BRACKET_EXPOSURE:
        if (mBracketNum < mBurstLength) {
            LOG1("Applying Exposure Bracketing: %.2f", mBracketing->currentValue);
            status = m3AControls->applyEv(mBracketing->currentValue);
            if (status != NO_ERROR) {
                ALOGE("Error applying exposure bracketing value EV = %.2f", mBracketing->currentValue);
                return status;
            }
            m3AControls->getExposureInfo(aeConfig);
            aeConfig.evBias = mBracketing->currentValue;

            LOG1("Adding aeConfig to list (size=%d+1)", mBracketingParams->size());
            mBracketingParams->push_front(aeConfig);

            mBracketNum++;
            if (mBracketNum < mBurstLength) {
                mBracketing->currentValue = mBracketing->values[mBracketNum];
                LOG1("@%s: setting next exposure value = %.2f", __FUNCTION__, mBracketing->values[mBracketNum]);
            }
        }
        break;
    case BRACKET_FOCUS:
        if(mBracketing->currentValue < mBracketing->step) {
            m3AControls->setManualFocusIncrement(mBracketing->currentValue);
            mBracketing->currentValue++;
        }
        break;
    case BRACKET_NONE:
        // Do nothing here
        break;
    }

    return status;
}

status_t OnlineBracket::startBracketing(int *expIdFrom)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    Message msg;
    msg.id = MESSAGE_ID_START_BRACKETING;

    // skip initial frames
    int doBracketNum = 0;
    int skipNum = 0;
    if (mBracketing->mode == BRACKET_EXPOSURE) {
        /*
         *  Because integration time and gain can not become effective immediately
         *  and delays depend on sensors, we need to skip first several frames
         *  See documentation for OnlineBracket::skipFrames()
         */
        int exposureLag = 0;
        PlatformData::HalConfig[mCameraId].getValue(exposureLag, CPF::Exposure, CPF::Lag);
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
        if (mIspCI->getCssMajorVersion() == 2) {
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
    } else if (mBracketing->mode == BRACKET_FOCUS && mFpsAdaptSkip < 1) {
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

status_t OnlineBracket::handleMessageStartBracketing()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    mState = STATE_BRACKETING;
    mMessageQueue.reply(MESSAGE_ID_START_BRACKETING, status);
    return status;
}

status_t OnlineBracket::stopBracketing()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    Message msg;
    msg.id = MESSAGE_ID_STOP_BRACKETING;
    status = mMessageQueue.send(&msg, MESSAGE_ID_STOP_BRACKETING);
    PERFORMANCE_TRACES_BREAKDOWN_STEP_NOPARAM();
    return status;
}

status_t OnlineBracket::handleMessageStopBracketing()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    mState = STATE_STOPPED;
    mSnapshotBufs.reset();
    mPostviewBufs.reset();
    mMessageQueue.reply(MESSAGE_ID_STOP_BRACKETING, status);
    return status;
}

status_t OnlineBracket::getSnapshot(AtomBuffer &snapshotBuf, AtomBuffer &postviewBuf)
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
 * Bracketing get snapshot buffer via OnlineBracket:getSnapshot
 * It's better to return the buffer via OnlineBracket too.
 */
status_t OnlineBracket::putSnapshot(AtomBuffer &snapshotBuf, AtomBuffer &postviewBuf)
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

status_t OnlineBracket::handleMessageGetSnapshot()
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

status_t OnlineBracket::handleMessagePutSnapshot(MessageCapture capture)
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

bool OnlineBracket::threadLoop()
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
            if ((!mMessageQueue.isEmpty() && mBurstCaptureNum > mSnapshotReqNum)) {
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

status_t OnlineBracket::waitForAndExecuteMessage()
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

status_t OnlineBracket::handleExit()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    mThreadRunning = false;
    return status;
}

status_t OnlineBracket::requestExitAndWait()
{
    LOG2("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_EXIT;
    // tell thread to exit
    // send message asynchronously
    mMessageQueue.send(&msg);

    return NO_ERROR;
}

} // namespace android
