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
#define LOG_TAG "Camera_Driver"

#include <Exif.h>
#include "LogHelper.h"
#include "CameraDriver.h"
#include "Callbacks.h"
#include "ColorConverter.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <cutils/properties.h>
#include <utils/String8.h>
#include "CameraBufferAllocator.h"
#include "VAConvertor.h"
#include "DumpImage.h"


#define CLEAR(x) memset (&(x), 0, sizeof (x))

#define MIN_VIDEO_FPS 24

#define RESOLUTION_VGA_WIDTH    640
#define RESOLUTION_VGA_HEIGHT   480

#define DEFAULT_PIC_SIZE "1920x1080"
#define DEFAULT_VID_SIZE "640x480"

// Zero doesn't work here.  Apps (e.g Gallery) use this as a
// denominator and blow up with a FPE.
// Zero will disable the exposure time in the exif
#define DEFAULT_EXPOSURE_TIME 0
#define DEFAULT_ISO_SPEED     100

namespace android {

////////////////////////////////////////////////////////////////////
//                          STATIC DATA
////////////////////////////////////////////////////////////////////

CameraDriver::CameraSensor *CameraDriver::mCameraSensor[MAX_CAMERAS];
Mutex CameraDriver::mCameraSensorLock;
int CameraDriver::numCameras = 0;

////////////////////////////////////////////////////////////////////
//                          PUBLIC METHODS
////////////////////////////////////////////////////////////////////

CameraDriver::CameraDriver(int cameraId) :
    mMode(MODE_NONE)
    ,mCallbacks(NULL)
    ,mSessionId(0)
    ,mCameraId(cameraId)
    ,mFormat(V4L2_PIX_FMT_YUYV)
    ,mBufAlloc(CameraMemoryAllocator::instance())
    ,mJpegDecoder(NULL)
{
    LOG1("@%s", __FUNCTION__);

    mConfig.num_snapshot = 1;
    mConfig.zoom = 0;

    mZoomMax = 100;
    mZoomMin = 100;

    mBrightMax = 0;
    mBrightMin = 0;

    mWBMode = WHITE_BALANCE_AUTO;
    mExpBias = 0;

    memset(&mBufferPool, 0, sizeof(mBufferPool));

    int ret = openDevice();
    if (ret < 0) {
        ALOGE("Failed to open device!");
        return;
    }

    int err = set_capture_mode(MODE_CAPTURE);
    if (err < 0) {
        ALOGE("Failed to init device to capture mode");
        closeDevice();
        return;
    }

    detectDeviceResolutions();

    closeDevice();
}

CameraDriver::~CameraDriver()
{
    LOG1("@%s", __FUNCTION__);
    /*
     * The destructor is called when the hw_module close mehod is called. The close method is called
     * in general by the camera client when it's done with the camera device, but it is also called by
     * System Server when the camera application crashes. System Server calls close in order to release
     * the camera hardware module. So, if we are not in MODE_NONE, it means that we are in the middle of
     * somthing when the close function was called. So it's our duty to stop first, then close the
     * camera device.
     */
    if (mMode != MODE_NONE) {
        stop();
    }
    if (mCallbacks.get())
        mCallbacks.clear();
}

void CameraDriver::getDefaultParameters(CameraParameters *params)
{
    LOG2("@%s", __FUNCTION__);
    int status = 0;
    if (!params) {
        ALOGE("params is null!");
        return;
    }

    // These are autodetected and don't care about whether we're front
    // or back facing.
    params->set(CameraParameters::KEY_PICTURE_SIZE, mBestPicSize.string());
    params->set(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES, mPicSizes.string());
    params->set(CameraParameters::KEY_VIDEO_SIZE, mBestVidSize.string());
    params->set(CameraParameters::KEY_SUPPORTED_VIDEO_SIZES, mVidSizes.string());
    params->set(CameraParameters::KEY_PREVIEW_SIZE, mBestVidSize.string());
    params->set(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES, mVidSizes.string());
    params->set(CameraParameters::KEY_PREFERRED_PREVIEW_SIZE_FOR_VIDEO, mBestVidSize.string());
    params->setPreviewFrameRate(30);
    params->set(CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES,"15,30"); // TODO: consider which FPS to support
    params->set(CameraParameters::KEY_PREVIEW_FPS_RANGE,"30000,30000");
    params->set(CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE,"(30000,30000)");
    params->set(CameraParameters::KEY_PREVIEW_FORMAT, "yuv420sp");
    params->set(CameraParameters::KEY_SUPPORTED_PREVIEW_FORMATS, "yuv420p,yuv420sp");

    params->set(CameraParameters::KEY_VIDEO_SNAPSHOT_SUPPORTED, CameraParameters::FALSE);

    params->set(CameraParameters::KEY_SUPPORTED_PICTURE_FORMATS,CameraParameters::PIXEL_FORMAT_JPEG);
    params->set(CameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES,"0x0,160x120"); // 0x0 indicates "not supported"
    params->set(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH, 160);
    params->set(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT, 120);

    params->set(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY, "75");
    params->set(CameraParameters::KEY_JPEG_QUALITY, "75");

    params->set(CameraParameters::KEY_ZOOM, 0);
    params->set(CameraParameters::KEY_ZOOM_SUPPORTED, CameraParameters::TRUE);
    getZoomRatios(MODE_PREVIEW, params);

    params->set(CameraParameters::KEY_EXPOSURE_COMPENSATION,0);
    params->set(CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION,"3");
    params->set(CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION,"-3");
    params->set(CameraParameters::KEY_EXPOSURE_COMPENSATION_STEP,"1");

    // effect modes
    if (mSupportedControls.hue) {
        params->set(CameraParameters::KEY_EFFECT, CameraParameters::EFFECT_NONE);
        char effectModes[200] = {0};
        int status = snprintf(effectModes, sizeof(effectModes)
                              ,"%s,%s,%s"
                              ,CameraParameters::EFFECT_NONE
                              ,CameraParameters::EFFECT_MONO
                              ,CameraParameters::EFFECT_SEPIA);

        if (status < 0) {
            ALOGE("Could not generate %s string: %s",
                  CameraParameters::KEY_SUPPORTED_EFFECTS, strerror(errno));
            return;
        } else if (static_cast<unsigned>(status) >= sizeof(effectModes)) {
            ALOGE("Truncated %s string. Reserved length: %d",
                  CameraParameters::KEY_SUPPORTED_EFFECTS, sizeof(effectModes));
            return;
        }
        params->set(CameraParameters::KEY_SUPPORTED_EFFECTS, effectModes);
    } else {
        params->set(CameraParameters::KEY_SUPPORTED_EFFECTS, CameraParameters::EFFECT_NONE);
    }

    // white-balance mode
    params->set(CameraParameters::KEY_WHITE_BALANCE, CameraParameters::WHITE_BALANCE_AUTO);
    if (mSupportedControls.whiteBalanceTemperature) {
        char wbModes[100] = {0};
        status = snprintf(wbModes, sizeof(wbModes)
                          ,"%s,%s,%s,%s,%s"
                          ,CameraParameters::WHITE_BALANCE_AUTO
                          ,CameraParameters::WHITE_BALANCE_INCANDESCENT
                          ,CameraParameters::WHITE_BALANCE_DAYLIGHT
                          ,CameraParameters::WHITE_BALANCE_FLUORESCENT
                          ,CameraParameters::WHITE_BALANCE_CLOUDY_DAYLIGHT);
        if (status < 0) {
            ALOGE("Could not generate %s string: %s",
                  CameraParameters::KEY_SUPPORTED_WHITE_BALANCE, strerror(errno));
            return;
        } else if (static_cast<unsigned>(status) >= sizeof(wbModes)) {
            ALOGE("Truncated %s string. Reserved length: %d",
                  CameraParameters::KEY_SUPPORTED_WHITE_BALANCE, sizeof(wbModes));
            return;
        }
        params->set(CameraParameters::KEY_SUPPORTED_WHITE_BALANCE, wbModes);
    } else {
        params->set(CameraParameters::KEY_SUPPORTED_WHITE_BALANCE, CameraParameters::WHITE_BALANCE_AUTO);
    }

    if (mCameraSensor[mCameraId]->info.facing == CAMERA_FACING_FRONT) {
        LOG1("Get Default Parameters for Front Camera ");

       // Front Camera is Fixed focus
       params->set(CameraParameters::KEY_FOCUS_MODE, CameraParameters::FOCUS_MODE_FIXED);
       params->set(CameraParameters::KEY_SUPPORTED_FOCUS_MODES, CameraParameters::FOCUS_MODE_FIXED);
       params->set(CameraParameters::KEY_MAX_NUM_FOCUS_AREAS, 0);
       params->setFloat(CameraParameters::KEY_FOCAL_LENGTH, 10.0);
    }  else {
        LOG1("Get Default Parameters for Rear Camera ");

        params->set(CameraParameters::KEY_FOCUS_MODE, CameraParameters::FOCUS_MODE_AUTO);
        params->set(CameraParameters::KEY_SUPPORTED_FOCUS_MODES, CameraParameters::FOCUS_MODE_AUTO);
        params->set(CameraParameters::KEY_MAX_NUM_FOCUS_AREAS, 1);
        params->set(CameraParameters::KEY_FOCUS_AREAS, "(0,0,0,0,0)");

        // TODO: find out actual focal length
        // TODO: also find out how to get sensor width and height which will likely be used with focal length
        float focalLength = 10; // focalLength unit is mm
        params->setFloat(CameraParameters::KEY_FOCAL_LENGTH, focalLength);

        getFocusDistances(params);

        // scene mode
        params->set(CameraParameters::KEY_SCENE_MODE, CameraParameters::SCENE_MODE_AUTO);
        params->set(CameraParameters::KEY_SUPPORTED_SCENE_MODES, CameraParameters::SCENE_MODE_AUTO);

        // 3a lock: auto-exposure lock
        params->set(CameraParameters::KEY_AUTO_EXPOSURE_LOCK, "");
        params->set(CameraParameters::KEY_AUTO_EXPOSURE_LOCK_SUPPORTED, CameraParameters::FALSE);

        // 3a lock: auto-whitebalance lock
        params->set(CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK, "");
        params->set(CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK_SUPPORTED, CameraParameters::FALSE);

    }
    /**
     * FLASH
     */
    params->set(CameraParameters::KEY_FLASH_MODE, CameraParameters::FLASH_MODE_OFF);
    params->set(CameraParameters::KEY_SUPPORTED_FLASH_MODES, CameraParameters::FLASH_MODE_OFF);

    // metering areas
    params->set(CameraParameters::KEY_MAX_NUM_METERING_AREAS, 0);

     /**
     * MISCELLANEOUS
     */
    params->set(CameraParameters::KEY_HORIZONTAL_VIEW_ANGLE, 45);
    params->set(CameraParameters::KEY_VERTICAL_VIEW_ANGLE, 45);

}

status_t CameraDriver::start(Mode mode,RenderTarget **all_targets,int targetBufNum)
{
    LOG1("@%s", __FUNCTION__);
    LOG1("mode = %d", mode);
    status_t status = NO_ERROR;

    switch (mode) {
    case MODE_PREVIEW:
        status = startPreview(all_targets,targetBufNum);
        break;

    case MODE_VIDEO:
        status = startRecording(all_targets,targetBufNum);
        break;

    case MODE_CAPTURE:
        status = startCapture(all_targets,targetBufNum);
        break;

    default:
        break;
    };

    if (status == NO_ERROR) {
        mMode = mode;
        mSessionId++;
    }

    return status;
}

status_t CameraDriver::stop()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

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

    default:
        break;
    };

    if (status == NO_ERROR)
        mMode = MODE_NONE;

    return status;
}

status_t CameraDriver::startPreview(RenderTarget **all_targets,int targetBufNum)
{
    LOG1("@%s", __FUNCTION__);
    int ret = 0;
    status_t status = NO_ERROR;

    ret = openDevice();
    if (ret < 0) {
        ALOGE("Open device failed!");
        status = UNKNOWN_ERROR;
        return status;
    }

    ret = configureDevice(
            MODE_PREVIEW,
            mConfig.preview.padding,
            mConfig.preview.height,
            NUM_DEFAULT_BUFFERS,
            all_targets,
            targetBufNum);
    if (ret < 0) {
        ALOGE("Configure device failed!");
        status = UNKNOWN_ERROR;
        goto exitClose;
    }

    // need to resend the current zoom value
    set_zoom(mCameraSensor[mCameraId]->fd, mConfig.zoom);

    ret = startDevice();
    if (ret < 0) {
        ALOGE("Start device failed!");
        status = UNKNOWN_ERROR;
        goto exitDeconfig;
    }
    return status;

exitDeconfig:
    deconfigureDevice();
exitClose:
    closeDevice();
    return status;
}

status_t CameraDriver::stopPreview()
{
    LOG1("@%s", __FUNCTION__);

    stopDevice();
    deconfigureDevice();
    closeDevice();

    return NO_ERROR;
}

status_t CameraDriver::startRecording(RenderTarget **all_targets,int targetBufNum)
{
    LOG1("@%s", __FUNCTION__);
    int ret = 0;
    status_t status = NO_ERROR;

    ret = openDevice();
    if (ret < 0) {
        ALOGE("Open device failed!");
        status = UNKNOWN_ERROR;
        return status;
    }

    ret = configureDevice(
            MODE_VIDEO,
            mConfig.preview.padding,
            mConfig.preview.height,
            NUM_DEFAULT_BUFFERS,
            all_targets,
            targetBufNum);
    if (ret < 0) {
        ALOGE("Configure device failed!");
        status = UNKNOWN_ERROR;
        goto exitClose;
    }

    ret = startDevice();
    if (ret < 0) {
        ALOGE("Start device failed!");
        status = UNKNOWN_ERROR;
        goto exitDeconfig;
    }

    return status;

exitDeconfig:
    deconfigureDevice();
exitClose:
    closeDevice();
    return status;
}

status_t CameraDriver::stopRecording()
{
    LOG1("@%s", __FUNCTION__);

    stopDevice();
    deconfigureDevice();
    closeDevice();

    return NO_ERROR;
}
status_t CameraDriver::startCapture(RenderTarget **all_targets,int targetBufNum)
{
    LOG1("@%s", __FUNCTION__);
    int ret = 0;
    status_t status = NO_ERROR;

    ret = openDevice();
    if (ret < 0) {
        ALOGE("Open device failed!");
        status = UNKNOWN_ERROR;
        return status;
    }

    ret = configureDevice(
            MODE_CAPTURE,
            mConfig.snapshot.width,
            mConfig.snapshot.height,
            NUM_DEFAULT_BUFFERS,
            all_targets,
            targetBufNum);
    if (ret < 0) {
        ALOGE("Configure device failed!");
        status = UNKNOWN_ERROR;
        goto exitClose;
    }

    // need to resend the current zoom value
    set_zoom(mCameraSensor[mCameraId]->fd, mConfig.zoom);

    ret = startDevice();
    if (ret < 0) {
        ALOGE("Start device failed!");
        status = UNKNOWN_ERROR;
        goto exitDeconfig;
    }

    return status;

exitDeconfig:
    deconfigureDevice();
exitClose:
    closeDevice();
    return status;
}

status_t CameraDriver::stopCapture()
{
    LOG1("@%s", __FUNCTION__);

    stopDevice();
    deconfigureDevice();
    closeDevice();

    return NO_ERROR;
}

int CameraDriver::configureDevice(Mode deviceMode, int w, int h, int numBuffers,RenderTarget **all_targets,int targetBufNum)
{
    LOG1("@%s", __FUNCTION__);
    int ret = 0;
    status_t status = NO_ERROR;
    LOG1("width:%d, height:%d, deviceMode:%d",
            w, h, deviceMode);

    if ((w <= 0) || (h <= 0)) {
        ALOGE("Wrong Width %d or Height %d", w, h);
        return -1;
    }

    int fd = mCameraSensor[mCameraId]->fd;

    //Switch the Mode before set the format. This is a driver requirement
    ret = set_capture_mode(deviceMode);
    if (ret < 0)
        return ret;

    String8 mode = String8::format("%dx%d", w, h);
    if(mJpegModes.find(mode) != mJpegModes.end()) {
        mJpegDecoder = new JpegDecoder();
        if(mJpegDecoder == NULL)
        {
            ALOGE("create JpegDecoder failed");
            return -1;
        }
        status = mJpegDecoder->init(w,h,all_targets,targetBufNum);
        if(status != JD_SUCCESS) {
            LOGE("init JpegDecoder failed");
            delete mJpegDecoder;
            mJpegDecoder = NULL;
            return -1;
        }
        ALOGI("Camera configured in MJPEG mode, %dx%d\n", w, h);
    }

    //Set the format
    ret = v4l2_capture_s_format(fd, w, h);
    if (ret < 0)
        return ret;

    status = allocateBuffers(numBuffers, w, h, mFormat);
    if (status != NO_ERROR) {
        ALOGE("error allocating buffers");
        ret = -1;
    }

    return ret;
}

int CameraDriver::deconfigureDevice()
{
    status_t status = freeBuffers();
    if (status != NO_ERROR) {
        ALOGE("Error freeing buffers");
        return -1;
    }

    delete mJpegDecoder;
    mJpegDecoder = NULL;

    return 0;
}

int CameraDriver::startDevice()
{
    LOG1("@%s fd=%d", __FUNCTION__, mCameraSensor[mCameraId]->fd);

    int ret;
    int fd = mCameraSensor[mCameraId]->fd;
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    for (int i = 0; i < mBufferPool.numBuffers; i++) {
        status_t status = queueBuffer(&mBufferPool.bufs[i].camBuff, true);
        if (status != NO_ERROR)
            return -1;
    }

    ret = ioctl(fd, VIDIOC_STREAMON, &type);
    if (ret < 0) {
        ALOGE("VIDIOC_STREAMON returned: %d (%s)", ret, strerror(errno));
        return ret;
    }

    return 0;
}

void CameraDriver::stopDevice()
{
    LOG1("@%s", __FUNCTION__);

    int ret;
    int fd = mCameraSensor[mCameraId]->fd;
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    ret = ioctl(fd, VIDIOC_STREAMOFF, &type);
    if (ret < 0) {
        ALOGE("VIDIOC_STREAMOFF returned: %d (%s)", ret, strerror(errno));
    }
}

int CameraDriver::openDevice()
{
    int fd;
    LOG1("@%s", __FUNCTION__);
    if (mCameraSensor[mCameraId] == 0) {
        ALOGE("%s: Try to open non-existent camera", __FUNCTION__);
        return -ENODEV;
    }

    if (mCameraSensor[mCameraId]->fd >= 0) {
        ALOGE("%s: camera is already opened", __FUNCTION__);
        return mCameraSensor[mCameraId]->fd;
    }
    const char *dev_name = mCameraSensor[mCameraId]->devName;

    fd = v4l2_capture_open(dev_name);

    if (fd == -1) {
        ALOGE("V4L2: capture_open failed: %s", strerror(errno));
        return fd;
    }

    // Query and check the capabilities
    struct v4l2_capability cap;
    if (v4l2_capture_querycap(fd, &cap) < 0) {
        ALOGE("V4L2: capture_querycap failed: %s", strerror(errno));
        v4l2_capture_close(fd);
        return -EFAULT;
    }

    mCameraSensor[mCameraId]->fd = fd;

    // Query the supported controls
    querySupportedControls();
    getZoomMaxMinValues();
    getBrightnessMaxMinValues();
    return mCameraSensor[mCameraId]->fd;
}

void CameraDriver::closeDevice()
{
    LOG1("@%s", __FUNCTION__);

    if (mCameraSensor[mCameraId] == 0) {
        ALOGE("%s: Try to open non-existent camera", __FUNCTION__);
        return;
    }

    if (mCameraSensor[mCameraId]->fd < 0) {
        ALOGE("oh no. this should not be happening");
        return;
    }

    v4l2_capture_close(mCameraSensor[mCameraId]->fd);

    mCameraSensor[mCameraId]->fd = -1;
}

CameraBuffer* CameraDriver::findBuffer(void* findMe) const
{

    for (int i = 0; i < mBufferPool.numBuffers; i++) {
        if (mBufferPool.bufs[i].camBuff.hasData(findMe))
            return &(mBufferPool.bufs[i].camBuff);
    }
    return 0;
}

status_t CameraDriver::allocateBuffer(int fd, int index, int w, int h, int format)
{
    struct v4l2_buffer *vbuf = &mBufferPool.bufs[index].vBuff;
    CameraBuffer *camBuf = &mBufferPool.bufs[index].camBuff;
    int ret;

    // query for buffer info
    vbuf->flags = 0x0;
    vbuf->index = index;
    vbuf->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vbuf->memory = V4L2_MEMORY_USERPTR;
    ret = ioctl(fd, VIDIOC_QUERYBUF, vbuf);
    if (ret < 0) {
        ALOGE("VIDIOC_QUERYBUF failed: %s", strerror(errno));
        return UNKNOWN_ERROR;
    }

    // allocate memory
    mBufAlloc->allocateMemory(camBuf, vbuf->length, mCallbacks.get(), w, h, format);
    camBuf->mID = index;
    vbuf->m.userptr = (unsigned int) camBuf->getData();
    LOG1("alloc mem addr=%p, index=%d size=%d", camBuf->getData(), index, vbuf->length);

    return NO_ERROR;
}

status_t CameraDriver::allocateBuffers(int numBuffers, int w, int h, int format)
{
    if (mBufferPool.bufs) {
        ALOGE("fail to alloc. non-null buffs");
        return UNKNOWN_ERROR;
    }

    int ret;
    int fd = mCameraSensor[mCameraId]->fd;
    struct v4l2_requestbuffers reqBuf;
    reqBuf.count = numBuffers;
    reqBuf.memory = V4L2_MEMORY_USERPTR;
    reqBuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    LOG1("VIDIOC_REQBUFS, count=%d", reqBuf.count);
    ret = ioctl(fd, VIDIOC_REQBUFS, &reqBuf);

    if (ret < 0) {
        ALOGE("VIDIOC_REQBUFS(%d) returned: %d (%s)",
            numBuffers, ret, strerror(errno));
        return UNKNOWN_ERROR;
    }

    mBufferPool.bufs = new DriverBuffer[numBuffers];

    status_t status = NO_ERROR;
    for (int i = 0; i < numBuffers; i++) {
        status = allocateBuffer(fd, i, w, h, format);
        if (status != NO_ERROR)
            goto fail;

        mBufferPool.numBuffers++;
    }

    return NO_ERROR;

fail:

    for (int i = 0; i < mBufferPool.numBuffers; i++) {
        freeBuffer(i);
    }

    delete [] mBufferPool.bufs;
    memset(&mBufferPool, 0, sizeof(mBufferPool));

    return status;
}

status_t CameraDriver::freeBuffer(int index)
{
    CameraBuffer *camBuf = &mBufferPool.bufs[index].camBuff;
    camBuf->releaseMemory();
    return NO_ERROR;
}

status_t CameraDriver::freeBuffers()
{
    if (!mBufferPool.bufs) {
        ALOGE("fail to free. null buffers");
        return NO_ERROR; // This is okay, just print an error
    }

    int ret;
    int fd = mCameraSensor[mCameraId]->fd;
    struct v4l2_requestbuffers reqBuf;
    reqBuf.count = 0;
    reqBuf.memory = V4L2_MEMORY_USERPTR;
    reqBuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    for (int i = 0; i < mBufferPool.numBuffers; i++) {
        freeBuffer(i);
    }

    LOG1("VIDIOC_REQBUFS, count=%d", reqBuf.count);
    ret = ioctl(fd, VIDIOC_REQBUFS, &reqBuf);

    if (ret < 0) {
        // Just print an error and continue with dealloc logic
        ALOGE("VIDIOC_REQBUFS returned: %d (%s)",
                ret, strerror(errno));
    }

    delete [] mBufferPool.bufs;
    memset(&mBufferPool, 0, sizeof(mBufferPool));

    return NO_ERROR;
}

status_t CameraDriver::queueBuffer(CameraBuffer *buff, bool init)
{
    // see if we are in session (not initializing the driver with buffers)
    if (init == false) {
        if (buff->mDriverPrivate != mSessionId)
            return DEAD_OBJECT;
    }

    int ret;
    int fd = mCameraSensor[mCameraId]->fd;
    struct v4l2_buffer *vbuff = &mBufferPool.bufs[buff->getID()].vBuff;

    ret = ioctl(fd, VIDIOC_QBUF, vbuff);
    if (ret < 0) {
        ALOGE("VIDIOC_QBUF index %d failed: %s",
             buff->getID(), strerror(errno));
        return UNKNOWN_ERROR;
    }

    mBufferPool.numBuffersQueued++;

    return NO_ERROR;
}
status_t CameraDriver::dequeueBuffer(CameraBuffer **driverbuff, CameraBuffer *yuvbuff, nsecs_t *timestamp, bool forJpeg)
{
    int ret;
    int fd = mCameraSensor[mCameraId]->fd;
    struct v4l2_buffer vbuff;
    RenderTarget *cur_target = yuvbuff->mDecTargetBuf;

    vbuff.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vbuff.memory = V4L2_MEMORY_USERPTR;

    ret = ioctl(fd, VIDIOC_DQBUF, &vbuff);
    if (ret < 0) {
        ALOGE("error dequeuing buffers");
        return UNKNOWN_ERROR;
    }

    CameraBuffer *camBuff = &mBufferPool.bufs[vbuff.index].camBuff;
    camBuff->mID = vbuff.index;
    camBuff->mDriverPrivate = mSessionId;
    *driverbuff = camBuff;

    if (timestamp)
        *timestamp = systemTime();

    mBufferPool.numBuffersQueued--;

    if(mJpegDecoder) {
        void * pSrc = camBuff->getData();
        camBuff->mSize = vbuff.bytesused;
        int len = vbuff.bytesused;

        //write_image(pSrc, len, mConfig.preview.width, mConfig.preview.height, ".jpeg",0);
        JpegInfo *jpginfo = new JpegInfo();
        int status = 0;

        jpginfo->buf = (uint8_t *)pSrc;
        jpginfo->bufsize = len;
        status = mJpegDecoder->parse(*jpginfo);
        if(status != 0)
        {
             ALOGE("parse fail for jpegdec,status=%d",status);
             delete jpginfo;
             return status;
        }
        status = mJpegDecoder->decode(*jpginfo, *cur_target);
        if(status != 0)
        {
            ALOGE("decoder fail,status=%d",status);
            delete jpginfo;
            return status;
        }
        delete jpginfo;
        LOG2("jpegdecoder over");
        /*
        //the following code is used for dump image after jpegdec with mapfunction in libjpegdec
        uint8_t *data;
        uint32_t offsets[3];
        uint32_t pitches[3];
        JpegDecoder::MapHandle maphandle = mJpegDecoder->mapData(*cur_target, (void**) &data, offsets, pitches);
       if(maphandle.valid == 0)
       {
           LOGE("-----maphandle.valid == 0");
       }
       char filename[80];
       snprintf(filename, sizeof(filename), "/data/nv12/dump_%d_%d_%03u_%s", yuvbuff->mDecTargetBuf->width,yuvbuff->mDecTargetBuf->height, count, "yuv422h.yuv");
       count ++;

       FILE* fpdump = fopen(filename, "wb");
       if(fpdump == 0)
       {
           LOGE("-----fpdump == 0");
       }
        // Y
        for (int i = 0; i < yuvbuff->mDecTargetBuf->height; ++i) {
           fwrite(data + offsets[0] + i * pitches[0], 1, yuvbuff->mDecTargetBuf->width, fpdump);
        }
        // U
        for (int i = 0; i < yuvbuff->mDecTargetBuf->height; ++i) {
           fwrite(data + offsets[1] + i * pitches[1], 1, yuvbuff->mDecTargetBuf->width/2, fpdump);
        }
        // V
        for (int i = 0; i < yuvbuff->mDecTargetBuf->height; ++i) {
           fwrite(data + offsets[2] + i * pitches[2], 1, yuvbuff->mDecTargetBuf->width/2, fpdump);
        }
        fclose(fpdump);
       printf("Dumped decoded YUV to /sdcard/dec_dump.yuv\n");
       mJpegDecoder->unmapData(*cur_target, maphandle);
       */
    }

    return NO_ERROR;
}

status_t CameraDriver::querySupportedControls()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    int fd = mCameraSensor[mCameraId]->fd;
    mSupportedControls.zoomAbsolute                = !(v4l2_capture_queryctrl(fd, V4L2_CID_ZOOM_ABSOLUTE));
    mSupportedControls.focusAuto                   = !(v4l2_capture_queryctrl(fd, V4L2_CID_FOCUS_AUTO));
    mSupportedControls.focusAbsolute               = !(v4l2_capture_queryctrl(fd, V4L2_CID_FOCUS_ABSOLUTE));
    mSupportedControls.tiltAbsolute                = !(v4l2_capture_queryctrl(fd, V4L2_CID_TILT_ABSOLUTE ));
    mSupportedControls.panAbsolute                 = !(v4l2_capture_queryctrl(fd, V4L2_CID_PAN_ABSOLUTE));
    mSupportedControls.exposureAutoPriority        = !(v4l2_capture_queryctrl(fd, V4L2_CID_EXPOSURE_AUTO_PRIORITY));
    mSupportedControls.exposureAbsolute            = !(v4l2_capture_queryctrl(fd, V4L2_CID_EXPOSURE_ABSOLUTE));
    mSupportedControls.exposureAuto                = !(v4l2_capture_queryctrl(fd, V4L2_CID_EXPOSURE_AUTO));
    mSupportedControls.backlightCompensation       = !(v4l2_capture_queryctrl(fd, V4L2_CID_BACKLIGHT_COMPENSATION));
    mSupportedControls.sharpness                   = !(v4l2_capture_queryctrl(fd, V4L2_CID_SHARPNESS));
    mSupportedControls.whiteBalanceTemperature     = !(v4l2_capture_queryctrl(fd, V4L2_CID_WHITE_BALANCE_TEMPERATURE));
    mSupportedControls.powerLineFrequency          = !(v4l2_capture_queryctrl(fd, V4L2_CID_POWER_LINE_FREQUENCY));
    mSupportedControls.gain                        = !(v4l2_capture_queryctrl(fd, V4L2_CID_GAIN));
    mSupportedControls.whiteBalanceTemperatureAuto = !(v4l2_capture_queryctrl(fd, V4L2_CID_AUTO_WHITE_BALANCE));
    mSupportedControls.saturation                  = !(v4l2_capture_queryctrl(fd, V4L2_CID_SATURATION));
    mSupportedControls.contrast                    = !(v4l2_capture_queryctrl(fd, V4L2_CID_CONTRAST));
    mSupportedControls.brightness                  = !(v4l2_capture_queryctrl(fd, V4L2_CID_BRIGHTNESS));
    mSupportedControls.hue                         = !(v4l2_capture_queryctrl(fd, V4L2_CID_HUE));
    return status;
}

void CameraDriver::detectDeviceResolutions()
{
    int pmax=0, vmax=0, fd=mCameraSensor[mCameraId]->fd;
    std::set<String8> vidmodes;
    for(int fmt=0; fmt<2; fmt++) {
        // Test YUYV modes first, then MJPEG if it's better
        int pixfmt = fmt == 0 ? mFormat : V4L2_PIX_FMT_MJPEG;
        for(int i=0; /**/; i++) {
            struct v4l2_frmsizeenum fs;
            fs.index = i;
            fs.pixel_format = pixfmt;
            if (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &fs) < 0)
                break;

            int w = fs.discrete.width, h = fs.discrete.height;
/*
            // just for test
            if (fs.discrete.width == 160)
                continue;
            if (fs.discrete.width == 320)
                continue;
            if (fs.discrete.width == 640)
                continue;
*/
            String8 sz = String8::format("%dx%d", w, h);

            int area = w*h;

            // Add to supported picture sizes and record the "best"
            // one seen to select it by default.  Note that we
            // disallow MJPEG modes for still picture.
            mPicSizes += String8(mPicSizes.size() ? "," : "") + sz;
            if (area > pmax && pixfmt != V4L2_PIX_FMT_MJPEG) {
                pmax = area;
                mBestPicSize = sz;
                mConfig.snapshot.setMax(w, h);
                setSnapshotFrameSize(w, h);
            }

            // Now enumerate fps alternatives to see if it's OK for video
            for (int j = 0; /**/; j++) {
                struct v4l2_frmivalenum fi;
                fi.pixel_format = pixfmt;
                fi.width = fs.discrete.width;
                fi.height = fs.discrete.height;
                fi.index = j;
                if (ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &fi) < 0
                    || fi.type != V4L2_FRMIVAL_TYPE_DISCRETE)
                    break;
                double hz = fi.discrete.denominator / (double)fi.discrete.numerator;
                if (hz >= MIN_VIDEO_FPS) {
                    if(pixfmt == V4L2_PIX_FMT_MJPEG) {
                        // For MJPEG modes, check that we don't
                        // already have this size in YUYV, and
                        // make sure the decoder works!
                        if(vidmodes.find(sz) != vidmodes.end()) {
//                           || !JpegDecoder(w, h).valid()) {
                            continue;
                        }
                        mJpegModes.insert(sz);
                        LOG2("@%s, line:%d, mJpegModes insert sz:%s", __FUNCTION__, __LINE__, sz.string());
                    } else
                        continue; // this can let the yuyv output disabled

                    vidmodes.insert(sz);
                    LOG2("@%s, line:%d, insert sz:%s, fmt:%d, j:%d", __FUNCTION__, __LINE__, sz.string(), fmt, j);
                    mVidSizes += String8(mVidSizes.size() ? "," : "") + sz;
                    if (area > vmax) {
                        vmax = area;
                        mBestVidSize = sz;
                        mConfig.preview.setMax(w, h);
                        mConfig.postview.setMax(w, h);
                        mConfig.recording.setMax(w, h);
                        setPreviewFrameSize(w, h);
                        setPostviewFrameSize(w, h);
                        setVideoFrameSize(w, h);
                    }
                    break;
                }
            }
        }
    }

    ALOGD("Detected picture sizes for camera %d: %s\n", mCameraId, mPicSizes.string());
    ALOGD("Detected video/preview sizes for camera %d: %s\n", mCameraId, mVidSizes.string());

    if (!mPicSizes.size()) {
        ALOGE("Failed to detect camera resolution! Use default settings");
        mPicSizes = DEFAULT_PIC_SIZE;
        mBestPicSize = DEFAULT_PIC_SIZE;
        mVidSizes = DEFAULT_VID_SIZE;
        mBestVidSize = DEFAULT_VID_SIZE;
        setSnapshotFrameSize(RESOLUTION_VGA_WIDTH, RESOLUTION_VGA_HEIGHT);
        setPreviewFrameSize(RESOLUTION_VGA_WIDTH, RESOLUTION_VGA_HEIGHT);
        setPostviewFrameSize(RESOLUTION_VGA_WIDTH, RESOLUTION_VGA_HEIGHT);
        setVideoFrameSize(RESOLUTION_VGA_WIDTH, RESOLUTION_VGA_HEIGHT);
    }
}

status_t CameraDriver::getZoomMaxMinValues()
{
    int ret = 0;
    int fd = mCameraSensor[mCameraId]->fd;
    struct v4l2_queryctrl queryctrl;
    memset (&queryctrl, 0, sizeof (queryctrl));
    queryctrl.id = V4L2_CID_ZOOM_ABSOLUTE;
    ret = ioctl(fd, VIDIOC_QUERYCTRL, &queryctrl);
    if (ret ==0)
    {
        mZoomMax = queryctrl.maximum;
        mZoomMin = queryctrl.minimum;
    }
    return ret;
}

status_t CameraDriver::getBrightnessMaxMinValues()
{
    int ret = 0;
    int fd = mCameraSensor[mCameraId]->fd;
    struct v4l2_queryctrl queryctrl;
    memset (&queryctrl, 0, sizeof (queryctrl));
    queryctrl.id = V4L2_CID_BRIGHTNESS;
    ret = ioctl(fd, VIDIOC_QUERYCTRL, &queryctrl);
    if (ret ==0)
    {
        mBrightMax = queryctrl.maximum;
        mBrightMin = queryctrl.minimum;
    }
    return ret;
}

status_t CameraDriver::setFrameInfo(FrameInfo *fi, int width, int height)
{
    if(width > fi->maxWidth || width <= 0)
        width = fi->maxWidth;
    if(height > fi->maxHeight || height <= 0)
        height = fi->maxHeight;
    fi->width = width;
    fi->height = height;
    fi->padding = paddingWidth(mFormat, width, height);
    fi->size = frameSize(mFormat, fi->padding, height);
    LOG1("width(%d), height(%d), pad_width(%d), size(%d)",
         width, height, fi->padding, fi->size);
    return NO_ERROR;
}

status_t CameraDriver::setPreviewFrameSize(int width, int height)
{
    LOG1("@%s", __FUNCTION__);
    return setFrameInfo(&mConfig.preview, width, height);
}

status_t CameraDriver::setPostviewFrameSize(int width, int height)
{
    LOG1("@%s", __FUNCTION__);
    return setFrameInfo(&mConfig.postview, width, height);
}

status_t CameraDriver::setSnapshotFrameSize(int width, int height)
{
    LOG1("@%s", __FUNCTION__);
    return setFrameInfo(&mConfig.snapshot, width, height);
}

void CameraDriver::getVideoSize(int *width, int *height)
{
    if (width && height) {
        *width = mConfig.recording.width;
        *height = mConfig.recording.height;
    }
}

status_t CameraDriver::setVideoFrameSize(int width, int height)
{
    LOG1("@%s", __FUNCTION__);
    int ret = 0;
    status_t status = NO_ERROR;

    if (mConfig.recording.width == width &&
        mConfig.recording.height == height) {
        // Do nothing
        return status;
    }

    if (mMode == MODE_VIDEO) {
        ALOGE("Reconfiguration in video mode unsupported. Stop the driver first");
        return INVALID_OPERATION;
    }
    mConfig.recording.width = width;
    mConfig.recording.height= height;
    return setFrameInfo(&mConfig.snapshot, width, height);
}

void CameraDriver::computeZoomRatios(char *zoom_ratio, int max_count){

    int zoom_step = 1;
    int ratio = mZoomMin ;
    int pos = 0;
    //Get zoom from mZoomMin to mZoomMax
    while((ratio <= mZoomMax + mZoomMin) && (pos < max_count)){
        sprintf(zoom_ratio + pos,"%d,",ratio);
        if (ratio < 1000)
            pos += 4;
        else
            pos += 5;
        ratio += zoom_step;
    }

    //Overwrite the last ',' with '\0'
    if (pos > 0)
        *(zoom_ratio + pos -1 ) = '\0';
}

void CameraDriver::setBufferAllocator(ICameraBufferAllocator* alloc)
{
    if (alloc == 0) {
        ALOGE("Ignore null allocator");
        return;
    }
    mBufAlloc = alloc;
}

void CameraDriver::getZoomRatios(Mode mode, CameraParameters *params)
{
    LOG1("@%s", __FUNCTION__);
    char *mZoomRatios;
    if(mSupportedControls.zoomAbsolute) {
        params->set(CameraParameters::KEY_MAX_ZOOM,mZoomMax);
        static const int zoomBytes = mZoomMax * 5 + 1;
        mZoomRatios = new char[zoomBytes];
        computeZoomRatios(mZoomRatios, zoomBytes);
        params->set(CameraParameters::KEY_ZOOM_RATIOS, mZoomRatios);
        delete[] mZoomRatios;
    } else {
        // zoom is not supported. this is indicated by placing a single zoom ratio in params
        params->set(CameraParameters::KEY_MAX_ZOOM, "0"); // zoom index 0 indicates first (and only) zoom ratio
        params->set(CameraParameters::KEY_ZOOM_RATIOS, "100");
    }
}

void CameraDriver::getFocusDistances(CameraParameters *params)
{
    LOG1("@%s", __FUNCTION__);
    // TODO: set focus distances (CameraParameters::KEY_FOCUS_DISTANCES,)
    params->set(CameraParameters::KEY_FOCUS_DISTANCES,"0.95,1.9,Infinity");
}

status_t CameraDriver::setZoom(int zoom)
{
    LOG1("@%s: zoom = %d", __FUNCTION__, zoom);
    if (zoom == mConfig.zoom)
        return NO_ERROR;
    if (mMode == MODE_CAPTURE)
        return NO_ERROR;

    int ret = set_zoom(mCameraSensor[mCameraId]->fd, zoom);
    if (ret < 0) {
        ALOGE("Error setting zoom to %d", zoom);
        return UNKNOWN_ERROR;
    }
    mConfig.zoom = zoom;
    return NO_ERROR;
}

status_t CameraDriver::getFNumber(unsigned int *fNumber)
{
    LOG1("@%s", __FUNCTION__);

    // TODO: implement

    return NO_ERROR;
}

status_t CameraDriver::getExposureInfo(CamExifExposureProgramType *exposureProgram,
                                       CamExifExposureModeType *exposureMode,
                                       int *exposureTime,
                                       float *exposureBias,
                                       int *aperture)
{
    // TODO: fill these with valid values
    *exposureProgram = EXIF_EXPOSURE_PROGRAM_NORMAL;
    *exposureMode = EXIF_EXPOSURE_AUTO;
    *exposureTime = DEFAULT_EXPOSURE_TIME;
    *exposureBias = (float)mExpBias;
    *aperture = 1;
    return NO_ERROR;
}

status_t CameraDriver::getBrightness(float *brightness)
{
    // TODO: fill these with valid values
    *brightness = 0.0;
    return NO_ERROR;
}

status_t CameraDriver::getIsoSpeed(int *isoSpeed)
{
    // TODO: fill with valid value, 0 is not accepted by testJpegExif
    *isoSpeed = DEFAULT_ISO_SPEED;
    return NO_ERROR;
}

status_t CameraDriver::getMeteringMode(CamExifMeteringModeType *meteringMode)
{
    *meteringMode = EXIF_METERING_UNKNOWN;
    return NO_ERROR;
}

status_t CameraDriver::getAWBMode(CamExifWhiteBalanceType *wbMode)
{
    *wbMode = (mWBMode == WHITE_BALANCE_AUTO) ? EXIF_WB_AUTO : EXIF_WB_MANUAL;
    return NO_ERROR;
}

status_t CameraDriver::getSceneMode(CamExifSceneCaptureType *sceneMode)
{
    *sceneMode = EXIF_SCENE_STANDARD;
    return NO_ERROR;
}

int CameraDriver::set_zoom(int fd, int zoom)
{
    LOG1("@%s", __FUNCTION__);
    if(mSupportedControls.zoomAbsolute) {
        if(set_attribute(fd, V4L2_CID_ZOOM_ABSOLUTE, zoom, "Zoom, Absolute" ) !=0) {
            ALOGE("Error in setting Zoom");
            return INVALID_OPERATION;
        }
    }

    return NO_ERROR;
}

int CameraDriver::set_attribute (int fd, int attribute_num,
                                             const int value, const char *name)
{
    LOG1("@%s", __FUNCTION__);
    struct v4l2_control control;
    struct v4l2_ext_controls controls;
    struct v4l2_ext_control ext_control;

    LOG1("setting attribute [%s] to %d", name, value);

    if (fd < 0)
        return -1;

    control.id = attribute_num;
    control.value = value;
    controls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;
    controls.count = 1;
    controls.controls = &ext_control;
    ext_control.id = attribute_num;
    ext_control.value = value;

    if (ioctl(fd, VIDIOC_S_CTRL, &control) == 0)
        return 0;

    if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls) == 0)
        return 0;

    controls.ctrl_class = V4L2_CTRL_CLASS_USER;
    if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls) == 0)
        return 0;

    ALOGE("Failed to set value %d for control %s (%d) on fd '%d', %s",
        value, name, attribute_num, fd, strerror(errno));
    return -1;
}

int CameraDriver::v4l2_capture_s_format(int fd, int w, int h)
{
    LOG1("@%s", __FUNCTION__);
    int ret;
    struct v4l2_format v4l2_fmt;
    CLEAR(v4l2_fmt);

    v4l2_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    LOG1("VIDIOC_G_FMT");
    ret = ioctl (fd,  VIDIOC_G_FMT, &v4l2_fmt);
    if (ret < 0) {
        ALOGE("VIDIOC_G_FMT failed: %s", strerror(errno));
        return -1;
    }

    v4l2_fmt.fmt.pix.width = w;
    v4l2_fmt.fmt.pix.height = h;
    v4l2_fmt.fmt.pix.pixelformat = mJpegDecoder ? V4L2_PIX_FMT_MJPEG : mFormat;
    v4l2_fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
    LOG1("VIDIOC_S_FMT: width: %d, height: %d, format: %d, field: %d",
                v4l2_fmt.fmt.pix.width,
                v4l2_fmt.fmt.pix.height,
                v4l2_fmt.fmt.pix.pixelformat,
                v4l2_fmt.fmt.pix.field);
    ret = ioctl(fd, VIDIOC_S_FMT, &v4l2_fmt);
    if (ret < 0) {
        ALOGE("VIDIOC_S_FMT failed: %s", strerror(errno));
        return -1;
    }
    return 0;

}

status_t CameraDriver::v4l2_capture_open(const char *devName)
{
    LOG1("@%s", __FUNCTION__);
    int fd;
    struct stat st;

    LOG1("---Open video device %s---", devName);

    if (stat (devName, &st) == -1) {
        ALOGE("Error stat video device %s: %s",
                devName, strerror(errno));
        return -1;
    }

    if (!S_ISCHR (st.st_mode)) {
        ALOGE("%s is not a device", devName);
        return -1;
    }

    fd = open(devName, O_RDWR);

    if (fd == -1) {
        ALOGE("Error opening video device %s: %s",
                devName, strerror(errno));
        return fd;
    }

    return fd;
}

status_t CameraDriver::v4l2_capture_close(int fd)
{
    LOG1("@%s", __FUNCTION__);
    /* close video device */
    LOG1("----close device ---");
    if (fd < 0) {
        ALOGW("Device not opened!");
        return INVALID_OPERATION;
    }

    if (close(fd) < 0) {
        ALOGE("Close video device failed: %s", strerror(errno));
        return UNKNOWN_ERROR;
    }
    return NO_ERROR;
}

status_t CameraDriver::v4l2_capture_querycap(int fd, struct v4l2_capability *cap)
{
    LOG1("@%s", __FUNCTION__);
    int ret = 0;

    ret = ioctl(fd, VIDIOC_QUERYCAP, cap);

    if (ret < 0) {
        ALOGE("VIDIOC_QUERYCAP returned: %d (%s)", ret, strerror(errno));
        return ret;
    }

    if (!(cap->capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        ALOGE("No capture devices");
        return -1;
    }

    if (!(cap->capabilities & V4L2_CAP_STREAMING)) {
        ALOGE("Is not a video streaming device");
        return -1;
    }

    LOG1( "driver:      '%s'", cap->driver);
    LOG1( "card:        '%s'", cap->card);
    LOG1( "bus_info:      '%s'", cap->bus_info);
    LOG1( "version:      %x", cap->version);
    LOG1( "capabilities:      %x", cap->capabilities);

    return ret;
}

status_t CameraDriver::v4l2_capture_queryctrl(int fd, int attribute_num)
{
    LOG1("@%s", __FUNCTION__);
    int ret = 0;
    struct v4l2_queryctrl queryctrl;
    memset (&queryctrl, 0, sizeof (queryctrl));
    queryctrl.id = attribute_num;
    ret = ioctl(fd, VIDIOC_QUERYCTRL, &queryctrl);
    return ret;
}

int CameraDriver::set_capture_mode(Mode deviceMode)
{
    LOG1("@%s", __FUNCTION__);
    struct v4l2_streamparm parm;

    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    parm.parm.capture.capturemode = deviceMode;
    LOG1("%s !! camID %d fd %d", __FUNCTION__, mCameraId, mCameraSensor[mCameraId]->fd);
    if (ioctl(mCameraSensor[mCameraId]->fd, VIDIOC_S_PARM, &parm) < 0) {
        ALOGE("error %s", strerror(errno));
        return -1;
    }

    return 0;
}

int CameraDriver::v4l2_capture_try_format(int fd, int *w, int *h)
{
    LOG1("@%s", __FUNCTION__);
    int ret;
    struct v4l2_format v4l2_fmt;
    CLEAR(v4l2_fmt);

    v4l2_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    v4l2_fmt.fmt.pix.width = *w;
    v4l2_fmt.fmt.pix.height = *h;
    v4l2_fmt.fmt.pix.pixelformat = mFormat;
    v4l2_fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

    ret = ioctl(fd, VIDIOC_TRY_FMT, &v4l2_fmt);
    if (ret < 0) {
        ALOGE("VIDIOC_TRY_FMT returned: %d (%s)", ret, strerror(errno));
        return -1;
    }

    *w = v4l2_fmt.fmt.pix.width;
    *h = v4l2_fmt.fmt.pix.height;

    return 0;
}

status_t CameraDriver::getPreviewFrame(CameraBuffer **driverbuff,CameraBuffer *yuvbuff)
{
    LOG2("@%s", __FUNCTION__);

    if (mMode == MODE_NONE)
        return INVALID_OPERATION;
    return dequeueBuffer(driverbuff,yuvbuff);
}

status_t CameraDriver::putPreviewFrame(CameraBuffer *buff)
{
    LOG2("@%s", __FUNCTION__);
    if (mMode == MODE_NONE)
        return INVALID_OPERATION;

    return queueBuffer(buff);
}

status_t CameraDriver::getRecordingFrame(CameraBuffer **driverbuff, CameraBuffer *yuvbuff,nsecs_t *timestamp)
{
    LOG2("@%s", __FUNCTION__);

    if (mMode == MODE_NONE)
        return INVALID_OPERATION;

    return dequeueBuffer(driverbuff,yuvbuff, timestamp);
}

status_t CameraDriver::putRecordingFrame(CameraBuffer *buff)
{
    LOG2("@%s", __FUNCTION__);
    if (mMode == MODE_NONE)
        return INVALID_OPERATION;

    return queueBuffer(buff);
}

status_t CameraDriver::getSnapshot(CameraBuffer **driverbuff, CameraBuffer *yuvbuff)
{
    LOG2("@%s", __FUNCTION__);

    if (mMode == MODE_NONE)
        return INVALID_OPERATION;

    return dequeueBuffer(driverbuff,yuvbuff, 0, true);
}

status_t CameraDriver::putSnapshot(CameraBuffer *buff)
{
    LOG2("@%s", __FUNCTION__);
    if (mMode == MODE_NONE)
        return INVALID_OPERATION;

    return queueBuffer(buff);
}
status_t CameraDriver::putThumbnail(CameraBuffer *buff)
{
    LOG1("@%s", __FUNCTION__);
    return INVALID_OPERATION;
}

bool CameraDriver::dataAvailable()
{
    return mBufferPool.numBuffersQueued > 0;
}

bool CameraDriver::isBufferValid(const CameraBuffer* buffer) const
{
    return buffer->mDriverPrivate == this->mSessionId;
}

////////////////////////////////////////////////////////////////////
//                          PRIVATE METHODS
////////////////////////////////////////////////////////////////////

int CameraDriver::getNumberOfCameras()
{
    LOG1("@%s", __FUNCTION__);
    return CameraDriver::enumerateCameras();
}

status_t CameraDriver::getCameraInfo(int cameraId, camera_info *cameraInfo)
{
    LOG1("@%s: cameraId = %d", __FUNCTION__, cameraId);
    if (cameraId >= MAX_CAMERAS || cameraId < 0 || mCameraSensor[cameraId] == 0)
        return BAD_VALUE;

    memcpy(cameraInfo, &mCameraSensor[cameraId]->info, sizeof(camera_info));
    LOG1("%s: cameraId = %d, %s, %d", __FUNCTION__, cameraId,
            cameraInfo->facing == CAMERA_FACING_FRONT ? "front" : "back",
            cameraInfo->orientation);
    return NO_ERROR;
}

// Prop definitions.
static const char *PROP_PREFIX = "ro.camera";
static const char *PROP_NUMBER = "number";
static const char *PROP_DEVNAME = "devname";
static const char *PROP_FACING = "facing";
static const char *PROP_ORIENTATION = "orientation";
static const char *PROP_FACING_FRONT = "front";
static const char *PROP_FACING_BACK = "back";

// This function could be called from HAL's get_number_of_cameras() interface
// even before any CameraDriver's instance is created. For ANY errors, we will
// return 0 cameras detected to Android.
int CameraDriver::enumerateCameras(){
    int terminated = 0;
    static struct CameraSensor *newDev;
    int claimed;
    LOG1("@%s", __FUNCTION__);

    Mutex::Autolock _l(mCameraSensorLock);

    // clean up old enumeration.
    cleanupCameras();

    //start a new enumeration for all cameras

    char propKey[PROPERTY_KEY_MAX];
    char propVal[PROPERTY_VALUE_MAX];

    // get total number of cameras
    snprintf(propKey, sizeof(propKey), "%s.%s", PROP_PREFIX, PROP_NUMBER);
    if (0 == property_get(propKey, propVal, 0)) {
        ALOGE("%s: Failed to get number of cameras from prop.", __FUNCTION__);
        goto abort;
    }

    claimed = atoi(propVal);

    if (claimed < 0) {
        ALOGE("%s: Invalid Claimed (%d) camera(s), abort.", __FUNCTION__, claimed);
        goto abort;
    }

    if (claimed > MAX_CAMERAS) {
        ALOGD("%s: Claimed (%d) camera(s), but we only support up to (%d) camera(s)",
                __FUNCTION__, claimed, MAX_CAMERAS);
        claimed = MAX_CAMERAS;
    }

    for (int i = 0; i < claimed; i++) {
        newDev = new CameraSensor;
        if (!newDev) {
            ALOGE("%s: No mem for enumeration, abort.", __FUNCTION__);
            goto abort;
        }
        memset(newDev, 0, sizeof(struct CameraSensor));

        newDev->devName = new char[PROPERTY_VALUE_MAX];
        if (!newDev->devName) {
            ALOGE("%s: No mem for dev name, abort.", __FUNCTION__);
            goto abort;
        }

        // each camera device must have a name
        snprintf(propKey, sizeof(propKey), "%s.%d.%s", PROP_PREFIX, i, PROP_DEVNAME);
        if (0 == property_get(propKey, newDev->devName, 0)) {
            ALOGE("%s: Failed to get name of camera %d from prop, abort.",
                    __FUNCTION__, i);
            goto abort;
        }

        // Setup facing info
        snprintf(propKey, sizeof(propKey), "%s.%d.%s", PROP_PREFIX, i, PROP_FACING);
        if (0 == property_get(propKey, propVal, 0)) {
            ALOGE("%s: Failed to get facing of camera %d from prop, abort.",
                    __FUNCTION__, i);
            goto abort;
        }
        if (!strncmp(PROP_FACING_FRONT, propVal, strlen(PROP_FACING_FRONT))) {
            newDev->info.facing = CAMERA_FACING_FRONT;
        }
        else if (!strncmp(PROP_FACING_BACK, propVal, strlen(PROP_FACING_BACK))) {
            newDev->info.facing = CAMERA_FACING_BACK;
        }
        else {
            ALOGE("%s: Invalid facing of camera %d from prop, abort.",
                    __FUNCTION__, i);
            goto abort;
        }

        // setup orientation
        snprintf(propKey, sizeof(propKey), "%s.%d.%s",
                PROP_PREFIX, i, PROP_ORIENTATION);

        if (0 == property_get(propKey, propVal, 0) ||
                (newDev->info.orientation = atoi(propVal)) < 0) {
            ALOGE("%s: Invalid orientation of camera %d from prop, abort.",
                    __FUNCTION__, i);
            goto abort;
        }

        newDev->fd = -1;

        //It seems we get all info of a new camera
        ALOGD("%s: Detected camera (%d) %s %s %d",
                __FUNCTION__, i, newDev->devName,
                newDev->info.facing == CAMERA_FACING_FRONT ? "front" : "back",
                newDev->info.orientation);
        mCameraSensor[i] = newDev;
        numCameras++;
    }

    return numCameras;

abort:
    ALOGE("%s: Terminate camera enumeration !!", __FUNCTION__);
    cleanupCameras();
    //something wrong, further cleaning job
    if (newDev) {
        if (newDev->devName) {
            delete []newDev->devName;
            newDev->devName = 0;
        }
        delete newDev;
        newDev = 0;
    }
    return 0;
}

// Clean up camera  enumeration info
// Caller needs to take care syncing
void CameraDriver::cleanupCameras(){
    // clean up old enumeration
    LOG1("@%s: clean up", __FUNCTION__);
    for (int i = 0; i < MAX_CAMERAS; i++) {
        if (mCameraSensor[i]) {
            LOG1("@%s: found old camera (%d)", __FUNCTION__, i);
            struct CameraSensor *cam = mCameraSensor[i];
            if (cam->fd > 0) {
                // Should we release buffers?
                close(cam->fd);
                cam->fd = -1;
            }
            if (cam->devName) {
                delete []cam->devName;
                cam->devName = 0;
            }
            delete cam;
            mCameraSensor[i] = 0;
        }
    }
    numCameras = 0;
}

status_t CameraDriver::autoFocus()
{
    LOG1("@%s Feature Implemented", __FUNCTION__);

    struct v4l2_control control;
    int fd = mCameraSensor[mCameraId]->fd;

    memset (&control, 0, sizeof (control));
    control.id = V4L2_CID_FOCUS_AUTO;
    control.value = 1;

    if (-1 == ioctl (fd, VIDIOC_S_CTRL, &control)) {
        ALOGE ("Auto Focus Failure in Camera Driver");
        return UNKNOWN_ERROR;
    }
    LOG1("Auto Focus ..............Done");
    return NO_ERROR;
}

status_t CameraDriver::cancelAutoFocus()
{
    LOG1("@%s Feature Implemented", __FUNCTION__);

    struct v4l2_control control;
    int fd = mCameraSensor[mCameraId]->fd;

    memset (&control, 0, sizeof (control));
    control.id = V4L2_CID_FOCUS_AUTO;
    control.value = 0;

    if (-1 == ioctl (fd, VIDIOC_S_CTRL, &control)) {
        ALOGE ("Cancel Auto Focus Failure in Camera Driver");
        return UNKNOWN_ERROR;
    }
    LOG1("Cancel Auto Focus ..............Done");
    return NO_ERROR;
}

status_t CameraDriver::setEffect(Effect effect)
{
    LOG1("@%s", __FUNCTION__);
    int ret = NO_ERROR;
    int hueVal = 0;
    int saturationVal = 0;
    int fd = mCameraSensor[mCameraId]->fd;

    if ((!mSupportedControls.hue)||(!mSupportedControls.saturation)){
        if(effect != EFFECT_NONE) {
            ALOGE("invalid color effect");
            return BAD_VALUE;
        } else {
            return NO_ERROR;
        }
    } else {
        switch (effect) {
        case EFFECT_NONE:
        {
            hueVal = 0;
            saturationVal = 128;
            break;
        }
        case EFFECT_MONO:
        {
            hueVal = 0;
            saturationVal = 0;
            break;
        }
        case EFFECT_SEPIA:
        {
            hueVal = 1200;
            saturationVal = 16;
            break;
        }
        default:
        {
            ALOGE("invalid color effect");
            ret = BAD_VALUE;
            break;
        }
        }
        if(set_attribute(fd, V4L2_CID_HUE, hueVal ,"Hue" )!=0) {
            ALOGE("Error in writing Hue value");
            ret = -1;
        }
        if(set_attribute(fd, V4L2_CID_SATURATION, saturationVal ,"Saturation" )!=0) {
            ALOGE("Error in writing Saturation value");
            ret = -1;
        }

    }
    return ret;
}

status_t CameraDriver::setFlashMode(FlashMode flashMode)
{
    if (flashMode != FLASH_MODE_OFF) {
        ALOGE("invalid flash mode");
        return BAD_VALUE;
    }

    // Do nothing. FLASH_MODE_OFF is all we support.

    return NO_ERROR;;
}

status_t CameraDriver::setSceneMode(SceneMode sceneMode)
{
    if (sceneMode != SCENE_MODE_AUTO) {
        ALOGE("invalid scene mode");
        return BAD_VALUE;
    }

    // Do nothing. SCENE_MODE_AUTO is all we support.

    return NO_ERROR;;
}

status_t CameraDriver::setFocusMode(FocusMode focusMode, CameraWindow *windows, int numWindows)
{
    if (focusMode != FOCUS_MODE_FIXED) {
        ALOGE("invalid focus mode");
        return BAD_VALUE;
    }

    if (windows != NULL || numWindows != 0) {
        ALOGE("focus windows not supported");
        return INVALID_OPERATION;
    }

    // Do nothing. FOCUS_MODE_FIXED is all we support.

    return NO_ERROR;
}

status_t CameraDriver::setExposureModeBrightness(float expNorm, int expBias)
{
    LOG1("@%s", __FUNCTION__);
    if (mSupportedControls.brightness) {
        int fd = mCameraSensor[mCameraId]->fd;
        int brightVal = 0;
        struct v4l2_control control;

        mExpBias = expBias;
        brightVal = (int)(mBrightMax * expNorm);
        control.id = V4L2_CID_BRIGHTNESS;
        control.value = brightVal;
        if (ioctl(fd, VIDIOC_S_CTRL, &control) == 0)
            return NO_ERROR;
        else {
            ALOGE("falied to set brightness control for camera");
            return BAD_VALUE;
        }
    }
    ALOGE("exposure compensation not supported");
    return BAD_VALUE;
}

status_t CameraDriver::setWhiteBalanceMode(WhiteBalanceMode wbMode)
{
    LOG1("@%s", __FUNCTION__);
    int color_tempreture = 0;
    int ret = NO_ERROR;
    int fd = mCameraSensor[mCameraId]->fd;
    mWBMode = WHITE_BALANCE_AUTO;

    if (wbMode < WHITE_BALANCE_AUTO || wbMode > WHITE_BALANCE_SHADE) {
        ALOGE("invalid white balance, wbMode:%d", wbMode);
        return BAD_VALUE;
    }

    if ((wbMode != WHITE_BALANCE_AUTO) && (!mSupportedControls.whiteBalanceTemperature)) {
        ALOGE("invalid white balance");
        return BAD_VALUE;
    } else if ( wbMode == WHITE_BALANCE_AUTO) {
        if(set_attribute(fd, V4L2_CID_AUTO_WHITE_BALANCE, 1 ,"White Balance Temperature, Auto" ) !=0) {
            ALOGE("Error in setting white balance mode");
            return INVALID_OPERATION;
        }
    } else {
        mWBMode = wbMode;
        switch (wbMode) {
        case WHITE_BALANCE_INCANDESCENT:
        {
            color_tempreture = 2800;
            break;
        }
        case WHITE_BALANCE_FLUORESCENT:
        {
            color_tempreture = 5000;
            break;
        }
        case WHITE_BALANCE_DAYLIGHT:
        {
            color_tempreture = 6000;
            break;
        }
        case WHITE_BALANCE_CLOUDY_DAYLIGHT:
        {
            color_tempreture = 6500;
            break;
        }
        default:
        {
            ALOGE("Unsupported white balance mode");
            ret = -1;
            break;
        }
        }
        if (color_tempreture > 0) {
           if(set_attribute(fd, V4L2_CID_AUTO_WHITE_BALANCE, 0 ,"White Balance Temperature, Auto" ) !=0) {
               ALOGE("Error in setting white balance mode");
           }
        if(set_attribute(fd, V4L2_CID_WHITE_BALANCE_TEMPERATURE, color_tempreture ,"White Balance Temperature" ) !=0) {
               ALOGE("Error in setting white balance mode");
           }
        }
    }

    return NO_ERROR;;
}

status_t CameraDriver::setAeLock(bool lock)
{
    ALOGE("ae lock not supported");
    return INVALID_OPERATION;
}

status_t CameraDriver::setAwbLock(bool lock)
{
    ALOGE("awb lock not supported");
    return INVALID_OPERATION;
}

status_t CameraDriver::setMeteringAreas(CameraWindow *windows, int numWindows)
{
    ALOGE("metering not supported");
    return INVALID_OPERATION;
}

} // namespace android
