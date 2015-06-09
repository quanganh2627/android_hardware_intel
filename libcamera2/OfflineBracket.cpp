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
#define LOG_TAG "Camera_OfflineBracket"
//#define LOG_NDEBUG 0

#include "LogHelper.h"
#include "PlatformData.h"
#include "OfflineBracket.h"
#include "PerformanceTraces.h"
#include "AtomISP.h"

namespace android {

OfflineBracket::OfflineBracket(AtomISP *atomISP, I3AControls *aaaControls, BracketManager *manager) :
     mManager(manager)
    ,m3AControls(aaaControls)
    ,mISP(atomISP)
    ,mState(STATE_STOPPED)
    ,mBurstLength(-1)
    ,mFpsAdaptSkip(0)
    ,mExpectedExpId(EXP_ID_INVALID)
{
    LOG1("@%s", __FUNCTION__);
    mBracketing = &mManager->mBracketing;
    mBracketingParams = &mManager->mBracketingParams;
}

OfflineBracket::~OfflineBracket()
{
    LOG1("@%s", __FUNCTION__);
}

status_t OfflineBracket::init(int length, int skip)
{
    LOG1("@%s: mode = %d", __FUNCTION__, mBracketing->mode);
    status_t status = NO_ERROR;

    mBurstLength = length;
    mFpsAdaptSkip = skip;

    return status;
}

status_t OfflineBracket::startBracketing(int *expIdFrom)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    if (mBracketing->mode == BRACKET_EXPOSURE) {
        SensorAeConfig aeConfig[mBurstLength];
        // apply EV group
        mExpectedExpId = m3AControls->applyEvGroup(mBracketing->values.get(), mBurstLength, aeConfig);
        for (int i = 0 ; i < mBurstLength ; i++)
            mBracketingParams->push_front(aeConfig[i]);

        if (expIdFrom != NULL)
            *expIdFrom = mExpectedExpId;
    } else if (mBracketing->mode == BRACKET_FOCUS) {

    }

    // start offline capture
    ContinuousCaptureConfig cfg;
    cfg.numCaptures = mBurstLength;
    cfg.offset = 1;
    cfg.skip = mFpsAdaptSkip;
    mISP->startOfflineCapture(cfg);
    mState = STATE_BRACKETING;

    return status;
}

status_t OfflineBracket::stopBracketing()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    mState = STATE_STOPPED;
    if (mBracketing->mode == BRACKET_EXPOSURE) {
        AeMode mode = m3AControls->getPublicAeMode();
        m3AControls->setAeMode(mode);
    } else if (mBracketing->mode == BRACKET_FOCUS) {

    }

    return status;
}

status_t OfflineBracket::getSnapshot(AtomBuffer &snapshotBuf, AtomBuffer &postviewBuf)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    if (mState != STATE_BRACKETING) {
        ALOGE("@%s: wrong state (%d)", __FUNCTION__, mState);
        return INVALID_OPERATION;
    }

    if ((status = mISP->getSnapshot(&snapshotBuf, &postviewBuf)) != NO_ERROR) {
        ALOGE("Error in grabbing offline braketing frame!");
        return status;
    }

    if (snapshotBuf.expId != mExpectedExpId) {
        ALOGW("get snapshot exp id:%d expected:%d", snapshotBuf.expId, mExpectedExpId);
    }

    // the next expected ID
    mExpectedExpId = NEXTN_EID(mExpectedExpId, mFpsAdaptSkip + 1);

    return status;
}

status_t OfflineBracket::putSnapshot(AtomBuffer &snapshotBuf, AtomBuffer &postviewBuf)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    if (mState != STATE_BRACKETING) {
        ALOGE("@%s: wrong state (%d)", __FUNCTION__, mState);
        status = INVALID_OPERATION;
    }

    status = mISP->putSnapshot(&snapshotBuf, &postviewBuf);
    if (status != NO_ERROR) {
        ALOGE("Error in offline bracketing putting buffer:%d", snapshotBuf.expId);
    }
    return status;
}

} // namespace android
