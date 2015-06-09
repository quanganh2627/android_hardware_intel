/*
 * Copyright (C) 2011 The Android Open Source Project
 * Copyright (C) 2011,2012,2013 Intel Corporation
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
#define LOG_TAG "Camera_ISP"

#include "LogHelper.h"
#include "AtomISP.h"
#include "Callbacks.h"
#include "ColorConverter.h"
#include "PlatformData.h"
#include "IntelParameters.h"
#include "PanoramaThread.h"
#include "CameraDump.h"
#include "MemoryUtils.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <poll.h>
#include <linux/media.h>
#include <linux/atomisp.h>
#include "PerformanceTraces.h"
#include <binder/IMemory.h>
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>

#define DEFAULT_SENSOR_FPS      15.0
#define DEFAULT_PREVIEW_FPS     30.0

#define MAX_FILE_INJECTION_SNAPSHOT_WIDTH    4160
#define MAX_FILE_INJECTION_SNAPSHOT_HEIGHT   3104
#define MAX_FILE_INJECTION_PREVIEW_WIDTH     1280
#define MAX_FILE_INJECTION_PREVIEW_HEIGHT    720
#define MAX_FILE_INJECTION_RECORDING_WIDTH   1920
#define MAX_FILE_INJECTION_RECORDING_HEIGHT  1088

#define MAX_SUPPORT_ZOOM        1600    // Support upto 16x and should not bigger than 99x
#define ZOOM_RATIO              100     // Conversion between zoom to really zoom effect
#define VIDEO_ZOOM_SKIP_FRAMES  3

#define INTEL_FILE_INJECT_CAMERA_ID 2

#define ATOMISP_PREVIEW_POLL_TIMEOUT 1000
#define ATOMISP_GETFRAME_RETRY_COUNT 60  // Times to retry poll/dqbuf in case of error
#define ATOMISP_GETFRAME_STARVING_WAIT 33000 // Time to usleep between retry's when stream is starving from buffers.
#define ATOMISP_MIN_CONTINUOUS_BUF_SIZE 3 // Min buffer len supported by CSS
#define ATOMISP_MIN_CONTINUOUS_BUF_NUM_CSS2X 5 // Min buffer len supported by CSS2.x
namespace android {

////////////////////////////////////////////////////////////////////
//                          STATIC DATA
////////////////////////////////////////////////////////////////////
Mutex AtomISP::sISPCountLock;
static struct devNameGroup devName[MAX_CAMERAS] = {
    {{"/dev/video0", "/dev/video1", "/dev/video2", "/dev/video3", "/dev/video4"},
        false,},
    {{"/dev/video5", "/dev/video6", "/dev/video7", "/dev/video8", "/dev/video9"},
        false,},
};

////////////////////////////////////////////////////////////////////
//                          PUBLIC METHODS
////////////////////////////////////////////////////////////////////

AtomISP::AtomISP(int cameraId, sp<ScalerService> scalerService, Callbacks *callbacks) :
     mPreviewStreamSource("PreviewStreamSource", this)
    ,m3AStatSource("AAAStatSource", this)
    ,mCameraId(cameraId)
    ,mGroupIndex (-1)
    ,mMode(MODE_NONE)
    ,mCallbacks(callbacks)
    ,mPreviewBuffersCached(true)
    ,mRecordingBuffers(NULL)
    ,mSwapRecordingDevice(false)
    ,mRecordingDeviceSwapped(false)
    ,mPreviewTooBigForVFPP(false)
    ,mHALZSLEnabled(false)
    ,mHALZSLBuffers(NULL)
    ,mClientSnapshotBuffersCached(true)
    ,mUsingClientSnapshotBuffers(false)
    ,mUsingClientPostviewBuffers(false)
    ,mStoreMetaDataInBuffers(false)
    ,mBufferSharingSessionID(DEFAULT_BUFFER_SHARING_SESSION_ID)
    ,mNumPreviewBuffersQueued(0)
    ,mNumRecordingBuffersQueued(0)
    ,mNumCapturegBuffersQueued(0)
    ,mFlashTorchSetting(0)
    ,mContCaptPrepared(false)
    ,mContCaptPriority(false)
    ,mInitialSkips(0)
    ,mStatisticSkips(0)
    ,mVideoZoomFrameSkips(0)
    ,mSensorHW(cameraId)
    ,mSessionId(0)
    ,mLowLight(false)
    ,mXnr(0)
    ,mZoomRatios(NULL)
    ,mRawDataDumpSize(0)
    ,m3AStatRequested(0)
    ,m3AStatscEnabled(false)
    ,mColorEffect(V4L2_COLORFX_NONE)
    ,mScaler(scalerService)
    ,mObserverManager()
    ,mNoiseReductionEdgeEnhancement(true)
    ,mFlashIsOn(false)
{
    LOG1("@%s", __FUNCTION__);

    CLEAR(mSnapshotBuffers);
    CLEAR(mContCaptConfig);
    mPostviewBuffers.clear();
    mPreviewBuffers.clear();
    CLEAR(mConfig);
    mFileInject.clear();
}

status_t AtomISP::initDevice()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    if (mGroupIndex < 0) {
        ALOGE("No mGroupIndex set. Could not run device init!");
        return NO_INIT;
    }

    mMainDevice.clear();
    mPreviewDevice.clear();
    mPostViewDevice.clear();
    mRecordingDevice.clear();
    m3AEventSubdevice.clear();
    mFileInjectDevice.clear();
    mOriginalPreviewDevice.clear();

    mMainDevice = new V4L2VideoNode(devName[mGroupIndex].dev[V4L2_MAIN_DEVICE], V4L2_MAIN_DEVICE);
    mPreviewDevice = new V4L2VideoNode(devName[mGroupIndex].dev[V4L2_PREVIEW_DEVICE], V4L2_PREVIEW_DEVICE);
    mPostViewDevice = new V4L2VideoNode(devName[mGroupIndex].dev[V4L2_POSTVIEW_DEVICE], V4L2_POSTVIEW_DEVICE);
    mRecordingDevice = new V4L2VideoNode(devName[mGroupIndex].dev[V4L2_RECORDING_DEVICE], V4L2_RECORDING_DEVICE);
    m3AEventSubdevice = new V4L2Subdevice(PlatformData::getISPSubDeviceName(),V4L2_ISP_SUBDEV);
    mFileInjectDevice = new V4L2VideoNode(devName[mGroupIndex].dev[V4L2_INJECT_DEVICE], V4L2_INJECT_DEVICE,
                                          V4L2VideoNode::OUTPUT_VIDEO_NODE);

    /**
     * In some situation we are swapping the preview device to be capturing device
     * like in HAL ZSL, or in cases where there is a limitation in ISP.
     * We keep this variable to the original preview device to be able to restore
     */
    mOriginalPreviewDevice = mPreviewDevice;

    status = mMainDevice->open();
    if (status != NO_ERROR) {
        ALOGE("Failed to open first device!");
        return NO_INIT;
    }

    struct v4l2_capability aCap;
    CLEAR(aCap);
    status = mMainDevice->queryCap(&aCap);
    if (status != NO_ERROR) {
        ALOGE("Failed basic capability check failed!");
        return NO_INIT;
    }

    PERFORMANCE_TRACES_BREAKDOWN_STEP("Open_Main_Device");

    initFileInject();

    mSensorHW.selectActiveSensor(mMainDevice);

    mSensorType = PlatformData::sensorType(mCameraId);
    LOG1("Sensor type detected: %s", (mSensorType == SENSOR_TYPE_RAW)?"RAW":"SOC");
    return status;
}

status_t AtomISP::initDVS()
{
    // OK for DVS1. Not supported.
    return NO_ERROR;
}
/**
 * Closes the main device
 *
 * This is specifically provided for error recovery and
 * expected to be called after AtomISP::stop(), where the
 * rest of the devices are already closed and associated
 * buffers are all freed.
 *
 * TODO: protect AtomISP API agains uninitialized state and
 *       support direct uninit regardless of the state.
 */
void AtomISP::deInitDevice()
{
    LOG1("@%s", __FUNCTION__);
    mMainDevice->close();
}

/**
 * Checks if main device is open
 */
bool AtomISP::isDeviceInitialized() const
{
    LOG1("@%s", __FUNCTION__);
    return mMainDevice->isOpen();
}

status_t AtomISP::init()
{
    LOG1("@%s", __FUNCTION__);

    status_t status = NO_ERROR;

    mConfig.num_recording_buffers = PlatformData::getRecordingBufNum();
    mConfig.num_preview_buffers = PlatformData::getPreviewBufNum();

    if (mGroupIndex < 0) {
        Mutex::Autolock lock(sISPCountLock);
        for (int i = 0; i < MAX_CAMERAS; i++) {
            if (devName[i].in_use == false) {
                mGroupIndex = i;
                devName[i].in_use = true;
                break;
            }
        }
        LOG1("@%s: new mGroupIndex = %d", __FUNCTION__, mGroupIndex);
    } else {
        LOG1("@%s: using old mGroupIndex = %d", __FUNCTION__, mGroupIndex);
    }

    if (mGroupIndex < 0) {
        ALOGE("No free device. Inititialize Atomisp failed.");
        return NO_INIT;
    }

    status = initDevice();
    if (status != NO_ERROR) {
        ALOGE("Device inititialize failure. Inititialize Atomisp failed.");
        return NO_INIT;
    }

    PERFORMANCE_TRACES_BREAKDOWN_STEP("Init_3A");

    initFrameConfig();

    AtomBuffer formatDescriptorPv
            = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_FORMAT_DESCRIPTOR, V4L2_PIX_FMT_NV12, RESOLUTION_POSTVIEW_WIDTH, RESOLUTION_POSTVIEW_HEIGHT);
    // Initialize the frame sizes
    setPreviewFrameFormat(RESOLUTION_VGA_WIDTH, RESOLUTION_VGA_HEIGHT,
                          pixelsToBytes(PlatformData::getPreviewPixelFormat(), RESOLUTION_VGA_WIDTH),
                          PlatformData::getPreviewPixelFormat());
    setPostviewFrameFormat(formatDescriptorPv);

    AtomBuffer formatDescriptorSs
        = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_FORMAT_DESCRIPTOR, V4L2_PIX_FMT_NV12);

    /* for stretch problems(BZ: 152956 and BZ: 140905),
     * hal doesn't set default picture size here, uses 0x0 as default.
     * then, in processStaticParameters(), if hal finds no valid picture size is set,
     * it will choose a picture size, if there is a valid picture size, it will use it.
     */
    setSnapshotFrameFormat(formatDescriptorSs);

    setVideoFrameFormat(RESOLUTION_VGA_WIDTH, RESOLUTION_VGA_HEIGHT, V4L2_PIX_FMT_NV12);

    status = computeZoomRatios();
    fetchIspVersions();

    return status;
}

/**
 * Returns IHWSensorControl interface
 *
 * Note: not ref counted
 *
 * Local mSensorHW is SensorHW object that implements IHWSensorControl.
 * Interface accessor is valid once AtomISP is initialized and remains
 * valid through AtomISP composition. This lifespan means roughly the
 * time when camera device remains open.
 */
IHWSensorControl* AtomISP::getSensorControlInterface()
{
    return isDeviceInitialized() ? (IHWSensorControl*) &mSensorHW : NULL;
}

/**
 * Convert zoom value to zoom ratio
 *
 * @param zoomValue zoom value to convert to zoom ratio
 *
 * @return zoom ratio multiplied by 100
 */
int AtomISP::zoomRatio(int zoomValue) const {
    return mZoomRatioTable[zoomValue];
}

void AtomISP::setRecordingFramerate(int fps)
{
    LOG1("@%s fps: %d", __FUNCTION__, fps);
    mConfig.recording_fps = fps;
}

/**
 * Only to be called from 2nd stage contructor AtomISP::init().
 */
void AtomISP::initFrameConfig()
{
    mConfig.fps = DEFAULT_PREVIEW_FPS;
    mConfig.preview_fps = DEFAULT_PREVIEW_FPS;
    mConfig.recording_fps = 0; // Default is the same as preview
    mConfig.num_snapshot = 1;
    mConfig.zoom = 0;

    if (mIsFileInject) {
        mConfig.snapshotLimits.maxWidth = MAX_FILE_INJECTION_SNAPSHOT_WIDTH;
        mConfig.snapshotLimits.maxHeight = MAX_FILE_INJECTION_SNAPSHOT_HEIGHT;
        mConfig.previewLimits.maxWidth = MAX_FILE_INJECTION_PREVIEW_WIDTH;
        mConfig.previewLimits.maxHeight = MAX_FILE_INJECTION_PREVIEW_HEIGHT;
        mConfig.recordingLimits.maxWidth = MAX_FILE_INJECTION_RECORDING_WIDTH;
        mConfig.recordingLimits.maxHeight = MAX_FILE_INJECTION_RECORDING_HEIGHT;
    }
    else {
        getMaxSnapShotSize(mCameraId, &(mConfig.snapshotLimits.maxWidth), &(mConfig.snapshotLimits.maxHeight));
    }

    if (mConfig.snapshotLimits.maxWidth >= RESOLUTION_1080P_WIDTH
        && mConfig.snapshotLimits.maxHeight >= RESOLUTION_1080P_HEIGHT) {
        mConfig.previewLimits.maxWidth = RESOLUTION_1080P_WIDTH;
        mConfig.previewLimits.maxHeight = RESOLUTION_1080P_HEIGHT;
    } else {
        mConfig.previewLimits.maxWidth = mConfig.snapshotLimits.maxWidth;
        mConfig.previewLimits.maxHeight =  mConfig.snapshotLimits.maxHeight;
    }

    if (mConfig.snapshotLimits.maxWidth >= RESOLUTION_1080P_WIDTH
        && mConfig.snapshotLimits.maxHeight >= RESOLUTION_1080P_HEIGHT) {
        mConfig.recordingLimits.maxWidth = RESOLUTION_1080P_WIDTH;
        mConfig.recordingLimits.maxHeight = RESOLUTION_1080P_HEIGHT;
    } else {
        mConfig.recordingLimits.maxWidth = mConfig.snapshotLimits.maxWidth;
        mConfig.recordingLimits.maxHeight = mConfig.snapshotLimits.maxHeight;
    }
}

/**
 * Only to be called from 2nd stage contructor AtomISP::init().
 */
void AtomISP::initFileInject()
{
    mIsFileInject = (PlatformData::supportsFileInject() == true) && (mCameraId == INTEL_FILE_INJECT_CAMERA_ID);
    mFileInject.active = false;
}

AtomISP::~AtomISP()
{
    LOG1("@%s", __FUNCTION__);
    Mutex::Autolock lock(sISPCountLock);
    if (mGroupIndex >= 0) {
        devName[mGroupIndex].in_use = false;
    }

    /*
     * The destructor is called when the hw_module close mehod is called. The close method is called
     * in general by the camera client when it's done with the camera device, but it is also called by
     * System Server when the camera application crashes. System Server calls close in order to release
     * the camera hardware module. So, if we are not in MODE_NONE, it means that we are in the middle of
     * something when the close function was called. So it's our duty to stop first, then close the
     * camera device.
     */
    if (mMode != MODE_NONE) {
        stop();
    }
    // note: AtomISP allows to stop capture without freeing, so
    //       we need to make sure we free them here.
    //       This is not needed for preview and recording buffers.
    freeSnapshotBuffers();
    freePostviewBuffers();

    mMainDevice->close();

    // clear the sp to the devices to destroy the objects.
    mMainDevice.clear();
    mPreviewDevice.clear();
    mPostViewDevice.clear();
    mRecordingDevice.clear();
    m3AEventSubdevice.clear();
    mFileInjectDevice.clear();
    mSensorSupportedFormats.clear();

    if (mZoomRatios) {
        delete[] mZoomRatios;
        mZoomRatios = NULL;
    }
}

void AtomISP::getDefaultParameters(CameraParameters *params, CameraParameters *intel_params)
{
    LOG2("@%s", __FUNCTION__);
    if (!params) {
        ALOGE("params is null!");
        return;
    }
    int cameraId = mCameraId;
    /**
     * PREVIEW
     */
    params->setPreviewSize(mConfig.preview.width, mConfig.preview.height);
    params->setPreviewFrameRate(DEFAULT_PREVIEW_FPS);

    params->set(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES, PlatformData::supportedPreviewSizes(cameraId));

    params->set(CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES,PlatformData::supportedPreviewFrameRate(cameraId));
    params->set(CameraParameters::KEY_PREVIEW_FPS_RANGE,PlatformData::defaultPreviewFPSRange(cameraId));
    params->set(CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE,PlatformData::supportedPreviewFPSRange(cameraId));

    /**
     * RECORDING
     */
    params->setVideoSize(mConfig.recording.width, mConfig.recording.height);
    params->set(CameraParameters::KEY_PREFERRED_PREVIEW_SIZE_FOR_VIDEO, PlatformData::preferredPreviewSizeForVideo(cameraId));
    params->set(CameraParameters::KEY_SUPPORTED_VIDEO_SIZES, PlatformData::supportedVideoSizes(cameraId));
    params->set(CameraParameters::KEY_VIDEO_FRAME_FORMAT,
                CameraParameters::PIXEL_FORMAT_YUV420SP);
    if (PlatformData::supportVideoSnapshot())
        params->set(CameraParameters::KEY_VIDEO_SNAPSHOT_SUPPORTED, CameraParameters::TRUE);
    else
        params->set(CameraParameters::KEY_VIDEO_SNAPSHOT_SUPPORTED, CameraParameters::FALSE);

    /**
     * SNAPSHOT
     */
    params->set(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES, PlatformData::supportedSnapshotSizes(cameraId));
    params->setPictureSize(mConfig.snapshot.width, mConfig.snapshot.height);
    params->set(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH,"320");
    params->set(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT,"240");
    params->set(CameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES, CAM_RESO_STR(LARGEST_THUMBNAIL_WIDTH,LARGEST_THUMBNAIL_HEIGHT)
                ",240x320,320x180,180x320,160x120,120x160,0x0");

    /**
     * ZOOM
     */
    params->set(CameraParameters::KEY_ZOOM, 0);
    params->set(CameraParameters::KEY_ZOOM_SUPPORTED, CameraParameters::TRUE);

    /**
     * FLASH
     */
    params->set(CameraParameters::KEY_FLASH_MODE, PlatformData::defaultFlashMode(cameraId));
    params->set(CameraParameters::KEY_SUPPORTED_FLASH_MODES, PlatformData::supportedFlashModes(cameraId));

    /**
     * FOCUS
     */

    params->set(CameraParameters::KEY_FOCUS_MODE, PlatformData::defaultFocusMode(cameraId));
    params->set(CameraParameters::KEY_SUPPORTED_FOCUS_MODES, PlatformData::supportedFocusModes(cameraId));

    /**
     * FOCAL LENGTH
     */
    // TODO: Probably this should come from PlatformData:CPF
    atomisp_makernote_info makerNote;
    getMakerNote(&makerNote);
    float focal_length = ((float)((makerNote.focal_length>>16) & 0xFFFF)) /
        ((float)(makerNote.focal_length & 0xFFFF));
    char focalLength[100] = {0};
    if (snprintf(focalLength, sizeof(focalLength),"%f", focal_length) < 0) {
        ALOGE("Could not generate %s string: %s",
            CameraParameters::KEY_FOCAL_LENGTH, strerror(errno));
        return;
    }
    params->set(CameraParameters::KEY_FOCAL_LENGTH,focalLength);

    /**
     * FOCUS DISTANCES
     */
    getFocusDistances(params);

    /**
     * DUAL VIDEO
     */
    if(PlatformData::supportDualVideo())
    {
        intel_params->set(IntelCameraParameters::KEY_DUAL_VIDEO_SUPPORTED,"true");
    }

    /**
     * HIGH SPEED
     */
    if(strcmp(PlatformData::supportedHighSpeedResolutionFps(cameraId), ""))
    {
       intel_params->set(IntelCameraParameters::KEY_SUPPORTED_HIGH_SPEED_RESOLUTION_FPS,
                         PlatformData::supportedHighSpeedResolutionFps(cameraId));
    }

    /**
     * RECORDING FRAME RATE
     */
    intel_params->set(IntelCameraParameters::KEY_SUPPORTED_RECORDING_FRAME_RATES,
                      PlatformData::supportedRecordingFramerates(cameraId));

    /**
     * MISCELLANEOUS
     */
    params->setFloat(CameraParameters::KEY_VERTICAL_VIEW_ANGLE,
                     PlatformData::verticalFOV(cameraId, mConfig.snapshot.width, mConfig.snapshot.height));
    params->setFloat(CameraParameters::KEY_HORIZONTAL_VIEW_ANGLE,
                     PlatformData::horizontalFOV(cameraId, mConfig.snapshot.width, mConfig.snapshot.height));

    /**
     * OVERLAY
     */
    if (PlatformData::renderPreviewViaOverlay(cameraId)) {
        intel_params->set(IntelCameraParameters::KEY_HW_OVERLAY_RENDERING_SUPPORTED, "true,false");
    } else {
        intel_params->set(IntelCameraParameters::KEY_HW_OVERLAY_RENDERING_SUPPORTED, "false");
    }
    intel_params->set(IntelCameraParameters::KEY_HW_OVERLAY_RENDERING,"false");

    /**
     * flicker mode
     */
    if(mSensorType == SENSOR_TYPE_RAW) {
        params->set(CameraParameters::KEY_ANTIBANDING, "auto");
        params->set(CameraParameters::KEY_SUPPORTED_ANTIBANDING, "off,50hz,60hz,auto");
    } else {
        params->set(CameraParameters::KEY_ANTIBANDING, "auto");
        params->set(CameraParameters::KEY_SUPPORTED_ANTIBANDING, "off,50hz,60hz,auto");
    }

    /**
     * XNR/ANR
     */
    if (mSensorType == SENSOR_TYPE_RAW) {
        intel_params->set(IntelCameraParameters::KEY_SUPPORTED_XNR, "true,false");
        intel_params->set(IntelCameraParameters::KEY_XNR, CameraParameters::FALSE);
        intel_params->set(IntelCameraParameters::KEY_SUPPORTED_ANR, "true,false");
        intel_params->set(IntelCameraParameters::KEY_ANR, CameraParameters::FALSE);
    }

    /**
     * EXPOSURE
     */
    params->set(CameraParameters::KEY_EXPOSURE_COMPENSATION, PlatformData::supportedDefaultEV(cameraId));
    params->set(CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION,PlatformData::supportedMaxEV(cameraId));
    params->set(CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION,PlatformData::supportedMinEV(cameraId));
    params->set(CameraParameters::KEY_EXPOSURE_COMPENSATION_STEP,PlatformData::supportedStepEV(cameraId));

    // No Capture bracketing
    intel_params->set(IntelCameraParameters::KEY_CAPTURE_BRACKET, "none");
    intel_params->set(IntelCameraParameters::KEY_SUPPORTED_CAPTURE_BRACKET, "none");

    // HDR imaging settings
    intel_params->set(IntelCameraParameters::KEY_HDR_IMAGING, PlatformData::defaultHdr(cameraId));
    intel_params->set(IntelCameraParameters::KEY_SUPPORTED_HDR_IMAGING, PlatformData::supportedHdr(cameraId));
    intel_params->set(IntelCameraParameters::KEY_HDR_SAVE_ORIGINAL, "off");
    intel_params->set(IntelCameraParameters::KEY_SUPPORTED_HDR_SAVE_ORIGINAL, "off");

    /**
     * Ultra-low light (ULL)
     */
    intel_params->set(IntelCameraParameters::KEY_ULL, PlatformData::defaultUltraLowLight(cameraId));
    intel_params->set(IntelCameraParameters::KEY_SUPPORTED_ULL, PlatformData::supportedUltraLowLight(cameraId));

    /**
     * Burst-mode
     */
    intel_params->set(IntelCameraParameters::KEY_SUPPORTED_BURST_FPS, PlatformData::supportedBurstFPS(cameraId));
    intel_params->set(IntelCameraParameters::KEY_BURST_LENGTH,"1");
    intel_params->set(IntelCameraParameters::KEY_SUPPORTED_BURST_LENGTH, PlatformData::supportedBurstLength(cameraId));
    // Bursts with negative start offset require a RAW sensor.
    const char* startIndexValues = "0";
    if (PlatformData::supportsContinuousCapture(cameraId))
        startIndexValues = "-4,-3,-2,-1,0";
    intel_params->set(IntelCameraParameters::KEY_BURST_FPS, "1");
    intel_params->set(IntelCameraParameters::KEY_BURST_SPEED, "fast");
    intel_params->set(IntelCameraParameters::KEY_SUPPORTED_BURST_SPEED, "fast,medium,low");
    intel_params->set(IntelCameraParameters::KEY_BURST_START_INDEX, "0");
    intel_params->set(IntelCameraParameters::KEY_SUPPORTED_BURST_START_INDEX, startIndexValues);

    // TODO: move to platform data
    intel_params->set(IntelCameraParameters::KEY_SUPPORTED_PREVIEW_UPDATE_MODE, PlatformData::supportedPreviewUpdateModes(cameraId));
    intel_params->set(IntelCameraParameters::KEY_PREVIEW_UPDATE_MODE, PlatformData::defaultPreviewUpdateMode(cameraId));

    intel_params->set(IntelCameraParameters::KEY_FILE_INJECT_FILENAME, "off");
    intel_params->set(IntelCameraParameters::KEY_FILE_INJECT_WIDTH, "0");
    intel_params->set(IntelCameraParameters::KEY_FILE_INJECT_HEIGHT, "0");
    intel_params->set(IntelCameraParameters::KEY_FILE_INJECT_BAYER_ORDER, "0");
    intel_params->set(IntelCameraParameters::KEY_FILE_INJECT_FORMAT,"0");

    // raw data format for snapshot
    intel_params->set(IntelCameraParameters::KEY_RAW_DATA_FORMAT, "none");
    intel_params->set(IntelCameraParameters::KEY_SUPPORTED_RAW_DATA_FORMATS, "none,yuv,bayer");

    // effect modes
    params->set(CameraParameters::KEY_EFFECT, PlatformData::defaultEffectMode(cameraId));
    params->set(CameraParameters::KEY_SUPPORTED_EFFECTS, PlatformData::supportedEffectModes(cameraId));
    intel_params->set(CameraParameters::KEY_SUPPORTED_EFFECTS, PlatformData::supportedIntelEffectModes(cameraId));
    //awb
    if (strcmp(PlatformData::supportedAwbModes(cameraId), "")) {
        params->set(CameraParameters::KEY_WHITE_BALANCE, PlatformData::defaultAwbMode(cameraId));
        params->set(CameraParameters::KEY_SUPPORTED_WHITE_BALANCE, PlatformData::supportedAwbModes(cameraId));
    }

    // scene mode
    if (strcmp(PlatformData::supportedSceneModes(cameraId), "")) {
        params->set(CameraParameters::KEY_SUPPORTED_SCENE_MODES, PlatformData::supportedSceneModes(cameraId));
        params->set(CameraParameters::KEY_SCENE_MODE, PlatformData::defaultSceneMode(cameraId));
    }

    // 3a lock: auto-exposure lock
    params->set(CameraParameters::KEY_AUTO_EXPOSURE_LOCK,CameraParameters::FALSE);
    params->set(CameraParameters::KEY_AUTO_EXPOSURE_LOCK_SUPPORTED, PlatformData::supportedAeLock(cameraId));
    // 3a lock: auto-whitebalance lock
    params->set(CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK, CameraParameters::FALSE);
    params->set(CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK_SUPPORTED, PlatformData::supportedAwbLock(cameraId));

    // ae metering mode (Intel extension)
    intel_params->set(IntelCameraParameters::KEY_AE_METERING_MODE, PlatformData::defaultAeMetering(cameraId));
    intel_params->set(IntelCameraParameters::KEY_SUPPORTED_AE_METERING_MODES, PlatformData::supportedAeMetering(cameraId));

    // manual iso control (Intel extension)
    intel_params->set(IntelCameraParameters::KEY_ISO, PlatformData::defaultIso(cameraId));
    intel_params->set(IntelCameraParameters::KEY_SUPPORTED_ISO, PlatformData::supportedIso(cameraId));

    // contrast control (Intel extension)
    intel_params->set(IntelCameraParameters::KEY_CONTRAST_MODE, PlatformData::defaultContrast(cameraId));
    intel_params->set(IntelCameraParameters::KEY_SUPPORTED_CONTRAST_MODES, PlatformData::supportedContrast(cameraId));

    // saturation control (Intel extension)
    intel_params->set(IntelCameraParameters::KEY_SATURATION_MODE, PlatformData::defaultSaturation(cameraId));
    intel_params->set(IntelCameraParameters::KEY_SUPPORTED_SATURATION_MODES, PlatformData::supportedSaturation(cameraId));

    // sharpness control (Intel extension)
    intel_params->set(IntelCameraParameters::KEY_SHARPNESS_MODE, PlatformData::defaultSharpness(cameraId));
    intel_params->set(IntelCameraParameters::KEY_SUPPORTED_SHARPNESS_MODES, PlatformData::supportedSharpness(cameraId));

    // save mirrored
    intel_params->set(IntelCameraParameters::KEY_SAVE_MIRRORED, "false");
    intel_params->set(IntelCameraParameters::KEY_SUPPORTED_SAVE_MIRRORED, "true,false");

    // GPS related (Intel extension)
    intel_params->set(IntelCameraParameters::KEY_GPS_IMG_DIRECTION_REF, "true-direction");
    intel_params->set(IntelCameraParameters::KEY_SUPPORTED_GPS_IMG_DIRECTION_REF, "true-direction,magnetic-direction");

    //Edge Enhancement and Noise Reduction
    intel_params->set(IntelCameraParameters::KEY_NOISE_REDUCTION_AND_EDGE_ENHANCEMENT, "true");
    intel_params->set(IntelCameraParameters::KEY_SUPPORTED_NOISE_REDUCTION_AND_EDGE_ENHANCEMENT, "true,false");
}

Size AtomISP::getHALZSLResolution()
{
    LOG1("@%s", __FUNCTION__);
    CameraParameters p;
    Vector<Size> supportedSizes;

    p.set(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES, PlatformData::supportedSnapshotSizes(mCameraId));
    p.getSupportedPictureSizes(supportedSizes);

    Size largest(0, 0);

    for (unsigned int i = 0; i < supportedSizes.size(); i++) {
        // check if last largest size is bigger than this size
        if (largest.width * largest.height < supportedSizes[i].width * supportedSizes[i].height) {
            // check if aspect ratios are same..
            if (fabs(((float)supportedSizes[i].width / supportedSizes[i].height) /
                     ((float)mConfig.snapshot.width  / mConfig.snapshot.height) - 1) < 0.01f) {
                // check if this size is really supported by ZSL
                if (PlatformData::snapshotResolutionSupportedByZSL(mCameraId,
                        supportedSizes[i].width, supportedSizes[i].height))
                    largest = supportedSizes[i];
            }
        }
    }

    LOG1("@%s selected HAL ZSL resolution %dx%d", __FUNCTION__, largest.width, largest.height);
    return largest;
}

void AtomISP::getMaxSnapShotSize(int cameraId, int* width, int* height)
{
    LOG1("@%s", __FUNCTION__);
    CameraParameters p;
    Vector<Size> supportedSizes;
    int maxWidth = 0, maxHeight = 0;

    p.set(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES, PlatformData::supportedSnapshotSizes(cameraId));
    p.getSupportedPictureSizes(supportedSizes);

    for (unsigned int i = 0; i < supportedSizes.size(); i++) {
        if (maxWidth < supportedSizes[i].width)
            maxWidth = supportedSizes[i].width;
            maxHeight = supportedSizes[i].height;
    }

    *width = maxWidth;
    *height = maxHeight;
}

/**
 * Applies ISP capture mode parameters to hardware
 *
 * Set latest requested values for capture mode parameters, and
 * pass them to kernel. These parameters cannot be set during
 * processing and are set only when starting capture.
 */
status_t AtomISP::updateCaptureParams()
{
    status_t status = NO_ERROR;
    if (mSensorType == SENSOR_TYPE_RAW) {
        if (mMainDevice->setControl(V4L2_CID_ATOMISP_LOW_LIGHT, mLowLight, "Low Light") < 0) {
            ALOGE("set low light failure");
            status = UNKNOWN_ERROR;
        }

        if (mMainDevice->xioctl(ATOMISP_IOC_S_XNR, &mXnr) < 0) {
            ALOGE("set XNR failure");
            status = UNKNOWN_ERROR;
        }

        LOG2("capture params: xnr %d, anr %d", mXnr, mLowLight);
    }

    return status;
}

status_t AtomISP::getDvsStatistics(struct atomisp_dis_statistics *stats,
                                   bool *tryAgain) const
{
    /* This is a blocking call, so we do not lock a mutex here. The method
       is const, so the mutex is not needed anyway. */
    status_t status = NO_ERROR;
    int ret;
    ret = mMainDevice->xioctl(ATOMISP_IOC_G_DIS_STAT, stats);
    if (tryAgain)
        *tryAgain = (errno == EAGAIN);
    if (errno == EAGAIN)
        return NO_ERROR;

    if (ret < 0) {
        ALOGE("failed to get DVS statistics");
        status = UNKNOWN_ERROR;
    }
    return status;
}

status_t AtomISP::setMotionVector(const struct atomisp_dis_vector *vector) const
{
    status_t status = NO_ERROR;
    if (mMainDevice->xioctl(ATOMISP_IOC_S_DIS_VECTOR, (struct atomisp_dis_vector *)vector) < 0) {
        ALOGE("failed to set motion vector");
        status = UNKNOWN_ERROR;
    }
    return status;
}

status_t AtomISP::setDvsCoefficients(const struct atomisp_dis_coefficients *coefs) const
{
    status_t status = NO_ERROR;
    if (mMainDevice->xioctl(ATOMISP_IOC_S_DIS_COEFS, (struct atomisp_dis_coefficients *)coefs) < 0) {
        ALOGE("failed to set dvs coefficients");
        status = UNKNOWN_ERROR;
    }
    return status;
}

status_t AtomISP::getIspParameters(struct atomisp_parm *isp_param) const
{
    status_t status = NO_ERROR;
    if (mMainDevice->xioctl(ATOMISP_IOC_G_ISP_PARM, isp_param) < 0) {
        ALOGE("failed to get ISP parameters");
        status = UNKNOWN_ERROR;
    }
    return status;
}

status_t AtomISP::applySensorFlip(void)
{
    int sensorFlip = PlatformData::sensorFlipping(mCameraId);

    if (sensorFlip == PlatformData::SENSOR_FLIP_NA
        || sensorFlip == PlatformData::SENSOR_FLIP_OFF)
        return NO_ERROR;

    if (mMainDevice->setControl(V4L2_CID_VFLIP,
        (sensorFlip & PlatformData::SENSOR_FLIP_V)?1:0, "vertical image flip"))
        return UNKNOWN_ERROR;

    if (mMainDevice->setControl(V4L2_CID_HFLIP,
        (sensorFlip & PlatformData::SENSOR_FLIP_H)?1:0, "horizontal image flip"))
        return UNKNOWN_ERROR;

    return NO_ERROR;
}

void AtomISP::fetchIspVersions()
{
    LOG1("@%s", __FUNCTION__);

    mCssMajorVersion = -1;
    mCssMinorVersion = -1;
    mIspHwMajorVersion = -1;
    mIspHwMinorVersion = -1;

    // Sensor drivers have been registered to media controller
    const char *mcPathName = "/dev/media0";
    int fd = open(mcPathName, O_RDONLY);
    if (fd == -1) {
        ALOGE("Error in opening media controller: %s!", strerror(errno));
    } else {
        struct media_device_info info;
        memset(&info, 0, sizeof(info));

        if (ioctl(fd, MEDIA_IOC_DEVICE_INFO, &info) < 0) {
            ALOGE("Error in getting media controller info: %s", strerror(errno));
        } else {
            int hw_version = (info.hw_revision & ATOMISP_HW_REVISION_MASK) >> ATOMISP_HW_REVISION_SHIFT;
            int hw_stepping = info.hw_revision & ATOMISP_HW_STEPPING_MASK;
            int css_version = info.driver_version & ATOMISP_CSS_VERSION_MASK;

            switch(hw_version) {
                case ATOMISP_HW_REVISION_ISP2300:
                    mIspHwMajorVersion = 23;
                    break;
                case ATOMISP_HW_REVISION_ISP2400:
                case ATOMISP_HW_REVISION_ISP2401_LEGACY:
                    mIspHwMajorVersion = 24;
                    break;
                case ATOMISP_HW_REVISION_ISP2401:
                    mIspHwMajorVersion = 2401;
                    break;
                default:
                    ALOGE("Unknown ISP HW version: %d", hw_version);
            }
            if (mIspHwMajorVersion > 0)
                LOG1("ISP HW version is: %d", mIspHwMajorVersion);

            switch(hw_stepping) {
                case ATOMISP_HW_STEPPING_A0:
                    mIspHwMinorVersion = 0;
                    break;
                case ATOMISP_HW_STEPPING_B0:
                    mIspHwMinorVersion = 1;
                    break;
                default:
                    ALOGE("Unknown ISP HW stepping: %d", hw_stepping);
            }
            if (mIspHwMinorVersion > 0)
                LOG1("ISP HW stepping is: %d", mIspHwMinorVersion);

            switch(css_version) {
                case ATOMISP_CSS_VERSION_15:
                    mCssMajorVersion = 1;
                    mCssMinorVersion = 5;
                    break;
                case ATOMISP_CSS_VERSION_20:
                    mCssMajorVersion = 2;
                    mCssMinorVersion = 0;
                    break;
                case ATOMISP_CSS_VERSION_21:
                    mCssMajorVersion = 2;
                    mCssMinorVersion = 1;
                    break;
                default:
                    ALOGE("Unknown CSS version: %d", css_version);
            }
            if (mCssMajorVersion > 0)
                LOG1("CSS version is: %d.%d", mCssMajorVersion, mCssMinorVersion);
        }
        close(fd);
    }
}

status_t AtomISP::configure(AtomMode mode)
{
    LOG1("@%s", __FUNCTION__);
    LOG1("mode = %d", mode);
    status_t status = NO_ERROR;
    mHALZSLEnabled = false; // configureContinuous turns this on, when needed
    if (mFileInject.active == true)
        startFileInject();
    switch (mode) {
    case MODE_PREVIEW:
        status = configurePreview();
        break;
    case MODE_VIDEO:
        status = configureRecording();
        break;
    case MODE_CAPTURE:
        status = configureCapture();
        break;
    case MODE_CONTINUOUS_CAPTURE:
        status = configureContinuous();
        break;
    default:
        status = UNKNOWN_ERROR;
        break;
    }

    if (status == NO_ERROR)
    {
        mMode = mode;
        dumpFrameInfo(mode);
        // Pipeline configured, triggering SensorHW::prepare()
        status = mSensorHW.prepare(mode == MODE_CAPTURE);
    }

    /**
     * Some sensors produce corrupted first frames
     * value is provided by the sensor driver as frames to skip.
     * value is specific to configuration, so we query it here after
     * devices are configured and propagate the value to distinct devices
     * at start.
     */
    mInitialSkips = getNumOfSkipFrames();
    mStatisticSkips = getNumOfSkipStatistics();

    return status;
}

status_t AtomISP::allocateBuffers(AtomMode mode)
{
    LOG1("@%s", __FUNCTION__);
    LOG1("mode = %d", mode);
    status_t status = NO_ERROR;

    switch (mode) {
    case MODE_PREVIEW:
    case MODE_CONTINUOUS_CAPTURE:
        /* Swap the preview device for the video recording device in case the preview is
         * too big to run VFPP or if we are using the HAL based ZSL implementation
         * This is just a trick to use a faster path inside the ISP
         *
         * We will restore this when we stop the preview. see stopPreview()
         **/
        if (mPreviewTooBigForVFPP || mHALZSLEnabled) {
            mPreviewDevice = mMainDevice;
        }
        if ((status = allocatePreviewBuffers()) != NO_ERROR)
            mPreviewDevice->stop();
        break;
    case MODE_VIDEO:
        if ((status = allocateRecordingBuffers()) != NO_ERROR)
            return status;
        if ((status = allocatePreviewBuffers()) != NO_ERROR)
            stopRecording();
        if (mStoreMetaDataInBuffers) {
          if ((status = allocateMetaDataBuffers()) != NO_ERROR)
              stopRecording();
        }
        break;
    case MODE_CAPTURE:
        if ((status = allocateSnapshotBuffers()) != NO_ERROR)
            return status;
        break;
    default:
        status = UNKNOWN_ERROR;
        break;
    }

    PERFORMANCE_TRACES_BREAKDOWN_STEP_PARAM("Mode:", mode);
    return status;
}

status_t AtomISP::start()
{
    LOG1("@%s", __FUNCTION__);
    LOG1("mode = %d", mMode);
    status_t status = NO_ERROR;

    if (mSensorType == SENSOR_TYPE_RAW) {
        // TODO: Workaround to be removed.
        // This is temporary workaround to support old FrameSyncSource
        // implementation moved from AtomISP into SensorHW class. To
        // ensure the observing thread is created regardless whether
        // there are clients attaching, we attach SensorHW itself.
        attachObserver((IAtomIspObserver *) &mSensorHW, OBSERVE_FRAME_SYNC_SOF);
    }
    mSensorHW.start();

    switch (mMode) {
    case MODE_CONTINUOUS_CAPTURE:
    case MODE_PREVIEW:
        status = startPreview();
        break;
    case MODE_VIDEO:
        status = startRecording();
        break;
    case MODE_CAPTURE:
        status = startCapture();
        break;
    default:
        status = UNKNOWN_ERROR;
        break;
    };

    if (status == NO_ERROR) {
        runStartISPActions();
        mSessionId++;
    } else {
        mMode = MODE_NONE;
    }

    return status;
}

/**
 * Perform actions after ISP kernel device has
 * been started.
 */
void AtomISP::runStartISPActions()
{
    LOG1("@%s", __FUNCTION__);
    if (mFlashTorchSetting > 0) {
        setTorchHelper(mFlashTorchSetting);
    }

    // Start All ObserverThread's
    mObserverManager.setState(OBSERVER_STATE_RUNNING, NULL);
}

/**
 * Perform actions before ISP kernel device is closed.
 */
void AtomISP::runStopISPActions()
{
    LOG1("@%s", __FUNCTION__);
    if (mFlashTorchSetting > 0) {
        setTorchHelper(0);
    }
}

status_t AtomISP::stop()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    if (mSensorType == SENSOR_TYPE_RAW) {
        // TODO: Workaround to be removed, See AtomISP::start()
        detachObserver((IAtomIspObserver *) &mSensorHW, OBSERVE_FRAME_SYNC_SOF);
    }

    runStopISPActions();

    switch (mMode) {
    case MODE_PREVIEW:
        status = stopPreview();
        break;

    case MODE_VIDEO:
        status = stopRecording();
        break;

    case MODE_CAPTURE:
        status = stopCapture();
        break;

    case MODE_CONTINUOUS_CAPTURE:
        status = stopContinuousPreview();
        break;

    default:
        break;
    };

    mSensorHW.stop();

    if (mFileInject.active == true)
        stopFileInject();

    if (status == NO_ERROR)
        mMode = MODE_NONE;

    return status;
}

status_t AtomISP::configurePreview()
{
    LOG1("@%s", __FUNCTION__);
    int ret = 0;
    status_t status = NO_ERROR;
    sp<V4L2VideoNode> activePreviewNode;

    activePreviewNode = mPreviewTooBigForVFPP ? mMainDevice : mPreviewDevice;

    ret = activePreviewNode->open();
    if (ret < 0) {
        ALOGE("Open preview device failed!");
        status = UNKNOWN_ERROR;
        return status;
    }

    struct v4l2_capability aCap;
    status = activePreviewNode->queryCap(&aCap);
    if (status != NO_ERROR) {
        ALOGE("Failed basic capability check failed!");
        return NO_INIT;
    }

    ret = configureDevice(
            activePreviewNode.get(),
            CI_MODE_PREVIEW,
            &(mConfig.preview),
            false);
    if (ret < 0) {
        status = UNKNOWN_ERROR;
        goto err;
    }

    atomisp_set_zoom(mConfig.zoom);

    return status;

err:
    activePreviewNode->stop();
    return status;
}

status_t AtomISP::startPreview()
{
    LOG1("@%s", __FUNCTION__);
    int ret = 0;
    status_t status = NO_ERROR;
    int bufcount = mHALZSLEnabled ? sNumHALZSLBuffers : mConfig.num_preview_buffers;

    /**
     * moving the hack from Gang to disable these ISP parameters, only needed
     * when starting preview in continuous capture mode and when starting capture
     * in online mode. This was required to enable SuperZoom POC.
     * */
    if (mMode == MODE_CONTINUOUS_CAPTURE &&  mNoiseReductionEdgeEnhancement == false) {
        //Disable the Noise Reduction and Edge Enhancement
        struct atomisp_ee_config ee_cfg;
        struct atomisp_nr_config nr_cfg;
        struct atomisp_de_config de_cfg;

        memset(&ee_cfg, 0, sizeof(struct atomisp_ee_config));
        memset(&nr_cfg, 0, sizeof(struct atomisp_nr_config));
        memset(&de_cfg, 0, sizeof(struct atomisp_de_config));
        ee_cfg.threshold = 65535;
        setNrConfig(&nr_cfg);
        setEeConfig(&ee_cfg);
        setDeConfig(&de_cfg);
        LOG1("Disabled NREE in %s", __func__);
    }

    ret = mPreviewDevice->start(bufcount, mInitialSkips);
    if (ret < 0) {
        ALOGE("Start preview device failed!");
        status = UNKNOWN_ERROR;
        goto err;
    }

    mNumPreviewBuffersQueued = bufcount;

    return status;

err:
    stopPreview();
    return status;
}

status_t AtomISP::stopPreview()
{
    LOG1("@%s", __FUNCTION__);

    status_t status = NO_ERROR;

    if (mPreviewDevice->isStarted())
        mPreviewDevice->stop();

    freePreviewBuffers();

    // In case some cases like HAL ZSL is enabled or preview is too big for VFPP
    // we have swapped preview and main devices, in this situation we do not close it
    // otherwise we will power down the ISP

    if (mPreviewDevice->mId != V4L2_MAIN_DEVICE) {
        mPreviewDevice->close();
    }

    // Restore now the preview device to the natural one in case we had swap it
    if (mOriginalPreviewDevice->mId != mPreviewDevice->mId) {
        mPreviewDevice = mOriginalPreviewDevice;
    }

    PERFORMANCE_TRACES_BREAKDOWN_STEP("Done");
    return status;
}

status_t AtomISP::configureRecording()
{
    LOG1("@%s", __FUNCTION__);
    int ret = 0;
    status_t status = NO_ERROR;
    AtomBuffer *previewConfig(NULL);
    AtomBuffer *recordingConfig(NULL);

    ret = mPreviewDevice->open();
    if (ret < 0) {
        ALOGE("Open preview device failed!");
        status = UNKNOWN_ERROR;
        goto err;
    }

    struct v4l2_capability aCap;
    status = mPreviewDevice->queryCap(&aCap);
    if (status != NO_ERROR) {
        ALOGE("Failed basic capability check failed!");
        return NO_INIT;
    }

    // See function description of applyISPLimitations(), Workaround 2
    if (mSwapRecordingDevice) {
        previewConfig = &(mConfig.recording);
        recordingConfig = &(mConfig.preview);
    } else {
        previewConfig  = &(mConfig.preview);
        recordingConfig = &(mConfig.recording);
    }

    //open recording device
    ret = mRecordingDevice->open();
    if (ret < 0) {
        ALOGE("Open recording device failed!");
        status = UNKNOWN_ERROR;
        goto err;
    }

    status = mRecordingDevice->queryCap(&aCap);
    if (status != NO_ERROR) {
        ALOGE("Failed basic capability check failed!");
        return NO_INIT;
    }

    ret = configureDevice(
            mRecordingDevice.get(),
            CI_MODE_VIDEO,
            recordingConfig,
            false);
    if (ret < 0) {
        ALOGE("Configure recording device failed!");
        status = UNKNOWN_ERROR;
        goto err;
    }

    // fake preview config size if VFPP is too slow, so that VFPP will not
    // cause FPS to drop
    if (mPreviewTooBigForVFPP) {
        previewConfig->width = 176;
        previewConfig->height = 144;
    }
    ret = configureDevice(
            mPreviewDevice.get(),
            CI_MODE_VIDEO,
            previewConfig,
            false);
    // restore original preview config size if VFPP was too slow
    if (mPreviewTooBigForVFPP) {
        // since we only support recording == preview resolution, we can simply do:
        *previewConfig = *recordingConfig;
        // now the configuration and the bpl in it will be correct,
        // when HAL uses it. Buffer allocation will be for the big size, bpl
        // etc. will be also for the big size, only ISP will use the small size
        // for VFPP. Notice the device swap is on in this case, so the fake size
        // was actually stored in the mConfig.recording and we just copied the
        // preview config back to recording config, even though the pointer
        // names suggest the other way around
    }

    if (ret < 0) {
        ALOGE("Configure recording device failed!");
        status = UNKNOWN_ERROR;
        goto err;
    }

    // The recording device must be configured first, so swap the devices after configuration
    if (mSwapRecordingDevice) {
        LOG1("@%s: swapping preview and recording devices", __FUNCTION__);
        sp<V4L2VideoNode> tmp = mPreviewDevice;
        mPreviewDevice = mRecordingDevice;
        mRecordingDevice = tmp;
        mRecordingDeviceSwapped = true;
    }

    return status;

err:
    stopRecording();
    return status;
}

status_t AtomISP::startRecording()
{
    LOG1("@%s", __FUNCTION__);
    int ret = 0;
    status_t status = NO_ERROR;

    ret = mRecordingDevice->start(mConfig.num_recording_buffers, mInitialSkips);
    if (ret < 0) {
        ALOGE("Start recording device failed");
        status = UNKNOWN_ERROR;
        goto err;
    }

    ret = mPreviewDevice->start(mConfig.num_preview_buffers, mInitialSkips);
    if (ret < 0) {
        ALOGE("Start preview device failed!");
        status = UNKNOWN_ERROR;
        goto err;
    }

    mNumPreviewBuffersQueued = mConfig.num_preview_buffers;
    mNumRecordingBuffersQueued = mConfig.num_recording_buffers;

    return status;

err:
    stopRecording();
    return status;
}

status_t AtomISP::stopRecording()
{
    LOG1("@%s", __FUNCTION__);

    if (mRecordingDeviceSwapped) {
        LOG1("@%s: swapping preview and recording devices back", __FUNCTION__);
        sp<V4L2VideoNode> tmp = mPreviewDevice;
        mPreviewDevice = mRecordingDevice;
        mRecordingDevice = tmp;
        mRecordingDeviceSwapped = false;
    }

    mRecordingDevice->stop();
    freeRecordingBuffers();

    mPreviewDevice->stop();
    freePreviewBuffers();
    mPreviewDevice->close();
    mRecordingDevice->close();

    return NO_ERROR;
}

status_t AtomISP::configureCapture()
{
    LOG1("@%s", __FUNCTION__);
    int ret;
    status_t status = NO_ERROR;

    updateCaptureParams();

    ret = configureDevice(
            mMainDevice.get(),
            CI_MODE_STILL_CAPTURE,
            &(mConfig.snapshot),
            isDumpRawImageReady());
    if (ret < 0) {
        ALOGE("configure first device failed!");
        status = UNKNOWN_ERROR;
        goto errorFreeBuf;
    }
    // Raw capture currently does not support postview
    if (isDumpRawImageReady())
        goto nopostview;

    ret = mPostViewDevice->open();
    if (ret < 0) {
        ALOGE("Open second device failed!");
        status = UNKNOWN_ERROR;
        goto errorFreeBuf;
    }

    struct v4l2_capability aCap;
    status = mPostViewDevice->queryCap(&aCap);
    if (status != NO_ERROR) {
        ALOGE("Failed basic capability check failed!");
        return NO_INIT;
    }

    ret = configureDevice(
            mPostViewDevice.get(),
            CI_MODE_STILL_CAPTURE,
            &(mConfig.postview),
            false);
    if (ret < 0) {
        ALOGE("configure second device failed!");
        status = UNKNOWN_ERROR;
        goto errorCloseSecond;
    }

nopostview:
    atomisp_set_zoom(mConfig.zoom);

    return status;

errorCloseSecond:
    mPostViewDevice->close();
errorFreeBuf:
    freeSnapshotBuffers();
    freePostviewBuffers();

    return status;
}

/**
 * Requests driver to capture main and postview images
 *
 * This call is used to trigger captures from the continuously
 * updated RAW ringbuffer maintained in ISP.
 *
 * Call is implemented using the the IOC_S_CONT_CAPTURE_CONFIG
 * atomisp ioctl.
 */
status_t AtomISP::requestContCapture(int numCaptures, int offset, unsigned int skip)
{
    LOG2("@%s", __FUNCTION__);

    struct atomisp_cont_capture_conf conf;

    CLEAR(conf);
    conf.num_captures = numCaptures;
    conf.offset = offset;
    conf.skip_frames = skip;

    int res = mMainDevice->xioctl(ATOMISP_IOC_S_CONT_CAPTURE_CONFIG, &conf);
    LOG1("@%s: CONT_CAPTURE_CONFIG num %d, offset %d, skip %u, res %d",
         __FUNCTION__, numCaptures, offset, skip, res);
    if (res != 0) {
        ALOGE("@%s: error with CONT_CAPTURE_CONFIG, res %d", __FUNCTION__, res);
        return UNKNOWN_ERROR;
    }

    return NO_ERROR;
}

/**
 * Configures the ISP continuous capture mode
 *
 * This takes effect in kernel when preview is started.
 */
status_t AtomISP::configureContinuousMode(bool enable)
{
    LOG2("@%s", __FUNCTION__);
    if (mMainDevice->setControl(V4L2_CID_ATOMISP_CONTINUOUS_MODE,
                              enable, "Continuous mode") < 0)
            return UNKNOWN_ERROR;

    enable = (!mContCaptPriority &&
              PlatformData::snapshotResolutionSupportedByCVF(mCameraId,
                                                             mConfig.snapshot.width,
                                                             mConfig.snapshot.height));
    if (mMainDevice->setControl(V4L2_CID_ATOMISP_CONTINUOUS_VIEWFINDER,
                              enable, "Continuous viewfinder") < 0)
            return UNKNOWN_ERROR;

    return NO_ERROR;
}


/**
 * Configures the ISP ringbuffer size in continuous mode.
 *
 * Set all ISP parameters that affect RAW ringbuffer
 * sizing in continuous mode.
 *
 * \see setContCaptureOffset(), setContCaptureSkip() and
 *      setContCaptureNumCaptures()
 */
status_t AtomISP::configureContinuousRingBuffer()
{
    int numBuffers = ATOMISP_MIN_CONTINUOUS_BUF_SIZE;
    int captures = mContCaptConfig.numCaptures;
    int offset = mContCaptConfig.offset;
    int lookback = abs(offset);

    if (lookback > captures)
        numBuffers += lookback;
    else
        numBuffers += captures;

    if (mCssMajorVersion >= 2) {
    // for css2.x, the minimum raw ring buffers number is ATOMISP_MIN_CONTINUOUS_BUF_NUM_CSS2X
        //when offset is -1 , one ring buffer is able to be optimized
        if (offset == -1)
            numBuffers -= 1;

        if (numBuffers < ATOMISP_MIN_CONTINUOUS_BUF_NUM_CSS2X)
            numBuffers = ATOMISP_MIN_CONTINUOUS_BUF_NUM_CSS2X;
    }

    if (numBuffers > PlatformData::maxContinuousRawRingBufferSize(mCameraId))
        numBuffers = PlatformData::maxContinuousRawRingBufferSize(mCameraId);

    LOG1("continuous mode ringbuffer size to %d (captures %d, offset %d)",
         numBuffers, captures, offset);

    if (mMainDevice->setControl(V4L2_CID_ATOMISP_CONTINUOUS_RAW_BUFFER_SIZE,
                                   numBuffers,
                                   "Continuous raw ringbuffer size") < 0)
        return UNKNOWN_ERROR;

    return NO_ERROR;
}

/**
 *  Returns the Number of continuous captures configured during start preview.
 *  This is in effect the size of the ring buffer allocated in the ISP used
 *  during continuous capture mode.
 *  This information is useful for example to know how many snapshot buffers
 *  to allocate.
 */
int AtomISP::getContinuousCaptureNumber() const
{

    return mContCaptConfig.numCaptures;
}

/**
 * Calculates the correct frame offset to capture to reach Zero
 * Shutter Lag.
 */
int AtomISP::shutterLagZeroAlign() const
{
    int delayMs = PlatformData::shutterLagCompensationMs();
    float frameIntervalMs = 1000.0 / DEFAULT_PREVIEW_FPS;
    int lagZeroOffset = round(float(delayMs) / frameIntervalMs);
    LOG2("@%s: delay %dms, zero offset %d",
         __FUNCTION__, delayMs, lagZeroOffset);
    return lagZeroOffset;
}

/**
 * Returns the minimum offset ISP supports.
 *
 * This values is the smallest value that can be passed
 * to prepareOfflineCapture() and startOfflineCapture().
 */
int AtomISP::continuousBurstNegMinOffset(void) const
{
    return -(PlatformData::maxContinuousRawRingBufferSize(mCameraId) - 2);
}

/**
 * Returns the needed buffer offset to capture frame
 * with negative time index 'startIndex' and when skippng
 * 'skip' input frames between each output frame.
 *
 * The resulting offset is aligned so that offset for
 * startIndex==0 matches the user perceived zero shutter lag
 * frame. This calibration is done by factoring in
 * PlatformData::shutterLagCompensationMs().
 *
 * As the ISP continuous capture buffer consists of frames stored
 * at full sensor rate, it depends on the requested output capture
 * rate, how much back in time one can go.
 *
 * \param skip how many input frames ISP skips after each output frame
 * \param startIndex index to first frame (counted at output rate)
 * \return negative offset to the requested frame (counted at full sensor rate)
 */
int AtomISP::continuousBurstNegOffset(int skip, int startIndex) const
{
    assert(startIndex <= 0);
    assert(skip >= 0);
    int targetRatio = skip + 1;
    int negOffset = targetRatio * startIndex - shutterLagZeroAlign();
    LOG2("@%s: offset %d, ratio %d, skip %d, align %d",
         __FUNCTION__, negOffset, targetRatio, skip, shutterLagZeroAlign());
    return negOffset;
}

/**
 * Configures the ISP to work on a mode where Continuous capture is implemented
 * in the HAL. This is currently only available for SoC sensors, hence the name.
 */
status_t AtomISP::configureContinuousSOC()
{
    LOG1("@%s", __FUNCTION__);

    // fix for driver zoom bug, reset to zero before entering HAL ZSL mode
    setZoom(0);

    mHALZSLEnabled = true;
    int ret = 0;
    status_t status = OK;

    mPreviewDevice = mMainDevice;

    if (!mPreviewDevice->isOpen()) {
        ret = mPreviewDevice->open();
        if (ret < 0) {
            ALOGE("Open preview device failed!");
            status = UNKNOWN_ERROR;
            return status;
        }

        struct v4l2_capability aCap;
        status = mPreviewDevice->queryCap(&aCap);
        if (status != NO_ERROR) {
            ALOGE("Failed basic capability check failed!");
            return NO_INIT;
        }
    }

    Size zslSize = getHALZSLResolution();
    mConfig.HALZSL = mConfig.snapshot;
    mConfig.HALZSL.width = zslSize.width;
    mConfig.HALZSL.height = zslSize.height;
    mConfig.HALZSL.fourcc = PlatformData::getPreviewPixelFormat();
    mConfig.HALZSL.bpl = SGXandDisplayBpl(mConfig.HALZSL.fourcc, mConfig.HALZSL.width);
    mConfig.HALZSL.size = frameSize(mConfig.HALZSL.fourcc, mConfig.HALZSL.width, mConfig.HALZSL.height);


    // fix the Preview bpl to have gfx bpl, as preview never goes to ISP
    mConfig.preview.bpl = SGXandDisplayBpl(mConfig.HALZSL.fourcc, mConfig.preview.width);

    ret = configureDevice(
            mPreviewDevice.get(),
            CI_MODE_PREVIEW,
            &(mConfig.HALZSL),
            false);
    if (ret < 0) {
        status = UNKNOWN_ERROR;
        ALOGE("@%s: configureDevice failed!", __FUNCTION__);
        goto err;
    }

    LOG1("@%s configured %dx%d bpl %d", __FUNCTION__, mConfig.HALZSL.width, mConfig.HALZSL.height, mConfig.HALZSL.bpl);

    return status;
err:
    mPreviewDevice->close();
    return status;
}

status_t AtomISP::configureContinuous()
{
    LOG1("@%s", __FUNCTION__);
    int ret;
    float capture_fps;
    status_t status = NO_ERROR;

    if (!mContCaptPrepared) {
        ALOGE("offline capture not prepared correctly");
        return UNKNOWN_ERROR;
    }

    updateCaptureParams();

    if(mSensorType == SENSOR_TYPE_SOC) {
        return configureContinuousSOC();
    }

    ret = configureContinuousMode(true);
    if (ret != NO_ERROR) {
        ALOGE("setting continuous mode failed");
        return ret;
    }

    ret = configureContinuousRingBuffer();
    if (ret != NO_ERROR) {
        ALOGE("setting continuous capture params failed");
        return ret;
    }

    ret = configureDevice(
            mMainDevice.get(),
            CI_MODE_PREVIEW,
            &(mConfig.snapshot),
            isDumpRawImageReady());
    if (ret < 0) {
        ALOGE("configure first device failed!");
        status = UNKNOWN_ERROR;
        goto errorFreeBuf;
    }
    // save the capture fps
    capture_fps = mConfig.fps;

    status = configurePreview();
    if (status != NO_ERROR) {
        return status;
    }

    ret = mPostViewDevice->open();
    if (ret < 0) {
        ALOGE("Open second device failed!");
        status = UNKNOWN_ERROR;
        goto errorFreeBuf;
    }

    struct v4l2_capability aCap;
    status = mPostViewDevice->queryCap(&aCap);
    if (status != NO_ERROR) {
        ALOGE("Failed basic capability check failed!");
        return NO_INIT;
    }

    ret = configureDevice(
            mPostViewDevice.get(),
            CI_MODE_PREVIEW,
            &(mConfig.postview),
            false);
    if (ret < 0) {
        ALOGE("configure second device failed!");
        status = UNKNOWN_ERROR;
        goto errorCloseSecond;
    }

    atomisp_set_zoom(mConfig.zoom);

    // restore the actual capture fps value
    mConfig.fps = capture_fps;

    return status;

errorCloseSecond:
    mPostViewDevice->close();
errorFreeBuf:
    freeSnapshotBuffers();
    freePostviewBuffers();

    return status;
}

status_t AtomISP::startCapture()
{
    int ret;
    status_t status = NO_ERROR;
    int i, initialSkips;

    // Limited by driver, raw bayer image dump can support only 1 frame when setting
    // snapshot number. Otherwise, the raw dump image would be corrupted.
    // also since CSS1.5 we cannot capture from postview at the same time
    int snapNum;
    if (mConfig.snapshot.fourcc == mSensorHW.getRawFormat())
        snapNum = 1;
    else
        snapNum = mConfig.num_snapshot;

    /**
     * handle initial skips here for normal capture mode
     * For continuous capture we cannot skip this way, we will control via the
     * offset calculation
     */
    if (mMode != MODE_CONTINUOUS_CAPTURE)
       initialSkips = mInitialSkips;
    else
       initialSkips = 0;

   /**
    * moving the hack from Gang to disable these ISP parameters, only needed
    * when starting preview in continuous capture mode and when starting capture
    * in online mode. This was required to enable SuperZoom POC.
    **/
    if (mMode != MODE_CONTINUOUS_CAPTURE &&  mNoiseReductionEdgeEnhancement == false) {
       //Disable the Noise Reduction and Edge Enhancement
       struct atomisp_ee_config ee_cfg;
       struct atomisp_nr_config nr_cfg;
       struct atomisp_de_config de_cfg;

       memset(&ee_cfg, 0, sizeof(struct atomisp_ee_config));
       memset(&nr_cfg, 0, sizeof(struct atomisp_nr_config));
       memset(&de_cfg, 0, sizeof(struct atomisp_de_config));
       ee_cfg.threshold = 65535;
       setNrConfig(&nr_cfg);
       setEeConfig(&ee_cfg);
       setDeConfig(&de_cfg);
       LOG1("Disabled NREE in %s", __func__);
    }

    ret = mMainDevice->start(snapNum, initialSkips);
    if (ret < 0) {
        ALOGE("start capture on first device failed!");
        status = UNKNOWN_ERROR;
        goto end;
    }

    if (isDumpRawImageReady()) {
        initialSkips = 0;
        goto nopostview;
    }


    ret = mPostViewDevice->start(snapNum, initialSkips);
    if (ret < 0) {
        ALOGE("start capture on second device failed!");
        status = UNKNOWN_ERROR;
        goto errorStopFirst;
    }

    for (i = 0; i < initialSkips; i++) {
        AtomBuffer s,p;
        ret = getSnapshot(&s,&p);
        if (ret == NO_ERROR)
            ret = putSnapshot(&s,&p);
    }

nopostview:
    mNumCapturegBuffersQueued = snapNum;
    return status;

errorStopFirst:
    mMainDevice->stop();
errorCloseSecond:
    mPostViewDevice->close();
errorFreeBuf:
    freeSnapshotBuffers();
    freePostviewBuffers();

end:
    return status;
}

status_t AtomISP::stopContinuousPreview()
{
    LOG1("@%s", __FUNCTION__);
    int error = 0;
    if (stopCapture() != NO_ERROR)
        ++error;
    // TODO: this call to requestContCapture() can be
    //       removed once PSI BZ 101676 is solved
    if (requestContCapture(0, 0, 0) != NO_ERROR)
        ++error;
    if (configureContinuousMode(false) != NO_ERROR)
        ++error;
    if (stopPreview() != NO_ERROR)
        ++error;
    if (error) {
        ALOGE("@%s: errors (%d) in stopping continuous capture",
             __FUNCTION__, error);
        return UNKNOWN_ERROR;
    }
    return NO_ERROR;
}

/**
 * Checks whether local preview buffer pool contains shared buffers
 *
 * @param reserved optional argument to check if any of the shared
 *                 buffers are currently queued
 */
bool AtomISP::isSharedPreviewBufferConfigured(bool *reserved) const
{
    bool configured = false;

    if (reserved)
        *reserved = false;

    if (!mPreviewBuffers.isEmpty())
        for (size_t i = 0 ; i < mPreviewBuffers.size(); i++)
            if (mPreviewBuffers[i].shared) {
                configured = true;
                if (reserved && mPreviewBuffers[i].id == -1)
                    *reserved = true;
            }

    return configured;
}

status_t AtomISP::stopCapture()
{
    LOG1("@%s", __FUNCTION__);

    if (mPostViewDevice->isStarted())
        mPostViewDevice->stop();

    if (mMainDevice->isStarted())
        mMainDevice->stop();

    // note: MAIN device is kept open on purpose
    if (mPostViewDevice->isOpen())
        mPostViewDevice->close();

    if (mUsingClientSnapshotBuffers) {
        mConfig.num_snapshot = 0;
        mUsingClientSnapshotBuffers = false;
    }

    if (mUsingClientPostviewBuffers) {
        mConfig.num_postviews = 0;
        mUsingClientPostviewBuffers = false;
    }

    dumpRawImageFlush();
    PERFORMANCE_TRACES_BREAKDOWN_STEP("Done");
    return NO_ERROR;
}

/**
 * Starts offline capture processing in the ISP.
 *
 * Snapshot and postview frame rendering is started
 * and frame(s) can be fetched with getSnapshot().
 *
 * Note that the capture params given in 'config' must be
 * equal or a subset of the configuration passed to
 * prepareOfflineCapture().
 *
 * \param config configuration container describing how many
 *               captures to take, skipping and the start offset
 */
status_t AtomISP::startOfflineCapture(ContinuousCaptureConfig &config)
{
    status_t res = NO_ERROR;

    LOG1("@%s", __FUNCTION__);
    if (mHALZSLEnabled)
        return OK;

    if (mMode != MODE_CONTINUOUS_CAPTURE) {
        ALOGE("@%s: invalid mode %d", __FUNCTION__, mMode);
        return INVALID_OPERATION;
    }
    else if (config.offset < 0 &&
             config.offset < mContCaptConfig.offset) {
        ALOGE("@%s: cannot start, offset %d, %d set at preview start",
             __FUNCTION__, config.offset, mContCaptConfig.offset);
        return UNKNOWN_ERROR;
    }
    else if (config.numCaptures > mContCaptConfig.numCaptures) {
        ALOGE("@%s: cannot start, num captures %d, %d set at preview start",
             __FUNCTION__, config.numCaptures, mContCaptConfig.numCaptures);
        return UNKNOWN_ERROR;
    }

    /**
     * If we are trying to take a picture when preview just started we need
     * to add the number of preview skipped  frames  to the offset
     * These frames are the ones skipped because sensor starts with
     * corrupted images. In normal situation the initial skips will have gone
     * down to 0.
     */
    config.offset += mPreviewDevice->getInitialFrameSkips();

    res = requestContCapture(config.numCaptures,
                             config.offset,
                             config.skip);
    if (res == NO_ERROR)
        res = startCapture();

    return res;
}

/**
 * Stops offline capture processing in the ISP:
 *
 * Performs a STREAM-OFF for snapshot and postview devices,
 * but does not free any buffers yet.
 */
status_t AtomISP::stopOfflineCapture()
{
    LOG1("@%s", __FUNCTION__);
    if (mMode != MODE_CONTINUOUS_CAPTURE) {
        ALOGE("@%s: invalid mode %d", __FUNCTION__, mMode);
        return INVALID_OPERATION;
    }

    if (mHALZSLEnabled)
        return OK;

    mMainDevice->stop(false);
    mPostViewDevice->stop(false);
    mContCaptPrepared = true;
    return NO_ERROR;
}

/**
 * Prepares ISP for offline capture
 *
 * \param config container to configure capture count, skipping
 *               and the start offset (see struct ContinuousCaptureConfig)
 */
status_t AtomISP::prepareOfflineCapture(ContinuousCaptureConfig &cfg, bool capturePriority)
{
    LOG1("@%s, numCaptures = %d", __FUNCTION__, cfg.numCaptures);
    if (cfg.offset < continuousBurstNegMinOffset()) {
        ALOGE("@%s: offset %d not supported, minimum %d",
             __FUNCTION__, cfg.offset, continuousBurstNegMinOffset());
        return UNKNOWN_ERROR;
    }
    mContCaptConfig = cfg;
    mContCaptPrepared = true;
    mContCaptPriority = capturePriority;
    return NO_ERROR;
}

bool AtomISP::isOfflineCaptureRunning() const
{
    if (mMode == MODE_CONTINUOUS_CAPTURE &&  mMainDevice->isStarted())
        return true;

    return false;
}

/**
 * Configures a particular device with a mode (preview, video or capture)
 *
 * The formatDescriptor contains information about the frame dimensions that
 * we are requesting to ISP
 * the fields bpl and size of the descriptor will be updated with the actual
 * bpl that the buffers need to have to meet the ISP constrains.
 * In effect the formatDescriptor struct is an IN/OUT parameter.
 */
int AtomISP::configureDevice(V4L2VideoNode *device, int deviceMode, AtomBuffer *formatDescriptor, bool raw)
{
    LOG1("@%s", __FUNCTION__);

    int ret = 0;
    int w,h,fourcc;
    w = formatDescriptor->width;
    h = formatDescriptor->height;
    fourcc = formatDescriptor->fourcc;
    LOG1("device: %d, width:%d, height:%d, deviceMode:%d fourcc:%s raw:%d", device->mId,
        w, h, deviceMode, v4l2Fmt2Str(fourcc), raw);

    if ((w <= 0) || (h <= 0)) {
        ALOGE("Wrong Width %d or Height %d", w, h);
        return -1;
    }

    //Switch the Mode before set the fourcc. This is the requirement of
    //atomisp
    ret = atomisp_set_capture_mode(deviceMode);
    if (ret < 0)
        return ret;

    if (device->mId == V4L2_MAIN_DEVICE ||
        device->mId == V4L2_RECORDING_DEVICE ||
        device->mId == V4L2_PREVIEW_DEVICE) {
        applySensorFlip();

        // Set high-speed video recording frame rate
        // Configure ISP with higher fps and do the frame skipping for
        // the other stream if needed.
        int fps = MAX(mConfig.preview_fps, mConfig.recording_fps);
        if (deviceMode == CI_MODE_VIDEO && fps > DEFAULT_RECORDING_FPS) {
            if (device->mId == V4L2_RECORDING_DEVICE) {
                status_t status;
                struct v4l2_streamparm parm;
                parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                parm.parm.capture.capturemode = CI_MODE_NONE;
                parm.parm.capture.timeperframe.numerator = 1;
                parm.parm.capture.timeperframe.denominator = fps;
                LOG1("setting fps: %d", fps);
                status = mMainDevice->setParameter(&parm);
                if (status != NO_ERROR) {
                    ALOGE("error setting fps: %d", fps);
                    return -1;
                }
                // Update the configuration with fps from the driver
                if (parm.parm.capture.timeperframe.denominator && parm.parm.capture.timeperframe.numerator) {
                    mConfig.fps = (float) parm.parm.capture.timeperframe.denominator / parm.parm.capture.timeperframe.numerator;
                    LOG1("Sensor fps: %.2f", mConfig.fps);
                } else {
                    ALOGW("Sensor driver returned invalid frame rate, using default");
                    mConfig.fps = DEFAULT_SENSOR_FPS;
                }
            }
        } else {
            ret = device->getFramerate(&mConfig.fps, w, h, fourcc);
            if (ret != NO_ERROR) {
            /*Error handler: if driver does not support FPS achieving,
                           just give the default value.*/
                mConfig.fps = DEFAULT_SENSOR_FPS;
                ret = 0;
            }
        }
    }

    //Set the format
    ret = device->setFormat(*formatDescriptor);
    if (ret < 0)
        return ret;

    // reduce FPS for still capture
    if (mFileInject.active == true) {
        if (deviceMode == CI_MODE_STILL_CAPTURE)
            mConfig.fps = DEFAULT_SENSOR_FPS;
    }

    PERFORMANCE_TRACES_BREAKDOWN_STEP_PARAM("DeviceId:", device->mId);
    //We need apply all the parameter settings when do the camera reset
    return ret;
}

status_t AtomISP::setPreviewFrameFormat(int width, int height, int bpl, int fourcc)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    if(fourcc == 0)
         fourcc = mConfig.preview.fourcc;
    if (width > mConfig.previewLimits.maxWidth || width <= 0)
        width = mConfig.previewLimits.maxWidth;
    if (height > mConfig.previewLimits.maxHeight || height <= 0)
        height = mConfig.previewLimits.maxHeight;
    mConfig.preview.width = width;
    mConfig.preview.height = height;
    mConfig.preview.fourcc = fourcc;
    mConfig.preview.bpl = bpl;
    mConfig.preview.size = frameSize(fourcc, bytesToPixels(fourcc, bpl), height);
    LOG1("width(%d), height(%d), bpl(%d), size(%d), fourcc(%x)",
        width, height, bpl, mConfig.preview.size, fourcc);
    return status;
}

/**
 * Configures the user requested FPS
 * ISP will drop frames to adjust the sensor fps to the requested fps
 * This frame-dropping operation is done in the PreviewStreamSource class
 *
 */
void AtomISP::setPreviewFramerate(int fps)
{
    LOG1("@%s: %d", __FUNCTION__, fps);
    mConfig.preview_fps = fps;
}

void AtomISP::getPostviewFrameFormat(AtomBuffer& formatDescriptor) const
{
    LOG1("@%s", __FUNCTION__);
    formatDescriptor = mConfig.postview;
}

status_t AtomISP::setPostviewFrameFormat(AtomBuffer& formatDescriptor)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    LOG1("@%s width(%d), height(%d), fourcc(%x)", __FUNCTION__,
         formatDescriptor.width, formatDescriptor.height, formatDescriptor.fourcc);
    if (formatDescriptor.width < 0 || formatDescriptor.height < 0) {
        ALOGE("Invalid postview size requested!");
        return BAD_VALUE;
    }
    if (formatDescriptor.width == 0 || formatDescriptor.height == 0) {
        // No thumbnail requested, we should anyway use postview to dequeue frames from ISP
        formatDescriptor.width = RESOLUTION_POSTVIEW_WIDTH;
        formatDescriptor.height = RESOLUTION_POSTVIEW_HEIGHT;
    }

    mConfig.postview = formatDescriptor;
    mConfig.postview.bpl = SGXandDisplayBpl(formatDescriptor.fourcc, formatDescriptor.width);
    mConfig.postview.size = frameSize(formatDescriptor.fourcc,
                                      bytesToPixels(formatDescriptor.fourcc, mConfig.postview.bpl),
                                      formatDescriptor.height);
    LOG1("width(%d), height(%d), bpl(%d), size(%d), fourcc(%x)",
         formatDescriptor.width, formatDescriptor.height, mConfig.postview.bpl, mConfig.postview.size, formatDescriptor.fourcc);
    return status;
}

status_t AtomISP::setSnapshotFrameFormat(AtomBuffer& formatDescriptor)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    if (formatDescriptor.width > mConfig.snapshotLimits.maxWidth || formatDescriptor.width < 0)
        formatDescriptor.width = mConfig.snapshotLimits.maxWidth;
    if (formatDescriptor.height > mConfig.snapshotLimits.maxHeight || formatDescriptor.height < 0)
        formatDescriptor.height = mConfig.snapshotLimits.maxHeight;

    mConfig.snapshot = formatDescriptor;
    mConfig.snapshot.bpl = SGXandDisplayBpl(formatDescriptor.fourcc, formatDescriptor.width);
    mConfig.snapshot.size = frameSize(formatDescriptor.fourcc, bytesToPixels(formatDescriptor.fourcc, mConfig.snapshot.bpl), formatDescriptor.height);
    LOG1("width(%d), height(%d), bpl(%d), size(%d), fourcc(%x)",
        formatDescriptor.width, formatDescriptor.height, mConfig.snapshot.bpl, mConfig.snapshot.size, formatDescriptor.fourcc);
    return status;
}

void AtomISP::getVideoSize(int *width, int *height, int *bpl = NULL)
{
    if (width && height) {
        *width = mConfig.recording.width;
        *height = mConfig.recording.height;
    }
    if (bpl)
        *bpl = mConfig.recording.bpl;
}

void AtomISP::getPreviewSize(int *width, int *height, int *bpl = NULL)
{
    if (width && height) {
        *width = mConfig.preview.width;
        *height = mConfig.preview.height;
    }
    if (bpl)
        *bpl = mConfig.preview.bpl;
}

int AtomISP::getNumSnapshotBuffers()
{
    return mConfig.num_snapshot;
}

status_t AtomISP::setVideoFrameFormat(int width, int height, int fourcc)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    /**
     * Workaround: When video size is 1080P(1920x1080), because video HW codec
     * requests 16x16 pixel block as sub-block to encode, So whatever apps set recording
     * size to 1080P, ISP always outputs 1920x1088
     * for encoder.
     * In current supported list of video size, only height 1080(1920x1080) isn't multiple of 16
     */
    if(height % 16)
        height = (height + 15) / 16 * 16;

    if(fourcc == 0)
         fourcc = mConfig.recording.fourcc;
    if (mConfig.recording.width == width &&
        mConfig.recording.height == height &&
        mConfig.recording.fourcc == fourcc) {
        // Do nothing
        return status;
    }

    if (mMode == MODE_VIDEO) {
        ALOGE("Reconfiguration in video mode unsupported. Stop the ISP first");
        return INVALID_OPERATION;
    }

    if (width > mConfig.recordingLimits.maxWidth || width <= 0) {
        ALOGE("invalid recording width %d. override to %d", width, mConfig.recordingLimits.maxWidth);
        width = mConfig.recordingLimits.maxWidth;
    }
    if (height > mConfig.recordingLimits.maxHeight || height <= 0) {
        ALOGE("invalid recording height %d. override to %d", height, mConfig.recordingLimits.maxHeight);
        height = mConfig.recordingLimits.maxHeight;
    }
    mConfig.recording.width = width;
    mConfig.recording.height = height;
    mConfig.recording.fourcc = fourcc;
    mConfig.recording.bpl = SGXandDisplayBpl(fourcc, width);
    mConfig.recording.size = frameSize(fourcc, bytesToPixels(fourcc, mConfig.recording.bpl), height);
    LOG1("width(%d), height(%d), bpl(%d), fourcc(%x)",
            width, height, mConfig.recording.bpl, fourcc);

    return status;
}


/**
 * Apply ISP limitations related to supported preview sizes.
 *
 * Workaround 1: with DVS enable, the fps in 1080p recording can't reach 30fps,
 * so check if the preview size is corresponding to recording, if yes, then
 * change preview size to 640x360
 * BZ: 49330 51853
 *
 * Workaround 2: The camera firmware doesn't support preview dimensions that
 * are bigger than video dimensions. If a single preview dimension is larger
 * than the video dimension then the preview and recording devices will be
 * swapped to work around this limitation.
 *
 * Workaround 3: With some sensors, the configuration for 1080p
 * recording does not give enough processing time (blanking time) to
 * the ISP, so the viewfinder resolution must be limited.
 * BZ: 55640 59636
 *
 * Workaround 4: When sensor vertical blanking time is too low to run ISP
 * viewfinder postprocess binary (vf_pp) during it, every other frame would be
 * dropped leading to halved frame rate. Add control V4L2_CID_ENABLE_VFPP to
 * disable vf_pp for still preview.
 *
 * Workaround 5: The camera firmware doesn't support video downscaling. For the
 * sensor imx132, it cannot keep the same FOV for different resolutions.
 * To keep the same FOV when recording at different resolutions, IMX132 driver has
 * provided a series of resolution settings with fixed full FOV height (1080).
 * To select a proper sensor setting, we need to re-calculate the preview
 * size with the same height of IMX132 full FOV height.
 * The mapping for video size and preview size would be:
 * Video Size		- Preview Size		- Sensor Setting
 * 1280x720		- 1920x1080		- 1936x1096
 * 720x480		- 1620x1080		- 1636x1096
 * 640x480&320x240	- 1440x1080		- 1056x1096
 * 352x288&176x144	- 1320x1080		- 1336x1096
 * If DVS is enabled, the preview size should be cut off by 20% so that driver can
 * add envelope for DVS.
 *
 * BZ 116055
 *
 * This mode can be enabled by setting VFPPLimitedResolutionList to a proper
 * value for the platform in the camera_profiles.xml. If e.g. for resolution
 * 1024*768 the FPS drops to half the normal because VFPP is too slow
 * to process the frame during the sensor dependent blanking time, add
 * 1024x768 to the XML resolution list.
 *
 * In case of video recording, the vf_pp is configured to small size, preview
 * and video are swapped and the video frames are created from preview frames.
 * Currently preview and recording resolutions must be the same in this case.
 *
 * In case of still mode preview, vf_pp is entirely disabled for the resolutions
 * in VFPPLimitedResolutionList.
 *
 * @param params
 * @param dvsEabled
 * @return true: updated preview size
 * @return false: not need to update preview size
 */
bool AtomISP::applyISPLimitations(CameraParameters *params,
        bool dvsEnabled, bool videoMode)
{
    LOG1("@%s", __FUNCTION__);
    bool ret = false;
    int previewWidth, previewHeight;
    int videoWidth, videoHeight;
    bool reducedVf = false;
    bool workaround2 = false;

    params->getPreviewSize(&previewWidth, &previewHeight);
    params->getVideoSize(&videoWidth, &videoHeight);

    if (videoMode || mMode == MODE_VIDEO) {
        // Workaround 3: with some sensors the VF resolution must be
        //               limited high-resolution video recordiing
        // TODO: if we get more cases like this, move to PlatformData.h
        const char* sensorName = "ov8830";
        if (strncmp(mSensorHW.getSensorName(), sensorName, sizeof(sensorName) - 1) == 0) {
            LOG1("Quirk for sensor %s, limiting video preview size", sensorName);
            reducedVf = true;
        }

        // Workaround 1+3, detail refer to the function description, BYT
        // doesn't need this WA to support 1080p preview
        if ((reducedVf || (dvsEnabled && mCssMajorVersion == 1)) &&
            PlatformData::supportPreviewLimitation()) {
            if ((previewWidth > RESOLUTION_VGA_WIDTH || previewHeight > RESOLUTION_VGA_HEIGHT) &&
                (videoWidth > RESOLUTION_720P_WIDTH || videoHeight > RESOLUTION_720P_HEIGHT)) {
                    ret = true;
                    params->setPreviewSize(640, 360);
                    LOG1("change preview size to 640x360 due to DVS on");
                } else {
                    LOG1("no need change preview size: %dx%d", previewWidth, previewHeight);
                }
        }
        //Workaround 5, video recording FOV issue
        const char manUsensorBName[] = "imx132";
        if (strncmp(mSensorHW.getSensorName(), manUsensorBName, sizeof(manUsensorBName) - 1) == 0) {
            // If DVS is not enabled, keep preview height as 1080 which is full FOV height for IMX132.
            // If DVS is enabled, keep preview height as 900 which is 20% cut off by 1080.
            // 1080 = 900 * (1 + 20%)
            // Refer to function description for more details.
            if (dvsEnabled)
                params->setPreviewSize(videoWidth*900/videoHeight, 900);
            else
                params->setPreviewSize(videoWidth*1080/videoHeight, 1080);
        }

        //Workaround 2, detail refer to the function description
        params->getPreviewSize(&previewWidth, &previewHeight);
        params->getVideoSize(&videoWidth, &videoHeight);
        if((previewWidth*previewHeight) > (videoWidth*videoHeight)) {
            if(videoWidth != mConfig.recording.width || videoHeight != mConfig.recording.height
               || !mRecordingDeviceSwapped) {
                ret = true;
                mSwapRecordingDevice = true;
                workaround2 = true;
                LOG1("Video dimension(s) [%d, %d] is smaller than preview dimension(s) [%d, %d]. "
                     "Triggering swapping of preview and recording devices.",
                     videoWidth, videoHeight, previewWidth, previewHeight);
            }
        } else {
            mSwapRecordingDevice = false;
        }
    }

    // workaround 4, detail refer to the function description
    bool previewSizeSupported = PlatformData::resolutionSupportedByVFPP(mCameraId, previewWidth, previewHeight);
    if (!previewSizeSupported) {
        if (videoMode || mMode == MODE_VIDEO) {
            if (workaround2) {
                // swapping is on already due to preview bigger than video (workaround 2)
                // we don't need to do anything vfpp related anymore
                // TODO support for situations where preview is bigger than
                // video, swapping is on due to workaround 2, but vfpp size is still too big
                mPreviewTooBigForVFPP = false;
                bool videoSizeSupported = PlatformData::resolutionSupportedByVFPP(mCameraId, videoWidth, videoWidth);
                if (!videoSizeSupported) {
                    ALOGE("@%s ERROR: Video recording with preview > video "
                         "resolution and video resolution > maxPreviewPixelCountForVFPP "
                         "is not yet supported.", __FUNCTION__);
                }
            } else {
                mPreviewTooBigForVFPP = true; // video mode, too big preview, not yet swapped for workaround 2
                mSwapRecordingDevice = true; // swap for this workaround 4, since vfpp is too slow
                if (previewWidth * previewHeight != videoWidth * videoHeight) {
                    ALOGE("@%s ERROR: Video recording with preview resolution > "
                         "maxPreviewPixelCountForVFPP and recording resolution > "
                         "preview resolution is not yet supported.", __FUNCTION__);
                }
            }
        } else
            mPreviewTooBigForVFPP = true; // not video mode, too big preview
    } else {
        mPreviewTooBigForVFPP = false; // preview size is small enough for vfpp
    }

    return ret;
}

void AtomISP::getZoomRatios(CameraParameters *params) const
{
    LOG1("@%s", __FUNCTION__);
    if (params) {
        params->set(CameraParameters::KEY_MAX_ZOOM, mZoomRatioTable.size() - 1);
        params->set(CameraParameters::KEY_ZOOM_RATIOS, mZoomRatios);
        params->set(CameraParameters::KEY_ZOOM_SUPPORTED, CameraParameters::TRUE);
    }
}

void AtomISP::getFocusDistances(CameraParameters *params)
{
    LOG1("@%s", __FUNCTION__);
    char focusDistance[100] = {0};
    float fDistances[3] = {0};  // 3 distances: near, optimal, and far

    // would be better if we could get these from driver instead of hard-coding
    if (PlatformData::cameraFacing(mCameraId) == CAMERA_FACING_BACK) {
        fDistances[0] = 2.0;
        fDistances[1] = 2.0;
        fDistances[2] = INFINITY;
    }
    else {
        fDistances[0] = 0.3;
        fDistances[1] = 0.65;
        fDistances[2] = INFINITY;
    }

    for (int i = 0; i < (int) (sizeof(fDistances)/sizeof(fDistances[0])); i++) {
        int left = sizeof(focusDistance) - strlen(focusDistance);
        int res;

        // use CameraParameters::FOCUS_DISTANCE_INFINITY for value of infinity
        if (fDistances[i] == INFINITY) {
            res = snprintf(focusDistance + strlen(focusDistance), left, "%s%s",
                    i ? "," : "", CameraParameters::FOCUS_DISTANCE_INFINITY);
        } else {
            res = snprintf(focusDistance + strlen(focusDistance), left, "%s%g",
                    i ? "," : "", fDistances[i]);
        }
        if (res < 0) {
            ALOGE("Could not generate %s string: %s",
                CameraParameters::KEY_FOCUS_DISTANCES, strerror(errno));
            return;
        }
    }
    params->set(CameraParameters::KEY_FOCUS_DISTANCES, focusDistance);
}

status_t AtomISP::setFlash(int numFrames)
{
    LOG1("@%s: numFrames = %d", __FUNCTION__, numFrames);
    if (PlatformData::cameraFacing(mCameraId) != CAMERA_FACING_BACK) {
        ALOGE("Flash is supported only for primary camera!");
        return INVALID_OPERATION;
    }
    if (numFrames) {
        if (mMainDevice->setControl(V4L2_CID_FLASH_MODE, ATOMISP_FLASH_MODE_FLASH, "Flash Mode flash") < 0)
            return UNKNOWN_ERROR;
        if (mMainDevice->setControl(V4L2_CID_REQUEST_FLASH, numFrames, "Request Flash") < 0)
            return UNKNOWN_ERROR;

        mFlashIsOn = true;
    } else {
        if (mMainDevice->setControl(V4L2_CID_FLASH_MODE, ATOMISP_FLASH_MODE_OFF, "Flash Mode flash") < 0)
            return UNKNOWN_ERROR;

        mFlashIsOn = false;
    }

    ALOGD("setFlash() mFlashIsOn: %d", mFlashIsOn);
    return NO_ERROR;
}

status_t AtomISP::setFlashIndicator(int intensity)
{
    LOG1("@%s: intensity = %d", __FUNCTION__, intensity);
    if (PlatformData::cameraFacing(mCameraId) != CAMERA_FACING_BACK) {
        ALOGE("Indicator intensity is supported only for primary camera!");
        return INVALID_OPERATION;
    }

    if (intensity) {
        if (mMainDevice->setControl(V4L2_CID_FLASH_INDICATOR_INTENSITY, intensity, "Indicator Intensity") < 0)
            return UNKNOWN_ERROR;
        if (mMainDevice->setControl(V4L2_CID_FLASH_MODE, ATOMISP_FLASH_MODE_INDICATOR, "Flash Mode") < 0)
            return UNKNOWN_ERROR;
    } else {
        if (mMainDevice->setControl(V4L2_CID_FLASH_MODE, ATOMISP_FLASH_MODE_OFF, "Flash Mode") < 0)
            return UNKNOWN_ERROR;
    }
    return NO_ERROR;
}

status_t AtomISP::setTorchHelper(int intensity)
{
    if (intensity) {
        if (mMainDevice->setControl(V4L2_CID_FLASH_TORCH_INTENSITY, intensity, "Torch Intensity") < 0)
            return UNKNOWN_ERROR;
        if (mMainDevice->setControl(V4L2_CID_FLASH_MODE, ATOMISP_FLASH_MODE_TORCH, "Flash Mode") < 0)
            return UNKNOWN_ERROR;

        mFlashIsOn = true;
    } else {
        if (mMainDevice->setControl(V4L2_CID_FLASH_MODE, ATOMISP_FLASH_MODE_OFF, "Flash Mode") < 0)
            return UNKNOWN_ERROR;

        mFlashIsOn = false;
    }

    ALOGD("mFlashIsOn: %d", mFlashIsOn);
    return NO_ERROR;
}

status_t AtomISP::setTorch(int intensity)
{
    LOG1("@%s: intensity = %d", __FUNCTION__, intensity);

    if (PlatformData::cameraFacing(mCameraId) != CAMERA_FACING_BACK) {
        ALOGE("Indicator intensity is supported only for primary camera!");
        return INVALID_OPERATION;
    }

    setTorchHelper(intensity);

    // closing the kernel device will not automatically turn off
    // flash light, so need to keep track in user-space
    mFlashTorchSetting = intensity;

    return NO_ERROR;
}

status_t AtomISP::setColorEffect(v4l2_colorfx effect)
{
    mColorEffect = effect;
    return NO_ERROR;
}

status_t AtomISP::applyColorEffect()
{
    LOG2("@%s: effect = %d", __FUNCTION__, mColorEffect);
    status_t status = NO_ERROR;

    // Color effect overrides configs that AIC has set
    if (mMainDevice->setControl(V4L2_CID_COLORFX, mColorEffect, "Colour Effect") < 0){
        ALOGE("Error setting color effect");
        return UNKNOWN_ERROR;
    }
    return status;
}

status_t AtomISP::setZoom(int zoom)
{
    LOG1("@%s: zoom = %d - %d", __FUNCTION__, zoom, mConfig.zoom);
    if (zoom == mConfig.zoom)
        return NO_ERROR;
    if (mMode == MODE_CAPTURE)
        return NO_ERROR;

    int ret = atomisp_set_zoom(zoom);

    if (ret < 0) {
        ALOGE("Error setting zoom to %d", zoom);
        return UNKNOWN_ERROR;
    }

    mConfig.zoom = zoom;
    return NO_ERROR;
}

int AtomISP::getDrvZoom(int zoom)
{
    return mZoomDriveTable[zoom];
}

status_t AtomISP::getMakerNote(atomisp_makernote_info *info)
{
    LOG1("@%s: info = %p", __FUNCTION__, info);

    info->focal_length = 0;
    info->f_number_curr = 0;
    info->f_number_range = 0;

    if (mMainDevice->xioctl(ATOMISP_IOC_ISP_MAKERNOTE, info) < 0) {
        ALOGW("WARNING: get maker note from driver failed!");
        return UNKNOWN_ERROR;
    }
    return NO_ERROR;
}

status_t AtomISP::getContrast(int *value)
{
    LOG1("@%s", __FUNCTION__);

    if (mMainDevice->getControl(V4L2_CID_CONTRAST, value) < 0) {
        ALOGW("WARNING: get Contrast from driver failed!");
        return UNKNOWN_ERROR;
    }
    LOG1("@%s: got %d", __FUNCTION__, *value);
    return NO_ERROR;
}

status_t AtomISP::setContrast(int value)
{
    LOG1("@%s: value:%d", __FUNCTION__, value);

    if (mMainDevice->setControl(V4L2_CID_CONTRAST, value, "Request Contrast") < 0) {
        ALOGW("WARNING: set Contrast from driver failed!");
        return UNKNOWN_ERROR;
    }
    return NO_ERROR;
}

status_t AtomISP::getSaturation(int *value)
{
    LOG1("@%s", __FUNCTION__);

    if (mMainDevice->getControl(V4L2_CID_SATURATION, value) < 0) {
        ALOGW("WARNING: get Saturation from driver failed!");
        return UNKNOWN_ERROR;
    }
    LOG1("@%s: got %d", __FUNCTION__, *value);
    return NO_ERROR;
}

status_t AtomISP::setSaturation(int value)
{
    LOG1("@%s: value:%d", __FUNCTION__, value);

    if (mMainDevice->setControl(V4L2_CID_SATURATION, value, "Request Saturation") < 0) {
        ALOGW("WARNING: set Saturation from driver failed!");
        return UNKNOWN_ERROR;
    }
    return NO_ERROR;
}

status_t AtomISP::getSharpness(int *value)
{
    LOG1("@%s", __FUNCTION__);

    if (mMainDevice->getControl(V4L2_CID_SHARPNESS, value) < 0) {
        ALOGW("WARNING: get Sharpness from driver failed!");
        return UNKNOWN_ERROR;
    }
    LOG1("@%s: got %d", __FUNCTION__, *value);
    return NO_ERROR;
}

status_t AtomISP::setSharpness(int value)
{
    LOG1("@%s: value:%d", __FUNCTION__, value);

    if (mMainDevice->setControl(V4L2_CID_SHARPNESS, value, "Request Sharpness") < 0) {
        ALOGW("WARNING: set Sharpness from driver failed!");
        return UNKNOWN_ERROR;
    }
    return NO_ERROR;
}

status_t AtomISP::setXNR(bool enable)
{
    LOG1("@%s: %d", __FUNCTION__, (int) enable);
    mXnr = enable;
    return NO_ERROR;
}

status_t AtomISP::setDVS(bool enable)
{
    // OK. DVS1 not supported
    return NO_ERROR;
}
/**
 * Because first frame should be the reference frame for DVS2 lib to calculate
 * morph table, and zoom factor should not take effect on first 3 frames, so those
 * frames should be skipped.
 * The function implements to set the skip frames number for video recording when
 * zoom factor is not equal zero.
 */
status_t AtomISP::setSkipFramesForVideoZoom()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    if(mCssMajorVersion != 2)
        return status;
    if(mConfig.zoom != 0)
        mVideoZoomFrameSkips = VIDEO_ZOOM_SKIP_FRAMES;
    return status;
}

bool AtomISP::dvsEnabled()
{
    // DVS1 not supported
    return false;
}

status_t AtomISP::setDVSSkipFrames(unsigned int skips)
{
    LOG1("@%s: %d", __FUNCTION__, skips);
    // No-op. DVS not supported.
    return NO_ERROR;
}

status_t AtomISP::setGDC(bool enable)
{
    LOG1("@%s: %d", __FUNCTION__, enable);
    status_t status;
    status = mMainDevice->setControl(V4L2_CID_ATOMISP_POSTPROCESS_GDC_CAC,
                                   enable, "GDC");

    return status;
}

void AtomISP::setPreviewBufNum(int num)
{
    LOG1("@%s: %d", __FUNCTION__, num);
    mConfig.num_preview_buffers = num;
}

status_t AtomISP::setLowLight(bool enable)
{
    LOG1("@%s: %d", __FUNCTION__, (int) enable);
    mLowLight = enable;
    return NO_ERROR;
}

int AtomISP::atomisp_set_zoom (int zoom)
{
    LOG1("@%s : value %d", __FUNCTION__, zoom);
    int ret = 0;

    if (!mHALZSLEnabled) { // fix for driver zoom bug, prevent setting in HAL ZSL mode
        int zoom_driver(mZoomDriveTable[zoom]);
        LOG1("set zoom %d to driver with %d", zoom, zoom_driver);
        ret = mMainDevice->setControl (V4L2_CID_ZOOM_ABSOLUTE, zoom_driver, "zoom");
    }

    return ret;
}

/**
 * Start inject image data from a file using the special-purpose
 * V4L2 device node.
 */
int AtomISP::startFileInject(void)
{
    LOG1("%s: enter", __FUNCTION__);

    int ret = 0;
    int buffer_count = 1;

    if (mFileInject.active != true) {
        ALOGE("%s: no input file set",  __func__);
        return -1;
    }

    if (mFileInjectDevice->open() != NO_ERROR) {
        goto error1;
    }

    // Query and check the capabilities
    struct v4l2_capability cap;
    if (mFileInjectDevice->queryCap(&cap) < 0)
        goto error1;

    struct v4l2_streamparm parm;
    memset(&parm, 0, sizeof(parm));
    parm.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    parm.parm.output.outputmode = OUTPUT_MODE_FILE;

    if (mFileInjectDevice->setParameter(&parm) != NO_ERROR) {
        ALOGE("error setting parameter V4L2_BUF_TYPE_VIDEO_OUTPUT %s", strerror(errno));
        return -1;
    }

    if (fileInjectSetSize() != NO_ERROR)
        goto error1;

    //Set the format
    struct v4l2_format format;
    format.fmt.pix.width = mFileInject.formatDescriptor.width;
    format.fmt.pix.height = mFileInject.formatDescriptor.height;
    format.fmt.pix.pixelformat = mFileInject.formatDescriptor.fourcc;
    format.fmt.pix.sizeimage = PAGE_ALIGN(mFileInject.formatDescriptor.size);
    format.fmt.pix.priv = mFileInject.bayerOrder;

    if (mFileInjectDevice->setFormat(format) != NO_ERROR)
        goto error1;

    // allocate buffer and copy file content into it

    if (mFileInject.dataAddr != NULL) {
        delete[] mFileInject.dataAddr;
        mFileInject.dataAddr = NULL;
    }

    mFileInject.dataAddr = new char[mFileInject.formatDescriptor.size];
    if (mFileInject.dataAddr == NULL) {
        ALOGE("Not enough memory to allocate injection buffer");
        goto error1;
    }

    // fill buffer with image data from file
    FILE *file;
    if (!(file = fopen(mFileInject.fileName.string(), "r"))) {
        ALOGE("ERR(%s): Failed to open %s\n", __func__, mFileInject.fileName.string());
        goto error1;
    }
    fread(mFileInject.dataAddr, 1,  mFileInject.formatDescriptor.size, file);
    fclose(file);

    // Set buffer pool
    ret = mFileInjectDevice->setBufferPool((void**)&mFileInject.dataAddr, buffer_count,
                                           &mFileInject.formatDescriptor,true);

    ret = mFileInjectDevice->createBufferPool(buffer_count);
    if (ret < 0)
        goto error1;

    // Queue the buffers to device
    ret = mFileInjectDevice->activateBufferPool();
    if (ret < 0)
        goto error0;

    return 0;

error0:
    mFileInjectDevice->destroyBufferPool();
error1:
    mFileInjectDevice->close();
    return -1;
}

/**
 * Stops file injection.
 *
 * Closes the kernel resources needed for file injection and
 * other resources.
 */
int AtomISP::stopFileInject(void)
{
    LOG1("%s: enter", __FUNCTION__);

    if (mFileInject.dataAddr != NULL) {
        delete[] mFileInject.dataAddr;
        mFileInject.dataAddr = NULL;
    }

    mFileInjectDevice->close();

    return 0;
}

/**
 * Configures image data injection.
 *
 * If 'fileName' is non-NULL, file injection is enabled with the given
 * settings. Once enabled, file injection will be performend when
 * start() is issued, and stopped when stop() is issued. Injection
 * applies to all device modes.
 */
int AtomISP::configureFileInject(const char *fileName, int width, int height, int fourcc, int bayerOrder)
{
    LOG1("%s: enter", __FUNCTION__);
    mFileInject.fileName = String8(fileName);
    if (mFileInject.fileName.isEmpty() != true) {
        LOG1("Enabling file injection, image file=%s", mFileInject.fileName.string());
        mFileInject.active = true;
        mFileInject.formatDescriptor.width = width;
        mFileInject.formatDescriptor.height = height;
        mFileInject.formatDescriptor.fourcc = fourcc;
        mFileInject.bayerOrder = bayerOrder;
    }
    else {
        mFileInject.active = false;
        LOG1("Disabling file injection");
    }
    return 0;
}

status_t AtomISP::fileInjectSetSize(void)
{
    int fileFd = -1;
    int fileSize = 0;
    struct stat st;
    const char* fileName = mFileInject.fileName.string();

    /* Open the file we will transfer to kernel */
    if ((fileFd = open(mFileInject.fileName.string(), O_RDONLY)) == -1) {
        ALOGE("ERR(%s): Failed to open %s\n", __FUNCTION__, fileName);
        return INVALID_OPERATION;
    }

    CLEAR(st);
    if (fstat(fileFd, &st) < 0) {
        ALOGE("ERR(%s): fstat %s failed\n", __func__, fileName);
        close(fileFd);
        return INVALID_OPERATION;
    }

    fileSize = st.st_size;
    if (fileSize == 0) {
        ALOGE("ERR(%s): empty file %s\n", __func__, fileName);
        close(fileFd);
        return -1;
    }

    LOG1("%s: file %s size of %u", __FUNCTION__, fileName, fileSize);

    mFileInject.formatDescriptor.size = fileSize;

    close(fileFd);
    return NO_ERROR;
}

int AtomISP::atomisp_set_capture_mode(int deviceMode)
{
    LOG1("@%s", __FUNCTION__);
    struct v4l2_streamparm parm;
    status_t status;
    int vfpp_mode = ATOMISP_VFPP_ENABLE;
    if (deviceMode == CI_MODE_PREVIEW && mPreviewTooBigForVFPP)
        vfpp_mode = ATOMISP_VFPP_DISABLE_SCALER;
    else if(mHALZSLEnabled)
        vfpp_mode = ATOMISP_VFPP_DISABLE_LOWLAT;

    CLEAR(parm);

    switch (deviceMode) {
    case CI_MODE_PREVIEW:
        LOG1("Setting CI_MODE_PREVIEW mode");
        break;
    case CI_MODE_STILL_CAPTURE:
        LOG1("Setting CI_MODE_STILL_CAPTURE mode");
        break;
    case CI_MODE_VIDEO:
        LOG1("Setting CI_MODE_VIDEO mode");
        break;
    default:
        break;
    }

    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    parm.parm.capture.capturemode = deviceMode;

    status = mMainDevice->setParameter(&parm);
    if (status != NO_ERROR) {
        ALOGE("error setting the mode %d", deviceMode);
        return -1;
    }

    if (mMainDevice->setControl(V4L2_CID_VFPP, vfpp_mode, "V4L2_CID_VFPP") != NO_ERROR) {
        /* Menu-style V4L2_CID_VFPP not available, try legacy V4L2_CID_ENABLE_VFPP */
        ALOGW("warning %s: V4L2_CID_VFPP %i failed, trying V4L2_CID_ENABLE_VFPP", strerror(errno), vfpp_mode);
        if (mMainDevice->setControl(V4L2_CID_ENABLE_VFPP, vfpp_mode == ATOMISP_VFPP_ENABLE, "V4L2_CID_ENABLE_VFPP") != NO_ERROR) {
            ALOGW("warning %s: V4L2_CID_ENABLE_VFPP failed", strerror(errno));
            if (vfpp_mode != ATOMISP_VFPP_ENABLE) {
                ALOGE("error: can not disable vf_pp");
                return -1;
            } /* else vf_pp enabled by default, so everything should be all right */
        }
    }

    return 0;
}


/**
 * Pushes all recording buffers back into driver except the ones already queued
 *
 * Note: Currently no support for shared buffers for cautions
 */
status_t AtomISP::returnRecordingBuffers()
{
    LOG1("@%s", __FUNCTION__);
    if (mRecordingBuffers) {
        for (int i = 0 ; i < mConfig.num_recording_buffers; i++) {
            if (mRecordingBuffers[i].shared)
                return UNKNOWN_ERROR;
            if (mRecordingBuffers[i].buff == NULL)
                return UNKNOWN_ERROR;
            // identifying already queued frames with negative id
            if (mRecordingBuffers[i].id == -1)
                continue;
            putRecordingFrame(&mRecordingBuffers[i]);
        }
    }
    return NO_ERROR;
}

void AtomISP::dumpHALZSLBufs() {
    size_t size = mHALZSLCaptureBuffers.size();

    LOG2("*** begin HAL ZSL DUMP ***");
    for (unsigned int i = 0; i < size; i++) {
        AtomBuffer buf = mHALZSLCaptureBuffers.itemAt(i);
        LOG2("HAL ZSL buffer %d framecounter %d timestamp %ld.%ld", i, buf.frameCounter, buf.capture_timestamp.tv_sec, buf.capture_timestamp.tv_usec);
    }
    LOG2("*** end HAL ZSL DUMP ***");
}

void AtomISP::dumpHALZSLPreviewBufs() {
    size_t size = mHALZSLPreviewBuffers.size();

    LOG2("*** begin HAL ZSL Preview DUMP ***");
    for (unsigned int i = 0; i < size; i++) {
        AtomBuffer buf = mHALZSLPreviewBuffers.itemAt(i);
        LOG2("HAL ZSL buffer %d framecounter %d timestamp %ld.%ld", i, buf.frameCounter, buf.capture_timestamp.tv_sec, buf.capture_timestamp.tv_usec);
    }
    LOG2("*** end HAL ZSL Preview DUMP ***");
}

/**
 * Waits for buffers to arrive in the given vector. If there aren't initially
 * any buffers, this sleeps and retries a predefined amount of cycles.
 *
 * Preconditions: snapshot case - mHALZSLLock is locked.
 *                preview case - mDevices[mPreviewDevice].mutex and
 *                               mHALZSLLock are locked in that order
 * \param vector of AtomBuffers
 * \param snapshot: boolean to distinguish whether we are waiting for a
 *                  ZSL buffer to get a snapshot or preview frame.
 * \return true if there are buffers in the vector, false if not
 */
bool AtomISP::waitForHALZSLBuffer(Vector<AtomBuffer> &vector, bool snapshot)
{
    LOG2("@%s", __FUNCTION__);
    int retryCount = sHALZSLRetryCount;
    size_t size = 0;
    do {
        size = vector.size();
        if (size == 0) {
            mHALZSLLock.unlock();
            if (!snapshot)
                mDeviceMutex[mPreviewDevice->mId].unlock();
            ALOGW("@%s, AtomISP starving from ZSL buffers.", __FUNCTION__);
            usleep(sHALZSLRetryUSleep);
            if (!snapshot)
                mDeviceMutex[mPreviewDevice->mId].lock(); // this locking order is significant!
            mHALZSLLock.lock();
        }
    } while(retryCount-- && size == 0);

    return (size != 0);
}

status_t AtomISP::getHALZSLPreviewFrame(AtomBuffer *buff)
{
    LOG2("@%s", __FUNCTION__);
    // NOTE: mDevices[mPreviewDevice].mutex locked in getPreviewFrame

    struct v4l2_buffer_info bufInfo;
    CLEAR(bufInfo);

    Mutex::Autolock mLock(mHALZSLLock);

    int index = mPreviewDevice->grabFrame(&bufInfo);
    if(index < 0){
        ALOGE("@%s Error in grabbing frame!", __FUNCTION__);
        return BAD_INDEX;
    }
    LOG2("Device: %d. Grabbed frame of size: %d", mPreviewDevice->mId, bufInfo.vbuffer.bytesused);
    mNumPreviewBuffersQueued--;
    LOG2("@%s mNumPreviewBuffersQueued:%d", __FUNCTION__, mNumPreviewBuffersQueued);

    mHALZSLBuffers[index].id = index;
    mHALZSLBuffers[index].frameCounter = mPreviewDevice->getFrameCount();
    mHALZSLBuffers[index].ispPrivate = mSessionId;
    mHALZSLBuffers[index].capture_timestamp = bufInfo.vbuffer.timestamp;
    mHALZSLBuffers[index].frameSequenceNbr = bufInfo.vbuffer.sequence;
    mHALZSLBuffers[index].status = (FrameBufferStatus)bufInfo.vbuffer.reserved;

    if (!waitForHALZSLBuffer(mHALZSLPreviewBuffers, false)) {
        ALOGE("@%s no preview buffers in FIFO!", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    AtomBuffer previewBuf = mHALZSLPreviewBuffers.itemAt(0);
    mHALZSLPreviewBuffers.removeAt(0);

    if (!(bufInfo.vbuffer.flags & V4L2_BUF_FLAG_ERROR)) {
        float zoomFactor(static_cast<float>(zoomRatio(mConfig.zoom)) / ZOOM_RATIO);
        mScaler->scaleAndZoom(&mHALZSLBuffers[index], &previewBuf, zoomFactor);
    }
    previewBuf.frameCounter = mHALZSLBuffers[index].frameCounter;
    previewBuf.capture_timestamp = mHALZSLBuffers[index].capture_timestamp;
    previewBuf.frameSequenceNbr = mHALZSLBuffers[index].frameSequenceNbr;
    previewBuf.status = mHALZSLBuffers[index].status;

    *buff = previewBuf;

    mHALZSLCaptureBuffers.push(mHALZSLBuffers[index]);

    return NO_ERROR;

}

status_t AtomISP::getPreviewFrame(AtomBuffer *buff)
{
    LOG2("@%s", __FUNCTION__);
    Mutex::Autolock lock(mDeviceMutex[mPreviewDevice->mId]);
    struct v4l2_buffer_info bufInfo;
    CLEAR(bufInfo);

    if (mMode == MODE_NONE)
        return INVALID_OPERATION;

    if (mHALZSLEnabled)
        return getHALZSLPreviewFrame(buff);

    int index = mPreviewDevice->grabFrame(&bufInfo);
    if(index < 0){
        ALOGE("Error in grabbing frame!");
        return BAD_INDEX;
    }

    LOG2("Device: %d. Grabbed frame of size: %d", mPreviewDevice->mId, bufInfo.vbuffer.bytesused);
    mPreviewBuffers.editItemAt(index).id = index;
    mPreviewBuffers.editItemAt(index).frameCounter = mPreviewDevice->getFrameCount();
    mPreviewBuffers.editItemAt(index).ispPrivate = mSessionId;
    mPreviewBuffers.editItemAt(index).capture_timestamp = bufInfo.vbuffer.timestamp;
    mPreviewBuffers.editItemAt(index).frameSequenceNbr = bufInfo.vbuffer.sequence;
    mPreviewBuffers.editItemAt(index).status = (FrameBufferStatus)bufInfo.vbuffer.reserved;
    mPreviewBuffers.editItemAt(index).size = bufInfo.vbuffer.bytesused;

    *buff = mPreviewBuffers[index];

    mNumPreviewBuffersQueued--;

    dumpPreviewFrame(index);

    return NO_ERROR;
}

status_t AtomISP::putHALZSLPreviewFrame(AtomBuffer *buff)
{
    LOG2("@%s", __FUNCTION__);
    // NOTE - mDevices[mPreviewDevice].mutex locked in putPreviewFrame

    Mutex::Autolock mLock(mHALZSLLock);
    mHALZSLPreviewBuffers.push(*buff);

    size_t size = mHALZSLCaptureBuffers.size();
    LOG2("@%s cap buffers size was %d", __FUNCTION__, size);

    if (size > sMaxHALZSLBuffersHeldInHAL) {
        AtomBuffer buf = mHALZSLCaptureBuffers.itemAt(0);
        mHALZSLCaptureBuffers.removeAt(0);

        if (mPreviewDevice->putFrame(buf.id) < 0) {
            return UNKNOWN_ERROR;
        }
        mNumPreviewBuffersQueued++;
        LOG2("@%s mNumPreviewBuffersQueued:%d", __FUNCTION__, mNumPreviewBuffersQueued);
    }

    dumpHALZSLBufs();

    return NO_ERROR;
}

status_t AtomISP::putPreviewFrame(AtomBuffer *buff)
{
    LOG2("@%s", __FUNCTION__);
    Mutex::Autolock lock(mDeviceMutex[mPreviewDevice->mId]);
    if (mMode == MODE_NONE)
        return INVALID_OPERATION;

    if (mHALZSLEnabled)
        return putHALZSLPreviewFrame(buff);

    if ((buff->type == ATOM_BUFFER_PREVIEW) && (buff->ispPrivate != mSessionId))
        return DEAD_OBJECT;

    if (mPreviewDevice->putFrame(buff->id) < 0) {
        return UNKNOWN_ERROR;
    }

    // using -1 index to identify queued buffers
    // id gets updated with dqbuf
    mPreviewBuffers.editItemAt(buff->id).id = -1;

    mNumPreviewBuffersQueued++;

    return NO_ERROR;
}

/**
 * Override function for IBufferOwner
 *
 * Note: currently used only for preview
 */
void AtomISP::returnBuffer(AtomBuffer* buff)
{
    LOG2("@%s", __FUNCTION__);
    status_t status;
    if ((buff->type != ATOM_BUFFER_PREVIEW_GFX) &&
        (buff->type != ATOM_BUFFER_PREVIEW)) {
        ALOGE("Received unexpected buffer!");
    } else {
        buff->owner = 0;
        status = putPreviewFrame(buff);
        if (status != NO_ERROR) {
            ALOGE("Failed queueing preview frame!");
        }
    }
}

/**
 * Sets the externally allocated graphic buffers to be used
 * for the preview stream
 */
status_t AtomISP::setGraphicPreviewBuffers(const AtomBuffer *buffs, int numBuffs, bool cached)
{
    LOG1("@%s: buffs = %p, numBuffs = %d", __FUNCTION__, buffs, numBuffs);

    if (buffs == NULL || numBuffs <= 0)
        return BAD_VALUE;

    if (mConfig.num_preview_buffers != numBuffs) {
        ALOGE("Invalid shared preview buffer count configuration");
        return UNKNOWN_ERROR;
    }

    freePreviewBuffers();

    for (int i = 0; i < numBuffs; i++) {
        mPreviewBuffers.push(buffs[i]);
    }

    mPreviewBuffersCached = cached;

    return NO_ERROR;
}

status_t AtomISP::getRecordingFrame(AtomBuffer *buff)
{
    LOG2("@%s", __FUNCTION__);
    struct v4l2_buffer_info buf;
    Mutex::Autolock lock(mDeviceMutex[mRecordingDevice->mId]);

    if (mMode != MODE_VIDEO)
        return INVALID_OPERATION;

    CLEAR(buf);

    int pollResult = mRecordingDevice->poll(0);
    if (pollResult < 1) {
        LOG2("No data in recording device, poll result: %d", pollResult);
        return NOT_ENOUGH_DATA;
    }

    int index = mRecordingDevice->grabFrame(&buf);
    LOG2("index = %d", index);
    if(index < 0) {
        ALOGE("Error in grabbing frame!");
        return BAD_INDEX;
    }
    LOG2("Device: %d. Grabbed frame of size: %d", mRecordingDevice->mId, buf.vbuffer.bytesused);
    mRecordingBuffers[index].id = index;
    mRecordingBuffers[index].frameCounter = mRecordingDevice->getFrameCount();
    mRecordingBuffers[index].ispPrivate = mSessionId;
    mRecordingBuffers[index].capture_timestamp = buf.vbuffer.timestamp;
    mRecordingBuffers[index].status = (FrameBufferStatus) buf.vbuffer.reserved;
    *buff = mRecordingBuffers[index];
    buff->bpl = mConfig.recording.bpl;

    mNumRecordingBuffersQueued--;

    dumpRecordingFrame(index);

    return NO_ERROR;
}

status_t AtomISP::putRecordingFrame(AtomBuffer *buff)
{
    LOG2("@%s", __FUNCTION__);
    Mutex::Autolock lock(mDeviceMutex[mRecordingDevice->mId]);

    if (mMode != MODE_VIDEO)
        return INVALID_OPERATION;

    if (buff->ispPrivate != mSessionId)
        return DEAD_OBJECT;

    if (mRecordingDevice->putFrame(buff->id) < 0) {
        return UNKNOWN_ERROR;
    }
    mRecordingBuffers[buff->id].id = -1;
    mNumRecordingBuffersQueued++;

    return NO_ERROR;
}

/**
 * Initializes the snapshot buffers allocated by the PictureThread to the
 * internal array
 */
status_t AtomISP::setSnapshotBuffers(Vector<AtomBuffer> *buffs, int numBuffs, bool cached)
{
    LOG1("@%s: buffs = %p, numBuffs = %d", __FUNCTION__, buffs, numBuffs);
    if (buffs == NULL || numBuffs <= 0)
        return BAD_VALUE;

    mClientSnapshotBuffersCached = cached;
    mConfig.num_snapshot = numBuffs;
    mUsingClientSnapshotBuffers = true;
    for (int i = 0; i < numBuffs; i++) {
        mSnapshotBuffers[i] = buffs->top();
        buffs->pop();
        LOG1("Snapshot buffer %d = %p", i, mSnapshotBuffers[i].dataPtr);
    }

    return NO_ERROR;
}

/**
 * Initializes the postview buffers to the internal array
 */
status_t AtomISP::setPostviewBuffers(Vector<AtomBuffer> *buffs, int numBuffs, bool cached)
{
    LOG1("@%s: buffs = %p, numBuffs = %d", __FUNCTION__, buffs, numBuffs);
    if (buffs == NULL || numBuffs <= 0)
        return BAD_VALUE;

    mClientSnapshotBuffersCached = cached;
    mConfig.num_postviews = numBuffs;
    mUsingClientPostviewBuffers = true;

    mPostviewBuffers.clear();

    for (int i = 0; i < numBuffs; i++) {
        mPostviewBuffers.push(buffs->top());
        buffs->pop();
        LOG1("@%s: postview buffer %d = %p", __FUNCTION__, i, mPostviewBuffers[i].dataPtr);
    }

    return NO_ERROR;
}

/**
 * Finds the matching preview buffer for a given frameCounter value.
 * Returns a pointer to the buffer, null if not found.
 */
AtomBuffer* AtomISP::findMatchingHALZSLPreviewFrame(int frameCounter)
{
    Vector<AtomBuffer>::iterator it = mHALZSLPreviewBuffers.begin();
    for (;it != mHALZSLPreviewBuffers.end(); ++it)
        if (it->frameCounter == frameCounter) {
            return it;
        }
    return NULL;
}

void AtomISP::copyOrScaleHALZSLBuffer(const AtomBuffer &captureBuf, const AtomBuffer *previewBuf,
        AtomBuffer *targetBuf, const AtomBuffer &localBuf, float zoomFactor) const
{
    LOG1("@%s", __FUNCTION__);
    targetBuf->capture_timestamp = captureBuf.capture_timestamp;
    targetBuf->frameSequenceNbr = captureBuf.frameSequenceNbr;
    targetBuf->id = captureBuf.id;
    targetBuf->frameCounter = mMainDevice->getFrameCount();
    targetBuf->ispPrivate = mSessionId;
    targetBuf->width = localBuf.width;
    targetBuf->height = localBuf.height;
    targetBuf->fourcc = localBuf.fourcc;
    targetBuf->size = localBuf.size;
    targetBuf->bpl = SGXandDisplayBpl(V4L2_PIX_FMT_NV12, localBuf.width);
    targetBuf->type = localBuf.type;
    targetBuf->gfxInfo = localBuf.gfxInfo;
    targetBuf->buff = localBuf.buff;
    targetBuf->dataPtr = localBuf.dataPtr;
    targetBuf->shared = localBuf.shared;

    // optimizations, if right sized frame already exists (captureBuf is not zoomed so memcpy only works for zoomFactor 1.0)
    if (zoomFactor == 1.0f && targetBuf->width == mConfig.HALZSL.width && targetBuf->height == mConfig.HALZSL.height) {
        memcpy(targetBuf->dataPtr, captureBuf.dataPtr, captureBuf.size);
    } else if (targetBuf->width == mConfig.preview.width && targetBuf->height == mConfig.preview.height &&
            previewBuf != NULL) {
        memcpy(targetBuf->dataPtr, previewBuf->dataPtr, previewBuf->size);
    } else
        mScaler->scaleAndZoom(&captureBuf, targetBuf, zoomFactor);
}

status_t AtomISP::getHALZSLSnapshot(AtomBuffer *snapshotBuf, AtomBuffer *postviewBuf)
{
    LOG1("@%s", __FUNCTION__);
    Mutex::Autolock mLock(mHALZSLLock);

    if (!waitForHALZSLBuffer(mHALZSLCaptureBuffers, true)) {
        ALOGE("@%s no capture buffers in FIFO!", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    AtomBuffer captureBuf = mHALZSLCaptureBuffers.itemAt(0);
    LOG1("@%s capture buffer framecounter %d timestamp %ld.%ld", __FUNCTION__, captureBuf.frameCounter, captureBuf.capture_timestamp.tv_sec, captureBuf.capture_timestamp.tv_usec);
    dumpHALZSLPreviewBufs();

    AtomBuffer* matchingPreviewBuf = findMatchingHALZSLPreviewFrame(captureBuf.frameCounter);

    float zoomFactor(static_cast<float>(zoomRatio(mConfig.zoom)) / ZOOM_RATIO);

    // snapshot
    copyOrScaleHALZSLBuffer(captureBuf, matchingPreviewBuf, snapshotBuf, mSnapshotBuffers[0], zoomFactor);
    // postview
    copyOrScaleHALZSLBuffer(captureBuf, matchingPreviewBuf, postviewBuf, mPostviewBuffers[0], zoomFactor);

    return OK;
}

status_t AtomISP::getSnapshot(AtomBuffer *snapshotBuf, AtomBuffer *postviewBuf)
{
    LOG1("@%s", __FUNCTION__);
    struct v4l2_buffer_info vinfo;
    int snapshotIndex, postviewIndex;

    if (mMode != MODE_CAPTURE && mMode != MODE_CONTINUOUS_CAPTURE)
        return INVALID_OPERATION;

    if (mHALZSLEnabled)
        return getHALZSLSnapshot(snapshotBuf, postviewBuf);

    CLEAR(vinfo);

    snapshotIndex = mMainDevice->grabFrame(&vinfo);
    if (snapshotIndex < 0) {
        ALOGE("Error in grabbing frame from 1'st device!");
        return BAD_INDEX;
    }
    LOG1("Device: %d. Grabbed frame of size: %d", V4L2_MAIN_DEVICE, vinfo.vbuffer.bytesused);
    mSnapshotBuffers[snapshotIndex].capture_timestamp = vinfo.vbuffer.timestamp;
    mSnapshotBuffers[snapshotIndex].frameSequenceNbr = vinfo.vbuffer.sequence;
    mSnapshotBuffers[snapshotIndex].status = (FrameBufferStatus)vinfo.vbuffer.reserved;

    if (isDumpRawImageReady()) {
        postviewIndex = snapshotIndex;
        goto nopostview;
    }


    postviewIndex = mPostViewDevice->grabFrame(&vinfo);
    if (postviewIndex < 0) {
        ALOGE("Error in grabbing frame from 2'nd device!");
        // If we failed with the second device, return the frame to the first device
        mMainDevice->putFrame(snapshotIndex);
        return BAD_INDEX;
    }
    LOG1("Device: %d. Grabbed frame of size: %d", V4L2_POSTVIEW_DEVICE, vinfo.vbuffer.bytesused);

    mPostviewBuffers.editItemAt(postviewIndex).capture_timestamp = vinfo.vbuffer.timestamp;
    mPostviewBuffers.editItemAt(postviewIndex).frameSequenceNbr = vinfo.vbuffer.sequence;
    mPostviewBuffers.editItemAt(postviewIndex).status = (FrameBufferStatus)vinfo.vbuffer.reserved;

    if (snapshotIndex != postviewIndex ||
            snapshotIndex >= MAX_V4L2_BUFFERS) {
        ALOGE("Indexes error! snapshotIndex = %d, postviewIndex = %d", snapshotIndex, postviewIndex);
        // Return the buffers back to driver
        mMainDevice->putFrame(snapshotIndex);
        mPostViewDevice->putFrame(postviewIndex);
        return BAD_INDEX;
    }

nopostview:
    mSnapshotBuffers[snapshotIndex].id = snapshotIndex;
    mSnapshotBuffers[snapshotIndex].frameCounter = mMainDevice->getFrameCount();
    mSnapshotBuffers[snapshotIndex].ispPrivate = mSessionId;
    *snapshotBuf = mSnapshotBuffers[snapshotIndex];
    snapshotBuf->width = mConfig.snapshot.width;
    snapshotBuf->height = mConfig.snapshot.height;
    snapshotBuf->fourcc = mConfig.snapshot.fourcc;
    snapshotBuf->size = mConfig.snapshot.size;
    snapshotBuf->bpl = mConfig.snapshot.bpl;

    mPostviewBuffers.editItemAt(postviewIndex).id = postviewIndex;
    mPostviewBuffers.editItemAt(postviewIndex).frameCounter = mPostViewDevice->getFrameCount();
    mPostviewBuffers.editItemAt(postviewIndex).ispPrivate = mSessionId;
    *postviewBuf = mPostviewBuffers[postviewIndex];
    postviewBuf->width = mConfig.postview.width;
    postviewBuf->height = mConfig.postview.height;
    postviewBuf->fourcc = mConfig.postview.fourcc;
    postviewBuf->size = mConfig.postview.size;
    postviewBuf->bpl = mConfig.postview.bpl;

    mNumCapturegBuffersQueued--;

    dumpSnapshot(snapshotIndex, postviewIndex);

    LOG1("@%s buffer id:%d frameCounter:%d frameSequenceNbr:%d", __FUNCTION__,
            snapshotBuf->id, snapshotBuf->frameCounter, snapshotBuf->frameSequenceNbr);
    return NO_ERROR;
}

status_t AtomISP::putSnapshot(AtomBuffer *snapshotBuf, AtomBuffer *postviewBuf)
{
    LOG1("@%s", __FUNCTION__);
    int ret0, ret1;

    if (mMode != MODE_CAPTURE && mMode != MODE_CONTINUOUS_CAPTURE)
        return INVALID_OPERATION;

    if (mHALZSLEnabled)
        return OK;

    if (snapshotBuf->ispPrivate != mSessionId || postviewBuf->ispPrivate != mSessionId)
        return DEAD_OBJECT;

    ret0 = mMainDevice->putFrame(snapshotBuf->id);

    if (mConfig.snapshot.fourcc == mSensorHW.getRawFormat()) {
        // for RAW captures we do not dequeue the postview, therefore we do
        // not need to return it.
        ret1 = 0;
    } else {
        ret1 = mPostViewDevice->putFrame(postviewBuf->id);
    }
    if (ret0 < 0 || ret1 < 0)
        return UNKNOWN_ERROR;

    mNumCapturegBuffersQueued++;

    return NO_ERROR;
}

bool AtomISP::dataAvailable()
{
    LOG2("@%s", __FUNCTION__);

    // For video/recording, make sure isp has a preview and a recording buffer
    if (mMode == MODE_VIDEO)
        return mNumRecordingBuffersQueued > 0 && mNumPreviewBuffersQueued > 0;

    // For capture, just make sure isp has a capture buffer
    if (mMode == MODE_CAPTURE)
        return mNumCapturegBuffersQueued > 0;

    // For preview, just make sure isp has a preview buffer
    if (mMode == MODE_PREVIEW || mMode == MODE_CONTINUOUS_CAPTURE)
        return mNumPreviewBuffersQueued > 0;

    ALOGE("Query for data in invalid mode");

    return false;
}

/**
 * Polls the preview device node fd for data
 *
 * \param timeout time to wait for data (in ms), timeout of -1
 *        means to wait indefinitely for data
 * \return -1 for error, 0 if time out, positive number
 *         if poll was successful
 */
int AtomISP::pollPreview(int timeout)
{
    LOG2("@%s", __FUNCTION__);
    return mPreviewDevice->poll(timeout);
}

/**
 * Polls the capture device node fd for data
 *
 * \param timeout time to wait for data (in ms), timeout of -1
 *        means to wait indefinitely for data
 * \return -1 for error, 0 if time out, positive number
 *         if poll was successful
 */
int AtomISP::pollCapture(int timeout)
{
    LOG2("@%s", __FUNCTION__);

    if (mHALZSLEnabled) {
        if (mHALZSLCaptureBuffers.size() > 0)
            return 1;
    }

    return mMainDevice->poll(timeout);
}

////////////////////////////////////////////////////////////////////
//                          PRIVATE METHODS
////////////////////////////////////////////////////////////////////

/**
 * Compute zoom ratios
 *
 * Compute zoom ratios support by ISP and store them to tables.
 * After compute string format is generated and stored for camera parameters.
 *
 * Calculation is based on following formula:
 * ratio = MaxZoomFactor / (MaxZoomFactor - ZoomDrive)
 */
status_t AtomISP::computeZoomRatios()
{
    LOG1("@%s", __FUNCTION__);
    int maxZoomFactor(PlatformData::getMaxZoomFactor());
    int zoomFactor(maxZoomFactor);
    int stringSize(0);
    int ratio((maxZoomFactor * ZOOM_RATIO + zoomFactor / 2) / zoomFactor);
    int preRatio(0);
    int preZoomFactor(0);

    mZoomRatioTable.clear();
    mZoomDriveTable.clear();
    mZoomRatioTable.setCapacity(maxZoomFactor);
    mZoomDriveTable.setCapacity(maxZoomFactor);

    while (ratio <= MAX_SUPPORT_ZOOM) {
        if (ratio == preRatio) {
            // replace zoom factor, if round error to 2 digit is smaller
            int target = maxZoomFactor * ZOOM_RATIO;
            if (abs(ratio * zoomFactor - target) < abs(ratio * preZoomFactor - target)) {
                mZoomDriveTable.editTop() = maxZoomFactor - zoomFactor;
                preZoomFactor = zoomFactor;
            }
        } else {
            mZoomRatioTable.push(ratio);
            mZoomDriveTable.push(maxZoomFactor - zoomFactor);
            preRatio = ratio;
            preZoomFactor = zoomFactor;

            // calculate stringSize needed also include comma char
            stringSize += 4;
            ratio = ratio / 1000;
            while (ratio) {
                stringSize += 1;
                ratio = ratio / 10;
            }
        }

        zoomFactor = zoomFactor - 1;
        if (zoomFactor == 0)
            break;
        ratio = (maxZoomFactor * ZOOM_RATIO + zoomFactor / 2) / zoomFactor;
    }

    LOG1("@%s: %d zoom ratios (string size = %d)", __FUNCTION__, mZoomRatioTable.size(), stringSize);

    int pos(0);

    if (mZoomRatios != NULL) {
        delete[] mZoomRatios;
        mZoomRatios = NULL;
    }

    mZoomRatios = new char[stringSize];
    if (mZoomRatios == NULL) {
        ALOGE("Error allocation memory for zoom ratios!");
        return NO_MEMORY;
    }

    for (Vector<int>::iterator it = mZoomRatioTable.begin(); it != mZoomRatioTable.end(); ++it)
        pos += snprintf(mZoomRatios + pos, stringSize - pos, "%d,", *it);

    //Overwrite the last ',' with '\0'
    mZoomRatios[stringSize - 1] = '\0';

    LOG2("@%s: zoom ratios list: %s", __FUNCTION__, mZoomRatios);
    return NO_ERROR;
}

/**
 * Marks the given buffers as cached
 *
 * A cached buffer in this context means that the buffer
 * memory may be accessed through some system caches, and
 * the V4L2 driver must do cache invalidation in case
 * the image data source is not updating system caches
 * in hardware.
 *
 * When false (not cached), V4L2 driver can assume no cache
 * invalidation/flushes are needed for this buffer.
 */
void AtomISP::markBufferCached(struct v4l2_buffer_info *vinfo, bool cached)
{
    LOG2("@%s, cached %d", __FUNCTION__, cached);
    uint32_t cacheflags =
        V4L2_BUF_FLAG_NO_CACHE_INVALIDATE |
        V4L2_BUF_FLAG_NO_CACHE_CLEAN;
    if (cached)
        vinfo->cache_flags = 0;
    else
        vinfo->cache_flags = cacheflags;
}


status_t AtomISP::allocateHALZSLBuffers()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = OK;
    void *bufPool[sNumHALZSLBuffers];

    if (mHALZSLBuffers == NULL) {
        mHALZSLBuffers = new AtomBuffer[sNumHALZSLBuffers];

        for (int i = 0; i < sNumHALZSLBuffers; i++) {
            AtomBuffer *buff = &mHALZSLBuffers[i];
            *buff = AtomBufferFactory::createAtomBuffer(); // init fields
            status = MemoryUtils::allocateGraphicBuffer(mHALZSLBuffers[i], mConfig.HALZSL);

            if(status != NO_ERROR) {
                ALOGE("@%s: Failed to allocate GraphicBuffer!", __FUNCTION__);
                break;
            }

            bufPool[i] = buff->dataPtr;
            mScaler->registerBuffer(*buff, ScalerService::SCALER_INPUT);
        }
    }

    if (status != OK && mHALZSLBuffers) {
        delete [] mHALZSLBuffers;
        mHALZSLBuffers = NULL;
    } else {
        mPreviewDevice->setBufferPool((void**)&bufPool, sNumHALZSLBuffers,
                                          &mConfig.HALZSL, false);
    }

    return status;
}

status_t AtomISP::freeHALZSLBuffers()
{
    LOG1("@%s", __FUNCTION__);
    Mutex::Autolock mLock(mHALZSLLock);
    if (mHALZSLBuffers != NULL) {
        for (int i = 0 ; i < sNumHALZSLBuffers; i++) {
            mScaler->unRegisterBuffer(mHALZSLBuffers[i], ScalerService::SCALER_INPUT);
            MemoryUtils::freeAtomBuffer(mHALZSLBuffers[i]);
        }
        delete [] mHALZSLBuffers;
        mHALZSLBuffers = NULL;

        // HALZSL cleanup..
        mHALZSLCaptureBuffers.clear();
    }
    return NO_ERROR;
}

status_t AtomISP::allocatePreviewBuffers()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    void *bufPool[MAX_V4L2_BUFFERS];

    if (mPreviewBuffers.isEmpty()) {
        AtomBuffer tmp = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_PREVIEW); // init fields
        mPreviewBuffersCached = true;

        LOG1("Allocating %d buffers of size %d", mConfig.num_preview_buffers, mConfig.preview.size);
        for (int i = 0; i < mConfig.num_preview_buffers; i++) {
            MemoryUtils::allocateGraphicBuffer(tmp, mConfig.preview);
            if (tmp.dataPtr == NULL) {
                ALOGE("Error allocation memory for preview buffers!");
                status = NO_MEMORY;
                goto errorFree;
            }
            bufPool[i] = tmp.dataPtr;

            if (mHALZSLEnabled) {
                mScaler->registerBuffer(tmp, ScalerService::SCALER_OUTPUT);
                mHALZSLPreviewBuffers.push(tmp);
            }
            mPreviewBuffers.push(tmp);
        }

    } else {
        for (size_t i = 0; i < mPreviewBuffers.size(); i++) {
            bufPool[i] = mPreviewBuffers[i].dataPtr;
            mPreviewBuffers.editItemAt(i).shared = true;
            if (mHALZSLEnabled) {
                mScaler->registerBuffer(mPreviewBuffers.editItemAt(i), ScalerService::SCALER_OUTPUT);
                mHALZSLPreviewBuffers.push(mPreviewBuffers[i]);
            }
        }
    }


    if (mHALZSLEnabled) {
        status = allocateHALZSLBuffers();
        if (status != OK) {
            ALOGE("Error allocation memory for HAL ZSL buffers!");
            goto errorFree;
        }
    } else {
        mPreviewDevice->setBufferPool((void**)&bufPool, mPreviewBuffers.size(),
                                      &mConfig.preview, mPreviewBuffersCached);
    }

    return status;

errorFree:

    freePreviewBuffers();

    return status;
}

status_t AtomISP::allocateRecordingBuffers()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    void *bufPool[MAX_V4L2_BUFFERS];
    int allocatedBufs = 0;
    bool cached = false;

    mRecordingBuffers = new AtomBuffer[mConfig.num_recording_buffers];
    if (!mRecordingBuffers) {
        ALOGE("Not enough mem for recording buffer array");
        status = NO_MEMORY;
        goto errorFree;
    }

    for (int i = 0; i < mConfig.num_recording_buffers; i++) {
        mRecordingBuffers[i] = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_VIDEO); // init fields
#ifdef INTEL_VIDEO_XPROC_SHARING
        /**
         * To share the buffer cross process, it should be MemoryHeap or Graphic buffer.
         * Prefer to use Graphic buffer but can not currently because:
         * 1. Graphic recording buffers are all locked in Camera HAL. It's hard to make it unlock for encoder.
         * 2. Encoder is a bit premature to support graphic buffer well for all platforms.
         * FIXME: Unified use graphic buffer if above case are all resolved.
         */
        MemoryUtils::allocateAtomBuffer(mRecordingBuffers[i], mConfig.recording, mCallbacks);
#else
        //recording buffers use uncached memory
        MemoryUtils::allocateGraphicBuffer(mRecordingBuffers[i], mConfig.recording);
#endif

        LOG1("allocate recording buffer[%d], buff=%p size=%d",
                i, mRecordingBuffers[i].dataPtr, mRecordingBuffers[i].size);
        if (mRecordingBuffers[i].dataPtr == NULL) {
            ALOGE("Error allocation memory for recording buffers!");
            status = NO_MEMORY;
            goto errorFree;
        }
        allocatedBufs++;
        bufPool[i] = mRecordingBuffers[i].dataPtr;
    }
    mRecordingDevice->setBufferPool((void**)&bufPool,mConfig.num_recording_buffers,
                                     &mConfig.recording,cached);
    return status;

errorFree:
    // On error, free the allocated buffers
    for (int i = 0 ; i < allocatedBufs; i++)
        MemoryUtils::freeAtomBuffer(mRecordingBuffers[i]);

    if (mRecordingBuffers != NULL) {
        delete[] mRecordingBuffers;
        mRecordingBuffers = NULL;
    }
    return status;
}

/**
 * Prepares V4L2  buffer info's for snapshot and postview buffers
 *
 * The snapshot and postview buffers are allocated by the client,
 * so there is no allocation done here, but only the preparation
 * of the v4l2 buffer pools.
 */
status_t AtomISP::allocateSnapshotBuffers()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    int allocatedSnaphotBufs = 0;
    int snapshotSize = mConfig.snapshot.size;
    void *bufPool[MAX_V4L2_BUFFERS];

    if (mUsingClientSnapshotBuffers) {
        LOG1("@%s using %d client snapshot buffers",__FUNCTION__, mConfig.num_snapshot);
        for (int i = 0; i < mConfig.num_snapshot; i++) {
            mSnapshotBuffers[i].type = ATOM_BUFFER_SNAPSHOT;
            mSnapshotBuffers[i].shared = false;
            if (!mHALZSLEnabled) {
                bufPool[i] = mSnapshotBuffers[i].dataPtr;
            }
        }
    } else {
        freeSnapshotBuffers();

        LOG1("@%s Allocating %d buffers of size: %d (snapshot), %d (postview)",
                __FUNCTION__,
                mConfig.num_snapshot,
                snapshotSize,
                mConfig.postview.size);
        for (int i = 0; i < mConfig.num_snapshot; i++) {
            mSnapshotBuffers[i] = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_SNAPSHOT);

            MemoryUtils::allocateAtomBuffer(mSnapshotBuffers[i], mConfig.snapshot, mCallbacks);
            if (mSnapshotBuffers[i].dataPtr == NULL) {
                ALOGE("Error allocation memory for snapshot buffers!");
                status = NO_MEMORY;
                goto errorFree;
            }
            allocatedSnaphotBufs++;
            bufPool[i] = mSnapshotBuffers[i].dataPtr;
        }
    } // if (mUsingClientSnapshotBuffers)
    if (!mHALZSLEnabled)
        mMainDevice->setBufferPool((void**)&bufPool,mConfig.num_snapshot,
                                   &mConfig.snapshot, mClientSnapshotBuffersCached);

    if (mUsingClientPostviewBuffers) {
        LOG1("@%s using %d client postview buffers",__FUNCTION__, mConfig.num_postviews);
        for (int i = 0; i < mConfig.num_postviews; ++i) {
            mPostviewBuffers.editItemAt(i).type = ATOM_BUFFER_POSTVIEW;
            mPostviewBuffers.editItemAt(i).shared = false;

            if (!mHALZSLEnabled) {
                bufPool[i] = mPostviewBuffers[i].dataPtr;
            }
        }
    } else if (needNewPostviewBuffers()) {
        // TODO: Remove this allocation stuff, it is done in PictureThread now...
        freePostviewBuffers();
        AtomBuffer postv = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_POSTVIEW);
        for (int i = 0; i < mConfig.num_snapshot; i++) {
            postv.buff = NULL;
            postv.size = 0;
            postv.dataPtr = NULL;
            if (mHALZSLEnabled) {
                MemoryUtils::allocateGraphicBuffer(postv, mConfig.postview);
            } else {
                MemoryUtils::allocateAtomBuffer(postv, mConfig.postview, mCallbacks);
            }

            if (postv.dataPtr == NULL) {
                ALOGE("Error allocation memory for postview buffers!");
                status = NO_MEMORY;
                goto errorFree;
            }

            bufPool[i] = postv.dataPtr;

            postv.shared = false;
            if (mHALZSLEnabled)
                mScaler->registerBuffer(postv, ScalerService::SCALER_OUTPUT);

            mPostviewBuffers.push(postv);
        }
    } else {
        for (size_t i = 0; i < mPostviewBuffers.size(); i++) {
            bufPool[i] = mPostviewBuffers[i].dataPtr;
        }
    }
    // In case of Raw capture we do not get postview, so no point in setting up the pool
    if (!mHALZSLEnabled && !isBayerFormat(mConfig.snapshot.fourcc))
        mPostViewDevice->setBufferPool((void**)&bufPool,mPostviewBuffers.size(),
                                        &mConfig.postview, true);
    return status;

errorFree:
    // On error, free the allocated buffers
    for (int i = 0 ; i < allocatedSnaphotBufs; i++)
        MemoryUtils::freeAtomBuffer(mSnapshotBuffers[i]);

    freePostviewBuffers();
    return status;
}

#ifdef ENABLE_INTEL_METABUFFER
void AtomISP::initMetaDataBuf(IntelMetadataBuffer* metaDatabuf)
{
    ValueInfo* vinfo = new ValueInfo;
    // Video buffers actually use cached memory,
    // even though uncached memory was requested
    vinfo->mode = MEM_MODE_MALLOC;
    vinfo->handle = 0;
    vinfo->width = mConfig.recording.width;
    vinfo->height = mConfig.recording.height;
    vinfo->size = mConfig.recording.size;
    // NOTE: We assume here that ValueInfo defines stride in pixels
    //       While format is also fixed to NV12 below, we are safe.
    vinfo->lumaStride = bytesToPixels(mConfig.recording.fourcc, mConfig.recording.bpl);
    vinfo->chromStride = vinfo->chromStride;
    LOG2("weight:%d  height:%d size:%d bpl:%d ", vinfo->width,
          vinfo->height, vinfo->size, vinfo->lumaStride);
    vinfo->format = STRING_TO_FOURCC("NV12");
    vinfo->s3dformat = 0xFFFFFFFF;
    metaDatabuf->SetValueInfo(vinfo);
    metaDatabuf->SetType(IntelMetadataBufferTypeCameraSource);
    delete vinfo;
    vinfo = NULL;

}
#endif

/**
 * Have to copy this private class here because we want to access the MemoryBase object
 * which is hiding in the handle of camera_memory_t
 * It's for camera recording to share MemoryHeap buffer only.
 * FIXME: remove it if we are able to share graphic buffer in the fulture
 */
class CameraHeapMemory : public RefBase {
public:
    CameraHeapMemory(int fd, size_t buf_size, uint_t num_buffers = 1) :
        mBufSize(buf_size),
        mNumBufs(num_buffers) {}

    CameraHeapMemory(size_t buf_size, uint_t num_buffers = 1) :
        mBufSize(buf_size),
        mNumBufs(num_buffers) {}

    void commonInitialization() {}

    virtual ~CameraHeapMemory() {}

    size_t mBufSize;
    uint_t mNumBufs;
    sp<MemoryHeapBase> mHeap;
    sp<MemoryBase> *mBuffers;
    camera_memory_t handle;
};

status_t AtomISP::allocateMetaDataBuffers()
{
    LOG1("@%s", __FUNCTION__);

    status_t status = NO_ERROR;
#ifdef ENABLE_INTEL_METABUFFER
    int allocatedBufs = 0;
    uint8_t* meta_data_prt;
    uint32_t meta_data_size;
    IntelMetadataBuffer* metaDataBuf = NULL;

    if(mRecordingBuffers) {
        for (int i = 0 ; i < mConfig.num_recording_buffers; i++) {
            MemoryUtils::freeAtomBufferMetadata(mRecordingBuffers[i]);
#ifdef INTEL_VIDEO_XPROC_SHARING
            IntelMetadataBuffer::ClearContext(mBufferSharingSessionID, true);
#endif
        }
    } else {
        // mRecordingBuffers is not ready, so it's invalid to allocate metadata buffers
        return INVALID_OPERATION;
    }

    for (int i = 0; i < mConfig.num_recording_buffers; i++) {
        metaDataBuf = new IntelMetadataBuffer();
        if(metaDataBuf) {
            if (PlatformData::isGraphicGen()) {
                metaDataBuf->SetType(IntelMetadataBufferTypeGrallocSource);
                metaDataBuf->SetValue((uint32_t)*mRecordingBuffers[i].gfxInfo_rec.gfxBufferHandle);
            } else {
                initMetaDataBuf(metaDataBuf);
#ifdef INTEL_VIDEO_XPROC_SHARING
                // for cross-process sharing
                metaDataBuf->SetSessionFlag(mBufferSharingSessionID);
                sp<CameraHeapMemory> mem(static_cast<CameraHeapMemory *>(mRecordingBuffers[i].buff->handle));
                metaDataBuf->ShareValue(mem->mBuffers[0]);
#else
                // for video recording only
                metaDataBuf->SetValue((uint32_t)mRecordingBuffers[i].dataPtr);
#endif
            }
            metaDataBuf->Serialize(meta_data_prt, meta_data_size);
            MemoryUtils::allocateAtomBufferMetadata(mRecordingBuffers[i], meta_data_size, mCallbacks);
            LOG1("allocate metadata buffer[%d]  buff=%p size=%d sID:%d",
                i, mRecordingBuffers[i].metadata_buff->data,
                mRecordingBuffers[i].metadata_buff->size, mBufferSharingSessionID);
            if (mRecordingBuffers[i].metadata_buff == NULL) {
                ALOGE("Error allocation memory for metadata buffers!");
                status = NO_MEMORY;
                goto errorFree;
            }
            memcpy(mRecordingBuffers[i].metadata_buff->data, meta_data_prt, meta_data_size);
            allocatedBufs++;

            delete metaDataBuf;
            metaDataBuf = NULL;
        } else {
            ALOGE("Error allocation memory for metaDataBuf!");
            status = NO_MEMORY;
            goto errorFree;
        }
    }
    return status;

errorFree:
    // On error, free the allocated buffers
    for (int i = 0 ; i < allocatedBufs; i++)
        MemoryUtils::freeAtomBuffer(mRecordingBuffers[i]);

    if (metaDataBuf) {
        delete metaDataBuf;
        metaDataBuf = NULL;
    }
#endif
    return status;
}

status_t AtomISP::freePreviewBuffers()
{
    LOG1("@%s", __FUNCTION__);

    if (!mPreviewBuffers.isEmpty()) {
        for (size_t i = 0 ; i < mPreviewBuffers.size(); i++) {
            LOG1("@%s mHALZSLEnabled = %d i=%d, mNum = %d", __FUNCTION__, mHALZSLEnabled, i, mConfig.num_preview_buffers);
            if (mHALZSLEnabled)
                mScaler->unRegisterBuffer(mPreviewBuffers.editItemAt(i), ScalerService::SCALER_OUTPUT);

            MemoryUtils::freeAtomBuffer(mPreviewBuffers.editItemAt(i));
        }
        mPreviewBuffers.clear();
    }

    if (mHALZSLEnabled) {
        freeHALZSLBuffers();
        mHALZSLPreviewBuffers.clear();
    }

    return NO_ERROR;
}

status_t AtomISP::freeRecordingBuffers()
{
    LOG1("@%s", __FUNCTION__);
    if(mRecordingBuffers != NULL) {
        for (int i = 0 ; i < mConfig.num_recording_buffers; i++)
            MemoryUtils::freeAtomBuffer(mRecordingBuffers[i]);

#ifdef INTEL_VIDEO_XPROC_SHARING
        if (mStoreMetaDataInBuffers)
            IntelMetadataBuffer::ClearContext(mBufferSharingSessionID, true);
#endif

        delete[] mRecordingBuffers;
        mRecordingBuffers = NULL;
    }
    return NO_ERROR;
}

status_t AtomISP::freeSnapshotBuffers()
{
    LOG1("@%s", __FUNCTION__);
    if (mUsingClientSnapshotBuffers) {
        LOG1("Using external Snapshotbuffers, nothing to free");
        return NO_ERROR;
    }

    for (int i = 0 ; i < mConfig.num_snapshot; i++)
        MemoryUtils::freeAtomBuffer(mSnapshotBuffers[i]);

    return NO_ERROR;
}

status_t AtomISP::freePostviewBuffers()
{
    LOG1("@%s", __FUNCTION__);

   if (mUsingClientPostviewBuffers) {
        LOG1("Using client\'s postview buffers, nothing to free");
        return NO_ERROR;
    }

    LOG1("@%s: freeing %d", __FUNCTION__, mPostviewBuffers.size());
    for (int i = 0 ; i < mConfig.num_postviews; i++) {
        AtomBuffer &buffer = mPostviewBuffers.editItemAt(i);
        if (buffer.gfxInfo.scalerId != -1)
            mScaler->unRegisterBuffer(buffer, ScalerService::SCALER_OUTPUT);

        MemoryUtils::freeAtomBuffer(buffer);
    }

    mPostviewBuffers.clear();

    return NO_ERROR;
}

/**
 * Helper method used during allocateSnapshot buffers
 * It is used to detect whether we need to re-allocate the postview buffers
 * or not.
 *
 * It checks two things:
 *  - First the number of allocated buffers in mPostviewBuffers[]
 *     if this differs from  mConfig.num_snapshot it returns true
 *
 *  - Second it checks the size of the first buffer is the same as the one
 *    in the configuration. If it is then it returns false, indicating that we
 *    do not need to re-allocate new postviews
 *
 */
bool AtomISP::needNewPostviewBuffers()
{
    if (mHALZSLEnabled)
        return true;

    if ((mPostviewBuffers.size() != (unsigned int)mConfig.num_snapshot) ||
         mPostviewBuffers.isEmpty())
        return true;

    if (mPostviewBuffers[0].size == mConfig.postview.size)
        return false;

    return true;
}

unsigned int AtomISP::getNumOfSkipFrames(void)
{
    int ret = 0;
    int num_skipframes = 0;

    ret = mMainDevice->getControl(V4L2_CID_G_SKIP_FRAMES, &num_skipframes);
    if (ret < 0) {
        ALOGD("Failed to query control V4L2_CID_G_SKIP_FRAMES!");
        num_skipframes = 0;
    } else if (num_skipframes < 0) {
        ALOGD("Negative value for V4L2_CID_G_SKIP_FRAMES!");
        num_skipframes = 0;
    }
    LOG1("%s: skipping %d initial frames", __FUNCTION__, num_skipframes);
    return (unsigned int)num_skipframes;
}

unsigned int AtomISP::getNumOfSkipStatistics(void)
{
    int num_skipstats = 0;
    PlatformData::HalConfig.getValue(num_skipstats, CPF::Statistics, CPF::InitialSkip);

    LOG1("%s: skipping %d initial statistics", __FUNCTION__, num_skipstats);
    return (unsigned int)num_skipstats;
}


/* ===================  ACCELERATION API EXTENSIONS ====================== */
/*
* Loads the acceleration firmware to ISP. Calls the appropriate
* Driver IOCTL calls. Driver checks the validity of the firmware
* and fills the "fw_handle"
*/
int AtomISP::loadAccFirmware(void *fw, size_t size,
                             unsigned int *fwHandle)
{
    LOG1("@%s\n", __FUNCTION__);
    int ret = -1;

    //Load the IOCTL struct
    atomisp_acc_fw_load fwData;
    fwData.size = size;
    fwData.fw_handle = 0;
    fwData.data = fw;
    LOG2("fwData : 0x%x fwData->data : 0x%x",
        (unsigned int)&fwData, (unsigned int)fwData.data );


    ret = mMainDevice->xioctl(ATOMISP_IOC_ACC_LOAD, &fwData);
    LOG1("%s IOCTL ATOMISP_IOC_ACC_LOAD ret : %d fwData->fw_handle: %d \n"\
            , __FUNCTION__, ret, fwData.fw_handle);


    //If IOCTRL call was returned successfully, get the firmware handle
    //from the structure and return it to the application.
    if(!ret){
        *fwHandle = fwData.fw_handle;
        LOG1("%s IOCTL Call returned : %d Handle: %ud\n",
                __FUNCTION__, ret, *fwHandle );
    }

    return ret;
}

int AtomISP::loadAccPipeFirmware(void *fw, size_t size,
                             unsigned int *fwHandle)
{
    LOG1("@%s", __FUNCTION__);
    int ret = -1;

    struct atomisp_acc_fw_load_to_pipe fwDataPipe;
    memset(&fwDataPipe, 0, sizeof(fwDataPipe));
    fwDataPipe.flags = ATOMISP_ACC_FW_LOAD_FL_PREVIEW;
    fwDataPipe.type = ATOMISP_ACC_FW_LOAD_TYPE_VIEWFINDER;

    /*  fwDataPipe.fw_handle filled by kernel and returned to caller */
    fwDataPipe.size = size;
    fwDataPipe.data = fw;

    ret = mMainDevice->xioctl(ATOMISP_IOC_ACC_LOAD_TO_PIPE, &fwDataPipe);
    LOG1("%s IOCTL ATOMISP_IOC_ACC_LOAD_TO_PIPE ret : %d fwDataPipe->fw_handle: %d"\
            , __FUNCTION__, ret, fwDataPipe.fw_handle);

    //If IOCTRL call was returned successfully, get the firmware handle
    //from the structure and return it to the application.
    if(!ret){
        *fwHandle = fwDataPipe.fw_handle;
        LOG1("%s IOCTL Call returned : %d Handle: %ud",
                __FUNCTION__, ret, *fwHandle );
    }

    return ret;
}

/*
 * Unloads the acceleration firmware from ISP.
 * Atomisp driver checks the validity of the handles and schedules
 * unloading the firmware on the current frame complete. After this
 * call handle is not valid any more.
 */
int AtomISP::unloadAccFirmware(unsigned int fwHandle)
{
    LOG1("@ %s fw_Handle: %d\n",__FUNCTION__, fwHandle);
    int ret = -1;

    ret = mMainDevice->xioctl(ATOMISP_IOC_ACC_UNLOAD, &fwHandle);
    LOG1("%s IOCTL ATOMISP_IOC_ACC_UNLOAD ret: %d \n", __FUNCTION__,ret);

    return ret;
}

int AtomISP::mapFirmwareArgument(void *val, size_t size, unsigned long *ptr)
{
    int ret = -1;
    struct atomisp_acc_map map;

    memset(&map, 0, sizeof(map));

    map.length = size;
    map.user_ptr = val;

    ret =  mMainDevice->xioctl(ATOMISP_IOC_ACC_MAP, &map);
    LOG1("%s ATOMISP_IOC_ACC_MAP ret: %d\n", __FUNCTION__, ret);

    *ptr = map.css_ptr;

    return ret;
}

int AtomISP::unmapFirmwareArgument(unsigned long val, size_t size)
{
    int ret = -1;
    struct atomisp_acc_map map;

    memset(&map, 0, sizeof(map));

    map.css_ptr = val;
    map.length = size;

    ret =  mMainDevice->xioctl(ATOMISP_IOC_ACC_UNMAP, &map);
    LOG1("%s ATOMISP_IOC_ACC_UNMAP ret: %d\n", __FUNCTION__, ret);

    return ret;
}

/*
 * Sets the arguments for the firmware loaded.
 * The loaded firmware is identified with the firmware handle.
 * Atomisp driver checks the validity of the handle.
 */
int AtomISP::setFirmwareArgument(unsigned int fwHandle, unsigned int num,
                                 void *val, size_t size)
{
    LOG1("@ %s fwHandle:%d\n", __FUNCTION__, fwHandle);
    int ret = -1;

    atomisp_acc_fw_arg arg;
    arg.fw_handle = fwHandle;
    arg.index = num;
    arg.value = val;
    arg.size = size;

    ret = mMainDevice->xioctl(ATOMISP_IOC_ACC_S_ARG, &arg);
    LOG1("%s IOCTL ATOMISP_IOC_ACC_S_ARG ret: %d \n", __FUNCTION__, ret);

    return ret;
}

int AtomISP::setMappedFirmwareArgument(unsigned int fwHandle, unsigned int mem,
                                       unsigned long val, size_t size)
{
    int ret = -1;
    struct atomisp_acc_s_mapped_arg arg;

    memset(&arg, 0, sizeof(arg));

    arg.fw_handle = fwHandle;
    arg.memory = mem;
    arg.css_ptr = val;
    arg.length = size;

    ret =  mMainDevice->xioctl(ATOMISP_IOC_ACC_S_MAPPED_ARG, &arg);
    LOG1("%s IOCTL ATOMISP_IOC_ACC_S_MAPPED_ARG ret: %d \n", __FUNCTION__, ret);

    return ret;
}

/*
 * For a stable argument, mark it is destabilized, i.e. flush it
 * was changed from user space and needs flushing from the cache
 * to provide CSS access to it.
 * The loaded firmware is identified with the firmware handle.
 * Atomisp driver checks the validity of the handle.
 */
int AtomISP::unsetFirmwareArgument(unsigned int fwHandle, unsigned int num)
{
    LOG1("@ %s fwHandle:%d", __FUNCTION__, fwHandle);
    int ret = -1;

    atomisp_acc_fw_arg arg;
    arg.fw_handle = fwHandle;
    arg.index = num;
    arg.value = NULL;
    arg.size = 0;

    ret = mMainDevice->xioctl(ATOMISP_IOC_ACC_DESTAB, &arg);
    LOG1("%s IOCTL ATOMISP_IOC_ACC_DESTAB ret: %d \n", __FUNCTION__, ret);

    return ret;
}

int AtomISP::startFirmware(unsigned int fwHandle)
{
    int ret;
    ret = mMainDevice->xioctl(ATOMISP_IOC_ACC_START, &fwHandle);
    LOG1("%s IOCTL ATOMISP_IOC_ACC_START ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int AtomISP::waitForFirmware(unsigned int fwHandle)
{
    int ret;
    ret = mMainDevice->xioctl(ATOMISP_IOC_ACC_WAIT, &fwHandle);
    LOG1("%s IOCTL ATOMISP_IOC_ACC_WAIT ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int AtomISP::abortFirmware(unsigned int fwHandle, unsigned int timeout)
{
    int ret;
    atomisp_acc_fw_abort abort;

    abort.fw_handle = fwHandle;
    abort.timeout = timeout;

    ret = mMainDevice->xioctl(ATOMISP_IOC_ACC_ABORT, &abort);
    LOG1("%s IOCTL ATOMISP_IOC_ACC_ABORT ret: %d\n", __FUNCTION__, ret);
    return ret;
}

status_t AtomISP::storeMetaDataInBuffers(bool enabled, int sID)
{
    LOG1("@%s: enabled = %d", __FUNCTION__, enabled);
    status_t status = NO_ERROR;
    mStoreMetaDataInBuffers = enabled;
    mBufferSharingSessionID = (sID == -1)? DEFAULT_BUFFER_SHARING_SESSION_ID : sID;

    /**
     * if we are not in video mode we just store the value
     * it will be used during preview start
     * if we are in video mode we can allocate the buffers
     * now and start using them
     */
    if (mStoreMetaDataInBuffers && mMode == MODE_VIDEO) {
      if ((status = allocateMetaDataBuffers()) != NO_ERROR)
          goto exitFreeRec;
    }
    return status;

exitFreeRec:
    ALOGE("Error allocating metadata buffers!");
    if(mRecordingBuffers) {
        for (int i = 0 ; i < mConfig.num_recording_buffers; i++) {
            MemoryUtils::freeAtomBufferMetadata(mRecordingBuffers[i]);
#ifdef INTEL_VIDEO_XPROC_SHARING
            IntelMetadataBuffer::ClearContext(mBufferSharingSessionID, true);
#endif
        }
    }
    return status;
}


int AtomISP::dumpFrameInfo(AtomMode mode)
{
    LOG2("@%s", __FUNCTION__);

    if (gLogLevel & CAMERA_DEBUG_LOG_PERF_TRACES) {
        const char *previewMode[5]={"NoPreview",
                                    "Preview",
                                    "Capture",
                                    "Video",
                                    "ContinuousCapture"};

        ALOGD("FrameInfo: PreviewMode: %s", previewMode[mode+1]);
        ALOGD("FrameInfo: previewSize: %dx%d, bpl: %d",
               mConfig.preview.width, mConfig.preview.height, mConfig.preview.bpl);
        ALOGD("FrameInfo: PostviewSize: %dx%d, bpl: %d",
               mConfig.postview.width, mConfig.postview.height, mConfig.postview.bpl);
        ALOGD("FrameInfo: PictureSize: %dx%d, bpl: %d",
               mConfig.snapshot.width, mConfig.snapshot.height, mConfig.snapshot.bpl);
        ALOGD("FrameInfo: videoSize: %dx%d, bpl: %d",
                mConfig.recording.width, mConfig.recording.height, mConfig.recording.bpl);
    }

    return 0;
}

int AtomISP::dumpPreviewFrame(int previewIndex)
{
    LOG2("@%s", __FUNCTION__);

    if (CameraDump::isDumpImageEnable(CAMERA_DEBUG_DUMP_PREVIEW)) {

        CameraDump *cameraDump = CameraDump::getInstance(mCameraId);

        camera_delay_dumpImage_T dump;
        dump.buffer_raw = mPreviewBuffers[previewIndex].dataPtr;
        dump.buffer_size =  mConfig.preview.size;
        dump.width =  mConfig.preview.width;
        dump.height = mConfig.preview.height;
        dump.bpl = mConfig.preview.bpl;
        if (mMode == MODE_VIDEO)
            cameraDump->dumpImage2File(&dump, DUMPIMAGE_RECORD_PREVIEW_FILENAME);
        else
            cameraDump->dumpImage2File(&dump, DUMPIMAGE_PREVIEW_FILENAME);
    }

    return 0;
}

int AtomISP::dumpRecordingFrame(int recordingIndex)
{
    LOG2("@%s", __FUNCTION__);
    if (CameraDump::isDumpImageEnable(CAMERA_DEBUG_DUMP_VIDEO)) {

        CameraDump *cameraDump = CameraDump::getInstance(mCameraId);

        camera_delay_dumpImage_T dump;
        dump.buffer_raw = mRecordingBuffers[recordingIndex].dataPtr;
        dump.buffer_size =  mConfig.recording.size;
        dump.width =  mConfig.recording.width;
        dump.height = mConfig.recording.height;
        dump.bpl = mConfig.recording.bpl;
        const char *name = DUMPIMAGE_RECORD_STORE_FILENAME;
        cameraDump->dumpImage2File(&dump, name);
    }

    return 0;
}

int AtomISP::dumpSnapshot(int snapshotIndex, int postviewIndex)
{
    LOG1("@%s", __FUNCTION__);
    if (CameraDump::isDumpImageEnable()) {
        camera_delay_dumpImage_T dump;
        CameraDump *cameraDump = CameraDump::getInstance(mCameraId);
        if (CameraDump::isDumpImageEnable(CAMERA_DEBUG_DUMP_SNAPSHOT)) {

            const char *name0 = "snap_v0.nv12";
            const char *name1 = "snap_v1.nv12";
            dump.buffer_raw = mSnapshotBuffers[snapshotIndex].dataPtr;
            dump.buffer_size = mSnapshotBuffers[snapshotIndex].size;
            dump.width = mSnapshotBuffers[snapshotIndex].width;
            dump.height = mSnapshotBuffers[snapshotIndex].height;
            dump.bpl = mSnapshotBuffers[snapshotIndex].bpl;
            cameraDump->dumpImage2File(&dump, name0);
            dump.buffer_raw = mPostviewBuffers[postviewIndex].dataPtr;
            dump.buffer_size = mConfig.postview.size;
            dump.width = mConfig.postview.width;
            dump.height = mConfig.postview.height;
            dump.bpl = mConfig.postview.bpl;
            cameraDump->dumpImage2File(&dump, name1);
        }

        if (CameraDump::isDumpImageEnable(CAMERA_DEBUG_DUMP_YUV) ||
             isDumpRawImageReady()  ) {
            dump.buffer_raw = mSnapshotBuffers[snapshotIndex].dataPtr;
            dump.buffer_size = mSnapshotBuffers[snapshotIndex].size;
            dump.width = mSnapshotBuffers[snapshotIndex].width;
            dump.height = mSnapshotBuffers[snapshotIndex].height;
            dump.bpl = mSnapshotBuffers[snapshotIndex].bpl;
            cameraDump->dumpImage2Buf(&dump);
        }

    }

    return 0;
}

int AtomISP::dumpRawImageFlush(void)
{
    LOG1("@%s", __FUNCTION__);
    if (CameraDump::isDumpImageEnable()) {
        CameraDump *cameraDump = CameraDump::getInstance(mCameraId);
        cameraDump->dumpImage2FileFlush();
    }
    return 0;
}

bool AtomISP::isDumpRawImageReady(void)
{

    bool ret = (mSensorType == SENSOR_TYPE_RAW) && CameraDump::isDumpImageEnable(CAMERA_DEBUG_DUMP_RAW);
    LOG1("@%s: %s", __FUNCTION__,ret?"Yes":"No");
    return ret;
}

int AtomISP::moveFocusToPosition(int position)
{
    LOG2("@%s", __FUNCTION__);

    // TODO: this code will be removed when the CPF file is valid for saltbay in the future
    if ((strcmp(PlatformData::getBoardName(), "saltbay") == 0) ||
        (strcmp(PlatformData::getBoardName(), "baylake") == 0)) {
        position = 1024 - position;
    }

    LOG2("@%s: V4L2_CID_FOCUS_ABSOLUTE = %d", __FUNCTION__, position);
    return mMainDevice->setControl(V4L2_CID_FOCUS_ABSOLUTE, position, "Set focus position");
}

int AtomISP::moveFocusToBySteps(int steps)
{
    int val = 0, rval;
    LOG2("@%s", __FUNCTION__);
    rval = mMainDevice->getControl(V4L2_CID_FOCUS_ABSOLUTE, &val);
    if (rval)
        return rval;
    return moveFocusToPosition(val + steps);
}

int AtomISP::getFocusPosition(int * position)
{
    LOG2("@%s", __FUNCTION__);
    return mMainDevice->getControl(V4L2_CID_FOCUS_ABSOLUTE , position);
}

int AtomISP::getFocusStatus(int *status)
{
    LOG2("@%s", __FUNCTION__);
    return mMainDevice->getControl(V4L2_CID_FOCUS_STATUS, status);
}

void AtomISP::getSensorDataFromFile(const char *file_name, sensorPrivateData *sensor_data)
{
    LOG2("@%s", __FUNCTION__);
    int otp_fd = -1;
    struct stat st;
    struct v4l2_private_int_data otpdata;

    otpdata.size = 0;
    otpdata.data = NULL;
    otpdata.reserved[0] = 0;
    otpdata.reserved[1] = 0;

    sensor_data->data = NULL;
    sensor_data->size = 0;

    /* Open the otp data file */
    if ((otp_fd = open(file_name, O_RDONLY)) == -1) {
        ALOGE("ERR(%s): Failed to open %s\n", __func__, file_name);
        return;
    }

    memset(&st, 0, sizeof (st));
    if (fstat(otp_fd, &st) < 0) {
        ALOGE("ERR(%s): fstat %s failed\n", __func__, file_name);
        close(otp_fd);
        return;
    }

    otpdata.size = st.st_size;
    otpdata.data = malloc(otpdata.size);
    if (otpdata.data == NULL) {
        ALOGD("Failed to allocate memory for OTP data.");
        close(otp_fd);
        return;
    }

    if ( (read(otp_fd, otpdata.data, otpdata.size)) == -1) {
        ALOGD("Failed to read OTP data\n");
        free(otpdata.data);
        close(otp_fd);
        return;
    }

    sensor_data->data = otpdata.data;
    sensor_data->size = otpdata.size;
    sensor_data->fetched = true;
    close(otp_fd);
}

int AtomISP::setAicParameter(struct atomisp_parameters *aic_param)
{
    LOG2("@%s", __FUNCTION__);
    int ret;

    if (mNoiseReductionEdgeEnhancement == false) {
        //Disable the Noise Reduction and Edge Enhancement
        memset(aic_param->ee_config, 0, sizeof(struct atomisp_ee_config));
        memset(aic_param->nr_config, 0, sizeof(struct atomisp_nr_config));
        memset(aic_param->de_config, 0, sizeof(struct atomisp_de_config));
        aic_param->ee_config->threshold = 65535;
        LOG2("Disabled NREE in 3A");
    }

    if (aic_param->wb_config == NULL) {
        LOG2("@%s: wb is NULL", __FUNCTION__);
    } else {
        LOG2("@%s: wb integer_bits=%u gr=%u r=%u b=%u gb=%u",
             __FUNCTION__, aic_param->wb_config->integer_bits,
             aic_param->wb_config->gr, aic_param->wb_config->r,
             aic_param->wb_config->b, aic_param->wb_config->gb);
    }

    ret = mMainDevice->xioctl(ATOMISP_IOC_S_PARAMETERS, aic_param);
    LOG2("%s IOCTL ATOMISP_IOC_S_PARAMETERS ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int AtomISP::setIspParameter(struct atomisp_parm *isp_param)
{
    LOG2("@%s", __FUNCTION__);
    int ret;
    ret = mMainDevice->xioctl(ATOMISP_IOC_S_ISP_PARM, isp_param);
    LOG2("%s IOCTL ATOMISP_IOC_S_ISP_PARM ret: %d\n", __FUNCTION__, ret);
    return ret;
}

/**
 * Retreive 3A statistics from driver
 */
int AtomISP::getIspStatistics(struct atomisp_3a_statistics *statistics)
{
    LOG2("@%s", __FUNCTION__);

    int ret = 0;
    ret = mMainDevice->xioctl(ATOMISP_IOC_G_3A_STAT, statistics);
    LOG2("%s IOCTL ATOMISP_IOC_G_3A_STAT ret: %d\n", __FUNCTION__, ret);

    if (ret == 0 && isOfflineCaptureRunning()) {
        // Detect the corrupt stats only for offline (continous) capture.
        // TODO: This hack to be removed, when BZ #119181 is fixed
        ret = detectCorruptStatistics(statistics);
    }

    return ret;
}

//
// TODO: Remove this function once BZ #119181 gets fixed by the firmware team!
//
int AtomISP::detectCorruptStatistics(struct atomisp_3a_statistics *statistics)
{
    LOG2("@%s", __FUNCTION__);
    int ret = 0;

    static long long int prev_ae_y = 0;
    static unsigned corruptCounter = 0;
    long long int ae_y = 0;
    double ae_ref;          /* Scene reflectance(18% ... center 144% ... saturated) */
    // Constants for adjusting the "algorithm" behavior:
    const int CORRUPT_STATS_DIFF_THRESHOLD = 200000;
    const unsigned int CORRUPT_STATS_RETRY_THRESHOLD = 2;

    for (unsigned int i = 0; i < statistics->grid_info.s3a_width*statistics->grid_info.s3a_height; ++i) {
        ae_y += statistics->data[i].ae_y;
    }

    ae_y /= (statistics->grid_info.s3a_width * statistics->grid_info.s3a_height);
    ae_ref = 1.0 * ae_y / (statistics->grid_info.s3a_bqs_per_grid_cell * statistics->grid_info.s3a_bqs_per_grid_cell);
    ae_ref = ae_ref * 144 / (1<<13);

    LOG2("AEStatistics (Ref:%3.1f Per Ave:%lld)", ae_ref, ae_y);
    LOG2("flash used: %d", mFlashIsOn);

    // Drop the stats, if decided that they are corrupt. This method is now a heuristic one,
    // based on the decision what we consider corrupt. Threshold can be adjusted.
    if (prev_ae_y != 0 && llabs(prev_ae_y - ae_y) > CORRUPT_STATS_DIFF_THRESHOLD &&
        !mFlashIsOn) {
        // We have already got first stats (prev_ae_y != 0), consider the stats over the
        // threshold corrupt.
        // NOTE: we must not drop stats during pre-flash.
        LOG2("@%s: \"corrupt\" stats.", __FUNCTION__);
        ++corruptCounter;
        ret = -1;
    } else if (corruptCounter >= CORRUPT_STATS_RETRY_THRESHOLD) {
        // clear counter once in a while, so we won't always do corrupt detection
        // and allow some statistics to be passed,
        // caller will re-run this function and try to refetch statistics upon EAGAIN
        ae_y = 0;
        corruptCounter = 0;
        ret = EAGAIN;
        LOG2("@%s: return EAGAIN", __FUNCTION__);
    }

    // Save the recent statistics
    prev_ae_y = ae_y;

    return ret;
}

int AtomISP::setMaccConfig(struct atomisp_macc_config *macc_tbl)
{
    LOG2("@%s", __FUNCTION__);
    int ret;
    ret = mMainDevice->xioctl(ATOMISP_IOC_S_ISP_MACC,macc_tbl);
    LOG2("%s IOCTL ATOMISP_IOC_S_ISP_MACC ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int AtomISP::setGammaTable(const struct atomisp_gamma_table *gamma_tbl)
{
    LOG2("@%s", __FUNCTION__);
    int ret;
    ret = mMainDevice->xioctl(ATOMISP_IOC_S_ISP_GAMMA, (struct atomisp_gamma_table *)gamma_tbl);
    LOG2("%s IOCTL ATOMISP_IOC_S_ISP_GAMMA ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int AtomISP::setCtcTable(const struct atomisp_ctc_table *ctc_tbl)
{
    LOG2("@%s", __FUNCTION__);
    int ret;
    ret = mMainDevice->xioctl(ATOMISP_IOC_S_ISP_CTC, (struct atomisp_ctc_table *)ctc_tbl);
    LOG2("%s IOCTL ATOMISP_IOC_S_ISP_CTC ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int AtomISP::setGdcConfig(const struct atomisp_morph_table *tbl)
{
    LOG2("@%s", __FUNCTION__);
    int ret;
    ret = mMainDevice->xioctl(ATOMISP_IOC_S_ISP_GDC_TAB, (struct atomisp_morph_table *)tbl);
    LOG2("%s IOCTL ATOMISP_IOC_S_ISP_GDC_TAB ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int AtomISP::setShadingTable(struct atomisp_shading_table *table)
{
    LOG2("@%s", __FUNCTION__);
    int ret;
    ret = mMainDevice->xioctl(ATOMISP_IOC_S_ISP_SHD_TAB, table);
    LOG2("%s IOCTL ATOMISP_IOC_S_ISP_SHD_TAB ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int AtomISP::setDeConfig(struct atomisp_de_config *de_cfg)
{
    LOG2("@%s", __FUNCTION__);
    int ret;
    ret = mMainDevice->xioctl(ATOMISP_IOC_S_ISP_FALSE_COLOR_CORRECTION, de_cfg);
    LOG2("%s IOCTL ATOMISP_IOC_S_ISP_FALSE_COLOR_CORRECTION ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int AtomISP::setTnrConfig(struct atomisp_tnr_config *tnr_cfg)
{
    LOG2("@%s", __FUNCTION__);
    int ret;
    ret = mMainDevice->xioctl(ATOMISP_IOC_S_TNR, tnr_cfg);
    LOG2("%s IOCTL ATOMISP_IOC_S_TNR ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int AtomISP::setEeConfig(struct atomisp_ee_config *ee_cfg)
{
    LOG2("@%s", __FUNCTION__);
    int ret;
    ret = mMainDevice->xioctl(ATOMISP_IOC_S_EE, ee_cfg);
    LOG2("%s IOCTL ATOMISP_IOC_S_EE ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int AtomISP::setNrConfig(struct atomisp_nr_config *nr_cfg)
{
    LOG2("@%s", __FUNCTION__);
    int ret;
    ret = mMainDevice->xioctl(ATOMISP_IOC_S_NR, nr_cfg);
    LOG2("%s IOCTL ATOMISP_IOC_S_NR ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int AtomISP::setDpConfig(struct atomisp_dp_config *dp_cfg)
{
    LOG2("@%s", __FUNCTION__);
    int ret;
    ret = mMainDevice->xioctl(ATOMISP_IOC_S_ISP_BAD_PIXEL_DETECTION, dp_cfg);
    LOG2("%s IOCTL ATOMISP_IOC_S_ISP_BAD_PIXEL_DETECTION ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int AtomISP::setWbConfig(struct atomisp_wb_config *wb_cfg)
{
    LOG2("@%s", __FUNCTION__);
    int ret;
    ret = mMainDevice->xioctl(ATOMISP_IOC_S_ISP_WHITE_BALANCE, wb_cfg);
    LOG2("%s IOCTL ATOMISP_IOC_S_ISP_WHITE_BALANCE ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int AtomISP::set3aConfig(const struct atomisp_3a_config *cfg)
{
    LOG2("@%s", __FUNCTION__);
    int ret;
    ret = mMainDevice->xioctl(ATOMISP_IOC_S_3A_CONFIG, (struct atomisp_3a_config *)cfg);
    LOG2("%s IOCTL ATOMISP_IOC_S_3A_CONFIG ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int AtomISP::setObConfig(struct atomisp_ob_config *ob_cfg)
{
    LOG2("@%s", __FUNCTION__);
    int ret;
    ret = mMainDevice->xioctl(ATOMISP_IOC_S_BLACK_LEVEL_COMP, ob_cfg);
    LOG2("%s IOCTL ATOMISP_IOC_S_BLACK_LEVEL_COMP ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int AtomISP::setGcConfig(const struct atomisp_gc_config *gc_cfg)
{
    LOG2("@%s", __FUNCTION__);
    int ret;
    ret = mMainDevice->xioctl(ATOMISP_IOC_S_ISP_GAMMA_CORRECTION, (struct atomisp_gc_config *)gc_cfg);
    LOG2("%s IOCTL ATOMISP_IOC_S_ISP_GAMMA_CORRECTION ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int AtomISP::setDvsConfig(const struct atomisp_dvs_6axis_config *dvs_6axis_cfg)
{
    LOG2("@%s", __FUNCTION__);
    int ret;
    ret = mMainDevice->xioctl(ATOMISP_IOC_S_DIS_VECTOR, (struct atomisp_dvs_6axis_config *)dvs_6axis_cfg);
    LOG2("%s IOCTL ATOMISP_IOC_S_6AXIS_CONFIG ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int AtomISP::getCssMajorVersion()
{
    return mCssMajorVersion;
}

int AtomISP::getCssMinorVersion()
{
    return mCssMinorVersion;
}

int AtomISP::getIspHwMajorVersion()
{
    return mIspHwMajorVersion;
}

int AtomISP::getIspHwMinorVersion()
{
    return mIspHwMinorVersion;
}

int AtomISP::setFlashIntensity(int intensity)
{
    LOG2("@%s", __FUNCTION__);
    return mMainDevice->setControl(V4L2_CID_FLASH_INTENSITY, intensity, "Set flash intensity");
}

/**
 * Return IObserverSubject for ObserverType
 */
IObserverSubject* AtomISP::observerSubjectByType(ObserverType t)
{
    IObserverSubject *s = NULL;
    switch (t) {
        case OBSERVE_PREVIEW_STREAM:
            s = &mPreviewStreamSource;
            break;
        case OBSERVE_FRAME_SYNC_SOF:
            s = mSensorHW.getFrameSyncSource();
            break;
        case OBSERVE_3A_STAT_READY:
            s = &m3AStatSource;
            break;
        default:
            break;
    }
    return s;
}

/**
 * Attaches an observer to one of the defined ObserverType's
 */
status_t AtomISP::attachObserver(IAtomIspObserver *observer, ObserverType t)
{
    status_t ret;

    if (t == OBSERVE_3A_STAT_READY) {
        m3AStatRequested++;
        if (m3AStatRequested == 1) {
          // subscribe to 3A frame sync event, only one subscription is needed
          // to serve multiple observers
          ret = m3AEventSubdevice->open();
          if (ret < 0) {
              ALOGE("Failed to open V4L2_ISP_SUBDEV2!");
              return UNKNOWN_ERROR;
          }

          ret = m3AEventSubdevice->subscribeEvent(V4L2_EVENT_ATOMISP_3A_STATS_READY);
          if (ret < 0) {
              ALOGE("Failed to subscribe to frame sync event!");
              m3AEventSubdevice->close();
              return UNKNOWN_ERROR;
          }
          m3AStatscEnabled = true;
        }
    }

exit:
    return mObserverManager.attachObserver(observer, observerSubjectByType(t));
}

/**
 * Detaches observer from one of the defined ObserverType's
 */
status_t AtomISP::detachObserver(IAtomIspObserver *observer, ObserverType t)
{
    IObserverSubject *s = observerSubjectByType(t);
    status_t ret;

    ret = mObserverManager.detachObserver(observer, s);
    if (ret != NO_ERROR) {
        ALOGE("%s failed!", __FUNCTION__);
        return ret;
    }

    if (t == OBSERVE_3A_STAT_READY) {
        if (--m3AStatRequested <= 0 && m3AStatscEnabled) {
            m3AEventSubdevice->unsubscribeEvent(V4L2_EVENT_ATOMISP_3A_STATS_READY);
            m3AEventSubdevice->close();
            m3AStatscEnabled = false;
            m3AStatRequested = 0;
        }
    }

    return ret;
}

/**
 * Pause and synchronise with observer
 *
 * Ability to sync into paused state is provided specifically
 * for ControlThread::stopPreviewCore() and OBSERVE_PREVIEW_STREAM.
 * This is for the sake of retaining the old semantics with buffers
 * flushing and keeping the initial preview parallelization changes
 * minimal (Bug 76149)
 *
 * Effectively this call blocks until observer method returns
 * normally and and then allows client to continue with the original
 * flow of flushing messages, AtomISP::stop() and release of buffers.
 *
 * TODO: The intention might rise e.g to optimize shutterlag
 * with CSS1.0 by streaming of the device and letting observers
 * to handle end-of-stream themself. Ideally, this can be done
 * with the help from IAtomIspObserver notify messages, but it
 * would require separating the stopping of threads operations
 * on buffers, the device streamoff and the release of buffers.
 */
void AtomISP::pauseObserver(ObserverType t)
{
    mObserverManager.setState(OBSERVER_STATE_PAUSED,
            observerSubjectByType(t), true);
}

/**
 * polls and dequeues 3A statistics ready event into IAtomIspObserver::Message
 */
status_t AtomISP::AAAStatSource::observe(IAtomIspObserver::Message *msg)
{
    LOG2("@%s", __FUNCTION__);
    struct v4l2_event event;
    int ret;

    if (!mISP->m3AStatscEnabled) {
        msg->id = IAtomIspObserver::MESSAGE_ID_ERROR;
        ALOGE("3A stat event enabled");
        return INVALID_OPERATION;
    }

    ret = mISP->m3AEventSubdevice->poll(FRAME_SYNC_POLL_TIMEOUT);

    if (ret <= 0) {
        ALOGE("Stats sync poll failed (%s), waiting recovery", (ret == 0) ? "timeout" : "error");
        ret = -1;
    } else {
        // poll was successful, dequeue the event right away
        do {
            ret = mISP->m3AEventSubdevice->dequeueEvent(&event);
            if (ret < 0) {
                ALOGE("Dequeue stats event failed");
            }
        } while (event.pending > 0);
    }

    if (ret < 0) {
        msg->id = IAtomIspObserver::MESSAGE_ID_ERROR;
        // We sleep a moment but keep passing error messages to observers
        // until further client controls.
        usleep(ATOMISP_EVENT_RECOVERY_WAIT);
        return NO_ERROR;
    }
     // fill observer message
    msg->id = IAtomIspObserver::MESSAGE_ID_EVENT;
    msg->data.event.sequence = event.sequence;
    msg->data.event.timestamp.tv_sec  = event.timestamp.tv_sec;
    msg->data.event.timestamp.tv_usec = event.timestamp.tv_nsec / 1000;
    if (mISP->mStatisticSkips > event.sequence) {
        LOG2("Skipping statistics num: %d", event.sequence);
        msg->data.event.type = IAtomIspObserver::EVENT_TYPE_STATISTICS_SKIPPED;
    } else {
        msg->data.event.type = IAtomIspObserver::EVENT_TYPE_STATISTICS_READY;
        // Note: Timestamp and sequence number of the event are useless.
        // Timestamp is from the event creation time and sequence is running
        // number of events - not providing the identifier for a frame.
        // Workaroud: Using exposure synchronization in SensorHW to identify
        // timestamp for the frame of statistics origin.
        nsecs_t frameTs = mISP->mSensorHW.getFrameTimestamp(TIMEVAL2USECS(&msg->data.event.timestamp));
        msg->data.event.timestamp.tv_sec = frameTs / 1000000;
        msg->data.event.timestamp.tv_usec = (frameTs % 1000000);
    }
    return NO_ERROR;
}

bool AtomISP::checkSkipFrameRecording(int frameNum)
{
    LOG2("@%s", __FUNCTION__);
    int targetFPS;
    if (mConfig.recording_fps)
        targetFPS = mConfig.recording_fps;
    else
        targetFPS = mConfig.preview_fps;

    return checkSkipFrame(frameNum, targetFPS);
}

/**
 * This function implements the frame skip algorithm for preview and video recording.
 * - If user requests half of sensor fps, drop every even frame
 * - If user requests third of sensor fps, drop two frames every three frames
 * @returns true: skip,  false: not skip
 */
bool AtomISP::checkSkipFrame(int frameNum, int targetFPS)
{
    LOG2("@%s", __FUNCTION__);
    float sensorFPS = mConfig.fps;

    if (fabs(sensorFPS / targetFPS - 2) < 0.1f && (frameNum % 2 == 0)) {
        LOG2("Target FPS: %d. Skipping frame num: %d", targetFPS, frameNum);
        return true;
    }

    if (fabs(sensorFPS / targetFPS - 3) < 0.1f && (frameNum % 3 != 0)) {
        LOG2("Target FPS: %d. Skipping frame num: %d", targetFPS, frameNum);
        return true;
    }

    // TODO skipping support for 25fps sensor framerate

    return false;
}

/**
 * This function implements the frame skip algorithm for video preview
 * when video recording start.
 */
bool AtomISP::checkSkipFrameForVideoZoom()
{
    LOG2("@%s", __FUNCTION__);
    if(mVideoZoomFrameSkips > 0) {
        mVideoZoomFrameSkips --;
        return true;
    }
    return false;
}

/**
 * polls and dequeues a preview frame into IAtomIspObserver::Message
 */
status_t AtomISP::PreviewStreamSource::observe(IAtomIspObserver::Message *msg)
{
    status_t status;
    int ret;
    LOG2("@%s", __FUNCTION__);
    int failCounter = 0;
    int retry_count = ATOMISP_GETFRAME_RETRY_COUNT;
    // Polling preview buffer needs more timeslot in file injection mode,
    // driver needs more than 20s to fill the 640x480 preview buffer, so
    // set retry count to 60
    if (mISP->isFileInjectionEnabled())
        retry_count = 40;
try_again:
    ret = mISP->mPreviewDevice->poll(ATOMISP_PREVIEW_POLL_TIMEOUT);
    if (ret > 0) {
        LOG2("@%s Entering dequeue : num-of-buffers queued %d", __FUNCTION__, mISP->mNumPreviewBuffersQueued);
        status = mISP->getPreviewFrame(&msg->data.frameBuffer.buff);
        if (status != NO_ERROR) {
            msg->id = IAtomIspObserver::MESSAGE_ID_ERROR;
            status = UNKNOWN_ERROR;
        } else {
            msg->data.frameBuffer.buff.owner = mISP;
            msg->id = IAtomIspObserver::MESSAGE_ID_FRAME;
            if (mISP->checkSkipFrame(msg->data.frameBuffer.buff.frameCounter, mISP->mConfig.preview_fps) ||
                mISP->checkSkipFrameForVideoZoom())
                msg->data.frameBuffer.buff.status = FRAME_STATUS_SKIPPED;
        }
    } else {
        ALOGE("@%s v4l2_poll for preview device failed! (%s)", __FUNCTION__, (ret==0)?"timeout":"error");
        msg->id = IAtomIspObserver::MESSAGE_ID_ERROR;
        status = (ret==0)?TIMED_OUT:UNKNOWN_ERROR;
    }

    if (status != NO_ERROR) {
        // check if reason is starving and enter sleep to wait
        // for returnBuffer()
        while(mISP->mNumPreviewBuffersQueued < 1) {
            if (++failCounter > retry_count) {
                ALOGD("@%s There were no preview buffers returned in time", __FUNCTION__);
                break;
            }
            ALOGW("@%s Preview stream starving from buffers!", __FUNCTION__);
            usleep(ATOMISP_GETFRAME_STARVING_WAIT);
        }

        if (++failCounter <= retry_count)
            goto try_again;
    }

    return status;
}

void AtomISP::setNrEE(bool en)
{
    LOG2("@%s", __FUNCTION__);
    mNoiseReductionEdgeEnhancement = en;
}

BatteryStatus AtomISP::getBatteryStatus()
{
    static const char *CamFlashCtrlFS = "/dev/bcu/camflash_ctrl";

    int  status;
    char buf[4];
    FILE *fp = NULL;

    LOG1("@%s", __FUNCTION__);
    // return BATTERY_STATUS_NORMAL directly if the sysfs file doesn't exist
    if (::access(CamFlashCtrlFS, R_OK)) {
        LOG1("@%s, file %s is not readable", __FUNCTION__, CamFlashCtrlFS);
        return BATTERY_STATUS_INVALID;
    }

    fp = ::fopen(CamFlashCtrlFS, "r");
    if (NULL == fp) {
        ALOGW("@%s, file %s open with err:%s", __FUNCTION__, CamFlashCtrlFS, strerror(errno));
        return BATTERY_STATUS_INVALID;
    }
    memset(buf, 0, 4);
    size_t len = ::fread(buf, 1, 1, fp);
    if (len == 0) {
        ALOGW("@%s, fail to read 1 byte from camflash_ctrl", __FUNCTION__);
        ::fclose(fp);
        return BATTERY_STATUS_INVALID;
    }
    ::fclose(fp);

    status = atoi(buf);
    if (status > BATTERY_STATUS_CRITICAL || status < BATTERY_STATUS_NORMAL)
        return BATTERY_STATUS_INVALID;

    if (status > BATTERY_STATUS_NORMAL)
        ALOGW("@%s warning battery status:%d", __FUNCTION__, status);

    return (BatteryStatus)status;
}

} // namespace android
