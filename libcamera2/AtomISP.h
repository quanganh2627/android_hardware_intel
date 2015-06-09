/*
 * Copyright (C) 2011 The Android Open Source Project
 * Copyright (c) 2014 Intel Corporation
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

#ifndef ANDROID_LIBCAMERA_ATOM_ISP
#define ANDROID_LIBCAMERA_ATOM_ISP

#include <utils/Timers.h>
#include <utils/Errors.h>
#include <utils/Vector.h>
#include <utils/Errors.h>
#include <utils/threads.h>
#include "AtomCommon.h"
#include "v4l2device.h"

#ifdef ENABLE_INTEL_METABUFFER
#include "IntelMetadataBuffer.h"
#endif

#include "PlatformData.h"
#include "CameraConf.h"
#include "ScalerService.h"
#include "ICameraHwControls.h"
#include "IDvs.h"
#include "SensorHWExtIsp.h"
#include "SensorEmbeddedMetaData.h"
#include "CamHeapMem.h"

namespace android {

#define MAX_V4L2_BUFFERS    MAX_BURST_BUFFERS
#define MAX_CAMERA_NODES    5

#define MAX_DEVICE_NODE_CHAR_NR  32

/**
 *  Minimum resolution of video frames to have DVS ON.
 *  Under this it will be disabled
 **/
#define LARGEST_THUMBNAIL_WIDTH 320
#define LARGEST_THUMBNAIL_HEIGHT 240

#define DEFAULT_BUFFER_SHARING_SESSION_ID 0
struct devNameGroup
{
    char dev[MAX_CAMERA_NODES + 1][MAX_DEVICE_NODE_CHAR_NR];
    bool in_use;
};

enum NarrowGammaStatus {
    NARROW_GAMMA_ON = 0,
    NARROW_GAMMA_OFF = 1
};

class Callbacks;

class AtomISP :
    public IHWIspControl,
    public IHWFlashControl,
    public IHWLensControl,
    public IBufferOwner {

// constructor/destructor
public:
    explicit AtomISP(int cameraId, sp<ScalerService> scalerService, Callbacks *callbacks);
    virtual ~AtomISP();


    status_t init();
    void deInitDevice();
    bool isDeviceInitialized() const;

// prevent copy constructor and assignment operator
private:
    AtomISP(const AtomISP& other);
    AtomISP& operator=(const AtomISP& other);

// public methods
public:

    IHWSensorControl* getSensorControlInterface();

    void getDefaultParameters(CameraParameters *params, CameraParameters *intel_params);

    status_t configure(AtomMode mode);
    status_t allocateBuffers(AtomMode mode);
    status_t start();
    status_t stop();

    AtomMode getMode() const { return mMode; };

    status_t startOfflineCapture(ContinuousCaptureConfig &config);
    status_t stopOfflineCapture();
    bool isOfflineCaptureRunning() const;
    int shutterLagZeroAlign() const;
    int continuousBurstNegMinOffset(void) const;
    int continuousBurstNegOffset(int skip, int startIndex) const;
    int getContinuousCaptureNumber() const;
    status_t prepareOfflineCapture(ContinuousCaptureConfig &config);

    // APIs for capture with raw buffer lock
    status_t rawBufferUnlock(int expId);
    status_t rawBufferCapture(int expId);

    void setExternalIspActionHint(ExtIspActionHint hint);

    status_t returnRecordingBuffers();
    bool isSharedPreviewBufferConfigured(bool *reserved = NULL) const;

    // TODO: client no longer using, can be moved to privates
    status_t getPreviewFrame(AtomBuffer *buff);
    status_t putPreviewFrame(AtomBuffer *buff);

    status_t getJpegCapturePreviewFrame(AtomBuffer *buff);
    status_t putJpegCapturePreviewFrame(AtomBuffer *buff);

    status_t setGraphicPreviewBuffers(const AtomBuffer *buffs, int numBuffs, bool cached);
    status_t getRecordingFrame(AtomBuffer *buff);
    status_t putRecordingFrame(AtomBuffer *buff);

    status_t setSnapshotBuffers(Vector<AtomBuffer> *buffs, int numBuffs, bool cached);
    status_t getSnapshot(AtomBuffer *snaphotBuf, AtomBuffer *postviewBuf);
    status_t putSnapshot(AtomBuffer *snaphotBuf, AtomBuffer *postviewBuf);

    status_t setPostviewBuffers(Vector<AtomBuffer> *buffs, int numBuffs, bool cached);

    int pollCapture(int timeout);

    status_t requestJpegCapture();
    status_t startJpegModeContinuousShooting();
    status_t stopJpegModeContinuousShooting();

    bool dataAvailable();

    bool isHALZSLEnabled() const { return mHALZSLEnabled; }

    status_t setPreviewFrameFormat(int width, int height, int bpl, int fourcc = 0);
    status_t setPostviewFrameFormat(AtomBuffer& formatDescriptor);
    void getSnapshotFrameFormat(AtomBuffer& formatDescriptor) const { formatDescriptor = mConfig.snapshot; }
    void getPostviewFrameFormat(AtomBuffer& formatDescriptor) const;
    status_t setSnapshotFrameFormat(AtomBuffer& formatDescriptor);
    status_t setVideoFrameFormat(int width, int height, int fourcc = 0);
    bool applyISPLimitations(CameraParameters *params, bool dvsEnabled, bool videoMode);

    void setPreviewFramerate(int fps);
    int getSnapshotPixelFormat() { return mConfig.snapshot.fourcc; }
    void getVideoSize(int *width, int *height, int *bpl);
    void getPreviewSize(int *width, int *height, int *bpl);
    int getNumSnapshotBuffers();

    // set/get recording frame rate
    void setRecordingFramerate(int fps);
    int getRecordingFramerate() { return mConfig.recording_fps; }
    bool checkSkipFrameRecording(int frameNum);
    bool checkSkipFrameForVideoZoom();

    void setPreviewBufNum(int num);

    status_t initDVS();
    status_t setDVSSkipFrames(unsigned int skips);
    bool dvsEnabled();

    status_t enableFrameSyncEvent(bool enable);
    status_t pollFrameSyncEvent();

    // file input/injection API
    int configureFileInject(const char* fileName, int width, int height, int fourcc, int bayerOrder);
    bool isFileInjectionEnabled(void) const { return mFileInject.active; }
    String8 getFileInjectionFileName(void) const { return mFileInject.fileName; }

    void getZoomRatios(CameraParameters *params) const;
    void getFocusDistances(CameraParameters *params);

    // Enable metadata buffer mode API
    status_t storeMetaDataInBuffers(bool enabled, int sID);
    // AtomIspObserver controls
    status_t attachObserver(IAtomIspObserver *observer, ObserverType t);
    status_t detachObserver(IAtomIspObserver *observer, ObserverType t);
    void pauseObserver(ObserverType t);
    void startObserver(ObserverType t);

    // IBufferOwner override
    virtual void returnBuffer(AtomBuffer* buff);

    // high speed fps setting
    status_t setHighSpeedResolutionFps(char* resolution, int fps);

    status_t allocateMetaDataBuffers(AtomBuffer *buffers, int numBuffers);

    status_t setSRESmode(bool mode);

    AtomBuffer getSnapshotDescriptor() const { return mConfig.snapshot; }

    AtomBuffer getPostviewDescriptor() const { return mConfig.postview; }

    status_t setColorBarPattern(bool enable);

protected:
    /* [BEGIN] IHWFlashControl overloads, */
    status_t setFlash(int numFrames);
    status_t setFlashIndicator(int intensity);
    status_t setTorch(int intensity, unsigned int torchId = 0);
    int setFlashIntensity(int intensity, unsigned int flashId = 0);

    // Check if battery is too low for flash control
    BatteryStatus getBatteryStatus();

    /* [END] IHWFlashControl overloads, */

protected:
    /* [BEGIN] IHWLensControl overloads, */
    int moveFocusToPosition(int position);
    int moveFocusToBySteps(int steps);
    int getFocusPosition(int * position);
    int getFocusStatus(int *status);
    /* [END] IHWLensControl overloads, */

protected:
    /* [BEGIN] IHWSensorControl overloads, */
    int getCameraId(void);
    const char * getSensorName(void);
    float getFrameRate() const { return mConfig.fps; }
    virtual unsigned int getExposureDelay() { return PlatformData::getSensorExposureLag(mCameraId); };
    virtual int setExposure(struct atomisp_exposure *exposure);

    void getSensorData(sensorPrivateData *sensor_data);
    int getModeInfo(struct atomisp_sensor_mode_data *mode_data);
    int getExposureTime(int *exposure_time);
    int getAperture(int *aperture);
    int getFNumber(unsigned short  *fnum_num, unsigned short *fnum_denom);
    int setExposureTime(int time);
    int setExposureMode(v4l2_exposure_auto_type type);
    int getExposureMode(v4l2_exposure_auto_type * type);
    int setExposureBias(int bias);
    int getExposureBias(int * bias);
    int setSceneMode(v4l2_scene_mode mode);
    int getSceneMode(v4l2_scene_mode * mode);
    int setWhiteBalance(v4l2_auto_n_preset_white_balance mode);
    int getWhiteBalance(v4l2_auto_n_preset_white_balance * mode);
    int setIso(int iso);
    int getIso(int * iso);
    int setAeMeteringMode(v4l2_exposure_metering mode);
    int getAeMeteringMode(v4l2_exposure_metering * mode);
    int setAeFlickerMode(v4l2_power_line_frequency mode);
    int setAfMode(v4l2_auto_focus_range mode);
    int getAfMode(v4l2_auto_focus_range * mode);
    int setAfEnabled(bool enable);
    int set3ALock(int aaaLock);
    int get3ALock(int * aaaLock);
    int setAeFlashMode(v4l2_flash_led_mode mode);
    int getAeFlashMode(v4l2_flash_led_mode * mode);

    void getMotorData(sensorPrivateData *sensor_data);
    int getRawFormat();
    /* [END] IHWSensorControl overloads, */

protected:
    /* [BEGIN] IHWIspControl overloads */
    int getCssMajorVersion();
    int getCssMinorVersion();
    int getIspHwMajorVersion();
    int getIspHwMinorVersion();
    // Get the ISP pipeline output frame size
    void getOutputSize(int *width, int *height, int *bpl = NULL);
    // return zoom ratio multiplied by 100 from given zoom value
    int zoomRatio(int zoomValue) const;
    status_t setZoom(int zoom);
    int getDrvZoom(int zoom);

    status_t setColorEffect(v4l2_colorfx effect);
    status_t applyColorEffect();
    status_t getMakerNote(atomisp_makernote_info *info);
    status_t getContrast(int *value);
    status_t setContrast(int value);
    status_t getSaturation(int *value);
    status_t setSaturation(int value);
    status_t getSharpness(int *value);
    status_t setSharpness(int value);
    status_t setXNR(bool enable);
    bool getXNR() const { return mXnr; };
    status_t setLowLight(bool enable);
    bool getLowLight() const { return mLowLight; };
    status_t setGDC(bool enable);
    status_t setDVS(bool enable);
    void setNrEE(bool en);
    status_t setHDR(int mode);
    status_t setLLS(int mode);
    status_t setShotMode(int mode);

    /* ISP related controls */
    int setAicParameter(struct atomisp_parameters *aic_params);
    int setIspParameter(struct atomisp_parm *isp_params);
    int getIspStatistics(struct atomisp_3a_statistics *statistics);
    int setGdcConfig(const struct morph_table *tbl);
    int setShadingTable(struct atomisp_shading_table *table);
    int setMaccConfig(struct atomisp_macc_config *macc_cfg);
    int setCtcTable(const struct atomisp_ctc_table *ctc_tbl);
    int setDeConfig(struct atomisp_de_config *de_cfg);
    int setTnrConfig(struct atomisp_tnr_config *tnr_cfg);
    int setEeConfig(struct atomisp_ee_config *ee_cfg);
    int setNrConfig(struct atomisp_nr_config *nr_cfg);
    int setDpConfig(struct atomisp_dp_config *dp_cfg);
    int setWbConfig(struct atomisp_wb_config *wb_cfg);
    int setObConfig(struct atomisp_ob_config *ob_cfg);
    int set3aConfig(const struct atomisp_3a_config *cfg);
    int setGammaTable(const struct atomisp_gamma_table *gamma_tbl);
    int setGcConfig(const struct atomisp_gc_config *gc_cfg);

    int setDvsConfig(const struct atomisp_dvs_6axis_config *dvs_6axis_cfg);

    status_t getDvsStatistics(struct atomisp_dis_statistics *stats,
                              bool *tryAgain) const;
    status_t setMotionVector(const struct atomisp_dis_vector *vector) const;
    status_t getIspDvs2BqResolutions(struct atomisp_dvs2_bq_resolutions *bq_res) const;
    status_t setDvsCoefficients(const struct atomisp_dis_coefficients *coefs) const;
    status_t getIspParameters(struct atomisp_parm *isp_param) const;

    /* Sensor Embedded Metadata */
    int getSensorEmbeddedMetaData(atomisp_metadata *metaData) const;
    status_t getDecodedExposureParams(ia_aiq_exposure_sensor_parameters* sensor_exp_p,
                                      ia_aiq_exposure_parameters* generic_exp_p, unsigned int exp_id = 0);
    int getSensorFrameId(unsigned int exp_id);

    /* Acceleration API extensions */
    int loadAccFirmware(void *fw, size_t size, unsigned int *fwHandle);
    int loadAccPipeFirmware(void *fw, size_t size, unsigned int *fwHandle, int dst);
    int unloadAccFirmware(unsigned int fwHandle);
    int mapFirmwareArgument(void *val, size_t size, unsigned long *ptr);
    int unmapFirmwareArgument(unsigned long val, size_t size);
    int setFirmwareArgument(unsigned int fwHandle, unsigned int num,
                            void *val, size_t size);
    int setMappedFirmwareArgument(unsigned int fwHandle, unsigned int mem,
                                  unsigned long val, size_t size);
    int unsetFirmwareArgument(unsigned int fwHandle, unsigned int num);
    int startFirmware(unsigned int fwHandle);
    int waitForFirmware(unsigned int fwHandle);
    int abortFirmware(unsigned int fwHandle, unsigned int timeout);
    int setStageState(unsigned int fwHandle, bool enable);
    int waitStageUpdate(unsigned int fwHandle);
    /* [END] IHWIspControl overloads */

// private types
private:

    static const int V4L2_MAIN_DEVICE       = 0;
    static const int V4L2_POSTVIEW_DEVICE   = 1;
    static const int V4L2_PREVIEW_DEVICE    = 2;
    static const int V4L2_RECORDING_DEVICE  = 3;
    static const int V4L2_INJECT_DEVICE     = 4;
    static const int V4L2_ISP_SUBDEV        = 5;
    static const int V4L2_ISP_SUBDEV2       = 6;

    /**
     * Maximum number of V4L2 devices node we support
     */
    static const int V4L2_MAX_DEVICE_COUNT  = V4L2_ISP_SUBDEV2 + 1;

    struct VideoNodeLimits {
        int minWidht;
        int minHeight;
        int maxWidth;
        int maxHeight;
    };

    struct Config {
        AtomBuffer preview;    // preview video node config
        AtomBuffer recording;  // recording video node config
        AtomBuffer snapshot;   // snapshot video node config
        AtomBuffer postview;   // postview (thumbnail for capture) video node config
        AtomBuffer HALZSL;     // HAL ZSL video node config
        VideoNodeLimits previewLimits;
        VideoNodeLimits recordingLimits;
        VideoNodeLimits snapshotLimits;
        VideoNodeLimits postviewLimits;
        float fps;                  // preview/recording (shared) output by sensor
        int preview_fps;            // preview fps requested by user
        int recording_fps;          // recording fps requested by user
        int num_snapshot;           // number of snapshots to take
        int num_postviews;          // number of allocated postviews
        int num_recording_buffers;  // number of recording buffers used
        int num_preview_buffers;    // number of preview buffers used
        int num_snapshot_buffers;   // number of snapshot buffer used
        int num_postview_buffers;   // number of postview buffer used
        int zoom;                   // zoom value
    };

// private methods
private:
    status_t initDevice();
    bool isPostviewInitialized() const;

    void computeZoomRatiosLinear();
    void computeZoomRatiosFactor();
    status_t computeZoomRatios();

    status_t initCameraInput();
    void initFileInject();
    void initFrameConfig();

    void dropACCStageUpdatesLocked(unsigned int fwHandle);
    void dropACCStageUpdates(unsigned int fwHandle);

    status_t configurePreview();
    status_t startPreview();
    status_t stopPreview();
    status_t configureRecording();
    status_t startRecording();
    status_t stopRecording();
    status_t configureCapture();
    status_t configureContinuousMode(bool enable);
    status_t configureDepthMode(bool enable);
    status_t configureContinuousRingBuffer();
    status_t configureContinuous();
    status_t configureContinuousJpegCapture();
    status_t configureContinuousSOC();
    status_t configureContinuousVideo();
    status_t configureContinuousJpegVideo();
    status_t startCapture();
    status_t stopCapture();
    status_t stopContinuousPreview();
    status_t stopContinuousVideo();

    status_t requestContCapture(int numCaptures, int offset, unsigned int skip);
    status_t rawBufferLockEnable(bool enable);

    void runStartISPActions();
    void runStopISPActions();

    Size getHALZSLResolution();
    status_t allocateHALZSLBuffers();
    status_t freeHALZSLBuffers();
    status_t getHALZSLPreviewFrame(AtomBuffer *buff);
    status_t putHALZSLPreviewFrame(AtomBuffer *buff);
    AtomBuffer* findMatchingHALZSLPreviewFrame(int frameCounter);
    void copyOrScaleHALZSLBuffer(const AtomBuffer &captureBuf, const AtomBuffer *previewBuf,
            AtomBuffer *targetBuf, const AtomBuffer &localBuf, float zoomFactor) const;
    status_t getHALZSLSnapshot(AtomBuffer *snapshotBuf, AtomBuffer *postviewBuf);
    bool waitForHALZSLBuffer(Vector<AtomBuffer> &vector, bool snapshot);
    void dumpHALZSLBufs();
    void dumpHALZSLPreviewBufs();

    status_t allocatePreviewBuffers();
    status_t allocateRecordingBuffers();
    status_t allocateSnapshotBuffers();
    status_t allocateMetaDataBuffers();
    status_t freePreviewBuffers();
    status_t freeRecordingBuffers();
    status_t freeSnapshotBuffers();
    status_t freePostviewBuffers();
    bool needNewPostviewBuffers();

    status_t stageUpdate(unsigned int stageHandle);
    status_t newAccPipeFw(unsigned int handle);

#ifdef ENABLE_INTEL_METABUFFER
    void initMetaDataBuf(IntelMetadataBuffer* metaDatabuf);
#endif

    void getMaxSnapShotSize(int cameraId, int* width, int* height);
    void getMaxVideoSize(int cameraId, int* width, int* height);

    status_t updateLowLight();
    status_t setTorchHelper(int intensity, int torchId = 0);
    status_t updateCaptureParams();

    int detectDeviceResolutions();
    int atomisp_set_capture_mode(int deviceMode);

    int configureDevice(V4L2VideoNode *device, int deviceMode, AtomBuffer *formatDescriptor, bool raw);

    int atomisp_set_zoom (int zoom);

    int setNarrowGamma(const struct atomisp_formats_config *formats_config);

    int startFileInject(void);
    int stopFileInject(void);
    status_t fileInjectSetSize(void);

    size_t setupCameraInfo();
    unsigned int getNumOfSkipFrames(void);
    unsigned int getNumOfSkipStatistics(void);
    status_t applySensorFlip(void);
    void fetchIspVersions();

    // TODO: Remove once BZ #119181 gets fixed by the firmware team!!
    int detectCorruptStatistics(struct atomisp_3a_statistics *statistics);

    bool checkSkipFrame(int frameNum, int targetFPS);
    status_t setSkipFramesForVideoZoom();
    inline bool inContinuousMode() const;
    inline bool inVideoMode() const;

    void initBufferArray(AtomBuffer *buffers, AtomBuffer &formatDescriptor, void **bufPool, int bufPoolNum);
    status_t configureMultiStreamsContinuousSOC();
    status_t configurePreviewAndCaptureStreamsSOC();
    status_t allocateMultiStreamsHALZSLBuffers();
    status_t freeMultiStreamsHALZSLBuffers();
    status_t getMultiStreamsHALZSLPreviewFrame(AtomBuffer *buff);
    status_t putMultiStreamsHALZSLPreviewFrame(AtomBuffer *buff);
    status_t getMultiStreamsHALZSLSnapshot(AtomBuffer *snapshotBuf, AtomBuffer *postviewBuf);
    status_t configureMultiStreamsContinuousVideoSOC();

private:
    // AtomIspObserver
    IObserverSubject* observerSubjectByType(ObserverType t);

    // Observer subject sub-classes
    class PreviewStreamSource: public IObserverSubject
    {
    public:
        PreviewStreamSource(const char*name, AtomISP *aisp)
            :mName(name), mISP(aisp), mISPTimeoutCount(0) { };

        // IObserverSubject override
        virtual const char* getName() { return mName.string(); };
        virtual status_t observe(IAtomIspObserver::Message *msg);

    private:
        String8  mName;
        AtomISP *mISP;
        int mISPTimeoutCount;
    } mPreviewStreamSource;

    class AAAStatSource: public IObserverSubject
    {
    public:
        AAAStatSource(const char*name, AtomISP *aisp)
            :mName(name), mISP(aisp) { };

        // IObserverSubject override
        virtual const char* getName() { return mName.string(); };
        virtual status_t observe(IAtomIspObserver::Message *msg);

    private:
        String8  mName;
        AtomISP *mISP;
    } m3AStatSource;

// private members
private:
    int mCameraId;

    // Embedded Metadata
    SensorEmbeddedMetaData*  mSensorEmbeddedMetaData;

    // Dvs
    IDvs *mDvs;
    bool mDvsEnabled;

    // Dual Mode
    int mGroupIndex;
    static Mutex sISPCountLock;

    AtomMode mMode;
    Callbacks *mCallbacks;

    Vector <AtomBuffer> mPreviewBuffers;
    bool mPreviewBuffersCached;
    AtomBuffer *mRecordingBuffers;
    bool mSwapRecordingDevice;
    bool mRecordingDeviceSwapped;

    bool mHALZSLEnabled; // use only one stream or 3 streams. not use raw ring buffers in driver like the raw sesnor, use buffer queue in hal instead.
    bool mHALSDVEnabled; // use 4 streams. not use raw ring buffers in driver like the raw sensor, use buffer queue in hal instead.
    bool mUseMultiStreamsForSoC; // this could be configured by according to the configuration file
    AtomBuffer *mHALZSLBuffers; // the 1 stream hal zsl will use it
    Vector<AtomBuffer> mHALZSLPreviewBuffers; // the 1 stream hal zsl will use it
    Vector<AtomBuffer> mHALZSLCaptureBuffers; // store the capture data in the hal
    Mutex mHALZSLLock;
    static const unsigned int sMaxHALZSLBuffersHeldInHAL = 2;
    static const int sNumHALZSLBuffers = sMaxHALZSLBuffersHeldInHAL + 4;
    static const int sHALZSLRetryCount = 5;
    static const int sHALZSLRetryUSleep = 33000;
    static const int ISP_DEVICE_NAME_LENGTH_MAX = 1024;

    bool mContinuousJpegCaptureEnabled;

    AtomBuffer *mMultiStreamsHALZSLCaptureBuffers;
    AtomBuffer *mMultiStreamsHALZSLPostviewBuffers;
    Vector<AtomBuffer> mMultiStreamsHALZSLCaptureBuffersQueue; // this queue is used to buffer the capture stream in hal
    Vector<AtomBuffer> mMultiStreamsHALZSLPostviewBuffersQueue; // this queue is used to buffer the postview stream in hal

    bool mClientSnapshotBuffersCached;
    bool mUsingClientSnapshotBuffers;
    bool mUsingClientPostviewBuffers;
    bool mStoreMetaDataInBuffers;
    int  mBufferSharingSessionID;

    Mutex mACCEventSystemLock;
    struct EventWaiter {
        Condition mWaitCond;
        Mutex mWaitLock;
        unsigned int handle;
        bool enabled;
    };
    KeyedVector<unsigned int, EventWaiter *> mAllWaiters;
    KeyedVector<unsigned int, EventWaiter *> mSleepingWaiters;
    KeyedVector<unsigned int, unsigned int>  mUpdates;

    AtomBuffer mSnapshotBuffers[MAX_BURST_BUFFERS];
    Vector <AtomBuffer> mPostviewBuffers;
    int mNumPreviewBuffersQueued;       /* TODO: move this tracking var to device video node class */
    int mNumRecordingBuffersQueued;
    int mNumCapturegBuffersQueued;
    int mFlashTorchSetting;
    Config mConfig;
    ContinuousCaptureConfig mContCaptConfig;
    bool mContCaptPrepared;
    unsigned int mInitialSkips;
    unsigned int mStatisticSkips;
    unsigned int mDVSFrameSkips;
    unsigned int mVideoZoomFrameSkips;

    int dumpFrameInfo(AtomMode mode);

    Mutex mDeviceMutex[V4L2_MAX_DEVICE_COUNT];  /*!< Used to ensure thread safety in some operations on the devices*/
    sp<V4L2VideoNode>  mMainDevice;
    sp<V4L2VideoNode>  mPreviewDevice;
    sp<V4L2VideoNode>  mRecordingDevice;
    sp<V4L2VideoNode>  mPostViewDevice;
    sp<V4L2Subdevice>  m3AEventSubdevice;
    sp<V4L2VideoNode>  mOriginalPreviewDevice;
    sp<V4L2VideoNode>  mFileInjectDevice;
    sp<SensorHW>       mSensorHW;              /* AtomISP owns this! */

    int dumpPreviewFrame(int previewIndex);
    int dumpRecordingFrame(int recordingIndex);
    int dumpSnapshot(int snapshotIndex, int postviewIndex);
    int dumpRawImageFlush(void);
    bool isDumpRawImageReady(void);
    bool isAllowedToSetFps(V4L2VideoNode *device, int deviceMode) const;

    bool mIsFileInject;
    struct FileInject {
        String8 fileName;
        bool active;
        AtomBuffer   formatDescriptor;
        int bayerOrder;
        char *dataAddr;
        void clear() {
            fileName.clear();
            active = false;
            CLEAR(formatDescriptor);
            dataAddr = NULL;
        };
    } mFileInject;

    int mSessionId; // uniquely identify each session

    SensorType mSensorType;

    bool mLowLight;
    int mXnr;

    Vector <int> mZoomRatioTable;
    Vector <int> mZoomDriveTable;
    char *mZoomRatios;

    int mRawDataDumpSize;
    int m3AStatRequested;
    bool m3AStatscEnabled;
    bool mSensorEmbeddedMetaDataSupported;
    v4l2_colorfx mColorEffect;

    sp<ScalerService> mScaler;

    AtomIspObserverManager mObserverManager;

    int mCssMajorVersion;
    int mCssMinorVersion;
    int mIspHwMajorVersion;
    int mIspHwMinorVersion;

    bool mHALVideoStabilization;
    bool mHALVideoNormal;
    bool mExtIspVideoHighSpeed;
    bool mNoiseReductionEdgeEnhancement;

    // Sensor helper fields
    Vector <v4l2_fmtdesc>    mSensorSupportedFormats;     /*!< List of V4L2 pixel format supported by the sensor */
    bool mFlashIsOn;                                    //!< Used in corrupt statistics detection to avoid dropping
                                                        // stats when flash is used */
}; // class AtomISP

}; // namespace android

#endif // ANDROID_LIBCAMERA_ATOM_ISP
