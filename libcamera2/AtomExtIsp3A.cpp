/*
 *
 * Copyright (c) 2014 Intel Corporation.
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

#define LOG_TAG "Camera_AtomExtIsp3A"

#include "AtomExtIsp3A.h"

namespace android {

AtomExtIsp3A::AtomExtIsp3A(int cameraId, HWControlGroup &hwcg)
    :AtomSoc3A(cameraId, hwcg)
    ,mCameraId(cameraId)
    ,mISP(hwcg.mIspCI)
    ,mSensorCI(hwcg.mSensorCI)
    ,mFlashCI(hwcg.mFlashCI)
    ,mLensCI(hwcg.mLensCI)
    ,mDrvAfMode(-1)
    ,mDrvFlashMode(-1)
    ,mFaceDetectionActive(false)
{

}

AtomExtIsp3A::~AtomExtIsp3A()
{

}

// TODO: Rename this to something more meaningful
// Looks like setAfEnabled() is a legacy from AcuteLogic times
// and this is no-op for Intel AIQ. The concepts in AF are somewhat overlapping.
// Now startStillAf() is something that actually starts an "AF sequence" and setAfEnabled()
// "starts AF" in general: running of CAF algorithms etc., so maybe more meaningful names would
// be appropriate in I3AControls interface for these functions.
status_t AtomExtIsp3A::setAfEnabled(bool en)
{
    LOG1("@%s: en: %d", __FUNCTION__, en);
    return mSensorCI->setAfEnabled(en);
}


status_t AtomExtIsp3A::setAfMode(AfMode mode)
{
    LOG1("@%s: %d", __FUNCTION__, mode);
    status_t status = NO_ERROR;
    int modeTmp;

    // TODO: add supported modes to PlatformData
    if (mCameraId > 0) {
        LOG1("@%s: not supported by current camera", __FUNCTION__);
        return INVALID_OPERATION;
    }

    switch (mode) {
        case CAM_AF_MODE_MACRO:
            modeTmp = EXT_ISP_FOCUS_MODE_MACRO;
            break;
            // TODO: Support both movie and still/preview CAF (AtomAIQ)
        case CAM_AF_MODE_CONTINUOUS:
            modeTmp = mFaceDetectionActive ? EXT_ISP_FOCUS_MODE_FACE_CAF : EXT_ISP_FOCUS_MODE_PREVIEW_CAF;
            break;
        case CAM_AF_MODE_AUTO:
            // For m10mo AF MODE "auto" == "normal"
            modeTmp = EXT_ISP_FOCUS_MODE_NORMAL;
            break;
        default:
            ALOGW("Unsupported ext-ISP AF mode (%d), using PREVIEW_CAF", mode);
            modeTmp = EXT_ISP_FOCUS_MODE_NORMAL;
            break;
    }

    int ret = mSensorCI->setAfMode(modeTmp);
    if (ret != 0) {
        ALOGE("Error setting AF  mode (%d) in the driver", modeTmp);
        status = UNKNOWN_ERROR;
    }

    if (status == NO_ERROR && mode == CAM_AF_MODE_CONTINUOUS) {
        // For other modes (auto, macro), explicit autoFocus() call from app is
        // needed to start AF. CAF needs to be started now.
        status = mSensorCI->setAfEnabled(true);
    }

    mDrvAfMode = modeTmp;
    return status;
}

/**
 * \returns The AF mode as defined in AfMode enum
 *
 * NOTE: With ext-ISP we currently have AF mode + its "touch-variant"
 * Touch-variant is handles as combination of AF-mode and AF window, e.g:
 *
 * CAM_AF_MODE_MACRO + AF window = EXT_ISP_FOCUS_MODE_TOUCH_MACRO
 */
AfMode AtomExtIsp3A::getAfMode()
{
    LOG2("@%s", __FUNCTION__);

    AfMode mode = CAM_AF_MODE_AUTO;
    int v4lMode = EXT_ISP_FOCUS_MODE_NORMAL;


    v4lMode = mDrvAfMode;

    // TODO: TEST: getAfMode() always returns 0, probably
    // as we need to "commit" to AF mode changes with AF_START.
    // This needs 'get' to be tested on NEO-device, as VV-board does not support
    // AF to be really run..
//     int ret = mSensorCI->getAfMode(&v4lMode);
//     if (ret != 0) {
//          ALOGE("Error getting AF mode from the driver");
//         status = UNKNOWN_ERROR;
//     }


    // TOUCH modes can be differentiated at touch window setting:
    // mode: CAF + area = CAM_AF_MODE_TOUCH_CAF
    // mode: MACRO + area = CAM_AF_MODE_TOUCH_MACRO 
    // etc.


    // CAM_AF_MODE_MOVIE_CAF needs to be implemented and mode added to our internal
    // list, that currently represents the Intel AIQ modes (has only one CAF mode)
    // Focus modes

    switch (v4lMode) {
        case EXT_ISP_FOCUS_MODE_MACRO:
        case EXT_ISP_FOCUS_MODE_TOUCH_MACRO:
            mode = CAM_AF_MODE_MACRO;
            break;
        case EXT_ISP_FOCUS_MODE_PREVIEW_CAF:
        case EXT_ISP_FOCUS_MODE_MOVIE_CAF:
            // TODO: MOVIE_CAF as its own mode
        case EXT_ISP_FOCUS_MODE_FACE_CAF:
        case EXT_ISP_FOCUS_MODE_TOUCH_CAF:
            mode = CAM_AF_MODE_CONTINUOUS;
            break;
        case EXT_ISP_FOCUS_MODE_TOUCH_AF:
        case EXT_ISP_FOCUS_MODE_NORMAL:
            mode = CAM_AF_MODE_AUTO;
            break;
        default:
            LOG2("Unsupported ext-ISP AF mode (%d), using CAM_AF_MODE_AUTO", mode);
            mode = CAM_AF_MODE_AUTO;
            break;
    }

    if(PlatformData::isFixedFocusCamera(mCameraId))
        mode = CAM_AF_MODE_FIXED;

    return mode;
}

status_t AtomExtIsp3A::setAfWindows(CameraWindow *windows, size_t numWindows, const AAAWindowInfo *convWindow)
{
    LOG1("@%s", __FUNCTION__);

    int drvModeToSet = EXT_ISP_FOCUS_MODE_NORMAL;
    // Get the current "non-touch" AF mode set
    AfMode afMode = getAfMode();


    if (numWindows == 0) {
        // Force-reset to non-touch mode.
        // The user of this class only sees the non-touch modes,
        // but the hardware may be in touch mode -> need reset.
        setAfMode(afMode);
        return NO_ERROR;
    }

    // Conversion function from AtomCommon
    convertAfWindows(windows, numWindows, convWindow);

    // In case actual AF window was provided, we go into *touch* mode.

    // select ext-ISP touch-AF mode corresponding to current AF mode:
    switch (afMode) {
        case CAM_AF_MODE_MACRO:
            drvModeToSet = EXT_ISP_FOCUS_MODE_TOUCH_MACRO;
            break;
        case CAM_AF_MODE_AUTO:
            drvModeToSet = EXT_ISP_FOCUS_MODE_TOUCH_AF;
            break;
        case CAM_AF_MODE_CONTINUOUS: // TODO: MOVIE CAF!
            drvModeToSet = EXT_ISP_FOCUS_MODE_TOUCH_CAF;
            break;
        default:
            ALOGW("Unsupported ext-ISP touch AF mode (%d), using NORMAL", afMode);
            drvModeToSet = EXT_ISP_FOCUS_MODE_NORMAL;
            break;
    }

    // TODO: convert from android to viewfinder user coordinates here

    int ret = mSensorCI->setAfMode(drvModeToSet);
    if (ret != 0) {
        return UNKNOWN_ERROR;
    }

    ret = mSensorCI->setAfWindows(windows, numWindows);
    if (ret != 0) {
        ALOGE("Error setting AF  window to driver");
        return UNKNOWN_ERROR;
    }

    // We need to explicitly start the AF after coordinates are set,
    // so that the AF will actually execute...
    if (setAfEnabled(true) != 0)
        ALOGW("Failed to enable AF");

    return NO_ERROR;
}

status_t AtomExtIsp3A::startStillAf()
{
    LOG1("@%s", __FUNCTION__);
    // NOTE: Intentionally empty implementation to make this
    // a no-op, at least for now.
    return NO_ERROR;

    // TODO: Fix the usage of setAfEnabled() and startStillAf() to be
    // more clear in the I3AControls subclasses. Now the usage is a bit
    // mixed and overlapping

}

status_t AtomExtIsp3A::stopStillAf()
{
    LOG1("@%s", __FUNCTION__);
    // NOTE: Intentionally empty implementation to make this
    // a no-op, at least for now.
    return NO_ERROR;
}

status_t AtomExtIsp3A::setAeFlashMode(FlashMode mode)
{
    LOG1("@%s: %d", __FUNCTION__, mode);
    status_t status = NO_ERROR;

    if (!strcmp(PlatformData::supportedFlashModes(mCameraId), "")) {
        LOG1("@%s: not supported by current camera", __FUNCTION__);
        return INVALID_OPERATION;
    }

    int modeTmp = EXT_ISP_FLASH_MODE_OFF;
    int currFlashMode = getAeFlashMode();

    // Current flash mode already the same as requested:
    if (currFlashMode == mode)
        return NO_ERROR;

    switch (mode) {
        case CAM_AE_FLASH_MODE_OFF:
            modeTmp = EXT_ISP_FLASH_MODE_OFF;
            break;
        case CAM_AE_FLASH_MODE_ON:
            modeTmp = EXT_ISP_FLASH_MODE_ON;
            break;
        case CAM_AE_FLASH_MODE_AUTO:
            modeTmp = EXT_ISP_FLASH_MODE_AUTO;
            break;
        case CAM_AE_FLASH_MODE_TORCH:
            modeTmp = EXT_ISP_LED_TORCH_ON;
            break;
        default:
            ALOGW("Unsupported Flash mode (%d), using OFF", mode);
            modeTmp = EXT_ISP_FLASH_MODE_OFF;
            break;
    }

    int ret = 0;

    // Set torch off, if it is currently on, and something else is requested:
    // NOTE: EXT_ISP_LED_TORCH_OFF is only used for setting torch off,
    // no need to remember the state in mDrvFlashMode
    if (mode != currFlashMode && currFlashMode == CAM_AE_FLASH_MODE_TORCH)
        ret = mSensorCI->setAeFlashMode(EXT_ISP_LED_TORCH_OFF);

    if (ret != 0) {
        ALOGE("Error setting torch to \"off\"");
        return UNKNOWN_ERROR;
    }

    // Set the requested flash mode:
    ret = mSensorCI->setAeFlashMode(modeTmp);
    if (ret != 0) {
        ALOGD("Error setting Flash mode (%d) in the driver", modeTmp);
        status = UNKNOWN_ERROR;
    } else {
        mDrvFlashMode = modeTmp;
    }

    return status;
}

FlashMode AtomExtIsp3A::getAeFlashMode()
{
    LOG1("@%s", __FUNCTION__);
    FlashMode mode = CAM_AE_FLASH_MODE_OFF;

    if (!strcmp(PlatformData::supportedFlashModes(mCameraId), "")) {
        LOG1("@%s: not supported by current camera", __FUNCTION__);
        return mode;
    }

    switch (mDrvFlashMode) {
        case EXT_ISP_FLASH_MODE_OFF:
            mode = CAM_AE_FLASH_MODE_OFF;
            break;
        case EXT_ISP_FLASH_MODE_ON:
            mode = CAM_AE_FLASH_MODE_ON;
            break;
        case EXT_ISP_FLASH_MODE_AUTO:
            mode = CAM_AE_FLASH_MODE_AUTO;
            break;
        case EXT_ISP_LED_TORCH_ON:
            mode = CAM_AE_FLASH_MODE_TORCH;
            break;
        default:
            ALOGW("Unsupported Flash mode got (%d), using NOT_SET", mDrvFlashMode);
            mode = CAM_AE_FLASH_MODE_NOT_SET;
            break;
    }

    return mode;
}

void AtomExtIsp3A::setFaceDetection(bool enabled)
{
    LOG1("@%s", __FUNCTION__);
    AfMode currAfMode = getAfMode();

    mFaceDetectionActive = enabled;

    // TODO: movie CAF and face detection?
    // - We do not support face detection in video currently...
    if (enabled && currAfMode == CAM_AF_MODE_CONTINUOUS) {
        // If current AF is continuous, set the AF mode to FACE_CAF
        int ret = mSensorCI->setAfMode(EXT_ISP_FOCUS_MODE_FACE_CAF);

        if (ret != 0) {
            ALOGE("Error setting FACE AF mode in the driver");
            return;
        }

        mDrvAfMode = EXT_ISP_FOCUS_MODE_FACE_CAF;
    } else if (!enabled) {
        // FD got disabled, reset to current "normal" AF mode
        setAfMode(currAfMode);
    }

    // Explicitly need AF_START to go into the set CAF mode
    setAfEnabled(true);
}

} // namespace android
