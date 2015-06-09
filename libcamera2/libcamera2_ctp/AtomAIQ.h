/*
 * Copyright (c) 2012 Intel Corporation.
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

#ifndef ANDROID_LIBCAMERA_AIQ_AAA
#define ANDROID_LIBCAMERA_AIQ_AAA


namespace android {
class AtomAIQ;
class I3AControls;
};

#include <utils/Errors.h>
#include <utils/threads.h>
#include <time.h>
#include <ia_types.h>
#include <ia_aiq_types.h>
#include <ia_aiq.h>
#include <ia_isp_1_5.h>
#include <ia_isp_2_2.h>
#include "AtomCommon.h"
#include "I3AControls.h"
#include "PlatformData.h"
#include "AtomFifo.h"
#include "ICameraHwControls.h"
#include "ia_face.h"

namespace android {

#define DEFAULT_GBCE            true
#define DEFAULT_GBCE_STRENGTH   0
#define AIQ_MAX_TIME_FOR_AF     2500 // milliseconds
#define TORCH_INTENSITY         20   // 20%
#define MAX_NUM_AF_WINDOW       9
#define AE_DELAY_FRAMES_DEFAULT 2

typedef struct {
    struct atomisp_parm               isp_params;
    ia_binary_data                    isp_output;
    bool                              exposure_changed;
    bool                              flash_intensity_changed;
} aiq_results;

typedef struct {
    ia_aiq_af_results              *af_results;
    ia_aiq_rect                     focus_rect;
    ia_aiq_manual_focus_parameters  focus_parameters;
    struct timespec                 lens_timestamp;
    unsigned long long              previous_sof;
    int32_t                         lens_position;
    bool                            aec_locked;
    bool                            assist_light;
    bool                            af_locked;
    AfMode                          afMode;
    int                             af_score_window_size;
} af_state;

typedef struct {
    ia_aiq_ae_results                 results;
    ia_aiq_hist_weight_grid           weight_grid;
    ia_aiq_exposure_parameters        exposure;
    ia_aiq_exposure_sensor_parameters sensor_exposure;
    ia_aiq_flash_parameters           flash;
} stored_ae_results;

typedef struct {
    bool                              ae_locked;
    ia_aiq_ae_results                 *ae_results;
    stored_ae_results                 feedback_results;
    unsigned int                      feedback_delay;
    AtomFifo<stored_ae_results>       *stored_results;
} ae_state;

typedef struct {
    struct atomisp_grid_info        curr_grid_info;
    bool                            reconfigured;
    ia_face_state                  *faces;
    ia_aiq                         *ia_aiq_handle;
    ia_aiq_scene_mode               detected_scene;
    ia_aiq_rgbs_grid                rgbs_grid;
    ia_aiq_af_grid                  af_grid;
    ia_aiq_frame_params             sensor_frame_params;
    ia_aiq_awb_manual_cct_range     cct_range;
    bool                            dsd_enabled;
    bool                            aic_enabled;
    ia_aiq_frame_use                frame_use;
    ia_aiq_statistics_input_params  statistics_input_parameters;
    ia_aiq_af_results               af_results_feedback;
    ia_aiq_dsd_input_params         dsd_input_parameters;
    struct atomisp_3a_statistics   *stats;
    bool                            stats_valid;
    int                             boot_events;
    struct timespec                 lens_timestamp;
    aiq_results                     results;
} aaa_state;


/*!
 * \brief Common structure for IA ISP Configuration input parameters
 *
 * The structure is a combination of all the input parameters needed for
 * different IA ISP Configuration versions.
 *
 */
typedef struct {
    ia_aiq_frame_use frame_use;                      /*!< Target frame type of the AIC calculations (Preview, Still, video etc.). */
    ia_aiq_frame_params        *sensor_frame_params; /*!< Sensor frame parameters (crop offsets, scaling denominators etc). */
    ia_aiq_exposure_parameters *exposure_results;    /*!< Exposure parameters which are to be used to calculate next ISP parameters. */
    ia_aiq_awb_results *awb_results;                 /*!< WB results which are to be used to calculate next ISP parameters (WB gains, color matrix,etc). */
    ia_aiq_gbce_results *gbce_results;               /*!< GBCE Gamma tables which are to be used to calculate next ISP parameters.
                                                          If NULL pointer is passed, AIC will use static gamma table from the CPF.  */
    ia_aiq_pa_results *pa_results;                   /*!< Parameter adaptor results from AIQ. */
    char manual_brightness;                          /*!< Manual brightness value range [-128,127]. */
    char manual_contrast;                            /*!< Manual contrast value range [-128,127]. */
    char manual_hue;                                 /*!< Manual hue value range [-128,127]. */
    char manual_saturation;                          /*!< Manual saturation value range [-128,127]. */
    char manual_sharpness;                           /*!< Manual setting for sharpness [-128,127]. */
    ia_aiq_effect effects;                           /*!< Manual setting for special effects.*/

} ispInputParameters;


/*!
 * \brief IIaIspAdaptor defines an interface for classes that interact
 * with different IA ISP Adaptation libraries.
 *
 * The "IA ISP Adaptation libraries" is a generic name for the libraries
 * running on the main IA CPU that implement conversions between HW specific
 * data structures used by HW ISP and the generic algorithms running on CPU.
 * this libraries are separated from the ones that implement generic algorithms
 *
 * This interface defines the common operation that different implementations
 * will offer. We do that in order to keep the code in AtomAIQ as ISP version
 * independent as possible.
 *
 * The specific conversions will be implemented in the classes that realize this
 * interface. The decision to which class to instantiate is done at initialization
 * time when we query the version of the HW ISP
 *
 * The classes should be named IaIspxx where xx stands for major and minor
 * version of CSS. If there are different kinds of ISPs in the future, the naming
 * needs to be adapted.
 */
class IIaIspAdaptor
{

public:
    virtual ~IIaIspAdaptor() {}

    /*!
     * \brief Initializes IA_ISP adaptor library and its submodules.
     *
     * \param[in]     cpfData          AIQ block from CPF file. Contains ISP specific parameters.
     * \param[in]     maxStatsWidth    Maximum width of RGBS and AF statistics grids from ISP.
     * \param[in]     maxStatsHeight   Maximum height of RGBS and AF statistics grids from ISP.
     * \param[in]     cmc              Parsed camera module characterization structure.
     * \param[in,out] mkn              Makernote handle which can be initialized with ia_mkn library. If debug data from AIQ
     *                                 is needed to be stored into EXIF, this parameter is needed.
     */
    virtual void initIaIspAdaptor(const ia_binary_data *cpfData,
                           unsigned int maxStatsWidth,
                           unsigned int maxStatsHeight,
                           ia_cmc_t *cmc,
                           ia_mkn *mkn) = 0;

    /*!
     * \brief Converts ISP HW specific statistics to IA_AIQ generic format.
     * ISP generated statistics may not be in the format in which AIQ algorithms expect.
     * Statistics need to be converted from various ISP formats into AIQ statistics format.
     *
     * \param[in]  statistics    Statistics in ISP specific format.
     * \param[out] outRgbsGrid   Pointer's pointer where address of converted statistics are stored.
     *                           Converted RGBS grid statistics. Output can be directly used as input in function ia_aiq_statistics_set.
     * \param[out] outAfGrid     Pointer's pointer where address of converted statistics are stored.
     *                           Converted AF grid statistics. Output can be directly used as input in function ia_aiq_statistics_set.
     * \return                   Error code.
    */
    virtual ia_err convertIspStatistics(void *statistics,
                                        ia_aiq_rgbs_grid **outRgbsGrid,
                                        ia_aiq_af_grid **outAfGrid) = 0;

    /*!
     * \brief Converts the generic output results from 3A and other SW algorithms
     * into HW specific configuration for the HW ISP
     *
     *
     * \param[in]  ispInputParams   Outcome of the 3A and other algorithms, this is an input to ISP.
     *                              This structure is the generic version produced by the SW algorithms.
     *
     * \param[out] ispOutputData    Opaque binary data structure with pointer to the ISP configuration structure.
     *                              This is HW specific
     * \return                      Error code.
     */
    virtual ia_err calculateIspParams(const ispInputParameters *ispInputParams,
                                      ia_binary_data *outputData) = 0;
protected:
    ia_isp  *mIspHandle;
}; //class IIaIspAdaptor

/**
 * \class AtomAIQ
 *
 * AtomAIQ is a singleton interface to Intel 3A Library (formerly known as libia_aiq).
 *
 * The libia_aiq library provides the 3A functionality(AF, AEC, AWB, GBCE, DSD, AIC)
 * Due to this, in addition to AAAThread that handles actual AAA processing, many other
 * subcomponents of HAL need to use AtomAIQ.
 *
 * All access to the imaging library go via AtomAIQ.
 *
 */
class AtomAIQ : public I3AControls {

// constructor/destructor
private:
    status_t getAiqConfig(ia_binary_data *cpfData);

    // Common functions for 3A, GBCE, AF etc.
    status_t run3aMain();

    //AE for flash
    int AeForFlash();
    int applyResults();
    bool changeSensorMode(void);

    //staticstics
    status_t getStatistics(const struct timeval *frame_timestamp);
    struct atomisp_3a_statistics * allocateStatistics(int grid_size);
    void freeStatistics(struct atomisp_3a_statistics *stats);
    bool needStatistics();

    // GBCE
    int setGammaEffect(bool inv_gamma);
    int enableGbce(bool enable);
    void resetGBCEParams();
    status_t runGBCEMain();

    // 3A control
    status_t _init3A();
    int run3aInit();
    int processForFlash();
    void get3aGridInfo(struct atomisp_grid_info *pgrid);
    void get3aStat();
    status_t populateFrameInfo(const struct timeval *frame_timestamp,
                               const struct timeval *sof_timestamp);

    //AIC
    status_t runAICMain();

    // AF
    void resetAFParams();
    status_t runAfMain();
    void setAfFocusMode(ia_aiq_af_operation_mode mode);
    void setAfFocusRange(ia_aiq_af_range range);
    void setAfMeteringMode(ia_aiq_af_metering_mode mode);
    status_t moveFocusDriveToPos(long position);
    void afUpdateTimestamp(void);


    //AE
    void resetAECParams();
    status_t runAeMain();
    bool getAeResults();
    bool getAeFlashResults();
    int applyExposure(ia_aiq_exposure_sensor_parameters *);
    ia_aiq_ae_results* storeAeResults(ia_aiq_ae_results *, int updateIdx = -1);
    ia_aiq_ae_results* peekAeStoredResults(unsigned int offset);
    ia_aiq_ae_results* pickAeFeedbackResults();

    //AWB
    void resetAWBParams();
    void runAwbMain();
    bool getAwbResults();

    //DSD
    void resetDSDParams();
    status_t runDSDMain();

    //ISP parameters
    int enableGdc(bool enable);
    int enableFpn(bool enable);
    int enableEe(bool enable);
    int enableNr(bool enable);
    int enableDp(bool enable);
    int enableOb(bool enable);
    int enableShadingCorrection(bool enable);

    // Get exposure time for AE
    void getAeExpCfg(int *exp_time,
                     short unsigned int *aperture_num,
                     short unsigned int *aperture_denum,
                     int *aec_apex_Tv, int *aec_apex_Sv, int *aec_apex_Av,
                     float *digital_gain,
                     float *total_gain);

    void getSensorFrameParams(ia_aiq_frame_params *frame_params);
    int dumpMknToFile();

// prevent copy constructor and assignment operator
private:
    AtomAIQ(const AtomAIQ& other);
    AtomAIQ& operator=(const AtomAIQ& other);

public:
    AtomAIQ(HWControlGroup &hwcg);
    ~AtomAIQ();

    virtual SensorType getType() { return SENSOR_TYPE_RAW; }

    virtual bool isIntel3A() { return true; }
    void getDefaultParams(CameraParameters *params, CameraParameters *intel_params);

    // Initialization functions
    virtual status_t init3A();
    virtual status_t deinit3A();
    virtual status_t switchModeAndRate(AtomMode mode, float fps);

    // Getters and Setters
    status_t setAeWindow(const CameraWindow *window);
    status_t setAfWindow(const CameraWindow *window);
    status_t setAeFlickerMode(FlickerMode mode);
    status_t setAfEnabled(bool en) { return 0; }
    status_t setAeSceneMode(SceneMode mode);
    SceneMode getAeSceneMode();
    status_t setAeMode(AeMode mode);
    AeMode getAeMode();
    virtual status_t setAfMode(AfMode mode);
    virtual AfMode getAfMode();
    bool getAfNeedAssistLight();
    status_t setAeFlashMode(FlashMode mode);
    FlashMode getAeFlashMode();
    bool getAeFlashNecessary();
    status_t setAwbMode(AwbMode mode);
    AwbMode getAwbMode();
    AwbMode getLightSource();
    status_t setAeMeteringMode(MeteringMode mode);
    MeteringMode getAeMeteringMode();
    status_t set3AColorEffect(const char *effect);
    virtual void setPublicAeMode(AeMode mode);
    virtual AeMode getPublicAeMode();
    virtual status_t setIsoMode(IsoMode mode);
    virtual IsoMode getIsoMode(void) { return CAM_AE_ISO_MODE_NOT_SET; }
    status_t getGBCEResults(ia_aiq_gbce_results *gbce_results);
    virtual bool getAeUllTrigger();

    status_t setAeLock(bool en);
    bool     getAeLock();
    status_t setAfLock(bool en);
    bool     getAfLock();
    AfStatus getCAFStatus();
    status_t setAwbLock(bool en);
    bool     getAwbLock();
    // returning an error in the following functions will cause some functions
    // not to be run in ControlThread
    size_t   getAeMaxNumWindows() { return 1; }
    size_t   getAfMaxNumWindows() { return MAX_NUM_AF_WINDOW; }
    status_t setAfWindows(const CameraWindow *windows, size_t numWindows);
    status_t getExposureInfo(SensorAeConfig& sensorAeConfig);
    status_t getAeManualBrightness(float *ret);
    status_t setManualFocus(int focus, bool applyNow);
    status_t setManualFocusIncrement(int step);
    status_t applyEv(float bias);
    status_t setEv(float bias);
    status_t getEv(float *ret);

    status_t setManualIso(int ret);
    status_t getManualIso(int *ret);
    status_t setManualShutter(float expTime);
    status_t getManualShutter(float *expTime);
    status_t setSmartSceneDetection(bool en);
    bool     getSmartSceneDetection();
    status_t getSmartSceneMode(int *sceneMode, bool *sceneHdr);
    status_t setFaces(const ia_face_state& faceState);

    status_t getGridWindow(AAAWindowInfo& window);

    //Bracketing
    status_t initAfBracketing(int stop, AFBracketingMode mode);
    virtual status_t initAeBracketing();

    // Flash control
    virtual status_t setFlash(int numFrames);

    // ISP processing functions
    status_t apply3AProcess(bool read_stats, struct timeval *frame_timestamp);

    status_t startStillAf();
    status_t stopStillAf();
    AfStatus isStillAfComplete();
    status_t applyPreFlashProcess(FlashStage stage);

    // Makernote
    ia_binary_data *get3aMakerNote(ia_mkn_trg mode);
    void put3aMakerNote(ia_binary_data *mknData);
    void reset3aMakerNote(void);
    int add3aMakerNoteRecord(ia_mkn_dfid mkn_format_id,
                             ia_mkn_dnid mkn_name_id,
                             const void *record,
                             unsigned short record_size);

// private members
private:

    FILE *pFile3aStatDump;
    IHWIspControl *mISP;
    ia_env mPrintFunctions;

    aaa_state m3aState;

    //STATISTICS
    ia_aiq_statistics_input_params mStatisticsInputParameters;

    //AF
    AfMode mAfMode;
    nsecs_t mStillAfStart;
    ia_aiq_af_input_params mAfInputParameters;
    af_state mAfState;
    int mFocusPosition;

    //AF bracketing
    ia_aiq_af_bracket_results* mAfBracketingResult;
    int mBracketingStops;

    //AE
    ia_aiq_ae_input_params mAeInputParameters;
    ia_aiq_exposure_sensor_descriptor mAeSensorDescriptor;
    AeMode mAeMode;
    AeMode mPublicAeMode;
    SceneMode mAeSceneMode;
    FlashMode mAeFlashMode;
    ae_state mAeState;
    ia_coordinate mAeCoord;

    //AE bracketing
    ia_aiq_ae_input_params mAeBracketingInputParameters;
    bool mBracketingRunning;
    ia_aiq_ae_results *mAEBracketingResult;

    //AW
    ia_aiq_awb_input_params mAwbInputParameters;
    ia_aiq_awb_results *mAwbResults;
    ia_aiq_awb_results mAwbStoredResults;
    AwbMode mAwbMode;
    bool mAwbLocked;
    int mAwbRunCount;

    //GBCE
    ia_aiq_gbce_results *mGBCEResults;
    bool mGBCEDefault;


    //ISP
    ispInputParameters mIspInputParams;

    //DSD
    ia_aiq_dsd_input_params mDSDInputParameters;
    ia_aiq_scene_mode mDetectedSceneMode;

    //MKN
    ia_mkn  *mMkn;

    IHWSensorControl*    mSensorCI;
    IHWFlashControl*    mFlashCI;
    IHWLensControl*    mLensCI;

    IIaIspAdaptor *mISPAdaptor;

}; // class AtomAIQ


/*!
 * \brief  Realization of the IIaIspAdaptor interface for VIED ISP running
 *         with CSS v1.5 firmware
 */
class IaIsp15 : public IIaIspAdaptor
{
public:
    IaIsp15();
    ~IaIsp15();

    void initIaIspAdaptor(const ia_binary_data *cpfData,
                 unsigned int maxStatsWidth,
                 unsigned int maxStatsHeight,
                 ia_cmc_t *cmc,
                 ia_mkn *mkn);

    ia_err convertIspStatistics(void *statistics,
                                ia_aiq_rgbs_grid **outRgbsGrid,
                                ia_aiq_af_grid **outAfGrid);

    ia_err calculateIspParams(const ispInputParameters *ispInputParams,
                  ia_binary_data *outputData);

private:
    ia_isp_1_5_input_params mIaIsp15InputParams;
}; //class IaIsp15


/*!
 * \brief  Realization of the IIaIspAdaptor interface for VIED ISP running
 *         with CSS v2.2 firmware
 */
class IaIsp22 : public IIaIspAdaptor
{
public:
    IaIsp22();
    ~IaIsp22();

    void initIaIspAdaptor(const ia_binary_data *cpfData,
                 unsigned int maxStatsWidth,
                 unsigned int maxStatsHeight,
                 ia_cmc_t *cmc,
                 ia_mkn *mkn);

    ia_err convertIspStatistics(void *statistics,
                                ia_aiq_rgbs_grid **outRgbsGrid,
                                ia_aiq_af_grid **outAfGrid);

    ia_err calculateIspParams(const ispInputParameters *ispInputParams,
                  ia_binary_data *outputData);


private:
    ia_isp_2_2_input_params mIaIsp22InputParams;
}; //class IaIsp22

}; // namespace android

#endif // ANDROID_LIBCAMERA_AIQ_AAA

