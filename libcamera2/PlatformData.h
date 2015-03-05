/*
 * Copyright (C) 2012 The Android Open Source Project
 * Copyright (c) 2012-2014 Intel Corporation
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
#ifndef PLATFORMDATA_H_
#define PLATFORMDATA_H_

#include "CameraConf.h"
#include <utils/String8.h>
#include <utils/Vector.h>

#define RESOLUTION_14MP_WIDTH   4352
#define RESOLUTION_14MP_HEIGHT  3264
#define RESOLUTION_13MP_WIDTH   4160
#define RESOLUTION_13MP_HEIGHT  3104
#define RESOLUTION_8MP_WIDTH    3264
#define RESOLUTION_8MP_HEIGHT   2448
#define RESOLUTION_6MP_WIDTH    3264
#define RESOLUTION_6MP_HEIGHT   1836
#define RESOLUTION_5MP_WIDTH    2560
#define RESOLUTION_5MP_HEIGHT   1920
#define RESOLUTION_3MP_WIDTH    2048
#define RESOLUTION_3MP_HEIGHT   1536
#define RESOLUTION_1_3MP_WIDTH  1280
#define RESOLUTION_1_3MP_HEIGHT 960
#define RESOLUTION_1080P_WIDTH  1920
#define RESOLUTION_1080P_HEIGHT 1080
#define RESOLUTION_2MP_WIDTH  1600
#define RESOLUTION_2MP_HEIGHT 1200
#define RESOLUTION_720P_WIDTH   1280
#define RESOLUTION_720P_HEIGHT  720
#define RESOLUTION_480P_WIDTH   768
#define RESOLUTION_480P_HEIGHT  480
#define RESOLUTION_VGA_WIDTH    640
#define RESOLUTION_VGA_HEIGHT   480
#define RESOLUTION_POSTVIEW_WIDTH    320
#define RESOLUTION_POSTVIEW_HEIGHT   240

#include <camera.h>
#include "AtomCommon.h"
#include <IntelParameters.h>

namespace android {

/**
 * \file PlatformData.h
 *
 * HAL internal interface for managing platform specific static
 * data.
 *
 * Design principles for platform data mechanism:
 *
 * 1. Make it easy as possible to add new configurable data.
 *
 * 2. Make it easy as possible to add new platforms.
 *
 * 3. Allow inheriting platforms from one another (as we'll typically
 *    have many derived platforms).
 *
 * 4. Split implementations into separate files, to avoid
 *    version conflicts with parallel work targeting different
 *    platforms.
 *
 * 5. Focus on plain flat data and avoid defining new abstractions
 *    and relations.
 *
 * 6. If any #ifdefs are needed, put them in platform files.
 *
 * 7. Keep the set of parameters to a minimum, and only add
 *    data that really varies from platform to another.
 */

// Forward declarations
class PlatformBase;

/**
 * \class PlatformData
 *
 * This class is a singleton that contains all the static information
 * from the platform. It doesn't store any state. It is a data repository
 * for static data. It provides convenience methods to initialize
 * some parameters based on the HW limitations.
 */

class PlatformData {

 private:
    static PlatformBase* mInstance;

    /**
     * Get access to the platform singleton.
     *
     * Note: this is implemented in PlatformFactory.cpp
     */
    static PlatformBase* getInstance(void);

    /**
     * Reads the required SpId hexadecimal value from /sys/spid
     *
     * \param spIdName  Value read from /sys/spid, e.g. vendor name
     * \param spIdValue The id of the desired value, e.g. vendor_id
     */
    static status_t readSpId(String8& spIdName, int& spIdValue);

    /**
     * Validates whether the given cameraId is within the allowed range
     *
     * \param cameraId  The camera ID to validate
     */
    static bool validCameraId(int cameraId, const char* functionName);

    /**
     * this function will return the camera index which is one of the getInstance()->mCameras[index]
     * for Kevlar, there are 3 sensors totally. From the software aspect, 1 is back, 2 are front.
     * so we need to distinguish the 2 front camera.
     * For the back camera, the camera id is 0. and for the front camera, the camera id is 1.
     * the parser of the camera profiles will store the back camera setting in the mCameras[0]
     * and it will store one of the front camera in the mCameras[1], another is in the mCameras[2]
     * in the camera_profiles.xml, we will use the "extendedCamera" to distinguish two front camera
     * this function will return the real index in the mCamera.
     * \param cameraId  The camera ID to validate
     */
    static int getActiveCamIdx(int cameraId);

 public:

    static AiqConf AiqConfig[MAX_CAMERAS];
    static HalConf HalConfig[MAX_CAMERAS];

    enum SensorFlip {
        SENSOR_FLIP_NA     = -1,   // Support Not-Available
        SENSOR_FLIP_OFF    = 0x00, // Both flip ctrls set to 0
        SENSOR_FLIP_H      = 0x01, // V4L2_CID_HFLIP 1
        SENSOR_FLIP_V      = 0x02, // V4L2_CID_VFLIP 1
    };

    /**
     * set the intelligent mode
     * the intelligent mode is running windowless mode with raw8 data out from ISP
     *
     * if the val is true, the intelligent mode is using
     *
     */
    static void setIntelligentMode(int cameraId, bool val);

    /**
     * get the intelligent mode
     *
     * if the intelligent mode is running, this function will return true.
     *
     */
    static bool getIntelligentMode(int cameraId);

    /**
     * to check if the 3A is needed to be disabled
     *
     * if the camera should disable 3A, this function will return true.
     * /param cameraId identifier passed to android.hardware.Camera.open()
     */
    static bool isDisable3A(int cameraId);

    /**
     * Number of cameras
     *
     * Returns number of cameras that may be opened with
     * android.hardware.Camera.open().
     *
     * \return a non-negative integer
     */
    static int numberOfCameras(void);

    /**
     * get the sensor Name
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return string which is the sensor Name
     */
    static const char* sensorName(int cameraId);

    /**
     * Sensor type of camera id
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return SensorType of camera.     */
    static SensorType sensorType(int cameraId);

    /**
     * Facing of camera id
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return int value as defined in Camera.CameraInfo
     */
    static int cameraFacing(int cameraId);

    /**
     * Whether the back camera has flash?
     *
     * \return true if flash is available
     */
    static bool supportsFlash(int cameraId);

    /**
     * Whether image data injection from file is supported?
     *
     * \return true if supported
     */
    static bool supportsFileInject(void);

    /**
     * Whether platform can support continuous capture mode in terms of
     * SoC and ISP performance.
     *
     * \return true if supported
     */
    static bool supportsContinuousCapture(int cameraId);

    /**
     * Whether platform can support burst capture in offlie mode
     *
     * \return true if supported
     */
    static bool supportsOfflineBurst(void);

    /**
     * Whether platform support continuous Jpeg mode capture with
     * external ISP
     *
     * \return true if supported
     */
    static bool supportsContinuousJpegCapture(int cameraId);

    /**
     * Whether platform can support bracket capture in offlie mode
     *
     * \return true if supported
     */
    static bool supportsOfflineBracket(void);

    /**
     * Whether platform can support HDR capture in offlie mode
     *
     * \return true if supported
     */

    static bool supportsOfflineHdr(void);
    /**
     * What's the maximum supported size of the RAW ringbuffer
     * for continuous capture maintained by the ISP.
     *
     * This depends both on kernel and CSS firmware, but also total
     * available system memory that should be used for imaging use-cases.
     *
     * \return int number 0...N, if supportsContinuousCapture() is
     *         false, this function will always return 0
     */
    static int maxContinuousRawRingBufferSize(int cameraId);

    /**
     * Returns the average lag between user pressing shutter UI button or
     * key, to camera HAL receiving take_picture method call.
     *
     * This value is used to fine-tune frame selection for Zero
     * Shutter Lag.
     *
     * \return int lag time in milliseconds
     */
    static int shutterLagCompensationMs(void);

    /**
     * Orientation of camera id
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return int value as defined in Camera.CameraInfo
     */
    static int cameraOrientation(int cameraId);

    /**
     * Returns string described preferred preview size for video
     *
     * \return string following getParameter value notation
     */
    static const char* preferredPreviewSizeForVideo(int cameraId);

    /**
     * Returns string describing default preview size
     *
     * \return string following getParameter value notation
     */
    static const char* defaultPreviewSize(int cameraId);

    /**
     * Returns string describing default video size
     *
     * \return string following getParameter value notation
     */
    static const char* defaultVideoSize(int cameraId);

    /**
     * Whether the camera supports Digital Video Stabilization or not
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return true if it is supported
     */
    static bool supportsDVS(int cameraId);

    /**
     * Whether the sensor supports Narrow Gamma or not
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return true if it is supported
     */
    static bool supportsNarrowGamma(int cameraId);

    /**
     * Returns the supported burst capture's fps list for the platform
     * \return Supported burst capture's fps list string.
     */
    static const char* supportedBurstFPS(int CameraId);

    /**
     * Returns the supported burst capture's length list for the platform
     * \return Supported burst capture's length list string.
     */
    static const char* supportedBurstLength(int CameraId);

    /**
     * Flipping controls to set for camera id
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return int value as defined in PlatformData::SENSOR_FLIP_FLAGS
     */
    static int sensorFlipping(int cameraId);

    /**
     * Exposure compensation default value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the default value as a string.
     */
    static const char* supportedDefaultEV(int cameraId);

    /**
     * Whether EV is supported?
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return true if supported, false if not supported
     */
    static bool supportEV(int cameraId);

    /**
     * Exposure compensation max value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the max as a string.
     */
    static const char* supportedMaxEV(int cameraId);

    /**
     * Exposure compensation min value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the min as a string.
     */
    static const char* supportedMinEV(int cameraId);

    /**
     * Exposure compensation step value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the step as a string.
     */
    static const char* supportedStepEV(int cameraId);

    /**
     * Ae Metering mode supported value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value supported for ae metering as a string.
     */
    static const char* supportedAeMetering(int cameraId);

    /**
     * Default Ae Metering value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the default ae metering as a string.
     */
    static const char* defaultAeMetering(int cameraId);

    /**
     * AE lock supported value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return true as a string if AE lock is supported, false if not.
     */
    static const char* supportedAeLock(int cameraId);

    /**
     * Saturation default value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the iso default value as a string.
     */
    static const char* defaultSaturation(int cameraId);

    /**
     * Saturation default value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the default saturation as a string.
     */
    static const char* supportedSaturation(int cameraId);

    /**
     * Saturation max value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the max saturation as a string.
     */
    static const char* supportedMaxSaturation(int cameraId);

    /**
     * Saturation min value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the min saturation as a string.
     */
    static const char* supportedMinSaturation(int cameraId);

    /**
     * Saturation step value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the saturation step as a string.
     */
    static const char* supportedStepSaturation(int cameraId);

    /**
     * Saturation low mode value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the saturation low mode as a integer.
     */
    static int lowSaturation(int cameraId);

    /**
     * Saturation high mode value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the saturation high mode as a integer.
     */
    static int highSaturation(int cameraId);

    /**
     * Contrast default value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the iso default value as a string.
     */
    static const char* defaultContrast(int cameraId);

    /**
     * Contrast default value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the default contrast as a string.
     */
    static const char* supportedContrast(int cameraId);

    /**
     * Contrast max value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the max contrast as a string.
     */
    static const char* supportedMaxContrast(int cameraId);

    /**
     * Contrast min value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the min contrast as a string.
     */
    static const char* supportedMinContrast(int cameraId);

    /**
     * Contrast step value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the contrast step as a string.
     */
    static const char* supportedStepContrast(int cameraId);

    /**
     * Contrast soft mode value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the contrast soft mode as a integer.
     */
    static int softContrast(int cameraId);

    /**
     * Contrast hard mode value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the contrast hard mode as a integer.
     */
    static int hardContrast(int cameraId);

    /**
     * Sharpness default value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the iso default value as a string.
     */
    static const char* defaultSharpness(int cameraId);

    /**
     * Sharpness default value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the default sharpness as a string.
     */
    static const char* supportedSharpness(int cameraId);

    /**
     * Sharpness max value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the max sharpness as a string.
     */
    static const char* supportedMaxSharpness(int cameraId);

    /**
     * Sharpness min value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the min sharpness as a string.
     */
    static const char* supportedMinSharpness(int cameraId);

    /**
     * Sharpness step value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the sharpness step as a string.
     */
    static const char* supportedStepSharpness(int cameraId);

    /**
     * Sharpness soft mode value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the sharpness soft mode as a integer.
     */
    static int softSharpness(int cameraId);

    /**
     * Sharpness hard mode value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the sharpness hard mode as a integer.
     */
    static int hardSharpness(int cameraId);

    /**
     * Flash mode supported value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the flash supported as a string.
     */
    static const char* supportedFlashModes(int cameraId);

    /**
     * Flash mode default value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the flash default value as a string.
     */
    static const char* defaultFlashMode(int cameraId);

    /**
     * Iso mode supported value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the flash supported as a string.
     */
    static const char* supportedIso(int cameraId);

    /**
     * Iso default value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the iso default value as a string.
     */
    static const char* defaultIso(int cameraId);

    /**
     * Returns the supported scene modes for the platform
     * \return Supported scene mode string, or empty string
     *  upon error.
     */
    static const char* supportedSceneModes(int cameraId);

    /**
     * scene mode default value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the scene mode default value as a string.
     */
    static const char* defaultSceneMode(int cameraId);

    /**
     * Returns the supported effect modes for the platform
     * \return Supported effect mode string, or empty string
     *  upon error.
     */
    static const char* supportedEffectModes(int cameraId);

    /**
     * Returns the supported Intel specific effect modes for the platform
     * \return Supported effect mode string, or empty string
     *  upon error.
     */
    static const char* supportedIntelEffectModes(int cameraId);

    /**
     * effect mode default value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the effect mode default value as a string.
     */
    static const char* defaultEffectMode(int cameraId);

    /**
     * Returns the supported awb modes for the platform
     * \return Supported awb mode string, or empty string
     *  upon error.
     */
    static const char* supportedAwbModes(int cameraId);

    /**
     * awb mode default value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the awb mode default value as a string.
     */
    static const char* defaultAwbMode(int cameraId);

    /**
     * AWB lock supported value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return true as a string if AWB lock is supported, false if not.
     */
    static const char* supportedAwbLock(int cameraId);

    /**
     * preview frame rate supported value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value supported for preview frame rate.
     */
    static const char* supportedPreviewFrameRate(int cameraId);

    /**
     * preview fps range supported value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value supported for preview fps range.
     */
    static const char* supportedPreviewFPSRange(int cameraId);

    /**
     * Default preview FPS range
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the default preview fps range as a string.
     */
    static const char* defaultPreviewFPSRange(int cameraId);

    /**
     * supported preview sizes
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the supported preview sizes as a string.
     */
    static const char* supportedPreviewSizes(int cameraId);

    /**
     * supported preview update modes
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the supported preview update mode as a string.
     */
    static const char* supportedPreviewUpdateModes(int cameraId);

    /**
     * default preview update mode
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the default preview update mode as a string.
     */
    static const char* defaultPreviewUpdateMode(int cameraId);

    /**
     * Whether the slow motion playback in high speed recording mode is supported?
     * \return true if the slow motion playback is supported
     */
    static bool supportsSlowMotion(int cameraId);

    /**
     * resolution and fps range supported value in high speed video recording mode
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value supported for resolution and fps range.
     */
    static const char* supportedHighSpeedResolutionFps(int cameraId);

    /**
     * Max Dvs Resolution in high speed mode
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the max resolution.
     */
    static const char* maxHighSpeedDvsResolution(int cameraId);

    /**
     * recording frame rate supported value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return supported recording frame rates.
     */
    static const char* supportedRecordingFramerates(int cameraId);

    /**
     * Whether the full resolution SDV is supported?
     * \return true if full resolution SDV is supported
     */
    static bool isFullResSdvSupported(int cameraId);

    /**
     * supported SDV sizes in difference aspect ratio
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the supported SDV sizes as a string.
     */
    static const char* supportedSdvSizes(int cameraId);

    /**
     * Focus mode supported value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the focus supported as a string.
     */
    static const char* supportedFocusModes(int cameraId);

    /**
     * Focus mode default value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the focus default value as a string.
     */
    static const char* defaultFocusMode(int cameraId);

    /**
     * \brief Number of supported focus areas
     *
     * this is the maximum amount of AF areas the camera supports for a given sensor/3A
     * configuration
     */
    static size_t getMaxNumFocusAreas(int cameraId);

    /**
     * Whether the raw camera's default focus mode is "fixed".
     * \return true if its default focus mode is "fixed".
     */
    static bool isFixedFocusCamera(int cameraId);

    /**
     * Hdr default value on current product
     * \return the value of the hdr default value.
     */
    static const char* defaultHdr(int cameraId);

    /**
     * Hdr mode supported value
     * \return the value of the Hdr supported.
     */
    static const char* supportedHdr(int cameraId);

    /**
     * UltraLowLight default value
     * \return the value of the UltraLowLight default.
     */
    static const char* defaultUltraLowLight(int cameraId);

    /**
     * UltraLowLight mode supported value
     * \return the value of the UltraLowLight supported.
     */
    static const char* supportedUltraLowLight(int cameraId);

    /**
     * FaceRecognition default value
     * \return the value of the FaceRecognition default value.
     */
    static const char* defaultFaceRecognition(int cameraId);

    /**
     * FaceRecognition mode supported value
     * \return the value of the FaceRecognition supported.
     */
    static const char* supportedFaceRecognition(int cameraId);

    /**
     * SmileShutter default value
     * \return the value of the SmileShutter default value.
     */
    static const char* defaultSmileShutter(int cameraId);

    /**
     * SmileShutter mode supported value
     * \return the value of the SmileShutter supported.
     */
    static const char* supportedSmileShutter(int cameraId);

    /**
     * BlinkShutter default value
     * \return the value of the BlinkShutter default value.
     */
    static const char* defaultBlinkShutter(int cameraId);

    /**
     * BlinkShutter supported value
     * \return the value of the BlinkShutter supported.
     */
    static const char* supportedBlinkShutter(int cameraId);

    /**
     * Panorama default value
     * \return the value of the Panorama default value.
     */
    static const char* defaultPanorama(int cameraId);

    /**
     * Panorama mode supported value
     * \return the value of the Panorama supported.
     */
    static const char* supportedPanorama(int cameraId);

    /**
     * SceneDetection default value
     * \return the value of the SceneDetection default value.
     */
    static const char* defaultSceneDetection(int cameraId);

    /**
     * SceneDetection supported value
     * \return the value of the SceneDetection supported.
     */
    static const char* supportedSceneDetection(int cameraId);

    /**
     * supported video sizes
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the supported video sizes as a string.
     */
    static const char* supportedVideoSizes(int cameraId);

    /**
     * supported snapshot sizes
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the value of the supported snapshot sizes as a string.
     */
    static const char* supportedSnapshotSizes(int cameraId);

    /**
     * Returns Jpeg compression quality default value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return quality, The range is 1 to 100, with 100 being the best.
     *         -1 on error (invalid cameraId)
     */
    static int defaultJpegQuality(int cameraId);

    /**
     * Returns EXIF thumbnail compression quality default value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return quality, The range is 1 to 100, with 100 being the best.
     *         -1 on error (invalid cameraId)
     */
    static int defaultJpegThumbnailQuality(int cameraId);

    /**
     * Returns the cpu cache line size
     *
     * \return int value with cache line size
     */
    static int cacheLineSize();

    /**
     * Returns the name of the product
     * This is meant to be used in the EXIF metadata
     *
     * \return string with product name
     */
    static const char* productName(void);

    /**
     * Returns the name of the manufacturer
     * This is meant to be used in the EXIF metadata
     *
     * \return string with product manufacturer
     */
    static const char* manufacturerName(void);

    /**
     * Returns the ISP sub device name
     *
     * \return the ISP sub device name, it'll return NULL when it fails
    */
    static const char* getISPSubDeviceName(void);

    /**
     * Returns the max panorama snapshot count
     * \return the max panorama snapshot count
     */
    static int getMaxPanoramaSnapshotCount();

    /**
     * Whether preview is rendered via HW overlay or GFx plane
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return true if rendered via HW overlay
     * \return false if rendered via Gfx
     */
    static bool renderPreviewViaOverlay(int cameraId);

    /**
     * Returns whether the snapshot resolution has ZSL support
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \param width of resolution
     * \param height of resolution
     * \return true if resolution support ZSL, false if not
     */
    static bool snapshotResolutionSupportedByZSL(int cameraId, int width, int height);

    /**
      * Returns whether the snapshot resolution is supported
      * to be used with Continuous Viewfinder.
      *
      * When false, viewfinder frame rate may drop when taking
      * pictures at this resolution.
      *
      * \param cameraId identifier passed to android.hardware.Camera.open()
      * \param width of resolution
      * \param height of resolution
      * \return true if resolution supports Continuous Viewfinder, false if not
      */
    static bool snapshotResolutionSupportedByCVF(int cameraId, int width, int height);

    /**
     * Returns the relative rotation between the camera normal scan order
     * and the display attached to the HW overlay.
     * A rotation of this magnitud is required to render correctly the preview
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return degrees required to rotate: 0,90,180,270
     */
    static int overlayRotation(int cameraId);

    /**
     * Returns the max zoom factor
     *
     * \param cameraId identifier passed from android.hardware.Camera.open()
     * \return the max zoom factor
     */
    static int getMaxZoomFactor(int cameraId);

    /**
     * Whether snapshot in video is supported?
     *
     * \return true if supported
     */
    static bool supportVideoSnapshot(void);

    /**
     * Returns the Recording buffers number
     *
     * \return the recording buffers number
    */
    static int getRecordingBufNum(void);

    /**
     * Returns the Preview buffers number
     *
     * \return the preview buffers number
    */
    static int getPreviewBufNum(void);

    /**
     * Returns the max number of YUV buffers for burst capture
     *
     * \return the max number of YUV buffers for burst capture
    */
    static int getMaxNumYUVBufferForBurst(int cameraId);

    /**
     * Returns the max number of YUV buffers for bracketing
     *
     * \return the max number of YUV buffers for bracketing
    */
    static int getMaxNumYUVBufferForBracket(int cameraId);

    /**
     * Returns Graphics HAL pixel format
     * \return the pixel format
     */
    static int getGFXHALPixelFormat(void);

    /**
     * Whether dual video is supported?
     *
     * \return true if supported
     */
    static bool supportDualVideo(void);

    /**
     * Whether preview limitation is supported
     *
     * \return true if supported
     */
    static bool supportPreviewLimitation(void);

    /**
     * Returns the preview format with V4l2 definition
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the preview format, V4L2_PIX_FMT_NV12 or V4L2_PIX_FMT_YVU420
    */
    static int getPreviewPixelFormat(int cameraId);

    /**
     * Returns the board name
     *
     * \return the board name, it'll return NULL when it fails
    */
    static const char* getBoardName(void);

    /**
     * Creates a string that defines the specific
     *
     * \return the board name, it'll return NULL when it fails
    */
    static status_t createVendorPlatformProductName(String8& name);

    /**
     * Returns frame latency for analog gain applying
     *
     * \param cameraId identifier passed from android.hardware.Camera.open()
     * \return the frame latency for analog gain applying
    */
    static int getSensorGainLag(int cameraId);

    /**
     * Returns frame latency for exposure applying
     *
     * \param cameraId identifier passed from android.hardware.Camera.open()
     * \return the frame latency for exposure applying
    */
    static int getSensorExposureLag(int cameraId);

    /**
     * Returns whether to use frame synchronization for exposure applying
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return true if synchronisation needed
    */
    static bool synchronizeExposure(int cameraId);

    /**
     * Used Ultra Low Light implementation
     *
     * \return true if Intel ULL implementation is used
     */
    static bool useIntelULL(void);

    /**
     * Returns vertical FOV
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \param snapshot width and height.
     * \return float angle in degrees
     */
    static float verticalFOV(int cameraId, int width, int height);

    /**
     * Retrns horizontal FOV
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \param snapshot width and height.
     * \return float angle in degrees
     */
    static float horizontalFOV(int cameraId, int width, int height);

    /**
     * Retrns focal length of depth mode
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return int focal length
     */
    static int defaultDepthFocalLength(int cameraId);

    /**
     * Whether Postview output is supported?
     *
     * \return true if supported
     */
    static bool supportsPostviewOutput(int cameraId);

    /**
     * Whether ISP supported continuous capture mode setting ?
     *
     * \return true if supported
     */
    static bool ispSupportContinuousCaptureMode(int cameraId);

    /**
     * Whether color-bar preview is supported?
     *
     * \return true if supported
     */
    static bool supportsColorBarPreview(int cameraId);

    /**
     * This value is configured by product. When continuous ISP timeout count
     * reach the max ISP timeout count, Camera HAL will try to recover preview.
     *
     * \return max ISP timeout count
     */
    static int getMaxISPTimeoutCount(void);

    /**
     * Retrns max number of preview buffer queue size for depth sensor
     *
     * \param cameraId identifier passed from android.hardware.Camera.open()
     * \return int max number of preview buffer queue
     */
    static int getMaxDepthPreviewBufferQueueSize(int cameraId);

    /**
     * supported DVS sizes
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * return the value of the supported dvs sizes as a string.
     */
    static const char* supportedDvsSizes(int cameraId);

    /**
     * supported sensor metadata
     *
     * \param cameraId identifier passed from android.hardware.Camera.open()
     * \return true if supported sensor metadata. Return false, if don't supported.
     */
    static bool supportedSensorMetadata(int cameraId);

    /**
     * Whether the graphic is GEN.
     * \return true if it's GEN. false, if it's not GEN.
     */
    static bool isGraphicGen(void);

    /**
     * \brief Divider to restrict the frequency of face callbacks
     *
     * The face callbacks need to be restricted to relieve the application
     * from calculation overhead, especially when several dozen of faces are
     * provided (up to MAX_FACES_DETECTABLE).
     *
     * \return a positive integer divider (N) for sending face callbacks.
     * Every Nth callback will be sent.
     */
    static int faceCallbackDivider();

    /**
     * \brief To check if HAL Video Stabilization-specific code paths are to be
     * enabled
     *
     * HAL VS allows customers to integrate their own VS algorithm. It requires
     * special preview-to-video flow in the HAL.
     */
    static bool useHALVS(int cameraId);

    /**
     * get the number of CPU cores
     * \return the number of CPU cores
     */
    static unsigned int getNumOfCPUCores();

    /**
     * \brief Get the number of warm-up frames for the still image capture:
     *
     * Some sensors (or ISP) may need warm-up skips in image capture. For example
     * some SoC sensors need initial capture frame skips in order to have their 3A (or 2A)
     * algorithms to converge before the actual good capture frame
     *
     * \return The number of warm-up frames needed for the platform
     */
    static int getNumOfCaptureWarmUpFrames(int cameraId);

    /**
     * check if we use multi streams for SoC sensor
     * \return true or false
     */
    static bool useMultiStreamsForSoC(int cameraId);

    /**
     * set true to use the extended camera
     * set false to not use the extended camera
     */
    static void useExtendedCamera(bool val);

    static bool isExtendedCameras(void);
    /**
     * it will check if the current cameraId camera is the extended camera
     * \return true or false
     */
    static bool isExtendedCamera(int cameraId);

    /**
     * Whether extended camera is supported?
     *
     * \return true if supported
     */
    static bool supportExtendedCamera(void);

    /**
     * Intelligent mode supported value
     *
     * \param cameraId identifier passed to android.hardware.Camera.open()
     * \return the supported list for intelligent mode
     */
    static const char* supportedIntelligentMode(int cameraId);

};

/**
 * \class PlatformBase
 *
 * Base class for defining static platform features and
 * related configuration data that is needed by rest of the HAL.
 *
 * Each platform will extend this class.
 */
class PlatformBase {

    friend class PlatformData;

public:
    PlatformBase()  //default
        :mUseExtendedCamera(false)
        ,mHasExtendedCamera(false)
        ,mExtendedCameraId(-1)
        ,mExtendedCameraIndex(-1)
        ,mFileInject(false)
        ,mSupportVideoSnapshot(true)
        ,mSupportsOfflineBurst(false)
        ,mSupportsOfflineBracket(false)
        ,mSupportsOfflineHdr(false)
        ,mMaxContinuousRawRingBuffer(0)
        ,mShutterLagCompensationMs(40)
        ,mPanoramaMaxSnapshotCount(10)
        ,mMaxZoomFactor(64)
        ,mNumRecordingBuffers(9)
        ,mNumPreviewBuffers(6)
        ,mSupportDualVideo(false)
        ,mSupportPreviewLimitation(true)
        ,mSensorGainLag(2)
        ,mSensorExposureLag(2)
        ,mUseIntelULL(false)
        ,mFaceCallbackDivider(1)
        ,mCacheLineSize(64)
        ,mMaxISPTimeoutCount(60)
    {}

protected:

    /**
     * Camera feature info that is specific to camera id
     */

    class CameraInfo {
    public:
        CameraInfo() {
            sensorName = "";
            extendedCamera = false;
            sensorType = SENSOR_TYPE_RAW;
            facing = CAMERA_FACING_BACK;
            orientation = 90;
            flipping = PlatformData::SENSOR_FLIP_NA;
            dvs = true;
            narrowGamma = false;
            supportedSnapshotSizes = "320x240,640x480,1024x768,1280x720,1920x1080,2048x1536,2560x1920,3264x1836,3264x2448";
            defaultJpegQuality = 80;
            defaultJpegThumbnailQuality = 50;
            mPreviewViaOverlay = false;
            overlayRelativeRotation = 90;
            continuousCapture = false;
            continuousJpegCapture = false;
            //burst
            supportedBurstFPS = "1,3,5,7,15";
            supportedBurstLength = "1,3,5,10";
            defaultBurstLength = "10";
            //EV
            maxEV = "2";
            minEV = "-2";
            stepEV = "0.33333333";
            defaultEV = "0";
            //Saturation
            maxSaturation = "";
            minSaturation = "";
            stepSaturation = "";
            defaultSaturation = "";
            supportedSaturation = "";
            lowSaturation = 0;
            highSaturation = 0;
            //Contrast
            maxContrast = "";
            minContrast = "";
            stepContrast = "";
            defaultContrast = "";
            supportedContrast = "";
            softContrast = 0;
            hardContrast = 0;
            //Sharpness
            maxSharpness = "";
            minSharpness = "";
            stepSharpness = "";
            defaultSharpness = "";
            supportedSharpness = "";
            softSharpness = 0;
            hardSharpness = 0;
            //FlashMode
            supportedFlashModes.appendFormat("%s,%s,%s,%s"
                ,CameraParameters::FLASH_MODE_AUTO
                ,CameraParameters::FLASH_MODE_OFF
                ,CameraParameters::FLASH_MODE_ON
                ,CameraParameters::FLASH_MODE_TORCH);
            defaultFlashMode.appendFormat("%s", CameraParameters::FLASH_MODE_OFF);
            //Iso
            supportedIso = "iso-auto,iso-100,iso-200,iso-400,iso-800";
            defaultIso = "iso-auto";
            //sceneMode
            supportedSceneModes.appendFormat("%s,%s,%s,%s,%s,%s,%s"
                ,CameraParameters::SCENE_MODE_AUTO
                ,CameraParameters::SCENE_MODE_PORTRAIT
                ,CameraParameters::SCENE_MODE_SPORTS
                ,CameraParameters::SCENE_MODE_LANDSCAPE
                ,CameraParameters::SCENE_MODE_NIGHT
                ,CameraParameters::SCENE_MODE_FIREWORKS
                ,CameraParameters::SCENE_MODE_BARCODE);

            defaultSceneMode.appendFormat("%s", CameraParameters::SCENE_MODE_AUTO);
            //effectMode
            supportedEffectModes.appendFormat("%s,%s,%s,%s"
                ,CameraParameters::EFFECT_NONE
                ,CameraParameters::EFFECT_MONO
                ,CameraParameters::EFFECT_NEGATIVE
                ,CameraParameters::EFFECT_SEPIA);

            supportedIntelEffectModes.appendFormat("%s,%s,%s,%s,%s,%s,%s,%s,%s,%s"
                ,CameraParameters::EFFECT_NONE
                ,CameraParameters::EFFECT_MONO
                ,CameraParameters::EFFECT_NEGATIVE
                ,CameraParameters::EFFECT_SEPIA
                ,IntelCameraParameters::EFFECT_VIVID
                ,IntelCameraParameters::EFFECT_STILL_SKY_BLUE
                ,IntelCameraParameters::EFFECT_STILL_GRASS_GREEN
                ,IntelCameraParameters::EFFECT_STILL_SKIN_WHITEN_LOW
                ,IntelCameraParameters::EFFECT_STILL_SKIN_WHITEN_MEDIUM
                ,IntelCameraParameters::EFFECT_STILL_SKIN_WHITEN_HIGH);
            defaultEffectMode.appendFormat("%s", CameraParameters::EFFECT_NONE);
            //awbmode
            supportedAwbModes.appendFormat("%s,%s,%s,%s,%s"
                ,CameraParameters::WHITE_BALANCE_AUTO
                ,CameraParameters::WHITE_BALANCE_INCANDESCENT
                ,CameraParameters::WHITE_BALANCE_FLUORESCENT
                ,CameraParameters::WHITE_BALANCE_DAYLIGHT
                ,CameraParameters::WHITE_BALANCE_CLOUDY_DAYLIGHT);
            defaultAwbMode.appendFormat("%s", CameraParameters::WHITE_BALANCE_AUTO);
            supportedAwbLock = "true";
            //ae metering
            supportedAeMetering = "auto,center,spot";
            defaultAeMetering = "auto";
            supportedAeLock = "true";
            //preview
            supportedPreviewFrameRate = "30,15,10";
            supportedPreviewFPSRange = "(10500,30304),(11000,30304),(11500,30304)";
            defaultPreviewFPSRange = "10500,30304";
            supportedVideoSizes = "176x144,320x240,352x288,640x480,720x480,720x576,1280x720,1920x1080";
            defaultPreviewSize = "640x480";
            defaultVideoSize = "1920x1080";
            //full resolution SDV
            supportedSdvSizes = "";
            // Leaving this empty. NOTE: values need to be given in derived classes.
            supportedPreviewSizes = "";
            supportedPreviewUpdateModes = "standard,continuous,during-capture,windowless";
            defaultPreviewUpdateMode = "standard";
            //For high speed recording, slow motion playback
            hasSlowMotion = false;
            supportedHighSpeedResolutionFps = "";
            supportedRecordingFramerates = "";
            //For max Dvs in high speed mode
            maxHighSpeedDvsResolution = "";
            // Flash support
            hasFlash = false;
            // focus modes
            supportedFocusModes.appendFormat("%s,%s,%s,%s,%s,%s"
                ,CameraParameters::FOCUS_MODE_AUTO
                ,CameraParameters::FOCUS_MODE_INFINITY
                ,CameraParameters::FOCUS_MODE_FIXED
                ,CameraParameters::FOCUS_MODE_MACRO
                ,CameraParameters::FOCUS_MODE_CONTINUOUS_VIDEO
                ,CameraParameters::FOCUS_MODE_CONTINUOUS_PICTURE);
            defaultFocusMode.appendFormat("%s", CameraParameters::FOCUS_MODE_AUTO);
            maxNumFocusAreas = 0;
            defaultHdr = "off";
            supportedHdr = "on,off";
            defaultUltraLowLight = "off";
            supportedUltraLowLight = "auto,on,off";
            defaultFaceRecognition = "off";
            supportedFaceRecognition = "on,off";
            defaultSmileShutter = "off";
            supportedSmileShutter = "on,off";
            defaultBlinkShutter = "off";
            supportedBlinkShutter = "on,off";
            defaultPanorama = "off";
            supportedPanorama = "on,off";
            defaultSceneDetection = "off";
            supportedSceneDetection = "on,off";
            supportedIntelligentMode = "false";
            synchronizeExposure = false;
            maxNumYUVBufferForBurst = 10;
            maxNumYUVBufferForBracket = 10;
            useHALVS = false;
            // FOV
            verticalFOV = "";
            horizontalFOV = "";
            //DVS
            supportedDvsSizes = "640x480,720x480,720x576,1280x720,1920x1080";
            captureWarmUpFrames = 0;

            mIntelligentMode = false;
            supportedSensorMetadata = false;

            disable3A = false;

            useMultiStreamsForSoC = false;
            mPreviewFourcc = V4L2_PIX_FMT_NV12;
            defaultDepthFocalLength = 0;
            maxDepthPreviewBufferQueueSize = 0;
            mSupportsPostviewOutput = true;
            mISPSupportContinuousCaptureMode = true;
            mSupportsColorBarPreview = false;
        }

        String8 sensorName;
        bool extendedCamera;
        SensorType sensorType;
        int facing;
        int orientation;
        int flipping;
        bool dvs;
        bool narrowGamma;
        String8 supportedSnapshotSizes;
        int defaultJpegQuality;
        int defaultJpegThumbnailQuality;
        bool mPreviewViaOverlay;
        int overlayRelativeRotation;  /*<! Relative rotation between the native scan order of the
                                           camera and the display attached to the overlay */
        Vector<Size> mZSLUnsupportedSnapshotResolutions; // snapshot resolutions not supported by ZSL

        // snapshot resolutions not supported when continuous
        // viewfinder is used
        Vector<Size> mCVFUnsupportedSnapshotResolutions;

        bool continuousCapture;
        bool continuousJpegCapture;
        // burst
        String8 supportedBurstFPS; // TODO: it will be removed in the future
        String8 supportedBurstLength;
        String8 defaultBurstLength;
        // exposure
        String8 maxEV;
        String8 minEV;
        String8 stepEV;
        String8 defaultEV;
        // AE metering
        String8 supportedAeMetering;
        String8 defaultAeMetering;
        String8 supportedAeLock;
        // saturation
        String8 maxSaturation;
        String8 minSaturation;
        String8 stepSaturation;
        String8 defaultSaturation;
        String8 supportedSaturation;
        int lowSaturation;
        int highSaturation;
        // contrast
        String8 maxContrast;
        String8 minContrast;
        String8 stepContrast;
        String8 defaultContrast;
        String8 supportedContrast;
        int softContrast;
        int hardContrast;
        // sharpness
        String8 maxSharpness;
        String8 minSharpness;
        String8 stepSharpness;
        String8 defaultSharpness;
        String8 supportedSharpness;
        int softSharpness;
        int hardSharpness;
        // flash
        String8 supportedFlashModes;
        String8 defaultFlashMode;
        // iso
        String8 supportedIso;
        String8 defaultIso;
        // scene modes
        String8 supportedSceneModes;
        String8 defaultSceneMode;
        // effect
        String8 supportedEffectModes;
        String8 supportedIntelEffectModes;
        String8 defaultEffectMode;
        // awb
        String8 supportedAwbModes;
        String8 defaultAwbMode;
        String8 supportedAwbLock;
        // preview
        String8 supportedPreviewFrameRate;
        String8 supportedPreviewFPSRange;
        String8 defaultPreviewFPSRange;
        String8 supportedPreviewSizes;
        String8 supportedPreviewUpdateModes;
        String8 defaultPreviewUpdateMode;
        String8 supportedVideoSizes;
        String8 mVideoPreviewSizePref;
        String8 defaultPreviewSize;
        String8 defaultVideoSize;
        // For high speed recording, slow motion playback
        bool hasSlowMotion;
        String8 supportedHighSpeedResolutionFps;
        String8 supportedRecordingFramerates;
        // For max Dvs resolution in high speed mode
        String8 maxHighSpeedDvsResolution;
        // full resolution SDV
        String8 supportedSdvSizes;
        // Flash support
        bool hasFlash;
        // focus modes
        String8 supportedFocusModes;
        String8 defaultFocusMode;
        size_t maxNumFocusAreas;
        // INTEL Extras
        String8 defaultHdr;
        String8 supportedHdr;
        String8 defaultUltraLowLight;
        String8 supportedUltraLowLight;
        String8 defaultFaceRecognition;
        String8 supportedFaceRecognition;
        String8 defaultSmileShutter;
        String8 supportedSmileShutter;
        String8 defaultBlinkShutter;
        String8 supportedBlinkShutter;
        String8 defaultPanorama;
        String8 supportedPanorama;
        String8 defaultSceneDetection;
        String8 supportedSceneDetection;
        String8 supportedIntelligentMode;

        // SensorSyncManager
        // TODO: implement more control for how to synchronize, e.g. into
        //       which event and how (per sensor specific implementations
        //       available)
        bool synchronizeExposure;

        // For enabling the HAL video stabilization buffer flow
        // code paths
        bool useHALVS;

        /**
         * For max number of snapshot buffers.
         * Make it configurable for each sensor
         * The number of snapshot buffers is not necessary equal to burst length as before.
         * Here we set a value to control the max number of buffer we allocate in any capture case
         * The number value is mainly depends on the speed of jpeg encoder.
         * Bracketing of AE and AF needs more buffers
         */
        int maxNumYUVBufferForBurst;
        int maxNumYUVBufferForBracket;

        // FOV
        String8 verticalFOV;
        String8 horizontalFOV;
        //DVS
        String8 supportedDvsSizes;

        // For defining amount of needed warm-up frames, when skipping frames
        // at image capture. For example, some SoC sensors need some frames to
        // be skipped in order for the SoC sensor's 3A (or 2A) to converge.
        int captureWarmUpFrames;

        // to store the intelligent mode
        // if it's true, the 3A should be disabled
        bool mIntelligentMode;

        // sensor meta data
        bool supportedSensorMetadata;

        // in some test purpose, we need to disable 3A.
        // if this is needed, please make the disable3A to true
        bool disable3A;

        /**
         * if the sensor is SoC, this variable is valuable
         * if the FW could provide more than one stream, the useMultiStreamsForSoC should be set to true
         * for example, the FW could provide all four streams for SDV case
         */
        bool useMultiStreamsForSoC;

        // the preview format setting for seperate sensors
        int mPreviewFourcc;

        int defaultDepthFocalLength;
        int maxDepthPreviewBufferQueueSize;

        // Postview output support
        bool mSupportsPostviewOutput;

        // Isp support continuous capture mode setting.
        bool mISPSupportContinuousCaptureMode;

        // Color-bar preview support.
        bool mSupportsColorBarPreview;
    };

    // note: Android NDK does not yet support C++11 and
    //       initializer lists, so avoiding structs for now
    //       in these definitions (2012/May)

    Vector<CameraInfo> mCameras;

    // to support the device which has extended camera, it means the device has 3 cameras
    bool mUseExtendedCamera; // this will be set by the app
    bool mHasExtendedCamera; // it will set by the parser
    int mExtendedCameraId; // which must be 0 (back) or 1(front)
    int mExtendedCameraIndex; // the position in the mCameras, it could be o, 1, 2

    bool mFileInject;
    bool mSupportVideoSnapshot;
    bool mSupportsOfflineBurst;
    bool mSupportsOfflineBracket;
    bool mSupportsOfflineHdr;

    int mMaxContinuousRawRingBuffer;
    int mShutterLagCompensationMs;

    int mPanoramaMaxSnapshotCount;

    /* For EXIF Metadata */
    String8 mProductName;
    String8 mManufacturerName;

    /* For Device name */
    String8 mSubDevName;

    /* For Zoom factor */
    int mMaxZoomFactor;

    /*
     * For Recording Buffers number
     * Because we have 512MB RAM devices, like the Lex,
     * we have less memory for the recording.
     * So we need to make the recording buffers can be configured.
    */
    int mNumRecordingBuffers;

    int mNumPreviewBuffers;

    /* For Dual Video */
    bool mSupportDualVideo;

    /* For Preview Size Limitation*/
    bool mSupportPreviewLimitation;

    /* blackbay, or merr_vv, or redhookbay, or victoriabay... */
    String8 mBoardName;


    int mSensorGainLag;

    int mSensorExposureLag;

    // Ultra Low Light
    bool mUseIntelULL;

    // Used for reducing the frequency of face callbacks
    int mFaceCallbackDivider;

    // cpu cache line size
    int mCacheLineSize;

    // Max ISP timeout count
    int mMaxISPTimeoutCount;
private:
    static status_t getSensorInfo(Vector<SensorNameAndPort>& sensorInfo);
};

} /* namespace android */
#endif /* PLATFORMDATA_H_ */
