/*
 * Copyright (C) 2013 The Android Open Source Project
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
#define LOG_TAG "Camera_SensorHW"

#include <linux/media.h>
#include <linux/v4l2-subdev.h>
#include <assert.h>
#include "SensorHW.h"
#include "v4l2device.h"
#include "PerformanceTraces.h"
#include "AtomCommon.h"

namespace android {

static sensorPrivateData gSensorDataCache[MAX_CAMERAS];

static const int ADJUST_ESTIMATE_DELTA_THRESHOLD_US = 2000; // Threshold for adjusting frame timestamp estimate at real frame sync
static const int MAX_EXPOSURE_HISTORY_SIZE = 10;
static const int DEFAULT_EXPOSURE_DELAY = 2;
static const char *ISP_SUBDEV_NAME_PREFIX = "ATOMISP_SUBDEV_";
static const int MAX_DEPTH = 5;


SensorHW::SensorHW(int cameraId):
    mSensorType(SENSOR_TYPE_NONE),
    mCameraId(cameraId),
    mStarted(false),
    mInitialModeDataValid(false),
    mRawBayerFormat(V4L2_PIX_FMT_SBGGR10),
    mFrameSyncSource(FRAME_SYNC_NA),
    mCssVersion(0),
    mActiveItemIndex(0),
    mDirectExposureIo(true),
    mPostponePrequeued(false),
    mExposureLag(0),
    mLatestExpId(EXP_ID_INVALID),
    mGainDelayFilter(NULL),
    mExposureHistory(NULL),
    mGroupId(0)
{
    // note init values are set also inside reset()
    reset(cameraId);
    sprintf(mIspSubDevName, "%s%d", ISP_SUBDEV_NAME_PREFIX, mGroupId);
}

SensorHW::~SensorHW()
{
    reset(mCameraId);
}

void SensorHW::reset(int cameraId)
{
    LOG1("@%s", __FUNCTION__);
    mSensorSubdevice.clear();
    mIspSubdevice.clear();
    mSyncEventDevice.clear();
    if (mGainDelayFilter) {
        delete mGainDelayFilter;
        mGainDelayFilter = NULL;
    }
    if (mExposureHistory) {
        delete mExposureHistory;
        mExposureHistory = NULL;
    }

    mSensorType = SENSOR_TYPE_NONE;
    mCameraId = cameraId;
    mStarted = false;
    mInitialModeDataValid = false;
    mRawBayerFormat = V4L2_PIX_FMT_SBGGR10;
    mFrameSyncSource = FRAME_SYNC_NA;
    mCssVersion = 0;
    mActiveItemIndex = 0;
    mDirectExposureIo = true;
    mPostponePrequeued = false;
    mExposureLag = 0;
    CLEAR(mCameraInput);
    CLEAR(mInitialModeData);
}

int SensorHW::getCurrentCameraId(void)
{
    return mCameraId;
}

size_t SensorHW::enumerateInputs(Vector<struct cameraInfo> &inputs)
{
    LOG1("@%s", __FUNCTION__);
    status_t ret = NO_ERROR;
    size_t numCameras = 0;
    struct v4l2_input input;
    struct cameraInfo sCamInfo;

    for (int i = 0; i < PlatformData::numberOfCameras(); ++i) {
        CLEAR(input);
        CLEAR(sCamInfo);
        input.index = i;
        ret = mDevice->enumerateInputs(&input);
        if (ret != NO_ERROR) {
            if (ret == INVALID_OPERATION || ret == BAD_INDEX)
                break;
            ALOGE("Device input enumeration failed for sensor input %d", i);
        } else {
            sCamInfo.index = i;
            strncpy(sCamInfo.name, (const char *)input.name, sizeof(sCamInfo.name)-1);
            sCamInfo.ispPort = (atomisp_camera_port)input.reserved[1];
            LOG1("Detected sensor %d: \"%s\" (port=%d)", i, sCamInfo.name, sCamInfo.ispPort);
        }
        inputs.push(sCamInfo);
        ++numCameras;
    }
    return numCameras;
}

void SensorHW::getPadFormat(sp<V4L2DeviceBase> &subdev, int padIndex, int &width, int &height)
{
    LOG1("@%s", __FUNCTION__);
    struct v4l2_subdev_format subdevFormat;
    int ret = 0;
    width = 0;
    height = 0;
    if (subdev == NULL)
        return;

    CLEAR(subdevFormat);
    subdevFormat.pad = padIndex;
    subdevFormat.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    ret = pxioctl(subdev, VIDIOC_SUBDEV_G_FMT, &subdevFormat);
    if (ret < 0) {
        ALOGE("Failed VIDIOC_SUBDEV_G_FMT");
    } else {
        width = subdevFormat.format.width;
        height = subdevFormat.format.height;
    }
}

status_t SensorHW::waitForFrameSync()
{
    LOG1("@%s", __FUNCTION__);
    Mutex::Autolock lock(mFrameSyncMutex);
    if (mFrameSyncSource == FRAME_SYNC_NA)
        return NO_INIT;

    return mFrameSyncCondition.wait(mFrameSyncMutex);
}

status_t SensorHW::selectActiveSensor(sp<V4L2VideoNode> &device)
{
    LOG1("@%s", __FUNCTION__);
    mDevice = device;
    status_t status = NO_ERROR;
    Vector<struct cameraInfo> camInfo;
    size_t numCameras = enumerateInputs(camInfo);

    mInitialModeDataValid = false;

    if (numCameras < (size_t) PlatformData::numberOfCameras()) {
        ALOGE("Number of detected sensors not matching static Platform data!");
    }

    if (numCameras < 1) {
        ALOGE("No detected sensors!");
        return UNKNOWN_ERROR;
    }

    // find v4l2_input.index using static mapping of isp port to
    // android camera id
    if (numCameras == 1) {
        mCameraInput = camInfo[0];
    } else {
        atomisp_camera_port targetPort;

        switch (mCameraId) {
        case 0:
            targetPort = ATOMISP_CAMERA_PORT_PRIMARY;
            break;
        case 1:
            targetPort = ATOMISP_CAMERA_PORT_SECONDARY;
            break;
        case 2:
            targetPort = ATOMISP_CAMERA_PORT_TERTIARY;
            break;
        default :
            ALOGE("Invalid camera id %d!", mCameraId);
            return BAD_VALUE;
        }

        size_t i = 0;

        for (i = 0; i < numCameras ; ++i) {
            if (camInfo[i].ispPort == targetPort) {

                unsigned minLen = MIN(strlen(PlatformData::sensorName(mCameraId)), strlen(camInfo[i].name));
                if (minLen && PlatformData::isExtendedCamera(mCameraId))
                    if (memcmp(PlatformData::sensorName(mCameraId), camInfo[i].name, minLen))
                        continue;

                mCameraInput = camInfo[i];
                break;
            }
        }

        if (i == numCameras) {
            ALOGE("No sensor with right port!");
            return UNKNOWN_ERROR;
        }
    }

    // Choose the camera sensor
    LOG1("Selecting camera sensor: %s, index: %d", mCameraInput.name, mCameraInput.index);
    status = mDevice->setInput(mCameraInput.index);
    if (status != NO_ERROR) {
        status = UNKNOWN_ERROR;
    } else {
        PERFORMANCE_TRACES_BREAKDOWN_STEP("capture_s_input");
        mSensorType = PlatformData::sensorType(mCameraId);

        // Query now the supported pixel formats
        Vector<v4l2_fmtdesc> formats;
        status = mDevice->queryCapturePixelFormats(formats);
        if (status != NO_ERROR) {
            ALOGW("Cold not query capture formats from sensor: %s", mCameraInput.name);
            status = NO_ERROR;   // This is not critical
        }
        sensorStoreRawFormat(formats);
    }

    status = initializeExposureFilter();
    if (status != NO_ERROR)
        ALOGE("Failed to configure exposure filter");

    return status;
}

status_t SensorHW:: setIspSubDevId(int id)
{
    LOG1("@%s", __FUNCTION__);
    if(id >= 0) {
        sprintf(mIspSubDevName, "%s%d", ISP_SUBDEV_NAME_PREFIX, id);
        mGroupId = id;
    }
    return NO_ERROR;
}
/**
 * Find V4L2 ATOMISP subdevice
 *
 * In CSS2 there are multiple ATOMISP subdevices (dual stream).
 * To find the correct one we travel through the pads and links
 * exposed by Media Controller API.
 */
status_t SensorHW::getIspDevicePath(char *ispDevPath, int size)
{
    LOG1("@%s", __FUNCTION__);
    int ret = 0;
    int val = 0;
    int sinkPadIndex = -1;
    int depth = 0;
    status_t status = NO_ERROR;
    char *sysName = new char[size];
    char *devName = new char[size];
    struct media_device_info mediaDeviceInfo;
    struct media_entity_desc mediaEntityDesc;
    struct media_entity_desc mediaEntityDescTmp;

    sp<V4L2DeviceBase> mediaCtl = new V4L2DeviceBase("/dev/media0", 0);
    status = mediaCtl->open();
    if (status != NO_ERROR) {
        ALOGE("Failed to open media device");
        goto exit_clean_mem;
    }

    if (!ispDevPath) {
        status = UNKNOWN_ERROR;
        ALOGE("ispDevPath is NULL, return");
        goto exit_clean_mem;
    }

    CLEAR(mediaDeviceInfo);
    ret = pxioctl(mediaCtl, MEDIA_IOC_DEVICE_INFO, &mediaDeviceInfo);
    if (ret < 0) {
        status = UNKNOWN_ERROR;
        ALOGE("Failed to get media device information");
        goto exit;
    }

    status = findMediaEntityByName(mediaCtl, mCameraInput.name, mediaEntityDesc);
    if (status != NO_ERROR) {
        ALOGE("Failed to find sensor subdevice");
        goto exit;
    }

    CLEAR(mediaEntityDescTmp);
    status = findConnectedEntityByName(mediaCtl, mediaEntityDesc, mediaEntityDescTmp, sinkPadIndex, mIspSubDevName, MAX_SENSOR_NAME_LENGTH, depth);

    if (status != NO_ERROR) {
        ALOGE("Unable to find connected ISP subdevice!");
        goto exit;
    }

    val += snprintf(devName, (size - val), "/sys/dev/char/%u:%u",
            mediaEntityDescTmp.v4l.major, mediaEntityDescTmp.v4l.minor);
    ret = readlink(devName, sysName, size);
    if (ret < 0) {
        status = UNKNOWN_ERROR;
        ALOGE("Unable to find subdevice node");
        goto exit;
    } else {
        sysName[ret] = 0;
        char *lastSlash = strrchr(sysName, '/');
        if (lastSlash == NULL) {
            status = UNKNOWN_ERROR;
            ALOGE("Invalid sysfs subdev path devName %s sysName %s", devName, sysName);
            goto exit;
        }
        val += snprintf(devName, (size - val), "/dev/%s", lastSlash + 1);
        LOG1("Subdevide node : %s", devName);
        strncpy(ispDevPath, devName, size);
        ispDevPath[size - 1] = '\0';
    }

exit:
    mediaCtl->close();
    mediaCtl.clear();
exit_clean_mem:
    delete[] sysName;
    delete[] devName;
    return status;
}

/**
 * Find and open V4L2 subdevices for direct access
 *
 * SensorHW class needs access to both sensor subdevice
 * and ATOMISP subdevice at the moment. In CSS2 there
 * are multiple ATOMISP subdevices (dual stream). To find
 * the correct one we travel through the pads and links
 * exposed by Media Controller API.
 *
 * Note: Current sensor selection above uses VIDIOC_ENUMINPUTS and
 *       VIDIOC_S_INPUT on  atomisp main device. The preferred method
 *       would be to have separate control over V4L2 subdevices, their
 *       pad formats and links using Media Controller API. Here in
 *       SensorHW class it would be natural to have direct controls and
 *       queries to sensor subdevice. This is not fully supported in our
 *       drivers so workarounds are done here to hide the facts from
 *       above layers.
 *
 * Workaround 1: use ISP subdev sink pad format temporarily to fetch
 *               reliable sensor output size.
 */
status_t SensorHW::openSubdevices()
{
    LOG1("@%s", __FUNCTION__);
    struct media_device_info mediaDeviceInfo;
    struct media_entity_desc mediaEntityDesc;
    struct media_entity_desc mediaEntityDescTmp;
    status_t status = NO_ERROR;
    int sinkPadIndex = -1;
    int ret = 0;

    sp<V4L2DeviceBase> mediaCtl = new V4L2DeviceBase("/dev/media0", 0);
    status = mediaCtl->open();
    if (status != NO_ERROR) {
        ALOGE("Failed to open media device");
        return status;
    }

    CLEAR(mediaDeviceInfo);
    ret = pxioctl(mediaCtl, MEDIA_IOC_DEVICE_INFO, &mediaDeviceInfo);
    if (ret < 0) {
        ALOGE("Failed to get media device information");
        mediaCtl.clear();
        return UNKNOWN_ERROR;
    }

    LOG1("Media device : %s", mediaDeviceInfo.driver);
    mCssVersion = mediaDeviceInfo.driver_version & ATOMISP_CSS_VERSION_MASK;

    status = findMediaEntityByName(mediaCtl, mCameraInput.name, mediaEntityDesc);
    if (status != NO_ERROR) {
        ALOGE("Failed to find sensor subdevice");
        return status;
    }

    status = openSubdevice(mSensorSubdevice, mediaEntityDesc.v4l.major, mediaEntityDesc.v4l.minor);
    if (status != NO_ERROR) {
        ALOGE("Failed to open sensor subdevice");
        return status;
    }

    CLEAR(mediaEntityDescTmp);
    int depth = 0;
    status = findConnectedEntityByName(mediaCtl, mediaEntityDesc, mediaEntityDescTmp, sinkPadIndex, mIspSubDevName, MAX_SENSOR_NAME_LENGTH, depth);

    if (status != NO_ERROR) {
        ALOGE("Unable to find connected ISP subdevice!");
        return status;
    }

    status = openSubdevice(mIspSubdevice, mediaEntityDescTmp.v4l.major, mediaEntityDescTmp.v4l.minor);
    if (status != NO_ERROR) {
        ALOGE("Failed to open sensor subdevice");
        return status;
    }

    // Currently only ISP sink pad format gives reliable size information
    // so we store it in the beginning.
    getPadFormat(mIspSubdevice, sinkPadIndex, mOutputWidth, mOutputHeight);

    mediaCtl->close();
    mediaCtl.clear();

    return status;
}

/**
 * Find description for given entity index
 *
 * Using media controller temporarily here to query entity with given name.
 */
status_t SensorHW::findMediaEntityById(sp<V4L2DeviceBase> &mediaCtl, int index,
        struct media_entity_desc &mediaEntityDesc)
{
    LOG1("@%s", __FUNCTION__);
    int ret = 0;
    CLEAR(mediaEntityDesc);
    mediaEntityDesc.id = index;
    ret = pxioctl(mediaCtl, MEDIA_IOC_ENUM_ENTITIES, &mediaEntityDesc);
    if (ret < 0) {
        LOG1("No more media entities");
        return UNKNOWN_ERROR;
    }
    return NO_ERROR;
}


/**
 * Find description for given entity name
 *
 * Using media controller temporarily here to query entity with given name.
 */
status_t SensorHW::findMediaEntityByName(sp<V4L2DeviceBase> &mediaCtl, char const* entityName,
        struct media_entity_desc &mediaEntityDesc)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = UNKNOWN_ERROR;
    for (int i = 0; ; i++) {
        status = findMediaEntityById(mediaCtl, i | MEDIA_ENT_ID_FLAG_NEXT, mediaEntityDesc);
        if (status != NO_ERROR)
            break;
        LOG2("Media entity %d : %s", i, mediaEntityDesc.name);
        if (strncmp(mediaEntityDesc.name, entityName, MAX_SENSOR_NAME_LENGTH) == 0)
            break;
    }

    return status;
}

/**
 * Find entity description for first outbound connection
 */
status_t SensorHW::findConnectedEntity(sp<V4L2DeviceBase> &mediaCtl,
        const struct media_entity_desc &mediaEntityDescSrc,
        struct media_entity_desc &mediaEntityDescDst, int &padIndex)
{
    LOG1("@%s", __FUNCTION__);
    struct media_links_enum links;
    status_t status = UNKNOWN_ERROR;
    int connectedEntity = -1;
    int ret = 0;

    LOG2("%s : pads %d links %d", mediaEntityDescSrc.name, mediaEntityDescSrc.pads, mediaEntityDescSrc.links);

    links.entity = mediaEntityDescSrc.id;
    links.pads = (struct media_pad_desc*) malloc(mediaEntityDescSrc.pads * sizeof(struct media_pad_desc));
    links.links = (struct media_link_desc*) malloc(mediaEntityDescSrc.links * sizeof(struct media_link_desc));

    ret = pxioctl(mediaCtl, MEDIA_IOC_ENUM_LINKS, &links);
    if (ret < 0) {
        ALOGE("Failed to query any links");
    } else {
        for (int i = 0; i < mediaEntityDescSrc.links; i++) {
            if (links.links[i].sink.entity != mediaEntityDescSrc.id) {
                connectedEntity = links.links[0].sink.entity;
                padIndex = links.links[0].sink.index;
            }
        }
        if (connectedEntity >= 0)
            status = NO_ERROR;
    }

    free(links.pads);
    free(links.links);

    if (status != NO_ERROR)
        return status;

    status = findMediaEntityById(mediaCtl, connectedEntity, mediaEntityDescDst);
    if (status != NO_ERROR)
        return status;

    LOG2("Connected entity ==> %s, pad %d", mediaEntityDescDst.name, padIndex);
    return status;
}

/**
 * Find entity description for first outbound connection
 */
status_t SensorHW::findConnectedEntityByName(sp<V4L2DeviceBase> &mediaCtl,
        struct media_entity_desc mediaEntityDescSrc,
        struct media_entity_desc &mediaEntityDescDst,
        int &padIndex, char const *name, int nameLen, int depth)
{
    LOG1("@%s", __FUNCTION__);
    struct media_links_enum links;
    status_t status = NAME_NOT_FOUND;
    int connectedEntity = -1;
    int ret = 0;

    LOG2("%s : pads %d links %d depth:%d", mediaEntityDescSrc.name, mediaEntityDescSrc.pads, mediaEntityDescSrc.links, depth);
    if (depth > MAX_DEPTH) {
        LOG1("reach max depth, exit");
        return status;
    }

    links.entity = mediaEntityDescSrc.id;
    links.pads = (struct media_pad_desc*) malloc(mediaEntityDescSrc.pads * sizeof(struct media_pad_desc));
    links.links = (struct media_link_desc*) malloc(mediaEntityDescSrc.links * sizeof(struct media_link_desc));

    if(links.pads == NULL || links.links == NULL)
        return NO_MEMORY;

    ret = mediaCtl->xioctl(MEDIA_IOC_ENUM_LINKS, &links);
    if (ret < 0) {
        ALOGE("Failed to query any links");
    } else {
        if( mediaEntityDescSrc.links == 0)
            goto exit;
        for (int i = 0; i < mediaEntityDescSrc.links; i++) {
            if (links.links[i].sink.entity != mediaEntityDescSrc.id) {
                connectedEntity = links.links[i].sink.entity;
                if (connectedEntity >= 0) {
                    padIndex = links.links[i].sink.index;
                    if (findMediaEntityById(mediaCtl, connectedEntity, mediaEntityDescSrc) == NO_ERROR) {
                        LOG2("Connected entity ==> %s, pad %d", mediaEntityDescSrc.name, padIndex);
                        if (strncmp(mediaEntityDescSrc.name, name, nameLen) == 0) {
                            LOG1("Connected ISP subdevice found");
                            CLEAR(mediaEntityDescDst);
                            memcpy(&mediaEntityDescDst, &mediaEntityDescSrc, sizeof(media_entity_desc));
                            status = NO_ERROR;
                            goto exit;
                        } else {
                            depth++;
                            status = findConnectedEntityByName(mediaCtl, mediaEntityDescSrc, mediaEntityDescDst, padIndex, name, nameLen, depth);
                            if (status == NO_ERROR)
                                goto exit;
                        }
                    }
                }
            }
        }
    }

exit:
    free(links.pads);
    free(links.links);
    return status;

}

/**
 * Open device node based on device identifier
 *
 * Helper method to find the device node name for V4L2 subdevices
 * from sysfs.
 */
status_t SensorHW::openSubdevice(sp<V4L2DeviceBase> &subdev, int major, int minor)
{
    LOG1("@%s :  major %d, minor %d", __FUNCTION__, major, minor);
    status_t status = UNKNOWN_ERROR;
    int ret = 0;
    char sysname[1024];
    char devname[1024];
    sprintf(devname, "/sys/dev/char/%u:%u", major, minor);
    ret = readlink(devname, sysname, sizeof(sysname));
    if (ret < 0) {
        ALOGE("Unable to find subdevice node");
    } else {
        sysname[ret] = 0;
        char *lastSlash = strrchr(sysname, '/');
        if (lastSlash == NULL) {
            ALOGE("Invalid sysfs subdev path");
            return status;
        }
        sprintf(devname, "/dev/%s", lastSlash + 1);
        LOG1("Subdevide node : %s", devname);
        subdev.clear();
        subdev = new V4L2DeviceBase(devname, 0);
        status = subdev->open();
        if (status != NO_ERROR) {
            ALOGE("Failed to open subdevice");
            subdev.clear();
        }
    }
    return status;
}

/**
 * Prepare Sensor HW for start streaming
 *
 * This function is to be called once V4L2 pipeline is fully
 * configured. Here we do the final settings or query the initial
 * sensor parameters.
*
 * Note: Set or query means hiding the fact that sensor controls
 * in legacy V4L2 are passed through ISP driver and mostly basing
 * on its format configuration. Media Controller API is not used to
 * build the links, but drivers are exposing the subdevices with
 * certain controls provided. SensorHW class is in roadmap to
 * utilize direct v4l2 subdevice IO meanwhile maintaining
 * transparent controls to clients through IHWSensorControl.
 *
 * After this call certain IHWSensorControls
 * are unavailable (controls that are not supported while streaming)
 *
 * TODO: Make controls not available during streaming more explicit
 *       by protecting the IOCs with streaming state.
 *
 * @param preQueuedExposure enable operation mode for exposure bracketing
 *                          where exposure parameters are kept in queue
 *                          for applying regardless of when they arrive.
 */
status_t SensorHW::prepare(bool preQueuedExposure)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    int ret = 0;

    mPostponePrequeued = preQueuedExposure;
    mDirectExposureIo = (mPostponePrequeued) ? false : !PlatformData::synchronizeExposure(mCameraId);

    // Open subdevice for direct IOCTL.
    status = openSubdevices();

    // Sensor is configured, readout the initial mode info
    ret = getModeInfo(&mInitialModeData);
    if (ret != 0)
        ALOGW("Reading initial sensor mode info failed!");

    if (mInitialModeData.frame_length_lines != 0 &&
        mInitialModeData.binning_factor_y != 0 &&
        mInitialModeData.vt_pix_clk_freq_mhz != 0) {
        mInitialModeDataValid = true;

        // do this only with more verbose debugging
        if (gLogLevel & CAMERA_DEBUG_LOG_LEVEL1)
        {
            // Debug logging for timings from SensorModeData
            long long vbi_ll = mInitialModeData.frame_length_lines - (mInitialModeData.crop_vertical_end -
                mInitialModeData.crop_vertical_start + 1) / mInitialModeData.binning_factor_y;

            LOG2("SensorModeData timings: FL %lldus, VBI %lldus, FPS %f",
                 ((long long) mInitialModeData.line_length_pck
                 * mInitialModeData.frame_length_lines) * 1000000
                 / mInitialModeData.vt_pix_clk_freq_mhz,
                 ((long long) mInitialModeData.line_length_pck * vbi_ll) * 1000000
                 / mInitialModeData.vt_pix_clk_freq_mhz,
                 ((double) mInitialModeData.vt_pix_clk_freq_mhz)
                 / (mInitialModeData.line_length_pck
                 * mInitialModeData.frame_length_lines));
        }
    }

    LOG1("Sensor output size %dx%d, FPS %f", mOutputWidth, mOutputHeight, getFramerate());

    return status;
}

/**
 * Start sensor HW (virtual concept)
 *
 * Atomisp driver is responsible of starting the actual sensor streaming IO
 * after its pipeline is configured and it has received VIDIOC_STREAMON for
 * video nodes it exposes.
 *
 * In virtual concept the SensorHW shall be started once the pipeline
 * configuration is ready and before the actual VIDIOC_STREAMON in order to
 * not to loose track of the initial frames. This context is also the first
 * place to query or set the initial sensor parameters.
 */
status_t SensorHW::start()
{
    LOG1("@%s", __FUNCTION__);
    int ret = 0;
    Mutex::Autolock lock(mFrameSyncMutex);
    // Subscribe to frame sync event in case of RAW sensor
    if (mIspSubdevice != NULL && mSensorType == SENSOR_TYPE_RAW && false == PlatformData::isDisable3A(mCameraId)) {
        ret = mIspSubdevice->subscribeEvent(FRAME_SYNC_SOF);
        if (ret < 0) {
            ret = mIspSubdevice->subscribeEvent(FRAME_SYNC_EOF);
            if (ret < 0) {
                ALOGE("Failed to subscribe to frame sync event!");
                return UNKNOWN_ERROR;
            } else {
                mFrameSyncSource = FRAME_SYNC_EOF;
                LOG1("@%s Using EOF event", __FUNCTION__);
            }
        } else {
            mFrameSyncSource = FRAME_SYNC_SOF;
            LOG1("@%s Using SOF event", __FUNCTION__);
        }
    }
    mStarted = true;
    return NO_ERROR;
}

/**
 * Stop sensor HW (virtual concept)
 *
 * Atomisp driver is responsible of stopping the actual sensor streaming IO.
 *
 * In virtual concept the SensorHW shall be stopped once sensor controls or
 * frame synchronization provided by the object are no longer needed.
 */
status_t SensorHW::stop()
{
    LOG1("@%s", __FUNCTION__);
    Mutex::Autolock lock(mFrameSyncMutex);
    if (mIspSubdevice != NULL && mFrameSyncSource != FRAME_SYNC_NA) {
        mIspSubdevice->unsubscribeEvent(mFrameSyncSource);
        mFrameSyncSource = FRAME_SYNC_NA;
    }
    mStarted = false;
    mDirectExposureIo = true;
    return NO_ERROR;
}

/**
 * Helper method for the sensor to select the prefered BAYER format
 * the supported pixel formats are retrieved when the sensor is selected.
 *
 * This helper method finds the first Bayer format and saves it to mRawBayerFormat
 * so that if raw dump feature is enabled we know what is the sensor
 * preferred format.
 *
 * TODO: sanity check, who needs a preferred format
 */
status_t SensorHW::sensorStoreRawFormat(Vector<v4l2_fmtdesc> &formats)
{
    LOG1("@%s", __FUNCTION__);
    Vector<v4l2_fmtdesc>::iterator it = formats.begin();

    for (;it != formats.end(); ++it) {
        /* store it only if is one of the Bayer formats */
        if (isBayerFormat(it->pixelformat)) {
            mRawBayerFormat = it->pixelformat;
            break;  //we take the first one, sensors tend to support only one
        }
    }
    return NO_ERROR;
}

void SensorHW::getMotorData(sensorPrivateData *sensor_data)
{
    LOG2("@%s", __FUNCTION__);
    int rc;
    struct v4l2_private_int_data motorPrivateData;

    motorPrivateData.size = 0;
    motorPrivateData.data = NULL;
    motorPrivateData.reserved[0] = 0;
    motorPrivateData.reserved[1] = 0;

    sensor_data->data = NULL;
    sensor_data->size = 0;
    // First call with size = 0 will return motor private data size.
    rc = pxioctl(mDevice, ATOMISP_IOC_G_MOTOR_PRIV_INT_DATA, &motorPrivateData);
    LOG2("%s IOCTL ATOMISP_IOC_G_MOTOR_PRIV_INT_DATA to get motor private data size ret: %d\n", __FUNCTION__, rc);
    if (rc != 0 || motorPrivateData.size == 0) {
        ALOGD("Failed to get motor private data size. Error: %d", rc);
        return;
    }

    motorPrivateData.data = malloc(motorPrivateData.size);
    if (motorPrivateData.data == NULL) {
        ALOGD("Failed to allocate memory for motor private data.");
        return;
    }

    // Second call with correct size will return motor private data.
    rc = pxioctl(mDevice, ATOMISP_IOC_G_MOTOR_PRIV_INT_DATA, &motorPrivateData);
    LOG2("%s IOCTL ATOMISP_IOC_G_MOTOR_PRIV_INT_DATA to get motor private data ret: %d\n", __FUNCTION__, rc);

    if (rc != 0 || motorPrivateData.size == 0) {
        ALOGD("Failed to read motor private data. Error: %d", rc);
        free(motorPrivateData.data);
        return;
    }

    sensor_data->data = motorPrivateData.data;
    sensor_data->size = motorPrivateData.size;
    sensor_data->fetched = true;
}

void SensorHW::getSensorData(sensorPrivateData *sensor_data)
{
    LOG2("@%s", __FUNCTION__);
    int rc;
    struct v4l2_private_int_data otpdata;
    int cameraId = getCurrentCameraId();

    sensor_data->data = NULL;
    sensor_data->size = 0;

    if ((gControlLevel & CAMERA_DISABLE_FRONT_NVM) || (gControlLevel & CAMERA_DISABLE_BACK_NVM)) {
        LOG1("NVM data reading disabled");
        sensor_data->fetched = false;
    }
    else {
        otpdata.size = 0;
        otpdata.data = NULL;
        otpdata.reserved[0] = 0;
        otpdata.reserved[1] = 0;

        if (gSensorDataCache[cameraId].fetched) {
            sensor_data->data = gSensorDataCache[cameraId].data;
            sensor_data->size = gSensorDataCache[cameraId].size;
            return;
        }
        // First call with size = 0 will return OTP data size.
        rc = pxioctl(mDevice, ATOMISP_IOC_G_SENSOR_PRIV_INT_DATA, &otpdata);
        LOG2("%s IOCTL ATOMISP_IOC_G_SENSOR_PRIV_INT_DATA to get OTP data size ret: %d\n", __FUNCTION__, rc);
        if (rc != 0 || otpdata.size == 0) {
            ALOGD("Failed to get OTP size. Error: %d", rc);
            return;
        }

        otpdata.data = calloc(otpdata.size, 1);
        if (otpdata.data == NULL) {
            ALOGD("Failed to allocate memory for OTP data.");
            return;
        }

        // Second call with correct size will return OTP data.
        rc = pxioctl(mDevice, ATOMISP_IOC_G_SENSOR_PRIV_INT_DATA, &otpdata);
        LOG2("%s IOCTL ATOMISP_IOC_G_SENSOR_PRIV_INT_DATA to get OTP data ret: %d\n", __FUNCTION__, rc);

        if (rc != 0 || otpdata.size == 0) {
            ALOGD("Failed to read OTP data. Error: %d", rc);
            free(otpdata.data);
            return;
        }

        sensor_data->data = otpdata.data;
        sensor_data->size = otpdata.size;
        sensor_data->fetched = true;
    }
    gSensorDataCache[cameraId] = *sensor_data;
}

int SensorHW::setExposureMode(v4l2_exposure_auto_type v4l2Mode)
{
    LOG2("@%s: %d", __FUNCTION__, v4l2Mode);
    return mDevice->setControl(V4L2_CID_EXPOSURE_AUTO, v4l2Mode, "AE mode");
}

int SensorHW::getExposureMode(v4l2_exposure_auto_type * type)
{
    LOG2("@%s", __FUNCTION__);
    return mDevice->getControl(V4L2_CID_EXPOSURE_AUTO, (int*)type);
}

int SensorHW::setExposureBias(int bias)
{
    LOG2("@%s: bias: %d", __FUNCTION__, bias);
    return mDevice->setControl(V4L2_CID_EXPOSURE, bias, "exposure");
}

int SensorHW::getExposureBias(int * bias)
{
    LOG2("@%s", __FUNCTION__);
    return mDevice->getControl(V4L2_CID_EXPOSURE, bias);
}

int SensorHW::setSceneMode(v4l2_scene_mode mode)
{
    LOG2("@%s: %d", __FUNCTION__, mode);
    return mDevice->setControl(V4L2_CID_SCENE_MODE, mode, "scene mode");
}

int SensorHW::getSceneMode(v4l2_scene_mode * mode)
{
    LOG2("@%s", __FUNCTION__);
    return mDevice->getControl(V4L2_CID_SCENE_MODE, (int*)mode);
}

int SensorHW::setWhiteBalance(v4l2_auto_n_preset_white_balance mode)
{
    LOG2("@%s: %d", __FUNCTION__, mode);
    return mDevice->setControl(V4L2_CID_AUTO_N_PRESET_WHITE_BALANCE, mode, "white balance");
}

int SensorHW::getWhiteBalance(v4l2_auto_n_preset_white_balance * mode)
{
    LOG2("@%s", __FUNCTION__);
    return mDevice->getControl(V4L2_CID_AUTO_N_PRESET_WHITE_BALANCE, (int*)mode);
}

int SensorHW::setIso(int iso)
{
    LOG2("@%s: ISO: %d", __FUNCTION__, iso);
    return mDevice->setControl(V4L2_CID_ISO_SENSITIVITY, iso, "iso");
}

int SensorHW::getIso(int * iso)
{
    LOG2("@%s", __FUNCTION__);
    return mDevice->getControl(V4L2_CID_ISO_SENSITIVITY, iso);
}

int SensorHW::setIsoMode(int mode)
{
    LOG2("@%s: ISO: %d", __FUNCTION__, mode);
    return mDevice->setControl(V4L2_CID_ISO_SENSITIVITY_AUTO, mode, "iso mode");
}

int SensorHW::setAeMeteringMode(v4l2_exposure_metering mode)
{
    LOG2("@%s: %d", __FUNCTION__, mode);
    return mDevice->setControl(V4L2_CID_EXPOSURE_METERING, mode, "AE metering mode");
}

int SensorHW::getAeMeteringMode(v4l2_exposure_metering * mode)
{
    LOG2("@%s", __FUNCTION__);
    return mDevice->getControl(V4L2_CID_EXPOSURE_METERING, (int*)mode);
}

int SensorHW::setAeFlickerMode(v4l2_power_line_frequency mode)
{
    LOG2("@%s: %d", __FUNCTION__, (int) mode);
    return mDevice->setControl(V4L2_CID_POWER_LINE_FREQUENCY,
                                    mode, "light frequency");
}

int SensorHW::setAfMode(int mode)
{
    LOG2("@%s: %d", __FUNCTION__, mode);
    if (!PlatformData::isFixedFocusCamera(mCameraId))
        return mDevice->setControl(V4L2_CID_AUTO_FOCUS_RANGE , mode, "AF mode");
    else
        return -1;
}

int SensorHW::getAfMode(int *mode)
{
    LOG2("@%s", __FUNCTION__);
    if (!PlatformData::isFixedFocusCamera(mCameraId))
        return mDevice->getControl(V4L2_CID_AUTO_FOCUS_RANGE, (int*)mode);
    else
        return -1;
}

int SensorHW::setAfEnabled(bool enable)
{
    LOG2("@%s", __FUNCTION__);
    if (!PlatformData::isFixedFocusCamera(mCameraId))
        return mDevice->setControl(V4L2_CID_FOCUS_AUTO, enable, "Auto Focus");
    else
        return -1;
}

int SensorHW::setAfWindows(const CameraWindow *windows, int numWindows)
{
    LOG1("@%s", __FUNCTION__);
    // Not supported
    return -1;
}

int SensorHW::set3ALock(int aaaLock)
{
    LOG2("@%s", __FUNCTION__);
    return mDevice->setControl(V4L2_CID_3A_LOCK, aaaLock, "AE Lock");
}

int SensorHW::get3ALock(int * aaaLock)
{
    LOG2("@%s", __FUNCTION__);
    return mDevice->getControl(V4L2_CID_3A_LOCK, aaaLock);
}


int SensorHW::setAeFlashMode(int mode)
{
    LOG2("@%s: %d", __FUNCTION__, mode);
    int ctl = V4L2_CID_FLASH_LED_MODE;
    return mDevice->setControl(ctl, mode, "Flash mode");
}

int SensorHW::getAeFlashMode(int *mode)
{
    LOG2("@%s", __FUNCTION__);
    return mDevice->getControl(V4L2_CID_FLASH_LED_MODE, (int*)mode);
}

int SensorHW::getModeInfo(struct atomisp_sensor_mode_data *mode_data)
{
    LOG2("@%s", __FUNCTION__);
    int ret;
    ret = pxioctl(mDevice, ATOMISP_IOC_G_SENSOR_MODE_DATA, mode_data);
    LOG2("%s IOCTL ATOMISP_IOC_G_SENSOR_MODE_DATA ret: %d\n", __FUNCTION__, ret);
    return ret;
}

int SensorHW::setSensorExposure(struct atomisp_exposure *exposure)
{
    int ret;
    ret = pxioctl(mDevice, ATOMISP_IOC_S_EXPOSURE, exposure);
    LOG2("%s IOCTL ATOMISP_IOC_S_EXPOSURE ret: %d, gain A:%d D:%d, itg C:%d F:%d\n", __FUNCTION__, ret, exposure->gain[0], exposure->gain[1], exposure->integration_time[0], exposure->integration_time[1]);
    return ret;
}

int SensorHW::setExposureTime(int time)
{
    LOG2("@%s", __FUNCTION__);
    return mDevice->setControl(V4L2_CID_EXPOSURE_ABSOLUTE, time, "Exposure time");
}

int SensorHW::getExposureTime(int *time)
{
    LOG2("@%s", __FUNCTION__);
    return mDevice->getControl(V4L2_CID_EXPOSURE_ABSOLUTE, time);
}

int SensorHW::getAperture(int *aperture)
{
    LOG2("@%s", __FUNCTION__);
    return mDevice->getControl(V4L2_CID_IRIS_ABSOLUTE, aperture);
}

int SensorHW::getFNumber(unsigned short *fnum_num, unsigned short *fnum_denom)
{
    LOG2("@%s", __FUNCTION__);
    int fnum = 0, ret;

    ret = mDevice->getControl(V4L2_CID_FNUMBER_ABSOLUTE, &fnum);

    *fnum_num = (unsigned short)(fnum >> 16);
    *fnum_denom = (unsigned short)(fnum & 0xFFFF);
    return ret;
}

/**
 * returns the V4L2 Bayer format preferred by the sensor
 */
int SensorHW::getRawFormat()
{
    return mRawBayerFormat;
}

const char * SensorHW::getSensorName(void)
{
    return mCameraInput.name;
}

/**
 * Set sensor framerate
 *
 * This function shall be called only before starting the stream and
 * also before querying sensor mode data.
 *
 * TODO: Make controls not available during streaming more explicit
 *       by protecting the IOCs with streaming state.
 */
status_t SensorHW::setFramerate(int fps)
{
    int ret = 0;
    LOG1("@%s: fps %d", __FUNCTION__, fps);

    if (mSensorSubdevice == NULL)
        return NO_INIT;

    struct v4l2_subdev_frame_interval subdevFrameInterval;
    CLEAR(subdevFrameInterval);
    subdevFrameInterval.pad = 0;
    subdevFrameInterval.interval.numerator = 1;
    subdevFrameInterval.interval.denominator = fps;
    ret = pxioctl(mSensorSubdevice, VIDIOC_SUBDEV_S_FRAME_INTERVAL, &subdevFrameInterval);
    if (ret < 0){
        ALOGE("Failed to set framerate to sensor subdevice");
        return UNKNOWN_ERROR;
    }
    return NO_ERROR;
}

/**
 * Returns maximum sensor framerate for active configuration
 */
float SensorHW::getFramerate() const
{
    LOG1("@%s", __FUNCTION__);
    int ret = 0;

    // Try initial mode data first
    if (mInitialModeDataValid) {
        LOG1("Using framerate from mode data");
        return ((float) mInitialModeData.vt_pix_clk_freq_mhz) /
               ( mInitialModeData.line_length_pck * mInitialModeData.frame_length_lines);
    }

    // Then subdev G_FRAME_INTERVAL
    if (mSensorSubdevice != NULL) {
        struct v4l2_subdev_frame_interval subdevFrameInterval;
        CLEAR(subdevFrameInterval);
        subdevFrameInterval.pad = 0;
        ret = pxioctl(mSensorSubdevice, VIDIOC_SUBDEV_G_FRAME_INTERVAL, &subdevFrameInterval);
        if (ret >= 0 && subdevFrameInterval.interval.numerator != 0) {
            LOG1("Using framerate from sensor subdevice");
            return ((float) subdevFrameInterval.interval.denominator) / subdevFrameInterval.interval.numerator;
        }
    }

    // Finally fall into videonode given framerate
    float fps = 0.0;
    ret = mDevice->getFramerate(&fps, mOutputWidth, mOutputHeight, mRawBayerFormat);
    if (ret < 0) {
        ALOGW("Failed to query the framerate");
        return 30.0;
    }
    LOG1("Using framerate provided by main video node");
    return fps;
}

void SensorHW::getFrameSizes(Vector<v4l2_subdev_frame_size_enum> &sizes)
{
    LOG2("@%s", __FUNCTION__);

    struct v4l2_subdev_frame_size_enum fsize;
    int ret = 0;

    CLEAR(fsize);
    fsize.index = 0;

    // ioctl() will return non-zero value, when enumerating out of range
    while (ret == 0) {
        ret = mSensorSubdevice->xioctl(VIDIOC_SUBDEV_ENUM_FRAME_SIZE, &fsize);

        if (ret == 0) {
            sizes.push(fsize);
            ++fsize.index;
        }
    }

    if (fsize.index == 0 && ret != 0)
        ALOGW("unable to get enumerated frame sizes");
}

/**
 * polls and dequeues frame synchronization events into IAtomIspObserver::Message
 */
status_t SensorHW::observe(IAtomIspObserver::Message *msg)
{
    LOG2("@%s", __FUNCTION__);
    struct v4l2_event event;
    int ret;
    nsecs_t ts = 0;

    ret = mIspSubdevice->poll(FRAME_SYNC_POLL_TIMEOUT);
    if (ret <= 0) {
        ALOGE("FrameSync poll failed (%s), waiting recovery..", (ret == 0) ? "timeout" : "error");
        ret = -1;
    } else {
        // poll was successful, dequeue the event right away
        do {
            ret = mIspSubdevice->dequeueEvent(&event);
            if (ret < 0) {
                ALOGE("Dequeue FrameSync event failed");
                break;
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

    // Fill observer message
    msg->id = IAtomIspObserver::MESSAGE_ID_EVENT;
    msg->data.event.type = IAtomIspObserver::EVENT_TYPE_SOF;
    // TODO: useless timestamp for delayed event
    msg->data.event.timestamp.tv_sec  = event.timestamp.tv_sec;
    msg->data.event.timestamp.tv_usec = event.timestamp.tv_nsec / 1000;
    msg->data.event.sequence = event.sequence;

    // Process exposure synchronization
    ts = TIMEVAL2USECS(&msg->data.event.timestamp);
    LOG2("-- FrameSync@%lldus --", ts);
    mFrameSyncMutex.lock();
    if (mFrameSyncSource == FRAME_SYNC_EOF) {
        // In CSS20, the buffered sensor mode and the event being close to
        // ~MIPI EOF means that we are at vbi when receiving the event here.
        // We delay sending the FrameSync event based on estimated active item vbi.
        // Note: Active item index is the estimated exposure item managed by
        //       updateExposureEstimate() below and produceExposureHistory()
        //       based on timestamp we receive and want to manipulate in case
        //       of CSS20 here. As update for this particular event has not yet
        //       happened, the active item is one more recent than at previous
        //       frame sync. Hereby, mActiveItemIndex-1.
        unsigned int vbiOffset = vbiIntervalForItem(mActiveItemIndex-1);
        LOG2("FrameSync: delaying over %dus timeout, active item index %d", vbiOffset, mActiveItemIndex);
        ts = ts + vbiOffset;
        msg->data.event.timestamp.tv_sec = ts / 1000000;
        msg->data.event.timestamp.tv_usec = (ts % 1000000);
        msg->data.event.sequence++;
        mFrameSyncMutex.unlock();
        usleep(vbiOffset);
        LOG2("FrameSync: timestamp offset %lldus, delta %lldus", TIMEVAL2USECS(&msg->data.event.timestamp), systemTime()/1000 - ts);
        mFrameSyncMutex.lock();
    }
    processExposureHistory(ts);
    // update mLatestExpId within lock
    mLatestExpId = event.u.frame_sync.frame_sequence;
    mFrameSyncMutex.unlock();
    mFrameSyncCondition.signal();

    return NO_ERROR;
}

/* Port of SensorSyncManager role [START] */

unsigned int SensorHW::getExposureDelay()
{
    unsigned int ret = mExposureLag;
    if (!mDirectExposureIo)
        ret++;
    return ret;
}

status_t SensorHW::initializeExposureFilter()
{
    LOG1("@%s", __FUNCTION__);
    int gainLag = PlatformData::getSensorGainLag(mCameraId);
    int exposureLag = PlatformData::getSensorExposureLag(mCameraId);
    bool useExposureSync = PlatformData::synchronizeExposure(mCameraId);
    unsigned int gainDelay = 0;

    LOG1("Exposure synchronization config read, gain lag %d, exposure lag %d, synchronize %s",
            gainLag, exposureLag, useExposureSync ? "true" : "false");

    // By default use exposure lag according to DEFAULT_EXPOSURE_DELAY
    // and align gain with it.
    exposureLag = (exposureLag == 0) ? DEFAULT_EXPOSURE_DELAY : exposureLag;
    gainLag = (gainLag == 0) ? exposureLag : gainLag;

    if (gainLag == exposureLag) {
        LOG1("Gain/Exposure re-aligning not needed");
    } else if (gainLag > exposureLag) {
        ALOGE("Check sensor latencies configuration, not supported");
    } else if (gainLag > 0 && !useExposureSync) {
        ALOGW("Analog gain re-aligning without synchronization!");
        // TODO: applying of final delayed gain not implemented
        gainDelay = exposureLag - gainLag;
    } else {
        gainDelay = exposureLag - gainLag;
    }

    mExposureLag = (gainLag <= exposureLag) ? exposureLag : gainLag;

    LOG1("sensor delays: gain %d, exposure %d", gainLag, exposureLag);
    LOG1("using sw gain delay %d, %s", gainDelay, useExposureSync ? "frame synchronized":"direct");

    // Note Fifo also used for continuous exposure history tracking
    mGainDelayFilter = new AtomDelayFilter <unsigned int> (0, gainDelay);
    mExposureHistory = new AtomFifo <struct exposure_history_item> (MAX_EXPOSURE_HISTORY_SIZE + mExposureLag);

    if (useExposureSync)
        mDirectExposureIo = false;

    return NO_ERROR;
}

inline void SensorHW::processGainDelay(struct atomisp_exposure *exposure)
{
    exposure->gain[0] = mGainDelayFilter->enqueue(exposure->gain[0]);
}

/**
 * Implements IHWSensorControl::setExposureGroup()
 */
int SensorHW::setExposureGroup(struct atomisp_exposure exposures[], int depth)
{
    int i, numItemNotApplied = 0;
    struct exposure_history_item *item = NULL;

    LOG2("@%s", __FUNCTION__);
    if (!exposures)
        return -1;

    Mutex::Autolock lock(mFrameSyncMutex);
    // cover the items which have not been applied
    for (i = 0; i < (int)mExposureHistory->getDepth(); i++) {
        item = mExposureHistory->peek(i);
        if (!item || item->applied)
            break;
        numItemNotApplied++;
    }

    LOG2("@%s numItemNotApplied:%d", __FUNCTION__, numItemNotApplied);
    // insert group
    for (i = 0 ; i < depth ; i++) {
        if (numItemNotApplied) {
            item = mExposureHistory->peek(numItemNotApplied - 1);
            if (item == NULL) {
                ALOGE("Has item that has not been applied but peek NULL");
                continue;
            }
            item->exposure = exposures[i];
            numItemNotApplied--;
        } else {
            processGainDelay(&exposures[i]);
            item = produceExposureHistory(&exposures[i], 0);
            mActiveItemIndex++;
        }
    }

    LOG1("@%s depth:%d mActiveItemIndex:%d, current exp id:%d",
            __FUNCTION__, depth, mActiveItemIndex, mLatestExpId);

    // For EOF event, current exposure id is N, It's the SOF of N+1.
    // Next exposure will be set from N+2, so setupLag is 2
    int setupLag = (mFrameSyncSource == FRAME_SYNC_EOF) ? 2 : 1;
    return (mLatestExpId + mExposureLag + setupLag) % EXP_ID_MAX;
}

/**
 * Implements IHWSensorControl::setExposure()
 */
int SensorHW::setExposure(struct atomisp_exposure *exposure)
{
    LOG2("@%s", __FUNCTION__);

    if (!exposure)
        return -1;

    Mutex::Autolock lock(mFrameSyncMutex);

    struct exposure_history_item *headItem = NULL;
    bool overwriteHead = false;

    if (mStarted) {
        headItem = mExposureHistory->peek(0);
        overwriteHead = (headItem && !headItem->applied && !mDirectExposureIo && !mPostponePrequeued);
        if (overwriteHead) {
            // This indicates that statistics event + 3A processing
            // ends up varying both sides of the FrameSync event and
            // though giving more than one exposure to apply in one frame
            // interval. If this value + mExposurelag grows higher than
            // mExposureHistory depth the exposure syncrhonization fails.
            // Ocassionally this is expected, and is know to cause momentary
            // AE miss sync until activate values are fed back properly.
            // In case of syncrhonizeExposure=true the decision is to apply
            // the most recently given parameters next. The same logic
            // works for direct applying, but we cannot rely what really
            // gets in before sensor readout.
            LOG1("Received two exposure controls in single frame interval");
            headItem->exposure = *exposure;
            // TODO: gain delay not handled here. would need to update filter.
        } else {
            processGainDelay(exposure);
            headItem = produceExposureHistory(exposure, 0);
            mActiveItemIndex++;
        }
    } else {
        mGainDelayFilter->reset(exposure->gain[0]);
        for (unsigned int i = 0; i <= mExposureLag; i++) {
            headItem = produceExposureHistory(exposure, 0);
        }
    }

    if (headItem && headItem->applied == false &&
        (mDirectExposureIo || !mStarted)) {
        // In immediate IO we expect that the apply we do below
        // reaches sensor registers before its readout for the next
        // frame. We are somewhere in middle of exposing frame N-1.
        // When this does not work well enough, the option is to
        // use exposureSynchronization.
        headItem->applied = true;
        return setSensorExposure(exposure);
    }

    return 0;
}

/**
 * Implement IAtomIspObserver::atomIspNotify()
 *
 * Note: almost nothing to do here. After moving SensorSyncManager
 * implementation, it is processed directly in SensorHW::observe().
 */
bool SensorHW::atomIspNotify(Message *msg, const ObserverState state)
{
   LOG2("@%s: msg id %d, state %d", __FUNCTION__, (msg) ? msg->id : -1, state);
   return false;
}

void SensorHW::resetEstimates(struct exposure_history_item *activeItem)
{
    struct exposure_history_item *tmpItem = NULL;
    unsigned int frameInterval = 0;
    int prevItemIdx = 0;
    struct exposure_history_item *item = getPrevAppliedItem(prevItemIdx);
    item = activeItem;
    for (int i = mActiveItemIndex - 1; i > prevItemIdx; i--) {
        tmpItem = mExposureHistory->peek(i);
        if (tmpItem) {
            frameInterval = frameIntervalForItem(i);
            tmpItem->frame_ts = item->frame_ts + frameInterval;
            item = tmpItem;
        }
    }
}

/**
 * Update frame timing estimate for previously applied exposure.
 *
 * Estimate is calculated according to timestamp of frame synchronization event
 * increased with the cumulated frame intervals of previously applied values.
 */
void SensorHW::updateExposureEstimate(nsecs_t timestamp)
{
    LOG2("@%s", __FUNCTION__);
    struct exposure_history_item *activeItem = NULL;
    int prevItemIdx = 0;

    struct exposure_history_item *item = getPrevAppliedItem(prevItemIdx);
    // This assertion failure would mean that applyExposureFromHistory() was
    // not called
    if (item == NULL) {
        ALOGE("getPrevAppliedItem error");
        return;
    }

    // Our estimate at FrameSync occurrence is that previously
    // applied item is N and active exposure would hereby be
    // N - mExposurLag where mExposureLag != 0.
    mActiveItemIndex = prevItemIdx + mExposureLag;
    activeItem = mExposureHistory->peek(mActiveItemIndex);
    if (activeItem == NULL) {
        ALOGE("peek active exposure error");
        return;
    }

    if (activeItem->frame_ts == 0) {
        LOG1("FrameSync: No estimate for active item, filling initials");
        activeItem->frame_ts = timestamp;
        resetEstimates(activeItem);
    }

    unsigned int frameLatency = cumulateFrameIntervals(prevItemIdx, mExposureLag);
    LOG2("FrameSync: latency for recently applied exposure %dus", frameLatency);
    assert(item->frame_ts == 0);
    item->frame_ts = timestamp + frameLatency;

    long deltaToReceived = (long) (timestamp - activeItem->frame_ts);
    // This print should reveal if use of exposureSynchronization is
    // preferred (ocassional deltas of frame-interval in cases when
    // exposure applying didn't reach the frame in time)
    // This is also useful to trace how the estimation logic performs
    // and how the configured exposure lags match with reality.
    LOG2("FrameSync: delta to active item estimate %ldus, act index %d, prev index %d",
          deltaToReceived, mActiveItemIndex, prevItemIdx);
    if (deltaToReceived > ADJUST_ESTIMATE_DELTA_THRESHOLD_US
        || deltaToReceived < -ADJUST_ESTIMATE_DELTA_THRESHOLD_US) {
        // update with received ts
        activeItem->frame_ts = timestamp;
    }

    activeItem->received = true;
}

/**
 * Return the most recently applied exposure_history_item
 */
SensorHW::exposure_history_item* SensorHW::getPrevAppliedItem(int &id)
{
    struct exposure_history_item *appliedExposure = NULL;
    for (unsigned int i = 0; i < mExposureHistory->getCount(); i++) {
        appliedExposure = mExposureHistory->peek(i);
        if (appliedExposure->applied) {
            id = i;
            return appliedExposure;
        }
    }
    return NULL;
}

/**
 * Return vbi latency for exposure history item
 */
unsigned int SensorHW::vbiIntervalForItem(unsigned int index)
{
    struct exposure_history_item *item = mExposureHistory->peek(index);
    unsigned int fll = (item) ? MAX(mInitialModeData.frame_length_lines, item->exposure.integration_time[0]) : mInitialModeData.frame_length_lines;

    unsigned int vbiLL = fll - (mInitialModeData.crop_vertical_end - mInitialModeData.crop_vertical_start + 1) / mInitialModeData.binning_factor_y;

    return ((long long) mInitialModeData.line_length_pck * vbiLL * 1000000)
           / mInitialModeData.vt_pix_clk_freq_mhz;
}

unsigned int SensorHW::frameIntervalForItem(unsigned int index)
{
    struct exposure_history_item *item = mExposureHistory->peek(index);
    unsigned int fll = (item) ? MAX(mInitialModeData.frame_length_lines, item->exposure.integration_time[0]) : mInitialModeData.frame_length_lines;
    return ((long long) mInitialModeData.line_length_pck * fll * 1000000)
            / mInitialModeData.vt_pix_clk_freq_mhz;
}

/**
 * Return cumulated frame intervals from exposure history
 *
 * \param index start index (normally the frame N-1)
 * \param frames number of items to cumulate (exposure lag)
 */
unsigned int SensorHW::cumulateFrameIntervals(unsigned int index, unsigned int frames)
{
    struct exposure_history_item *appliedExposure = NULL;
    unsigned int cumulatedFrameLengths = 0;
    unsigned int cumulatedFrameCount = 0;
    unsigned int retUs = 0;

    for (unsigned i = index; i < mExposureHistory->getCount() && cumulatedFrameCount < frames; i++) {
        appliedExposure = mExposureHistory->peek(i);
        if (appliedExposure) {
            cumulatedFrameLengths += MAX(mInitialModeData.frame_length_lines,
                                     appliedExposure->exposure.integration_time[0]);
            cumulatedFrameCount++;
        }
    }

    if (cumulatedFrameCount < frames) {
        cumulatedFrameLengths += (frames - cumulatedFrameCount) * mInitialModeData.frame_length_lines;
        LOG2("Cumulated initial mode data, ll %d", cumulatedFrameLengths);
    }

    retUs = ((long long)mInitialModeData.line_length_pck * cumulatedFrameLengths * 1000000)
            / mInitialModeData.vt_pix_clk_freq_mhz;

    LOG2("Cumulated history, %d frames, timeUs %dus", cumulatedFrameCount, retUs);

    return retUs;
}

/**
 * Produce new exposure_history_item into FiFo
 */
SensorHW::exposure_history_item* SensorHW::produceExposureHistory(struct atomisp_exposure *exposure, nsecs_t frame_ts)
{
    LOG2("@%s", __FUNCTION__);
    struct exposure_history_item item;

    item.frame_ts = frame_ts;
    item.exposure = *exposure;
    item.applied = false;
    item.received = false;

    // when fifo is full drop the oldest
    if (mExposureHistory->getCount() >= mExposureHistory->getDepth())
        mExposureHistory->dequeue();

    mExposureHistory->enqueue(item);

    return mExposureHistory->peek(0);
}

/**
 * Do operations needed for exposure history tracking
 *
 * To be called once for each FrameSyncEvent with timestamp close
 * to the beginning of frame integration.
 *
 * If there is nothing to apply, we roll the history here.
 *
 * Note: Even when syncronizeExposure=false this function is called to
 *       roll the history and potentially apply the delayed gain(s).
 */
void SensorHW::processExposureHistory(nsecs_t ts)
{
    LOG2("@%s", __FUNCTION__);
    struct exposure_history_item *item = NULL;
    unsigned int itemsToApply = 0;
    bool applyToSensor = false;
    int ret = 0;

    // find prev applied and quantity of not applied
    for (unsigned int i = 0; i < mExposureHistory->getDepth(); i++) {
        item = mExposureHistory->peek(i);
        if (!item || item->applied)
            break;
        itemsToApply++;
    }

    if (itemsToApply == 0) {
        if (item == NULL) {
            // No initials from 3A or FiFo has been filled with
            // new exposures in between FrameSyncs.
            ALOGW("No initial exposure set by 3A!");
            struct atomisp_exposure dummyExposure;
            CLEAR(dummyExposure);
            // TODO: Could use sensor initials here
            item = produceExposureHistory(&dummyExposure, 0);
            if (item != NULL)
                item->applied = true;
        } else if (item->frame_ts != 0) {
            // there is already estimated ts for previously applied
            // item. This means we rolled over one frame without
            // updating exposure (setExposure() was not called)
            // Keep rolling and pick delayd gain
            item = produceExposureHistory(&item->exposure, 0);
            if (item == NULL) {
                ALOGE("produceExposureHistory error");
                return;
            }
            unsigned int prevGain = item->exposure.gain[0];
            unsigned int gainTail = mGainDelayFilter->tail();
            item->exposure.gain[0] = mGainDelayFilter->enqueue(gainTail);
            LOG2("Gain filter values, prev %d, tail %d => %d",
                 prevGain, gainTail, item->exposure.gain[0]);
            if (item->exposure.gain[0] != prevGain) {
                applyToSensor = true;
            } else {
                item->applied = true;
            }
        }
    } else {
        if (itemsToApply > 1) {
            LOG1("## More than one new exposure to apply (%d)", itemsToApply);
        }
        applyToSensor = true;
        item = mExposureHistory->peek(itemsToApply - 1);
        if (item == NULL) {
            ALOGE("peek %d exposure history error", itemsToApply-1);
            return;
        }
    }

    if (applyToSensor) {
        ret = setSensorExposure(&item->exposure);
        if (ret != 0) {
            ALOGE("Setting sensor exposure failed!");
        }
        item->applied = true;
    }

    updateExposureEstimate(ts);
}

/**
 * Return timestamp estimate of frame by event timestamp
 *
 * This is place for platform specific workarounds needed
 * to couple incoming events like 3A statistics into active
 * sensor parameter data only by the timestamp of event.
 */
nsecs_t SensorHW::getFrameTimestamp(nsecs_t event_ts)
{
    LOG2("@%s-%lld", __FUNCTION__, event_ts);
    Mutex::Autolock lock(mFrameSyncMutex);
    struct exposure_history_item *receivedItem = NULL;
    long deltaToReceived = 0;
    long itgInterval = 0;
    nsecs_t retTs = 0;

    // Travel through items with received frame sync event
    // and consider delta to each. Consider that if delta
    // is smaller than integration interval of that item,
    // any event received at that time must origin from the
    // previous frame.
    for (unsigned int i = 0; i < mExposureHistory->getCount(); i++) {
        receivedItem = mExposureHistory->peek(i);
        if (receivedItem && receivedItem->received) {
            if (mFrameSyncSource == FRAME_SYNC_EOF) {
                //NOTE: In CSS2.0 where FrameSync is delayed from EOF event to
                //      reprecent the next SOF and Sensor Buffered Mode
                //      increases the latency of events, we consider that
                //      receiving ISP events for frame during its vbi is
                //      impossible.
                //      Using full frame interval comparison
                itgInterval = frameIntervalForItem(i);
            } else {
                itgInterval = frameIntervalForItem(i) - vbiIntervalForItem(i);
            }
            retTs = receivedItem->frame_ts;
            deltaToReceived = (long)(event_ts - retTs);
            LOG2("%s: received item %i delta %ldus, itg interval %ldus", __FUNCTION__, i, deltaToReceived, itgInterval);
            if (deltaToReceived > itgInterval) {
                break;
            } else {
                //NOTE: In CSS2.0 where FrameSync is delayed from EOF event to
                //      reprecent the next SOF, it is usual that we get
                //      negative delta here. This means we receive 3A stats
                //      during the VBI, but that origins from frame before.
                LOG2("%s: negative delta to FrameSync", __FUNCTION__);
            }
        }
    }
    assert(receivedItem);
    // Note: if the above logic fails e.g due heavy cpu burden there isn't
    //       enough exposure history, we returns the closest match there is
    //       or 0 in corner cases. Enable asserts to trace related issues.
    LOG2("%s: frame timestamp %lldus returned", __FUNCTION__, retTs);
    return retTs;
}

/* Port of SensorSyncManager role [END] */

}
