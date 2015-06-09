/*
 * Copyright (c) 2012 Intel Corporation
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

#ifndef I3ACONTROLS_H_
#define I3ACONTROLS_H_

#include <camera/CameraParameters.h>
#include <ia_mkn_types.h>
#include <ia_aiq_types.h>
#include "AtomCommon.h"
#include "ia_face.h"

namespace android {

#define EV_MIN -2
#define EV_MAX 2

// DetermineFlash: returns true if flash should be determined according to current exposure
#define DetermineFlash(x) (x == CAM_AE_FLASH_MODE_AUTO || \
                           x == CAM_AE_FLASH_MODE_DAY_SYNC || \
                           x == CAM_AE_FLASH_MODE_SLOW_SYNC) \

#define EV_LOWER_BOUND         -100
#define EV_UPPER_BOUND          100

enum AeMode
{
    CAM_AE_MODE_NOT_SET = -1,
    CAM_AE_MODE_AUTO,
    CAM_AE_MODE_MANUAL,
    CAM_AE_MODE_SHUTTER_PRIORITY,
    CAM_AE_MODE_APERTURE_PRIORITY
};

enum AfStatus
{
    CAM_AF_STATUS_FAIL = -1,
    CAM_AF_STATUS_IDLE,
    CAM_AF_STATUS_BUSY,
    CAM_AF_STATUS_SUCCESS
};

enum SceneMode
{
    CAM_AE_SCENE_MODE_NOT_SET = -1,
    CAM_AE_SCENE_MODE_AUTO,
    CAM_AE_SCENE_MODE_PORTRAIT,
    CAM_AE_SCENE_MODE_SPORTS,
    CAM_AE_SCENE_MODE_LANDSCAPE,
    CAM_AE_SCENE_MODE_NIGHT,
    CAM_AE_SCENE_MODE_NIGHT_PORTRAIT,
    CAM_AE_SCENE_MODE_FIREWORKS,
    CAM_AE_SCENE_MODE_TEXT,
    CAM_AE_SCENE_MODE_SUNSET,
    CAM_AE_SCENE_MODE_PARTY,
    CAM_AE_SCENE_MODE_CANDLELIGHT,
    CAM_AE_SCENE_MODE_BEACH_SNOW,
    CAM_AE_SCENE_MODE_DAWN_DUSK,
    CAM_AE_SCENE_MODE_FALL_COLORS,
    CAM_AE_SCENE_MODE_BACKLIGHT
};

enum AwbMode
{
    CAM_AWB_MODE_NOT_SET = -1,
    CAM_AWB_MODE_AUTO,
    CAM_AWB_MODE_MANUAL_INPUT,
    CAM_AWB_MODE_DAYLIGHT,
    CAM_AWB_MODE_SUNSET,
    CAM_AWB_MODE_CLOUDY,
    CAM_AWB_MODE_TUNGSTEN,
    CAM_AWB_MODE_FLUORESCENT,
    CAM_AWB_MODE_WARM_FLUORESCENT,
    CAM_AWB_MODE_SHADOW,
    CAM_AWB_MODE_WARM_INCANDESCENT
};

enum MeteringMode
{
    CAM_AE_METERING_MODE_NOT_SET = -1,
    CAM_AE_METERING_MODE_AUTO,
    CAM_AE_METERING_MODE_SPOT,
    CAM_AE_METERING_MODE_CENTER,
    CAM_AE_METERING_MODE_CUSTOMIZED
};

/** add Iso mode*/
/* ISO control mode setting */
enum IsoMode {
    CAM_AE_ISO_MODE_NOT_SET = -1,
    CAM_AE_ISO_MODE_AUTO,   /* Automatic */
    CAM_AE_ISO_MODE_MANUAL  /* Manual */
};

enum AfMode
{
    CAM_AF_MODE_NOT_SET = -1,
    CAM_AF_MODE_AUTO,
    CAM_AF_MODE_MACRO,
    CAM_AF_MODE_INFINITY,
    CAM_AF_MODE_FIXED,
    CAM_AF_MODE_MANUAL,
    CAM_AF_MODE_CONTINUOUS
};

struct SensorAeConfig
{
    float evBias;
    int expTime;
    short unsigned int aperture_num;
    short unsigned int aperture_denum;
    int aecApexTv;
    int aecApexSv;
    int aecApexAv;
    float digitalGain;
    float totalGain;
};

enum FlashMode
{
    CAM_AE_FLASH_MODE_NOT_SET = -1,
    CAM_AE_FLASH_MODE_AUTO,
    CAM_AE_FLASH_MODE_OFF,
    CAM_AE_FLASH_MODE_ON,
    CAM_AE_FLASH_MODE_DAY_SYNC,
    CAM_AE_FLASH_MODE_SLOW_SYNC,
    CAM_AE_FLASH_MODE_TORCH
};

enum FlashStage
{
    CAM_FLASH_STAGE_NOT_SET = -1,
    CAM_FLASH_STAGE_NONE,
    CAM_FLASH_STAGE_PRE,
    CAM_FLASH_STAGE_MAIN
};

enum FlickerMode
{
    CAM_AE_FLICKER_MODE_NOT_SET = -1,
    CAM_AE_FLICKER_MODE_OFF,
    CAM_AE_FLICKER_MODE_50HZ,
    CAM_AE_FLICKER_MODE_60HZ,
    CAM_AE_FLICKER_MODE_AUTO
};

enum AFBracketingMode
{
    CAM_AF_BRACKETING_MODE_SYMMETRIC,
    CAM_AF_BRACKETING_MODE_TOWARDS_NEAR,
    CAM_AF_BRACKETING_MODE_TOWARDS_FAR,
};

/**
 * I3AControls defines an interface for 3A controls.
 * For RAW cameras the 3A controls are handled in Intel 3A library,
 * and for SoC cameras they are set via V4L2 commands and handled
 * in the driver.
 *
 * This interface is implemented by AtomAIQ (for RAW) and
 * AtomSoc3A (for SoC) classes.
 */
class I3AControls
{
public:
    virtual ~I3AControls() {}
    virtual status_t init3A() = 0;
    virtual status_t deinit3A() = 0;

    virtual SensorType getType() = 0;

    virtual void getDefaultParams(CameraParameters *params, CameraParameters *intel_params) = 0;
    virtual status_t setAeMode(AeMode mode) = 0;
    virtual AeMode getAeMode() = 0;
    virtual status_t setEv(float bias) = 0;
    virtual status_t getEv(float *ret) = 0;
    virtual status_t setAeSceneMode(SceneMode mode) = 0;
    virtual SceneMode getAeSceneMode() = 0;
    virtual status_t setAwbMode(AwbMode mode) = 0;
    virtual AwbMode getAwbMode() = 0;
    virtual status_t setManualIso(int iso) = 0;
    virtual status_t getManualIso(int *ret) = 0;
    /** expose iso mode setting*/
    virtual status_t setIsoMode(IsoMode mode) = 0;
    virtual IsoMode getIsoMode(void) = 0;
    virtual status_t setAeMeteringMode(MeteringMode mode) = 0;
    virtual MeteringMode getAeMeteringMode() = 0;
    virtual status_t set3AColorEffect(const char *effect) = 0;
    virtual status_t setAfMode(AfMode mode) = 0;
    virtual AfMode getAfMode() = 0;
    virtual status_t setAfEnabled(bool en) = 0;
    virtual status_t setAeWindow(const CameraWindow *window) = 0;
    virtual status_t setAfWindows(const CameraWindow *windows, size_t numWindows) = 0;
    virtual status_t setAeFlickerMode(FlickerMode mode) = 0;

    // Intel 3A specific
    virtual bool isIntel3A() = 0;
    virtual status_t getAeManualBrightness(float *ret) = 0;
    virtual status_t setManualFocusIncrement(int step) = 0;
    virtual status_t initAfBracketing(int stops, AFBracketingMode mode) = 0;
    virtual status_t initAeBracketing() = 0;
    virtual status_t applyEv(float bias) = 0;
    virtual status_t getExposureInfo(SensorAeConfig& sensorAeConfig) = 0;
    virtual size_t   getAeMaxNumWindows() = 0;
    virtual size_t   getAfMaxNumWindows() = 0;
    virtual status_t getGridWindow(AAAWindowInfo& window) = 0;
    virtual bool     getAeLock() = 0;
    virtual status_t setAeLock(bool en) = 0;
    virtual bool     getAfLock() = 0;
    virtual status_t setAfLock(bool en) = 0;
    virtual status_t setAwbLock(bool en) = 0;
    virtual bool     getAwbLock() = 0;
    virtual status_t setAeFlashMode(FlashMode mode) = 0;
    virtual FlashMode getAeFlashMode() = 0;
    virtual bool getAfNeedAssistLight() = 0;
    virtual bool getAeFlashNecessary() = 0;
    virtual AwbMode getLightSource() = 0;
    virtual status_t setManualShutter(float expTime) = 0;
    virtual status_t setSmartSceneDetection(bool en) = 0;
    virtual bool     getSmartSceneDetection() = 0;
    virtual status_t getSmartSceneMode(int *sceneMode, bool *sceneHdr) = 0;
    virtual void setPublicAeMode(AeMode mode) = 0;
    virtual AeMode getPublicAeMode() = 0;
    virtual AfStatus getCAFStatus() = 0;
    virtual status_t setFaces(const ia_face_state& faceState) = 0;
    virtual status_t setFlash(int numFrames) = 0;
    virtual status_t getGBCEResults(ia_aiq_gbce_results *gbce_results) = 0;
    virtual bool getAeUllTrigger() = 0;

    virtual status_t switchModeAndRate(AtomMode mode, float fps) = 0;
    virtual status_t apply3AProcess(bool read_stats, struct timeval *frame_timestamp) = 0;
    virtual status_t startStillAf() = 0;
    virtual status_t stopStillAf() = 0;
    virtual AfStatus isStillAfComplete() = 0;
    virtual status_t applyPreFlashProcess(FlashStage stage) = 0;

    // Makernote
    virtual ia_binary_data *get3aMakerNote(ia_mkn_trg mode) = 0;
    virtual void put3aMakerNote(ia_binary_data *mknData) = 0;
    virtual void reset3aMakerNote(void) = 0;
    virtual int add3aMakerNoteRecord(ia_mkn_dfid mkn_format_id,
                                     ia_mkn_dnid mkn_name_id,
                                     const void *record,
                                     unsigned short record_size) = 0;
};

}

#endif /* I3ACONTROLS_H_ */
