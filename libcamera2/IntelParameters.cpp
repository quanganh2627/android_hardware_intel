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

#include "IntelParameters.h"
#include "LogHelper.h"
#include <stdlib.h>

namespace android {

    // IMPORTANT NOTE:
    //
    // Intel specific parameters should be defined here,
    // so that any version of HAL can be built against
    // non-Intel libcamera_client (e.g. vanilla AOS
    // libcamera_client). This allows in cases where customers
    // are making own modifications and may not have all Intel
    // parameters defined in their version of CameraParameters.cpp.

    const char IntelCameraParameters::KEY_FOCUS_WINDOW[] = "focus-window";
    const char IntelCameraParameters::KEY_RAW_DATA_FORMAT[] = "raw-data-format";
    const char IntelCameraParameters::KEY_SUPPORTED_RAW_DATA_FORMATS[] = "raw-data-format-values";
    const char IntelCameraParameters::KEY_CAPTURE_BRACKET[] = "capture-bracket";
    const char IntelCameraParameters::KEY_SUPPORTED_CAPTURE_BRACKET[] = "capture-bracket-values";
    const char IntelCameraParameters::KEY_HDR_IMAGING[] = "hdr-imaging";
    const char IntelCameraParameters::KEY_SUPPORTED_HDR_IMAGING[] = "hdr-imaging-values";
    const char IntelCameraParameters::KEY_HDR_SAVE_ORIGINAL[] = "hdr-save-original";
    const char IntelCameraParameters::KEY_SUPPORTED_HDR_SAVE_ORIGINAL[] = "hdr-save-original-values";
    const char IntelCameraParameters::KEY_ROTATION_MODE[] = "rotation-mode";
    const char IntelCameraParameters::KEY_SUPPORTED_ROTATION_MODES[] = "rotation-mode-values";
    const char IntelCameraParameters::KEY_FRONT_SENSOR_FLIP[] = "front-sensor-flip";

    // Dual mode
    const char IntelCameraParameters::KEY_DUAL_MODE[] = "dual-mode";
    const char IntelCameraParameters::KEY_DUAL_MODE_SUPPORTED[] = "dual-mode-supported";

    // Dual camera mode
    const char IntelCameraParameters::KEY_DUAL_CAMERA_MODE[] = "dual-camera-mode";
    const char IntelCameraParameters::KEY_SUPPORTED_DUAL_CAMERA_MODE[] = "dual-camera-mode-values";
    const char IntelCameraParameters::DUAL_CAMERA_MODE_NORMAL[] = "normal";
    const char IntelCameraParameters::DUAL_CAMERA_MODE_DEPTH[] = "depth";

    // Ultra low light
    const char IntelCameraParameters::KEY_ULL[] = "ull";
    const char IntelCameraParameters::KEY_SUPPORTED_ULL[] = "ull-values";

    // SDV
    const char IntelCameraParameters::KEY_SDV[] = "sdv";
    const char IntelCameraParameters::KEY_SDV_SUPPORTED[] = "sdv-supported";

    // continuous shooting
    const char IntelCameraParameters::KEY_CONTINUOUS_SHOOTING[] = "continuous-shooting";
    const char IntelCameraParameters::KEY_CONTINUOUS_SHOOTING_SUPPORTED[] = "continuous-shooting-supported";
    const char IntelCameraParameters::KEY_CONTINUOUS_SHOOTING_FILEPATH[] = "capture-burst-filepath";

    // 3A related
    const char IntelCameraParameters::KEY_AE_MODE[] = "ae-mode";
    const char IntelCameraParameters::KEY_SUPPORTED_AE_MODES[] = "ae-mode-values";

    const char IntelCameraParameters::KEY_AE_METERING_MODE[] = "ae-metering-mode";
    const char IntelCameraParameters::KEY_SUPPORTED_AE_METERING_MODES[] = "ae-metering-mode-values";
    const char IntelCameraParameters::KEY_AF_METERING_MODE[] = "af-metering-mode";
    const char IntelCameraParameters::KEY_SUPPORTED_AF_METERING_MODES[] = "af-metering-mode-values";
    const char IntelCameraParameters::KEY_AF_LOCK_MODE[] = "af-lock-mode";
    const char IntelCameraParameters::KEY_SUPPORTED_AF_LOCK_MODES[] = "af-lock-mode-values";
    const char IntelCameraParameters::KEY_BACK_LIGHTING_CORRECTION_MODE[] = "back-lighting-correction-mode";
    const char IntelCameraParameters::KEY_SUPPORTED_BACK_LIGHTING_CORRECTION_MODES[] = "back-lighting-correction-mode-values";
    const char IntelCameraParameters::KEY_AWB_MAPPING_MODE[] = "awb-mapping-mode";
    const char IntelCameraParameters::KEY_SUPPORTED_AWB_MAPPING_MODES[] = "awb-mapping-mode-values";
    const char IntelCameraParameters::KEY_SHUTTER[] = "shutter";
    const char IntelCameraParameters::KEY_SUPPORTED_SHUTTER[] = "shutter-values";
    const char IntelCameraParameters::KEY_APERTURE[] = "aperture";
    const char IntelCameraParameters::KEY_SUPPORTED_APERTURE[] = "aperture-values";
    const char IntelCameraParameters::KEY_ISO[] = "iso";
    const char IntelCameraParameters::KEY_SUPPORTED_ISO[] = "iso-values";
    const char IntelCameraParameters::KEY_COLOR_TEMPERATURE[] = "color-temperature";

    // ISP related
    const char IntelCameraParameters::KEY_XNR[] = "xnr";
    const char IntelCameraParameters::KEY_SUPPORTED_XNR[] = "xnr-values";
    const char IntelCameraParameters::KEY_ANR[] = "anr";
    const char IntelCameraParameters::KEY_SUPPORTED_ANR[] = "anr-values";
    const char IntelCameraParameters::KEY_NOISE_REDUCTION_AND_EDGE_ENHANCEMENT[] = "noise-reduction-and-edge-enhancement";
    const char IntelCameraParameters::KEY_SUPPORTED_NOISE_REDUCTION_AND_EDGE_ENHANCEMENT[] = "noise-reduction-and-edge-enhancement-values";
    const char IntelCameraParameters::KEY_MULTI_ACCESS_COLOR_CORRECTION[] = "multi-access-color-correction";
    const char IntelCameraParameters::KEY_SUPPORTED_MULTI_ACCESS_COLOR_CORRECTIONS[] = "multi-access-color-correction-values";

    const char IntelCameraParameters::EFFECT_STILL_SKY_BLUE[] = "still-sky-blue";
    const char IntelCameraParameters::EFFECT_STILL_GRASS_GREEN[] = "still-grass-green";
    const char IntelCameraParameters::EFFECT_STILL_SKIN_WHITEN_LOW[] = "still-skin-whiten-low";
    const char IntelCameraParameters::EFFECT_STILL_SKIN_WHITEN_MEDIUM[] = "still-skin-whiten-medium";
    const char IntelCameraParameters::EFFECT_STILL_SKIN_WHITEN_HIGH[] = "still-skin-whiten-high";

    // burst capture
    const char IntelCameraParameters::KEY_SUPPORTED_BURST_LENGTH[] = "burst-length-values";
    const char IntelCameraParameters::KEY_BURST_LENGTH[] = "burst-length";
    const char IntelCameraParameters::KEY_SUPPORTED_BURST_FPS[] = "burst-fps-values"; // TODO: old API, it will be deleted in the future.
    const char IntelCameraParameters::KEY_BURST_FPS[] = "burst-fps"; // TODO: old API, it will be deleted in the future.
    const char IntelCameraParameters::KEY_SUPPORTED_BURST_SPEED[] = "burst-speed-values";
    const char IntelCameraParameters::KEY_BURST_SPEED[] = "burst-speed";
    const char IntelCameraParameters::KEY_BURST_START_INDEX[] = "burst-start-index";
    const char IntelCameraParameters::KEY_SUPPORTED_BURST_START_INDEX[] = "burst-start-index-values";
    const char IntelCameraParameters::KEY_MAX_BURST_LENGTH_WITH_NEGATIVE_START_INDEX[] = "burst-max-length-negative";
    //values for burst speed
    const char IntelCameraParameters::BURST_SPEED_FAST[] = "fast";
    const char IntelCameraParameters::BURST_SPEED_MEDIUM[] = "medium";
    const char IntelCameraParameters::BURST_SPEED_LOW[] = "low";

    // preview update mode
    const char IntelCameraParameters::KEY_SUPPORTED_PREVIEW_UPDATE_MODE[] = "preview-update-mode-values";
    const char IntelCameraParameters::KEY_PREVIEW_UPDATE_MODE[] = "preview-update-mode";
    const char IntelCameraParameters::PREVIEW_UPDATE_MODE_STANDARD[] = "standard";
    const char IntelCameraParameters::PREVIEW_UPDATE_MODE_DURING_CAPTURE[] = "during-capture";
    const char IntelCameraParameters::PREVIEW_UPDATE_MODE_CONTINUOUS[] = "continuous";
    const char IntelCameraParameters::PREVIEW_UPDATE_MODE_WINDOWLESS[] = "windowless";

    // smart shutter
    const char IntelCameraParameters::KEY_SMILE_SHUTTER[] = "smile-shutter";
    const char IntelCameraParameters::KEY_SUPPORTED_SMILE_SHUTTER[] = "smile-shutter-values";
    const char IntelCameraParameters::KEY_SMILE_SHUTTER_THRESHOLD[] = "smile-shutter-threshold";
    const char IntelCameraParameters::KEY_BLINK_SHUTTER[] = "blink-shutter";
    const char IntelCameraParameters::KEY_SUPPORTED_BLINK_SHUTTER[] = "blink-shutter-values";
    const char IntelCameraParameters::KEY_BLINK_SHUTTER_THRESHOLD[] = "blink-shutter-threshold";

    const char IntelCameraParameters::KEY_PANORAMA[] = "panorama";
    const char IntelCameraParameters::KEY_SUPPORTED_PANORAMA[] = "panorama-values";
    const char IntelCameraParameters::KEY_PANORAMA_MAX_SNAPSHOT_COUNT[] = "panorama-max-snapshot-count";
    const char IntelCameraParameters::KEY_SUPPORTED_FACE_DETECTION[] = "face-detection-values";
    const char IntelCameraParameters::KEY_SUPPORTED_FACE_RECOGNITION[] = "face-recognition-values";
    const char IntelCameraParameters::KEY_SCENE_DETECTION[] = "scene-detection";
    const char IntelCameraParameters::KEY_SUPPORTED_SCENE_DETECTION[] = "scene-detection-values";
    // for 3A
    // Values for ae mode settings
    const char IntelCameraParameters::AE_MODE_AUTO[] = "auto";
    const char IntelCameraParameters::AE_MODE_MANUAL[] = "manual";
    const char IntelCameraParameters::AE_MODE_SHUTTER_PRIORITY[] = "shutter-priority";
    const char IntelCameraParameters::AE_MODE_APERTURE_PRIORITY[] = "aperture-priority";

    // Values for focus mode settings.
    const char IntelCameraParameters::FOCUS_MODE_MANUAL[] = "manual";
    const char IntelCameraParameters::FOCUS_MODE_TOUCH[] = "touch";

    // Values for flash mode settings.
    const char IntelCameraParameters::FLASH_MODE_DAY_SYNC[] = "day-sync";
    const char IntelCameraParameters::FLASH_MODE_SLOW_SYNC[] = "slow-sync";

    //values for ae metering mode
    const char IntelCameraParameters::AE_METERING_MODE_AUTO[] = "auto";
    const char IntelCameraParameters::AE_METERING_MODE_SPOT[] = "spot";
    const char IntelCameraParameters::AE_METERING_MODE_CENTER[] = "center";
    const char IntelCameraParameters::AE_METERING_MODE_CUSTOMIZED[] = "customized";

    //values for af metering mode
    const char IntelCameraParameters::AF_METERING_MODE_AUTO[] = "auto";
    const char IntelCameraParameters::AF_METERING_MODE_SPOT[] = "spot";

    //values for ae lock mode
    const char IntelCameraParameters::AE_LOCK_LOCK[] = "lock";
    const char IntelCameraParameters::AE_LOCK_UNLOCK[] = "unlock";

    //values for af lock mode
    const char IntelCameraParameters::AF_LOCK_LOCK[] = "lock";
    const char IntelCameraParameters::AF_LOCK_UNLOCK[] = "unlock";

    //values for awb lock mode
    const char IntelCameraParameters::AWB_LOCK_LOCK[] = "lock";
    const char IntelCameraParameters::AWB_LOCK_UNLOCK[] = "unlock";

    //values for back light correction mode
    const char IntelCameraParameters::BACK_LIGHT_CORRECTION_ON[] = "on";
    const char IntelCameraParameters::BACK_LIGHT_COORECTION_OFF[] = "off";

    //values for red eye mode
    const char IntelCameraParameters::RED_EYE_REMOVAL_ON[] = "on";
    const char IntelCameraParameters::RED_EYE_REMOVAL_OFF[] = "off";

    //Panorama live preview size
    const char IntelCameraParameters::KEY_PANORAMA_LIVE_PREVIEW_SIZE[] = "panorama-live-preview-size";
    const char IntelCameraParameters::KEY_SUPPORTED_PANORAMA_LIVE_PREVIEW_SIZES[] = "panorama-live-preview-sizes";

    //values for awb mapping
    const char IntelCameraParameters::AWB_MAPPING_AUTO[] = "auto";
    const char IntelCameraParameters::AWB_MAPPING_INDOOR[] = "indoor";
    const char IntelCameraParameters::AWB_MAPPING_OUTDOOR[] = "outdoor";

    const char IntelCameraParameters::EFFECT_VIVID[] = "vivid";

    const char IntelCameraParameters::KEY_FILE_INJECT_FILENAME[] = "file-inject-name";
    const char IntelCameraParameters::KEY_FILE_INJECT_WIDTH[] = "file-inject-width";
    const char IntelCameraParameters::KEY_FILE_INJECT_HEIGHT[] = "file-inject-height";
    const char IntelCameraParameters::KEY_FILE_INJECT_BAYER_ORDER[] = "file-inject-bayer-order";
    const char IntelCameraParameters::KEY_FILE_INJECT_FORMAT[] = "file-inject-format";

    // HW Overlay support
    const char IntelCameraParameters::KEY_HW_OVERLAY_RENDERING_SUPPORTED[] = "overlay-render-values";
    const char IntelCameraParameters::KEY_HW_OVERLAY_RENDERING[] = "overlay-render";

    // high speed recording, slow motion playback
    const char IntelCameraParameters::KEY_SLOW_MOTION_RATE[] = "slow-motion-rate";
    const char IntelCameraParameters::KEY_SUPPORTED_SLOW_MOTION_RATE[] = "slow-motion-rate-values";
    const char IntelCameraParameters::SLOW_MOTION_RATE_1X[] = "1x";
    const char IntelCameraParameters::SLOW_MOTION_RATE_2X[] = "2x";
    const char IntelCameraParameters::SLOW_MOTION_RATE_3X[] = "3x";
    const char IntelCameraParameters::SLOW_MOTION_RATE_4X[] = "4x";

    const char IntelCameraParameters::KEY_RECORDING_FRAME_RATE[] = "recording-fps";
    const char IntelCameraParameters::KEY_SUPPORTED_RECORDING_FRAME_RATES[] = "recording-fps-values";
    const char IntelCameraParameters::KEY_SUPPORTED_HIGH_SPEED_RESOLUTION_FPS[] = "high-speed-resolution-fps-values";

    // EXIF data
    const char IntelCameraParameters::KEY_EXIF_MAKER[] = "exif-maker-name";
    const char IntelCameraParameters::KEY_EXIF_MODEL[] = "exif-model-name";
    const char IntelCameraParameters::KEY_EXIF_SOFTWARE[] = "exif-software-name";

    // V4L2 specific scene modes
    const char IntelCameraParameters::SCENE_MODE_BEACH_SNOW[] = "beach";
    const char IntelCameraParameters::SCENE_MODE_DAWN_DUSK[] = "dusk-dawn";
    const char IntelCameraParameters::SCENE_MODE_FALL_COLORS[] = "fall-color";
    const char IntelCameraParameters::SCENE_MODE_BACKLIGHT[] = "back-light";

    // contrast
    const char IntelCameraParameters::KEY_CONTRAST_MODE[] = "contrast-mode";
    const char IntelCameraParameters::KEY_SUPPORTED_CONTRAST_MODES[] = "contrast-mode-values";
    const char IntelCameraParameters::CONTRAST_MODE_NORMAL[] = "normal";
    const char IntelCameraParameters::CONTRAST_MODE_SOFT[] = "soft";
    const char IntelCameraParameters::CONTRAST_MODE_HARD[] = "hard";

    // saturation
    const char IntelCameraParameters::KEY_SATURATION_MODE[] = "saturation-mode";
    const char IntelCameraParameters::KEY_SUPPORTED_SATURATION_MODES[] = "saturation-mode-values";
    const char IntelCameraParameters::SATURATION_MODE_NORMAL[] = "normal";
    const char IntelCameraParameters::SATURATION_MODE_LOW[] = "low";
    const char IntelCameraParameters::SATURATION_MODE_HIGH[] = "high";

    // sharpness
    const char IntelCameraParameters::KEY_SHARPNESS_MODE[] = "sharpness-mode";
    const char IntelCameraParameters::KEY_SUPPORTED_SHARPNESS_MODES[] = "sharpness-mode-values";
    const char IntelCameraParameters::SHARPNESS_MODE_NORMAL[] = "normal";
    const char IntelCameraParameters::SHARPNESS_MODE_SOFT[] = "soft";
    const char IntelCameraParameters::SHARPNESS_MODE_HARD[] = "hard";

    // save mirrored
    const char IntelCameraParameters::KEY_SAVE_MIRRORED[] = "save-mirrored";
    const char IntelCameraParameters::KEY_SUPPORTED_SAVE_MIRRORED[] = "save-mirrored-values";

    // GPS extension
    const char IntelCameraParameters::KEY_GPS_IMG_DIRECTION[] = "gps-img-direction";
    const char IntelCameraParameters::KEY_GPS_IMG_DIRECTION_REF[] = "gps-img-direction-ref";
    const char IntelCameraParameters::KEY_SUPPORTED_GPS_IMG_DIRECTION_REF[] = "gps-img-direction-ref-values";
    // possible value for the KEY_GPS_IMG_DIRECTION_REF
    const char IntelCameraParameters::GPS_IMG_DIRECTION_REF_TRUE[] = "true-direction";
    const char IntelCameraParameters::GPS_IMG_DIRECTION_REF_MAGNETIC[] = "magnetic-direction";

    const char IntelCameraParameters::KEY_INTELLIGENT_MODE[] = "intelligent-mode";
    const char IntelCameraParameters::KEY_SUPPORTED_INTELLIGENT_MODE[] = "intelligent-mode-values";

    // color-bar preview test pattern
    const char IntelCameraParameters::KEY_COLORBAR[] = "color-bar-preview";
    const char IntelCameraParameters::KEY_SUPPORTED_COLORBAR[] = "color-bar-preview-values";

    // Others
    const char IntelCameraParameters::REC_BUFFER_SHARING_SESSION_ID[] = "buffer-sharing-session-id";

    void IntelCameraParameters::getPanoramaLivePreviewSize(int &width, int &height, const CameraParameters &params)
    {
        width = height = -1;
        // Get the current string, if it doesn't exist, leave the -1x-1
        const char *p = params.get(KEY_PANORAMA_LIVE_PREVIEW_SIZE);
        if (p == 0)
            return;

        parseResolution(p, width, height);
    }

    void IntelCameraParameters::parseResolutionList(const char *sizeStr, Vector<Size> &sizes)
    {
        LOG1("@%s", __FUNCTION__);
        if (sizeStr == 0) {
            return;
        }

        char *sizeStartPtr = (char *)sizeStr;

        while (true) {
            int width = -1, height = -1;
            status_t status = parseResolution(sizeStartPtr, width, height,
                                     &sizeStartPtr);
            if (status != OK || (*sizeStartPtr != ',' && *sizeStartPtr != '\0')) {
                ALOGE("Resolution string \"%s\" contains invalid character.", sizeStr);
                return;
            }
            sizes.push(Size(width, height));
            LOG1("@%s added resolution %dx%d", __FUNCTION__, width, height);

            if (*sizeStartPtr == '\0') {
                return;
            }
            sizeStartPtr++;
        }
    }

    /**
     * parses a list from a string using strtok_r, inserts into Vector.
     * Note that this modifies the first param "char *str".
     */
    void IntelCameraParameters::parseList(char *str, const char *delim, Vector<String8> &elements)
    {
        LOG1("@%s", __FUNCTION__);
        if (str == 0) {
            return;
        }

        char *saveptr = NULL;
        char *token = strtok_r(str, delim, &saveptr);

        while (token != NULL) {
            elements.push_back(String8(token));
            token = strtok_r(NULL, delim, &saveptr);
        }
    }

    status_t IntelCameraParameters::parseResolution(const char *p, int &width, int &height,
            char **endptr)
    {
        LOG1("@%s", __FUNCTION__);
        char *xptr = NULL;
        width = (int) strtol(p, &xptr, 10);
        if (xptr == NULL || *xptr != 'x') // strtol stores location of x into xptr
            return BAD_VALUE;

        height = (int) strtol(xptr + 1, &xptr, 10);

        if (endptr) {
            *endptr = xptr;
        }

        return OK;
    }

    const char* IntelCameraParameters::getSupportedPanoramaLivePreviewSizes(const CameraParameters &params)
    {
        // live preview images are implemented with the thumbnails
        return params.get(KEY_SUPPORTED_PANORAMA_LIVE_PREVIEW_SIZES);
    }

}; // ns android
