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
#include <PVROverlayHAL.h>
#include <OverlayHALUtils.h>
#include <IOverlayDevice.h>

#include <bufferclass_video_linux.h>

#include <fcntl.h>
#include <errno.h>

#include <cutils/log.h>
#include <cutils/atomic.h>
#include <cutils/ashmem.h>
#include <sys/mman.h>

PVROverlayHAL *PVROverlayHAL::mInstance = NULL;

PVROverlayHAL::PVROverlayHAL(){
    pthread_mutex_init(&mLock, NULL);
    mPVR2DHandle = 0;
    mSharedFd = -1;
    mSharedSize = 0;
    mSharedContext = NULL;
    drmFD = -1;
    mDrmModeChanged = false;
    mVideoBridgeIoctl = 0;
}

PVROverlayHAL:: ~PVROverlayHAL() {
    LOGV("%s: destroying overlay HAL...\n", __func__);

    this->lock();

    /*tear down wsbm instance*/
    delete mWsbm;

    pvr2DDestroy();
    drmDestroy();

    this->unlock();

    /*destroy device lock*/
    pthread_mutex_destroy(&mLock);

    mInstance = NULL;
}

bool PVROverlayHAL::createSharedContext()
{
    int fd;
    int size = sizeof(IntelOverlayHALContext);
    IntelOverlayHALContext * sharedContext;

    LOGV("%s\n", __func__);

    this->lock();

    if (mSharedContext) {
        LOGI("%s: shared context already exist\n", __func__);
        this->unlock();
        /*FIXME: should I increase refCount here?*/
        return true;
    }

    if ((fd = ashmem_create_region("intel_overlay_ctx", size)) < 0) {
        LOGE("%s: create share memory failed\n", __func__);
        this->unlock();
        return false;
    }

    sharedContext = (IntelOverlayHALContext *)mmap(NULL, size,
                    PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (sharedContext == MAP_FAILED) {
        LOGE("%s: Map shared Context failed\n", __func__);
        close(fd);
        this->unlock();
        return false;
    }

    /*init sharedContext*/
    memset(sharedContext, 0, size);
    sharedContext->refCount = 1;

    if (pthread_mutexattr_init(&sharedContext->attr)) {
        LOGE("%s: Initialize overlay mutex attr failed\n", __func__);
        goto mutexattr_init_err;
    }

    if (pthread_mutexattr_setpshared(&sharedContext->attr,
                                     PTHREAD_PROCESS_SHARED)) {
        LOGE("%s: set pshared error\n", __func__);
        goto pshared_err;
    }


    if (pthread_mutex_init(&sharedContext->lock, &sharedContext->attr)) {
        LOGE("%s: shared block lock init failed\n", __func__);
        goto mutex_init_err;
    }

    mSharedContext = sharedContext;
    mSharedFd = fd;
    mSharedSize = size;

    this->unlock();

    LOGV("%s: create succussfully fd %d\n", __func__, fd);

    return true;
mutex_init_err:
pshared_err:
    pthread_mutexattr_destroy(&sharedContext->attr);
mutexattr_init_err:
    munmap(sharedContext, size);
    close(fd);
    this->unlock();
    return false;
}

void PVROverlayHAL::destroySharedContext(bool closeFd)
{
    LOGV("%s: close Fd %s\n", __func__, closeFd ? "yes" : "no");

    this->lock();

    if (!mSharedContext) {
        LOGE("%s: Invalid shared context\n", __func__);
        this->unlock();
        return;
    }

    if (android_atomic_dec(&mSharedContext->refCount) == 1) {
        LOGV("%s: destroying mutex...\n", __func__);

        /*destroy it since only control device is using it*/
        if (pthread_mutex_destroy(&mSharedContext->lock)) {
            LOGE("%s: destroy share context lock failed\n", __func__);
        }

        if (pthread_mutexattr_destroy(&mSharedContext->attr)) {
            LOGE("%s: destroy mutex attr failed\n", __func__);
        }
    }

    if (munmap(mSharedContext, mSharedSize)) {
        LOGE("%s: Unmap shared context failed\n", __func__);
    }

    if (closeFd && close(mSharedFd)) {
        LOGE("%s: close shared fd error fd = %d\n", __func__, mSharedFd);
    }

    mSharedContext = NULL;

    this->unlock();
}

bool PVROverlayHAL::openSharedContext(int fd, int size)
{
    IntelOverlayHALContext *sharedContext;

    LOGV("%s: fd %d, size %d\n", __func__, fd, size);

    this->lock();

    if (mSharedContext) {
        LOGI("%s: Shared Context existed\n", __func__);
        this->unlock();
        return true;
    }

    sharedContext = (IntelOverlayHALContext *)mmap(0, size,
                    PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (sharedContext == MAP_FAILED) {
        LOGE("%s: map shared context failed\n", __func__);
        this->unlock();
        return false;
    }

    android_atomic_inc(&sharedContext->refCount);

    LOGV("%s: open successufully, shared context %p\n",
         __func__, sharedContext);

    mSharedFd = fd;
    mSharedSize = size;
    mSharedContext = sharedContext;

    this->unlock();

    return true;
}

int PVROverlayHAL::getSharedFd()
{
    return mSharedFd;
}

int PVROverlayHAL::getSharedSize()
{
    return mSharedSize;
}

void PVROverlayHAL::lock() {
    pthread_mutex_lock(&mLock);
}

void PVROverlayHAL::unlock() {
    pthread_mutex_unlock(&mLock);
}

bool PVROverlayHAL::sharedContextLock()
{
    LOGV("%s:shared context %p\n", __func__, mSharedContext);

    if (!mSharedContext) {
        LOGE("%s: Invalid shared context\n", __func__);
        return false;
    }

    pthread_mutex_lock(&mSharedContext->lock);

    return true;
}

void PVROverlayHAL::sharedContextUnlock()
{
    LOGV("%s:shared context %p\n", __func__, mSharedContext);

    if (!mSharedContext) {
        LOGE("%s: Invalid shared context\n", __func__);
        return;
    }

    pthread_mutex_unlock(&mSharedContext->lock);
}

void PVROverlayHAL::setPipe(int overlayIndex, int pipe)
{
    if(overlayIndex < 0 || overlayIndex > MDFLD_OVERLAY_MAX)
        return;

    if (pipe < 0 || pipe > 2)
        return;

    if (!sharedContextLock())
        return;

    mSharedContext->pipe[overlayIndex] = (IntelDCPipe)pipe;

    sharedContextUnlock();
}

int PVROverlayHAL::getPipe(int overlayIndex, int *pipe)
{
    int p;

    if(overlayIndex < 0 || overlayIndex > MDFLD_OVERLAY_MAX)
        return -EINVAL;

    if (!sharedContextLock()) {
        LOGE("%s: cannot lock shared context\n", __func__);
        return -EINVAL;
    }

    p = mSharedContext->pipe[overlayIndex];

    *pipe = p;

    sharedContextUnlock();

    return 0;
}

void PVROverlayHAL::setSize(int overlayIndex, uint32_t w, uint32_t h)
{
    if (overlayIndex < 0 || overlayIndex > MDFLD_OVERLAY_MAX)
        return;

    if (!sharedContextLock())
        return;

    mSharedContext->size[overlayIndex].width = w;
    mSharedContext->size[overlayIndex].height = h;

    sharedContextUnlock();
}

int PVROverlayHAL::getSize(int overlayIndex, IntelOverlaySize *size)
{
    if (overlayIndex < 0 || overlayIndex > MDFLD_OVERLAY_MAX)
        return -EINVAL;

    if (!size)
        return -EINVAL;

    if (!sharedContextLock()) {
        LOGE("%s: cannot lock shared context\n", __func__);
        return -EINVAL;
    }

    size->width = mSharedContext->size[overlayIndex].width;
    size->height = mSharedContext->size[overlayIndex].height;

    sharedContextUnlock();

    return 0;
}

/**
 * Init PVR2D
 * NOTE: called it with HAL locked;
 */
bool PVROverlayHAL::pvr2DInit()
{
    bool ret = false;
    int pvrDevices = 0;
    PVR2DDEVICEINFO * pvrDevInfo = NULL;
    unsigned long pvrDevID = 0;
    PVR2DERROR eResult = PVR2D_OK;

    LOGV("%s: init...\n", __FUNCTION__);

    if (mPVR2DHandle) {
        LOGV("%s: overlay HAL already has PVR2D handle %p\n",
              __func__, mPVR2DHandle);
        return true;
    }

    pvrDevices = PVR2DEnumerateDevices(0);
    if (pvrDevices <= 0) {
        if (pvrDevices == PVR2DERROR_DEVICE_UNAVAILABLE) {
            LOGE("%s: Cannot connect to PVR services\n", __func__);
            /*FIXME: should I wait here?*/
        }

        LOGE("%s: device not found\n", __FUNCTION__);
        goto pvr_init_err;
    }

    pvrDevInfo =  (PVR2DDEVICEINFO *)malloc(pvrDevices * sizeof(PVR2DDEVICEINFO));
    if (!pvrDevInfo) {
        LOGE("%s: no memory\n", __FUNCTION__);
        goto pvr_init_err;
    }

    pvrDevices = PVR2DEnumerateDevices(pvrDevInfo);
    if(pvrDevices != PVR2D_OK) {
        LOGE("%s: Enumerate device failed\n", __func__);
        goto pvr_init_err;
    }

    /*use the 1st device*/
    pvrDevID = pvrDevInfo[0].ulDevID;
    eResult = PVR2DCreateDeviceContext(pvrDevID, &mPVR2DHandle, 0);
    if (eResult != PVR2D_OK) {
        LOGE("%s: Create device context failed\n", __func__);
        goto pvr_init_err;
    }

    LOGV("%s: pvr2d context inited. handle %p\n", __func__, mPVR2DHandle);
    ret = true;

pvr_init_err:
    if(pvrDevInfo) {
        free(pvrDevInfo);
    }

    return ret;
}

/**
 * Init libdrm
 * NOTE: called it with HAL locked;
 */
bool PVROverlayHAL::drmInit()
{
    LOGV("%s: init...\n", __FUNCTION__);

    /**
     * FIXME: drmOpen will lead to a system hung issue in IMG drop
     * use open instead first.
     */
    drmFD = open("/dev/card0", O_RDWR, 0);

    if (drmFD < 0) {
        LOGE("%s: drmOpen failed\n", __func__);
        return false;
    }

    LOGV("%s: successfully. drmFD %d\n", __func__, drmFD);
    return true;
}

/**
 * destroy PVR2D context
 * NOTE: called it with HAL locked;
 */
void PVROverlayHAL::pvr2DDestroy()
{
    LOGV("%s: destroying...\n", __func__);

    if(mPVR2DHandle) {
        PVR2DDestroyDeviceContext(mPVR2DHandle);
        mPVR2DHandle = 0;
    }
}

/**
 * destroy drm
 * NOTE: called it with HAL locked;
 */
void PVROverlayHAL::drmDestroy()
{
    LOGV("%s: destroying...\n", __func__);

    if(drmFD != -1) {
        drmClose(drmFD);
        drmFD = -1;
    }
}

bool PVROverlayHAL::initialize()
{
    bool ret = false;

    LOGV("%s: init...\n", __func__);

    /*lock the overlay HAL while doing initailize*/
    this->lock();

    if (drmFD < 0) {
        ret = drmInit();
        if(ret == false) {
            LOGE("%s: drmInit failed\n", __func__);
            this->unlock();
            return ret;
        }
    }

    /*get video bridge ioctl offset for external YUV buffer*/
    if (!mVideoBridgeIoctl) {
        ret = getVideoBridgeIoctl();
        if (ret == false) {
            LOGE("%s: failed to video bridge ioctl\n", __func__);
            drmDestroy();
            this->unlock();
            return ret;
        }
    }

    if (!mPVR2DHandle) {
        ret = pvr2DInit();
        if(ret == false) {
            LOGE("%s: pvr2DInit failed\n", __func__);
            drmDestroy();
            this->unlock();
            return ret;
        }
    }

    /*if using TTM buffer, creat a new WSBM object*/
    mWsbm = new PVRWsbm(drmFD);
    ret = mWsbm->initialize();
    if(mWsbm == false) {
        LOGE("%s: WSBM intialization failed\n", __func__);
        pvr2DDestroy();
        drmDestroy();
        this->unlock();
        return ret;
    }

    this->unlock();

    LOGV("%s: finish successfully.\n", __func__);

    return true;
}

bool PVROverlayHAL::allocatePVR2DBuffer(uint32_t size, uint32_t align, PVR2DMEMINFO ** buf)
{
    if(!buf) {
        LOGE("%s: invalid buffer\n", __func__);
        return false;
    }

    if(!mPVR2DHandle) {
        LOGE("%s: overlay HAL is not initialized\n", __func__);
        return false;
    }

    LOGV("%s: allocating PVR2D buffer... size %d\n", __func__, size);

    /*NOTE: buffer size should align to page size, or gttMap will fail*/
    PVR2DERROR eResult = PVR2DMemAlloc(mPVR2DHandle,
                                       align_to(size, 4096),
                                       align,
                                       0,
                                       buf);
    if(eResult != PVR2D_OK) {
        LOGE("%s: pvr2d memory allocation error\n", __func__);
        return false;
    }

    LOGV("%s: allocation successfully\n", __func__);
    return true;
}

bool PVROverlayHAL::destroyPVR2DBuffer(PVR2DMEMINFO * buf)
{
    LOGV("%s: destroying pvr2D buffer %p\n", __func__, buf);

    if(!buf || !mPVR2DHandle) {
        LOGE("%s: invalid parameters. pvr2d handle %p\n",
             __func__, mPVR2DHandle);
        return false;
    }

    PVR2DERROR eResult = PVR2DMemFree(mPVR2DHandle, buf);
    if(eResult != PVR2D_OK) {
        LOGE("%s: pvr2d memory free failed\n", __func__);
        return false;
    }

    return true;
}

bool PVROverlayHAL::gttMap(PVR2DMEMINFO *buf, int *offset, uint32_t gttAlign)
{
    struct psb_gtt_mapping_arg arg;
    void * hKernelMemInfo = NULL;

    if (!buf || !offset) {
        LOGE("%s: invalid parameters.\n", __func__);
        return false;
    }

    LOGV("%s: mapping to gtt. buffer %p, offset %p\n",
         __func__, buf, offset);

    if (drmFD < 0) {
        LOGE("%s: drm is not ready\n", __func__);
        return false;
    }

    hKernelMemInfo = (void *)buf->hPrivateMapData;
    if(!hKernelMemInfo) {
        LOGE("%s: kernel meminfo handle is NULL\n", __func__);
        return false;
    }

    arg.hKernelMemInfo = hKernelMemInfo;
    arg.page_align = gttAlign;

    int ret = drmCommandWriteRead(drmFD, DRM_PSB_GTT_MAP, &arg, sizeof(arg));
    if (ret) {
        LOGE("%s: gtt mapping failed\n", __func__);
        return false;
    }

    *offset =  arg.offset_pages;

    LOGV("%s: mapped succussfully, gtt offset %d\n", __func__, *offset);
    return true;
}

bool PVROverlayHAL::gttUnmap(PVR2DMEMINFO * buf)
{
    struct psb_gtt_mapping_arg arg;
    void * hKernelMemInfo = NULL;

    if(!buf) {
        LOGE("%s: invalid parameters.\n", __func__);
        return false;
    }

    LOGV("%s: unmapping from gtt. buffer %p\n", __func__, buf);

    if(drmFD < 0) {
        LOGE("%s: drm is not ready\n", __func__);
        return false;
    }

    hKernelMemInfo = (void *)buf->hPrivateMapData;
    if(!hKernelMemInfo) {
        LOGE("%s: kernel meminfo handle is NULL\n", __func__);
        return false;
    }

    arg.hKernelMemInfo = hKernelMemInfo;

    int ret = drmCommandWrite(drmFD, DRM_PSB_GTT_UNMAP, &arg, sizeof(arg));
    if(ret) {
        LOGE("%s: gtt unmapping failed\n", __func__);
        return false;
    }

    LOGV("%s: unmapped successfully.\n", __func__);
    return true;
}

bool PVROverlayHAL::allocateOverlayBuffer(struct pvr_overlay_buffer_t * buf)
{
    if (!buf) {
        LOGE("%s: invalid parameter\n", __func__);
        return false;
    }

    LOGV("%s: overlay buffer %p ystride %d uvstride %d\n", __func__,
                                                           buf,
                                                           buf->yStride,
                                                           buf->uvStride);

    PVR2DMEMINFO * pvrMemInfo = NULL;

    /*Should I care about the original meminfo here?*/

    /*allocate pvr2dmeminfo*/
    bool ret = allocatePVR2DBuffer(buf->size, buf->align, &pvrMemInfo);
    if(ret == false) {
        LOGE("%s: pvr2d buffer allocation failed\n", __func__);
        return false;
    }

    /*map meminfo into gtt*/
    int offset = 0;
    ret = gttMap(pvrMemInfo, &offset, buf->gttAlign);

    if(ret == false) {
        LOGE("%s: mapping failed\n", __func__);
        destroyPVR2DBuffer(pvrMemInfo);
        return false;
    }

    buf->overlayBuffer = (void *)pvrMemInfo;
    buf->gttOffset = offset;
    buf->overlayCPUAddress = pvrMemInfo->pBase;

    /*allocate data buffer*/
    pvrMemInfo = NULL;
    if(buf->dataSize) {
        ret = allocatePVR2DBuffer(buf->dataSize, buf->align, &pvrMemInfo);
        if(ret == false) {
            LOGE("%s: allocate data buffer failed\n", __func__);
            gttUnmap((PVR2DMEMINFO *)buf->overlayBuffer);
            destroyPVR2DBuffer((PVR2DMEMINFO *)buf->overlayBuffer);
            return false;
        }

        buf->dataBuffer = (void *)pvrMemInfo;
        buf->dataCPUAddress = pvrMemInfo->pBase;
    }

    LOGV("%s: successfully.\n", __func__);
    return true;
}

bool PVROverlayHAL::destroyOverlayBuffer(struct pvr_overlay_buffer_t * buf)
{
    if (!buf) {
        LOGE("%s: invalid parameter\n", __func__);
        return false;
    }

    PVR2DMEMINFO * pvrMemInfo = (PVR2DMEMINFO *)buf->overlayBuffer;

    if(!pvrMemInfo) {
        LOGE("%s: trying to destroy an uninitialized overlay buffer\n",
             __func__);
        return false;
    }

    /*unmap meminfo from gtt*/
    bool ret = gttUnmap(pvrMemInfo);
    if (ret == false) {
        LOGE("%s: unmapping failed\n", __func__);
        return false;
    }

    /*free meminfo*/
    ret = destroyPVR2DBuffer(pvrMemInfo);
    if (ret == false) {
        LOGE("%s: destroy meminfo failed\n", __func__);
        /*FIXME: How to make sure system works correctly after doing this*/
        return false;
    }

    buf->gttOffset = 0;
    buf->overlayBuffer = NULL;

    /*destroy data buffer*/
    pvrMemInfo = (PVR2DMEMINFO *)buf->dataBuffer;
    if(!pvrMemInfo) {
        LOGE("%s: weird! data buffer is NULL\n", __func__);
        return false;
    }

    ret = destroyPVR2DBuffer(pvrMemInfo);
    if (ret == false) {
        LOGE("%s: destory data buffer failed\n", __func__);
        return false;
    }

    return true;
}

bool PVROverlayHAL::updateOverlay(struct pvr_overlay_buffer_t * controlBuffer,
                                  struct pvr_overlay_control_block_t * controlBlk,
                                  bool loadCoefficients)
{
    struct drm_psb_register_rw_arg arg;
    bool overlayAActive = false;
    bool overlayCActive = false;
    uint32_t overlayAPipe = 0;
    uint32_t overlayCPipe = 0;

    if(!controlBuffer || !controlBlk) {
        LOGE("%s: Invalid parameters\n", __func__);
        return false;
    }

    if(controlBuffer->overlayCPUAddress != controlBlk) {
        LOGE("%s: blk and buffer are not equal\n", __func__);
        return false;
    }

    if(drmFD < 0) {
        LOGE("%s: drm is not initialized\n", __func__);
        return false;
    }

    IntelOverlayDisplayMode displayMode = getOverlayDisplayMode();

    LOGV("%s: displayMode %d\n", __func__, displayMode);

    switch (displayMode) {
    case MDFLD_OVERLAY_SINGLE_MIPI0:
    case MDFLD_OVERLAY_UNKNOWN:
        /*use overlayA & assign it to pipeA*/
        overlayAActive = true;
        overlayAPipe = (0x0 << 6);
        break;
    case MDFLD_OVERLAY_SINGLE_MIPI1:
        /*use overlayA & assign it to pipeA*/
        overlayAActive = true;
        overlayAPipe = (0x01 << 6);
        break;
    case MDFLD_OVERLAY_DUAL_MIPI:
        /*use overlayA/C & assign them to pipeA/C*/
        overlayAActive = true;
        overlayCActive = true;
        overlayAPipe = (0x0 << 6);
        overlayCPipe = (0x01 << 6);
        break;
    case MDFLD_OVERLAY_HDMI_CONNECTED:
        /*use overlayA & assign it to pipeB*/
        overlayAActive = true;
        overlayAPipe = (0x02 << 6);
        break;
    default:
        LOGE("%s: Invalid overlay display mode\n", __func__);
        return false;
    }

    LOGV("%s: updating overlay registers... offset in page %d\n",
         __func__, controlBuffer->gttOffset);

    if (overlayAActive == true) {
        memset(&arg, 0, sizeof(arg));
        arg.overlay_write_mask = 1;
        arg.overlay_read_mask = 0;
        arg.overlay.b_wait_vblank = 0;
        arg.overlay.OVADD = (controlBuffer->gttOffset << 12) | overlayAPipe;
        if (loadCoefficients)
            arg.overlay.OVADD |= 0x1;

        LOGV("%s: ovadd 0x%x\n", __func__, arg.overlay.OVADD);

        int ret = drmCommandWriteRead(drmFD, DRM_PSB_REGISTER_RW, &arg, sizeof(arg));
        if(ret) {
            LOGE("%s: overlay update failed with error code %d\n",
                 __func__, ret);
        }
    }

    if (overlayCActive == true) {
        memset(&arg, 0, sizeof(arg));
        arg.overlay_write_mask = 4;
        arg.overlay_read_mask = 0;
        arg.overlay.b_wait_vblank = 0;
        arg.overlay.OVADD = (controlBuffer->gttOffset << 12) | overlayCPipe;
        if (loadCoefficients)
            arg.overlay.OVADD |= 0x1;

        LOGV("%s: ovadd 0x%x\n", __func__, arg.overlay.OVADD);

        int ret = drmCommandWriteRead(drmFD, DRM_PSB_REGISTER_RW, &arg, sizeof(arg));
        if(ret) {
            LOGE("%s: overlay update failed with error code %d\n",
                 __func__, ret);
        }
    }

    LOGV("%s: updated overlay successfully\n", __func__);

    return true;
}

bool PVROverlayHAL::getVideoBridgeIoctl()
{
    union drm_psb_extension_arg arg;
    /*video bridge ioctl = lnc_video_getparam + 1, I know it's ugly!!*/
    const char lncExt[] = "lnc_video_getparam";
    int ret = 0;

    LOGV("%s: get video bridge ioctl num...\n", __func__);

    if(drmFD <= 0) {
        LOGE("%s: invalid drm fd %d\n", __func__, drmFD);
        return false;
    }

    LOGV("%s: DRM_PSB_EXTENSION %d\n", __func__, DRM_PSB_EXTENSION);

    /*get devOffset via drm IOCTL*/
    strncpy(arg.extension, lncExt, sizeof(lncExt));

    ret = drmCommandWriteRead(drmFD, 6/*DRM_PSB_EXTENSION*/, &arg, sizeof(arg));
    if(ret || !arg.rep.exists) {
        LOGE("%s: get device offset failed with error code %d\n",
                  __func__, ret);
        return false;
    }

    LOGV("%s: video ioctl offset 0x%x\n",
              __func__,
              arg.rep.driver_ioctl_offset + 1);

    mVideoBridgeIoctl = arg.rep.driver_ioctl_offset + 1;

    return true;
}

uint32_t PVROverlayHAL::getBufferHandle(uint32_t device, uint32_t handle)
{
    BC_Video_ioctl_package ioctl_package;
    int ret = 0;

    if (!mVideoBridgeIoctl) {
        LOGE("%s: invalid video bridge ioctl offset\n", __func__);
        return 0;
    }

    LOGV("%s: getting kernel handle for 0x%x from BCD device 0x%x\n",
         __func__, handle, device);

    ioctl_package.ioctl_cmd = BC_Video_ioctl_get_buffer_handle;
    ioctl_package.device_id = device;
    ioctl_package.inputparam = (int)handle;
    ret = drmCommandWriteRead(drmFD,
                              mVideoBridgeIoctl,
                              &ioctl_package,
                              sizeof(ioctl_package));
    if (ret) {
        LOGE("%s: fail to get kernel handle, err = %d\n",
             __func__, ret);
        return 0;
    }

    LOGV("%s: got kernel handle 0x%x\n",
         __func__,ioctl_package.outputparam);

    return ioctl_package.outputparam;
}

bool PVROverlayHAL::wrapExternalTTMBuffer(struct pvr_overlay_buffer_t *buf,
                                          uint32_t device,
                                          uint32_t handle)
{
    if (!buf) {
        LOGE("%s: Invalid externel buffer\n", __func__);
        return false;
    }

    LOGV("%s: wrapping externel buffer handle 0x%x\n", __func__, handle);

    if(!mWsbm) {
        LOGE("%s: WSBM uninitialized.\n", __func__);
        return false;
    }

    uint32_t wsbmKernelHandle = getBufferHandle(device, handle);
    if (!wsbmKernelHandle) {
        LOGE("%s: fail to get wsbmKernel Handle\n", __func__);
        return false;
    }

    void * ttmBuffer;

    bool ret = mWsbm->wrapTTMBuffer(wsbmKernelHandle, &ttmBuffer);
    if (ret == false) {
        LOGE("%s: wrap ttm buffer failed\n", __func__);
        return false;
    }

    buf->overlayBuffer = (void *)ttmBuffer;
    buf->gttOffset = mWsbm->getGttOffset(ttmBuffer);

    LOGV("%s: wrapped overlay buffer. cpu %p, gtt %d\n",
         __func__,
         buf->overlayCPUAddress,
         buf->gttOffset);

    mWsbm->unreferenceTTMBuffer(ttmBuffer);

    LOGV("%s: done\n", __func__);

    return true;
}

void PVROverlayHAL::setOverlayPipe(int overlayIndex, int pipe)
{
    /*lock shared context*/
    if (sharedContextLock() == false) {
        LOGE("%s: lock shared context error\n", __func__);
        return;
    }

    mSharedContext->pipe[overlayIndex] = (IntelDCPipe)pipe;

    sharedContextUnlock();
}

void PVROverlayHAL::setOverlayUsage(int overlayIndex, int usage)
{
    /*lock shared context*/
    if (sharedContextLock() == false) {
        LOGE("%s: lock shared context error\n", __func__);
        return;
    }

    mSharedContext->usage[overlayIndex] = (IntelOverlayUsage)usage;

    sharedContextUnlock();
}

int PVROverlayHAL::getOverlayPipe(int overlayIndex)
{
    int pipe;

    /*lock shared context*/
    if (sharedContextLock() == false) {
        LOGE("%s: lock shared context error\n", __func__);
        return -EINVAL;
    }

    pipe = mSharedContext->pipe[overlayIndex];

    sharedContextUnlock();

    return pipe;
}

int PVROverlayHAL::getOverlayUsage(int overlayIndex)
{
    int usage;

    /*lock shared context*/
    if (sharedContextLock() == false) {
        LOGE("%s: lock shared context error\n", __func__);
        return -EINVAL;
    }

    usage = mSharedContext->usage[overlayIndex];

    sharedContextUnlock();

    return usage;
}

void PVROverlayHAL::setPosition(int overlayIndex, int x, int y,
                                int w, int h)
{
    IntelOverlayPosition *position;

    /*lock shared context*/
    if (sharedContextLock() == false) {
        LOGE("%s: lock shared context error\n", __func__);
        return;
    }

    position = &mSharedContext->position[overlayIndex];

    position->x = x;
    position->y = y;
    position->width = w;
    position->height = h;

    sharedContextUnlock();
}

int PVROverlayHAL::getPosition(int overlayIndex, int *x, int *y,
                               int *w, int *h)
{
    IntelOverlayPosition *position;
    IntelOverlayDisplayMode displayMode;

    if (!x || !y || !w || !h) {
        LOGE("%s: Invalid parameter\n", __func__);
        return -EINVAL;
    }

    /*lock shared context*/
    if (sharedContextLock() == false) {
        LOGE("%s: lock shared context error\n", __func__);
        return -EINVAL;
    }

    position = &mSharedContext->position[overlayIndex];

    displayMode = mSharedContext->modeInfo.displayMode;

    /*display full screen size overlay when HDMI is connected*/
    if (displayMode == MDFLD_OVERLAY_HDMI_CONNECTED) {
        drmModeModeInfoPtr hdmiMode =
            &mSharedContext->modeInfo.modes[MDFLD_OUTPUT_HDMI];
        *x = 0;
        *y = 0;
        *w = hdmiMode->hdisplay;
        *h = hdmiMode->vdisplay;
    } else {
        *x = position->x;
        *y = position->y;
        *w = position->width;
        *h = position->height;

        /*check video position*/
        drmModeModeInfoPtr mipiMode =
            &mSharedContext->modeInfo.modes[MDFLD_OUTPUT_MIPI0];
        if (*x < 0)
            *x = 0;
        if (*y < 0)
            *y = 0;
        if ((*x + *w) > mipiMode->hdisplay)
            *w = mipiMode->hdisplay - *x;
        if ((*y + *h) > mipiMode->vdisplay)
            *h = mipiMode->vdisplay - *y;
    }

    sharedContextUnlock();

    return 0;
}

void PVROverlayHAL::drmModeChanged(struct pvr_overlay_buffer_t * controlBuffer,
                                   struct pvr_overlay_control_block_t * controlBlk)
{
    IntelOverlayDisplayMode oldDisplayMode;
    IntelOverlayDisplayMode newDisplayMode;
    struct drm_psb_register_rw_arg arg;
    uint32_t overlayAPipe = 0;
    bool ret = true;

    if (!mDrmModeChanged)
        return;

    if (!controlBlk || !controlBuffer) {
        LOGE("%s: invalid parameter\n", __func__);
        goto mode_change_done;
    }

    oldDisplayMode = getOverlayDisplayMode();

    /*detect new drm mode*/
    ret = detectDrmModeInfo();
    if (ret == false) {
        LOGE("%s: failed to detect DRM mode\n", __func__);
        goto mode_change_done;
    }

    /*get new drm mode*/
    newDisplayMode = getOverlayDisplayMode();

    LOGV("%s: old %d, new %d\n", __func__, oldDisplayMode, newDisplayMode);

    if (oldDisplayMode == newDisplayMode) {
        goto mode_change_done;
    }

    if (oldDisplayMode == MDFLD_OVERLAY_SINGLE_MIPI0)
        overlayAPipe = (0x0 << 6);
    else if (oldDisplayMode == MDFLD_OVERLAY_HDMI_CONNECTED)
        overlayAPipe = (0x2 << 6);

    /*disable overlay*/
    controlBlk->OCMD &= ~0x1;

    /*disable overlayA by default*/
    memset(&arg, 0, sizeof(arg));
    arg.overlay_write_mask = 1;
    arg.overlay_read_mask = 0;
    arg.overlay.b_wait_vblank = 1;
    arg.overlay.OVADD = (controlBuffer->gttOffset << 12) | overlayAPipe;

    LOGV("%s: ovadd 0x%x\n", __func__, arg.overlay.OVADD);

    ret = drmCommandWriteRead(drmFD, DRM_PSB_REGISTER_RW, &arg, sizeof(arg));
    if(ret) {
        LOGE("%s: overlay update failed with error code %d\n",
             __func__, ret);
    }

    /*disable overlayC if switch to HDMI*/
    if (oldDisplayMode == MDFLD_OVERLAY_DUAL_MIPI ||
        oldDisplayMode == MDFLD_OVERLAY_SINGLE_MIPI1) {
        memset(&arg, 0, sizeof(arg));
        arg.overlay_write_mask = 4;
        arg.overlay_read_mask = 0;
        arg.overlay.b_wait_vblank = 1;
        arg.overlay.OVADD = (controlBuffer->gttOffset << 12) | (0x1 << 6) ;

        LOGV("%s: ovadd 0x%x\n", __func__, arg.overlay.OVADD);

        ret = drmCommandWriteRead(drmFD, DRM_PSB_REGISTER_RW, &arg, sizeof(arg));
        if(ret) {
            LOGE("%s: overlay update failed with error code %d\n",
                 __func__, ret);
        }
    }

    /*enable overlay*/
    controlBlk->OCMD |= 0x1;

mode_change_done:
    setDrmModeChanged(false);
}

bool PVROverlayHAL::detectDrmModeInfo()
{
    LOGV("%s: detecting drm mode info...\n", __func__);

    if (drmFD < 0) {
        LOGE("%s: invalid drm FD\n", __func__);
        return false;
    }

    /*try to get drm resources*/
    drmModeResPtr resources = drmModeGetResources(drmFD);
    if (!resources) {
        LOGE("%s: fail to get drm resources. %s\n", __func__, strerror(errno));
        return false;
    }

    /*get mipi0 info*/
    drmModeConnectorPtr connector = NULL;
    drmModeEncoderPtr encoder = NULL;
    drmModeCrtcPtr crtc = NULL;
    drmModeConnectorPtr connectors[MDFLD_OUTPUT_NUM];
    drmModeModeInfoPtr mode = NULL;

    for (int i = 0; i < resources->count_connectors; i++) {
        connector = drmModeGetConnector(drmFD, resources->connectors[i]);
        if (!connector) {
            LOGW("%s: fail to get drm connector\n", __func__);
            continue;
        }

        int outputIndex = -1;

        if (connector->connector_type == DRM_MODE_CONNECTOR_MIPI) {
            LOGV("%s: got MIPI connector\n", __func__);

            /*see if it's mipi0 or mipi1*/
            if (connector->connector_type_id == 1) {
                LOGV("%s: got MIPI0 connnector\n", __func__);
                outputIndex = MDFLD_OUTPUT_MIPI0;
            } else if (connector->connector_type_id == 2) {
                LOGV("%s: got MIPI1 connector\n", __func__);
                outputIndex = MDFLD_OUTPUT_MIPI1;
            } else {
                LOGV("%s: invalid MIPI connector type\n", __func__);
                continue;
            }
        } else if (connector->connector_type == DRM_MODE_CONNECTOR_DVID) {
            LOGV("%s: got HDMI connector\n", __func__);
            outputIndex = MDFLD_OUTPUT_HDMI;
        }

        /*get & update connection status*/
        sharedContextLock();

        mSharedContext->modeInfo.connections[outputIndex] =
            connector->connection;

        sharedContextUnlock();

        /*get related encoder*/
        encoder = drmModeGetEncoder(drmFD, connector->encoder_id);
        if (!encoder) {
            LOGV("%s: fail to get drm encoder\n", __func__);
            drmModeFreeConnector(connector);
            continue;
        }

        /*get related crtc*/
        crtc = drmModeGetCrtc(drmFD, encoder->crtc_id);
        if (!crtc) {
            LOGV("%s: fail to get drm crtc\n", __func__);
            drmModeFreeEncoder(encoder);
            drmModeFreeConnector(connector);
            continue;
        }

        /*get current mode*/
        sharedContextLock();

        mode = &mSharedContext->modeInfo.modes[outputIndex];
        memcpy(mode, &crtc->mode, sizeof(drmModeModeInfo));

        sharedContextUnlock();

        /*free all crtc/connector/encoder*/
        drmModeFreeCrtc(crtc);
        drmModeFreeEncoder(encoder);
        drmModeFreeConnector(connector);
    }

    drmModeFreeResources(resources);

    /*setup overlay display mode basing on detected connection status*/
    sharedContextLock();

    IntelDrmModeInfo *modeInfo = &mSharedContext->modeInfo;

    drmModeConnection mipi0 =
        modeInfo->connections[MDFLD_OUTPUT_MIPI0];
    drmModeConnection mipi1 =
        modeInfo->connections[MDFLD_OUTPUT_MIPI1];
    drmModeConnection hdmi =
        modeInfo->connections[MDFLD_OUTPUT_HDMI];

    if (hdmi == DRM_MODE_CONNECTED) {
        modeInfo->displayMode = MDFLD_OVERLAY_HDMI_CONNECTED;
    } else if (mipi0 == DRM_MODE_CONNECTED) {
        if (mipi1 == DRM_MODE_CONNECTED)
            modeInfo->displayMode = MDFLD_OVERLAY_DUAL_MIPI;
        else
            modeInfo->displayMode = MDFLD_OVERLAY_SINGLE_MIPI0;
    } else if (mipi1 == DRM_MODE_CONNECTED) {
        if (mipi0 == DRM_MODE_CONNECTED)
            modeInfo->displayMode = MDFLD_OVERLAY_DUAL_MIPI;
        else
            modeInfo->displayMode = MDFLD_OVERLAY_SINGLE_MIPI1;
    } else {
        modeInfo->displayMode = MDFLD_OVERLAY_UNKNOWN;
    }

    LOGV("%s: mipi0 %s, mipi1 %s, hdmi %s\n",
        __func__,
        ((mipi0 == DRM_MODE_CONNECTED) ? "connected" : "disconnected"),
        ((mipi1 == DRM_MODE_CONNECTED) ? "connected" : "disconnected"),
        ((hdmi == DRM_MODE_CONNECTED) ? "connected" : "disconnected"));

    modeInfo->initialized = true;

    sharedContextUnlock();

    return true;
}

IntelOverlayDisplayMode PVROverlayHAL::getOverlayDisplayMode()
{
    IntelOverlayDisplayMode displayMode;

    LOGV("%s: getting overlay display mode...\n", __func__);

    sharedContextLock();

    displayMode = mSharedContext->modeInfo.displayMode;

    sharedContextUnlock();

    LOGV("%s: display mode %d\n", __func__, displayMode);

    return displayMode;
}

void PVROverlayHAL::setDrmModeChanged(bool changed)
{
    this->lock();

    mDrmModeChanged = changed;

    this->unlock();
}
