/*
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

#define LOG_TAG "Camera_AtomAIQ"

#include <math.h>
#include <time.h>
#include <dlfcn.h>
#include <utils/String8.h>

#include "LogHelper.h"
#include "AtomCommon.h"
#include "PlatformData.h"
#include "PerformanceTraces.h"
#include "cameranvm.h"
#include "ia_cmc_parser.h"
#include "ia_isp_types.h"

#include "AtomAIQ.h"
#include "ia_mkn_encoder.h"
#include "ia_mkn_decoder.h"
#include "CameraDump.h"

const unsigned int AE_DELAY_FRAMES_DEFAULT = 2;
const nsecs_t AIQ_MAX_TIME_FOR_AF = 2500; // milliseconds
const int TORCH_INTENSITY = 20; // 20%
const float EPSILON = 0.00001;
const int RETRY_COUNT = 5;

namespace android {


#if ENABLE_PROFILING
    #define PERFORMANCE_TRACES_AAA_PROFILER_START() \
        do { \
            PerformanceTraces::AAAProfiler::enable(true); \
            PerformanceTraces::AAAProfiler::start(); \
           } while(0)

    #define PERFORMANCE_TRACES_AAA_PROFILER_STOP() \
        do {\
            PerformanceTraces::AAAProfiler::stop(); \
           } while(0)
#else
     #define PERFORMANCE_TRACES_AAA_PROFILER_START()
     #define PERFORMANCE_TRACES_AAA_PROFILER_STOP()
#endif

#define MAX_STATISTICS_WIDTH 150
#define MAX_STATISTICS_HEIGHT 150
#define IA_AIQ_MAX_NUM_FACES 5

AtomAIQ::AtomAIQ(HWControlGroup &hwcg, int cameraId):
    mISP(hwcg.mIspCI)
    ,mAfMode(CAM_AF_MODE_NOT_SET)
    ,mStillAfStart(0)
    ,mFocusPosition(0)
    ,mAfBracketingResult(NULL)
    ,mBracketingStops(0)
    ,mAeMode(CAM_AE_MODE_NOT_SET)
    ,mPublicAeMode(CAM_AE_MODE_NOT_SET)
    ,mAeSceneMode(CAM_AE_SCENE_MODE_NOT_SET)
    ,mAeFlashMode(CAM_AE_FLASH_MODE_NOT_SET)
    ,mFlashCount(0)
    ,mUllEnabled(false)
    ,mBracketingRunning(false)
    ,mAEBracketingResult(NULL)
    ,mAwbResults(NULL)
    ,mAwbMode(CAM_AWB_MODE_NOT_SET)
    ,mAwbLocked(false)
    ,mAwbRunCount(0)
    ,mGBCEResults(NULL)
    ,mPaResults(NULL)
    ,mMkn(NULL)
    ,mSensorCI(hwcg.mSensorCI)
    ,mFlashCI(hwcg.mFlashCI)
    ,mLensCI(hwcg.mLensCI)
    ,mISPAdaptor(NULL)
    ,mFileInjection(false)
    ,mCameraId(cameraId)
{
    LOG1("@%s", __FUNCTION__);
    CLEAR(mPrintFunctions);
    CLEAR(m3aState);
    CLEAR(mStatisticsInputParameters);
    CLEAR(mCustomExposureWeightGrid);
    CLEAR(mAfInputParameters);
    CLEAR(mAfState);
    CLEAR(mAeInputParameters);
    CLEAR(mAeManualLimits);
    CLEAR(mAeSensorDescriptor);
    CLEAR(mAeState);
    CLEAR(mAeCoord);
    CLEAR(mAeBracketingInputParameters);
    CLEAR(mAwbInputParameters);
    CLEAR(mIspInputParams);
    CLEAR(mDSDInputParameters);
    CLEAR(mDetectedSceneMode);
}

AtomAIQ::~AtomAIQ()
{
    LOG1("@%s", __FUNCTION__);

    // We don't need this memory anymore
    PlatformData::AiqConfig[mCameraId].clear();
}

status_t AtomAIQ::init3A()
{
    LOG1("@%s", __FUNCTION__);

    mFileInjection = (mCameraId == INTEL_FILE_INJECT_CAMERA_ID);
    status_t status = _init3A();

    return status;
}

status_t AtomAIQ::_init3A()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    ia_err ret = ia_err_none;
    String8 fullName, spIdName;
    int spacePos;

    ia_binary_data cpfData;
    status = getAiqConfig(&cpfData);
    if (status != NO_ERROR) {
        ALOGE("Error retrieving sensor params");
        return status;
    }

    ia_binary_data *aicNvm = NULL;
    ia_binary_data sensorData, motorData;
    mSensorCI->getSensorData((sensorPrivateData *) &sensorData);
    mSensorCI->getMotorData((sensorPrivateData *)&motorData);

    // Combine sensor name and spId
    PlatformData::createVendorPlatformProductName(spIdName);
    fullName = mSensorCI->getSensorName();

    spacePos = fullName.find(" ");

    if (spacePos < 0){
        fullName = mSensorCI->getSensorName();
    }
    else {
        fullName.setTo(fullName, spacePos);
    }

    fullName += "-";
    fullName += spIdName;
    LOG1("Sensor-vendor-platform-product name: %s", fullName.string());

    cameranvm_create( fullName,
                      &sensorData,
                      &motorData,
                      &aicNvm);

    mMkn = ia_mkn_init(ia_mkn_cfg_compression, 32000, 100000);
    if(mMkn == NULL)
        ALOGE("Error makernote init");
    ret = ia_mkn_enable(mMkn, true);
    if(ret != ia_err_none)
        ALOGE("Error makernote init");

    ia_cmc_t *cmc = ia_cmc_parser_init((ia_binary_data*)&(cpfData));

    // determine flash count from CPF data
    mFlashCount = 0;
    if (cmc) {
        if (cmc->cmc_multi_led_flashes)
            mFlashCount = cmc->cmc_multi_led_flashes->num_flash_devices;
        else if (cmc->cmc_parsed_flash_chromaticity.cmc_flash_chromaticity)
            mFlashCount = 1;
    }

    m3aState.ia_aiq_handle = ia_aiq_init((ia_binary_data*)&(cpfData),
                                         (ia_binary_data*)aicNvm,
                                         NULL,
                                         MAX_STATISTICS_WIDTH,
                                         MAX_STATISTICS_HEIGHT,
                                         NUM_EXPOSURES,
                                         cmc,
                                         mMkn);

    if ((mISP->getCssMajorVersion() == 1) && (mISP->getCssMinorVersion() == 5)){
        mISPAdaptor = new IaIsp15();
    }
    else if (((mISP->getCssMajorVersion() == 2) && (mISP->getCssMinorVersion() == 0)) ||
             ((mISP->getCssMajorVersion() == 2) && (mISP->getCssMinorVersion() == 1))){
        mISPAdaptor = new IaIsp22();
    }

    if (mISPAdaptor == NULL) {
        ALOGE("Ambiguous CSS version used: %d.%d", mISP->getCssMajorVersion(), mISP->getCssMinorVersion());
        cameranvm_delete(aicNvm);
        return UNKNOWN_ERROR;
    }

    mISPAdaptor->initIaIspAdaptor((ia_binary_data*)&(cpfData),
                         MAX_STATISTICS_WIDTH,
                         MAX_STATISTICS_HEIGHT,
                         cmc,
                         mMkn,
                         mCameraId);

    ia_cmc_parser_deinit(cmc);


    if (!m3aState.ia_aiq_handle) {
        cameranvm_delete(aicNvm);
        return UNKNOWN_ERROR;
    }

    m3aState.frame_use = ia_aiq_frame_use_preview;
    m3aState.dsd_enabled = false;

    run3aInit();

    cameranvm_delete(aicNvm);
    m3aState.stats = NULL;
    m3aState.stats_valid = false;
    CLEAR(m3aState.results);

    if(init_weighting_map() != NO_ERROR)
        LOG2("There is no weight map in cpf file");

    return status;
}

status_t AtomAIQ::getAiqConfig(ia_binary_data *cpfData)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    if (PlatformData::AiqConfig[mCameraId] && cpfData != NULL) {
        cpfData->data = PlatformData::AiqConfig[mCameraId].ptr();
        cpfData->size = PlatformData::AiqConfig[mCameraId].size();
    } else {
        status = UNKNOWN_ERROR;
    }
    return status;
}

status_t AtomAIQ::init_weighting_map()
{
    LOG2("@%s", __FUNCTION__);
    status_t ret = NO_ERROR;
    int weight_width = 0, weight_height = 0;
    PlatformData::HalConfig[mCameraId].getValue(weight_width, CPF::ExposureWeights, CPF::Width);
    PlatformData::HalConfig[mCameraId].getValue(weight_height, CPF::ExposureWeights, CPF::Height);
    if(weight_width > 0 && weight_height > 0) {
        const char *  weight_p= NULL;
        char * end_ptr = NULL;
        const int weight_len = weight_width * weight_height;
        unsigned char * exposure_weight_value = (unsigned char *)malloc(sizeof(unsigned char) * weight_len);
        if(exposure_weight_value == NULL) {
            ret = NO_MEMORY;
            return ret;
        }
        memset(exposure_weight_value, 0, sizeof(unsigned char) * weight_len);
        weight_p = PlatformData::HalConfig[mCameraId].getString(CPF::ExposureWeights, CPF::WeightGrid);
        if(weight_p == NULL){
            ret = BAD_VALUE;
            return ret;
        }
        for(int i = 0; i < weight_len; i++) {
            exposure_weight_value[i] = strtol(weight_p, &end_ptr, 10);
            weight_p = end_ptr + 1;
        }
        mCustomExposureWeightGrid.width = (unsigned short int)weight_width;
        mCustomExposureWeightGrid.height = (unsigned short int)weight_height;
        mCustomExposureWeightGrid.weights = exposure_weight_value;
    }
    return ret;
}

status_t AtomAIQ::deinit3A()
{
    LOG1("@%s", __FUNCTION__);
    if (mAeState.stored_results) {
        delete mAeState.stored_results;
        mAeState.stored_results = NULL;
    }
    free(m3aState.faces);
    m3aState.faces = NULL;
    freeStatistics(m3aState.stats);
    ia_aiq_deinit(m3aState.ia_aiq_handle);
    delete mISPAdaptor;
    mISPAdaptor = NULL;
    ia_mkn_uninit(mMkn);
    mISP = NULL;
    mAfMode = CAM_AF_MODE_NOT_SET;
    mAwbMode = CAM_AWB_MODE_NOT_SET;
    mFocusPosition = 0;
    free(mCustomExposureWeightGrid.weights);
    mCustomExposureWeightGrid.weights = NULL;
    return NO_ERROR;
}

unsigned int AtomAIQ::getExposureDelay() const {
    LOG2("@%s", __FUNCTION__);

    unsigned int ret(mSensorCI->getExposureDelay());

    if (ret == 0)
        ret = AE_DELAY_FRAMES_DEFAULT;

    return ret;
}

status_t AtomAIQ::switchModeAndRate(AtomMode mode, float fps)
{
    status_t status = NO_ERROR;
    LOG1("@%s: mode = %d", __FUNCTION__, mode);

    ia_aiq_frame_use isp_mode;
    switch (mode) {
    case MODE_PREVIEW:
        isp_mode = ia_aiq_frame_use_preview;
        break;
    case MODE_CAPTURE:
        isp_mode = ia_aiq_frame_use_still;
        break;
    case MODE_VIDEO:
    case MODE_CONTINUOUS_VIDEO:
        isp_mode = ia_aiq_frame_use_video;
        break;
    case MODE_CONTINUOUS_CAPTURE:
        isp_mode = ia_aiq_frame_use_continuous;
        break;
    default:
        isp_mode = ia_aiq_frame_use_preview;
        ALOGW("SwitchMode: Wrong sensor mode %d", mode);
        break;
    }

    m3aState.frame_use = isp_mode;
    mAfInputParameters.frame_use = m3aState.frame_use;
    mAfState.previous_sof = 0;
    mAeInputParameters.frame_use = m3aState.frame_use;
    mAeInputParameters.manual_limits->manual_frame_time_us_min = (long) 1/fps*1000000;
    mAwbInputParameters.frame_use = m3aState.frame_use;

    // In high speed recording, set the AE operation mode as action to notify AIQ
    if (mode == MODE_VIDEO && fps > DEFAULT_RECORDING_FPS) {
        mAeInputParameters.operation_mode = ia_aiq_ae_operation_mode_action;
    } else if (mAeSceneMode == CAM_AE_SCENE_MODE_NOT_SET || mAeSceneMode == CAM_AE_SCENE_MODE_AUTO) {
        // When the default AE scene mode (AUTO) is used, the application will not set the
        // scene mode when switching between different capture modes. Reset the AE operation
        // mode to default value when not in HS recording mode
        setAeOperationModeAutoOrUll();
    }

    /* usually the grid changes as well when the mode changes. */
    changeSensorMode();
    if (mBracketingRunning) {
        mAeBracketingInputParameters = mAeInputParameters;
        mAeBracketingInputParameters.frame_use =  m3aState.frame_use;
    }

    // Using fixed delay to sync active sensor exposure with AEC input
    // parameters.
    // Note: fixed delay needs to be big enough to cover latencies of
    //       3A process itself (timing of applying compared to frames)
    //       and the pre-defined latencies by sensor
    // TODO: Use API's further to control this setting
    if (mAeInputParameters.frame_use == ia_aiq_frame_use_still)
        mAeState.feedback_delay = 0;
    else
        mAeState.feedback_delay = getExposureDelay();

    /* Invalidate and re-run AEC to re-calculate sensor exposure for potential changes
     * in sensor settings */
    mAeState.ae_results = NULL;
    status = runAeMain();

    /* Default GBCE is used with flash.
     * We use default GBCE flag to indicate when to restore results from
     * pre-flash start condition.
     * Note:
     * - AWB is run during flash sequence and results are therefore explicitly stored
     * - GBCE is run during flash sequence with default gamma
     *   runAICMain()
     */

    status |= runGBCEMain();

    if (mGBCEDefault && isp_mode != ia_aiq_frame_use_still) {
        LOG1("Restoring AWBresults that preceded flash");
        mGBCEDefault = false;
        mAwbResults = &mAwbStoredResults;
    } else {
        /* run AWB to get initial values */
        runAwbMain();
    }

    /* Re-run AIC to get new results for new mode. LSC needs to be updated if resolution changes. */
    status |= runAICMain();

    return status;
}

status_t AtomAIQ::setAeWindow(CameraWindow *window, const AAAWindowInfo*)
{

    LOG1("@%s", __FUNCTION__);

    if (window == NULL)
        return BAD_VALUE;

    // Convert to Intel AIQ coordinates
    convertFromAndroidToIaCoordinates(*window, *window);

    LOG1("window (%d,%d,%d,%d,%d)", window->x_left,
                                    window->y_top,
                                    window->x_right,
                                    window->y_bottom,
                                    window->weight);


    /* Calculate center coordinate of window. */
    int width =  window->x_right - window->x_left;
    int height = window->y_bottom - window->y_top;
    mAeCoord.x = window->x_left + width/2;
    mAeCoord.y = window->y_top + height/2;

    return NO_ERROR;
}

status_t AtomAIQ::setAfWindow(const CameraWindow *window)
{
    LOG1("@%s: window = %p (%d,%d,%d,%d,%d)", __FUNCTION__,
            window,
            window->x_left,
            window->y_top,
            window->x_right,
            window->y_bottom,
            window->weight);

    // In case of null-window, the HAL can decide which metering is used. Use "auto".
    if (window[0].x_left == window[0].x_right && window[0].y_top == window[0].y_bottom &&
        window[0].x_left == 0 && window[0].y_top == 0) {
            setAfMeteringMode(ia_aiq_af_metering_mode_auto);
    } else {
        // When window is set, obey the coordinates. Use touch.
        setAfMeteringMode(ia_aiq_af_metering_mode_touch);
        LOG1("Af window not NULL, metering mode touch");
    }

    mAfInputParameters.focus_rect->left = window[0].x_left;
    mAfInputParameters.focus_rect->top = window[0].y_top;
    mAfInputParameters.focus_rect->width = window[0].x_right - window[0].x_left;
    mAfInputParameters.focus_rect->height = window[0].y_bottom - window[0].y_top;

    //ToDo: Make sure that all coordinates passed to AIQ are in format/range defined in ia_coordinate.h.

    return NO_ERROR;
}

status_t AtomAIQ::setAfWindows(CameraWindow *windows, size_t numWindows, const AAAWindowInfo*)
{
    LOG2("@%s: windows = %p, num = %u", __FUNCTION__, windows, numWindows);

    // If no windows given, equal to null-window. HAL meters as it wants -> "auto".
    if (numWindows == 0) {
        setAfMeteringMode(ia_aiq_af_metering_mode_auto);
        return NO_ERROR;
    }

    // Conversion func from AtomCommon:
    // convWindow left to the default parameter NULL value
    //  -> Intel AIQ coordinates used.
    convertAfWindows(windows, numWindows);

    // at the moment we only support one window
    return setAfWindow(windows);
}

// TODO: no manual setting for scene mode, map that into AE/AF operation mode
status_t AtomAIQ::setAeSceneMode(SceneMode mode)
{
    LOG1("@%s: mode = %d", __FUNCTION__, mode);

    mAeSceneMode = mode;
    resetAECParams();
    resetAWBParams();
    switch (mode) {
    case CAM_AE_SCENE_MODE_AUTO:
        break;
    case CAM_AE_SCENE_MODE_PORTRAIT:
        break;
    case CAM_AE_SCENE_MODE_SPORTS:
        mAeInputParameters.operation_mode = ia_aiq_ae_operation_mode_action;
        break;
    case CAM_AE_SCENE_MODE_LANDSCAPE:
        break;
    case CAM_AE_SCENE_MODE_NIGHT:
        mAeInputParameters.operation_mode = ia_aiq_ae_operation_mode_long_exposure;
        mAeInputParameters.flash_mode = ia_aiq_flash_mode_off;
        break;
    case CAM_AE_SCENE_MODE_FIREWORKS:
        mAeInputParameters.operation_mode = ia_aiq_ae_operation_mode_fireworks;
        mAwbInputParameters.scene_mode = ia_aiq_awb_operation_mode_manual_cct_range;
        m3aState.cct_range.min_cct = 5500;
        m3aState.cct_range.max_cct = 5500;
        mAwbInputParameters.manual_cct_range = &m3aState.cct_range;
        break;
    default:
        ALOGE("Get: invalid AE scene mode (%d).", mode);
    }
    return NO_ERROR;
}

SceneMode AtomAIQ::getAeSceneMode()
{
    LOG1("@%s", __FUNCTION__);
    return mAeSceneMode;
}

status_t AtomAIQ::setAeFlickerMode(FlickerMode mode)
{
    LOG1("@%s: mode = %d", __FUNCTION__, mode);

    switch(mode) {
    case CAM_AE_FLICKER_MODE_50HZ:
        mAeInputParameters.flicker_reduction_mode = ia_aiq_ae_flicker_reduction_50hz;
        break;
    case CAM_AE_FLICKER_MODE_60HZ:
        mAeInputParameters.flicker_reduction_mode = ia_aiq_ae_flicker_reduction_60hz;
        break;
    case CAM_AE_FLICKER_MODE_AUTO:
        mAeInputParameters.flicker_reduction_mode = ia_aiq_ae_flicker_reduction_auto;
        break;
    case CAM_AE_FLICKER_MODE_OFF:
    default:
        mAeInputParameters.flicker_reduction_mode = ia_aiq_ae_flicker_reduction_off;
        break;
    }

    return NO_ERROR;
}

status_t AtomAIQ::setUllEnabled(bool enabled)
{
    LOG1("@%s: enabled = %s", __FUNCTION__, enabled?"true":"false");

    mUllEnabled = enabled;

    if (mAeInputParameters.operation_mode == ia_aiq_ae_operation_mode_ultra_low_light ||
        mAeInputParameters.operation_mode == ia_aiq_ae_operation_mode_automatic) {
        setAeOperationModeAutoOrUll();
    }

    return NO_ERROR;
}

// No support for aperture priority, always in shutter priority mode
status_t AtomAIQ::setAeMode(AeMode mode)
{
    LOG1("@%s: mode = %d", __FUNCTION__, mode);

    mAeMode = mode;
    switch(mode) {
    case CAM_AE_MODE_MANUAL:
        break;
    case CAM_AE_MODE_AUTO:
    case CAM_AE_MODE_SHUTTER_PRIORITY:
    case CAM_AE_MODE_APERTURE_PRIORITY:
    default:
        mAeInputParameters.manual_analog_gain = -1;
        mAeInputParameters.manual_iso = -1;
        mAeInputParameters.manual_exposure_time_us = -1;
        setAeOperationModeAutoOrUll();
        break;
    }
    return NO_ERROR;
}

AeMode AtomAIQ::getAeMode()
{
    LOG1("@%s", __FUNCTION__);
    return mAeMode;
}

status_t AtomAIQ::setAfMode(AfMode mode)
{
    LOG1("@%s: mode = %d", __FUNCTION__, mode);

    switch (mode) {
    case CAM_AF_MODE_CONTINUOUS:
        setAfFocusMode(ia_aiq_af_operation_mode_auto);
        setAfFocusRange(ia_aiq_af_range_normal);
        setAfMeteringMode(ia_aiq_af_metering_mode_auto);
        break;
    case CAM_AF_MODE_AUTO:
        // we use hyperfocal default lens position in hyperfocal mode
        setAfFocusMode(ia_aiq_af_operation_mode_manual);
        setAfFocusRange(ia_aiq_af_range_extended);
        setAfMeteringMode(ia_aiq_af_metering_mode_auto);
        break;
    case CAM_AF_MODE_MACRO:
        setAfFocusMode(ia_aiq_af_operation_mode_manual);
        setAfFocusRange(ia_aiq_af_range_macro);
        setAfMeteringMode(ia_aiq_af_metering_mode_auto);
        break;
    case CAM_AF_MODE_INFINITY:
        setAfFocusMode(ia_aiq_af_operation_mode_infinity);
        setAfFocusRange(ia_aiq_af_range_extended);
        break;
    case CAM_AF_MODE_FIXED:
        setAfFocusMode(ia_aiq_af_operation_mode_hyperfocal);
        setAfFocusRange(ia_aiq_af_range_extended);
        break;
    case CAM_AF_MODE_MANUAL:
        setAfFocusMode(ia_aiq_af_operation_mode_manual);
        setAfFocusRange(ia_aiq_af_range_extended);
        break;
    default:
        ALOGE("Set: invalid AF mode: %d. Using AUTO!", mode);
        mode = CAM_AF_MODE_AUTO;
        setAfFocusMode(ia_aiq_af_operation_mode_auto);
        setAfFocusRange(ia_aiq_af_range_normal);
        setAfMeteringMode(ia_aiq_af_metering_mode_auto);
        break;
    }

    mAfMode = mode;

    return NO_ERROR;
}

AfMode AtomAIQ::getAfMode()
{
    LOG2("@%s, afMode: %d", __FUNCTION__, mAfMode);
    return mAfMode;
}

status_t AtomAIQ::setAeFlashMode(FlashMode mode)
{
    LOG1("@%s: mode = %d", __FUNCTION__, mode);
    // no support for slow sync and day sync flash mode,
    // just use auto flash mode to replace
    ia_aiq_flash_mode wr_val;
    switch (mode) {
    case CAM_AE_FLASH_MODE_ON:
    case CAM_AE_FLASH_MODE_DAY_SYNC:
    case CAM_AE_FLASH_MODE_SLOW_SYNC:
        wr_val = ia_aiq_flash_mode_on;
        break;
    case CAM_AE_FLASH_MODE_TORCH:
        if (mAeFlashMode != CAM_AE_FLASH_MODE_TORCH)
            mFlashCI->setTorch(TORCH_INTENSITY);
        // Note: intentional omit of break, AEC is run with
        //       ia_aiq_flash_mode_off when torch is used.
    case CAM_AE_FLASH_MODE_OFF:
        wr_val = ia_aiq_flash_mode_off;
        break;
    case CAM_AE_FLASH_MODE_AUTO:
        wr_val = ia_aiq_flash_mode_auto;
        break;
    default:
        ALOGE("Set: invalid flash mode: %d. Using AUTO!", mode);
        mode = CAM_AE_FLASH_MODE_AUTO;
        wr_val = ia_aiq_flash_mode_auto;
    }
    if (mAeFlashMode == CAM_AE_FLASH_MODE_TORCH
        && mode != CAM_AE_FLASH_MODE_TORCH)
        mFlashCI->setTorch(0);
    mAeFlashMode = mode;
    mAeInputParameters.flash_mode = wr_val;

    return NO_ERROR;
}

FlashMode AtomAIQ::getAeFlashMode()
{
    LOG2("@%s", __FUNCTION__);
    return mAeFlashMode;
}

bool AtomAIQ::getAfNeedAssistLight()
{
    LOG1("@%s", __FUNCTION__);
    bool ret = false;
    if (mAfState.assist_light)
        ret = true;
    else if (mAfState.af_results)
        ret = mAfState.af_results->use_af_assist;
    return ret;
}

// ToDo: check if this function is needed or if the info
// could be used directly from AE results
FlashStage AtomAIQ::getAeFlashNecessity()
{
    FlashStage ret(CAM_FLASH_STAGE_NONE);

    if(mAeState.ae_results) {
        switch(mAeState.ae_results->flashes->status) {
        case ia_aiq_flash_status_no:
            ret = CAM_FLASH_STAGE_NONE;
            break;
        case ia_aiq_flash_status_torch:
        case ia_aiq_flash_status_red_eye_reduction:
        case ia_aiq_flash_status_pre:
            ret = CAM_FLASH_STAGE_PRE;
            break;
        case ia_aiq_flash_status_main:
            ret = CAM_FLASH_STAGE_MAIN;
            break;
        default:
            ALOGE("Unkown ae results flash status!");
            ret = CAM_FLASH_STAGE_NONE;
            break;
        }
    }

    return ret;
}

status_t AtomAIQ::setAwbMode (AwbMode mode)
{
    LOG1("@%s: mode = %d", __FUNCTION__, mode);
    ia_aiq_awb_operation_mode wr_val;
    switch (mode) {
    case CAM_AWB_MODE_DAYLIGHT:
        wr_val = ia_aiq_awb_operation_mode_daylight;
        break;
    case CAM_AWB_MODE_CLOUDY:
        wr_val = ia_aiq_awb_operation_mode_partly_overcast;
        break;
    case CAM_AWB_MODE_SUNSET:
        wr_val = ia_aiq_awb_operation_mode_sunset;
        break;
    case CAM_AWB_MODE_TUNGSTEN:
        wr_val = ia_aiq_awb_operation_mode_incandescent;
        break;
    case CAM_AWB_MODE_FLUORESCENT:
        wr_val = ia_aiq_awb_operation_mode_fluorescent;
        break;
    case CAM_AWB_MODE_WARM_FLUORESCENT:
        wr_val = ia_aiq_awb_operation_mode_fluorescent;
        break;
    case CAM_AWB_MODE_WARM_INCANDESCENT:
        wr_val = ia_aiq_awb_operation_mode_incandescent;
        break;
    case CAM_AWB_MODE_SHADOW:
        wr_val = ia_aiq_awb_operation_mode_fully_overcast;
        break;
    case CAM_AWB_MODE_MANUAL_INPUT:
        wr_val = ia_aiq_awb_operation_mode_manual_white;
        break;
    case CAM_AWB_MODE_AUTO:
        wr_val = ia_aiq_awb_operation_mode_auto;
        break;
    default:
        ALOGE("Set: invalid AWB mode: %d. Using AUTO!", mode);
        mode = CAM_AWB_MODE_AUTO;
        wr_val = ia_aiq_awb_operation_mode_auto;
    }

    mAwbMode = mode;
    mAwbInputParameters.scene_mode = wr_val;
    LOG2("@%s: Intel mode = %d", __FUNCTION__, mAwbInputParameters.scene_mode);
    return NO_ERROR;
}

AwbMode AtomAIQ::getAwbMode()
{
    LOG1("@%s", __FUNCTION__);
    return mAwbMode;
}

AwbMode AtomAIQ::getLightSource()
{
    LOG1("@%s", __FUNCTION__);
    AwbMode mode = getAwbMode();
    return mode;
}

status_t AtomAIQ::setAeMeteringMode(MeteringMode mode)
{
    LOG1("@%s: mode = %d", __FUNCTION__, mode);

    /* Don't use exposure coordinate in other than SPOT mode. */
    mAeInputParameters.exposure_coordinate = NULL;

    ia_aiq_ae_metering_mode wr_val;
    switch (mode) {
    case CAM_AE_METERING_MODE_SPOT:
        mAeInputParameters.exposure_coordinate = &mAeCoord;
        wr_val = ia_aiq_ae_metering_mode_evaluative;
        break;
    case CAM_AE_METERING_MODE_CENTER:
        wr_val = ia_aiq_ae_metering_mode_center;
        break;
    case CAM_AE_METERING_MODE_CUSTOMIZED:
    case CAM_AE_METERING_MODE_AUTO:
        wr_val = ia_aiq_ae_metering_mode_evaluative;
        break;
    default:
        ALOGE("Set: invalid AE metering mode: %d. Using AUTO!", mode);
        wr_val = ia_aiq_ae_metering_mode_evaluative;
    }
    mAeInputParameters.metering_mode = wr_val;

    return NO_ERROR;
}

MeteringMode AtomAIQ::getAeMeteringMode()
{
    LOG1("@%s", __FUNCTION__);
    MeteringMode mode = CAM_AE_METERING_MODE_NOT_SET;

    ia_aiq_ae_metering_mode rd_val = mAeInputParameters.metering_mode;
    switch (rd_val) {
    case ia_aiq_ae_metering_mode_evaluative:
        /* Handle SPOT mode. */
        if (mAeInputParameters.exposure_coordinate)
            mode = CAM_AE_METERING_MODE_SPOT;
        else
            mode = CAM_AE_METERING_MODE_AUTO;
        break;
    case ia_aiq_ae_metering_mode_center:
        mode = CAM_AE_METERING_MODE_CENTER;
        break;
    default:
        ALOGE("Get: invalid AE metering mode: %d. Using SPOT!", rd_val);
        mode = CAM_AE_METERING_MODE_SPOT;
    }

    return mode;
}

status_t AtomAIQ::setAeLock(bool en)
{
    LOG1("@%s: en = %d", __FUNCTION__, en);
    mAeState.ae_locked = en;
    return NO_ERROR;
}

bool AtomAIQ::getAeLock()
{
    LOG1("@%s", __FUNCTION__);
    bool ret = mAeState.ae_locked;
    return ret;
}

status_t AtomAIQ::setAfLock(bool en)
{
    LOG1("@%s: en = %d", __FUNCTION__, en);
    mAfState.af_locked = en;
    return NO_ERROR;
}

bool AtomAIQ::getAfLock()
{
    LOG1("@%s, af_locked: %d ", __FUNCTION__, mAfState.af_locked);
    bool ret = false;
    ret = mAfState.af_locked;
    return ret;
}

AfStatus AtomAIQ::getCAFStatus()
{
    LOG2("@%s", __FUNCTION__);
    AfStatus status = CAM_AF_STATUS_BUSY;
    if (mAfState.af_results != NULL) {
        if (mAfState.af_results->status == ia_aiq_af_status_success && (mAfState.af_results->final_lens_position_reached || mStillAfStart == 0)) {
            status = CAM_AF_STATUS_SUCCESS;
        }
        else if (mAfState.af_results->status == ia_aiq_af_status_fail && (mAfState.af_results->final_lens_position_reached || mStillAfStart == 0)) {
            status  = CAM_AF_STATUS_FAIL;
        }
        else {
            status = CAM_AF_STATUS_BUSY;
        }
    }

    if (mStillAfStart != 0 && status == CAM_AF_STATUS_BUSY) {
        if (((systemTime() - mStillAfStart) / 1000000) > AIQ_MAX_TIME_FOR_AF) {
            ALOGW("Auto-focus sequence for still capture is taking too long. Cancelling!");
            status = CAM_AF_STATUS_FAIL;
        }
    }
    LOG2("af_results->status:%d", status);
    return status;
}

status_t AtomAIQ::setAwbLock(bool en)
{
    LOG1("@%s: en = %d", __FUNCTION__, en);
    mAwbLocked = en;
    return NO_ERROR;
}

bool AtomAIQ::getAwbLock()
{
    LOG1("@%s, AsbLocked: %d", __FUNCTION__, mAwbLocked);
    bool ret = mAwbLocked;
    return ret;
}

status_t AtomAIQ::set3AColorEffect(const char *effect)
{
    LOG1("@%s: effect = %s", __FUNCTION__, effect);
    status_t status = NO_ERROR;

    ia_isp_effect ispEffect = ia_isp_effect_none;
    if (strncmp(effect, CameraParameters::EFFECT_MONO, strlen(effect)) == 0)
        ispEffect = ia_isp_effect_grayscale;
    else if (strncmp(effect, CameraParameters::EFFECT_NEGATIVE, strlen(effect)) == 0)
        ispEffect = ia_isp_effect_negative;
    else if (strncmp(effect, CameraParameters::EFFECT_SEPIA, strlen(effect)) == 0)
        ispEffect = ia_isp_effect_sepia;
    else if (strncmp(effect, IntelCameraParameters::EFFECT_STILL_SKY_BLUE, strlen(effect)) == 0)
        ispEffect = ia_isp_effect_sky_blue;
    else if (strncmp(effect, IntelCameraParameters::EFFECT_STILL_GRASS_GREEN, strlen(effect)) == 0)
        ispEffect = ia_isp_effect_grass_green;
    else if (strncmp(effect, IntelCameraParameters::EFFECT_STILL_SKIN_WHITEN_LOW, strlen(effect)) == 0)
        ispEffect = ia_isp_effect_skin_whiten_low;
    else if (strncmp(effect, IntelCameraParameters::EFFECT_STILL_SKIN_WHITEN_MEDIUM, strlen(effect)) == 0)
        ispEffect = ia_isp_effect_skin_whiten;
    else if (strncmp(effect, IntelCameraParameters::EFFECT_STILL_SKIN_WHITEN_HIGH, strlen(effect)) == 0)
        ispEffect = ia_isp_effect_skin_whiten_high;
    else if (strncmp(effect, IntelCameraParameters::EFFECT_VIVID, strlen(effect)) == 0)
        ispEffect = ia_isp_effect_vivid;
    else if (strncmp(effect, CameraParameters::EFFECT_NONE, strlen(effect)) != 0){
        ALOGE("Color effect not found.");
        status = -1;
        // Fall back to the effect NONE
    }
    mIspInputParams.effects = ispEffect;
    return status;
}

void AtomAIQ::setPublicAeMode(AeMode mode)
{
    LOG2("@%s, mPublicAeMode: %d", __FUNCTION__, mode);
    mPublicAeMode = mode;
}

AeMode AtomAIQ::getPublicAeMode()
{
    LOG2("@%s, mPublicAeMode: %d", __FUNCTION__, mPublicAeMode);
    return mPublicAeMode;
}

status_t AtomAIQ::startStillAf()
{
    LOG1("@%s", __FUNCTION__);
    if (mAeFlashMode != CAM_AE_FLASH_MODE_TORCH
        && mAeFlashMode != CAM_AE_FLASH_MODE_OFF) {
        mAfState.assist_light = getAfNeedAssistLight();
        if (mAfState.assist_light) {
            LOG1("Using AF assist light with auto-focus");
            mFlashCI->setTorch(TORCH_INTENSITY);
            mAeInputParameters.frame_use = ia_aiq_frame_use_still;
            if (mAeState.ae_results) {
                memcpy(&(mPreAssistLightAeResults.results), mAeState.ae_results, sizeof(ia_aiq_ae_results));
                memcpy(&(mPreAssistLightAeResults.weight_grid), mAeState.ae_results->weight_grid, sizeof(ia_aiq_hist_weight_grid));
                memcpy(&(mPreAssistLightAeResults.exposures[0]), mAeState.ae_results->exposures[0].exposure, sizeof(ia_aiq_exposure_parameters));
                memcpy(&(mPreAssistLightAeResults.sensor_exposures[0]), mAeState.ae_results->exposures[0].sensor_exposure, sizeof(ia_aiq_exposure_sensor_parameters));
                memcpy(mPreAssistLightAeResults.flash, mAeState.ae_results->flashes, sizeof(ia_aiq_flash_parameters) * mAeState.ae_results->num_flashes);
                // fixes cross-dependencies for container
                mPreAssistLightAeResults.results.weight_grid =  &mPreAssistLightAeResults.weight_grid;
                mPreAssistLightAeResults.results.exposures = mPreAssistLightAeResults.exposure_result_array;
                mPreAssistLightAeResults.results.exposures[0].exposure_index = 0;
                mPreAssistLightAeResults.results.exposures[0].exposure =  &mPreAssistLightAeResults.exposures[0];
                mPreAssistLightAeResults.results.exposures[0].sensor_exposure =  &mPreAssistLightAeResults.sensor_exposures[0];
                mPreAssistLightAeResults.results.flashes =  mPreAssistLightAeResults.flash;
                LOG1("@%s: AE results copy to pre assist results. (flash status = %d)", __FUNCTION__, mPreAssistLightAeResults.results.flashes->status);
            } else {
                LOG1("@%s: No current AE results, clean pre assist light AE result.", __FUNCTION__ );
                CLEAR(mPreAssistLightAeResults);
            }
        }
    }
    setAfFocusMode(ia_aiq_af_operation_mode_auto);
    mAfInputParameters.frame_use = ia_aiq_frame_use_still;
    mStillAfStart = systemTime();
    return NO_ERROR;
}

status_t AtomAIQ::stopStillAf()
{
    LOG1("@%s", __FUNCTION__);
    if (mAfState.assist_light) {
        LOG1("Turning off AF assist light");
        mFlashCI->setTorch(0);
        mAfState.assist_light = false;
        mAeInputParameters.frame_use = m3aState.frame_use;
    }

    if (mAfMode == CAM_AF_MODE_AUTO || mAfMode == CAM_AF_MODE_MACRO) {
        setAfFocusMode(ia_aiq_af_operation_mode_manual);
    }

    mAfInputParameters.frame_use = m3aState.frame_use;
    mStillAfStart = 0;
    return NO_ERROR;
}

AfStatus AtomAIQ::isStillAfComplete()
{
    LOG2("@%s", __FUNCTION__);
    if (mStillAfStart == 0) {
        // startStillAf wasn't called? return error
        ALOGE("Call startStillAf before calling %s!", __FUNCTION__);
        return CAM_AF_STATUS_FAIL;
    }
    AfStatus ret = getCAFStatus();
    return ret;
}

status_t AtomAIQ::getExposureInfo(SensorAeConfig& aeConfig)
{
    LOG2("@%s", __FUNCTION__);

    // evBias not reset, so not using memset
    aeConfig.expTime = 0;
    aeConfig.aperture_num = 0;
    aeConfig.aperture_denum = 1;
    aeConfig.aecApexTv = 0;
    aeConfig.aecApexSv = 0;
    aeConfig.aecApexAv = 0;
    aeConfig.digitalGain = 0;
    getAeExpCfg(&aeConfig.expTime,
            &aeConfig.aperture_num,
            &aeConfig.aperture_denum,
            &aeConfig.aecApexTv,
            &aeConfig.aecApexSv,
            &aeConfig.aecApexAv,
            &aeConfig.digitalGain,
            &aeConfig.totalGain);

    return NO_ERROR;
}

// TODO: it needed by exif, so need AIQ provide
status_t AtomAIQ::getAeManualBrightness(float *ret)
{
    LOG1("@%s", __FUNCTION__);
    return INVALID_OPERATION;
}

// Focus operations
status_t AtomAIQ::initAfBracketing(int stops, AFBracketingMode mode)
{
    LOG1("@%s", __FUNCTION__);
    mBracketingStops = stops;
    ia_aiq_af_bracket_input_params param;
    switch (mode) {
    case CAM_AF_BRACKETING_MODE_SYMMETRIC:
        param.af_bracket_mode = ia_aiq_af_bracket_mode_symmetric;
        break;
    case CAM_AF_BRACKETING_MODE_TOWARDS_NEAR:
        param.af_bracket_mode = ia_aiq_af_bracket_mode_towards_near;
        break;
    case CAM_AF_BRACKETING_MODE_TOWARDS_FAR:
        param.af_bracket_mode = ia_aiq_af_bracket_mode_towards_far;
        break;
    default:
        param.af_bracket_mode = ia_aiq_af_bracket_mode_symmetric;
    }
    param.focus_positions = (char) stops;
    //first run AF to get the af result
    runAfMain();
    memcpy(&param.af_results, mAfState.af_results, sizeof(ia_aiq_af_results));
    ia_aiq_af_bracket(m3aState.ia_aiq_handle, &param, &mAfBracketingResult);
    for(int i = 0; i < stops; i++)
        LOG1("i=%d, postion=%d", i, mAfBracketingResult->lens_positions_bracketing[i]);

    return  NO_ERROR;
}

status_t AtomAIQ::setManualFocusIncrement(int steps)
{
    LOG1("@%s", __FUNCTION__);
    status_t ret = NO_ERROR;
    if(steps >= 0 && steps < mBracketingStops) {
        int position = mAfBracketingResult->lens_positions_bracketing[steps];
        int focus_moved = mLensCI->moveFocusToPosition(position);
        if(focus_moved != 0)
            ret = UNKNOWN_ERROR;
    }
    return ret;
}

status_t AtomAIQ::initAeBracketing()
{
    LOG1("@%s", __FUNCTION__);
    mBracketingRunning = true;
    return NO_ERROR;
}

status_t AtomAIQ::deinitAeBracketing()
{
    LOG1("@%s", __FUNCTION__);
    mBracketingRunning = false;
    return NO_ERROR;
}

/**
 * Apply a group of EV value for offline exposure bracketing
 *
 * return the expected applied exposure id. -1 in error
 */
int AtomAIQ::applyEvGroup(float biases[], int depth, SensorAeConfig aeResults[])
{
    struct atomisp_exposure exposures[depth];
    if (biases == NULL || aeResults == NULL || depth <= 1) {
        ALOGE("Invalid ev group setting");
        return -1;
    }

    if (!m3aState.ia_aiq_handle) {
        ALOGE("Invalid aiq handle to apply ae group");
        return -1;
    }

    if (!mBracketingRunning) {
        ALOGE("not allowed to apply group of Ev in normal mode");
        return -1;
    } else {
        mAeBracketingInputParameters = mAeInputParameters;
        mAeBracketingInputParameters.frame_use =  m3aState.frame_use;
        mAeBracketingInputParameters.flash_mode = ia_aiq_flash_mode_off;
    }

    for (int i = 0 ; i < depth ; i++) {
        mAeBracketingInputParameters.ev_shift = biases[i];
        ia_aiq_ae_run(m3aState.ia_aiq_handle, &mAeBracketingInputParameters, &mAEBracketingResult);

        if (mAEBracketingResult != NULL) {
            exposures[i].integration_time[0] = mAEBracketingResult->exposures[0].sensor_exposure->coarse_integration_time;
            exposures[i].integration_time[1] = mAEBracketingResult->exposures[0].sensor_exposure->fine_integration_time;
            exposures[i].gain[0] = mAEBracketingResult->exposures[0].sensor_exposure->analog_gain_code_global;
            exposures[i].gain[1] = mAEBracketingResult->exposures[0].sensor_exposure->digital_gain_global;
            exposures[i].aperture = 100;

            ALOGD("@%s I%d itime0:%d itime1:%d gain0:%d gain1:%d",__FUNCTION__, i,
                    exposures[i].integration_time[0], exposures[i].integration_time[1],
                    exposures[i].gain[0], exposures[i].gain[1]);
            aeResults[i].expTime = 0;
            aeResults[i].evBias = biases[i];
            aeResults[i].aperture_num = 0;
            aeResults[i].aperture_denum = 1;
            aeResults[i].aecApexTv = -1.0 * (log10((double)(mAEBracketingResult->exposures[0].exposure)->exposure_time_us/1000000) / log10(2.0)) * 65536;
            aeResults[i].aecApexSv = log10(pow(2.0, -7.0/4.0) * (mAEBracketingResult->exposures[0].exposure)->iso) / log10(2.0) * 65536;
            aeResults[i].aecApexAv = log10(pow((mAEBracketingResult->exposures[0].exposure)->aperture_fn, 2))/log10(2.0) * 65536;
            aeResults[i].digitalGain = (mAEBracketingResult->exposures[0].exposure)->digital_gain;
            if (aeResults[i].digitalGain > float(0))
                aeResults[i].totalGain = (mAEBracketingResult->exposures[0].exposure)->analog_gain * aeResults[i].digitalGain;
            else
                aeResults[i].totalGain = (mAEBracketingResult->exposures[0].exposure)->digital_gain;
        } else {
            ALOGE("Invalid AE bracketing result");
            return -1;
        }
    }

    /* Apply ae group settings */
    return mSensorCI->setExposureGroup(exposures, depth);
}

// Exposure operations
// For exposure bracketing
status_t AtomAIQ::applyEv(float bias)
{
    LOG1("@%s: bias=%.2f", __FUNCTION__, bias);
    mAeBracketingInputParameters = mAeInputParameters;
    mAeBracketingInputParameters.ev_shift = bias;
    mAeBracketingInputParameters.frame_use =  m3aState.frame_use;
    mAeBracketingInputParameters.flash_mode = ia_aiq_flash_mode_off;
    status_t ret = NO_ERROR;
    if (m3aState.ia_aiq_handle){
        ia_aiq_ae_run(m3aState.ia_aiq_handle, &mAeBracketingInputParameters, &mAEBracketingResult);
    }
    if (mAEBracketingResult != NULL) {
        struct atomisp_exposure exposure;
        CLEAR(exposure);
        exposure.integration_time[0] = mAEBracketingResult->exposures[0].sensor_exposure->coarse_integration_time;
        exposure.integration_time[1] = mAEBracketingResult->exposures[0].sensor_exposure->fine_integration_time;
        exposure.gain[0] = mAEBracketingResult->exposures[0].sensor_exposure->analog_gain_code_global;
        exposure.gain[1] = mAEBracketingResult->exposures[0].sensor_exposure->digital_gain_global;
        exposure.aperture = 100;
        LOG2("AEC integration_time[0]: %d", exposure.integration_time[0]);
        LOG2("AEC integration_time[1]: %d", exposure.integration_time[1]);
        LOG2("AEC gain[0]: %d", exposure.gain[0]);
        LOG2("AEC gain[1]: %d", exposure.gain[1]);
        LOG2("AEC aperture: %d\n", exposure.aperture);
        /* Apply Sensor settings */
        ret |= mSensorCI->setExposure(&exposure);
    }
    return ret;
}

status_t AtomAIQ::setEv(float bias)
{
    LOG1("@%s: bias=%.2f", __FUNCTION__, bias);
    if(bias > 4 || bias < -4)
        return BAD_VALUE;
    mAeInputParameters.ev_shift = bias;

    return NO_ERROR;
}

status_t AtomAIQ::getEv(float *ret)
{
    LOG2("@%s", __FUNCTION__);
    *ret = mAeInputParameters.ev_shift;
    return NO_ERROR;
}

// TODO: need confirm if it's correct.
status_t AtomAIQ::setManualShutter(float expTime)
{
    LOG1("@%s, expTime: %f", __FUNCTION__, expTime);
    mAeInputParameters.manual_exposure_time_us = expTime * 1000000;
    return NO_ERROR;
}

// Saturation
status_t AtomAIQ::setSaturation(char saturation)
{
    LOG1("@%s - %d", __FUNCTION__, saturation);
    mIspInputParams.manual_saturation = saturation;
    return NO_ERROR;
}

// Sharpness,
status_t AtomAIQ::setSharpness(char sharpness)
{
    LOG1("@%s - %d", __FUNCTION__, sharpness)
    mIspInputParams.manual_sharpness = sharpness;
    return NO_ERROR;
}

// Contrast
status_t AtomAIQ::setContrast(char contrast)
{
    LOG1("@%s - %d", __FUNCTION__, contrast)
    mIspInputParams.manual_contrast = contrast;
    return NO_ERROR;
}

status_t AtomAIQ::setIsoMode(IsoMode mode)
{
    LOG1("@%s - %d", __FUNCTION__, mode);

    if (mode == CAM_AE_ISO_MODE_AUTO)
        mAeInputParameters.manual_iso = -1;

    return NO_ERROR;
}

status_t AtomAIQ::setManualIso(int sensitivity)
{
    LOG1("@%s - %d", __FUNCTION__, sensitivity);
    mAeInputParameters.manual_iso = sensitivity;
    return NO_ERROR;
}

status_t AtomAIQ::getManualIso(int *ret)
{
    LOG2("@%s - %d", __FUNCTION__, mAeInputParameters.manual_iso);

    status_t status = NO_ERROR;

    if (mAeInputParameters.manual_iso > 0) {
    *ret = mAeInputParameters.manual_iso;
    } else if(mAeState.ae_results && mAeState.ae_results->exposures[0].exposure) {
        // in auto iso mode result current real iso values
        *ret = mAeState.ae_results->exposures[0].exposure->iso;
    } else {
        ALOGW("no ae result available for ISO value");
        *ret = 0;
        status = UNKNOWN_ERROR;
    }

    return status;
}

status_t AtomAIQ::applyPreFlashProcess(FlashStage stage, struct timeval captureTimestamp, int orientation, uint32_t expId)
{
    LOG2("@%s", __FUNCTION__);

    status_t ret = NO_ERROR;

    // Upper layer is skipping frames for exposure delay,
    // setting feedback delay to 0.
    mAeState.feedback_delay = 0;

    if (stage == CAM_FLASH_STAGE_PRE || stage == CAM_FLASH_STAGE_MAIN)
    {
        /* Store previous state of 3A locks. */
        bool prev_af_lock = getAfLock();
        bool prev_ae_lock = getAeLock();
        bool prev_awb_lock = getAwbLock();
        mGBCEDefault = true;

        /* AF is not run during flash sequence. */
        setAfLock(true);

        /* During flash sequence AE and AWB must be enabled in order to calculate correct parameters for the final image. */
        setAeLock(false);
        setAwbLock(false);

        mAeInputParameters.frame_use = ia_aiq_frame_use_still;

        ret = apply3AProcess(true, &captureTimestamp, orientation, expId);

        mAeInputParameters.frame_use = m3aState.frame_use;

        /* Restore previous state of 3A locks. */
        setAfLock(prev_af_lock);
        setAeLock(prev_ae_lock);
        setAwbLock(prev_awb_lock);
    }
    else
    {
        ret = apply3AProcess(true, &captureTimestamp, orientation, expId);

        if (mAwbResults)
            mAwbStoredResults = *mAwbResults;
    }
    return ret;
}


status_t AtomAIQ::setFlash(int numFrames)
{
    LOG1("@%s: numFrames = %d", __FUNCTION__, numFrames);
    return mFlashCI->setFlash(numFrames);
}

status_t AtomAIQ::apply3AProcess(bool read_stats,
                                 struct timeval *frame_timestamp,
                                 int orientation, uint32_t expId)
{
    LOG2("@%s: read_stats = %d", __FUNCTION__, read_stats);
    status_t status = NO_ERROR;

    if (read_stats) {
        status = getStatistics(frame_timestamp, orientation, expId);
    }

    if (m3aState.stats_valid) {
        status |= run3aMain();
    }

    return status;
}

status_t AtomAIQ::setSmartSceneDetection(bool en)
{
    LOG1("@%s: en = %d", __FUNCTION__, en);

    m3aState.dsd_enabled = en;
    return NO_ERROR;
}

bool AtomAIQ::getSmartSceneDetection()
{
    LOG2("@%s", __FUNCTION__);
    return m3aState.dsd_enabled;
}

status_t AtomAIQ::getSmartSceneMode(String8 &sceneMode, bool &sceneHdr)
{
    LOG1("@%s", __FUNCTION__);

    switch (mDetectedSceneMode) {
    case ia_aiq_scene_mode_none:
        sceneMode.setTo("auto");
        break;
    case ia_aiq_scene_mode_close_up_portrait:
        sceneMode.setTo("close_up_portrait");
        break;
    case ia_aiq_scene_mode_portrait:
        sceneMode.setTo("portrait");
        break;
    case ia_aiq_scene_mode_lowlight_portrait:
        sceneMode.setTo("night_portrait");
        break;
    case ia_aiq_scene_mode_low_light:
        sceneMode.setTo("night");
        break;
    case ia_aiq_scene_mode_action:
        sceneMode.setTo("action");
        break;
    case ia_aiq_scene_mode_backlight:
        sceneMode.setTo("backlight");
        break;
    case ia_aiq_scene_mode_landscape:
        sceneMode.setTo("landscape");
        break;
    case ia_aiq_scene_mode_document:
        sceneMode.setTo("document");
        break;
    case ia_aiq_scene_mode_firework:
        sceneMode.setTo("firework");
        break;
    case ia_aiq_scene_mode_lowlight_action:
        sceneMode.setTo("lowlight_action");
        break;
    case ia_aiq_scene_mode_baby:
        sceneMode.setTo("baby");
        break;
    case ia_aiq_scene_mode_barcode:
        sceneMode.setTo("barcode");
        break;
    default:
        ALOGW("Unhandled detected scene mode: 0x%x", mDetectedSceneMode);
        sceneMode.setTo("auto");
        break;
   }

   sceneHdr = (mAeState.ae_results->multiframe & ia_aiq_bracket_mode_hdr) ? true : false;
   LOG1("scene detected: %s - hdr hint: %d", sceneMode.string(), sceneHdr);
   return NO_ERROR;
}

status_t AtomAIQ::setFaces(const ia_face_state& faceState)
{
    LOG2("@%s", __FUNCTION__);

    m3aState.faces->num_faces = faceState.num_faces;
    if(m3aState.faces->num_faces > IA_AIQ_MAX_NUM_FACES)
        m3aState.faces->num_faces = IA_AIQ_MAX_NUM_FACES;

    /*ia_aiq assumes that the faces are ordered in the order of importance*/
    memcpy(m3aState.faces->faces, faceState.faces, m3aState.faces->num_faces*sizeof(ia_face));

    return NO_ERROR;
}

ia_binary_data *AtomAIQ::get3aMakerNote(ia_mkn_trg mknMode)
{
    LOG2("@%s", __FUNCTION__);
    ia_mkn_trg mknTarget = ia_mkn_trg_section_1;

    ia_binary_data *makerNote;
    makerNote = (ia_binary_data *)malloc(sizeof(ia_binary_data));
    if (!makerNote) {
        ALOGE("Error allocation memory for mknote!");
        return NULL;
    }
    // detailed makernote data for RAW images
    if(mknMode == ia_mkn_trg_section_2)
        mknTarget = ia_mkn_trg_section_2;
    ia_binary_data mkn_binary_data = ia_mkn_prepare(mMkn, mknTarget);

    makerNote->size = mkn_binary_data.size;
    makerNote->data = (char*)malloc(makerNote->size);
    if (makerNote->data)
    {
        memcpy(makerNote->data, mkn_binary_data.data, makerNote->size);
    } else {
        ALOGE("Error allocation memory for mknote data!");
        free(makerNote);
        return NULL;
    }
    return makerNote;
}

void AtomAIQ::put3aMakerNote(ia_binary_data *mknData)
{
    LOG2("@%s", __FUNCTION__);

    if (mknData) {
        if (mknData->data) {
            free(mknData->data);
            mknData->data = NULL;
        }
        free(mknData);
        mknData = NULL;
    }
}

void AtomAIQ::reset3aMakerNote(void)
{
    LOG2("@%s", __FUNCTION__);
    ia_mkn_reset(mMkn);
}

int AtomAIQ::add3aMakerNoteRecord(ia_mkn_dfid mkn_format_id,
                                  ia_mkn_dnid mkn_name_id,
                                  const void *record,
                                  unsigned short record_size)
{
    LOG2("@%s", __FUNCTION__);

    if(record != NULL)
        ia_mkn_add_record(mMkn, mkn_format_id, mkn_name_id, record, record_size, NULL);

    return 0;
}

void AtomAIQ::get3aGridInfo(struct atomisp_grid_info *pgrid)
{
    LOG2("@%s", __FUNCTION__);
    *pgrid = m3aState.results.isp_params.info;
}


status_t AtomAIQ::getGridWindow(AAAWindowInfo& window)
{
    struct atomisp_grid_info gridInfo;
    CLEAR(gridInfo);

    // Get the 3A grid info
    get3aGridInfo(&gridInfo);

    // This is how the 3A library defines the statistics grid window measurements
    // BQ = bar-quad = 2x2 pixels
    window.width = gridInfo.s3a_width * gridInfo.s3a_bqs_per_grid_cell * 2;
    window.height = gridInfo.s3a_height * gridInfo.s3a_bqs_per_grid_cell * 2;

    return NO_ERROR;
}

struct atomisp_3a_statistics * AtomAIQ::allocateStatistics(int grid_size)
{
    LOG2("@%s", __FUNCTION__);
    struct atomisp_3a_statistics *stats;
    stats = (atomisp_3a_statistics*)malloc(sizeof(*stats));
    if (!stats)
        return NULL;

    stats->data = (atomisp_3a_output*)malloc(grid_size * sizeof(*stats->data));
    if (!stats->data) {
        free(stats);
        stats = NULL;
        return NULL;
    }
    LOG2("@%s success", __FUNCTION__);
    return stats;
}

void AtomAIQ::freeStatistics(struct atomisp_3a_statistics *stats)
{
    if (stats) {
        if (stats->data) {
            free(stats->data);
            stats->data = NULL;
        }
        free(stats);
        stats = NULL;
    }
}

/**
 * Helper method to store AE results into mAeState.stored_results
 *
 * mAeState.stored_results is a fixed depth fifo queue. This method
 * pushes new results into fifo and automatically dequeues the oldest
 * when fifo is full. In case updateIdx argument is given, a specific
 * index in fifo is updated instead of normal operation.
 *
 * Why results are stored ?
 *
 * AEC requires userspace synching solution to workaround the fact that
 * lower level cannot reliably map statistics to a frame and that sensor
 * metadata is not available through the pipeline. Nevertheless, AEC needs
 * input parameters containing its own results from history as a feedback.
 * This feedback needs to correspond to parameters active in sensor for a
 * frame wherefrom the statistics get read.
 *
 * AIQ API expects to receive feedback as a compound struct ia_aiq_ae_results,
 * combining 5 result structures (ae_results, weight_grid, exposure,
 * sensor_exposure and flash). New compound struct stored_ae_results is
 * defined to be able to keep the history in a generic container (AtomFifo).
 *
 * Original ae_results structure contains pointers to corresponding
 * weight_grid, exposure, sensor_exposure and flash structures. Helper
 * methods storeAeResults(), peekAeStoredResults() and pickAeFeedbackResults()
 * are provided to consolidate the related memcpy's and pointer operations.
 *
 * Sensor specific latency (currently AE_DELAY_FRAMES) and the timing of
 * applying results will specify the number of history results we need to
 * retain in memory for delayed feedback results (mAeState.feedback_delay).
 *
 * \see #peekAeStoredResults()
 * \see #pickAeFeedbackResults()
 *
 * \param ae_results AE results to be stored
 * \param updateIdx offset to be updated instead of pushing (default -1 == off)
 * \return pointer to stored structure (a copy of ae_results in fifo)
 */
ia_aiq_ae_results* AtomAIQ::storeAeResults(ia_aiq_ae_results *ae_results, int updateIdx)
{
    LOG2("@%s", __FUNCTION__);
    stored_ae_results new_stored_results_element;
    stored_ae_results *new_stored_results = &new_stored_results_element;
    if (mAeState.stored_results == NULL)
        return NULL;

    if (updateIdx >= 0) {
        // instead of dropping and placing new from stack,
        // replace the head when requested
        new_stored_results = mAeState.stored_results->peek(updateIdx);
        if (new_stored_results == NULL)
            return NULL;
    }

    if (ae_results != NULL) {
        ia_aiq_ae_results *store_results = &(new_stored_results->results);
        memcpy(store_results, ae_results, sizeof(ia_aiq_ae_results));
        memcpy(&(new_stored_results->weight_grid), ae_results->weight_grid, sizeof(ia_aiq_hist_weight_grid));
        memcpy(&(new_stored_results->exposures[0]), ae_results->exposures[0].exposure, sizeof(ia_aiq_exposure_parameters));
        memcpy(&(new_stored_results->sensor_exposures[0]), ae_results->exposures[0].sensor_exposure, sizeof(ia_aiq_exposure_sensor_parameters));
        memcpy(new_stored_results->flash, ae_results->flashes, sizeof(ia_aiq_flash_parameters) * ae_results->num_flashes);
        // clean cross-dependencies for container
        store_results->weight_grid = NULL;
        store_results->exposures = NULL;
        store_results->flashes = NULL;
    } else {
        memset(new_stored_results, 0, sizeof(stored_ae_results));
    }

    if (updateIdx < 0) {
        // when fifo is full drop the oldest
        if (mAeState.stored_results->getCount() >= mAeState.stored_results->getDepth())
            mAeState.stored_results->dequeue();

        int ret = mAeState.stored_results->enqueue(*new_stored_results);
        if (ret != 0)
            return NULL;
    }

    return peekAeStoredResults(0);
}

/**
 * Helper method to peek AE results from mAeState.stored_results
 *
 * \see #storeAeResults()
 *
 * \param offset to pick results from history (0 - latest)
 * \return pointer to results structure in fifo
 */
ia_aiq_ae_results* AtomAIQ::peekAeStoredResults(unsigned int offset)
{
    LOG2("@%s, offset %d", __FUNCTION__, offset);
    ia_aiq_ae_results *ae_results;
    if (!mAeState.stored_results)
        return NULL;

    stored_ae_results *stored_results_element = mAeState.stored_results->peek(offset);
    if (stored_results_element == NULL) {
        LOG1("%s: No results with offset %d", __FUNCTION__, offset);
        return NULL;
    }

    ae_results = &stored_results_element->results;

    // restore cross-dependencies
    ae_results->weight_grid = &stored_results_element->weight_grid;
    ae_results->exposures = stored_results_element->exposure_result_array;
    ae_results->exposures[0].exposure_index = 0;
    ae_results->exposures[0].exposure = &stored_results_element->exposures[0];
    ae_results->exposures[0].sensor_exposure = &stored_results_element->sensor_exposures[0];
    ae_results->flashes = stored_results_element->flash;

    return ae_results;
}

/**
 * Helper method to pick copy of stored AE results from mAeState.stored_results
 * into mAeState.feedback_results passed as feedback with getStatistics() to
 * AEC.
 *
 * \see #storeAeResults()
 * \return pointer to mAeState.feedback_results.results
 */
ia_aiq_ae_results* AtomAIQ::pickAeFeedbackResults()
{
    LOG2("@%s, delay %d", __FUNCTION__, mAeState.feedback_delay);
    ia_aiq_ae_results *feedback_results = &mAeState.feedback_results.results;
    ia_aiq_ae_results *stored_results = peekAeStoredResults(mAeState.feedback_delay);
    if (stored_results == NULL) {
        ALOGE("Failed to pick AE results from history, delay %d", mAeState.feedback_delay);
        return NULL;
    } else {
        LOG2("Picked AE results from history, delay %d, depth %d",
                mAeState.feedback_delay, mAeState.stored_results->getCount());
        LOG2("Feedback AEC integration_time[0]: %d",
                stored_results->exposures[0].sensor_exposure->coarse_integration_time);
        LOG2("Feedback AEC integration_time[1]: %d",
                stored_results->exposures[0].sensor_exposure->fine_integration_time);
        LOG2("Feedback AEC gain[0]: %x",
                stored_results->exposures[0].sensor_exposure->analog_gain_code_global);
        LOG2("Feedback AEC gain[1]: %x",
                stored_results->exposures[0].sensor_exposure->digital_gain_global);

        memcpy(feedback_results, stored_results, sizeof(ia_aiq_ae_results));
        feedback_results->weight_grid = &mAeState.feedback_results.weight_grid;
        feedback_results->exposures = mAeState.feedback_results.exposure_result_array;
        feedback_results->exposures[0].exposure_index = 0;
        feedback_results->exposures[0].exposure = &mAeState.feedback_results.exposures[0];
        feedback_results->exposures[0].sensor_exposure = &mAeState.feedback_results.sensor_exposures[0];
        feedback_results->flashes = mAeState.feedback_results.flash;
        feedback_results->num_exposures = NUM_EXPOSURES;
        memcpy(feedback_results->weight_grid, stored_results->weight_grid, sizeof(ia_aiq_hist_weight_grid));
        memcpy(feedback_results->exposures[0].exposure, stored_results->exposures[0].exposure, sizeof(ia_aiq_exposure_parameters));
        memcpy(feedback_results->exposures[0].sensor_exposure, stored_results->exposures[0].sensor_exposure, sizeof(ia_aiq_exposure_sensor_parameters));
        memcpy(feedback_results->flashes, stored_results->flashes, sizeof(ia_aiq_flash_parameters) * stored_results->num_flashes);
        return feedback_results;
    }
}

/**
 * Convert AIQ sensor_exposure to atomisp exposure and apply
 */
int AtomAIQ::applyExposure(ia_aiq_exposure_sensor_parameters *sensor_exposure)
{
    int ret = 0;
    // create struct atomisp_exposure for sensor interface
    struct atomisp_exposure atomispExposure;
    CLEAR(atomispExposure);

    atomispExposure.integration_time[0] = sensor_exposure->coarse_integration_time;
    atomispExposure.integration_time[1] = sensor_exposure->fine_integration_time;
    atomispExposure.gain[0] = sensor_exposure->analog_gain_code_global;
    atomispExposure.gain[1] = sensor_exposure->digital_gain_global;
    atomispExposure.aperture = 100;

    LOG2("AEC integration_time[0]: %d", atomispExposure.integration_time[0]);
    LOG2("AEC integration_time[1]: %d", atomispExposure.integration_time[1]);
    LOG2("AEC gain[0]: %d", atomispExposure.gain[0]);
    LOG2("AEC gain[1]: %d", atomispExposure.gain[1]);
    LOG2("AEC aperture: %d\n", atomispExposure.aperture);

    /* Apply Sensor settings as exposure changes*/
    ret = mSensorCI->setExposure(&atomispExposure);
    if (ret != 0) {
        ALOGE("Exposure applying failed");
    }

    return ret;
}

int AtomAIQ::run3aInit()
{
    LOG1("@%s", __FUNCTION__);
    CLEAR(m3aState.curr_grid_info);
    CLEAR(m3aState.rgbs_grid);
    CLEAR(m3aState.af_grid);

    m3aState.faces = (ia_face_state*)malloc(sizeof(ia_face_state) + IA_AIQ_MAX_NUM_FACES*sizeof(ia_face));
    if (m3aState.faces) {
        m3aState.faces->num_faces = 0;
        m3aState.faces->faces = (ia_face*)((char*)m3aState.faces + sizeof(ia_face_state));
    }
    else
        return -1;

    resetAFParams();
    mAfState.af_results = NULL;
    CLEAR(mAeCoord);
    CLEAR(mAeState);
    if (mSensorCI == NULL)
        return -1;
    unsigned int store_size = getExposureDelay() + 1; /* max delay + current results */
    mAeState.stored_results = new AtomFifo<stored_ae_results>(store_size);
    if (mAeState.stored_results == NULL)
        return -1;
    LOG1("@%s: keeping %d AE history results stored", __FUNCTION__, store_size);
    resetAECParams();
    resetAWBParams();
    mAwbResults = NULL;
    resetGBCEParams();
    resetDSDParams();

    return 0;
}

/* returns false for error, true for success */
bool AtomAIQ::changeSensorMode(void)
{
    LOG1("@%s", __FUNCTION__);

    /* Get new sensor frame params needed by AIC for LSC calculation. */
    getSensorFrameParams(&m3aState.sensor_frame_params);

    struct atomisp_sensor_mode_data sensor_mode_data;
    CLEAR(sensor_mode_data);

    if (mSensorCI->getModeInfo(&sensor_mode_data) < 0) {
        ALOGE("Get mode info failed");
        return false;
    }

    if (sensor_mode_data.binning_factor_y == 0) {
        ALOGE("error: binning factor y is zero!");
        return false;
    }

    if (mISP->getIspParameters(&m3aState.results.isp_params) < 0) {
        ALOGE("get isp parameters failed");
        return false;
    }

    struct morph_table *gdc_table = getGdcTable(sensor_mode_data.output_width, sensor_mode_data.output_height);
    if (gdc_table) {
        LOG1("Initialise gdc_table size %d x %d ", gdc_table->width, gdc_table->height);
        mISP->setGdcConfig(gdc_table);
        mISP->setGDC(true);
        freeGdcTable(gdc_table);
    }
    else {
        LOG1("Empty GDC table -> GDC disabled");
        mISP->setGDC(false);
    }

    /* Reconfigure 3A grid */
    ia_aiq_exposure_sensor_descriptor *sd = &mAeSensorDescriptor;
    sd->pixel_clock_freq_mhz = sensor_mode_data.vt_pix_clk_freq_mhz/1000000.0f;
    sd->pixel_periods_per_line = sensor_mode_data.line_length_pck;
    sd->line_periods_per_field = sensor_mode_data.frame_length_lines;
    sd->line_periods_vertical_blanking = sensor_mode_data.frame_length_lines
            - (sensor_mode_data.crop_vertical_end - sensor_mode_data.crop_vertical_start + 1)
            / sensor_mode_data.binning_factor_y;
    sd->fine_integration_time_min = sensor_mode_data.fine_integration_time_def;
    sd->fine_integration_time_max_margin = sensor_mode_data.line_length_pck - sensor_mode_data.fine_integration_time_def;
    sd->coarse_integration_time_min = sensor_mode_data.coarse_integration_time_min;
    sd->coarse_integration_time_max_margin = sensor_mode_data.coarse_integration_time_max_margin;

    LOG2("sensor_descriptor assign complete:");

    LOG2("sensor descriptor: pixel_clock_freq_mhz %f", sd->pixel_clock_freq_mhz);
    LOG2("sensor descriptor: pixel_periods_per_line %d", sd->pixel_periods_per_line);
    LOG2("sensor descriptor: line_periods_per_field %d", sd->line_periods_per_field);
    LOG2("sensor descriptor: coarse_integration_time_min %d", sd->coarse_integration_time_min);
    LOG2("sensor descriptor: coarse_integration_time_max_margin %d", sd->coarse_integration_time_max_margin);
    LOG2("sensor descriptor: binning_factor_y %d", sensor_mode_data.binning_factor_y);

    m3aState.lsc_update = true;

    if (m3aState.stats)
        freeStatistics(m3aState.stats);

    m3aState.curr_grid_info = m3aState.results.isp_params.info;
    int grid_size = m3aState.curr_grid_info.s3a_width * m3aState.curr_grid_info.s3a_height;
    m3aState.stats = allocateStatistics(grid_size);
    if (m3aState.stats != NULL) {
        m3aState.stats->grid_info = m3aState.curr_grid_info;
        m3aState.stats_valid  = false;
    } else {
        ALOGE("Statistics memory allocation failed");
        return false;
    }

    return true;
}

/*
 * \brief Dequeues 3A statistics from the ISP
 *
 * This function dequeues the statistics from the ISP and stores them internally
 * to the AtomAIQ object.
 *
 * This method can also be used by outside callers to synchronize
 * the statistics, for example in pre-flash sequence, if needed.
 */
status_t AtomAIQ::dequeueStatistics()
{
    status_t ret = NO_ERROR;
    ret = mISP->getIspStatistics(m3aState.stats);

    if (ret == -EAGAIN) {
        ALOGV("buffer for isp statistics reallocated according resolution changing\n");
        if (changeSensorMode() == false)
            ALOGE("error in calling changeSensorMode()\n");
        ret = mISP->getIspStatistics(m3aState.stats);
    }

    if (m3aState.stats) {
        LOG2("m3aState.stats: grid_info: %d  %d %d ",
              m3aState.stats->grid_info.s3a_width,m3aState.stats->grid_info.s3a_height,m3aState.stats->grid_info.s3a_bqs_per_grid_cell);
    }

    return ret;
}

status_t AtomAIQ::getStatistics(const struct timeval *frame_timestamp_struct, int orientation, uint32_t expId)
{
    LOG2("@%s", __FUNCTION__);
    status_t ret = NO_ERROR;

    PERFORMANCE_TRACES_AAA_PROFILER_START();

    ret = dequeueStatistics();

    if (ret != NO_ERROR) {
        ALOGE("Error dequeuing statistics: %d", ret);
    } else if (!m3aState.stats) {
        ALOGE("Statistics dequeued; returned NULL statistics.");
    }

    while (m3aState.stats && expId != EXPOSURE_ID_NOT_DEFINED && expId != m3aState.stats->exp_id) {
        // Get statistics from the driver stats queue, until the one
        // matching to the requested ID is gotten.
        // This operation is needed in pre-flash sequence, in which the stats handling
        // is skipped in AAAThread::handleMessageNewStats(). The stats will then pile up
        // in the driver queue, and we will fall out of sync with the stats. This is how we ensure
        // getting the most recent stats, matching to the expId requested (the expId of the preview frame)
        ret = dequeueStatistics();

        if (!m3aState.stats) {
            ALOGE("Statistics dequeued; returned NULL statistics, ret: %d, requested expId: %u.", ret, expId);
        } else if (ret != NO_ERROR) {
            ALOGW("Error dequeuing requested statistics with expId %u. We have %u", expId, m3aState.stats->exp_id);
        } else {
            LOG2("Finding matching stats: requested expId: %u, stats expId: %u - %s",
                 expId,  m3aState.stats->exp_id, (expId == m3aState.stats->exp_id) ? "MATCH": "");
        }
    }

    PERFORMANCE_TRACES_AAA_PROFILER_STOP();

    if (ret == 0)
    {
        ia_err err = ia_err_none;
        ia_aiq_statistics_input_params statistics_input_parameters;
        ia_aiq_af_results frame_af_parameters;
        CLEAR(statistics_input_parameters);
        statistics_input_parameters.rgbs_grids = mRgbsGridArray;
        statistics_input_parameters.num_rgbs_grids = NUM_EXPOSURES;
        statistics_input_parameters.af_grids = mAfGridArray;
        statistics_input_parameters.num_af_grids = NUM_EXPOSURES;

        statistics_input_parameters.frame_timestamp = TIMEVAL2USECS(frame_timestamp_struct);
        statistics_input_parameters.frame_id = statistics_input_parameters.frame_timestamp + 1;

        statistics_input_parameters.frame_af_parameters = NULL;
        statistics_input_parameters.external_histograms = NULL;
        statistics_input_parameters.num_external_histograms = 0;
        statistics_input_parameters.awb_results = NULL;

        if(m3aState.faces)
            statistics_input_parameters.faces = m3aState.faces;

        if(mPaResults)
            statistics_input_parameters.frame_pa_parameters = mPaResults;

        if (mAeState.ae_results) {
            statistics_input_parameters.frame_ae_parameters = pickAeFeedbackResults();
        }

        //update the exposure params with the sensor metadata
        if (statistics_input_parameters.frame_ae_parameters) {
            unsigned int exp_id = 0;
            if (m3aState.stats)
                exp_id = m3aState.stats->exp_id;
            mISP->getDecodedExposureParams(statistics_input_parameters.frame_ae_parameters->exposures[0].sensor_exposure,
                                           statistics_input_parameters.frame_ae_parameters->exposures[0].exposure,
                                           exp_id);
        }

        if (mAfState.af_results) {
            // pass AF results as AEC input during still AF, AIQ will
            // internally let AEC to converge to assist light
            frame_af_parameters = *mAfState.af_results;
            frame_af_parameters.use_af_assist = mAfState.assist_light;
            statistics_input_parameters.frame_af_parameters = &frame_af_parameters;
            LOG2("AF assist light %s", (frame_af_parameters.use_af_assist) ? "on":"off");
        }

        // TODO: take into account camera mount orientation. AIQ needs device orientation to handle statistics.
        switch (orientation) {
        case 0:
            statistics_input_parameters.camera_orientation = ia_aiq_camera_orientation_rotate_0;
            break;
        case 90:
            statistics_input_parameters.camera_orientation = ia_aiq_camera_orientation_rotate_90;
            break;
        case 180:
            statistics_input_parameters.camera_orientation = ia_aiq_camera_orientation_rotate_180;
            break;
        case 270:
            statistics_input_parameters.camera_orientation = ia_aiq_camera_orientation_rotate_270;
            break;
        default:
            statistics_input_parameters.camera_orientation = ia_aiq_camera_orientation_unknown;
        }

        LOG2("@%s: statistics_input_parameters.camera_orientation = %d", __FUNCTION__,  statistics_input_parameters.camera_orientation);

        ret = mISPAdaptor->convertIspStatistics(m3aState.stats,
                                                const_cast<ia_aiq_rgbs_grid**>(&statistics_input_parameters.rgbs_grids[0]),
                                                const_cast<ia_aiq_af_grid**>(&statistics_input_parameters.af_grids[0]));

        if (ret == ia_err_none) {
            LOG2("rgb_grid: grid_width:%u, grid_height:%u, thr_r:%u, thr_gr:%u,thr_gb:%u, thr_b:%u",
                  statistics_input_parameters.rgbs_grids[0]->grid_width,
                  statistics_input_parameters.rgbs_grids[0]->grid_height,
                  statistics_input_parameters.rgbs_grids[0]->blocks_ptr->avg_r,
                  statistics_input_parameters.rgbs_grids[0]->blocks_ptr->avg_gr,
                  statistics_input_parameters.rgbs_grids[0]->blocks_ptr->avg_gb,
                  statistics_input_parameters.rgbs_grids[0]->blocks_ptr->avg_b);

            if(mCustomExposureWeightGrid.weights != NULL && statistics_input_parameters.frame_ae_parameters != NULL) {
                ((ia_aiq_ae_results*)statistics_input_parameters.frame_ae_parameters)->weight_grid = &mCustomExposureWeightGrid;
            }

            if(mAwbResults)
                statistics_input_parameters.awb_results = mAwbResults;

            err = ia_aiq_statistics_set(m3aState.ia_aiq_handle, &statistics_input_parameters);

            m3aState.stats_valid = true;
        }
    }

    return ret;
}

void AtomAIQ::setAfFocusMode(ia_aiq_af_operation_mode mode)
{
    mAfInputParameters.focus_mode = mode;
}

void AtomAIQ::setAfFocusRange(ia_aiq_af_range range)
{
    mAfInputParameters.focus_range = range;
}

void AtomAIQ::setAfMeteringMode(ia_aiq_af_metering_mode mode)
{
    mAfInputParameters.focus_metering_mode = mode;
}

void AtomAIQ::resetAFParams()
{
    LOG2("@%s", __FUNCTION__);
    mAfInputParameters.focus_mode = ia_aiq_af_operation_mode_auto;
    mAfInputParameters.focus_range = ia_aiq_af_range_extended;
    mAfInputParameters.focus_metering_mode = ia_aiq_af_metering_mode_auto;
    mAfInputParameters.flash_mode = ia_aiq_flash_mode_auto;

    mAfInputParameters.focus_rect = &mAfState.focus_rect;
    mAfInputParameters.focus_rect->height = 0;
    mAfInputParameters.focus_rect->width = 0;
    mAfInputParameters.focus_rect->left = 0;
    mAfInputParameters.focus_rect->top = 0;
    mAfInputParameters.frame_use = m3aState.frame_use;
    mAfInputParameters.lens_position = 0;

    mAfInputParameters.manual_focus_parameters = &mAfState.focus_parameters;
    mAfInputParameters.manual_focus_parameters->manual_focus_action = ia_aiq_manual_focus_action_none;
    mAfInputParameters.manual_focus_parameters->manual_focus_distance = 500;
    mAfInputParameters.manual_focus_parameters->manual_lens_position = 0;
    mAfInputParameters.trigger_new_search = false;

    mAfState.af_locked = false;
    mAfState.aec_locked = false;
    mAfState.previous_sof = 0;
}

void AtomAIQ::setManualFocusParameters(ia_aiq_manual_focus_parameters focusParameters)
{
    LOG2("@%s", __FUNCTION__);
    if (mAfMode == CAM_AF_MODE_MANUAL) {
        mAfInputParameters.manual_focus_parameters->manual_focus_action = focusParameters.manual_focus_action;
        mAfInputParameters.manual_focus_parameters->manual_focus_distance = focusParameters.manual_focus_distance;
    }
}

status_t AtomAIQ::runAfMain()
{
    LOG2("@%s", __FUNCTION__);
    status_t ret = NO_ERROR;

    if (mAfState.af_locked)
        return ret;

    ia_err err = ia_err_none;

    LOG2("@af window = (%d,%d,%d,%d)",mAfInputParameters.focus_rect->height,
                 mAfInputParameters.focus_rect->width,
                 mAfInputParameters.focus_rect->left,
                 mAfInputParameters.focus_rect->top);

    if(m3aState.ia_aiq_handle)
        err = ia_aiq_af_run(m3aState.ia_aiq_handle, &mAfInputParameters, &mAfState.af_results);

    ia_aiq_af_results* af_results_ptr = mAfState.af_results;

    /* Move the lens to the required lens position */
    LOG2("lens_driver_action:%d", af_results_ptr->lens_driver_action);
    if (err == ia_err_none && af_results_ptr->lens_driver_action == ia_aiq_lens_driver_action_move_to_unit)
    {
        LOG2("next lens position:%d", af_results_ptr->next_lens_position);
        ret = mLensCI->moveFocusToPosition(af_results_ptr->next_lens_position);
        if (ret == NO_ERROR)
        {
            clock_gettime(CLOCK_MONOTONIC, &m3aState.lens_timestamp);
            mAfInputParameters.lens_movement_start_timestamp = (unsigned long long)((m3aState.lens_timestamp.tv_sec*1000000000LL + m3aState.lens_timestamp.tv_nsec)/1000LL);
            mAfInputParameters.lens_position = af_results_ptr->next_lens_position; /*Assume that the lens has moved to the requested position*/
        }
    }

    return ret;
}

void AtomAIQ::resetAECParams()
{
    LOG2("@%s", __FUNCTION__);
    mAeMode = CAM_AE_MODE_NOT_SET;

    mAeInputParameters.frame_use = m3aState.frame_use;
    mAeInputParameters.flash_mode = ia_aiq_flash_mode_auto;
    setAeOperationModeAutoOrUll();
    mAeInputParameters.metering_mode = ia_aiq_ae_metering_mode_evaluative;
    mAeInputParameters.priority_mode = ia_aiq_ae_priority_mode_normal;
    mAeInputParameters.flicker_reduction_mode = ia_aiq_ae_flicker_reduction_auto;
    mAeInputParameters.sensor_descriptor = &mAeSensorDescriptor;
    mAeInputParameters.exposure_window = NULL; // TODO: exposure window should be used with digital zoom.
    mAeInputParameters.exposure_coordinate = NULL;
    mAeInputParameters.ev_shift = 0;
    mAeInputParameters.manual_exposure_time_us = -1;
    mAeInputParameters.manual_analog_gain = -1;
    mAeInputParameters.manual_iso = -1;
    mAeInputParameters.manual_limits = &mAeManualLimits;
    mAeInputParameters.manual_limits->manual_exposure_time_min = -1;
    mAeInputParameters.manual_limits->manual_exposure_time_max = -1;
    mAeInputParameters.manual_limits->manual_frame_time_us_min = -1;
    mAeInputParameters.manual_limits->manual_frame_time_us_max = -1;
    mAeInputParameters.manual_limits->manual_iso_min = -1;
    mAeInputParameters.manual_limits->manual_iso_max = -1;
    mAeInputParameters.aec_features = NULL; // Using AEC feature definitions from CPF
    mAeInputParameters.num_exposures = NUM_EXPOSURES;
}

void AtomAIQ::setAeOperationModeAutoOrUll()
{
    if (mUllEnabled) {
        mAeInputParameters.operation_mode = ia_aiq_ae_operation_mode_ultra_low_light;
    } else {
        mAeInputParameters.operation_mode = ia_aiq_ae_operation_mode_automatic;
    }
}

void AtomAIQ::setTorchHelper(int intensity)
{
    LOG1("@%s intensity:%d", __FUNCTION__, intensity);

    if (mFlashCount == 0)
        return;

    // split intensity between leds
    int splitIntensity = intensity / mFlashCount;
    for (int i = 0; i < mFlashCount; i++)
        mFlashCI->setTorch(splitIntensity, i);
}

status_t AtomAIQ::runAeMain()
{
    LOG2("@%s", __FUNCTION__);

    status_t ret = NO_ERROR;
    bool invalidated = false;

    if (mAeState.ae_results == NULL) {
        LOG2("AEC invalidated, frame_use %d", mAeInputParameters.frame_use);
        // Note: Invalidation means different behaviour in still and preview
        //  - In preview, invalidation will flush the AEC results history
        //    (mAeState.feedback_results) and fill it with new results.
        //    (this is meant to happen in mode switches)
        //
        //  - In still, runAeMain() keeps updating the latest results only
        //    while previous preview results are kept in history for later
        //    restoring when mode is switched back to preview.
        //
        //  When invalidation occurs with stored history results, stored
        //  results are picked and re-used to re-calculate the corresponding
        //  sensor exposure. (this is meant for re-calculating for changed
        //  sensor settings in mode switch and to always start with previously
        //  known good values)
        invalidated = true;
    }

    // AE locked, do nothing
    if (!invalidated && mAeState.ae_locked)
        return ret;

    // ToDo:
    // More intelligent handling of ae_lock should be implemented:

    ia_err err = ia_err_none;
    ia_aiq_ae_results *new_ae_results = NULL;

    if (mAfState.assist_light && m3aState.stats_valid && getCAFStatus() != CAM_AF_STATUS_BUSY) {
        // In case assist light is turned off, return ae state before assist light turn on
        invalidated = true;
        new_ae_results = &mPreAssistLightAeResults.results;
    } else if (m3aState.ia_aiq_handle) {
        LOG2("AEC manual_exposure_time_us: %d manual_analog_gain: %f manual_iso: %d", mAeInputParameters.manual_exposure_time_us, mAeInputParameters.manual_analog_gain, mAeInputParameters.manual_iso);
        LOG2("AEC sensor_descriptor ->line_periods_per_field: %d", mAeInputParameters.sensor_descriptor->line_periods_per_field);
        LOG2("AEC mAeInputParameters.frame_use: %d",mAeInputParameters.frame_use);

        err = ia_aiq_ae_run(m3aState.ia_aiq_handle, &mAeInputParameters, &new_ae_results);
        LOG2("@%s result: %d", __FUNCTION__, err);
    }

    if (new_ae_results != NULL) {
        // always apply when invalidated
        bool apply_exposure = invalidated;
        bool apply_flash_intensity = invalidated;
        bool update_results_history = (mAeInputParameters.frame_use != ia_aiq_frame_use_still || mAfState.assist_light);

        LOG2("AEC %s", new_ae_results->exposures[0].converged ? "converged":"converging");

        // Fill history with these values when invalidated
        if (invalidated && update_results_history) {
            /*
             * Fill AE results history with first AE results because there is no AE delay in the beginning OR
             * Fill AE results history with first AE results because there is no AE delay after mode change (handled with 'first_run' flag - see switchModeAndRate()) OR
             * Fill AE results history with flash AE results because flash process skips partially illuminated frames removing the AE delay.
             */
            for (unsigned int i = 0; i < mAeState.stored_results->getDepth(); i++)
                storeAeResults(new_ae_results);
        }

        // when there's previous values, compare and apply only if changed
        if (mAeState.ae_results) {
            // Compare sensor exposure parameters instead of generic exposure parameters to take into account mode changes when exposure time doesn't change but sensor parameters do change.
            if (!apply_exposure) {
                ia_aiq_exposure_sensor_parameters *prev_exposure = mAeState.ae_results->exposures[0].sensor_exposure;
                ia_aiq_exposure_sensor_parameters *new_exposure = new_ae_results->exposures[0].sensor_exposure;
                apply_exposure =
                    (prev_exposure->coarse_integration_time != new_exposure->coarse_integration_time
                    || prev_exposure->fine_integration_time != new_exposure->fine_integration_time
                    || prev_exposure->digital_gain_global != new_exposure->digital_gain_global
                    || prev_exposure->analog_gain_code_global != new_exposure->analog_gain_code_global);
            }

            // Compare flash
            if (!apply_flash_intensity) {
                // TODO: Verify that checking the power change is enough.
                // Should status be checked (rer/pre/main).
                ia_aiq_flash_parameters *prev_flash = mAeState.ae_results->flashes;
                ia_aiq_flash_parameters *new_flash = new_ae_results->flashes;
                for (unsigned int i = 0; i < mAeState.ae_results->num_flashes; i++) {
                    if (prev_flash[i].power_prc != new_flash[i].power_prc) {
                        apply_flash_intensity = true;
                        break;
                    }
                }
            }
        }

        /* Apply Sensor settings */
        if (apply_exposure)
            ret |= applyExposure(new_ae_results->exposures[0].sensor_exposure);

        /* Apply Flash settings */
        if (apply_flash_intensity) {
            for (unsigned int i = 0; i < new_ae_results->num_flashes; i++) {
                ret |= mFlashCI->setFlashIntensity((int)(new_ae_results->flashes[i]).power_prc, i);
            }
        }

        if (update_results_history) {
            mAeState.ae_results = storeAeResults(new_ae_results);
        } else {
            // Delaying of results is not needed in still mode, since upper
            // layer sequences are handling exposure delays with skipping.
            // Update the head and keep the rest for restoring in mode switch
            mAeState.ae_results = storeAeResults(new_ae_results, 0);
        }

        if (mAeState.ae_results == NULL) {
           ALOGE("Failed to store AE Results");
           mAeState.ae_results = new_ae_results;
        }
    } else {
        LOG1("%s: no new results", __FUNCTION__);
    }
    return ret;
}

void AtomAIQ::resetAWBParams()
{
    LOG2("@%s", __FUNCTION__);
    mAwbInputParameters.frame_use = m3aState.frame_use;
    mAwbInputParameters.scene_mode = ia_aiq_awb_operation_mode_auto;
    mAwbInputParameters.manual_cct_range = NULL;
    mAwbInputParameters.manual_white_coordinate = NULL;
}

void AtomAIQ::runAwbMain()
{
    LOG2("@%s", __FUNCTION__);

    if (mAwbLocked)
        return;
    ia_err ret = ia_err_none;

    if(m3aState.ia_aiq_handle)
    {
        //mAwbInputParameters.scene_mode = ia_aiq_awb_operation_mode_auto;
        LOG2("before ia_aiq_awb_run() param-- frame_use:%d scene_mode:%d", mAwbInputParameters.frame_use, mAwbInputParameters.scene_mode);
        ret = ia_aiq_awb_run(m3aState.ia_aiq_handle, &mAwbInputParameters, &mAwbResults);
        LOG2("@%s result: %d", __FUNCTION__, ret);
    }
}

void AtomAIQ::resetGBCEParams()
{
    mGBCEDefault = false;
    mGBCEResults = NULL;
}

status_t AtomAIQ::runGBCEMain()
{
    LOG2("@%s", __FUNCTION__);
    if (m3aState.ia_aiq_handle) {
        ia_aiq_gbce_input_params gbce_input_params;
        if (mGBCEDefault)
        {
            gbce_input_params.gbce_level = ia_aiq_gbce_level_bypass;
        } else
        {
            gbce_input_params.gbce_level = ia_aiq_gbce_level_use_tuning;
        }
        gbce_input_params.frame_use = m3aState.frame_use;
        getEv(&gbce_input_params.ev_shift);
        ia_err err = ia_aiq_gbce_run(m3aState.ia_aiq_handle, &gbce_input_params, &mGBCEResults);
        if(err == ia_err_none)
            LOG2("@%s success", __FUNCTION__);
    } else {
        mGBCEResults = NULL;
    }
    return NO_ERROR;
}

status_t AtomAIQ::getGBCEResults(ia_aiq_gbce_results *gbce_results)
{
    LOG2("@%s", __FUNCTION__);

    if (!gbce_results)
        return BAD_VALUE;

    if (mGBCEResults)
        *gbce_results = *mGBCEResults;
    else
        return INVALID_OPERATION;

    return NO_ERROR;
}

status_t AtomAIQ::getExposureParameters(ia_aiq_exposure_parameters *exposure)
{
    LOG1("@%s", __FUNCTION__);

    ia_aiq_ae_results *ae_results;

    if (!exposure)
        return BAD_VALUE;

    ae_results = peekAeStoredResults(0);

    if (ae_results)
        *exposure = *ae_results->exposures[0].exposure;
    else
        return INVALID_OPERATION;

    return NO_ERROR;
}

void AtomAIQ::resetDSDParams()
{
    m3aState.dsd_enabled = false;
}

status_t AtomAIQ::runDSDMain()
{
    LOG2("@%s", __FUNCTION__);
    if (m3aState.ia_aiq_handle && m3aState.dsd_enabled)
    {
        mDSDInputParameters.af_results = mAfState.af_results;
        mDSDInputParameters.scene_modes_selection = (ia_aiq_scene_mode)
            (ia_aiq_scene_mode_close_up_portrait |
            ia_aiq_scene_mode_portrait |
            ia_aiq_scene_mode_lowlight_portrait |
            ia_aiq_scene_mode_low_light |
            ia_aiq_scene_mode_action |
            ia_aiq_scene_mode_backlight |
            ia_aiq_scene_mode_landscape |
            ia_aiq_scene_mode_firework |
            ia_aiq_scene_mode_lowlight_action);

        ia_err ret = ia_aiq_dsd_run(m3aState.ia_aiq_handle, &mDSDInputParameters, &mDetectedSceneMode);
        if(ret == ia_err_none)
            LOG2("@%s success, detected scene mode: %d", __FUNCTION__, mDetectedSceneMode);
    }
    return NO_ERROR;
}

status_t AtomAIQ::run3aMain()
{
    LOG2("@%s", __FUNCTION__);
    status_t ret = NO_ERROR;

    if (mBracketingRunning) {
        LOG1("@%s bracketing now, skip 3a running", __FUNCTION__);
        return ret;
    }

    if(!mFileInjection)
        ret |= runAfMain();

    // if no DSD enable, should disable that
    if(!mFileInjection)
        ret |= runDSDMain();

    if(!mFileInjection)
        ret |= runAeMain();

    runAwbMain();

    if(mAeMode != CAM_AE_MODE_MANUAL)
        ret |= runGBCEMain();
    else
        mGBCEResults = NULL;

    // get AIC result and apply into ISP
    ret |= runAICMain();

    return ret;
}
/*
int AtomAIQ::run3aMain()
{
    LOG2("@%s", __FUNCTION__);
    int ret = 0;
    //
    switch(mFlashStage) {
    case CAM_FLASH_STAGE_AF:
        mAfInputParameters.frame_use = ia_aiq_frame_use_still;
        runAfMain();
        if(! mAfState.af_results->status == ia_aiq_af_status_success &&
           ! mAfState.af_results->status == ia_aiq_af_status_fail)
        //need more frame for AF run
            return -1;
        mFlashStage = CAM_FLASH_STAGE_AE;
        if(m3aState.dsd_enabled)
            runDSDMain();
        if(mAfState.af_results->use_af_assist) {
            LOG2("af assist on, need to set off");
            mAfInputParameters.flash_mode = ia_aiq_flash_mode_off;
            //need more frame for AEC run
            return -1;
        }
        return -1;
        break;
    case CAM_FLASH_STAGE_AE:
        ret = AeForFlash();
        if(ret < 0)
            return ret;
        mFlashStage = CAM_FLASH_STAGE_AF;
        break;
    default:
        LOG2("flash stage finished");
    }

    runAwbMain();
    if(mGBCEEnable && mAeMode != CAM_AE_MODE_MANUAL)
        runGBCEMain();

    // get AIC result and apply into ISP
    runAICMain();

    return 0;
}


//run 3A for flash
int AtomAIQ::AeForFlash()
{
    LOG2("@%s", __FUNCTION__);

    runAeMain();
    if (mAeState.ae_results->flash->status == ia_aiq_flash_status_no) {
        if(!mAeState.ae_results->converged) {
            mAeInputParameters.frame_use = ia_aiq_frame_use_preview;
            mAeInputParameters.flash_mode = ia_aiq_flash_mode_off;
            //need one more frame for AE
            LOG2("@%s need one more frame for AE when result not converged with flash off", __FUNCTION__);
            return -1;
        }
        if(mAeState.ae_results->converged) {
            m3aState.frame_use = ia_aiq_frame_use_still;
            runAeMain();
            LOG2("flash off and converged");
            return 0;
        }
    }else if (mAeState.ae_results->flash->status ==  ia_aiq_flash_status_torch) {
            m3aState.frame_use = ia_aiq_frame_use_still;
            runAeMain();
            if(mAeState.ae_results->flash->status == ia_aiq_flash_status_pre) {
                mAeInputParameters.flash_mode = ia_aiq_flash_mode_on;
                LOG2("@%s need one more frame for AE when result in pre with flash on", __FUNCTION__);
                return -1;
            }
            if (mAeState.ae_results->flash->status == ia_aiq_flash_status_main) {
                LOG2("@%s go ahead for capturing, it's main stage", __FUNCTION__);
                return 0;
            }
    }

    return NO_ERROR;
}
*/
status_t AtomAIQ::runAICMain()
{
    LOG2("@%s", __FUNCTION__);

    status_t ret = NO_ERROR;

    if (m3aState.ia_aiq_handle) {
        ia_aiq_pa_input_params pa_input_params;

        pa_input_params.frame_use = m3aState.frame_use;
        mIspInputParams.frame_use = m3aState.frame_use;

        pa_input_params.awb_results = NULL;
        mIspInputParams.awb_results = NULL;
        pa_input_params.color_gains = NULL;

        mIspInputParams.exposure_results = (mAeState.ae_results) ? mAeState.ae_results->exposures[0].exposure : NULL;
        pa_input_params.exposure_params = (mAeState.ae_results) ? mAeState.ae_results->exposures[0].exposure : NULL;

        if (mAwbResults) {
            LOG2("awb factor:%f", mAwbResults->accurate_b_per_g);
        }
        pa_input_params.awb_results = mAwbResults;
        mIspInputParams.awb_results = mAwbResults;

        if (mGBCEResults) {
            LOG2("gbce :%d", mGBCEResults->ctc_gains_lut_size);
        }
        mIspInputParams.gbce_results = mGBCEResults;

        pa_input_params.sensor_frame_params = &m3aState.sensor_frame_params;
        mIspInputParams.sensor_frame_params = &m3aState.sensor_frame_params;
        LOG2("@%s  2 sensor native width %d", __FUNCTION__, pa_input_params.sensor_frame_params->cropped_image_width);

        // Calculate ISP independent ISP parameters (e.g. LSC table, color correction matrix)
        ia_aiq_pa_results *pa_results;
        ret = ia_aiq_pa_run(m3aState.ia_aiq_handle, &pa_input_params, &pa_results);
        LOG2("@%s  ia_aiq_pa_run :%d", __FUNCTION__, ret);
        mPaResults =  pa_results;

        mIspInputParams.pa_results = pa_results;
        mIspInputParams.manual_brightness = 0;
        mIspInputParams.manual_hue = 0;

        // Forces updating LSC in ISP on each ISP reset. It is assumed that
        // changeSensorMode is invoked when ISP reset occurs.
        if (m3aState.lsc_update) {
            mIspInputParams.pa_results->lsc_update = true;
            m3aState.lsc_update = false;
        }

        ret = mISPAdaptor->calculateIspParams(&mIspInputParams, &((m3aState.results).isp_output));

        /* Apply ISP settings */
        if (m3aState.results.isp_output.data) {
            memcpy(&mAicOutStruct, m3aState.results.isp_output.data, sizeof(struct atomisp_parameters));
            mAicOutStruct.isp_config_id = 0;
            mAicOutStruct.output_frame = NULL;
            ret |= mISP->setAicParameter(&mAicOutStruct);
        }

        if (mFileInjection && ret == 0 && mAwbResults != NULL) {
            // When the awb result converged, and reach the max try count,
            // dump the makernote into file
            if ((mAwbResults->distance_from_convergence >= -EPSILON &&
                 mAwbResults->distance_from_convergence <= EPSILON) &&
                 mAwbRunCount > RETRY_COUNT ) {
                mAwbRunCount = 0;
                CameraDump::getInstance(mCameraId)->dumpMkn2File();
            } else if(mAwbResults->distance_from_convergence >= -EPSILON &&
                      mAwbResults->distance_from_convergence <= EPSILON){
                mAwbRunCount++;
                LOG2("AWB converged:%d", mAwbRunCount);
            }
        }
    }
    return ret;
}

int AtomAIQ::dumpMknToFile()
{
    LOG1("@%s", __FUNCTION__);
    FILE *fp;
    size_t bytes;
    String8 fileName;
    //get binary of makernote and store
    ia_binary_data *aaaMkNote;
    aaaMkNote = get3aMakerNote(ia_mkn_trg_section_2);
    if(aaaMkNote) {
        fileName = mISP->getFileInjectionFileName();
        fileName += ".mkn";
        LOG2("filename:%s",  fileName.string());
        fp = fopen (fileName.string(), "w+");
        if (fp == NULL) {
            ALOGE("open file %s failed %s", fileName.string(), strerror(errno));
            put3aMakerNote(aaaMkNote);
            return -1;
        }
        if ((bytes = fwrite(aaaMkNote->data, aaaMkNote->size, 1, fp)) < (size_t)aaaMkNote->size)
            ALOGW("Write less mkn bytes to %s: %d, %d", fileName.string(), aaaMkNote->size, bytes);
        fclose (fp);
    }
    return 0;
}

int AtomAIQ::enableFpn(bool enable)
{
    // No longer supported, use CPF instead
    ALOGE("%s: ERROR, should not be in here", __FUNCTION__);
    return NO_ERROR;
}

/*! \fn  getAEExpCfg
 * \brief Get sensor's configuration for AE
 * exp_time: Preview exposure time
 * aperture: Aperture
 * Get the AEC outputs (which we hope are used by the sensor)
 * @param exp_time - exposure time
 * @param aperture - aperture
 * @param aec_apex_Tv - Shutter speed
 * @param aec_apex_Sv - Sensitivity
 * @param aec_apex_Av - Aperture
 * @param digital_gain - digital_gain
 */
void AtomAIQ::getAeExpCfg(int *exp_time,
                          short unsigned int *aperture_num,
                          short unsigned int *aperture_denum,
                     int *aec_apex_Tv, int *aec_apex_Sv, int *aec_apex_Av,
                     float *digital_gain,
                     float *total_gain)
{
    LOG2("@%s", __FUNCTION__);

    mSensorCI->getExposureTime(exp_time);
    mSensorCI->getFNumber(aperture_num, aperture_denum);
    ia_aiq_ae_results *latest_ae_results = NULL;
    if (mBracketingRunning && mAEBracketingResult) {
        latest_ae_results = mAEBracketingResult;
    } else {
        latest_ae_results = mAeState.ae_results;
    }

    if (latest_ae_results != NULL) {
        *digital_gain = (latest_ae_results->exposures[0].exposure)->digital_gain;
        *aec_apex_Tv = -1.0 * (log10((double)(latest_ae_results->exposures[0].exposure)->exposure_time_us/1000000) / log10(2.0)) * 65536;
        *aec_apex_Av = log10(pow((latest_ae_results->exposures[0].exposure)->aperture_fn, 2))/log10(2.0) * 65536;
        *aec_apex_Sv = log10(pow(2.0, -7.0/4.0) * (latest_ae_results->exposures[0].exposure)->iso) / log10(2.0) * 65536;
        if (*digital_gain > float(0)) {
            *total_gain = latest_ae_results->exposures[0].exposure->analog_gain * *digital_gain;
        }
        else {
            *total_gain = latest_ae_results->exposures[0].exposure->analog_gain;
        }

        LOG2("total_gain: %f  digital gain: %f", *total_gain, *digital_gain);
    }
}

bool AtomAIQ::getAeUllTrigger()
{
    LOG2("@%s", __FUNCTION__);
    if (mAeState.ae_results) {
        return (mAeState.ae_results->multiframe & ia_aiq_bracket_mode_ull) ? true : false;
    } else {
        return false;
    }
}

void AtomAIQ::getDefaultParams(CameraParameters *params, CameraParameters *intel_params)
{
    LOG2("@%s", __FUNCTION__);
    if (!params) {
        ALOGE("params is null!");
        return;
    }

    // ae mode
    intel_params->set(IntelCameraParameters::KEY_AE_MODE, "auto");
    intel_params->set(IntelCameraParameters::KEY_SUPPORTED_AE_MODES, "auto,manual,shutter-priority,aperture-priority");

    // 3a lock: auto-exposure lock
    params->set(CameraParameters::KEY_AUTO_EXPOSURE_LOCK, CameraParameters::FALSE);
    params->set(CameraParameters::KEY_AUTO_EXPOSURE_LOCK_SUPPORTED, CameraParameters::TRUE);
    // 3a lock: auto-whitebalance lock
    params->set(CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK, CameraParameters::FALSE);
    params->set(CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK_SUPPORTED, CameraParameters::TRUE);

    // Intel/UMG parameters for 3A locks
    // TODO: only needed until upstream key is available for AF lock
    intel_params->set(IntelCameraParameters::KEY_AF_LOCK_MODE, "unlock");
    intel_params->set(IntelCameraParameters::KEY_SUPPORTED_AF_LOCK_MODES, "lock,unlock");
    // TODO: add UMG-style AE/AWB locking for Test Camera?

    // manual shutter control (Intel extension)
    intel_params->set(IntelCameraParameters::KEY_SHUTTER, "60");
    intel_params->set(IntelCameraParameters::KEY_SUPPORTED_SHUTTER, "1s,2,4,8,15,30,60,125,250,500");

    if (!PlatformData::isFixedFocusCamera(mCameraId)) {
        // multipoint focus
        params->set(CameraParameters::KEY_MAX_NUM_FOCUS_AREAS, getAfMaxNumWindows());
    } else {
        params->set(CameraParameters::KEY_MAX_NUM_FOCUS_AREAS, 0);
    }

    // set empty area
    params->set(CameraParameters::KEY_FOCUS_AREAS, "(0,0,0,0,0)");

    // metering areas
    params->set(CameraParameters::KEY_MAX_NUM_METERING_AREAS, getAeMaxNumWindows());
    // set empty area
    params->set(CameraParameters::KEY_METERING_AREAS, "(0,0,0,0,0)");

    // Capture bracketing
    intel_params->set(IntelCameraParameters::KEY_CAPTURE_BRACKET, "none");
    intel_params->set(IntelCameraParameters::KEY_SUPPORTED_CAPTURE_BRACKET, "none,exposure,focus");

    intel_params->set(IntelCameraParameters::KEY_HDR_IMAGING, PlatformData::defaultHdr(mCameraId));
    intel_params->set(IntelCameraParameters::KEY_SUPPORTED_HDR_IMAGING, PlatformData::supportedHdr(mCameraId));
    intel_params->set(IntelCameraParameters::KEY_HDR_SAVE_ORIGINAL, "off");
    intel_params->set(IntelCameraParameters::KEY_SUPPORTED_HDR_SAVE_ORIGINAL, "on,off");

    // AWB mapping mode
    intel_params->set(IntelCameraParameters::KEY_AWB_MAPPING_MODE, IntelCameraParameters::AWB_MAPPING_AUTO);
    intel_params->set(IntelCameraParameters::KEY_SUPPORTED_AWB_MAPPING_MODES, "auto,indoor,outdoor");

}

void AtomAIQ::getSensorFrameParams(ia_aiq_frame_params *frame_params)
{
    LOG2("@%s", __FUNCTION__);

    struct atomisp_sensor_mode_data sensor_mode_data;
    CLEAR(sensor_mode_data);

    if(mSensorCI->getModeInfo(&sensor_mode_data) < 0) {
        sensor_mode_data.crop_horizontal_start = 0;
        sensor_mode_data.crop_vertical_start = 0;
        sensor_mode_data.crop_vertical_end = 0;
        sensor_mode_data.crop_horizontal_end = 0;
    }
    frame_params->horizontal_crop_offset = sensor_mode_data.crop_horizontal_start;
    frame_params->vertical_crop_offset = sensor_mode_data.crop_vertical_start;
    // The +1 needed as the *_end and *_start values are index values.
    frame_params->cropped_image_height = sensor_mode_data.crop_vertical_end - sensor_mode_data.crop_vertical_start + 1;
    frame_params->cropped_image_width = sensor_mode_data.crop_horizontal_end - sensor_mode_data.crop_horizontal_start +1;
    /* TODO: Get scaling factors from sensor configuration parameters */
    frame_params->horizontal_scaling_denominator = 254;
    frame_params->vertical_scaling_denominator = 254;

    if ((frame_params->cropped_image_width == 0) || (frame_params->cropped_image_height == 0)){
    // the driver gives incorrect values for the frame width or height
        frame_params->horizontal_scaling_numerator = 0;
        frame_params->vertical_scaling_numerator = 0;
        ALOGE("Invalid sensor frame parameters. Cropped image width: %d, cropped image height: %d",
              frame_params->cropped_image_width, frame_params->cropped_image_height );
        ALOGE("This causes lens shading table not to be used.");
    } else {
        frame_params->horizontal_scaling_numerator =
                sensor_mode_data.output_width * 254 * sensor_mode_data.binning_factor_x/ frame_params->cropped_image_width;
        frame_params->vertical_scaling_numerator =
                sensor_mode_data.output_height * 254 * sensor_mode_data.binning_factor_y / frame_params->cropped_image_height;
    }
}

/********************************
        IaIsp15
*********************************/
IaIsp15::IaIsp15()
{
    LOG1("@%s", __FUNCTION__);
}

IaIsp15::~IaIsp15()
{
    LOG1("@%s", __FUNCTION__);
    ia_isp_1_5_deinit(mIspHandle);
}

void IaIsp15::initIaIspAdaptor(const ia_binary_data *cpfData,
                           unsigned int maxStatsWidth,
                           unsigned int maxStatsHeight,
                           ia_cmc_t *cmc,
                           ia_mkn *mkn,
                           int cameraId)
{
    LOG1("@%s", __FUNCTION__);

    int value = 0;
    PlatformData::HalConfig[cameraId].getValue(value, CPF::IspVamemType);
    mIaIsp15InputParams.isp_vamem_type = value;

    mIspHandle = ia_isp_1_5_init(cpfData,
                                 maxStatsWidth,
                                 maxStatsHeight,
                                 cmc,
                                 mkn);

}

ia_err IaIsp15::convertIspStatistics(void *statistics,
                                          ia_aiq_rgbs_grid **out_rgbs_grid,
                                          ia_aiq_af_grid **out_af_grid)
{
    LOG2("@%s", __FUNCTION__);
    return ia_isp_1_5_statistics_convert(mIspHandle, statistics, out_rgbs_grid, out_af_grid);
}

ia_err IaIsp15::calculateIspParams(const ispInputParameters *isp_input_params,
                            ia_binary_data *output_data)
{
    LOG2("@%s", __FUNCTION__);

    mIaIsp15InputParams.frame_use = isp_input_params->frame_use;
    mIaIsp15InputParams.sensor_frame_params = isp_input_params->sensor_frame_params;
    mIaIsp15InputParams.exposure_results = isp_input_params->exposure_results;
    mIaIsp15InputParams.awb_results = isp_input_params->awb_results;
    mIaIsp15InputParams.gbce_results = isp_input_params->gbce_results;
    mIaIsp15InputParams.pa_results = isp_input_params->pa_results;
    mIaIsp15InputParams.manual_brightness = isp_input_params->manual_brightness;
    mIaIsp15InputParams.manual_contrast = isp_input_params->manual_contrast;
    mIaIsp15InputParams.manual_hue = isp_input_params->manual_hue;
    mIaIsp15InputParams.manual_saturation = isp_input_params->manual_saturation;
    mIaIsp15InputParams.nr_setting.feature_level = ia_isp_feature_level_high;
    mIaIsp15InputParams.nr_setting.strength = 0;
    mIaIsp15InputParams.ee_setting.feature_level = ia_isp_feature_level_high;
    mIaIsp15InputParams.ee_setting.strength = 0;
    mIaIsp15InputParams.effects = isp_input_params->effects;

    return ia_isp_1_5_run(mIspHandle, &mIaIsp15InputParams, output_data);
}


/********************************
        IaIsp22
*********************************/
IaIsp22::IaIsp22()
{
    LOG1("@%s", __FUNCTION__);
    CLEAR(mIaIsp22InputParams);
}

IaIsp22::~IaIsp22()
{
    LOG1("@%s", __FUNCTION__);
    ia_isp_2_2_deinit(mIspHandle);
}

void IaIsp22::initIaIspAdaptor(const ia_binary_data *cpfData,
                           unsigned int maxStatsWidth,
                           unsigned int maxStatsHeight,
                           ia_cmc_t *cmc,
                           ia_mkn *mkn,
                           int cameraId)
{
    LOG1("@%s", __FUNCTION__);

    int value = 0;
    PlatformData::HalConfig[cameraId].getValue(value, CPF::IspVamemType);
    mIaIsp22InputParams.isp_vamem_type = value;

    mIspHandle = ia_isp_2_2_init(cpfData,
                                 maxStatsWidth,
                                 maxStatsHeight,
                                 cmc,
                                 mkn);

}

ia_err IaIsp22::convertIspStatistics(void *statistics,
                                          ia_aiq_rgbs_grid **out_rgbs_grid,
                                          ia_aiq_af_grid **out_af_grid)
{
    LOG2("@%s", __FUNCTION__);
    return ia_isp_2_2_statistics_convert(mIspHandle, statistics, out_rgbs_grid, out_af_grid);
}

ia_err IaIsp22::calculateIspParams(const ispInputParameters *isp_input_params,
                            ia_binary_data *output_data)
{
    LOG2("@%s", __FUNCTION__);

    mIaIsp22InputParams.frame_use = isp_input_params->frame_use;
    mIaIsp22InputParams.sensor_frame_params = isp_input_params->sensor_frame_params;
    mIaIsp22InputParams.exposure_results = isp_input_params->exposure_results;
    mIaIsp22InputParams.awb_results = isp_input_params->awb_results;
    mIaIsp22InputParams.gbce_results = isp_input_params->gbce_results;
    mIaIsp22InputParams.pa_results = isp_input_params->pa_results;
    mIaIsp22InputParams.manual_brightness = isp_input_params->manual_brightness;
    mIaIsp22InputParams.manual_contrast = isp_input_params->manual_contrast;
    mIaIsp22InputParams.manual_hue = isp_input_params->manual_hue;
    mIaIsp22InputParams.manual_saturation = isp_input_params->manual_saturation;
    mIaIsp22InputParams.nr_setting.feature_level = ia_isp_feature_level_high;
    mIaIsp22InputParams.nr_setting.strength = 0;
    mIaIsp22InputParams.ee_setting.feature_level = ia_isp_feature_level_high;
    mIaIsp22InputParams.ee_setting.strength = isp_input_params->manual_sharpness;
    mIaIsp22InputParams.effects = isp_input_params->effects;

    return ia_isp_2_2_run(mIspHandle, &mIaIsp22InputParams, output_data);
}

} //  namespace android
