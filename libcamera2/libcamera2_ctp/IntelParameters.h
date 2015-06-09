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

/**
 *\file IntelParameters.h
 *
 * Define and document Intel specific HAL parameters.
 *
 * The standard Android Camera HAL parameters are defined in
 * frameworks/base/include/camera/CameraParameters.h
 */

#ifndef INTELPARAMETERS_H_
#define INTELPARAMETERS_H_

#include <camera/CameraParameters.h>

namespace android {

  class IntelCameraParameters {
  public:

    static const char KEY_XNR[];
    static const char KEY_SUPPORTED_XNR[];
    static const char KEY_ANR[];
    static const char KEY_SUPPORTED_ANR[];
    static const char KEY_NOISE_REDUCTION_AND_EDGE_ENHANCEMENT[];
    static const char KEY_SUPPORTED_NOISE_REDUCTION_AND_EDGE_ENHANCEMENT[];
    static const char KEY_MULTI_ACCESS_COLOR_CORRECTION[];
    static const char KEY_SUPPORTED_MULTI_ACCESS_COLOR_CORRECTIONS[];
    static const char KEY_AE_MODE[];
    static const char KEY_SUPPORTED_AE_MODES[];
    static const char KEY_AE_METERING_MODE[];
    static const char KEY_SUPPORTED_AE_METERING_MODES[];
    static const char KEY_SHUTTER[];
    static const char KEY_SUPPORTED_SHUTTER[];
    static const char KEY_APERTURE[];
    static const char KEY_SUPPORTED_APERTURE[];
    static const char KEY_ISO[];
    static const char KEY_SUPPORTED_ISO[];
    // af lock key to communicate with camera driver
    static const char KEY_AF_LOCK_MODE[];
    static const char KEY_SUPPORTED_AF_LOCK_MODES[];
    // back light correction key to communicate with camera driver
    static const char KEY_BACK_LIGHTING_CORRECTION_MODE[];
    static const char KEY_SUPPORTED_BACK_LIGHTING_CORRECTION_MODES[];
    //af metering mode key to communicate with camera driver
    static const char KEY_AF_METERING_MODE[];
    static const char KEY_SUPPORTED_AF_METERING_MODES[];
    // The focus window described by coordinates of two point.
    static const char KEY_FOCUS_WINDOW[];
    // When using auto white balance, there are some mode such as "indoor" or
    // "outdoor" to choose
    static const char KEY_AWB_MAPPING_MODE[];
    static const char KEY_SUPPORTED_AWB_MAPPING_MODES[];
    // manual color temperature in K
    static const char KEY_COLOR_TEMPERATURE[];
    // burst capture
    static const char KEY_SUPPORTED_BURST_LENGTH[];
    static const char KEY_BURST_LENGTH[];
    static const char KEY_SUPPORTED_BURST_FPS[]; // TODO: old API, it will be deleted in the future.
    static const char KEY_BURST_FPS[]; // TODO: old API, it will be deleted in the future.
    static const char KEY_SUPPORTED_BURST_SPEED[];
    static const char KEY_BURST_SPEED[];
    //values for burst speed
    static const char BURST_SPEED_FAST[];
    static const char BURST_SPEED_MEDIUM[];
    static const char BURST_SPEED_LOW[];

    // preview update mode
    static const char KEY_SUPPORTED_PREVIEW_UPDATE_MODE[];
    static const char KEY_PREVIEW_UPDATE_MODE[];
    static const char PREVIEW_UPDATE_MODE_STANDARD[];
    static const char PREVIEW_UPDATE_MODE_DURING_CAPTURE[];
    static const char PREVIEW_UPDATE_MODE_CONTINUOUS[];
    static const char PREVIEW_UPDATE_MODE_WINDOWLESS[];
    // raw data format for snapshot
    static const char KEY_RAW_DATA_FORMAT[];
    static const char KEY_SUPPORTED_RAW_DATA_FORMATS[];
    // capture bracketing
    static const char KEY_CAPTURE_BRACKET[];
    static const char KEY_SUPPORTED_CAPTURE_BRACKET[];
    // rotation mode
    static const char KEY_ROTATION_MODE[];
    static const char KEY_SUPPORTED_ROTATION_MODES[];
    // Value for KEY_FRONT_SENSOR_FLIP
    static const char KEY_FRONT_SENSOR_FLIP[];
    static const char EFFECT_VIVID[];
    // Values for auto exposure mode settings
    static const char AE_MODE_AUTO[];
    static const char AE_MODE_MANUAL[];
    static const char AE_MODE_SHUTTER_PRIORITY[];
    static const char AE_MODE_APERTURE_PRIORITY[];
    // Flash mode
    static const char FLASH_MODE_DAY_SYNC[];
    static const char FLASH_MODE_SLOW_SYNC[];
    // control focus distance directly.
    static const char FOCUS_MODE_MANUAL[];
    // used for touch focus
    static const char FOCUS_MODE_TOUCH[];
    // values for ae metering mode.
    static const char AE_METERING_MODE_AUTO[];
    static const char AE_METERING_MODE_SPOT[];
    static const char AE_METERING_MODE_CENTER[];
    static const char AE_METERING_MODE_CUSTOMIZED[];
    // values for ae lock mode
    static const char AE_LOCK_LOCK[];
    static const char AE_LOCK_UNLOCK[];
    // values for af lock mode
    static const char AF_LOCK_LOCK[];
    static const char AF_LOCK_UNLOCK[];
    // values for awb lock mode
    static const char AWB_LOCK_LOCK[];
    static const char AWB_LOCK_UNLOCK[];
    // values for af metering mode
    static const char AF_METERING_MODE_AUTO[];
    static const char AF_METERING_MODE_SPOT[];
    // values for red eye mode
    static const char RED_EYE_REMOVAL_ON[];
    static const char RED_EYE_REMOVAL_OFF[];
    // values for awb mapping mode
    static const char AWB_MAPPING_AUTO[];
    static const char AWB_MAPPING_INDOOR[];
    static const char AWB_MAPPING_OUTDOOR[];
    //values for back light correction mode
    static const char BACK_LIGHT_CORRECTION_ON[];
    static const char BACK_LIGHT_COORECTION_OFF[];
    // HDR imaging
    static const char KEY_HDR_IMAGING[];
    static const char KEY_SUPPORTED_HDR_IMAGING[];
    // HDR save original
    static const char KEY_HDR_SAVE_ORIGINAL[];
    static const char KEY_SUPPORTED_HDR_SAVE_ORIGINAL[];
    // Smart Shutter
    static const char KEY_SMILE_SHUTTER[];
    static const char KEY_SUPPORTED_SMILE_SHUTTER[];
    static const char KEY_SMILE_SHUTTER_THRESHOLD[];
    static const char KEY_BLINK_SHUTTER[];
    static const char KEY_SUPPORTED_BLINK_SHUTTER[];
    static const char KEY_BLINK_SHUTTER_THRESHOLD[];

    // Face features
    static const char KEY_SUPPORTED_FACE_DETECTION[];
    static const char KEY_SUPPORTED_FACE_RECOGNITION[];

    // Scene detection
    static const char KEY_SCENE_DETECTION[];
    static const char KEY_SUPPORTED_SCENE_DETECTION[];

    // Panorama
    static const char KEY_PANORAMA[];
    static const char KEY_SUPPORTED_PANORAMA[];
    static const char KEY_PANORAMA_LIVE_PREVIEW_SIZE[];
    static const char KEY_SUPPORTED_PANORAMA_LIVE_PREVIEW_SIZES[];
    static const char KEY_PANORAMA_MAX_SNAPSHOT_COUNT[];

    //MACC effect
    static const char EFFECT_STILL_SKY_BLUE[];
    static const char EFFECT_STILL_GRASS_GREEN[];
    static const char EFFECT_STILL_SKIN_WHITEN_LOW[];
    static const char EFFECT_STILL_SKIN_WHITEN_MEDIUM[];
    static const char EFFECT_STILL_SKIN_WHITEN_HIGH[];

    // File injection
    static const char KEY_FILE_INJECT_FILENAME[];
    static const char KEY_FILE_INJECT_WIDTH[];
    static const char KEY_FILE_INJECT_HEIGHT[];
    static const char KEY_FILE_INJECT_BAYER_ORDER[];
    static const char KEY_FILE_INJECT_FORMAT[];

    // HW Overlay support
    static const char KEY_HW_OVERLAY_RENDERING_SUPPORTED[];
    static const char KEY_HW_OVERLAY_RENDERING[];

    //high speed recording, slow motion playback
    static const char KEY_SLOW_MOTION_RATE[];
    static const char KEY_SUPPORTED_SLOW_MOTION_RATE[];
    static const char SLOW_MOTION_RATE_1X[];
    static const char SLOW_MOTION_RATE_2X[];
    static const char SLOW_MOTION_RATE_3X[];
    static const char SLOW_MOTION_RATE_4X[];
    static const char KEY_RECORDING_FRAME_RATE[];
    static const char KEY_SUPPORTED_RECORDING_FRAME_RATES[];
    static const char KEY_SUPPORTED_HIGH_SPEED_RESOLUTION_FPS[];

    // dual video
    static const char KEY_DUAL_VIDEO_SUPPORTED[];

    // Time nudge
    static const char KEY_BURST_START_INDEX[];
    static const char KEY_SUPPORTED_BURST_START_INDEX[];

    // EXIF data
    static const char KEY_EXIF_MAKER[];
    static const char KEY_EXIF_MODEL[];
    static const char KEY_EXIF_SOFTWARE[];

    // V4L2 specific scene modes
    static const char SCENE_MODE_BEACH_SNOW[];
    static const char SCENE_MODE_DAWN_DUSK[];
    static const char SCENE_MODE_FALL_COLORS[];
    static const char SCENE_MODE_BACKLIGHT[];

    // contrast
    static const char KEY_CONTRAST_MODE[];
    static const char KEY_SUPPORTED_CONTRAST_MODES[];
    static const char CONTRAST_MODE_NORMAL[];
    static const char CONTRAST_MODE_SOFT[];
    static const char CONTRAST_MODE_HARD[];

    // saturation
    static const char KEY_SATURATION_MODE[];
    static const char KEY_SUPPORTED_SATURATION_MODES[];
    static const char SATURATION_MODE_NORMAL[];
    static const char SATURATION_MODE_LOW[];
    static const char SATURATION_MODE_HIGH[];

    // sharpness
    static const char KEY_SHARPNESS_MODE[];
    static const char KEY_SUPPORTED_SHARPNESS_MODES[];
    static const char SHARPNESS_MODE_NORMAL[];
    static const char SHARPNESS_MODE_SOFT[];
    static const char SHARPNESS_MODE_HARD[];

    // Ultra low light
    static const char KEY_ULL[];
    static const char KEY_SUPPORTED_ULL[];

    static const char KEY_SAVE_MIRRORED[];
    static const char KEY_SUPPORTED_SAVE_MIRRORED[];

    // GPS extension
    static const char KEY_GPS_IMG_DIRECTION[];
    static const char KEY_GPS_IMG_DIRECTION_REF[];
    static const char KEY_SUPPORTED_GPS_IMG_DIRECTION_REF[];
    // possible value for the KEY_GPS_IMG_DIRECTION_REF
    static const char GPS_IMG_DIRECTION_REF_TRUE[];
    static const char GPS_IMG_DIRECTION_REF_MAGNETIC[];

    // Others
    static const char REC_BUFFER_SHARING_SESSION_ID[];

    static void getPanoramaLivePreviewSize(int &width, int &height, const CameraParameters &params);
    static const char* getSupportedPanoramaLivePreviewSizes(const CameraParameters &params);
    static void parseResolutionList(const char *sizeStr, Vector<Size> &sizes);
  private:
      IntelCameraParameters(void) {}

      static status_t parseResolution(const char *p, int &width, int &height, char **endptr = NULL);
  };
}; // ns android

#endif
