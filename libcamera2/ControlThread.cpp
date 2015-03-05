/*
 * Copyright (C) 2011 The Android Open Source Project
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
#define LOG_TAG "Camera_ControlThread"

#include "ControlThread.h"
#include "LogHelper.h"
#include "PerformanceTraces.h"
#include "CameraConf.h"
#include "PreviewThread.h"
#include "PictureThread.h"
#include "AtomAIQ.h"
#include "AtomSoc3A.h"
#include "AtomExtIsp3A.h"
#include "AtomISP.h"
#include "Callbacks.h"
#include "CallbacksThread.h"
#include "ColorConverter.h"
#include "PlatformData.h"
#include "IntelParameters.h"
#include "ValidateParameters.h"
#include "MemoryUtils.h"
#include <utils/Vector.h>
#include <math.h>
#include <cutils/properties.h>
#include <binder/IServiceManager.h>
#include "intel_camera_extensions.h"
#include "ICameraHwControls.h"
#include "AtomDvs2.h"

namespace android {
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

// default snapshot buffer count for continuous shooting
const int DEFAULT_BUF_NUM_FOR_CONT_SHOOTING = 4;

// number pictures for smart stabilization capture
const int NUM_PICS_FOR_SS = 5;

const char* ControlThread::sCaptureSubstateStrings[]= {
      "INIT",
      "STARTED",
      "ENCODING_DONE",
      "PICTURE_DONE",
      "IDLE",
      "CONTINUOUS_SHOOTING"
};

ControlThread::ControlThread(int cameraId) :
    Thread(true) // callbacks may call into java
    ,mCameraId(cameraId)
    ,mISP(NULL)
    ,mCP(NULL)
    ,mULL(NULL)
    ,m3AControls(NULL)
    ,mBracketManager(NULL)
    ,mPreviewThread(NULL)
    ,mPictureThread(NULL)
    ,mVideoThread(NULL)
    ,m3AThread(NULL)
    ,mPostProcThread(NULL)
    ,mPanoramaThread(NULL)
    ,mScalerService(NULL)
    ,mWarperService(NULL)
    ,mPostCaptureThread(NULL)
    ,mAccManagerThread(NULL)
    ,mThermalThrottleThread(NULL)
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
    ,mBurstBufsToReturn(0)
    ,mUllBurstLength(UltraLowLight::MAX_INPUT_BUFFERS)
    ,mSmartStabilization(false)
    ,mAELockFlashStage(CAM_FLASH_STAGE_NONE)
    ,mPublicShutter(-1)
    ,mDvsEnable(false)
    ,mDualVideo(false)
    ,mJpegContinuousShootingRunning(false)
    ,mParamCache(NULL)
    ,mPreviewForceChanged(false)
    ,mCameraDump(NULL)
    ,mFocusAreas()
    ,mMeteringAreas()
    ,mEnableFocusCbAtStart(false)
    ,mEnableFocusMoveCbAtStart(false)
    ,mStillCaptureInProgress(false)
    ,mPreviewUpdateMode(IntelCameraParameters::PREVIEW_UPDATE_MODE_STANDARD)
    ,mSaveMirrored(false)
    ,mExtIspAction(EXT_ISP_ACTION_NA)
    ,mCurrentOrientation(0)
    ,mFullSizeSdv(false)
    ,mRawBufferLockMode(false)
    ,mSkipPreview(false)
    ,mCurrentExpID(EXP_ID_INVALID)
    ,mNextExpID(EXP_ID_INVALID)
    ,mNumCaptures(0)
    ,mNumSounds(0)
    ,mDepthMode(false)
    ,mContShootingState(CONT_SHOOTING_NONE)
    ,mContShootingEnabled(false)
    ,mContShootingSkipFirstFrame(false)
    ,mContinuousPicsReady(0)
{
    // DO NOT PUT ANY ALLOCATION CODE IN THIS METHOD!!!
    // Put all init code in the init() method.
    // This is a workaround for an issue with Thread reference counting.

    LOG1("@%s", __FUNCTION__);
}

ControlThread::~ControlThread()
{
    // DO NOT PUT ANY CODE IN THIS METHOD!!!
    // Put all deinit code in the deinit() method.
    // This is a workaround for an issue with Thread reference counting.
    LOG1("@%s", __FUNCTION__);
    if(mMessageQueue.size() > 0) {
        Message msg;
        LOGE("At this point Message Q should be empty, found %d message(s)",mMessageQueue.size());
        mMessageQueue.receive(&msg);
        LOGE(" Id of first message is %d",msg.id);
    }
}

status_t ControlThread::init()
{
    LOG1("@%s: cameraId = %d", __FUNCTION__, mCameraId);

    // disable intelligent mode by default
    PlatformData::setIntelligentMode(mCameraId, false);

    status_t status = NO_ERROR;
    bool extIsp = PlatformData::supportsContinuousJpegCapture(mCameraId);
    CameraDump::setDumpDataFlag();

    AtomISP * isp = NULL;
    mScalerService = new ScalerService(mCameraId);
    if (mScalerService == NULL) {
        LOGE("error creating ScalerService");
        goto bail;
    }

    mWarperService = new WarperService();
    if (mWarperService == NULL) {
        LOGE("error creating WarperService");
        goto bail;
    }

    mCallbacks = new Callbacks();
    if (mCallbacks == NULL) {
        LOGE("error creating Callbacks");
        goto bail;
    }

    // we implement ICallbackPicture interface
    mCallbacksThread = new CallbacksThread(mCallbacks, this);
    if (mCallbacksThread == NULL) {
        LOGE("error creating CallbacksThread");
        goto bail;
    }

    isp = new AtomISP(mCameraId, mScalerService, mCallbacks);
    if (isp == NULL) {
        LOGE("error creating ISP");
        goto bail;
    }
    mISP = isp;
    status = mISP->init();
    if (status != NO_ERROR) {
        LOGE("Error initializing ISP");
        goto bail;
    }

    // Assign local HWControlGroup
    mHwcg.mIspCI = (IHWIspControl*)isp;
    mHwcg.mSensorCI = isp->getSensorControlInterface();
    mHwcg.mFlashCI = (IHWFlashControl*)isp;
    mHwcg.mLensCI = (IHWLensControl*)isp;

    // Choose 3A interface based on the sensor type
    if (createAtom3A() != NO_ERROR) {
        LOGE("error creating AAA");
        goto bail;
    }

    if (m3AControls->init3A() != NO_ERROR) {
        LOGE("Error initializing 3A controls");
        goto bail;
    }
    PERFORMANCE_TRACES_BREAKDOWN_STEP("Init_3A");

    mCP = new AtomCP(mHwcg);
    if (mCP == NULL) {
        LOGE("error creating CP");
        goto bail;
    }

    mULL = new UltraLowLight(mCallbacks, mWarperService);
    if (mULL == NULL) {
        LOGE("error creating ULL");
        goto bail;
    }
    mUllBurstLength = mULL->getULLBurstLength();

    mCameraDump = CameraDump::getInstance(mCameraId);
    if (mCameraDump == NULL) {
        LOGE("error creating CameraDump");
        goto bail;
    }
    mCameraDump->set3AControls(m3AControls);
    mCameraDump->setAtomISP(isp);

    // we implement the ICallbackPreview interface, so pass
    // this as argument
    mPreviewThread = new PreviewThread(mCallbacksThread, mCallbacks, mCameraId, mHwcg.mIspCI);
    if (mPreviewThread == NULL) {
        LOGE("error creating PreviewThread");
        goto bail;
    }

    mPictureThread = new PictureThread(m3AControls, mScalerService, mCallbacksThread, mCallbacks, this, mCameraId);
    if (mPictureThread == NULL) {
        LOGE("error creating PictureThread");
        goto bail;
    }

    mVideoThread = new VideoThread(mISP, mCallbacksThread);
    if (mVideoThread == NULL) {
        LOGE("error creating VideoThread");
        goto bail;
    }

    // we implement ICallbackAAA interface
    m3AThread = new AAAThread(this, mULL, m3AControls, mCallbacksThread, mCameraId, extIsp);
    if (m3AThread == NULL) {
        LOGE("error creating 3AThread");
        goto bail;
    }

    mPanoramaThread = new PanoramaThread(this, m3AControls, mCallbacksThread, mCallbacks, mCameraId);
    if (mPanoramaThread == NULL) {
        LOGE("error creating PanoramaThread");
        goto bail;
    }

    mPostProcThread = new PostProcThread(this, mPanoramaThread.get(), m3AControls, mCallbacksThread, mCallbacks, mCameraId);
    if (mPostProcThread == NULL) {
        LOGE("error creating PostProcThread");
        goto bail;
    }

    if (mPostProcThread->init((void*)mISP) != NO_ERROR) {
        LOGE("error initializing face engine");
        goto bail;
    }

    mBracketManager = new BracketManager(mISP, m3AControls, mCameraId);
    if (mBracketManager == NULL) {
        LOGE("error creating BracketManager");
        goto bail;
    }

    mPostCaptureThread = new PostCaptureThread(this);
    if (mPostCaptureThread == NULL) {
        LOGE("error creating PostCapture");
        goto bail;
    }

    mAccManagerThread = new AccManagerThread(mHwcg, mCallbacksThread, mCallbacks, mCameraId);
    if (mAccManagerThread == NULL) {
        LOGE("error creating AccManagerThread");
        goto bail;
    }

    mThermalThrottleThread = new ThermalThrottleThread(mHwcg.mSensorCI);
    if (mThermalThrottleThread == NULL) {
        LOGE("error creating ThermalThrottleThread");
        goto bail;
    }

    // DVS needs to be started after AIQ init.
    if (!PlatformData::useHALVS(mCameraId)) {
        status = mISP->initDVS();
        if (status != NO_ERROR) {
            LOGE("Error in initializing DVS");
            goto bail;
        }
    }

    // get default params from AtomISP and JPEG encoder
    mISP->getDefaultParameters(&mParameters, &mIntelParameters);
    m3AControls->getDefaultParams(&mParameters, &mIntelParameters);
    mPictureThread->getDefaultParameters(&mParameters, mCameraId);
    mPreviewThread->getDefaultParameters(&mParameters, mCameraId);
    mPanoramaThread->getDefaultParameters(&mIntelParameters, mCameraId);
    mPostProcThread->getDefaultParameters(&mParameters, &mIntelParameters, mCameraId);
    mVideoThread->getDefaultParameters(&mIntelParameters, mCameraId);
    updateParameterCache();

    status = mScalerService->run("CamHAL_SCALER");
    if (status != NO_ERROR) {
        LOGE("Error starting scaler service!");
        goto bail;
    }
    status = mWarperService->run("CamHAL_WARPER");
    if (status != NO_ERROR) {
        LOGE("Error starting warper service!");
        goto bail;
    }
    status = m3AThread->run("CamHAL_3A");
    if (status != NO_ERROR) {
        LOGE("Error starting 3A thread!");
        goto bail;
    }
    status = mPreviewThread->run("CamHAL_PREVIEW");
    if (status != NO_ERROR) {
        LOGE("Error starting preview thread!");
        goto bail;
    }
    status = mPictureThread->run("CamHAL_PICTURE");
    if (status != NO_ERROR) {
        LOGW("Error starting picture thread!");
        goto bail;
    }
    status = mCallbacksThread->run("CamHAL_CALLBACK");
    if (status != NO_ERROR) {
        LOGW("Error starting callbacks thread!");
        goto bail;
    }
    status = mVideoThread->run("CamHAL_VIDEO");
    if (status != NO_ERROR) {
        LOGW("Error starting video thread!");
        goto bail;
    }
    status = mPostProcThread->run("CamHAL_POSTPROC");
    if (status != NO_ERROR) {
        LOGW("Error starting Post Processing thread!");
        goto bail;
    }
    status = mPanoramaThread->run("CamHAL_PANO");
    if (status != NO_ERROR) {
        LOGW("Error Starting Panorama Thread!");
        goto bail;
    }
    status = mPostCaptureThread->run("CamHAL_POSTCAP");
    if (status != NO_ERROR) {
        LOGW("Error Starting PostCaptureThread!");
        goto bail;
    }

    status = mAccManagerThread->run("CamHAL_ACCMANAGER");
    if (status != NO_ERROR) {
        LOGW("Error starting Acceleration Manager thread!");
        goto bail;
    }

    status = mThermalThrottleThread->run("CamHAL_THERMALTHROTTLE");
    if (status != NO_ERROR) {
        LOGW("Error starting thermal throttle thread!");
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

    CLEAR(mExtIsp);

    //default flash modes
    mSavedFlashSupported = PlatformData::supportedFlashModes(mCameraId);
    mSavedFlashMode = PlatformData::defaultFlashMode(mCameraId);

    // default focus mode
    mSavedFocusMode = PlatformData::defaultFocusMode(mCameraId);

    // Set property to inform system what camera is in use
    char facing[PROPERTY_VALUE_MAX];
    snprintf(facing, PROPERTY_VALUE_MAX, "%d", mCameraId);
    property_set("media.camera.facing", facing);

    // Set default parameters so that settings propagate to 3A
    MessageSetParameters msg;
    msg.params = mParamCache;
    handleMessageSetParameters(&msg);

    // set preview update mode from platform data
    processPreviewUpdateMode(&mParameters, &mIntelParameters);

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
        delete mBracketManager;
        mBracketManager = NULL;
    }

    if (mPostProcThread != NULL) {
        mPostProcThread->requestExitAndWait();
        mPostProcThread.clear();
    }

    if (mAccManagerThread != NULL) {
        mAccManagerThread->requestExitAndWait();
        mAccManagerThread.clear();
    }

    if (mThermalThrottleThread != NULL) {
        if (mThermalThrottleThread->isMonitoring())
            mThermalThrottleThread->stopMonitoring();
        mThermalThrottleThread->requestExitAndWait();
        mThermalThrottleThread.clear();
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

    // clean local HWControlGroup
    mHwcg.mIspCI = NULL;
    mHwcg.mSensorCI = NULL;
    mHwcg.mFlashCI = NULL;
    mHwcg.mLensCI = NULL;

    if (mISP != NULL) {
        mISP->deInitDevice();
        delete mISP;
        mISP = NULL;
        PERFORMANCE_TRACES_BREAKDOWN_STEP("DeleteISP");
    }

    if (mScalerService != NULL) {
        mScalerService->requestExitAndWait();
        mScalerService.clear();
    }

    if (mWarperService != NULL) {
        mWarperService->requestExitAndWait();
        mWarperService.clear();
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

status_t ControlThread::recoverPreview()
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_TIMEOUT;
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
    LOG2("@%s", __FUNCTION__);
    // handled by video thread
    return mVideoThread->releaseRecordingFrame(buff);
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

void ControlThread::sceneDetected(String8 sceneMode, bool sceneHdr)
{
    LOG2("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_SCENE_DETECTED;
    if (!sceneMode.isEmpty()) {
        strlcpy(msg.data.sceneDetected.sceneMode, sceneMode.string(), (size_t)SCENE_STRING_LENGTH);
        msg.data.sceneDetected.sceneHdr = sceneHdr;
        mMessageQueue.send(&msg);
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

    status = mPictureThread->encode(picMetaData, &(msg->buff), pPvBuff, false);
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
void ControlThread::atPostviewPresent() {
    LOG2("@%s", __FUNCTION__);
    status_t status(NO_ERROR);

    // we do this without msg to make sure preview is hidden right a way.

    // standard Google API request stop preview on take picture
    if (mPreviewUpdateMode == IntelCameraParameters::PREVIEW_UPDATE_MODE_STANDARD &&
        !mJpegContinuousShootingRunning) {
        LOG1("@%s: Hide preview on continuous JPEG capture", __FUNCTION__);
        status = mPreviewThread->setPreviewState(PreviewThread::STATE_ENABLED_HIDDEN);
        if(status != NO_ERROR) {
            LOGE("Could not hide preview");
        }
    }

    if (mJpegContinuousShootingRunning) {
        LOG2("@%s: request next continuous shooting picture", __FUNCTION__);
        mCallbacksThread->requestTakePicture(false, false , false);
    }
}

void ControlThread::encodingDone(AtomBuffer *snapshotBuf, AtomBuffer *postviewBuf)
{
    LOG1("@%s: snapshotBuf = %p, postviewBuf = %p, id = %d",
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
    case STATE_JPEG_CAPTURE:
        handleMessageStopPreview();
        break;
    case STATE_RECORDING:
        handleMessageStopRecording();
        break;
    case STATE_STOPPED:
        // do nothing
        break;
    default:
        LOGW("Exiting in an invalid state, this should not happen!!!");
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

    if (mState != STATE_CONTINUOUS_CAPTURE && mState != STATE_JPEG_CAPTURE)
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
    LOG1("@%s", __FUNCTION__);
    PreviewThread::PreviewState previewState;

    if (mState != STATE_CONTINUOUS_CAPTURE && mState != STATE_JPEG_CAPTURE)
        return NO_INIT;

    previewState = mPreviewThread->getPreviewState();
    // already in continuous-state, startPreview case
    if (mISP->isOfflineCaptureRunning() && mContShootingState == CONT_SHOOTING_NONE) {
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
        LOGE("Trying to resume continuous preview from unexpected state!");
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
    ContinuousCaptureConfig cfg;
    CLEAR(cfg);

    // capture priority by default
    cfg.capturePriority = true;
    // enable raw buffer lock mode if bracketing in offline mode
    if (mBurstLength > 1 && mBracketManager->getBracketMode() != BRACKET_NONE) {
        cfg.rawBufferLock = true;
        // disable capture priority in lock mode
        cfg.capturePriority = false;
        mRawBufferLockMode = true;
    }

    if (mPreviewUpdateMode == IntelCameraParameters::PREVIEW_UPDATE_MODE_CONTINUOUS ||
            (mFullSizeSdv && (mState == STATE_PREVIEW_VIDEO || mState == STATE_RECORDING)))
        cfg.capturePriority = false;

    if (mULL->isActive() || mBurstLength > 1)
        cfg.numCaptures = MAX(mUllBurstLength, mBurstLength);
    else
        cfg.numCaptures = ((mHwcg.mIspCI->getCssMajorVersion() == 2) && (mHwcg.mIspCI->getCssMinorVersion() == 0))? 2 : 1;

    if (mSmartStabilization)
        cfg.numCaptures = NUM_PICS_FOR_SS;

    // for continuous shooting, unlimited number of capture
    if (mContShootingEnabled) {
        cfg.numCaptures = -1;
        cfg.capturePriority = false;
    }

    cfg.offset = -(mISP->shutterLagZeroAlign());
    cfg.skip = 0;
    continuousConfigApplyLimits(cfg);

    LOG1("%s numcaptures %d, offset %d, skip %d",__FUNCTION__,
                                                cfg.numCaptures,
                                                cfg.offset,
                                                cfg.skip);

    return mISP->prepareOfflineCapture(cfg);
}

/**
 * Save the current context of camera parameters that describe:
 * - picture size
 * - thumbnail size
 * - supported picture sizes
 * - supported thumbnail sizes
 *
 * This is used in initSdv when we start video preview because we need to
 * impose restrictions on these values to implement video snapshot feature
 * Use clearSavedPictureParams() to clear it.
 */
void ControlThread::saveCurrentPictureParams()
{
    clearSavedPictureParams();
    mParameters.getPictureSize(&mStillPictContext.snapshotWidth,
                               &mStillPictContext.snapshotHeight);
    mStillPictContext.thumbnailWidth = mParameters.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH);
    mStillPictContext.thumbnailHeigth = mParameters.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT);

    const char* supportedSnapshotSizes = mParameters.get(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES);
    if (supportedSnapshotSizes) {
        mStillPictContext.supportedSnapshotSizes = supportedSnapshotSizes;
    } else {
        LOGE("Missing supported picture sizes");
        mStillPictContext.supportedSnapshotSizes = "";
    }

    const char* supportedThumbnailSizes = mParameters.get(CameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES);
    if (supportedThumbnailSizes) {
        mStillPictContext.suportedThumnailSizes = supportedThumbnailSizes;
    } else {
        LOGE("Missing supported thumbnail sizes");
        mStillPictContext.suportedThumnailSizes = "";
    }
}

/**
 * Clear the saved picture parameters.
 */
void ControlThread::clearSavedPictureParams()
{
    mStillPictContext.thumbnailWidth  = 0;
    mStillPictContext.thumbnailHeigth = 0;
    mStillPictContext.snapshotWidth   = 0;
    mStillPictContext.snapshotHeight  = 0;
    mStillPictContext.clear();
}

/**
 * update current parameters according to video size
 * to update:
 * 1. KEY_PICTURE_SIZE
 * 2. KEY_SUPPORTED_PICTURE_SIZES
 * 3. KEY_JPEG_THUMBNAIL_SIZE
 * 4. KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES
 * When recording is stopped a reciprocal call to sdvRestoreParams()
 * will be done
 */
status_t ControlThread::sdvUpdateParams(bool offline, bool updateCache)
{
    int  width, height, vidWidth, vidHeight;
    char sizes[25];
    // decide picture size
    if (offline || PlatformData::supportsContinuousJpegCapture(mCameraId)) {
        if (!selectSdvSize(width, height)) {
            LOGE("no proper picture size.");
            return UNKNOWN_ERROR;
        }
    } else {
        //picture size equal to video size in online mode
        mISP->getVideoSize(&width, &height, NULL);
    }
    mParameters.setPictureSize(width, height);
    snprintf(sizes, 25, "%dx%d", width,height);
    mParameters.set(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES, sizes);
    LOG1("video snapshot size %dx%d", width, height);

    //decide postview(used for thumbnail) size
    selectPostviewSize(width, height);
    mParameters.getVideoSize(&vidWidth, &vidHeight);
    if (width > vidWidth) {
        width  = vidWidth;
        height = vidHeight;
    }

    // Limit thumbnail size less than 480p to reduce thumbnail Jpeg size.
    // Make sure total Exif size less than 64k.
    if (height >= RECONFIGURE_THUMBNAIL_HEIGHT_LIMIT) {
        reconfigureThumbnailSize(width, height);
    }

    LOG1("video snapshot thumbnail size %dx%d", width, height);
    mParameters.set(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH, width);
    mParameters.set(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT, height);
    snprintf(sizes, 25, "%dx%d,0x0", width, height);
    mParameters.set(CameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES, sizes);

    if (updateCache)
        return updateParameterCache();

    return NO_ERROR;
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
status_t ControlThread::sdvRestoreParams(bool updateCache)
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

    if (updateCache)
        return updateParameterCache();

    return NO_ERROR;
}

bool ControlThread::isFullSizeSdvSupportedVideoSize(int width, int height, int previewWidth, int previewHeight)
{
    int minW, minH;
    getSdvSupportedMinVideoSize(minW, minH);
    if ((previewWidth * previewHeight) > (width * height)) // see AtomISP::applyISPLimitations workarounds
        return (previewWidth >= minW && previewHeight >= minH) ? true : false; // AtomISP swaps the devices, so compare to preview size
    else
        return (width >= minW && height >= minH) ? true : false;
}

status_t ControlThread::getSdvSupportedMinVideoSize(int &width, int &height)
{
    int w, h;
    // current max bayer downscaling factor is 8X
    const float maxBdsFactor = 8.0f;
    // Thus, the max downscaling factor is 8 x max_YUV_downscaling_ratio
    float maxDsFactor = maxBdsFactor * AtomDvs2::maxDvs2YUVDSRatio;

    // Video pipe is downscaled from main output frame. So we caculate the max
    // supported video size from the max main output size
    if (selectSdvSize(w, h)) {
        width  = (float)w / maxDsFactor;
        height = (float)h / maxDsFactor;
        LOG1("@%s get SDV supported min video size %dx%d", __FUNCTION__, width, height);
        return NO_ERROR;
    }

    // set to a default value
    width  = RESOLUTION_VGA_WIDTH;
    height = RESOLUTION_VGA_HEIGHT;
    LOGW("@%s SDV size is not found, set min video size to defaut value %dx%d", __FUNCTION__, width, height);
    return UNKNOWN_ERROR;
}

/**
 * Configures parameters for SDV.
 *
 * In online video mode, we support the snapshot size equal to video size only
 * In offline video mode, snapshot during video can be 8M or 5M decided by ISP limitation
 * Parameters for both capture and preview need to be set up before starting the ISP.
 */
status_t ControlThread::initSdv(bool offline)
{
    LOG1("@%s %s", __FUNCTION__, offline?"offline":"online");
    status_t status = NO_ERROR;

    // store old parameters.
    saveCurrentPictureParams();

    // update new parameters for SDV
    sdvUpdateParams(offline, false);

    if (offline) {
        int thumbWidth, thumbHeight;
        int fourcc = mISP->getSnapshotPixelFormat();
        AtomBuffer formatDescriptorSdv = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_FORMAT_DESCRIPTOR, fourcc);
        mParameters.getPictureSize(&formatDescriptorSdv.width, &formatDescriptorSdv.height);
        thumbWidth  = mParameters.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH);
        thumbHeight = mParameters.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT);
        AtomBuffer formatDescriptorPv
            = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_FORMAT_DESCRIPTOR, selectPostviewFormat(), thumbWidth, thumbHeight);

        mISP->setSnapshotFrameFormat(formatDescriptorSdv);
        configureContinuousRingBuffer();
        mISP->setPostviewFrameFormat(formatDescriptorPv);

        burstStateReset();
    }

    return status;
}

status_t ControlThread::captureSdvSoC(bool fullsize)
{
    LOG1("@%s: %s", __FUNCTION__, fullsize ? "full size" : "not full size");
    status_t status = NO_ERROR;

    mCallbacksThread->requestTakePicture(true, true);
    mPictureThread->initialize(mParameters, mHwcg.mIspCI->zoomRatio(mParameters.getInt(CameraParameters::KEY_ZOOM)));

    if (fullsize) {
        // allocate buffer struct
        AtomBuffer snapshotBuffer
            = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_SNAPSHOT);
        AtomBuffer postviewBuffer
            = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_POSTVIEW);
        // stop face detection if necessary
        stopFaceDetection();

        status = waitForCaptureStart();
        if (status != NO_ERROR) {
            LOGE("Error while waiting for capture to start");
            return status;
        }
        status = mISP->getSnapshot(&snapshotBuffer, &postviewBuffer);
        if (status != NO_ERROR) {
            LOGE("Error in grabbing snapshot!");
            return status;
        }

        // encode a frame
        PictureThread::MetaData picMetaData;
        fillPicMetaData(picMetaData, false);
        LOG1("TEST-TRACE: starting picture encode: Time: %lld", systemTime());
        status = mPictureThread->encode(picMetaData, &snapshotBuffer, &postviewBuffer);
        if (status != NO_ERROR) {
            picMetaData.free(m3AControls);
            LOGE("@%s: failed to call PictureThread to encode", __FUNCTION__);
        }
    } else {
        AtomBuffer snapshotBuffer;
        // get snapshotBuffer from VideoThread
        status = mVideoThread->getVideoSnapshot(snapshotBuffer);
        if (status != NO_ERROR) {
            LOGE("Error in getVideoSnapshot from VideoThread");
            return UNKNOWN_ERROR;
        }

        // encode this buffer
        encodeVideoSnapshot(snapshotBuffer);
    }

    return status;
}

status_t ControlThread::captureStillPicFromPreview()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    mCallbacksThread->requestTakePicture(false, true);
    mPictureThread->initialize(mParameters, mHwcg.mIspCI->zoomRatio(mParameters.getInt(CameraParameters::KEY_ZOOM)));

    // allocate buffer struct
    AtomBuffer snapshotBuffer
        = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_SNAPSHOT);

    status = mPreviewThread->getPreviewBufferById(snapshotBuffer);
    if (status == NO_ERROR) {
        // encode a frame
        PictureThread::MetaData picMetaData;
        fillPicMetaData(picMetaData, false);
        LOG1("TEST-TRACE: starting picture encode: Time: %lld", systemTime());
        status = mPictureThread->encode(picMetaData, &snapshotBuffer, NULL);
        if (status != NO_ERROR) {
            picMetaData.free(m3AControls);
            LOGE("@%s: failed to call PictureThread to encode", __FUNCTION__);
        }
    }

    return status;
}

status_t ControlThread::captureSdv(bool offline)
{
    LOG1("@%s: %s", __FUNCTION__, offline ? "offline" : "online");
    status_t status = NO_ERROR;

    mCallbacksThread->requestTakePicture(true, true);
    mPictureThread->initialize(mParameters, mHwcg.mIspCI->zoomRatio(mParameters.getInt(CameraParameters::KEY_ZOOM)));

    if (offline) {
        // allocate buffer struct
        AtomBuffer snapshotBuffer
            = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_SNAPSHOT);
        AtomBuffer postviewBuffer
            = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_POSTVIEW);
        // stop face detection if necessary
        stopFaceDetection();

        // start offline capture
        status = continuousStartStillCapture(false);

        status = waitForCaptureStart();
        if (status != NO_ERROR) {
            LOGE("Error while waiting for capture to start");
            return status;
        }
        status = mISP->getSnapshot(&snapshotBuffer, &postviewBuffer);
        if (status != NO_ERROR) {
            LOGE("Error in grabbing snapshot!");
            return status;
        }

        // request to play the sound although it would be shielded by up layer
        mCallbacksThread->shutterSound();

        // encode a frame
        PictureThread::MetaData picMetaData;
        fillPicMetaData(picMetaData, false);
        LOG1("TEST-TRACE: starting picture encode: Time: %lld", systemTime());
        status = mPictureThread->encode(picMetaData, &snapshotBuffer, &postviewBuffer);
        if (status != NO_ERROR) {
            picMetaData.free(m3AControls);
            LOGE("@%s: failed to call PictureThread to encode", __FUNCTION__);
        }

        // stop offline capture
        stopOfflineCapture();
    } else {
        AtomBuffer snapshotBuffer;
        // get snapshotBuffer from VideoThread
        status = mVideoThread->getVideoSnapshot(snapshotBuffer);
        if (status != NO_ERROR) {
            LOGE("Error in getVideoSnapshot from VideoThread");
            return UNKNOWN_ERROR;
        }

        // encode this buffer
        encodeVideoSnapshot(snapshotBuffer);
    }

    return status;
}

status_t ControlThread::cancelCaptureSdv()
{
    status_t status = NO_ERROR;
    if (mCaptureSubState == STATE_CAPTURE_STARTED && mState == STATE_RECORDING) {
        // cancel video snapshot
        mPictureThread->flushBuffers();
        LOG1("CaptureSubState %s -> IDLE", sCaptureSubstateStrings[mCaptureSubState]);
        mCaptureSubState = STATE_CAPTURE_IDLE;
    }

    return status;
}

status_t ControlThread::deinitSdv(bool offline)
{
    LOG1("@%s %s", __FUNCTION__, offline?"offline":"online");
    status_t status = NO_ERROR;

    // cancel SDV if we are doing it
    cancelCaptureSdv();
    clearSavedPictureParams();
    return status;
}

/**
 * Configures parameters for continuous jpeg capture.
 *
 * In continuous jpeg capture mode, parameters for both capture
 * and preview need to be set up before starting the ISP.
 */
status_t ControlThread::initContinuousJpegCapture()
{
    LOG1("@%s", __FUNCTION__);
    status_t status(NO_ERROR);

    int fourccSs(V4L2_PIX_FMT_CONTINUOUS_JPEG);
    int widthSs(0);
    int heightSs(0);

    mParameters.getPictureSize(&widthSs, &heightSs);
    AtomBuffer formatDescriptorSs = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_FORMAT_DESCRIPTOR, fourccSs,widthSs, heightSs);
    mISP->setSnapshotFrameFormat(formatDescriptorSs);

    PERFORMANCE_TRACES_BREAKDOWN_STEP("Done");
    return status;
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
    // TODO the bpl should take isp format configure result into account
    // but the timing of this function call is not quite right for that so use
    // bpl = width as the best guess
    formatDescriptorSs.bpl = formatDescriptorSs.width;

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
    mPictureThread->initialize(mParameters, mHwcg.mIspCI->zoomRatio(mParameters.getInt(CameraParameters::KEY_ZOOM)));

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
            LOGE("Error flushing PictureThread!");
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
            if (PlatformData::supportsContinuousJpegCapture(mCameraId))
                ret = SHOOTING_MODE_JPEG;
            else
                ret = SHOOTING_MODE_SINGLE;
            break;

        case STATE_RECORDING:
            if (PlatformData::supportsContinuousJpegCapture(mCameraId) && mExtIspAction != EXT_ISP_ACTION_VIDEOHS)
                ret = SHOOTING_MODE_JPEG;
            else
                ret = SHOOTING_MODE_VIDEO_SNAP;
            break;

        case STATE_CONTINUOUS_CAPTURE:
            if (isBurstRunning())
                ret = SHOOTING_MODE_ZSL_BURST;
            else if (mContShootingState != CONT_SHOOTING_NONE) {
                ret = SHOOTING_MODE_CONTINUOUS_SHOOTING;
                break;
            }
            else if (mSmartStabilization) {
                ret = SHOOTING_MODE_SMARTSTABILIZATION;
                break;
            }
            else if (!PlatformData::ispSupportContinuousCaptureMode(mCameraId))
                /*
                 * Note: Althrough we enable continuousCapture in camera_profiles.xml for
                 * the follow capture mode, but it is not real ZSL mode. Previwe is stopped
                 * for a while during capture. So SHOOTING_MODE_SINGLE is used for clarify
                 *
                 * Why we don't use online capture and preview? Because preview can not be stopped
                 * during the capture, and the sequence of the mode is the same as continuousCapture
                 *
                 * Another difference is that ISP doesn't support Continuous mode, so we don't set
                 * ISP to continuous mode by setting <ispSupportContinuousCaptureMode value="false"/>
                 * in camera_profiles.xml
                 */
                ret = SHOOTING_MODE_SINGLE;
            else
                ret = SHOOTING_MODE_ZSL;
            // Trigger ULL in single capture only when user did not force flash
            // and when we have enough available buffers
            if (mULL->isActive() && mULL->trigger() && !flashOn && mBurstLength <= 1 &&
                mAvailableSnapshotBuffers.size() >= (size_t)mUllBurstLength &&
                mAvailablePostviewBuffers.size() >= (size_t)mUllBurstLength)
                ret = SHOOTING_MODE_ULL;
            break;

        case STATE_CAPTURE:
            if (isBurstRunning())
                ret = SHOOTING_MODE_BURST;
            break;

        case STATE_JPEG_CAPTURE:
            if (mExtIsp.HDR || mExtIsp.LLS)
                ret = SHOOTING_MODE_EXTISP_HDR_LLS;
            else
                ret = SHOOTING_MODE_JPEG;
            break;

        case STATE_STOPPED:
        default:
            LOGW("Unexpected state (%d) to select the shooting mode",mState);
            break;
    }

    LOG1("Shooting Mode selected: %d", ret);
    return ret;
}

/**
 * Selects which still preview mode to use.
 *
 * @return STATE_CONTINUOUS_CAPTURE or STATE_PREVIEW_STILL
 */
ControlThread::State ControlThread::selectPreviewMode(const CameraParameters &params)
{
    // If continuous JPEG capture is supported select
    if (PlatformData::supportsContinuousJpegCapture(mCameraId)) {
        LOG1("@%s: Selecting jpeg mode, supported by platform", __FUNCTION__);
        return STATE_JPEG_CAPTURE;
    }

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

    if (mHdr.enabled && !PlatformData::supportsOfflineHdr()) {
        LOG1("@%s: HDR enabled, disabling continuous mode",
             __FUNCTION__);
        goto online_preview;
    }

    // Bracketing not supported in continuous mode as the number captures is not fixed.
    if (mBurstLength > 1 && mBracketManager->getBracketMode() != BRACKET_NONE &&
            !PlatformData::supportsOfflineBracket()) {
        LOG1("@%s: Bracketing requested, disabling continuous mode",
                __FUNCTION__);
        goto online_preview;
    }

    if (mBurstLength > 1 && mBurstStart >= 0 && !PlatformData::supportsOfflineBurst()) {
        LOG1("@%s: platform doesn't support offline burst, disabling continuous mode",
                __FUNCTION__);
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
    }

    if (CameraDump::isDumpImageEnable(CAMERA_DEBUG_DUMP_RAW)) {
        LOG1("@%s: Raw dump enabled, disabling continuous mode", __FUNCTION__);
        goto online_preview;
    }

    if (mHwcg.mIspCI->getLowLight()) {
        LOG1("@%s: ANR enabled, disabling continuous mode", __FUNCTION__);
        goto online_preview;
    }

    // No continuous mode for 3rd party firmware
    if (mIspExtensionsEnabled) {
        LOG1("@%s: ISP Extensions enabled, disabling continuous mode", __FUNCTION__);
        goto online_preview;
    }

    // Online mode for dual video
    if (mDualVideo) {
        LOG1("@%s: Dual video, disabling continuous mode", __FUNCTION__);
        goto online_preview;
    }

    LOG1("@%s: Selecting continuous still preview mode", __FUNCTION__);
    return STATE_CONTINUOUS_CAPTURE;

online_preview:
    // In online preview we cannot support preview update modes 'during_capture' and 'continuous'
    if (mPreviewUpdateMode == IntelCameraParameters::PREVIEW_UPDATE_MODE_DURING_CAPTURE ||
        mPreviewUpdateMode == IntelCameraParameters::PREVIEW_UPDATE_MODE_CONTINUOUS) {
        mPreviewUpdateMode = IntelCameraParameters::PREVIEW_UPDATE_MODE_STANDARD;
        LOGW("Forcing preview update mode to standard, conflicting settings");
    }
    return STATE_PREVIEW_STILL;

}

/**
 * Get ISP flip configuration, this is for saved-mirror feature of front camera.
 */
int ControlThread::getNeededIspFlipType()
{
    if (!mSaveMirrored || PlatformData::cameraFacing(mCameraId) != CAMERA_FACING_FRONT) {
        return ISP_FLIP_NONE;
    }

    int sensorOrientation = PlatformData::cameraOrientation(mCameraId);
    int orientation = (sensorOrientation - mCurrentOrientation + 360) % 360;
    if (orientation == 90 || orientation == 270) {
        return ISP_FLIP_V;
    } else {
        return ISP_FLIP_H;
    }
}

status_t ControlThread::startPreviewCore(bool videoMode)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    int width(0);
    int height(0);
    int cb_fourcc(0);
    int bpl(0);
    int fps(0);
    State state(STATE_STOPPED);
    AtomMode mode(MODE_NONE);
    bool useNV21 = false;

    if (mState != STATE_STOPPED) {
        LOGE("Must be in STATE_STOPPED to start preview");
        return INVALID_OPERATION;
    }

    // disable raw buffer lock by default
    mRawBufferLockMode = false;

    PerformanceTraces::SwitchCameras::called(videoMode);

    // ISP can be de-initialized during ErrorPreview notification.
    // It is there necessary to check if the ISP is still Initialized everytime we restart it.
    if (!mISP->isDeviceInitialized())
        mISP->init();

    mContShootingSkipFirstFrame = false; // reset this in preview start
    mExtIspAction = EXT_ISP_ACTION_NA;
    mISP->setExternalIspActionHint(EXT_ISP_ACTION_NA); // set it off from ISP also
    if (videoMode) {
        state = STATE_PREVIEW_VIDEO;

        if (mIntelParamsAllowed) {
            // Update fps value to cover the case that preview restart due to ISP timeout
            fps = mParameters.getInt(IntelCameraParameters::KEY_RECORDING_FRAME_RATE);
            mISP->setRecordingFramerate(fps);
        }

        mParameters.getVideoSize(&width, &height);
        // Video size is updated later than other parameters, so validate high speed  params here
        if (!validateHighSpeedResolutionFps(width, height, fps)) {
            return BAD_VALUE;
        }

        int previewWidth, previewHeight;
        mParameters.getPreviewSize(&previewWidth, &previewHeight);
        if (PlatformData::supportsContinuousJpegCapture(mCameraId) && fps > DEFAULT_RECORDING_FPS) {
            // external ISP video high speed
            mExtIspAction = EXT_ISP_ACTION_VIDEOHS;
        } else if(PlatformData::useHALVS(mCameraId) && mDvsEnable &&
                width == previewWidth && height == previewHeight) {
            // HAL video stabilization for external ISP
            mExtIspAction = EXT_ISP_ACTION_HALVS;
        }
        mISP->setExternalIspActionHint(mExtIspAction);
        mISP->setVideoFrameFormat(width, height);

        /**
         * SDV Limitations:
         * 1. High speed and continuous SDV can not coexist due to ISP limitation. If user enable high speed and
         *    if the setting is valid, we should disable continuous video.
         * 2. Currently bayer downcaling + yuv downscaling can not downscale to a too small size
         */
        const char *sdvValue = mIntelParamsAllowed ? mParameters.get(IntelCameraParameters::KEY_SDV):
                                                mIntelParameters.get(IntelCameraParameters::KEY_SDV);
        String8 sdvParam (sdvValue, (sdvValue == NULL ? 0 : strlen(sdvValue)));
        mFullSizeSdv = PlatformData::isFullResSdvSupported(mCameraId) && // platform supported
            !mDualVideo && // not dual video
            sdvParam == CameraParameters::TRUE && // user doesn't request to disable sdv by parameter
            fps <= DEFAULT_RECORDING_FPS && // not high speed mode
            isFullSizeSdvSupportedVideoSize(width, height, previewWidth, previewHeight) && // video size limitation
            !PlatformData::supportsContinuousJpegCapture(mCameraId); // SDV JPEG comes inside ext-isp stream

        mode = mFullSizeSdv ? MODE_CONTINUOUS_VIDEO : MODE_VIDEO;
        LOG1("Starting preview in %s mode", mode == MODE_VIDEO? "video":"continuous video");
        initSdv(mFullSizeSdv);

        if (mExtIspAction != EXT_ISP_ACTION_NA) {
            mode = MODE_CONTINUOUS_JPEG;
            status = mHwcg.mIspCI->setDVS(false); // disable isp stabilization, use hal version
        } else {
            if(PlatformData::supportsContinuousJpegCapture(mCameraId))
                mode = MODE_CONTINUOUS_JPEG_VIDEO;

            status = mHwcg.mIspCI->setDVS(mDvsEnable);
        }

        if (status != NO_ERROR) {
            LOGW("@%s: Failed to set DVS %s", __FUNCTION__, mDvsEnable ? "enabled" : "disabled");
        }

    } else {
        LOG1("Starting preview in still mode");
        state = selectPreviewMode(mParameters);

        switch (state) {
        case STATE_PREVIEW_STILL:
            mode = MODE_PREVIEW;
            break;

        case STATE_JPEG_CAPTURE:
            mode = MODE_CONTINUOUS_JPEG;
            break;

        default:
            mode = MODE_CONTINUOUS_CAPTURE;
            break;
        }
    }

    if (state == STATE_CONTINUOUS_CAPTURE) {
        if (initContinuousCapture() != NO_ERROR) {
            LOGE("failed to init continuous capture");
            return BAD_VALUE;
        }
    } else if (state == STATE_JPEG_CAPTURE ||
              (videoMode && (mode == MODE_CONTINUOUS_JPEG || mode == MODE_CONTINUOUS_JPEG_VIDEO))) {
        if (initContinuousJpegCapture() != NO_ERROR) {
            LOGE("failed to init continuous jpeg capture");
            return BAD_VALUE;
        }
    } else {
        // set default snapshot frame format to isp
        int fourcc = mISP->getSnapshotPixelFormat();
        AtomBuffer formatDescriptorSs = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_FORMAT_DESCRIPTOR, fourcc);
        mParameters.getPictureSize(&formatDescriptorSs.width, &formatDescriptorSs.height);
        formatDescriptorSs.bpl = formatDescriptorSs.width;
        mISP->setSnapshotFrameFormat(formatDescriptorSs);
    }

    const char* cb_fourcc_s = mParameters.getPreviewFormat();
    cb_fourcc = V4L2Format(cb_fourcc_s);
    if (!cb_fourcc) {
        LOGW("Unsupported preview callback fourcc : %s", cb_fourcc_s ? cb_fourcc_s : "not set");
    }

    if (videoMode && mISP->getRecordingFramerate() == 60) {
        // 60 fps recording only supports VF size equaling recording size, so
        // take the preview width and height from video size during that use case
        mParameters.getVideoSize(&width, &height);
    } else
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
    // JPEG capture mode we use sharedGfxBuffers (0-copy) even in standard mode

    bool useSharedGfxBuffers =
        (mPreviewUpdateMode != IntelCameraParameters::PREVIEW_UPDATE_MODE_WINDOWLESS)
        && (mIntelParamsAllowed || (mode != MODE_CONTINUOUS_CAPTURE));

    // for certain ext-isp use cases (6MP panorama, kids mode), enable zero-copy
    // shared mode always in order have zero-copy preview callbacks
    if ((PlatformData::supportsContinuousJpegCapture(mCameraId) &&
        width == RESOLUTION_6MP_WIDTH && height == RESOLUTION_6MP_HEIGHT) ||
        mExtIsp.kidsMode) {
        useSharedGfxBuffers = true;
        useNV21 = true;
    }

    if (mExtIspAction == EXT_ISP_ACTION_HALVS) {
        // hal video stabilization needs bigger capture buffers of its own, so
        // it can't use the smaller preview buffers for capturing -> sharing off
        useSharedGfxBuffers = false;
    }

    mPreviewThread->setPreviewConfig(width, height, cb_fourcc, useSharedGfxBuffers, mExtIspAction == EXT_ISP_ACTION_HALVS, mNumBuffers);

    // Get the preview size from PreviewThread and pass the configuration to AtomISP.
    status = mPreviewThread->fetchPreviewBufferGeometry(&width, &height, &bpl);
    if (status != NO_ERROR) {
        LOGE("Error fetch preview buffer geometry");
        return status;
    }

    if (PlatformData::getIntelligentMode(mCameraId))
        mISP->setPreviewFrameFormat(width, height, bpl, V4L2_PIX_FMT_SGRBG8);
    else
        mISP->setPreviewFrameFormat(width, height, bpl, useNV21 ? V4L2_PIX_FMT_NV21 : PlatformData::getPreviewPixelFormat(mCameraId));

    if (useSharedGfxBuffers) {
        Vector<AtomBuffer> sharedGfxBuffers;
        status = mPreviewThread->fetchPreviewBuffers(sharedGfxBuffers);
        if (status == NO_ERROR) {
            if ((int)sharedGfxBuffers.size() != mNumBuffers) {
                LOGE("Invalid shared preview buffer count configuration");
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
        LOGE("Error configuring ISP");
        mPreviewThread->returnPreviewBuffers();
        return status;
    }

    if (videoMode) {
        allocateSnapshotAndPostviewBuffers(true);
    }

    status = mISP->allocateBuffers(mode);
    if (status != NO_ERROR) {
        LOGE("Error allocate buffers in ISP");
        mPreviewThread->returnPreviewBuffers();
        return status;
    }

    if (m3AControls->isIntel3A() && (!(gPowerLevel & CAMERA_POWERBREAKDOWN_DISABLE_3A))) {
        // Enable auto-focus by default
        m3AControls->setAfEnabled(true);
        m3AThread->enable3A();
        if (m3AThread->switchModeAndRate(mode, mHwcg.mSensorCI->getFramerate()) != NO_ERROR)
            LOGE("Failed switching 3A at %.2f fps", mHwcg.mSensorCI->getFramerate());

        mISP->attachObserver(m3AThread.get(), OBSERVE_3A_STAT_READY);
    } else {
        if (PlatformData::supportsContinuousJpegCapture(mCameraId) &&
                mExtIspAction != EXT_ISP_ACTION_VIDEOHS) {
            // For ext-isp, let's listen to frame events for auto-focus handling.
            mISP->attachObserver(m3AThread.get(), OBSERVE_PREVIEW_STREAM);
        }

        // We need to recieve Sensor Metadata event, then get some informations of sensor frame.
        if (PlatformData::supportedSensorMetadata(mCameraId))
            mISP->attachObserver(m3AThread.get(), OBSERVE_3A_STAT_READY);
    }


    // Update focus areas for the proper window size
    if (!mFaceDetectionActive && !mFocusAreas.isEmpty()) {
        size_t winCount(mFocusAreas.numOfAreas());
        CameraWindow *focusWindows = new CameraWindow[winCount];
        mFocusAreas.toWindows(focusWindows);
        if (setAfWindowsTo3a(focusWindows, winCount) != NO_ERROR) {
            LOGE("Could not set AF windows. Resseting the AF to %d", CAM_AF_MODE_AUTO);
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

        mMeteringAreas.toWindows(meteringWindows);

        if (m3AControls->setAeWindow(&meteringWindows[0]) != NO_ERROR) {
            LOGW("Error setting AE metering window. Metering will not work");
        }
        delete[] meteringWindows;
        meteringWindows = NULL;
    }

    // VideoThread must be the observer before PreviewThread to ensure that
    // the recording buffer dequeue handling message is guaranteed to happen
    // before any possible preview return buffer handlers. Since the preview
    // thread will get the observer notification later with this order, that is
    // guaranteed. Thus we know, that if the recording buffer is using the
    // preview buffer data for encoding, the handler for the recording buffer
    // dequeue has run before the preview return buffer handler runs.
    mISP->attachObserver(this, OBSERVE_PREVIEW_STREAM);
    if (videoMode && mExtIspAction == EXT_ISP_ACTION_NA)
        mISP->attachObserver(mVideoThread.get(), OBSERVE_PREVIEW_STREAM);
    mISP->attachObserver(mPreviewThread.get(), OBSERVE_PREVIEW_STREAM);
    if (state == STATE_JPEG_CAPTURE || (videoMode && mExtIspAction != EXT_ISP_ACTION_VIDEOHS &&
            (mode == MODE_CONTINUOUS_JPEG || mode == MODE_CONTINUOUS_JPEG_VIDEO))) {
        mISP->attachObserver(mPictureThread.get(), OBSERVE_PREVIEW_STREAM);
    }

    if (!mIspExtensionsEnabled) {

        mPreviewThread->detachCallback(NULL, ICallbackPreview::OUTPUT_WITH_DATA);

        if (mExtIspAction != EXT_ISP_ACTION_NA) {
            // PreviewThread->VideoThread is special flow for HAL video stabilization.
            // This can be done, since in our normal case video mode does not use post proc (Face detection etc, for now..)
            // In the normal flow VideoThread "pulls" the buffers from AtomISP,
            // and is attached to AtomISP using attachObserver() above, in halVS we "push" the frames via previewBufferCallback()
            mPreviewThread->setCallback(
                static_cast<ICallbackPreview*>(mVideoThread.get()),
                ICallbackPreview::OUTPUT_WITH_DATA);
        } else {
            mPreviewThread->setCallback(
                static_cast<ICallbackPreview*>(mPostProcThread.get()),
                ICallbackPreview::OUTPUT_WITH_DATA);
        }
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

        // initialize ISP mirror
        mISP->setIspMirror(getNeededIspFlipType());
    } else {
        LOGE("Error starting ISP!");
        mPreviewThread->returnPreviewBuffers();
        mISP->detachObserver(mPreviewThread.get(), OBSERVE_PREVIEW_STREAM);
        if (state == STATE_JPEG_CAPTURE || (videoMode && mExtIspAction != EXT_ISP_ACTION_VIDEOHS &&
                    (mode == MODE_CONTINUOUS_JPEG || mode == MODE_CONTINUOUS_JPEG_VIDEO)))
            mISP->detachObserver(mPictureThread.get(), OBSERVE_PREVIEW_STREAM);
        mISP->detachObserver(this, OBSERVE_PREVIEW_STREAM);
        if (videoMode)
            mISP->detachObserver(mVideoThread.get(), OBSERVE_PREVIEW_STREAM);

        if (m3AControls->isIntel3A()) {
            mISP->detachObserver(m3AThread.get(), OBSERVE_3A_STAT_READY);
        } else {
            if (PlatformData::supportsContinuousJpegCapture(mCameraId) &&
                mExtIspAction != EXT_ISP_ACTION_VIDEOHS)
                mISP->detachObserver(m3AThread.get(), OBSERVE_PREVIEW_STREAM);
            if (PlatformData::supportedSensorMetadata(mCameraId))
                mISP->detachObserver(m3AThread.get(), OBSERVE_3A_STAT_READY);
        }
    }

    // For ext-isp, enable AF here, after the preview is running
    if (PlatformData::supportsContinuousJpegCapture(mCameraId)) {
        // The initial parameters from app comes before startPreview()
        // but M10MO expects the Af mode be set after preview is running,
        // so need to do this here:
        AfMode initialAfMode = m3AControls->getAfMode();
        m3AControls->setAfMode(initialAfMode);
        // TODO: Need to set AF window here
        m3AControls->setAfEnabled(true);
    }

    if (videoMode) {
        /**
         * The CameraParameters must be changed for SDV configuration in video preview
         * But we don't want it affect the preview mode include video preview.
         * Restore the parameters here. It could be updated again when start recording.
         */
        sdvRestoreParams(false);

        // start monitoring the thermal throttle notify when recording in 60fps or
        // stop monitoring in normal fps case
        mParameters.getVideoSize(&width, &height);
        if (mISP->getRecordingFramerate() > DEFAULT_RECORDING_FPS
                        && mExtIspAction != EXT_ISP_ACTION_VIDEOHS
                        && height == RESOLUTION_1080P_HEIGHT) {
            mThermalThrottleThread->startMonitoring();
        } else if (mThermalThrottleThread->isMonitoring()) {
            mThermalThrottleThread->stopMonitoring();
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

    mPostProcThread->flushFrames();

    if (mState == STATE_JPEG_CAPTURE) {
        status = cancelPictureThread();
        if (status != NO_ERROR) {
            LOGE("Error canceling PictureThread!");
        }
    }

    State oldState = mState;
    status = mISP->stop();
    if (status == NO_ERROR) {
        mState = STATE_STOPPED;
    } else {
        LOGE("Error stopping ISP in preview mode!");
    }

    if (oldState == STATE_PREVIEW_VIDEO || oldState == STATE_RECORDING) {
        mISP->detachObserver(mVideoThread.get(), OBSERVE_PREVIEW_STREAM);
        status = mVideoThread->flushBuffers();
        deinitSdv(mFullSizeSdv);
    }

    mISP->detachObserver(mPreviewThread.get(), OBSERVE_PREVIEW_STREAM);

    if (oldState == STATE_JPEG_CAPTURE || mExtIspAction == EXT_ISP_ACTION_HALVS ||
        ((oldState == STATE_PREVIEW_VIDEO || oldState == STATE_RECORDING) && PlatformData::supportsContinuousJpegCapture(mCameraId))) {
        // External ISP video high speed doesn't need observing this stream
        if (mExtIspAction != EXT_ISP_ACTION_VIDEOHS)
            mISP->detachObserver(mPictureThread.get(), OBSERVE_PREVIEW_STREAM);
    }

    // we only need to attach the 3AThread to preview stream for RAW type of cameras
    // when we use the 3A algorithm running on Atom
    if (m3AControls->isIntel3A()) {
        mISP->detachObserver(m3AThread.get(), OBSERVE_3A_STAT_READY);
        // Detaching DVS observer. Just to make sure, although it might not be attached:
        // might be a non-RAW sensor, or enabling failed on startPreviewCore().
        // It is OK to detach; if the observer is not attached, detachObserver()
        // returns BAD_VALUE.
    } else {
        if (PlatformData::supportsContinuousJpegCapture(mCameraId))
            mISP->detachObserver(m3AThread.get(), OBSERVE_PREVIEW_STREAM);
        if (PlatformData::supportedSensorMetadata(mCameraId))
            mISP->detachObserver(m3AThread.get(), OBSERVE_3A_STAT_READY);
    }
    mISP->detachObserver(this, OBSERVE_PREVIEW_STREAM);

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
        LOGE("Must be in STATE_CAPTURE to stop capture");
        return INVALID_OPERATION;
    }
    if (mHdr.inProgress)
        mBracketManager->stopBracketing();

    status = cancelPictureThread();
    if (status != NO_ERROR) {
        LOGE("Error canceling PictureThread!");
        return status;
    }

    mPreviewThread->flushBuffers();

    status = mISP->stop();
    if (status != NO_ERROR) {
        LOGE("Error stopping ISP!");
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
    if (mState == STATE_CONTINUOUS_CAPTURE || mState == STATE_JPEG_CAPTURE)
        cancelPictureThread();

    stopFaceDetection(true);
    status_t status = stopPreviewCore();
    if (status == NO_ERROR && previewEn)
        status = startPreviewCore(videoMode);
    if (faceActive)
        startFaceDetection();

    // if restart preview in video mode, we need the message to update SDV parameter
    if (videoMode)
        mPreviewThread->setCallback(this, ICallbackPreview::INPUT_ONCE);
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
    cfg.offset = -(mISP->shutterLagZeroAlign());
    cfg.skip = 0;
    if (mBurstLength > 1)
        cfg.numCaptures = mBurstLength;
    else if (mContShootingState == CONT_SHOOTING_PREPARED)
        cfg.numCaptures = -1; // continuous shooting
    else
        cfg.numCaptures = 1;
    continuousConfigApplyLimits(cfg);

    // in case preview has just started, we need to limit
    // how long we can look back
    int framesDone = mPreviewThread->getFramesDone();
    if (framesDone < -cfg.offset)
        cfg.offset = -framesDone;

    // trigger shutter sound for offline burst
    if (mBurstLength > 1) {
        // control from the next frame. Maybe it's not very accurate, never mind.
        triggerOfflineCaptureControl(mBurstLength, NEXT_EID(mCurrentExpID));
    }

    mISP->startOfflineCapture(cfg);

    return NO_ERROR;
}

status_t ControlThread::handleMessageStartPreview()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    PERFORMANCE_TRACES_BREAKDOWN_STEP_NOPARAM();
    mCallbacksThread->resumePreviewCallbacks();
    if (mState == STATE_CAPTURE) {
        status = stopCapture();
        if (status != NO_ERROR) {
            LOGE("Could not stop capture before start preview!");
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
        stopFaceDetection();

        // for offline mode HDR
        if (mHdr.enabled || mHdr.inProgress)
            hdrRelease();

        if (mPreviewThread->isWindowConfigured() || mISP->isFileInjectionEnabled()
            || mPreviewUpdateMode == IntelCameraParameters::PREVIEW_UPDATE_MODE_WINDOWLESS) {
            LOGI("Preview windowless mode");
            bool videoMode = isParameterSet(CameraParameters::KEY_RECORDING_HINT);
            status = startPreviewCore(videoMode);
        } else {
            LOGI("Preview window not set deferring start preview until then");
            mPreviewThread->setPreviewState(PreviewThread::STATE_NO_WINDOW);
        }
    } else if (mState == STATE_CONTINUOUS_CAPTURE || mState == STATE_JPEG_CAPTURE) {
        // already in continuous-state
        status = handleContinuousPreviewForegrounding();
    } else if (mState == STATE_PREVIEW_VIDEO && PlatformData::supportsContinuousJpegCapture(mCameraId)) {
        // nothing to do, we didn't maybe stop preview after capture so just ignore the start
        status = OK;
    } else {
        LOGE("Invalid state for startPreview: %d", mState);
        status = INVALID_OPERATION;
    }

    if (status != NO_ERROR) {
        LOGE("Error starting preview. Invalid state!");
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
        LOGE("There was failures while canceling capture process");

    // In STATE_CAPTURE, preview is already stopped, nothing to do
    if (mState != STATE_CAPTURE) {
        stopFaceDetection(true);
        if (mState == STATE_CONTINUOUS_CAPTURE || mState == STATE_JPEG_CAPTURE) {
            status = handleContinuousPreviewBackgrounding();
            if (status == NO_ERROR)
                goto preview_stopped;
        }
        if (mState != STATE_STOPPED) {
            status = stopPreviewCore();
        } else {
            LOGE("Error stopping preview. Invalid state!");
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
        LOGE("Preview was stopped due error in stream, trying to recover...");
        // try to recover preview
        recoverPreview();
    } else {
        LOGE("Preview stream error unhandled, unexpected state (%d)", mState);
    }

    return status;
}

/**
 * Handler for MessageQueue::receive timeout
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
            LOGE("Error initializing ISP");
        } else {
            if (!PlatformData::useHALVS(mCameraId)) {
                status = mISP->initDVS();
                if (status != NO_ERROR) {
                    LOGE("Error in initializing DVS");
                    return status;
                }
            }
            bool videoMode = isParameterSet(CameraParameters::KEY_RECORDING_HINT) ? true : false;
            status = startPreviewCore(videoMode);
            if (status)
                LOGE("%s: Restart Preview failed", __FUNCTION__);
        }
        if (status != NO_ERROR) {
            // When recover preview failed, just send out error to app
            LOGW("When recover preview failed, send error to application.");
            mCallbacksThread->sendError(CAMERA_ERROR_SERVER_DIED);
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
        status = startPreviewCore(videoMode);
        if (status != NO_ERROR) {
            // When start preview failed, just send out error to app
            LOGW("When start preview failed, send error to application.");
            mCallbacksThread->sendError(CAMERA_ERROR_SERVER_DIED);
        }
    } else if (msg->window != NULL
        && mPreviewUpdateMode == IntelCameraParameters::PREVIEW_UPDATE_MODE_WINDOWLESS
        && currentState != PreviewThread::STATE_STOPPED) {
        // preview was started windowless, force back to standard and make it public
        mPreviewUpdateMode = IntelCameraParameters::PREVIEW_UPDATE_MODE_STANDARD;
        if (mIntelParamsAllowed) {
            mParameters.set(IntelCameraParameters::KEY_PREVIEW_UPDATE_MODE,
                            IntelCameraParameters::PREVIEW_UPDATE_MODE_STANDARD);
        }
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
               && (mState == STATE_CONTINUOUS_CAPTURE  || mState == STATE_JPEG_CAPTURE)) {
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
    int vidWidth, vidHeight, width, height;

    if (mState == STATE_PREVIEW_VIDEO) {
        mParameters.getVideoSize(&vidWidth, &vidHeight);
        mISP->getVideoSize(&width, &height, NULL);

        if (vidWidth != width || vidHeight != height) {
            LOG1("video size doesn't match, restartPreview %dx%d => %dx%d", width, height, vidWidth, vidHeight);
            bool videoMode = true;
            mISP->applyISPLimitations(&mParameters, mDvsEnable, videoMode);
            status = restartPreview(videoMode);
            if (status != NO_ERROR) {
                LOGE("Error restarting preview in video mode");
            }
        }
        mState = STATE_RECORDING;
    } else if (mState == STATE_PREVIEW_STILL ||
               mState == STATE_CONTINUOUS_CAPTURE ||
               mState == STATE_JPEG_CAPTURE) {
        /* We are in PREVIEW_STILL mode; in order to start recording
         * we first need to stop AtomISP and restart it with MODE_VIDEO
         */
        bool videoMode = true;
        mISP->applyISPLimitations(&mParameters, mDvsEnable, videoMode);
        status = restartPreview(videoMode);
        if (status != NO_ERROR) {
            LOGE("Error restarting preview in video mode");
        }
        mState = STATE_RECORDING;
    } else {
        LOGE("Error starting recording. Invalid state!");
        status = INVALID_OPERATION;
    }

    // update parameter for SDV
    sdvUpdateParams(mFullSizeSdv, true);

    mISP->setIspMirror(getNeededIspFlipType());
    // call video thread to start recording
    mVideoThread->startRecording();

    // return status and unblock message sender
    mMessageQueue.reply(MESSAGE_ID_START_RECORDING, status);
    return status;
}

status_t ControlThread::handleMessageStopRecording()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    // finalize on-going capture during video
    cancelCaptureSdv();
    // restore picture parameters
    sdvRestoreParams(true);

    if (mState == STATE_RECORDING) {
        // request video thread to stop recording
        status = mVideoThread->stopRecording();
        if (status != NO_ERROR)
            LOGE("Error in video thread stopRecording");
        /*
         * Even if startRecording was called from PREVIEW_STILL mode, we can
         * switch back to PREVIEW_VIDEO now since we got a startRecording
         */
        mState = STATE_PREVIEW_VIDEO;
    } else {
        LOGE("Error stopping recording. Invalid state!");
        status = INVALID_OPERATION;
    }

    // release buffers owned by encoder since it is not going to return them
    mISP->returnRecordingBuffers();
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
        LOGE("error input thumbnail height");
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
            LOGE("Error in grabbing warm-up frame %d!", i);
            return status;
        }
        status = mISP->putSnapshot(&snapshotBuffer, &postviewBuffer);
        if (status == DEAD_OBJECT) {
            LOG1("Stale snapshot buffer returned to ISP");
        } else if (status != NO_ERROR) {
            LOGE("Error in putting skip frame %d!", i);
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
            String8 sceneMode;
            bool sceneHdr = false;
            m3AThread->getCurrentSmartScene(sceneMode, sceneHdr);
            // Force XNR and ANR in case of lowlight scene
            if (sceneMode == "night_portrait" || sceneMode == "night") {
                LOG1("Low-light scene detected, forcing XNR and ANR");
                mHwcg.mIspCI->setXNR(true);
                // Forcing mParameters to true, to be in sync with app update.
                mParameters.set(IntelCameraParameters::KEY_XNR, "true");

                mHwcg.mIspCI->setLowLight(true);
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
        LOGE("Error %d capturing panorama picture.", status);
        return status;
    }

    mPanoramaThread->stitch(&snapshotBuffer, &postviewBuffer); // synchronous

    if (mState != STATE_CONTINUOUS_CAPTURE) {
        // we can return buffers now that panorama has (synchronously) processed
        // (copied) the buffers
        status = mISP->putSnapshot(&snapshotBuffer, &postviewBuffer);
        if (status != NO_ERROR)
            LOGE("error returning panorama capture buffers");

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
    mBurstBufsToReturn = 0;
}


status_t ControlThread::handleMessageTakePicture() {
    LOG1("@%s:", __FUNCTION__);
    status_t status = NO_ERROR;

    mShootingMode = selectShootingMode();
    LOG1("CaptureSubState %s -> STARTED", sCaptureSubstateStrings[mCaptureSubState]);
    mCaptureSubState = STATE_CAPTURE_STARTED;

    switch(mShootingMode) {

        case SHOOTING_MODE_SINGLE:
            if (PlatformData::isExtendedCamera(mCameraId))
                status = captureStillPicFromPreview();
            else
                status = captureStillPic();
            break;

        case SHOOTING_MODE_ZSL:
            status = captureStillPic();
            break;

        case SHOOTING_MODE_CONTINUOUS_SHOOTING:
            status = captureContinuousShooting(true);
            break;

        case SHOOTING_MODE_ZSL_BURST:
            status = captureFixedBurstPic(true);
            break;

        case SHOOTING_MODE_BURST:
            status = captureBurstPic(true);
            break;

        case SHOOTING_MODE_VIDEO_SNAP:
            if (PlatformData::sensorType(mCameraId) == SENSOR_TYPE_SOC
                    && PlatformData::useMultiStreamsForSoC(mCameraId))
                status = captureSdvSoC(mFullSizeSdv);
            else if (mExtIspAction == EXT_ISP_ACTION_VIDEOHS)
                status = captureSdv(false);
            else
                status = captureSdv(mFullSizeSdv);
            break;

        case SHOOTING_MODE_ULL:
            status = captureULLPic();
            break;

        case SHOOTING_MODE_JPEG:
            status = captureJpegPic();
            break;

        case SHOOTING_MODE_EXTISP_HDR_LLS:
            status = captureExtIspHDRLLSPic();
            break;

        case SHOOTING_MODE_SMARTSTABILIZATION:
            status = captureSmartStabilizationPic();
            break;

        default:
            LOGE("Taking picture when recording is not supported!");
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
            LOGE("%s: Error in grabbing snapshot!", __FUNCTION__);
            break;
        }

        if (snapshotBuffer->status == FRAME_STATUS_FLASH_EXPOSED) {
            LOG2("flash exposed, frame %d", cnt);
            break;
        }
        else if (snapshotBuffer->status  == FRAME_STATUS_FLASH_FAILED) {
            LOGE("%s: flash fail, frame %d", __FUNCTION__, cnt);
            break;
        }

        if (cnt++ == FLASH_TIMEOUT_FRAMES) {
            LOGE("%s: unexpected flash timeout, frame %d", __FUNCTION__, cnt);
            break;
        }

        mISP->putSnapshot(snapshotBuffer, postviewBuffer);
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
    int32_t numFaces = 0;
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
        if (!PlatformData::supportsContinuousJpegCapture(mCameraId)) // getExposureTime not available with ext-isp
            if (mHwcg.mSensorCI->getExposureTime(&aeConfig->expTime))
                aeConfig->expTime = 0;
    }

    //       SensorAeConfig information, so setting as NULL on purpose
    mBracketManager->getNextAeConfig(aeConfig);
    if (m3AControls->isIntel3A()) {
        // TODO: add support for raw mknote
        aaaMkNote = m3AControls->get3aMakerNote(ia_mkn_trg_section_1);
        if (!aaaMkNote)
            LOGW("No 3A makernote data available");
    }

    atomisp_makernote_info tmp;
    status_t status = mHwcg.mIspCI->getMakerNote(&tmp);
    if (status == NO_ERROR) {
        atomispMkNote = new atomisp_makernote_info;
        *atomispMkNote = tmp;
    }
    else {
        LOGW("Could not get AtomISP makernote information!");
    }

    numFaces = m3AThread->getFaceNum();
    if (numFaces > 0) {
        metaData.faceState.faces = new ia_face[numFaces];
        if (metaData.faceState.faces == NULL) {
            metaData.faceState.num_faces = 0;
            LOGE("Error allocation face detection memory");
        } else
            m3AThread->getFaces(metaData.faceState);
    } else {
        metaData.faceState.faces = NULL;
        metaData.faceState.num_faces = 0;
    }

    metaData.flashFired = flashFired;
    // note: the following may be null, if info not available
    metaData.aeConfig = aeConfig;
    metaData.ia3AMkNote = aaaMkNote;
    metaData.atomispMkNote = atomispMkNote;

    // disable sw mirror since ISP supports it
    metaData.saveMirrored = false;
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
    stopFaceDetection();

    if (mState != STATE_CONTINUOUS_CAPTURE) {
        status = stopPreviewCore();
        if (status != NO_ERROR) {
            LOGE("Error stopping preview!");
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
    mPictureThread->initialize(mParameters, mHwcg.mIspCI->zoomRatio(mParameters.getInt(CameraParameters::KEY_ZOOM)));

    // configure thumbnail size
    thumbnailWidth = mParameters.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH);
    thumbnailHeight= mParameters.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT);
    mPanoramaThread->setThumbnailSize(thumbnailWidth, thumbnailHeight);

    status = setExternalSnapshotBuffers(fourcc, width, height);
    if (status != NO_ERROR) {
        LOGE("Error %d in capture panoroma picture when set snapshot buffers", status);
        return status;
    }

    if (mState != STATE_CONTINUOUS_CAPTURE) {
        // Configure and start the ISP
        AtomBuffer formatDescriptorSs
            = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_FORMAT_DESCRIPTOR, fourcc, width, height);
        AtomBuffer formatDescriptorPv
            = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_FORMAT_DESCRIPTOR, selectPostviewFormat(), lpvWidth, lpvHeight);

        mISP->setSnapshotFrameFormat(formatDescriptorSs);
        mISP->setPostviewFrameFormat(formatDescriptorPv);

        if ((status = mISP->configure(MODE_CAPTURE)) != NO_ERROR) {
            LOGE("Error configuring the ISP driver for CAPTURE mode");
            return status;
        }

        status = mISP->allocateBuffers(MODE_CAPTURE);
        if (status != NO_ERROR) {
            LOGE("Error allocate buffers in ISP");
            return status;
        }

        if (m3AControls->isIntel3A()
            && (m3AThread->switchModeAndRate(MODE_CAPTURE, mHwcg.mSensorCI->getFramerate()) != NO_ERROR)) {
            LOGE("Failed to switch 3A to capture mode at %.2f fps",mHwcg.mSensorCI->getFramerate());
        }

        if ((status = mISP->start()) != NO_ERROR) {
            LOGE("Error starting the ISP driver in CAPTURE mode!");
            return status;
        }
    } else {
        /* Necessary to update the buffer pools before we start to capture */
        status = mISP->allocateBuffers(MODE_CAPTURE);
        if (status != NO_ERROR) {
            LOGE("Error allocate buffers in ISP");
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
        int warmUpFrames = PlatformData::getNumOfCaptureWarmUpFrames(mCameraId);
        // Only try to skip, if it is meaningful (more than 0 frames)
        if (warmUpFrames > 0 && (status = skipFrames(warmUpFrames)) != NO_ERROR) {
            LOGE("Error skipping warm-up frames!");
            return status;
        }
    }

    // Turn off flash
    mHwcg.mFlashCI->setFlashIndicator(0);

    // Get the snapshot
    if ((status = mISP->getSnapshot(&snapshotBuffer, &postviewBuffer)) != NO_ERROR) {
        LOGE("Error in grabbing snapshot!");
        return status;
    }

    if (mState == STATE_CONTINUOUS_CAPTURE)
        stopOfflineCapture();

    snapshotBuffer.owner = NULL;

    mCallbacksThread->shutterSound();

    return status;
}

void ControlThread::recycleUnusedBufferInISP()
{
    LOGI("@%s buffers in ISP %d,%d", __FUNCTION__, mSnapshotBuffersInISP.size(), mPostviewBuffersInISP.size());
    if (mSnapshotBuffersInISP.size() != mPostviewBuffersInISP.size()) {
        LOGW("@%s buffer number snapshot(%d) != postview(%d), find the bug", __FUNCTION__,
                mSnapshotBuffersInISP.size(), mPostviewBuffersInISP.size());
    }

    AtomBuffer snapshotBuffer, postviewBuffer;
    while (mSnapshotBuffersInISP.size()) {
        getSnapshot(&snapshotBuffer, &postviewBuffer);
        mAvailableSnapshotBuffers.push(snapshotBuffer);
        mAvailablePostviewBuffers.push(postviewBuffer);
        LOG1("%s snapshot buffer %p back to available queue", __FUNCTION__, snapshotBuffer.dataPtr);
    }
}

void ControlThread::stopOfflineCapture()
{
    LOG1("@%s: ", __FUNCTION__);
    if ((mState == STATE_CONTINUOUS_CAPTURE || mState == STATE_RECORDING) &&
            mISP->isOfflineCaptureRunning()) {
        resetOfflineCaptureControl();
        recycleUnusedBufferInISP();
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
            LOGE("Error skipping burst frames!");
        }
    }
    return status;
}

/**
 * Starts the capture process in continuous capture mode or continuous video mode.
 */
status_t ControlThread::continuousStartStillCapture(bool useFlash)
{
    LOG2("@%s: ", __FUNCTION__);
    status_t status = NO_ERROR;
    int picWidth, picHeight, fourcc;
    int size;

    if (useFlash == false) {
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

        status = setExternalSnapshotBuffers(fourcc, picWidth, picHeight);
        if (status != NO_ERROR) {
            LOGE("Error %d in starting contunuous capture when set snapshot buffers", status);
            return status;
        }

        status = mISP->allocateBuffers(MODE_CAPTURE);
        if (status != NO_ERROR) {
           LOGE("Error allocate buffers in ISP");
            return status;
        }

        // offline bracketing start capture inside BracketManager
        if (mBurstLength > 1 && mBracketManager->getBracketMode() != BRACKET_NONE)
            return status;

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
            LOGE("Error stopping preview!");
    }
    return status;
}

bool ControlThread::selectSdvSize(int &width, int &height)
{
    LOG1("@%s ", __FUNCTION__);

    Vector<Size> supportedSizes;
    int vidWidth, vidHeight;
    float vidAspect = 0.0f, picAspect = 0.0f;

    mParameters.getVideoSize(&vidWidth, &vidHeight);
    vidAspect = static_cast<float>(vidWidth) / static_cast<float>(vidHeight);

    //find proper picture size in supported SDV list
    parseSizesList(PlatformData::supportedSdvSizes(mCameraId), supportedSizes);
    for (unsigned int i = 0 ; i < supportedSizes.size() ; i++) {
        picAspect = static_cast<float>(supportedSizes[i].width) / static_cast<float>(supportedSizes[i].height);
        if (fabsf(picAspect - vidAspect) < 0.009f) {
            width  = supportedSizes[i].width;
            height = supportedSizes[i].height;
            LOG1("@%s prefer picture size:%dx%d", __FUNCTION__, width, height);
            return true;
        }
    }

    // todo remove this block when ext-isp has proper aspect capture size(s) available
    if (PlatformData::supportsContinuousJpegCapture(mCameraId) &&
        ((vidWidth == 720 && vidHeight == 480) ||
         (vidWidth == 176 && vidHeight == 144)) &&
        supportedSizes.size() > 0) {
        // since there is no proper aspect ratio capture yet, we will have to just pick
        // one for the ext-isp case
        width = supportedSizes[0].width;
        height = supportedSizes[0].height;
        return true;
    }

    LOGW("failed to select a proper size for video snapshot");
    return false;
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
        LOGW("Postview aspect difference (%f) beyond tolerance (%f)",
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
        LOGW("Thumbnail size doesn't match the picture aspect"
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

    if (mHdr.enabled) {
        // HDR library only support NV12 format.
        fourcc = V4L2_PIX_FMT_NV12;
    } else if (mPanoramaThread->getState() == PANORAMA_STOPPED) {
        fourcc = PlatformData::getPreviewPixelFormat(mCameraId);
    } else {
        fourcc = V4L2_PIX_FMT_NV21;
    }

    return fourcc;
}

void ControlThread::resetOfflineCaptureControl()
{
    mSkipPreview = false;
    mNextExpID   = EXP_ID_INVALID;
    mNumSounds   = 0;
    mNumCaptures = 0;
}

void ControlThread::triggerOfflineCaptureControl(int numSounds, int startId, bool skip)
{
    LOG1("@%s num sound:%d start exp id:%d skip:%d", __FUNCTION__, numSounds, startId, skip);
    Mutex::Autolock lock(mOfflineControlLock);
    if (startId < EXP_ID_MIN || startId > EXP_ID_MAX)
        return;

    mSkipPreview = skip;
    mNextExpID   = startId;
    mNumSounds   = numSounds;
    mNumCaptures = mBurstLength;
}

/**
 * Controlling for offline features such as:
 * 1. offline burst;
 * 2. offline bracketing;
 * 3. offline HDR;
 * Works:
 * 1. Unlock or capture locked raw buffer if need
 * 2. Shutter sound control
 * 3. Skip buffer display in viewfinder if need
 */
void ControlThread::handleOfflineCaptureControl(AtomBuffer *buff)
{
    status_t status = NO_ERROR;
    bool unlockIt = true;

    Mutex::Autolock lock(mOfflineControlLock);
    // update current exposure ID
    mCurrentExpID = buff->expId;

    LOG2("@%s for id:%d", __FUNCTION__, mCurrentExpID)
    if (mNumSounds > 0 || mNumCaptures > 0) {
        LOG1("@%s num sound:%d current ID:%d expected exposure id:%d", __FUNCTION__,
                mNumSounds, mCurrentExpID, mNextExpID);

        if (mCurrentExpID > mNextExpID && ((mCurrentExpID - mNextExpID) < (EXP_ID_MAX >> 1))) {
            // trigger late or frame droped
            LOGW("late trigger comes at :%d while current is :%d", mNextExpID, mCurrentExpID);
            mNextExpID = mCurrentExpID;
        }

        if (mCurrentExpID == mNextExpID) {
            // play shutter sound if need
            if (mNumSounds > 0) {
                mCallbacksThread->shutterSound();
                mNumSounds--;
            }

            if (mNumCaptures > 0) {
                // trigger locked buffer capture if in lock mode
                if (mRawBufferLockMode) {
                    mISP->rawBufferCapture(mCurrentExpID);
                    unlockIt = false;
                }
                mNumCaptures--;
            }

            // skip to show in viewfinder, currently for hdr only
            if (mSkipPreview) {
                buff->status = FRAME_STATUS_SKIPPED;
            }

            // the next
            if (mFpsAdaptSkip > 0) {
                mNextExpID = NEXTN_EID(mNextExpID, mFpsAdaptSkip + 1);
            } else {
                mNextExpID = NEXT_EID(mNextExpID);
            }
        }
    }

    if (mRawBufferLockMode && unlockIt) {
        //unlock the buffer if it will not be used for capture
        status = mISP->rawBufferUnlock(mCurrentExpID);
        if (status != NO_ERROR) {
            LOGE("Error in unlocking raw buffer :%d", mCurrentExpID);
        }
    }
}

status_t ControlThread::getSnapshot(AtomBuffer *snapshotBuf, AtomBuffer *postviewBuf)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    status = mISP->getSnapshot(snapshotBuf, postviewBuf);
    if (status == NO_ERROR) {
        rmBufferInQueue(snapshotBuf, &mSnapshotBuffersInISP);
        rmBufferInQueue(postviewBuf, &mPostviewBuffersInISP);
    }
    LOG1("@%s ok", __FUNCTION__);
    return status;
}

status_t ControlThread::putSnapshot(AtomBuffer *snapshotBuf, AtomBuffer *postviewBuf)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    status = mISP->putSnapshot(snapshotBuf, postviewBuf);
    if (status == NO_ERROR) {
        mSnapshotBuffersInISP.push(*snapshotBuf);
        mPostviewBuffersInISP.push(*postviewBuf);
    }
    LOG1("@%s ok", __FUNCTION__);
    return status;
}

status_t ControlThread::captureStillPic()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    AtomBuffer snapshotBuffer
            = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_SNAPSHOT);
    AtomBuffer postviewBuffer
            = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_POSTVIEW);
    int width, height, fourcc, size;
    int pvWidth, pvHeight, pvSize;
    FlashMode flashMode = m3AControls->getAeFlashMode();
    FlashStage flashStage(CAM_FLASH_STAGE_NONE);
    bool flashFired = false;
    bool flashSequenceStarted = false;
    // Decide whether we display the postview
    bool postviewDisplayable = selectPostviewSize(pvWidth, pvHeight);
    bool displayPostview = postviewDisplayable                   // postview matches size of preview
                           && !mHdr.enabled                      // HDR not enabled
                           && !(mBurstLength > 1 && mState == STATE_CONTINUOUS_CAPTURE) // not offline burst
                           && (mPreviewUpdateMode == IntelCameraParameters::PREVIEW_UPDATE_MODE_STANDARD
                              || mBurstLength > 1)               // proprietary preview update mode or online burst
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

    stopFaceDetection();

    sp<AutoReset> autoReset;

    if (m3AControls->isIntel3A()
        && flashMode != CAM_AE_FLASH_MODE_TORCH
        && flashMode != CAM_AE_FLASH_MODE_OFF) {

        // If flash mode is not TORCH or OFF, check need for flash
        if (m3AControls->getAeLock()) {
            LOG1("AE was locked in %s, using old flash decision from AE "
                 "locking time (%d)", __FUNCTION__, mAELockFlashStage);
            flashStage = mAELockFlashStage;
        } else {
            flashStage = m3AControls->getAeFlashNecessity();
        }

        if (flashStage == CAM_FLASH_STAGE_PRE) {
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
                LOGE("Pre-flash sequence failed");
                flashStage = CAM_FLASH_STAGE_NONE;
            } else {
                flashStage = m3AControls->getAeFlashNecessity();
            }

            // display postview when flash is triggered
            // regardless of preview update mode
            displayPostview = postviewDisplayable;
        }
    }

    // set isp mirror for capture
    mISP->setIspMirror(getNeededIspFlipType());

    if (mState == STATE_CONTINUOUS_CAPTURE) {
        // initialize offline bracketing
        if (mBurstLength > 1 && mBracketManager->getBracketMode() != BRACKET_NONE) {
            mBracketManager->initBracketing(mBurstLength, mFpsAdaptSkip, IMPL_OFFLINE);
        }
        bool useFlash = flashStage == CAM_FLASH_STAGE_MAIN;
        status = continuousStartStillCapture(useFlash);
    } else {
        status = stopPreviewCore();
        if (status != NO_ERROR) {
            LOGE("Error stopping preview!");
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
    // Get the current params
    mParameters.getPictureSize(&width, &height);
    fourcc = mISP->getSnapshotPixelFormat();
    size = frameSize(fourcc, width, height);
    pvSize = frameSize(fourcc, pvWidth, pvHeight);

    // Configure PictureThread
    mPictureThread->initialize(mParameters, mHwcg.mIspCI->zoomRatio(mParameters.getInt(CameraParameters::KEY_ZOOM)));

    if (mState != STATE_CONTINUOUS_CAPTURE) {
        // Possible smart scene parameter changes (XNR, ANR)
        if ((status = setSmartSceneParams()) != NO_ERROR)
            LOG1("set smart scene parameters failed");

        // Configure and start the ISP
        AtomBuffer formatDescriptorSs
            = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_FORMAT_DESCRIPTOR, fourcc, width, height);
        AtomBuffer formatDescriptorPv
            = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_FORMAT_DESCRIPTOR, selectPostviewFormat(), pvWidth, pvHeight);
        formatDescriptorSs.bpl = formatDescriptorSs.width; // initial guess
        mISP->setSnapshotFrameFormat(formatDescriptorSs);
        mISP->setPostviewFrameFormat(formatDescriptorPv);

        status = setExternalSnapshotBuffers(fourcc, width, height);
        if (status != NO_ERROR) {
            LOGE("Error %d in still capture when set snapshot buffers", status);
            return status;
        }

        // Initialize bracketing manager before streaming starts
        if (mBurstLength > 1 && mBracketManager->getBracketMode() != BRACKET_NONE) {
            mBracketManager->initBracketing(mBurstLength, mFpsAdaptSkip, IMPL_ONLINE);
        }

        if ((status = mISP->configure(MODE_CAPTURE)) != NO_ERROR) {
            LOGE("Error configuring the ISP driver for CAPTURE mode");
            return status;
        }

        status = mISP->allocateBuffers(MODE_CAPTURE);
        if (status != NO_ERROR) {
            LOGE("Error allocate buffers in ISP");
            return status;
        }

        if (m3AControls->isIntel3A()
            && (m3AThread->switchModeAndRate(MODE_CAPTURE, mHwcg.mSensorCI->getFramerate()) != NO_ERROR)) {
            LOGE("Failed to switch 3A to capture mode at %.2f fps",mHwcg.mSensorCI->getFramerate());
        }
        if ((status = mISP->start()) != NO_ERROR) {
            LOGE("Error starting the ISP driver in CAPTURE mode");
            return status;
        }
    }

    // Start the actual bracketing sequence
    if (mBurstLength > 1 && mBracketManager->getBracketMode() != BRACKET_NONE) {
        int startExpId;
        mBracketManager->startBracketing(&startExpId);
        // offline bracketing
        if (mState == STATE_CONTINUOUS_CAPTURE) {
            if (mHdr.enabled) // HDR capture 3 with 1 shutter sound only
                triggerOfflineCaptureControl(1, startExpId, true);
            else
                triggerOfflineCaptureControl(mBurstLength, startExpId);
        }
    }

    // HDR init
    if (mHdr.enabled &&
       (status = hdrInit( pvSize, pvWidth, pvHeight)) != NO_ERROR) {
        LOGE("Error initializing HDR!");
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
                RAW_CAPTURE_SKIP : PlatformData::getNumOfCaptureWarmUpFrames(mCameraId);

        if ((status = skipFrames(framesToSkip)) != NO_ERROR) {
            LOGE("Error skipping warm-up frames!");
            return status;
        }
    }

    // Turn on flash. If flash mode is torch, then torch is already on
    if (flashStage == CAM_FLASH_STAGE_MAIN && mBurstLength <= 1) {
        if (mHwcg.mFlashCI->setFlash(1) != NO_ERROR) {
            LOGE("Failed to enable the Flash!");
        }
        else {
            flashFired = true;
        }
    }

    status = burstCaptureSkipFrames();
    if (status != NO_ERROR) {
        LOGE("Error skipping burst frames!");
        return status;
    }

    if (mState == STATE_CONTINUOUS_CAPTURE) {
        status = waitForCaptureStart();
        if (status != NO_ERROR) {
            LOGE("Error while waiting for capture to start");
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

    if (mDepthMode && !PlatformData::isExtendedCamera(mCameraId)) {
        // send sensor frame id to application
        LOG2("send sensorFrameId (%d) to application", snapshotBuffer.sensorFrameId);
        mCallbacksThread->sendFrameId(snapshotBuffer.sensorFrameId);
    }

    if (mRawBufferLockMode)
        mISP->rawBufferUnlock(snapshotBuffer.expId);

    if (status != NO_ERROR) {
        LOGE("Error in grabbing snapshot!");
        return status;
    }

    // Configure PictureThread
    mPictureThread->initialize(mParameters, mHwcg.mIspCI->zoomRatio(mParameters.getInt(CameraParameters::KEY_ZOOM)));

    PerformanceTraces::ShutterLag::snapshotTaken(&snapshotBuffer.capture_timestamp);

    PictureThread::MetaData picMetaData;
    fillPicMetaData(picMetaData, flashFired);

    // HDR Processing
    if (mHdr.enabled &&
       (status = hdrProcess(&snapshotBuffer, &postviewBuffer)) != NO_ERROR) {
        LOGE("HDR: Error in compute CDF for capture %d in HDR sequence!", mBurstCaptureNum);
        picMetaData.free(m3AControls);
        return status;
    }

    mBurstCaptureNum++;

    // here the shutter sound is for single capture or online burst
    if (mBurstLength <= 1 || mState != STATE_CONTINUOUS_CAPTURE)
        mCallbacksThread->shutterSound();

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
        LOGE("Error skipping burst frames!");
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
        LOGE("Error in grabbing snapshot!");
        return status;
    }

    if (displayPostview)
        mPreviewThread->postview(&postviewBuffer, false);

    PictureThread::MetaData picMetaData;
    fillPicMetaData(picMetaData, false);

   // HDR Processing
    if ( mHdr.enabled &&
        (status = hdrProcess(&snapshotBuffer, &postviewBuffer)) != NO_ERROR) {
        LOGE("Error processing HDR!");
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
            LOGE("Error composing HDR picture");
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
 * Prepare for continuous shooting
 */
status_t ControlThread::prepareContinuousShooting()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    if (PlatformData::supportsContinuousJpegCapture(mCameraId)) {
        stopFaceDetection();
        return startJpegPicContinuousShooting();
    }

    if (mState != STATE_CONTINUOUS_CAPTURE || !mContShootingEnabled) {
        LOGE("Invalid calling for preparing continuous capture in state:%d enabled:%d",
                mState, mContShootingEnabled);
        return INVALID_OPERATION;
    }

    // Configure PictureThread
    mPictureThread->initialize(mParameters, mHwcg.mIspCI->zoomRatio(mParameters.getInt(CameraParameters::KEY_ZOOM)));
    stopFaceDetection();

    mContShootingState = CONT_SHOOTING_PREPARED;
    mContinuousPicsReady = 0;
    return status;
}

/**
 * This method is used to restore all snapshot buffers forcibly according to allocated list
 * It can only be used after cancelPictureThread(). Not recommanded to call it elsewhere
 *
 * CallbacksThread::flushPictures is not reliable to recycle all snapshot buffers because its
 * FLUSH cmd is asynchronous. mBuffers in it would be neglected just due to this.
 */
void ControlThread::forceRestoreSnapshotPostviewBuffers()
{
    if (mAvailableSnapshotBuffers.size() == mAllocatedSnapshotBuffers.size())
        return;

    Vector<AtomBuffer>::iterator it = mAllocatedSnapshotBuffers.begin();
    for (;it != mAllocatedSnapshotBuffers.end(); ++it) {
        if (!findBufferByData(it, &mAvailableSnapshotBuffers)) {
            LOG1("@%s snapshot buffer id:%d", __FUNCTION__, it->id);
            mAvailableSnapshotBuffers.push(*it);
        }
    }

    it = mAllocatedPostviewBuffers.begin();
    for (;it != mAllocatedPostviewBuffers.end(); ++it) {
        if (!findBufferByData(it, &mAvailablePostviewBuffers)) {
            LOG1("@%s postview buffer id:%d", __FUNCTION__, it->id);
            mAvailablePostviewBuffers.push(*it);
        }
    }
}

/**
 * Finalize continuous shooting
 */
status_t ControlThread::finalizeContinuousShooting()
{
    if (PlatformData::supportsContinuousJpegCapture(mCameraId))
        return stopJpegPicContinuousShooting();

    mContShootingState = CONT_SHOOTING_NONE;
    stopOfflineCapture();
    cancelPictureThread();
    forceRestoreSnapshotPostviewBuffers();
    mContinuousPicsReady = 0;
    // Call to finalizeContinuousShooting means first time continuous shooting has finished
    // User released the capture button. Now we will skip first frame in the next time continuous shooting
    mContShootingSkipFirstFrame = true;
    return NO_ERROR;
}

bool ControlThread::holdOnContinuousShooting()
{
    static const int MAX_NUM_PICTRUE_WAITING = 2;
    return mContinuousPicsReady >= MAX_NUM_PICTRUE_WAITING;
}

/**
 * Starts continuous shooting.
 */
status_t ControlThread::captureContinuousShooting(bool clientRequest = false)
{
    LOG1("@%s client request:%d ", __FUNCTION__, clientRequest);
    status_t status = NO_ERROR;

    AtomBuffer snapshotBuffer
            = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_SNAPSHOT);
    AtomBuffer postviewBuffer
            = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_POSTVIEW);

    assert(mState == STATE_CONTINUOUS_CAPTURE);
    PERFORMANCE_TRACES_SHOT2SHOT_TAKE_PICTURE_HANDLE();

    if (clientRequest) {
        mCallbacksThread->requestTakePicture(true, true);
        mContinuousPicsReady--;

        // Check whether more frames are needed
        if (compressedFrameQueueFull())
            return NO_ERROR;
    }

    if (holdOnContinuousShooting()) {
        LOG1("@%s enough buffer :%d ready", __FUNCTION__, mContinuousPicsReady);
        return NO_ERROR;
    }

    if (!mISP->isOfflineCaptureRunning()) {
        // css will be truly start when user trigger the first capture.
        status = continuousStartStillCapture(false);
        if (status != NO_ERROR) {
            LOGE("Error %d in preparing continuous capture", status);
            return status;
        }

        mContShootingState = CONT_SHOOTING_STARTED;

        status = waitForCaptureStart();
        if (status != NO_ERROR) {
            LOGE("Error while waiting for capture to start");
            stopOfflineCapture();
            return status;
        }

        /**
         * FIXME: remove the code of skipping one frame when FW bug:3039 is fixed.
         */
        if (mContShootingSkipFirstFrame) {
            status = getSnapshot(&snapshotBuffer, &postviewBuffer);
            if (status != NO_ERROR) {
                LOGE("%s Error in grabbing snapshot!", __FUNCTION__);
                stopOfflineCapture();
                return status;
            }

            LOG1("Continuous shooting skip the first buffer exp id:%d", snapshotBuffer.expId);

            status = putSnapshot(&snapshotBuffer, &postviewBuffer);
            if (status != NO_ERROR) {
                LOGE("Error %d in putting snapshot buffer:%p for continuous shooting", status,
                        snapshotBuffer.dataPtr);
            }
        }
    }

    if (clientRequest)
        mCallbacksThread->shutterSound();

    // Get the snapshot
    status = getSnapshot(&snapshotBuffer, &postviewBuffer);
    if (status != NO_ERROR) {
        LOGE("%s Error in grabbing snapshot!", __FUNCTION__);
        stopOfflineCapture();
        return status;
    }
    PERFORMANCE_TRACES_BREAKDOWN_STEP_PARAM("ISPGotFrame", snapshotBuffer.frameCounter);

    // Do jpeg encoding
    PictureThread::MetaData picMetaData;
    fillPicMetaData(picMetaData, false);
    LOG1("TEST-TRACE: starting picture encode: Time: %lld", systemTime());
    status = mPictureThread->encode(picMetaData, &snapshotBuffer, &postviewBuffer);
    mContinuousPicsReady++;

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

    // Note: Postview is not displayed with any of fixed burst scenarios,
    //       just having it here for conformity and noticing.
    //       Continuous mode with negative mBurstStart index would lead to
    //       disordered displaying of postview and preview frames.
    bool displayPostview = false;

    assert(mState == STATE_CONTINUOUS_CAPTURE);

    if (clientRequest) {
        mCallbacksThread->requestTakePicture(true, true);

        // Check whether more frames are needed
        if (compressedFrameQueueFull())
            return NO_ERROR;

        // Check if ISP has free buffers we can use
        if (!mISP->dataAvailable()) {
            // If ISP has no data, do nothing and return
            return NO_ERROR;
        }
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

    if (mBurstLength > 1 && mBracketManager->getBracketMode() != BRACKET_NONE) {
        status = mBracketManager->getSnapshot(snapshotBuffer, postviewBuffer);
    } else {
        status = mISP->getSnapshot(&snapshotBuffer, &postviewBuffer);
    }

    if (mRawBufferLockMode)
        mISP->rawBufferUnlock(snapshotBuffer.expId);

    if (status != NO_ERROR) {
        LOGE("Error in grabbing snapshot!");
        picMetaData.free(m3AControls);
        stopOfflineCapture();
        burstStateReset();
        return status;
    }

    // HDR Processing
    if ( mHdr.enabled &&
        (status = hdrProcess(&snapshotBuffer, &postviewBuffer)) != NO_ERROR) {
        LOGE("Error processing HDR!");
        picMetaData.free(m3AControls);
        stopOfflineCapture();
        burstStateReset();
        return status;
    }

    mBurstCaptureNum++;

    if (displayPostview)
        mPreviewThread->postview(&postviewBuffer, false);

    bool hdrSaveOrig = (mHdr.enabled && mHdr.saveOrig && picMetaData.aeConfig->evBias == 0);
    if (!mHdr.enabled || hdrSaveOrig) {
        if (hdrSaveOrig)
            snapshotBuffer.status = FRAME_STATUS_SKIPPED;
        // Do jpeg encoding
        LOG1("TEST-TRACE: starting picture encode: Time: %lld", systemTime());
        status = mPictureThread->encode(picMetaData, &snapshotBuffer, &postviewBuffer);
    } else {
        picMetaData.free(m3AControls);
    }

    // If all captures have been requested, ISP capture device
    // can be stopped. Otherwise requeue buffers back to ISP.
    if (mBurstCaptureNum == mBurstLength && mBurstLength > 1) {
        if (mBracketManager->getBracketMode() != BRACKET_NONE ) {
            LOG1("@%s: Bracketing done, got all %d snapshots", __FUNCTION__, mBurstLength);
            mBracketManager->stopBracketing();
        }
        stopOfflineCapture();
    }

    if (mHdr.enabled && mBurstCaptureNum == mHdr.bracketNum) {
        // This was the last capture in HDR sequence, compose the final HDR image
        LOG1("HDR: last capture, composing HDR image...");
        status = hdrCompose();
        if (status != NO_ERROR)
            LOGE("Error composing HDR picture");
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

    ia_binary_data aiqb_data;
    CLEAR(aiqb_data);
    status = m3AControls->getAiqConfig(&aiqb_data);
    if (status != NO_ERROR)
        LOGW("@%s: cannot retrieve CPF binary data for ULL capture", __FUNCTION__);

    status = mULL->init(picWidth,picHeight,0,&aiqb_data);
    if (status != NO_ERROR) {
      mULL->deinit();
      LOGE("Failed to initialize the ULL algorithm");
      return NO_INIT;
    }

    PERFORMANCE_TRACES_SHOT2SHOT_TAKE_PICTURE_HANDLE();

    mCallbacksThread->requestTakePicture(true, true, displayPostview);

    stopFaceDetection();
    // Initialize the burst control variables for the ULL burst
    mBurstLength = mUllBurstLength;
    mBurstStart = 0;

    status = continuousStartStillCapture(false);

    // Configure PictureThread, inform of the picture and thumbnail resolutions
    mPictureThread->initialize(mParameters, mHwcg.mIspCI->zoomRatio(mParameters.getInt(CameraParameters::KEY_ZOOM)));

    // Let application know that we are going to produce an ULL image
    mCallbacksThread->ullTriggered(mULL->getCurrentULLid());

    // Get the snapshots
    for (int i=0; i< mBurstLength; i++) {
        status = mISP->getSnapshot(&snapshotBuffer, &postviewBuffer);
        if (status != NO_ERROR) {
            LOGE("Error in grabbing snapshot!");
            goto exit;
        }
        if (i == 0) {
            PerformanceTraces::ShutterLag::snapshotTaken(&snapshotBuffer.capture_timestamp);

            // request shutter sound only for first image
            mCallbacksThread->shutterSound();

            PictureThread::MetaData firstPicMetaData;
            PictureThread::MetaData ullPicMetaData;
            fillPicMetaData(firstPicMetaData, false);
            fillPicMetaData(ullPicMetaData, false);
            ia_aiq_exposure_parameters exposure;
            CLEAR(exposure);
            m3AControls->getExposureParameters(&exposure);
            mULL->addSnapshotMetadata(ullPicMetaData, exposure);

            mULL->allocateCopyBuffers(mISP->getSnapshotDescriptor(), mISP->getPostviewDescriptor());

            AtomBuffer *snapshotCopy = mULL->getSnapshotCopyZoom(&snapshotBuffer);
            AtomBuffer *postviewCopy = mULL->getPostviewCopyZoom(&postviewBuffer);

            if (displayPostview)
                mPreviewThread->postview(postviewCopy, true);
            /*
             *  Mark the snapshot as skipped.
             *  This is done so that the snapshot buffer is not made available after
             *  the JPEG encoding. This buffer will be made available after
             *  the ULL processing completes.
             *  By "making available" we mean the buffer is to be pushed to the
             *  mAvailableSnapshotBuffers vector
             */
            snapshotBuffer.status = FRAME_STATUS_SKIPPED;
            snapshotCopy->status = FRAME_STATUS_SKIPPED;
            status = mPictureThread->encode(firstPicMetaData, snapshotCopy, postviewCopy);
            if (status != NO_ERROR) {
                // normally this is done by PictureThread, but as no
                // encoding was done, free the allocated metadata
                firstPicMetaData.free(m3AControls);
                LOGE("Error encoding first image of the ULL burst");
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

status_t ControlThread::captureExtIspHDRLLSPic()
{
    LOG1("@%s: ", __FUNCTION__);
    status_t status = OK;

    // pause preview observer, because extisp preview dies during HDR/LLS capture anyway,
    // plus we want full control here for grabbing the frames (incl. preview for thumb)
    mISP->pauseObserver(OBSERVE_PREVIEW_STREAM);

    // flush picturethread, because our driver seems to deadlock if picturethread
    // returns buffers at the same time we try to do getSnapshot.
    mPictureThread->flushBuffers();

    // Notify CallbacksThread that a picture was requested, so grab one from queue
    bool syncJpegCbWithPostview = false;
    bool requestPostviewCallback = false; // TODO CJC: true;
    bool requestRawCallback = true;
    mCallbacksThread->requestTakePicture(requestPostviewCallback, requestRawCallback, syncJpegCbWithPostview);

    // request the capture - note preview dies in HDR/LLS mode, so we don't seem to get
    // reliably a preview frame for thumb before the YUV images for HDR
    mISP->requestJpegCapture();

    // Send request to play the Shutter Sound
    mCallbacksThread->shutterSound();

    // now fetch the buffers from snapshot device
    AtomBuffer yuvBuffers[5];
    memset(yuvBuffers, 0, sizeof(yuvBuffers));
    AtomBuffer buffer = AtomBufferFactory::createAtomBuffer();
    bool succesfull = false;
    const int MAX_FRAME_GRABS = 16;
    char *hdrinfo = NULL;
    for (int i = 0; i < MAX_FRAME_GRABS; i++) {
        mISP->getSnapshot(&buffer, NULL);
        buffer.owner = mISP;
        hdrinfo = ((char*)buffer.dataPtr) + HDR_INFO_START;
        if (strncmp((char*) &hdrinfo[HDR_INFO_START_MARKER_ADDR], HDR_INFO_START_MARKER, sizeof(HDR_INFO_START_MARKER) - 1) == 0) {

            int frameIndex = hdrinfo[HDR_INFO_COUNT_ADDR];
            yuvBuffers[frameIndex] = buffer; // store (copy) AtomBuffer

            if ((hdrinfo[HDR_INFO_MODE_ADDR] == HDR_INFO_MODE_HDR && frameIndex == 0x02) ||
                (hdrinfo[HDR_INFO_MODE_ADDR] == HDR_INFO_MODE_LLS && frameIndex == 0x04)) {
                // restart preview (these can be left out if preview needs to stay stopped)
                mHwcg.mIspCI->setHDR(2); // works for LLS also
                mISP->startObserver(OBSERVE_PREVIEW_STREAM);
                succesfull = true;
                break; // out of for-loop
            }
        } else
            mISP->putSnapshot(&buffer, NULL); // wrong buf, return immediately
    }

    if (!succesfull) {
        LOGE("Failed to capture HDR/LLS picture from extisp.");
        // restart preview
        mHwcg.mIspCI->setHDR(2); // works for LLS also
        mISP->startObserver(OBSERVE_PREVIEW_STREAM);
        return UNKNOWN_ERROR;
    }

    // now, the yuvBuffers are properly captured in the array "yuvBuffers"
    int frameCount = (hdrinfo[HDR_INFO_MODE_ADDR] == HDR_INFO_MODE_HDR) ? 3 : 5;
    for (int i = 0; i < frameCount; i++) {
        // yuv422 is 2Bpp
        int width, height;
        mParameters.getPictureSize(&width, &height);
        int size = width * height * 2;
        int offset = YUV422_DATA_START;
        LOG1("Sending yuv422 frame with offset %d and size %d", offset, size);
        mCallbacksThread->extispFrame(yuvBuffers[i], offset, size);
    }

    LOG1("@%s: HDR/LLS DONE!", __FUNCTION__);

    return status;
}

/**
 * Captures a picture using Continuous JPEG capture
 */
status_t ControlThread::captureJpegPic()
{
    LOG1("@%s", __FUNCTION__);

    status_t status = NO_ERROR;
    int retryCount = 3;

    // in continuous mode we already take pictures as fast we can
    if (mJpegContinuousShootingRunning) {
        LOG2("TakePicture called during Continuous shooting.");
        mCallbacksThread->requestTakePicture(false, false, false);
        mCallbacksThread->shutterSound();
        return NO_ERROR;
    }

    // Configure PictureThread, inform of the picture and thumbnail resolutions
    mPictureThread->initialize(mParameters, mHwcg.mIspCI->zoomRatio(mParameters.getInt(CameraParameters::KEY_ZOOM)));

    // TODO CJC
    stopFaceDetection();

    PictureThread::MetaData picMetaData;
    fillPicMetaData(picMetaData, false);
    mPictureThread->setMakerNote(*picMetaData.atomispMkNote);

    // Notify CallbacksThread that a picture was requested, so grab one from queue
    bool syncJpegCbWithPostview = false;
    bool requestPostviewCallback = false; // TODO CJC: true;
    bool requestRawCallback = true;
    mCallbacksThread->requestTakePicture(requestPostviewCallback, requestRawCallback, syncJpegCbWithPostview);

    // Send request to play the Shutter Sound
    mCallbacksThread->shutterSound();

    m3AControls->setAfEnabled(false);

    //Retry to send Jpeg request command when device busy.
    do {
        status = mISP->requestJpegCapture();
    } while(status != NO_ERROR && retryCount--);

    return status;
}

/**
 * Capture pictures with smart stabilization
 */
status_t ControlThread::captureSmartStabilizationPic()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    AtomBuffer snapshotBuffer
        = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_SNAPSHOT);
    AtomBuffer postviewBuffer
        = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_POSTVIEW);

    bool syncJpegCbWithPostview =false;
    bool requestPostviewCallback = false;
    bool requestRawCallback = false;
    int cachedBurstLength, cachedBurstStart;

    //cache burst related parameters
    cachedBurstLength = mBurstLength;
    cachedBurstStart = mBurstStart;

    // Notify CallbacksThread that a picture was requested, so grab one from queue
    mCallbacksThread->requestTakePicture(requestPostviewCallback, requestRawCallback, syncJpegCbWithPostview);

    stopFaceDetection();

    // Initialize the burst control variables for the ULL burst
    mBurstLength = NUM_PICS_FOR_SS;
    mBurstStart = 0;
    if (mState == STATE_CONTINUOUS_CAPTURE) {
        status = continuousStartStillCapture(false);

        // Configure PictureThread
        mPictureThread->initialize(mParameters, mHwcg.mIspCI->zoomRatio(mParameters.getInt(CameraParameters::KEY_ZOOM)));

        status = waitForCaptureStart();
        if (status != NO_ERROR) {
            LOGE("Error while waiting for capture to start");
            mCallbacksThread->sendError(CAMERA_ERROR_UNKNOWN);
            goto exit;
        }
    }

    // Send request to play the Shutter Sound
    mCallbacksThread->shutterSound();

    // now fetch the buffers from snapshot device
    AtomBuffer yuvBuffers[NUM_PICS_FOR_SS];
    CLEAR(yuvBuffers);
    for (int i = 0; i < NUM_PICS_FOR_SS; i++) {
        status = mISP->getSnapshot(&snapshotBuffer, &postviewBuffer);
        if (status != NO_ERROR) {
            LOGE("Error in grabbing snapshot!");
            mCallbacksThread->sendError(CAMERA_ERROR_UNKNOWN);
            goto exit;
        }

        //snapshotBuffer.owner = mISP;
        yuvBuffers[i] = snapshotBuffer;
        mCallbacksThread->smartStabilizationFrameDone(yuvBuffers[i]);
    }

    stopOfflineCapture();

exit:
    // Restore the Burst related control variables
    mBurstLength = cachedBurstLength;
    mBurstStart = cachedBurstStart;
    return status;
}


/**
 * Start capture pictures until stop cancelpicture or stopJpegPicContinuousShooting
 * is called
 */
status_t ControlThread::startJpegPicContinuousShooting()
{
    LOG1("@%s: ", __FUNCTION__);

    if (mJpegContinuousShootingRunning) {
        LOGE("Continuous shooting alread running!");
        return INVALID_OPERATION;
    }

    // Configure PictureThread, inform of the picture and thumbnail resolutions
    mPictureThread->initialize(mParameters, mHwcg.mIspCI->zoomRatio(mParameters.getInt(CameraParameters::KEY_ZOOM)));

    // Notify CallbacksThread that a picture was requested, so grab one from queue
    mCallbacksThread->requestTakePicture(false, false, false);

    // TODO: do we need somekind of shutter sound

    mJpegContinuousShootingRunning = true;
    LOG1("CaptureSubState %s -> CONTINUOUS_SHOOTING (startJpegPicContinuousShooting)"
         , sCaptureSubstateStrings[mCaptureSubState]);
    mCaptureSubState = STATE_CAPTURE_CONTINUOUS_SHOOTING;

    return mISP->startJpegModeContinuousShooting();
}

/**
 * Stop capture continuous shooting pictures in continuous jpeg mode
 */
status_t ControlThread::stopJpegPicContinuousShooting()
{
    LOG1("@%s: ", __FUNCTION__);
    status_t status(NO_ERROR);

    if (!mJpegContinuousShootingRunning) {
        LOGE("Continuous shooting not running!");
        return INVALID_OPERATION;
    }

    mISP->stopJpegModeContinuousShooting();
    status = cancelPictureThread();
    mJpegContinuousShootingRunning = false;
    LOG1("CaptureSubState %s -> IDLE (stopJpegPicContinuousShooting)",
         sCaptureSubstateStrings[mCaptureSubState]);
    mCaptureSubState = STATE_CAPTURE_IDLE;


    return status;
}

void ControlThread::encodeVideoSnapshot(AtomBuffer &buff)
{
    LOG1("@%s: ", __FUNCTION__);
    PictureThread::MetaData aDummyMetaData;

    fillPicMetaData(aDummyMetaData, false);
    LOG1("Encoding a video snapshot couple buf id:%d", buff.id);
    LOG2("snapshot size %dx%d bpl %d fourcc %s 0x%x", buff.width
         ,buff.height, buff.bpl, v4l2Fmt2Str(buff.fourcc), buff.fourcc);

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
    for (it = canceledPictures.begin(); it != canceledPictures.end(); ++it) {
        status = handleMessagePictureDone(&it->data.pictureDone);
        if (status != NO_ERROR)
            LOGD("Failed handling pictureDone-messages while canceling!");
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
            LOGD("Failed handling postCaptureProcessingDone while canceling!");
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

    if (mJpegContinuousShootingRunning)
        stopJpegPicContinuousShooting();

    if (mState == STATE_CAPTURE) {
        // online capture
        status = stopCapture();
    } else if (mState == STATE_CONTINUOUS_CAPTURE || mState == STATE_JPEG_CAPTURE) {
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

    // In continuous shooting case capture is not cancelled until
    // receiving stop command.
    if (!mJpegContinuousShootingRunning) {
        mBurstLength = 0;
        status = cancelPictureThread();
        mStillCaptureInProgress = false;
    }

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

void ControlThread::previewBufferCallback(AtomBuffer *buff, ICallbackPreview::CallbackType t)
{
    LOG2("@%s", __FUNCTION__);
    if (t != ICallbackPreview::INPUT_ONCE) {
        LOGE("Received unexpected preview callback");
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

    // allocate snapshot buffers right after preview started
    allocateSnapshotAndPostviewBuffers(videoMode);

    return NO_ERROR;
}

status_t ControlThread::handleMessageEncodingDone(MessagePicture *msg)
{
    LOG1("@%s", __FUNCTION__);

    if (mCaptureSubState == STATE_CAPTURE_CONTINUOUS_SHOOTING) {
        LOG1("ENCODING DONE, stay CaptureSubState CONTINUOUS_SHOOTING");
    } else {
        LOG1("CaptureSubState %s -> ENCODING DONE", sCaptureSubstateStrings[mCaptureSubState]);
        mCaptureSubState = STATE_CAPTURE_ENCODING_DONE;
    }

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
        if (!mFullSizeSdv) { //online sdv or continuous JpegCapture mode
            if (!PlatformData::supportsContinuousJpegCapture(mCameraId) || mExtIspAction == EXT_ISP_ACTION_VIDEOHS)
                mVideoThread->putVideoSnapshot(&msg->snapshotBuf);
        } else { //offline SDV
            if (findBufferByData(&msg->snapshotBuf, &mAllocatedSnapshotBuffers) == NULL) {
                LOGE("Stale snapshot buffer %p returned... this should not happen", msg->snapshotBuf.dataPtr);
            } else if (findBufferByData(&msg->snapshotBuf, &mAvailableSnapshotBuffers) == NULL) {
                mAvailableSnapshotBuffers.push(msg->snapshotBuf);
                // recycal postview buffer as well since they are 1:1
                mAvailablePostviewBuffers.push(msg->postviewBuf);
                LOG1("%s offline SDV: pushed %p to mAvailableSnapshotBuffers, size %d",
                        __FUNCTION__, msg->snapshotBuf.dataPtr, mAvailableSnapshotBuffers.size());
            }

        }
        return status;
    } else if (mState == STATE_CAPTURE || mState == STATE_CONTINUOUS_CAPTURE ||
            (mState == STATE_STOPPED && mHdr.inProgress)) {
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
            msg->postviewBuf.status = FRAME_STATUS_OK;
            if (findBufferByData(&msg->snapshotBuf, &mAllocatedSnapshotBuffers) == NULL) {
                LOGE("Stale snapshot buffer %p returned... this should not happen", msg->snapshotBuf.dataPtr);

            } else if (findBufferByData(&msg->snapshotBuf, &mAvailableSnapshotBuffers) == NULL) {
                // It's safe to recycle this buffer if needed
                if (mBurstLength > 1 && mBurstBufsToReturn > 0) {
                    if (mBracketManager->getBracketMode() != BRACKET_NONE) {
                        status = mBracketManager->putSnapshot(msg->snapshotBuf, msg->postviewBuf);
                    } else {
                        status = mISP->putSnapshot(&msg->snapshotBuf, &msg->postviewBuf);
                    }

                    if (status != NO_ERROR) {
                        LOGE("Error %d in putting snapshot buffer:%p postviewBuf:%p!", status,
                                msg->snapshotBuf.dataPtr, msg->postviewBuf.dataPtr);
                    } else {
                        LOG1("Recycle snapshot buffer:%p postview buffer:%p", msg->snapshotBuf.dataPtr, msg->postviewBuf.dataPtr);
                    }
                    mBurstBufsToReturn--;
                } else if (mContShootingState == CONT_SHOOTING_STARTED) {
                    status = putSnapshot(&msg->snapshotBuf, &msg->postviewBuf);
                    if (status != NO_ERROR) {
                        LOGE("Error %d in putting snapshot buffer:%p postviewBuf:%p for continuous shooting", status,
                                msg->snapshotBuf.dataPtr, msg->postviewBuf.dataPtr);
                    } else {
                        LOG1("Recycle snapshot buffer:%p postview buffer:%p  for continuous shooting",
                                msg->snapshotBuf.dataPtr, msg->postviewBuf.dataPtr);
                    }
                } else {
                    mAvailableSnapshotBuffers.push(msg->snapshotBuf);
                    if (findBufferByData(&msg->postviewBuf, &mAllocatedPostviewBuffers) != NULL &&
                            findBufferByData(&msg->postviewBuf, &mAvailablePostviewBuffers) == NULL) {
                        mAvailablePostviewBuffers.push(msg->postviewBuf);
                    } else {
                        LOGW("Exceptional postview buffer returned, should not happen");
                    }
                    LOG1("%s  pushed snapshot buffer:%p postview buffer:%p to available queue, size %d:%d",
                            __FUNCTION__, msg->snapshotBuf.dataPtr, msg->postviewBuf.dataPtr,
                            mAvailableSnapshotBuffers.size(), mAvailablePostviewBuffers.size());
                }
            } else {
                LOGE("%s Already available snapshot buffer arrived. Find the bug!!", __FUNCTION__);
            }
        }

        if (isBurstRunning()) {
            ++mBurstCaptureDoneNum;
            LOG2("Burst req %d done %d len %d",
                 mBurstCaptureNum, mBurstCaptureDoneNum, mBurstLength);
            if (mBurstCaptureDoneNum >= mBurstLength &&
                (!mHdr.enabled || msg->snapshotBuf.dataPtr == mHdr.outMainBuf.dataPtr)) {
                LOGW("Last pic in burst received, terminating");
                burstStateReset();
            }
        }

        // transit to idle once all buffers are returned
        if (!isBurstRunning() && mAllocatedSnapshotBuffers.size() == mAvailableSnapshotBuffers.size()) {
            LOG1("CaptureSubState %s -> IDLE",sCaptureSubstateStrings[mCaptureSubState]);
            mCaptureSubState = STATE_CAPTURE_IDLE;
        }
    } else if (mState == STATE_JPEG_CAPTURE) {
        LOG1("@%s: STATE_JPEG_CAPTURE", __FUNCTION__);
        // continuous jpeg capture use atomisp frame not snapshot or postview here
        if (mCaptureSubState == STATE_CAPTURE_CONTINUOUS_SHOOTING) {
            LOG1("PICTURE DONE, stay CaptureSubState CONTINUOUS_SHOOTING");
        } else {
            LOG1("CaptureSubState %s -> IDLE",sCaptureSubstateStrings[mCaptureSubState]);
            mCaptureSubState = STATE_CAPTURE_IDLE;
        }
    } else {
        LOGW("Received a picture Done during invalid state %d; buf id:%d, ptr=%p", mState, msg->snapshotBuf.id, msg->snapshotBuf.buff);
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
            ++it;
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
    if (buf == NULL || buf->dataPtr == NULL || aVector == NULL) {
        return NULL;
    }

    Vector<AtomBuffer>::iterator it = aVector->begin();
    for (;it != aVector->end(); ++it) {
        if (buf->dataPtr == it->dataPtr)
            return it;
    }

    return NULL;
}

/**
 * Utility method to remove the buffers in vectors of AtomBuffers
 * the comparison is done based on the value of the data pointer
 * return NULL if the buffe is not found
 */
AtomBuffer* ControlThread::rmBufferInQueue(AtomBuffer *buf,Vector<AtomBuffer> *aVector)
{
    Vector<AtomBuffer>::iterator it = aVector->begin();
    for (;it != aVector->end(); ++it) {
        if (buf->dataPtr == it->dataPtr)
            return aVector->erase(it);
    }

    LOGW("@%s buff:%p not found", __FUNCTION__, buf->dataPtr);
    return NULL;
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
            LOGE("Unsupported high-speed video size@fps combination: %s, supported: %s", sizeFpsStr, supportedSizeFpsCombos);
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
                    LOGE("Could not configure Overlay preview rendering");
                }
            }
        } else {
            LOGW("Overlay cannot be enabled in other state than stop, ignoring request");
        }
    }
    return status;
}

status_t ControlThread::processParamDualVideo(const CameraParameters *oldParams,
        CameraParameters *newParams, bool &restartNeeded)
{
    LOG1("@%s restartNeeded:%d", __FUNCTION__, restartNeeded);
    status_t status = NO_ERROR;

    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams, IntelCameraParameters::KEY_DUAL_VIDEO);
    if (!newVal.isEmpty()) {
        mDualVideo = (newVal == CameraParameters::TRUE);
        // Dual video only prefer online mode
        if ((mDualVideo == true && (mState == STATE_CONTINUOUS_CAPTURE || mISP->getMode() == MODE_CONTINUOUS_VIDEO))
                || (mDualVideo == false && (mState == STATE_PREVIEW_STILL || mState == STATE_CAPTURE)))
            restartNeeded = true;
    }
    return status;
}

status_t ControlThread::processParamDualCameraMode(CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams, IntelCameraParameters::KEY_DUAL_CAMERA_MODE);

    if (!newVal.isEmpty()) {
        if (newVal == IntelCameraParameters::DUAL_CAMERA_MODE_DEPTH) {
            PlatformData::useExtendedCamera(true);
            mDepthMode = true;

            // For Kevlar project, we expect to use the same focal length for both SBS camera
            // and main back camera in depth mode. We will force to set afMode to CAM_AF_MODE_MANUAL
            // to AtomAIQ, and set default depth focal length to driver.
            if (!PlatformData::isFixedFocusCamera(mCameraId)
               && PlatformData::supportExtendedCamera()) {
                const char* str = newParams->get(CameraParameters::KEY_FOCUS_MODE);
                if (str) {
                    LOG2("current focus mode = %s", str);
                    mSavedFocusMode = String8(str);
                }

                newParams->set(CameraParameters::KEY_FOCUS_MODE, "infinity");

                // set focus mode to fixed too in old parameter and let focus don't set again.
                oldParams->set(CameraParameters::KEY_FOCUS_MODE, "infinity");

                int defaultFocusLength = PlatformData::defaultDepthFocalLength(mCameraId);
                // set AF mode to CAM_AF_MODE_MANUAL
                if (m3AControls && (defaultFocusLength != 0)) {
                    m3AControls->setAfMode(CAM_AF_MODE_MANUAL);
                    ia_aiq_manual_focus_parameters focusParameters;
                    focusParameters.manual_focus_action = ia_aiq_manual_focus_action_set_distance;
                    focusParameters.manual_focus_distance = defaultFocusLength;
                    m3AControls->setManualFocusParameters(focusParameters);
                    LOG2("defaultFocusLength = %d", defaultFocusLength);
                }
            }
        } else {
            PlatformData::useExtendedCamera(false);
            mDepthMode = false;

            if (!PlatformData::isFixedFocusCamera(mCameraId)
               && PlatformData::supportExtendedCamera()) {

                if (m3AControls) {
                    ia_aiq_manual_focus_parameters focusParameters;
                    focusParameters.manual_focus_action = ia_aiq_manual_focus_action_none;
                    focusParameters.manual_focus_distance = 0;
                    m3AControls->setManualFocusParameters(focusParameters);
                }

                LOG2("restore focus mode = %s", mSavedFocusMode.string());
                newParams->set(CameraParameters::KEY_FOCUS_MODE, mSavedFocusMode.string());

                // remove focus parameter in old parameter to force updating focus mode.
                oldParams->remove(CameraParameters::KEY_FOCUS_MODE);
            }
        }
    }
    return status;
}

status_t ControlThread::processParamContinuousShooting(const CameraParameters *oldParams,
        CameraParameters *newParams, bool &restartNeeded)
{
    LOG1("@%s restartNeeded:%d", __FUNCTION__, restartNeeded);
    status_t status = NO_ERROR;

    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams, IntelCameraParameters::KEY_CONTINUOUS_SHOOTING);
    if (!newVal.isEmpty()) {
        mContShootingEnabled = (newVal == CameraParameters::TRUE);
        if (!PlatformData::supportsContinuousJpegCapture(mCameraId))
            restartNeeded = true;
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
        status = mHwcg.mIspCI->setZoom(newZoom);
        mPostProcThread->setZoom(mHwcg.mIspCI->zoomRatio(newZoom));
        mULL->setZoomFactor(mHwcg.mIspCI->zoomRatio(newZoom));
    } else {
        LOGD("not supported zoom setting");
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
            // Smart Shutter Capture
            status = processParamSmartShutter(oldParams, newParams);
        }

        if (status == NO_ERROR) {
            // shutter manual setting (Intel extension)
            status = processParamShutter(oldParams, newParams);
        }
    }

    if (status == NO_ERROR) {
        status = processParamIntelligentMode(oldParams, newParams);
    }

    return status;
}

/**
 * get the number of snapshot buffers that we actually need
 */
int ControlThread::getNeededSnapshotBufNum(bool videoMode)
{
    LOG1("@%s video mode:%d", __FUNCTION__, videoMode);
    int maxNum, recommendedNum, contShootingLimit, clipTo;

    if (PlatformData::supportsContinuousJpegCapture(mCameraId)) {
        // In continuousJpegCapture use buffers allocated in AtomISP
        return 0;
    }

    if (videoMode && !mFullSizeSdv) {
        // online video mode doesn't need extra snapshot buffer
        return 0;
    }

    /**
     * Bracketing needs more buffers than burst.
     * so we make a difference between them
     */
    recommendedNum = ((mBracketManager->getBracketMode() != BRACKET_NONE) ?
        PlatformData::getMaxNumYUVBufferForBracket(mCameraId) :
        PlatformData::getMaxNumYUVBufferForBurst(mCameraId));

    // continuous shooting need one more buffer reserved
    if (mISP->getContinuousCaptureNumber() > mUllBurstLength) { // timenudge
        contShootingLimit = mISP->getContinuousCaptureNumber();
    } else if (mContShootingState !=  CONT_SHOOTING_NONE) { // in continuous shooting
        contShootingLimit = DEFAULT_BUF_NUM_FOR_CONT_SHOOTING;
    } else if (mULL->isActive()) {
        contShootingLimit = mULL->getULLBurstLength() + 1;
    } else {
        contShootingLimit = mISP->getContinuousCaptureNumber() + 1;
    }

    /**
     * Get the buffer required and clip it to ensure we
     * allocate proper number of YUV buffers.
     */
    clipTo = MAX(recommendedNum, contShootingLimit);
    maxNum = MAX(mBurstLength, contShootingLimit);

    return CLIP(maxNum, clipTo, 1);
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
 * already known to the HW encoder. This speeds up the encoding.
 *
 * This call is used in the following situations:
 * - when preview has already started
 * - when processing parameters and those require new buffers
 *
 * care needs to be taken not to allocate the buffers at a time when ControlThread
 * needs to be fast for some performance metric, like when taking a snapshot
 * or when we are starting preview
 */
status_t ControlThread::allocateSnapshotAndPostviewBuffers(bool videoMode)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    uint32_t numBufRequired = 0;
    bool allocSnapshot = true;
    bool allocPostview = true;
    AtomBuffer tmpBuffer;

    // check if it's safe to allocate buffer now
    if (mAllocatedSnapshotBuffers.size() != mAvailableSnapshotBuffers.size()) {
        LOGW("not safe to allocate now, maybe some snapshot buffers are not returned %d:%d",
                mAllocatedSnapshotBuffers.size(), mAvailableSnapshotBuffers.size());
        return INVALID_OPERATION;
    }

    // check if really need to allocate buffer
    numBufRequired = getNeededSnapshotBufNum(videoMode);

    if (numBufRequired == 0 && mAllocatedSnapshotBuffers.size() == 0 && mAllocatedPostviewBuffers.size() == 0) {
        LOG1("No need to request Snapshot or Postview, zero buffers is needed.");
        return NO_ERROR;
    }

    AtomBuffer formatDescriptorPv = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_FORMAT_DESCRIPTOR);
    AtomBuffer formatDescriptorSs = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_FORMAT_DESCRIPTOR);

    mISP->getSnapshotFrameFormat(formatDescriptorSs);
    /**
     * if in continuous JPEG capture mark that to snapshot format, else
     * snapshot format is hardcoded to NV12, this is the format between
     * camera and JPEG encoder. In cases where we need to capture bayer
     * then the format changes to RGB adn JPEG encoding breaks (i.e. image is
     * green) this is a known limitation of the raw capture sequence in ISP fW
     */
    if (PlatformData::supportsContinuousJpegCapture(mCameraId)) {
        formatDescriptorSs.fourcc = V4L2_PIX_FMT_CONTINUOUS_JPEG;
    } else if (CameraDump::isDumpImageEnable(CAMERA_DEBUG_DUMP_RAW)) {
        formatDescriptorSs.fourcc = mHwcg.mSensorCI->getRawFormat();
        checkAndUpdateRawSize(formatDescriptorSs);
        mISP->setSnapshotFrameFormat(formatDescriptorSs);
    } else {
        formatDescriptorSs.fourcc = V4L2_PIX_FMT_NV12;
    }


    if (!mAllocatedSnapshotBuffers.isEmpty()) {
        tmpBuffer = mAllocatedSnapshotBuffers.itemAt(0);

        if ( (tmpBuffer.width  == formatDescriptorSs.width) &&
             (tmpBuffer.height == formatDescriptorSs.height) &&
             (tmpBuffer.fourcc == formatDescriptorSs.fourcc) &&
             (mAllocatedSnapshotBuffers.size() == numBufRequired)) {
            LOG1("No need to request Snapshot, buffers already available");
            allocSnapshot = false;
        }
    }

    mISP->getPostviewFrameFormat(formatDescriptorPv);
    if (!mAllocatedPostviewBuffers.isEmpty()) {
        tmpBuffer = mAllocatedPostviewBuffers.itemAt(0);

        if ((tmpBuffer.width  == formatDescriptorPv.width) &&
            (tmpBuffer.height == formatDescriptorPv.height) &&
            (tmpBuffer.fourcc == formatDescriptorPv.fourcc) &&
            (mAllocatedPostviewBuffers.size() == numBufRequired)) {
            LOG1("No need to request Postview, buffers already available");
            allocPostview = false;
        }
    }

    if (!allocSnapshot && !allocPostview) {
        LOG1("@%s: No need to allocate postview or snapshot buffers. Already available.", __FUNCTION__);
        return NO_ERROR;
    }

    LOGI("Request to allocate %d bufs of (%dx%d) fourcc: %s 0x%x", numBufRequired,
         formatDescriptorSs.width, formatDescriptorSs.height, v4l2Fmt2Str(formatDescriptorSs.fourcc), formatDescriptorSs.fourcc);
    LOG1("Currently allocated: %d , available %d",
         mAllocatedSnapshotBuffers.size(), mAvailableSnapshotBuffers.size());

    // check need to register bufs to scaler.. can't use ISP since ISP isn't
    // configured yet, so do it by checking the preview mode and sensor type
    ControlThread::State state = selectPreviewMode(mParameters);
    bool registerToScaler = (PlatformData::sensorType(mCameraId) == SENSOR_TYPE_SOC) &&
            (state == STATE_CONTINUOUS_CAPTURE);

    if (allocSnapshot) {
        mAllocatedSnapshotBuffers.clear();
        mAvailableSnapshotBuffers.clear();

        status = mPictureThread->allocSnapshotBuffers(formatDescriptorSs, numBufRequired,
                                                      &mAllocatedSnapshotBuffers,
                                                      registerToScaler);

        if (status != NO_ERROR) {
            LOGE("Could not pre-allocate snapshot buffers!");
        } else {
            mAvailableSnapshotBuffers = mAllocatedSnapshotBuffers;
        }

        // update configuration inside  AtomISP class
        mISP->setSnapshotFrameFormat(formatDescriptorSs);
    }

    if (allocPostview) {
        mAllocatedPostviewBuffers.clear();
        mAvailablePostviewBuffers.clear();

        status = mPictureThread->allocPostviewBuffers(formatDescriptorPv, numBufRequired,
                                                      &mAllocatedPostviewBuffers,
                                                      registerToScaler);

        if (status != NO_ERROR) {
            LOGE("Could not pre-allocate postview buffers!");
        } else {
            mAvailablePostviewBuffers = mAllocatedPostviewBuffers;
        }
    }

    return status;
}

/**
 * Checks and updates the snapshot format for RAW capture
 * For RAW capture, we need to use valid sensor output size
 * for the capture buffer allocation. Also, buffer BPL needs
 * to be 128-aligned
 *
 * This function checks the application-requested buffer size
 * against the sensor output sizes, and selects the sensor
 * output size closest to requested image properties. BPL is set
 * to image width and aligned to 128, if needed.
 */
void ControlThread::checkAndUpdateRawSize(AtomBuffer& formatDesc)
{
    LOG1("@%s", __FUNCTION__);

    Vector<v4l2_subdev_frame_size_enum> sizes;
    mHwcg.mSensorCI->getFrameSizes(sizes);

    Vector<v4l2_subdev_frame_size_enum>::iterator iter = sizes.begin();
    Vector<v4l2_subdev_frame_size_enum>::iterator endIter = sizes.end();

    int sensorWidth = 0, sensorHeight = 0;
    float aspect = static_cast<float>(formatDesc.width) / static_cast<float>(formatDesc.height);
    float sensorAspect = 0.0;
    float aspectDiff = 0.0;
    const float ASPECT_TOLERANCE_SIZE = 0.009;

    while (iter != endIter) {
        sensorWidth = iter->max_width;
        sensorHeight = iter->max_height;

        sensorAspect =  static_cast<float>(sensorWidth) / sensorHeight;
        aspectDiff = fabs(sensorAspect - aspect);

        LOG2("requested size %dx%d, sensor output size: %dx%d, aspect diff %f",
             formatDesc.width, formatDesc.height, sensorWidth, sensorHeight, aspectDiff);

        if (aspectDiff < ASPECT_TOLERANCE_SIZE && formatDesc.width <= sensorWidth) {
            LOG1("found proper sensor size: %dx%d", sensorWidth, sensorHeight);
            break;
        }

        ++iter;
    }

    if (iter == endIter) {
        LOGW("No proper sensor frame size for RAW buffer allocation found");
        return;
    }

    formatDesc.width = sensorWidth;
    formatDesc.height = sensorHeight;

    // FIXME: remove this BPL handling when firmware is fixed
    // to report correct BPL size
    formatDesc.bpl = ALIGN_WIDTH(formatDesc.width, 128);

    LOG1("RAW final size %dx%d, bpl %d", formatDesc.width, formatDesc.height, formatDesc.bpl);
}


void ControlThread::processParamFileInject(CameraParameters *newParams)
{
#ifdef ENABLE_FILE_INJECTION
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
    LOG1("file name=%s,width=%d,height=%d,fourcc=%s 0x%x,bayer-order=%d.",
         fileName, width, height,v4l2Fmt2Str(fourcc), fourcc, bayerOrder);
    mISP->configureFileInject(fileName, width, height, fourcc, bayerOrder);
#endif
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
            LOGE("Invalid value received for %s: %s", IntelCameraParameters::KEY_AF_LOCK_MODE, newVal.string());
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
            LOGE("Invalid value received for %s: %s", CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK, newVal.string());
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
        if (mHwcg.mIspCI->getXNR() != xnr) {
            LOG2("XNR value new %s", newVal.string());
            XNR_ANR_changed = true;
            mHwcg.mIspCI->setXNR(xnr);
        }
    }

    // ANR
    newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              IntelCameraParameters::KEY_ANR);
    if (!newVal.isEmpty()) {
        bool anr = (newVal == CameraParameters::TRUE);
        // note: due add/remove of intel parameters newVal doesn't always
        // reflect changes of value in AtomISP level
        if (mHwcg.mIspCI->getLowLight() != anr) {
            LOG2("ANR value new %s", newVal.string());
            XNR_ANR_changed = true;
            mHwcg.mIspCI->setLowLight(anr);
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
 *  Switch ISP to color-bar test pattern.
 *
 *  "on" is set ISP to color-bar preview output
 *  "off" is set ISP to normal preview output
 */
status_t ControlThread::processParamColorBar(const CameraParameters *oldParams,
        CameraParameters *newParams, bool &restartNeeded)
{
    LOG1("@%s", __FUNCTION__);

    String8 pattern = paramsReturnNewIfChanged(oldParams, newParams,
                                               IntelCameraParameters::KEY_COLORBAR);
    if (!pattern.isEmpty()) {
        if (pattern == CameraParameters::TRUE) {
            mISP->setColorBarPattern(true);
        } else if (pattern == CameraParameters::FALSE) {
            mISP->setColorBarPattern(false);
        } else {
            // color-bar maybe not set
            return NO_ERROR;
        }

        restartNeeded = true;
    }

    return NO_ERROR;
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
            LOGE("Invalid value received for %s: %s", CameraParameters::KEY_AUTO_EXPOSURE_LOCK, newVal.string());
            return INVALID_OPERATION;
        }

        status = m3AThread->lockAe(ae_lock);
        if (status == NO_ERROR) {
            LOG1("Changed: %s -> %s", CameraParameters::KEY_AUTO_EXPOSURE_LOCK, newVal.string());
            if (ae_lock) {
                mAELockFlashStage = m3AControls->getAeFlashNecessity();
                LOG1("AE locked, storing flash necessity decision (%d)",  mAELockFlashStage);
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

        status = m3AControls->setAeFlashMode(flash);
        if (status == NO_ERROR) {
            LOG1("Changed: %s -> %s", CameraParameters::KEY_FLASH_MODE, newVal.string());
        } else {
            // Ok in general for SOC sensors.
            // TODO: Kernel driver should support querying which controls the sensors support
            LOGW("Error in setting flash mode \'%s\' (%d), 3A ctrl type: %d",
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
                LOGE("Windowless operation cannot be enabled, window already configured!");
                return INVALID_OPERATION;
            }
            if (mPreviewThread->getPreviewState() == PreviewThread::STATE_NO_WINDOW) {
                LOGE("Windowless operation cannot be enabled, startPreview() already called");
                return INVALID_OPERATION;
            }
            mPreviewUpdateMode = IntelCameraParameters::PREVIEW_UPDATE_MODE_WINDOWLESS;
        } else {
            LOGE("Unknown preview update mode received %s", newVal.string());
        }
    }

    // when intelligent mode is used, the windowless mode should be used.
    if (PlatformData::getIntelligentMode(mCameraId)) {
        LOG1("@%s, the intelligent mode is used, so set to use windowless mode for preview", __FUNCTION__);
        mPreviewUpdateMode = IntelCameraParameters::PREVIEW_UPDATE_MODE_WINDOWLESS;
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
        CameraParameters *newParams, bool &restartNeeded)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    BracketingMode oldMode;
    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              IntelCameraParameters::KEY_CAPTURE_BRACKET);

    if (!newVal.isEmpty()) {
        oldMode = mBracketManager->getBracketMode();
        if(newVal == "exposure") {
            mBracketManager->setBracketMode(BRACKET_EXPOSURE);
        } else if(newVal == "focus") {
            mBracketManager->setBracketMode(BRACKET_FOCUS);
        } else if(newVal == "none") {
            mBracketManager->setBracketMode(BRACKET_NONE);
        } else {
            LOGE("Invalid value received for %s: %s", IntelCameraParameters::KEY_CAPTURE_BRACKET, newVal.string());
            status = BAD_VALUE;
        }
        if (status == NO_ERROR) {
            LOG1("Changed: %s -> %s", IntelCameraParameters::KEY_CAPTURE_BRACKET, newVal.string());
        }

        // No need to restart preview only if change between exposure and focus
        if (status == NO_ERROR && !((oldMode == BRACKET_FOCUS && newVal == "exposure") ||
                (oldMode == BRACKET_EXPOSURE && newVal == "focus")))
            restartNeeded = true;
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
            LOGE("Invalid value received for %s: %d, set to default %d",
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
            LOGE("Invalid value received for %s: %d, set to default %d",
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
        LOGW("%s: attempt to change hdr parameters during hdr capture", __FUNCTION__);
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

            ia_binary_data aiqb_data;
            CLEAR(aiqb_data);
            status = m3AControls->getAiqConfig(&aiqb_data);
            if (status != NO_ERROR)
                LOGW("@%s: cannot retrieve CPF binary data for HDR capture", __FUNCTION__);

            status = mCP->initializeHDR(newWidth, newHeight, &aiqb_data);
            if (status == NO_ERROR) {
                mHdr.enabled = true;
                mHdr.bracketMode = BRACKET_EXPOSURE;
                mHdr.savedBracketMode = mBracketManager->getBracketMode();
                mHdr.bracketNum = DEFAULT_HDR_BRACKETING;
            } else {
                LOGE("HDR buffer allocation failed");
            }
        } else if ((newValIntel.isEmpty() && newVal != CameraParameters::SCENE_MODE_HDR)
                    || (newValIntel == "off" && newVal != CameraParameters::SCENE_MODE_HDR)) {
            if(mHdr.enabled) {
                status = mCP->uninitializeHDR();
                if (status != NO_ERROR)
                    LOGE("HDR buffer release failed");
            }
            mHdr.enabled = false;
            mBracketManager->setBracketMode(mHdr.savedBracketMode);
        } else {
            if(!newValIntel.isEmpty()) {
            LOGE("Invalid value received for %s: %s", IntelCameraParameters::KEY_HDR_IMAGING, newVal.string());
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
                ia_binary_data aiqb_data;
                CLEAR(aiqb_data);
                status = m3AControls->getAiqConfig(&aiqb_data);
                if (status != NO_ERROR)
                    LOGW("@%s: cannot retrieve CPF binary data for HDR capture", __FUNCTION__);

                status = mCP->initializeHDR(newWidth, newHeight, &aiqb_data);
                if (status != NO_ERROR) {
                    LOGE("HDR buffer allocation failed");
                }
            } else {
                LOGE("HDR buffer release failed");
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
            LOGW("Invalid value received for %s: %s", IntelCameraParameters::KEY_HDR_SAVE_ORIGINAL, newVal.string());
            localStatus = BAD_VALUE;
        }
        if (localStatus == NO_ERROR) {
            LOG1("Changed: %s -> %s", IntelCameraParameters::KEY_HDR_SAVE_ORIGINAL, newVal.string());
        }
    }

    return status;
}

status_t ControlThread::processParamSDV(const CameraParameters *oldParams,
        CameraParameters *newParams, bool *restartPreview)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              IntelCameraParameters::KEY_SDV);
    if (!newVal.isEmpty()) {
        LOG1("SDV param new value: %s", newVal.string());

        if (restartPreview && mState == STATE_PREVIEW_VIDEO) {
            *restartPreview = true;
        } else {
            LOGD("@%s value changes to %s in state:%d, no need to restart preview",
                    __FUNCTION__, newVal.string(), mState);
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
            mISP->setSRESmode(true);
        } else if (newVal == "auto") {
            mULL->setMode(UltraLowLight::ULL_AUTO);
            ullActive = true;
            mISP->setSRESmode(false);
        } else {
            mULL->setMode(UltraLowLight::ULL_OFF);
            mISP->setSRESmode(false);
        }

        m3AControls->setUllEnabled(ullActive);

        /**
         * If applications enables ULL while in Continuous Capture mode and
         * the current ring buffer configuration is not big enough we need
         * to re-start preview to make sure we have the correct configuration
         *
         */
        if (ullActive && mState != STATE_STOPPED) {
            if (mISP->getContinuousCaptureNumber() < mUllBurstLength) {
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
            LOGW("@%s low battery status warning", __FUNCTION__);
            //TODO call 3a interface
            break;
        case BATTERY_STATUS_ALERT:
            LOGW("@%s low battery status alert", __FUNCTION__);
            //TODO call 3a interface
            break;
        case BATTERY_STATUS_CRITICAL:
            LOGW("@%s critical low battery status", __FUNCTION__);
            //TODO call 3a interface
            lowBattery = true;
            break;
        case BATTERY_STATUS_INVALID:
            LOGW("@%s invalid battery status", __FUNCTION__);
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
            LOGW("@%s low battery for flash, force set flash mode to off", __FUNCTION__);
            //Callback to user
            mCallbacksThread->lowBattery();
        }

        // Save flash mode in burst mode or low battery state.
        mSavedFlashMode = currRequestedFlashMode;

        newParams->set(CameraParameters::KEY_SUPPORTED_FLASH_MODES, CameraParameters::FLASH_MODE_OFF);
        newParams->set(CameraParameters::KEY_FLASH_MODE, CameraParameters::FLASH_MODE_OFF);
    } else if ((mBurstLength == 1 || mBurstLength == 0) && !mHdr.enabled && !lowBattery) {
        // When not in burst mode or low battery state, we should restore flash mode
        // if supported flash mode is still only off mode.
        if (currSupportedFlashModes == CameraParameters::FLASH_MODE_OFF) {
            // Restore the supported flash modes to the values prior to forcing to "off":
            newParams->set(CameraParameters::KEY_SUPPORTED_FLASH_MODES, mSavedFlashSupported.string());
            // Restore flash mode in single mode.
            newParams->set(CameraParameters::KEY_FLASH_MODE, mSavedFlashMode.string());
            LOG2("force to restore flash mode[%s] and support list[%s]", mSavedFlashMode.string(), mSavedFlashSupported.string());
        }
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
                }
                newParams->set(CameraParameters::KEY_WHITE_BALANCE, CameraParameters::WHITE_BALANCE_AUTO);
                newParams->set(CameraParameters::KEY_ANTIBANDING, CameraParameters::ANTIBANDING_AUTO);
                newParams->set(IntelCameraParameters::KEY_AWB_MAPPING_MODE, IntelCameraParameters::AWB_MAPPING_AUTO);
                newParams->set(IntelCameraParameters::KEY_ISO, PlatformData::defaultIso(mCameraId));
                newParams->set(IntelCameraParameters::KEY_XNR, CameraParameters::FALSE);
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
                }
                newParams->set(IntelCameraParameters::KEY_ISO, PlatformData::defaultIso(mCameraId));
                newParams->set(CameraParameters::KEY_WHITE_BALANCE, CameraParameters::WHITE_BALANCE_AUTO);
                newParams->set(CameraParameters::KEY_ANTIBANDING, CameraParameters::ANTIBANDING_OFF);
                newParams->set(IntelCameraParameters::KEY_AWB_MAPPING_MODE, IntelCameraParameters::AWB_MAPPING_AUTO);
                newParams->set(IntelCameraParameters::KEY_AE_METERING_MODE, IntelCameraParameters::AE_METERING_MODE_AUTO);
                newParams->set(IntelCameraParameters::KEY_XNR, CameraParameters::FALSE);
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
                }
                newParams->set(IntelCameraParameters::KEY_ISO, PlatformData::defaultIso(mCameraId));
                newParams->set(CameraParameters::KEY_WHITE_BALANCE, CameraParameters::WHITE_BALANCE_AUTO);
                newParams->set(CameraParameters::KEY_ANTIBANDING, CameraParameters::ANTIBANDING_OFF);
                newParams->set(IntelCameraParameters::KEY_AWB_MAPPING_MODE, IntelCameraParameters::AWB_MAPPING_OUTDOOR);
                newParams->set(IntelCameraParameters::KEY_AE_METERING_MODE, IntelCameraParameters::AE_METERING_MODE_AUTO);
                newParams->set(IntelCameraParameters::KEY_XNR, CameraParameters::FALSE);
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
                }
                newParams->set(IntelCameraParameters::KEY_ISO, PlatformData::defaultIso(mCameraId));
                newParams->set(CameraParameters::KEY_WHITE_BALANCE, CameraParameters::WHITE_BALANCE_AUTO);
                newParams->set(CameraParameters::KEY_ANTIBANDING, CameraParameters::ANTIBANDING_OFF);
                newParams->set(IntelCameraParameters::KEY_AWB_MAPPING_MODE, IntelCameraParameters::AWB_MAPPING_AUTO);
                newParams->set(IntelCameraParameters::KEY_AE_METERING_MODE, IntelCameraParameters::AE_METERING_MODE_AUTO);
                newParams->set(IntelCameraParameters::KEY_XNR, CameraParameters::TRUE);
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
                }
                newParams->set(IntelCameraParameters::KEY_ISO, PlatformData::defaultIso(mCameraId));
                newParams->set(CameraParameters::KEY_WHITE_BALANCE, CameraParameters::WHITE_BALANCE_AUTO);
                newParams->set(CameraParameters::KEY_ANTIBANDING, CameraParameters::ANTIBANDING_OFF);
                newParams->set(IntelCameraParameters::KEY_AWB_MAPPING_MODE, IntelCameraParameters::AWB_MAPPING_AUTO);
                newParams->set(IntelCameraParameters::KEY_AE_METERING_MODE, IntelCameraParameters::AE_METERING_MODE_AUTO);
                newParams->set(IntelCameraParameters::KEY_XNR, CameraParameters::TRUE);
                newParams->set(IntelCameraParameters::KEY_ANR, CameraParameters::TRUE);
                newParams->set(CameraParameters::KEY_EXPOSURE_COMPENSATION, PlatformData::supportedDefaultEV(mCameraId));
                newParams->set(CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION, PlatformData::supportedMaxEV(mCameraId));
                newParams->set(CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION, PlatformData::supportedMinEV(mCameraId));
                newParams->set(CameraParameters::KEY_EXPOSURE_COMPENSATION_STEP, PlatformData::supportedStepEV(mCameraId));
            }
            if (PlatformData::supportsFlash(mCameraId)) {
                mSavedFlashSupported = String8("off,on");
                mSavedFlashMode = String8(CameraParameters::FLASH_MODE_ON);
                selectFlashModeForScene(newParams);
            }
        } else if (newScene == CameraParameters::SCENE_MODE_HDR) {
            sceneMode = CAM_AE_SCENE_MODE_AUTO;
            if (PlatformData::sensorType(mCameraId) == SENSOR_TYPE_RAW) {
                if (!PlatformData::isFixedFocusCamera(mCameraId)) {
                    newParams->set(CameraParameters::KEY_FOCUS_MODE, CameraParameters::FOCUS_MODE_CONTINUOUS_PICTURE);
                }
                newParams->set(IntelCameraParameters::KEY_ISO, PlatformData::defaultIso(mCameraId));
                newParams->set(IntelCameraParameters::KEY_AWB_MAPPING_MODE, IntelCameraParameters::AWB_MAPPING_AUTO);
                newParams->set(IntelCameraParameters::KEY_AE_METERING_MODE, IntelCameraParameters::AE_METERING_MODE_AUTO);
                newParams->set(IntelCameraParameters::KEY_BACK_LIGHTING_CORRECTION_MODE, IntelCameraParameters::BACK_LIGHT_COORECTION_OFF);
                newParams->set(IntelCameraParameters::KEY_XNR, CameraParameters::FALSE);
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
                }
                newParams->set(IntelCameraParameters::KEY_ISO, PlatformData::defaultIso(mCameraId));
                newParams->set(CameraParameters::KEY_WHITE_BALANCE, CameraParameters::WHITE_BALANCE_AUTO);
                newParams->set(CameraParameters::KEY_ANTIBANDING, CameraParameters::ANTIBANDING_OFF);
                newParams->set(IntelCameraParameters::KEY_AWB_MAPPING_MODE, IntelCameraParameters::AWB_MAPPING_AUTO);
                newParams->set(IntelCameraParameters::KEY_AE_METERING_MODE, IntelCameraParameters::AE_METERING_MODE_AUTO);
                newParams->set(IntelCameraParameters::KEY_XNR, CameraParameters::FALSE);
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
                }
                newParams->set(IntelCameraParameters::KEY_ISO, PlatformData::defaultIso(mCameraId));
                newParams->set(CameraParameters::KEY_WHITE_BALANCE, CameraParameters::WHITE_BALANCE_AUTO);
                newParams->set(CameraParameters::KEY_ANTIBANDING, CameraParameters::ANTIBANDING_AUTO);
                newParams->set(IntelCameraParameters::KEY_AWB_MAPPING_MODE, IntelCameraParameters::AWB_MAPPING_AUTO);
                newParams->set(IntelCameraParameters::KEY_AE_METERING_MODE, IntelCameraParameters::AE_METERING_MODE_AUTO);
                newParams->set(IntelCameraParameters::KEY_XNR, CameraParameters::FALSE);
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
                }
                newParams->set(IntelCameraParameters::KEY_ISO, PlatformData::defaultIso(mCameraId));
                newParams->set(CameraParameters::KEY_WHITE_BALANCE, CameraParameters::WHITE_BALANCE_AUTO);
                newParams->set(CameraParameters::KEY_ANTIBANDING, CameraParameters::ANTIBANDING_AUTO);
                newParams->set(IntelCameraParameters::KEY_AWB_MAPPING_MODE, IntelCameraParameters::AWB_MAPPING_AUTO);
                newParams->set(IntelCameraParameters::KEY_AE_METERING_MODE, IntelCameraParameters::AE_METERING_MODE_AUTO);
                newParams->set(IntelCameraParameters::KEY_XNR, CameraParameters::FALSE);
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

status_t ControlThread::setAfWindowsTo3a(CameraWindow *focusWindows, size_t winCount)
{
    LOG1("@%s", __FUNCTION__);

    status_t status = NO_ERROR;
    AAAWindowInfo convWindow;

    // Currently ext-ISP needs conversion window, otherwise we use the default
    // conversion to Intel AIQ coordinates (i.e. no window)
    if (PlatformData::supportsContinuousJpegCapture(mCameraId)) {
        int width, height;
        mParameters.getPreviewSize(&width, &height);
        convWindow.width = width;
        convWindow.height = height;
        status = m3AControls->setAfWindows(focusWindows, winCount, &convWindow);
    } else {
        // convWindow == NULL, does the default coordinate conversion to Intel AIQ coord
        status = m3AControls->setAfWindows(focusWindows, winCount);
    }

    return status;
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
            // TODO: Due to ext-isp af modes, we could use different enumerations for video/still CAF
            // then need to change setAfMode() switch-case to use new enums
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
            LOGW("Could not set AF mode to \'%s\' (%d),  3A ctrl type: %d",
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

                if (setAfWindowsTo3a(focusWindows, winCount) != NO_ERROR) {
                    // If focus windows couldn't be set, previous AF mode is used
                    curAfMode = m3AControls->getAfMode();
                    LOGW("Could not set AF windows. Resetting the AF back to %d", curAfMode);
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

        mMeteringAreas.toWindows(meteringWindows);

        if (m3AControls->setAeMeteringMode(CAM_AE_METERING_MODE_SPOT) == NO_ERROR) {
            LOG1("@%s, Got metering area, and \"spot\" mode set. Setting window.", __FUNCTION__ );
            // convWindow == NULL, using default conversion to Intel AIQ.
            // AE windows only supported in Intel AIQ for now.
            if (m3AControls->setAeWindow(&meteringWindows[0]) != NO_ERROR) {
                LOGW("Error setting AE metering window. Metering will not work");
            }
        } else {
                LOGW("Error setting AE metering mode to \"spot\". Metering will not work");
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
        LOG1("exposure compensation to \"%s\" (%d), ev value %f, res %d",
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
            LOGW("unknown AE_MODE \"%s\", falling back to AUTO", newVal.string());
            ae_mode = CAM_AE_MODE_AUTO;
        }
        m3AControls->setPublicAeMode(ae_mode);
        m3AControls->setAeMode(ae_mode);
        LOG1("Changed ae mode to \"%s\" (%d)", newVal.string(), ae_mode);

        if (mPublicShutter >= 0 &&
                (ae_mode == CAM_AE_MODE_SHUTTER_PRIORITY ||
                ae_mode == CAM_AE_MODE_MANUAL)) {
            m3AControls->setManualShutter(mPublicShutter);
            LOG1("Changed shutter to %f", mPublicShutter);
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
            LOGE("User trying to set AE metering mode \"spot\" with an AE metering area.");
        }

        m3AControls->setAeMeteringMode(mode);
        LOG1("Changed ae metering mode to \"%s\" (%d)", newVal.string(), mode);
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
            LOG1("Changed manual iso to \"%s\" (%d)", newVal.string(), iso);
        } else {
            LOG1("Changed auto iso to \"%s\"", newVal.string());
        }
        m3AControls->setIsoMode(iso_mode);
    }
    return status;
}

status_t ControlThread::processParamContrast(const CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              IntelCameraParameters::KEY_CONTRAST_MODE);
    if (!newVal.isEmpty()) {
        if (PlatformData::sensorType(mCameraId) == SENSOR_TYPE_RAW) {
            char value;
            if (newVal == IntelCameraParameters::CONTRAST_MODE_SOFT)
                value = PlatformData::softContrast(mCameraId);
            else if (newVal == IntelCameraParameters::CONTRAST_MODE_HARD)
                value = PlatformData::hardContrast(mCameraId);
            else
                value = 0;

            m3AControls->setContrast(value);
        } else {
            int value;
            if (newVal == IntelCameraParameters::CONTRAST_MODE_SOFT)
                value = EXIF_CONTRAST_SOFT;
            else if (newVal == IntelCameraParameters::CONTRAST_MODE_HARD)
                value = EXIF_CONTRAST_HARD;
            else
                value = EXIF_CONTRAST_NORMAL;

            mHwcg.mIspCI->setContrast(value);
        }
    }
    return status;
}

status_t ControlThread::processParamSaturation(const CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              IntelCameraParameters::KEY_SATURATION_MODE);
    if (!newVal.isEmpty()) {
        if (PlatformData::sensorType(mCameraId) == SENSOR_TYPE_RAW) {
            char value;
            if (newVal == IntelCameraParameters::SATURATION_MODE_LOW)
                value = PlatformData::lowSaturation(mCameraId);
            else if (newVal == IntelCameraParameters::SATURATION_MODE_HIGH)
                value = PlatformData::highSaturation(mCameraId);
            else
                value = 0;

            m3AControls->setSaturation(value);
        } else {
            int value;
            if (newVal == IntelCameraParameters::SATURATION_MODE_LOW)
                value = EXIF_SATURATION_LOW;
            else if (newVal == IntelCameraParameters::SATURATION_MODE_HIGH)
                value = EXIF_SATURATION_HIGH;
            else
                value = EXIF_SATURATION_NORMAL;

            mHwcg.mIspCI->setSaturation(value);
        }
    }
    return status;
}

status_t ControlThread::processParamSharpness(const CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              IntelCameraParameters::KEY_SHARPNESS_MODE);
    if (!newVal.isEmpty()) {
        if (PlatformData::sensorType(mCameraId) == SENSOR_TYPE_RAW) {
            char value;
            if (newVal == IntelCameraParameters::SHARPNESS_MODE_SOFT)
                value = PlatformData::softSharpness(mCameraId);
            else if (newVal == IntelCameraParameters::SHARPNESS_MODE_HARD)
                value = PlatformData::hardSharpness(mCameraId);
            else
                value = 0;

            m3AControls->setSharpness(value);
        } else {
            int value;
            if (newVal == IntelCameraParameters::SHARPNESS_MODE_SOFT)
                value = EXIF_SHARPNESS_SOFT;
            else if (newVal == IntelCameraParameters::SHARPNESS_MODE_HARD)
                value = EXIF_SHARPNESS_HARD;
            else
                value = EXIF_SHARPNESS_NORMAL;

            mHwcg.mIspCI->setSharpness(value);
        }
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
                LOG1("Changed shutter to \"%s\" (%f)", newVal.string(), shutter);
            }
        }
    }

    return status;
}

status_t ControlThread::processParamIntelligentMode(const CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              IntelCameraParameters::KEY_INTELLIGENT_MODE);
    if (!newVal.isEmpty()) {
        PlatformData::setIntelligentMode(mCameraId, newVal == "true" ? true : false);
    }

    return NO_ERROR;
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
            LOGW("Error while setting AWB mode \'%s\' (%d), 3A ctrl type: %d",
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

status_t ControlThread::processPreviewCallbackSize(const CameraParameters *newParams, int videomode)
{
    LOG1("@%s", __FUNCTION__);
    int width, height;

    newParams->getPreviewSize(&width, &height);
    mPreviewThread->setCallbackPreviewSize(width, height, videomode);

    return NO_ERROR;
}

status_t ControlThread::processParamPreviewFrameRate(const CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s : NOTE: DEPRECATED", __FUNCTION__);

    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              CameraParameters::KEY_PREVIEW_FRAME_RATE);

    if (!newVal.isEmpty()) {
        LOGI("DEPRECATED: Got new preview frame rate: %s", newVal.string());
        int fps = newParams->getPreviewFrameRate();
        // Save the set FPS for doing frame dropping
        mISP->setPreviewFramerate(fps);
        mPreviewThread->setPreviewCallbackFps(fps);
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
    int width, height;

    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              IntelCameraParameters::KEY_RECORDING_FRAME_RATE);

    if (!newVal.isEmpty()) {
        int fps = newParams->getInt(IntelCameraParameters::KEY_RECORDING_FRAME_RATE);
        LOG1("Got new recording fps: %d", fps);
        // stop monitoring if not in 1080p@60fps case.
        mParameters.getVideoSize(&width, &height);
        if ((fps <= DEFAULT_RECORDING_FPS || height != RESOLUTION_1080P_HEIGHT) &&
                mThermalThrottleThread->isMonitoring())
            mThermalThrottleThread->stopMonitoring();

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
            mCurrentOrientation = SensorThread::getInstance()->registerOrientationListener(this);
         } else {
            mSaveMirrored = false;
            SensorThread::getInstance()->unRegisterOrientationListener(this);
        }
        LOG1("Changed: %s -> %s", IntelCameraParameters::KEY_SAVE_MIRRORED, newVal.string());
    }

    return NO_ERROR;
}


/**
 * Noise Reduction and Edge Enhancement
 */
status_t ControlThread::processParamNREE(const CameraParameters *oldParams,
        CameraParameters *newParams)
{
    LOG1("@%s", __FUNCTION__);
    String8 newVal = paramsReturnNewIfChanged(oldParams, newParams,
                                              IntelCameraParameters::KEY_NOISE_REDUCTION_AND_EDGE_ENHANCEMENT);

    if (!newVal.isEmpty()) {
        mHwcg.mIspCI->setNrEE((newVal == CameraParameters::TRUE));
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
        LOGD("Application doesn't set picture size, hal chooses %dx%d to match preview size", pictureWidth, pictureHeight);
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
                LOGW("Requested video (%dx%d) aspect ratio does not match preview \
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

    // Capture bracketing
    status = processParamBracket(oldParams, newParams, restartNeeded);

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

    status = processParamDualVideo(oldParams,newParams, restartNeeded);

    status = processParamDualCameraMode(oldParams,newParams);

    status = processParamContinuousShooting(oldParams,newParams, restartNeeded);

    status = processParamDvs(oldParams,newParams);

    status = processParamSDV(oldParams,newParams, &restartNeeded);

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

    /*
     *  Process preview size for Callbacks, preview size will be changed by
     *  ISPLimitations in video mode, but some application allocate buffer
     *  according to the original setting size to store preview JPEG data. So
     *  the original preview size is useful for callbacks when allocating buffer.
     */
    if (status == NO_ERROR)
        processPreviewCallbackSize(newParams, videoMode);

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

    if (PlatformData::supportsColorBarPreview(mCameraId) && (status == NO_ERROR)) {
        // set color-bar test pattern
        status = processParamColorBar(oldParams, newParams, restartNeeded);
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
        LOGE("@%s: strndup failed, len = %d", __FUNCTION__, len);
    }

    mParamCacheLock.unlock();

    return status;
}

/**
 * Create 3A instance according to sensor type and platform requirement:
 * - AtomAIQ for RAW cameras that use IA AIQ
 * - AtomSoc3A for SoC cameras that have their own 3A
 */
status_t ControlThread::createAtom3A()
{
    status_t status = NO_ERROR;

    if (PlatformData::sensorType(mCameraId) == SENSOR_TYPE_RAW && false == PlatformData::isDisable3A(mCameraId)) {
        m3AControls = new AtomAIQ(mHwcg, mCameraId);
    } else if (PlatformData::supportsContinuousJpegCapture(mCameraId)) {
        m3AControls = new AtomExtIsp3A(mCameraId, mHwcg);
    } else {
        m3AControls = new AtomSoc3A(mCameraId, mHwcg);
    }
    if (m3AControls == NULL) {
        LOGE("error creating AAA");
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
    bool needRestartPreview = false;

    CameraAreas newFocusAreas;
    CameraAreas newMeteringAreas;
    String8 str_params(msg->params);
    newParams.unflatten(str_params);

    bool videoMode = android::isParameterSet(CameraParameters::KEY_RECORDING_HINT, newParams) ? true : false;

    // print all old and new params for comparison (debug)
    if (gLogLevel & CAMERA_DEBUG_LOG_LEVEL1) {
        CameraParamsLogger newParamLogger (msg->params);
        CameraParamsLogger oldParamLogger (mParameters.flatten().string());

        LOG1("----------BEGIN PARAM DIFFERENCE----------");
        newParamLogger.dumpDifference(oldParamLogger);
        LOG1("----------END PARAM DIFFERENCE----------");

        LOG2("----------- BEGIN OLD PARAMS -------- ");
        oldParamLogger.dump();
        LOG2("----------- END OLD PARAMS -------- ");

        LOG2("----------- BEGIN NEW PARAMS -------- ");
        newParamLogger.dump();
        LOG2("----------- END NEW PARAMS -------- ");
    }

    status = validateParameters(&oldParams, &newParams, mCameraId);
    if (status != NO_ERROR)
        goto exit;

    if (mCaptureSubState == STATE_CAPTURE_STARTED) {
        LOGE("setParameters happened during capturing. Changing parameters during capturing would produce "
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
        LOGE("bad focus area");
        goto exit;
    }
    LOG1("scanning AE metering areas");
    status = newMeteringAreas.scan(newParams.get(CameraParameters::KEY_METERING_AREAS),
                                   m3AControls->getAeMaxNumWindows());
    if (status != NO_ERROR) {
        LOGE("bad metering area");
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
                 mState == STATE_CONTINUOUS_CAPTURE ||
                 mState == STATE_JPEG_CAPTURE) {

            // Preview needs to be restarted if the preview mode changes, or
            // with any picture size change when in continuous mode.
            if (selectPreviewMode(newParams) != mState ||
                mState == STATE_CONTINUOUS_CAPTURE || mState == STATE_JPEG_CAPTURE) {
                needRestartPreview = true;
                videoMode = false;
                // cancel picture processing to get all snapshot buffers back to its nest
                cancelPictureThread();
            }
        }

        if (mWarperService != NULL) {
            status = mWarperService->updateFrameDimensions(picWidth, picHeight);
            if (status != NO_ERROR) {
                LOGE("Failed to update Warper Service frame dimensions.");
                goto exit;
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
        allocateSnapshotAndPostviewBuffers(videoMode);
    }

    ProcessOverlayEnable(&oldParams, &newParams);

    if (needRestartPreview == true) {

        if (msg->stopPreviewRequest) {
            if (mState != STATE_CONTINUOUS_CAPTURE)
                LOGD("%s: Invalid stopPreviewRequest!", __FUNCTION__);
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
            case STATE_JPEG_CAPTURE:
                status = restartPreview(videoMode);
                break;
            case STATE_STOPPED:
                break;
            default:
                LOGE("formats can only be changed while in preview or stop states");
                break;
        }
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
    LOG2("@%s, line:%d, cmd:%d, arg1:%d, arg2:%d", __FUNCTION__, __LINE__, msg->cmd_id, msg->arg1, msg->arg2);
    status_t status = BAD_VALUE;
    switch (msg->cmd_id)
    {
    case CAMERA_CMD_START_FACE_DETECTION:
        status = startFaceDetection();
        break;
    case CAMERA_CMD_STOP_FACE_DETECTION:
        status = stopFaceDetection();
        break;
    case CAMERA_CMD_BURST_START:
    case CAMERA_CMD_START_CONTINUOUS_SHOOTING:
        mContShootingEnabled = true;
        mCallbacks->setContShooting(mContShootingEnabled,
            mParameters.get(IntelCameraParameters::KEY_CONTINUOUS_SHOOTING_FILEPATH));
        status = prepareContinuousShooting();
        break;
    case CAMERA_CMD_BURST_STOP:
    case CAMERA_CMD_STOP_CONTINUOUS_SHOOTING:
        status = finalizeContinuousShooting();
        mContShootingEnabled = false;
        mCallbacks->setContShooting(mContShootingEnabled);
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
    case CAMERA_CMD_EXTISP_HDR:
        if (msg->arg1 == EXTISP_FEAT_ON)
            mHwcg.mIspCI->setShotMode(EXT_ISP_SHOT_MODE_RICH_TONE_HDR);
        else
            mHwcg.mIspCI->setShotMode(EXT_ISP_SHOT_MODE_AUTO); // todo restore previous HAL shot mode when that is implemented

        mHwcg.mIspCI->setHDR(msg->arg1);
        mExtIsp.HDR = (msg->arg1 == EXTISP_FEAT_ON);
        status = NO_ERROR;
        break;
    case CAMERA_CMD_EXTISP_LLS:
        status = handleLowLightMode(msg->arg1 == EXTISP_FEAT_ON);
        break;
    case CAMERA_CMD_EXTISP_KIDS_MODE:
        status = handleKidsMode(msg->arg1);
        break;
    case CAMERA_CMD_AUTO_LOW_LIGHT:
        mSmartStabilization = (msg->arg1 == 1);
        mPostProcThread->setAutoLowLightReporting(msg->arg1 == 1);
        status = NO_ERROR;
        break;
    case CAMERA_CMD_FRONT_SS:
        mSmartStabilization = (msg->arg1 == 1);
        break;
    case CAMERA_CMD_PAUSE_PREVIEW_FRAME_UPDATE:
        status = mPreviewThread->pausePreviewFrameUpdate();
        break;
    case CAMERA_CMD_RESUME_PREVIEW_FRAME_UPDATE:
        status = mPreviewThread->resumePreviewFrameUpdate();
        break;
    case CAMERA_CMD_SET_PREVIEW_FRAME_CAPTURE_ID:
        status = mPreviewThread->setPreviewFrameCaptureId(msg->arg1);
        break;
    default:
        break;
    }

    if (status != NO_ERROR)
        LOGE("@%s command id %d failed", __FUNCTION__, msg->cmd_id);
    return status;
}

void ControlThread::lowLightDetected(bool needLLS)
{
    LOG2("@%s", __FUNCTION__);

    Message msg;
    msg.id = MESSAGE_ID_COMMAND;
    msg.data.command.cmd_id = CAMERA_CMD_EXTISP_LLS;
    msg.data.command.arg1 = needLLS ? EXTISP_FEAT_ON : EXTISP_FEAT_OFF;
    mMessageQueue.send(&msg);
}

status_t ControlThread::handleLowLightMode(bool enableLLS)
{
    LOG1("@%s: enableLLS = %s", __FUNCTION__, enableLLS ? "true" : "false");

    if (enableLLS)
        mHwcg.mIspCI->setShotMode(EXT_ISP_SHOT_MODE_NIGHT);
    else
        mHwcg.mIspCI->setShotMode(EXT_ISP_SHOT_MODE_AUTO); // todo restore previous HAL shot mode when that is implemented

    mHwcg.mIspCI->setLLS(enableLLS);
    mExtIsp.LLS = enableLLS;
    return NO_ERROR;
}

status_t ControlThread::handleKidsMode(int value)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = OK;
    mExtIsp.kidsMode = (value == 1);
    if (mExtIsp.kidsMode)
        mPreviewThread->setCallbackMode(PreviewThread::PREVIEW_CALLBACK_BEFORE_DISPLAY);
    else
        mPreviewThread->setCallbackMode(PreviewThread::PREVIEW_CALLBACK_NORMAL);

    // if preview is running we need
    // to stop, reconfigure, and restart the isp and all threads.
    // this is because kids mode has the special NV21 preview format.
    switch (mState) {
        case STATE_PREVIEW_VIDEO:
        case STATE_PREVIEW_STILL:
        case STATE_CONTINUOUS_CAPTURE:
        case STATE_JPEG_CAPTURE:
            status = restartPreview(false);
            break;
        case STATE_STOPPED:
            break;
        default:
            LOGE("kids mode can only be changed while in preview or stop states");
            break;
    }

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
            mState != STATE_CONTINUOUS_CAPTURE &&
            mState != STATE_JPEG_CAPTURE) {
        LOGE("Cannot configure metadata buffers in this state: %d", mState);
        status = BAD_VALUE;
        mMessageQueue.reply(MESSAGE_ID_STORE_METADATA_IN_BUFFER, status);
        return status;
    }

    //find the setted buffer sharing session ID
    int sID = mParameters.getInt(IntelCameraParameters::REC_BUFFER_SHARING_SESSION_ID);
    status = mISP->storeMetaDataInBuffers(msg->enabled, sID);
    if(status == NO_ERROR)
        status = mCallbacks->storeMetaDataInBuffers(msg->enabled);
    else
        LOGE("Error configuring metadatabuffers in ISP!");

    mMessageQueue.reply(MESSAGE_ID_STORE_METADATA_IN_BUFFER, status);
    return status;
}

void ControlThread::postCaptureProcesssingDone(IPostCaptureProcessItem* item, status_t procStatus)
{
    LOG1("@%s", __FUNCTION__);
    // send message
    Message msg;
    msg.id = MESSAGE_ID_POST_CAPTURE_PROCESSING_DONE;
    msg.data.postCapture.item = item;
    msg.data.postCapture.status = procStatus;

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
        LOGW("PostCapture Processing failed !!");
        goto cleanup;
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
     * processedBuffer and postviewBuffer were originally with status FRAME_STATUS_SKIPPED
     * to avoid being pushed to corresponding available buffers.
     * Marking status as FRAME_STATUS_OK enables it to be made available again.
     */
    processedBuffer.status = FRAME_STATUS_OK;
    processedBuffer.type = ATOM_BUFFER_ULL;
    postviewBuffer.status = FRAME_STATUS_OK;
    postviewBuffer.type = ATOM_BUFFER_ULL;

    status = mPictureThread->encode(picMetaData, &processedBuffer, &postviewBuffer, false);
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
        LOGE("%s input buffer (n = %d) and postview buffer count (m = %d) mismatch",
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
        LOGE("%s:We do not have any snapshotbuffers yet... find the bug",__FUNCTION__);
        return NO_MEMORY;
    }

    int size = mAllocatedSnapshotBuffers[0].size;
    int width = mAllocatedSnapshotBuffers[0].width;
    int bpl = mAllocatedSnapshotBuffers[0].bpl;
    int height = mAllocatedSnapshotBuffers[0].height;
    int fourcc = mAllocatedSnapshotBuffers[0].fourcc;

    mCallbacks->allocateMemory(&mHdr.outMainBuf, size);
    if (mHdr.outMainBuf.dataPtr == NULL) {
        LOGE("HDR: Error allocating memory for HDR main buffer!");
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
        LOGE("HDR: Error allocating memory for HDR postview buffer!");
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
        LOGE("HDR: Error allocating memory for HDR CI buffers!");
        return NO_MEMORY;
    }

    status = AtomCP::setIaFrameFormat(&mHdr.ciBufOut.ciMainBuf[0], fourcc);
    if (status != NO_ERROR) {
        LOGE("HDR: pixel format %s 0x%x not supported", v4l2Fmt2Str(fourcc), fourcc);
        return status;
    }

    mHdr.ciBufOut.ciMainBuf->data = mHdr.outMainBuf.dataPtr;
    mHdr.ciBufOut.ciMainBuf[0].width = mHdr.outMainBuf.width = width;
    mHdr.ciBufOut.ciMainBuf[0].stride = mHdr.outMainBuf.bpl = bpl;
    mHdr.ciBufOut.ciMainBuf[0].height = mHdr.outMainBuf.height = height;
    mHdr.outMainBuf.fourcc = fourcc;
    mHdr.ciBufOut.ciMainBuf[0].size = mHdr.outMainBuf.size = size;

    LOG1("HDR: Initialized output CI main     buff @%p: (data=%p, size=%d, width=%d, height=%d, fourcc=%s 0x%x)",
            &mHdr.ciBufOut.ciMainBuf[0],
            mHdr.ciBufOut.ciMainBuf[0].data,
            mHdr.ciBufOut.ciMainBuf[0].size,
            mHdr.ciBufOut.ciMainBuf[0].width,
            mHdr.ciBufOut.ciMainBuf[0].height,
            v4l2Fmt2Str(mHdr.ciBufOut.ciMainBuf[0].format),
            mHdr.ciBufOut.ciMainBuf[0].format);

    mHdr.ciBufOut.ciPostviewBuf[0].data = mHdr.outPostviewBuf.dataPtr;
    mHdr.ciBufOut.ciPostviewBuf[0].width = mHdr.outPostviewBuf.width = pvWidth;
    mHdr.ciBufOut.ciPostviewBuf[0].stride = mHdr.outPostviewBuf.bpl = pvWidth;
    mHdr.ciBufOut.ciPostviewBuf[0].height = mHdr.outPostviewBuf.height = pvHeight;
    AtomCP::setIaFrameFormat(&mHdr.ciBufOut.ciPostviewBuf[0], fourcc);
    mHdr.outPostviewBuf.fourcc = fourcc;
    mHdr.ciBufOut.ciPostviewBuf[0].size = mHdr.outPostviewBuf.size = pvSize;

    LOG1("HDR: Initialized output CI postview buff @%p: (data=%p, size=%d, width=%d, height=%d, fourcc=%s 0x%x",
            &mHdr.ciBufOut.ciPostviewBuf[0],
            mHdr.ciBufOut.ciPostviewBuf[0].data,
            mHdr.ciBufOut.ciPostviewBuf[0].size,
            mHdr.ciBufOut.ciPostviewBuf[0].width,
            mHdr.ciBufOut.ciPostviewBuf[0].height,
            v4l2Fmt2Str(mHdr.ciBufOut.ciPostviewBuf[0].format),
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

    LOG1("HDR: Initialized input CI main     buff %d @%p: (addr=%p, length=%d, width=%d, height=%d, fourcc=%s 0x%x)",
            mBurstCaptureNum,
            &mHdr.ciBufIn.ciMainBuf[mBurstCaptureNum],
            mHdr.ciBufIn.ciMainBuf[mBurstCaptureNum].data,
            mHdr.ciBufIn.ciMainBuf[mBurstCaptureNum].size,
            mHdr.ciBufIn.ciMainBuf[mBurstCaptureNum].width,
            mHdr.ciBufIn.ciMainBuf[mBurstCaptureNum].height,
            v4l2Fmt2Str(mHdr.ciBufIn.ciMainBuf[mBurstCaptureNum].format),
            mHdr.ciBufIn.ciMainBuf[mBurstCaptureNum].format);

    mHdr.ciBufIn.ciPostviewBuf[mBurstCaptureNum].data = postviewBuffer->dataPtr;
    mHdr.ciBufIn.ciPostviewBuf[mBurstCaptureNum].width = postviewBuffer->width;
    mHdr.ciBufIn.ciPostviewBuf[mBurstCaptureNum].stride = postviewBuffer->bpl;
    mHdr.ciBufIn.ciPostviewBuf[mBurstCaptureNum].height = postviewBuffer->height;
    mHdr.ciBufIn.ciPostviewBuf[mBurstCaptureNum].size = postviewBuffer->size;
    AtomCP::setIaFrameFormat(&mHdr.ciBufIn.ciPostviewBuf[mBurstCaptureNum], postviewBuffer->fourcc);

    LOG1("HDR: Initialized input CI postview buff %d @%p: (addr=%p, length=%d, width=%d, height=%d, fourcc=%s 0x%x)",
            mBurstCaptureNum,
            &mHdr.ciBufIn.ciPostviewBuf[mBurstCaptureNum],
            mHdr.ciBufIn.ciPostviewBuf[mBurstCaptureNum].data,
            mHdr.ciBufIn.ciPostviewBuf[mBurstCaptureNum].size,
            mHdr.ciBufIn.ciPostviewBuf[mBurstCaptureNum].width,
            mHdr.ciBufIn.ciPostviewBuf[mBurstCaptureNum].height,
            v4l2Fmt2Str(mHdr.ciBufIn.ciPostviewBuf[mBurstCaptureNum].format),
            mHdr.ciBufIn.ciPostviewBuf[mBurstCaptureNum].format);

    mHdr.inputBuffers[mBurstCaptureNum].snapshotBuf = *snapshotBuffer;
    mHdr.inputBuffers[mBurstCaptureNum].postviewBuf = *postviewBuffer;

    return NO_ERROR;
}

void ControlThread::hdrRelease()
{
    LOG1("@%s", __FUNCTION__);
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
    CLEAR(gbce_results);

    // initialize the meta data with last picture of
    // the HDR sequence
    PictureThread::MetaData hdrPicMetaData;
    fillPicMetaData(hdrPicMetaData, false);

    // Collect the GBCE results if Intel 3A is available
    if (m3AControls->isIntel3A()) {
        status = m3AControls->getGBCEResults(&gbce_results);
        if (status != NO_ERROR) {
            hdrPicMetaData.free(m3AControls);
            LOGE("Error collecting the GBCE results!");
            return status;
        }
    }

    /*
     * Stop ISP before composing HDR since standalone acceleration requires ISP to be stopped.
     * The below call won't release the capture buffers since they are needed by HDR compose
     * method. The capture buffers will be released in stopCapture method.
     */
    if (mState == STATE_CONTINUOUS_CAPTURE) {
        status = stopPreviewCore(false);
    } else if (mState == STATE_CAPTURE) {
        status = mISP->stop();
    }
    if (status != NO_ERROR) {
        hdrPicMetaData.free(m3AControls);
        LOGE("Error stopping ISP!");
        return status;
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
        status = mPictureThread->encode(hdrPicMetaData, &mHdr.outMainBuf, &mHdr.outPostviewBuf, false);
        if (status == NO_ERROR) {
            doEncode = true;
        }
    } else {
        LOGE("HDR Composition failed !");
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
 * \return error if failed
 */
status_t ControlThread::setExternalSnapshotBuffers(int fourcc, int width, int height)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    uint32_t numberOfSnapshots, numToSet;

    status = allocateSnapshotAndPostviewBuffers(false);
    if (status == INVALID_OPERATION && mAvailableSnapshotBuffers.size()) {
        LOGW("@%s can't allocate buffer atm, available:%d", __FUNCTION__, mAvailableSnapshotBuffers.size());
    } else if (status != NO_ERROR) {
        LOGE("allocateSnapshotAndPostviewBuffers error:%d", status);
        return status;
    } else if (mAvailableSnapshotBuffers.size() == 0) {
        LOGE("@%s no available buffer after allocation, find the bug", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    numberOfSnapshots = MAX(1,mBurstLength);
    if (mContShootingState != CONT_SHOOTING_NONE && mState == STATE_CONTINUOUS_CAPTURE) {
        numToSet = mAvailableSnapshotBuffers.size();
    } else {
        numToSet = MIN(numberOfSnapshots, mAvailableSnapshotBuffers.size());
        if (numToSet < numberOfSnapshots)
            mBurstBufsToReturn = numberOfSnapshots - numToSet;
    }

    LOG1("@%s number of snapshots %d: numToSet:%d return for burst:%d Available %d Allocated: %d ",
            __FUNCTION__, numberOfSnapshots, numToSet, mBurstBufsToReturn,
            mAvailableSnapshotBuffers.size(), mAllocatedSnapshotBuffers.size());

    // NOTE: Workaround: for RAW capture, we select the closest matching
    // sensor output frame size, and allocate buffers based on sensor output
    // size, instead of application-requested size. See checkAndUpdateRawSize().
    // Hence, omitting this size check for RAW.
    if (!CameraDump::isDumpImageEnable(CAMERA_DEBUG_DUMP_RAW) &&
        (mAllocatedSnapshotBuffers[0].width != width ||
        mAllocatedSnapshotBuffers[0].height != height)) {
        LOGE("We got allocated snapshot buffers of wrong resolution (%dx%d), "
                "this should not happen!! we wanted (%dx%d)",
                mAllocatedSnapshotBuffers[0].width,
                mAllocatedSnapshotBuffers[0].height,
                width, height);
        return UNKNOWN_ERROR;
    }

    if (mContShootingState != CONT_SHOOTING_NONE && mState == STATE_CONTINUOUS_CAPTURE) {
        // save all the buffer in ISP
        mSnapshotBuffersInISP.clear();
        mPostviewBuffersInISP.clear();
        size_t index = mAvailableSnapshotBuffers.size() - 1;
        for (size_t i = 0 ; i < numToSet ; i++) {
            mSnapshotBuffersInISP.push(mAvailableSnapshotBuffers.itemAt(index));
            mPostviewBuffersInISP.push(mAvailablePostviewBuffers.itemAt(index));
            index--;
        }
    }

    bool cached = false;
    status = mISP->setSnapshotBuffers(&mAvailableSnapshotBuffers, numToSet, cached);
    status = mISP->setPostviewBuffers(&mAvailablePostviewBuffers, numToSet, cached);
    return NO_ERROR;
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
        LOGE("starting FD in stop state");
        return INVALID_OPERATION;
    }

    m3AControls->setFaceDetection(true);

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

    m3AControls->setFaceDetection(false);
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
        LOGW("@%s: face recognition already started", __FUNCTION__);
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
    LOG1("@%s", __FUNCTION__);
    // intel parameters support more effects
    // so use supported effects list stored in mIntelParameters.
    if (mIntelParameters.get(CameraParameters::KEY_SUPPORTED_EFFECTS))
        mParameters.remove(CameraParameters::KEY_SUPPORTED_EFFECTS);

    status_t status = NO_ERROR;
    String8 params(mParameters.flatten());
    String8 intel_params(mIntelParameters.flatten());
    String8 delimiter(";");
    params += delimiter;
    params += intel_params;

    CameraParameters mergedParameters;
    mergedParameters.unflatten(params);

    status = processDynamicParameters(&mParameters, &mergedParameters);
    if (status != NO_ERROR) {
        LOGE("@%s - processDynamicParameters failed", __FUNCTION__);
        mIntelParamsAllowed = false;
    } else {
        mParameters = mergedParameters;
        updateParameterCache();
        mIntelParamsAllowed = true;
    }

    return status;
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
        LOGE("Must enable ISP extensions in stop state");
        return INVALID_OPERATION;
    }
    if (mIspExtensionsEnabled) {
        LOGD("ISP extensions already enabled");
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
            LOGE("Invalid message");
            status = BAD_VALUE;
            break;
    };

    if (status != NO_ERROR)
        LOGE("Error handling message: %d", (int) msg.id);
    return status;
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

bool ControlThread::atomIspNotify(IAtomIspObserver::Message *msg, const ObserverState state)
{
    LOG2("@%s", __FUNCTION__);

    if (msg) {
        AtomBuffer *buff = &msg->data.frameBuffer.buff;
        if (msg->id != IAtomIspObserver::MESSAGE_ID_FRAME) {
            LOG1("Received unexpected notify message id %d!", msg->id);
            if (msg->id == IAtomIspObserver::MESSAGE_ID_ERROR) {
                LOGE("Error in preview stream");
                errorPreview();
            }
            return false;
        }

        // controlling offline capture by exposure id
        handleOfflineCaptureControl(buff);

        //Check the battery status regularly during recording.
        //If the battery level is too low, turn off the flash, notify the application and update the parameters.
        if (mState == STATE_RECORDING && (buff->frameSequenceNbr % BATTERY_CHECK_INTERVAL_FRAME_UNIT) == 0) {
            // note: String8 segfaults if given a NULL, so thus check it for that here
            const char* flash_mode = mParameters.get(CameraParameters::KEY_FLASH_MODE);
            String8 val(flash_mode, (flash_mode == NULL ? 0 : strlen(flash_mode)));
            if (val != CameraParameters::FLASH_MODE_OFF) {
                CameraParameters param(mParameters);
                preProcessFlashMode(&param);
                processParamFlash(&mParameters,&param);
            }
        }
    }
    return false;
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
                if (burstMoreCapturesNeeded()) {
                    status = captureFixedBurstPic();
                } else if (mContShootingState == CONT_SHOOTING_STARTED && !holdOnContinuousShooting()) {
                    LOG1("@%s continuous shooting for next", __FUNCTION__);
                    captureContinuousShooting(false);
                } else
                    status = waitForAndExecuteMessage();
            }
            break;

        case STATE_JPEG_CAPTURE:
            LOG2("In STATE_JPEG_CAPTURE...");
            status = waitForAndExecuteMessage();
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

    /**
     * The ISP flip parameter needs a little time to take effect. Set ISP mirror early when
     * phone orientation changed can speed up capture or recording start
     */
    if (mSaveMirrored && (PlatformData::cameraFacing(mCameraId) == CAMERA_FACING_FRONT))
        mISP->setIspMirror(getNeededIspFlipType());

    return NO_ERROR;
}

} // namespace android
