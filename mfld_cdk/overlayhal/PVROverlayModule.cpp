/*
 * Copyright (C) 2008 The Android Open Source Project
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
#include <PVROverlayControlDevice.h>
#include <PVROverlayDataDevice.h>
#include <PVROverlay.h>
#include <OverlayHALUtils.h>
#include <PVROverlayHAL.h>
#include <fcntl.h>
#include <errno.h>

#include <cutils/log.h>
#include <cutils/atomic.h>
#include <math.h>

/***********************************************************************
 * Overlay HAL Data module callbacks implementation
 **********************************************************************/
int overlay_initialize(struct overlay_data_device_t *dev,
                       overlay_handle_t handle)
{
    LOGV("%s: overlay init.... %p\n", __func__, handle);

    struct pvr_overlay_handle_t * pvrOverlayHandle =
        (pvr_overlay_handle_t *)(handle);

    if(!pvrOverlayHandle) {
        LOGE("%s: overlay handle is NULL\n", __func__);
        return -EINVAL;
    }

    if(!dev) {
        LOGE("%s: overlay data device is NULL\n", __func__);
        return -EINVAL;
    }

    PVROverlayDataDevice * pvrDataDevice =
        static_cast<PVROverlayDataDevice *>(dev);

    bool ret = pvrDataDevice->initialize(pvrOverlayHandle);
    if(ret == false) {
        LOGE("%s: overlay data device initialize failed\n", __func__);
        return -EINVAL;
    }

    LOGV("%s: overlay initialized successfully\n", __func__);

    return 0;
}

int overlay_setCrop(struct overlay_data_device_t *dev,
                    uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    LOGV("%s: x %d, y %d, w %d h %d\n", __func__, x, y, w, h);

    if(!dev) {
        LOGE("%s: invalid parameter\n", __func__);
        return -EINVAL;
    }

    PVROverlayDataDevice * pvrDataDevice =
        static_cast<PVROverlayDataDevice *>(dev);

    bool ret = pvrDataDevice->setCrop(x, y, w, h);
    if(ret == false) {
        LOGE("%s: setCrop failed\n", __func__);
        return -EINVAL;
    }

    LOGV("%s: done successfully\n", __func__);

    return true;
}

int overlay_getCrop(struct overlay_data_device_t *dev,
                    uint32_t *x, uint32_t *y, uint32_t *w, uint32_t *h)
{
    LOGV("%s: getting Crop...\n", __func__);

    if(!dev || !x || !y || !w || !h) {
        LOGE("%s: invalid parameter\n", __func__);
        return -EINVAL;
    }

    PVROverlayDataDevice * pvrDataDevice =
        static_cast<PVROverlayDataDevice *>(dev);

    bool ret = pvrDataDevice->getCrop(x, y, w, h);
    if(ret == false) {
        LOGE("%s: getCrop failed\n", __func__);
        return -EINVAL;
    }

    LOGV("%s: done successfully. (%d, %d, %d, %d)\n",
         __func__, *x, *y, *w, *h);

    return true;
}

int overlay_dequeueBuffer(struct overlay_data_device_t *dev,
                          overlay_buffer_t* buf)
{
    LOGV("%s: overlay dequeue buffer ...\n", __func__);

    PVROverlayDataDevice * pvrDataDevice =
        static_cast<PVROverlayDataDevice *>(dev);

    if(!pvrDataDevice) {
        LOGE("%s: invalid overlay data device\n", __func__);
        *buf = NULL;
        return -EINVAL;
    }

    /*get first availiabe data buffer*/
    struct pvr_overlay_buffer_t * dataBuffer = pvrDataDevice->getBuffer();
    if(!dataBuffer) {
        LOGE("%s: wierd thing happened, NULL data buffer\n", __func__);
        *buf = NULL;
        return -EINVAL;
    }

    *buf = (overlay_buffer_t)dataBuffer;

    LOGV("%s: get free overlay data buffer %p\n",
         __func__, dataBuffer->overlayBuffer);

    return 0;
}

int overlay_setParameter(struct overlay_data_device_t *dev,
                         int param, int value)
{
    LOGV("%s: param: %d, value %d\n", __func__, param, value);

    if (!dev) {
        LOGE("%s: invalid data device\n", __func__);
        return -EINVAL;
    }

    PVROverlayDataDevice * pvrDataDevice =
        static_cast<PVROverlayDataDevice *>(dev);

    switch (param) {
    case MDFLD_OVERLAY_PIPE:
        if ((value < 0) || (value > 2)) {
            LOGE("%s: Invaild pipe %d\n", __func__, value);
            return -EINVAL;
        }
        pvrDataDevice->setOverlayPipe(value);
        break;
    case MDFLD_OVERLAY_USAGE:
        if (value != MDFLD_OVERLAY_CLONE &&
            value != MDFLD_OVERLAY_EXTEND) {
            LOGE("%s: usage must be one of clone/extend\n", __func__);
            return -EINVAL;
        }
        pvrDataDevice->setOverlayUsage(value);
        break;
    case MDFLD_OVERLAY_DISPLAY_BUFFER:
        struct overlay_display_buffer_param *param;
        if (!value) {
            LOGE("%s: should be a pointer\n", __func__);
            return -EINVAL;
        }
        param = (struct overlay_display_buffer_param *)value;
        pvrDataDevice->postExternal(param->device, param->handle);
        break;
    case MDFLD_OVERLAY_Y_STRIDE:
        pvrDataDevice->setStride(value);
        break;
    case MDFLD_OVERLAY_RESET:
        pvrDataDevice->resetOverlay();
        break;
    case MDFLD_OVERLAY_HDMI_STATUS:
        if (value != MDFLD_HDMI_STATUS_CONNECTED_EXTEND &&
            value != MDFLD_HDMI_STATUS_DISCONNECTED) {
            LOGE("%s: invalid hdmi status %d\n", __func__, value);
            return -EINVAL;
        }
        pvrDataDevice->setDrmModeChanged(true);
    default:
        return -EINVAL;
    }

    return 0;
}

int overlay_queueBuffer(struct overlay_data_device_t *dev,
                        overlay_buffer_t buffer)
{
    LOGV("%s: overlay queue buffer ...\n", __func__);

    PVROverlayDataDevice * pvrDataDevice =
        static_cast<PVROverlayDataDevice *>(dev);

    if(!pvrDataDevice) {
        LOGE("%s: invalid overlay data device\n", __func__);
        return -EINVAL;
    }

    struct pvr_overlay_buffer_t * pvrOverlayBuffer =
        (struct pvr_overlay_buffer_t *)buffer;

    if(!pvrOverlayBuffer) {
        LOGE("%s: invalid overlay buffer\n", __func__);
        return -EINVAL;
    }

    /*post it*/
    pvrDataDevice->post(pvrOverlayBuffer);

    /*lock this device in order to upate the buffer list*/

    /*check buffer vsync status*/
    if(pvrOverlayBuffer->vsyncState == PVR_OVERLAY_VSYNC_DONE) {
        pvrDataDevice->putBuffer(pvrOverlayBuffer);
    } else {
        pvrOverlayBuffer->vsyncState = PVR_OVERLAY_VSYNC_INIT;
        pvrDataDevice->putBuffer(pvrOverlayBuffer);
    }

    LOGV("%s: overlay queue buffer %p done.\n",
         __func__, pvrOverlayBuffer->overlayBuffer);

    return 0;
}

void * overlay_getBufferAddress(struct overlay_data_device_t *dev,
                                overlay_buffer_t buffer)
{
    LOGV("%s: get buffer address.\n", __func__);

    if(!buffer || !dev) {
        LOGE("%s: invalid parameters\n", __func__);
        return NULL;
    }

    struct pvr_overlay_buffer_t * pvrOverlayBuffer =
        (struct pvr_overlay_buffer_t *)buffer;

    return (void *)pvrOverlayBuffer->dataCPUAddress;
}

static int overlay_data_close(struct hw_device_t *dev)
{
    overlay_data_device_t * dataDevice = (overlay_data_device_t *)dev;
    if((! dataDevice)) {
        LOGE("%s: invalid parameter\n", __func__);
        return -EINVAL;
    }

    PVROverlayDataDevice  *pvrDataDevice =
        static_cast<PVROverlayDataDevice *>(dataDevice);

    /*delete data device*/
    delete pvrDataDevice;


    /*
     * delete HAL instance
     * NOTE: comment this out for super source usage.
     * overlay usage of super source was not compatible with Android overlay
     * architecture.
     * FIXME: uncomment it.
     */
    // delete PVROverlayHAL::getInstance();

    return 0;
}

/***********************************************************************
 * Overlay HAL Control module callbacks implementation
 **********************************************************************/
static int overlay_get(struct overlay_control_device_t *dev, int name)
{
    int result = -1;
    switch (name) {
    case OVERLAY_MINIFICATION_LIMIT:
        result = 0; // 0 = no limit
        break;
    case OVERLAY_MAGNIFICATION_LIMIT:
        result = 0; // 0 = no limit
        break;
    case OVERLAY_SCALING_FRAC_BITS:
        result = 0; // 0 = infinite
        break;
    case OVERLAY_ROTATION_STEP_DEG:
        result = 90; // 90 rotation steps (for instance)
        break;
    case OVERLAY_HORIZONTAL_ALIGNMENT:
        result = 1; // 1-pixel alignment
        break;
    case OVERLAY_VERTICAL_ALIGNMENT:
        result = 1; // 1-pixel alignment
        break;
    case OVERLAY_WIDTH_ALIGNMENT:
        result = 1; // 1-pixel alignment
        break;
    case OVERLAY_HEIGHT_ALIGNMENT:
        result = 1; // 1-pixel alignment
        break;
    }

    return result;
}

static overlay_t* overlay_createOverlay(struct overlay_control_device_t *dev,
                                        uint32_t w, uint32_t h,
                                        int32_t format)
{
    uint32_t yStride = 0;
    uint32_t uvStride = 0;
    uint32_t size = 0;
    uint32_t dataSize = 0;
    uint32_t yBufferSize = 0;
    uint32_t uBufferSize = 0;
    uint32_t vBufferSize = 0;

    PVROverlayControlDevice * pvrControlDevice =
        static_cast<PVROverlayControlDevice *>(dev);

    if(!pvrControlDevice) {
        LOGE("%s: overlay control device is NULL\n", __func__);
        return NULL;
    }

    /*check width and height*/
    if (w <= 0 || w > PVR_OVERLAY_MAX_WIDTH ||
        h <= 0 || h > PVR_OVERLAY_MAX_HEIGHT) {
        LOGE("%s: Invalid overlay dimension %d x %d\n",
                  __func__, w, h);
        return NULL;
    }

    /* check the input params, reject if not supported or invalid */
    switch (format) {
    case OVERLAY_FORMAT_YCbYCr_420_I:           /*I420*/
    //case OVERLAY_FORMAT_CbYCrY_420_I:	        /*YV12*/
        yStride = align_to(w, 64);
        uvStride = align_to(w >> 1, 64);
        yBufferSize = align_to(yStride * h, 4096);
        uBufferSize = align_to(uvStride * (h >> 1), 4096);
        vBufferSize = uBufferSize;
        dataSize = (w * h) + (((w >> 1) * (h >> 1)) << 1);
        break;
    case OVERLAY_FORMAT_YCbCr_420_SP:           /*NV12*/
        yStride = align_to(w, 64);
        uvStride = yStride;
        yBufferSize = align_to(yStride * h, 4096);
        uBufferSize = align_to(yStride * (h >> 1), 4096);
        vBufferSize = 0;
        dataSize = (w * h) + (((w >> 1) * (h >> 1)) << 1);
        break;
    case OVERLAY_FORMAT_YCbYCr_422_I:           /*YUY2*/
    //case OVERLAY_FORMAT_CbYCrY_422_I:         /*UYVY*/
        yStride = align_to(w << 1, 64);
        uvStride = 0;
        yBufferSize = align_to(yStride * h, 4096);
        uBufferSize = 0;
        vBufferSize = 0;
        dataSize = (w * h) << 1;
        break;
    default:
        LOGE("%s: unsupported format %d\n", __func__, format);
        return NULL;
    }

    /*TODO: check request width and height*/

    LOGV("%s: creating new overlay (%dx%d) format %d\n",
         __func__, w, h, format);

    size = yBufferSize + uBufferSize + vBufferSize;

    PVROverlay * overlay = pvrControlDevice->getOverlay(w, h, format,
        yStride, uvStride, size, dataSize);
    if (!overlay) {
        LOGE("No overlay valid\n");
        return NULL;
    }

    return overlay;
}

static void overlay_destroyOverlay(struct overlay_control_device_t *dev,
                                   overlay_t* overlay)
{
    LOGV("%s\n", __func__);

    if (!dev || !overlay) {
        LOGE("%s: invalid parameters\n", __func__);
        return;
    }

    PVROverlayControlDevice *pvrControlDevice =
        static_cast<PVROverlayControlDevice *>(dev);

    PVROverlay *pvrOverlay = static_cast<PVROverlay *>(overlay);

    pvrControlDevice->putOverlay(pvrOverlay);
}

static int overlay_setPosition(struct overlay_control_device_t *dev,
                               overlay_t* overlay,
                               int x, int y, uint32_t w, uint32_t h)
{
    LOGV("setPosition: %d, %d, %d, %d\n", x, y, w, h);

    if (!dev || !overlay) {
        LOGE("%s: invalid parameters\n", __func__);
        return -EINVAL;
    }

    PVROverlayControlDevice *pvrControlDevice =
        static_cast<PVROverlayControlDevice *>(dev);

    PVROverlay *pvrOverlay = static_cast<PVROverlay *>(overlay);

    if (!pvrControlDevice || !pvrOverlay) {
        LOGE("%s: Invalid parameters\n", __func__);
        return -EINVAL;
    }

    uint32_t overlayIndex = pvrOverlay->getOverlayIndex();

    LOGV("setPostion: setting position for overlay %s\n",
         overlayIndex ? "C" : "A");

    pvrControlDevice->setPosition(overlayIndex, x, y, w, h);

    return 0;
}

static int overlay_getPosition(struct overlay_control_device_t *dev,
                               overlay_t* overlay,
                               int* x, int* y, uint32_t* w, uint32_t* h)
{
    /*TODO: implement this later*/
    return -EINVAL;
}

static int overlay_setParameter(struct overlay_control_device_t *dev,
                                overlay_t* overlay,
                                int param, int value)
{

    int result = 0;

    LOGV("%s: param %d, value %d\n", __func__, param, value);

    switch (param) {
    case OVERLAY_ROTATION_DEG:
        break;
    case OVERLAY_DITHER:
        break;
    case OVERLAY_TRANSFORM:
        break;
    default:
        result = -EINVAL;
        break;
    }
    return result;
}

static int overlay_control_close(struct hw_device_t *dev)
{
    struct overlay_control_device_t* controlDevice =
        (struct overlay_control_device_t*)dev;

    if(!controlDevice) {
        LOGE("%s: dev is NULL\n", __func__);
        return -EINVAL;
    }

    LOGV("%s: closing overlay control device...", __func__);

    PVROverlayControlDevice * pvrControlDevice =
        static_cast<PVROverlayControlDevice *>(controlDevice);

    delete pvrControlDevice;

    delete PVROverlayHAL::getInstance();

    return 0;
}

static int overlay_commit(struct overlay_control_device_t *dev,
                          overlay_t* overlay) {
    LOGV("%s\n", __func__);
    return 0;
}

/***********************************************************************
 * Overlay HAL open callback implementation
 **********************************************************************/
static int intel_overlay_device_open(const struct hw_module_t* module,
                                     const char* name,
                                     struct hw_device_t** device)
{
    LOGV("%s: name %s\n", __func__, name);

    int status = -EINVAL;

    if ((PVROverlayHAL::Instance().initialize()) == false) {
        LOGE("%s: overlay hardware init failed\n", __FUNCTION__);
        return status;
    }

    /*creat overlay device for this request*/
    if (!strcmp(name, OVERLAY_HARDWARE_CONTROL)) {
        PVROverlayControlDevice * dev = new PVROverlayControlDevice();

        /* initialize the procs */
        dev->overlay_control_device_t::common.tag = HARDWARE_DEVICE_TAG;
        dev->overlay_control_device_t::common.version = 0;
        dev->overlay_control_device_t::common.module =
            const_cast<hw_module_t*>(module);
        dev->overlay_control_device_t::common.close = overlay_control_close;
        dev->overlay_control_device_t::get = overlay_get;
        dev->overlay_control_device_t::createOverlay = overlay_createOverlay;
        dev->overlay_control_device_t::destroyOverlay = overlay_destroyOverlay;
        dev->overlay_control_device_t::setPosition = overlay_setPosition;
        dev->overlay_control_device_t::getPosition = overlay_getPosition;
        dev->overlay_control_device_t::setParameter = overlay_setParameter;
        dev->overlay_control_device_t::commit = overlay_commit;

        *device = &dev->overlay_control_device_t::common;
        status = 0;
    } else if (!strcmp(name, OVERLAY_HARDWARE_DATA)) {
        PVROverlayDataDevice * dev = new PVROverlayDataDevice();

        /* initialize the procs */
        dev->overlay_data_device_t::common.tag = HARDWARE_DEVICE_TAG;
        dev->overlay_data_device_t::common.version = 0;
        dev->overlay_data_device_t::common.module = const_cast<hw_module_t*>(module);
        dev->overlay_data_device_t::common.close = overlay_data_close;

        dev->overlay_data_device_t::initialize = overlay_initialize;
        dev->overlay_data_device_t::dequeueBuffer = overlay_dequeueBuffer;
        dev->overlay_data_device_t::queueBuffer = overlay_queueBuffer;
        dev->overlay_data_device_t::setParameter = overlay_setParameter;
        dev->overlay_data_device_t::getBufferAddress = overlay_getBufferAddress;
        dev->overlay_data_device_t::setCrop = overlay_setCrop;
        dev->overlay_data_device_t::getCrop = overlay_getCrop;

        *device = &dev->overlay_data_device_t::common;
        status = 0;
    }

    LOGV("%s: open successfully\n", __FUNCTION__);

    return status;
}

/***********************************************************************
 * Overlay HAL module handler
 **********************************************************************/
static struct hw_module_methods_t intel_overlay_module_methods = {
    open: intel_overlay_device_open
};

struct overlay_module_t HAL_MODULE_INFO_SYM = {
    common: {
        tag: HARDWARE_MODULE_TAG,
        version_major: 1,
        version_minor: 0,
        id: OVERLAY_HARDWARE_MODULE_ID,
        name: "Intel Overlay module",
        author: "Intel UMSE",
        methods: &intel_overlay_module_methods,
    }
};
