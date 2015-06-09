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

#ifndef ANDROID_LIBCAMERA_ATOM_SOC_3A
#define ANDROID_LIBCAMERA_ATOM_SOC_3A

#include <utils/Timers.h>
#include <utils/Errors.h>
#include <utils/Vector.h>
#include <utils/Errors.h>
#include <utils/threads.h>
#include <utils/String8.h>
#include <camera/CameraParameters.h>
#include "IntelParameters.h"
#include "AtomCommon.h"

#include "PlatformData.h"
#include "CameraConf.h"
#include "I3AControls.h"
#include "ICameraHwControls.h"

namespace android {

/**
 * \class AtomSoc3A
 *
 * AtomSoc3A implement I3AControls interface for SOC sensor.
 *
 * SOC sensor driver will provide simple 3A control functionality.
 * Class AtomSoc3A encapsulates these device operation.
 *
 * All access to sensor driver for 3A related queries will go via AtomSoc3A.
 *
 */
class AtomSoc3A : public I3AControls {
private:
    // constructor/destructor
    // prevent copy constructor and assignment operator
    AtomSoc3A(const AtomSoc3A& other);
    AtomSoc3A& operator=(const AtomSoc3A& other);

public:
    AtomSoc3A(int cameraId, HWControlGroup &hwcg);
    virtual ~AtomSoc3A();

// public methods
public:
    virtual SensorType getType() { return SENSOR_TYPE_SOC; }
    virtual status_t init3A();
    virtual status_t deinit3A();
    virtual void getDefaultParams(CameraParameters *params, CameraParameters *intel_params);
    virtual status_t setAeMode(AeMode mode);
    virtual AeMode getAeMode();
    virtual status_t setEv(float bias);
    virtual status_t getEv(float *ret);
    virtual status_t setAeSceneMode(SceneMode mode);
    virtual SceneMode getAeSceneMode();
    virtual status_t setAwbMode(AwbMode mode);
    virtual AwbMode getAwbMode();
    virtual status_t setManualIso(int iso);
    virtual status_t getManualIso(int *ret);
    /** expose iso mode setting*/
    virtual status_t setIsoMode(IsoMode mode);
    virtual IsoMode getIsoMode(void);
    virtual status_t setAeMeteringMode(MeteringMode mode);
    virtual MeteringMode getAeMeteringMode();
    virtual status_t set3AColorEffect(const char *effect);
    virtual status_t setAeFlickerMode(FlickerMode flickerMode);
    virtual status_t setAfMode(AfMode mode);
    virtual AfMode getAfMode();
    virtual status_t setAfEnabled(bool en);
    int     get3ALock(); // helper method for 3A lock setters/getters
    virtual bool     getAeLock();
    virtual status_t setAeLock(bool en);
    virtual bool     getAfLock();
    virtual status_t setAfLock(bool en);
    virtual status_t setAwbLock(bool en);
    virtual bool     getAwbLock();
    virtual status_t applyEv(float bias);
    virtual status_t setManualShutter(float expTime);
    virtual status_t setAeFlashMode(FlashMode mode);
    virtual FlashMode getAeFlashMode();
    virtual void setPublicAeMode(AeMode mode);
    virtual AeMode getPublicAeMode();

    // Only supported by Intel 3A
    virtual bool isIntel3A() { return false; }
    virtual status_t getAeManualBrightness(float *ret) { return INVALID_OPERATION; }
    virtual size_t   getAeMaxNumWindows() { return 0; }
    virtual size_t   getAfMaxNumWindows() { return 0; }
    virtual status_t setAeWindow(const CameraWindow *window) { return INVALID_OPERATION; }
    virtual status_t setAfWindows(const CameraWindow *windows, size_t numWindows) { return INVALID_OPERATION; }
    virtual status_t setManualFocusIncrement(int step) { return INVALID_OPERATION; }
    virtual status_t initAfBracketing(int stops,  AFBracketingMode mode) { return INVALID_OPERATION; }
    virtual status_t initAeBracketing() { return INVALID_OPERATION; }
    virtual status_t getExposureInfo(SensorAeConfig& sensorAeConfig) { return INVALID_OPERATION; }
    virtual status_t getGridWindow(AAAWindowInfo& window);
    virtual bool getAfNeedAssistLight() { return false; }
    virtual bool getAeFlashNecessary() { return false; }
    virtual AwbMode getLightSource() { return CAM_AWB_MODE_NOT_SET; }
    virtual status_t setAeBacklightCorrection(bool en) { return INVALID_OPERATION; }
    virtual status_t apply3AProcess(bool read_stats, struct timeval *frame_timestamp) { return INVALID_OPERATION; }
    virtual status_t startStillAf() { return INVALID_OPERATION; }
    virtual status_t stopStillAf() { return INVALID_OPERATION; }
    virtual AfStatus isStillAfComplete() { return CAM_AF_STATUS_FAIL; }
    virtual status_t applyPreFlashProcess(FlashStage stage) { return INVALID_OPERATION; }
    virtual status_t getGBCEResults(ia_aiq_gbce_results *gbce_results) { return INVALID_OPERATION; }

    virtual ia_binary_data *get3aMakerNote(ia_mkn_trg mode) { return NULL; }
    virtual void put3aMakerNote(ia_binary_data *mknData) { }
    virtual void reset3aMakerNote(void) { }
    virtual int add3aMakerNoteRecord(ia_mkn_dfid mkn_format_id,
                                     ia_mkn_dnid mkn_name_id,
                                     const void *record,
                                     unsigned short record_size) { return -1; }

    virtual status_t setSmartSceneDetection(bool en) { return INVALID_OPERATION; }
    virtual bool     getSmartSceneDetection() { return false; }
    virtual status_t switchModeAndRate(AtomMode mode, float fps) { return INVALID_OPERATION; }

    virtual AfStatus getCAFStatus() { return CAM_AF_STATUS_FAIL; }
    status_t getSmartSceneMode(int *sceneMode, bool *sceneHdr) { return INVALID_OPERATION; }
    status_t setFaces(const ia_face_state& faceState) { return INVALID_OPERATION; }
    status_t setFlash(int numFrames);
    virtual bool getAeUllTrigger() { return false; }

private:
    int mCameraId;
    IHWIspControl * mISP;
    IHWSensorControl * mSensorCI;
    IHWFlashControl * mFlashCI;
    IHWLensControl * mLensCI;
    AeMode mPublicAeMode;
}; // class AtomSoc3A

}; // namespace android

#endif // ANDROID_LIBCAMERA_ATOM_SOC_3A
