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

#ifndef ANDROID_LIBCAMERA_CONTROL_THREAD_H
#define ANDROID_LIBCAMERA_CONTROL_THREAD_H

#include <utils/threads.h>
#include <limits.h>

#include <camera.h>
#include <camera/CameraParameters.h>
#include "IntelParameters.h"
#include <utils/List.h>
#include "MessageQueue.h"
#include "PreviewThread.h"
#include "PictureThread.h"
#include "VideoThread.h"
#include "AtomCommon.h"
#include "CallbacksThread.h"
#include "AAAThread.h"
#include "I3AControls.h"
#include "CameraConf.h"
#include "PostProcThread.h"
#include "PanoramaThread.h"
#include "SensorThread.h"
#include "ScalerService.h"
#include "WarperService.h"
#include "PostCaptureThread.h"
#include "CameraDump.h"
#include "CameraAreas.h"
#include "AtomCP.h"
#include "UltraLowLight.h"
#include "BracketManager.h"
#include "I3AControls.h"
#include "IAtomIspObserver.h"
#include "PictureThread.h"
#include "SensorThread.h"
#include "ICameraHwControls.h"
#include "AccManagerThread.h"
#include "ThermalThrottleThread.h"

namespace android {

class AtomISP;
//
// ControlThread implements most of the operations defined
// by camera_device_ops_t. Refer to hardware/camera.h
// for documentation on each operation.
//
class ControlThread :
    public Thread,
    public ICallbackPreview,
    public ICallbackPicture,
    public ICallbackAAA,
    public ICallbackPostProc,
    public ICallbackPanorama,
    public IAtomIspObserver,
    public IPostCaptureProcessObserver,
    public IBufferOwner,
    public IOrientationListener {

// constructor destructor
public:
    explicit ControlThread(int cameraId);
    virtual ~ControlThread();

// prevent copy constructor and assignment operator
private:
    ControlThread(const ControlThread& other);
    ControlThread& operator=(const ControlThread& other);

public:
    virtual bool atomIspNotify(IAtomIspObserver::Message *msg, const ObserverState state);

// Thread overrides
public:
    status_t requestExitAndWait();

// public methods
public:

    status_t init();
    void deinit();

    status_t setPreviewWindow(struct preview_stream_ops *window);

    // message callbacks
    void setCallbacks(camera_notify_callback notify_cb,
                      camera_data_callback data_cb,
                      camera_data_timestamp_callback data_cb_timestamp,
                      camera_request_memory get_memory,
                      void* user);
    void enableMsgType(int32_t msg_type);
    void disableMsgType(int32_t msg_type);
    bool msgTypeEnabled(int32_t msg_type);

    status_t startPreview();
    // synchronous (blocking) state machine methods
    status_t stopPreview();
    status_t errorPreview();
    status_t startRecording();
    status_t stopRecording();
    status_t recoverPreview();

    void sendCommand( int32_t cmd, int32_t arg1, int32_t arg2);

    // return true if preview or recording is enabled
    bool previewEnabled();
    status_t storeMetaDataInBuffers(bool enabled);
    bool recordingEnabled();

    // parameter APIs
    status_t setParameters(const char *params);
    char *getParameters();
    void putParameters(char *params);
    status_t updateParameterCache(void);

    // snapshot (asynchronous)
    status_t takePicture();
    status_t cancelPicture();

    // autofocus commands (asynchronous)
    status_t autoFocus();
    status_t cancelAutoFocus();

    // return recording frame.
    status_t releaseRecordingFrame(void *buff);

    void atomRelease();

    // IBufferOwner override
    virtual void returnBuffer(AtomBuffer* buff);

    // Implementation of IPostCaptureProcessObserver interface
    void postCaptureProcesssingDone(IPostCaptureProcessItem* item, status_t status);

    // IOrientationListener
    void orientationChanged(int orientation);

// callback methods
private:
    virtual void previewBufferCallback(AtomBuffer *buff, ICallbackPreview::CallbackType t);
    virtual void atPostviewPresent();
    virtual void encodingDone(AtomBuffer *snapshotBuf, AtomBuffer *postviewBuf);
    virtual void pictureDone(AtomBuffer *snapshotBuf, AtomBuffer *postviewBuf);
    virtual void postProcCaptureTrigger();
    virtual void sceneDetected(String8 sceneMode, bool sceneHdr);
    virtual void facesDetected(const ia_face_state *faceState);
    virtual void lowLightDetected(bool needLLS);
    virtual void panoramaCaptureTrigger();
    virtual void panoramaFinalized(AtomBuffer *buff, AtomBuffer *pvBuff);

// private types
private:

    // thread message id's
    enum MessageId {

        MESSAGE_ID_EXIT = 0,            // call requestExitAndWait
        MESSAGE_ID_START_PREVIEW,
        MESSAGE_ID_RESTART_PREVIEW,
        MESSAGE_ID_STOP_PREVIEW,
        MESSAGE_ID_ERROR_PREVIEW,
        MESSAGE_ID_START_RECORDING,
        MESSAGE_ID_STOP_RECORDING,
        MESSAGE_ID_TAKE_PICTURE,
        MESSAGE_ID_SMART_SHUTTER_PICTURE,
        MESSAGE_ID_CANCEL_PICTURE,
        MESSAGE_ID_AUTO_FOCUS,
        MESSAGE_ID_CANCEL_AUTO_FOCUS,
        MESSAGE_ID_RELEASE_PREVIEW_FRAME,//This is only a callback from other
                                         // HAL threads to signal preview buffer
                                         // is not used and is free to queue back
                                         // AtomISP.
        MESSAGE_ID_RELEASE,
        MESSAGE_ID_PREVIEW_STARTED,
        MESSAGE_ID_ENCODING_DONE,
        MESSAGE_ID_PICTURE_DONE,
        MESSAGE_ID_SET_PARAMETERS,
        MESSAGE_ID_GET_PARAMETERS,
        MESSAGE_ID_COMMAND,
        MESSAGE_ID_FACES_DETECTED,
        MESSAGE_ID_SET_PREVIEW_WINDOW,
        MESSAGE_ID_SCENE_DETECTED,
        MESSAGE_ID_RETURN_BUFFER,
        MESSAGE_ID_PANORAMA_PICTURE,
        MESSAGE_ID_PANORAMA_CAPTURE_TRIGGER,
        MESSAGE_ID_PANORAMA_FINALIZE,
        MESSAGE_ID_POST_PROC_CAPTURE_TRIGGER,

        // Message for enabling metadata buffer mode
        MESSAGE_ID_STORE_METADATA_IN_BUFFER,

        MESSAGE_ID_POST_CAPTURE_PROCESSING_DONE,
        MESSAGE_ID_SET_ORIENTATION,

        // timeout handler
        MESSAGE_ID_TIMEOUT,
        // max number of messages
        MESSAGE_ID_MAX
    };

    //
    // message data structures
    //

    struct MessageReturnBuffer {
        AtomBuffer returnBuf;
    };

    struct MessagePicture {
        AtomBuffer snapshotBuf;
        AtomBuffer postviewBuf;
    };

    struct MessageGetParameters {
        char** params;
    };

    struct MessageExit {
        bool stopThread;
    };

    struct MessageSetParameters {
        char* params;
        bool stopPreviewRequest;
    };

    struct MessageCommand{
        int32_t cmd_id;
        int32_t arg1;
        int32_t arg2;
    };

    struct MessageFacesDetected {
        camera_frame_metadata_t* meta;
        AtomBuffer buf;
    };

    struct MessagePreviewWindow {
        struct preview_stream_ops *window;
        bool synchronous;
    };

    struct MessageStoreMetaDataInBuffers {
        bool enabled;
    };

    struct MessageSceneDetected {
        char sceneMode[SCENE_STRING_LENGTH];
        bool sceneHdr;
    };

    struct MessagePanoramaFinalize {
        AtomBuffer buff;
        AtomBuffer pvBuff;
    };

    struct MessagePostCaptureProcDone {
        IPostCaptureProcessItem* item;
        status_t status;
    };

    struct MessageOrientation {
        int value;
    };

    // union of all message data
    union MessageData {

        // MESSAGE_ID_ENCODING_DONE
        MessagePicture encodingDone;

        // MESSAGE_ID_PICTURE_DONE
        MessagePicture pictureDone;

        // MESSAGE_ID_GET_PARAMETERS
        MessageGetParameters getParameters;

        // MESSAGE_ID_SET_PARAMETERS
        MessageSetParameters setParameters;

        // MESSAGE_ID_COMMAND
        MessageCommand command;
        //MESSAGE_ID_FACES_DETECTED
        MessageFacesDetected FacesDetected;

        // MESSAGE_ID_SET_PREVIEW_WINDOW
        MessagePreviewWindow    previewWin;

        // MESSAGE_ID_STORE_METADATA_IN_BUFFER
        MessageStoreMetaDataInBuffers storeMetaDataInBuffers;

        // MESSAGE_ID_SCENE_DETECTED
        MessageSceneDetected    sceneDetected;

        // MESSAGE_ID_PANORAMA_FINALIZE
        MessagePanoramaFinalize   panoramaFinalized;

        // MESSAGE_ID_POST_CAPTURE_PROCESSING_DONE
        MessagePostCaptureProcDone postCapture;

        // MESSAGE_ID_SET_ORIENTATION
        MessageOrientation  orientation;

        // MESSAGE_ID_EXIT
        MessageExit exit;

        // MESSAGE_ID_RETURN_BUFFER
        MessageReturnBuffer returnBuf;
    };

    // message id and message data
    struct Message {
        MessageId id;
        MessageData data;
    };

    // thread states
    enum State {
        STATE_STOPPED,
        STATE_PREVIEW_STILL,
        STATE_PREVIEW_VIDEO,
        STATE_RECORDING,
        STATE_CAPTURE,
        STATE_CONTINUOUS_CAPTURE,
        STATE_JPEG_CAPTURE
    };

    // capture substates
    // Note: keep in sync with helper array of string used for logging
    // CaptureSubstateStrings
    enum CaptureSubState {
        STATE_CAPTURE_INIT,          // initial capture state
        STATE_CAPTURE_STARTED,       // when takePicture is received
        STATE_CAPTURE_ENCODING_DONE, // when encoding done callback is received
        STATE_CAPTURE_PICTURE_DONE,  // when picture done callback is received
        STATE_CAPTURE_IDLE,          // when preview is started again
        STATE_CAPTURE_CONTINUOUS_SHOOTING,  // when pictures is captured continuously
        STATE_CAPTURE_LAST           // Fake state used to calculate number of states keep always last
    };

    /**
     * \enum ShootingMode
     * Describes the active shooting mode
     */
    enum ShootingMode {
        SHOOTING_MODE_NONE,         /*!< initial value */
        SHOOTING_MODE_SINGLE,       /*!< Normal single shot */
        SHOOTING_MODE_BURST,        /*!< Burst of x frames; x<MAX_BURST_LEN (Platform dependent) */
        SHOOTING_MODE_ZSL,          /*!< Zero Shutter lag shoot*/
        SHOOTING_MODE_VIDEO_SNAP,   /*!< Picture taking while recording is active */
        SHOOTING_MODE_ZSL_BURST,    /*!< Burst of ZSL where we can take images before the shutter is pressed */
        SHOOTING_MODE_ULL,          /*!< Ultra LowLight */
        SHOOTING_MODE_JPEG,         /*!< Continuous JPEG capture */
        SHOOTING_MODE_EXTISP_HDR_LLS, /*!< ext-isp HDR/LLS capture */
        SHOOTING_MODE_CONTINUOUS_SHOOTING,   /*!< Continuous shooting */
        SHOOTING_MODE_SMARTSTABILIZATION, /*!< Smart Stabilization capture */
    };

    /**
     * \enum ContinuousShootingStatus
     */
    enum ContinuousShootingState {
        CONT_SHOOTING_NONE      = 0x1,  /*!< initial value*/
        CONT_SHOOTING_PREPARED  = 0x2,  /*!< prepared for the first capture*/
        CONT_SHOOTING_STARTED   = 0x4,  /*!< shooting started, usually after the first catpure*/
    };

    struct HdrImaging {
        BracketingMode bracketMode;
        BracketingMode savedBracketMode;
        int  bracketNum;
        bool enabled;
        bool inProgress;
        bool saveOrig;
        AtomBuffer outMainBuf;
        AtomBuffer outPostviewBuf;
        CiUserBuffer ciBufIn;
        CiUserBuffer ciBufOut;
        MessagePicture *inputBuffers;
    };

    struct ExtIsp {
        bool LLS;
        bool HDR;
        bool kidsMode;
    };

    struct StillPicParamsCtx {
        int    snapshotWidth;
        int    snapshotHeight;
        int    thumbnailWidth;
        int    thumbnailHeigth;
        String8 supportedSnapshotSizes;
        String8 suportedThumnailSizes;
        void clear() {
            supportedSnapshotSizes.clear();
            suportedThumnailSizes.clear();
        };
    };

    class TemporarySetting : public RefBase
    {
    public:
        TemporarySetting(ControlThread *controlThread) : mControlThread(controlThread) {}
        virtual ~TemporarySetting() {}
        virtual void set() = 0;   // generic setting "setter"
        virtual void reset() = 0; // generic setting "resetter"
    protected:
        ControlThread *mControlThread;
    };
    friend class TemporarySetting;
    class AutoReset : public RefBase // this class allows to do for arbitrary settings similar to what Mutex::AutoLock does for locks
    {
    public:
        AutoReset(TemporarySetting *setting) : mTemporarySetting(setting) { mTemporarySetting->set(); }
        ~AutoReset() { mTemporarySetting->reset(); }
    private:
        sp<TemporarySetting> mTemporarySetting;
    };

// private methods
private:

    // state machine helper functions
    status_t restartPreview(bool videoMode);
    status_t startPreviewCore(bool videoMode);
    status_t stopPreviewCore(bool flushPictures = true);

    status_t initContinuousJpegCapture();
    status_t initContinuousCapture();
    void releaseContinuousCapture(bool flushPictures);
    status_t startOfflineCapture();
    status_t getSnapshot(AtomBuffer *snapshot, AtomBuffer *postview);
    status_t putSnapshot(AtomBuffer *snapshot, AtomBuffer *postview);
    State selectPreviewMode(const CameraParameters &params);
    ShootingMode selectShootingMode();
    status_t handleContinuousPreviewBackgrounding();
    status_t handleContinuousPreviewForegrounding();
    void flushContinuousPreviewToDisplay(nsecs_t snapshotTs);
    void continuousConfigApplyLimits(ContinuousCaptureConfig &cfg) const;
    status_t configureContinuousRingBuffer();
    status_t continuousStartStillCapture(bool useFlash);

    // thread message execution functions
    status_t handleMessageExit(MessageExit *msg);
    status_t handleMessageStartPreview();
    status_t handleMessageStopPreview();
    status_t handleMessageErrorPreview();
    status_t handleMessageStartRecording();
    status_t handleMessageStopRecording();
    status_t handleMessageTakePicture();
    status_t handleMessageTakeSmartShutterPicture();
    status_t handleMessageCancelPicture();
    status_t handleMessageAutoFocus();
    status_t handleMessageCancelAutoFocus();
    status_t handleMessagePreviewStarted();
    status_t handleMessageEncodingDone(MessagePicture *msg);
    status_t handleMessagePictureDone(MessagePicture *msg);
    status_t handleMessageSetParameters(MessageSetParameters *msg);
    status_t handleMessageGetParameters(MessageGetParameters *msg);
    status_t handleMessageCommand(MessageCommand* msg);
    status_t handleMessageSetPreviewWindow(MessagePreviewWindow *msg);
    status_t handleMessageStoreMetaDataInBuffers(MessageStoreMetaDataInBuffers *msg);
    status_t handleMessagePanoramaPicture();
    status_t handleMessagePanoramaCaptureTrigger();
    status_t handleMessagePanoramaFinalize(MessagePanoramaFinalize *msg);
    status_t handleMessageReturnBuffer(MessageReturnBuffer *msg);
    status_t handleMessageTimeout();
    status_t handleMessagePostCaptureProcessingDone(MessagePostCaptureProcDone *msg);
    status_t handleMessageSetOrientation(MessageOrientation *msg);

    status_t startFaceDetection();
    status_t stopFaceDetection(bool wait=false);
    status_t enableFocusMoveMsg(bool enable);
    status_t startSmartShutter(SmartShutterMode mode);
    status_t stopSmartShutter(SmartShutterMode mode);
    status_t cancelSmartShutterPicture();
    status_t forceSmartShutterPicture();

    status_t handleMessageFacesDetected(MessageFacesDetected* msg);
    status_t startSmartSceneDetection();
    status_t stopSmartSceneDetection();
    status_t handleMessageStopCapture();
    status_t enableIntelParameters();
    void releasePreviewFrame(AtomBuffer* buff);
    status_t handleMessageSceneDetected(MessageSceneDetected *msg);
    status_t startPanorama();
    status_t stopPanorama();
    status_t startFaceRecognition();
    status_t stopFaceRecognition();
    status_t enableIspExtensions();
    status_t handleMessageRelease();
    status_t handleKidsMode(int value);
    status_t handleLowLightMode(bool enableLLS);

    status_t cancelPictureThread();
    status_t cancelPostCaptureThread();
    status_t cancelCapture();

    // main message function
    status_t waitForAndExecuteMessage();

    AtomBuffer* findBufferByData(AtomBuffer *buf,Vector<AtomBuffer> *aVector);
    AtomBuffer* rmBufferInQueue(AtomBuffer *buf,Vector<AtomBuffer> *aVector);

    status_t skipFrames(size_t numFrames);
    status_t initBracketing();
    status_t applyBracketing();
    status_t skipPreviewFrames(int numFrames, AtomBuffer* buff);
    status_t setSmartSceneParams();

    bool runPreFlashSequence();
    void waitForAllocatedSnapshotBuffers();

    // parameters handling functions
    bool isParameterSet(const char* param);
    String8 paramsReturnNewIfChanged(const CameraParameters *oldParams,
            CameraParameters *newParams,
            const char *key);
    bool paramsHasPictureSizeChanged(const CameraParameters *oldParams,
            CameraParameters *newParams) const;
    bool hasPictureFormatChanged();

    // Process flashmode based on shooting mode criteria etc.
    // E.g., changes supported flash modes in burst and HDR modes.
    // NOTE: Need to call processParamHDR() and processParamBurst() before
    // this!
    void preProcessFlashMode(CameraParameters *newParams);

    // These are parameters that can be set while the ISP is running (most params can be
    // set while the isp is stopped as well).
    status_t processDynamicParameters(const CameraParameters *oldParams,
            CameraParameters *newParams);
    status_t processParamDvs(const CameraParameters *oldParams, CameraParameters *newParams);
    status_t processParamDualVideo(const CameraParameters *oldParams,
            CameraParameters *newParams, bool &restartPreview);
    status_t processParamDualCameraMode(CameraParameters *oldParams,
            CameraParameters *newParams);
    status_t processParamContinuousShooting(const CameraParameters *oldParams,
                CameraParameters *newParams, bool &restartPreview);
    status_t processParamBurst(const CameraParameters *oldParams,
                CameraParameters *newParams);
    status_t processParamFlash(const CameraParameters *oldParams,
                CameraParameters *newParams);
    status_t processParamAELock(const CameraParameters *oldParams,
                CameraParameters *newParams);
    status_t processParamAFLock(const CameraParameters *oldParams,
                CameraParameters *newParams);
    status_t processParamAWBLock(const CameraParameters *oldParams,
            CameraParameters *newParams);
    status_t processParamEffect(const CameraParameters *oldParams,
            CameraParameters *newParams);
    status_t processParamSceneMode(CameraParameters *oldParams,
            CameraParameters *newParams, bool &restartNeeded);
    status_t processParamXNR_ANR(const CameraParameters *oldParams,
            CameraParameters *newParams, bool &restartNeeded);
    status_t processParamColorBar(const CameraParameters *oldParams,
            CameraParameters *newParams, bool &restartNeeded);
    status_t processParamAntiBanding(const CameraParameters *oldParams,
                                           CameraParameters *newParams);
    status_t processParamFocusMode(const CameraParameters *oldParams,
            CameraParameters *newParams);
    status_t processParamWhiteBalance(const CameraParameters *oldParams,
            CameraParameters *newParams);
    status_t processParamSetMeteringAreas(const CameraParameters * oldParams,
            CameraParameters * newParams);
    status_t processParamBracket(const CameraParameters *oldParams,
                CameraParameters *newParams, bool &restartNeeded);
    status_t processParamSmartShutter(const CameraParameters *oldParams,
                CameraParameters *newParams);
    status_t processParamHDR(const CameraParameters *oldParams,
            CameraParameters *newParams);
    status_t processParamSDV(const CameraParameters *oldParams,
            CameraParameters *newParams, bool *restartNeeded);
    status_t processParamULL(const CameraParameters *oldParams,
            CameraParameters *newParams, bool *restartNeeded);
    status_t processParamExposureCompensation(const CameraParameters *oldParams,
            CameraParameters *newParams);
    status_t processParamAutoExposureMode(const CameraParameters *oldParams,
            CameraParameters *newParams);
    status_t processParamAutoExposureMeteringMode(const CameraParameters *oldParams,
            CameraParameters *newParams);
    status_t processParamIso(const CameraParameters *oldParams,
            CameraParameters *newParams);
    status_t processParamContrast(const CameraParameters *oldParams,
            CameraParameters *newParams);
    status_t processParamSaturation(const CameraParameters *oldParams,
            CameraParameters *newParams);
    status_t processParamSharpness(const CameraParameters *oldParams,
            CameraParameters *newParams);
    status_t processParamShutter(const CameraParameters *oldParams,
            CameraParameters *newParams);
    status_t processParamIntelligentMode(const CameraParameters *oldParams,
        CameraParameters *newParams);
    status_t processParamRawDataFormat(const CameraParameters *oldParams,
            CameraParameters *newParams, bool &previewRestartNeeded);
    // NOTE: processParamPreviewFrameRate is deprecated since Android API level 9
    status_t processParamPreviewFrameRate(const CameraParameters *oldParams,
            CameraParameters *newParams);
    status_t processPreviewCallbackSize(const CameraParameters *oldParams,
            int videomode);
    status_t ProcessOverlayEnable(const CameraParameters *oldParams,
            CameraParameters *newParams);
    // EXIF data
    status_t processParamExifMaker(const CameraParameters *oldParams,
            CameraParameters *newParams);
    status_t processParamExifModel(const CameraParameters *oldParams,
            CameraParameters *newParams);
    status_t processParamExifSoftware(const CameraParameters *oldParams,
            CameraParameters *newParams);
    status_t processPreviewUpdateMode(const CameraParameters *oldParams,
            CameraParameters *newParams);

    status_t processParamSlowMotionRate(const CameraParameters *oldParams,
        CameraParameters *newParams);
    status_t processParamRecordingFramerate(const CameraParameters *oldParams,
        CameraParameters *newParams);

    status_t processParamMirroring(const CameraParameters *oldParams,
        CameraParameters *newParams);

    void processParamFileInject(CameraParameters *newParams);

    status_t processParamNREE(const CameraParameters *oldParams,
        CameraParameters *newParams);

    status_t setAfWindowsTo3a(CameraWindow *focusWindows, size_t winCount);

    void selectFlashModeForScene(CameraParameters *newParams);

    bool selectPostviewSize(int &width, int &height);
    int  selectPostviewFormat();

    // These are params that can only be set while the ISP is stopped. If the parameters
    // changed while the ISP is running, the ISP will need to be stopped, reconfigured, and
    // restarted. Static parameters will most likely affect buffer size and/or format so buffers
    // must be deallocated and reallocated accordingly.
    status_t processStaticParameters(CameraParameters *oldParams,
            CameraParameters *newParams, bool &restartNeeded);

    bool validateHighSpeedResolutionFps(int width, int height, int fps) const;

    status_t stopCapture();
    void     stopOfflineCapture();
    void     recycleUnusedBufferInISP();
    status_t waitForCaptureStart();

    // HDR helper functions
    status_t hdrInit(int pvSize, int pvWidth, int pvHeight);
    status_t hdrProcess(AtomBuffer * snapshotBuffer, AtomBuffer* postviewBuffer);
    status_t hdrCompose();
    void     hdrRelease();

    // snapshot buffer management
    int      getNeededSnapshotBufNum(bool videoMode);
    status_t allocateSnapshotAndPostviewBuffers(bool videoMode);
    status_t setExternalSnapshotBuffers(int fourcc, int width, int heigth);
    void     forceRestoreSnapshotPostviewBuffers();

    // Capture Flow helpers
    status_t getFlashExposedSnapshot(AtomBuffer *snaphotBuffer, AtomBuffer *postviewBuffer);
    void     fillPicMetaData(PictureThread::MetaData &metadata, bool flashFired);

    // Burst helper functions
    bool     isBurstRunning();
    bool     burstMoreCapturesNeeded();
    void     burstStateReset();
    void     requestTakePicture();
    bool     compressedFrameQueueFull();
    status_t burstCaptureSkipFrames();

    status_t captureStillPic();
    status_t captureStillPicFromPreview();
    status_t captureBurstPic(bool clientRequest);
    status_t captureFixedBurstPic(bool clientRequest);
    status_t capturePanoramaPic(AtomBuffer &snapshotBuffer, AtomBuffer &postviewBuffer);
    status_t captureULLPic();
    status_t captureJpegPic();
    status_t captureExtIspHDRLLSPic();
    status_t captureSmartStabilizationPic();
    status_t startJpegPicContinuousShooting();
    status_t stopJpegPicContinuousShooting();

    // for continuous shooting
    bool     holdOnContinuousShooting();
    status_t prepareContinuousShooting();
    status_t captureContinuousShooting(bool clientRequest);
    status_t finalizeContinuousShooting();
    // snapshot during video functions
    status_t initSdv(bool offline);
    status_t deinitSdv(bool offline);
    status_t captureSdvSoC(bool fullsize);
    status_t captureSdv(bool offline);
    status_t cancelCaptureSdv();
    status_t sdvUpdateParams(bool offline, bool updateCache);
    status_t sdvRestoreParams(bool updateCache);
    status_t getSdvSupportedMinVideoSize(int &width, int &height);
    bool isFullSizeSdvSupportedVideoSize(int width, int height, int previewWidth, int previewHeight);
    void saveCurrentPictureParams();
    void clearSavedPictureParams();
    bool selectSdvSize(int &width, int &height);
    void encodeVideoSnapshot(AtomBuffer &buff);

    // offline capture control by exposure id
    void triggerOfflineCaptureControl(int numSounds, int startId, bool skip = false);
    void resetOfflineCaptureControl();
    void handleOfflineCaptureControl(AtomBuffer *buff);

    status_t updateSpotWindow(const int &width, const int &height);

    MeteringMode aeMeteringModeFromString(const String8& modeStr);

    status_t createAtom3A();

    void enableFocusCallbacks();
    void disableFocusCallbacks();

    int getCameraID();

    void reconfigureThumbnailSize(int &width, int &height);

    void checkAndUpdateRawSize(AtomBuffer &formatDesc);
    // front camera saved-mirror
    int getNeededIspFlipType();

// inherited from Thread
private:
    virtual bool threadLoop();

// private data
private:

    int mCameraId;
    HWControlGroup mHwcg;
    AtomISP *mISP;
    AtomCP  *mCP;
    UltraLowLight *mULL;
    I3AControls *m3AControls;
    BracketManager *mBracketManager;
    sp<PreviewThread> mPreviewThread;
    sp<PictureThread> mPictureThread;
    sp<VideoThread> mVideoThread;
    sp<AAAThread>     m3AThread;
    sp<PostProcThread> mPostProcThread;
    sp<PanoramaThread> mPanoramaThread;
    sp<ScalerService> mScalerService;
    sp<WarperService> mWarperService;
    sp<PostCaptureThread> mPostCaptureThread;
    sp<AccManagerThread> mAccManagerThread;
    sp<ThermalThrottleThread> mThermalThrottleThread;

    MessageQueue<Message, MessageId> mMessageQueue;
    List<Message> mPostponedMessages;
    bool mPostponedMsgProcessing;
    State mState;
    CaptureSubState mCaptureSubState;
    ShootingMode    mShootingMode;
    bool mThreadRunning;
    Callbacks *mCallbacks;
    sp<CallbacksThread> mCallbacksThread;

    int mNumBuffers;

    CameraParameters mParameters;
    CameraParameters mIntelParameters;
    bool mIntelParamsAllowed;           /*<! Flag that signals whether the caller is allowed to use Intel extended paramters*/
    String8 mSavedFlashSupported;   /*<! Save single shot supported flash values,
                                    in case burst multi shot capture is called and forces flash to be off.*/
    String8 mSavedFlashMode;        /*<! Save single shot current flash mode,
                                    in case burst multi shot capture is called and forces flash to be off.*/

    String8 mSavedFocusMode;    /*<! Save current focus mode before changing focus mode for depth mode. */

    bool mFaceDetectionActive;
    bool mIspExtensionsEnabled;     /*<! Flag that signals whether the caller wants to run a 3rd party ISP extension*/

    /* Burst configuration: */
    int  mFpsAdaptSkip;
    int  mBurstLength;          /*<! Burst length 1..N */
    int  mBurstStart;           /*<! Relative offset at which burst
                                  capture should start, where 0 marks
                                  the zero shutter lag case. */

    /* Burst runtime state, \see burstStateReset() */

    int  mBurstCaptureNum;      /*<! Number of the most recent burst
                                  capture that was started. In range
                                  1...N, where N is mBurstLength. */
    int  mBurstCaptureDoneNum;  /*<! Number of the most recent burst
                                  capture that was completed. In range
                                  1...N, where N is mBurstLength. */
    int  mBurstBufsToReturn; /*<! Number of buffers should be returned to ISP for reuse
                                exp:mBurstLength is 9, mAllocatedSnapshotBuffers is 5,
                                mBurstBufsToReturn should be 4*/
    int mUllBurstLength;     /*<! Burst length for ULL*/
    bool mSmartStabilization; /*<! Smart stabilization for front camera */
    HdrImaging mHdr;
    ExtIsp mExtIsp;
    FlashStage mAELockFlashStage;
    float mPublicShutter;       /* Shutter set by application */

    bool mDvsEnable;
    bool mDualVideo;

    bool mJpegContinuousShootingRunning;

    Mutex mParamCacheLock;
    char* mParamCache;

    bool mPreviewForceChanged; /*!< Stores whether preview size has been forced and no further fixing of aspect
                                    ratios or similar should be done.
                                    NOTE: Do not touch this variable from other threads than the camera service
                                    thread which is running the setParameters and the processStaticParameters,
                                    which is currently the only access point. */

    CameraDump *mCameraDump;

    CameraAreas mFocusAreas;
    CameraAreas mMeteringAreas;

    struct StillPicParamsCtx mStillPictContext; /*!< we store the current still image parameters
                                                    It is used when video recording starts so the settings
                                                    can be restore when video recording stops
                                                 */

    bool mEnableFocusCbAtStart;     /* for internal control of focus cb's in continuous-mode */
    bool mEnableFocusMoveCbAtStart; /* for internal control of focus-move cb's in continuous-mode */


    bool mStillCaptureInProgress;   /*!< indicates ongoing capture sequence for Camera_HAL API.
                                         note: threadsafe to use only in Camera_HAL calling context
                                               or synchronous messages from them
                                         note: is set to true when takePicture() is called in other
                                               than video recording state. Remains true until following
                                               call to startPreview() or cancelPicture(). */

    const char* mPreviewUpdateMode;       /*!< indicates the active preview update mode.
                                               See parameter preview-update-mode */

    Vector<AtomBuffer> mAllocatedSnapshotBuffers; /*!< Current set of allocated snapshot buffers */
    Vector<AtomBuffer> mAvailableSnapshotBuffers; /*!< Current set of available snapshot buffers */
    Vector<AtomBuffer> mSnapshotBuffersInISP;     /*!< Current set of snapshot buffers in ISP*/

    Vector<AtomBuffer> mAllocatedPostviewBuffers; /*!< Current set of allocated postview buffers */
    Vector<AtomBuffer> mAvailablePostviewBuffers; /*!< Current set of available postview buffers */
    Vector<AtomBuffer> mPostviewBuffersInISP;     /*!< Current set of postview buffers in ISP*/

    bool mSaveMirrored;
    ExtIspActionHint mExtIspAction; /*!< Expected action for external ISP such as HAL video stabilization
                                         and external ISP video high speed */
    int mCurrentOrientation;        /*!< Current orientation of the device. Used in case the image is
                                         saved as mirrored. The image will be mirrored based on the
                                         camera sensor orientation and device orientation. */
    bool mFullSizeSdv;              /*!< is full resolution sdv?*/

    // offline burst capture control
    Mutex mOfflineControlLock;
    bool mRawBufferLockMode;        /*!< is raw buffer lock mode enabled? */
    bool mSkipPreview;              /*!< if to skip this frame in preview */
    unsigned int mCurrentExpID;     /*!< exposure ID of current preview frame*/
    unsigned int mNextExpID;        /*!< next expected buffer exposure ID */
    int mNumCaptures;               /*!< control the the number of capture */
    int mNumSounds;                 /*!< shutter sound times,trigger shutter sound by EOF/preview buffer event*/

    bool mDepthMode;                /*!< if working in depth mode */

    // continuous capture
    ContinuousShootingState mContShootingState;   /*!< continuous shooting state */
    bool mContShootingEnabled;                    /*!< app controls to enable of disable continuous shooting mode*/
    // FIXME: remove this when CSS able to clear all old frames
    bool mContShootingSkipFirstFrame;           /*!< This is a workaround for FW bug 2477. We get remaining frames when set
                                                     number_capture to -1. FW can clear all the tagger buffer but still
                                                     has one buffer which has already been processing in capture
                                                     pipe can't be retrieved safely. In camera HAL currently we need to skip
                                                     this frame to avoid getting old frame */
    int mContinuousPicsReady;                     /*!< number buffer ready*/

    /*----------- Debugging helpers --------------------*/
    static const char* sCaptureSubstateStrings[STATE_CAPTURE_LAST];

}; // class ControlThread

}; // namespace android

#endif // ANDROID_LIBCAMERA_CONTROL_THREAD_H
