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
#include "OnlineBracket.h"
#include "OfflineBracket.h"
#include "PerformanceTraces.h"
#include "AtomISP.h"

namespace android {

BracketManager::BracketManager(AtomISP *atomISP, I3AControls *aaaControls, int cameraId) :
     m3AControls(aaaControls)
    ,mISP(atomISP)
    ,mState(STATE_STOPPED)
    ,mImplMethod(IMPL_OFFLINE)
    ,mBracketImpl(NULL)
    ,mBurstLength(-1)
    ,mCameraId(cameraId)
{
    LOG1("@%s", __FUNCTION__);
    mBracketing.mode = BRACKET_NONE;
}

BracketManager::~BracketManager()
{
    LOG1("@%s", __FUNCTION__);
    destroyImpl();
}

status_t BracketManager::createImpl(BracketImplMethod method)
{
    status_t status = NO_ERROR;
    LOG1("@%s method:%d", __FUNCTION__, method);

    if (mBracketImpl != NULL) {
        ALOGW("Braket impl already exist");
        return status;
    }

    if (method == IMPL_ONLINE) {
        OnlineBracket *onlineBracket = new OnlineBracket(mISP, m3AControls, this, mCameraId);
        status = onlineBracket->run("CamHAL_ONLINE_BRACKET");
        if (status != NO_ERROR) {
            ALOGW("Error Starting online bracket");
        }
        mBracketImpl = onlineBracket;
    } else {
        mBracketImpl = new OfflineBracket(mISP, m3AControls, this);
    }

    if (mBracketImpl == NULL)  {
        ALOGE("Error in creating bracket implementation");
        status = UNKNOWN_ERROR;
    }

    return status;
}

void BracketManager::destroyImpl()
{
    LOG1("@%s ", __FUNCTION__);
    if (mBracketImpl != NULL) {
        if (mImplMethod == IMPL_ONLINE) {
            sp<OnlineBracket> onlineBracket = static_cast<OnlineBracket *>(mBracketImpl);
            if (onlineBracket.get() != NULL) {
                // thread exit and this would be destroyed by Thread
                onlineBracket->requestExitAndWait();
                onlineBracket.clear();
            } else {
                ALOGE("Error in destroy online bracket implementation");
            }
        } else {
            delete mBracketImpl;
        }

        mBracketImpl = NULL;
    }
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

void BracketManager::getNextAeConfig(SensorAeConfig *aeConfig)
{
    LOG1("@%s", __FUNCTION__);

    if (!mBracketingParams.empty()) {
        LOG1("Popping sensorAeConfig from list (size=%d-1)", mBracketingParams.size());
        if (aeConfig)
            *aeConfig = *(--mBracketingParams.end());
        mBracketingParams.erase(--mBracketingParams.end());
    }
}

status_t BracketManager::initBracketing(int length, int skip, BracketImplMethod impl, float *bracketValues)
{
    status_t status = NO_ERROR;
    LOG1("@%s: mode = %d impl:%d", __FUNCTION__, mBracketing.mode, impl);

    mBurstLength = length;
    mBracketingParams.clear();

    switch (mBracketing.mode) {
    case BRACKET_EXPOSURE:
        if (mBurstLength > 1) {
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
            mBracketing.currentValue = mBracketing.values[0];
        } else {
            LOG1("Can't do bracketing with only one capture, disable bracketing!");
            mBracketing.mode = BRACKET_NONE;
        }
        break;
    case BRACKET_FOCUS:
        if (mBurstLength > 1) {
            mBracketing.step = mBurstLength;
            mBracketing.currentValue = 0;
        } else {
            LOG1("Can't do bracketing with only one capture, disable bracketing!");
            mBracketing.mode = BRACKET_NONE;
        }
        break;
    case BRACKET_NONE:
        // Do nothing here
        break;
    }

    if (mImplMethod != impl && mBracketing.mode != BRACKET_NONE) {
        LOG1("Changing braketing implementation from %d to %d", mImplMethod, impl);
        destroyImpl();
        mImplMethod  = impl;
    }

    status = createImpl(mImplMethod);
    if (mBracketImpl && status == NO_ERROR)
        mBracketImpl->init(length, skip);

    return status;
}

status_t BracketManager::startBracketing(int *expIdFrom)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    if (mBracketing.mode == BRACKET_EXPOSURE) {
        m3AControls->initAeBracketing();
        m3AControls->setAeMode(CAM_AE_MODE_MANUAL);
    } else if (mBracketing.mode == BRACKET_FOCUS) {
        m3AControls->initAfBracketing(mBracketing.step, CAM_AF_BRACKETING_MODE_SYMMETRIC);
    } else {
        ALOGE("No braket mode specified for starting");
    }

    status = mBracketImpl->startBracketing(expIdFrom);
    if (status == NO_ERROR)
        mState = STATE_BRACKETING;

    return  status;
}

status_t BracketManager::stopBracketing()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    if (mState == STATE_STOPPED)
        return status;

    if (mBracketing.mode == BRACKET_EXPOSURE) {
        m3AControls->deinitAeBracketing();
    } else if (mBracketing.mode == BRACKET_FOCUS) {
    } else {
        ALOGE("No braket mode specified for stopping");
    }

    status = mBracketImpl->stopBracketing();
    m3AControls->deinitAeBracketing();
    if (status == NO_ERROR)
        mState = STATE_STOPPED;

    mBracketing.values.reset();
    return status;
}

status_t BracketManager::getSnapshot(AtomBuffer &snapshotBuf, AtomBuffer &postviewBuf)
{
    LOG1("@%s", __FUNCTION__);
    if (mState != STATE_BRACKETING)
        return INVALID_OPERATION;

    return mBracketImpl->getSnapshot(snapshotBuf, postviewBuf);
}

status_t BracketManager::putSnapshot(AtomBuffer &snapshotBuf, AtomBuffer &postviewBuf)
{
    LOG1("@%s", __FUNCTION__);
    if (mState != STATE_BRACKETING)
        return INVALID_OPERATION;

    return mBracketImpl->putSnapshot(snapshotBuf, postviewBuf);
}

} // namespace android
