/*
 * Copyright (C) 2011 The Android Open Source Project
 * Copyright (c) 2011-2014 Intel Corporation
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
#include "HALVideoStabilization.h"
#include "PerformanceTraces.h"
#include "exif/Exif.h"

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

#define ATOMISP_PREVIEW_POLL_TIMEOUT 1000
#define ATOMISP_GETFRAME_RETRY_COUNT 60  // Times to retry poll/dqbuf in case of error
#define ATOMISP_GETFRAME_STARVING_WAIT 33000 // Time to usleep between retry's when stream is starving from buffers.
#define ATOMISP_MIN_CONTINUOUS_BUF_SIZE 3 // Min buffer len supported by CSS
#define ATOMISP_MIN_CONTINUOUS_BUF_NUM_CSS2X 5 // Min buffer len supported by CSS2.x
#define ATOMISP_RAW_BUF_NUM_FOR_INFINITE_CAP 7 // Min raw buffer number when numberCapture set to -1
namespace android {

// TODO CJC: get from cpf or kernel driver
const int ZOOM_LINEAR_RATIO_STEP(10); // 0.1
const int ZOOM_LINEAR_MIN_DRIVE(1);
const int ZOOM_LINEAR_MAX_DRIVE(31);
static const unsigned int MAX_NUMBER_PENDING_UPDATES(20);
static const unsigned long MEM_2G = 2147483648U;

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
    ,mSensorEmbeddedMetaData(NULL)
    ,mDvs(NULL)
    ,mDvsEnabled(false)
    ,mGroupIndex (-1)
    ,mMode(MODE_NONE)
    ,mCallbacks(callbacks)
    ,mPreviewBuffersCached(true)
    ,mRecordingBuffers(NULL)
    ,mSwapRecordingDevice(false)
    ,mRecordingDeviceSwapped(false)
    ,mHALZSLEnabled(false)
    ,mHALSDVEnabled(false)
    ,mUseMultiStreamsForSoC(PlatformData::useMultiStreamsForSoC(mCameraId))
    ,mHALZSLBuffers(NULL)
    ,mContinuousJpegCaptureEnabled(false)
    ,mMultiStreamsHALZSLCaptureBuffers(NULL)
    ,mMultiStreamsHALZSLPostviewBuffers(NULL)
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
    ,mInitialSkips(0)
    ,mStatisticSkips(0)
    ,mDVSFrameSkips(0)
    ,mVideoZoomFrameSkips(0)
    ,mIsFileInject(false)
    ,mSessionId(0)
    ,mLowLight(false)
    ,mXnr(0)
    ,mZoomRatios(NULL)
    ,mRawDataDumpSize(0)
    ,m3AStatRequested(0)
    ,m3AStatscEnabled(false)
    ,mSensorEmbeddedMetaDataSupported(false)
    ,mColorEffect(V4L2_COLORFX_NONE)
    ,mScaler(scalerService)
    ,mObserverManager()
    ,mCssMajorVersion(0)
    ,mCssMinorVersion(0)
    ,mIspHwMajorVersion(0)
    ,mIspHwMinorVersion(0)
    ,mHALVideoStabilization(false)
    ,mHALVideoNormal(false)
    ,mExtIspVideoHighSpeed(false)
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

    /**
     * needn't to initialize the postview device, if platform doesn't supports postview output
     */
    if (PlatformData::supportsPostviewOutput(mCameraId))
        mPostViewDevice = new V4L2VideoNode(devName[mGroupIndex].dev[V4L2_POSTVIEW_DEVICE], V4L2_POSTVIEW_DEVICE);

    mRecordingDevice = new V4L2VideoNode(devName[mGroupIndex].dev[V4L2_RECORDING_DEVICE], V4L2_RECORDING_DEVICE);
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

    mSensorHW->selectActiveSensor(mMainDevice);

    /**
     * open ATOMISP subdevice according to sensor
     */
    mSensorHW->setIspSubDevId(mGroupIndex);
    char ispDev[ISP_DEVICE_NAME_LENGTH_MAX];
    status = mSensorHW->getIspDevicePath(ispDev, ISP_DEVICE_NAME_LENGTH_MAX);
    if (status == NO_ERROR) {
        m3AEventSubdevice = new V4L2Subdevice(ispDev, V4L2_ISP_SUBDEV);
    } else {
        ALOGW("Failed to get isp device path! Failing back to XML config %s", PlatformData::getISPSubDeviceName());
        m3AEventSubdevice = new V4L2Subdevice(PlatformData::getISPSubDeviceName(), V4L2_ISP_SUBDEV);
        status = NO_ERROR;
    }

    mSensorType = PlatformData::sensorType(mCameraId);
    LOG1("Sensor type detected: %s", (mSensorType == SENSOR_TYPE_RAW)?"RAW":"SOC");

    HWControlGroup hwcg;
    hwcg.mIspCI = this;
    mSensorEmbeddedMetaData = new SensorEmbeddedMetaData(hwcg, mCameraId);
    return status;
}

status_t AtomISP::initDVS()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    HWControlGroup hwcg;
    hwcg.mIspCI = (IHWIspControl*) this;
    hwcg.mSensorCI = getSensorControlInterface();
    mDvs = IDvs::createAtomDvs(hwcg);
    if (!mDvs)
        return NO_INIT;

    status = mDvs->dvsInit();
    if (status != NO_ERROR) {
        ALOGE("%s: Failed to initiate DVS", __FUNCTION__);
        delete mDvs;
        mDvs = NULL;
    }
    return status;
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
    if (mDvs != NULL) {
        delete mDvs;
        mDvs = NULL;
    }

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

/**
 * Checks if postview device is initialized.
 */
bool AtomISP::isPostviewInitialized() const
{
    LOG1("@%s", __FUNCTION__);
    return (mPostViewDevice != NULL);
}

status_t AtomISP::init()
{
    LOG1("@%s", __FUNCTION__);

    status_t status = NO_ERROR;

    if (mSensorHW != NULL) {
        // because sensorHW pointer is give out reset it instead making
        // new instance
        ALOGD("@%s: SensorHW already exists, use old one.", __FUNCTION__);
        mSensorHW->reset(mCameraId);
    } else {
        if (PlatformData::supportsContinuousJpegCapture(mCameraId)) {
            // This is for external ISP...
            mSensorHW = new SensorHWExtIsp(mCameraId);
        } else {
            // "Normal" sensor HW:
            mSensorHW = new SensorHW(mCameraId);
        }
    }

    if (mSensorHW == NULL) {
        ALOGE("Failed to allocate SensorHW");
        return NO_MEMORY;
    }

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
    int width  = 0;
    int height = 0;
    const char *size = PlatformData::defaultPreviewSize(mCameraId);
    if (!size) {
        ALOGE("No default preview size from platformdata. mCameraId(%d) must be bad.", mCameraId);
        return NO_INIT;
    }

    IntelCameraParameters::parseResolution(size, width, height);
    setPreviewFrameFormat(width, height,
                          pixelsToBytes(PlatformData::getPreviewPixelFormat(mCameraId), width),
                          PlatformData::getPreviewPixelFormat(mCameraId));

    setPostviewFrameFormat(formatDescriptorPv);

    AtomBuffer formatDescriptorSs
        = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_FORMAT_DESCRIPTOR, V4L2_PIX_FMT_NV12);

    /* for stretch problems(BZ: 152956 and BZ: 140905),
     * hal doesn't set default picture size here, uses 0x0 as default.
     * then, in processStaticParameters(), if hal finds no valid picture size is set,
     * it will choose a picture size, if there is a valid picture size, it will use it.
     */
    setSnapshotFrameFormat(formatDescriptorSs);

    width  = 0;
    height = 0;
    size = PlatformData::defaultVideoSize(mCameraId);
    if (!size) {
        ALOGE("No default video size from platformdata. mCameraId(%d) must be bad.", mCameraId);
        return NO_INIT;
    }

    IntelCameraParameters::parseResolution(size, width, height);
    setVideoFrameFormat(width, height, V4L2_PIX_FMT_NV12);

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
    return isDeviceInitialized() ? (IHWSensorControl*) mSensorHW.get() : NULL;
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
    mConfig.num_snapshot = 0;
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

    // Supported video sizes list already exist in camera_profiles.xml. So we should get limitation from it.
    getMaxVideoSize(mCameraId, &(mConfig.recordingLimits.maxWidth), &(mConfig.recordingLimits.maxHeight));

    /*
     * Since the snapshot size is same with video size in videosnapshot mode when online
     * SDV is running. So the limitation of max snapshot size shouldn't be less than max
     * video size. Thus, select max width and height for snapshot size in both max
     * snapshot and video size.
     */
    if (mConfig.snapshotLimits.maxWidth < mConfig.recordingLimits.maxWidth)
        mConfig.snapshotLimits.maxWidth = mConfig.recordingLimits.maxWidth;
    if (mConfig.snapshotLimits.maxHeight < mConfig.recordingLimits.maxHeight)
        mConfig.snapshotLimits.maxHeight = mConfig.recordingLimits.maxHeight;
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

    delete mSensorEmbeddedMetaData;
    mSensorEmbeddedMetaData = NULL;

    mSensorHW.clear();
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
    if (getMakerNote(&makerNote) != NO_ERROR)
        makerNote.focal_length = ((EXIF_DEF_FOCAL_LEN_NUM << 16) | (EXIF_DEF_FOCAL_LEN_DEN & 0xFFFF));
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
     * DIGITAL VIDEO STABILIZATION
     */
    if (PlatformData::supportsDVS(cameraId)) {
        params->set(CameraParameters::KEY_VIDEO_STABILIZATION_SUPPORTED, CameraParameters::TRUE);
        params->set(CameraParameters::KEY_VIDEO_STABILIZATION, CameraParameters::TRUE);
    } else {
        params->set(CameraParameters::KEY_VIDEO_STABILIZATION_SUPPORTED, CameraParameters::FALSE);
        params->set(CameraParameters::KEY_VIDEO_STABILIZATION, CameraParameters::FALSE);
    }

    /**
     * SDV(still during video)
     */
    if (PlatformData::isFullResSdvSupported(cameraId)) {
        intel_params->set(IntelCameraParameters::KEY_SDV_SUPPORTED, CameraParameters::TRUE);
        intel_params->set(IntelCameraParameters::KEY_SDV, CameraParameters::TRUE);
    } else {
        intel_params->set(IntelCameraParameters::KEY_SDV_SUPPORTED, CameraParameters::FALSE);
        intel_params->set(IntelCameraParameters::KEY_SDV, CameraParameters::FALSE);
    }

    /**
     * DUAL MODE
     */
    if (PlatformData::supportDualMode())
    {
        intel_params->set(IntelCameraParameters::KEY_DUAL_MODE_SUPPORTED,"true");
        intel_params->set(IntelCameraParameters::KEY_DUAL_MODE,"false");
    }

    /**
     * DUAL CAMERA MODE
     */
    if (false == PlatformData::supportExtendedCamera())
        intel_params->set(IntelCameraParameters::KEY_SUPPORTED_DUAL_CAMERA_MODE, "normal");
    else
        intel_params->set(IntelCameraParameters::KEY_SUPPORTED_DUAL_CAMERA_MODE, "normal,depth");
    intel_params->set(IntelCameraParameters::KEY_DUAL_CAMERA_MODE, "normal");

    /**
     * Continuous shooting
     */
    if (mSensorType == SENSOR_TYPE_RAW || PlatformData::supportsContinuousJpegCapture(mCameraId)) {
        intel_params->set(IntelCameraParameters::KEY_CONTINUOUS_SHOOTING_SUPPORTED, CameraParameters::TRUE);
        intel_params->set(IntelCameraParameters::KEY_CONTINUOUS_SHOOTING, CameraParameters::FALSE);
    } else {
        intel_params->set(IntelCameraParameters::KEY_CONTINUOUS_SHOOTING_SUPPORTED, CameraParameters::FALSE);
        intel_params->set(IntelCameraParameters::KEY_CONTINUOUS_SHOOTING, CameraParameters::FALSE);
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
        params->set(CameraParameters::KEY_ANTIBANDING, "50hz");
        params->set(CameraParameters::KEY_SUPPORTED_ANTIBANDING, "50hz,60hz");
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
    intel_params->set(IntelCameraParameters::KEY_MAX_BURST_LENGTH_WITH_NEGATIVE_START_INDEX, PlatformData::maxContinuousRawRingBufferSize(mCameraId)-1);

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

    // intelligent mode
    intel_params->set(IntelCameraParameters::KEY_INTELLIGENT_MODE, "false");
    intel_params->set(IntelCameraParameters::KEY_SUPPORTED_INTELLIGENT_MODE, PlatformData::supportedIntelligentMode(cameraId));

    // color-bar mode
    intel_params->set(IntelCameraParameters::KEY_COLORBAR, CameraParameters::FALSE);
    if (PlatformData::supportsColorBarPreview(cameraId))
        intel_params->set(IntelCameraParameters::KEY_SUPPORTED_COLORBAR, "true,false");
    else
        intel_params->set(IntelCameraParameters::KEY_SUPPORTED_COLORBAR, CameraParameters::FALSE);

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
        if ((maxWidth < supportedSizes[i].width) || (maxHeight < supportedSizes[i].height)) {
            maxWidth = supportedSizes[i].width;
            maxHeight = supportedSizes[i].height;
        }
    }

    *width = maxWidth;
    *height = maxHeight;
}

void AtomISP::getMaxVideoSize(int cameraId, int* width, int* height)
{
    LOG1("@%s", __FUNCTION__);
    CameraParameters p;
    Vector<Size> supportedSizes;
    int maxWidth = 0, maxHeight = 0;

    p.set(CameraParameters::KEY_SUPPORTED_VIDEO_SIZES, PlatformData::supportedVideoSizes(cameraId));
    p.getSupportedVideoSizes(supportedSizes);

    for (unsigned int i = 0; i < supportedSizes.size(); i++) {
        if ((maxWidth < supportedSizes[i].width) || (maxHeight < supportedSizes[i].height)) {
            maxWidth = supportedSizes[i].width;
            maxHeight = supportedSizes[i].height;
        }
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

        if (pxioctl(mMainDevice, ATOMISP_IOC_S_XNR, &mXnr) < 0) {
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
    ret = pxioctl(mMainDevice, ATOMISP_IOC_G_DIS_STAT, stats);
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
    if (pxioctl(mMainDevice, ATOMISP_IOC_S_DIS_VECTOR, (struct atomisp_dis_vector *)vector) < 0) {
        ALOGE("failed to set motion vector");
        status = UNKNOWN_ERROR;
    }
    return status;
}

status_t AtomISP::setDvsCoefficients(const struct atomisp_dis_coefficients *coefs) const
{
    status_t status = NO_ERROR;
    if (pxioctl(mMainDevice, ATOMISP_IOC_S_DIS_COEFS, (struct atomisp_dis_coefficients *)coefs) < 0) {
        ALOGE("failed to set dvs coefficients");
        status = UNKNOWN_ERROR;
    }
    return status;
}

status_t AtomISP::getIspDvs2BqResolutions(struct atomisp_dvs2_bq_resolutions *bq_res) const
{
    status_t status = NO_ERROR;
    if (pxioctl(mMainDevice, ATOMISP_IOC_G_DVS2_BQ_RESOLUTIONS, bq_res) < 0) {
        ALOGE("failed to get ISP dvs2 bq resolutions");
        status = UNKNOWN_ERROR;
    }
    return status;
}


status_t AtomISP::getIspParameters(struct atomisp_parm *isp_param) const
{
    status_t status = NO_ERROR;
    if (pxioctl(mMainDevice, ATOMISP_IOC_G_ISP_PARM, isp_param) < 0) {
        ALOGE("failed to get ISP parameters");
        status = UNKNOWN_ERROR;
    }
    return status;
}

/**
 * This method is for external ISP only.
 * Needs to inform AtomISP if any special feature is going to run on external ISP
 * So that the external ISP configuration could pay attention to the hint
 *
 * External ISP video high speed has only 1 stream output. we extend this
 * configuration on the continuous jpeg capture mode. Just skip to operate
 * the main device.
 */
void AtomISP::setExternalIspActionHint(ExtIspActionHint hint)
{
    mHALVideoStabilization = hint & EXT_ISP_ACTION_HALVS;
    mHALVideoNormal = hint & EXT_ISP_ACTION_NORMAL;
    mExtIspVideoHighSpeed  = hint & EXT_ISP_ACTION_VIDEOHS;
    LOG1("@%s hal video stabilization:%d, normal video: %d, external ISP video high speed:%d",
            __FUNCTION__, mHALVideoStabilization, mHALVideoNormal, mExtIspVideoHighSpeed);
}

//Set device fps base on device mode and platform
bool AtomISP::isAllowedToSetFps(V4L2VideoNode *device, int deviceMode) const
{
    if (device->mId == V4L2_RECORDING_DEVICE && deviceMode == CI_MODE_VIDEO) {
        return true;
    } else if (mExtIspVideoHighSpeed && device->mId == V4L2_PREVIEW_DEVICE) {
        return true;
    } else
        return false;
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
    mHALSDVEnabled = false;
    mContinuousJpegCaptureEnabled = false;
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
    case MODE_CONTINUOUS_VIDEO:
        status = configureContinuousVideo();
        break;
    case MODE_CONTINUOUS_JPEG:
        status = configureContinuousJpegCapture();
        break;
    case MODE_CONTINUOUS_JPEG_VIDEO:
        status = configureContinuousJpegVideo();
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
        status = mSensorHW->prepare(mode == MODE_CAPTURE);

        // Need to double check actual fps from sensor.
        // This may be different from one that was requested.
        mConfig.fps = mSensorHW->getFramerate();
        LOG1("Sensor fps: %.2f", mConfig.fps);
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
        if (mHALZSLEnabled && (false == mUseMultiStreamsForSoC))
            mPreviewDevice = mMainDevice;
        if ((status = allocatePreviewBuffers()) != NO_ERROR)
            mPreviewDevice->stop();
        break;
    case MODE_CONTINUOUS_JPEG_VIDEO:
        if ((status = allocateSnapshotBuffers()) != NO_ERROR) {
            stopRecording();
            return status;
        }
        // intentional fall-through without break
    case MODE_CONTINUOUS_VIDEO:
    case MODE_VIDEO:
        if (!mHALVideoStabilization && !mHALVideoNormal) // no need to allocate in halVS mode which uses preview bufs for recording
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
    case MODE_CONTINUOUS_JPEG:
        if ((status = allocatePreviewBuffers()) != NO_ERROR) {
            mPreviewDevice->stop();
            return status;
        }

        // external ISP video high speed mode doesn't need to allocate snapshot buffer
        if (mExtIspVideoHighSpeed)
            break;

        if ((status = allocateSnapshotBuffers()) != NO_ERROR) {
            return status;
        }
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
        attachObserver((IAtomIspObserver *) mSensorHW.get(), OBSERVE_FRAME_SYNC_SOF);
    }
    mSensorHW->start();

    /** The reason to call the function "mSensorEmbeddedMetadata.init()" here is
     * the mSensorEmbeddedMetadata need the metadata buffer size from ISP to
     * malloc the buffer, but the size should be ready after format setting.
     */
    if (mSensorEmbeddedMetaData) {
        mSensorEmbeddedMetaData->init();
        mSensorEmbeddedMetaDataSupported = mSensorEmbeddedMetaData->isSensorEmbeddedMetaDataSupported();
    }

    switch (mMode) {
    case MODE_CONTINUOUS_CAPTURE:
    case MODE_CONTINUOUS_JPEG:
    case MODE_PREVIEW:
        status = startPreview();
        break;
    case MODE_CONTINUOUS_JPEG_VIDEO:
        status = startRecording();
        break;
    case MODE_VIDEO:
    case MODE_CONTINUOUS_VIDEO:
        // in CSS2.0 mDvs is mandatory for zoom functionality
        if (mDvs && (mCssMajorVersion == 2 || mDvsEnabled)) {
            if (mDvs->reconfigure() == NO_ERROR) {
                attachObserver(mDvs, OBSERVE_PREVIEW_STREAM);
                LOG1("%s: attach mDvs to Preview Stream Observer", __FUNCTION__);
                mDvs->setZoom(mConfig.zoom);
                if (mMode == MODE_CONTINUOUS_VIDEO)
                    atomisp_set_zoom(mConfig.zoom);
            }
            //Because DVS2 lib calculates the morph table for video zoom by the first frame,
            //so the first several frames should have no zoom effect, need to be skipped
            setSkipFramesForVideoZoom();
        }
        status = startRecording();
        break;
    case MODE_CAPTURE:
        status = startCapture();
        break;
    default:
        ALOGE("Invalid mode!");
        status = UNKNOWN_ERROR;
        break;
    };

    if (status == NO_ERROR) {
        runStartISPActions();
        mSessionId++;
    } else {
        mMode = MODE_NONE;
        if (mDvs) {
            setDVS(false);
            detachObserver(mDvs, OBSERVE_PREVIEW_STREAM);
            LOG1("%s: detach mDvs from Preview Stream Observer", __FUNCTION__);
        }
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
        detachObserver((IAtomIspObserver *) mSensorHW.get(), OBSERVE_FRAME_SYNC_SOF);
    }

    runStopISPActions();

    switch (mMode) {
    case MODE_CONTINUOUS_JPEG:
    case MODE_PREVIEW:
        status = stopPreview();
        break;

    case MODE_VIDEO:
    case MODE_CONTINUOUS_JPEG_VIDEO:
        status = stopRecording();
        break;

    case MODE_CAPTURE:
        status = stopCapture();
        break;

    case MODE_CONTINUOUS_CAPTURE:
        status = stopContinuousPreview();
        break;

    case MODE_CONTINUOUS_VIDEO:
        status = stopContinuousVideo();
        break;

    default:
        ALOGW("ISP stop called in wrong mode!");
        break;
    };

    mSensorHW->stop();

    if (mFileInject.active == true)
        stopFileInject();

    if (status == NO_ERROR)
        mMode = MODE_NONE;

    if (mDvs) {
        detachObserver(mDvs, OBSERVE_PREVIEW_STREAM);
        LOG1("%s: detach mDvs from Preview Stream Observer", __FUNCTION__);
    }

    return status;
}

status_t AtomISP::configurePreview()
{
    LOG1("@%s", __FUNCTION__);
    int ret = 0;
    status_t status = NO_ERROR;

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

    configureDepthMode(PlatformData::isExtendedCameras());

    ret = configureDevice(
            mPreviewDevice.get(),
            CI_MODE_PREVIEW,
            &(mConfig.preview),
            false);
    if (ret < 0) {
        status = UNKNOWN_ERROR;
        goto err;
    }

    // need to resend the current zoom value
    if ((mMode == MODE_VIDEO || mMode == MODE_CONTINUOUS_VIDEO) && mDvs && mCssMajorVersion == 2) {
        mDvs->setZoom(mConfig.zoom);
        if (mMode == MODE_CONTINUOUS_VIDEO)
            atomisp_set_zoom(mConfig.zoom);
    } else {
        atomisp_set_zoom(mConfig.zoom);
    }
    return status;

err:
    mPreviewDevice->stop();
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

    if ((mContinuousJpegCaptureEnabled && !mExtIspVideoHighSpeed) ||
            (mHALZSLEnabled && mUseMultiStreamsForSoC)) {
        ret = mMainDevice->start(mConfig.num_snapshot_buffers, mInitialSkips);
        if (ret < 0) {
            ALOGE("start capture on first device failed!");
            status = UNKNOWN_ERROR;
            goto err;
        }
        mNumCapturegBuffersQueued = mConfig.num_snapshot_buffers;
    }

    ret = mPreviewDevice->start(bufcount, mInitialSkips);
    if (ret < 0) {
        ALOGE("Start preview device failed!");
        status = UNKNOWN_ERROR;
        goto err;
    }

    if (mHALZSLEnabled && mUseMultiStreamsForSoC) {
        if (isPostviewInitialized()) {
            ret = mPostViewDevice->start(mConfig.num_postview_buffers, mInitialSkips);
            if (ret < 0) {
                ALOGE("start capture on second device failed!");
                status = UNKNOWN_ERROR;
                goto err;
            }
        }
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

    if (mContinuousJpegCaptureEnabled && !mExtIspVideoHighSpeed) {
        mMainDevice->stop();
        freeSnapshotBuffers();
    }

    if (mHALZSLEnabled && mUseMultiStreamsForSoC) {
        mMainDevice->stop();
        if (isPostviewInitialized()) {
            mPostViewDevice->stop();
            mPostViewDevice->close();
        }
    }

    freePreviewBuffers();

    // In case some cases like HAL ZSL is enabled
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

    configureDepthMode(PlatformData::isExtendedCameras());

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

    ret = configureDevice(
            mPreviewDevice.get(),
            CI_MODE_VIDEO,
            previewConfig,
            false);
    if (ret < 0) {
        ALOGE("Configure recording device failed!");
        status = UNKNOWN_ERROR;
        goto err;
    }

    // Set NARROW_GAMMA_ON to driver when support narrow gamma in video mode.
    struct atomisp_formats_config formats_config;
    formats_config.video_full_range_flag = NARROW_GAMMA_OFF;
    if (PlatformData::supportsNarrowGamma(mCameraId)) {
        LOG2("set narrow gamma on to driver");
        formats_config.video_full_range_flag = NARROW_GAMMA_ON;
    }
    setNarrowGamma(&formats_config);

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

    if (!mHALVideoStabilization && !mHALVideoNormal) {
        if ((mHALSDVEnabled && mUseMultiStreamsForSoC) || mMode == MODE_CONTINUOUS_JPEG_VIDEO) {
            ret = mMainDevice->start(mConfig.num_snapshot_buffers, mInitialSkips);
            if (ret < 0) {
                ALOGE("start capture on first device failed!");
                status = UNKNOWN_ERROR;
                goto err;
            }

            if (isPostviewInitialized()) {
                ret = mPostViewDevice->start(mConfig.num_postview_buffers, mInitialSkips);
                if (ret < 0) {
                    ALOGE("start capture on second device failed!");
                    status = UNKNOWN_ERROR;
                    goto err;
                }
            }
        }

        //workaround: when DVS is on, the first several frames are greenish, need to be skipped.
        ret = mRecordingDevice->start(mConfig.num_recording_buffers, mInitialSkips + mDVSFrameSkips);
        if (ret < 0) {
            ALOGE("Start recording device failed");
            status = UNKNOWN_ERROR;
            goto err;
        }
    }

    ret = mPreviewDevice->start(mConfig.num_preview_buffers, mInitialSkips + mDVSFrameSkips);
    if (ret < 0) {
        ALOGE("Start preview device failed!");
        status = UNKNOWN_ERROR;
        goto err;
    }

    mNumPreviewBuffersQueued = mConfig.num_preview_buffers;

    if (!mHALVideoStabilization && !mHALVideoNormal) {
        mNumRecordingBuffersQueued = mConfig.num_recording_buffers;
    } else {
        mNumRecordingBuffersQueued = 0; // halVS doesn't use rec bufs
    }

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
    mRecordingDevice->close();

    mPreviewDevice->stop();
    freePreviewBuffers();
    mPreviewDevice->close();
    mRecordingDevice->close();

    if (mMode == MODE_CONTINUOUS_JPEG_VIDEO) {
        mMainDevice->stop();
        freeSnapshotBuffers();
    }

    if (mHALSDVEnabled && mUseMultiStreamsForSoC) {
        mMainDevice->stop();
        if (isPostviewInitialized()) {
            mPostViewDevice->stop();
            mPostViewDevice->close();
        }
    }

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

    if (isPostviewInitialized()) {
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
    }

nopostview:
    // need to resend the current zoom value
    if ((mMode == MODE_VIDEO || mMode == MODE_CONTINUOUS_VIDEO) && mDvs && mCssMajorVersion == 2) {
        mDvs->setZoom(mConfig.zoom);
        if (mMode == MODE_CONTINUOUS_VIDEO)
            atomisp_set_zoom(mConfig.zoom);
    } else {
        atomisp_set_zoom(mConfig.zoom);
    }

    return status;

errorCloseSecond:
    if (isPostviewInitialized()) {
        mPostViewDevice->close();
    }
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

    if (!PlatformData::ispSupportContinuousCaptureMode(mCameraId)) {
        return NO_ERROR;
    }

    struct atomisp_cont_capture_conf conf;

    CLEAR(conf);
    conf.num_captures = numCaptures;
    conf.offset = offset;
    conf.skip_frames = skip;

    int res = pxioctl(mMainDevice, ATOMISP_IOC_S_CONT_CAPTURE_CONFIG, &conf);
    LOG1("@%s: CONT_CAPTURE_CONFIG num %d, offset %d, skip %u, res %d",
         __FUNCTION__, numCaptures, offset, skip, res);
    if (res != 0) {
        ALOGE("@%s: error with CONT_CAPTURE_CONFIG, res %d", __FUNCTION__, res);
        return UNKNOWN_ERROR;
    }

    return NO_ERROR;
}

/**
 * Configures the ISP depth mode
 *
 * This takes effect in kernel when preview is started.
 */
status_t AtomISP::configureDepthMode(bool enable)
{
    LOG2("@%s", __FUNCTION__);
    if (mMainDevice->setControl(V4L2_CID_DEPTH_MODE,
                              enable, "Depth mode") < 0)
            return UNKNOWN_ERROR;

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

    if (!PlatformData::ispSupportContinuousCaptureMode(mCameraId)) {
        return NO_ERROR;
    }

    if (mMainDevice->setControl(V4L2_CID_ATOMISP_CONTINUOUS_MODE,
                              enable, "Continuous mode") < 0)
            return UNKNOWN_ERROR;

    enable = (!mContCaptConfig.capturePriority &&
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

    if (lookback > captures && !mContCaptConfig.rawBufferLock)
        numBuffers += lookback;
    else
        numBuffers += abs(captures); // maybe negative

    if (mCssMajorVersion >= 2) {
    // for css2.x, the minimum raw ring buffers number is ATOMISP_MIN_CONTINUOUS_BUF_NUM_CSS2X
        //when offset is -1 , one ring buffer is able to be optimized
        if (offset == -1 && !mContCaptConfig.rawBufferLock)
            numBuffers -= 1;

        if (numBuffers < ATOMISP_MIN_CONTINUOUS_BUF_NUM_CSS2X)
            numBuffers = ATOMISP_MIN_CONTINUOUS_BUF_NUM_CSS2X;

        if (captures == -1)
            numBuffers = ATOMISP_RAW_BUF_NUM_FOR_INFINITE_CAP;
    }

    if (numBuffers > PlatformData::maxContinuousRawRingBufferSize(mCameraId))
        numBuffers = PlatformData::maxContinuousRawRingBufferSize(mCameraId);

    LOG1("continuous mode ringbuffer size to %d (captures %d, offset %d)",
         numBuffers, captures, offset);

    if (mMainDevice->setControl(V4L2_CID_ATOMISP_CONTINUOUS_RAW_BUFFER_SIZE,
                                   numBuffers,
                                   "Continuous raw ringbuffer size") < 0)
        return UNKNOWN_ERROR;

    // enable or disable raw buffer lock mode
    rawBufferLockEnable(mContCaptConfig.rawBufferLock);

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
    mConfig.HALZSL.fourcc = PlatformData::getPreviewPixelFormat(mCameraId);
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


status_t AtomISP::configureContinuousJpegCapture()
{
    LOG1("@%s", __FUNCTION__);
    status_t status(OK);
    int ret(0);

    mContinuousJpegCaptureEnabled = true;
    // configure the main device
    mConfig.snapshot.fourcc = V4L2_PIX_FMT_CONTINUOUS_JPEG;
    mConfig.snapshot.bpl = SGXandDisplayBpl(mConfig.snapshot.fourcc, mConfig.snapshot.width);
    mConfig.snapshot.size = frameSize(mConfig.snapshot.fourcc, mConfig.snapshot.width, mConfig.snapshot.height);
    LOG1("@%s configured main %dx%d bpl %d size %d", __FUNCTION__, mConfig.snapshot.width, mConfig.snapshot.height, mConfig.snapshot.bpl,  mConfig.snapshot.size);

    if (!mExtIspVideoHighSpeed) {
        ret = configureDevice(
                mMainDevice.get(),
                CI_MODE_PREVIEW,
                &(mConfig.snapshot),
                isDumpRawImageReady());
        if (ret < 0) {
            ALOGE("@%s, configure main device failed!", __FUNCTION__);
            return UNKNOWN_ERROR;
        }
    }

    // configure the preview device
    ret = mPreviewDevice->open();
    if (ret < 0) {
        ALOGE("Open preview device failed!");
        status = UNKNOWN_ERROR;
        return NO_INIT;
    }

    struct v4l2_capability aCap;
    status = mPreviewDevice->queryCap(&aCap);
    if (status != NO_ERROR) {
        ALOGE("Failed basic capability check failed!");
        return NO_INIT;
    }

    ret = configureDevice(
            mPreviewDevice.get(),
            CI_MODE_PREVIEW,
            &(mConfig.preview),
            false);
    if (ret < 0) {
        ALOGE("@%s, configure preview device failed!", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    if (!mExtIspVideoHighSpeed) {
        mConfig.num_snapshot = NUM_OF_JPEG_CAPTURE_SNAPSHOT_BUF;
        mConfig.num_snapshot_buffers = NUM_OF_JPEG_CAPTURE_SNAPSHOT_BUF;
        mUsingClientSnapshotBuffers = false;
    }

    return status;
}

/**
 * Configures the ISP to work on a mode which only support two streams output in the HAL.
 * One is preview output, and another is capture output. In this mode, postview output is
 * also not supported, so the postview and thumbnail are generated by GPU.
 */
status_t AtomISP::configurePreviewAndCaptureStreamsSOC()
{
    LOG1("@%s", __FUNCTION__);

    int ret = 0;
    status_t status = OK;
    struct v4l2_capability aCap;

    // configure the main device
    Size zslSize = getHALZSLResolution();
    mConfig.snapshot.width = zslSize.width;
    mConfig.snapshot.height = zslSize.height;
    mConfig.snapshot.bpl = SGXandDisplayBpl(mConfig.snapshot.fourcc, mConfig.snapshot.width);
    mConfig.snapshot.size = frameSize(mConfig.snapshot.fourcc, mConfig.snapshot.width, mConfig.snapshot.height);
    LOG1("@%s configured %dx%d bpl %d", __FUNCTION__, mConfig.snapshot.width, mConfig.snapshot.height, mConfig.snapshot.bpl);

    ret = configureDevice(
            mMainDevice.get(),
            CI_MODE_STILL_CAPTURE,
            &(mConfig.snapshot),
            isDumpRawImageReady());
    if (ret < 0) {
        ALOGE("@%s, configure main device failed!", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    // configure the preview device
    ret = mPreviewDevice->open();
    if (ret < 0) {
        ALOGE("Open preview device failed!");
        status = UNKNOWN_ERROR;
        return NO_INIT;
    }

    status = mPreviewDevice->queryCap(&aCap);
    if (status != NO_ERROR) {
        ALOGE("Failed basic capability check failed!");
        return NO_INIT;
    }

    mConfig.preview.bpl = SGXandDisplayBpl(mConfig.preview.fourcc, mConfig.preview.width);
    ret = configureDevice(
            mPreviewDevice.get(),
            CI_MODE_PREVIEW,
            &(mConfig.preview),
            false);
    if (ret < 0) {
        ALOGE("@%s, configure preview device failed!", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    return status;
}

/**
 * Configures the ISP to work on a mode where Continuous capture is implemented
 * in the HAL. This is currently only available for SoC sensors, hence the name.
 * the difference with the configureContinuousSOC() is that the configureContinuousSOC() just use one stream from driver
 * but the configureMultiStreamsContinuousSOC will use the yuvpp pipe and let the driver output more streams.
 */
status_t AtomISP::configureMultiStreamsContinuousSOC()
{
    LOG1("@%s", __FUNCTION__);

    int ret = 0;
    status_t status = OK;
    struct v4l2_capability aCap;

    mHALZSLEnabled = true;

    requestContCapture(-1, -1, 0);

    ret = configureContinuousMode(true);
    if (ret != NO_ERROR) {
        ALOGE("@%s, set continuous mode failed", __FUNCTION__);
        return ret;
    }

    // configure the main device
    Size zslSize = getHALZSLResolution();
    mConfig.snapshot.width = zslSize.width;
    mConfig.snapshot.height = zslSize.height;
    mConfig.snapshot.bpl = SGXandDisplayBpl(mConfig.snapshot.fourcc, mConfig.snapshot.width);
    mConfig.snapshot.size = frameSize(mConfig.snapshot.fourcc, mConfig.snapshot.width, mConfig.snapshot.height);
    LOG1("@%s configured %dx%d bpl %d", __FUNCTION__, mConfig.snapshot.width, mConfig.snapshot.height, mConfig.snapshot.bpl);

    ret = configureDevice(
            mMainDevice.get(),
            CI_MODE_PREVIEW,
            &(mConfig.snapshot),
            isDumpRawImageReady());
    if (ret < 0) {
        ALOGE("@%s, configure main device failed!", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    if (isPostviewInitialized()) {
        // configure the postview device
        ret = mPostViewDevice->open();
        if (ret < 0) {
            ALOGE("@%s, open postview device failed!", __FUNCTION__);
            return UNKNOWN_ERROR;
        }

        status = mPostViewDevice->queryCap(&aCap);
        if (status != NO_ERROR) {
            ALOGE("@%s, query postview device failed!", __FUNCTION__);
            return NO_INIT;
        }

        ret = configureDevice(
                mPostViewDevice.get(),
                CI_MODE_PREVIEW,
                &(mConfig.postview),
                false);
        if (ret < 0) {
            ALOGE("@%s, configure postview device failed!", __FUNCTION__);
            return UNKNOWN_ERROR;
        }
    }

    // configure the preview device
    ret = mPreviewDevice->open();
    if (ret < 0) {
        ALOGE("Open preview device failed!");
        status = UNKNOWN_ERROR;
        return NO_INIT;
    }

    status = mPreviewDevice->queryCap(&aCap);
    if (status != NO_ERROR) {
        ALOGE("Failed basic capability check failed!");
        return NO_INIT;
    }

    mConfig.preview.bpl = SGXandDisplayBpl(mConfig.preview.fourcc, mConfig.preview.width);
    ret = configureDevice(
            mPreviewDevice.get(),
            CI_MODE_PREVIEW,
            &(mConfig.preview),
            false);
    if (ret < 0) {
        ALOGE("@%s, configure preview device failed!", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    return status;
}

status_t AtomISP::configureMultiStreamsContinuousVideoSOC()
{
    LOG1("@%s", __FUNCTION__);
    int ret = 0;
    status_t status = OK;
    struct v4l2_capability aCap;

    mHALSDVEnabled = true;

    requestContCapture(-1, -1, 0);

    ret = configureContinuousMode(true);
    if (ret != NO_ERROR) {
        ALOGE("@%s, set continuous mode failed", __FUNCTION__);
        return ret;
    }

    // configure the main device
    Size zslSize = getHALZSLResolution();
    mConfig.snapshot.width = zslSize.width;
    mConfig.snapshot.height = zslSize.height;
    mConfig.snapshot.bpl = SGXandDisplayBpl(mConfig.snapshot.fourcc, mConfig.snapshot.width);
    mConfig.snapshot.size = frameSize(mConfig.snapshot.fourcc, mConfig.snapshot.width, mConfig.snapshot.height);
    LOG1("@%s configured %dx%d bpl %d", __FUNCTION__, mConfig.snapshot.width, mConfig.snapshot.height, mConfig.snapshot.bpl);

    ret = configureDevice(
            mMainDevice.get(),
            CI_MODE_VIDEO,
            &(mConfig.snapshot),
            isDumpRawImageReady());
    if (ret < 0) {
        ALOGE("@%s, configure main device failed!", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    // configure the recording device
    ret = mRecordingDevice->open();
    if (ret < 0) {
        ALOGE("Open recording device failed!");
        status = UNKNOWN_ERROR;
        return NO_INIT;
    }

    status = mRecordingDevice->queryCap(&aCap);
    if (status != NO_ERROR) {
        ALOGE("Failed basic capability check failed!");
        return NO_INIT;
    }

    ret = configureDevice(
            mRecordingDevice.get(),
            CI_MODE_VIDEO,
            &(mConfig.recording),
            false);
    if (ret < 0) {
        ALOGE("@%s, configure recording device failed!", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    // configure the preview device
    ret = mPreviewDevice->open();
    if (ret < 0) {
        ALOGE("Open preview device failed!");
        status = UNKNOWN_ERROR;
        return NO_INIT;
    }

    status = mPreviewDevice->queryCap(&aCap);
    if (status != NO_ERROR) {
        ALOGE("Failed basic capability check failed!");
        return NO_INIT;
    }

    mConfig.preview.bpl = SGXandDisplayBpl(mConfig.preview.fourcc, mConfig.preview.width);
    ret = configureDevice(
            mPreviewDevice.get(),
            CI_MODE_VIDEO,
            &(mConfig.preview),
            false);
    if (ret < 0) {
        ALOGE("@%s, configure preview device failed!", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    if (isPostviewInitialized()) {
        // configure the postview device
        ret = mPostViewDevice->open();
        if (ret < 0) {
            ALOGE("@%s, open postview device failed!", __FUNCTION__);
            return UNKNOWN_ERROR;
        }

        status = mPostViewDevice->queryCap(&aCap);
        if (status != NO_ERROR) {
            ALOGE("@%s, query postview device failed!", __FUNCTION__);
            return NO_INIT;
        }

        ret = configureDevice(
                mPostViewDevice.get(),
                CI_MODE_VIDEO,
                &(mConfig.postview),
                false);
        if (ret < 0) {
            ALOGE("@%s, configure postview device failed!", __FUNCTION__);
            return UNKNOWN_ERROR;
        }
    }

    return status;
}

status_t AtomISP::configureContinuousVideo()
{
    LOG1("@%s", __FUNCTION__);
    int ret;
    status_t status = NO_ERROR;

    // continuous mode does not support low_light mode capture
    setLowLight(false);
    updateCaptureParams();

    if(mSensorType == SENSOR_TYPE_SOC && mUseMultiStreamsForSoC) {
        return configureMultiStreamsContinuousVideoSOC();
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
            CI_MODE_VIDEO,
            &(mConfig.snapshot),
            isDumpRawImageReady());
    if (ret < 0) {
        ALOGE("configure first device failed!");
        status = UNKNOWN_ERROR;
        goto errorFreeBuf;
    }

    status = configureRecording();
    if (status != NO_ERROR) {
        return status;
    }

    if (isPostviewInitialized()) {
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
                CI_MODE_VIDEO,
                &(mConfig.postview),
                false);
        if (ret < 0) {
            ALOGE("configure second device failed!");
            status = UNKNOWN_ERROR;
            goto errorCloseSecond;
        }
    }

    // need to resend the current zoom value
    if (mDvs && mCssMajorVersion == 2)
        mDvs->setZoom(mConfig.zoom);

    atomisp_set_zoom(mConfig.zoom);

    return status;

errorCloseSecond:
    if (isPostviewInitialized()) {
        mPostViewDevice->close();
    }
errorFreeBuf:
    freeSnapshotBuffers();
    freePostviewBuffers();

    return status;
}

status_t AtomISP::configureContinuousJpegVideo()
{
    LOG1("@%s", __FUNCTION__);
    status_t status(OK);
    int ret(0);

    mContinuousJpegCaptureEnabled = true;

    // configure the main device
    mConfig.snapshot.fourcc = V4L2_PIX_FMT_CONTINUOUS_JPEG;
    mConfig.snapshot.bpl = SGXandDisplayBpl(mConfig.snapshot.fourcc, mConfig.snapshot.width);
    mConfig.snapshot.size = frameSize(mConfig.snapshot.fourcc, mConfig.snapshot.width, mConfig.snapshot.height);
    LOG1("@%s configured main %dx%d bpl %d size %d", __FUNCTION__, mConfig.snapshot.width, mConfig.snapshot.height, mConfig.snapshot.bpl,  mConfig.snapshot.size);

    ret = configureDevice(
        mMainDevice.get(),
        CI_MODE_PREVIEW,
        &(mConfig.snapshot),
        isDumpRawImageReady());
    if (ret < 0) {
        ALOGE("@%s, configure main device failed!", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    status = configureRecording();
    if (status != OK) {
        return status;
    }

    mConfig.num_snapshot = NUM_OF_JPEG_CAPTURE_SNAPSHOT_BUF;
    mConfig.num_snapshot_buffers = NUM_OF_JPEG_CAPTURE_SNAPSHOT_BUF;
    mUsingClientSnapshotBuffers = false;



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
        if (!PlatformData::supportsPostviewOutput(mCameraId)) {
            return configurePreviewAndCaptureStreamsSOC();
        } else {
            return mUseMultiStreamsForSoC
                    ? configureMultiStreamsContinuousSOC()
                    : configureContinuousSOC();
        }
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

    if (isPostviewInitialized()) {
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
    }

    // need to resend the current zoom value
    if ((mMode == MODE_VIDEO || mMode == MODE_CONTINUOUS_VIDEO) && mDvs && mCssMajorVersion == 2) {
        mDvs->setZoom(mConfig.zoom);
        if (mMode == MODE_CONTINUOUS_VIDEO)
            atomisp_set_zoom(mConfig.zoom);
    } else {
        atomisp_set_zoom(mConfig.zoom);
    }

    // restore the actual capture fps value
    mConfig.fps = capture_fps;

    return status;

errorCloseSecond:
    if (isPostviewInitialized()) {
        mPostViewDevice->close();
    }
errorFreeBuf:
    freeSnapshotBuffers();
    freePostviewBuffers();

    return status;
}

status_t AtomISP::startCapture()
{
    LOG1("@%s", __FUNCTION__);

    int ret;
    status_t status = NO_ERROR;
    int i, initialSkips;

    // Limited by driver, raw bayer image dump can support only 1 frame when setting
    // snapshot number. Otherwise, the raw dump image would be corrupted.
    // also since CSS1.5 we cannot capture from postview at the same time
    int snapNum;
    if (mConfig.snapshot.fourcc == mSensorHW->getRawFormat())
        snapNum = 1;
    else
        snapNum = mConfig.num_snapshot;

    /**
     * handle initial skips here for normal capture mode
     * For continuous capture we cannot skip this way, we will control via the
     * offset calculation
     */
    if (inContinuousMode())
       initialSkips = 0;
    else
       initialSkips = mInitialSkips;

   /**
    * moving the hack from Gang to disable these ISP parameters, only needed
    * when starting preview in continuous capture mode and when starting capture
    * in online mode. This was required to enable SuperZoom POC.
    **/
    if (!inContinuousMode() && mNoiseReductionEdgeEnhancement == false) {
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

    if (isPostviewInitialized()) {
        ret = mPostViewDevice->start(snapNum, initialSkips);
        if (ret < 0) {
            ALOGE("start capture on second device failed!");
            status = UNKNOWN_ERROR;
            goto errorStopFirst;
        }
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
    if (isPostviewInitialized()) {
        mPostViewDevice->close();
    }
errorFreeBuf:
    freeSnapshotBuffers();
    freePostviewBuffers();

end:
    return status;
}

status_t AtomISP::stopContinuousVideo()
{
    LOG1("@%s", __FUNCTION__);
    int error = 0;
    CLEAR(mContCaptConfig);
    if (stopCapture() != NO_ERROR)
        ++error;
    // TODO: this call to requestContCapture() can be
    //       removed once PSI BZ 101676 is solved
    if (requestContCapture(0, 0, 0) != NO_ERROR)
        ++error;
    if (configureContinuousMode(false) != NO_ERROR)
        ++error;
    if (stopRecording() != NO_ERROR)
        ++error;
    if (error) {
        ALOGE("@%s: errors (%d) in stopping continuous capture",
             __FUNCTION__, error);
        return UNKNOWN_ERROR;
    }
    return NO_ERROR;
}

status_t AtomISP::stopContinuousPreview()
{
    LOG1("@%s", __FUNCTION__);
    int error = 0;
    CLEAR(mContCaptConfig);
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
    // always disable raw buffer lock mode
    rawBufferLockEnable(false);
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
        CLEAR(mSnapshotBuffers);
        mConfig.num_snapshot = 0;
        mUsingClientSnapshotBuffers = false;
    }

    if (mUsingClientPostviewBuffers) {
        mPostviewBuffers.clear();
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
    if (mHALZSLEnabled || mHALSDVEnabled)
        return OK;

    if (!inContinuousMode() || mMode == MODE_CONTINUOUS_JPEG || mMode == MODE_CONTINUOUS_JPEG_VIDEO) {
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
    if (!inContinuousMode() || mMode == MODE_CONTINUOUS_JPEG || mMode == MODE_CONTINUOUS_JPEG_VIDEO) {
        ALOGE("@%s: invalid mode %d", __FUNCTION__, mMode);
        return INVALID_OPERATION;
    }

    if (mHALZSLEnabled || mHALSDVEnabled)
        return OK;

    mMainDevice->stop(false);
    if (isPostviewInitialized()) {
        mPostViewDevice->stop(false);
    }
    mContCaptPrepared = true;
    return NO_ERROR;
}

/**
 * Prepares ISP for offline capture
 *
 * \param config container to configure capture count, skipping
 *               and the start offset (see struct ContinuousCaptureConfig)
 */
status_t AtomISP::prepareOfflineCapture(ContinuousCaptureConfig &cfg)
{
    LOG1("@%s, numCaptures = %d raw lock mode:%d capture priority:%d",
            __FUNCTION__, cfg.numCaptures, cfg.rawBufferLock, cfg.capturePriority);
    if (cfg.offset < continuousBurstNegMinOffset() && !cfg.rawBufferLock) {
        ALOGE("@%s: offset %d not supported, minimum %d",
             __FUNCTION__, cfg.offset, continuousBurstNegMinOffset());
        return UNKNOWN_ERROR;
    }
    mContCaptConfig = cfg;
    mContCaptPrepared = true;
    return NO_ERROR;
}

/**
 * Enable or disable the CSS raw buffer lock mode
 *
 * \param enable, true to enable, false to disable
 */
status_t AtomISP::rawBufferLockEnable(bool enable)
{
    LOG2("@%s :%s", __FUNCTION__, enable ? "true" : "false");
    return mMainDevice->setControl(V4L2_CID_ENABLE_RAW_BUFFER_LOCK,
                        enable, "Continuous raw buffer lock mode");
}

/**
 * Unlock the locked raw buffer in css
 * raw buffers will be locked by default if we enable raw buffer lock mode
 * So no rawBufferLock method in AtomISP. Just need to unlock in time.
 *
 * \param expId: exposure ID of the raw buffer to be unlocked
 */
status_t AtomISP::rawBufferUnlock(int expId)
{
    LOG2("@%s ID:%d", __FUNCTION__, expId);
    int ret;

    if (mMode != MODE_CONTINUOUS_CAPTURE || mContCaptConfig.rawBufferLock == false) {
        ALOGW("invalide raw buffer unlock call in mode:%d", mMode);
        return INVALID_OPERATION;
    }

    ret = mMainDevice->xioctl(ATOMISP_IOC_EXP_ID_UNLOCK, &expId);
    LOG2("%s IOCTL ATOMISP_IOC_EXP_ID_UNLOCK ret: %d\n", __FUNCTION__, ret);
    return ret;
}

/**
 * Request to capture the image with specified exposure ID.
 * The raw buffer must be locked before you call this method
 *
 * \param expId: exposure ID of the raw buffer to be processed
 */
status_t AtomISP::rawBufferCapture(int expId)
{
    LOG2("@%s ID:%d", __FUNCTION__, expId);
    int ret;
    if (mMode != MODE_CONTINUOUS_CAPTURE || mContCaptConfig.rawBufferLock == false) {
        ALOGW("invalide raw buffer capture call in mode:%d", mMode);
        return INVALID_OPERATION;
    }

    ret = mMainDevice->xioctl(ATOMISP_IOC_EXP_ID_CAPTURE, &expId);
    LOG2("%s IOCTL ATOMISP_IOC_EXP_ID_CAPTURE ret: %d\n", __FUNCTION__, ret);
    return ret;
}

bool AtomISP::isOfflineCaptureRunning() const
{
    if (inContinuousMode() && mMode != MODE_CONTINUOUS_JPEG &&
        mMode != MODE_CONTINUOUS_JPEG_VIDEO && mMainDevice->isStarted())
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
    LOG1("device: %d, width:%d, height:%d, deviceMode:%d fourcc:%s 0x%x raw:%d", device->mId,
         w, h, deviceMode, v4l2Fmt2Str(fourcc), fourcc, raw);

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
        if (fps > DEFAULT_RECORDING_FPS && isAllowedToSetFps(device, deviceMode)) {

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
        } else {
            // Use default for now.
            // Correct value will be known after SensorHW::prepare.
            mConfig.fps = DEFAULT_SENSOR_FPS;
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
    // for ext isp, we allow in a special 6MP preview resolution for panorama.
    // Otherwise, we cap the resolution to maxWidth/maxHeight
    if (!(PlatformData::supportsContinuousJpegCapture(mCameraId) &&
        width == RESOLUTION_6MP_WIDTH && height == RESOLUTION_6MP_HEIGHT)) {
        if (width > mConfig.previewLimits.maxWidth || width <= 0)
            width = mConfig.previewLimits.maxWidth;
        if (height > mConfig.previewLimits.maxHeight || height <= 0)
            height = mConfig.previewLimits.maxHeight;
    }

    // add the envelope size for the hal videostabilization, if it is enabled
    if (mHALVideoStabilization) {
        HALVideoStabilization::getEnvelopeSize(width, height, width, height, bpl);
    }

    mConfig.preview.width = width;
    mConfig.preview.height = height;
    mConfig.preview.fourcc = fourcc;
    mConfig.preview.bpl = bpl;
    mConfig.preview.size = frameSize(fourcc, bytesToPixels(fourcc, bpl), height);
    LOG1("width(%d), height(%d), bpl(%d), size(%d), fourcc(%s 0x%x)",
         width, height, bpl, mConfig.preview.size, v4l2Fmt2Str(fourcc),fourcc);
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

    LOG1("@%s width(%d), height(%d), fourcc(%s 0x%x)", __FUNCTION__,
         formatDescriptor.width, formatDescriptor.height,
         v4l2Fmt2Str(formatDescriptor.fourcc), formatDescriptor.fourcc);
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
    LOG1("width(%d), height(%d), bpl(%d), size(%d), fourcc(%s 0x%x)",
         formatDescriptor.width, formatDescriptor.height, mConfig.postview.bpl, mConfig.postview.size, v4l2Fmt2Str(formatDescriptor.fourcc), formatDescriptor.fourcc);
    return status;
}

status_t AtomISP::setSnapshotFrameFormat(AtomBuffer& formatDescriptor)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    // In raw capture mode, ISP is in copy mode and cannot crop the frame.
    // Otherwise we obey the limits.
    if (!CameraDump::isDumpImageEnable(CAMERA_DEBUG_DUMP_RAW)) {
        if (formatDescriptor.width > mConfig.snapshotLimits.maxWidth || formatDescriptor.width < 0)
            formatDescriptor.width = mConfig.snapshotLimits.maxWidth;
        if (formatDescriptor.height > mConfig.snapshotLimits.maxHeight || formatDescriptor.height < 0)
            formatDescriptor.height = mConfig.snapshotLimits.maxHeight;
    }

    mConfig.snapshot = formatDescriptor;
    mConfig.snapshot.bpl = SGXandDisplayBpl(formatDescriptor.fourcc,
        bytesToPixels(formatDescriptor.fourcc, formatDescriptor.bpl));
    mConfig.snapshot.size = frameSize(formatDescriptor.fourcc, bytesToPixels(formatDescriptor.fourcc, mConfig.snapshot.bpl), formatDescriptor.height);
    LOG1("width(%d), height(%d), bpl(%d), size(%d), fourcc(%s 0x%x)",
         formatDescriptor.width, formatDescriptor.height, mConfig.snapshot.bpl, mConfig.snapshot.size,v4l2Fmt2Str(formatDescriptor.fourcc), formatDescriptor.fourcc);
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

    if(fourcc == 0)
         fourcc = mConfig.recording.fourcc;
    if (mConfig.recording.width == width &&
        mConfig.recording.height == height &&
        mConfig.recording.fourcc == fourcc) {
        // Do nothing
        return status;
    }

    if (inVideoMode()) {
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
    LOG1("width(%d), height(%d), bpl(%d), fourcc(%s 0x%x)",
         width, height, mConfig.recording.bpl, v4l2Fmt2Str(fourcc), fourcc);

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
 * Workaround 4: In case that preview is too big for VFPP. was removed since
 * it's only valid in CTP
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

    if (videoMode || inVideoMode()) {
        // Workaround 3: with some sensors the VF resolution must be
        //               limited high-resolution video recordiing
        // TODO: if we get more cases like this, move to PlatformData.h
        const char* sensorName = "ov8830";
        if (strncmp(mSensorHW->getSensorName(), sensorName, sizeof(sensorName) - 1) == 0) {
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
        if (strncmp(mSensorHW->getSensorName(), manUsensorBName, sizeof(manUsensorBName) - 1) == 0) {
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

status_t AtomISP::setTorchHelper(int intensity, int torchId)
{
    if (intensity) {
        // move the index to the second byte for the driver
        torchId = torchId << 8;
        // bitwise add the led index to the second byte of the intensity value
        int intensityWithIndex = intensity | torchId;
        if (mMainDevice->setControl(V4L2_CID_FLASH_TORCH_INTENSITY, intensityWithIndex, "Torch intensity") < 0)
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

status_t AtomISP::setTorch(int intensity, unsigned int torchId)
{
    LOG1("@%s: intensity = %d", __FUNCTION__, intensity);

    if (PlatformData::cameraFacing(mCameraId) != CAMERA_FACING_BACK) {
        ALOGE("Indicator intensity is supported only for primary camera!");
        return INVALID_OPERATION;
    }

    setTorchHelper(intensity, torchId);

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

    if ((mMode == MODE_VIDEO || mMode == MODE_CONTINUOUS_VIDEO) && mDvs && mCssMajorVersion == 2 && mSensorType == SENSOR_TYPE_RAW &&
        !mHALVideoStabilization && !mHALVideoNormal) {
        mDvs->setZoom(zoom);
        if (mMode == MODE_CONTINUOUS_VIDEO) {
            int ret = atomisp_set_zoom(zoom);
            if (ret < 0) {
                ALOGE("Error setting zoom to %d", zoom);
                return UNKNOWN_ERROR;
            }
        }
    } else {
        int ret = atomisp_set_zoom(zoom);
        if (ret < 0) {
            ALOGE("Error setting zoom to %d", zoom);
            return UNKNOWN_ERROR;
        }
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

    if (pxioctl(mMainDevice, ATOMISP_IOC_ISP_MAKERNOTE, info) < 0) {
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
    LOG1("@%s: %d", __FUNCTION__, enable);
    status_t status = NO_ERROR;

    if (mMode != MODE_NONE) {
        ALOGW("@%s: Cannot change DVS status while ISP is running, mode = %d", __FUNCTION__, mMode);
        return INVALID_OPERATION;
    }

    if (enable && mDvs && !mDvs->isDvsValid()) {
        enable = false;
        ALOGW("@%s: Cannot start DVS due to some restrictions in size or frame rate", __FUNCTION__);
    }

    status = mMainDevice->setControl(V4L2_CID_ATOMISP_VIDEO_STABLIZATION,
                                    enable, "Video Stabilization");
    if(status != 0)
    {
        ALOGE("Error setting DVS in the driver");
        status = INVALID_OPERATION;
    } else {
        mDvsEnabled = enable;
    }
    return status;
}

status_t AtomISP::setHDR(int mode)
{
    LOG1("@%s: %d", __FUNCTION__, mode);
    struct atomisp_ext_isp_ctrl m10mo_ctrl;
    CLEAR(m10mo_ctrl);
    m10mo_ctrl.id = EXT_ISP_CID_CAPTURE_HDR;
    m10mo_ctrl.data = mode;
    status_t status = mMainDevice->xioctl(ATOMISP_IOC_EXT_ISP_CTRL, &m10mo_ctrl);

    return status;
}

status_t AtomISP::setLLS(int mode)
{
    LOG1("@%s: %d", __FUNCTION__, mode);
    struct atomisp_ext_isp_ctrl m10mo_ctrl;
    CLEAR(m10mo_ctrl);
    m10mo_ctrl.id = EXT_ISP_CID_CAPTURE_LLS;
    m10mo_ctrl.data = mode;
    status_t status = mMainDevice->xioctl(ATOMISP_IOC_EXT_ISP_CTRL, &m10mo_ctrl);

    return status;
}

status_t AtomISP::setShotMode(int mode)
{
    LOG1("@%s: %d", __FUNCTION__, mode);
    struct atomisp_ext_isp_ctrl m10mo_ctrl;
    CLEAR(m10mo_ctrl);
    m10mo_ctrl.id = EXT_ISP_CID_SHOT_MODE;
    m10mo_ctrl.data = mode;
    status_t status = mMainDevice->xioctl(ATOMISP_IOC_EXT_ISP_CTRL, &m10mo_ctrl);

    return status;
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

inline bool AtomISP::inContinuousMode() const
{
    return mMode == MODE_CONTINUOUS_VIDEO || mMode == MODE_CONTINUOUS_CAPTURE ||
           mMode == MODE_CONTINUOUS_JPEG || mMode == MODE_CONTINUOUS_JPEG_VIDEO;
}

inline bool AtomISP::inVideoMode() const
{
    return mMode == MODE_CONTINUOUS_VIDEO || mMode == MODE_VIDEO || mMode == MODE_CONTINUOUS_JPEG_VIDEO;
}

bool AtomISP::dvsEnabled()
{
    return mDvsEnabled;
}

status_t AtomISP::setDVSSkipFrames(unsigned int skips)
{
    LOG1("@%s: %d", __FUNCTION__, skips);
    status_t status = NO_ERROR;
    mDVSFrameSkips = skips;
    return status;
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

    if (PlatformData::supportsContinuousJpegCapture(mCameraId)) {
        struct atomisp_ext_isp_ctrl m10mo_ctrl;
        CLEAR(m10mo_ctrl);
        m10mo_ctrl.id = EXT_ISP_CID_ZOOM;
        m10mo_ctrl.data = mZoomDriveTable[zoom];
        mMainDevice->xioctl(ATOMISP_IOC_EXT_ISP_CTRL, &m10mo_ctrl);
    } else if (!mHALZSLEnabled && !mHALSDVEnabled) { // fix for driver zoom bug, prevent setting in HAL ZSL mode
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
#ifdef ENABLE_FILE_INJECTION
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
#else
    return 0;
#endif
}

/**
 * Stops file injection.
 *
 * Closes the kernel resources needed for file injection and
 * other resources.
 */
int AtomISP::stopFileInject(void)
{
#ifdef ENABLE_FILE_INJECTION
    LOG1("%s: enter", __FUNCTION__);
    if (mFileInject.dataAddr != NULL) {
        delete[] mFileInject.dataAddr;
        mFileInject.dataAddr = NULL;
    }

    mFileInjectDevice->close();

    return 0;
#else
    return 0;
#endif
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
#ifdef ENABLE_FILE_INJECTION
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
#else
    return 0;
#endif
}

status_t AtomISP::fileInjectSetSize(void)
{
#ifdef ENABLE_FILE_INJECTION
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
#else
    return NO_ERROR;
#endif
}

int AtomISP::atomisp_set_capture_mode(int deviceMode)
{
    LOG1("@%s", __FUNCTION__);
    struct v4l2_streamparm parm;
    status_t status;
    int vfpp_mode = ATOMISP_VFPP_ENABLE;

    if(mHALZSLEnabled && (false == mUseMultiStreamsForSoC))
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
            if (mRecordingBuffers[i].dataPtr == NULL)
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
    mHALZSLBuffers[index].status = (FrameBufferStatus)(bufInfo.vbuffer.reserved & FRAME_STATUS_MASK);

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

status_t AtomISP::getMultiStreamsHALZSLPreviewFrame(AtomBuffer *buff)
{
    LOG2("@%s", __FUNCTION__);

    int previewIndex, snapshotIndex, postviewIndex;
    struct v4l2_buffer_info bufInfo;

    Mutex::Autolock mLock(mHALZSLLock);

    // get the preview buffers
    CLEAR(bufInfo);
    previewIndex = mPreviewDevice->grabFrame(&bufInfo);
    LOG2("@%s, previewIndex = %d", __FUNCTION__, previewIndex);
    if(previewIndex < 0) {
        ALOGE("@%s, Error in grabbing preview frame!", __FUNCTION__);
        return BAD_INDEX;
    }
    LOG2("Device: %d. Grabbed frame of size: %d", mPreviewDevice->mId, bufInfo.vbuffer.bytesused);

    mPreviewBuffers.editItemAt(previewIndex).id = previewIndex;
    mPreviewBuffers.editItemAt(previewIndex).frameCounter = mPreviewDevice->getFrameCount();
    mPreviewBuffers.editItemAt(previewIndex).ispPrivate = mSessionId;
    mPreviewBuffers.editItemAt(previewIndex).capture_timestamp = bufInfo.vbuffer.timestamp;
    mPreviewBuffers.editItemAt(previewIndex).frameSequenceNbr = bufInfo.vbuffer.sequence;
    mPreviewBuffers.editItemAt(previewIndex).status = (FrameBufferStatus)bufInfo.vbuffer.reserved;
    mPreviewBuffers.editItemAt(previewIndex).size = bufInfo.vbuffer.bytesused;

    *buff = mPreviewBuffers[previewIndex];

    mNumPreviewBuffersQueued--;
    dumpPreviewFrame(previewIndex);

    // get the capture buffers
    CLEAR(bufInfo);
    snapshotIndex = mMainDevice->grabFrame(&bufInfo);
    LOG2("@%s, snapshotIndex = %d", __FUNCTION__, snapshotIndex);
    if (snapshotIndex < 0) {
        ALOGE("@%s, Error in grabbing snapshot frame!", __FUNCTION__);
        return BAD_INDEX;
    }
    LOG2("Device: %d. Grabbed frame of size: %d", mMainDevice->mId, bufInfo.vbuffer.bytesused);

    mMultiStreamsHALZSLCaptureBuffers[snapshotIndex].id = snapshotIndex;
    mMultiStreamsHALZSLCaptureBuffers[snapshotIndex].capture_timestamp = bufInfo.vbuffer.timestamp;
    mMultiStreamsHALZSLCaptureBuffers[snapshotIndex].frameSequenceNbr = bufInfo.vbuffer.sequence;
    mMultiStreamsHALZSLCaptureBuffers[snapshotIndex].status = (FrameBufferStatus)bufInfo.vbuffer.reserved;
    mMultiStreamsHALZSLCaptureBuffersQueue.push_front(mMultiStreamsHALZSLCaptureBuffers[snapshotIndex]);

    if (isPostviewInitialized()) {
        // get the postview buffers
        CLEAR(bufInfo);
        postviewIndex = mPostViewDevice->grabFrame(&bufInfo);
        LOG2("@%s, postviewIndex = %d", __FUNCTION__, postviewIndex);
        if (postviewIndex < 0) {
            ALOGE("@%s, Error in grabbing postview frame!", __FUNCTION__);
            // If we failed with the second device, return the frame to the first device
            mMainDevice->putFrame(snapshotIndex);
            return BAD_INDEX;
        }
        LOG2("Device: %d. Grabbed frame of size: %d", mPostViewDevice->mId, bufInfo.vbuffer.bytesused);

        mMultiStreamsHALZSLPostviewBuffers[postviewIndex].id = postviewIndex;
        mMultiStreamsHALZSLPostviewBuffers[postviewIndex].capture_timestamp = bufInfo.vbuffer.timestamp;
        mMultiStreamsHALZSLPostviewBuffers[postviewIndex].frameSequenceNbr = bufInfo.vbuffer.sequence;
        mMultiStreamsHALZSLPostviewBuffers[postviewIndex].status = (FrameBufferStatus)bufInfo.vbuffer.reserved;
        mMultiStreamsHALZSLPostviewBuffersQueue.push_front(mMultiStreamsHALZSLPostviewBuffers[postviewIndex]);
    }

    return NO_ERROR;
}

status_t AtomISP::getJpegCapturePreviewFrame(AtomBuffer *buff)
{
    LOG2("@%s", __FUNCTION__);

    int previewIndex(-1);
    int snapshotIndex(-1);
    struct v4l2_buffer_info bufInfo;

    // get the preview buffers
    CLEAR(bufInfo);
    previewIndex = mPreviewDevice->grabFrame(&bufInfo);
    LOG2("@%s, previewIndex = %d", __FUNCTION__, previewIndex);
    if(previewIndex < 0) {
        ALOGE("@%s, Error in grabbing preview frame!", __FUNCTION__);
        return BAD_INDEX;
    }
    LOG2("Device: %d. Grabbed frame of size: %d", mPreviewDevice->mId, bufInfo.vbuffer.bytesused);

    mPreviewBuffers.editItemAt(previewIndex).id = previewIndex;
    mPreviewBuffers.editItemAt(previewIndex).frameCounter = mPreviewDevice->getFrameCount();
    mPreviewBuffers.editItemAt(previewIndex).ispPrivate = mSessionId;
    mPreviewBuffers.editItemAt(previewIndex).capture_timestamp = bufInfo.vbuffer.timestamp;
    mPreviewBuffers.editItemAt(previewIndex).frameSequenceNbr = bufInfo.vbuffer.sequence;
    mPreviewBuffers.editItemAt(previewIndex).status = (FrameBufferStatus)bufInfo.vbuffer.reserved;
    mPreviewBuffers.editItemAt(previewIndex).size = bufInfo.vbuffer.bytesused;

    --mNumPreviewBuffersQueued;
    dumpPreviewFrame(previewIndex);

    // external ISP high speed only has 1 stream output
    if (mExtIspVideoHighSpeed) {
        *buff = mPreviewBuffers[previewIndex];
        return NO_ERROR;
    }

    // get the capture buffers
    CLEAR(bufInfo);
    snapshotIndex = mMainDevice->grabFrame(&bufInfo);
    LOG2("@%s, snapshotIndex = %d", __FUNCTION__, snapshotIndex);
    if (snapshotIndex < 0) {
        ALOGE("@%s, Error in grabbing snapshot frame!", __FUNCTION__);
        return BAD_INDEX;
    }

    --mNumCapturegBuffersQueued;
    LOG2("Device: %d. Grabbed frame of size: %d", mMainDevice->mId, bufInfo.vbuffer.bytesused);

    mSnapshotBuffers[snapshotIndex].id = snapshotIndex;
    mSnapshotBuffers[snapshotIndex].capture_timestamp = bufInfo.vbuffer.timestamp;
    mSnapshotBuffers[snapshotIndex].frameSequenceNbr = bufInfo.vbuffer.sequence;
    mSnapshotBuffers[snapshotIndex].status = (FrameBufferStatus)bufInfo.vbuffer.reserved;

    mSnapshotBuffers[snapshotIndex].owner = this;

    mPreviewBuffers.editItemAt(previewIndex).auxBuf = &mSnapshotBuffers[snapshotIndex];

    *buff = mPreviewBuffers[previewIndex];

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

    if (mContinuousJpegCaptureEnabled)
        return getJpegCapturePreviewFrame(buff);

    if (mHALZSLEnabled || mHALSDVEnabled) {
        return mUseMultiStreamsForSoC
                ? getMultiStreamsHALZSLPreviewFrame(buff)
                : getHALZSLPreviewFrame(buff);
    }

    int index = mPreviewDevice->grabFrame(&bufInfo);
    if(index < 0){
        ALOGE("Error in grabbing frame!");
        return BAD_INDEX;
    }

    mPreviewBuffers.editItemAt(index).id = index;
    mPreviewBuffers.editItemAt(index).expId = (bufInfo.vbuffer.reserved >> 16) & 0xFFFF;
    mPreviewBuffers.editItemAt(index).frameCounter = mPreviewDevice->getFrameCount();
    mPreviewBuffers.editItemAt(index).ispPrivate = mSessionId;
    mPreviewBuffers.editItemAt(index).capture_timestamp = bufInfo.vbuffer.timestamp;
    mPreviewBuffers.editItemAt(index).frameSequenceNbr = bufInfo.vbuffer.sequence;
    mPreviewBuffers.editItemAt(index).status = (FrameBufferStatus)(bufInfo.vbuffer.reserved & FRAME_STATUS_MASK);
    mPreviewBuffers.editItemAt(index).size = bufInfo.vbuffer.bytesused;
    mPreviewBuffers.editItemAt(index).sensorFrameId = getSensorFrameId(mPreviewBuffers.editItemAt(index).expId);

    *buff = mPreviewBuffers[index];

    mNumPreviewBuffersQueued--;

    LOG2("Device: %d. Grabbed frame of size: %d exp id:%d",
            mPreviewDevice->mId, bufInfo.vbuffer.bytesused, buff->expId);

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

status_t AtomISP::putMultiStreamsHALZSLPreviewFrame(AtomBuffer *buff)
{
    LOG2("@%s", __FUNCTION__);
    size_t size;

    Mutex::Autolock mLock(mHALZSLLock);

    // put preview
    if (mPreviewDevice->putFrame(buff->id) < 0) {
        ALOGE("@%s, mPreviewDevice, putFrame fail, id:%d", __FUNCTION__, buff->id);
        return UNKNOWN_ERROR;
    }
    mNumPreviewBuffersQueued++;
    LOG2("@%s mNumPreviewBuffersQueued:%d", __FUNCTION__, mNumPreviewBuffersQueued);

    // put snapshot
    size = mMultiStreamsHALZSLCaptureBuffersQueue.size();
    LOG2("@%s: mNewHALZSLCaptureBuffersQueue size was %d", __FUNCTION__, size);
    if (size > sMaxHALZSLBuffersHeldInHAL) {
        AtomBuffer buf = mMultiStreamsHALZSLCaptureBuffersQueue.top();
        mMultiStreamsHALZSLCaptureBuffersQueue.pop();
        LOG2("@%s: mMaindevice, putFrame:%d", __FUNCTION__, buf.id);
        if (mMainDevice->putFrame(buf.id) < 0) {
            ALOGE("@%s, mMainDevice, putFrame fail, id:%d", __FUNCTION__, buf.id);
            return UNKNOWN_ERROR;
        }
    }

    if (isPostviewInitialized()) {
        // put postview
        size = mMultiStreamsHALZSLPostviewBuffersQueue.size();
        LOG2("@%s: mMultiStreamsHALZSLPostviewBuffersQueue size was %d", __FUNCTION__, size);
        if (size > sMaxHALZSLBuffersHeldInHAL) {
            AtomBuffer buf = mMultiStreamsHALZSLPostviewBuffersQueue.top();
            mMultiStreamsHALZSLPostviewBuffersQueue.pop();
            if (mPostViewDevice->putFrame(buf.id) < 0) {
                ALOGE("@%s, mPostViewDevice, putFrame fail, id:%d", __FUNCTION__, buf.id);
                return UNKNOWN_ERROR;
            }
        }
    }

    return NO_ERROR;
}

status_t AtomISP::putJpegCapturePreviewFrame(AtomBuffer *buff)
{
    LOG2("@%s", __FUNCTION__);

    // put preview
    if (mPreviewDevice->putFrame(buff->id) < 0) {
        ALOGE("@%s, mPreviewDevice, putFrame fail, id:%d", __FUNCTION__, buff->id);
        return UNKNOWN_ERROR;
    }
    ++mNumPreviewBuffersQueued;
    LOG2("@%s mNumPreviewBuffersQueued:%d", __FUNCTION__, mNumPreviewBuffersQueued);

    return NO_ERROR;
}


status_t AtomISP::putPreviewFrame(AtomBuffer *buff)
{
    LOG2("@%s", __FUNCTION__);
    Mutex::Autolock lock(mDeviceMutex[mPreviewDevice->mId]);
    if (mMode == MODE_NONE)
        return INVALID_OPERATION;

    if (mContinuousJpegCaptureEnabled) {
        return putJpegCapturePreviewFrame(buff);
    }

    if (mHALZSLEnabled || mHALSDVEnabled) {
        return mUseMultiStreamsForSoC
                ? putMultiStreamsHALZSLPreviewFrame(buff)
                : putHALZSLPreviewFrame(buff);
    }
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
    status_t status = NO_ERROR;
    switch (buff->type) {
        case ATOM_BUFFER_PREVIEW_GFX:
        case ATOM_BUFFER_PREVIEW:
            buff->owner = 0;
            status = putPreviewFrame(buff);
            if (status != NO_ERROR) {
                ALOGE("Failed queueing preview frame!");
            }
            break;

        case ATOM_BUFFER_VIDEO:
            buff->owner = 0;
            status = putRecordingFrame(buff);
            if (status == DEAD_OBJECT) {
                ALOGW("Stale recording buffer returned to ISP");
            } else if (status != NO_ERROR) {
                ALOGE("Error putting recording frame to ISP");
            }
            break;

        case ATOM_BUFFER_SNAPSHOT:
            buff->owner = 0;
            if (mMode != MODE_CONTINUOUS_JPEG && mMode != MODE_CONTINUOUS_JPEG_VIDEO) {
                ALOGE("Capture frame return on wrong mode");
                break;
            }
            if (mMainDevice->putFrame(buff->id) < 0) {
                ALOGE("While frame return, mMainDevice putFrame fail, id:%d", buff->id);
                break;
            }
            ++mNumCapturegBuffersQueued;
            break;

        default:
            ALOGE("Received unexpected buffer!");
            break;
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

    if (!inVideoMode())
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
    mRecordingBuffers[index].expId = (buf.vbuffer.reserved >> 16) & 0xFFFF;
    mRecordingBuffers[index].frameCounter = mRecordingDevice->getFrameCount();
    mRecordingBuffers[index].ispPrivate = mSessionId;
    mRecordingBuffers[index].capture_timestamp = buf.vbuffer.timestamp;
    mRecordingBuffers[index].status = (FrameBufferStatus)(buf.vbuffer.reserved & FRAME_STATUS_MASK);
    mRecordingBuffers[index].owner = this;
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

    if (!inVideoMode())
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

    if (mContinuousJpegCaptureEnabled) {
        ALOGE("Not external snapshot buffers can use in continuous jpeg capture!");
        return INVALID_OPERATION;
    }

    mClientSnapshotBuffersCached = cached;
    mConfig.num_snapshot = numBuffs;
    mConfig.num_snapshot_buffers = numBuffs;
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

    if (mContinuousJpegCaptureEnabled) {
        ALOGE("Not external postview buffers can use in continuous jpeg capture!");
        return INVALID_OPERATION;
    }

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
    // Graphic HW have the capability to scale and colorconvert at the same time. Copy only if color format is also the same.
    // FIXME, even if buffer can be copy directly, HW copy should be faster than memcpy. So maybe we can use HW copy instead.
    if (zoomFactor == 1.0f && targetBuf->fourcc == mConfig.HALZSL.fourcc &&
            targetBuf->width == mConfig.HALZSL.width && targetBuf->height == mConfig.HALZSL.height) {
        memcpy(targetBuf->dataPtr, captureBuf.dataPtr, captureBuf.size);
    } else if (targetBuf->width == mConfig.preview.width && targetBuf->height == mConfig.preview.height &&
            previewBuf != NULL && targetBuf->fourcc == mConfig.HALZSL.fourcc) {
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

status_t AtomISP::getMultiStreamsHALZSLSnapshot(AtomBuffer *snapshotBuf, AtomBuffer *postviewBuf)
{
    LOG1("@%s", __FUNCTION__);
    Mutex::Autolock mLock(mHALZSLLock);

    if (!waitForHALZSLBuffer(mMultiStreamsHALZSLCaptureBuffersQueue, true)) {
        ALOGE("@%s no capture buffers in FIFO!", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    AtomBuffer captureBuf = mMultiStreamsHALZSLCaptureBuffersQueue.itemAt(0);
    LOG1("@%s capture buffer framecounter %d timestamp %ld.%ld", __FUNCTION__, captureBuf.frameCounter, captureBuf.capture_timestamp.tv_sec, captureBuf.capture_timestamp.tv_usec);

    *snapshotBuf = captureBuf;
    *postviewBuf = mMultiStreamsHALZSLPostviewBuffersQueue.itemAt(0);

    return OK;
}

status_t AtomISP::getSnapshot(AtomBuffer *snapshotBuf, AtomBuffer *postviewBuf)
{
    LOG1("@%s", __FUNCTION__);
    struct v4l2_buffer_info vinfo;
    int snapshotIndex = 0, postviewIndex = 0;

    if (mMode != MODE_CAPTURE && !inContinuousMode())
        return INVALID_OPERATION;

    if (postviewBuf && (mHALZSLEnabled || mHALSDVEnabled)) {
        return mUseMultiStreamsForSoC
                ? getMultiStreamsHALZSLSnapshot(snapshotBuf, postviewBuf)
                : getHALZSLSnapshot(snapshotBuf, postviewBuf);
    }

    CLEAR(vinfo);

    snapshotIndex = mMainDevice->grabFrame(&vinfo);
    if (snapshotIndex < 0) {
        ALOGE("Error in grabbing frame from 1'st device!");
        return BAD_INDEX;
    }
    LOG1("Device: %d. Grabbed frame of size: %d", V4L2_MAIN_DEVICE, vinfo.vbuffer.bytesused);
    mSnapshotBuffers[snapshotIndex].capture_timestamp = vinfo.vbuffer.timestamp;
    mSnapshotBuffers[snapshotIndex].frameSequenceNbr = vinfo.vbuffer.sequence;
    mSnapshotBuffers[snapshotIndex].status = (FrameBufferStatus)(vinfo.vbuffer.reserved & FRAME_STATUS_MASK);
    mSnapshotBuffers[snapshotIndex].expId = (vinfo.vbuffer.reserved >> 16) & 0xFFFF;
    mSnapshotBuffers[snapshotIndex].sensorFrameId = getSensorFrameId(mSnapshotBuffers[snapshotIndex].expId);

    if (isDumpRawImageReady() || postviewBuf == NULL || !isPostviewInitialized()) {
        postviewIndex = snapshotIndex;
        goto nopostview;
    }

    if (isPostviewInitialized()) {
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
        mPostviewBuffers.editItemAt(postviewIndex).status = (FrameBufferStatus)(vinfo.vbuffer.reserved & FRAME_STATUS_MASK);
    }

    if (snapshotIndex != postviewIndex ||
            snapshotIndex >= MAX_V4L2_BUFFERS) {
        ALOGE("Indexes error! snapshotIndex = %d, postviewIndex = %d", snapshotIndex, postviewIndex);
        // Return the buffers back to driver
        mMainDevice->putFrame(snapshotIndex);
        if (isPostviewInitialized()) {
            mPostViewDevice->putFrame(postviewIndex);
        }
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

    if (isPostviewInitialized() && postviewBuf) {
        mPostviewBuffers.editItemAt(postviewIndex).id = postviewIndex;
        mPostviewBuffers.editItemAt(postviewIndex).frameCounter = mPostViewDevice->getFrameCount();
        mPostviewBuffers.editItemAt(postviewIndex).ispPrivate = mSessionId;
        *postviewBuf = mPostviewBuffers[postviewIndex];
        postviewBuf->width = mConfig.postview.width;
        postviewBuf->height = mConfig.postview.height;
        postviewBuf->fourcc = mConfig.postview.fourcc;
        postviewBuf->size = mConfig.postview.size;
        postviewBuf->bpl = mConfig.postview.bpl;
    }

    mNumCapturegBuffersQueued--;

    dumpSnapshot(snapshotIndex, postviewIndex);

    LOG1("@%s buffer id:%d frameCounter:%d frameSequenceNbr:%d exp id:%d", __FUNCTION__,
            snapshotBuf->id, snapshotBuf->frameCounter, snapshotBuf->frameSequenceNbr, snapshotBuf->expId);
    return NO_ERROR;
}

status_t AtomISP::putSnapshot(AtomBuffer *snapshotBuf, AtomBuffer *postviewBuf)
{
    LOG1("@%s", __FUNCTION__);
    int ret0, ret1;

    if (mMode != MODE_CAPTURE && !inContinuousMode())
        return INVALID_OPERATION;

    if (mHALZSLEnabled || mHALSDVEnabled)
        return OK;

    if (snapshotBuf->ispPrivate != mSessionId || (postviewBuf && (postviewBuf->ispPrivate != mSessionId)))
        return DEAD_OBJECT;

    ret0 = mMainDevice->putFrame(snapshotBuf->id);

    if (mConfig.snapshot.fourcc == mSensorHW->getRawFormat() || postviewBuf == NULL || !isPostviewInitialized()) {
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
    if (inVideoMode())
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

    if (mHALZSLEnabled || mHALSDVEnabled) {
        if (mHALZSLCaptureBuffers.size() > 0)
            return 1;
    }

    return mMainDevice->poll(timeout);
}

/**
 * Send jpeg capture command to kernel
 *
 */
status_t AtomISP::requestJpegCapture()
{
    LOG1("@%s", __FUNCTION__);

    if (!mContinuousJpegCaptureEnabled) {
        ALOGE("Jpeg capture can take only in continuous jpeg capture mode!");
        return INVALID_OPERATION;
    }

    // ATOMISP_IOC_S_CONT_CAPTURE_CONFIG is used to ask jpeg capture
    return requestContCapture(1, 0, 0);
}

status_t AtomISP::startJpegModeContinuousShooting()
{
    LOG1("@%s", __FUNCTION__);

    if (!mContinuousJpegCaptureEnabled) {
        ALOGE("Jpeg continuous shooting can take only in continuous jpeg capture mode!");
        return INVALID_OPERATION;
    }

    struct atomisp_ext_isp_ctrl m10mo_ctrl;
    CLEAR(m10mo_ctrl);
    m10mo_ctrl.id = EXT_ISP_CID_CAPTURE_BURST;
    m10mo_ctrl.data = EXT_ISP_BURST_CAPTURE_CTRL_START;
    return mMainDevice->xioctl(ATOMISP_IOC_EXT_ISP_CTRL, &m10mo_ctrl);
}

status_t AtomISP::stopJpegModeContinuousShooting()
{
    LOG1("@%s", __FUNCTION__);

    if (!mContinuousJpegCaptureEnabled) {
        ALOGE("Jpeg continuous shooting can take only in continuous jpeg capture mode!");
        return INVALID_OPERATION;
    }

    struct atomisp_ext_isp_ctrl m10mo_ctrl;
    CLEAR(m10mo_ctrl);
    m10mo_ctrl.id = EXT_ISP_CID_CAPTURE_BURST;
    m10mo_ctrl.data = EXT_ISP_BURST_CAPTURE_CTRL_STOP;
    status_t status = mMainDevice->xioctl(ATOMISP_IOC_EXT_ISP_CTRL, &m10mo_ctrl);

    return status;
}

////////////////////////////////////////////////////////////////////
//                          PRIVATE METHODS
////////////////////////////////////////////////////////////////////

void AtomISP::computeZoomRatiosLinear()
{
    LOG1("@%s", __FUNCTION__);

    mZoomRatioTable.clear();
    mZoomDriveTable.clear();
    mZoomRatioTable.setCapacity(ZOOM_LINEAR_MAX_DRIVE - ZOOM_LINEAR_MIN_DRIVE);
    mZoomDriveTable.setCapacity(ZOOM_LINEAR_MAX_DRIVE - ZOOM_LINEAR_MIN_DRIVE);

    int ratio(100); // first ratio always 1x
    for (int drive = ZOOM_LINEAR_MIN_DRIVE; drive <= ZOOM_LINEAR_MAX_DRIVE; ++drive) {
        mZoomRatioTable.push(ratio);
        mZoomDriveTable.push(drive);
        ratio += ZOOM_LINEAR_RATIO_STEP;
    }
}

void AtomISP::computeZoomRatiosFactor() {
    int maxZoomFactor(PlatformData::getMaxZoomFactor(mCameraId));
    int zoomFactor(maxZoomFactor);
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
        }

        zoomFactor = zoomFactor - 1;
        if (zoomFactor == 0)
            break;
        ratio = (maxZoomFactor * ZOOM_RATIO + zoomFactor / 2) / zoomFactor;
    }
}


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
    int stringSize(0);
    int ratio(0);

    if (PlatformData::supportsContinuousJpegCapture(mCameraId)) {
        computeZoomRatiosLinear();
    } else {
        computeZoomRatiosFactor();
    }

    // calculate stringSize needed also include comma char
    for (Vector<int>::iterator it = mZoomRatioTable.begin(); it != mZoomRatioTable.end(); ++it) {
        ratio = *it;
        stringSize += 4;
        ratio = ratio / 1000;
        while (ratio) {
            stringSize += 1;
            ratio = ratio / 10;
        }
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

void AtomISP::initBufferArray(AtomBuffer *buffers, AtomBuffer &formatDescriptor, void **bufPool, int bufPoolNum)
{
    status_t status = NO_ERROR;

    for (int i = 0; i < bufPoolNum; ++i) {
        AtomBuffer *buff = &buffers[i];
        *buff = AtomBufferFactory::createAtomBuffer();
        status = MemoryUtils::allocateAtomBuffer(buffers[i], formatDescriptor, mCallbacks);
        if(status != NO_ERROR) {
            ALOGE("@%s: Failed to allocate the %i GraphicBuffer for capture!", __FUNCTION__, i);
            break;
        }

        bufPool[i] = buff->dataPtr;
        LOG1("%s, allocateAtomBuffer [%d], buff=%p size=%d", __FUNCTION__, i, buff->dataPtr, buff->size);
    }
}

status_t AtomISP::allocateMultiStreamsHALZSLBuffers()
{
    LOG1("@%s", __FUNCTION__);
    int i = 0;
    status_t status = NO_ERROR;
    void *bufPool[sNumHALZSLBuffers];

    // allocate capture buffers
    if (mMultiStreamsHALZSLCaptureBuffers == NULL) {
        mMultiStreamsHALZSLCaptureBuffers = new AtomBuffer[sNumHALZSLBuffers];
        if (NULL == mMultiStreamsHALZSLCaptureBuffers) {
            ALOGE("@%s: allocate mNewHALZSLCaptureBuffers fail", __FUNCTION__);
            return NO_MEMORY;
        }

        CLEAR(bufPool);
        initBufferArray(mMultiStreamsHALZSLCaptureBuffers, mConfig.snapshot, bufPool, sNumHALZSLBuffers);
    }

    if (status != NO_ERROR) {
        for (int j = 0; j < i; ++j)
            MemoryUtils::freeAtomBuffer(mMultiStreamsHALZSLCaptureBuffers[j]);
        delete[] mMultiStreamsHALZSLCaptureBuffers;
        mMultiStreamsHALZSLCaptureBuffers = NULL;

        return status;
    }

    mMainDevice->setBufferPool((void**)&bufPool, sNumHALZSLBuffers,
                                      &mConfig.snapshot, false);
    mConfig.num_snapshot_buffers = sNumHALZSLBuffers;

    // allocate postview buffers
    if (mMultiStreamsHALZSLPostviewBuffers == NULL) {
        mMultiStreamsHALZSLPostviewBuffers = new AtomBuffer[sNumHALZSLBuffers];
        if (NULL == mMultiStreamsHALZSLPostviewBuffers) {
            ALOGE("@%s: allocate mMultiStreamsHALZSLPostviewBuffers fail", __FUNCTION__);
            return NO_MEMORY;
        }

        CLEAR(bufPool);
        initBufferArray(mMultiStreamsHALZSLPostviewBuffers, mConfig.postview, bufPool, sNumHALZSLBuffers);
    }

    if (status != NO_ERROR) {
        for (int j = 0; j < sNumHALZSLBuffers; ++j)
            MemoryUtils::freeAtomBuffer(mMultiStreamsHALZSLCaptureBuffers[j]);
        delete[] mMultiStreamsHALZSLCaptureBuffers;
        mMultiStreamsHALZSLCaptureBuffers = NULL;

        for (int j = 0; j < i; ++j)
            MemoryUtils::freeAtomBuffer(mMultiStreamsHALZSLPostviewBuffers[j]);
        delete[] mMultiStreamsHALZSLPostviewBuffers;
        mMultiStreamsHALZSLPostviewBuffers = NULL;

        return status;
    }

    if (isPostviewInitialized()) {
        mPostViewDevice->setBufferPool((void**)&bufPool, sNumHALZSLBuffers,
                                          &mConfig.postview, false);
        mConfig.num_postview_buffers = sNumHALZSLBuffers;
    }

    return status;
}

status_t AtomISP::freeMultiStreamsHALZSLBuffers()
{
    LOG1("@%s", __FUNCTION__);
    Mutex::Autolock mLock(mHALZSLLock);

    if (mMultiStreamsHALZSLCaptureBuffers) {
        for (int i = 0 ; i < sNumHALZSLBuffers; i++)
            MemoryUtils::freeAtomBuffer(mMultiStreamsHALZSLCaptureBuffers[i]);
        delete[] mMultiStreamsHALZSLCaptureBuffers;
        mMultiStreamsHALZSLCaptureBuffers = NULL;
    }

    if (mMultiStreamsHALZSLPostviewBuffers) {
        for (int i = 0 ; i < sNumHALZSLBuffers; i++)
            MemoryUtils::freeAtomBuffer(mMultiStreamsHALZSLPostviewBuffers[i]);
        delete[] mMultiStreamsHALZSLPostviewBuffers;
        mMultiStreamsHALZSLPostviewBuffers = NULL;
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
            /* Graphic don't support UYUV format for some platforms */
            if (V4L2_PIX_FMT_UYVY == mConfig.preview.fourcc)
                MemoryUtils::allocateAtomBuffer(tmp, mConfig.preview, mCallbacks);
            else
                MemoryUtils::allocateGraphicBuffer(tmp, mConfig.preview);
            if (tmp.dataPtr == NULL) {
                ALOGE("Error allocation memory for preview buffers!");
                status = NO_MEMORY;
                goto errorFree;
            }
            bufPool[i] = tmp.dataPtr;
            LOG2("allocate preview buffer[%d], buff=%p size=%d", i, tmp.dataPtr, tmp.size);

            if ((mHALZSLEnabled || mHALSDVEnabled) && (false == mUseMultiStreamsForSoC)) {
                mScaler->registerBuffer(tmp, ScalerService::SCALER_OUTPUT);
                mHALZSLPreviewBuffers.push(tmp);
            }
            mPreviewBuffers.push(tmp);
        }

    } else {
        for (size_t i = 0; i < mPreviewBuffers.size(); i++) {
            bufPool[i] = mPreviewBuffers[i].dataPtr;
            LOG2("preview buffer[%d], buff=%p size=%d", i, mPreviewBuffers[i].dataPtr, mPreviewBuffers[i].size);
            mPreviewBuffers.editItemAt(i).shared = true;
            if ((mHALZSLEnabled || mHALSDVEnabled) && (false == mUseMultiStreamsForSoC)) {
                mScaler->registerBuffer(mPreviewBuffers.editItemAt(i), ScalerService::SCALER_OUTPUT);
                mHALZSLPreviewBuffers.push(mPreviewBuffers[i]);
            }
        }
    }

    if ((mHALZSLEnabled || mHALSDVEnabled) && (false == mUseMultiStreamsForSoC)) {
        status = allocateHALZSLBuffers();
        if (status != OK) {
            ALOGE("Error allocation memory for HAL ZSL buffers!");
            goto errorFree;
        }
    } else {
        mPreviewDevice->setBufferPool((void**)&bufPool, mPreviewBuffers.size(),
                                      &mConfig.preview, mPreviewBuffersCached);
    }

    if ((mHALZSLEnabled || mHALSDVEnabled) && mUseMultiStreamsForSoC) {
        status = allocateMultiStreamsHALZSLBuffers();
        if (status != OK) {
            ALOGE("Error allocation memory for HAL ZSL buffers!");
            goto errorFree;
        }
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
        if (PlatformData::getTotalRam() > MEM_2G)
            MemoryUtils::allocateGraphicBuffer(mRecordingBuffers[i], mConfig.recording);
        else
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
            if (!mHALZSLEnabled && !mHALSDVEnabled) {
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
    if (!mHALZSLEnabled && !mHALSDVEnabled)
        mMainDevice->setBufferPool((void**)&bufPool,mConfig.num_snapshot,
                                   &mConfig.snapshot, mClientSnapshotBuffersCached);

    if (mUsingClientPostviewBuffers) {
        LOG1("@%s using %d client postview buffers",__FUNCTION__, mConfig.num_postviews);
        for (int i = 0; i < mConfig.num_postviews; ++i) {
            mPostviewBuffers.editItemAt(i).type = ATOM_BUFFER_POSTVIEW;
            mPostviewBuffers.editItemAt(i).shared = false;

            if (!mHALZSLEnabled && !mHALSDVEnabled) {
                bufPool[i] = mPostviewBuffers[i].dataPtr;
            }
        }
    } else if (mContinuousJpegCaptureEnabled) {
        LOG1("@%s no postview needed for continuous JPEG capture", __FUNCTION__);
    } else if (needNewPostviewBuffers()) {
        // TODO: Remove this allocation stuff, it is done in PictureThread now...
        freePostviewBuffers();
        AtomBuffer postv = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_POSTVIEW);
        for (int i = 0; i < mConfig.num_snapshot; i++) {
            postv.buff = NULL;
            postv.size = 0;
            postv.dataPtr = NULL;
            if (mHALZSLEnabled || mHALSDVEnabled) {
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
            if (mHALZSLEnabled || mHALSDVEnabled)
                mScaler->registerBuffer(postv, ScalerService::SCALER_OUTPUT);

            mPostviewBuffers.push(postv);
        }
    } else {
        for (size_t i = 0; i < mPostviewBuffers.size(); i++) {
            bufPool[i] = mPostviewBuffers[i].dataPtr;
        }
    }
    // In case of Raw capture we do not get postview, so no point in setting up the pool
    if (isPostviewInitialized() && !mContinuousJpegCaptureEnabled && !mHALZSLEnabled && !mHALSDVEnabled && !isBayerFormat(mConfig.snapshot.fourcc))
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

status_t AtomISP::setColorBarPattern(bool enable)
{
    LOG1("@%s: %d", __FUNCTION__, (int) enable);

    mMainDevice->setControl(V4L2_CID_TEST_PATTERN, enable, "SetColorBarPattern");

    return NO_ERROR;
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

status_t AtomISP::allocateMetaDataBuffers(AtomBuffer *buffers, int numBuffers)
{
    LOG1("@%s", __FUNCTION__);

    if (buffers == NULL || numBuffers <= 0)
        return BAD_VALUE;

    status_t status = NO_ERROR;
#ifdef ENABLE_INTEL_METABUFFER
    int allocatedBufs = 0;
    uint8_t* meta_data_prt;
    uint32_t meta_data_size;
    IntelMetadataBuffer* metaDataBuf = NULL;

    for (int i = 0; i < numBuffers; i++) {
        metaDataBuf = new IntelMetadataBuffer();
        if(metaDataBuf) {
            if (PlatformData::isGraphicGen()) {
                metaDataBuf->SetType(IntelMetadataBufferTypeGrallocSource);
                metaDataBuf->SetValue((uint32_t)*buffers[i].gfxInfo_rec.gfxBufferHandle);
            } else {
                // TODO: Safe to combine to upper-level if-else ?
                if (mHALVideoStabilization || mHALVideoNormal) {
                    metaDataBuf->SetType(IntelMetadataBufferTypeGrallocSource);
                    metaDataBuf->SetValue((uint32_t)*buffers[i].gfxInfo.gfxBufferHandle);
                } else if (mExtIspVideoHighSpeed) {
                    initMetaDataBuf(metaDataBuf);
                    metaDataBuf->SetValue((uint32_t)buffers[i].dataPtr);
                } else {
                    initMetaDataBuf(metaDataBuf);
#ifdef INTEL_VIDEO_XPROC_SHARING
                    if (PlatformData::getTotalRam() > MEM_2G) {
                        metaDataBuf->SetValue((uint32_t)buffers[i].dataPtr);
                    } else {
                        // for cross-process sharing
                        metaDataBuf->SetSessionFlag(mBufferSharingSessionID);
                        sp<CameraHeapMemory> mem(static_cast<CameraHeapMemory *>(buffers[i].buff->handle));
                        metaDataBuf->ShareValue(mem->mBuffers[0]);
                    }
#else
                    // for video recording only
                    metaDataBuf->SetValue((uint32_t)buffers[i].dataPtr);
#endif // INTEL_VIDEO_XPROC_SHARING
                }
            }
            metaDataBuf->Serialize(meta_data_prt, meta_data_size);
            MemoryUtils::allocateAtomBufferMetadata(buffers[i], meta_data_size, mCallbacks);
            LOG1("allocate metadata buffer[%d]  buff=%p size=%d sID:%d",
                i, buffers[i].metadata_buff->data,
                buffers[i].metadata_buff->size, mBufferSharingSessionID);
            if (buffers[i].metadata_buff == NULL) {
                ALOGE("Error allocation memory for metadata buffers!");
                status = NO_MEMORY;
                goto errorFree;
            }
            memcpy(buffers[i].metadata_buff->data, meta_data_prt, meta_data_size);
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
        MemoryUtils::freeAtomBuffer(buffers[i]);

    if (metaDataBuf) {
        delete metaDataBuf;
        metaDataBuf = NULL;
    }
#endif // ENABLE_INTEL_METABUFFER
    return status;

}

status_t AtomISP::allocateMetaDataBuffers()
{
    LOG1("@%s", __FUNCTION__);

    if (mHALVideoStabilization) {
        return OK; // halVS allocates metadatabuffers on the fly to preview bufs
    }

#ifdef ENABLE_INTEL_METABUFFER
    if(mRecordingBuffers) {
        for (int i = 0 ; i < mConfig.num_recording_buffers; i++) {
            MemoryUtils::freeAtomBufferMetadata(mRecordingBuffers[i]);
#ifdef INTEL_VIDEO_XPROC_SHARING
            IntelMetadataBuffer::ClearContext(mBufferSharingSessionID, true);
#else
            if (PlatformData::getTotalRam() > MEM_2G)
                IntelMetadataBuffer::ClearContext(mBufferSharingSessionID, true);
#endif
        }
    } else {
        ALOGE("mRecordingBuffers is not ready, so it's invalid to allocate metadata buffers");
        return INVALID_OPERATION;
    }
#endif
    return allocateMetaDataBuffers(mRecordingBuffers, mConfig.num_recording_buffers);
}

status_t AtomISP::freePreviewBuffers()
{
    LOG1("@%s", __FUNCTION__);

    if (!mPreviewBuffers.isEmpty()) {
        for (size_t i = 0 ; i < mPreviewBuffers.size(); i++) {
            LOG1("@%s mHALZSLEnabled = %d i=%d, mNum = %d", __FUNCTION__, mHALZSLEnabled, i, mConfig.num_preview_buffers);
            if ((mHALZSLEnabled || mHALSDVEnabled) && (false == mUseMultiStreamsForSoC))
                mScaler->unRegisterBuffer(mPreviewBuffers.editItemAt(i), ScalerService::SCALER_OUTPUT);

            MemoryUtils::freeAtomBuffer(mPreviewBuffers.editItemAt(i));
        }
        mPreviewBuffers.clear();
    }

    if (mHALZSLEnabled || mHALSDVEnabled) {
        if (mUseMultiStreamsForSoC)
            freeMultiStreamsHALZSLBuffers();
        else {
            freeHALZSLBuffers();
            mHALZSLPreviewBuffers.clear();
        }
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
#else
        if (PlatformData::getTotalRam() > MEM_2G)
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
    if (mHALZSLEnabled || mHALSDVEnabled)
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
    PlatformData::HalConfig[mCameraId].getValue(num_skipstats, CPF::Statistics, CPF::InitialSkip);

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


    ret = pxioctl(mMainDevice, ATOMISP_IOC_ACC_LOAD, &fwData);
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
                             unsigned int *fwHandle,
                             int destination)
{
    LOG1("@%s", __FUNCTION__);
    int ret = -1;

    struct atomisp_acc_fw_load_to_pipe fwDataPipe;
    memset(&fwDataPipe, 0, sizeof(fwDataPipe));

    switch(destination) {
        case CAPTURE_OUTPUT:
            fwDataPipe.flags = ATOMISP_ACC_FW_LOAD_FL_CAPTURE;
            fwDataPipe.type = ATOMISP_ACC_FW_LOAD_TYPE_OUTPUT;
            break;
        case CAPTURE_VFPP:
            fwDataPipe.flags = ATOMISP_ACC_FW_LOAD_FL_CAPTURE;
            fwDataPipe.type = ATOMISP_ACC_FW_LOAD_TYPE_VIEWFINDER;
            break;
        case PREVIEW_VFPP:
            fwDataPipe.flags = ATOMISP_ACC_FW_LOAD_FL_PREVIEW;
            fwDataPipe.type = ATOMISP_ACC_FW_LOAD_TYPE_VIEWFINDER;
            break;
        case ACC_QOS:
            fwDataPipe.flags = ATOMISP_ACC_FW_LOAD_FL_ACC;
            fwDataPipe.type = ATOMISP_ACC_FW_LOAD_TYPE_VIEWFINDER;
            break;
        default:
            ALOGE("@%s: Invalid acc destination", __FUNCTION__);
            return BAD_VALUE;
            break;
    }

    /*  fwDataPipe.fw_handle filled by kernel and returned to caller */
    fwDataPipe.size = size;
    fwDataPipe.data = fw;

    ret = pxioctl(mMainDevice, ATOMISP_IOC_ACC_LOAD_TO_PIPE, &fwDataPipe);
    LOG1("%s IOCTL ATOMISP_IOC_ACC_LOAD_TO_PIPE ret: %d fwDataPipe->fw_handle: %d "
         "flags: %d type: %d", __FUNCTION__, ret, fwDataPipe.fw_handle,
         fwDataPipe.flags, fwDataPipe.type);

    //If IOCTL call was returned successfully, get the firmware handle
    //from the structure and return it to the application.
    if(!ret){
        *fwHandle = fwDataPipe.fw_handle;
        LOG1("%s IOCTL Call returned : %d Handle: %ud",
                __FUNCTION__, ret, *fwHandle );

        if (fwDataPipe.flags == ATOMISP_ACC_FW_LOAD_FL_ACC) {
            ret = newAccPipeFw(fwDataPipe.fw_handle);
        }
    }

    return ret;
}

int AtomISP::waitStageUpdate(unsigned int handle)
{
    LOG2("@%s: ATOMISP_IOC_S_ACC_STATE handle: %x", __FUNCTION__, handle);
    status_t status = OK;

    mACCEventSystemLock.lock(); // this must be held while using the vectors
    int updateIndex = mUpdates.indexOfKey(handle);
    if (updateIndex >= 0) {
        // found pending update, remove it from vector, then unlock and return
        mUpdates.removeItemsAt(updateIndex, 1);
    } else {
        // find the waiter object and sleep against its condition
        int waiterIndex = mAllWaiters.indexOfKey(handle);
        if (waiterIndex >= 0) {
            EventWaiter *waiter = mAllWaiters.valueAt(waiterIndex);
            if (waiter) { // to keep static code analyzers happy
                // add this waiter object to sleeping waiter vector
                mSleepingWaiters.add(handle, waiter);
                waiter->mWaitLock.lock(); // to ensure no signals are too early
                mACCEventSystemLock.unlock(); // can't hold this during the cond wait
                waiter->mWaitCond.wait(waiter->mWaitLock);
                mACCEventSystemLock.lock(); // remove the sleeper from vector
                mSleepingWaiters.removeItem(handle);
                mACCEventSystemLock.unlock();
                waiter->mWaitLock.unlock();
                return OK;
            }
        } else {
            ALOGE("Unknown waiter handle %x given", handle);
            status = BAD_VALUE;
        }
    }
    mACCEventSystemLock.unlock();
    return status;
}

status_t AtomISP::stageUpdate(unsigned int handle)
{
    LOG2("@%s: update handle: %x", __FUNCTION__, handle);

    Mutex::Autolock lock(mACCEventSystemLock);

    // first check if stage is disabled, drop the update if it is
    int index = mAllWaiters.indexOfKey(handle);
    if (index >= 0) {
        EventWaiter *waiter = mAllWaiters.valueAt(index);
        if (waiter) {// to keep static code analyzers happy
            if (!waiter->enabled) {
                ALOGW("Stage update happened for a disabled stage. Dropping it, maybe scheduling was unlucky.");
                return OK; // drop the update
            }
        }
    } else {
        ALOGE("Unknown stage!");
        return BAD_VALUE;
    }

    index = mSleepingWaiters.indexOfKey(handle);
    if (index >= 0) {
        EventWaiter *waiter = mSleepingWaiters.valueAt(index);
        if (waiter) {// to keep static code analyzers happy
            Mutex::Autolock waiterLock(waiter->mWaitLock); // to ensure no signals are too early
            waiter->mWaitCond.signal(); // wake up the sleeping thread
        }
    } else {
        mUpdates.add(handle, handle);
        if (mUpdates.size() > MAX_NUMBER_PENDING_UPDATES) {
            ALOGW("Pending acc stage updates now already: %d", mUpdates.size());
        }
    }
    return OK;
}

status_t AtomISP::newAccPipeFw(unsigned int handle)
{
    LOG1("@%s: handle: %x", __FUNCTION__, handle);

    status_t status = OK;
    EventWaiter *waiter = new EventWaiter();

    Mutex::Autolock lock(mACCEventSystemLock);
    if (waiter) {// for code analyzers
        waiter->handle = handle;
        mAllWaiters.add(handle, waiter);
    } else {
        ALOGE("Not enough memory to allocate event waiter");
        return NO_MEMORY;
    }

    return status;
}

/*
 * Drops pending stage updates for the given fw handle. Takes the
 * mACCEventSystemLock lock.
 */
void AtomISP::dropACCStageUpdatesLocked(unsigned int fwHandle)
{
    LOG1("@ %s fw_Handle: %d\n",__FUNCTION__, fwHandle);
    Mutex::Autolock lock(mACCEventSystemLock);
    dropACCStageUpdates(fwHandle);
}

/*
 * Drops pending stage updates for the given fw handle. Caller must take
 * the mACCEventSystemLock itself!
 */
void AtomISP::dropACCStageUpdates(unsigned int fwHandle)
{
    LOG1("@ %s fw_Handle: %d\n",__FUNCTION__, fwHandle);
    int index = -1;
    do {
        index = mUpdates.indexOfKey(fwHandle);
        if (index >= 0)
            mUpdates.removeItemsAt(index, 1);
    } while (index >= 0);
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

    // remove acc event waiter for the stage, if it exists
    Mutex::Autolock lock(mACCEventSystemLock);
    int index = mAllWaiters.indexOfKey(fwHandle);
    if (index >= 0) {
        EventWaiter *waiter = mAllWaiters.editValueAt(index);
        // unsubscribe v4l2 events for this fw handle
        if (waiter)
            m3AEventSubdevice->unsubscribeEvent(V4L2_EVENT_ATOMISP_ACC_COMPLETE, fwHandle);

        mAllWaiters.removeItem(fwHandle);
        delete waiter;
        waiter = NULL;
        dropACCStageUpdates(fwHandle);
    }

    ret = pxioctl(mMainDevice, ATOMISP_IOC_ACC_UNLOAD, &fwHandle);
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

    ret =  pxioctl(mMainDevice, ATOMISP_IOC_ACC_MAP, &map);
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

    ret =  pxioctl(mMainDevice, ATOMISP_IOC_ACC_UNMAP, &map);
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

    ret = pxioctl(mMainDevice, ATOMISP_IOC_ACC_S_ARG, &arg);
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

    ret = pxioctl(mMainDevice, ATOMISP_IOC_ACC_S_MAPPED_ARG, &arg);
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

    ret = pxioctl(mMainDevice, ATOMISP_IOC_ACC_DESTAB, &arg);
    LOG1("%s IOCTL ATOMISP_IOC_ACC_DESTAB ret: %d \n", __FUNCTION__, ret);

    return ret;
}

int AtomISP::startFirmware(unsigned int fwHandle)
{
    int ret;
    ret = pxioctl(mMainDevice, ATOMISP_IOC_ACC_START, &fwHandle);
    LOG1("%s IOCTL ATOMISP_IOC_ACC_START ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int AtomISP::setStageState(unsigned int fwHandle, bool enable)
{
    LOG1("@%s", __FUNCTION__);

    // set the stage struct internal state first
    Mutex::Autolock lock(mACCEventSystemLock);
    int index = mAllWaiters.indexOfKey(fwHandle);
    if (index >= 0) {
        EventWaiter *waiter = mAllWaiters.valueAt(index);
        if (waiter) {// to keep static code analyzers happy
            waiter->enabled = enable;
        }
    } else {
        ALOGE("Bad ACC fwHandle given!");
        return BAD_VALUE;
    }

    atomisp_acc_state accState;
    accState.fw_handle = fwHandle;
    accState.flags = enable ? ATOMISP_STATE_FLAG_ENABLE : 0;
    int ret = pxioctl(mMainDevice, ATOMISP_IOC_S_ACC_STATE, &accState);
    LOG1("@%s: ATOMISP_IOC_S_ACC_STATE handle:%x, enable:%d ret:%d", __FUNCTION__, fwHandle, enable, ret);

    if (!enable)
        dropACCStageUpdates(fwHandle);

    return ret;
}

int AtomISP::waitForFirmware(unsigned int fwHandle)
{
    int ret;
    ret = pxioctl(mMainDevice, ATOMISP_IOC_ACC_WAIT, &fwHandle);
    LOG1("%s IOCTL ATOMISP_IOC_ACC_WAIT ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int AtomISP::abortFirmware(unsigned int fwHandle, unsigned int timeout)
{
    int ret;
    atomisp_acc_fw_abort abort;

    abort.fw_handle = fwHandle;
    abort.timeout = timeout;

    ret = pxioctl(mMainDevice, ATOMISP_IOC_ACC_ABORT, &abort);
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
    if (mStoreMetaDataInBuffers && inVideoMode()) {
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
#else
            if (PlatformData::getTotalRam() > MEM_2G)
                IntelMetadataBuffer::ClearContext(mBufferSharingSessionID, true);
#endif
        }
    }
    return status;
}


int AtomISP::dumpFrameInfo(AtomMode mode)
{
    LOG2("@%s", __FUNCTION__);

    if (gPerfLevel & CAMERA_DEBUG_LOG_PERF_TRACES) {
        /**
         * XXX: Don't forget to adjust the array size when adding support
         * for more modes; the extra +1 is to compensate for the enum
         * beginning at -1 rather than at 0
         */
        const char *previewMode[MODE_MAX+2]={"NoPreview",
                                             "Preview",
                                             "Capture",
                                             "Video",
                                             "ContinuousCapture",
                                             "ContinuousVideo",
                                             "ContinuousJPEG",
                                             "ContinuousJPEGVideo",
                                             "Unknown mode"};

        // Sanity check
        if (mode > MODE_MAX)
            mode = MODE_MAX;

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
        if (inVideoMode())
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

    LOG2("@%s: V4L2_CID_FOCUS_ABSOLUTE = %d", __FUNCTION__, position);
    if (!PlatformData::isFixedFocusCamera(mCameraId))
        return mMainDevice->setControl(V4L2_CID_FOCUS_ABSOLUTE, position, "Set focus position");
    else
        return -1;
}

int AtomISP::moveFocusToBySteps(int steps)
{
    int val = 0, rval;
    LOG2("@%s", __FUNCTION__);
    if (!PlatformData::isFixedFocusCamera(mCameraId)) {
        rval = mMainDevice->getControl(V4L2_CID_FOCUS_ABSOLUTE, &val);
        if (rval)
            return rval;
        return moveFocusToPosition(val + steps);
    } else {
        return -1;
    }
}

int AtomISP::getFocusPosition(int * position)
{
    LOG2("@%s", __FUNCTION__);
    if (!PlatformData::isFixedFocusCamera(mCameraId))
        return mMainDevice->getControl(V4L2_CID_FOCUS_ABSOLUTE , position);
    else
        return -1;
}

int AtomISP::getFocusStatus(int *status)
{
    LOG2("@%s", __FUNCTION__);
    if (!PlatformData::isFixedFocusCamera(mCameraId))
        return mMainDevice->getControl(V4L2_CID_FOCUS_STATUS, status);
    else
        return -1;
}

int AtomISP::getModeInfo(struct atomisp_sensor_mode_data *mode_data)
{
    LOG2("@%s", __FUNCTION__);
    int ret;
    ret = pxioctl(mMainDevice, ATOMISP_IOC_G_SENSOR_MODE_DATA, mode_data);
    LOG2("%s IOCTL ATOMISP_IOC_G_SENSOR_MODE_DATA ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int AtomISP::setExposure(struct atomisp_exposure *exposure)
{
    int ret;
    ret = pxioctl(mMainDevice, ATOMISP_IOC_S_EXPOSURE, exposure);
    LOG2("%s IOCTL ATOMISP_IOC_S_EXPOSURE ret: %d, gain %d, citg %d\n", __FUNCTION__, ret, exposure->gain[0], exposure->integration_time[0]);
    return ret;
}

int AtomISP::setExposureTime(int time)
{
    LOG2("@%s", __FUNCTION__);

    return mMainDevice->setControl(V4L2_CID_EXPOSURE_ABSOLUTE, time, "Exposure time");
}

int AtomISP::getExposureTime(int *time)
{
    LOG2("@%s", __FUNCTION__);
    return mMainDevice->getControl(V4L2_CID_EXPOSURE_ABSOLUTE, time);
}

int AtomISP::getAperture(int *aperture)
{
    LOG2("@%s", __FUNCTION__);
    return mMainDevice->getControl(V4L2_CID_IRIS_ABSOLUTE, aperture);
}

int AtomISP::getFNumber(unsigned short *fnum_num, unsigned short *fnum_denom)
{
    LOG2("@%s", __FUNCTION__);
    int fnum = 0, ret;

    ret = mMainDevice->getControl(V4L2_CID_FNUMBER_ABSOLUTE, &fnum);

    *fnum_num = (unsigned short)(fnum >> 16);
    *fnum_denom = (unsigned short)(fnum & 0xFFFF);
    return ret;
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

    ret = pxioctl(mMainDevice, ATOMISP_IOC_S_PARAMETERS, aic_param);
    LOG2("%s IOCTL ATOMISP_IOC_S_PARAMETERS ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int AtomISP::setIspParameter(struct atomisp_parm *isp_param)
{
    LOG2("@%s", __FUNCTION__);
    int ret;
    ret = pxioctl(mMainDevice, ATOMISP_IOC_S_ISP_PARM, isp_param);
    LOG2("%s IOCTL ATOMISP_IOC_S_ISP_PARM ret: %d\n", __FUNCTION__, ret);
    return ret;
}

status_t AtomISP::getDecodedExposureParams(ia_aiq_exposure_sensor_parameters* sensor_exp_p,
                                            ia_aiq_exposure_parameters* generic_exp_p, unsigned int exp_id)
{
    LOG2("@%s", __FUNCTION__);
    status_t ret = UNKNOWN_ERROR;
    if (mSensorEmbeddedMetaData)
        ret = mSensorEmbeddedMetaData->getDecodedExposureParams(sensor_exp_p, generic_exp_p, exp_id);

    return ret;
}

int AtomISP::getSensorFrameId(unsigned int exp_id)
{
    LOG2("@%s", __FUNCTION__);
    status_t ret = UNKNOWN_ERROR;
    int sensorFrameId = -1;
    if (mSensorEmbeddedMetaData) {
        ia_emd_misc_parameters_t misc_parameters;
        memset(&misc_parameters, 0, sizeof(ia_emd_misc_parameters_t));
        ret = mSensorEmbeddedMetaData->getDecodedMiscParams(&misc_parameters, exp_id);
        if (ret == NO_ERROR)
            sensorFrameId = misc_parameters.frame_counter;
    }
    return sensorFrameId;
}

/**
 * Retreive 3A statistics from driver
 */
int AtomISP::getIspStatistics(struct atomisp_3a_statistics *statistics)
{
    LOG2("@%s", __FUNCTION__);

    int ret = 0;
    ret = pxioctl(mMainDevice, ATOMISP_IOC_G_3A_STAT, statistics);
    LOG2("%s IOCTL ATOMISP_IOC_G_3A_STAT ret: %d\n", __FUNCTION__, ret);

    if (ret == 0 && isOfflineCaptureRunning()) {
        // Detect the corrupt stats only for offline (continuous) capture.
        // TODO: This hack to be removed, when BZ #119181 is fixed
        ret = detectCorruptStatistics(statistics);
    }

    return ret;
}

int AtomISP::getSensorEmbeddedMetaData(atomisp_metadata *metaData) const
{
    LOG2("@%s", __FUNCTION__);
    int ret;
    ret = pxioctl(mMainDevice, ATOMISP_IOC_G_METADATA, metaData);
    LOG2("%s IOCTL ATOMISP_IOC_G_METADATA ret: %d\n", __FUNCTION__, ret);
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
    ret = pxioctl(mMainDevice, ATOMISP_IOC_S_ISP_MACC,macc_tbl);
    LOG2("%s IOCTL ATOMISP_IOC_S_ISP_MACC ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int AtomISP::setGammaTable(const struct atomisp_gamma_table *gamma_tbl)
{
    LOG2("@%s", __FUNCTION__);
    int ret;
    ret = pxioctl(mMainDevice, ATOMISP_IOC_S_ISP_GAMMA, (struct atomisp_gamma_table *)gamma_tbl);
    LOG2("%s IOCTL ATOMISP_IOC_S_ISP_GAMMA ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int AtomISP::setCtcTable(const struct atomisp_ctc_table *ctc_tbl)
{
    LOG2("@%s", __FUNCTION__);
    int ret;
    ret = pxioctl(mMainDevice, ATOMISP_IOC_S_ISP_CTC, (struct atomisp_ctc_table *)ctc_tbl);
    LOG2("%s IOCTL ATOMISP_IOC_S_ISP_CTC ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int AtomISP::setNarrowGamma(const struct atomisp_formats_config *formats_config)
{
    LOG2("@%s", __FUNCTION__);
    int ret;
    ret = pxioctl(mMainDevice, ATOMISP_IOC_S_FORMATS_CONFIG, (struct atomisp_formats_config *)formats_config);
    LOG2("%s ATOMISP_IOC_S_FORMATS_CONFIG ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int AtomISP::setGdcConfig(const struct morph_table *tbl)
{
    LOG2("@%s", __FUNCTION__);
    int ret;
    ret = pxioctl(mMainDevice, ATOMISP_IOC_S_ISP_GDC_TAB, (struct morph_table *)tbl);
    LOG2("%s IOCTL ATOMISP_IOC_S_ISP_GDC_TAB ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int AtomISP::setShadingTable(struct atomisp_shading_table *table)
{
    LOG2("@%s", __FUNCTION__);
    int ret;
    ret = pxioctl(mMainDevice, ATOMISP_IOC_S_ISP_SHD_TAB, table);
    LOG2("%s IOCTL ATOMISP_IOC_S_ISP_SHD_TAB ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int AtomISP::setDeConfig(struct atomisp_de_config *de_cfg)
{
    LOG2("@%s", __FUNCTION__);
    int ret;
    ret = pxioctl(mMainDevice, ATOMISP_IOC_S_ISP_FALSE_COLOR_CORRECTION, de_cfg);
    LOG2("%s IOCTL ATOMISP_IOC_S_ISP_FALSE_COLOR_CORRECTION ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int AtomISP::setTnrConfig(struct atomisp_tnr_config *tnr_cfg)
{
    LOG2("@%s", __FUNCTION__);
    int ret;
    ret = pxioctl(mMainDevice, ATOMISP_IOC_S_TNR, tnr_cfg);
    LOG2("%s IOCTL ATOMISP_IOC_S_TNR ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int AtomISP::setEeConfig(struct atomisp_ee_config *ee_cfg)
{
    LOG2("@%s", __FUNCTION__);
    int ret;
    ret = pxioctl(mMainDevice, ATOMISP_IOC_S_EE, ee_cfg);
    LOG2("%s IOCTL ATOMISP_IOC_S_EE ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int AtomISP::setNrConfig(struct atomisp_nr_config *nr_cfg)
{
    LOG2("@%s", __FUNCTION__);
    int ret;
    ret = pxioctl(mMainDevice, ATOMISP_IOC_S_NR, nr_cfg);
    LOG2("%s IOCTL ATOMISP_IOC_S_NR ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int AtomISP::setDpConfig(struct atomisp_dp_config *dp_cfg)
{
    LOG2("@%s", __FUNCTION__);
    int ret;
    ret = pxioctl(mMainDevice, ATOMISP_IOC_S_ISP_BAD_PIXEL_DETECTION, dp_cfg);
    LOG2("%s IOCTL ATOMISP_IOC_S_ISP_BAD_PIXEL_DETECTION ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int AtomISP::setWbConfig(struct atomisp_wb_config *wb_cfg)
{
    LOG2("@%s", __FUNCTION__);
    int ret;
    ret = pxioctl(mMainDevice, ATOMISP_IOC_S_ISP_WHITE_BALANCE, wb_cfg);
    LOG2("%s IOCTL ATOMISP_IOC_S_ISP_WHITE_BALANCE ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int AtomISP::set3aConfig(const struct atomisp_3a_config *cfg)
{
    LOG2("@%s", __FUNCTION__);
    int ret;
    ret = pxioctl(mMainDevice, ATOMISP_IOC_S_3A_CONFIG, (struct atomisp_3a_config *)cfg);
    LOG2("%s IOCTL ATOMISP_IOC_S_3A_CONFIG ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int AtomISP::setObConfig(struct atomisp_ob_config *ob_cfg)
{
    LOG2("@%s", __FUNCTION__);
    int ret;
    ret = pxioctl(mMainDevice, ATOMISP_IOC_S_BLACK_LEVEL_COMP, ob_cfg);
    LOG2("%s IOCTL ATOMISP_IOC_S_BLACK_LEVEL_COMP ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int AtomISP::setGcConfig(const struct atomisp_gc_config *gc_cfg)
{
    LOG2("@%s", __FUNCTION__);
    int ret;
    ret = pxioctl(mMainDevice, ATOMISP_IOC_S_ISP_GAMMA_CORRECTION, (struct atomisp_gc_config *)gc_cfg);
    LOG2("%s IOCTL ATOMISP_IOC_S_ISP_GAMMA_CORRECTION ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int AtomISP::setDvsConfig(const struct atomisp_dvs_6axis_config *dvs_6axis_cfg)
{
    LOG2("@%s", __FUNCTION__);
    int ret;
    ret = pxioctl(mMainDevice, ATOMISP_IOC_S_DIS_VECTOR, (struct atomisp_dvs_6axis_config *)dvs_6axis_cfg);
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

/**
 * Returns the ISP output frame size for main pipeline
 * \param[out] width width of the output frame
 * \param[out] height height of the output frame
 * \param[out] output frame bytes-per-line (optional)
 *
 * Note: returns zero width and height if HAL ZSL is currently configured
 */
void AtomISP::getOutputSize(int *width, int *height, int *bpl)
{
    LOG1("@%s", __FUNCTION__)
    // check which value to return if mSwapRecordingDevice = true
    int widthTmp = 0, heightTmp = 0, bplTmp = 0;

    if (width == NULL || height == NULL) {
        ALOGE("%s: NULL width or height output parameter", __FUNCTION__);
        return;
    }

    if (mConfig.HALZSL.width != 0 && mConfig.HALZSL.height != 0) {
        ALOGW("Requesting ISP pipeline output size, while HAL ZSL configured (GPU output).");
        widthTmp = mConfig.HALZSL.width;
        heightTmp = mConfig.HALZSL.height;
        bplTmp =  mConfig.HALZSL.bpl;
    } else if (mSwapRecordingDevice) {
        // Device configurations swapped, see Workaroud 2 in applyISPLimitations().
        // Return preview device informaton as main device info
        widthTmp = mConfig.preview.width;
        heightTmp = mConfig.preview.height;
        bplTmp = mConfig.preview.bpl;
    } else if (!mSwapRecordingDevice) {
        // Normal device configuration
        widthTmp = mConfig.recording.width;
        heightTmp = mConfig.recording.height;
        bplTmp = mConfig.recording.bpl;
    } else {
        ALOGE("%s: Invalid ISP pipeline output size query.", __FUNCTION__);
    }

    *width = widthTmp;
    *height = heightTmp;

    if (bpl)
        *bpl = bplTmp;
}

/**
 * \param intensity value between 0-100
 * \param flashId the flash identifier. Starts from 0, up to e.g. led count -1.
 */
int AtomISP::setFlashIntensity(int intensity, unsigned int flashId)
{
    LOG1("@%s", __FUNCTION__);
    flashId = flashId << 8; // move the index to the second byte for the driver
    intensity |= flashId;   // bitwise add the led index to the second byte of the intensity value
    LOG1("flash intensity is: %x", intensity);
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
            s = mSensorHW->getFrameSyncSource();
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
    status_t ret = 0;

    if (t == OBSERVE_3A_STAT_READY) {
        m3AStatRequested++;
        if (m3AStatRequested == 1) {
          // subscribe to 3A frame sync event, only one subscription is needed
          // to serve multiple observers
          if (!m3AEventSubdevice->isOpen())
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

          ret = m3AEventSubdevice->subscribeEvent(V4L2_EVENT_ATOMISP_METADATA_READY);
          if (ret < 0) {
              ALOGE("Failed to subscribe to sensor metadata event!");
              m3AEventSubdevice->close();
              return UNKNOWN_ERROR;
          }

          for (unsigned int i = 0; i < mAllWaiters.size(); i++) {
              EventWaiter *waiter = mAllWaiters.editValueAt(i);
              status_t status = m3AEventSubdevice->subscribeEvent(V4L2_EVENT_ATOMISP_ACC_COMPLETE, waiter->handle);
              if (status < 0) {
                  ALOGE("Failed to subscribe to ACC complete event!");
                  m3AEventSubdevice->close();
                  return UNKNOWN_ERROR;
              }
          }
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

void AtomISP::startObserver(ObserverType t)
{
    mObserverManager.setState(OBSERVER_STATE_RUNNING,
            observerSubjectByType(t), false);
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
        ALOGE("3A stat event and sensor metadata event not enabled");
        return INVALID_OPERATION;
    }

    ret = mISP->m3AEventSubdevice->poll(FRAME_SYNC_POLL_TIMEOUT);

    if (ret <= 0) {
        ALOGE("Stats sync poll failed (%s), waiting recovery", (ret == 0) ? "timeout" : "error");
        ret = -1;
    } else {
        // poll was successful, dequeue the event right away
        ret = mISP->m3AEventSubdevice->dequeueEvent(&event);
        if (ret < 0) {
            ALOGE("Dequeue stats event failed");
        }
    }

    if (ret < 0) {
        msg->id = IAtomIspObserver::MESSAGE_ID_ERROR;
        // We sleep a moment but keep passing error messages to observers
        // until further client controls.
        usleep(ATOMISP_EVENT_RECOVERY_WAIT);
        return NO_ERROR;
    }

    // poll was successful
    if (event.type == V4L2_EVENT_ATOMISP_ACC_COMPLETE) {
        mISP->stageUpdate(event.id);
        msg->id = IAtomIspObserver::MESSAGE_ID_EVENT;
        msg->data.event.type = IAtomIspObserver::EVENT_TYPE_ACC_COMPLETE;
        msg->data.event.handle = event.id;
    } else if (mISP->mSensorEmbeddedMetaDataSupported && event.type == V4L2_EVENT_ATOMISP_METADATA_READY) {
        mISP->mSensorEmbeddedMetaData->handleSensorEmbeddedMetaData();
        msg->id = IAtomIspObserver::MESSAGE_ID_EVENT;
        msg->data.event.type = IAtomIspObserver::EVENT_TYPE_METADATA_READY;
    } else {
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
            nsecs_t frameTs = mISP->mSensorHW->getFrameTimestamp(TIMEVAL2USECS(&msg->data.event.timestamp));
            msg->data.event.timestamp.tv_sec = frameTs / 1000000;
            msg->data.event.timestamp.tv_usec = (frameTs % 1000000);
        }
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

    int maxTimeoutCount = PlatformData::getMaxISPTimeoutCount();

try_again:
    ret = mISP->mPreviewDevice->poll(ATOMISP_PREVIEW_POLL_TIMEOUT);
    if (ret > 0) {
        LOG2("@%s Entering dequeue : num-of-buffers queued %d", __FUNCTION__, mISP->mNumPreviewBuffersQueued);
        status = mISP->getPreviewFrame(&msg->data.frameBuffer.buff);
        if (status != NO_ERROR) {
            msg->id = IAtomIspObserver::MESSAGE_ID_ERROR;
            status = UNKNOWN_ERROR;
        } else {
            if (msg->data.frameBuffer.buff.status != FRAME_STATUS_CORRUPTED) {
                // Initialized timeout count when get normal preview frame
                mISPTimeoutCount = 0;
            }
            msg->data.frameBuffer.buff.owner = mISP;
            msg->id = IAtomIspObserver::MESSAGE_ID_FRAME;
            if (mISP->checkSkipFrame(msg->data.frameBuffer.buff.frameCounter,
                       mISP->mExtIspVideoHighSpeed ? mISP->mConfig.recording_fps : mISP->mConfig.preview_fps)
                || mISP->checkSkipFrameForVideoZoom())
                msg->data.frameBuffer.buff.status = FRAME_STATUS_SKIPPED;
        }
    } else {
        ALOGE("@%s v4l2_poll for preview device failed! (%s)", __FUNCTION__, (ret==0)?"timeout":"error");
        msg->id = IAtomIspObserver::MESSAGE_ID_ERROR;
        status = ret ? UNKNOWN_ERROR : TIMED_OUT;
        // If ISP timeout count is larger than max count, send out error message.
        ++mISPTimeoutCount;
        if (ret == 0 && mISPTimeoutCount >= maxTimeoutCount) {
            return status;
        }
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

status_t AtomISP::setSRESmode(bool mode)
{
    status_t status = NO_ERROR;
    unsigned int val = mode ? 1 : 0;

    LOG1("@%s", __FUNCTION__);

    if (mMainDevice->xioctl(ATOMISP_IOC_S_ENABLE_DZ_CAPT_PIPE, &val) < 0) {
        ALOGE("Failed to set enable flag for capture pipe digital zoom");
        status = UNKNOWN_ERROR;
    }
    return status;
}

} // namespace android
