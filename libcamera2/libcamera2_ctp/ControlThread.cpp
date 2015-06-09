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
#define LOG_TAG "Camera_ControlThread"

#include "ControlThread.h"
#include "LogHelper.h"
#include "PerformanceTraces.h"
#include "CameraConf.h"
#include "PreviewThread.h"
#include "PictureThread.h"
#include "AtomAIQ.h"
#include "AtomSoc3A.h"
#include "AtomISP.h"
#include "Callbacks.h"
#include "CallbacksThread.h"
#include "ColorConverter.h"
#include "PlatformData.h"
#include "IntelParameters.h"
#include "MemoryUtils.h"
#include <utils/Vector.h>
#include <math.h>
#include <cutils/properties.h>
#include <binder/IServiceManager.h>
#include "intel_camera_extensions.h"
#include "ICameraHwControls.h"

namespace android {

/*
 * NUM_WARMUP_FRAMES: used for front camera only
 * Since front camera does not 3A, it actually has 2A (auto-exposure and auto-whitebalance),
 * it needs about 4 for internal 2A from driver to gather enough information and establish
 * the correct values for 2A.
 */
#define NUM_WARMUP_FRAMES 4
/*
 * RAW_CAPTURE_SKIP: Number of frames we skip from capture device before we dump
 * a raw image.
 */
#define RAW_CAPTURE_SKIP  2
/*
 * NUM_BURST_BUFFERS: used for burst captures
 */
#define NUM_BURST_BUFFERS 10
/*
 * MAX_JPEG_BUFFERS: the maximum numbers of queued JPEG buffers
 */
#define MAX_JPEG_BUFFERS 4
/*
 * FLASH_TIMEOUT_FRAMES: maximum number of frames to wait for
 * a correctly exposed frame
 */
#define FLASH_TIMEOUT_FRAMES 5
/*
 * ASPECT_TOLERANCE: the tolerance between aspect ratios to consider them the same
 */
#define ASPECT_TOLERANCE 0.001

/*
 * DEFAULT_HDR_BRACKETING: the number of bracketed captures to be made in order to compose
 * a HDR image.
 */
#define DEFAULT_HDR_BRACKETING 3

/*
 * Timeout for ControlThread::waitForAndExecuteMessage()
 */
#define MESSAGE_QUEUE_RECEIVE_TIMEOUT_MSEC 5000

#define ATOMISP_CAPTURE_POLL_TIMEOUT 2000

/*
 * RECONFIGURE_THUMBNAIL_HEIGHT_LIMIT: limit thumbnail size less than 480 to reduce thumbnail Jpeg size.
 * Make sure total Exif size less than 64k.
 */
#define RECONFIGURE_THUMBNAIL_HEIGHT_LIMIT 480

/*BATTERY_CHECK_INTERVAL_FRAME_UNIT: battery check interval base on frames to make sure to turn off
flash when battery is lower than 15%*/
const int BATTERY_CHECK_INTERVAL_FRAME_UNIT = 300;

// Minimum value of our supported preview FPS
const int MIN_PREVIEW_FPS = 11;
// Max value of our supported preview fps:
// TODO: This value should be gotten from sensor dynamically, instead of hardcoding:
const int MAX_PREVIEW_FPS = 30;

const char* ControlThread::sCaptureSubstateStrings[]= {
      "INIT",
      "STARTED",
      "ENCODING_DONE",
      "PICTURE_DONE",
      "IDLE"
};

const char *scene_mode_detected[NUM_SCENE_DETECTED] = {
                                                   "auto",
                                                   "close_up_portrait",
                                                   "portrait",
                                                   "night_portrait",
                                                   "night",
                                                   "action",
                                                   "backlight",
                                                   "landscape",
                                                   "barcode",
                                                   "firework",
};
ControlThread::ControlThread(int cameraId) :
    Thread(true) // callbacks may call into java
    ,mCameraId(cameraId)
    ,mISP(NULL)
    ,mCP(NULL)
    ,mULL(NULL)
    ,m3AControls(NULL)
    ,mPreviewThread(NULL)
    ,mPictureThread(NULL)
    ,mVideoThread(NULL)
    ,m3AThread(NULL)
    ,mPostProcThread(NULL)
    ,mPanoramaThread(NULL)
    ,mScalerService(NULL)
    ,mBracketManager(NULL)
    ,mPostCaptureThread(NULL)
    ,mAccManagerThread(NULL)
    ,mMessageQueue("ControlThread", (int) MESSAGE_ID_MAX)
    ,mPostponedMsgProcessing(false)
    ,mState(STATE_STOPPED)
    ,mCaptureSubState(STATE_CAPTURE_INIT)
    ,mShootingMode(SHOOTING_MODE_NONE)
    ,mThreadRunning(false)
    ,mCallbacks(NULL)
    ,mCallbacksThread(NULL)
    ,mNumBuffers(0)
    ,mIntelParamsAllowed(false)
    ,mFaceDetectionActive(false)
    ,mIspExtensionsEnabled(false)
    ,mFpsAdaptSkip(0)
    ,mBurstLength(0)
    ,mBurstStart(0)
    ,mBurstCaptureNum(-1)
    ,mBurstCaptureDoneNum(-1)
    ,mBurstQbufs(0)
    ,mBurstBufsToReturn(0)
    ,mAELockFlashNeed(false)
    ,mPublicShutter(-1)
    ,mDvsEnable(false)
    ,mParamCache(NULL)
    ,mStoreMetaDataInBuffers(false)
    ,mPreviewForceChanged(false)
    ,mCameraDump(NULL)
    ,mFocusAreas()
    ,mMeteringAreas()
    ,mVideoSnapshotrequested(0)
    ,mEnableFocusCbAtStart(false)
    ,mEnableFocusMoveCbAtStart(false)
    ,mStillCaptureInProgress(false)
    ,mPreviewUpdateMode(IntelCameraParameters::PREVIEW_UPDATE_MODE_STANDARD)
    ,mSaveMirrored(false)
    ,mCurrentOrientation(0)
    ,mRecordingOrientation(0)
{
    // DO NOT PUT ANY ALLOCATION CODE IN THIS METHOD!!!
    // Put all init code in the init() method.
    // This is a workaround for an issue with Thread reference counting.

    LOG1("@%s", __FUNCTION__);

    PlatformData::setActiveCameraId(mCameraId);
}

ControlThread::~ControlThread()
{
    // DO NOT PUT ANY CODE IN THIS METHOD!!!
    // Put all deinit code in the deinit() method.
    // This is a workaround for an issue with Thread reference counting.
    LOG1("@%s", __FUNCTION__);
    if(mMessageQueue.size() > 0) {
        Message msg;
        ALOGE("At this point Message Q should be empty, found %d message(s)",mMessageQueue.size());
        mMessageQueue.receive(&msg);
        ALOGE(" Id of first message is %d",msg.id);
    }

    PlatformData::freeActiveCameraId(mCameraId);
}

status_t ControlThread::init()
{
    LOG1("@%s: cameraId = %d", __FUNCTION__, mCameraId);

    status_t status = NO_ERROR;
    CameraDump::setDumpDataFlag();

    AtomISP * isp = NULL;
    mScalerService = new ScalerService();
    if (mScalerService == NULL) {
        ALOGE("error creating ScalerService");
        goto bail;
    }

    mCallbacks = new Callbacks();
    if (mCallbacks == NULL) {
        ALOGE("error creating Callbacks");
        goto bail;
    }

    // we implement ICallbackPicture interface
    mCallbacksThread = new CallbacksThread(mCallbacks, this);
    if (mCallbacksThread == NULL) {
        ALOGE("error creating CallbacksThread");
        goto bail;
    }

    isp = new AtomISP(mCameraId, mScalerService, mCallbacks);
    if (isp == NULL) {
        ALOGE("error creating ISP");
        goto bail;
    }
    mISP = isp;
    status = mISP->init();
    if (status != NO_ERROR) {
        ALOGE("Error initializing ISP");
        goto bail;
    }

    // Assign local HWControlGroup
    mHwcg.mIspCI = (IHWIspControl*)isp;
    mHwcg.mSensorCI = isp->getSensorControlInterface();
    mHwcg.mFlashCI = (IHWFlashControl*)isp;
    mHwcg.mLensCI = (IHWLensControl*)isp;

    // Choose 3A interface based on the sensor type
    if (createAtom3A() != NO_ERROR) {
        ALOGE("error creating AAA");
        goto bail;
    }

    if (m3AControls->init3A() != NO_ERROR) {
        ALOGE("Error initializing 3A controls");
        goto bail;
    }
    PERFORMANCE_TRACES_BREAKDOWN_STEP("Init_3A");

    mCP = new AtomCP(mHwcg);
    if (mCP == NULL) {
        ALOGE("error creating CP");
        goto bail;
    }

    mULL = new UltraLowLight(mCallbacks);
    if (mULL == NULL) {
        ALOGE("error creating ULL");
        goto bail;
    }

    mCameraDump = CameraDump::getInstance(mCameraId);
    if (mCameraDump == NULL) {
        ALOGE("error creating CameraDump");
        goto bail;
    }
    mCameraDump->set3AControls(m3AControls);

    // we implement the ICallbackPreview interface, so pass
    // this as argument
    mPreviewThread = new PreviewThread(mCallbacksThread, mCallbacks);
    if (mPreviewThread == NULL) {
        ALOGE("error creating PreviewThread");
        goto bail;
    }

    mPictureThread = new PictureThread(m3AControls, mScalerService, mCallbacksThread, mCallbacks);
    if (mPictureThread == NULL) {
        ALOGE("error creating PictureThread");
        goto bail;
    }

    mVideoThread = new VideoThread(mCallbacksThread);
    if (mVideoThread == NULL) {
        ALOGE("error creating VideoThread");
        goto bail;
    }

    // we implement ICallbackAAA interface
    m3AThread = new AAAThread(this, mULL, m3AControls, mCallbacksThread);
    if (m3AThread == NULL) {
        ALOGE("error creating 3AThread");
        goto bail;
    }

    mPanoramaThread = new PanoramaThread(this, m3AControls, mCallbacksThread, mCallbacks, mCameraId);
    if (mPanoramaThread == NULL) {
        ALOGE("error creating PanoramaThread");
        goto bail;
    }

    mPostProcThread = new PostProcThread(this, mPanoramaThread.get(), m3AControls, mCallbacksThread, mCallbacks, mCameraId);
    if (mPostProcThread == NULL) {
        ALOGE("error creating PostProcThread");
        goto bail;
    }

    if (mPostProcThread->init((void*)mISP) != NO_ERROR) {
        ALOGE("error initializing face engine");
        goto bail;
    }

    mSensorThread = SensorThread::getInstance(mCameraId);
    if (mSensorThread == NULL) {
        ALOGE("error creating SensorThread");
        goto bail;
    }

    mBracketManager = new BracketManager(mHwcg, m3AControls);
    if (mBracketManager == NULL) {
        ALOGE("error creating BracketManager");
        goto bail;
    }

    mPostCaptureThread = new PostCaptureThread(this);
    if (mPostCaptureThread == NULL) {
        ALOGE("error creating PostCapture");
        goto bail;
    }

    mAccManagerThread = new AccManagerThread(mHwcg, mCallbacksThread, mCallbacks, mCameraId);
    if (mAccManagerThread == NULL) {
        ALOGE("error creating AccManagerThread");
        goto bail;
    }

    // DVS needs to be started after AIQ init.
    status = mISP->initDVS();
    if (status != NO_ERROR) {
        ALOGE("Error in initializing DVS");
        goto bail;
    }

    // get default params from AtomISP and JPEG encoder
    mISP->getDefaultParameters(&mParameters, &mIntelParameters);
    m3AControls->getDefaultParams(&mParameters, &mIntelParameters);
    mPictureThread->getDefaultParameters(&mParameters);
    mPreviewThread->getDefaultParameters(&mParameters);
    mPanoramaThread->getDefaultParameters(&mIntelParameters, mCameraId);
    mPostProcThread->getDefaultParameters(&mParameters, &mIntelParameters, mCameraId);
    mVideoThread->getDefaultParameters(&mIntelParameters, mCameraId);
    updateParameterCache();

    status = mSensorThread->run("CamHAL_SENSOR");
    if (status != NO_ERROR) {
        ALOGE("Error starting sensor thread!");
        goto bail;
    }
    status = mScalerService->run("CamHAL_SCALER");
    if (status != NO_ERROR) {
        ALOGE("Error starting scaler service!");
        goto bail;
    }
    status = m3AThread->run("CamHAL_3A");
    if (status != NO_ERROR) {
        ALOGE("Error starting 3A thread!");
        goto bail;
    }
    status = mPreviewThread->run("CamHAL_PREVIEW");
    if (status != NO_ERROR) {
        ALOGE("Error starting preview thread!");
        goto bail;
    }
    status = mPictureThread->run("CamHAL_PICTURE");
    if (status != NO_ERROR) {
        ALOGW("Error starting picture thread!");
        goto bail;
    }
    status = mCallbacksThread->run("CamHAL_CALLBACK");
    if (status != NO_ERROR) {
        ALOGW("Error starting callbacks thread!");
        goto bail;
    }
    status = mVideoThread->run("CamHAL_VIDEO");
    if (status != NO_ERROR) {
        ALOGW("Error starting video thread!");
        goto bail;
    }
    status = mPostProcThread->run("CamHAL_POSTPROC");
    if (status != NO_ERROR) {
        ALOGW("Error starting Post Processing thread!");
        goto bail;
    }
    status = mPanoramaThread->run("CamHAL_PANO");
    if (status != NO_ERROR) {
        ALOGW("Error Starting Panorama Thread!");
        goto bail;
    }
    status = mBracketManager->run("CamHAL_BRACKET");
    if (status != NO_ERROR) {
        ALOGW("Error Starting Bracketing Manager!");
        goto bail;
    }

    status = mPostCaptureThread->run("CamHAL_POSTCAP");
    if (status != NO_ERROR) {
        ALOGW("Error Starting PostCaptureThread!");
        goto bail;
    }

    status = mAccManagerThread->run("CamHAL_ACCMANAGER");
    if (status != NO_ERROR) {
        ALOGW("Error starting Acceleration Manager thread!");
        goto bail;
    }

    // Disable bracketing by default
    mBracketManager->setBracketMode(BRACKET_NONE);

    // Disable HDR by default
    CLEAR(mHdr);
    mHdr.enabled = false;
    mHdr.inProgress = false;
    mHdr.savedBracketMode = BRACKET_NONE;
    mHdr.saveOrig = false;
    mHdr.outMainBuf = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_SNAPSHOT);
    mHdr.outPostviewBuf = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_POSTVIEW);

    //default flash modes
    mSavedFlashSupported = PlatformData::supportedFlashModes(mCameraId);
    mSavedFlashMode = PlatformData::defaultFlashMode(mCameraId);

    // Set property to inform system what camera is in use
    char facing[PROPERTY_VALUE_MAX];
    snprintf(facing, PROPERTY_VALUE_MAX, "%d", mCameraId);
    property_set("media.camera.facing", facing);

    // Set default parameters so that settings propagate to 3A
    MessageSetParameters msg;
    msg.params = mParamCache;
    handleMessageSetParameters(&msg);

    return NO_ERROR;

bail:

    // this should clean up only what NEEDS to be cleaned up
    deinit();

    if (status == NO_ERROR)
        status = UNKNOWN_ERROR;    // If we get here, it is always an error

    return status;
}

void ControlThread::deinit()
{
    // NOTE: This method should clean up only what NEEDS to be cleaned up.
    //       Refer to ControlThread::init(). This method will be called if
    //       even if only partial or no initialization was successful.
    //       Therefore it is important that each specific deinit step
    //       is checked for successful initialization before proceeding
    //       with deinit (eg. check for NULL / non-NULL).

    LOG1("@%s", __FUNCTION__);

    if (mPostCaptureThread != NULL) {
        mPostCaptureThread->requestExitAndWait();
        mPostCaptureThread.clear();
    }

    if (mBracketManager != NULL) {
        mBracketManager->requestExitAndWait();
        mBracketManager.clear();
    }

    if (mSensorThread != NULL) {
        mSensorThread->requestExitAndWait();
        mSensorThread.clear();
    }

    if (mPostProcThread != NULL) {
        mPostProcThread->requestExitAndWait();
        mPostProcThread.clear();
    }

    if (mAccManagerThread != NULL) {
        mAccManagerThread->requestExitAndWait();
        mAccManagerThread.clear();
    }

    if (mPanoramaThread != NULL) {
        mPanoramaThread->requestExitAndWait();
        mPanoramaThread.clear();
    }

    if (mPreviewThread != NULL) {
        mPreviewThread->requestExitAndWait();
        mPreviewThread.clear();
    }

    if (mVideoThread != NULL) {
        mVideoThread->requestExitAndWait();
        mVideoThread.clear();
    }

    if (mPictureThread != NULL) {
        mPictureThread->requestExitAndWait();
        mPictureThread.clear();
        PERFORMANCE_TRACES_BREAKDOWN_STEP("PictureThread-Clear");
    }

    if (m3AThread != NULL) {
        m3AThread->requestExitAndWait();
        m3AThread.clear();
    }

    if (mParamCache != NULL) {
        free(mParamCache);
        mParamCache = NULL;
    }

    if (m3AControls != NULL) {
        m3AControls->deinit3A();
        delete m3AControls;
        m3AControls = NULL;
    }

    if (mCP != NULL) {
        if (mHdr.enabled)
            mCP->uninitializeHDR();
        delete mCP;
        mCP = NULL;
    }

    if (mCallbacksThread != NULL) {
        mCallbacksThread->requestExitAndWait();
        mCallbacksThread.clear();
    }

    if (mISP != NULL) {
        delete mISP;
        mISP = NULL;
        PERFORMANCE_TRACES_BREAKDOWN_STEP("DeleteISP");
    }

    if (mScalerService != NULL) {
        mScalerService->requestExitAndWait();
        mScalerService.clear();
    }

    if (mULL != NULL) {
        delete mULL;
        mULL = NULL;
    }

    if (mCameraDump != NULL) {
        delete mCameraDump;
        mCameraDump = NULL;
    }

    if (mCallbacks != NULL) {
        delete mCallbacks;
        mCallbacks = NULL;
    }
    List<Message>::iterator it = mPostponedMessages.begin();
    for ( ;it != mPostponedMessages.end(); it++)
        if (it->id == MESSAGE_ID_SET_PARAMETERS) {
            free(it->data.setParameters.params); // was strdupped, needs free
            it->data.setParameters.params = NULL;
        }
    mPostponedMessages.clear();

    LOG1("@%s- complete", __FUNCTION__);
}

status_t ControlThread::setPreviewWindow(struct preview_stream_ops *window)
{
    LOG1("@%s: window = %p, state %d", __FUNCTION__, window, mState);

    PERFORMANCE_TRACES_BREAKDOWN_STEP_NOPARAM();
    Message msg;
    msg.id = MESSAGE_ID_SET_PREVIEW_WINDOW;
    msg.data.previewWin.window = window;
    msg.data.previewWin.synchronous = false;
    // When the window is set to NULL, we should release all Graphic buffer handles synchronously.
    if ((window == NULL) || (mPreviewThread->getPreviewState() == PreviewThread::STATE_NO_WINDOW)) {
        // In case of "deferred start" for preview, we need to be synchronous
        // with the window setting, to properly go through the start preview
        // sequence that is supposed to be synchronous.
        msg.data.previewWin.synchronous = true;
        return mMessageQueue.send(&msg, MESSAGE_ID_SET_PREVIEW_WINDOW);
    } else {
        // Otherwise we can act asynchronously
        return mMessageQueue.send(&msg);
    }
}

void ControlThread::setCallbacks(camera_notify_callback notify_cb,
                                 camera_data_callback data_cb,
                                 camera_data_timestamp_callback data_cb_timestamp,
                                 camera_request_memory get_memory,
                                 void* user)
{
    LOG1("@%s", __FUNCTION__);
    mCallbacks->setCallbacks(notify_cb,
            data_cb,
            data_cb_timestamp,
            get_memory,
            user);
}

void ControlThread::enableMsgType(int32_t msgType)
{
    LOG2("@%s", __FUNCTION__);
    mCallbacks->enableMsgType(msgType);
}

void ControlThread::disableMsgType(int32_t msgType)
{
    LOG2("@%s", __FUNCTION__);
    mCallbacks->disableMsgType(msgType);
}

bool ControlThread::msgTypeEnabled(int32_t msgType)
{
    LOG2("@%s", __FUNCTION__);
    return mCallbacks->msgTypeEnabled(msgType);
}

/**
 * Disable focus callbacks
 */
void ControlThread::disableFocusCallbacks()
{
    if (!mEnableFocusCbAtStart)
        mEnableFocusCbAtStart = msgTypeEnabled(CAMERA_MSG_FOCUS);
    if (!mEnableFocusMoveCbAtStart)
        mEnableFocusMoveCbAtStart = msgTypeEnabled(CAMERA_MSG_FOCUS_MOVE);
    disableMsgType(CAMERA_MSG_FOCUS_MOVE);
    disableMsgType(CAMERA_MSG_FOCUS);
}

/**
 *  Enable focus callbacks in case we disabled them
 */
void ControlThread::enableFocusCallbacks()
{
    if (mEnableFocusCbAtStart)
        enableMsgType(CAMERA_MSG_FOCUS);

    if (mEnableFocusMoveCbAtStart)
        enableMsgType(CAMERA_MSG_FOCUS_MOVE);
}


status_t ControlThread::startPreview()
{
    LOG1("@%s", __FUNCTION__);
    // send message
    Message msg;
    msg.id = MESSAGE_ID_START_PREVIEW;
    return mMessageQueue.send(&msg, MESSAGE_ID_START_PREVIEW);
}

status_t ControlThread::stopPreview()
{
    LOG1("@%s", __FUNCTION__);
    if (mState == STATE_STOPPED) {
        return NO_ERROR;
    }
    // send message and block until thread processes message
    bool videoMode = isParameterSet(CameraParameters::KEY_RECORDING_HINT);
    PerformanceTraces::SwitchCameras::getOriginalMode(videoMode);

    Message msg;
    msg.id = MESSAGE_ID_STOP_PREVIEW;
    return mMessageQueue.send(&msg, MESSAGE_ID_STOP_PREVIEW);
}

/**
 * Sends preview error message to the ControlThread message queue
 *
 * Should be called when asynchronous error occurs during
 * preview streaming. Message handler will try to reset the
 * camera device and restart the preview.
 *
 * See ControlThread::handleMessageErrorPreview()
 */
status_t ControlThread::errorPreview()
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_ERROR_PREVIEW;
    return mMessageQueue.send(&msg);
}

status_t ControlThread::startRecording()
{
    LOG1("@%s", __FUNCTION__);
    // send message and block until thread processes message
    Message msg;
    msg.id = MESSAGE_ID_START_RECORDING;
    return mMessageQueue.send(&msg, MESSAGE_ID_START_RECORDING);
}

status_t ControlThread::stopRecording()
{
    LOG1("@%s", __FUNCTION__);
    // send message and block until thread processes message
    Message msg;
    msg.id = MESSAGE_ID_STOP_RECORDING;
    return mMessageQueue.send(&msg, MESSAGE_ID_STOP_RECORDING);
}

bool ControlThread::previewEnabled()
{
    LOG2("@%s", __FUNCTION__);
    // Preview is essentially shown enabled whenever PreviewThread's
    // state is other than stopped.
    bool enabled =
        (mPreviewThread->getPreviewState() != PreviewThread::STATE_STOPPED);

    // mStillCaptureInProgress indicates a previous call to takePicture()
    // and previewEnabled() needs to return false to act according to API
    // specification. Reality of preview state may be different depending
    // on mState (capture mode) and configuration.
    enabled &= !mStillCaptureInProgress;

    return enabled;
}

bool ControlThread::recordingEnabled()
{
    LOG2("@%s", __FUNCTION__);
    return mState == STATE_RECORDING;
}

status_t ControlThread::setParameters(const char *params)
{
    LOG1("@%s: params = %p", __FUNCTION__, params);

    {
        Mutex::Autolock mLock(mParamCacheLock);
        if (mParamCache && strcmp(mParamCache, params) == 0)
            return OK;
    }

    Message msg;
    msg.id = MESSAGE_ID_SET_PARAMETERS;
    msg.data.setParameters.params = const_cast<char*>(params); // We swear we won't modify params :)

    // mStillCaptureInProgress indicates that application is reconfiguring
    // after takePicture() without stopping. This is valid use case since by
    // the specification we should be stopped after takePicture(). However,
    // continuous-mode may leave the preview running in which case such
    // reconfiguration may cause multiple restartPreviews(). Following
    // startPreview() is required, so we can stop before handling parameters.
    PreviewThread::PreviewState previewState = mPreviewThread->getPreviewState();
    msg.data.setParameters.stopPreviewRequest =
        mStillCaptureInProgress
        && (previewState == PreviewThread::STATE_ENABLED
         || previewState == PreviewThread::STATE_ENABLED_HIDDEN);

    return mMessageQueue.send(&msg, MESSAGE_ID_SET_PARAMETERS);
}

char* ControlThread::getParameters()
{
    LOG2("@%s", __FUNCTION__);

    // Fast path. Just return the static copy right away.
    //
    // This is needed as some applications call getParameters()
    // from various HAL callbacks, causing deadlocks like the following:
    //   A. HAL is flushing picture/video thread and message loop
    //      is blocked until the operation finishes
    //   B. one of the pending picture/video messages, which was
    //      processed just before the flush, has called an app
    //      callback, which again calls HAL getParameters()
    //   C. the app call to getParameters() is synchronous
    //   D. deadlock results, as HAL/ControlThread is blocked on the
    //      flush call of step (A), and cannot process getParameters()
    //
    // Solution: implement getParameters so that it can be called
    //           even when ControlThread's message loop is blocked.
    char *params = NULL;
    mParamCacheLock.lock();
    if (mParamCache)
        params = strdup(mParamCache);
    mParamCacheLock.unlock();

    // Slow path. If cache was empty, send a message.
    //
    // The above case will not get triggered when param cache is NULL
    // (only happens when initially starting).
    if (params == NULL) {
        Message msg;
        msg.id = MESSAGE_ID_GET_PARAMETERS;
        msg.data.getParameters.params = &params; // let control thread allocate and set pointer
        mMessageQueue.send(&msg, MESSAGE_ID_GET_PARAMETERS);
    }

    return params;
}

void ControlThread::putParameters(char* params)
{
    LOG2("@%s: params = %p", __FUNCTION__, params);
    if (params) {
        free(params);
        params = NULL;
    }
}

bool ControlThread::isParameterSet(const char* param)
{
    return android::isParameterSet(param, mParameters);
}

/**
 * Returns value of 'key' in newParams, but only if it is different
 * from its value, or not defined, in oldParams.
 */
String8 ControlThread::paramsReturnNewIfChanged(
        const CameraParameters *oldParams,
        CameraParameters *newParams,
        const char *key)
{
    // note: CameraParameters::get() returns a NULL, but internally it
    //       does not distinguish between a param that is not set,
    //       from a param that is zero length, so we do not make
    //       the disctinction either.

    const char* o = oldParams->get(key);
    const char* n = newParams->get(key);

    // note: String8 segfaults if given a NULL, so thus check
    //      for that here
    String8 oldVal (o, (o == NULL ? 0 : strlen(o)));
    String8 newVal (n, (n == NULL ? 0 : strlen(n)));

    if (oldVal != newVal || !mThreadRunning) // return if changed or if set during init() (thread not running yet)
        return newVal;

    return String8::empty();
}

status_t ControlThread::takePicture()
{
    status_t status = NO_ERROR;
    LOG1("@%s", __FUNCTION__);
    Message msg;

    PERFORMANCE_TRACES_TAKE_PICTURE_QUEUE();

    if (mPanoramaThread->getState() != PANORAMA_STOPPED)
        msg.id = MESSAGE_ID_PANORAMA_PICTURE;
    else if (mPostProcThread->isSmartRunning()) // delaying capture for smart shutter case
        msg.id = MESSAGE_ID_SMART_SHUTTER_PICTURE;
    else
        msg.id = MESSAGE_ID_TAKE_PICTURE;

    status = mMessageQueue.send(&msg);
    if (status == NO_ERROR) {
        mStillCaptureInProgress = (mState != STATE_RECORDING) ? true : false;
        // We need to disable focus callbacks here to ensure application
        // is not receiving them after this call and until the next
        // startPreview(). This is because scenarios that left AF running
        // are possible and applications (including Google reference) get
        // confused from receiving focus callbacks.
        if (mStillCaptureInProgress)
            disableFocusCallbacks();
    }
    return status;
}

status_t ControlThread::cancelPicture()
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_CANCEL_PICTURE;
    return mMessageQueue.send(&msg, MESSAGE_ID_CANCEL_PICTURE);
}

status_t ControlThread::autoFocus()
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_AUTO_FOCUS;
    // Inform focus activation to CallbacksThread
    // (See CallbacksThred::autoFocusActive())
    mCallbacksThread->autoFocusActive(true);
    return mMessageQueue.send(&msg);
}

status_t ControlThread::cancelAutoFocus()
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_CANCEL_AUTO_FOCUS;
    mCallbacksThread->autoFocusActive(false);
    return mMessageQueue.send(&msg);
}

status_t ControlThread::releaseRecordingFrame(void *buff)
{
    LOG2("@%s: buff = %p", __FUNCTION__, buff);
    Message msg;
    msg.id = MESSAGE_ID_RELEASE_RECORDING_FRAME;
    msg.data.releaseRecordingFrame.buff = buff;
    return mMessageQueue.send(&msg);
}

status_t ControlThread::storeMetaDataInBuffers(bool enabled)
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_STORE_METADATA_IN_BUFFER;
    msg.data.storeMetaDataInBuffers.enabled = enabled;
    return  mMessageQueue.send(&msg, MESSAGE_ID_STORE_METADATA_IN_BUFFER);
}

void ControlThread::atomRelease()
{
    LOG2("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_RELEASE;
    mMessageQueue.send(&msg, MESSAGE_ID_RELEASE);
}

void ControlThread::sceneDetected(int sceneMode, bool sceneHdr)
{
    LOG2("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_SCENE_DETECTED;
    if (sceneMode >= 0 && sceneMode < NUM_SCENE_DETECTED) {
        strlcpy(msg.data.sceneDetected.sceneMode, scene_mode_detected[sceneMode], (size_t)SCENE_STRING_LENGTH);
        msg.data.sceneDetected.sceneHdr = sceneHdr;
        mMessageQueue.send(&msg);
    } else {
        ALOGW("%s: the scene mode (%d) provided is not in the defined range", __FUNCTION__, sceneMode);
    }
}

void ControlThread::facesDetected(const ia_face_state *faceState)
{
    LOG2("@%s", __FUNCTION__);
    m3AThread->setFaces(*faceState);
}

int ControlThread::getCameraID()
{
    return mCameraId;
}

void ControlThread::panoramaFinalized(AtomBuffer *buff, AtomBuffer *pvBuff)
{
    LOG1("panorama Finalized frame buffer data %p, id = %d", buff, buff->id);
    Message msg;
    msg.id = MESSAGE_ID_PANORAMA_FINALIZE;
    msg.data.panoramaFinalized.buff = *buff;
    if (pvBuff)
        msg.data.panoramaFinalized.pvBuff = *pvBuff;
    else
        msg.data.panoramaFinalized.pvBuff.buff = NULL;
    mMessageQueue.send(&msg);
}

status_t ControlThread::handleMessagePanoramaFinalize(MessagePanoramaFinalize *msg)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = mCallbacksThread->requestTakePicture(false, false);

    if (status != OK)
        return status;

    PictureThread::MetaData picMetaData;
    fillPicMetaData(picMetaData, false);

    // Initialize the picture thread with the size of the final stiched image
    CameraParameters tmpParam = mParameters;
    tmpParam.setPictureSize(msg->buff.width, msg->buff.height);
    mPictureThread->initialize(tmpParam, 1);

    AtomBuffer *pPvBuff = msg->pvBuff.buff ? &(msg->pvBuff) : NULL;

    status = mPictureThread->encode(picMetaData, &(msg->buff), pPvBuff);
    return status;
}

void ControlThread::panoramaCaptureTrigger()
{
    LOG2("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_PANORAMA_CAPTURE_TRIGGER;
    mMessageQueue.send(&msg);
}

// -- ICallbackPicture implementations
void ControlThread::encodingDone(AtomBuffer *snapshotBuf, AtomBuffer *postviewBuf)
{
    LOG2("@%s: snapshotBuf = %p, postviewBuf = %p, id = %d",
                __FUNCTION__,
                snapshotBuf->dataPtr,
                postviewBuf->dataPtr,
                snapshotBuf->id);
    Message msg;
    msg.id = MESSAGE_ID_ENCODING_DONE;
    msg.data.encodingDone.snapshotBuf = *snapshotBuf;
    msg.data.encodingDone.postviewBuf = *postviewBuf;
    mMessageQueue.send(&msg);
}

void ControlThread::pictureDone(AtomBuffer *snapshotBuf, AtomBuffer *postviewBuf)
{
    LOG1("@%s: snapshotBuf = %p, postviewBuf = %p, id = %d",
            __FUNCTION__,
            snapshotBuf->dataPtr,
            postviewBuf->dataPtr,
            snapshotBuf->id);
    Message msg;
    msg.id = MESSAGE_ID_PICTURE_DONE;
    msg.data.pictureDone.snapshotBuf = *snapshotBuf;
    msg.data.pictureDone.postviewBuf = *postviewBuf;

    mMessageQueue.send(&msg);
}

// -- end ICallbackPicture implementations

void ControlThread::sendCommand(int32_t cmd, int32_t arg1, int32_t arg2)
{
    Message msg;
    msg.id = MESSAGE_ID_COMMAND;
    msg.data.command.cmd_id = cmd;
    msg.data.command.arg1 = arg1;
    msg.data.command.arg2 = arg2;

    // App should wait here until ENABLE_INTEL_PARAMETERS command finish.
    if (cmd == CAMERA_CMD_ENABLE_INTEL_PARAMETERS)
        mMessageQueue.send(&msg, MESSAGE_ID_COMMAND);
    else
        mMessageQueue.send(&msg);
}

void ControlThread::postProcCaptureTrigger()
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_POST_PROC_CAPTURE_TRIGGER;
    mMessageQueue.send(&msg);
}

status_t ControlThread::handleMessageExit(MessageExit *msg)
{
    LOG1("@%s state = %d", __FUNCTION__, mState);
    status_t status;
    if (msg->stopThread)
        mThreadRunning = false;

    switch (mState) {
    case STATE_CAPTURE:
        status = stopCapture();
        break;
    case STATE_PREVIEW_STILL:
    case STATE_PREVIEW_VIDEO:
    case STATE_CONTINUOUS_CAPTURE:
        handleMessageStopPreview();
        break;
    case STATE_RECORDING:
        handleMessageStopRecording();
        break;
    case STATE_STOPPED:
        // do nothing
        break;
    default:
        ALOGW("Exiting in an invalid state, this should not happen!!!");
        break;
    }

    return NO_ERROR;
}

/**
 * Helper function for handleMessageStopPreview() to hanle backgrounding of
 * currently running continuous-mode preview stream.
 *
 * PreviewBackgrounding is allowed in single scenario: when taking a single
 * picture in continuous-mode. Call to stopPreview() is handled through this
 * function and if allowed and possible - the preview stream is left running
 * without stopping. This is to improve shot2shot in special case of application
 * calling stopPreview() (e.g. to reset the window handle) in between shots.
 */
status_t ControlThread::handleContinuousPreviewBackgrounding()
{
    LOG1("@%s", __FUNCTION__);

    if (mThreadRunning == false)
        return INVALID_OPERATION;

    if (mState != STATE_CONTINUOUS_CAPTURE)
        return NO_INIT;

    // allow backgrounding only in post capture sequence
    if (!mStillCaptureInProgress)
        return INVALID_OPERATION;

    // Post-capture stopPreview case
    if (!mISP->isSharedPreviewBufferConfigured()) {
        // Hide the preview first to prevent unnessary debug logs
        mPreviewThread->setPreviewState(PreviewThread::STATE_ENABLED_HIDDEN);
        // When not sharing the window buffers with AtomISP we can
        // just return the Gfx buffers in PreviewThreads possession.
        mPreviewThread->returnPreviewBuffers();
        // Set preview to stopped state, since only re-configuration
        // or closing may happen next.
        mPreviewThread->setPreviewState(PreviewThread::STATE_STOPPED);
        LOG1("Continuous-mode is left running in background");
    } else {
        LOG1("Preview buffers shared, continuous-mode needs to stop");
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

status_t ControlThread::handleContinuousPreviewForegrounding()
{
    PreviewThread::PreviewState previewState;

    if (mState != STATE_CONTINUOUS_CAPTURE)
        return NO_INIT;

    previewState = mPreviewThread->getPreviewState();
    // already in continuous-state, startPreview case
    if (mISP->isOfflineCaptureRunning()) {
        mISP->stopOfflineCapture();
        LOG1("Capture stopped, resuming continuous viewfinder");
    }
    if (previewState == PreviewThread::STATE_STOPPED) {
        // just re-configure previewThread
        int cb_fourcc, width, height, bpl;
        cb_fourcc = V4L2Format(mParameters.getPreviewFormat());
        mISP->getPreviewSize(&width, &height,&bpl);
        mPreviewThread->setPreviewConfig(width, height, cb_fourcc, false);
    } else if (previewState != PreviewThread::STATE_ENABLED
            && previewState != PreviewThread::STATE_ENABLED_HIDDEN) {
        ALOGE("Trying to resume continuous preview from unexpected state!");
        return INVALID_OPERATION;
    }

    mPreviewThread->setPreviewState(PreviewThread::STATE_ENABLED);
    // Check the camera.hal.power property if disable the Preview
    if (gPowerLevel & CAMERA_POWERBREAKDOWN_DISABLE_PREVIEW) {
        mPreviewThread->setPreviewState(PreviewThread::STATE_ENABLED_HIDDEN);
    }
    LOG1("Continuous preview is resumed by foregrounding");
    return NO_ERROR;
}


/**
 * Adapts continuous capture params to fit platform limits.
 *
 * In case the requested combination is not supported (platform
 * does not have big enough ringbuffer for RAW frames),
 * burst-start-index takes priority over burst-fps.
 *
 * The FPS is increased (by reducing skipping done in ISP), until
 * the requested burst-start-index can be supported.
 *
 * \param cfg configuration container to modify
 */
void ControlThread::continuousConfigApplyLimits(ContinuousCaptureConfig &cfg) const
{
    int minOffset = mISP->continuousBurstNegMinOffset();
    int skip = 0;

    if (cfg.numCaptures > 1)
        skip = mFpsAdaptSkip;

    if (mBurstStart < 0) {
        int offset = 0;
        for(offset = minOffset-1; offset < minOffset; skip--) {
            offset = mISP->continuousBurstNegOffset(skip, mBurstStart);
            if (skip == 0)
                break;
        }
        cfg.offset = offset;
    }
    cfg.skip = skip;

    LOG2("@%s: offset %d, skip %d (for start-index %d)",
         __FUNCTION__, cfg.offset, skip, mBurstStart);
}

/**
 * Configures the ISP ringbuffer size in continuous mode.
 *
 * This configuration must be done before preview pipeline
 * is started. During runtime, user-space may modify
 * capture configuration (number of captures, skip, offset),
 * but only to smaller values. If any number of captures or
 * offset needs be changed so that a larger ringbuffer would
 * be needed, then ISP needs to be restarted. The values set
 * here are thus the maximum values.
 * In case algorithms like Ultra Low light are active
 * we need to prepare a big enough ring buffers to satisfy the demands of it
 * This allows us to trigger small bursts of ZSL captures.
 */
status_t ControlThread::configureContinuousRingBuffer()
{
    LOG2("@%s", __FUNCTION__);
    bool capturePriority = true;
    if (mPreviewUpdateMode == IntelCameraParameters::PREVIEW_UPDATE_MODE_CONTINUOUS)
        capturePriority = false;

    ContinuousCaptureConfig cfg;
    if (mULL->isActive() || mBurstLength > 1)
        cfg.numCaptures = MAX(mULL->MAX_INPUT_BUFFERS, mBurstLength);
    else
        cfg.numCaptures = 1;

    cfg.offset = -(mISP->shutterLagZeroAlign());
    cfg.skip = 0;
    continuousConfigApplyLimits(cfg);

    LOG1("%s numcaptures %d, offset %d, skip %d",__FUNCTION__,
                                                cfg.numCaptures,
                                                cfg.offset,
                                                cfg.skip);

    return mISP->prepareOfflineCapture(cfg, capturePriority);
}

/**
 * Configures parameters for continuous capture.
 *
 * In continuous capture mode, parameters for both capture
 * and preview need to be set up before starting the ISP.
 */
status_t ControlThread::initContinuousCapture()
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;


    int fourcc = mISP->getSnapshotPixelFormat();
    AtomBuffer formatDescriptorSs = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_FORMAT_DESCRIPTOR, fourcc);

    mParameters.getPictureSize(&formatDescriptorSs.width, &formatDescriptorSs.height);

    int pvWidth;
    int pvHeight;

    if (mPanoramaThread->getState() == PANORAMA_STOPPED) {
        selectPostviewSize(pvWidth, pvHeight);
    } else {
        IntelCameraParameters::getPanoramaLivePreviewSize(pvWidth, pvHeight, mParameters);
    }

    AtomBuffer formatDescriptorPv
        = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_FORMAT_DESCRIPTOR, selectPostviewFormat(), pvWidth, pvHeight);
    // Configure PictureThread
    mPictureThread->initialize(mParameters, mISP->zoomRatio(mParameters.getInt(CameraParameters::KEY_ZOOM)));

    mISP->setSnapshotFrameFormat(formatDescriptorSs);
    configureContinuousRingBuffer();
    mISP->setPostviewFrameFormat(formatDescriptorPv);

    burstStateReset();

    PERFORMANCE_TRACES_BREAKDOWN_STEP("Done");
    return status;
}

/**
 * Frees resources related to continuous capture
 *
 * \param flushPictures whether to flush the picture thread
 */
void ControlThread::releaseContinuousCapture(bool flushPictures)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    if (flushPictures) {
        // This covers cases when we need to fallback from
        // continuous mode to online mode to do a capture.
        // As capture is not runnning in these cases, flush
        // is not needed.
        status = cancelPictureThread();
        if (status != NO_ERROR) {
            ALOGE("Error flushing PictureThread!");
        }
    }
}

/**
 *  Selects which shooting mode is active.
 *  The selection is based on the HAL state and on other burst related variables
 *  This selection is done when take_picture is received.
 *  The actual variables involved in the decision process may change at other
 *  times for other reasons.
 *
 * \return One of the shooting modes
 * \sa ShootingMode
 */
ControlThread::ShootingMode ControlThread::selectShootingMode()
{
    ShootingMode ret = SHOOTING_MODE_NONE;
    FlashMode flashMode = m3AControls->getAeFlashMode();
    bool flashOn = (flashMode == CAM_AE_FLASH_MODE_TORCH ||
                   flashMode == CAM_AE_FLASH_MODE_ON);

    switch (mState) {
        case STATE_PREVIEW_STILL:
        case STATE_PREVIEW_VIDEO:
            ret = SHOOTING_MODE_SINGLE;
            break;

        case STATE_RECORDING:
            ret = SHOOTING_MODE_VIDEO_SNAP;
            break;

        case STATE_CONTINUOUS_CAPTURE:
            if (isBurstRunning())
                ret = SHOOTING_MODE_ZSL_BURST;
            else
                ret = SHOOTING_MODE_ZSL;
            /* Trigger ULL only when user did not force flash and when we have enough available buffers */
            if (mULL->isActive() && mULL->trigger() && !flashOn &&
                mAvailableSnapshotBuffers.size() >= static_cast<size_t>(mULL->MAX_INPUT_BUFFERS) &&
                mAvailablePostviewBuffers.size() >= static_cast<size_t>(mULL->MAX_INPUT_BUFFERS) /* TODO Use getULLBurstLength()*/)
                ret = SHOOTING_MODE_ULL;
            break;

        case STATE_CAPTURE:
            if (isBurstRunning())
                ret = SHOOTING_MODE_BURST;
            break;

        case STATE_STOPPED:
        default:
            ALOGW("Unexpected state (%d) to select the shooting mode",mState);
            break;
    }
    LOG1("Shooting Mode selected: %d",ret);
    return ret;
}

/**
 * Selects which still preview mode to use.
 *
 * @return STATE_CONTINUOUS_CAPTURE or STATE_PREVIEW_STILL
 */
ControlThread::State ControlThread::selectPreviewMode(const CameraParameters &params)
{
    // Whether hardware (SoC, memories) supports continuous mode?
    if (PlatformData::supportsContinuousCapture(mCameraId) == false) {
        LOG1("@%s: Disabling continuous mode, not supported by platform", __FUNCTION__);
        return STATE_PREVIEW_STILL;
    }

    // Picture-sizes smaller than preview-size do not work with
    // current CSS firmwares in continuous/ZSL mode.
    // TODO: should be removed when CSS can handle this, see PSI BZ 73112
    int picWidth = 0, picHeight = 0;
    int vfWidth = 0, vfHeight = 0;
    params.getPictureSize(&picWidth, &picHeight);
    params.getPreviewSize(&vfWidth, &vfHeight);
    if (!PlatformData::snapshotResolutionSupportedByZSL(mCameraId, picWidth, picHeight)) {
        LOG1("@%s: picture-size %dx%d, disabling continuous mode",
             __FUNCTION__, picWidth, picHeight);
        goto online_preview;
    }

    // Low preview resolutions have known issues in continuous mode.
    // TODO: to be removed, tracked in BZ 81396
    if (PlatformData::sensorType(mCameraId) == SENSOR_TYPE_RAW &&
        vfWidth < 640 && vfHeight < 360) {
        LOG1("@%s: continuous mode not available for preview size %ux%u",
             __FUNCTION__, vfWidth, vfHeight);
        goto online_preview;
    }

    if (mHdr.enabled) {
        LOG1("@%s: HDR enabled, disabling continuous mode",
             __FUNCTION__);
        goto online_preview;
    }

    if (mBurstLength > 1 && mBurstStart >= 0) {
        LOG1("@%s: Burst length of %d requested, disabling continuous mode",
             __FUNCTION__, mBurstLength);
        goto online_preview;
    }

    if (mBurstStart < 0) {
        // One buffer in the raw ringbuffer is reserved for streaming
        // from sensor, so output frame count is limited to maxSize-1.
        int maxBufSize = PlatformData::maxContinuousRawRingBufferSize(mCameraId);
        if (mBurstLength > maxBufSize - 1) {
             LOG1("@%s: Burst length of %d with offset %d requested, disabling continuous mode",
                  __FUNCTION__, mBurstLength, mBurstStart);
             goto online_preview;
        }

        // Bracketing not supported in continuous mode as the number
        // captures is not fixed.
        if (mBracketManager->getBracketMode() != BRACKET_NONE) {
            LOG1("@%s: Bracketing requested, disabling continuous mode",
                 __FUNCTION__);
            goto online_preview;
        }
    }

    if (CameraDump::isDumpImageEnable(CAMERA_DEBUG_DUMP_RAW)) {
        LOG1("@%s: Raw dump enabled, disabling continuous mode", __FUNCTION__);
        goto online_preview;
    }

    if (mISP->getLowLight()) {
        LOG1("@%s: ANR enabled, disabling continuous mode", __FUNCTION__);
        goto online_preview;
    }

    // No continuous mode for 3rd party firmware
    if (mIspExtensionsEnabled) {
        LOG1("@%s: ISP Extensions enabled, disabling continuous mode", __FUNCTION__);
        goto online_preview;
    }

    LOG1("@%s: Selecting continuous still preview mode", __FUNCTION__);
    return STATE_CONTINUOUS_CAPTURE;

online_preview:
    // In online preview we cannot support preview update modes 'during_capture' and 'continuous'
    if (mPreviewUpdateMode == IntelCameraParameters::PREVIEW_UPDATE_MODE_DURING_CAPTURE ||
        mPreviewUpdateMode == IntelCameraParameters::PREVIEW_UPDATE_MODE_CONTINUOUS) {
        mPreviewUpdateMode = IntelCameraParameters::PREVIEW_UPDATE_MODE_STANDARD;
        ALOGW("Forcing preview update mode to standard, conflicting settings");
    }
    return STATE_PREVIEW_STILL;

}

status_t ControlThread::startPreviewCore(bool videoMode)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    int width;
    int height;
    int cb_fourcc;
    int bpl;
    int fps;
    State state;
    AtomMode mode;

    if (mState != STATE_STOPPED) {
        ALOGE("Must be in STATE_STOPPED to start preview");
        return INVALID_OPERATION;
    }

    PerformanceTraces::SwitchCameras::called(videoMode);

    // ISP can be de-initialized during ErrorPreview notification.
    // It is there necessary to check if the ISP is still Initialized everytime we restart it.
    if (!mISP->isDeviceInitialized())
        mISP->init();

    if (videoMode) {
        LOG1("Starting preview in video mode");
        state = STATE_PREVIEW_VIDEO;
        mode = MODE_VIDEO;
        fps = mISP->getRecordingFramerate();
        mParameters.getVideoSize(&width, &height);

        // Video size is updated later than other parameters, so validate high speed  params here
        if (!validateHighSpeedResolutionFps(width, height, fps)) {
            return BAD_VALUE;
        }

        // Workaround: In high speed recording, the scene mode should be SPORTS
        // Note: scene mode setting resets all 3A parameters
        if (fps > DEFAULT_RECORDING_FPS)
            m3AControls->setAeSceneMode(CAM_AE_SCENE_MODE_SPORTS);

        mISP->setVideoFrameFormat(width, height);

        status = mISP->setDVS(mDvsEnable);

        if (status != NO_ERROR) {
            ALOGW("@%s: Failed to set DVS %s", __FUNCTION__, mDvsEnable ? "enabled" : "disabled");
        }

    } else {
        LOG1("Starting preview in still mode");
        state = selectPreviewMode(mParameters);
        if (state == STATE_PREVIEW_STILL)
            mode = MODE_PREVIEW;
        else
            mode = MODE_CONTINUOUS_CAPTURE;
    }

    if (state == STATE_CONTINUOUS_CAPTURE) {
        if (initContinuousCapture() != NO_ERROR) {
            return BAD_VALUE;
        }
    }

    const char* cb_fourcc_s = mParameters.getPreviewFormat();
    cb_fourcc = V4L2Format(cb_fourcc_s);
    if (!cb_fourcc) {
        ALOGW("Unsupported preview callback fourcc : %s", cb_fourcc_s ? cb_fourcc_s : "not set");
    }
    mParameters.getPreviewSize(&width, &height);


    // Load any ISP extensions before ISP is started

    // workaround for FR during HAL ZSL - do not use extensions
    if (mISP->isHALZSLEnabled()) {
        mPostProcThread->unloadIspExtensions(); // sends NULL to ia_face_set_acceleration -> enables SW FR
    } else if (!mIspExtensionsEnabled) {
        mPostProcThread->loadIspExtensions(videoMode);
    } else {
        // load 3rd party ISP extensions
        mAccManagerThread->loadIspExtensions();
    }
    PERFORMANCE_TRACES_BREAKDOWN_STEP("loadIspExt");

    // By default, the number of preview and video buffers are set
    // based on PlatformData.
    // Exception to this is that in video-mode we currently use
    // as many preview-buffers as recording-buffers. This decision
    // is done explicitly here.
    // TODO: Preview and recording buffers are no longer coupled and
    //       one can consider removing this rule.
    if (videoMode) {
        mNumBuffers = PlatformData::getRecordingBufNum();
    } else {
        mNumBuffers = PlatformData::getPreviewBufNum();
    }
    mISP->setPreviewBufNum(mNumBuffers);

    // using mIntelParamsAllowed to distinquish applications using public
    // API from ones using agreed sequences when in continuous mode.
    // For API compliant continuous-mode we disable sharedGfxBuffers (0-copy)
    // to be able to release and re-acquire external buffers while keeping
    // continuous mode running over stopPreview() and startPreview() after
    // takePicture(). This is done for faster shot2shot.
    // TODO: support for fluent transitions regardless of buffer type
    //       transparently

    bool useSharedGfxBuffers =
        (mPreviewUpdateMode != IntelCameraParameters::PREVIEW_UPDATE_MODE_WINDOWLESS)
        && (mIntelParamsAllowed || mode != MODE_CONTINUOUS_CAPTURE);
    mPreviewThread->setPreviewConfig(width, height, cb_fourcc, useSharedGfxBuffers, mNumBuffers);

    // Get the preview size from PreviewThread and pass the configuration to AtomISP.
    mPreviewThread->fetchPreviewBufferGeometry(&width, &height, &bpl);
    mISP->setPreviewFrameFormat(width, height, bpl);

    if (useSharedGfxBuffers) {
        Vector<AtomBuffer> sharedGfxBuffers;
        status = mPreviewThread->fetchPreviewBuffers(sharedGfxBuffers);
        if (status == NO_ERROR) {
            if ((int)sharedGfxBuffers.size() != mNumBuffers) {
                ALOGE("Invalid shared preview buffer count configuration");
                return UNKNOWN_ERROR;
            }
            bool cached = isParameterSet(IntelCameraParameters::KEY_HW_OVERLAY_RENDERING) ? true: false;
            LOG1("Setting GFX preview: %d bufs, cached/overlay %d, shared 0-copy mode", mNumBuffers, cached);
            mISP->setGraphicPreviewBuffers(sharedGfxBuffers.editArray(), mNumBuffers, cached);
            PERFORMANCE_TRACES_BREAKDOWN_STEP("setGFXPreviewBuffers");
        } else {
            LOG1("PreviewThread not sharing Gfx buffers, using internal buffers");
        }
    }

    status = mISP->configure(mode);
    if (status != NO_ERROR) {
        ALOGE("Error configuring ISP");
        return status;
    }

    status = mISP->allocateBuffers(mode);
    if (status != NO_ERROR) {
        ALOGE("Error allocate buffers in ISP");
        return status;
    }

    if (m3AControls->isIntel3A() && (!(gPowerLevel & CAMERA_POWERBREAKDOWN_DISABLE_3A))) {
        // Enable auto-focus by default
        m3AControls->setAfEnabled(true);
        m3AThread->enable3A();
        if (m3AControls->switchModeAndRate(mode, mHwcg.mSensorCI->getFramerate()) != NO_ERROR)
            ALOGE("Failed switching 3A at %.2f fps", mHwcg.mSensorCI->getFramerate());


        mISP->attachObserver(m3AThread.get(), OBSERVE_3A_STAT_READY);
    }

    // Update focus areas for the proper window size
    if (!mFaceDetectionActive && !mFocusAreas.isEmpty()) {
        size_t winCount(mFocusAreas.numOfAreas());
        CameraWindow *focusWindows = new CameraWindow[winCount];
        mFocusAreas.toWindows(focusWindows);
        convertAfWindows(focusWindows, winCount);
        if (m3AControls->setAfWindows(focusWindows, winCount) != NO_ERROR) {
            ALOGE("Could not set AF windows. Resseting the AF to %d", CAM_AF_MODE_AUTO);
            m3AControls->setAfMode(CAM_AF_MODE_AUTO);
        }
        delete[] focusWindows;
        focusWindows = NULL;
    }

    // Update the spot mode window for the proper window size.
    if (m3AControls->getAeMeteringMode() == CAM_AE_METERING_MODE_SPOT && mMeteringAreas.isEmpty()) {
        // Update for the "fixed" AE spot window (Intel extension):
        LOG1("%s: setting forced spot window.", __FUNCTION__);
        AAAWindowInfo aaaWindow;
        m3AControls->getGridWindow(aaaWindow);
        updateSpotWindow(aaaWindow.width, aaaWindow.height);
    } else if (m3AControls->getAeMeteringMode() == CAM_AE_METERING_MODE_SPOT) {
        // This update is when the AE metering is internally set to
        // "spot" mode by the HAL, when user has set the AE metering window.
        LOG1("%s: setting metering area with spot window.", __FUNCTION__);
        size_t winCount(mMeteringAreas.numOfAreas());
        CameraWindow *meteringWindows = new CameraWindow[winCount];
        CameraWindow aeWindow;
        mMeteringAreas.toWindows(meteringWindows);
        convertFromAndroidToIaCoordinates(meteringWindows[0], aeWindow);

        if (m3AControls->setAeWindow(&aeWindow) != NO_ERROR) {
            ALOGW("Error setting AE metering window. Metering will not work");
        }
        delete[] meteringWindows;
        meteringWindows = NULL;
    }

    // ControlThread must be the observer before PreviewThread to ensure that
    // the recording buffer dequeue handling message is guaranteed to happen
    // before any possible preview return buffer handlers. Since the preview
    // thread will get the observer notification later with this order, that is
    // guaranteed. Thus we know, that if the recording buffer is using the
    // preview buffer data for encoding, the handler for the recording buffer
    // dequeue has run before the preview return buffer handler runs.
    mISP->attachObserver(this, OBSERVE_PREVIEW_STREAM);
    mISP->attachObserver(mPreviewThread.get(), OBSERVE_PREVIEW_STREAM);

    if (!mIspExtensionsEnabled) {
        mPreviewThread->setCallback(
            static_cast<ICallbackPreview*>(mPostProcThread.get()),
            ICallbackPreview::OUTPUT_WITH_DATA);
    } else {
        mPreviewThread->setCallback(
                static_cast<ICallbackPreview*>(mAccManagerThread.get()),
                ICallbackPreview::OUTPUT_WITH_DATA);
    }

    PERFORMANCE_TRACES_BREAKDOWN_STEP("set3AParams");
    // start the data flow
    status = mISP->start();
    if (status == NO_ERROR) {
        mState = state;
        mPreviewThread->setPreviewState(PreviewThread::STATE_ENABLED);
        // Check the camera.hal.power property if disable the Preview
        if (gPowerLevel & CAMERA_POWERBREAKDOWN_DISABLE_PREVIEW) {
            mPreviewThread->setPreviewState(PreviewThread::STATE_ENABLED_HIDDEN);
        }
    } else {
        ALOGE("Error starting ISP!");
        mPreviewThread->returnPreviewBuffers();
        mISP->detachObserver(mPreviewThread.get(), OBSERVE_PREVIEW_STREAM);
        mISP->detachObserver(this, OBSERVE_PREVIEW_STREAM);
        if (m3AControls->isIntel3A()) {
            mISP->detachObserver(m3AThread.get(), OBSERVE_PREVIEW_STREAM);
        }
    }

    return status;
}

/**
 * Stops ISP and frees allocated resources
 *
 * \param flushPictures whether to flush the picture thread
 */
status_t ControlThread::stopPreviewCore(bool flushPictures)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    // synchronize and pause the preview dequeueing
    mISP->pauseObserver(OBSERVE_FRAME_SYNC_SOF);
    mISP->pauseObserver(OBSERVE_PREVIEW_STREAM);
    mISP->pauseObserver(OBSERVE_3A_STAT_READY);

    // Before stopping the ISP, flush any buffers in picture
    // and video threads. This is needed as AtomISP::stop() may
    // deallocate buffers and the picture/video threads might
    // otherwise hold invalid references.
    mPreviewThread->flushBuffers();

    // Flush also the pending messages done based on Preview
    mMessageQueue.remove(MESSAGE_ID_DEQUEUE_RECORDING);

    mPostProcThread->flushFrames();

    if (mState == STATE_PREVIEW_VIDEO ||
        mState == STATE_RECORDING) {
        status = mVideoThread->flushBuffers();
    }
    State oldState = mState;
    status = mISP->stop();
    if (status == NO_ERROR) {
        mState = STATE_STOPPED;
    } else {
        ALOGE("Error stopping ISP in preview mode!");
    }

    mISP->detachObserver(mPreviewThread.get(), OBSERVE_PREVIEW_STREAM);

    // we only need to attach the 3AThread to preview stream for RAW type of cameras
    // when we use the 3A algorithm running on Atom
    if (m3AControls->isIntel3A()) {
        mISP->detachObserver(m3AThread.get(), OBSERVE_3A_STAT_READY);
        // Detaching DVS observer. Just to make sure, although it might not be attached:
        // might be a non-RAW sensor, or enabling failed on startPreviewCore().
        // It is OK to detach; if the observer is not attached, detachObserver()
        // returns BAD_VALUE.
    }
    mISP->detachObserver(this, OBSERVE_PREVIEW_STREAM);
    mMessageQueue.remove(MESSAGE_ID_DEQUEUE_RECORDING);

    status = mPreviewThread->returnPreviewBuffers();
    if (!mIspExtensionsEnabled) {
        mPostProcThread->unloadIspExtensions();
    } else {
        mAccManagerThread->unloadIspExtensions();
        mIspExtensionsEnabled = false;
    }

    if (oldState == STATE_CONTINUOUS_CAPTURE)
        releaseContinuousCapture(flushPictures);

    mPreviewThread->setPreviewState(PreviewThread::STATE_STOPPED);

    LOG2("Preview stopped after %d frames", mPreviewThread->getFramesDone());

    PERFORMANCE_TRACES_BREAKDOWN_STEP("Done");
    return status;
}

status_t ControlThread::stopCapture()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    if (mState != STATE_CAPTURE) {
        ALOGE("Must be in STATE_CAPTURE to stop capture");
        return INVALID_OPERATION;
    }
    if (mHdr.inProgress)
        mBracketManager->stopBracketing();

    status = cancelPictureThread();
    if (status != NO_ERROR) {
        ALOGE("Error canceling PictureThread!");
        return status;
    }

    mPreviewThread->flushBuffers();

    status = mISP->stop();
    if (status != NO_ERROR) {
        ALOGE("Error stopping ISP!");
        return status;
    }

    mState = STATE_STOPPED;
    burstStateReset();

    // Reset AE and AF in case HDR/bracketing was used (these features
    // manually configure AE and AF during takePicture)
    if (mBracketManager->getBracketMode() == BRACKET_EXPOSURE) {
        AeMode publicAeMode = m3AControls->getPublicAeMode();
        m3AControls->setAeMode(publicAeMode);
    }

    if (mBracketManager->getBracketMode() == BRACKET_FOCUS) {
        AfMode afMode = m3AControls->getAfMode();
        m3AControls->setAfMode(afMode);
    }

    if (mHdr.enabled || mHdr.inProgress) {
        hdrRelease();
    }
    return status;
}

status_t ControlThread::restartPreview(bool videoMode)
{
    LOG1("@%s: mode = %s", __FUNCTION__, videoMode?"VIDEO":"STILL");
    bool faceActive = mFaceDetectionActive;
    // Check if the preview is actually running while restart is requested.
    // We don't want to trigger preview start, e.g., during setParameters(), unless
    // the preview was running in the first place.
    bool previewEn = previewEnabled();

    /**
     * Postcapture processing items must be completed when preview is stopped
     * or re-started. See the comment in handleMessageStopPreview
     * The re-start because of change of settings is triggered by application
     * that should wait for the post-capture processing to complete.
     */
    cancelPostCaptureThread();
    // cancelPictureThread as well to avoid it happens in stopPreviewCore
    if (mState == STATE_CONTINUOUS_CAPTURE)
        cancelPictureThread();

    stopFaceDetection(true);
    status_t status = stopPreviewCore();
    if (status == NO_ERROR && previewEn)
        status = startPreviewCore(videoMode);
    if (faceActive)
        startFaceDetection();
    return status;
}


/**
 * Starts rendering an output frame from the raw
 * ringbuffer.
 */
status_t ControlThread::startOfflineCapture()
{
    assert(mState == STATE_CONTINUOUS_CAPTURE);

    ContinuousCaptureConfig cfg;
    cfg.numCaptures = 1;
    cfg.offset = -(mISP->shutterLagZeroAlign());
    cfg.skip = 0;
    if (mBurstLength > 0)
        cfg.numCaptures = mBurstLength;
    else
        cfg.numCaptures = 1;
    continuousConfigApplyLimits(cfg);

    // in case preview has just started, we need to limit
    // how long we can look back
    int framesDone = mPreviewThread->getFramesDone();
    if (framesDone < -cfg.offset)
        cfg.offset = -framesDone;

    mISP->startOfflineCapture(cfg);

    return NO_ERROR;
}

status_t ControlThread::handleMessageStartPreview()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    PERFORMANCE_TRACES_BREAKDOWN_STEP_NOPARAM();
    if (mState == STATE_CAPTURE) {
        status = stopCapture();
        if (status != NO_ERROR) {
            ALOGE("Could not stop capture before start preview!");
            mMessageQueue.reply(MESSAGE_ID_START_PREVIEW, status);
            return status;
        }
    }

    mStillCaptureInProgress = false;
    LOG1("Reset CaptureSubState %s -> IDLE (start preview)", sCaptureSubstateStrings[mCaptureSubState]);
    mCaptureSubState = STATE_CAPTURE_IDLE;

    // Check if we previously disabled focus callbacks
    enableFocusCallbacks();

    if (mState == STATE_STOPPED) {
        // API says apps should call startFaceDetection when resuming preview
        // stop FD here to avoid accidental FD.
        stopFaceDetection(true);
        if (mPreviewThread->isWindowConfigured() || mISP->isFileInjectionEnabled()
            || mPreviewUpdateMode == IntelCameraParameters::PREVIEW_UPDATE_MODE_WINDOWLESS) {
            bool videoMode = isParameterSet(CameraParameters::KEY_RECORDING_HINT);
            status = startPreviewCore(videoMode);
        } else {
            ALOGI("Preview window not set deferring start preview until then");
            mPreviewThread->setPreviewState(PreviewThread::STATE_NO_WINDOW);
        }
    } else if (mState == STATE_CONTINUOUS_CAPTURE) {
        // already in continuous-state
        status = handleContinuousPreviewForegrounding();
    } else {
        status = INVALID_OPERATION;
    }

    if (status != NO_ERROR) {
        ALOGE("Error starting preview. Invalid state!");
    }

preview_started:
    mPreviewThread->setCallback(this, ICallbackPreview::INPUT_ONCE);
    mMessageQueue.reply(MESSAGE_ID_START_PREVIEW, status);
    return status;
}

status_t ControlThread::handleMessageStopPreview()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    /**
     * We cancel any ongoing capture process (and post-capture
     * processing) based on assumption that application is no longer interested
     * in receiving the jpeg if it is stoping the preview.
     * This is done to protect racing conditions with unfinished capture
     * process and camera reconfiguration (setParameters) in general.
     *
     * Note: In case snapshot is already sent to PictureThread for
     *       encoding, we may or may not end up calling picture
     *       callbacks. Callback would get blocked until this
     *       stopPreview finishes.
     *       It is up to application to ensure it blocks for jpeg
     *       before letting other API calls to happen or touches
     *       into callback interfaces given with takePicture().
     *       If we are here, ANR is expected - just protecting
     *       against crashes.
     */

    status = cancelCapture();
    if (status != NO_ERROR)
        ALOGE("There was failures while canceling capture process");

    // In STATE_CAPTURE, preview is already stopped, nothing to do
    if (mState != STATE_CAPTURE) {
        stopFaceDetection(true);
        if (mState == STATE_CONTINUOUS_CAPTURE) {
            status = handleContinuousPreviewBackgrounding();
            if (status == NO_ERROR)
                goto preview_stopped;
        }
        if (mState != STATE_STOPPED) {
            status = stopPreviewCore();
        } else {
            ALOGE("Error stopping preview. Invalid state!");
            status = INVALID_OPERATION;
        }
    }

    // Loose our preview window handle and let service maintain
    // it between stop and start
    mPreviewThread->setPreviewWindow(NULL);
preview_stopped:
    // return status and unblock message sender
    mMessageQueue.reply(MESSAGE_ID_STOP_PREVIEW, status);
    return status;
}

/**
 * Handler for error in preview stream
 *
 * Stops the preview core without losing the window handle and
 * calls AtomISP::deInitDevice() for complete reset to the camera driver.
 *
 * AtomISP state is checked specifically in the message queue timeout handler.
 *
 * See handleMessageTimeout().
 */
status_t ControlThread::handleMessageErrorPreview()
{
    LOG1("@%s", __FUNCTION__)
    status_t status = NO_ERROR;
    if (mState != STATE_STOPPED && mState != STATE_CAPTURE) {
        status = stopPreviewCore(true);
        mISP->deInitDevice();
        ALOGE("Preview was stopped due error in stream, trying to recover (timeout 5s)...");
    } else {
        ALOGE("Preview stream error unhandled, unexpected state (%d)", mState);
    }

    return status;
}

/**
 * Handler for MessageQueue::receive timeout (5s)
 *
 * Initially checks whether we were stopped because of an error in
 * preview and tries to recover the preview state.
 */
status_t ControlThread::handleMessageTimeout()
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    if (!mISP->isDeviceInitialized()) {
        status = mISP->init();
        if (status != NO_ERROR) {
            ALOGE("Error initializing ISP");
        } else {
            bool videoMode = isParameterSet(CameraParameters::KEY_RECORDING_HINT) ? true : false;
            status = startPreviewCore(videoMode);
            if (status)
                ALOGE("%s: Restart Preview failed", __FUNCTION__);
        }
    } else {
        LOG2("%s: nothing to do", __FUNCTION__);
    }

    return status;
}

/**
 *  Message Handler for setPreviewWindow HAL call
 *  Actual configuration is taken care of by PreviewThread
 *  Preview restart is done if preview is enabled
 */
status_t ControlThread::handleMessageSetPreviewWindow(MessagePreviewWindow *msg)
{
    LOG1("@%s state = %d window %p", __FUNCTION__, mState, msg->window);
    status_t status = NO_ERROR;

    if (mPreviewThread == NULL)
        return NO_INIT;

    bool videoMode = isParameterSet(CameraParameters::KEY_RECORDING_HINT) ? true : false;
    PreviewThread::PreviewState currentState = mPreviewThread->getPreviewState();

    if (currentState == PreviewThread::STATE_NO_WINDOW
        && (msg->window != NULL)) {
        status = mPreviewThread->setPreviewWindow(msg->window);
        // Start preview if it was already requested by user
        startPreviewCore(videoMode);
    } else if (msg->window != NULL
        && mPreviewUpdateMode == IntelCameraParameters::PREVIEW_UPDATE_MODE_WINDOWLESS
        && currentState != PreviewThread::STATE_STOPPED) {
        // preview was started windowless, force back to standard and make it public
        mPreviewUpdateMode = IntelCameraParameters::PREVIEW_UPDATE_MODE_STANDARD;
        mParameters.set(IntelCameraParameters::KEY_PREVIEW_UPDATE_MODE,
                        IntelCameraParameters::PREVIEW_UPDATE_MODE_STANDARD);
        // stop preview
        bool faceActive = mFaceDetectionActive;
        stopFaceDetection(true);
        stopPreviewCore();
        // start preview with new window
        status = mPreviewThread->setPreviewWindow(msg->window);
        startPreviewCore(videoMode);
        if (faceActive)
            startFaceDetection();
    } else if (msg->window == NULL
               && currentState == PreviewThread::STATE_STOPPED
               && mState == STATE_CONTINUOUS_CAPTURE) {
        // if we are in continuous-mode and backgrounding-state
        // and window is set to null, then stop review
        stopPreviewCore();
        status = mPreviewThread->setPreviewWindow(msg->window);
    } else if (msg->window == NULL
               && currentState == PreviewThread::STATE_ENABLED) {
        // stop face detection before free preview buffer.
        stopFaceDetection(true);
        // Notes:
        //  1. msg->window == NULL comes from CameraService
        //     before calling stopPreview().
        //  2. when the window is set to NULL, must free all Graphic buffer
        //     handles synchronously.
        //  3. change preview state to STATE_NO_WINDOW.
        //  4. don't know if application will set a new window to Camera
        //     HAL after window was set to NULL.
        status = mPreviewThread->setPreviewWindow(msg->window);
        mPreviewThread->setPreviewState(PreviewThread::STATE_NO_WINDOW);
    } else {
        // Notes:
        //  1. msg->window != NULL may come from applications explicit call
        //     to setPreviewDisplay() or setPreviewTexture():
        //      - API if preview is stopped
        //      - running preview does not currently continue
        //  2. msg->window != NULL is always called by CameraService before
        //     startPreview(), with the handle that was previosly set.
        status = mPreviewThread->setPreviewWindow(msg->window);
    }

    // Send the reply, in case we need to be synchronous. See: setPreviewWindow()
    if (msg->synchronous)
        mMessageQueue.reply(MESSAGE_ID_SET_PREVIEW_WINDOW, status);

    return status;
}

status_t ControlThread::handleMessageStartRecording()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    int width,height,widthPreview,heightPreview;
    char sizes[25];

    if (mState == STATE_PREVIEW_VIDEO) {
        mState = STATE_RECORDING;
    } else if (mState == STATE_PREVIEW_STILL ||
               mState == STATE_CONTINUOUS_CAPTURE) {
        /* We are in PREVIEW_STILL mode; in order to start recording
         * we first need to stop AtomISP and restart it with MODE_VIDEO
         */
        bool videoMode = true;
        mISP->applyISPLimitations(&mParameters, mDvsEnable, videoMode);
        status = restartPreview(videoMode);
        if (status != NO_ERROR) {
            ALOGE("Error restarting preview in video mode");
        }
        mState = STATE_RECORDING;
    } else {
        ALOGE("Error starting recording. Invalid state!");
        status = INVALID_OPERATION;
    }

   /* Change the snapshot size and thumbnail size as per current video
    * snapshot limitations.
    * Only supported size is the size of the video
    * and thumbnail size is the size of preview.
    */
    storeCurrentPictureParams();

    mISP->getVideoSize(&width, &height, NULL);
    mParameters.setPictureSize(width, height);

    if (mAllocatedSnapshotBuffers.size() == mAvailableSnapshotBuffers.size()) {
        allocateSnapshotAndPostviewBuffers(true);
    } else {
        LOG1("%s not safe to allocate now, some snapshot buffers are not returned, skipping", __FUNCTION__);
    }

    snprintf(sizes, 25, "%dx%d", width,height);
    LOG1("video snapshot size %dx%d", width, height);
    mParameters.set(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES, sizes);
    mParameters.getPreviewSize(&widthPreview, &heightPreview);

    // avoid that thumbnail is larger than image in case of small video size
    if (widthPreview > width) {
        widthPreview = width;
        heightPreview = height;
    }

    // Limit thumbnail size less than 480p to reduce thumbnail Jpeg size.
    // Make sure total Exif size less than 64k.
    if (heightPreview >= RECONFIGURE_THUMBNAIL_HEIGHT_LIMIT) {
        reconfigureThumbnailSize(widthPreview, heightPreview);
    }

    LOG1("video snapshot thumbnail size %dx%d", widthPreview, heightPreview);
    mParameters.set(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH, widthPreview);
    mParameters.set(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT, heightPreview);
    snprintf(sizes, 25, "%dx%d,0x0", widthPreview,heightPreview);
    mParameters.set(CameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES, sizes);
    updateParameterCache();

    // Store device orientation at the start of video recording
    if (mSaveMirrored && (PlatformData::cameraFacing(mCameraId) == CAMERA_FACING_FRONT)) {
        mRecordingOrientation = mCurrentOrientation;
    }

    // return status and unblock message sender
    mMessageQueue.reply(MESSAGE_ID_START_RECORDING, status);
    return status;
}

status_t ControlThread::handleMessageStopRecording()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    if (mState == STATE_RECORDING) {
        /*
         * Even if startRecording was called from PREVIEW_STILL mode, we can
         * switch back to PREVIEW_VIDEO now since we got a startRecording
         */
        status = mVideoThread->flushBuffers();
        if (status != NO_ERROR)
            ALOGE("Error flushing video thread");
        mState = STATE_PREVIEW_VIDEO;
    } else {
        ALOGE("Error stopping recording. Invalid state!");
        status = INVALID_OPERATION;
    }

    if (mCaptureSubState == STATE_CAPTURE_STARTED) {
        // cancel video snapshot
        mPictureThread->flushBuffers();
        LOG1("CaptureSubState %s -> IDLE (stopRecording)", sCaptureSubstateStrings[mCaptureSubState]);
        mCaptureSubState = STATE_CAPTURE_IDLE;
    }
    // clear reserved lists
    mVideoSnapshotBuffers.clear();
    mRecordingBuffers.clear();

    // release buffers owned by encoder since it is not going to return them
    mISP->returnRecordingBuffers();

    /**
     * Restore the actual still picture parameters before we started video
     * In this way we lift the restrictions that we imposed because of
     * video snapshot implementation
     */
    restoreCurrentPictureParams();

    // return status and unblock message sender
    mMessageQueue.reply(MESSAGE_ID_STOP_RECORDING, status);
    return status;
}

/* This function is used to reduce thumbnail size in Video Snapshot Mode when start recording video.
 * Make sure total Exif size less than 64k and include one thumbnail image.
 * Reconfigure the thumbnail width and height to default value same with still capture.
 * Based on aspect ratio, change it to jpeg-thumbnail-size-values(320x240,240x320,320x180,180x320).
 */
void ControlThread::reconfigureThumbnailSize(int &width, int &height)
{
    LOG1("@%s", __FUNCTION__);

    /* check input parameter */
    if (height <= 0) {
        ALOGE("error input thumbnail height");
        return;
    }

    float aspect = 0.0f;
    const float tolerance = 0.005f;
    aspect = static_cast<float>(width) / static_cast<float>(height);
    if (fabsf(aspect - 1.333) < tolerance) {
        /* into 4:3 aspect ratios */
        width = 320;
        height = 240;
    } else if (fabsf(aspect - 1.777) < tolerance) {
        /* into 16:9 aspect ratios */
        width = 320;
        height = 180;
    } else if (fabsf(aspect - 0.75) < tolerance) {
        /* into 3:4 aspect ratios */
        width = 240;
        height = 320;
    } else if (fabsf(aspect - 0.562) < tolerance) {
        /* into 9:16 aspect ratios */
        width = 180;
        height = 320;
    } else {
        /* default use 4:3 aspect ratios */
        width = 320;
        height = 240;
    }
}

status_t ControlThread::skipFrames(size_t numFrames)
{
    LOG1("@%s: numFrames=%d", __FUNCTION__, numFrames);
    status_t status = NO_ERROR;

    AtomBuffer snapshotBuffer
            = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_SNAPSHOT);
    AtomBuffer postviewBuffer
            = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_POSTVIEW);

    for (size_t i = 0; i < numFrames; i++) {
        if ((status = mISP->getSnapshot(&snapshotBuffer, &postviewBuffer)) != NO_ERROR) {
            ALOGE("Error in grabbing warm-up frame %d!", i);
            return status;
        }
        status = mISP->putSnapshot(&snapshotBuffer, &postviewBuffer);
        if (status == DEAD_OBJECT) {
            LOG1("Stale snapshot buffer returned to ISP");
        } else if (status != NO_ERROR) {
            ALOGE("Error in putting skip frame %d!", i);
            return status;
        }
    }
    PERFORMANCE_TRACES_BREAKDOWN_STEP_PARAM("Skip--", numFrames);
    return status;
}

/* If smart scene detection is enabled and user scene is set to "Auto",
 * change settings based on the detected scene
 */
status_t ControlThread::setSmartSceneParams(void)
{
    const char *scene_mode = mParameters.get(CameraParameters::KEY_SCENE_MODE);

    // Exit if IntelParams are not supported (xnr and anr)
    if (!mIntelParamsAllowed)
        return INVALID_OPERATION;

    if (scene_mode && !strcmp(scene_mode, CameraParameters::SCENE_MODE_AUTO)) {
        bool sceneDetectionSupported = strcmp(PlatformData::supportedSceneDetection(mCameraId), "") != 0;
        //scene mode detection should always be working, but we shouldn't take it in account whenever HDR is on.
        if (!mHdr.enabled && sceneDetectionSupported && m3AControls->getSmartSceneDetection()) {
            int sceneMode = 0;
            bool sceneHdr = false;
            m3AThread->getCurrentSmartScene(sceneMode, sceneHdr);
            // Force XNR and ANR in case of lowlight scene
            if (sceneMode == ia_aiq_scene_mode_lowlight_portrait ||
                sceneMode == ia_aiq_scene_mode_low_light) {
                LOG1("Low-light scene detected, forcing XNR and ANR");
                mISP->setXNR(true);
                // Forcing mParameters to true, to be in sync with app update.
                mParameters.set(IntelCameraParameters::KEY_XNR, "true");

                mISP->setLowLight(true);
                // Forcing mParameters to true, to be in sync with app update.
                mParameters.set(IntelCameraParameters::KEY_ANR, "true");
            }
        }
    }
    return NO_ERROR;
}

status_t ControlThread::handleMessagePanoramaCaptureTrigger()
{
    LOG1("@%s:", __FUNCTION__);
    status_t status = NO_ERROR;
    AtomBuffer snapshotBuffer
            = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_SNAPSHOT);
    AtomBuffer postviewBuffer
            = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_POSTVIEW);

    status = capturePanoramaPic(snapshotBuffer, postviewBuffer);
    if (status != NO_ERROR) {
        ALOGE("Error %d capturing panorama picture.", status);
        return status;
    }

    mPanoramaThread->stitch(&snapshotBuffer, &postviewBuffer); // synchronous

    if (mState != STATE_CONTINUOUS_CAPTURE) {
        // we can return buffers now that panorama has (synchronously) processed
        // (copied) the buffers
        status = mISP->putSnapshot(&snapshotBuffer, &postviewBuffer);
        if (status != NO_ERROR)
            ALOGE("error returning panorama capture buffers");

        //restart preview
        Message msg;
        msg.id = MESSAGE_ID_START_PREVIEW;
        mMessageQueue.send(&msg);
    } else {
        // recycle the buffer as if the picture would be done
        MessagePicture picMsg;
        picMsg.postviewBuf = postviewBuffer;
        picMsg.snapshotBuf = snapshotBuffer;
        handleMessagePictureDone(&picMsg);
    }

    return status;
}

status_t ControlThread::handleMessagePanoramaPicture() {
    LOG1("@%s:", __FUNCTION__);
    status_t status = NO_ERROR;
    if (mPanoramaThread->getState() == PANORAMA_STARTED) {
        LOG1("CaptureSubState %s -> STARTED (panorama)", sCaptureSubstateStrings[mCaptureSubState]);
        mCaptureSubState = STATE_CAPTURE_STARTED;
        mPanoramaThread->startPanoramaCapture();
        handleMessagePanoramaCaptureTrigger();
    } else {
        mPanoramaThread->finalize();
    }

    return status;
}

/**
 * Is a burst capture sequence ongoing?
 *
 * Returns true until the last burst picture has been
 * delivered to application.
 *
 * @see burstMoreCapturesNeeded()
 */
bool ControlThread::isBurstRunning()
{
    if (mBurstCaptureDoneNum != -1 &&
        mBurstLength > 1)
        return true;

    return false;
}

/**
 * Do we need to request more pictures from ISP to
 * complete the capture burst.
 *
 * Returns true until the last burst picture has been
 * requested from application.
 *
 * @see isBurstRunnning()
 */
bool ControlThread::burstMoreCapturesNeeded()
{
    if (isBurstRunning() &&
        mBurstCaptureNum < mBurstLength)
        return true;

    return false;
}

/**
 * Resets the burst state managed in control thread.
 */
void ControlThread::burstStateReset()
{
    mBurstCaptureNum = -1;
    mBurstCaptureDoneNum = -1;
    mBurstQbufs = 0;
    mBurstBufsToReturn = 0;
}


status_t ControlThread::handleMessageTakePicture() {
    LOG1("@%s:", __FUNCTION__);
    status_t status = NO_ERROR;

    mShootingMode = selectShootingMode();
    LOG1("CaptureSubState %s -> STARTED ", sCaptureSubstateStrings[mCaptureSubState]);
    mCaptureSubState = STATE_CAPTURE_STARTED;

    switch(mShootingMode) {

        case SHOOTING_MODE_SINGLE:
            status = captureStillPic();
            break;

        case SHOOTING_MODE_ZSL:
            status = captureStillPic();
            break;

        case SHOOTING_MODE_ZSL_BURST:
            status = captureFixedBurstPic(true);
            break;

        case SHOOTING_MODE_BURST:
            status = captureBurstPic(true);
            break;

        case SHOOTING_MODE_VIDEO_SNAP:
            status = captureVideoSnap();
            break;

        case SHOOTING_MODE_ULL:
            status = captureULLPic();
            break;
        default:
            ALOGE("Taking picture when recording is not supported!");
            status = INVALID_OPERATION;
            break;
    }

    if (status != OK) {
        LOG2("CaptureSubState = IDLE (error)");
        mCaptureSubState = STATE_CAPTURE_IDLE;
    }

    return status;
}

/**
 * Gets a snapshot/postview frame pair from ISP when
 * using flash.
 *
 * To ensure flash sync, the function fetches frames in
 * a loop until a properly exposed frame is available.
 */
status_t ControlThread::getFlashExposedSnapshot(AtomBuffer *snapshotBuffer, AtomBuffer *postviewBuffer)
{
    LOG2("@%s:", __FUNCTION__);
    status_t status = NO_ERROR;
    for (int cnt = 0;;) {

        status = mISP->getSnapshot(snapshotBuffer, postviewBuffer);
        if (status != NO_ERROR) {
            ALOGE("%s: Error in grabbing snapshot!", __FUNCTION__);
            break;
        }

        if (snapshotBuffer->status == FRAME_STATUS_FLASH_EXPOSED) {
            LOG2("flash exposed, frame %d", cnt);
            break;
        }
        else if (snapshotBuffer->status  == FRAME_STATUS_FLASH_FAILED) {
            ALOGE("%s: flash fail, frame %d", __FUNCTION__, cnt);
            break;
        }

        if (cnt++ == FLASH_TIMEOUT_FRAMES) {
            ALOGE("%s: unexpected flash timeout, frame %d", __FUNCTION__, cnt);
            break;
        }

        mISP->putSnapshot(snapshotBuffer, postviewBuffer);;
    }

    return status;
}

/**
 * Fetches meta data from 3A, ISP and sensors and fills
 * the data into struct that can be sent to PictureThread.
 *
 * The caller is responsible for freeing the data.
 */
void ControlThread::fillPicMetaData(PictureThread::MetaData &metaData, bool flashFired)
{
    LOG1("@%s: ", __FUNCTION__);

    ia_binary_data *aaaMkNote = 0;
    atomisp_makernote_info *atomispMkNote = 0;
    SensorAeConfig *aeConfig = new SensorAeConfig;

    if (m3AControls->isIntel3A()) {
        m3AControls->getExposureInfo(*aeConfig);
        if (PlatformData::supportEV(mHwcg.mSensorCI->getCurrentCameraId())) {
            if (m3AControls->getEv(&aeConfig->evBias) != NO_ERROR)
                aeConfig->evBias = EV_UPPER_BOUND;
        }
    } else {
        memset(aeConfig, 0, sizeof(SensorAeConfig));
        if (mHwcg.mSensorCI->getExposureTime(&aeConfig->expTime))
            aeConfig->expTime = 0;
    }

    //       SensorAeConfig information, so setting as NULL on purpose
    mBracketManager->getNextAeConfig(aeConfig);
    if (m3AControls->isIntel3A()) {
        // TODO: add support for raw mknote
        aaaMkNote = m3AControls->get3aMakerNote(ia_mkn_trg_section_1);
        if (!aaaMkNote)
            ALOGW("No 3A makernote data available");
    }

    atomisp_makernote_info tmp;
    status_t status = mISP->getMakerNote(&tmp);
    if (status == NO_ERROR) {
        atomispMkNote = new atomisp_makernote_info;
        *atomispMkNote = tmp;
    }
    else {
        ALOGW("Could not get AtomISP makernote information!");
    }

    metaData.flashFired = flashFired;
    // note: the following may be null, if info not available
    metaData.aeConfig = aeConfig;
    metaData.ia3AMkNote = aaaMkNote;
    metaData.atomispMkNote = atomispMkNote;

    // Request mirroring for snapshot and postview buffers (only for front camera)
    // Do mirroring only in still capture mode, video snapshots are mirrored in dequeueRecording()
    metaData.saveMirrored = mSaveMirrored && (PlatformData::cameraFacing(mCameraId) == CAMERA_FACING_FRONT) &&
                            (mState != STATE_RECORDING);
    metaData.cameraOrientation = PlatformData::cameraOrientation(mCameraId);
    metaData.currentOrientation = mCurrentOrientation;
}

status_t ControlThread::capturePanoramaPic(AtomBuffer &snapshotBuffer, AtomBuffer &postviewBuffer)
{
    LOG1("@%s: ", __FUNCTION__);
    status_t status = NO_ERROR;
    int fourcc, size, width, height;
    int lpvWidth, lpvHeight, lpvSize;
    int thumbnailWidth, thumbnailHeight;

    postviewBuffer.owner = NULL;
    stopFaceDetection(true);

    if (mState != STATE_CONTINUOUS_CAPTURE) {
        status = stopPreviewCore();
        if (status != NO_ERROR) {
            ALOGE("Error stopping preview!");
            return status;
        }
        mState = STATE_CAPTURE;
    }
    mBurstCaptureNum = 0;

    // Get the current params
    mParameters.getPictureSize(&width, &height);
    IntelCameraParameters::getPanoramaLivePreviewSize(lpvWidth, lpvHeight, mParameters);
    fourcc = mISP->getSnapshotPixelFormat();
    size = frameSize(fourcc, width, height);
    lpvSize = frameSize(fourcc, lpvWidth, lpvHeight);

    // Configure PictureThread
    mPictureThread->initialize(mParameters, mISP->zoomRatio(mParameters.getInt(CameraParameters::KEY_ZOOM)));

    // configure thumbnail size
    thumbnailWidth = mParameters.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH);
    thumbnailHeight= mParameters.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT);
    mPanoramaThread->setThumbnailSize(thumbnailWidth, thumbnailHeight);

    setExternalSnapshotBuffers(fourcc,width,height);

    if (mState != STATE_CONTINUOUS_CAPTURE) {
        // Configure and start the ISP
        AtomBuffer formatDescriptorSs
            = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_FORMAT_DESCRIPTOR, fourcc, width, height);
        AtomBuffer formatDescriptorPv
            = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_FORMAT_DESCRIPTOR, selectPostviewFormat(), lpvWidth, lpvHeight);

        mISP->setSnapshotFrameFormat(formatDescriptorSs);
        mISP->setPostviewFrameFormat(formatDescriptorPv);

        if ((status = mISP->configure(MODE_CAPTURE)) != NO_ERROR) {
            ALOGE("Error configuring the ISP driver for CAPTURE mode");
            return status;
        }

        status = mISP->allocateBuffers(MODE_CAPTURE);
        if (status != NO_ERROR) {
            ALOGE("Error allocate buffers in ISP");
            return status;
        }

        if (m3AControls->switchModeAndRate(MODE_CAPTURE, mHwcg.mSensorCI->getFramerate()) != NO_ERROR)
            ALOGE("Failed to switch 3A to capture mode at %.2f fps",mHwcg.mSensorCI->getFramerate());

        if ((status = mISP->start()) != NO_ERROR) {
            ALOGE("Error starting the ISP driver in CAPTURE mode!");
            return status;
        }
    } else {
        /* Necessary to update the buffer pools before we start to capture */
        status = mISP->allocateBuffers(MODE_CAPTURE);
        if (status != NO_ERROR) {
            ALOGE("Error allocate buffers in ISP");
            return status;
        }

        assert(mBurstLength <= 1);
        ContinuousCaptureConfig config;
        config.numCaptures = 1;
        config.offset = 0;
        config.skip = 0,
        mISP->startOfflineCapture(config);
    }

    /*
     *  If the current camera does not have 3A, then we should skip the first
     *  frames in order to allow the sensor to warm up.
     */
    if (PlatformData::sensorType(mCameraId) == SENSOR_TYPE_SOC) {
        if ((status = skipFrames(NUM_WARMUP_FRAMES)) != NO_ERROR) {
            ALOGE("Error skipping warm-up frames!");
            return status;
        }
    }

    // Turn off flash
    mHwcg.mFlashCI->setFlashIndicator(0);

    // Get the snapshot
    if ((status = mISP->getSnapshot(&snapshotBuffer, &postviewBuffer)) != NO_ERROR) {
        ALOGE("Error in grabbing snapshot!");
        return status;
    }

    if (mState == STATE_CONTINUOUS_CAPTURE)
        stopOfflineCapture();

    snapshotBuffer.owner = NULL;

    mCallbacksThread->shutterSound();

    return status;
}

void ControlThread::stopOfflineCapture()
{
    LOG1("@%s: ", __FUNCTION__);
    if (mState == STATE_CONTINUOUS_CAPTURE &&
            mISP->isOfflineCaptureRunning()) {
        mISP->stopOfflineCapture();
    }
}

/**
 * Blocks until capture frame is ready and
 * available for reading from ISP.
 */
status_t ControlThread::waitForCaptureStart()
{
    LOG2("@%s: ", __FUNCTION__);
    status_t status = NO_ERROR;

    // Check if capture frame is availble (no wait)
    int time_out = ATOMISP_CAPTURE_POLL_TIMEOUT;
    // Polling captured image needs more timeslot in file injection mode,
    // driver needs more than 30s to fill the snapshot buffer with 13M image,
    // so set max timeout to 60s
    if (mISP->isFileInjectionEnabled())
        time_out = 60000;
    int res = mISP->pollCapture(time_out);
    if (res == 0) {
        LOG1("%s: timed out!", __FUNCTION__);
        status = UNKNOWN_ERROR;
    } else if (res < 0) {
        LOG1("%s: error while waiting capture!", __FUNCTION__);
        status = UNKNOWN_ERROR;
    }

    return status;
}

/**
 * Skips initial snapshot frames if target FPS is lower
 * than the ISP burst frame rate.
 */
status_t ControlThread::burstCaptureSkipFrames()
{
    LOG2("@%s: ", __FUNCTION__);
    status_t status = NO_ERROR;

    // In continuous mode the output frame count is fixed, so
    // we cannot arbitrarily skip frames. We return NO_ERROR as
    // this function is used to hide differences between
    // capture modes.
    if (mState == STATE_CONTINUOUS_CAPTURE)
        return NO_ERROR;

    if (mBurstLength > 1 &&
            mFpsAdaptSkip > 0 &&
            mBracketManager->getBracketMode() == BRACKET_NONE) {
        LOG1("Skipping %d burst frames", mFpsAdaptSkip);
        if ((status = skipFrames(mFpsAdaptSkip)) != NO_ERROR) {
            ALOGE("Error skipping burst frames!");
        }
    }
    return status;
}

/**
 * Starts the capture process in continuous capture mode.
 */
status_t ControlThread::continuousStartStillCapture(bool useFlash)
{
    LOG2("@%s: ", __FUNCTION__);
    status_t status = NO_ERROR;
    int picWidth, picHeight, fourcc;
    int size;

    if (useFlash == false) {
        mCallbacksThread->shutterSound();

        /**
         * At this stage we need to re-configure the v4l2 buffer pools
         * in case the number of buffers have change.
         * We do  not have an api to do this only. So we use these ones
         * It may look that we are re-allocating buffers, but we are not.
         * we are only changing the number of buffers queued to the driver
         *
         * The number of buffers queued may change up to the amount
         * configured during  start preview. This is how we can do single still
         * captures and burst of N (like for ULL) without re-starting the preview
         * (Assuming we started continuous preview with N buffers in the ring)
         *
         */
        mParameters.getPictureSize(&picWidth, &picHeight);
        fourcc = mISP->getSnapshotPixelFormat();
        size = frameSize(fourcc, picWidth, picHeight);

        setExternalSnapshotBuffers(fourcc, picWidth, picHeight);

        status = mISP->allocateBuffers(MODE_CAPTURE);
        if (status != NO_ERROR) {
           ALOGE("Error allocate buffers in ISP");
            return status;
        }
        startOfflineCapture();
    }
    else {
        // Flushing pictures will also clear counters for
        // requested pictures, which would break the
        // flash-fallback, so we need to avoid the flush (this
        // is ok as we have just run preflash sequence).
        LOG1("Fallback from continuous to normal mode for flash");
        bool flushPicThread = false;
        status = stopPreviewCore(flushPicThread);
        if (status == NO_ERROR)
            mState = STATE_CAPTURE;
        else
            ALOGE("Error stopping preview!");
    }
    return status;
}

/**
 * Select resolution to be used as capture postview size
 *
 * We prefer that postview is configured to preview resolution to be able
 * to pass preview buffers into the preview surface. Since picture-size,
 * preview-size and thumbnail resolutions are all public API parameters,
 * we run checks for aspect-ratio conflict. When ratios do not match
 * we prefer FoV correctness with the resulting image.
 *
 * \return true if selected size matches preview-size
 */
bool ControlThread::selectPostviewSize(int &width, int &height)
{
    LOG1("@%s: ", __FUNCTION__);
    int picWidth, picHeight;
    int thuWidth, thuHeight;
    int preWidth, preHeight;
    float preAspect = 0.0f, picAspect = 0.0f, aspectDiff = 0.0f;

    mParameters.getPictureSize(&picWidth, &picHeight);
    mParameters.getPreviewSize(&preWidth, &preHeight);
    thuWidth = mParameters.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH);
    thuHeight = mParameters.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT);

    // Need to use tolerance checking for picture sizes that do not strictly fall
    // into 4:3 or 16:9 aspect ratios, like 13MP in our case
    const float POSTVIEW_ASPECT_TOLERANCE = 0.005f;
    preAspect = static_cast<float>(preHeight) / static_cast<float>(preWidth);
    picAspect = static_cast<float>(picHeight) / static_cast<float>(picWidth);
    aspectDiff = fabsf(picAspect - preAspect);

    // try preview size first
    if (preWidth > picWidth || preHeight > picHeight) {
        LOG1("Preferred postview size larger than picture size");
    } else if (aspectDiff < POSTVIEW_ASPECT_TOLERANCE) {
        LOG1("Postview aspect difference (%f) within aspect tolerance (%f)",
             aspectDiff, POSTVIEW_ASPECT_TOLERANCE);
        width = preWidth;
        height = preHeight;
        return true;
    } else {
        ALOGW("Postview aspect difference (%f) beyond tolerance (%f)",
             aspectDiff, POSTVIEW_ASPECT_TOLERANCE);
    }

    // then thumbnail
    if (thuWidth > picWidth || thuHeight > picHeight) {
        LOG1("Thumbnail size larger than picture size");
        // use picture-size
        width = picWidth;
        height = picHeight;
        // Note: resulting thumbnail leaves up to sw, currently not supported
    } else if (thuWidth == 0) {
        width = 0;
        height = 0;
        return false;
    } else if (picWidth * thuHeight / thuWidth != picHeight) {
        ALOGW("Thumbnail size doesn't match the picture aspect"
             "(%d,%d) -> (%d,%d), check your configuration",
             picWidth, picHeight, thuWidth, thuHeight);
        int heightByPicAspect = thuWidth * picHeight / picWidth;
        if (heightByPicAspect < thuHeight) {
            // maintain height
            // width = thuHeight * picWidth / picHeight;
            // height = thuHeight;
            // Note: not supported configuration, letting ISP to stretch
            width = thuWidth;
            height = thuHeight;
        } else {
            // maintain width
            width = thuWidth;
            height = heightByPicAspect;
            LOG1("Wider thumbnail compared to picture, cropping %dx%d "
                 "-> %dx%d with sw scaler", width, height, thuWidth, thuHeight);
        }
    } else {
        width = thuWidth;
        height = thuHeight;
    }

    return false;
}

/**
 * Select pixel format for postview pipeline suitable with current mode
 * and configuration.
 *
 * Normally postview pipe is configured with preview size and format
 * to be able to render snapshot postview frame buffers on viewfinder as
 * they are and pass them through the generic operations in preview-pipeline
 * when needed.
 *
 * Proprietary panorama feature is an exception where we expose custom
 * live-preview callback and do not utilize the postview-pipe output to
 * anything else. In case panorama is started, format is fixed to NV21.
 */
int ControlThread::selectPostviewFormat()
{
    int fourcc = 0;

   if (mPanoramaThread->getState() == PANORAMA_STOPPED) {
        fourcc = PlatformData::getPreviewPixelFormat();
    } else {
        fourcc = V4L2_PIX_FMT_NV21;
    }

    return fourcc;
}

status_t ControlThread::captureStillPic()
{
    LOG1("@%s: ", __FUNCTION__);
    status_t status = NO_ERROR;
    AtomBuffer snapshotBuffer
            = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_SNAPSHOT);
    AtomBuffer postviewBuffer
            = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_POSTVIEW);
    int width, height, fourcc, size;
    int pvWidth, pvHeight, pvSize;
    FlashMode flashMode = m3AControls->getAeFlashMode();
    bool flashOn = (flashMode == CAM_AE_FLASH_MODE_TORCH ||
                    flashMode == CAM_AE_FLASH_MODE_ON);
    bool flashFired = false;
    bool flashSequenceStarted = false;
    // Decide whether we display the postview
    bool postviewDisplayable = selectPostviewSize(pvWidth, pvHeight);
    bool displayPostview = postviewDisplayable                   // postview matches size of preview
                           && !mHdr.enabled                      // HDR not enabled
                           && (mPreviewUpdateMode == IntelCameraParameters::PREVIEW_UPDATE_MODE_STANDARD
                              || mBurstLength > 1)               // proprietary preview update mode or burst
                           && mBurstStart >= 0;                  // negative fixed burst start index
    // Synchronise jpeg callback with postview rendering in case of single capture
    bool syncJpegCbWithPostview = !mHdr.enabled && (mBurstLength <= 1) &&
            (mPreviewUpdateMode == IntelCameraParameters::PREVIEW_UPDATE_MODE_STANDARD);
    bool requestPostviewCallback = true;
    bool requestRawCallback = true;

    // TODO: Fix the TestCamera application bug and remove this workaround
    // WORKAROUND BEGIN: Due to a TesCamera application bug send the POSTVIEW and RAW callbacks only for single shots
    if ( mBurstLength > 1) {
        requestPostviewCallback = false;
        requestRawCallback = false;
    }
    // WORKAROUND END
    // Notify CallbacksThread that a picture was requested, so grab one from queue
    mCallbacksThread->requestTakePicture(requestPostviewCallback, requestRawCallback, syncJpegCbWithPostview);
    if (!mHdr.enabled) {
        PERFORMANCE_TRACES_SHOT2SHOT_TAKE_PICTURE_HANDLE();
    }

    stopFaceDetection(true);

    sp<AutoReset> autoReset;

    if (m3AControls->isIntel3A()) {
        // If flash mode is not ON or TORCH, check for other
        // modes: AUTO, DAY_SYNC, SLOW_SYNC

        if (!flashOn && DetermineFlash(flashMode)) {
            if (m3AControls->getAeLock()) {
                LOG1("AE was locked in %s, using old flash decision from AE "
                     "locking time (%s)", __FUNCTION__, mAELockFlashNeed ? "ON" : "OFF");
                flashOn = mAELockFlashNeed;
            }
            else
                flashOn = m3AControls->getAeFlashNecessary();
        }

        if (flashOn) {
            if (m3AControls->getAeMode() != CAM_AE_MODE_MANUAL &&
                    flashMode != CAM_AE_FLASH_MODE_TORCH) {
                // first a workaround for BZ: 133025. Set ae metering mode to auto for the flash duration
                // we can safely use a temporary setting since any client setParameters will be postponed
                // for the duration of the capture
                // this class defines the temporary setting behavior
                class TemporaryAeMetering : public TemporarySetting {
                public:
                    TemporaryAeMetering(ControlThread *controlThread) : TemporarySetting(controlThread) { mMode = mControlThread->m3AControls->getAeMeteringMode(); }
                    void set() { mControlThread->m3AControls->setAeMeteringMode(CAM_AE_METERING_MODE_AUTO); }
                    void reset() { mControlThread->m3AControls->setAeMeteringMode(mMode); }
                    MeteringMode mMode;
                };
                // instantiate - the smart pointers take care of destruction
                autoReset = new AutoReset(new TemporaryAeMetering(this));

                flashSequenceStarted = true;
                // hide preview frames already during pre-flash sequence
                mPreviewThread->setPreviewState(PreviewThread::STATE_ENABLED_HIDDEN);
                mISP->attachObserver(m3AThread.get(), OBSERVE_PREVIEW_STREAM);
                status = m3AThread->enterFlashSequence(AAAThread::FLASH_STAGE_PRE_EXPOSED);
                if (status != NO_ERROR) {
                    flashOn = false;
                }
                // display postview when flash is triggered
                // regardless of preview update mode
                displayPostview = postviewDisplayable;
            }
        }
    }

    if (mState == STATE_CONTINUOUS_CAPTURE) {
        bool useFlash = flashOn && flashMode != CAM_AE_FLASH_MODE_TORCH;
        status = continuousStartStillCapture(useFlash);
    } else {
        status = stopPreviewCore();
        if (status != NO_ERROR) {
            ALOGE("Error stopping preview!");
            return status;
        }
        mState = STATE_CAPTURE;
    }

    if (flashSequenceStarted) {
        m3AThread->exitFlashSequence();
        mISP->detachObserver(m3AThread.get(), OBSERVE_PREVIEW_STREAM);
    }

    mBurstCaptureNum = 0;
    mBurstCaptureDoneNum = 0;
    mBurstQbufs = 0;
    // Get the current params
    mParameters.getPictureSize(&width, &height);
    fourcc = mISP->getSnapshotPixelFormat();
    size = frameSize(fourcc, width, height);
    pvSize = frameSize(fourcc, pvWidth, pvHeight);

    if (mState != STATE_CONTINUOUS_CAPTURE) {
        // Possible smart scene parameter changes (XNR, ANR)
        if ((status = setSmartSceneParams()) != NO_ERROR)
            LOG1("set smart scene parameters failed");

        // Configure and start the ISP
        AtomBuffer formatDescriptorSs
            = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_FORMAT_DESCRIPTOR, fourcc, width, height);
        AtomBuffer formatDescriptorPv
            = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_FORMAT_DESCRIPTOR, selectPostviewFormat(), pvWidth, pvHeight);
        mISP->setSnapshotFrameFormat(formatDescriptorSs);
        mISP->setPostviewFrameFormat(formatDescriptorPv);

        setExternalSnapshotBuffers(fourcc, width, height);

        // Initialize bracketing manager before streaming starts
        if (mBurstLength > 1 && mBracketManager->getBracketMode() != BRACKET_NONE) {
            mBracketManager->initBracketing(mBurstLength, mFpsAdaptSkip);
        }

        if ((status = mISP->configure(MODE_CAPTURE)) != NO_ERROR) {
            ALOGE("Error configuring the ISP driver for CAPTURE mode");
            return status;
        }

        status = mISP->allocateBuffers(MODE_CAPTURE);
        if (status != NO_ERROR) {
            ALOGE("Error allocate buffers in ISP");
            return status;
        }

        if (m3AControls->switchModeAndRate(MODE_CAPTURE, mHwcg.mSensorCI->getFramerate()) != NO_ERROR)
            ALOGE("Failed to switch 3A to capture mode at %.2f fps", mHwcg.mSensorCI->getFramerate());
        if ((status = mISP->start()) != NO_ERROR) {
            ALOGE("Error starting the ISP driver in CAPTURE mode");
            return status;
        }
    }

    // Start the actual bracketing sequence
    if (mBurstLength > 1 && mBracketManager->getBracketMode() != BRACKET_NONE) {
        mBracketManager->startBracketing();
    }

    // HDR init
    if (mHdr.enabled &&
       (status = hdrInit( pvSize, pvWidth, pvHeight)) != NO_ERROR) {
        ALOGE("Error initializing HDR!");
        return status;
    }

    /*
     *  Pre-capture skip.
     *  we can skip frames for 2 reasons:
     *  - if the we are using a SOC sensor in on-line mode, we just changed
     *    modes and we need to skip some frames for the sensor to converge to
     *    decent 3A params
     *  - if we are using a raw sensor to capture (and dump) raw bayer images.
     *    We are also using online mode and we need the skip for sensor
     */
    if ((mState != STATE_CONTINUOUS_CAPTURE && PlatformData::sensorType(mCameraId) == SENSOR_TYPE_SOC) ||
         CameraDump::isDumpImageEnable(CAMERA_DEBUG_DUMP_RAW)) {

        int framesToSkip = CameraDump::isDumpImageEnable(CAMERA_DEBUG_DUMP_RAW) ?
                         RAW_CAPTURE_SKIP:NUM_WARMUP_FRAMES;
        if ((status = skipFrames(framesToSkip)) != NO_ERROR) {
            ALOGE("Error skipping warm-up frames!");
            return status;
        }
    }

    // Turn on flash. If flash mode is torch, then torch is already on
    if (flashOn && flashMode != CAM_AE_FLASH_MODE_TORCH && mBurstLength <= 1) {
        LOG1("Requesting flash");
        if (mHwcg.mFlashCI->setFlash(1) != NO_ERROR) {
            ALOGE("Failed to enable the Flash!");
        }
        else {
            flashFired = true;
        }
    } else if (DetermineFlash(flashMode)) {
        mHwcg.mFlashCI->setFlashIndicator(TORCH_INTENSITY);
    }

    status = burstCaptureSkipFrames();
    if (status != NO_ERROR) {
        ALOGE("Error skipping burst frames!");
        return status;
    }

    if (mState == STATE_CONTINUOUS_CAPTURE) {
        // TODO: to be removed once preview data flow is moved fully to
        //       a separate thread
        if (mBurstLength > 1)
            mBurstQbufs = mISP->getNumSnapshotBuffers();
        status = waitForCaptureStart();
        if (status != NO_ERROR) {
            ALOGE("Error while waiting for capture to start");
            mCallbacksThread->sendError(CAMERA_ERROR_UNKNOWN);
            return status;
        }
    }

    // Get the snapshot
    if (flashFired) {
        status = getFlashExposedSnapshot(&snapshotBuffer, &postviewBuffer);
        // Set flash off only if torch is not used
        if (flashMode != CAM_AE_FLASH_MODE_TORCH)
            mHwcg.mFlashCI->setFlash(0);
    } else {
        if (mBurstLength > 1 && mBracketManager->getBracketMode() != BRACKET_NONE) {
            status = mBracketManager->getSnapshot(snapshotBuffer, postviewBuffer);
            PERFORMANCE_TRACES_BREAKDOWN_STEP_PARAM("BreaketGotFrame",
                        snapshotBuffer.frameCounter);
        } else {
            status = mISP->getSnapshot(&snapshotBuffer, &postviewBuffer);
            PERFORMANCE_TRACES_BREAKDOWN_STEP_PARAM("ISPGotFrame",
                        snapshotBuffer.frameCounter);
        }
    }

    if (status != NO_ERROR) {
        ALOGE("Error in grabbing snapshot!");
        return status;
    }

    // Configure PictureThread
    mPictureThread->initialize(mParameters, mISP->zoomRatio(mParameters.getInt(CameraParameters::KEY_ZOOM)));

    PerformanceTraces::ShutterLag::snapshotTaken(&snapshotBuffer.capture_timestamp);

    PictureThread::MetaData picMetaData;
    fillPicMetaData(picMetaData, flashFired);

    // HDR Processing
    if (mHdr.enabled &&
       (status = hdrProcess(&snapshotBuffer, &postviewBuffer)) != NO_ERROR) {
        ALOGE("HDR: Error in compute CDF for capture %d in HDR sequence!", mBurstCaptureNum);
        picMetaData.free(m3AControls);
        return status;
    }

    mBurstCaptureNum++;

    if (mState != STATE_CONTINUOUS_CAPTURE &&
            (!mHdr.enabled || (mHdr.enabled && mBurstCaptureNum == 1))) {
        // Send request to play the Shutter Sound: in single shots or when burst-length is specified
        mCallbacksThread->shutterSound();
    }

    // Turn off flash
    if (!flashOn && DetermineFlash(flashMode) && mBurstLength <= 1) {
        mHwcg.mFlashCI->setFlashIndicator(0);
    }

    // Do postview for preview-keep-alive feature synchronously before the possible mirroring.
    // Otherwise mirrored image will be shown in postview.
    if (displayPostview || syncJpegCbWithPostview) {
        // We sync with single capture, where we also need preview to stall.
        // So, hide preview after postview when syncJpegCbWithPostview is true
        bool syncPostview = mSaveMirrored && (PlatformData::cameraFacing(mCameraId) == CAMERA_FACING_FRONT);
        mPreviewThread->postview(displayPostview?&postviewBuffer:NULL, syncJpegCbWithPostview, syncPostview);
    }

    // Do jpeg encoding in other cases except HDR. Encoding HDR will be done later.
    bool doEncode = false;
    if (!mHdr.enabled) {
        LOG1("TEST-TRACE: starting picture encode: Time: %lld", systemTime());
        status = mPictureThread->encode(picMetaData, &snapshotBuffer, &postviewBuffer);
        if (status == NO_ERROR) {
            doEncode = true;
        }
    }

    if (doEncode == false) {
        // normally this is done by PictureThread, but as no
        // encoding was done, free the allocated metadata
        picMetaData.free(m3AControls);
    }

    if (mState == STATE_CONTINUOUS_CAPTURE && mBurstLength <= 1)
        stopOfflineCapture();

    return status;
}

status_t ControlThread::captureBurstPic(bool clientRequest = false)
{
    LOG1("@%s: client request %d", __FUNCTION__, clientRequest);
    status_t status = NO_ERROR;

    AtomBuffer snapshotBuffer
            = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_SNAPSHOT);
    AtomBuffer postviewBuffer
            = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_POSTVIEW);

    int pvWidth, pvHeight;
    // Note: Burst (online mode) does not need to handle preview-update-mode
    //       preview is stopped and we always display postview when size matches
    //       and HDR is not enabled.
    bool displayPostview = selectPostviewSize(pvWidth, pvHeight) && !mHdr.enabled;

    if (clientRequest) {
        bool requestPostviewCallback = true;
        bool requestRawCallback = true;

        // Notify CallbacksThread that a picture was requested, so grab one from queue
        mCallbacksThread->requestTakePicture(requestPostviewCallback, requestRawCallback);

        /*
         *  If the CallbacksThread has already JPEG buffers in queue, make sure we use them, before
         *  continuing to dequeue frames from ISP and encode them
         */

        if (mCallbacksThread->getQueuedBuffersNum() > MAX_JPEG_BUFFERS) {
            return NO_ERROR;
        }
        // Check if ISP has free buffers we can use
        if (mBracketManager->getBracketMode() == BRACKET_NONE && !mISP->dataAvailable()) {
            // If ISP has no data, do nothing and return
            return NO_ERROR;
        }

        // If burst length was specified stop capturing when reached the requested burst captures
        if (mBurstLength > 1 && mBurstCaptureNum >= mBurstLength) {
            return NO_ERROR;
        }
    }

    // note: flash is not supported in burst and continuous shooting
    //       modes (this would be the place to enable it)

    status = burstCaptureSkipFrames();
    if (status != NO_ERROR) {
        ALOGE("Error skipping burst frames!");
        return status;
    }

    // Get the snapshot
    if (mBurstLength > 1 && mBracketManager->getBracketMode() != BRACKET_NONE) {
        status = mBracketManager->getSnapshot(snapshotBuffer, postviewBuffer);
        PERFORMANCE_TRACES_BREAKDOWN_STEP_PARAM("BracketGotFrame", snapshotBuffer.frameCounter);
    } else {
        status = mISP->getSnapshot(&snapshotBuffer, &postviewBuffer);
        PERFORMANCE_TRACES_BREAKDOWN_STEP_PARAM("ISPGotFrame", snapshotBuffer.frameCounter);
    }

    if (status != NO_ERROR) {
        ALOGE("Error in grabbing snapshot!");
        return status;
    }

    if (displayPostview)
        mPreviewThread->postview(&postviewBuffer, false);

    PictureThread::MetaData picMetaData;
    fillPicMetaData(picMetaData, false);

   // HDR Processing
    if ( mHdr.enabled &&
        (status = hdrProcess(&snapshotBuffer, &postviewBuffer)) != NO_ERROR) {
        ALOGE("Error processing HDR!");
        picMetaData.free(m3AControls);
        return status;
    }

    mBurstCaptureNum++;

    // Do jpeg encoding
    bool hdrSaveOrig = (mHdr.enabled && mHdr.saveOrig && picMetaData.aeConfig->evBias == 0);
    bool doEncode = false;
    if (!mHdr.enabled || hdrSaveOrig) {
        doEncode = true;
        mCallbacksThread->shutterSound();
        // in case of save-original, let hdrCompose do the recycling
        if (hdrSaveOrig) {
            snapshotBuffer.status = FRAME_STATUS_SKIPPED;
        }
        LOG1("TEST-TRACE: starting picture encode: Time: %lld", systemTime());
        status = mPictureThread->encode(picMetaData, &snapshotBuffer, &postviewBuffer);
    }

    if (mHdr.enabled && mBurstCaptureNum == mHdr.bracketNum) {
        // This was the last capture in HDR sequence, compose the final HDR image
        LOG1("HDR: last capture, composing HDR image...");

        status = hdrCompose();
        if (status != NO_ERROR)
            ALOGE("Error composing HDR picture");
    }

    if (doEncode == false) {
        // normally this is done by PictureThread, but as no
        // encoding was done, free the allocated metadata
        picMetaData.free(m3AControls);
    }

    if (mBurstLength > 1 && mBracketManager->getBracketMode() != BRACKET_NONE && (mBurstCaptureNum == mBurstLength)) {
        LOG1("@%s: Bracketing done, got all %d snapshots", __FUNCTION__, mBurstLength);
        mBracketManager->stopBracketing();
    }

    return status;
}

/**
 * Notifies CallbacksThread that a picture was requested by
 * the application.
 */
void ControlThread::requestTakePicture()
{
    bool requestPostviewCallback = true;
    bool requestRawCallback = true;

    // Notify CallbacksThread that a picture was requested, so grab one from queue
    mCallbacksThread->requestTakePicture(requestPostviewCallback, requestRawCallback);
}

/**
 * Whether the JPEG/compressed frame queue in CallbacksThread is
 * already full?
 */
bool ControlThread::compressedFrameQueueFull()
{
    return mCallbacksThread->getQueuedBuffersNum() > MAX_JPEG_BUFFERS;
}

/**
 * TEMPORARILY DISABLED
 * Queues unused snapshot buffers to ISP.
 *
 * Note: in certain use-cases like single captures,
 * this step can be omitted to save in capture time.
 *
 * TODO: Once postview buffers are allocated same as snapshots then we
 * can allocage less buffers than the burst length required. In this
 * case we can re-sue this method
 */
status_t ControlThread::queueSnapshotBuffers()
{
    LOG1("@%s:", __FUNCTION__);
    status_t status = NO_ERROR;
    /*for (size_t i = 0; i < mAvailableSnapshotBuffers.size(); i++) {
        AtomBuffer snapshotBuf = mAvailableSnapshotBuffers[i].snapshotBuf;

        LOG2("return snapshot buffer %u to ISP", i);
        status = mISP->putSnapshot(&snapshotBuf, &postviewBuf);
        if (status == NO_ERROR) {
            ++mBurstQbufs;
        }
        else if (status == DEAD_OBJECT) {
            LOG1("Stale snapshot buffer returned to ISP");
        } else if (status != NO_ERROR) {
            ALOGE("Error in putting snapshot!");
        }
    }
    mAvailableSnapshotBuffers.clear();*/
    return status;
}

/**
 * Starts capture of the next picture of the ongoing fixed-size burst.
 */
status_t ControlThread::captureFixedBurstPic(bool clientRequest = false)
{
    LOG1("@%s: ", __FUNCTION__);
    status_t status = NO_ERROR;

    AtomBuffer snapshotBuffer
            = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_SNAPSHOT);
    AtomBuffer postviewBuffer
            = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_POSTVIEW);

    int pvW, pvH;
    // Note: Postview is not displayed with any of fixed burst scenarios,
    //       just having it here for conformity and noticing.
    //       Continuous mode with negative mBurstStart index would lead to
    //       disordered displaying of postview and preview frames.
    bool displayPostview = selectPostviewSize(pvW, pvH)
                           && mPreviewUpdateMode == IntelCameraParameters::PREVIEW_UPDATE_MODE_STANDARD
                           && mBurstStart >= 0;

    assert(mState == STATE_CONTINUOUS_CAPTURE);

    if (clientRequest) {
        mCallbacksThread->requestTakePicture(true, true);

        // Check whether more frames are needed
        if (compressedFrameQueueFull())
            return NO_ERROR;
    }

    if (mBurstCaptureNum != -1 &&
        mBurstLength > 1 &&
        mBurstCaptureNum >= mBurstLength) {
        // All frames of the burst have been requested (but not necessarily
        // yet all dequeued).
        return NO_ERROR;
    }

    PERFORMANCE_TRACES_SHOT2SHOT_TAKE_PICTURE_HANDLE();

    PictureThread::MetaData picMetaData;
    fillPicMetaData(picMetaData, false);

    // Get the snapshot
    status = mISP->getSnapshot(&snapshotBuffer, &postviewBuffer);

    if (status != NO_ERROR) {
        ALOGE("Error in grabbing snapshot!");
        picMetaData.free(m3AControls);
        stopOfflineCapture();
        burstStateReset();
        return status;
    }

    mBurstCaptureNum++;

    if (displayPostview)
        mPreviewThread->postview(&postviewBuffer, false);

    // Do jpeg encoding
    LOG1("TEST-TRACE: starting picture encode: Time: %lld", systemTime());
    status = mPictureThread->encode(picMetaData, &snapshotBuffer, &postviewBuffer);

    // If all captures have been requested, ISP capture device
    // can be stopped. Otherwise requeue buffers back to ISP.
    if (mBurstCaptureNum == mBurstLength) {
        stopOfflineCapture();
    }
    else if (mBurstLength > mISP->getNumSnapshotBuffers() &&
             mBurstQbufs < mBurstLength) {
        // To save capture time, only requeue buffers if total
        // burst length exceeds the ISP buffer queue size, and
        // more buffers are needed.
        //queueSnapshotBuffers();
        // This i sno longer possible: TODO: allow less buffers than the
        // burst length to be allocated.
    }

    return status;
}

/**
 * Captures a picture and processes it using ULL algorithm
 * This shooting mode is only used in continuous mode and it doesn't support
 * flash
 * This mode performs a burst of 3 captures, but it doesn't go through the
 * normal ThreadLoop.
 * for that reason we need to overwrite some of the Burst capture variables
 */
status_t ControlThread::captureULLPic()
{
    LOG1("@%s: ", __FUNCTION__);
    status_t status = NO_ERROR;
    AtomBuffer snapshotBuffer
            = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_SNAPSHOT);
    AtomBuffer postviewBuffer
            = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_POSTVIEW);
    int pvWidth, pvHeight;
    int picWidth, picHeight, fourcc;
    int cachedBurstLength, cachedBurstStart;
    // In case ULL gets triggered with standard preview update mode
    // we display the first postview frame, sync and hide the preview as
    // with standard single capture. Application needs to handle the ULL
    // postview out from callbacks if this is the intention.
    bool displayPostview = selectPostviewSize(pvWidth, pvHeight)
                           && mPreviewUpdateMode == IntelCameraParameters::PREVIEW_UPDATE_MODE_STANDARD;
    //cache burst related parameters
    cachedBurstLength = mBurstLength;
    cachedBurstStart = mBurstStart;

    mParameters.getPictureSize(&picWidth, &picHeight);
    fourcc = mISP->getSnapshotPixelFormat();

    status = mULL->init(picWidth,picHeight,0);
    if (status != NO_ERROR) {
      mULL->deinit();
      ALOGE("Failed to initialize the ULL algorithm");
      return NO_INIT;
    }

    PERFORMANCE_TRACES_SHOT2SHOT_TAKE_PICTURE_HANDLE();

    mCallbacksThread->requestTakePicture(true, true, displayPostview);

    stopFaceDetection(true);
    // Initialize the burst control variables for the ULL burst
    mBurstLength = mULL->getULLBurstLength();
    mBurstStart = 0;

    status = continuousStartStillCapture(false);

    // Configure PictureThread, inform of the picture and thumbnail resolutions
    mPictureThread->initialize(mParameters, mISP->zoomRatio(mParameters.getInt(CameraParameters::KEY_ZOOM)));

    // Let application know that we are going to produce an ULL image
    mCallbacksThread->ullTriggered(mULL->getCurrentULLid());

    // Get the snapshots
    for (int i=0; i< mBurstLength; i++) {
       status = mISP->getSnapshot(&snapshotBuffer, &postviewBuffer);
       if (status != NO_ERROR) {
           ALOGE("Error in grabbing snapshot!");
           goto exit;
       }
       if (i == 0) {
           PerformanceTraces::ShutterLag::snapshotTaken(&snapshotBuffer.capture_timestamp);

           PictureThread::MetaData firstPicMetaData;
           PictureThread::MetaData ullPicMetaData;
           fillPicMetaData(firstPicMetaData, false);
           fillPicMetaData(ullPicMetaData, false);
           mULL->addSnapshotMetadata(ullPicMetaData);
           if (displayPostview)
               mPreviewThread->postview(&postviewBuffer, true);
           /*
            *  Mark the snapshot as skipped.
            *  This is done so that the snapshot buffer is not made available after
            *  the JPEG encoding. This buffer will be made available after
            *  the ULL processing completes.
            *  By "making available" we mean the buffer is to be pushed to the
            *  mAvailableSnapshotBuffers vector
            */
           snapshotBuffer.status = FRAME_STATUS_SKIPPED;
           status = mPictureThread->encode(firstPicMetaData,&snapshotBuffer, &postviewBuffer);
           if (status != NO_ERROR) {
               // normally this is done by PictureThread, but as no
               // encoding was done, free the allocated metadata
               firstPicMetaData.free(m3AControls);
               ALOGE("Error encoding first image of the ULL burst");
               goto exit;
           }
       }

       mULL->addInputFrame(&snapshotBuffer,  &postviewBuffer);
    }

    // send the  ULL processing to the postcapture thread. once it completes it
    // will call the method postCaptureProcesssingDone()
    mPostCaptureThread->sendProcessItem((IPostCaptureProcessItem*)mULL);

    stopOfflineCapture();

exit:
    // Restore the Burst related control variables
    mBurstLength = cachedBurstLength;
    mBurstStart = cachedBurstStart;
    return status;
}

status_t ControlThread::captureVideoSnap()
{
    LOG1("@%s: ", __FUNCTION__);
    status_t status = NO_ERROR;

    mCallbacksThread->requestTakePicture(true, true);

    // Configure PictureThread
    mPictureThread->initialize(mParameters, mISP->zoomRatio(mParameters.getInt(CameraParameters::KEY_ZOOM)));

    /* Request a new video snapshot in the next capture cycle
     * In the next call of dequeueRecording we will send the
     * recording frame to encode
     */
    mVideoSnapshotrequested++;

    return status;
}

void ControlThread::encodeVideoSnapshot(AtomBuffer &buff)
{
    LOG1("@%s: ", __FUNCTION__);
    PictureThread::MetaData aDummyMetaData;

    fillPicMetaData(aDummyMetaData, false);
    LOG1("Encoding a video snapshot couple buf id:%d", buff.id);
    LOG2("snapshot size %dx%d bpl %d fourcc %d", buff.width
            ,buff.height, buff.bpl, buff.fourcc);

    mVideoSnapshotBuffers.push(buff);

    mCallbacksThread->shutterSound();

    // TODO: PictureThread create thumbnail from single input.
    // PictureThread doesn't ensure that passing single buffer works
    mPictureThread->encode(aDummyMetaData, &buff, &buff);
}

status_t ControlThread::updateSpotWindow(const int &width, const int &height)
{
    LOG1("@%s", __FUNCTION__);
    // TODO: Check, if these window fractions are right. Copied off from libcamera1
    CameraWindow spotWin = {
            static_cast<int>(width * 7.0 / 16.0),
            static_cast<int>(width * 9.0 / 16.0),
            static_cast<int>(height * 7.0 / 16.0),
            static_cast<int>(height * 9.0 / 16.0), 255 };

    return m3AControls->setAeWindow(&spotWin);
}

MeteringMode ControlThread::aeMeteringModeFromString(const String8& modeStr)
{
    LOG1("@%s", __FUNCTION__);
    MeteringMode mode(CAM_AE_METERING_MODE_AUTO);

    if (modeStr == "auto") {
        mode = CAM_AE_METERING_MODE_AUTO;
    } else if (modeStr == "center") {
        mode = CAM_AE_METERING_MODE_CENTER;
    } else if(modeStr == "spot") {
        mode = CAM_AE_METERING_MODE_SPOT;
    }

    return mode;
}

status_t ControlThread::handleMessageTakeSmartShutterPicture()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    // In case of smart shutter with HDR, we need to trigger save orig as a normal capture.
    if (mHdr.enabled && mHdr.saveOrig && mPostProcThread->isSmartCaptureTriggered()) {
        mPostProcThread->resetSmartCaptureTrigger();
        status = handleMessageTakePicture();
    } else {   //normal smart shutter capture
        LOG1("CaptureSubState %s -> STARTED (smart shutter)", sCaptureSubstateStrings[mCaptureSubState]);
        mCaptureSubState = STATE_CAPTURE_STARTED;
        mPostProcThread->captureOnTrigger();
        mState = selectPreviewMode(mParameters);
    }

    return status;
}

/**
 * Cancel ongoig encoding
 *
 * Flushed PictureThread and handles pictureDone's received
 */
status_t ControlThread::cancelPictureThread()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = mPictureThread->flushBuffers();
    Vector<Message> canceledPictures;
    Vector<Message>::iterator it;
    mMessageQueue.remove(MESSAGE_ID_PICTURE_DONE, &canceledPictures);
    for (it = canceledPictures.begin(); it != canceledPictures.end(); it++) {
        status = handleMessagePictureDone(&it->data.pictureDone);
        if (status != NO_ERROR)
            ALOGD("Failed handling pictureDone-messages while canceling!");
    }

    return status;
}

/**
 * Cancel ongoing capture post process
 *
 * Cancels ULL and handles postCaptureProcessingDone
 * TODO: generalization, ULL atm the one and only post capture processing item
 */
status_t ControlThread::cancelPostCaptureThread()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    if (mPostCaptureThread->isBusy()) {
        status = mPostCaptureThread->cancelProcessingItem((IPostCaptureProcessItem *)mULL);
    }

    Vector<Message> canceledPictures;
    Vector<Message>::iterator it;
    mMessageQueue.remove(MESSAGE_ID_POST_CAPTURE_PROCESSING_DONE, &canceledPictures);
    for (it = canceledPictures.begin(); it != canceledPictures.end(); it++) {
        status = handleMessagePostCaptureProcessingDone(&it->data.postCapture);
        if (status != NO_ERROR)
            ALOGD("Failed handling postCaptureProcessingDone while canceling!");
    }

    return status;
}

/**
 * Cancel ongoing capture and any ongoing post capture processing
 */
status_t ControlThread::cancelCapture()
{
    LOG1("@%s: CaptureSubState %s", __FUNCTION__, sCaptureSubstateStrings[mCaptureSubState]);
    status_t status = NO_ERROR;

    if (mCaptureSubState == STATE_CAPTURE_IDLE) {
        LOG1("No ongoing capture to cancel");
        status = cancelPostCaptureThread();
        return status;
    }

    if (mState == STATE_CAPTURE) {
        // online capture
        status = stopCapture();
    } else if (mState == STATE_CONTINUOUS_CAPTURE) {
        // offline capture
        stopOfflineCapture();
        status |= cancelPostCaptureThread();
        status |= cancelPictureThread();
    }
    mStillCaptureInProgress = false;
    LOG1("CaptureSubState %s -> IDLE (cancelCapture)", sCaptureSubstateStrings[mCaptureSubState]);
    mCaptureSubState = STATE_CAPTURE_IDLE;
    return status;
}

status_t ControlThread::handleMessageCancelPicture()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    mBurstLength = 0;
    status = cancelPictureThread();

    mStillCaptureInProgress = false;

    mMessageQueue.reply(MESSAGE_ID_CANCEL_PICTURE, status);
    return status;
}

status_t ControlThread::handleMessageRelease()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    // use exit handler to stop (but do not stop message handling)
    Message msg;
    msg.data.exit.stopThread = false;
    status = handleMessageExit(&msg.data.exit);
    // return Gfx buffers
    mPreviewThread->returnPreviewBuffers();
    mMessageQueue.reply(MESSAGE_ID_RELEASE, status);
    return status;
}

status_t ControlThread::handleMessageAutoFocus()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    PERFORMANCE_TRACES_BREAKDOWN_STEP("In");
    status = m3AThread->autoFocus();

    return status;
}

status_t ControlThread::handleMessageCancelAutoFocus()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    status = m3AThread->cancelAutoFocus();
    LOG2("auto focus is off");
    /*
     * The normal autoFocus sequence is:
     * - camera client is calling autoFocus (we run the AF sequence and lock AF)
     * - camera client is calling:
     *     - takePicture: AF is locked, so the picture will have the focus established
     *       in previous step. In this case, we have to reset the auto-focus to enabled
     *       when the camera client will call startPreview.
     *     - cancelAutoFocus: AF is locked, camera client no longer wants this focus position
     *       so we should switch back to auto-focus in 3A library
     */
    if (m3AControls->isIntel3A()) {
        m3AControls->setAfEnabled(true);
    }
    return status;
}

status_t ControlThread::handleMessageReleaseRecordingFrame(MessageReleaseRecordingFrame *msg)
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    if (mState == STATE_RECORDING) {
        AtomBuffer *recBuff = findRecordingBuffer(msg->buff);
        if (recBuff == NULL) {
            // This may happen with buffer sharing. When the omx component is stopped
            // it disables buffer sharing and deallocates its buffers. Internally we check
            // to see if sharing was disabled then we restart the ISP with new buffers. In
            // the mean time, the app is returning us shared buffers when we are no longer
            // using them.
            ALOGE("Could not find recording buffer: %p", msg->buff);
            return DEAD_OBJECT;
        }
        int curBuff = recBuff->id;
        LOG2("Recording buffer released from encoder, buff id = %d", curBuff);
        if (curBuff < mNumBuffers) {
            // check if also reserved by snapshot
            if (!mVideoSnapshotBuffers.empty()) {
                AtomBuffer *videoBuffer = findVideoSnapshotBuffer(curBuff);
                if (videoBuffer) {
                    LOG1("Recording buffer found reserved for video snapshot");
                    // drop from reserved list
                    mRecordingBuffers.erase(recBuff);
                    return NO_ERROR;
                }
            }

            // return to AtomISP
            status = mISP->putRecordingFrame(recBuff);
            if (status == DEAD_OBJECT) {
                ALOGW("Stale recording buffer returned to ISP");
            } else if (status != NO_ERROR) {
                ALOGE("Error putting recording frame to ISP");
            } else {
                // drop from reserved list
                mRecordingBuffers.erase(recBuff);
            }
        } else {
            ALOGE("Recording buffer out of array");
        }
    }
    return status;
}

void ControlThread::previewBufferCallback(AtomBuffer *buff, ICallbackPreview::CallbackType t)
{
    LOG2("@%s", __FUNCTION__);
    if (t != ICallbackPreview::INPUT_ONCE) {
        ALOGE("Received unexpected preview callback");
        return;
    }
    Message msg;
    msg.id = MESSAGE_ID_PREVIEW_STARTED;
    mMessageQueue.send(&msg);
}

status_t ControlThread::handleMessagePreviewStarted()
{
    LOG1("@%s", __FUNCTION__);

    /**
    * First preview frame was rendered.
    * Now preview is ongoing. Complete now any initialization that is not
    * strictly needed to do, before preview is started so it doesn't
    * impact launch to preview time.
    *
    */

    /* NOTE: handleMessageTakePicture can be called before this function, if application
     *       call take_picture fast after preview start. So we must take care of that case.
     */

    /* Now that preview is started let's send the asynchronous msg to PictureThread
     * to start the allocation of snapshot buffers.
     */
    bool videoMode = isParameterSet(CameraParameters::KEY_RECORDING_HINT) ? true : false;

    /**
     * if we have all the allocated buffers available then it is safe to re-allocate
     *
     */

    if (mAllocatedSnapshotBuffers.size() == mAvailableSnapshotBuffers.size()) {
        allocateSnapshotAndPostviewBuffers(videoMode);
    } else {
        ALOGW("%s: not safe to allocate now, some snapshot buffers are not returned, skipping", __FUNCTION__);
    };

    return NO_ERROR;
}

AtomBuffer* ControlThread::findVideoSnapshotBuffer(int index)
{
    Vector<AtomBuffer>::iterator it = mVideoSnapshotBuffers.begin();
    for (;it != mVideoSnapshotBuffers.end(); ++it)
        if (it->id == index) {
            return it;
        }
    return NULL;
}

status_t ControlThread::handleMessageEncodingDone(MessagePicture *msg)
{
    LOG1("@%s", __FUNCTION__);

    LOG1("CaptureSubState %s -> ENCODING DONE", sCaptureSubstateStrings[mCaptureSubState]);
    mCaptureSubState = STATE_CAPTURE_ENCODING_DONE;

    return OK;
}

status_t ControlThread::handleMessagePictureDone(MessagePicture *msg)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    if (msg->snapshotBuf.type == ATOM_BUFFER_PANORAMA) {
        // panorama pictures are special, they use the panorama engine memory.
        // we return them to panorama for releasing
        msg->snapshotBuf.owner->returnBuffer(&msg->snapshotBuf);
        msg->snapshotBuf.owner->returnBuffer(&msg->postviewBuf);
    } else if (mState == STATE_RECORDING) {
        int curBuff = msg->snapshotBuf.id;
        if (!mVideoSnapshotBuffers.empty()) {
            AtomBuffer *videoBuffer = findVideoSnapshotBuffer(curBuff);

            if (videoBuffer) {
                // check if also reserved by encoder
                if (!mRecordingBuffers.empty()) {
                    AtomBuffer *recBuffer = NULL;
                    if(mStoreMetaDataInBuffers)
                        recBuffer = findRecordingBuffer((void*) videoBuffer->metadata_buff->data);
                    else
                        recBuffer = findRecordingBuffer((void*) videoBuffer->dataPtr);
                    if (recBuffer) {
                        LOG1("Snapshot buffer found reserved for video encoding");
                        // drop from reserved list
                        mVideoSnapshotBuffers.erase(videoBuffer);
                        return NO_ERROR;
                    }
                }

                status = mISP->putRecordingFrame(videoBuffer);
                if (status == DEAD_OBJECT) {
                    LOG1("Stale preview buffer returned to ISP");
                } else if (status != NO_ERROR) {
                    ALOGE("Error putting preview frame to ISP");
                } else {
                    // drop from reserved list
                    mVideoSnapshotBuffers.erase(videoBuffer);
                }

                if (mVideoSnapshotBuffers.isEmpty()) {
                    LOG1("CaptureSubState %s -> IDLE (videoSnapshot)", sCaptureSubstateStrings[mCaptureSubState]);
                    mCaptureSubState = STATE_CAPTURE_IDLE;
                }
            }
            return status;
        }
    } else if (mState == STATE_CAPTURE || mState == STATE_CONTINUOUS_CAPTURE) {
        /**
         * Snapshot buffer recycle
         * Buffers marked with FRAME_STATUS SKIPPED are not meant to be made
         * available, this is used for example in case of HDR composed image
         * and the first snapshot in ULL sequence
         *
         * We check if the buffer returned is in the array of allocated buffers,
         * this should always be the case.
         * Then we check that it is not already in the list of available buffers
         */
        if (msg->snapshotBuf.status != FRAME_STATUS_SKIPPED) {
            if (mCaptureSubState == STATE_CAPTURE_IDLE) {
                LOG1("Recycling buffer after canceled post-capture-processing");
            } else {
                LOG1("CaptureSubState %s -> PICTURE DONE", sCaptureSubstateStrings[mCaptureSubState]);
                mCaptureSubState = STATE_CAPTURE_PICTURE_DONE;
            }

            msg->snapshotBuf.status = FRAME_STATUS_OK;
            if (findBufferByData(&msg->snapshotBuf, &mAllocatedSnapshotBuffers) == NULL) {
                ALOGE("Stale snapshot buffer %p returned... this should not happen", msg->snapshotBuf.dataPtr);

            } else if (findBufferByData(&msg->snapshotBuf, &mAvailableSnapshotBuffers) == NULL) {
                // It's safe to recycle this buffer if needed
                if (mBurstLength > 1 && mBurstBufsToReturn > 0) {
                    if (mBracketManager->getBracketMode() != BRACKET_NONE) {
                        status = mBracketManager->putSnapshot(msg->snapshotBuf, msg->postviewBuf);
                    } else {
                        status = mISP->putSnapshot(&msg->snapshotBuf, &msg->postviewBuf);
                    }

                    if (status != NO_ERROR) {
                        ALOGE("Error %d in putting snapshot buffer:%p postviewBuf:%p!", status,
                                msg->snapshotBuf.dataPtr, msg->postviewBuf.dataPtr);
                    } else {
                        LOG1("Recycle snapshot buffer:%p postviewBuf:%p", msg->snapshotBuf.dataPtr, msg->postviewBuf.dataPtr);
                    }
                    mBurstBufsToReturn--;
                } else {
                    mAvailableSnapshotBuffers.push(msg->snapshotBuf);
                    LOG1("%s  pushed %p to mAvailableSnapshotBuffers, size %d",
                            __FUNCTION__, msg->snapshotBuf.dataPtr, mAvailableSnapshotBuffers.size());
                }
            } else {
                ALOGE("%s Already available snapshot buffer arrived. Find the bug!!", __FUNCTION__);
            }
        }

        if (msg->postviewBuf.status != FRAME_STATUS_SKIPPED) {
            // Postview buffer availability:
            if (msg->postviewBuf.dataPtr == NULL) {
                // Recycled postview buffer was null. This is OK in some cases,
                // like for ULL post-processed image: a NULL postview image is sent to encoding.
                LOG1("@%s NULL postview buffer cycled", __FUNCTION__);
            } else if (findBufferByData(&msg->postviewBuf, &mAllocatedPostviewBuffers) == NULL) {
                ALOGE("Stale postview buffer, dataPtr = %p returned... this should not happen",
                    msg->postviewBuf.dataPtr);
            } else if (findBufferByData(&msg->postviewBuf, &mAvailablePostviewBuffers) == NULL) {
                mAvailablePostviewBuffers.push(msg->postviewBuf);
                LOG1("%s: pushed postview buffer ptr = %p to mAvailablePostviewBuffers, size %d",
                    __FUNCTION__, msg->postviewBuf.dataPtr, mAvailablePostviewBuffers.size());
            } else {
                ALOGE("%s Already available postview buffer arrived. Find the bug!!", __FUNCTION__);
            }
        }

        if (isBurstRunning()) {
            ++mBurstCaptureDoneNum;
            LOG2("Burst req %d done %d len %d",
                 mBurstCaptureNum, mBurstCaptureDoneNum, mBurstLength);
            if (mBurstCaptureDoneNum >= mBurstLength &&
                (!mHdr.enabled || msg->snapshotBuf.dataPtr == mHdr.outMainBuf.dataPtr)) {
                ALOGW("Last pic in burst received, terminating");
                burstStateReset();
            }
        }

        // transit to idle once all buffers are returned
        if (!isBurstRunning() && mAllocatedSnapshotBuffers.size() == mAvailableSnapshotBuffers.size()) {
            LOG1("CaptureSubState %s -> IDLE",sCaptureSubstateStrings[mCaptureSubState]);
            mCaptureSubState = STATE_CAPTURE_IDLE;
        }

    } else {
        ALOGW("Received a picture Done during invalid state %d; buf id:%d, ptr=%p", mState, msg->snapshotBuf.id, msg->snapshotBuf.buff);
    }

    // It is possible that handleMessageSetParameters here will callback to
    // handleMessagePictureDone again in some cases with processing postponed
    // messages. We need to avoid the dead loop
    // It's also not safe to set parameter when mCaptureSubState is not IDLE.
    if (mPostponedMsgProcessing || mCaptureSubState != STATE_CAPTURE_IDLE) {
        LOG1("skip to handle postponed messages mPostponedMsgProcessing:%d mCaptureSubState:%d", mPostponedMsgProcessing, mCaptureSubState);
        return status;
    }
    // handle postponed setparameters which may have occured during capture
    // TODO: ensure that this goes correctly with e.g ULL recycling more than
    //       one buffers before capture process is done
    List<Message>::iterator it = mPostponedMessages.begin();
    while (it != mPostponedMessages.end()) {
        if (it->id == MESSAGE_ID_SET_PARAMETERS) {
            LOG1("@%s handling postponed setparameter message", __FUNCTION__);
            mPostponedMsgProcessing = true;
            handleMessageSetParameters(&it->data.setParameters);
            free(it->data.setParameters.params); // was strdupped, needs free
            it->data.setParameters.params = NULL;
            it = mPostponedMessages.erase(it); // returns pointer to next item in list
            mPostponedMsgProcessing = false;
        } else {
            it++;
        }
    }

    return status;
}

/**
 * Utility method to find buffers in vectors of AtomBuffers
 * the comparison is done based on the value of the data pointer
 * inside camera_memory_t
 */
AtomBuffer* ControlThread::findBufferByData(AtomBuffer *buf,Vector<AtomBuffer> *aVector)
{
    Vector<AtomBuffer>::iterator it = aVector->begin();
    for (;it != aVector->end(); ++it) {
        if (buf->dataPtr == it->dataPtr)
            return it;
    }

    return NULL;
}

bool ControlThread::validateSize(int width, int height, Vector<Size> &supportedSizes) const
{
    if (width < 0 || height < 0)
        return false;

    for (Vector<Size>::iterator it = supportedSizes.begin(); it != supportedSizes.end(); ++it)
        if (width == it->width && height == it->height)
            return true;

    ALOGW("WARNING: The Size %dx%d is not fully supported. Some issues might occur!", width, height);
    return true;
}

bool ControlThread::validateString(const char* value,  const char* supportList) const{
    // value should not set if support list is empty
    if (value !=NULL && supportList == NULL) {
        return false;
    }

    if (value == NULL || supportList == NULL) {
        return true;
    }

    size_t len = strlen(value);
    const char* startPtr(supportList);
    const char* endPtr(supportList);
    int bracketLevel(0);

    // divide support list to values and compare those to given values.
    // values are separated with comma in support list, but commas also exist
    // part of values inside bracket.
    while (true) {
        if ( *endPtr == '(') {
            ++bracketLevel;
        } else if (*endPtr == ')') {
            --bracketLevel;
        } else if ( bracketLevel == 0 && ( *endPtr == '\0' || *endPtr == ',')) {
            if (((startPtr + len) == endPtr) &&
                (strncmp(value, startPtr, len) == 0)) {
                return true;
            }

            // bracket can use circle values in supported list
            if (((startPtr + len + 2 ) == endPtr) &&
                ( *startPtr == '(') &&
                (strncmp(value, startPtr + 1, len) == 0)) {
                return true;
            }
            startPtr = endPtr + 1;
        }

        if (*endPtr == '\0') {
            return false;
        }
        ++endPtr;
    }

    return false;
}

status_t ControlThread::validateParameters(const CameraParameters *params)
{
    LOG1("@%s: params = %p", __FUNCTION__, params);
    // PREVIEW
    int width, height;
    Vector<Size> supportedSizes;
    params->getSupportedPreviewSizes(supportedSizes);
    params->getPreviewSize(&width, &height);
    if (!validateSize(width, height, supportedSizes)) {
        ALOGE("bad preview size");
        return BAD_VALUE;
    }

    // PREVIEW_FPS_RANGE
    int minFPS, maxFPS;
    params->getPreviewFpsRange(&minFPS, &maxFPS);
    // getPreviewFrameRate() returns -1 fps value if the range-pair string is malformatted
    const char* fpsRange = params->get(CameraParameters::KEY_PREVIEW_FPS_RANGE);
    const char* fpsRanges = params->get(CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE);
    if ((fpsRange && fpsRanges && strstr(fpsRanges, fpsRange) == NULL) ||
        minFPS < 0 || maxFPS < 0) {
            ALOGE("invalid fps range: %s; supported %s", fpsRange, fpsRanges);
            return BAD_VALUE;
    }

    // VIDEO
    params->getVideoSize(&width, &height);
    supportedSizes.clear();
    params->getSupportedVideoSizes(supportedSizes);
    if (!validateSize(width, height, supportedSizes)) {
        ALOGE("bad video size %dx%d", width, height);
        return BAD_VALUE;
    }

    // RECORDING FRAME RATE
    const char* recordingFps = params->get(IntelCameraParameters::KEY_RECORDING_FRAME_RATE);
    const char* supportedRecordingFps = params->get(IntelCameraParameters::KEY_SUPPORTED_RECORDING_FRAME_RATES);
    if (!validateString(recordingFps, supportedRecordingFps)) {
        ALOGE("bad recording frame rate: %s, supported: %s", recordingFps, supportedRecordingFps);
        return BAD_VALUE;
    }

    // SNAPSHOT
    params->getPictureSize(&width, &height);
    supportedSizes.clear();
    params->getSupportedPictureSizes(supportedSizes);
    if (!validateSize(width, height, supportedSizes)) {
        ALOGE("bad picture size");
        return BAD_VALUE;
    }

    // JPEG QUALITY
    int jpegQuality = params->getInt(CameraParameters::KEY_JPEG_QUALITY);
    if (jpegQuality < 1 || jpegQuality > 100) {
        ALOGE("bad jpeg quality: %d", jpegQuality);
        return BAD_VALUE;
    }

    // THUMBNAIL QUALITY
    int thumbQuality = params->getInt(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY);
    if (thumbQuality < 1 || thumbQuality > 100) {
        ALOGE("bad thumbnail quality: %d", thumbQuality);
        return BAD_VALUE;
    }

    // THUMBNAIL SIZE
    int thumbWidth = params->getInt(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH);
    int thumbHeight = params->getInt(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT);
    char* thumbnailSizes = (char*) params->get(CameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES);
    supportedSizes.clear();
    if (thumbnailSizes != NULL) {
        while (true) {
            int width = (int)strtol(thumbnailSizes, &thumbnailSizes, 10);
            int height = (int)strtol(thumbnailSizes+1, &thumbnailSizes, 10);
            supportedSizes.push(Size(width, height));
            if (*thumbnailSizes == '\0')
                break;
            ++thumbnailSizes;
        }
        if (!validateSize(thumbWidth, thumbHeight, supportedSizes)) {
            ALOGE("bad thumbnail size: (%d,%d)", thumbWidth, thumbHeight);
            return BAD_VALUE;
        }
    } else {
        ALOGE("bad thumbnail size");
        return BAD_VALUE;
    }
    // PICTURE FORMAT
    const char* picFormat = params->get(CameraParameters::KEY_PICTURE_FORMAT);
    const char* picFormats = params->get(CameraParameters::KEY_SUPPORTED_PICTURE_FORMATS);
    if (!validateString(picFormat, picFormats)) {
        ALOGE("bad picture fourcc: %s", picFormat);
        return BAD_VALUE;
    }

    // PREVIEW FORMAT
    const char* preFormat = params->get(CameraParameters::KEY_PREVIEW_FORMAT);
    const char* preFormats = params->get(CameraParameters::KEY_SUPPORTED_PREVIEW_FORMATS);
    if (!validateString(preFormat, preFormats))  {
        ALOGE("bad preview fourcc: %s", preFormat);
        return BAD_VALUE;
    }

    // ROTATION, can only be 0 ,90, 180 or 270.
    int rotation = params->getInt(CameraParameters::KEY_ROTATION);
    if (rotation != 0 && rotation != 90 && rotation != 180 && rotation != 270) {
        ALOGE("bad rotation value: %d", rotation);
        return BAD_VALUE;
    }


    // WHITE BALANCE
    const char* whiteBalance = params->get(CameraParameters::KEY_WHITE_BALANCE);
    const char* whiteBalances = params->get(CameraParameters::KEY_SUPPORTED_WHITE_BALANCE);
    if (!validateString(whiteBalance, whiteBalances)) {
        ALOGE("bad white balance mode: %s", whiteBalance);
        return BAD_VALUE;
    }

    // ZOOM
    int zoom = params->getInt(CameraParameters::KEY_ZOOM);
    int maxZoom = params->getInt(CameraParameters::KEY_MAX_ZOOM);
    if (zoom > maxZoom || zoom < 0) {
        ALOGE("bad zoom index: %d", zoom);
        return BAD_VALUE;
    }

    // FLASH
    const char* flashMode = params->get(CameraParameters::KEY_FLASH_MODE);
    const char* flashModes = params->get(CameraParameters::KEY_SUPPORTED_FLASH_MODES);
    if (!validateString(flashMode, flashModes)) {
        ALOGE("bad flash mode");
        return BAD_VALUE;
    }

    // SCENE MODE
    const char* sceneMode = params->get(CameraParameters::KEY_SCENE_MODE);
    const char* sceneModes = params->get(CameraParameters::KEY_SUPPORTED_SCENE_MODES);
    if (!validateString(sceneMode, sceneModes)) {
        ALOGE("bad scene mode: %s; supported: %s", sceneMode, sceneModes);
        return BAD_VALUE;
    }

    // FOCUS
    const char* focusMode = params->get(CameraParameters::KEY_FOCUS_MODE);
    const char* focusModes = params->get(CameraParameters::KEY_SUPPORTED_FOCUS_MODES);
    if (!validateString(focusMode, focusModes)) {
        ALOGE("bad focus mode: %s; supported: %s", focusMode, focusModes);
        return BAD_VALUE;
    }

    // BURST LENGTH
    const char* burstLength = params->get(IntelCameraParameters::KEY_BURST_LENGTH);
    const char* burstLengths = params->get(IntelCameraParameters::KEY_SUPPORTED_BURST_LENGTH);
    if (!validateString(burstLength, burstLengths)) {
        ALOGE("bad burst length: %s; supported: %s", burstLength, burstLengths);
        return BAD_VALUE;
    }
    const char* burstStart = params->get(IntelCameraParameters::KEY_BURST_START_INDEX);
    if (burstStart) {
        int burstStartInt = atoi(burstStart);
        if (burstStartInt < 0) {
            const char* captureBracket = params->get(IntelCameraParameters::KEY_CAPTURE_BRACKET);
            if (captureBracket && String8(captureBracket) != "none") {
                ALOGE("negative start-index and bracketing not supported concurrently");
                return BAD_VALUE;
            }
            int len = burstLength ? atoi(burstLength) : 0;
            if (len > PlatformData::maxContinuousRawRingBufferSize(mCameraId)-1) {
                ALOGE("negative start-index and burst-length=%d not supported concurrently", len);
                return BAD_VALUE;
            }
        }
    }

    // BURST SPEED
    const char* burstSpeed = params->get(IntelCameraParameters::KEY_BURST_SPEED);
    const char* burstSpeeds = params->get(IntelCameraParameters::KEY_SUPPORTED_BURST_SPEED);
    if (!validateString(burstSpeed, burstSpeeds)) {
        ALOGE("bad burst speed: %s; supported: %s", burstSpeed, burstSpeeds);
        return BAD_VALUE;
    }

    // OVERLAY
    const char* overlaySupported = params->get(IntelCameraParameters::KEY_HW_OVERLAY_RENDERING_SUPPORTED);
    const char* overlay = params->get(IntelCameraParameters::KEY_HW_OVERLAY_RENDERING);
        if (!validateString(overlay, overlaySupported)) {
        ALOGE("bad overlay rendering mode: %s; supported: %s", overlay, overlaySupported);
        return BAD_VALUE;
    }

    // MISCELLANEOUS
    const char *size = params->get(IntelCameraParameters::KEY_PANORAMA_LIVE_PREVIEW_SIZE);
    const char *livePreviewSizes = IntelCameraParameters::getSupportedPanoramaLivePreviewSizes(*params);
    if (!validateString(size, livePreviewSizes)) {
        ALOGE("bad panorama live preview size");
        return BAD_VALUE;
    }

    // ANTI FLICKER
    const char* flickerMode = params->get(CameraParameters::KEY_ANTIBANDING);
    const char* flickerModes = params->get(CameraParameters::KEY_SUPPORTED_ANTIBANDING);
    if (!validateString(flickerMode, flickerModes)) {
        ALOGE("bad anti flicker mode");
        return BAD_VALUE;
    }

    // COLOR EFFECT
    const char* colorEffect = params->get(CameraParameters::KEY_EFFECT);
    const char* colorEffects = params->get(CameraParameters::KEY_SUPPORTED_EFFECTS);
    if (!validateString(colorEffect, colorEffects)) {
        ALOGE("bad color effect: %s", colorEffect);
        return BAD_VALUE;
    }

    // EXPOSURE COMPENSATION
    int exposure = params->getInt(CameraParameters::KEY_EXPOSURE_COMPENSATION);
    int minExposure = params->getInt(CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION);
    int maxExposure = params->getInt(CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION);
    if (exposure > maxExposure || exposure < minExposure) {
        ALOGE("bad exposure compensation value: %d", exposure);
        return BAD_VALUE;
    }

    //Note: here for Intel expand parameters, add additional validity check
    //for their supported list. when they're null, we return bad value for
    //these intel parameters setting. As "noise reduction and edge enhancement"
    //and "multi access color correction" are not supported yet.

    // NOISE_REDUCTION_AND_EDGE_ENHANCEMENT
    const char* noiseReductionAndEdgeEnhancement = params->get(IntelCameraParameters::KEY_NOISE_REDUCTION_AND_EDGE_ENHANCEMENT);
    const char* noiseReductionAndEdgeEnhancements = params->get(IntelCameraParameters::KEY_SUPPORTED_NOISE_REDUCTION_AND_EDGE_ENHANCEMENT);
    if (!validateString(noiseReductionAndEdgeEnhancement, noiseReductionAndEdgeEnhancements)) {
        ALOGE("bad noise reduction and edge enhancement value : %s", noiseReductionAndEdgeEnhancement);
        return BAD_VALUE;
    }

    // MULTI_ACCESS_COLOR_CORRECTION
    const char* multiAccessColorCorrection = params->get(IntelCameraParameters::KEY_MULTI_ACCESS_COLOR_CORRECTION);
    const char* multiAccessColorCorrections = params->get(IntelCameraParameters::KEY_SUPPORTED_MULTI_ACCESS_COLOR_CORRECTIONS);
    if (!validateString(multiAccessColorCorrection, multiAccessColorCorrections)) {
        ALOGE("bad multi access color correction value : %s", multiAccessColorCorrection);
        return BAD_VALUE;
    }

    //DVS
    const char* dvsEnable = params->get(CameraParameters::KEY_VIDEO_STABILIZATION);
    const char* dvsEnables = params->get(CameraParameters::KEY_VIDEO_STABILIZATION_SUPPORTED);
    if (!validateString(dvsEnable, dvsEnables)) {
        ALOGE("bad value for dvs enable : %s, supported are: %s", dvsEnable, dvsEnables);
        return BAD_VALUE;
    }

    return NO_ERROR;
}

bool ControlThread::validateHighSpeedResolutionFps(int width, int height, int fps) const
{
    LOG1("@%s size: %dx%d @ %d", __FUNCTION__, width, height, fps);

    if (fps > DEFAULT_RECORDING_FPS) {
        LOG1("high speed video recording mode");
        char sizeFpsStr[20];
        snprintf(sizeFpsStr, 20, "%dx%d@%d", width, height, fps);
        const char* supportedSizeFpsCombos = mParameters.get(IntelCameraParameters::KEY_SUPPORTED_HIGH_SPEED_RESOLUTION_FPS);
        if (!validateString(sizeFpsStr, supportedSizeFpsCombos)) {
            ALOGE("Unsupported high-speed video size@fps combination: %s, supported: %s", sizeFpsStr, supportedSizeFpsCombos);
            return false;
        }
    }
    return true;
}

status_t ControlThread::ProcessOverlayEnable(const CameraParameters *oldParams,
                                                   CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              IntelCameraParameters::KEY_HW_OVERLAY_RENDERING);

    if (!newVal.isEmpty()) {
        if (mState == STATE_STOPPED) {
            if (newVal == "true") {
                if (mPreviewThread->enableOverlay(true, PlatformData::overlayRotation(mCameraId)) == NO_ERROR) {
                    newParams->set(IntelCameraParameters::KEY_HW_OVERLAY_RENDERING, "true");
                    LOG1("@%s: Preview Overlay rendering enabled!", __FUNCTION__);
                } else {
                    ALOGE("Could not configure Overlay preview rendering");
                }
            }
        } else {
            ALOGW("Overlay cannot be enabled in other state than stop, ignoring request");
        }
    }
    return status;
}

status_t ControlThread::processParamDvs(const CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams, CameraParameters::KEY_VIDEO_STABILIZATION);
    if (!newVal.isEmpty()) {
        if (newVal == CameraParameters::TRUE)
            mDvsEnable = true;
        else
            mDvsEnable = false;
    }
    return status;
}

status_t ControlThread::processParamBurst(const CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    // Burst mode
    // Get the burst length
    mBurstLength = newParams->getInt(IntelCameraParameters::KEY_BURST_LENGTH);
    mFpsAdaptSkip = 0;
    mBurstLength = CLIP(mBurstLength,NUM_BURST_BUFFERS,0);
    if (mBurstLength > 0) {

        // Get the burst speed
        const char* s_speed = newParams->get(IntelCameraParameters::KEY_BURST_SPEED);
        String8 speed(s_speed, s_speed == NULL ? 0 : strlen(s_speed));
        if (speed == IntelCameraParameters::BURST_SPEED_LOW)
            mFpsAdaptSkip = BURST_SPEED_LOW_SKIP_NUM;
        else if (speed == IntelCameraParameters::BURST_SPEED_MEDIUM)
            mFpsAdaptSkip = BURST_SPEED_MEDIUM_SKIP_NUM;
        else
            mFpsAdaptSkip = BURST_SPEED_FAST_SKIP_NUM;
        LOG1("%s, mFpsAdaptSkip:%d", __FUNCTION__, mFpsAdaptSkip);
    }

    // Burst start-index (for Time Nudge et al)
    const char* burstStart = newParams->get(IntelCameraParameters::KEY_BURST_START_INDEX);
    int burstStartInt = burstStart ? atoi(burstStart) : 0;
    if (burstStartInt != mBurstStart) {
        LOG1("Burst start-index set %d -> %d", mBurstStart, burstStartInt);
        mBurstStart = burstStartInt;
    }

    return status;
}

status_t ControlThread::processDynamicParameters(const CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    // Zoom processing
    int newZoom = newParams->getInt(CameraParameters::KEY_ZOOM);
    bool zoomSupported = isParameterSet(CameraParameters::KEY_ZOOM_SUPPORTED) ? true : false;
    if (zoomSupported) {
        status = mISP->setZoom(newZoom);
        mPostProcThread->setZoom(mISP->zoomRatio(newZoom));
    } else {
        ALOGD("not supported zoom setting");
    }

    // Preview update mode
    if (status == NO_ERROR) {
        status = processPreviewUpdateMode(oldParams, newParams);
    }

    // Color effect
    if (status == NO_ERROR) {
        status = processParamEffect(oldParams, newParams);
    }

    // anti flicker
    if (status == NO_ERROR) {
        status = processParamAntiBanding(oldParams, newParams);
    }

    // preview framerate
    // NOTE: This is deprecated since Android API level 9, applications should use
    // setPreviewFpsRange()
    if (status == NO_ERROR) {
        status = processParamPreviewFrameRate(oldParams, newParams);
    }

    // slow motion value settings in high speed recording mode
    if (status == NO_ERROR) {
        status = processParamSlowMotionRate(oldParams, newParams);
    }

    // recording fps setting
    if (status == NO_ERROR) {
        status = processParamRecordingFramerate(oldParams, newParams);
    }

    if (status == NO_ERROR) {
        // white balance
        status = processParamWhiteBalance(oldParams, newParams);
    }

    if (status == NO_ERROR) {
        // exposure compensation
        status = processParamExposureCompensation(oldParams, newParams);
    }

    if (status == NO_ERROR) {
        // ISO manual setting (Intel extension)
        status = processParamIso(oldParams, newParams);
    }

    if (status == NO_ERROR) {
        status = processParamExifMaker(oldParams, newParams);
    }

    if (status == NO_ERROR) {
        status = processParamExifModel(oldParams, newParams);
    }

    if (status == NO_ERROR) {
        status = processParamExifSoftware(oldParams, newParams);
    }

    if (status == NO_ERROR) {
        // Saturation setting (Intel extension)
        status = processParamSaturation(oldParams, newParams);
    }

    if (status == NO_ERROR) {
        // Contrast setting (Intel extension)
        status = processParamContrast(oldParams, newParams);
    }

    if (status == NO_ERROR) {
        // Sharpness setting (Intel extension)
        status = processParamSharpness(oldParams, newParams);
    }

    if (!mFaceDetectionActive && status == NO_ERROR) {
        // customize metering
        status = processParamSetMeteringAreas(oldParams, newParams);
    }

    if (status == NO_ERROR) {
        // flash settings
        preProcessFlashMode(newParams);
        status = processParamFlash(oldParams, newParams);
    }

    if (status == NO_ERROR) {
        //Focus Mode
        status = processParamFocusMode(oldParams, newParams);
    }

    if (status == NO_ERROR) {
        // ae mode
        status = processParamAutoExposureMeteringMode(oldParams, newParams);
    }

    if (status == NO_ERROR) {
        // ae mode
        status = processParamAutoExposureMode(oldParams, newParams);
    }

    if (status == NO_ERROR) {
        // save mirrored image (for front camera)
        status = processParamMirroring(oldParams, newParams);
    }

    if (status == NO_ERROR) {
        // ae lock
        status = processParamAELock(oldParams, newParams);
    }

    if (status == NO_ERROR) {
        // awb lock
        status = processParamAWBLock(oldParams, newParams);
    }

    if (status == NO_ERROR) {
        // disable/enable Noise Reduction and Edge Enhancement
        status = processParamNREE(oldParams, newParams);
    }

    if (m3AControls->isIntel3A()) {
        if (status == NO_ERROR) {
            // af lock
            status = processParamAFLock(oldParams, newParams);
        }

        if (status == NO_ERROR) {
            // Capture bracketing
            status = processParamBracket(oldParams, newParams);
        }

        if (status == NO_ERROR) {
            // Smart Shutter Capture
            status = processParamSmartShutter(oldParams, newParams);
        }

        if (status == NO_ERROR) {
            // shutter manual setting (Intel extension)
            status = processParamShutter(oldParams, newParams);
        }
    }

    return status;
}
/**
 * Sends a request to PictureThread to allocate the snapshot and postview buffers
 *
 * if we already have the same buffer configuration available, returns without
 * asking PictureThread.
 *
 * The allocation request is synchronous.
 *
 * The buffers are allocated in the PictureThread to register the allocated
 * buffers with the HW JPEG encoder in this way the snapshot buffers are
 * already known to the HW encoder. This  speeds up the encoding.
 *
 * This call is used in the following situations:
 * - when preview has already started
 * - when processing parameters and those require new buffers
 *
 * care needs to be taken not to allocate the buffers at a time when ControlThread
 * needs to be fast for some performance metric, like when taking a snapshot
 * or when we are starting preview
 *
 */
status_t ControlThread::allocateSnapshotAndPostviewBuffers(bool videoMode)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    AtomBuffer formatDescriptorPv = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_FORMAT_DESCRIPTOR);
    AtomBuffer formatDescriptorSs = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_FORMAT_DESCRIPTOR);
    unsigned int bufCount = MAX(mBurstLength, mISP->getContinuousCaptureNumber()+1);

    mParameters.getPictureSize(&formatDescriptorSs.width, &formatDescriptorSs.height);

    /**
     * Snapshot format is hardcoded to NV12, this is the format between
     * camera and JPEG encoder. In cases where we need to capture bayer
     * then the format changes to RGB adn JPEG encoding breaks (i.e. image is
     * green) this is a known limitation of the raw capture sequence in ISP fW
     */
    if (CameraDump::isDumpImageEnable(CAMERA_DEBUG_DUMP_RAW))
        formatDescriptorSs.fourcc = mHwcg.mSensorCI->getRawFormat();
    else
        formatDescriptorSs.fourcc = V4L2_PIX_FMT_NV12;

    int recommendedNum = ((mBracketManager->getBracketMode() != BRACKET_NONE)?
        PlatformData::getMaxNumYUVBufferForBracket(mCameraId) : PlatformData::getMaxNumYUVBufferForBurst(mCameraId));
    /**
     * Get the buffer required and clip it to ensure we
     * allocate proper number of YUV buffers.
     */
    unsigned int clipTo = MAX(recommendedNum, (mISP->getContinuousCaptureNumber()+1));
    bufCount = CLIP(bufCount, clipTo, 1);

    if(videoMode){
       /**
        * In video mode we configure the Picture thread not to pre-allocate
        * the snapshot buffers. This means that there will be no active libVA
        * context created. we cannot have more than one libVA (encoder) context
        * active, and in video mode the video encoder already creates one.
        */
       bufCount = 0;
    }

    LOG1("Request to allocate %d bufs of (%dx%d) fourcc: %d",bufCount,
         formatDescriptorSs.width, formatDescriptorSs.height, formatDescriptorSs.fourcc);
    LOG1("Currently allocated: %d , available %d",
         mAllocatedSnapshotBuffers.size(), mAvailableSnapshotBuffers.size());


    bool allocSnapshot = true;
    bool allocPostview = true;

    // Check if we need to allocate new snapshot buffers
    if (!mAllocatedSnapshotBuffers.isEmpty()) {
        AtomBuffer tmp;
        tmp = mAllocatedSnapshotBuffers.itemAt(0);

        if ( (tmp.width == formatDescriptorSs.width) &&
             (tmp.height == formatDescriptorSs.height) &&
             (tmp.fourcc == formatDescriptorSs.fourcc) &&
             (mAllocatedSnapshotBuffers.size() == bufCount)) {
            LOG1("No need to request Snapshot, buffers already available");
            allocSnapshot = false;
        }
    }

    mISP->getPostviewFrameFormat(formatDescriptorPv);

    // Check if we need to allocate new postview buffers
    if (!mAllocatedPostviewBuffers.isEmpty()) {
        AtomBuffer tmp = mAllocatedPostviewBuffers.itemAt(0);

        if ((tmp.width == formatDescriptorPv.width) &&
            (tmp.height == formatDescriptorPv.height) &&
            (tmp.fourcc == formatDescriptorPv.fourcc) &&
            (mAllocatedPostviewBuffers.size() == bufCount)) {
            LOG1("No need to request Postview, buffers already available");
            allocPostview = false;
        }
    }

    if (!allocSnapshot && !allocPostview) {
        LOG1("@%s: No need to allocate postview or snapshot buffers. Already available.", __FUNCTION__);
        return NO_ERROR;
    }

    // check need to register bufs to scaler.. can't use ISP since ISP isn't
    // configured yet, so do it by checking the preview mode and sensor type
    ControlThread::State state = selectPreviewMode(mParameters);
    bool registerToScaler = (PlatformData::sensorType(mCameraId) == SENSOR_TYPE_SOC) &&
            (state == STATE_CONTINUOUS_CAPTURE);

    if (allocSnapshot) {
        mAllocatedSnapshotBuffers.clear();
        mAvailableSnapshotBuffers.clear();

        status = mPictureThread->allocSnapshotBuffers(formatDescriptorSs, bufCount,
                                                      &mAllocatedSnapshotBuffers,
                                                      registerToScaler);

        if (status != NO_ERROR) {
            ALOGE("Could not pre-allocate snapshot buffers!");
        } else {
            mAvailableSnapshotBuffers = mAllocatedSnapshotBuffers;
        }

        // update configuration inside  AtomISP class
        mISP->setSnapshotFrameFormat(formatDescriptorSs);
    }

    if (allocPostview) {
        mAllocatedPostviewBuffers.clear();
        mAvailablePostviewBuffers.clear();


        status = mPictureThread->allocPostviewBuffers(formatDescriptorPv, bufCount,
                                                      &mAllocatedPostviewBuffers,
                                                      registerToScaler);

        if (status != NO_ERROR) {
            ALOGE("Could not pre-allocate postview buffers!");
        } else {
            mAvailablePostviewBuffers = mAllocatedPostviewBuffers;
        }
    }

    return status;
}

void ControlThread::processParamFileInject(CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);

    unsigned int width = 0, height = 0, bayerOrder = 0, fourcc = 0;
    const char *fileName = newParams->get(IntelCameraParameters::KEY_FILE_INJECT_FILENAME);
    if (!fileName || !strncmp(fileName, "off", sizeof("off")))
        return;

    width = newParams->getInt(IntelCameraParameters::KEY_FILE_INJECT_WIDTH);
    height = newParams->getInt(IntelCameraParameters::KEY_FILE_INJECT_HEIGHT);
    bayerOrder = newParams->getInt(IntelCameraParameters::KEY_FILE_INJECT_BAYER_ORDER);
    fourcc = newParams->getInt(IntelCameraParameters::KEY_FILE_INJECT_FORMAT);

    LOG1("FILE INJECTION new parameter dumping:");
    LOG1("file name=%s,width=%d,height=%d,fourcc=%d,bayer-order=%d.",
          fileName, width, height, fourcc, bayerOrder);
    mISP->configureFileInject(fileName, width, height, fourcc, bayerOrder);

}
status_t ControlThread::processParamAFLock(const CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    // af lock mode
    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              IntelCameraParameters::KEY_AF_LOCK_MODE);
    if (!newVal.isEmpty()) {
        bool af_lock;
        // TODO: once available, use the definitions in Intel
        //       parameter namespace, see UMG BZ26264
        const char* PARAM_LOCK = "lock";
        const char* PARAM_UNLOCK = "unlock";

        if(newVal == PARAM_LOCK) {
            af_lock = true;
        } else if(newVal == PARAM_UNLOCK) {
            af_lock = false;
        } else {
            ALOGE("Invalid value received for %s: %s", IntelCameraParameters::KEY_AF_LOCK_MODE, newVal.string());
            return INVALID_OPERATION;
        }
        status = m3AControls->setAfLock(af_lock);

        if (status == NO_ERROR) {
            LOG1("Changed: %s -> %s", IntelCameraParameters::KEY_AF_LOCK_MODE, newVal.string());
        }
    }

    return status;
}

status_t ControlThread::processParamAWBLock(const CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    // awb lock mode
    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK);

    if (!newVal.isEmpty()) {
        bool awb_lock;

        if(newVal == CameraParameters::TRUE) {
            awb_lock = true;
        } else if(newVal == CameraParameters::FALSE) {
            awb_lock = false;
        } else {
            ALOGE("Invalid value received for %s: %s", CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK, newVal.string());
            return INVALID_OPERATION;
        }
        status = m3AThread->lockAwb(awb_lock);
        if (status == NO_ERROR) {
            LOG1("Changed: %s -> %s", CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK, newVal.string());
        }
    }

    return status;
}

/**
 *  Noise reduction algorithms
 *  XNR is currently supported during continuous capture
 *  ANR is NOT supported on continuous capture
 *
 *  For the above reasons if ANR is activated we need to force a preview re-start
 *  that will switch from Continuous Capture preview to "old-style" online preview
 *  In selectPreviewMode we check the status of ANR to decide this.
 *
 */
status_t ControlThread::processParamXNR_ANR(const CameraParameters *oldParams,
        CameraParameters *newParams, bool &restartNeeded)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    bool XNR_ANR_changed = false;

    // XNR
    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              IntelCameraParameters::KEY_XNR);
    if (!newVal.isEmpty()) {
        bool xnr = (newVal == CameraParameters::TRUE);
        // note: due add/remove of intel parameters newVal doesn't always
        // reflect changes of value in AtomISP level
        if (mISP->getXNR() != xnr) {
            LOG2("XNR value new %s", newVal.string());
            XNR_ANR_changed = true;
            mISP->setXNR(xnr);
        }
    }

    // ANR
    newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              IntelCameraParameters::KEY_ANR);
    if (!newVal.isEmpty()) {
        bool anr = (newVal == CameraParameters::TRUE);
        // note: due add/remove of intel parameters newVal doesn't always
        // reflect changes of value in AtomISP level
        if (mISP->getLowLight() != anr) {
            LOG2("ANR value new %s", newVal.string());
            XNR_ANR_changed = true;
            mISP->setLowLight(anr);
        }
    }

    if (XNR_ANR_changed) {
        if (mState == STATE_CONTINUOUS_CAPTURE) {
            // XNR needs continuous mode restart atm.
            // ANR is not supported at all, See selectPreviewMode().
            restartNeeded = true;
        } else if (restartNeeded == false &&
                   mState == STATE_PREVIEW_STILL) {
            // XNR/ANR is changing and restart is not requested for other reasons
            // check whether we can switch back to continuous-mode
            if (mState != selectPreviewMode(*newParams)) {
                restartNeeded = true;
            }
        }
    }

    return status;
}

/**
 * Processing of antibanding parameters
 * it checks if the parameter changed and then it selects the correct
 * FlickerMode
 * If 3A is supported by the sensor (i.e is a raw sensor) then configure
 * 3A library,
 * if it is a SOC sensor then the auto-exposure is controled via the sensor driver
 * so configure ISP
 * @param oldParams old parameters
 * @param newParams new parameters
 * @return NO_ERROR: everything went fine. settigns are applied
 * @return UNKNOWN_ERROR: error configuring 3A or V4L2
 */
status_t ControlThread::processParamAntiBanding(const CameraParameters *oldParams,
                                                      CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    FlickerMode lightFrequency;

    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              CameraParameters::KEY_ANTIBANDING);
    if (!newVal.isEmpty()) {

        if (newVal == CameraParameters::ANTIBANDING_50HZ)
            lightFrequency = CAM_AE_FLICKER_MODE_50HZ;
        else if (newVal == CameraParameters::ANTIBANDING_60HZ)
            lightFrequency = CAM_AE_FLICKER_MODE_60HZ;
        else if (newVal == CameraParameters::ANTIBANDING_AUTO)
            lightFrequency = CAM_AE_FLICKER_MODE_AUTO;
        else
            lightFrequency = CAM_AE_FLICKER_MODE_OFF;

        lightFrequency = CAM_AE_FLICKER_MODE_60HZ;
        status = m3AControls->setAeFlickerMode(lightFrequency);
    }

    return status;
}

status_t ControlThread::processParamAELock(const CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    // ae lock mode

    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              CameraParameters::KEY_AUTO_EXPOSURE_LOCK);
    if (!newVal.isEmpty()) {
        bool ae_lock;

        if(newVal == CameraParameters::TRUE) {
            ae_lock = true;
        } else  if(newVal == CameraParameters::FALSE) {
            ae_lock = false;
        } else {
            ALOGE("Invalid value received for %s: %s", CameraParameters::KEY_AUTO_EXPOSURE_LOCK, newVal.string());
            return INVALID_OPERATION;
        }

        status = m3AThread->lockAe(ae_lock);
        if (status == NO_ERROR) {
            LOG1("Changed: %s -> %s", CameraParameters::KEY_AUTO_EXPOSURE_LOCK, newVal.string());
            if (ae_lock) {
                mAELockFlashNeed = m3AControls->getAeFlashNecessary();
                LOG1("AE locked, storing flash necessity decision (%s)", mAELockFlashNeed ? "ON" : "OFF");
            }
        }
    }

    return status;
}

status_t ControlThread::processParamFlash(const CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              CameraParameters::KEY_FLASH_MODE);

    if (!newVal.isEmpty()) {
        FlashMode flash = CAM_AE_FLASH_MODE_AUTO;
        if(newVal == CameraParameters::FLASH_MODE_AUTO)
            flash = CAM_AE_FLASH_MODE_AUTO;
        else if(newVal == CameraParameters::FLASH_MODE_OFF)
            flash = CAM_AE_FLASH_MODE_OFF;
        else if(newVal == CameraParameters::FLASH_MODE_ON)
            flash = CAM_AE_FLASH_MODE_ON;
        else if(newVal == CameraParameters::FLASH_MODE_TORCH)
            flash = CAM_AE_FLASH_MODE_TORCH;
        else if(newVal == IntelCameraParameters::FLASH_MODE_SLOW_SYNC)
            flash = CAM_AE_FLASH_MODE_SLOW_SYNC;
        else if(newVal == IntelCameraParameters::FLASH_MODE_DAY_SYNC)
            flash = CAM_AE_FLASH_MODE_DAY_SYNC;

        mSavedFlashMode = newVal;

        status = m3AControls->setAeFlashMode(flash);
        if (status == NO_ERROR) {
            LOG1("Changed: %s -> %s", CameraParameters::KEY_FLASH_MODE, newVal.string());
        } else {
            // Ok in general for SOC sensors.
            // TODO: Kernel driver should support querying which controls the sensors support
            ALOGW("Error in setting flash mode \'%s\' (%d), 3A ctrl type: %d",
                 newVal.string(), flash, m3AControls->getType());
        }
    }

    // Return no error always, as we check and indicate the failure above.
    return NO_ERROR;
}

status_t ControlThread::processPreviewUpdateMode(const CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              IntelCameraParameters::KEY_PREVIEW_UPDATE_MODE);

    if (!newVal.isEmpty()) {
        if (newVal == IntelCameraParameters::PREVIEW_UPDATE_MODE_DURING_CAPTURE)
            mPreviewUpdateMode = IntelCameraParameters::PREVIEW_UPDATE_MODE_DURING_CAPTURE;
        else if (newVal == IntelCameraParameters::PREVIEW_UPDATE_MODE_CONTINUOUS)
            mPreviewUpdateMode = IntelCameraParameters::PREVIEW_UPDATE_MODE_CONTINUOUS;
        else if (newVal == IntelCameraParameters::PREVIEW_UPDATE_MODE_STANDARD)
            mPreviewUpdateMode = IntelCameraParameters::PREVIEW_UPDATE_MODE_STANDARD;
        else if (newVal == IntelCameraParameters::PREVIEW_UPDATE_MODE_WINDOWLESS) {
            if (mPreviewThread->isWindowConfigured()) {
                ALOGE("Windowless operation cannot be enabled, window already configured!");
                return INVALID_OPERATION;
            }
            if (mPreviewThread->getPreviewState() == PreviewThread::STATE_NO_WINDOW) {
                ALOGE("Windowless operation cannot be enabled, startPreview() already called");
                return INVALID_OPERATION;
            }
            mPreviewUpdateMode = IntelCameraParameters::PREVIEW_UPDATE_MODE_WINDOWLESS;
        } else {
            ALOGE("Unknown preview update mode received %s", newVal.string());
        }
    }
    return status;
}

status_t ControlThread::processParamEffect(const CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              CameraParameters::KEY_EFFECT);

    if (!newVal.isEmpty()) {
        status = m3AControls->set3AColorEffect(newVal.string());
        if (status == NO_ERROR) {
            LOG1("Changed: %s -> %s", CameraParameters::KEY_EFFECT, newVal.string());
        }
    }
    return status;
}

status_t ControlThread::processParamBracket(const CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              IntelCameraParameters::KEY_CAPTURE_BRACKET);

    if (!newVal.isEmpty()) {
        if(newVal == "exposure") {
            mBracketManager->setBracketMode(BRACKET_EXPOSURE);
        } else if(newVal == "focus") {
            mBracketManager->setBracketMode(BRACKET_FOCUS);
        } else if(newVal == "none") {
            mBracketManager->setBracketMode(BRACKET_NONE);
        } else {
            ALOGE("Invalid value received for %s: %s", IntelCameraParameters::KEY_CAPTURE_BRACKET, newVal.string());
            status = BAD_VALUE;
        }
        if (status == NO_ERROR) {
            LOG1("Changed: %s -> %s", IntelCameraParameters::KEY_CAPTURE_BRACKET, newVal.string());
        }
    }
    return status;
}

status_t ControlThread::processParamSmartShutter(const CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    //smile shutter threshold
    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              IntelCameraParameters::KEY_SMILE_SHUTTER_THRESHOLD);
    if (!newVal.isEmpty()) {
        int value = newParams->getInt(IntelCameraParameters::KEY_SMILE_SHUTTER_THRESHOLD);
        if (value < 0 || value > SMILE_THRESHOLD_MAX) {
            ALOGE("Invalid value received for %s: %d, set to default %d",
                IntelCameraParameters::KEY_SMILE_SHUTTER_THRESHOLD, value, SMILE_THRESHOLD);
            status = BAD_VALUE;
        }
        if (status == NO_ERROR) {
            LOG1("Changed: %s -> %d", IntelCameraParameters::KEY_SMILE_SHUTTER_THRESHOLD, value);
        }
    }

    //blink shutter threshold
    newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                      IntelCameraParameters::KEY_BLINK_SHUTTER_THRESHOLD);
    if (!newVal.isEmpty()) {
        int value = newParams->getInt(IntelCameraParameters::KEY_BLINK_SHUTTER_THRESHOLD);
        if (value < 0 || value > BLINK_THRESHOLD_MAX) {
            ALOGE("Invalid value received for %s: %d, set to default %d",
                IntelCameraParameters::KEY_BLINK_SHUTTER_THRESHOLD, value, BLINK_THRESHOLD);
            status = BAD_VALUE;
        }
        if (status == NO_ERROR) {
            LOG1("Changed: %s -> %d", IntelCameraParameters::KEY_BLINK_SHUTTER_THRESHOLD, value);
        }
    }
    return status;
}

status_t ControlThread::processParamHDR(const CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    status_t localStatus = NO_ERROR;
    int newWidth, newHeight;
    int oldWidth, oldHeight;

    newParams->getPictureSize(&newWidth, &newHeight);
    oldParams->getPictureSize(&oldWidth, &oldHeight);

    if (mHdr.inProgress) {
        ALOGW("%s: attempt to change hdr parameters during hdr capture", __FUNCTION__);
        // keep the value of mBurstLength when hdr is still runing.
        mBurstLength = mHdr.bracketNum;
        return INVALID_OPERATION;
    }

    //TODO remove newValIntel whenever we only use HDR scene mode
    // Check the HDR parameters
    String8 newValIntel = paramsReturnNewIfChanged(oldParams, newParams,
                                              IntelCameraParameters::KEY_HDR_IMAGING);
    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              CameraParameters::KEY_SCENE_MODE);

    if (!newVal.isEmpty() || !newValIntel.isEmpty()) {
        if(newValIntel == "on" || newVal == CameraParameters::SCENE_MODE_HDR) {
            mHdr.enabled = true;
            mHdr.bracketMode = BRACKET_EXPOSURE;
            mHdr.bracketNum = DEFAULT_HDR_BRACKETING;
            status = mCP->initializeHDR(newWidth, newHeight);
            if (status == NO_ERROR) {
                mHdr.enabled = true;
                mHdr.bracketMode = BRACKET_EXPOSURE;
                mHdr.savedBracketMode = mBracketManager->getBracketMode();
                mHdr.bracketNum = DEFAULT_HDR_BRACKETING;
            } else {
                ALOGE("HDR buffer allocation failed");
            }
        } else if ((newValIntel.isEmpty() && newVal != CameraParameters::SCENE_MODE_HDR)
                    || (newValIntel == "off" && newVal != CameraParameters::SCENE_MODE_HDR)) {
            if(mHdr.enabled) {
                status = mCP->uninitializeHDR();
                if (status != NO_ERROR)
                    ALOGE("HDR buffer release failed");
            }
            mHdr.enabled = false;
            mBracketManager->setBracketMode(mHdr.savedBracketMode);
        } else {
            if(!newValIntel.isEmpty()) {
            ALOGE("Invalid value received for %s: %s", IntelCameraParameters::KEY_HDR_IMAGING, newVal.string());
            status = BAD_VALUE;
            }
        }
    } else {
        // Re-allocate buffers if resolution changed and HDR was ON
        const char* oIntel = oldParams->get(IntelCameraParameters::KEY_HDR_IMAGING);
        const char* o = oldParams->get(CameraParameters::KEY_SCENE_MODE);
        String8 oldVal (o, (o == NULL ? 0 : strlen(o)));
        String8 oldValIntel (oIntel, (oIntel == NULL ? 0 : strlen(oIntel)));
        if((oldValIntel == "on" || oldVal == CameraParameters::SCENE_MODE_HDR) && (newWidth != oldWidth || newHeight != oldHeight)) {
            status = mCP->uninitializeHDR();
            if (status == NO_ERROR) {
                status = mCP->initializeHDR(newWidth, newHeight);
                if (status != NO_ERROR) {
                    ALOGE("HDR buffer allocation failed");
                }
            } else {
                ALOGE("HDR buffer release failed");
            }
        }
    }

    if (mHdr.enabled) {
        // Dependency parameters
        mBurstLength = mHdr.bracketNum;
        mBracketManager->setBracketMode(mHdr.bracketMode);
    }

    newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              IntelCameraParameters::KEY_HDR_SAVE_ORIGINAL);
    if (!newVal.isEmpty()) {
        localStatus = NO_ERROR;
        if(newVal == "on") {
            mHdr.saveOrig = true;
        } else if(newVal == "off") {
            mHdr.saveOrig = false;
        } else {
            // the default value is kept
            ALOGW("Invalid value received for %s: %s", IntelCameraParameters::KEY_HDR_SAVE_ORIGINAL, newVal.string());
            localStatus = BAD_VALUE;
        }
        if (localStatus == NO_ERROR) {
            LOG1("Changed: %s -> %s", IntelCameraParameters::KEY_HDR_SAVE_ORIGINAL, newVal.string());
        }
    }

    return status;
}

status_t ControlThread::processParamULL(const CameraParameters *oldParams,
        CameraParameters *newParams, bool *restartPreview)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    bool ullActive = false;
    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              IntelCameraParameters::KEY_ULL);
    if (!newVal.isEmpty()) {
        LOG1("ULL param new value: %s", newVal.string());

        if (newVal == "on") {
            mULL->setMode(UltraLowLight::ULL_ON);
            ullActive = true;
        } else if (newVal == "auto") {
            mULL->setMode(UltraLowLight::ULL_AUTO);
            ullActive = true;
        } else {
            mULL->setMode(UltraLowLight::ULL_OFF);
        }
        /**
         * If applications enables ULL while in Continuous Capture mode and
         * the current ring buffer configuration is not big enough we need
         * to re-start preview to make sure we have the correct configuration
         *
         */
        if (ullActive && mState != STATE_STOPPED) {
            if (mISP->getContinuousCaptureNumber() < mULL->MAX_INPUT_BUFFERS) {
                if (restartPreview)
                    *restartPreview = true;
            }
        }
    }

    return status;
}

void ControlThread::preProcessFlashMode(CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);

    // If there is no flash in device, then flash mode should be set
    // as off, that shall avoid HAL to go through the preFlashSequence.
    if (!PlatformData::supportsFlash(mCameraId)) {
        m3AControls->setAeFlashMode(CAM_AE_FLASH_MODE_OFF);
        return;
    }

    bool lowBattery = false;
    BatteryStatus bs = mHwcg.mFlashCI->getBatteryStatus();
    switch (bs) {
        case BATTERY_STATUS_WARNING:
            ALOGW("@%s low battery status warning", __FUNCTION__);
            //TODO call 3a interface
            break;
        case BATTERY_STATUS_ALERT:
            ALOGW("@%s low battery status alert", __FUNCTION__);
            //TODO call 3a interface
            break;
        case BATTERY_STATUS_CRITICAL:
            ALOGW("@%s critical low battery status", __FUNCTION__);
            //TODO call 3a interface
            lowBattery = true;
            break;
        case BATTERY_STATUS_INVALID:
            ALOGW("@%s invalid battery status", __FUNCTION__);
        case BATTERY_STATUS_NORMAL:
            //do nothing
        default:
            break;
    }

    const char* supportedFlashModes = newParams->get(CameraParameters::KEY_SUPPORTED_FLASH_MODES);
    String8 currSupportedFlashModes(supportedFlashModes, supportedFlashModes == NULL ? 0 : strlen(supportedFlashModes));
    const char* requestedFlashMode  = newParams->get(CameraParameters::KEY_FLASH_MODE);
    String8 currRequestedFlashMode(requestedFlashMode, requestedFlashMode == NULL ? 0 : strlen(requestedFlashMode));

    // If burst or HDR is enabled, the only supported flash mode is "off".
    // Also, we only want to record only the first change to "off".
    if (((mBurstLength > 1 || mHdr.enabled) && currSupportedFlashModes != CameraParameters::FLASH_MODE_OFF)
            || (lowBattery && currRequestedFlashMode != CameraParameters::FLASH_MODE_OFF)) {
        if (lowBattery) {
            ALOGW("@%s low battery for flash, force set flash mode to off", __FUNCTION__);
            //Callback to user
            mCallbacksThread->lowBattery();
        }
        newParams->set(CameraParameters::KEY_SUPPORTED_FLASH_MODES, CameraParameters::FLASH_MODE_OFF);
        newParams->set(CameraParameters::KEY_FLASH_MODE, CameraParameters::FLASH_MODE_OFF);
    } else if ((mBurstLength == 1 || mBurstLength == 0) && !mHdr.enabled && !lowBattery) {
        // Restore the supported flash modes to the values prior to forcing to "off":
        newParams->set(CameraParameters::KEY_SUPPORTED_FLASH_MODES, mSavedFlashSupported);
    }
}

/**
 * select flash mode for single or burst capture
 * in burst capture, the flash is forced to off, otherwise
 * saved single capture flash mode is applied.
 * \param newParams
 */
void ControlThread::selectFlashModeForScene(CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    // !mBurstLength is only for CTS to pass
    if (mBurstLength == 1 || !mBurstLength) {
        newParams->set(CameraParameters::KEY_SUPPORTED_FLASH_MODES, mSavedFlashSupported.string());
        newParams->set(CameraParameters::KEY_FLASH_MODE, mSavedFlashMode.string());
    } else {
        LOG1("Forcing flash off");
        newParams->set(CameraParameters::KEY_SUPPORTED_FLASH_MODES, "off");
        newParams->set(CameraParameters::KEY_FLASH_MODE, CameraParameters::FLASH_MODE_OFF);
    }
}

status_t ControlThread::processParamSceneMode(CameraParameters *oldParams,
        CameraParameters *newParams, bool &needRestart)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    String8 newScene = paramsReturnNewIfChanged(oldParams, newParams, CameraParameters::KEY_SCENE_MODE);

    // we can't run this during init() because CTS mandates flash to be off. Thus we will initially be in auto
    // scene mode with flash off, thanks to CTS. Therefore we check mThreadRunning which is off during init().
    if (!newScene.isEmpty() && mThreadRunning) {
        SceneMode sceneMode = CAM_AE_SCENE_MODE_AUTO;
        if (newScene == CameraParameters::SCENE_MODE_PORTRAIT) {
            sceneMode = CAM_AE_SCENE_MODE_PORTRAIT;
            if (PlatformData::sensorType(mCameraId) == SENSOR_TYPE_RAW) {
                if (!PlatformData::isFixedFocusCamera(mCameraId)) {
                    newParams->set(CameraParameters::KEY_FOCUS_MODE, CameraParameters::FOCUS_MODE_CONTINUOUS_PICTURE);
                    newParams->set(CameraParameters::KEY_SUPPORTED_FOCUS_MODES, "auto,continuous-picture");
                }
                newParams->set(CameraParameters::KEY_WHITE_BALANCE, CameraParameters::WHITE_BALANCE_AUTO);
                newParams->set(CameraParameters::KEY_SUPPORTED_ANTIBANDING, "off,50hz,60hz,auto");
                newParams->set(CameraParameters::KEY_ANTIBANDING, CameraParameters::ANTIBANDING_AUTO);
                newParams->set(IntelCameraParameters::KEY_AWB_MAPPING_MODE, IntelCameraParameters::AWB_MAPPING_AUTO);
                newParams->set(IntelCameraParameters::KEY_SUPPORTED_ISO, PlatformData::defaultIso(mCameraId));
                newParams->set(IntelCameraParameters::KEY_ISO, PlatformData::defaultIso(mCameraId));
                newParams->set(IntelCameraParameters::KEY_SUPPORTED_AE_METERING_MODES, "auto,center");
                newParams->set(IntelCameraParameters::KEY_SUPPORTED_XNR, "true,false");
                newParams->set(IntelCameraParameters::KEY_XNR, CameraParameters::FALSE);
                newParams->set(IntelCameraParameters::KEY_SUPPORTED_ANR, "false");
                newParams->set(IntelCameraParameters::KEY_ANR, CameraParameters::FALSE);
                newParams->set(CameraParameters::KEY_EXPOSURE_COMPENSATION, PlatformData::supportedDefaultEV(mCameraId));
                newParams->set(CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION, PlatformData::supportedMaxEV(mCameraId));
                newParams->set(CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION, PlatformData::supportedMinEV(mCameraId));
                newParams->set(CameraParameters::KEY_EXPOSURE_COMPENSATION_STEP, PlatformData::supportedStepEV(mCameraId));
            }
            if (PlatformData::supportsFlash(mCameraId)) {
                mSavedFlashSupported = String8("auto,off,on,torch");
                mSavedFlashMode = String8(CameraParameters::FLASH_MODE_AUTO);
                selectFlashModeForScene(newParams);
            }

        } else if (newScene == CameraParameters::SCENE_MODE_SPORTS || newScene == CameraParameters::SCENE_MODE_PARTY) {
            sceneMode = (newScene == CameraParameters::SCENE_MODE_SPORTS) ? CAM_AE_SCENE_MODE_SPORTS : CAM_AE_SCENE_MODE_PARTY;
            if (PlatformData::sensorType(mCameraId) == SENSOR_TYPE_RAW) {
                if (!PlatformData::isFixedFocusCamera(mCameraId)) {
                    newParams->set(CameraParameters::KEY_FOCUS_MODE, CameraParameters::FOCUS_MODE_INFINITY);
                    newParams->set(CameraParameters::KEY_SUPPORTED_FOCUS_MODES, "infinity");
                }
                newParams->set(IntelCameraParameters::KEY_SUPPORTED_ISO, PlatformData::defaultIso(mCameraId));
                newParams->set(IntelCameraParameters::KEY_ISO, PlatformData::defaultIso(mCameraId));
                newParams->set(CameraParameters::KEY_WHITE_BALANCE, CameraParameters::WHITE_BALANCE_AUTO);
                newParams->set(CameraParameters::KEY_SUPPORTED_ANTIBANDING, "off,50hz,60hz,auto");
                newParams->set(CameraParameters::KEY_ANTIBANDING, CameraParameters::ANTIBANDING_OFF);
                newParams->set(IntelCameraParameters::KEY_AWB_MAPPING_MODE, IntelCameraParameters::AWB_MAPPING_AUTO);
                newParams->set(IntelCameraParameters::KEY_AE_METERING_MODE, IntelCameraParameters::AE_METERING_MODE_AUTO);
                newParams->set(IntelCameraParameters::KEY_SUPPORTED_XNR, "true,false");
                newParams->set(IntelCameraParameters::KEY_XNR, CameraParameters::FALSE);
                newParams->set(IntelCameraParameters::KEY_SUPPORTED_ANR, "false");
                newParams->set(IntelCameraParameters::KEY_ANR, CameraParameters::FALSE);
                newParams->set(CameraParameters::KEY_EXPOSURE_COMPENSATION, PlatformData::supportedDefaultEV(mCameraId));
                newParams->set(CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION, PlatformData::supportedMaxEV(mCameraId));
                newParams->set(CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION, PlatformData::supportedMinEV(mCameraId));
                newParams->set(CameraParameters::KEY_EXPOSURE_COMPENSATION_STEP, PlatformData::supportedStepEV(mCameraId));
            }
            if (PlatformData::supportsFlash(mCameraId)) {
                mSavedFlashSupported = String8("off");
                mSavedFlashMode = String8(CameraParameters::FLASH_MODE_OFF);
                selectFlashModeForScene(newParams);
            }
        } else if (newScene == CameraParameters::SCENE_MODE_LANDSCAPE || newScene == CameraParameters::SCENE_MODE_SUNSET) {
            sceneMode = (newScene == CameraParameters::SCENE_MODE_LANDSCAPE) ? CAM_AE_SCENE_MODE_LANDSCAPE : CAM_AE_SCENE_MODE_SUNSET;
            if (PlatformData::sensorType(mCameraId) == SENSOR_TYPE_RAW) {
                if (!PlatformData::isFixedFocusCamera(mCameraId)) {
                    newParams->set(CameraParameters::KEY_FOCUS_MODE, CameraParameters::FOCUS_MODE_INFINITY);
                    newParams->set(CameraParameters::KEY_SUPPORTED_FOCUS_MODES, "infinity");
                }
                newParams->set(IntelCameraParameters::KEY_SUPPORTED_ISO, PlatformData::defaultIso(mCameraId));
                newParams->set(IntelCameraParameters::KEY_ISO, PlatformData::defaultIso(mCameraId));
                newParams->set(CameraParameters::KEY_WHITE_BALANCE, CameraParameters::WHITE_BALANCE_AUTO);
                newParams->set(CameraParameters::KEY_SUPPORTED_ANTIBANDING, "off,50hz,60hz,auto");
                newParams->set(CameraParameters::KEY_ANTIBANDING, CameraParameters::ANTIBANDING_OFF);
                newParams->set(IntelCameraParameters::KEY_AWB_MAPPING_MODE, IntelCameraParameters::AWB_MAPPING_OUTDOOR);
                newParams->set(IntelCameraParameters::KEY_AE_METERING_MODE, IntelCameraParameters::AE_METERING_MODE_AUTO);
                newParams->set(IntelCameraParameters::KEY_SUPPORTED_XNR, "true,false");
                newParams->set(IntelCameraParameters::KEY_XNR, CameraParameters::FALSE);
                newParams->set(IntelCameraParameters::KEY_SUPPORTED_ANR, "false");
                newParams->set(IntelCameraParameters::KEY_ANR, CameraParameters::FALSE);
                newParams->set(CameraParameters::KEY_EXPOSURE_COMPENSATION, PlatformData::supportedDefaultEV(mCameraId));
                newParams->set(CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION, PlatformData::supportedMaxEV(mCameraId));
                newParams->set(CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION, PlatformData::supportedMinEV(mCameraId));
                newParams->set(CameraParameters::KEY_EXPOSURE_COMPENSATION_STEP, PlatformData::supportedStepEV(mCameraId));
            }
            if (PlatformData::supportsFlash(mCameraId)) {
                mSavedFlashSupported = String8("off");
                mSavedFlashMode = String8(CameraParameters::FLASH_MODE_OFF);
                selectFlashModeForScene(newParams);
            }
        } else if (newScene == CameraParameters::SCENE_MODE_NIGHT) {
            sceneMode = CAM_AE_SCENE_MODE_NIGHT;
            if (PlatformData::sensorType(mCameraId) == SENSOR_TYPE_RAW) {
                if (!PlatformData::isFixedFocusCamera(mCameraId)) {
                    newParams->set(CameraParameters::KEY_FOCUS_MODE, CameraParameters::FOCUS_MODE_INFINITY);
                    newParams->set(CameraParameters::KEY_SUPPORTED_FOCUS_MODES, "infinity");
                }
                newParams->set(IntelCameraParameters::KEY_SUPPORTED_ISO, PlatformData::defaultIso(mCameraId));
                newParams->set(IntelCameraParameters::KEY_ISO, PlatformData::defaultIso(mCameraId));
                newParams->set(CameraParameters::KEY_WHITE_BALANCE, CameraParameters::WHITE_BALANCE_AUTO);
                newParams->set(CameraParameters::KEY_SUPPORTED_ANTIBANDING, "off,50hz,60hz,auto");
                newParams->set(CameraParameters::KEY_ANTIBANDING, CameraParameters::ANTIBANDING_OFF);
                newParams->set(IntelCameraParameters::KEY_AWB_MAPPING_MODE, IntelCameraParameters::AWB_MAPPING_AUTO);
                newParams->set(IntelCameraParameters::KEY_AE_METERING_MODE, IntelCameraParameters::AE_METERING_MODE_AUTO);
                newParams->set(IntelCameraParameters::KEY_SUPPORTED_XNR, "true");
                newParams->set(IntelCameraParameters::KEY_XNR, CameraParameters::TRUE);
                newParams->set(IntelCameraParameters::KEY_SUPPORTED_ANR, "true");
                newParams->set(IntelCameraParameters::KEY_ANR, CameraParameters::TRUE);
                newParams->set(CameraParameters::KEY_EXPOSURE_COMPENSATION, PlatformData::supportedDefaultEV(mCameraId));
                newParams->set(CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION, PlatformData::supportedMaxEV(mCameraId));
                newParams->set(CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION, PlatformData::supportedMinEV(mCameraId));
                newParams->set(CameraParameters::KEY_EXPOSURE_COMPENSATION_STEP, PlatformData::supportedStepEV(mCameraId));
            }
            if (PlatformData::supportsFlash(mCameraId)) {
                mSavedFlashSupported = String8("off");
                mSavedFlashMode = String8(CameraParameters::FLASH_MODE_OFF);
                selectFlashModeForScene(newParams);
            }
        } else if (newScene == CameraParameters::SCENE_MODE_NIGHT_PORTRAIT) {
            sceneMode = CAM_AE_SCENE_MODE_NIGHT_PORTRAIT;
            if (PlatformData::sensorType(mCameraId) == SENSOR_TYPE_RAW) {
                if (!PlatformData::isFixedFocusCamera(mCameraId)) {
                    newParams->set(CameraParameters::KEY_FOCUS_MODE, CameraParameters::FOCUS_MODE_CONTINUOUS_PICTURE);
                    newParams->set(CameraParameters::KEY_SUPPORTED_FOCUS_MODES, "auto,continuous-picture");
                }
                newParams->set(IntelCameraParameters::KEY_SUPPORTED_ISO, PlatformData::defaultIso(mCameraId));
                newParams->set(IntelCameraParameters::KEY_ISO, PlatformData::defaultIso(mCameraId));
                newParams->set(CameraParameters::KEY_WHITE_BALANCE, CameraParameters::WHITE_BALANCE_AUTO);
                newParams->set(CameraParameters::KEY_SUPPORTED_ANTIBANDING, "off,50hz,60hz,auto");
                newParams->set(CameraParameters::KEY_ANTIBANDING, CameraParameters::ANTIBANDING_OFF);
                newParams->set(IntelCameraParameters::KEY_AWB_MAPPING_MODE, IntelCameraParameters::AWB_MAPPING_AUTO);
                newParams->set(IntelCameraParameters::KEY_AE_METERING_MODE, IntelCameraParameters::AE_METERING_MODE_AUTO);
                newParams->set(IntelCameraParameters::KEY_SUPPORTED_XNR, "true");
                newParams->set(IntelCameraParameters::KEY_XNR, CameraParameters::TRUE);
                newParams->set(IntelCameraParameters::KEY_SUPPORTED_ANR, "true");
                newParams->set(IntelCameraParameters::KEY_ANR, CameraParameters::TRUE);
                newParams->set(CameraParameters::KEY_EXPOSURE_COMPENSATION, PlatformData::supportedDefaultEV(mCameraId));
                newParams->set(CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION, PlatformData::supportedMaxEV(mCameraId));
                newParams->set(CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION, PlatformData::supportedMinEV(mCameraId));
                newParams->set(CameraParameters::KEY_EXPOSURE_COMPENSATION_STEP, PlatformData::supportedStepEV(mCameraId));
            }
            if (PlatformData::supportsFlash(mCameraId)) {
                mSavedFlashSupported = String8("on");
                mSavedFlashMode = String8(CameraParameters::FLASH_MODE_ON);
                selectFlashModeForScene(newParams);
            }
        } else if (newScene == CameraParameters::SCENE_MODE_HDR) {
            sceneMode = CAM_AE_SCENE_MODE_AUTO;
            if (PlatformData::sensorType(mCameraId) == SENSOR_TYPE_RAW) {
                if (!PlatformData::isFixedFocusCamera(mCameraId)) {
                    newParams->set(CameraParameters::KEY_FOCUS_MODE, CameraParameters::FOCUS_MODE_CONTINUOUS_PICTURE);
                }
                newParams->set(IntelCameraParameters::KEY_SUPPORTED_ISO, PlatformData::defaultIso(mCameraId));
                newParams->set(IntelCameraParameters::KEY_ISO, PlatformData::defaultIso(mCameraId));
                newParams->set(IntelCameraParameters::KEY_AWB_MAPPING_MODE, IntelCameraParameters::AWB_MAPPING_AUTO);
                newParams->set(IntelCameraParameters::KEY_AE_METERING_MODE, IntelCameraParameters::AE_METERING_MODE_AUTO);
                newParams->set(CameraParameters::KEY_SUPPORTED_FOCUS_MODES, PlatformData::supportedFocusModes(mCameraId));
                newParams->set(IntelCameraParameters::KEY_BACK_LIGHTING_CORRECTION_MODE, IntelCameraParameters::BACK_LIGHT_COORECTION_OFF);
                newParams->set(IntelCameraParameters::KEY_SUPPORTED_XNR, "false");
                newParams->set(IntelCameraParameters::KEY_XNR, CameraParameters::FALSE);
                newParams->set(IntelCameraParameters::KEY_SUPPORTED_ANR, "false");
                newParams->set(IntelCameraParameters::KEY_ANR, CameraParameters::FALSE);
                newParams->set(CameraParameters::KEY_EXPOSURE_COMPENSATION, "0");
                newParams->set(CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION, "0");
                newParams->set(CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION, "0");
                newParams->set(CameraParameters::KEY_EXPOSURE_COMPENSATION_STEP, "0");
            }
            if (PlatformData::supportsFlash(mCameraId)) {
                mSavedFlashSupported = String8("off");
                mSavedFlashMode = String8(CameraParameters::FLASH_MODE_OFF);
                selectFlashModeForScene(newParams);
            }
        } else if (newScene == CameraParameters::SCENE_MODE_FIREWORKS) {
            sceneMode = CAM_AE_SCENE_MODE_FIREWORKS;
            if (PlatformData::sensorType(mCameraId) == SENSOR_TYPE_RAW) {
                if (!PlatformData::isFixedFocusCamera(mCameraId)) {
                    newParams->set(CameraParameters::KEY_FOCUS_MODE, CameraParameters::FOCUS_MODE_INFINITY);
                    newParams->set(CameraParameters::KEY_SUPPORTED_FOCUS_MODES, "infinity");
                }
                newParams->set(IntelCameraParameters::KEY_SUPPORTED_ISO, PlatformData::defaultIso(mCameraId));
                newParams->set(IntelCameraParameters::KEY_ISO, PlatformData::defaultIso(mCameraId));
                newParams->set(CameraParameters::KEY_WHITE_BALANCE, CameraParameters::WHITE_BALANCE_AUTO);
                newParams->set(CameraParameters::KEY_SUPPORTED_ANTIBANDING, "off,50hz,60hz,auto");
                newParams->set(CameraParameters::KEY_ANTIBANDING, CameraParameters::ANTIBANDING_OFF);
                newParams->set(IntelCameraParameters::KEY_AWB_MAPPING_MODE, IntelCameraParameters::AWB_MAPPING_AUTO);
                newParams->set(IntelCameraParameters::KEY_AE_METERING_MODE, IntelCameraParameters::AE_METERING_MODE_AUTO);
                newParams->set(IntelCameraParameters::KEY_SUPPORTED_XNR, "true,false");
                newParams->set(IntelCameraParameters::KEY_XNR, CameraParameters::FALSE);
                newParams->set(IntelCameraParameters::KEY_SUPPORTED_ANR, "false");
                newParams->set(IntelCameraParameters::KEY_ANR, CameraParameters::FALSE);
                newParams->set(CameraParameters::KEY_EXPOSURE_COMPENSATION, PlatformData::supportedDefaultEV(mCameraId));
                newParams->set(CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION, PlatformData::supportedMaxEV(mCameraId));
                newParams->set(CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION, PlatformData::supportedMinEV(mCameraId));
                newParams->set(CameraParameters::KEY_EXPOSURE_COMPENSATION_STEP, PlatformData::supportedStepEV(mCameraId));
            }
            if (PlatformData::supportsFlash(mCameraId)) {
                mSavedFlashSupported = String8("off");
                mSavedFlashMode = String8(CameraParameters::FLASH_MODE_OFF);
                selectFlashModeForScene(newParams);
            }
        } else if (newScene == CameraParameters::SCENE_MODE_BARCODE) {
            sceneMode = CAM_AE_SCENE_MODE_TEXT;
            if (PlatformData::sensorType(mCameraId) == SENSOR_TYPE_RAW) {
                if (!PlatformData::isFixedFocusCamera(mCameraId)) {
                    newParams->set(CameraParameters::KEY_FOCUS_MODE, CameraParameters::FOCUS_MODE_MACRO);
                    newParams->set(CameraParameters::KEY_SUPPORTED_FOCUS_MODES, "macro,continuous-picture");
                }
                newParams->set(IntelCameraParameters::KEY_SUPPORTED_ISO, PlatformData::defaultIso(mCameraId));
                newParams->set(IntelCameraParameters::KEY_ISO, PlatformData::defaultIso(mCameraId));
                newParams->set(CameraParameters::KEY_WHITE_BALANCE, CameraParameters::WHITE_BALANCE_AUTO);
                newParams->set(CameraParameters::KEY_ANTIBANDING, CameraParameters::ANTIBANDING_AUTO);
                newParams->set(CameraParameters::KEY_SUPPORTED_ANTIBANDING, "off,50hz,60hz,auto");
                newParams->set(IntelCameraParameters::KEY_AWB_MAPPING_MODE, IntelCameraParameters::AWB_MAPPING_AUTO);
                newParams->set(IntelCameraParameters::KEY_AE_METERING_MODE, IntelCameraParameters::AE_METERING_MODE_AUTO);
                newParams->set(IntelCameraParameters::KEY_SUPPORTED_XNR, "true,false");
                newParams->set(IntelCameraParameters::KEY_XNR, CameraParameters::FALSE);
                newParams->set(IntelCameraParameters::KEY_SUPPORTED_ANR, "false");
                newParams->set(IntelCameraParameters::KEY_ANR, CameraParameters::FALSE);
                newParams->set(CameraParameters::KEY_EXPOSURE_COMPENSATION, PlatformData::supportedDefaultEV(mCameraId));
                newParams->set(CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION, PlatformData::supportedMaxEV(mCameraId));
                newParams->set(CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION, PlatformData::supportedMinEV(mCameraId));
                newParams->set(CameraParameters::KEY_EXPOSURE_COMPENSATION_STEP, PlatformData::supportedStepEV(mCameraId));
            }
            if (PlatformData::supportsFlash(mCameraId)) {
                mSavedFlashSupported = String8("auto,off,on,torch");
                mSavedFlashMode = String8(CameraParameters::FLASH_MODE_AUTO);
                selectFlashModeForScene(newParams);
            }
        } else {
            if (newScene == CameraParameters::SCENE_MODE_CANDLELIGHT) {
                sceneMode = CAM_AE_SCENE_MODE_CANDLELIGHT;
            } else if (newScene == IntelCameraParameters::SCENE_MODE_BEACH_SNOW) {
                sceneMode = CAM_AE_SCENE_MODE_BEACH_SNOW;
            } else if (newScene == IntelCameraParameters::SCENE_MODE_DAWN_DUSK) {
                sceneMode = CAM_AE_SCENE_MODE_DAWN_DUSK;
            } else if (newScene == IntelCameraParameters::SCENE_MODE_FALL_COLORS) {
                sceneMode = CAM_AE_SCENE_MODE_FALL_COLORS;
            } else if (newScene == IntelCameraParameters::SCENE_MODE_BACKLIGHT) {
                sceneMode = CAM_AE_SCENE_MODE_BACKLIGHT;
            } else {
                LOG1("Unsupported %s: %s. Using AUTO!", CameraParameters::KEY_SCENE_MODE, newScene.string());
                sceneMode = CAM_AE_SCENE_MODE_AUTO;
            }

            if (PlatformData::sensorType(mCameraId) == SENSOR_TYPE_RAW) {
                if (!PlatformData::isFixedFocusCamera(mCameraId)) {
                    newParams->set(CameraParameters::KEY_FOCUS_MODE, CameraParameters::FOCUS_MODE_CONTINUOUS_PICTURE);
                    newParams->set(CameraParameters::KEY_SUPPORTED_FOCUS_MODES, "auto,infinity,fixed,macro,continuous-video,continuous-picture");
                }
                newParams->set(IntelCameraParameters::KEY_SUPPORTED_ISO, PlatformData::supportedIso(mCameraId));
                newParams->set(IntelCameraParameters::KEY_ISO, PlatformData::defaultIso(mCameraId));
                newParams->set(CameraParameters::KEY_WHITE_BALANCE, CameraParameters::WHITE_BALANCE_AUTO);
                newParams->set(CameraParameters::KEY_SUPPORTED_ANTIBANDING, "off,50hz,60hz,auto");
                newParams->set(CameraParameters::KEY_ANTIBANDING, CameraParameters::ANTIBANDING_AUTO);
                newParams->set(IntelCameraParameters::KEY_AWB_MAPPING_MODE, IntelCameraParameters::AWB_MAPPING_AUTO);
                newParams->set(IntelCameraParameters::KEY_SUPPORTED_AE_METERING_MODES, "auto,center,spot");
                newParams->set(IntelCameraParameters::KEY_AE_METERING_MODE, IntelCameraParameters::AE_METERING_MODE_AUTO);
                newParams->set(IntelCameraParameters::KEY_SUPPORTED_XNR, "true,false");
                newParams->set(IntelCameraParameters::KEY_XNR, CameraParameters::FALSE);
                newParams->set(IntelCameraParameters::KEY_SUPPORTED_ANR, "true,false");
                newParams->set(IntelCameraParameters::KEY_ANR, CameraParameters::FALSE);
                newParams->set(CameraParameters::KEY_EXPOSURE_COMPENSATION, PlatformData::supportedDefaultEV(mCameraId));
                newParams->set(CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION, PlatformData::supportedMaxEV(mCameraId));
                newParams->set(CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION, PlatformData::supportedMinEV(mCameraId));
                newParams->set(CameraParameters::KEY_EXPOSURE_COMPENSATION_STEP, PlatformData::supportedStepEV(mCameraId));
            }
            if (PlatformData::supportsFlash(mCameraId)) {
                mSavedFlashSupported = String8("auto,off,on,torch");
                mSavedFlashMode = String8(CameraParameters::FLASH_MODE_AUTO);
                selectFlashModeForScene(newParams);
            }
        }

        m3AControls->setAeSceneMode(sceneMode);
        if (status == NO_ERROR) {
            LOG1("Changed: %s -> %s", CameraParameters::KEY_SCENE_MODE, newScene.string());
        }

        // Forget current parameters to enforce refreshing the parameters to 3A
        // this is done due that setAeSceneMode() resets AIQ configuration to initial defaults
        oldParams->remove(CameraParameters::KEY_FOCUS_MODE);
        oldParams->remove(CameraParameters::KEY_FLASH_MODE);
        oldParams->remove(CameraParameters::KEY_WHITE_BALANCE);
        oldParams->remove(CameraParameters::KEY_ANTIBANDING);
        oldParams->remove(IntelCameraParameters::KEY_ISO);
        oldParams->remove(IntelCameraParameters::KEY_AWB_MAPPING_MODE);
        oldParams->remove(IntelCameraParameters::KEY_AE_METERING_MODE);
        oldParams->remove(CameraParameters::KEY_EXPOSURE_COMPENSATION);

        // If Intel params are not allowed,
        // we should update Intel params setting to HW, and remove them here.
        if (!mIntelParamsAllowed) {

            processParamIso(oldParams, newParams);
            processParamXNR_ANR(oldParams, newParams, needRestart);

            newParams->remove(IntelCameraParameters::KEY_SUPPORTED_ISO);
            newParams->remove(IntelCameraParameters::KEY_ISO);
            newParams->remove(IntelCameraParameters::KEY_SUPPORTED_AWB_MAPPING_MODES);
            newParams->remove(IntelCameraParameters::KEY_AWB_MAPPING_MODE);
            newParams->remove(IntelCameraParameters::KEY_SUPPORTED_AE_METERING_MODES);
            newParams->remove(IntelCameraParameters::KEY_SUPPORTED_XNR);
            newParams->remove(IntelCameraParameters::KEY_XNR);
            newParams->remove(IntelCameraParameters::KEY_SUPPORTED_ANR);
            newParams->remove(IntelCameraParameters::KEY_ANR);
        }
    }

    return status;
}

void ControlThread::convertAfWindows(CameraWindow* focusWindows, size_t winCount)
{
    LOG1("@%s", __FUNCTION__);
    if (winCount > 0) {

        for (size_t i = 0; i < winCount; i++) {
            // Camera KEY_FOCUS_AREAS Coordinates range from -1000 to 1000. Let's convert..
            convertFromAndroidToIaCoordinates(focusWindows[i], focusWindows[i]);
            LOG1("Converted AF window %d: (%d,%d,%d,%d)",
                    i,
                    focusWindows[i].x_left,
                    focusWindows[i].y_top,
                    focusWindows[i].x_right,
                    focusWindows[i].y_bottom);
        }
    }
}

status_t ControlThread::processParamFocusMode(const CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams, CameraParameters::KEY_FOCUS_MODE);
    AfMode afMode = CAM_AF_MODE_NOT_SET;
    AfMode curAfMode = CAM_AF_MODE_NOT_SET;

    if (!newVal.isEmpty()) {
        if (newVal == CameraParameters::FOCUS_MODE_AUTO) {
            afMode = CAM_AF_MODE_AUTO;
        } else if (newVal == CameraParameters::FOCUS_MODE_INFINITY) {
            afMode = CAM_AF_MODE_INFINITY;
        } else if (newVal == CameraParameters::FOCUS_MODE_FIXED) {
            afMode = CAM_AF_MODE_FIXED;
        } else if (newVal == CameraParameters::FOCUS_MODE_MACRO) {
            afMode = CAM_AF_MODE_MACRO;
        } else if (newVal == CameraParameters::FOCUS_MODE_CONTINUOUS_VIDEO ||
                   newVal == CameraParameters::FOCUS_MODE_CONTINUOUS_PICTURE) {
            afMode = CAM_AF_MODE_CONTINUOUS;
        } else {
            afMode = CAM_AF_MODE_MANUAL;
        }

        curAfMode = m3AControls->getAfMode();

        if (status == NO_ERROR && curAfMode != afMode) {
            // See if we have to change the actual mode (it could be correct already)
            status = m3AControls->setAfMode(afMode);
        }

        if (status == NO_ERROR) {
            LOG1("Changed: %s -> %s", CameraParameters::KEY_FOCUS_MODE, newVal.string());
        } else {
            // Ok in general for SOC sensors.
            // TODO: Kernel driver should support querying which controls the sensors support
            ALOGW("Could not set AF mode to \'%s\' (%d),  3A ctrl type: %d",
                 newVal.string(), afMode, m3AControls->getType());
        }
    }

    if (!mFaceDetectionActive) {
        curAfMode = m3AControls->getAfMode();
        // Based on Google specs, the focus area is effective only for modes:
        // (framework side constants:) FOCUS_MODE_AUTO, FOCUS_MODE_MACRO, FOCUS_MODE_CONTINUOUS_VIDEO
        // or FOCUS_MODE_CONTINUOUS_PICTURE.
        if (curAfMode == CAM_AF_MODE_AUTO ||
            curAfMode == CAM_AF_MODE_CONTINUOUS ||
            curAfMode == CAM_AF_MODE_MACRO) {

                size_t winCount(mFocusAreas.numOfAreas());
                CameraWindow *focusWindows = new CameraWindow[winCount];
                mFocusAreas.toWindows(focusWindows);
                convertAfWindows(focusWindows, winCount);

                if (m3AControls->setAfWindows(focusWindows, winCount) != NO_ERROR) {
                    // If focus windows couldn't be set, previous AF mode is used
                    curAfMode = m3AControls->getAfMode();
                    ALOGW("Could not set AF windows. Resetting the AF back to %d", curAfMode);
                    m3AControls->setAfMode(curAfMode);
                }

                delete[] focusWindows;
                focusWindows = NULL;
        }
    }

    // Return NO_ERROR always. Setting AF to SOC sensor may fail, but
    // we don't consider this as an error.
    return NO_ERROR;
}

status_t ControlThread:: processParamSetMeteringAreas(const CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    // TODO: Support for more windows. At the moment we only support one?
    if (!mMeteringAreas.isEmpty()) {
        //int w, h;
        size_t winCount(mMeteringAreas.numOfAreas());
        CameraWindow *meteringWindows = new CameraWindow[winCount];
        CameraWindow aeWindow;

        mMeteringAreas.toWindows(meteringWindows);
        convertFromAndroidToIaCoordinates(meteringWindows[0], aeWindow);

        if (m3AControls->setAeMeteringMode(CAM_AE_METERING_MODE_SPOT) == NO_ERROR) {
            LOG1("@%s, Got metering area, and \"spot\" mode set. Setting window.", __FUNCTION__ );
            if (m3AControls->setAeWindow(&aeWindow) != NO_ERROR) {
                ALOGW("Error setting AE metering window. Metering will not work");
            }
        } else {
                ALOGW("Error setting AE metering mode to \"spot\". Metering will not work");
        }

        delete[] meteringWindows;
        meteringWindows = NULL;
    } else {
        // Resetting back to previous AE metering mode, if it was set (Intel extension, so
        // standard app won't be using "previous mode")
        const char* modeStr = newParams->get(IntelCameraParameters::KEY_AE_METERING_MODE);
        MeteringMode oldMode = CAM_AE_METERING_MODE_AUTO;
        if (modeStr != NULL) {
            oldMode = aeMeteringModeFromString(String8(modeStr));
        }

        if (oldMode != m3AControls->getAeMeteringMode()) {
            LOG1("Resetting from \"spot\" to (previous) AE metering mode (%d).", oldMode);
            m3AControls->setAeMeteringMode(oldMode);
        }

        if (oldMode == CAM_AE_METERING_MODE_SPOT) {
            AAAWindowInfo aaaWindow;
            m3AControls->getGridWindow(aaaWindow);
            updateSpotWindow(aaaWindow.width, aaaWindow.height);
        }
    }

    return status;
}

status_t ControlThread::processParamExposureCompensation(const CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              CameraParameters::KEY_EXPOSURE_COMPENSATION);
    if (!newVal.isEmpty()) {
        int exposure = newParams->getInt(CameraParameters::KEY_EXPOSURE_COMPENSATION);
        float comp_step = newParams->getFloat(CameraParameters::KEY_EXPOSURE_COMPENSATION_STEP);
        if (PlatformData::supportEV(mHwcg.mSensorCI->getCurrentCameraId()))
            status = m3AControls->setEv(exposure * comp_step);
        float ev = 0;
        if (PlatformData::supportEV(mHwcg.mSensorCI->getCurrentCameraId()))
            m3AControls->getEv(&ev);
        ALOGD("exposure compensation to \"%s\" (%d), ev value %f, res %d",
             newVal.string(), exposure, ev, status);
    }
    return status;
}

/**
 * Sets AutoExposure mode

 * Note, this is an Intel extension, so the values are not defined in
 * Android documentation.
 */
status_t ControlThread::processParamAutoExposureMode(const CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              IntelCameraParameters::KEY_AE_MODE);
    if (!newVal.isEmpty()) {
        AeMode ae_mode (CAM_AE_MODE_AUTO);

        if (newVal == "auto") {
            ae_mode = CAM_AE_MODE_AUTO;
        } else if (newVal == "manual") {
            ae_mode = CAM_AE_MODE_MANUAL;
        } else if (newVal == "shutter-priority") {
            ae_mode = CAM_AE_MODE_SHUTTER_PRIORITY;
            // antibanding cannot be supported when shutter-priority
            // is selected, so turning antibanding off (see BZ17480)
            newParams->set(CameraParameters::KEY_ANTIBANDING, "off");
        } else if (newVal == "aperture-priority") {
            ae_mode = CAM_AE_MODE_APERTURE_PRIORITY;
        } else {
            ALOGW("unknown AE_MODE \"%s\", falling back to AUTO", newVal.string());
            ae_mode = CAM_AE_MODE_AUTO;
        }
        m3AControls->setPublicAeMode(ae_mode);
        m3AControls->setAeMode(ae_mode);
        ALOGD("Changed ae mode to \"%s\" (%d)", newVal.string(), ae_mode);

        if (mPublicShutter >= 0 &&
                (ae_mode == CAM_AE_MODE_SHUTTER_PRIORITY ||
                ae_mode == CAM_AE_MODE_MANUAL)) {
            m3AControls->setManualShutter(mPublicShutter);
            ALOGD("Changed shutter to %f", mPublicShutter);
        }
    }
    return status;
}

/**
 * Sets Auto Exposure Metering Mode
 *
 * Note, this is an Intel extension, so the values are not defined in
 * Android documentation.
 */
status_t ControlThread::processParamAutoExposureMeteringMode(
        const CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              IntelCameraParameters::KEY_AE_METERING_MODE);
    if (!newVal.isEmpty()) {
        MeteringMode mode = aeMeteringModeFromString(newVal);

        // The fixed "spot" metering mode (and area) should be set only when user has set the
        // AE metering area to null (isEmpty() == true)
        if (mode == CAM_AE_METERING_MODE_SPOT && mMeteringAreas.isEmpty()) {
            AAAWindowInfo aaaWindow;
            m3AControls->getGridWindow(aaaWindow);
            // Let's set metering area to fixed position here. We will also get arbitrary area
            // when using touch AE, which is handled in processParamSetMeteringAreas().
            updateSpotWindow(aaaWindow.width, aaaWindow.height);
        } else if (mode == CAM_AE_METERING_MODE_SPOT) {
            ALOGE("User trying to set AE metering mode \"spot\" with an AE metering area.");
        }

        m3AControls->setAeMeteringMode(mode);
        ALOGD("Changed ae metering mode to \"%s\" (%d)", newVal.string(), mode);
    }

    return status;
}

/**
 * Sets manual ISO sensitivity value
 *
 * Note, this is an Intel extension, so the values are not defined in
 * Android documentation.
 */
status_t ControlThread::processParamIso(const CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                   IntelCameraParameters::KEY_ISO);
    if (newVal.isEmpty()) return status;
    // note: value format is 'iso-NNN'
    const size_t iso_prefix_len = 4;
    if (newVal.length() > iso_prefix_len) {
        IsoMode iso_mode(CAM_AE_ISO_MODE_AUTO);
        const char* isostr = newVal.string() + iso_prefix_len;
        if (strcmp("auto", isostr)) {
            iso_mode = CAM_AE_ISO_MODE_MANUAL;
            int iso = atoi(isostr);
            m3AControls->setManualIso(iso);
            ALOGD("Changed manual iso to \"%s\" (%d)", newVal.string(), iso);
        } else {
            ALOGD("Changed auto iso to \"%s\"", newVal.string());
        }
        m3AControls->setIsoMode(iso_mode);
    }
    return status;
}

status_t ControlThread::processParamContrast(const CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    int value;
    status_t status = NO_ERROR;
    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              IntelCameraParameters::KEY_CONTRAST_MODE);
    if (!newVal.isEmpty()) {
        if (!strcmp(newVal.string(), IntelCameraParameters::CONTRAST_MODE_SOFT))
            value = EXIF_CONTRAST_SOFT;
        else if (!strcmp(newVal.string(), IntelCameraParameters::CONTRAST_MODE_HARD))
            value = EXIF_CONTRAST_HARD;
        else
            value = EXIF_CONTRAST_NORMAL;

        mISP->setContrast(value);
    }
    return status;
}

status_t ControlThread::processParamSaturation(const CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    int value;
    status_t status = NO_ERROR;
    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              IntelCameraParameters::KEY_SATURATION_MODE);
    if (!newVal.isEmpty()) {
        if (!strcmp(newVal.string(), IntelCameraParameters::SATURATION_MODE_LOW))
            value = EXIF_SATURATION_LOW;
        else if (!strcmp(newVal.string(), IntelCameraParameters::SATURATION_MODE_HIGH))
            value = EXIF_SATURATION_HIGH;
        else
            value = EXIF_SATURATION_NORMAL;

        mISP->setSaturation(value);
    }
    return status;
}

status_t ControlThread::processParamSharpness(const CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    int value;
    status_t status = NO_ERROR;
    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              IntelCameraParameters::KEY_SHARPNESS_MODE);
    if (!newVal.isEmpty()) {
        if (!strcmp(newVal.string(), IntelCameraParameters::SHARPNESS_MODE_SOFT))
            value = EXIF_SHARPNESS_SOFT;
        else if (!strcmp(newVal.string(), IntelCameraParameters::SHARPNESS_MODE_HARD))
            value = EXIF_SHARPNESS_HARD;
        else
            value = EXIF_SHARPNESS_NORMAL;

        mISP->setSharpness(value);
    }
    return status;
}

/**
 * Sets manual shutter time value
 *
 * Note, this is an Intel extension, so the values are not defined in
 * Android documentation.
 */
status_t ControlThread::processParamShutter(const CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              IntelCameraParameters::KEY_SHUTTER);
    if (!newVal.isEmpty()) {
        float shutter = -1;
        bool flagParsed = false;

        if (strchr(newVal.string(), 's') != NULL) {
            // ns: n seconds
            shutter = atof(newVal.string());
            flagParsed = true;
        } else if (strchr(newVal.string(), 'm') != NULL) {
            // nm: n minutes
            shutter = atof(newVal.string()) * 60;
            flagParsed = true;
        } else {
            // n: 1/n second
            float tmp = atof(newVal.string());
            if (tmp > 0) {
                shutter = 1.0 / atof(newVal.string());
                flagParsed = true;
            }
        }

        if (flagParsed) {
            mPublicShutter = shutter;
            if (m3AControls->getAeMode() == CAM_AE_MODE_MANUAL ||
                (m3AControls->getAeMode() == CAM_AE_MODE_SHUTTER_PRIORITY)) {
                m3AControls->setManualShutter(mPublicShutter);
                ALOGD("Changed shutter to \"%s\" (%f)", newVal.string(), shutter);
            }
        }
    }

    return status;
}

status_t ControlThread::processParamWhiteBalance(const CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              CameraParameters::KEY_WHITE_BALANCE);
    if (!newVal.isEmpty()) {
        AwbMode wbMode = CAM_AWB_MODE_AUTO;
        // TODO: once available, use the definitions in Intel
        //       parameter namespace, see UMG BZ26264
        const char* PARAM_MANUAL = "manual";

        if(newVal == CameraParameters::WHITE_BALANCE_AUTO) {
            wbMode = CAM_AWB_MODE_AUTO;
        } else if(newVal == CameraParameters::WHITE_BALANCE_INCANDESCENT) {
            wbMode = CAM_AWB_MODE_WARM_INCANDESCENT;
        } else if(newVal == CameraParameters::WHITE_BALANCE_FLUORESCENT) {
            wbMode = CAM_AWB_MODE_FLUORESCENT;
        } else if(newVal == CameraParameters::WHITE_BALANCE_WARM_FLUORESCENT) {
            wbMode = CAM_AWB_MODE_WARM_FLUORESCENT;
        } else if(newVal == CameraParameters::WHITE_BALANCE_DAYLIGHT) {
            wbMode = CAM_AWB_MODE_DAYLIGHT;
        } else if(newVal == CameraParameters::WHITE_BALANCE_CLOUDY_DAYLIGHT) {
            wbMode = CAM_AWB_MODE_CLOUDY;
        } else if(newVal == CameraParameters::WHITE_BALANCE_TWILIGHT) {
            wbMode = CAM_AWB_MODE_SUNSET;
        } else if(newVal == CameraParameters::WHITE_BALANCE_SHADE) {
            wbMode = CAM_AWB_MODE_SHADOW;
        } else if(newVal == PARAM_MANUAL) {
            wbMode = CAM_AWB_MODE_MANUAL_INPUT;
        }

        status = m3AControls->setAwbMode(wbMode);

        if (status == NO_ERROR) {
            LOG1("Changed: %s -> %s", CameraParameters::KEY_WHITE_BALANCE, newVal.string());
        } else {
            // For SOC sensors, this is generally OK.
            // TODO: should query the support from kernel driver, when driver supports this.
            ALOGW("Error while setting AWB mode \'%s\' (%d), 3A ctrl type: %d",
                 newVal.string() , wbMode, m3AControls->getType());
        }
    }

    // Return NO_ERROR always, although setting the AWB might fail, for example on SOC sensors
    // that do not support this.
    return NO_ERROR;
}

status_t ControlThread::processParamRawDataFormat(const CameraParameters *oldParams,
        CameraParameters *newParams, bool &previewRestartNeeded)
{
    LOG1("@%s", __FUNCTION__);
    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              IntelCameraParameters::KEY_RAW_DATA_FORMAT);
    if (!newVal.isEmpty()) {
        if (newVal == "bayer") {
            CameraDump::setDumpDataFlag(CAMERA_DEBUG_DUMP_RAW);
            mCameraDump = CameraDump::getInstance(mCameraId);
            previewRestartNeeded = true;
        } else if (newVal == "yuv") {
            CameraDump::setDumpDataFlag(CAMERA_DEBUG_DUMP_YUV);
            mCameraDump = CameraDump::getInstance(mCameraId);
        } else
            CameraDump::setDumpDataFlag(RAW_NONE);
    }
    return NO_ERROR;
}

status_t ControlThread::processParamPreviewFrameRate(const CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s : NOTE: DEPRECATED", __FUNCTION__);

    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              CameraParameters::KEY_PREVIEW_FRAME_RATE);

    if (!newVal.isEmpty()) {
        ALOGI("DEPRECATED: Got new preview frame rate: %s", newVal.string());
        int fps = newParams->getPreviewFrameRate();
        // Save the set FPS for doing frame dropping
        mISP->setPreviewFramerate(fps);
    }

    return NO_ERROR;
}

/**
 * Sets slow motion rate value in high speed recording mode
 *
 * Note, this is an Intel extension, so the values are not defined in
 * Android documentation.
 */
status_t ControlThread::processParamSlowMotionRate(const CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);

    status_t status = NO_ERROR;
    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              IntelCameraParameters::KEY_SLOW_MOTION_RATE);
    if (!newVal.isEmpty()) {
        int slowMotionRate = 1;
        if(newVal == IntelCameraParameters::SLOW_MOTION_RATE_1X) {
            slowMotionRate = 1;
        } else if (newVal == IntelCameraParameters::SLOW_MOTION_RATE_2X) {
            slowMotionRate = 2;
        } else if (newVal == IntelCameraParameters::SLOW_MOTION_RATE_3X) {
            slowMotionRate = 3;
        } else if (newVal == IntelCameraParameters::SLOW_MOTION_RATE_4X) {
            slowMotionRate = 4;
        } else {
            return BAD_VALUE;
        }
        status = mVideoThread->setSlowMotionRate(slowMotionRate);
        if(status == NO_ERROR)
            LOG1("Changed hs value to \"%s\" (%d)", newVal.string(), slowMotionRate);
    }
    return status;
}

/**
 * Sets fps in high speed recording mode
 *
 * Note, this is an Intel extension, so the values are not defined in
 * Android documentation.
 */
status_t ControlThread::processParamRecordingFramerate(const CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              IntelCameraParameters::KEY_RECORDING_FRAME_RATE);

    if (!newVal.isEmpty()) {
        int fps = newParams->getInt(IntelCameraParameters::KEY_RECORDING_FRAME_RATE);
        LOG1("Got new recording fps: %d", fps);
        mISP->setRecordingFramerate(fps);
    }
    return status;
}

status_t ControlThread::processParamExifMaker(const CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              IntelCameraParameters::KEY_EXIF_MAKER);

    if (!newVal.isEmpty()) {
        LOG1("Got new Exif maker: %s", newVal.string());
        mPictureThread->setExifMaker(newVal);
    }

    return NO_ERROR;
}

status_t ControlThread::processParamExifModel(const CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              IntelCameraParameters::KEY_EXIF_MODEL);

    if (!newVal.isEmpty()) {
        LOG1("Got new Exif model: %s", newVal.string());
        mPictureThread->setExifModel(newVal);
    }

    return NO_ERROR;
}

status_t ControlThread::processParamExifSoftware(const CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              IntelCameraParameters::KEY_EXIF_SOFTWARE);

    if (!newVal.isEmpty()) {
        LOG1("Got new Exif software: %s", newVal.string());
        mPictureThread->setExifSoftware(newVal);
    }

    return NO_ERROR;
}

status_t ControlThread::processParamMirroring(const CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              IntelCameraParameters::KEY_SAVE_MIRRORED);

    if (!newVal.isEmpty()) {
        if (newVal == CameraParameters::TRUE) {
            mSaveMirrored = true;
            mCurrentOrientation = SensorThread::getInstance(mCameraId)->registerOrientationListener(this);
         } else {
            mSaveMirrored = false;
            SensorThread::getInstance(mCameraId)->unRegisterOrientationListener(this);
        }
        LOG1("Changed: %s -> %s", IntelCameraParameters::KEY_SAVE_MIRRORED, newVal.string());
    }

    return NO_ERROR;
}

status_t ControlThread::processParamNREE(const CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              IntelCameraParameters::KEY_NOISE_REDUCTION_AND_EDGE_ENHANCEMENT);

    if (!newVal.isEmpty()) {
        if (newVal == CameraParameters::TRUE) {
            //Disable Noise Reduction and Edge Enhancement
            mISP->setNrEE(true);
        } else {
            mISP->setNrEE(false);
        }

        LOG1("Changed: %s -> %s",
             IntelCameraParameters::KEY_NOISE_REDUCTION_AND_EDGE_ENHANCEMENT, newVal.string());
    }

    return NO_ERROR;
}

/*
 * Process parameters that require the ISP to be stopped.
 *
 * @param[in] oldParams the previous parameters
 * @param[in] newParams the new parameters which are being set
 * @param[out] restartNeeded boolean to detect whether a preview re-start is needed.
 */
status_t ControlThread::processStaticParameters(CameraParameters *oldParams,
        CameraParameters *newParams, bool &restartNeeded)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    float previewAspectRatio = 0.0f;
    float pictureAspectRatio = 0.0f;
    float videoAspectRatio = 0.0f;
    Vector<Size> sizes;
    bool videoMode = android::isParameterSet(CameraParameters::KEY_RECORDING_HINT, *newParams) ? true : false;
    int oldWidth, newWidth;
    int oldHeight, newHeight;
    int previewWidth, previewHeight;
    int pictureWidth, pictureHeight;
    int oldFormat, newFormat;
    // see if preview params have changed
    newParams->getPreviewSize(&newWidth, &newHeight);
    oldParams->getPreviewSize(&oldWidth, &oldHeight);
    newFormat = V4L2Format(newParams->getPreviewFormat());
    oldFormat = V4L2Format(oldParams->getPreviewFormat());
    previewWidth = oldWidth;
    previewHeight = oldHeight;
    previewAspectRatio = 1.0 * newWidth / newHeight;
    if (newWidth != oldWidth || newHeight != oldHeight ||
            oldFormat != newFormat) {
        previewWidth = newWidth;
        previewHeight = newHeight;
        LOG1("Preview size/cb_fourcc is changing: old=%dx%d %s; new=%dx%d %s; ratio=%.3f",
                oldWidth, oldHeight, v4l2Fmt2Str(oldFormat),
                newWidth, newHeight, v4l2Fmt2Str(newFormat),
                previewAspectRatio);
        restartNeeded = true;
        mPreviewForceChanged = false;
    } else {
        LOG1("Preview size/cb_fourcc is unchanged: old=%dx%d %s; ratio=%.3f",
                oldWidth, oldHeight, v4l2Fmt2Str(oldFormat),
                previewAspectRatio);
    }

    newParams->getPictureSize(&pictureWidth, &pictureHeight);
    if (pictureWidth == 0 || pictureHeight == 0) {
        newParams->getSupportedPictureSizes(sizes);
        for (size_t i = 0; i < sizes.size(); i++) {
            pictureAspectRatio = 1.0 * sizes[i].width / sizes[i].height;
            if (fabsf(pictureAspectRatio - previewAspectRatio) <= ASPECT_TOLERANCE) {
                pictureWidth = sizes[i].width;
                pictureHeight = sizes[i].height;
                newParams->setPictureSize(pictureWidth, pictureHeight);
                break;
            }
        }
        ALOGD("Application doesn't set picture size, hal chooses %dx%d to match preview size", pictureWidth, pictureHeight);
    }

    if(videoMode) {
        // see if video params have changed
        newParams->getVideoSize(&newWidth, &newHeight);
        oldParams->getVideoSize(&oldWidth, &oldHeight);
        if (newWidth != oldWidth || newHeight != oldHeight) {
            videoAspectRatio = 1.0 * newWidth / newHeight;
            LOG1("Video size is changing: old=%dx%d; new=%dx%d; ratio=%.3f",
                    oldWidth, oldHeight,
                    newWidth, newHeight,
                    videoAspectRatio);
            restartNeeded = true;
            /*
             *  Camera client requested a new video size, so make sure that requested
             *  video size matches requested preview size. If it does not, then select
             *  a corresponding preview size to match the aspect ratio with video
             *  aspect ratio. Also, the video size must be at least as preview size
             */
            if (fabsf(videoAspectRatio - previewAspectRatio) > ASPECT_TOLERANCE) {
                ALOGW("Requested video (%dx%d) aspect ratio does not match preview \
                     (%dx%d) aspect ratio! The preview will be stretched!",
                        newWidth, newHeight,
                        previewWidth, previewHeight);
            }
        } else {
            videoAspectRatio = 1.0 * oldWidth / oldHeight;
            LOG1("Video size is unchanged: old=%dx%d; ratio=%.3f",
                    oldWidth, oldHeight,
                    videoAspectRatio);
            /*
             *  Camera client did not specify any video size, so make sure that
             *  requested preview size matches our default video size. If it does
             *  not, then select a corresponding video size to match the aspect
             *  ratio with preview aspect ratio.
             */
            if (fabsf(videoAspectRatio - previewAspectRatio) > ASPECT_TOLERANCE
                && !mPreviewForceChanged) {
                LOG1("Our video (%dx%d) aspect ratio does not match preview (%dx%d) aspect ratio!",
                      newWidth, newHeight, previewWidth, previewHeight);
                newParams->getSupportedVideoSizes(sizes);
                for (size_t i = 0; i < sizes.size(); i++) {
                    float thisSizeAspectRatio = 1.0 * sizes[i].width / sizes[i].height;
                    if (fabsf(thisSizeAspectRatio - previewAspectRatio) <= ASPECT_TOLERANCE) {
                        if (sizes[i].width < previewWidth || sizes[i].height < previewHeight) {
                            // This video size is smaller than preview, can't use it
                            continue;
                        }
                        newWidth = sizes[i].width;
                        newHeight = sizes[i].height;
                        LOG1("Forcing video to %dx%d to match preview aspect ratio!", newWidth, newHeight);
                        newParams->setVideoSize(newWidth, newHeight);
                        break;
                    }
                }
            }
        }
    }

    // Burst mode and HDR
    int oldBurstLength = mBurstLength;
    int oldFpsAdaptSkip = mFpsAdaptSkip;
    status = processParamBurst(oldParams, newParams);
    if (status == NO_ERROR) {
      status = processParamHDR(oldParams, newParams);
    }
    if (mBurstLength != oldBurstLength || mFpsAdaptSkip != oldFpsAdaptSkip) {
        LOG1("Burst configuration changed, restarting preview");
        restartNeeded = true;
    }

    status = processParamDvs(oldParams,newParams);

    status = processParamULL(oldParams,newParams, &restartNeeded);

    /*
     *  Process parameter that controls raw data format for snapshot,
     *  this may change the pixel format if raw bayer is selected. In this case
     *  we trigger a preview re-start because Raw capture is only supported
     *  in good-old online mode.
     *
     */
    if (status == NO_ERROR) {
        status = processParamRawDataFormat(oldParams, newParams, restartNeeded);
    }

    /**
     * There are multiple workarounds related to what preview and video
     * size combinations can be supported by ISP (also impacted by
     * sensor configuration).
     *
     * Check the inline documentation for applyISPLimitations()
     * in AtomISP.cpp to see detailed description of the limitations.
     *
     */
    if (mISP->applyISPLimitations(newParams, mDvsEnable, videoMode)) {
        mPreviewForceChanged = true;
        restartNeeded = true;
    }

    // Changing the scene may change many parameters, including
    // flash, awb. Thus the order of how processParamFoo() are
    // called is important for the parameter changes to take
    // effect, and processParamSceneMode needs to be called first.
    if (status == NO_ERROR) {
        // Scene Mode
        status = processParamSceneMode(oldParams, newParams, restartNeeded);
    }

    if (status == NO_ERROR) {
        // xnr/anr
        status = processParamXNR_ANR(oldParams, newParams, restartNeeded);
    }

    return status;
}

/**
 * Update public parameter cache
 *
 * To implement a fast-path for GetParameters HAL call, update
 * a cached copy of parameters every time a modification is done.
 */
status_t ControlThread::updateParameterCache()
{
    status_t status = NO_ERROR;

    mParamCacheLock.lock();

    // let app know if we support zoom in the preview mode indicated
    mISP->getZoomRatios(&mParameters);
    mISP->getFocusDistances(&mParameters);

    String8 params = mParameters.flatten();
    int len = params.length();
    if (mParamCache) {
        free(mParamCache);
        mParamCache = NULL;
    }
    mParamCache = strndup(params.string(), sizeof(char) * len);
    if (!mParamCache) {
        status = NO_MEMORY;
        ALOGE("@%s: strndup failed, len = %d", __FUNCTION__, len);
    }

    mParamCacheLock.unlock();

    return status;
}

/**
 * Save the current context of camera parameters that describe:
 * - picture size
 * - thumbnail size
 * - supported picture sizes
 * - supported thumbnail sizes
 *
 * This is used when we start video recording because we need to impose restric
 * tions on these values to implement video snapshot feature
 * When recording is stopped a reciprocal call to restoreCurrentPictureParams
 * will be done
 */
void ControlThread::storeCurrentPictureParams()
{
    mStillPictContext.clear();

    mParameters.getPictureSize(&mStillPictContext.snapshotWidth,
                               &mStillPictContext.snapshotHeight);
    mStillPictContext.thumbnailWidth = mParameters.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH);
    mStillPictContext.thumbnailHeigth = mParameters.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT);

    const char* supportedSnapshotSizes = mParameters.get(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES);
    if (supportedSnapshotSizes) {
        mStillPictContext.supportedSnapshotSizes = supportedSnapshotSizes;
    } else {
        ALOGE("Missing supported picture sizes");
        mStillPictContext.supportedSnapshotSizes = "";
    }

    const char* supportedThumbnailSizes = mParameters.get(CameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES);
    if (supportedThumbnailSizes) {
        mStillPictContext.suportedThumnailSizes = supportedThumbnailSizes;
    } else {
        ALOGE("Missing supported thumbnail sizes");
        mStillPictContext.suportedThumnailSizes = "";
    }
}

/**
 * Restores from the member variable mStillPictContext the following camera
 * parameters:
 * - picture size
 * - thumbnail size
 * - supported picture sizes
 * - supported thumbnail sizes
 * This is used when video recording stops to restore the state before video
 * recording started and to lift the limitations of the current video snapshot
 */
void ControlThread::restoreCurrentPictureParams()
{
    mParameters.setPictureSize(mStillPictContext.snapshotWidth,
                               mStillPictContext.snapshotHeight);
    mParameters.set(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH,
                    mStillPictContext.thumbnailWidth);
    mParameters.set(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT,
                    mStillPictContext.thumbnailHeigth);

    mParameters.set(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES,
                    mStillPictContext.supportedSnapshotSizes.string());
    mParameters.set(CameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES,
                    mStillPictContext.suportedThumnailSizes.string());

    mStillPictContext.clear();
    updateParameterCache();

    if (mAllocatedSnapshotBuffers.size() == mAvailableSnapshotBuffers.size()) {
        allocateSnapshotAndPostviewBuffers(false);
    } else {
        ALOGW("%s: not safe to allocate now, some snapshot buffers are not returned, skipping", __FUNCTION__);
    }


}

/**
 * Create 3A instance according to sensor type and platform requirement:
 * - AtomAIQ for RAW cameras that use IA AIQ
 * - AtomSoc3A for SoC cameras that have their own 3A
 */
status_t ControlThread::createAtom3A()
{
    status_t status = NO_ERROR;

    if (PlatformData::sensorType(mCameraId) == SENSOR_TYPE_RAW) {
        m3AControls = new AtomAIQ(mHwcg);
    } else {
        m3AControls = new AtomSoc3A(mCameraId, mHwcg);
    }
    if (m3AControls == NULL) {
        ALOGE("error creating AAA");
        status = BAD_VALUE;
    }
    return status;
}

bool ControlThread::paramsHasPictureSizeChanged(const CameraParameters *oldParams,
                                                CameraParameters *newParams) const
{
    int newWidth, newHeight;
    int oldWidth, oldHeight;

    newParams->getPictureSize(&newWidth, &newHeight);
    oldParams->getPictureSize(&oldWidth, &oldHeight);

    if (newWidth != oldWidth || newHeight != oldHeight)
        return true;

    return false;
}

bool ControlThread::hasPictureFormatChanged()
{
    int currentFormat = mISP->getSnapshotPixelFormat();
    int newFormat = V4L2_PIX_FMT_NV12;

    if (CameraDump::isDumpImageEnable(CAMERA_DEBUG_DUMP_RAW))
       newFormat = mHwcg.mSensorCI->getRawFormat();

    return (newFormat != currentFormat);
}

status_t ControlThread::handleMessageSetParameters(MessageSetParameters *msg)
{
    LOG1("@%s", __FUNCTION__);

    status_t status = NO_ERROR;
    CameraParameters newParams;
    CameraParameters oldParams = mParameters;
    CameraParamsLogger newParamLogger (msg->params);
    CameraParamsLogger oldParamLogger (mParameters.flatten().string());
    bool needRestartPreview = false;

    CameraAreas newFocusAreas;
    CameraAreas newMeteringAreas;
    String8 str_params(msg->params);
    newParams.unflatten(str_params);

    bool videoMode = android::isParameterSet(CameraParameters::KEY_RECORDING_HINT, newParams) ? true : false;

    // print all old and new params for comparison (debug)
    LOG1("----------BEGIN PARAM DIFFERENCE----------");
    newParamLogger.dumpDifference(oldParamLogger);
    LOG1("----------END PARAM DIFFERENCE----------");

    LOG2("----------- BEGIN OLD PARAMS -------- ");
    oldParamLogger.dump();
    LOG2("----------- END OLD PARAMS -------- ");

    LOG2("----------- BEGIN NEW PARAMS -------- ");
    newParamLogger.dump();
    LOG2("----------- END NEW PARAMS -------- ");

    status = validateParameters(&newParams);
    if (status != NO_ERROR)
        goto exit;

    if (mCaptureSubState == STATE_CAPTURE_STARTED) {
        ALOGE("setParameters happened during capturing. Changing parameters during capturing would produce "
             "undeterministic results, so postponing the params! Fix your application!");
        Message message;
        message.id = MESSAGE_ID_SET_PARAMETERS;
        message.data.setParameters = *msg;
        message.data.setParameters.params = strdup(msg->params); // need to copy, because mem could already be released when handled
        mPostponedMessages.push_back(message);
        status = NO_ERROR;
        goto exit;
    }

    LOG1("scanning AF focus areas");
    status = newFocusAreas.scan(newParams.get(CameraParameters::KEY_FOCUS_AREAS),
                                m3AControls->getAfMaxNumWindows());
    if (status != NO_ERROR) {
        ALOGE("bad focus area");
        goto exit;
    }
    LOG1("scanning AE metering areas");
    status = newMeteringAreas.scan(newParams.get(CameraParameters::KEY_METERING_AREAS),
                                   m3AControls->getAeMaxNumWindows());
    if (status != NO_ERROR) {
        ALOGE("bad metering area");
        goto exit;
    }

    // Take care of parameters that need to be set while the ISP is stopped
    status = processStaticParameters(&oldParams, &newParams, needRestartPreview);
    if (status != NO_ERROR)
        goto exit;

    if (paramsHasPictureSizeChanged(&oldParams, &newParams)) {
        LOG1("Picture size has changed while camera is active!");

        // get current picture size, update FOV
        int picWidth, picHeight;
        newParams.getPictureSize(&picWidth, &picHeight);
        newParams.setFloat(CameraParameters::KEY_HORIZONTAL_VIEW_ANGLE,
                            PlatformData::horizontalFOV(mCameraId, picWidth, picHeight));
        newParams.setFloat(CameraParameters::KEY_VERTICAL_VIEW_ANGLE,
                            PlatformData::verticalFOV(mCameraId, picWidth, picHeight));

        if (mState == STATE_CAPTURE) {
            status = stopCapture();
        }
        else if (mState == STATE_PREVIEW_STILL ||
                 mState == STATE_CONTINUOUS_CAPTURE) {

            // Preview needs to be restarted if the preview mode changes, or
            // with any picture size change when in continuous mode.
            if (selectPreviewMode(newParams) != mState ||
                mState == STATE_CONTINUOUS_CAPTURE) {
                needRestartPreview = true;
                videoMode = false;
                // cancel picture processing to get all snapshot buffers back to its nest
                cancelPictureThread();
            }
        }

    }

    mParameters = newParams;
    mFocusAreas = newFocusAreas;
    mMeteringAreas = newMeteringAreas;

    /**
     * we need to re-allocate the snapshots in the following scenarios:
     * - if the size has changed
     * - if the pixel format has change (when dumping Raw bayer)
     * - if the number of buffers (burst) have changed
     *
     * If the burst parameters change, a preview restart is triggered.
     * If preview is re-started we will allocate the snapshots
     * after preview has started, not impacting L2P. Here we only handle the
     * first 2 cases
     *
     * In cases where we receive set_params before we start preview we do not allocate
     * not to impact L2P and because application needs to start preview before
     * taking a picture.
     */
    if ((paramsHasPictureSizeChanged(&oldParams, &newParams) || hasPictureFormatChanged())&& mState != STATE_STOPPED) {

        if (mAllocatedSnapshotBuffers.size() == mAvailableSnapshotBuffers.size()) {
            allocateSnapshotAndPostviewBuffers(videoMode);
        } else {
            ALOGW("%s: not safe to allocate now, some snapshot buffers are not returned, skipping", __FUNCTION__);
        }
    }

    ProcessOverlayEnable(&oldParams, &newParams);

    if (needRestartPreview == true) {

        if (msg->stopPreviewRequest) {
            if (mState != STATE_CONTINUOUS_CAPTURE)
                ALOGD("%s: Invalid stopPreviewRequest!", __FUNCTION__);
            status = stopPreviewCore();
            if (status != NO_ERROR)
                return status;
        }
        // if preview is running and preview format has changed, then we need
        // to stop, reconfigure, and restart the isp and all threads.
        // Update the current params before we re-start
        switch (mState) {
            case STATE_PREVIEW_VIDEO:
            case STATE_PREVIEW_STILL:
            case STATE_CONTINUOUS_CAPTURE:
                status = restartPreview(videoMode);
                break;
            case STATE_STOPPED:
                break;
            default:
                ALOGE("formats can only be changed while in preview or stop states");
                break;
        };
    }

    // if file injection is enabled, get file injection parameters and save
    // them in AtomISP
    if (mISP->isFileInjectionEnabled())
        processParamFileInject(&newParams);

    // Take care of parameters that can be set while ISP is running
    status = processDynamicParameters(&oldParams, &newParams);
    if (status != NO_ERROR)
        goto exit;

    mParameters = newParams;
    updateParameterCache();

exit:
    // return status and unblock message sender
    mMessageQueue.reply(MESSAGE_ID_SET_PARAMETERS, status);
    // return status
    return status;
}

status_t ControlThread::handleMessageGetParameters(MessageGetParameters *msg)
{
    status_t status = BAD_VALUE;

    if (msg->params) {
        // let app know if we support zoom in the preview mode indicated
        mISP->getZoomRatios(&mParameters);
        mISP->getFocusDistances(&mParameters);

        String8 params = mParameters.flatten();
        int len = params.length();
        *msg->params = strndup(params.string(), sizeof(char) * len);
        status = NO_ERROR;

    }
    mMessageQueue.reply(MESSAGE_ID_GET_PARAMETERS, status);
    return status;
}
status_t ControlThread::handleMessageCommand(MessageCommand* msg)
{
    status_t status = BAD_VALUE;
    switch (msg->cmd_id)
    {
    case CAMERA_CMD_START_FACE_DETECTION:
        status = startFaceDetection();
        break;
    case CAMERA_CMD_STOP_FACE_DETECTION:
        status = stopFaceDetection();
        break;
    case CAMERA_CMD_START_SCENE_DETECTION:
        status = startSmartSceneDetection();
        break;
    case CAMERA_CMD_STOP_SCENE_DETECTION:
        status = stopSmartSceneDetection();
        break;
    case CAMERA_CMD_START_SMILE_SHUTTER:
        status = startSmartShutter(SMILE_MODE);
        break;
    case CAMERA_CMD_START_BLINK_SHUTTER:
        status = startSmartShutter(BLINK_MODE);
        break;
    case CAMERA_CMD_STOP_SMILE_SHUTTER:
        status = stopSmartShutter(SMILE_MODE);
        break;
    case CAMERA_CMD_STOP_BLINK_SHUTTER:
        status = stopSmartShutter(BLINK_MODE);
        break;
    case CAMERA_CMD_CANCEL_SMART_SHUTTER_PICTURE:
        status = cancelSmartShutterPicture();
        break;
    case CAMERA_CMD_FORCE_SMART_SHUTTER_PICTURE:
        status = forceSmartShutterPicture();
        break;
    case CAMERA_CMD_ENABLE_INTEL_PARAMETERS:
        status = enableIntelParameters();
        mMessageQueue.reply(MESSAGE_ID_COMMAND, status);
        break;
    case CAMERA_CMD_START_PANORAMA:
        status = startPanorama();
        break;
    case CAMERA_CMD_STOP_PANORAMA:
        status = stopPanorama();
        break;
    case CAMERA_CMD_START_FACE_RECOGNITION:
        status = startFaceRecognition();
        break;
    case CAMERA_CMD_STOP_FACE_RECOGNITION:
        status = stopFaceRecognition();
        break;
    case CAMERA_CMD_ENABLE_FOCUS_MOVE_MSG:
        status = enableFocusMoveMsg(static_cast<bool>(msg->arg1));
        break;
    case CAMERA_CMD_ENABLE_ISP_EXTENSION:
        status = enableIspExtensions();
        break;
    case CAMERA_CMD_ACC_LOAD:
        status = mAccManagerThread->load(msg->arg1);
        break;
    case CAMERA_CMD_ACC_ALLOC:
        status = mAccManagerThread->alloc(msg->arg1);
        break;
    case CAMERA_CMD_ACC_FREE:
        status = mAccManagerThread->free(msg->arg1);
        break;
    case CAMERA_CMD_ACC_MAP:
        status = mAccManagerThread->map(msg->arg1);
        break;
    case CAMERA_CMD_ACC_UNMAP:
        status = mAccManagerThread->unmap(msg->arg1);
        break;
    case CAMERA_CMD_ACC_SEND_ARG:
        status = mAccManagerThread->setArgToBeSend(msg->arg1);
        break;
    case CAMERA_CMD_ACC_CONFIGURE_ISP_STANDALONE:
        status = mAccManagerThread->configureIspStandalone(msg->arg1);
        break;
    case CAMERA_CMD_ACC_RETURN_BUFFER:
        status = mAccManagerThread->returnBuffer(msg->arg1);
        break;
    default:
        break;
    }

    if (status != NO_ERROR)
        ALOGE("@%s command id %d failed", __FUNCTION__, msg->cmd_id);
    return status;
}

status_t ControlThread::handleMessageSceneDetected(MessageSceneDetected *msg)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    camera_scene_detection_metadata_t metadata;
    strlcpy(metadata.scene, msg->sceneMode, (size_t)SCENE_STRING_LENGTH);
    metadata.hdr = msg->sceneHdr;
    status = mCallbacksThread->sceneDetected(metadata);
    return status;
}

/**
 * Start Smart scene detection. This should be called after preview is started.
 * The camera will notify Camera.SmartSceneDetectionListener when a new scene
 * is detected.
 */
status_t ControlThread::startSmartSceneDetection()
{
    LOG1("@%s", __FUNCTION__);
    if (mState == STATE_STOPPED || m3AControls->getSmartSceneDetection()) {
        return INVALID_OPERATION;
    }
    enableMsgType(CAMERA_MSG_SCENE_DETECT);
    if (m3AThread != NULL)
        m3AThread->resetSmartSceneValues();
    return m3AControls->setSmartSceneDetection(true);
}

status_t ControlThread::stopSmartSceneDetection()
{
    LOG1("@%s", __FUNCTION__);
    if (mState == STATE_STOPPED || !m3AControls->getSmartSceneDetection()) {
        return INVALID_OPERATION;
    }
    disableMsgType(CAMERA_MSG_SCENE_DETECT);
    return m3AControls->setSmartSceneDetection(false);
}

status_t ControlThread::handleMessageStoreMetaDataInBuffers(MessageStoreMetaDataInBuffers *msg)
{
    LOG1("@%s. state = %d", __FUNCTION__, mState);
    status_t status = NO_ERROR;
    //Prohibit to enable metadata mode if state of HAL isn't equal stopped or in preview
    if (mState != STATE_STOPPED &&
            mState != STATE_PREVIEW_VIDEO &&
            mState != STATE_PREVIEW_STILL &&
            mState != STATE_CONTINUOUS_CAPTURE) {
        ALOGE("Cannot configure metadata buffers in this state: %d", mState);
        status = BAD_VALUE;
        mMessageQueue.reply(MESSAGE_ID_STORE_METADATA_IN_BUFFER, status);
        return status;
    }

    //find the setted buffer sharing session ID
    int sID = mParameters.getInt(IntelCameraParameters::REC_BUFFER_SHARING_SESSION_ID);

    mStoreMetaDataInBuffers = msg->enabled;
    status = mISP->storeMetaDataInBuffers(msg->enabled, sID);
    if(status == NO_ERROR)
        status = mCallbacks->storeMetaDataInBuffers(msg->enabled);
    else
        ALOGE("Error configuring metadatabuffers in ISP!");

    mMessageQueue.reply(MESSAGE_ID_STORE_METADATA_IN_BUFFER, status);
    return status;
}

void ControlThread::postCaptureProcesssingDone(IPostCaptureProcessItem* item, status_t procStatus, int retries)
{
    LOG1("@%s", __FUNCTION__);
    // send message
    Message msg;
    msg.id = MESSAGE_ID_POST_CAPTURE_PROCESSING_DONE;
    msg.data.postCapture.item = item;
    msg.data.postCapture.status = procStatus;
    msg.data.postCapture.retriesLeft = retries;   /* Number of attempts to handle this message */

    mMessageQueue.send(&msg);
}

status_t ControlThread::handleMessagePostCaptureProcessingDone(MessagePostCaptureProcDone *msg)
{
    LOG1("@%s, item = %p status= %d", __FUNCTION__, msg->item, msg->status);
    status_t status;
    AtomBuffer processedBuffer, postviewBuffer;
    PictureThread::MetaData picMetaData;
    int ULLid = 0;

    if(msg->status != NO_ERROR)  {
        ALOGW("PostCapture Processing failed !!");
        goto cleanup;
    }

    if(mCaptureSubState != STATE_CAPTURE_IDLE) {
        // we are in the middle of another capture, let's delay this
        LOG1("Delaying processing of post capture processed image, image capture in progress, CaptureSubState %s", sCaptureSubstateStrings[mCaptureSubState]);
        if (msg->retriesLeft == 0) {
            ALOGE("@%s:Waited too long to handle this message, canceling post capture processing", __FUNCTION__);
            goto cleanup;
        }
        postCaptureProcesssingDone(msg->item, msg->status,msg->retriesLeft--);
        return NO_ERROR;
    }

    // ATM the only post capture processing is ULL, no need to check which one
    status = mULL->getOuputResult(&processedBuffer, &postviewBuffer, &picMetaData, &ULLid);
    if (status != NO_ERROR) {
        /* This can only mean that ULL was cancel, cleanup and go */
        goto cleanup;
    }

    LOG1("CaptureSubState %s -> STARTED (Post-Capture-Proc)", sCaptureSubstateStrings[mCaptureSubState]);
    mCaptureSubState = STATE_CAPTURE_STARTED;
    mCallbacksThread->requestULLPicture(ULLid);

    /*
     * We stop using the postview buffer since it maybe de-allocated
     * this is because we still allocated the postview buffers in the AtomISP
     * which means that if a capture is triggered while ULL was processing
     * the postview will be freed and allocated again
     *
     * processedBuffer was originally with status FRAME_STATUS_SKIPPED to
     * avoid being pushed to mAvailableSnapshotBuffers. Marking status as
     * FRAME_STATUS_OK enables it to be made available again.
     */
    processedBuffer.status = FRAME_STATUS_OK;
    processedBuffer.type = ATOM_BUFFER_ULL;

    status = mPictureThread->encode(picMetaData, &processedBuffer, NULL);
    if (status != NO_ERROR) {
        // normally this is done by PictureThread, but as no
        // encoding was done, free the allocated metadata
        picMetaData.free(m3AControls);
    }

cleanup:
    /**
     * retrieve input buffers from ULL class and return them for re-cycling
     */
    Vector<AtomBuffer> inputs;
    Vector<AtomBuffer> postviews;

    mULL->getInputBuffers(&inputs);
    mULL->getPostviewBuffers(&postviews);

    if (inputs.size() != postviews.size()) {
        // Just to check that we got the right amount of buffers
        ALOGE("%s input buffer (n = %d) and postview buffer count (m = %d) mismatch",
             __FUNCTION__, inputs.size(), postviews.size());
        return UNKNOWN_ERROR;
    }

    Vector<AtomBuffer>::iterator itInput = inputs.begin();
    Vector<AtomBuffer>::iterator itPostview = postviews.begin();
    MessagePicture picMsg;

    // Iterate inputs vector only, the sizes should match as we checked above...
    while (itInput != inputs.end()) {
        picMsg.snapshotBuf = *itInput;
        picMsg.postviewBuf = *itPostview;
        // Recycle the post-processing buffers:
        handleMessagePictureDone(&picMsg);
        ++itInput;
        ++itPostview;
    }

    return NO_ERROR;
}

status_t ControlThread::hdrInit(int pvSize, int pvWidth, int pvHeight)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    // Initialize the HDR output buffers
    // Main output buffer should have same dimensions initially as one of the input
    // buffers, so take those details from the vector of allocated buffers
    if (mAllocatedSnapshotBuffers.isEmpty()) {
        ALOGE("%s:We do not have any snapshotbuffers yet... find the bug",__FUNCTION__);
        return NO_MEMORY;
    }

    int size = mAllocatedSnapshotBuffers[0].size;
    int width = mAllocatedSnapshotBuffers[0].width;
    int bpl = mAllocatedSnapshotBuffers[0].bpl;
    int height = mAllocatedSnapshotBuffers[0].height;
    int fourcc = mAllocatedSnapshotBuffers[0].fourcc;

    mCallbacks->allocateMemory(&mHdr.outMainBuf, size);
    if (mHdr.outMainBuf.dataPtr == NULL) {
        ALOGE("HDR: Error allocating memory for HDR main buffer!");
        return NO_MEMORY;
    }
    mHdr.outMainBuf.shared = false;
    // merging multiple images from ISP, so just set counter to 1
    mHdr.outMainBuf.frameCounter = 1;
    mHdr.outMainBuf.type = ATOM_BUFFER_SNAPSHOT;

    LOG1("HDR: using %p as HDR main output buffer", mHdr.outMainBuf.dataPtr);
    // Postview output buffer
    mCallbacks->allocateMemory(&mHdr.outPostviewBuf, pvSize);
    if (mHdr.outPostviewBuf.dataPtr == NULL) {
        ALOGE("HDR: Error allocating memory for HDR postview buffer!");
        return NO_MEMORY;
    }
    mHdr.outPostviewBuf.shared = false;
    mHdr.outPostviewBuf.type = ATOM_BUFFER_POSTVIEW;

    LOG1("HDR: using %p as HDR postview output buffer", mHdr.outPostviewBuf.dataPtr);

    // Initialize the input buffers store
    mHdr.inputBuffers = new MessagePicture[mHdr.bracketNum];
    memset(mHdr.inputBuffers, 0, mHdr.bracketNum * sizeof(MessagePicture));

    // Initialize the CI input buffers (will be initialized later, when snapshots are taken)
    mHdr.ciBufIn.ciBufNum = mHdr.bracketNum;
    mHdr.ciBufIn.ciMainBuf = new ia_frame[mHdr.ciBufIn.ciBufNum];
    mHdr.ciBufIn.ciPostviewBuf = new ia_frame[mHdr.ciBufIn.ciBufNum];
    mHdr.ciBufIn.hist = new ia_cp_histogram[mHdr.ciBufIn.ciBufNum];

    // Initialize the CI output buffers
    mHdr.ciBufOut.ciBufNum = mHdr.bracketNum;
    mHdr.ciBufOut.ciMainBuf = new ia_frame[1];
    mHdr.ciBufOut.ciPostviewBuf = new ia_frame[1];
    mHdr.ciBufOut.hist = NULL;

    if (mHdr.ciBufIn.ciMainBuf == NULL ||
        mHdr.ciBufIn.ciPostviewBuf == NULL ||
        mHdr.ciBufIn.hist == NULL ||
        mHdr.ciBufOut.ciMainBuf == NULL ||
        mHdr.ciBufOut.ciPostviewBuf == NULL) {
        ALOGE("HDR: Error allocating memory for HDR CI buffers!");
        return NO_MEMORY;
    }

    status = AtomCP::setIaFrameFormat(&mHdr.ciBufOut.ciMainBuf[0], fourcc);
    if (status != NO_ERROR) {
        ALOGE("HDR: pixel format %d not supported", fourcc);
        return status;
    }

    mHdr.ciBufOut.ciMainBuf->data = mHdr.outMainBuf.dataPtr;
    mHdr.ciBufOut.ciMainBuf[0].width = mHdr.outMainBuf.width = width;
    mHdr.ciBufOut.ciMainBuf[0].stride = mHdr.outMainBuf.bpl = bpl;
    mHdr.ciBufOut.ciMainBuf[0].height = mHdr.outMainBuf.height = height;
    mHdr.outMainBuf.fourcc = fourcc;
    mHdr.ciBufOut.ciMainBuf[0].size = mHdr.outMainBuf.size = size;

    LOG1("HDR: Initialized output CI main     buff @%p: (data=%p, size=%d, width=%d, height=%d, fourcc=%d)",
            &mHdr.ciBufOut.ciMainBuf[0],
            mHdr.ciBufOut.ciMainBuf[0].data,
            mHdr.ciBufOut.ciMainBuf[0].size,
            mHdr.ciBufOut.ciMainBuf[0].width,
            mHdr.ciBufOut.ciMainBuf[0].height,
            mHdr.ciBufOut.ciMainBuf[0].format);

    mHdr.ciBufOut.ciPostviewBuf[0].data = mHdr.outPostviewBuf.dataPtr;
    mHdr.ciBufOut.ciPostviewBuf[0].width = mHdr.outPostviewBuf.width = pvWidth;
    mHdr.ciBufOut.ciPostviewBuf[0].stride = mHdr.outPostviewBuf.bpl = pvWidth;
    mHdr.ciBufOut.ciPostviewBuf[0].height = mHdr.outPostviewBuf.height = pvHeight;
    AtomCP::setIaFrameFormat(&mHdr.ciBufOut.ciPostviewBuf[0], fourcc);
    mHdr.outPostviewBuf.fourcc = fourcc;
    mHdr.ciBufOut.ciPostviewBuf[0].size = mHdr.outPostviewBuf.size = pvSize;

    LOG1("HDR: Initialized output CI postview buff @%p: (data=%p, size=%d, width=%d, height=%d, fourcc=%d)",
            &mHdr.ciBufOut.ciPostviewBuf[0],
            mHdr.ciBufOut.ciPostviewBuf[0].data,
            mHdr.ciBufOut.ciPostviewBuf[0].size,
            mHdr.ciBufOut.ciPostviewBuf[0].width,
            mHdr.ciBufOut.ciPostviewBuf[0].height,
            mHdr.ciBufOut.ciPostviewBuf[0].format);

    mHdr.inProgress = true;

    return status;
}

status_t ControlThread::hdrProcess(AtomBuffer * snapshotBuffer, AtomBuffer* postviewBuffer)
{
    LOG1("@%s", __FUNCTION__);

    // Initialize the HDR CI input buffers (main/postview) for this capture
    mHdr.ciBufIn.ciMainBuf[mBurstCaptureNum].data = snapshotBuffer->dataPtr;
    mHdr.ciBufIn.ciMainBuf[mBurstCaptureNum].width = snapshotBuffer->width;
    mHdr.ciBufIn.ciMainBuf[mBurstCaptureNum].stride = snapshotBuffer->bpl;
    mHdr.ciBufIn.ciMainBuf[mBurstCaptureNum].height = snapshotBuffer->height;
    mHdr.ciBufIn.ciMainBuf[mBurstCaptureNum].size = snapshotBuffer->size;
    AtomCP::setIaFrameFormat(&mHdr.ciBufIn.ciMainBuf[mBurstCaptureNum], snapshotBuffer->fourcc);

    LOG1("HDR: Initialized input CI main     buff %d @%p: (addr=%p, length=%d, width=%d, height=%d, fourcc=%d)",
            mBurstCaptureNum,
            &mHdr.ciBufIn.ciMainBuf[mBurstCaptureNum],
            mHdr.ciBufIn.ciMainBuf[mBurstCaptureNum].data,
            mHdr.ciBufIn.ciMainBuf[mBurstCaptureNum].size,
            mHdr.ciBufIn.ciMainBuf[mBurstCaptureNum].width,
            mHdr.ciBufIn.ciMainBuf[mBurstCaptureNum].height,
            mHdr.ciBufIn.ciMainBuf[mBurstCaptureNum].format);

    mHdr.ciBufIn.ciPostviewBuf[mBurstCaptureNum].data = postviewBuffer->dataPtr;
    mHdr.ciBufIn.ciPostviewBuf[mBurstCaptureNum].width = postviewBuffer->width;
    mHdr.ciBufIn.ciPostviewBuf[mBurstCaptureNum].stride = postviewBuffer->bpl;
    mHdr.ciBufIn.ciPostviewBuf[mBurstCaptureNum].height = postviewBuffer->height;
    mHdr.ciBufIn.ciPostviewBuf[mBurstCaptureNum].size = postviewBuffer->size;
    AtomCP::setIaFrameFormat(&mHdr.ciBufIn.ciPostviewBuf[mBurstCaptureNum], postviewBuffer->fourcc);

    LOG1("HDR: Initialized input CI postview buff %d @%p: (addr=%p, length=%d, width=%d, height=%d, fourcc=%d)",
            mBurstCaptureNum,
            &mHdr.ciBufIn.ciPostviewBuf[mBurstCaptureNum],
            mHdr.ciBufIn.ciPostviewBuf[mBurstCaptureNum].data,
            mHdr.ciBufIn.ciPostviewBuf[mBurstCaptureNum].size,
            mHdr.ciBufIn.ciPostviewBuf[mBurstCaptureNum].width,
            mHdr.ciBufIn.ciPostviewBuf[mBurstCaptureNum].height,
            mHdr.ciBufIn.ciPostviewBuf[mBurstCaptureNum].format);

    mHdr.inputBuffers[mBurstCaptureNum].snapshotBuf = *snapshotBuffer;
    mHdr.inputBuffers[mBurstCaptureNum].postviewBuf = *postviewBuffer;

    return NO_ERROR;
}

void ControlThread::hdrRelease()
{
    // Deallocate memory
    MemoryUtils::freeAtomBuffer(mHdr.outMainBuf);
    MemoryUtils::freeAtomBuffer(mHdr.outPostviewBuf);
    if (mHdr.ciBufIn.ciMainBuf != NULL) {
        delete[] mHdr.ciBufIn.ciMainBuf;
        mHdr.ciBufIn.ciMainBuf = NULL;
    }
    if (mHdr.ciBufIn.ciPostviewBuf != NULL) {
        delete[] mHdr.ciBufIn.ciPostviewBuf;
        mHdr.ciBufIn.ciPostviewBuf = NULL;
    }
    if (mHdr.ciBufIn.hist != NULL) {
        delete[] mHdr.ciBufIn.hist;
        mHdr.ciBufIn.hist = NULL;
    }
    if (mHdr.ciBufOut.ciMainBuf != NULL) {
        delete[] mHdr.ciBufOut.ciMainBuf;
        mHdr.ciBufOut.ciMainBuf = NULL;
    }
    if (mHdr.ciBufOut.ciPostviewBuf != NULL) {
        delete[] mHdr.ciBufOut.ciPostviewBuf;
        mHdr.ciBufOut.ciPostviewBuf = NULL;
    }
    if (mHdr.inputBuffers != NULL) {
        delete[] mHdr.inputBuffers;
        mHdr.inputBuffers = NULL;
    }
    mHdr.inProgress = false;
}

status_t ControlThread::hdrCompose()
{
    LOG1("%s",__FUNCTION__);
    status_t status = NO_ERROR;
    ia_aiq_gbce_results gbce_results;

    // initialize the meta data with last picture of
    // the HDR sequence
    PictureThread::MetaData hdrPicMetaData;
    fillPicMetaData(hdrPicMetaData, false);

    // Collect the GBCE results if Intel 3A is available
    if (m3AControls->isIntel3A()) {
        status = m3AControls->getGBCEResults(&gbce_results);
        if (status != NO_ERROR) {
            hdrPicMetaData.free(m3AControls);
            ALOGE("Error collecting the GBCE results!");
            return status;
        }
    }

    /*
     * Stop ISP before composing HDR since standalone acceleration requires ISP to be stopped.
     * The below call won't release the capture buffers since they are needed by HDR compose
     * method. The capture buffers will be released in stopCapture method.
     */
    status = mISP->stop();
    if (status != NO_ERROR) {
        hdrPicMetaData.free(m3AControls);
        ALOGE("Error stopping ISP!");
        return status;
    }

    if (status != NO_ERROR) {
        hdrPicMetaData.free(m3AControls);
        ALOGE("HDR buffer allocation failed");
        return UNKNOWN_ERROR;
    }

    bool doEncode = false;
    status = mCP->composeHDR(mHdr.ciBufIn, mHdr.ciBufOut, gbce_results);
    if (status == NO_ERROR) {
        mHdr.outMainBuf.width = mHdr.ciBufOut.ciMainBuf->width;
        mHdr.outMainBuf.height = mHdr.ciBufOut.ciMainBuf->height;
        mHdr.outMainBuf.size = mHdr.ciBufOut.ciMainBuf->size;
        if (hdrPicMetaData.aeConfig) {
            hdrPicMetaData.aeConfig->evBias = 0.0;
        }

        // recycle HDR input buffers
        for (int i = 0; i < mHdr.bracketNum; i++) {
            if (mHdr.inputBuffers[i].snapshotBuf.dataPtr != NULL) {
                handleMessagePictureDone(&mHdr.inputBuffers[i]);
                mHdr.inputBuffers[i].snapshotBuf.dataPtr = NULL;
            }
        }

        // The output frame is allocated by the HDR module so it is not one of the
        // snapshot buffers allocated by the PictureThread. We mark this in the
        // status field as frame skipped. This field is only checked by the
        // logic in handleMessagePictureDone(), so we make sure this frame is not added to the
        // pool of mAvailableSnapshotBuffers
        mHdr.outMainBuf.status = FRAME_STATUS_SKIPPED;
        mHdr.outPostviewBuf.status = FRAME_STATUS_SKIPPED;
        status = mPictureThread->encode(hdrPicMetaData, &mHdr.outMainBuf, &mHdr.outPostviewBuf);
        if (status == NO_ERROR) {
            doEncode = true;
        }
    } else {
        ALOGE("HDR Composition failed !");
    }

    if (doEncode == false)
        hdrPicMetaData.free(m3AControls);

    /**
     * TODO: to have a cleaner buffer recycle we should return the snapshot buffers
     * to the pool of available buffers. This is not done here, but it works
     * because we reset the available buffer list with all allocated buffers
     * in StopCapture
     */
    return status;
}

/*
 * Helper method used during the takePicture sequences
 *
 * It  passes the  buffers allocated asynchronously by PictureThread to the AtomISP
 * prior device initialization
 *
 * The allocation in the picture thread is triggered also by the Control Thread
 * \sa allocateSnapshotBuffers()
 *
 * In this method we check whether we have enough available buffers to satisfy
 * the request.
 * If we do not have enough available but there are enough allocated it means
 * snapshot buffers are being held somewhere else, this is an indication of a bug
 *
 *
 * The input parameters are at the moment mostly for double checking. It is
 * assumed that the allocatedSnapshotbuffers was previously called with the correct
 * resolution and format
 *
 * \param [in] format V4L2 color space format of the frame (not used, see TODO)
 * \param [in] width width in pixels
 * \param [in] height height in lines
 */
void ControlThread::setExternalSnapshotBuffers(int fourcc, int width, int height)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    /**
     * Bracketing needs more buffers than burst.
     * so we make a difference between them
     */
    int recommendedNum = ((mBracketManager->getBracketMode() != BRACKET_NONE)?
        PlatformData::getMaxNumYUVBufferForBracket(mCameraId) : PlatformData::getMaxNumYUVBufferForBurst(mCameraId));

    int clipTo = MAX(recommendedNum, (mISP->getContinuousCaptureNumber()+1));
    unsigned int bufNeeded = CLIP(MAX(mBurstLength, mISP->getContinuousCaptureNumber()+1), clipTo, 1);

    if (mAllocatedSnapshotBuffers.size() == mAvailableSnapshotBuffers.size()) {
        // allocatedSnapshotbuffers will decide if really need to allocate
        allocateSnapshotAndPostviewBuffers(false);
    } else {
        ALOGW("%s: not safe to allocate now, some snapshot buffers are not returned, skipping", __FUNCTION__);
    }

    unsigned int numberOfSnapshots = MAX(1,mBurstLength);
    unsigned int numToSet = MIN(numberOfSnapshots, mAvailableSnapshotBuffers.size());
    if (numToSet < numberOfSnapshots)
        mBurstBufsToReturn = numberOfSnapshots - numToSet;
    LOG1("Number of snapshots %d: Buffers needed:%d numToSet:%d To be returned:%d Available %d Allocated: %d ",
            numberOfSnapshots, bufNeeded, numToSet, mBurstBufsToReturn,
            mAvailableSnapshotBuffers.size(), mAllocatedSnapshotBuffers.size());

    /**
     * Here size of mAvailableSnapshotBuffers maybe < mAvailableSnapshotBuffers
     * In case that some buffers are still in process. But if only available buffer
     * is enough to use, go ahead.
     * For Example:
     *  ULL needs 3 buffers, it will take a long time to process and here the number of
     *  available buffer maybe only 1. But it's ok for ZSL shooting to use.
     */
    if (mAvailableSnapshotBuffers.size() >= numToSet) {
        if ((mAllocatedSnapshotBuffers[0].width != width) ||
            (mAllocatedSnapshotBuffers[0].height != height)) {
            ALOGE("We got allocated snapshot buffers of wrong resolution (%dx%d), "
                  "this should not happen!! we wanted (%dx%d)",
                  mAllocatedSnapshotBuffers[0].width,
                  mAllocatedSnapshotBuffers[0].height,
                  width, height);
        }
        bool cached = false;
        status = mISP->setSnapshotBuffers(&mAvailableSnapshotBuffers, numToSet, cached);
        status = mISP->setPostviewBuffers(&mAvailablePostviewBuffers, numToSet, cached);
    } else {
        /**
         * The places when we allocate snapshot buffers should ensure that at take
         * picture time there are enough buffers. This situation may arise if
         * not enough buffers were allocated, and some buffers are in used
         * ControlThread needs to ensure it allocates enough.
         * It is not possible to re-allocate now if we do not have all the
         * snapshot buffers back in mAvailableSnapshotBuffers.
         */
        ALOGE("Not enough available buffers for this request. This should not happen");
    }
}

/**
 * From Android API:
 * Starts the face detection. This should be called after preview is started.
 * The camera will notify Camera.FaceDetectionListener
 *  of the detected faces in the preview frame. The detected faces may be the same as
 *  the previous ones.
 *
 *  Applications should call stopFaceDetection() to stop the face detection.
 *
 *  This method is supported if getMaxNumDetectedFaces() returns a number larger than 0.
 *  If the face detection has started, apps should not call this again.
 *  When the face detection is running, setWhiteBalance(String), setFocusAreas(List),
 *  and setMeteringAreas(List) have no effect.
 *  The camera uses the detected faces to do auto-white balance, auto exposure, and autofocus.
 *
 *  If the apps call autoFocus(AutoFocusCallback), the camera will stop sending face callbacks.
 *
 *  The last face callback indicates the areas used to do autofocus.
 *  After focus completes, face detection will resume sending face callbacks.
 *
 *  If the apps call cancelAutoFocus(), the face callbacks will also resume.
 *
 *  After calling takePicture(Camera.ShutterCallback, Camera.PictureCallback, Camera.PictureCallback)
 *  or stopPreview(), and then resuming preview with startPreview(),
 *  the apps should call this method again to resume face detection.
 *
 */
status_t ControlThread::startFaceDetection()
{
    LOG2("@%s", __FUNCTION__);
    // Check the camera.hal.power property if disable FDFR
    if (gPowerLevel & CAMERA_POWERBREAKDOWN_DISABLE_FDFR) {
        return NO_ERROR;
    }

    if (mState == STATE_STOPPED || mFaceDetectionActive) {
        ALOGE("starting FD in stop state");
        return INVALID_OPERATION;
    }
    if (mPostProcThread != 0) {
        mPostProcThread->startFaceDetection();
        mFaceDetectionActive = true;
        enableMsgType(CAMERA_MSG_PREVIEW_METADATA);
        return NO_ERROR;
    } else{
        return INVALID_OPERATION;
    }
}

status_t ControlThread::stopFaceDetection(bool wait)
{
    LOG2("@%s", __FUNCTION__);
    if(!mFaceDetectionActive) {
        return NO_ERROR;
    }

    mFaceDetectionActive = false;
    disableMsgType(CAMERA_MSG_PREVIEW_METADATA);
    if (mPostProcThread != 0) {
        mPostProcThread->stopFaceDetection(wait);
        return NO_ERROR;
    } else {
        return INVALID_OPERATION;
    }
}

status_t ControlThread::startSmartShutter(SmartShutterMode mode)
{
    LOG1("@%s", __FUNCTION__);
    if (mState == STATE_STOPPED)
        return INVALID_OPERATION;

    int level = 0;

    if (mode == SMILE_MODE && !mPostProcThread->isSmileRunning()) {
        level = mParameters.getInt(IntelCameraParameters::KEY_SMILE_SHUTTER_THRESHOLD);
    } else if (mode == BLINK_MODE && !mPostProcThread->isBlinkRunning()) {
        level = mParameters.getInt(IntelCameraParameters::KEY_BLINK_SHUTTER_THRESHOLD);
    } else {
        return INVALID_OPERATION;
    }

    mPostProcThread->startSmartShutter(mode, level);
    LOG1("%s: mode: %d Active Mode: (smile %d (%d) , blink %d (%d), smart %d)",
         __FUNCTION__, mode,
         mPostProcThread->isSmileRunning(), mPostProcThread->getSmileThreshold(),
         mPostProcThread->isBlinkRunning(), mPostProcThread->getBlinkThreshold(),
         mPostProcThread->isSmartRunning());

    return NO_ERROR;
}

status_t ControlThread::stopSmartShutter(SmartShutterMode mode)
{
    LOG1("@%s", __FUNCTION__);

    mPostProcThread->stopSmartShutter(mode);
    LOG1("%s: mode: %d Active Mode: (smile %d (%d) , blink %d (%d), smart %d)",
         __FUNCTION__, mode,
         mPostProcThread->isSmileRunning(), mPostProcThread->getSmileThreshold(),
         mPostProcThread->isBlinkRunning(), mPostProcThread->getBlinkThreshold(),
         mPostProcThread->isSmartRunning());

    return NO_ERROR;
}

status_t ControlThread::startFaceRecognition()
{
    LOG1("@%s", __FUNCTION__);
    if (mPostProcThread->isFaceRecognitionRunning()) {
        ALOGW("@%s: face recognition already started", __FUNCTION__);
        return NO_ERROR;
    }
    mPostProcThread->startFaceRecognition();
    return NO_ERROR;
}

status_t ControlThread::stopFaceRecognition()
{
    LOG1("@%s", __FUNCTION__);
    if (mPostProcThread->isFaceRecognitionRunning()) {
        mPostProcThread->stopFaceRecognition();
    }
    return NO_ERROR;
}

status_t ControlThread::enableFocusMoveMsg(bool enable)
{
    LOG1("@%s", __FUNCTION__);
    if (enable) {
        enableMsgType(CAMERA_MSG_FOCUS_MOVE);
    } else {
        disableMsgType(CAMERA_MSG_FOCUS_MOVE);
    }

    return NO_ERROR;
}

status_t ControlThread::enableIntelParameters()
{
    // intel parameters support more effects
    // so use supported effects list stored in mIntelParameters.
    if (mIntelParameters.get(CameraParameters::KEY_SUPPORTED_EFFECTS))
        mParameters.remove(CameraParameters::KEY_SUPPORTED_EFFECTS);

    String8 params(mParameters.flatten());
    String8 intel_params(mIntelParameters.flatten());
    String8 delimiter(";");
    params += delimiter;
    params += intel_params;
    mParameters.unflatten(params);
    updateParameterCache();

    mIntelParamsAllowed = true;
    return NO_ERROR;
}

status_t ControlThread::cancelSmartShutterPicture()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    if(mPostProcThread !=0 && mPostProcThread->isSmartRunning())
        mPostProcThread->stopCaptureOnTrigger();
    return status;
}

status_t ControlThread::forceSmartShutterPicture()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    if(mPostProcThread !=0 && mPostProcThread->isSmartRunning())
        mPostProcThread->forceSmartCaptureTrigger();
    return status;
}

status_t ControlThread::startPanorama()
{
    LOG1("@%s", __FUNCTION__);

    if (mPanoramaThread != 0) {
        if (mPanoramaThread->getState() != PANORAMA_STOPPED) {
            return INVALID_OPERATION;
        }

        mPanoramaThread->startPanorama();

        // in continuous capture mode, check if postview size matches live preview size.
        // if not, restart preview so that pv size gets set to lpv size
        if (mState == STATE_CONTINUOUS_CAPTURE) {
            int lpwWidth, lpwHeight;
            IntelCameraParameters::getPanoramaLivePreviewSize(lpwWidth, lpwHeight, mParameters);
            AtomBuffer formatDescriptor = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_FORMAT_DESCRIPTOR);
            mISP->getPostviewFrameFormat(formatDescriptor);

            if (lpwWidth != formatDescriptor.width || lpwHeight != formatDescriptor.height
                || formatDescriptor.fourcc != V4L2_PIX_FMT_NV21)
                restartPreview(false);
        }

        return NO_ERROR;
    } else {
        return INVALID_OPERATION;
    }
}

status_t ControlThread::stopPanorama()
{
    LOG1("@%s", __FUNCTION__);

    if (mPanoramaThread != 0) {
        if (mPanoramaThread->getState() == PANORAMA_STOPPED)
            return NO_ERROR;

        // Panorama stop released panorama engine memory. Before stop flush
        // the picture thread so that it is done with panorama engine memory.
        mPictureThread->flushBuffers();

        // now we can stop the panorama engine, which releases its memory.
        mPanoramaThread->stopPanorama(true); // synchronous call

        // Remove for the finalization message which may have arrived during this
        // function. The finalization message includes pointers to released memory.
        mMessageQueue.remove(MESSAGE_ID_PANORAMA_FINALIZE); // drop message

        return NO_ERROR;
    } else {
        return INVALID_OPERATION;
    }
}

status_t ControlThread::enableIspExtensions()
{
    LOG2("@%s", __FUNCTION__);
    if (mState != STATE_STOPPED) {
        ALOGE("Must enable ISP extensions in stop state");
        return INVALID_OPERATION;
    }
    if (mIspExtensionsEnabled) {
        ALOGD("ISP extensions already enabled");
        return NO_ERROR;
    }
    if (mAccManagerThread != NULL) {
        mIspExtensionsEnabled = true;
        return NO_ERROR;
    } else {
        return INVALID_OPERATION;
    }
}

status_t ControlThread::waitForAndExecuteMessage()
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    Message msg;
    // Note: MessageQueue::receive overrides msg in case of new message.
    // If no messages, we timeout in 5s and execute the timeout handler
    msg.id = MESSAGE_ID_TIMEOUT;
    status = mMessageQueue.receive(&msg, MESSAGE_QUEUE_RECEIVE_TIMEOUT_MSEC);

    switch (msg.id) {

        case MESSAGE_ID_EXIT:
            status = handleMessageExit(&msg.data.exit);
            break;

        case MESSAGE_ID_RETURN_BUFFER:
            status = handleMessageReturnBuffer(&msg.data.returnBuf);
            break;

        case MESSAGE_ID_START_PREVIEW:
            status = handleMessageStartPreview();
            break;

        case MESSAGE_ID_STOP_PREVIEW:
            status = handleMessageStopPreview();
            break;

        case MESSAGE_ID_ERROR_PREVIEW:
            status = handleMessageErrorPreview();
            break;

        case MESSAGE_ID_START_RECORDING:
            status = handleMessageStartRecording();
            break;

        case MESSAGE_ID_STOP_RECORDING:
            status = handleMessageStopRecording();
            break;

        case MESSAGE_ID_PANORAMA_PICTURE:
            status = handleMessagePanoramaPicture();
            break;

        case MESSAGE_ID_TAKE_PICTURE:
            status = handleMessageTakePicture();
            break;

        case MESSAGE_ID_SMART_SHUTTER_PICTURE:
                status = handleMessageTakeSmartShutterPicture();
            break;

        case MESSAGE_ID_CANCEL_PICTURE:
            status = handleMessageCancelPicture();
            break;

        case MESSAGE_ID_AUTO_FOCUS:
            status = handleMessageAutoFocus();
            break;

        case MESSAGE_ID_CANCEL_AUTO_FOCUS:
            status = handleMessageCancelAutoFocus();
            break;

        case MESSAGE_ID_RELEASE_RECORDING_FRAME:
            status = handleMessageReleaseRecordingFrame(&msg.data.releaseRecordingFrame);
            break;

        case MESSAGE_ID_PREVIEW_STARTED:
            status = handleMessagePreviewStarted();
            break;

        case MESSAGE_ID_ENCODING_DONE:
            status = handleMessageEncodingDone(&msg.data.encodingDone);
            break;

        case MESSAGE_ID_PICTURE_DONE:
            status = handleMessagePictureDone(&msg.data.pictureDone);
            break;

        case MESSAGE_ID_SET_PARAMETERS:
            status = handleMessageSetParameters(&msg.data.setParameters);
            break;

        case MESSAGE_ID_GET_PARAMETERS:
            status = handleMessageGetParameters(&msg.data.getParameters);
            break;

        case MESSAGE_ID_COMMAND:
            status = handleMessageCommand(&msg.data.command);
            break;

        case MESSAGE_ID_SET_PREVIEW_WINDOW:
            status = handleMessageSetPreviewWindow(&msg.data.previewWin);
            break;

        case MESSAGE_ID_STORE_METADATA_IN_BUFFER:
            status = handleMessageStoreMetaDataInBuffers(&msg.data.storeMetaDataInBuffers);
            break;

        case MESSAGE_ID_SCENE_DETECTED:
            status = handleMessageSceneDetected(&msg.data.sceneDetected);
            break;

        case MESSAGE_ID_PANORAMA_CAPTURE_TRIGGER:
            status = handleMessagePanoramaCaptureTrigger();
            break;

        case MESSAGE_ID_POST_PROC_CAPTURE_TRIGGER:
            status = handleMessageTakePicture();
            // in Smart Shutter with HDR, we need to reset the flag in case no save original
            // to have a clean flag for new capture sequence.
            if (!mHdr.enabled || !mHdr.saveOrig)
                mPostProcThread->resetSmartCaptureTrigger();
            break;

        case MESSAGE_ID_PANORAMA_FINALIZE:
             status = handleMessagePanoramaFinalize(&msg.data.panoramaFinalized);
             break;

        case MESSAGE_ID_DEQUEUE_RECORDING:
            status = dequeueRecording(&msg.data.dequeueRecording);
            break;
        case MESSAGE_ID_RELEASE:
            status = handleMessageRelease();
            break;

        case MESSAGE_ID_TIMEOUT:
            status = handleMessageTimeout();
            break;

        case MESSAGE_ID_POST_CAPTURE_PROCESSING_DONE:
            status = handleMessagePostCaptureProcessingDone(&msg.data.postCapture);
            break;

        case MESSAGE_ID_SET_ORIENTATION:
            status = handleMessageSetOrientation(&msg.data.orientation);
            break;

        default:
            ALOGE("Invalid message");
            status = BAD_VALUE;
            break;
    };

    if (status != NO_ERROR)
        ALOGE("Error handling message: %d", (int) msg.id);
    return status;
}

AtomBuffer* ControlThread::findRecordingBuffer(void *ptr)
{
    Vector<AtomBuffer>::iterator it = mRecordingBuffers.begin();
    for (;it != mRecordingBuffers.end(); ++it) {
        if(mStoreMetaDataInBuffers) {
            if (it->metadata_buff->data == ptr)
                return it;
        } else {
             if (it->dataPtr == ptr)
                return it;
        }
    }

    return NULL;
}

/**
 * Override function for IBufferOwner
 *
 * Note: currently used only for preview
 */
void ControlThread::returnBuffer(AtomBuffer* buff)
{
    // NOTE: it is important that this is done through a message, both
    // for obvious thread safety reasons and also for synchronization purposes
    LOG2("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_RETURN_BUFFER;
    msg.data.returnBuf.returnBuf = *buff;
    mMessageQueue.send(&msg);
}

status_t ControlThread::handleMessageReturnBuffer(MessageReturnBuffer *msg)
{
    LOG2("@%s", __FUNCTION__);

    // thanks to the observer ordering (control thread first,
    // preview thread after it) this message will be handled after the
    // recording dequeue message which makes the copy
    msg->returnBuf.owner->returnBuffer(&msg->returnBuf);
    return OK;
}

/**
 * override for IAtomIspObserver::atomIspNotify()
 *
 * ControlThread is attached to receive preview stream notifications
 * to handle dequeueing of recording frames in video mode.
 * NOTE: not touching Preview buffer here and ignoring state changes
 */
bool ControlThread::atomIspNotify(IAtomIspObserver::Message *msg, const ObserverState state)
{
    LOG2("@%s", __FUNCTION__);

    if (msg) {
        AtomBuffer *buff = &msg->data.frameBuffer.buff;
        if (msg->id != IAtomIspObserver::MESSAGE_ID_FRAME) {
            LOG1("Received unexpected notify message id %d!", msg->id);
            if (msg->id == IAtomIspObserver::MESSAGE_ID_ERROR) {
                ALOGE("Error in preview stream");
                errorPreview();
            }
            return false;
        }

        if (mISP->getMode() == MODE_VIDEO) {
            // steal the owner, if vfpp has no time for processing - in that
            // case the preview will be used for creating the recording content,
            // and we need to steal the ownership to ensure the dequeue
            // recording message is always handled before the preview buffer is
            // returned to the ISP
            if (mISP->getPreviewTooBigForVFPP())
                buff->owner = this;

            bool skipFrame = mISP->checkSkipFrameRecording(msg->data.frameBuffer.buff.frameCounter);

            Message local_msg;
            local_msg.id = MESSAGE_ID_DEQUEUE_RECORDING;
            local_msg.data.dequeueRecording.previewFrame = *buff;
            local_msg.data.dequeueRecording.skipFrame =
               (buff->status == FRAME_STATUS_CORRUPTED) || skipFrame;
            mMessageQueue.send(&local_msg);
        }
    }
    return false;
}

status_t ControlThread::dequeueRecording(MessageDequeueRecording *msg)
{
    LOG2("@%s", __FUNCTION__);
    AtomBuffer buff = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_VIDEO);

    status_t status = NO_ERROR;

    // after ISP timeout, we will get a burst of notifications without really that
    // many recording buffers, so we need to skip the unnecessary notifications
    status = mISP->getRecordingFrame(&buff);
    if (status == NOT_ENOUGH_DATA) {
        ALOGW("@%s - recording frame was not ready. Maybe there was an ISP timeout?", __FUNCTION__);
        return NO_ERROR;
    }

    if (status == NO_ERROR) {
       if (buff.status != FRAME_STATUS_CORRUPTED) {
            // Check whether driver has run out of buffers
            if (!mISP->dataAvailable()) {
                ALOGE("Video frame dropped, buffers reserved : %d video encoder, %d video snapshot",
                        mRecordingBuffers.size(), mVideoSnapshotBuffers.size());
                msg->skipFrame = true;
            }
            // See if recording has started (state).
            // If it has, process the buffer, unless frame is to be dropped.
            // If recording hasn't started or frame is dropped, return the buffer to the driver
            if (mState == STATE_RECORDING && !msg->skipFrame) {
                // check recording
                if (mISP->getPreviewTooBigForVFPP()) {
                    memcpy(buff.dataPtr, msg->previewFrame.dataPtr, msg->previewFrame.size);
                }

                // Mirror the recording buffer if mirroring is enabled (only for front camera)
                // TODO: this should be moved into VideoThread
                if (mSaveMirrored && PlatformData::cameraFacing(mCameraId) == CAMERA_FACING_FRONT) {
                    mirrorBuffer(&buff, mRecordingOrientation, PlatformData::cameraOrientation(mCameraId));
                }

                if (mVideoSnapshotrequested && mVideoSnapshotBuffers.size() < 3) {
                    mVideoSnapshotrequested--;
                    encodeVideoSnapshot(buff);
                }
                mVideoThread->video(&buff);
                mRecordingBuffers.push(buff);
            } else {
                mISP->putRecordingFrame(&buff);
            }

            //Check the battery status regularly during recording.
            //If the battery level is too low, turn off the flash, notify the application and update the parameters.
            if (buff.frameSequenceNbr % BATTERY_CHECK_INTERVAL_FRAME_UNIT == 0) {
               // note: String8 segfaults if given a NULL, so thus check it for that here
               const char* flash_mode = mParameters.get(CameraParameters::KEY_FLASH_MODE);
               String8 val(flash_mode, (flash_mode == NULL ? 0 : strlen(flash_mode)));
               if (val != CameraParameters::FLASH_MODE_OFF) {
                   CameraParameters param(mParameters);
                   preProcessFlashMode(&param);
                   processParamFlash(&mParameters,&param);
               }
            }
        } else {
            ALOGD("Recording frame %d corrupted, ignoring", buff.id);
            mISP->putRecordingFrame(&buff);
        }
    } else {
        ALOGE("Error: getting recording from isp\n");
    }

    return status;
}

bool ControlThread::threadLoop()
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    mThreadRunning = true;
    while (mThreadRunning) {

        switch (mState) {

        case STATE_STOPPED:
            LOG2("In STATE_STOPPED");
            // in these states all we do is wait for messages
            status = waitForAndExecuteMessage();
            break;

        case STATE_CAPTURE:
            LOG2("In STATE_CAPTURE...");
            // message queue always has priority over getting data from the
            // isp driver no matter what state we are in
            if (!mMessageQueue.isEmpty()) {
                status = waitForAndExecuteMessage();
            } else {
                // make sure ISP has data before we ask for some
                if (mISP->dataAvailable() && burstMoreCapturesNeeded()) {
                    status = captureBurstPic();
                } else {
                    status = waitForAndExecuteMessage();
                }
            }
            break;

        case STATE_PREVIEW_STILL:
            LOG2("In STATE_PREVIEW_STILL...");
            status = waitForAndExecuteMessage();
            break;

        case STATE_PREVIEW_VIDEO:
        case STATE_RECORDING:
            LOG2("In %s...", mState == STATE_PREVIEW_VIDEO ? "STATE_PREVIEW_VIDEO" : "STATE_RECORDING");
            status = waitForAndExecuteMessage();
            break;

        case STATE_CONTINUOUS_CAPTURE:
            LOG2("In STATE_CONTINUOUS_CAPTURE...");
            // message queue always has priority over getting data from the
            // isp driver no matter what state we are in
            if (!mMessageQueue.isEmpty()) {
                status = waitForAndExecuteMessage();
            } else {
                // make sure ISP has data before we ask for some
                if (burstMoreCapturesNeeded())
                    status = captureFixedBurstPic();
                else
                    status = waitForAndExecuteMessage();
            }
            break;

        default:
            break;
        };
    }

    return false;
}

status_t ControlThread::requestExitAndWait()
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_EXIT;
    msg.data.exit.stopThread = true;

    // tell thread to exit
    // send message asynchronously
    mMessageQueue.send(&msg);

    // propagate call to base class
    return Thread::requestExitAndWait();
}

void ControlThread::orientationChanged(int orientation)
{
    LOG1("@%s: orientation = %d", __FUNCTION__, orientation);
    Message msg;
    msg.id = MESSAGE_ID_SET_ORIENTATION;
    msg.data.orientation.value = orientation;
    mMessageQueue.send(&msg);
}

status_t ControlThread::handleMessageSetOrientation(MessageOrientation *msg)
{
    LOG1("@%s: orientation = %d", __FUNCTION__, msg->value);
    mCurrentOrientation = msg->value;
    return NO_ERROR;
}

} // namespace android
