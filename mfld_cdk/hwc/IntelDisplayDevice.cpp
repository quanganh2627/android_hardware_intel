/*
 * Copyright © 2012 Intel Corporation
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
 *    Jackie Li <yaodong.li@intel.com>
 *
 */
#include <cutils/log.h>
#include <cutils/atomic.h>

#include <IntelHWComposer.h>
#include <IntelDisplayDevice.h>
#include <IntelOverlayUtil.h>
#include <IntelHWComposerCfg.h>

IntelDisplayDevice::IntelDisplayDevice(IntelDisplayPlaneManager *pm,
                                IntelHWComposerDrm *drm,
                                IntelBufferManager *bm,
                                IntelBufferManager *gm,
                                uint32_t index)
       :  IntelHWComposerDump(), mWsbm(NULL),
          mPlaneManager(pm), mDrm(drm), mBufferManager(bm),
          mGrallocBufferManager(gm), mLayerList(0),
          mRotationBufProvider(NULL),
          mDisplayIndex(index), mForceSwapBuffer(false),
          mHotplugEvent(false), mIsConnected(false),
          mInitialized(false), mIsScreenshotActive(false),
          mIsBlank(false), mVideoSeekingActive(false)
{
   ALOGD_IF(ALLOW_HWC_PRINT, "%s\n", __func__);
   initializeRotationBufProvider();
   memset(mFBBuffers, 0, sizeof(mFBBuffers));
   mNextBuffer = 0;
}

IntelDisplayDevice::IntelDisplayDevice::~IntelDisplayDevice()
{
    ALOGD_IF(ALLOW_HWC_PRINT, "%s\n", __func__);
    destroyRotationBufProvider();
}


int IntelDisplayDevice::getMetaDataTransform(hwc_layer_1_t *layer,
        uint32_t &transform) {
    if (!layer || !mGrallocBufferManager)
        return -1;

    IMG_native_handle_t *grallocHandle =
        (IMG_native_handle_t*)layer->handle;
    if(!grallocHandle) {
        ALOGE("%s: gralloc handle invalid.\n", __func__);
        return -1;
    }

    if (grallocHandle->iFormat != HAL_PIXEL_FORMAT_INTEL_HWC_NV12_VED &&
        grallocHandle->iFormat != HAL_PIXEL_FORMAT_INTEL_HWC_NV12_TILE) {
        ALOGV("%s: SW decoder, ignore this checking.", __func__);
        return 0;
    }

    // get payload buffer
    IntelPayloadBuffer buffer(mGrallocBufferManager, grallocHandle->fd[1]);

    intel_gralloc_payload_t *payload =
            (intel_gralloc_payload_t*)buffer.getCpuAddr();
    if (!payload) {
        ALOGE("%s: invalid address\n", __func__);
        return -1;
    }

    transform = payload->metadata_transform;

    return 0;
}

bool IntelDisplayDevice::isVideoPutInWindow(int output, hwc_layer_1_t *layer) {
    bool inWindow = false;

    if (mDrm == NULL) {
        ALOGE("mDrm is NULL!");
        return false;
    }

    drmModeFBPtr fbInfo = mDrm->getOutputFBInfo(output);
    if (fbInfo == NULL) {
        ALOGE("Get FB info failed!");
        return false;
    }

    int fbW = fbInfo->width;
    int fbH = fbInfo->height;

    int srcWidth = (int)(layer->sourceCropf.right - layer->sourceCropf.left);
    int srcHeight = (int)(layer->sourceCropf.bottom - layer->sourceCropf.top);
    uint32_t metadata_transform = 0;
    if (getMetaDataTransform(layer, metadata_transform) == -1) {
        ALOGE("Get meta data transform failed!");
        return false;
    }

    if (metadata_transform == HAL_TRANSFORM_ROT_90 ||
            metadata_transform == HAL_TRANSFORM_ROT_270) {
        int temp;
        temp = srcWidth; srcWidth = srcHeight; srcHeight = temp;
    }

    int dstWidth = layer->displayFrame.right - layer->displayFrame.left;
    int dstHeight = layer->displayFrame.bottom - layer->displayFrame.top;

    ALOGD_IF(ALLOW_HWC_PRINT,
            "output:%d fbW:%d fbH:%d dst_w:%d dst_h:%d src_w:%d src_h:%d",
            output, fbW, fbH, dstWidth, dstHeight, srcWidth, srcHeight);
    /*
     * Check if the app put the video to a window
     * 1) Both dst width and height are smaller than FB w/h
     * 2) For device(e.g. phone), fbW < fbH:
     check if it is back to portait mode.
     * 3) For device(e.g. tablet, hdtv), fbW > fbH:
     check if it is back to portait mode.
     */
    if (dstWidth < fbW - 1 && dstHeight < fbH - 1) {
        inWindow = true;
    } else if (fbW < fbH) {
        if ((dstWidth > dstHeight && srcWidth >= srcHeight) ||
            (dstWidth < dstHeight && srcWidth <= srcHeight))
            inWindow = true;
    } else if (fbW > fbH) {
        if ((dstWidth > dstHeight && srcWidth <= srcHeight) ||
            (dstWidth < dstHeight && srcWidth >= srcHeight))
            inWindow = true;
    }

    return inWindow;
}

bool IntelDisplayDevice::rgbOverlayPrepare(int index,
                                            hwc_layer_1_t *layer, int flags)
{
    if (!layer) {
        ALOGE("%s: Invalid layer\n", __func__);
        return false;
    }

    // allocate overlay plane
    IntelDisplayPlane *plane = mPlaneManager->getRGBOverlayPlane();
    if (!plane) {
        ALOGE("%s: failed to create RGB overlay plane\n", __func__);
        return false;
    }

    int dstLeft = layer->displayFrame.left;
    int dstTop = layer->displayFrame.top;
    int dstRight = layer->displayFrame.right;
    int dstBottom = layer->displayFrame.bottom;

    // setup plane parameters
    plane->setPosition(dstLeft, dstTop, dstRight, dstBottom);

    // always assign RGB overlays to MIPI
    plane->setPipeByMode(OVERLAY_MIPI0);

    // attach plane to hwc layer
    mLayerList->attachPlane(index, plane, flags);

    return true;
}

bool IntelDisplayDevice::spritePrepare(int index, hwc_layer_1_t *layer, int flags)
{
    if (!layer) {
        ALOGE("%s: Invalid layer\n", __func__);
        return false;
    }

    // allocate sprite plane
    IntelDisplayPlane *plane = mPlaneManager->getSpritePlane();
    if (!plane) {
        ALOGE("%s: failed to create sprite plane\n", __func__);
        return false;
    }

    int dstLeft = layer->displayFrame.left;
    int dstTop = layer->displayFrame.top;
    int dstRight = layer->displayFrame.right;
    int dstBottom = layer->displayFrame.bottom;

    // setup plane parameters
    plane->setPosition(dstLeft, dstTop, dstRight, dstBottom);

    // attach plane to hwc layer
    mLayerList->attachPlane(index, plane, flags);

    return true;
}

bool IntelDisplayDevice::primaryPrepare(int index, hwc_layer_1_t *layer, int flags)
{
    if (!layer) {
        ALOGE("%s: Invalid layer\n", __func__);
        return false;
    }
    int dstLeft = layer->displayFrame.left;
    int dstTop = layer->displayFrame.top;
    int dstRight = layer->displayFrame.right;
    int dstBottom = layer->displayFrame.bottom;

    // allocate sprite plane
    IntelDisplayPlane *plane = mPlaneManager->getPrimaryPlane(mDisplayIndex);
    if (!plane) {
        ALOGE("%s: failed to create sprite plane\n", __func__);
        return false;
    }

    // TODO: check external display status, and attach plane

    // setup plane parameters
    plane->setPosition(dstLeft, dstTop, dstRight, dstBottom);

    // attach plane to hwc layer
    mLayerList->attachPlane(index, plane, flags);

    return true;
}

// TODO: re-implement it for each device
bool IntelDisplayDevice::isOverlayLayer(hwc_display_contents_1_t *list,
                                     int index,
                                     hwc_layer_1_t *layer,
                                     int& flags)
{
    return false;
}

// TODO: re-implement it for each device
bool IntelDisplayDevice::isRGBOverlayLayer(hwc_display_contents_1_t *list,
                                           int index,
                                           hwc_layer_1_t *layer,
                                           int& flags)
{
    return false;
}

// TODO: re-implement it for each device
bool IntelDisplayDevice::isSpriteLayer(hwc_display_contents_1_t *list,
                                    int index,
                                    hwc_layer_1_t *layer,
                                    int& flags)
{
   return false;
}

// TODO: re-implement it for each device
bool IntelDisplayDevice::isPrimaryLayer(hwc_display_contents_1_t *list,
                                     int index,
                                     hwc_layer_1_t *layer,
                                     int& flags)
{
   return false;
}

bool IntelDisplayDevice::flipFramebufferContexts(void *contexts,
                                          hwc_layer_1_t *layer)
{
    intel_sprite_context_t *context;
    mdfld_plane_contexts_t *planeContexts;
    int zOrderConfig;
    bool forceBottom = false;

    ALOGD_IF(ALLOW_HWC_PRINT, "flipFrameBufferContexts");

    if (!contexts) {
        ALOGE("%s: Invalid plane contexts\n", __func__);
        return false;
    }

    IMG_native_handle_t *grallocHandle =
                 (IMG_native_handle_t*)layer->handle;

    if (!grallocHandle)
        return false;

    IntelDisplayBuffer *buffer = NULL;

    for (int i = 0; i < NUM_FB_BUFFERS; i++) {
        if (mFBBuffers[i].ui64Stamp == grallocHandle->ui64Stamp) {
            ALOGD_IF(ALLOW_HWC_PRINT,
                     "%s: buf stamp %lld...\n", __func__,grallocHandle->ui64Stamp);
            buffer = mFBBuffers[i].buffer;
            break;
        }
    }

    if (!buffer) {
        // release the buffer in the next slot
        if (mFBBuffers[mNextBuffer].ui64Stamp ||
                    mFBBuffers[mNextBuffer].buffer) {
            mGrallocBufferManager->unmap(mFBBuffers[mNextBuffer].buffer);
            mFBBuffers[mNextBuffer].ui64Stamp = 0;
            mFBBuffers[mNextBuffer].buffer = 0;
        }

        buffer = mGrallocBufferManager->map(grallocHandle->fd[0]);

        if (!buffer) {
            ALOGE("%s: failed to map HDMI handle !\n", __func__);
            return false;
        }

        mFBBuffers[mNextBuffer].ui64Stamp = grallocHandle->ui64Stamp;
        mFBBuffers[mNextBuffer].buffer = buffer;
        // move mNextBuffer pointer
        mNextBuffer = (mNextBuffer + 1) % NUM_FB_BUFFERS;
    }

    planeContexts = (mdfld_plane_contexts_t*)contexts;

    uint32_t gttOffsetInPage = buffer->getGttOffsetInPage();

    // update layer info;
    uint32_t fbWidth = (uint32_t)(layer->sourceCropf.right);
    uint32_t fbHeight = (uint32_t)(layer->sourceCropf.bottom);

    context = &planeContexts->sprite_contexts[mDisplayIndex];
    context->update_mask = SPRITE_UPDATE_ALL;
    context->index = mDisplayIndex;
    context->pipe = mDisplayIndex;
    context->linoff = 0;

    context->stride = align_to((4 * fbWidth), 128);
    context->pos = 0;
    context->size = ((fbHeight - 1) & 0xfff) << 16 | ((fbWidth - 1) & 0xfff);
    context->surf = gttOffsetInPage << 12;

    // config z order; switch z order may cause flicker
    zOrderConfig = mPlaneManager->getZOrderConfig(mDisplayIndex);
    if ((zOrderConfig == IntelDisplayPlaneManager::ZORDER_OcOaP) ||
        (zOrderConfig == IntelDisplayPlaneManager::ZORDER_OaOcP))
        forceBottom = true;

    // if RGB layer is bottom, force fb to be bottom and bgrx format
    // to solve blank screen of some 3D applications with premulti alpha.
    if (forceBottom) {
        context->cntr = INTEL_SPRITE_PIXEL_FORMAT_BGRX8888;
        context->cntr |= INTEL_SPRITE_FORCE_BOTTOM;
    } else if (mLayerList->getAttachedPlanesCount() == 0) {
        context->cntr = INTEL_SPRITE_PIXEL_FORMAT_BGRX8888;
    }
    else {
        context->cntr = INTEL_SPRITE_PIXEL_FORMAT_BGRA8888;
    }
    context->cntr |= 0x80000000;

    // update active sprite
    planeContexts->active_sprites |= (1 << mDisplayIndex);

    return true;
}

void IntelDisplayDevice::dumpLayerList(hwc_display_contents_1_t *list)
{
    if (!list)
        return;

    ALOGD("\n");
    ALOGD("DUMP LAYER LIST START");
    ALOGD("num of layers: %d", list->numHwLayers);
    for (size_t i = 0; i < list->numHwLayers; i++) {
        int srcLeft = (int)(list->hwLayers[i].sourceCropf.left);
        int srcTop = (int)(list->hwLayers[i].sourceCropf.top);
        int srcRight = (int)(list->hwLayers[i].sourceCropf.right);
        int srcBottom = (int)(list->hwLayers[i].sourceCropf.bottom);

        int dstLeft = list->hwLayers[i].displayFrame.left;
        int dstTop = list->hwLayers[i].displayFrame.top;
        int dstRight = list->hwLayers[i].displayFrame.right;
        int dstBottom = list->hwLayers[i].displayFrame.bottom;

        ALOGD("Layer type: %s",
        (mLayerList->getLayerType(i) != IntelHWComposerLayer::LAYER_TYPE_YUV) ?
        "RGB" : "YUV");
        ALOGD("Layer blending: 0x%x", list->hwLayers[i].blending);
        ALOGD("Layer source: (%d, %d) - (%dx%d)", srcLeft, srcTop,
             srcRight - srcLeft, srcBottom - srcTop);
        ALOGD("Layer positon: (%d, %d) - (%dx%d)", dstLeft, dstTop,
             dstRight - dstLeft, dstBottom - dstTop);
        ALOGD("Layer transform: 0x%x\n", list->hwLayers[i].transform);
        ALOGD("Layer handle: 0x%x\n", (uint32_t)list->hwLayers[i].handle);
        ALOGD("Layer flags: 0x%x\n", list->hwLayers[i].flags);
        ALOGD("Layer compositionType: 0x%x\n", list->hwLayers[i].compositionType);
        ALOGD("\n");
    }
    ALOGD("DUMP LAYER LIST END");
    ALOGD("\n");
}

bool IntelDisplayDevice::isScreenshotActive(hwc_display_contents_1_t *list)
{
    mIsScreenshotActive = false;

    if (!list || !list->numHwLayers)
        return false;

    // no layer in list except framebuffer target
    if (mLayerList->getLayersCount() <= 0)
        return false;

    // Bypass ext video mode
    if (mDrm->getDisplayMode() == OVERLAY_EXTEND)
        return false;

    // bypass protected video
    for (size_t i = 0; i < (size_t)mLayerList->getLayersCount(); i++) {
        if (mLayerList->isProtectedLayer(i))
            return false;
    }

    // check topLayer before getting its size
    hwc_layer_1_t *topLayer = &list->hwLayers[mLayerList->getLayersCount()-1];
    if (!topLayer || !(topLayer->flags & HWC_SKIP_LAYER))
        return false;

    int x = topLayer->displayFrame.left;
    int y = topLayer->displayFrame.top;
    int w = topLayer->displayFrame.right - topLayer->displayFrame.left;
    int h = topLayer->displayFrame.bottom - topLayer->displayFrame.top;

    drmModeFBPtr fbInfo =
        IntelHWComposerDrm::getInstance().getOutputFBInfo(mDisplayIndex);

    if (!fbInfo) {
        ALOGE("Invalid fbINfo\n");
        return false;
    }

    if (x == 0 && y == 0 &&
        w == int(fbInfo->width) && h == int(fbInfo->height)) {
        mIsScreenshotActive = true;
        return true;
    }

    return false;
}

// Check the usage of a buffer
// only following usages were accepted:
// TODO: list valid usages
bool IntelDisplayDevice::isHWCUsage(int usage)
{
#if 0
    // For SW access buffer, should handle with
    // FB in order to avoid tearing
    if ((usage & GRALLOC_USAGE_SW_WRITE_OFTEN) &&
         (usage & GRALLOC_USAGE_SW_READ_OFTEN))
        return false;
#endif

    if (!(usage & GRALLOC_USAGE_HW_COMPOSER))
        return false;

    return true;
}

// Check the data format of a buffer
// only following formats were accepted:
// TODO: list valid formats
bool IntelDisplayDevice::isHWCFormat(int format)
{
    return true;
}

// Check the transform of a layer
// we can only support 180 degree rotation
// TODO: list all supported transforms
// FIXME: for video playback we need to ignore this,
// since video decoding engine will take care of rotation.
bool IntelDisplayDevice::isHWCTransform(uint32_t transform)
{
    return false;
}

// Check the blending of a layer
// Currently, no blending support
bool IntelDisplayDevice::isHWCBlending(uint32_t blending)
{
    return false;
}

// Check whether a layer can be handle by our HWC
// if HWC_SKIP_LAYER was set, skip this layer
// TODO: add more
bool IntelDisplayDevice::isHWCLayer(hwc_layer_1_t *layer)
{
    if (!layer)
        return false;

    // if (layer->flags & HWC_SKIP_LAYER)
    //    return false;

    // bypass HWC_FRAMEBUFFER_TARGET
    if (layer->compositionType == HWC_FRAMEBUFFER_TARGET)
        return false;

    // check transform
    // if (!isHWCTransform(layer->transform))
    //    return false;

    // check blending
    // if (!isHWCBlending(layer->blending))
    //    return false;

    IMG_native_handle_t *grallocHandle =
        (IMG_native_handle_t*)layer->handle;

    if (!grallocHandle)
        return false;

    // check buffer usage
    if (!isHWCUsage(grallocHandle->usage))
        return false;

    // check format
    // if (!isHWCFormat(grallocHandle->iFormat))
    //    return false;

    return true;
}

bool IntelDisplayDevice::isLayerSandwiched(int index,
                                           hwc_display_contents_1_t *list)
{
    return false;
}

// Check whehter two layers are intersect
bool IntelDisplayDevice::areLayersIntersecting(hwc_layer_1_t *top,
                                            hwc_layer_1_t* bottom)
{
    if (!top || !bottom)
        return false;

    hwc_rect_t *topRect = &top->displayFrame;
    hwc_rect_t *bottomRect = &bottom->displayFrame;

    if (bottomRect->right <= topRect->left ||
        bottomRect->left >= topRect->right ||
        bottomRect->top >= topRect->bottom ||
        bottomRect->bottom <= topRect->top)
        return false;

    return true;
}

bool IntelDisplayDevice::release()
{
    ALOGD("display device release");

    if (!initCheck() || !mLayerList)
        return false;

    // disable all attached planes
    for (int i=0 ; i<mLayerList->getLayersCount() ; i++) {
        IntelDisplayPlane *plane = mLayerList->getPlane(i);

        if (plane) {
            // disable all attached planes
            plane->disable();
            plane->waitForFlipCompletion();

            // release all data buffers
            plane->invalidateDataBuffer();
        }
    }

    return true;
}

bool IntelDisplayDevice::dump(char *buff,
                           int buff_len, int *cur_len)
{
    return true;
}

void IntelDisplayDevice::onHotplugEvent(bool hpd)
{
    // go through layer list and call plane's onModeChange()
    for (int i = 0 ; i < mLayerList->getLayersCount(); i++) {
        IntelDisplayPlane *plane = mLayerList->getPlane(i);
        if (plane)
            plane->onDrmModeChange();
    }

    mHotplugEvent = hpd;
    if (hpd && mDrm->isOverlayOff()) {
        if (mPlaneManager->hasReclaimedOverlays())
            mPlaneManager->disableReclaimedPlanes(IntelDisplayPlane::DISPLAY_PLANE_OVERLAY);
    }
}

bool IntelDisplayDevice::blank(int blank)
{
    bool ret=false;

    if (blank == 1)
        mIsBlank = true;
    else
        mIsBlank = false;

    if (mDrm)
        ret = mDrm->setDisplayDpms(mDisplayIndex, blank);

    return ret;
}

bool IntelDisplayDevice::getDisplayConfig(uint32_t* configs,
                                        size_t* numConfigs)
{
    return false;
}

bool IntelDisplayDevice::getDisplayAttributes(uint32_t config,
            const uint32_t *attributes, int32_t* values)
{
    return false;
}


bool IntelDisplayDevice::updateLayersData(hwc_display_contents_1_t *list)
{
    IntelDisplayPlane *plane = 0;
    bool ret = true;
    bool handled = true;

    mYUVOverlay = -1;

    if (!list)
	return false;

    for (size_t i=0 ; i<(size_t)mLayerList->getLayersCount(); i++) {
        hwc_layer_1_t *layer = &list->hwLayers[i];
        // layer safety check
        if (!isHWCLayer(layer) || !layer)
            continue;

        IMG_native_handle_t *grallocHandle =
            (IMG_native_handle_t*)layer->handle;

        // check plane
        plane = mLayerList->getPlane(i);
        if (!plane)
            continue;

        // get layer parameter
        int bobDeinterlace;
        int srcX = (int)(layer->sourceCropf.left);
        int srcY = (int)(layer->sourceCropf.top);
        int srcWidth = (int)(layer->sourceCropf.right - layer->sourceCropf.left);
        int srcHeight = (int)(layer->sourceCropf.bottom - layer->sourceCropf.top);
        int planeType = plane->getPlaneType();

        if(srcHeight == 1 || srcWidth == 1) {
            mLayerList->detachPlane(i, plane);
            layer->compositionType = HWC_FRAMEBUFFER;
            handled = false;
            continue;
        }
        if (planeType == IntelDisplayPlane::DISPLAY_PLANE_OVERLAY) {
            if (mDrm->isOverlayOff()) {
                plane->disable();
                handled = false;
                layer->compositionType = HWC_FRAMEBUFFER;
                continue;
            }
        }

        // get & setup data buffer and buffer format
        IntelDisplayBuffer *buffer = plane->getDataBuffer();
        IntelDisplayDataBuffer *dataBuffer =
            reinterpret_cast<IntelDisplayDataBuffer*>(buffer);
        if (!dataBuffer) {
            ALOGE("%s: invalid data buffer\n", __func__);
            continue;
        }

        int bufferWidth = grallocHandle->iWidth;
        int bufferHeight = grallocHandle->iHeight;
        uint32_t bufferHandle = grallocHandle->fd[0];
        int format = grallocHandle->iFormat;
        uint32_t transform = layer->transform;

        if (planeType == IntelDisplayPlane::DISPLAY_PLANE_OVERLAY) {
            int flags = mLayerList->getFlags(i);
            if (flags & IntelDisplayPlane::DELAY_DISABLE) {
                ALOGD_IF(ALLOW_HWC_PRINT,
                       "updateLayerData: disable plane (DELAY)!");
                flags &= ~IntelDisplayPlane::DELAY_DISABLE;
                mLayerList->setFlags(i, flags);
                plane->disable();
            }
#if 0
            //FIXME: is a workaround
            // Bypass overlay layer, if
            // device is rotated
            // and not presentation mode
            // and video is not only attached to HDMI
            if (list && (mDrm->getDisplayMode() == OVERLAY_EXTEND &&
                     !mDrm->isPresentationMode() && !mDrm->onlyHdmiHasVideo())) {
                ALOGI_IF(ALLOW_HWC_PRINT, "Bypass overlay layer");
                mLayerList->detachPlane(i, plane);
                layer->compositionType = HWC_OVERLAY;
                handled = false;
                continue;
            }
#endif
            // check if can switch to overlay
            bool useOverlay = useOverlayRotation(layer, i,
                                                 bufferHandle,
                                                 bufferWidth,
                                                 bufferHeight,
                                                 srcX,
                                                 srcY,
                                                 srcWidth,
                                                 srcHeight,
                                                 transform);

            if (!useOverlay) {
                ALOGD_IF(ALLOW_HWC_PRINT,
                       "updateLayerData: useOverlayRotation failed!");
                if (!mLayerList->getForceOverlay(i)) {
                    ALOGD_IF(ALLOW_HWC_PRINT,
                           "updateLayerData: fallback to ST to do rendering!");
                    // fallback to ST to render this frame
                    layer->compositionType = HWC_FRAMEBUFFER;
                    mForceSwapBuffer = true;
                    handled = false;
                }
                // disable overlay when rotated buffer is not ready
                flags |= IntelDisplayPlane::DELAY_DISABLE;
                mLayerList->setFlags(i, flags);
                continue;
            }

            bobDeinterlace = isBobDeinterlace(layer);
            if (bobDeinterlace) {
                flags |= IntelDisplayPlane::BOB_DEINTERLACE;
            } else {
                flags &= ~IntelDisplayPlane::BOB_DEINTERLACE;
            }
            mLayerList->setFlags(i, flags);

            // switch to overlay
            layer->compositionType = HWC_OVERLAY;

            // transformed buffer not from gralloc, can't use it's stride directly
            uint32_t grallocStride = !transform ? grallocHandle->iStride : align_to(bufferWidth, 32);
            int format = grallocHandle->iFormat;

            dataBuffer->setFormat(format);
            dataBuffer->setStride(grallocStride);
            dataBuffer->setWidth(bufferWidth);
            dataBuffer->setHeight(bufferHeight);
            dataBuffer->setCrop(srcX, srcY, srcWidth, srcHeight);
            dataBuffer->setDeinterlaceType(bobDeinterlace);
            // set the data buffer back to plane
            ret = ((IntelOverlayPlane*)plane)->setDataBuffer(bufferHandle,
                                                             transform,
                                                             grallocHandle);
            if (!ret) {
                ALOGE("%s: failed to update overlay data buffer\n", __func__);
                mLayerList->detachPlane(i, plane);
                layer->compositionType = HWC_FRAMEBUFFER;
                handled = false;
            }
            if (layer->compositionType == HWC_OVERLAY &&
                format == HAL_PIXEL_FORMAT_INTEL_HWC_NV12)
                mYUVOverlay = i;
        } else if (planeType == IntelDisplayPlane::DISPLAY_PLANE_RGB_OVERLAY) {
            IntelRGBOverlayPlane *rgbOverlayPlane =
                reinterpret_cast<IntelRGBOverlayPlane*>(plane);
            uint32_t yuvBufferHandle =
                rgbOverlayPlane->convert((uint32_t)grallocHandle,
                                          srcWidth, srcHeight,
                                          srcX, srcY);
            if (!yuvBufferHandle) {
                ALOGE("updateLayersData: failed to convert\n");
                continue;
            }

            grallocHandle = (IMG_native_handle_t*)yuvBufferHandle;
            bufferWidth = grallocHandle->iWidth;
            bufferHeight = grallocHandle->iHeight;
            bufferHandle = grallocHandle->fd[0];
            format = grallocHandle->iFormat;

            uint32_t grallocStride = grallocHandle->iStride;

            dataBuffer->setFormat(format);
            dataBuffer->setStride(grallocStride);
            dataBuffer->setWidth(bufferWidth);
            dataBuffer->setHeight(bufferHeight);
            dataBuffer->setCrop(srcX, srcY, srcWidth, srcHeight);
            dataBuffer->setDeinterlaceType(0);

            // set the data buffer back to plane
            ret = rgbOverlayPlane->setDataBuffer(bufferHandle,
                                                 0, grallocHandle);
            if (!ret) {
                ALOGE("%s: failed to update overlay data buffer\n", __func__);
                mLayerList->detachPlane(i, plane);
                layer->compositionType = HWC_FRAMEBUFFER;
                handled = false;
            }
        } else if (planeType == IntelDisplayPlane::DISPLAY_PLANE_SPRITE ||
                   planeType == IntelDisplayPlane::DISPLAY_PLANE_PRIMARY) {

            // adjust the buffer format if no blending is needed
            // some test cases would fail due to a weird format!
            if (layer->blending == HWC_BLENDING_NONE) {
                switch (format) {
                case HAL_PIXEL_FORMAT_BGRA_8888:
                    format = HAL_PIXEL_FORMAT_BGRX_8888;
                    break;
                case HAL_PIXEL_FORMAT_RGBA_8888:
                    format = HAL_PIXEL_FORMAT_RGBX_8888;
                    break;
                }
            }

            // set data buffer format
            dataBuffer->setFormat(format);
            dataBuffer->setWidth(bufferWidth);
            dataBuffer->setHeight(bufferHeight);
            dataBuffer->setCrop(srcX, srcY, srcWidth, srcHeight);
            // set the data buffer back to plane
            ret = plane->setDataBuffer(bufferHandle, transform, grallocHandle);
            if (!ret) {
                ALOGE("%s: failed to update sprite data buffer\n", __func__);
                mLayerList->detachPlane(i, plane);
                layer->compositionType = HWC_FRAMEBUFFER;
                handled = false;
            }
        } else {
            ALOGW("%s: invalid plane type %d\n", __func__, planeType);
            continue;
        }

        // clear layer's visible region if need clear up flag was set
        // and sprite plane was used as primary plane (point to FB)
        if (mLayerList->getNeedClearup(i) &&
            mPlaneManager->primaryAvailable(0)) {
            ALOGD_IF(ALLOW_HWC_PRINT,
                  "updateLayersData: clear visible region of layer %d", i);
            list->hwLayers[i].hints |= HWC_HINT_CLEAR_FB;
        }
    }

    return handled;
}


void IntelDisplayDevice::revisitLayerList(hwc_display_contents_1_t *list,
                                              bool isGeometryChanged)
{
    if (!list)
        return;

    for (size_t i = 0; i < (size_t)mLayerList->getLayersCount(); i++) {

        // also need check whether a layer can be handled in general
        if (!isHWCLayer(&list->hwLayers[i]))
            continue;

        // make sure all protected layers were marked as overlay
        if (mLayerList->isProtectedLayer(i))
            list->hwLayers[i].compositionType = HWC_OVERLAY;

        int flags = 0;
        if (isPrimaryLayer(list, i, &list->hwLayers[i], flags)) {
            bool ret = primaryPrepare(i, &list->hwLayers[i], flags);
            if (!ret) {
                ALOGE("%s: failed to prepare primary\n", __func__);
                list->hwLayers[i].compositionType = HWC_FRAMEBUFFER;
                list->hwLayers[i].hints = 0;
            }
        }
    }

    if (isGeometryChanged)
        updateZorderConfig();
}

bool IntelDisplayDevice::initializeRotationBufProvider()
{
    bool ret = false;
    if (!mBufferManager) {
        return false;
    }

    int DrmFd = mBufferManager->getDrmFd();
    if (DrmFd <= 0) {
        ALOGE("invalid drm FD");
        return false;
    }

    mWsbm = new IntelWsbm(DrmFd);
    if (!mWsbm) {
        ALOGE("failed to create wsbm object");
        return false;
    }

    ret = mWsbm->initialize();
    if (ret == false) {
        ALOGE("failed to initialize wsbm");
        delete mWsbm;
        mWsbm = NULL;
        return false;
    }

    mRotationBufProvider = new RotationBufferProvider(mWsbm);
    if (mRotationBufProvider == NULL) {
        ALOGE("failed to new RotationBufferProvider");
        return false;
    }

    if (!mRotationBufProvider->initialize()) {
        ALOGE("failed to initialize RotationBufferProvider");
        return false;
    }

    return true;
}

void IntelDisplayDevice::destroyRotationBufProvider()
{
    if (mRotationBufProvider) {
        mRotationBufProvider->deinitialize();
        delete mRotationBufProvider;
        mRotationBufProvider = NULL;
    }

    if (mWsbm) {
        delete mWsbm;
        mWsbm = NULL;
    }
}

void IntelDisplayDevice::updateZorderConfig()
{
    int zOrderConfig = IntelDisplayPlaneManager::ZORDER_POaOc;

    if (mLayerList->getAttachedOverlayCount()) {
        IntelDisplayPlane* plane = mLayerList->getPlane(0);
        // If there is a blending to overlay, the overlay should be
        // the first layer always; other wise, we should put overlay
        // on top.
        if (plane && plane->getPlaneType() ==
                IntelDisplayPlane::DISPLAY_PLANE_OVERLAY)
            zOrderConfig = IntelDisplayPlaneManager::ZORDER_POcOa;
        else
            zOrderConfig = IntelDisplayPlaneManager::ZORDER_OcOaP;
    }

    mPlaneManager->setZOrderConfig(zOrderConfig, 0);
}

// This function performs:
// 1) update layer's transform to data buffer's payload buffer, so that video
//    driver can get the latest transform info of this layer
// 2) if rotation is needed, video driver would setup the rotated buffer, then
//    update buffer's payload to inform HWC rotation buffer is available.
// 3) HWC would keep using ST till all rotation buffers are ready.
// Return: false if HWC is NOT ready to switch to overlay, otherwise true.
bool IntelDisplayDevice::useOverlayRotation(hwc_layer_1_t *layer,
                                         int index,
                                         uint32_t& handle,
                                         int& w, int& h,
                                         int& srcX, int& srcY,
                                         int& srcW, int& srcH,
                                         uint32_t &transform)
{
    bool useOverlay = false;
    uint32_t hwcLayerTransform;

    // FIXME: workaround for rotation issue, remove it later
    static int counter = 0;
    uint32_t metadata_transform = 0;
    uint32_t displayMode = 0;

    if (!layer)
        return false;

    IMG_native_handle_t *grallocHandle =
        (IMG_native_handle_t*)layer->handle;

    if (!grallocHandle)
        return false;

    // detect video mode change
    displayMode = mDrm->getDisplayMode();

    if (grallocHandle->iFormat == HAL_PIXEL_FORMAT_INTEL_HWC_NV12_VED ||
        grallocHandle->iFormat == HAL_PIXEL_FORMAT_INTEL_HWC_NV12_TILE) {

        // get payload buffer
        IntelPayloadBuffer buffer(mGrallocBufferManager, grallocHandle->fd[1]);

        intel_gralloc_payload_t *payload =
                (intel_gralloc_payload_t*)buffer.getCpuAddr();
        if (!payload) {
            ALOGE("%s: invalid address\n", __func__);
            return false;
        }

        if (payload->force_output_method == OUTPUT_FORCE_GPU) {
            ALOGD_IF(ALLOW_HWC_PRINT,
                    "%s: force to use surface texture.", __func__);
            return false;
        }

        metadata_transform = payload->metadata_transform;

        //For extend mode, we ignore WM rotate info
        if (displayMode == OVERLAY_EXTEND) {
            transform = metadata_transform;
        }

        if (!transform) {
            ALOGD_IF(ALLOW_HWC_PRINT,
                    "%s: use overlay to display original buffer.", __func__);
            return true;
        }

        if (transform != uint32_t(payload->client_transform)) {
            payload->hwc_timestamp = systemTime();
            payload->layer_transform = transform;

            ALOGD_IF(ALLOW_HWC_PRINT,
                    "%s: rotation buffer was not prepared by client! ui64Stamp = %llu", __func__, grallocHandle->ui64Stamp);

            if ( payload->force_output_method == OUTPUT_FORCE_OVERLAY ||
                payload->surface_protected) {
                bool ret = false;
                if (!mRotationBufProvider) {
                    ALOGE("failed to initialize RotationBufProvider");
                    return false;
                }
                ret = mRotationBufProvider->setupRotationBuffer(payload, transform);
                if (ret == false) {
                    ALOGE("failed to provider the rotation buffer");
                    return false;
                }
            } else
                return false;
        }

        // update handle, w & h to rotation buffer
        handle = payload->rotated_buffer_handle;
        w = payload->rotated_width;
        h = payload->rotated_height;
        // NOTE: exchange the srcWidth & srcHeight since
        // video driver currently doesn't call native_window_*
        // helper functions to update info for rotation buffer.
        if (transform == HAL_TRANSFORM_ROT_90 ||
                transform == HAL_TRANSFORM_ROT_270) {
            int temp = srcH;
            srcH = srcW;
            srcW = temp;
            temp = srcX;
            srcX = srcY;
            srcY = temp;

        }

        // skip pading bytes in rotate buffer
        switch(transform) {
            case HAL_TRANSFORM_ROT_90:
                srcX += ((srcW + 0xf) & ~0xf) - srcW;
                break;
            case HAL_TRANSFORM_ROT_180:
                srcX += ((srcW + 0xf) & ~0xf) - srcW;
                srcY += ((srcH + 0xf) & ~0xf) - srcH;
                break;
            case HAL_TRANSFORM_ROT_270:
                srcY += ((srcH + 0xf) & ~0xf) - srcH;
                break;
            default:
                break;
        }
    } else {
        //For software codec, overlay can't handle rotate
        //and fallback to surface texture.
        if ((displayMode != OVERLAY_EXTEND) && transform) {
            ALOGD_IF(ALLOW_HWC_PRINT,
                    "%s: software codec rotation, back to ST!", __func__);
            return false;
        }
        //set this flag for mapping correct buffer
        transform = 0;
    }

    //for most of cases, we handle rotate by overlay
    useOverlay = true;
    return useOverlay;
}

// This function performs:
// Acquire bool BobDeinterlace from video driver
// If ture, use bob deinterlace, otherwise, use weave deinterlace
bool IntelDisplayDevice::isBobDeinterlace(hwc_layer_1_t *layer)
{
    bool bobDeinterlace = false;

    if (!layer)
        return bobDeinterlace;

    IMG_native_handle_t *grallocHandle =
        (IMG_native_handle_t*)layer->handle;

    if (!grallocHandle) {
        return bobDeinterlace;
    }

    if (grallocHandle->iFormat != HAL_PIXEL_FORMAT_INTEL_HWC_NV12_VED &&
        grallocHandle->iFormat != HAL_PIXEL_FORMAT_INTEL_HWC_NV12_TILE)
        return bobDeinterlace;

    // get payload buffer
    IntelPayloadBuffer buffer(mGrallocBufferManager, grallocHandle->fd[1]);

    intel_gralloc_payload_t *payload =
            (intel_gralloc_payload_t*)buffer.getCpuAddr();
    if (!payload) {
        ALOGE("%s: invalid address\n", __func__);
        return bobDeinterlace;
    }

    bobDeinterlace = (payload->bob_deinterlace == 1) ? true : false;
    return bobDeinterlace;
}
