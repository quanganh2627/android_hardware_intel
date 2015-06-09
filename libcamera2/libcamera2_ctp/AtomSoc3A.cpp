/*
 * Copyright (c) 2013 Intel Corporation.
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
#define LOG_TAG "Camera_SOC3A"

#include "LogHelper.h"
#include "Callbacks.h"
#include "ColorConverter.h"
#include "PlatformData.h"
#include "IntelParameters.h"
#include "CameraDump.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <poll.h>
#include <linux/media.h>
#include <linux/atomisp.h>
#include "PerformanceTraces.h"
#include "AtomSoc3A.h"
#include "ICameraHwControls.h"

namespace android {

////////////////////////////////////////////////////////////////////
//                          PUBLIC METHODS
////////////////////////////////////////////////////////////////////

AtomSoc3A::AtomSoc3A(int cameraId, HWControlGroup &hwcg) :
    mCameraId(cameraId)
    ,mISP(hwcg.mIspCI)
    ,mSensorCI(hwcg.mSensorCI)
    ,mFlashCI(hwcg.mFlashCI)
    ,mLensCI(hwcg.mLensCI)
    ,mPublicAeMode(CAM_AE_MODE_AUTO)
{
    LOG2("@%s", __FUNCTION__);
}

AtomSoc3A::~AtomSoc3A()
{
    LOG2("@%s", __FUNCTION__);
}

// I3AControls

status_t AtomSoc3A::init3A()
{
    // We don't need this memory anymore
    PlatformData::AiqConfig.clear();

    return NO_ERROR;
}

status_t AtomSoc3A::deinit3A()
{
    return NO_ERROR;
}

status_t AtomSoc3A::setAeMode(AeMode mode)
{
    LOG1("@%s: %d", __FUNCTION__, mode);
    status_t status = NO_ERROR;
    v4l2_exposure_auto_type v4lMode;

    // TODO: add supported modes to PlatformData
    if (mCameraId > 0) {
        LOG1("@%s: not supported by current camera", __FUNCTION__);
        return INVALID_OPERATION;
    }

    switch (mode)
    {
        case CAM_AE_MODE_AUTO:
            v4lMode = V4L2_EXPOSURE_AUTO;
            break;
        case CAM_AE_MODE_MANUAL:
            v4lMode = V4L2_EXPOSURE_MANUAL;
            break;
        case CAM_AE_MODE_SHUTTER_PRIORITY:
            v4lMode = V4L2_EXPOSURE_SHUTTER_PRIORITY;
            break;
        case CAM_AE_MODE_APERTURE_PRIORITY:
            v4lMode = V4L2_EXPOSURE_APERTURE_PRIORITY;
            break;
        default:
            ALOGW("Unsupported AE mode (%d), using AUTO", mode);
            v4lMode = V4L2_EXPOSURE_AUTO;
    }

    // TODO: If the sensor does not support the setting, we should return NO_ERROR

    int ret = mSensorCI->setExposureMode(v4lMode);
    if (ret != 0) {
        ALOGE("Error setting AE mode (%d) in the driver", v4lMode);
        status = UNKNOWN_ERROR;
    }

    return status;
}

AeMode AtomSoc3A::getAeMode()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    AeMode mode = CAM_AE_MODE_NOT_SET;
    v4l2_exposure_auto_type v4lMode = V4L2_EXPOSURE_AUTO;

    // TODO: add supported modes to PlatformData
    if (mCameraId > 0) {
        LOG1("@%s: not supported by current camera", __FUNCTION__);
        return mode;
    }

    int ret = mSensorCI->getExposureMode(&v4lMode);
    if (ret != 0) {
        ALOGE("Error getting AE mode from the driver");
        status = UNKNOWN_ERROR;
    }

    switch (v4lMode)
    {
        case V4L2_EXPOSURE_AUTO:
            mode = CAM_AE_MODE_AUTO;
            break;
        case V4L2_EXPOSURE_MANUAL:
            mode = CAM_AE_MODE_MANUAL;
            break;
        case V4L2_EXPOSURE_SHUTTER_PRIORITY:
            mode = CAM_AE_MODE_SHUTTER_PRIORITY;
            break;
        case V4L2_EXPOSURE_APERTURE_PRIORITY:
            mode = CAM_AE_MODE_APERTURE_PRIORITY;
            break;
        default:
            ALOGW("Unsupported AE mode (%d), using AUTO", v4lMode);
            mode = CAM_AE_MODE_AUTO;
    }

    return mode;
}

status_t AtomSoc3A::setEv(float bias)
{
    status_t status = NO_ERROR;
    int evValue = (int)bias;
    LOG1("@%s: bias: %f, EV value: %d", __FUNCTION__, bias, evValue);

    int ret = mSensorCI->setExposureBias(evValue);
    if (ret != 0) {
        ALOGE("Error setting EV in the driver");
        status = UNKNOWN_ERROR;
    }

    return status;
}

status_t AtomSoc3A::getEv(float *bias)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    int evValue = 0;

    int ret = mSensorCI->getExposureBias(&evValue);
    if (ret != 0) {
        ALOGE("Error getting EV from the driver");
        status = UNKNOWN_ERROR;
    }
    *bias = (float)evValue;

    return status;
}

status_t AtomSoc3A::setAeSceneMode(SceneMode mode)
{
    LOG1("@%s: %d", __FUNCTION__, mode);
    status_t status = NO_ERROR;
    v4l2_scene_mode v4lMode;

    if (!strcmp(PlatformData::supportedSceneModes(mCameraId), "")) {
        LOG1("@%s: not supported by current camera", __FUNCTION__);
        return INVALID_OPERATION;
    }

    switch (mode)
    {
        case CAM_AE_SCENE_MODE_AUTO:
            v4lMode = V4L2_SCENE_MODE_NONE;
            break;
        case CAM_AE_SCENE_MODE_PORTRAIT:
            v4lMode = V4L2_SCENE_MODE_PORTRAIT;
            break;
        case CAM_AE_SCENE_MODE_SPORTS:
            v4lMode = V4L2_SCENE_MODE_SPORTS;
            break;
        case CAM_AE_SCENE_MODE_LANDSCAPE:
            v4lMode = V4L2_SCENE_MODE_LANDSCAPE;
            break;
        case CAM_AE_SCENE_MODE_NIGHT:
            v4lMode = V4L2_SCENE_MODE_NIGHT;
            break;
        case CAM_AE_SCENE_MODE_NIGHT_PORTRAIT:
            v4lMode = V4L2_SCENE_MODE_NIGHT;
            break;
        case CAM_AE_SCENE_MODE_FIREWORKS:
            v4lMode = V4L2_SCENE_MODE_FIREWORKS;
            break;
        case CAM_AE_SCENE_MODE_TEXT:
            v4lMode = V4L2_SCENE_MODE_TEXT;
            break;
        case CAM_AE_SCENE_MODE_SUNSET:
            v4lMode = V4L2_SCENE_MODE_SUNSET;
            break;
        case CAM_AE_SCENE_MODE_PARTY:
            v4lMode = V4L2_SCENE_MODE_PARTY_INDOOR;
            break;
        case CAM_AE_SCENE_MODE_CANDLELIGHT:
            v4lMode = V4L2_SCENE_MODE_CANDLE_LIGHT;
            break;
        case CAM_AE_SCENE_MODE_BEACH_SNOW:
            v4lMode = V4L2_SCENE_MODE_BEACH_SNOW;
            break;
        case CAM_AE_SCENE_MODE_DAWN_DUSK:
            v4lMode = V4L2_SCENE_MODE_DAWN_DUSK;
            break;
        case CAM_AE_SCENE_MODE_FALL_COLORS:
            v4lMode = V4L2_SCENE_MODE_FALL_COLORS;
            break;
        case CAM_AE_SCENE_MODE_BACKLIGHT:
            v4lMode = V4L2_SCENE_MODE_BACKLIGHT;
            break;
        default:
            ALOGW("Unsupported scene mode (%d), using NONE", mode);
            v4lMode = V4L2_SCENE_MODE_NONE;
    }

    // TODO: If the sensor does not support the setting, we should return NO_ERROR

    int ret = mSensorCI->setSceneMode(v4lMode);
    if (ret != 0) {
        ALOGE("Error setting scene mode in the driver");
        status = UNKNOWN_ERROR;
    }

    return status;
}

SceneMode AtomSoc3A::getAeSceneMode()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    SceneMode mode = CAM_AE_SCENE_MODE_NOT_SET;
    v4l2_scene_mode v4lMode = V4L2_SCENE_MODE_NONE;

    if (!strcmp(PlatformData::supportedSceneModes(mCameraId), "")) {
        LOG1("@%s: not supported by current camera", __FUNCTION__);
        return mode;
    }

    int ret = mSensorCI->getSceneMode(&v4lMode);
    if (ret != 0) {
        ALOGE("Error getting scene mode from the driver");
        status = UNKNOWN_ERROR;
    }

    switch (v4lMode)
    {
        case V4L2_SCENE_MODE_NONE:
            mode = CAM_AE_SCENE_MODE_AUTO;
            break;
        case V4L2_SCENE_MODE_PORTRAIT:
            mode = CAM_AE_SCENE_MODE_PORTRAIT;
            break;
        case V4L2_SCENE_MODE_SPORTS:
            mode = CAM_AE_SCENE_MODE_SPORTS;
            break;
        case V4L2_SCENE_MODE_LANDSCAPE:
            mode = CAM_AE_SCENE_MODE_LANDSCAPE;
            break;
        case V4L2_SCENE_MODE_NIGHT:
            mode = CAM_AE_SCENE_MODE_NIGHT;
            break;
        case V4L2_SCENE_MODE_FIREWORKS:
            mode = CAM_AE_SCENE_MODE_FIREWORKS;
            break;
        case V4L2_SCENE_MODE_TEXT:
            mode = CAM_AE_SCENE_MODE_TEXT;
            break;
        case V4L2_SCENE_MODE_SUNSET:
            mode = CAM_AE_SCENE_MODE_SUNSET;
            break;
        case V4L2_SCENE_MODE_PARTY_INDOOR:
            mode = CAM_AE_SCENE_MODE_PARTY;
            break;
        case V4L2_SCENE_MODE_CANDLE_LIGHT:
            mode = CAM_AE_SCENE_MODE_CANDLELIGHT;
            break;
        case V4L2_SCENE_MODE_BEACH_SNOW:
            mode = CAM_AE_SCENE_MODE_BEACH_SNOW;
            break;
        case V4L2_SCENE_MODE_DAWN_DUSK:
            mode = CAM_AE_SCENE_MODE_DAWN_DUSK;
            break;
        case V4L2_SCENE_MODE_FALL_COLORS:
            mode = CAM_AE_SCENE_MODE_FALL_COLORS;
            break;
        case V4L2_SCENE_MODE_BACKLIGHT:
            mode = CAM_AE_SCENE_MODE_BACKLIGHT;
            break;
        default:
            ALOGW("Unsupported scene mode (%d), using AUTO", v4lMode);
            mode = CAM_AE_SCENE_MODE_AUTO;
    }

    return mode;
}

status_t AtomSoc3A::setAwbMode(AwbMode mode)
{
    LOG1("@%s: %d", __FUNCTION__, mode);
    status_t status = NO_ERROR;
    v4l2_auto_n_preset_white_balance v4lMode;

    if (!strcmp(PlatformData::supportedAwbModes(mCameraId), "")) {
        LOG1("@%s: not supported by current camera", __FUNCTION__);
        return INVALID_OPERATION;
    }

    switch (mode)
    {
        case CAM_AWB_MODE_AUTO:
            v4lMode = V4L2_WHITE_BALANCE_AUTO;
            break;
        case CAM_AWB_MODE_MANUAL_INPUT:
            v4lMode = V4L2_WHITE_BALANCE_MANUAL;
            break;
        case CAM_AWB_MODE_DAYLIGHT:
            v4lMode = V4L2_WHITE_BALANCE_DAYLIGHT;
            break;
        case CAM_AWB_MODE_SUNSET:
            v4lMode = V4L2_WHITE_BALANCE_INCANDESCENT;
            break;
        case CAM_AWB_MODE_CLOUDY:
            v4lMode = V4L2_WHITE_BALANCE_CLOUDY;
            break;
        case CAM_AWB_MODE_TUNGSTEN:
            v4lMode = V4L2_WHITE_BALANCE_INCANDESCENT;
            break;
        case CAM_AWB_MODE_FLUORESCENT:
            v4lMode = V4L2_WHITE_BALANCE_FLUORESCENT;
            break;
        case CAM_AWB_MODE_WARM_FLUORESCENT:
            v4lMode = V4L2_WHITE_BALANCE_FLUORESCENT_H;
            break;
        case CAM_AWB_MODE_SHADOW:
            v4lMode = V4L2_WHITE_BALANCE_SHADE;
            break;
        case CAM_AWB_MODE_WARM_INCANDESCENT:
            v4lMode = V4L2_WHITE_BALANCE_INCANDESCENT;
            break;
        default:
            ALOGW("Unsupported AWB mode %d", mode);
            v4lMode = V4L2_WHITE_BALANCE_AUTO;
    }

    // TODO: If the sensor does not support the setting, we should return NO_ERROR

    int ret = mSensorCI->setWhiteBalance(v4lMode);
    if (ret != 0) {
        ALOGE("Error setting WB mode (%d) in the driver", v4lMode);
        status = UNKNOWN_ERROR;
    }

    return status;
}

AwbMode AtomSoc3A::getAwbMode()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    AwbMode mode = CAM_AWB_MODE_NOT_SET;
    v4l2_auto_n_preset_white_balance v4lMode = V4L2_WHITE_BALANCE_AUTO;

    if (!strcmp(PlatformData::supportedAwbModes(mCameraId), "")) {
        LOG1("@%s: not supported by current camera", __FUNCTION__);
        return mode;
    }

    int ret = mSensorCI->getWhiteBalance(&v4lMode);
    if (ret != 0) {
        ALOGE("Error getting WB mode from the driver");
        status = UNKNOWN_ERROR;
    }

    switch (v4lMode)
    {
        case V4L2_WHITE_BALANCE_AUTO:
            mode = CAM_AWB_MODE_AUTO;
            break;
        case V4L2_WHITE_BALANCE_MANUAL:
            mode = CAM_AWB_MODE_MANUAL_INPUT;
            break;
        case V4L2_WHITE_BALANCE_DAYLIGHT:
            mode = CAM_AWB_MODE_DAYLIGHT;
            break;
        case V4L2_WHITE_BALANCE_CLOUDY:
            mode = CAM_AWB_MODE_CLOUDY;
            break;
        case V4L2_WHITE_BALANCE_INCANDESCENT:
            mode = CAM_AWB_MODE_TUNGSTEN;
            break;
        case V4L2_WHITE_BALANCE_FLUORESCENT:
            mode = CAM_AWB_MODE_FLUORESCENT;
            break;
        case V4L2_WHITE_BALANCE_FLUORESCENT_H:
            mode = CAM_AWB_MODE_WARM_FLUORESCENT;
            break;
        case V4L2_WHITE_BALANCE_SHADE:
            mode = CAM_AWB_MODE_SHADOW;
            break;
        default:
            ALOGW("Unsupported AWB mode %d", v4lMode);
            mode = CAM_AWB_MODE_AUTO;
    }

    return mode;
}

status_t AtomSoc3A::setManualIso(int iso)
{
    LOG1("@%s: ISO: %d", __FUNCTION__, iso);
    status_t status = NO_ERROR;

    if (!strcmp(PlatformData::supportedIso(mCameraId), "")) {
        LOG1("@%s: not supported by current camera", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    int ret = mSensorCI->setIso(iso);
    if (ret != 0) {
        ALOGE("Error setting ISO in the driver");
        status = UNKNOWN_ERROR;
    }

    return status;
}

status_t AtomSoc3A::getManualIso(int *iso)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    if (!strcmp(PlatformData::supportedIso(mCameraId), "")) {
        LOG1("@%s: not supported by current camera", __FUNCTION__);
        return INVALID_OPERATION;
    }

    int ret = mSensorCI->getIso(iso);
    if (ret != 0) {
        ALOGE("Error getting ISO from the driver");
        status = UNKNOWN_ERROR;
    }

    return status;
}

status_t AtomSoc3A::setIsoMode(IsoMode mode) {
    /*ISO mode not supported for SOC sensor yet.*/
    LOG1("@%s", __FUNCTION__);
    status_t status = INVALID_OPERATION;
    return status;
}

IsoMode AtomSoc3A::getIsoMode(void) {
    /*ISO mode not supported for SOC sensor yet.*/
    return CAM_AE_ISO_MODE_NOT_SET;
}

status_t AtomSoc3A::setAeMeteringMode(MeteringMode mode)
{
    LOG1("@%s: %d", __FUNCTION__, mode);
    status_t status = NO_ERROR;
    v4l2_exposure_metering v4lMode;

    if (!strcmp(PlatformData::supportedAeMetering(mCameraId), "")) {
        LOG1("@%s: not supported by current camera", __FUNCTION__);
        return INVALID_OPERATION;
    }

    switch (mode)
    {
        case CAM_AE_METERING_MODE_AUTO:
            v4lMode = V4L2_EXPOSURE_METERING_AVERAGE;
            break;
        case CAM_AE_METERING_MODE_SPOT:
            v4lMode = V4L2_EXPOSURE_METERING_SPOT;
            break;
        case CAM_AE_METERING_MODE_CENTER:
            v4lMode = V4L2_EXPOSURE_METERING_CENTER_WEIGHTED;
            break;
        default:
            ALOGW("Unsupported AE metering mode (%d), using AVERAGE", mode);
            v4lMode = V4L2_EXPOSURE_METERING_AVERAGE;
            break;
    }

    int ret = mSensorCI->setAeMeteringMode(v4lMode);
    if (ret != 0) {
        ALOGE("Error setting AE metering mode (%d) in the driver", v4lMode);
        status = UNKNOWN_ERROR;
    }

    return status;
}

MeteringMode AtomSoc3A::getAeMeteringMode()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    MeteringMode mode = CAM_AE_METERING_MODE_NOT_SET;
    v4l2_exposure_metering v4lMode = V4L2_EXPOSURE_METERING_AVERAGE;

    if (!strcmp(PlatformData::supportedAeMetering(mCameraId), "")) {
        LOG1("@%s: not supported by current camera", __FUNCTION__);
        return mode;
    }

    int ret = mSensorCI->getAeMeteringMode(&v4lMode);
    if (ret != 0) {
        ALOGE("Error getting AE metering mode from the driver");
        status = UNKNOWN_ERROR;
    }

    switch (v4lMode)
    {
        case V4L2_EXPOSURE_METERING_CENTER_WEIGHTED:
            mode = CAM_AE_METERING_MODE_CENTER;
            break;
        case V4L2_EXPOSURE_METERING_SPOT:
            mode = CAM_AE_METERING_MODE_SPOT;
            break;
        case V4L2_EXPOSURE_METERING_AVERAGE:
            mode = CAM_AE_METERING_MODE_AUTO;
            break;
        default:
            ALOGW("Unsupported AE metering mode (%d), using AUTO", v4lMode);
            mode = CAM_AE_METERING_MODE_AUTO;
            break;
    }

    return mode;
}

status_t AtomSoc3A::set3AColorEffect(const char *effect)
{
    LOG1("@%s: effect = %s", __FUNCTION__, effect);
    status_t status = NO_ERROR;

    v4l2_colorfx v4l2Effect = V4L2_COLORFX_NONE;
    if (strncmp(effect, CameraParameters::EFFECT_MONO, sizeof(effect)) == 0)
        v4l2Effect = V4L2_COLORFX_BW;
    else if (strncmp(effect, CameraParameters::EFFECT_NEGATIVE, sizeof(effect)) == 0)
        v4l2Effect = V4L2_COLORFX_NEGATIVE;
    else if (strncmp(effect, CameraParameters::EFFECT_SEPIA, sizeof(effect)) == 0)
        v4l2Effect = V4L2_COLORFX_SEPIA;
    else if (strncmp(effect, IntelCameraParameters::EFFECT_STILL_SKY_BLUE, sizeof(effect)) == 0)
        v4l2Effect = V4L2_COLORFX_SKY_BLUE;
    else if (strncmp(effect, IntelCameraParameters::EFFECT_STILL_GRASS_GREEN, sizeof(effect)) == 0)
        v4l2Effect = V4L2_COLORFX_GRASS_GREEN;
    else if (strncmp(effect, IntelCameraParameters::EFFECT_STILL_SKIN_WHITEN_MEDIUM, sizeof(effect)) == 0)
        v4l2Effect = V4L2_COLORFX_SKIN_WHITEN;
    else if (strncmp(effect, IntelCameraParameters::EFFECT_VIVID, sizeof(effect)) == 0)
        v4l2Effect = V4L2_COLORFX_VIVID;

    // following two values need a explicit cast as the
    // definitions in hardware/intel/linux-2.6/include/linux/atomisp.h
    // have incorrect type (properly defined values are in videodev2.h)
    else if (effect == IntelCameraParameters::EFFECT_STILL_SKIN_WHITEN_LOW)
        v4l2Effect = (v4l2_colorfx)V4L2_COLORFX_SKIN_WHITEN_LOW;
    else if (effect == IntelCameraParameters::EFFECT_STILL_SKIN_WHITEN_HIGH)
        v4l2Effect = (v4l2_colorfx)V4L2_COLORFX_SKIN_WHITEN_HIGH;
    else if (strncmp(effect, CameraParameters::EFFECT_NONE, strlen(effect)) != 0){
        ALOGE("Color effect not found.");
        status = -1;
        // Fall back to the effect NONE
    }

    status = mISP->setColorEffect(v4l2Effect);
    status = mISP->applyColorEffect();
    return status;
}

status_t AtomSoc3A::setAeFlickerMode(FlickerMode flickerMode)
{
    LOG2("@%s: %d", __FUNCTION__, (int)flickerMode);
    status_t status(NO_ERROR);
    v4l2_power_line_frequency theMode(V4L2_CID_POWER_LINE_FREQUENCY_DISABLED);

    switch(flickerMode) {
    case CAM_AE_FLICKER_MODE_50HZ:
        theMode = V4L2_CID_POWER_LINE_FREQUENCY_50HZ;
        break;
    case CAM_AE_FLICKER_MODE_60HZ:
        theMode = V4L2_CID_POWER_LINE_FREQUENCY_60HZ;
        break;
    case CAM_AE_FLICKER_MODE_OFF:
        theMode = V4L2_CID_POWER_LINE_FREQUENCY_DISABLED;
        break;
    case CAM_AE_FLICKER_MODE_AUTO:
        theMode = V4L2_CID_POWER_LINE_FREQUENCY_AUTO;
        break;
    default:
        ALOGE("unsupported light frequency mode(%d)", (int)flickerMode);
        status = BAD_VALUE;
        return status;
    }

    int ret = mSensorCI->setAeFlickerMode(theMode);
    if (ret != 0) {
        ALOGE("Error setting AE flicker mode (%d) in the driver", theMode);
        status = UNKNOWN_ERROR;
    }

    return status;
}

void AtomSoc3A::getDefaultParams(CameraParameters *params, CameraParameters *intel_params)
{
    LOG1("@%s", __FUNCTION__);
    if (!params) {
        ALOGE("params is null!");
        return;
    }

    // multipoint focus
    params->set(CameraParameters::KEY_MAX_NUM_FOCUS_AREAS, 0);
    // set empty area
    params->set(CameraParameters::KEY_FOCUS_AREAS, "(0,0,0,0,0)");

    // metering areas
    params->set(CameraParameters::KEY_MAX_NUM_METERING_AREAS, 0);
    // set empty area
    params->set(CameraParameters::KEY_METERING_AREAS, "(0,0,0,0,0)");

    // TODO: Add here any V4L2 3A specific settings
}

status_t AtomSoc3A::setAfMode(AfMode mode)
{
    LOG1("@%s: %d", __FUNCTION__, mode);
    status_t status = NO_ERROR;
    v4l2_auto_focus_range v4lMode;

    // TODO: add supported modes to PlatformData
    if (mCameraId > 0) {
        LOG1("@%s: not supported by current camera", __FUNCTION__);
        return INVALID_OPERATION;
    }

    switch (mode)
    {
        case CAM_AF_MODE_AUTO:
            v4lMode = V4L2_AUTO_FOCUS_RANGE_AUTO;
            break;
        case CAM_AF_MODE_MACRO:
            v4lMode = V4L2_AUTO_FOCUS_RANGE_MACRO;
            break;
        case CAM_AF_MODE_INFINITY:
            v4lMode = V4L2_AUTO_FOCUS_RANGE_INFINITY;
            break;
        default:
            ALOGW("Unsupported AF mode (%d), using AUTO", mode);
            v4lMode = V4L2_AUTO_FOCUS_RANGE_AUTO;
            break;
    }

    // TODO: If the sensor does not support the setting, we should return NO_ERROR

    int ret = mSensorCI->setAfMode(v4lMode);
    if (ret != 0) {
        ALOGE("Error setting AF  mode (%d) in the driver", v4lMode);
        status = UNKNOWN_ERROR;
    }

    return status;
}

AfMode AtomSoc3A::getAfMode()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    AfMode mode = CAM_AF_MODE_AUTO;
    v4l2_auto_focus_range v4lMode = V4L2_AUTO_FOCUS_RANGE_AUTO;

    // TODO: add supported modes to PlatformData
    if (mCameraId > 0) {
        LOG1("@%s: not supported by current camera", __FUNCTION__);
        return mode;
    }

    int ret = mSensorCI->getAfMode(&v4lMode);
    if (ret != 0) {
        ALOGE("Error getting AF mode from the driver");
        status = UNKNOWN_ERROR;
    }

    switch (v4lMode)
    {
        case V4L2_AUTO_FOCUS_RANGE_AUTO:
            mode = CAM_AF_MODE_AUTO;
            break;
        case CAM_AF_MODE_MACRO:
            mode = CAM_AF_MODE_MACRO;
            break;
        case CAM_AF_MODE_INFINITY:
            mode = CAM_AF_MODE_INFINITY;
            break;
        default:
            ALOGW("Unsupported AF mode (%d), using AUTO", v4lMode);
            mode = CAM_AF_MODE_AUTO;
            break;
    }

    return mode;
}

status_t AtomSoc3A::getGridWindow(AAAWindowInfo& window)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    atomisp_parm isp_param;
    if (mISP->getIspParameters(&isp_param) < 0)
        return UNKNOWN_ERROR;

    window.width = isp_param.info.s3a_width * isp_param.info.s3a_bqs_per_grid_cell * 2;
    window.height = isp_param.info.s3a_height * isp_param.info.s3a_bqs_per_grid_cell * 2;

    return status;
}

status_t AtomSoc3A::setAfEnabled(bool enable)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    // TODO: add supported modes to PlatformData
    if (mCameraId > 0) {
        LOG1("@%s: not supported by current camera", __FUNCTION__);
        return INVALID_OPERATION;
    }

    int ret = mSensorCI->setAfEnabled(enable);
    if (ret != 0) {
        ALOGE("Error setting Auto Focus (%d) in the driver", enable);
        status = UNKNOWN_ERROR;
    }

    return status;
}

int AtomSoc3A::get3ALock()
{
    LOG1("@%s", __FUNCTION__);
    int aaaLock = 0;

    int ret = mSensorCI->get3ALock(&aaaLock);
    if (ret != 0) {
        ALOGE("Error getting 3A Lock setting from the driver");
    }

    return aaaLock;
}

bool AtomSoc3A::getAeLock()
{
    LOG1("@%s", __FUNCTION__);
    bool aeLockEnabled = false;

    int aaaLock = get3ALock();
    if (aaaLock & V4L2_LOCK_EXPOSURE)
        aeLockEnabled = true;

    return aeLockEnabled;
}

status_t AtomSoc3A::setAeLock(bool enable)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    int aaaLock = get3ALock();
    if (enable)
        aaaLock |= V4L2_LOCK_EXPOSURE;
    else
        aaaLock &= ~V4L2_LOCK_EXPOSURE;

    int ret = mSensorCI->set3ALock(aaaLock);
    if (ret != 0) {
        ALOGE("Error setting AE lock (%d) in the driver", enable);
        status = UNKNOWN_ERROR;
    }

    return status;
}

bool AtomSoc3A::getAfLock()
{
    LOG1("@%s", __FUNCTION__);
    bool afLockEnabled = false;

    int aaaLock = get3ALock();
    if (aaaLock & V4L2_LOCK_FOCUS)
        afLockEnabled = true;

    return afLockEnabled;
}

status_t AtomSoc3A::setAfLock(bool enable)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    int aaaLock = get3ALock();
    if (enable)
        aaaLock |= V4L2_LOCK_FOCUS;
    else
        aaaLock &= ~V4L2_LOCK_FOCUS;

    int ret = mSensorCI->set3ALock(aaaLock);
    if (ret != 0) {
        ALOGE("Error setting AF lock (%d) in the driver", enable);
        status = UNKNOWN_ERROR;
    }

    return status;
}

bool AtomSoc3A::getAwbLock()
{
    LOG1("@%s", __FUNCTION__);
    bool awbLockEnabled = false;

    int aaaLock = get3ALock();
    if (aaaLock & V4L2_LOCK_WHITE_BALANCE)
        awbLockEnabled = true;

    return awbLockEnabled;
}

status_t AtomSoc3A::setAwbLock(bool enable)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    int aaaLock = get3ALock();
    if (enable)
        aaaLock |= V4L2_LOCK_WHITE_BALANCE;
    else
        aaaLock &= ~V4L2_LOCK_WHITE_BALANCE;

    int ret = mSensorCI->set3ALock(aaaLock);
    if (ret != 0) {
        ALOGE("Error setting AWB lock (%d) in the driver", enable);
        status = UNKNOWN_ERROR;
    }

    return status;
}

status_t AtomSoc3A::applyEv(float bias)
{
    LOG1("@%s: bias: %f", __FUNCTION__, bias);
    return setEv(bias);
}

status_t AtomSoc3A::setManualShutter(float expTime)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    int time = expTime / 0.0001; // 100 usec units

    int ret = mSensorCI->setExposureTime(time);
    if (ret != 0) {
        ALOGE("Error setting Exposure time (%d) in the driver", time);
        status = UNKNOWN_ERROR;
    }

    return status;
}

status_t AtomSoc3A::setAeFlashMode(FlashMode mode)
{
    LOG1("@%s: %d", __FUNCTION__, mode);
    status_t status = NO_ERROR;
    v4l2_flash_led_mode v4lMode;

    if (!strcmp(PlatformData::supportedFlashModes(mCameraId), "")) {
        LOG1("@%s: not supported by current camera", __FUNCTION__);
        return INVALID_OPERATION;
    }

    switch (mode)
    {
        case CAM_AE_FLASH_MODE_OFF:
            v4lMode = V4L2_FLASH_LED_MODE_NONE;
            break;
        case CAM_AE_FLASH_MODE_ON:
            v4lMode = V4L2_FLASH_LED_MODE_FLASH;
            break;
        case CAM_AE_FLASH_MODE_TORCH:
            v4lMode = V4L2_FLASH_LED_MODE_TORCH;
            break;
        default:
            ALOGW("Unsupported Flash mode (%d), using OFF", mode);
            v4lMode = V4L2_FLASH_LED_MODE_NONE;
            break;
    }

    int ret = mSensorCI->setAeFlashMode(v4lMode);
    if (ret != 0) {
        ALOGE("Error setting Flash mode (%d) in the driver", v4lMode);
        status = UNKNOWN_ERROR;
    }

    return status;
}

FlashMode AtomSoc3A::getAeFlashMode()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    FlashMode mode = CAM_AE_FLASH_MODE_OFF;
    v4l2_flash_led_mode v4lMode = V4L2_FLASH_LED_MODE_NONE;

    if (!strcmp(PlatformData::supportedFlashModes(mCameraId), "")) {
        LOG1("@%s: not supported by current camera", __FUNCTION__);
        return mode;
    }

    int ret = mSensorCI->setAeFlashMode(v4lMode);
    if (ret != 0) {
        ALOGE("Error getting Flash mode from the driver");
        status = UNKNOWN_ERROR;
    }

    switch (v4lMode)
    {
        case V4L2_FLASH_LED_MODE_NONE:
            mode = CAM_AE_FLASH_MODE_OFF;
            break;
        case V4L2_FLASH_LED_MODE_FLASH:
            mode = CAM_AE_FLASH_MODE_ON;
            break;
        case V4L2_FLASH_LED_MODE_TORCH:
            mode = CAM_AE_FLASH_MODE_TORCH;
            break;
        default:
            ALOGW("Unsupported Flash mode (%d), using OFF", v4lMode);
            mode = CAM_AE_FLASH_MODE_OFF;
            break;
    }

    return mode;
}

void AtomSoc3A::setPublicAeMode(AeMode mode)
{
    LOG2("@%s", __FUNCTION__);
    mPublicAeMode = mode;
}

AeMode AtomSoc3A::getPublicAeMode()
{
    LOG2("@%s", __FUNCTION__);
    return mPublicAeMode;
}

status_t AtomSoc3A::setFlash(int numFrames)
{
    return mFlashCI->setFlash(numFrames);
}

} // namespace android
