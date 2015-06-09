/*
 * Copyright (C) 2011 The Android Open Source Project
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

namespace android {

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

// Thread overrides
public:
    status_t requestExitAndWait();

public:
    virtual bool atomIspNotify(IAtomIspObserver::Message *msg, const ObserverState state);

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

    // return recording frame to driver (asynchronous)
    status_t releaseRecordingFrame(void *buff);

    void atomRelease();

    // IBufferOwner override
    virtual void returnBuffer(AtomBuffer* buff);

    // Implementation of IPostCaptureProcessObserver interface
    void postCaptureProcesssingDone(IPostCaptureProcessItem* item, status_t status, int retries = MAX_MSG_RETRIES);

    // IOrientationListener
    void orientationChanged(int orientation);

// callback methods
private:
    virtual void previewBufferCallback(AtomBuffer *buff, ICallbackPreview::CallbackType t);
    virtual void encodingDone(AtomBuffer *snapshotBuf, AtomBuffer *postviewBuf);
    virtual void pictureDone(AtomBuffer *snapshotBuf, AtomBuffer *postviewBuf);
    virtual void postProcCaptureTrigger();
    virtual void sceneDetected(int sceneMode, bool sceneHdr);
    virtual void facesDetected(const ia_face_state *faceState);
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
        MESSAGE_ID_RELEASE_RECORDING_FRAME,
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

        MESSAGE_ID_DEQUEUE_RECORDING,
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

    struct MessageReleaseRecordingFrame {
        void *buff;
    };

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

    struct MessageDequeueRecording {
        bool    skipFrame;
        AtomBuffer previewFrame; // special case for VFPP limited cases, where recording frame is created from preview
    };

    struct MessagePostCaptureProcDone {
        IPostCaptureProcessItem* item;
        status_t status;
        int retriesLeft;
    };

    struct MessageOrientation {
        int value;
    };

    // union of all message data
    union MessageData {

        // MESSAGE_ID_RELEASE_RECORDING_FRAME
        MessageReleaseRecordingFrame releaseRecordingFrame;

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

        // MESSAGE_ID_DEQUEUE_RECORDING
        MessageDequeueRecording   dequeueRecording;

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
        STATE_CONTINUOUS_CAPTURE
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

    status_t initContinuousCapture();
    void releaseContinuousCapture(bool flushPictures);
    status_t startOfflineCapture();
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
    status_t handleMessageReleaseRecordingFrame(MessageReleaseRecordingFrame *msg);
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

    status_t cancelPictureThread();
    status_t cancelPostCaptureThread();
    status_t cancelCapture();

    // main message function
    status_t waitForAndExecuteMessage();

    AtomBuffer* findRecordingBuffer(void *findMe);
    AtomBuffer* findBufferByData(AtomBuffer *buf,Vector<AtomBuffer> *aVector);

    // dequeue buffers from driver and deliver them
    status_t dequeuePreview();
    status_t dequeueRecording(MessageDequeueRecording *msg);

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
    status_t processParamAntiBanding(const CameraParameters *oldParams,
                                           CameraParameters *newParams);
    status_t processParamFocusMode(const CameraParameters *oldParams,
            CameraParameters *newParams);
    status_t processParamWhiteBalance(const CameraParameters *oldParams,
            CameraParameters *newParams);
    status_t processParamSetMeteringAreas(const CameraParameters * oldParams,
            CameraParameters * newParams);
    status_t processParamBracket(const CameraParameters *oldParams,
                CameraParameters *newParams);
    status_t processParamSmartShutter(const CameraParameters *oldParams,
                CameraParameters *newParams);
    status_t processParamHDR(const CameraParameters *oldParams,
            CameraParameters *newParams);
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
    status_t processParamRawDataFormat(const CameraParameters *oldParams,
            CameraParameters *newParams, bool &previewRestartNeeded);
    // NOTE: processParamPreviewFrameRate is deprecated since Android API level 9
    status_t processParamPreviewFrameRate(const CameraParameters *oldParams,
            CameraParameters *newParams);
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

    void convertAfWindows(CameraWindow* focusWindows, size_t winCount);

    void selectFlashModeForScene(CameraParameters *newParams);

    bool selectPostviewSize(int &width, int &height);
    int  selectPostviewFormat();

    // These are params that can only be set while the ISP is stopped. If the parameters
    // changed while the ISP is running, the ISP will need to be stopped, reconfigured, and
    // restarted. Static parameters will most likely affect buffer size and/or format so buffers
    // must be deallocated and reallocated accordingly.
    status_t processStaticParameters(CameraParameters *oldParams,
            CameraParameters *newParams, bool &restartNeeded);
    status_t validateParameters(const CameraParameters *params);
    // validation helpers
    bool validateSize(int width, int height, Vector<Size> &supportedSizes) const;
    bool validateString(const char* value,  const char* supportList) const;
    bool validateHighSpeedResolutionFps(int width, int height, int fps) const;

    status_t stopCapture();
    void     stopOfflineCapture();
    status_t waitForCaptureStart();

    // HDR helper functions
    status_t hdrInit(int pvSize, int pvWidth, int pvHeight);
    status_t hdrProcess(AtomBuffer * snapshotBuffer, AtomBuffer* postviewBuffer);
    status_t hdrCompose();
    void     hdrRelease();
    status_t allocateSnapshotAndPostviewBuffers(bool videoMode);
    void     setExternalSnapshotBuffers(int fourcc, int width, int heigth);

    // Capture Flow helpers
    status_t getFlashExposedSnapshot(AtomBuffer *snaphotBuffer, AtomBuffer *postviewBuffer);
    void     fillPicMetaData(PictureThread::MetaData &metadata, bool flashFired);

    // Burst helper functions
    bool     isBurstRunning();
    bool     burstMoreCapturesNeeded();
    void     burstStateReset();
    void     requestTakePicture();
    bool     compressedFrameQueueFull();
    status_t queueSnapshotBuffers();
    status_t burstCaptureSkipFrames();

    status_t captureStillPic();
    status_t captureBurstPic(bool clientRequest);
    status_t captureFixedBurstPic(bool clientRequest);
    status_t capturePanoramaPic(AtomBuffer &snapshotBuffer, AtomBuffer &postviewBuffer);
    status_t captureULLPic();
    status_t captureVideoSnap(void);
    AtomBuffer* findVideoSnapshotBuffer(int index);
    void     encodeVideoSnapshot(AtomBuffer &buff);

    status_t updateSpotWindow(const int &width, const int &height);

    MeteringMode aeMeteringModeFromString(const String8& modeStr);

    void storeCurrentPictureParams();
    void restoreCurrentPictureParams();

    status_t createAtom3A();

    void enableFocusCallbacks();
    void disableFocusCallbacks();

    int getCameraID();

    void reconfigureThumbnailSize(int &width, int &height);

// inherited from Thread
private:
    virtual bool threadLoop();

// private data
private:

    int mCameraId;
    HWControlGroup mHwcg;
    IHWIspControl *mISP;
    AtomCP  *mCP;
    UltraLowLight *mULL;
    I3AControls *m3AControls;
    sp<PreviewThread> mPreviewThread;
    sp<PictureThread> mPictureThread;
    sp<VideoThread> mVideoThread;
    sp<AAAThread>     m3AThread;
    sp<PostProcThread> mPostProcThread;
    sp<PanoramaThread> mPanoramaThread;
    sp<ScalerService> mScalerService;
    sp<SensorThread> mSensorThread;
    sp<BracketManager> mBracketManager;
    sp<PostCaptureThread> mPostCaptureThread;
    sp<AccManagerThread> mAccManagerThread;

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
    int  mBurstQbufs;           /*<! Number of buffers queued so far
                                  to ISP, 1..N where N is mBurstLength */
    int  mBurstBufsToReturn; /*<! Number of buffers should be returned to ISP for reuse
                                exp:mBurstLength is 9, mAllocatedSnapshotBuffers is 5,
                                mBurstBufsToReturn should be 4*/
    HdrImaging mHdr;
    bool mAELockFlashNeed;
    float mPublicShutter;       /* Shutter set by application */

    bool mDvsEnable;

    Mutex mParamCacheLock;
    char* mParamCache;

    bool mStoreMetaDataInBuffers;

    bool mPreviewForceChanged; /*!< Stores whether preview size has been forced and no further fixing of aspect
                                    ratios or similar should be done.
                                    NOTE: Do not touch this variable from other threads than the camera service
                                    thread which is running the setParameters and the processStaticParameters,
                                    which is currently the only access point. */

    CameraDump *mCameraDump;

    CameraAreas mFocusAreas;
    CameraAreas mMeteringAreas;

    int mVideoSnapshotrequested;    /*!< number of video snapshots requested */
    Vector<AtomBuffer> mVideoSnapshotBuffers; /*!< buffers reserved from stream for videosnapshot */
    Vector<AtomBuffer> mRecordingBuffers; /*!< buffers reserverd from stream for video encoding */

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

    Vector<AtomBuffer> mAllocatedPostviewBuffers; /*!< Current set of allocated postview buffers */
    Vector<AtomBuffer> mAvailablePostviewBuffers; /*!< Current set of available postview buffers */

    bool mSaveMirrored;
    int mCurrentOrientation;        /*!< Current orientation of the device. Used in case the image is
                                         saved as mirrored. The image will be mirrored based on the
                                         camera sensor orientation and device orientation. */
    int mRecordingOrientation;      /*!< Device orientation at the start of video recording. */

    /*----------- Debugging helpers --------------------*/
    static const char* sCaptureSubstateStrings[STATE_CAPTURE_LAST];

}; // class ControlThread

}; // namespace android

#endif // ANDROID_LIBCAMERA_CONTROL_THREAD_H
