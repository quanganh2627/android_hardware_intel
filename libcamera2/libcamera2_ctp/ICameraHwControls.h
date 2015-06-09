/*
 * Copyright (c) 2013 Intel Corporation
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

#ifndef __LIBCAMERA2_ICAMERA_HW_CONTROLS_H__
#define __LIBCAMERA2_ICAMERA_HW_CONTROLS_H__

#include <utils/String8.h>
#include <camera/CameraParameters.h>
#include "IntelParameters.h"
#include "AtomCommon.h"
#include "AtomIspObserverManager.h"

namespace android {

#define CAM_WXH_STR(w,h) STRINGIFY_(w##x##h)
#define CAM_RESO_STR(w,h) CAM_WXH_STR(w,h) // example: CAM_RESO_STR(VGA_WIDTH,VGA_HEIGHT) -> "640x480"

struct sensorPrivateData
{
    void *data;
    unsigned int size;
    bool fetched; // true if data has been attempted to read, false otherwise
};

enum ObserverType {
    OBSERVE_PREVIEW_STREAM,
    OBSERVE_FRAME_SYNC_SOF,
    OBSERVE_3A_STAT_READY,
};

struct ContinuousCaptureConfig {
    int numCaptures;        /*!< Number of captures
                             * -1 = capture continuously
                             * 0 = disabled, stop captures
                             * >0 = burst of N snapshots
                             */
    int offset;             /*!< burst start offset */
    int skip;               /*!< skip factor */
};

/**
 * Define 4 states for current battery condition
 * Camera flash will be controlled more precisely
 */
enum BatteryStatus {
    BATTERY_STATUS_INVALID = -1, //invalid
    BATTERY_STATUS_NORMAL  = 0,  //flash on with 1.8A
    BATTERY_STATUS_WARNING,      //flash on with 1A
    BATTERY_STATUS_ALERT,        //flash on with <1A/Unused
    BATTERY_STATUS_CRITICAL,     //disable flash
};
/* Abstraction of HW algorithms control interface for 3A support*/
// Temporarily current AtomISP APIs are in IHWIspControl, as the top level IF for the reset of the HAL.
// Our target is APIs will be seperated to several interface classes in the near future
class IHWIspControl
{
public:
    /* **********************************************************
     * Constructor/Destructor
     */
    virtual ~IHWIspControl() {};

    virtual status_t initDevice() = 0;
    virtual status_t init() = 0;
    virtual void deInitDevice() = 0;
    virtual bool isDeviceInitialized() const = 0;

// public methods
public:
    /* **********************************************************
     * General
     */
    virtual void getDefaultParameters(CameraParameters *params, CameraParameters *intel_params) = 0;

    /* **********************************************************
     * ISP capabilities query
     */
    virtual int getCssMajorVersion() = 0; // TBD
    virtual int getCssMinorVersion(void) = 0; // TBD
    virtual int getIspHwMajorVersion(void) = 0; // TBD
    virtual int getIspHwMinorVersion(void) = 0; // TBD

    /* **********************************************************
     * Device controls
     */
    virtual AtomMode getMode(void) const = 0;
    virtual void setPreviewFramerate(int fps) = 0;
    virtual bool applyISPLimitations(CameraParameters *params, bool dvsEnabled, bool videoMode) = 0;

    /* **********************************************************
     * Pipeline controls
     */
    virtual status_t configure(AtomMode mode) = 0;
    virtual status_t allocateBuffers(AtomMode mode) = 0;
    virtual status_t start() = 0;
    virtual status_t stop() = 0;

    // For continous mode
    virtual bool isHALZSLEnabled() const = 0;
    virtual status_t startOfflineCapture(ContinuousCaptureConfig &config) = 0;
    virtual status_t stopOfflineCapture() = 0;
    virtual bool isOfflineCaptureRunning() const = 0;
    virtual int shutterLagZeroAlign() const = 0; // TBD
    virtual int continuousBurstNegMinOffset(void) const = 0; // TBD
    virtual int continuousBurstNegOffset(int skip, int startIndex) const = 0; // TBD
    virtual int getContinuousCaptureNumber() const = 0; // TBD
    virtual status_t prepareOfflineCapture(ContinuousCaptureConfig &config, bool capturePriority) = 0; // TBD

    virtual bool isSharedPreviewBufferConfigured(bool *reserved = NULL) const = 0; // TBD

    virtual int pollPreview(int timeout) = 0;
    virtual int pollCapture(int timeout) = 0;

    // For preview pipeline
    virtual status_t setPreviewFrameFormat(int width, int height, int bpl, int fourcc = 0) = 0;
    virtual void getPreviewSize(int *width, int *height, int *bpl) = 0;
    virtual status_t getPreviewFrame(AtomBuffer *buff) = 0;
    virtual status_t putPreviewFrame(AtomBuffer *buff) = 0;
    virtual status_t setGraphicPreviewBuffers(const AtomBuffer *buffs, int numBuffs, bool cached) = 0;
    virtual void setPreviewBufNum(int num) = 0;

    // For Video pipeline
    virtual status_t setVideoFrameFormat(int width, int height, int fourcc = 0) = 0;
    virtual void setRecordingFramerate(int fps) = 0;
    virtual int getRecordingFramerate() = 0;
    virtual bool checkSkipFrameRecording(int frameNum) = 0;
    virtual void getVideoSize(int *width, int *height, int *bpl) = 0;
    virtual status_t getRecordingFrame(AtomBuffer *buff) = 0;
    virtual status_t putRecordingFrame(AtomBuffer *buff) = 0;
    virtual status_t returnRecordingBuffers() = 0;
    // Enable metadata buffer mode API
    virtual status_t storeMetaDataInBuffers(bool enabled, int sID) = 0;

    // For capture pipelines
    virtual status_t setSnapshotFrameFormat(AtomBuffer& formatDescriptor) = 0;
    virtual int getSnapshotPixelFormat() = 0;

    virtual status_t getSnapshot(AtomBuffer *snaphotBuf, AtomBuffer *postviewBuf) = 0;
    virtual status_t putSnapshot(AtomBuffer *snaphotBuf, AtomBuffer *postviewBuf) = 0;
    virtual int getNumSnapshotBuffers() = 0;
    virtual status_t setSnapshotBuffers(Vector<AtomBuffer> *buffs, int numBuffs, bool cached) = 0;

    virtual status_t setPostviewFrameFormat(AtomBuffer& formatDescriptor) = 0;
    virtual void getPostviewFrameFormat(AtomBuffer& formatDescriptor) const = 0;
    virtual status_t setPostviewBuffers(Vector<AtomBuffer> *buffs, int numBuffs, bool cached) = 0;

    virtual bool dataAvailable() = 0;

    /* **********************************************************
     * ISP features
     */
    virtual int zoomRatio(int zoomValue) const = 0;
    virtual void getZoomRatios(CameraParameters *params) const = 0;
    virtual void getFocusDistances(CameraParameters *params) = 0;
    virtual status_t setZoom(int zoom) = 0;
    virtual int getDrvZoom(int zoom) = 0;
    virtual status_t setColorEffect(v4l2_colorfx effect) = 0;
    virtual status_t applyColorEffect() = 0;
    virtual status_t getMakerNote(atomisp_makernote_info *info) = 0;
    virtual status_t getContrast(int *value) = 0;
    virtual status_t setContrast(int value) = 0;
    virtual status_t getSaturation(int *value) = 0;
    virtual status_t setSaturation(int value) = 0;
    virtual status_t getSharpness(int *value) = 0;
    virtual status_t setSharpness(int value) = 0;
    virtual status_t setXNR(bool enable) = 0;
    virtual bool getXNR() const = 0;
    virtual status_t setLowLight(bool enable) = 0;
    virtual bool getLowLight() const = 0;
    virtual status_t setGDC(bool enable) = 0;
    virtual status_t initDVS() = 0;
    virtual status_t setDVSSkipFrames(unsigned int skips) = 0;
    virtual status_t setDVS(bool enable) = 0;
    virtual bool dvsEnabled() = 0;
    virtual void setNrEE(bool en) = 0;

    virtual int setAicParameter(struct atomisp_parameters *aic_params) = 0;
    virtual int setIspParameter(struct atomisp_parm *isp_params) = 0;
    virtual int getIspStatistics(struct atomisp_3a_statistics *statistics) = 0;
    virtual int setGdcConfig(const struct atomisp_morph_table *tbl) = 0;
    virtual int setShadingTable(struct atomisp_shading_table *table) = 0;
    virtual int setMaccConfig(struct atomisp_macc_config *macc_cfg) = 0;
    virtual int setCtcTable(const struct atomisp_ctc_table *ctc_tbl) = 0;
    virtual int setDeConfig(struct atomisp_de_config *de_cfg) = 0;
    virtual int setTnrConfig(struct atomisp_tnr_config *tnr_cfg) = 0;
    virtual int setEeConfig(struct atomisp_ee_config *ee_cfg) = 0;
    virtual int setNrConfig(struct atomisp_nr_config *nr_cfg) = 0;
    virtual int setDpConfig(struct atomisp_dp_config *dp_cfg) = 0;
    virtual int setWbConfig(struct atomisp_wb_config *wb_cfg) = 0;
    virtual int setObConfig(struct atomisp_ob_config *ob_cfg) = 0;
    virtual int set3aConfig(const struct atomisp_3a_config *cfg) = 0;
    virtual int setGammaTable(const struct atomisp_gamma_table *gamma_tbl) = 0;
    virtual int setGcConfig(const struct atomisp_gc_config *gc_cfg) = 0;
    virtual int setDvsConfig(const struct atomisp_dvs_6axis_config *dvs_6axis_cfg) = 0;

    virtual bool getPreviewTooBigForVFPP() = 0; // TBD

    virtual status_t getDvsStatistics(struct atomisp_dis_statistics *stats,
                              bool *tryAgain) const = 0;
    virtual status_t setMotionVector(const struct atomisp_dis_vector *vector) const = 0;
    virtual status_t setDvsCoefficients(const struct atomisp_dis_coefficients *coefs) const = 0;
    virtual status_t getIspParameters(struct atomisp_parm *isp_param) const = 0;

    virtual void getSensorDataFromFile(const char *file_name, sensorPrivateData *sensor_data) = 0;


    /* **********************************************************
     * Acceleration API extensions
     */
    virtual int loadAccFirmware(void *fw, size_t size, unsigned int *fwHandle) = 0;
    virtual int loadAccPipeFirmware(void *fw, size_t size, unsigned int *fwHandle) = 0;
    virtual int unloadAccFirmware(unsigned int fwHandle) = 0;
    virtual int mapFirmwareArgument(void *val, size_t size, unsigned long *ptr) = 0;
    virtual int unmapFirmwareArgument(unsigned long val, size_t size) = 0;
    virtual int setFirmwareArgument(unsigned int fwHandle, unsigned int num,
                            void *val, size_t size) = 0;
    virtual int setMappedFirmwareArgument(unsigned int fwHandle, unsigned int mem,
                                  unsigned long val, size_t size) = 0;
    virtual int unsetFirmwareArgument(unsigned int fwHandle, unsigned int num) = 0;
    virtual int startFirmware(unsigned int fwHandle) = 0;
    virtual int waitForFirmware(unsigned int fwHandle) = 0;
    virtual int abortFirmware(unsigned int fwHandle, unsigned int timeout) = 0;

    /* **********************************************************
     * File input/injection API
     */
    virtual int configureFileInject(const char* fileName, int width, int height, int fourcc, int bayerOrder) = 0;
    virtual bool isFileInjectionEnabled(void) const = 0;
    virtual String8 getFileInjectionFileName(void) const = 0;

    /* **********************************************************
     * AtomIspObserver controls
     */
    virtual status_t attachObserver(IAtomIspObserver *observer, ObserverType t) = 0;
    virtual status_t detachObserver(IAtomIspObserver *observer, ObserverType t) = 0;
    virtual void pauseObserver(ObserverType t) = 0;

};

/* Abstraction of HW sensor control interface for 3A support */
class IHWSensorControl
{
public:
    virtual ~IHWSensorControl() { };

    virtual const char * getSensorName(void) = 0;
    virtual int getCurrentCameraId(void) = 0;

    virtual float getFramerate() const = 0;
    virtual status_t waitForFrameSync() = 0;

    virtual unsigned int getExposureDelay() = 0;

    virtual int setExposure(struct atomisp_exposure *) = 0;
    virtual void getSensorData(sensorPrivateData *sensor_data) = 0;
    virtual int  getModeInfo(struct atomisp_sensor_mode_data *mode_data) = 0;
    virtual int  getExposureTime(int *exposure_time) = 0;
    virtual int  getAperture(int *aperture) = 0;
    virtual int  getFNumber(unsigned short  *fnum_num, unsigned short *fnum_denom) = 0;
    virtual int setExposureTime(int time) = 0;
    virtual int setExposureMode(v4l2_exposure_auto_type type) = 0;
    virtual int getExposureMode(v4l2_exposure_auto_type * type) = 0;
    virtual int setExposureBias(int bias) = 0;
    virtual int getExposureBias(int * bias) = 0;
    virtual int setSceneMode(v4l2_scene_mode mode) = 0;
    virtual int getSceneMode(v4l2_scene_mode * mode) = 0;
    virtual int setWhiteBalance(v4l2_auto_n_preset_white_balance mode) = 0;
    virtual int getWhiteBalance(v4l2_auto_n_preset_white_balance * mode) = 0;
    virtual int setIso(int iso) = 0;
    virtual int getIso(int * iso) = 0;
    virtual int setAeMeteringMode(v4l2_exposure_metering mode) = 0;
    virtual int getAeMeteringMode(v4l2_exposure_metering * mode) = 0;
    virtual int setAeFlickerMode(v4l2_power_line_frequency mode) = 0;
    virtual int setAfMode(v4l2_auto_focus_range mode) = 0;
    virtual int getAfMode(v4l2_auto_focus_range * mode) = 0;
    virtual int setAfEnabled(bool enable) = 0;
    virtual int set3ALock(int aaaLock) = 0;
    virtual int get3ALock(int * aaaLock) = 0;
    virtual int setAeFlashMode(v4l2_flash_led_mode mode) = 0;
    virtual int getAeFlashMode(v4l2_flash_led_mode * mode) = 0;

    virtual void getMotorData(sensorPrivateData *sensor_data) = 0;
    virtual int getRawFormat() = 0;
};

/* Abstraction of HW flash control interface for 3A support */
class IHWFlashControl
{
public:
    virtual ~IHWFlashControl() { };

    virtual status_t setFlash(int numFrames) = 0;
    virtual status_t setFlashIndicator(int intensity) = 0;
    virtual status_t setTorch(int intensity) = 0;
    /* Flash related controls */
    virtual int setFlashIntensity(int intensity) = 0;
    /* Check battery status for flash control */
    virtual BatteryStatus getBatteryStatus() = 0;
};

/* Abstraction of HW lens control interface for 3A support */
class IHWLensControl
{
public:
    virtual ~IHWLensControl() { };

    virtual int moveFocusToPosition(int position) = 0;
    virtual int moveFocusToBySteps(int steps) = 0;
    virtual int getFocusPosition(int * position) = 0;
    virtual int  getFocusStatus(int *status) = 0;
};

/* Compound object for HW control interfaces for 3A */
class HWControlGroup
{
public:
    HWControlGroup():
        mIspCI(NULL),
        mSensorCI(NULL),
        mFlashCI(NULL),
        mLensCI(NULL) { };
    ~HWControlGroup() { };

    IHWIspControl       *mIspCI;
    IHWSensorControl    *mSensorCI;
    IHWFlashControl     *mFlashCI;
    IHWLensControl      *mLensCI;
};


} // namespace android

#endif
