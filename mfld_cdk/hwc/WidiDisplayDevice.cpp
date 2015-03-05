/*
 * Copyright © 2013 Intel Corporation
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Brian Rogers <brian.e.rogers@intel.com>
 *
 */
#include <cutils/log.h>
#include <utils/Errors.h>

#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>

#include <IntelHWComposer.h>
#include <IntelDisplayDevice.h>
#include <WidiDisplayDevice.h>
#include <IntelHWComposerCfg.h>

using namespace android;

WidiDisplayDevice::CachedBuffer::CachedBuffer(IntelBufferManager *gbm, IntelDisplayBuffer* buffer)
    : grallocBufferManager(gbm),
      displayBuffer(buffer)
{
}

WidiDisplayDevice::CachedBuffer::~CachedBuffer()
{
    grallocBufferManager->unmap(displayBuffer);
}

WidiDisplayDevice::HeldCscBuffer::HeldCscBuffer(const sp<WidiDisplayDevice>& wdd, const sp<GraphicBuffer>& gb)
    : wdd(wdd),
      buffer(gb)
{
}

WidiDisplayDevice::HeldCscBuffer::~HeldCscBuffer()
{
    Mutex::Autolock _l(wdd->mCscLock);
    if (buffer->getWidth() == wdd->mCscWidth && buffer->getHeight() == wdd->mCscHeight)
        wdd->mAvailableCscBuffers.push_back(buffer);
    else
        wdd->mCscBuffersToCreate++;
}

WidiDisplayDevice::HeldDecoderBuffer::HeldDecoderBuffer(const android::sp<CachedBuffer>& cachedBuffer)
    : cachedBuffer(cachedBuffer)
{
    intel_gralloc_payload_t *p = (intel_gralloc_payload_t*)cachedBuffer->displayBuffer->getCpuAddr();
    p->renderStatus = 1;
}

WidiDisplayDevice::HeldDecoderBuffer::~HeldDecoderBuffer()
{
    intel_gralloc_payload_t *p = (intel_gralloc_payload_t*)cachedBuffer->displayBuffer->getCpuAddr();
    p->renderStatus = 0;
}

WidiDisplayDevice::WidiDisplayDevice(IntelBufferManager *bm,
                                     IntelBufferManager *gm,
                                     IntelDisplayPlaneManager *pm,
                                     IntelHWComposerDrm *drm,
                                     WidiExtendedModeInfo *extinfo,
                                     uint32_t index)
                                   : IntelDisplayDevice(pm, drm, bm, gm, index),
                                     mExtendedModeInfo(extinfo),
                                     mExtLastKhandle(0),
                                     mExtLastTimestamp(0),
                                     mGrallocModule(0),
                                     mGrallocDevice(0)
{
    ALOGD_IF(ALLOW_WIDI_PRINT, "%s", __func__);

    //check buffer manager
    if (!mBufferManager) {
        ALOGE("%s: Invalid buffer manager", __func__);
        goto init_err;
    }

    // check buffer manager for gralloc buffer
    if (!mGrallocBufferManager) {
        ALOGE("%s: Invalid Gralloc buffer manager", __func__);
        goto init_err;
    }

    // check display plane manager
    if (!mPlaneManager) {
        ALOGE("%s: Invalid plane manager", __func__);
        goto init_err;
    }

    mLayerList = NULL; // not used

    mNextConfig.typeChangeListener = NULL;
    mNextConfig.policy.scaledWidth = 0;
    mNextConfig.policy.scaledHeight = 0;
    mNextConfig.policy.xdpi = 96;
    mNextConfig.policy.ydpi = 96;
    mNextConfig.policy.refresh = 60;
    mNextConfig.extendedModeEnabled = false;
    mNextConfig.forceNotify = false;
    mCurrentConfig = mNextConfig;
    mLayerToSend = 0;

    mCscBuffersToCreate = 4;
    mCscWidth = 0;
    mCscHeight = 0;

    memset(&mLastInputFrameInfo, 0, sizeof(mLastInputFrameInfo));
    memset(&mLastOutputFrameInfo, 0, sizeof(mLastOutputFrameInfo));

    // init class private members
    hw_module_t const* module;

    if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module) == 0) {
        mGrallocModule = (IMG_gralloc_module_public_t*)module;
        gralloc_open(module, &mGrallocDevice);
    }

    if (!mGrallocModule || !mGrallocDevice) {
        LOGE("Init failure");
        return;
    }

    mInitialized = true;

    {
        // Publish frame server service with service manager
        status_t ret = defaultServiceManager()->addService(String16("hwc.widi"), this);
        if (ret != NO_ERROR) {
            ALOGE("%s: Could not register hwc.widi with service manager, error = %d", __func__, ret);
        }
        ProcessState::self()->startThreadPool();
    }

    return;

init_err:
    mInitialized = false;
    return;
}

WidiDisplayDevice::~WidiDisplayDevice()
{
    ALOGI("%s", __func__);
}

sp<WidiDisplayDevice::CachedBuffer> WidiDisplayDevice::getMappedBuffer(uint32_t handle)
{
    ssize_t index = mMappedBufferCache.indexOfKey(handle);
    sp<CachedBuffer> cachedBuffer;
    if (index == NAME_NOT_FOUND) {
        IntelDisplayBuffer* displayBuffer = mGrallocBufferManager->map(handle);
        if (displayBuffer != NULL) {
            cachedBuffer = new CachedBuffer(mGrallocBufferManager, displayBuffer);
            mMappedBufferCache.add(handle, cachedBuffer);
        }
    }
    else {
        cachedBuffer = mMappedBufferCache[index];
    }
    return cachedBuffer;
}

status_t WidiDisplayDevice::start(sp<IFrameTypeChangeListener> typeChangeListener, bool disableExtVideoMode) {
    ALOGD_IF(ALLOW_WIDI_PRINT, "%s", __func__);
    Mutex::Autolock _l(mConfigLock);
    mNextConfig.typeChangeListener = typeChangeListener;
    mNextConfig.policy.scaledWidth = 0;
    mNextConfig.policy.scaledHeight = 0;
    mNextConfig.policy.xdpi = 96;
    mNextConfig.policy.ydpi = 96;
    mNextConfig.policy.refresh = 60;
    mNextConfig.extendedModeEnabled = !disableExtVideoMode;
    mNextConfig.forceNotify = true;
    return NO_ERROR;
}

status_t WidiDisplayDevice::stop(bool isConnected) {
    ALOGD_IF(ALLOW_WIDI_PRINT, "%s", __func__);
    Mutex::Autolock _l(mConfigLock);
    mNextConfig.typeChangeListener = NULL;
    mNextConfig.policy.scaledWidth = 0;
    mNextConfig.policy.scaledHeight = 0;
    mNextConfig.policy.xdpi = 96;
    mNextConfig.policy.ydpi = 96;
    mNextConfig.policy.refresh = 60;
    mNextConfig.extendedModeEnabled = false;
    mNextConfig.forceNotify = false;
    return NO_ERROR;
}

status_t WidiDisplayDevice::notifyBufferReturned(int khandle) {
    ALOGD_IF(ALLOW_WIDI_PRINT, "%s khandle=%x", __func__, (uint32_t)khandle);
    Mutex::Autolock _l(mHeldBuffersLock);
    ssize_t index = mHeldBuffers.indexOfKey(khandle);
    if (index == NAME_NOT_FOUND) {
        LOGE("Couldn't find returned khandle %x", khandle);
    }
    else {
        mHeldBuffers.removeItemsAt(index, 1);
    }
    return NO_ERROR;
}

status_t WidiDisplayDevice::setResolution(const FrameProcessingPolicy& policy, sp<IFrameListener> listener) {
    Mutex::Autolock _l(mConfigLock);
    mNextConfig.frameListener = listener;
    mNextConfig.policy = policy;
    return NO_ERROR;
}

bool WidiDisplayDevice::prepare(hwc_display_contents_1_t *list)
{
    ALOGD_IF(ALLOW_WIDI_PRINT, "%s", __func__);

    if (!initCheck()) {
        ALOGE("%s: failed to initialize HWComposer", __func__);
        return false;
    }

    mRenderTimestamp = systemTime();

    {
        Mutex::Autolock _l(mConfigLock);
        mCurrentConfig = mNextConfig;
        mNextConfig.forceNotify = false;
    }

    if (mCurrentConfig.typeChangeListener == NULL)
        return false;

    // by default send the FRAMEBUFFER_TARGET layer (composited image)
    mLayerToSend = list->numHwLayers-1;

    if (mExtendedModeInfo->widiExtHandle != NULL &&
        (!mCurrentConfig.extendedModeEnabled || !mDrm->isVideoPlaying()))
    {
        mExtendedModeInfo->widiExtHandle = NULL;
    }

    // if we have an extended mode handle, find out which layer it is
    // and send that to widi instead of the framebuffer
    if (mExtendedModeInfo->widiExtHandle) {
        for (size_t i = 0; i < list->numHwLayers-1; i++) {
            hwc_layer_1_t& layer = list->hwLayers[i];
            IMG_native_handle_t *grallocHandle =
                (IMG_native_handle_t*)layer.handle;
            if (grallocHandle == mExtendedModeInfo->widiExtHandle)
            {
                mLayerToSend = i;
                break;
            }
        }
    }
    else if ((list->numHwLayers-1) == 1) {
        hwc_layer_1_t& layer = list->hwLayers[0];
        if (layer.transform == 0 && layer.blending == HWC_BLENDING_NONE) {
            mLayerToSend = 0;
        }
    }
    hwc_layer_1_t& streamingLayer = list->hwLayers[mLayerToSend];

    // if we're streaming the target framebuffer, just notify widi stack and return
    if (streamingLayer.compositionType == HWC_FRAMEBUFFER_TARGET) {
        FrameInfo frameInfo;
        memset(&frameInfo, 0, sizeof(frameInfo));
        frameInfo.frameType = HWC_FRAMETYPE_NOTHING;
        if (mCurrentConfig.forceNotify || memcmp(&frameInfo, &mLastInputFrameInfo, sizeof(frameInfo)) != 0)
        {
            // something changed, notify type change listener
            mCurrentConfig.typeChangeListener->frameTypeChanged(frameInfo);
            mCurrentConfig.typeChangeListener->bufferInfoChanged(frameInfo);

            mExtLastTimestamp = 0;
            mExtLastKhandle = 0;

            mMappedBufferCache.clear();
            mLastInputFrameInfo = frameInfo;
            mLastOutputFrameInfo = frameInfo;
        }
        return true;
    }

    // if we're streaming one layer (extended mode or background mode), no need to composite
    for (size_t i = 0; i < list->numHwLayers-1; i++) {
        hwc_layer_1_t& layer = list->hwLayers[i];
        layer.compositionType = HWC_OVERLAY;
        layer.flags |= HWC_HINT_DISABLE_ANIMATION;
    }

    sendToWidi(streamingLayer);

    return true;
}

bool WidiDisplayDevice::commit(hwc_display_contents_1_t *list,
                                    buffer_handle_t *bh,
                                    int &numBuffers)
{
    ALOGD_IF(ALLOW_WIDI_PRINT, "%s", __func__);

    if (!initCheck()) {
        ALOGE("%s: failed to initialize HWComposer", __func__);
        return false;
    }

    return true;
}

void WidiDisplayDevice::sendToWidi(const hwc_layer_1_t& layer)
{
    IMG_native_handle_t* grallocHandle =
        (IMG_native_handle_t*)layer.handle;

    if (grallocHandle == NULL) {
        ALOGE("%s: layer has no handle set", __func__);
        return;
    }

    uint32_t handle = (uint32_t)grallocHandle;
    HWCBufferHandleType handleType = HWC_HANDLE_TYPE_GRALLOC;
    int64_t mediaTimestamp = -1;

    sp<RefBase> heldBuffer;

    FrameInfo inputFrameInfo;
    memset(&inputFrameInfo, 0, sizeof(inputFrameInfo));
    inputFrameInfo.frameType = HWC_FRAMETYPE_FRAME_BUFFER;
    inputFrameInfo.contentWidth = layer.sourceCrop.right - layer.sourceCrop.left;
    inputFrameInfo.contentHeight = layer.sourceCrop.bottom - layer.sourceCrop.top;
    inputFrameInfo.contentFrameRateN = 60;
    inputFrameInfo.contentFrameRateD = 1;

    FrameInfo outputFrameInfo;
    outputFrameInfo = inputFrameInfo;

    if (grallocHandle->iFormat == HAL_PIXEL_FORMAT_INTEL_HWC_NV12 ||
        grallocHandle->iFormat == HAL_PIXEL_FORMAT_INTEL_HWC_NV12_VED ||
        grallocHandle->iFormat == HAL_PIXEL_FORMAT_INTEL_HWC_NV12_TILE)
    {
        sp<CachedBuffer> payloadBuffer;
        intel_gralloc_payload_t *p;
        if ((payloadBuffer = getMappedBuffer(grallocHandle->fd[1])) == NULL) {
            ALOGE("%s: Failed to map display buffer", __func__);
            return;
        }
        if ((p = (intel_gralloc_payload_t*)payloadBuffer->displayBuffer->getCpuAddr()) == NULL) {
            ALOGE("%s: Got null payload from display buffer", __func__);
            return;
        }
        heldBuffer = new HeldDecoderBuffer(payloadBuffer);

        mediaTimestamp = p->timestamp;

        // default fps to 0. widi stack will decide what correct fps should be
        int displayW = 0, displayH = 0, fps = 0, isInterlace = 0;
        mDrm->getVideoInfo(&displayW, &displayH, &fps, &isInterlace);
        if (fps > 0) {
            inputFrameInfo.contentFrameRateN = fps;
            inputFrameInfo.contentFrameRateD = 1;
        }

        if (p->metadata_transform & HAL_TRANSFORM_ROT_90) {
            inputFrameInfo.contentWidth = layer.sourceCrop.bottom - layer.sourceCrop.top;
            inputFrameInfo.contentHeight = layer.sourceCrop.right - layer.sourceCrop.left;
        }
        inputFrameInfo.cropLeft = 0;
        inputFrameInfo.cropTop = 0;
        // skip pading bytes in rotate buffer
        switch(p->metadata_transform) {
            case HAL_TRANSFORM_ROT_90: {
                int contentWidth = inputFrameInfo.contentWidth;
                inputFrameInfo.contentWidth = (contentWidth + 0xf) & ~0xf;
                inputFrameInfo.cropLeft = inputFrameInfo.contentWidth - contentWidth;
            } break;
            case HAL_TRANSFORM_ROT_180: {
                int contentWidth = inputFrameInfo.contentWidth;
                int contentHeight = inputFrameInfo.contentHeight;
                inputFrameInfo.contentWidth = (contentWidth + 0xf) & ~0xf;
                inputFrameInfo.contentHeight = (contentHeight + 0xf) & ~0xf;
                inputFrameInfo.cropLeft = inputFrameInfo.contentWidth - contentWidth;
                inputFrameInfo.cropTop = inputFrameInfo.contentHeight - contentHeight;
            } break;
            case HAL_TRANSFORM_ROT_270: {
                int contentHeight = inputFrameInfo.contentHeight;
                inputFrameInfo.contentHeight = (contentHeight + 0xf) & ~0xf;
                inputFrameInfo.cropTop = inputFrameInfo.contentHeight - contentHeight;
            } break;
            default:
                break;
        }

        outputFrameInfo = inputFrameInfo;
        outputFrameInfo.bufferFormat = p->format;

        handleType = HWC_HANDLE_TYPE_KBUF;
        if (p->rotated_buffer_handle != 0 && p->metadata_transform != 0)
        {
            handle = p->rotated_buffer_handle;
            outputFrameInfo.bufferWidth = p->rotated_width;
            outputFrameInfo.bufferHeight = p->rotated_height;
            outputFrameInfo.lumaUStride = p->rotate_luma_stride;
            outputFrameInfo.chromaUStride = p->rotate_chroma_u_stride;
            outputFrameInfo.chromaVStride = p->rotate_chroma_v_stride;
        }
        else if (p->khandle != 0)
        {
            handle = p->khandle;
            outputFrameInfo.bufferWidth = p->width;
            outputFrameInfo.bufferHeight = p->height;
            outputFrameInfo.lumaUStride = p->luma_stride;
            outputFrameInfo.chromaUStride = p->chroma_u_stride;
            outputFrameInfo.chromaVStride = p->chroma_v_stride;
        }
        else {
            ALOGE("Couldn't get any khandle");
            return;
        }

        if (outputFrameInfo.bufferFormat == 0 ||
            outputFrameInfo.bufferWidth < outputFrameInfo.contentWidth ||
            outputFrameInfo.bufferHeight < outputFrameInfo.contentHeight ||
            outputFrameInfo.contentWidth <= 0 || outputFrameInfo.contentHeight <= 0 ||
            outputFrameInfo.lumaUStride <= 0 ||
            outputFrameInfo.chromaUStride <= 0 || outputFrameInfo.chromaVStride <= 0)
        {
            ALOGI("Payload cleared or inconsistent info, not sending frame");
            return;
        }
    }
    else {
        sp<GraphicBuffer> destBuffer;
        {
            Mutex::Autolock _l(mCscLock);
            if (mCscWidth != mCurrentConfig.policy.scaledWidth || mCscHeight != mCurrentConfig.policy.scaledHeight) {
                ALOGI("CSC buffers changing from %dx%d to %dx%d",
                      mCscWidth, mCscHeight, mCurrentConfig.policy.scaledWidth, mCurrentConfig.policy.scaledHeight);
                mAvailableCscBuffers.clear();
                mCscWidth = mCurrentConfig.policy.scaledWidth;
                mCscHeight = mCurrentConfig.policy.scaledHeight;
                mCscBuffersToCreate = 4;
            }

            if (mAvailableCscBuffers.empty()) {
                if (mCscBuffersToCreate <= 0) {
                    ALOGW("%s: Out of CSC buffers, dropping frame", __func__);
                    return;
                }
                mCscBuffersToCreate--;
                sp<GraphicBuffer> graphicBuffer =
                    new GraphicBuffer(mCurrentConfig.policy.scaledWidth, mCurrentConfig.policy.scaledHeight,
                                      OMX_INTEL_COLOR_FormatYUV420PackedSemiPlanar,
                                      GRALLOC_USAGE_HW_VIDEO_ENCODER | GRALLOC_USAGE_HW_RENDER);
                mAvailableCscBuffers.push_back(graphicBuffer);

            }
            destBuffer = *mAvailableCscBuffers.begin();
            mAvailableCscBuffers.erase(mAvailableCscBuffers.begin());
        }
        heldBuffer = new HeldCscBuffer(this, destBuffer);
        if (mGrallocModule->Blit2(mGrallocModule,
                (buffer_handle_t)handle, destBuffer->handle,
                mCurrentConfig.policy.scaledWidth, mCurrentConfig.policy.scaledHeight, 0, 0))
        {
            ALOGE("%s: Blit2 failed", __func__);
            return;
        }
        grallocHandle = (IMG_native_handle_t*)destBuffer->handle;
        handle = (uint32_t)grallocHandle;

        outputFrameInfo.contentWidth = mCurrentConfig.policy.scaledWidth;
        outputFrameInfo.contentHeight = mCurrentConfig.policy.scaledHeight;
        outputFrameInfo.bufferWidth = grallocHandle->iWidth;
        outputFrameInfo.bufferHeight = grallocHandle->iHeight;
        outputFrameInfo.lumaUStride = grallocHandle->iWidth;
        outputFrameInfo.chromaUStride = grallocHandle->iWidth;
        outputFrameInfo.chromaVStride = grallocHandle->iWidth;
    }

    if (mCurrentConfig.forceNotify ||
        memcmp(&inputFrameInfo, &mLastInputFrameInfo, sizeof(inputFrameInfo)) != 0)
    {
        // something changed, notify type change listener
        mCurrentConfig.typeChangeListener->frameTypeChanged(inputFrameInfo);
        mLastInputFrameInfo = inputFrameInfo;
    }

    if (mCurrentConfig.policy.scaledWidth == 0 || mCurrentConfig.policy.scaledHeight == 0)
        return;

    if (mCurrentConfig.forceNotify ||
        memcmp(&outputFrameInfo, &mLastOutputFrameInfo, sizeof(outputFrameInfo)) != 0)
    {
        mCurrentConfig.typeChangeListener->bufferInfoChanged(outputFrameInfo);
        mLastOutputFrameInfo = outputFrameInfo;

        if (handleType == HWC_HANDLE_TYPE_GRALLOC)
            mMappedBufferCache.clear();
    }

    if (handleType == HWC_HANDLE_TYPE_KBUF &&
        handle == mExtLastKhandle && mediaTimestamp == mExtLastTimestamp)
    {
        return;
    }

    {
        Mutex::Autolock _l(mHeldBuffersLock);
        mHeldBuffers.add(handle, heldBuffer);
    }
    status_t result = mCurrentConfig.frameListener->onFrameReady((int32_t)handle, handleType, mRenderTimestamp, mediaTimestamp);
    if (result != OK) {
        Mutex::Autolock _l(mHeldBuffersLock);
        mHeldBuffers.removeItem(handle);
    }
    if (handleType == HWC_HANDLE_TYPE_KBUF) {
        mExtLastKhandle = handle;
        mExtLastTimestamp = mediaTimestamp;
    }
}

bool WidiDisplayDevice::dump(char *buff,
                           int buff_len, int *cur_len)
{
    bool ret = true;

    mDumpBuf = buff;
    mDumpBuflen = buff_len;
    mDumpLen = (int)(*cur_len);

    *cur_len = mDumpLen;
    return ret;
}

void WidiDisplayDevice::onHotplugEvent(bool hpd)
{
    // overriding superclass and doing nothing
}

// SurfaceFlinger does not call this on virtual displays
bool WidiDisplayDevice::getDisplayConfig(uint32_t* configs,
                                        size_t* numConfigs)
{
    ALOGE("%s not expected to be called", __func__);
    return false;
}

// SurfaceFlinger does not call this on virtual displays
bool WidiDisplayDevice::getDisplayAttributes(uint32_t config,
            const uint32_t* attributes, int32_t* values)
{
    ALOGE("%s not expected to be called", __func__);
    return false;
}
