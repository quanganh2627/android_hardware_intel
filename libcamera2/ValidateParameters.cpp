/*
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

#define LOG_TAG "Camera_ValidateParameters"

#include "ValidateParameters.h"
#include "AtomCommon.h"
#include "LogHelper.h"
#include "PlatformData.h"

#include <stdlib.h>

namespace android {

static bool validateSize(int width, int height, Vector<Size> &supportedSizes, bool onlyWarning)
{
    if (width < 0 || height < 0)
        return false;

    for (Vector<Size>::iterator it = supportedSizes.begin(); it != supportedSizes.end(); ++it)
        if (width == it->width && height == it->height)
            return true;

    if (onlyWarning) {
        ALOGW("WARNING: The Size %dx%d is not fully supported. Some issues might occur!", width, height);
        return true;
    } else {
        ALOGE("Invalid size %dx%d is not in supported list.", width, height);
        return false;
    }
}

/**
 * Validate that given parameter value is allowed boolean string
 * ("true","false" or not set) and true value is only set, when
 * isSupported() is true.
 *
 * \param paramName [in] name of parameter to validate
 * \param paramSupportedName [in] isSupported parameter name
 * \param params [in] camera parameters set
 *
 *  \return true if parameter value is valid, else false
 **/
static bool validateBoolParameter(const char* paramName, const char* paramSupportedName, const CameraParameters *params)
{
    LOG2("@%s", __FUNCTION__);

    if (paramName == NULL || paramSupportedName == NULL || params == NULL) {
        ALOGE("%s: Invalid argument!",  __FUNCTION__);
        return false;
    }

    const char* value = params->get(paramName);

    // allow not set (it mean false value)
    if (value == NULL)  {
        return true;
    }

    if (strcmp(value, CameraParameters::TRUE) != 0  && strcmp(value, CameraParameters::FALSE) != 0) {
        ALOGE("Bad value(%s) for %s. Not bool value.", value,  paramName);
        return false;
    }

    if (isParameterSet(paramName, *params) && !isParameterSet(paramSupportedName, *params)) {
        ALOGE("bad value for %s, is set, but not supported", paramName);
        return false;
    }

    return true;
}

bool validateString(const char* value,  const char* supportList)
{
    // value should not set if support list is empty
    if (value !=NULL && supportList == NULL) {
        return false;
    }

    if (value == NULL || supportList == NULL) {
        return true;
    }

    size_t len = strlen(value);
    const char* startPtr(supportList);
    const char* endPtr(supportList);
    int bracketLevel(0);

    // divide support list to values and compare those to given values.
    // values are separated with comma in support list, but commas also exist
    // part of values inside bracket.
    while (true) {
        if ( *endPtr == '(') {
            ++bracketLevel;
        } else if (*endPtr == ')') {
            --bracketLevel;
        } else if ( bracketLevel == 0 && ( *endPtr == '\0' || *endPtr == ',')) {
            if (((startPtr + len) == endPtr) &&
                (strncmp(value, startPtr, len) == 0)) {
                return true;
            }

            // bracket can use circle values in supported list
            if (((startPtr + len + 2 ) == endPtr) &&
                ( *startPtr == '(') &&
                (strncmp(value, startPtr + 1, len) == 0)) {
                return true;
            }
            startPtr = endPtr + 1;
        }

        if (*endPtr == '\0') {
            return false;
        }
        ++endPtr;
    }

    return false;
}

static bool isParamsEqual(const char *oldParam, const char *newParam)
{
    // same if both are null
    if (oldParam == NULL && newParam == NULL) {
        return true;
    }

    // different if one is null
    if (oldParam == NULL || newParam == NULL) {
        return false;
    }

    // both set, so compare strings
    return (strcmp(oldParam,newParam) == 0);
}

/**
 * Validate read-only prarameters
 *
 * This function check that values of given parameters are same in old and
 * new parameters sets.
 *
 * \param oldParams    is pointer to old parameters
 * \param params       is pointer new parameters
 * \param ...          is NULL terminated list of parameters to validate
 *
 */
static void validateReadOnlyParameters(const CameraParameters *oldParams, const CameraParameters *params, ...)
{
    LOG2("@%s", __FUNCTION__);

    va_list arguments;
    const char* name = NULL;

    va_start(arguments, params);
    name = va_arg(arguments, const char*);

    while (name != NULL) {
        if(!isParamsEqual(oldParams->get(name), params->get(name))) {
              ALOGW("Change of read-only parameter %s", name);
              LOG1("The changed value was %s, restoring the old value %s", params->get(name), oldParams->get(name));
              CameraParameters *wParams = const_cast<CameraParameters *>(params);
              wParams->set(name, oldParams->get(name));
        }
        name = va_arg(arguments, const char*);
    }

    va_end(arguments);
}

status_t validateParameters(const CameraParameters *oldParams, const CameraParameters *params, int cameraId)
{
    LOG1("@%s: oldparams= %p, params = %p", __FUNCTION__, oldParams, params);

    bool sizeErrorOnlyWarning(true);
    if (PlatformData::supportsContinuousJpegCapture(cameraId)) {
        sizeErrorOnlyWarning = false;
    }

    // READ-ONLY PARAMETERS
    validateReadOnlyParameters(oldParams, params,
                               // Google Parameters
                               CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES,
                               CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE,
                               CameraParameters::KEY_SUPPORTED_PREVIEW_FORMATS,
                               CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES,
                               CameraParameters::KEY_SUPPORTED_PICTURE_SIZES,
                               CameraParameters::KEY_SUPPORTED_PICTURE_FORMATS,
                               CameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES,
                               CameraParameters::KEY_SUPPORTED_WHITE_BALANCE,
                               CameraParameters::KEY_SUPPORTED_EFFECTS,
                               CameraParameters::KEY_SUPPORTED_SCENE_MODES,
                               CameraParameters::KEY_MAX_NUM_FOCUS_AREAS,
                               CameraParameters::KEY_AUTO_EXPOSURE_LOCK_SUPPORTED,
                               CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK_SUPPORTED,
                               CameraParameters::KEY_MAX_ZOOM,
                               CameraParameters::KEY_ZOOM_RATIOS,
                               CameraParameters::KEY_ZOOM_SUPPORTED,
                               CameraParameters::KEY_SMOOTH_ZOOM_SUPPORTED,
                               CameraParameters::KEY_SUPPORTED_VIDEO_SIZES,
                               CameraParameters::KEY_MAX_NUM_DETECTED_FACES_HW,
                               CameraParameters::KEY_MAX_NUM_DETECTED_FACES_SW,
                               CameraParameters::KEY_VIDEO_SNAPSHOT_SUPPORTED,
                               CameraParameters::KEY_VIDEO_STABILIZATION_SUPPORTED,
                               // Intel Parameters
                               IntelCameraParameters::KEY_SUPPORTED_NOISE_REDUCTION_AND_EDGE_ENHANCEMENT,
                               IntelCameraParameters::KEY_SUPPORTED_MULTI_ACCESS_COLOR_CORRECTIONS,
                               IntelCameraParameters::KEY_SUPPORTED_AE_MODES,
                               IntelCameraParameters::KEY_SUPPORTED_SHUTTER,
                               IntelCameraParameters::KEY_SUPPORTED_APERTURE,
                               IntelCameraParameters::KEY_SUPPORTED_AF_LOCK_MODES,
                               IntelCameraParameters::KEY_SUPPORTED_BACK_LIGHTING_CORRECTION_MODES,
                               IntelCameraParameters::KEY_SUPPORTED_AF_METERING_MODES,
                               IntelCameraParameters::KEY_SUPPORTED_BURST_LENGTH,
                               IntelCameraParameters::KEY_SUPPORTED_BURST_FPS,
                               IntelCameraParameters::KEY_SUPPORTED_BURST_SPEED,
                               IntelCameraParameters::KEY_SUPPORTED_PREVIEW_UPDATE_MODE,
                               IntelCameraParameters::KEY_SUPPORTED_RAW_DATA_FORMATS,
                               IntelCameraParameters::KEY_SUPPORTED_CAPTURE_BRACKET,
                               IntelCameraParameters::KEY_SUPPORTED_ROTATION_MODES,
                               IntelCameraParameters::KEY_SUPPORTED_HDR_IMAGING,
                               IntelCameraParameters::KEY_SUPPORTED_HDR_SAVE_ORIGINAL,
                               IntelCameraParameters::KEY_SUPPORTED_SMILE_SHUTTER,
                               IntelCameraParameters::KEY_SUPPORTED_BLINK_SHUTTER,
                               IntelCameraParameters::KEY_SUPPORTED_FACE_DETECTION,
                               IntelCameraParameters::KEY_SUPPORTED_FACE_RECOGNITION,
                               IntelCameraParameters::KEY_SUPPORTED_SCENE_DETECTION,
                               IntelCameraParameters::KEY_SUPPORTED_PANORAMA,
                               IntelCameraParameters::KEY_SUPPORTED_PANORAMA_LIVE_PREVIEW_SIZES,
                               IntelCameraParameters::KEY_PANORAMA_MAX_SNAPSHOT_COUNT,
                               IntelCameraParameters::KEY_HW_OVERLAY_RENDERING_SUPPORTED,
                               IntelCameraParameters::KEY_SUPPORTED_SLOW_MOTION_RATE,
                               IntelCameraParameters::KEY_SUPPORTED_RECORDING_FRAME_RATES,
                               IntelCameraParameters::KEY_SUPPORTED_HIGH_SPEED_RESOLUTION_FPS,
                               IntelCameraParameters::KEY_DUAL_MODE_SUPPORTED,
                               IntelCameraParameters::KEY_SUPPORTED_DUAL_CAMERA_MODE,
                               IntelCameraParameters::KEY_SUPPORTED_BURST_START_INDEX,
                               IntelCameraParameters::KEY_MAX_BURST_LENGTH_WITH_NEGATIVE_START_INDEX,
                               IntelCameraParameters::KEY_SUPPORTED_CONTRAST_MODES,
                               IntelCameraParameters::KEY_SUPPORTED_SATURATION_MODES,
                               IntelCameraParameters::KEY_SUPPORTED_SHARPNESS_MODES,
                               IntelCameraParameters::KEY_SUPPORTED_ULL,
                               IntelCameraParameters::KEY_SDV_SUPPORTED,
                               IntelCameraParameters::KEY_SUPPORTED_SAVE_MIRRORED,
                               IntelCameraParameters::KEY_SUPPORTED_GPS_IMG_DIRECTION_REF,
                               NULL);

    // scene mode change overdrive some read-only parameters in process parameters, so we check
    // those only if scene mode is not changed.
    if(isParamsEqual(oldParams->get(CameraParameters::KEY_SCENE_MODE), params->get(CameraParameters::KEY_SCENE_MODE))) {
        validateReadOnlyParameters(oldParams, params,
                                   // Google Parameters
                                   CameraParameters::KEY_SUPPORTED_FOCUS_MODES,
                                   CameraParameters::KEY_SUPPORTED_ANTIBANDING,
                                   CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION,
                                   CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION,
                                   CameraParameters::KEY_EXPOSURE_COMPENSATION_STEP,
                                   // Intel Parameters
                                   IntelCameraParameters::KEY_SUPPORTED_ISO,
                                   IntelCameraParameters::KEY_SUPPORTED_AWB_MAPPING_MODES,
                                   IntelCameraParameters::KEY_SUPPORTED_AE_METERING_MODES,
                                   IntelCameraParameters::KEY_SUPPORTED_XNR,
                                   IntelCameraParameters::KEY_SUPPORTED_ANR,
                                   NULL);
    }

    // PREVIEW
    int width, height;
    Vector<Size> supportedSizes;
    params->getSupportedPreviewSizes(supportedSizes);
    if (PlatformData::supportsContinuousJpegCapture(cameraId)) {
        // for ext-isp, we add the 6MP resolution so that application can set
        // that for panorama. It is not a public supported resolution for any
        // other use case (capable of only 15fps).
        Size size6mp(RESOLUTION_6MP_WIDTH, RESOLUTION_6MP_HEIGHT);
        supportedSizes.add(size6mp);
    }
    params->getPreviewSize(&width, &height);
    if (!validateSize(width, height, supportedSizes, sizeErrorOnlyWarning)) {
        ALOGE("bad preview size");
        return BAD_VALUE;
    }

    // PREVIEW_FPS_RANGE
    int minFPS, maxFPS;
    params->getPreviewFpsRange(&minFPS, &maxFPS);
    // getPreviewFrameRate() returns -1 fps value if the range-pair string is malformatted
    const char* fpsRange = params->get(CameraParameters::KEY_PREVIEW_FPS_RANGE);
    const char* fpsRanges = params->get(CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE);
    if ((fpsRange && fpsRanges && strstr(fpsRanges, fpsRange) == NULL) ||
        minFPS < 0 || maxFPS < 0) {
            ALOGE("invalid fps range: %s; supported %s", fpsRange, fpsRanges);
            return BAD_VALUE;
    }

    // VIDEO
    params->getVideoSize(&width, &height);
    supportedSizes.clear();
    params->getSupportedVideoSizes(supportedSizes);
    if (!validateSize(width, height, supportedSizes, sizeErrorOnlyWarning)) {
        ALOGE("bad video size %dx%d", width, height);
        return BAD_VALUE;
    }

    // RECORDING FRAME RATE
    const char* recordingFps = params->get(IntelCameraParameters::KEY_RECORDING_FRAME_RATE);
    const char* supportedRecordingFps = params->get(IntelCameraParameters::KEY_SUPPORTED_RECORDING_FRAME_RATES);
    if (!validateString(recordingFps, supportedRecordingFps)) {
        ALOGE("bad recording frame rate: %s, supported: %s", recordingFps, supportedRecordingFps);
        return BAD_VALUE;
    }

    // SNAPSHOT
    params->getPictureSize(&width, &height);
    supportedSizes.clear();
    params->getSupportedPictureSizes(supportedSizes);
    if (width == 0 && height == 0) {
        LOG2("@%s: snapshot size auto select HACK in use", __FUNCTION__);
    } else {
        if (!validateSize(width, height, supportedSizes, sizeErrorOnlyWarning)) {
            ALOGE("bad picture size");
            return BAD_VALUE;
        }
    }

    // JPEG QUALITY
    int jpegQuality = params->getInt(CameraParameters::KEY_JPEG_QUALITY);
    if (jpegQuality < 1 || jpegQuality > 100) {
        ALOGE("bad jpeg quality: %d", jpegQuality);
        return BAD_VALUE;
    }

    // THUMBNAIL QUALITY
    int thumbQuality = params->getInt(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY);
    if (thumbQuality < 1 || thumbQuality > 100) {
        ALOGE("bad thumbnail quality: %d", thumbQuality);
        return BAD_VALUE;
    }

    // THUMBNAIL SIZE
    int thumbWidth = params->getInt(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH);
    int thumbHeight = params->getInt(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT);
    char* thumbnailSizes = (char*) params->get(CameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES);
    supportedSizes.clear();
    if (thumbnailSizes != NULL) {
        while (true) {
            int width = (int)strtol(thumbnailSizes, &thumbnailSizes, 10);
            int height = (int)strtol(thumbnailSizes+1, &thumbnailSizes, 10);
            supportedSizes.push(Size(width, height));
            if (*thumbnailSizes == '\0')
                break;
            ++thumbnailSizes;
        }
        if (!validateSize(thumbWidth, thumbHeight, supportedSizes, sizeErrorOnlyWarning)) {
            ALOGE("bad thumbnail size: (%d,%d)", thumbWidth, thumbHeight);
            return BAD_VALUE;
        }
    } else {
        ALOGE("bad thumbnail size");
        return BAD_VALUE;
    }
    // PICTURE FORMAT
    const char* picFormat = params->get(CameraParameters::KEY_PICTURE_FORMAT);
    const char* picFormats = params->get(CameraParameters::KEY_SUPPORTED_PICTURE_FORMATS);
    if (!validateString(picFormat, picFormats)) {
        ALOGE("bad picture fourcc: %s", picFormat);
        return BAD_VALUE;
    }

    // PREVIEW FORMAT
    const char* preFormat = params->get(CameraParameters::KEY_PREVIEW_FORMAT);
    const char* preFormats = params->get(CameraParameters::KEY_SUPPORTED_PREVIEW_FORMATS);
    if (!validateString(preFormat, preFormats))  {
        ALOGE("bad preview fourcc: %s", preFormat);
        return BAD_VALUE;
    }

    // ROTATION, can only be 0 ,90, 180 or 270.
    int rotation = params->getInt(CameraParameters::KEY_ROTATION);
    if (rotation != 0 && rotation != 90 && rotation != 180 && rotation != 270) {
        ALOGE("bad rotation value: %d", rotation);
        return BAD_VALUE;
    }


    // WHITE BALANCE
    const char* whiteBalance = params->get(CameraParameters::KEY_WHITE_BALANCE);
    const char* whiteBalances = params->get(CameraParameters::KEY_SUPPORTED_WHITE_BALANCE);
    if (!validateString(whiteBalance, whiteBalances)) {
        ALOGE("bad white balance mode: %s", whiteBalance);
        return BAD_VALUE;
    }

    // ZOOM
    int zoom = params->getInt(CameraParameters::KEY_ZOOM);
    int maxZoom = params->getInt(CameraParameters::KEY_MAX_ZOOM);
    if (zoom > maxZoom || zoom < 0) {
        ALOGE("bad zoom index: %d", zoom);
        return BAD_VALUE;
    }

    // FLASH
    const char* flashMode = params->get(CameraParameters::KEY_FLASH_MODE);
    const char* flashModes = params->get(CameraParameters::KEY_SUPPORTED_FLASH_MODES);
    if (!validateString(flashMode, flashModes)) {
        ALOGE("bad flash mode");
        return BAD_VALUE;
    }

    // SCENE MODE
    const char* sceneMode = params->get(CameraParameters::KEY_SCENE_MODE);
    const char* sceneModes = params->get(CameraParameters::KEY_SUPPORTED_SCENE_MODES);
    if (!validateString(sceneMode, sceneModes)) {
        ALOGE("bad scene mode: %s; supported: %s", sceneMode, sceneModes);
        return BAD_VALUE;
    }

    // FOCUS
    const char* focusMode = params->get(CameraParameters::KEY_FOCUS_MODE);
    const char* focusModes = params->get(CameraParameters::KEY_SUPPORTED_FOCUS_MODES);
    if (!validateString(focusMode, focusModes)) {
        ALOGE("bad focus mode: %s; supported: %s", focusMode, focusModes);
        return BAD_VALUE;
    }

    // BURST LENGTH
    const char* burstLength = params->get(IntelCameraParameters::KEY_BURST_LENGTH);
    const char* burstLengths = params->get(IntelCameraParameters::KEY_SUPPORTED_BURST_LENGTH);
     int burstMaxLengthNegative = params->getInt(IntelCameraParameters::KEY_MAX_BURST_LENGTH_WITH_NEGATIVE_START_INDEX);
    if (!validateString(burstLength, burstLengths)) {
        ALOGE("bad burst length: %s; supported: %s", burstLength, burstLengths);
        return BAD_VALUE;
    }
    const char* burstStart = params->get(IntelCameraParameters::KEY_BURST_START_INDEX);
    if (burstStart) {
        int burstStartInt = atoi(burstStart);
        if (burstStartInt < 0) {
            const char* captureBracket = params->get(IntelCameraParameters::KEY_CAPTURE_BRACKET);
            if (captureBracket && String8(captureBracket) != "none") {
                ALOGE("negative start-index and bracketing not supported concurrently");
                return BAD_VALUE;
            }
            int len = burstLength ? atoi(burstLength) : 0;
            if (len > burstMaxLengthNegative) {
                ALOGE("negative start-index and burst-length=%d not supported concurrently", len);
                return BAD_VALUE;
            }
        }
    }

    // BURST SPEED
    const char* burstSpeed = params->get(IntelCameraParameters::KEY_BURST_SPEED);
    const char* burstSpeeds = params->get(IntelCameraParameters::KEY_SUPPORTED_BURST_SPEED);
    if (!validateString(burstSpeed, burstSpeeds)) {
        ALOGE("bad burst speed: %s; supported: %s", burstSpeed, burstSpeeds);
        return BAD_VALUE;
    }

    // OVERLAY
    const char* overlaySupported = params->get(IntelCameraParameters::KEY_HW_OVERLAY_RENDERING_SUPPORTED);
    const char* overlay = params->get(IntelCameraParameters::KEY_HW_OVERLAY_RENDERING);
        if (!validateString(overlay, overlaySupported)) {
        ALOGE("bad overlay rendering mode: %s; supported: %s", overlay, overlaySupported);
        return BAD_VALUE;
    }

    // MISCELLANEOUS
    const char *size = params->get(IntelCameraParameters::KEY_PANORAMA_LIVE_PREVIEW_SIZE);
    const char *livePreviewSizes = IntelCameraParameters::getSupportedPanoramaLivePreviewSizes(*params);
    if (!validateString(size, livePreviewSizes)) {
        ALOGE("bad panorama live preview size");
        return BAD_VALUE;
    }

    // ANTI FLICKER
    const char* flickerMode = params->get(CameraParameters::KEY_ANTIBANDING);
    const char* flickerModes = params->get(CameraParameters::KEY_SUPPORTED_ANTIBANDING);
    if (!validateString(flickerMode, flickerModes)) {
        ALOGE("bad anti flicker mode");
        return BAD_VALUE;
    }

    // COLOR EFFECT
    const char* colorEffect = params->get(CameraParameters::KEY_EFFECT);
    const char* colorEffects = params->get(CameraParameters::KEY_SUPPORTED_EFFECTS);
    if (!validateString(colorEffect, colorEffects)) {
        ALOGE("bad color effect: %s", colorEffect);
        return BAD_VALUE;
    }

    // EXPOSURE COMPENSATION
    int exposure = params->getInt(CameraParameters::KEY_EXPOSURE_COMPENSATION);
    int minExposure = params->getInt(CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION);
    int maxExposure = params->getInt(CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION);
    if (exposure > maxExposure || exposure < minExposure) {
        ALOGE("bad exposure compensation value: %d", exposure);
        return BAD_VALUE;
    }

    //Note: here for Intel expand parameters, add additional validity check
    //for their supported list. when they're null, we return bad value for
    //these intel parameters setting. As "noise reduction and edge enhancement"
    //and "multi access color correction" are not supported yet.

    // NOISE_REDUCTION_AND_EDGE_ENHANCEMENT
    const char* noiseReductionAndEdgeEnhancement = params->get(IntelCameraParameters::KEY_NOISE_REDUCTION_AND_EDGE_ENHANCEMENT);
    const char* noiseReductionAndEdgeEnhancements = params->get(IntelCameraParameters::KEY_SUPPORTED_NOISE_REDUCTION_AND_EDGE_ENHANCEMENT);
    if (!validateString(noiseReductionAndEdgeEnhancement, noiseReductionAndEdgeEnhancements)) {
        ALOGE("bad noise reduction and edge enhancement value : %s", noiseReductionAndEdgeEnhancement);
        return BAD_VALUE;
    }

    // MULTI_ACCESS_COLOR_CORRECTION
    const char* multiAccessColorCorrection = params->get(IntelCameraParameters::KEY_MULTI_ACCESS_COLOR_CORRECTION);
    const char* multiAccessColorCorrections = params->get(IntelCameraParameters::KEY_SUPPORTED_MULTI_ACCESS_COLOR_CORRECTIONS);
    if (!validateString(multiAccessColorCorrection, multiAccessColorCorrections)) {
        ALOGE("bad multi access color correction value : %s", multiAccessColorCorrection);
        return BAD_VALUE;
    }

    // AUTO EXPOSURE LOCK
    if (!validateBoolParameter(CameraParameters::KEY_AUTO_EXPOSURE_LOCK,CameraParameters::KEY_AUTO_EXPOSURE_LOCK_SUPPORTED, params)) {
        ALOGE("bad value for auto exporsure lock");
        return BAD_VALUE;
    }

    // AUTO WHITEBALANCE LOCK
    if (!validateBoolParameter(CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK, CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK_SUPPORTED, params)) {
        ALOGE("bad value for auto whitebalance lock");
        return BAD_VALUE;
    }

    // DVS (VIDEO STABILIZATION)
    if (!validateBoolParameter(CameraParameters::KEY_VIDEO_STABILIZATION, CameraParameters::KEY_VIDEO_STABILIZATION_SUPPORTED, params)) {
        ALOGE("bad value for DVS");
        return BAD_VALUE;
    }

    // SDV (still during video)
    if (!validateBoolParameter(IntelCameraParameters::KEY_SDV, IntelCameraParameters::KEY_SDV_SUPPORTED, params)) {
        ALOGE("bad value for SDV");
        return BAD_VALUE;
    }

    // Dual mode
    if (!validateBoolParameter(IntelCameraParameters::KEY_DUAL_MODE, IntelCameraParameters::KEY_DUAL_MODE_SUPPORTED, params)) {
        ALOGE("bad value for dual mode");
        return BAD_VALUE;
    }

    // Dual Camera Mode
    const char* dualCameraMode = params->get(IntelCameraParameters::KEY_DUAL_CAMERA_MODE);
    const char* dualCameraModes = params->get(IntelCameraParameters::KEY_SUPPORTED_DUAL_CAMERA_MODE);
    if (!validateString(dualCameraMode, dualCameraModes)) {
        ALOGE("bad value for dual camera mode: %s", dualCameraMode);
        return BAD_VALUE;
    }

    // continuous shooting
    if (!validateBoolParameter(IntelCameraParameters::KEY_CONTINUOUS_SHOOTING, IntelCameraParameters::KEY_CONTINUOUS_SHOOTING_SUPPORTED, params)) {
        ALOGE("bad value for continuous shooting");
        return BAD_VALUE;
    }

    // CONTRAST
    const char* contrastMode = params->get(IntelCameraParameters::KEY_CONTRAST_MODE);
    const char* contrastModes = params->get(IntelCameraParameters::KEY_SUPPORTED_CONTRAST_MODES);
    if (!validateString(contrastMode, contrastModes)) {
        ALOGE("bad contrast mode: %s", contrastMode);
        return BAD_VALUE;
    }

    // SATURATION
    const char* saturationMode = params->get(IntelCameraParameters::KEY_SATURATION_MODE);
    const char* saturationModes = params->get(IntelCameraParameters::KEY_SUPPORTED_SATURATION_MODES);
    if (!validateString(saturationMode, saturationModes)) {
        ALOGE("bad saturation mode: %s", saturationMode);
        return BAD_VALUE;
    }

    // SHARPNESS
    const char* sharpnessMode = params->get(IntelCameraParameters::KEY_SHARPNESS_MODE);
    const char* sharpnessModes = params->get(IntelCameraParameters::KEY_SUPPORTED_SHARPNESS_MODES);
    if (!validateString(sharpnessMode, sharpnessModes)) {
        ALOGE("bad sharpness mode: %s", sharpnessMode);
        return BAD_VALUE;
    }

    // intelligent mode
    const char* intelligentMode = params->get(IntelCameraParameters::KEY_INTELLIGENT_MODE);
    const char* intelligentModes = params->get(IntelCameraParameters::KEY_SUPPORTED_INTELLIGENT_MODE);
    if (!validateString(intelligentMode, intelligentModes)) {
        ALOGE("bad intelligent mode: %s", intelligentMode);
        return BAD_VALUE;
    }

    // color-bar mode
    const char* colorbarMode = params->get(IntelCameraParameters::KEY_COLORBAR);
    const char* colorbarModes = params->get(IntelCameraParameters::KEY_SUPPORTED_COLORBAR);
    if (!validateString(colorbarMode, colorbarModes)) {
        ALOGE("bad colorbar mode: %s", colorbarMode);
        return BAD_VALUE;
    }

    return NO_ERROR;
}

};
