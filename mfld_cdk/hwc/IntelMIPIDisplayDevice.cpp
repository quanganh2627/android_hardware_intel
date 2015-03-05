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
#include <cutils/properties.h>

#include <IntelHWComposer.h>
#include <IntelDisplayDevice.h>
#include <IntelOverlayUtil.h>
#include <IntelHWComposerCfg.h>
#define INTEL_EXT_SF_ANIMATION_HINT

IntelMIPIDisplayDevice::IntelMIPIDisplayDevice(IntelBufferManager *bm,
                                       IntelBufferManager *gm,
                                       IntelDisplayPlaneManager *pm,
                                       IntelHWComposerDrm *drm,
                                       WidiExtendedModeInfo *extinfo,
                                       uint32_t index)
                                     : IntelDisplayDevice(pm, drm, bm, gm, index),
                                       mExtendedModeInfo(extinfo),
                                       mVideoSentToWidi(false)
{
    ALOGD_IF(ALLOW_HWC_PRINT, "%s\n", __func__);

    //check buffer manager
    if (!mBufferManager) {
        ALOGE("%s: Invalid buffer manager\n", __func__);
        goto init_err;
    }

    // check buffer manager for gralloc buffer
    if (!mGrallocBufferManager) {
        ALOGE("%s: Invalid Gralloc buffer manager\n", __func__);
        goto init_err;
    }

    //create new DRM object if not exists
    if (!mDrm) {
        ALOGE("%s: failed to initialize DRM instance\n", __func__);
        goto init_err;
    }

    // check display plane manager
    if (!mPlaneManager) {
        ALOGE("%s: Invalid plane manager\n", __func__);
        goto init_err;
    }

    // create layer list
    mLayerList = new IntelHWComposerLayerList(mPlaneManager);
    if (!mLayerList) {
        ALOGE("%s: Failed to create layer list\n", __func__);
        goto init_err;
    }

    mSkipComposition = false;
    mHasGlesComposition = false;
    mHasSkipLayer = false;
    memset(&mLastHandles[0], 0, sizeof(mLastHandles));

    memset(&mPrevFlipHandles[0], 0, sizeof(mPrevFlipHandles));

    mInitialized = true;
    return;

init_err:
    mInitialized = false;
    return;
}

IntelMIPIDisplayDevice::~IntelMIPIDisplayDevice()
{
    delete mLayerList;
}

// isSpriteLayer: check whether a given @layer can be handled
// by a hardware sprite plane.
// A layer is a sprite layer when
// 1) layer is RGB layer &&
// 2) No active external display (TODO: support external display)
// 3) HWC_SKIP_LAYER flag wasn't set by surface flinger
// 4) layer requires no blending or premultipled blending
// 5) layer has no transform (rotation, scaling)
bool IntelMIPIDisplayDevice::isSpriteLayer(hwc_display_contents_1_t *list,
                                    int index,
                                    hwc_layer_1_t *layer,
                                    int& flags)
{
    bool needClearFb = false;
    bool forceSprite = false;
    bool useSprite = false;

    int srcWidth, srcHeight;
    int dstWidth, dstHeight;

    if (!list || !layer)
        return false;

    IMG_native_handle_t *grallocHandle =
        (IMG_native_handle_t*)layer->handle;

    if (!grallocHandle) {
        ALOGD_IF(ALLOW_HWC_PRINT, "%s: invalid gralloc handle\n", __func__);
        return false;
    }

    // check whether pixel format is supported RGB formats
    if (mLayerList->getLayerType(index) != IntelHWComposerLayer::LAYER_TYPE_RGB) {
        ALOGD_IF(ALLOW_HWC_PRINT,
                "%s: invalid format 0x%x\n", __func__, grallocHandle->iFormat);
        useSprite = false;
        goto out_check;
    }

    // fall back if HWC_SKIP_LAYER was set
    if ((layer->flags & HWC_SKIP_LAYER)) {
        ALOGD_IF(ALLOW_HWC_PRINT, "isSpriteLayer: HWC_SKIP_LAYER");
        useSprite = false;
        goto out_check;
    }

    // check usage???

    // check blending, only support none & premultipled blending
    // clear frame buffer region if layer has no blending
    if (layer->blending != HWC_BLENDING_PREMULT &&
        layer->blending != HWC_BLENDING_NONE) {
        ALOGD("isSpriteLayer: unsupported blending");
        useSprite = false;
        goto out_check;
    }

    // check rotation
    if (layer->transform) {
        ALOGD_IF(ALLOW_HWC_PRINT, "isSpriteLayer: need do transform");
        useSprite = false;
        goto out_check;
    }

     // check scaling
    srcWidth = layer->sourceCrop.right - layer->sourceCrop.left;
    srcHeight = layer->sourceCrop.bottom - layer->sourceCrop.top;
    dstWidth = layer->displayFrame.right - layer->displayFrame.left;
    dstHeight = layer->displayFrame.bottom - layer->displayFrame.top;

    if ((srcWidth == dstWidth) && (srcHeight == dstHeight))
        useSprite = true;
    else
        ALOGD_IF(ALLOW_HWC_PRINT,
               "isSpriteLayer: src W,H [%d, %d], dst W,H [%d, %d]",
               srcWidth, srcHeight, dstWidth, dstHeight);

    if (layer->blending == HWC_BLENDING_NONE)
        needClearFb = true;
out_check:
    if (forceSprite) {
        // clear HWC_SKIP_LAYER flag so that force to use overlay
        ALOGD("isSpriteLayer: force to use sprite");
        layer->flags &= ~HWC_SKIP_LAYER;
        mLayerList->setForceOverlay(index, true);
        layer->compositionType = HWC_OVERLAY;
        useSprite = true;
    }

    // check if frame buffer clear is needed
    if (useSprite) {
        ALOGD_IF(ALLOW_HWC_PRINT, "isSpriteLayer: got a sprite layer");
        if (needClearFb) {
            ALOGD_IF(ALLOW_HWC_PRINT, "isSpriteLayer: clear fb");
            //layer->hints |= HWC_HINT_CLEAR_FB;
            mForceSwapBuffer = true;
        }
        layer->compositionType = HWC_OVERLAY;
    }

    flags = 0;
    return useSprite;
}

// isPrimaryLayer: check whether we can use primary plane to handle
// the given @layer.
// primary plane can be used only when
// 1) @layer is on the top of other layers (FIXME: not necessary, remove it
//    after introducing z order configuration)
// 2) all other layers were handled by HWC.
// 3) @layer is a sprite layer
// 4) @layer wasn't handled by sprite
bool IntelMIPIDisplayDevice::isPrimaryLayer(hwc_display_contents_1_t *list,
                                     int index,
                                     hwc_layer_1_t *layer,
                                     int& flags)
{
    // if a layer has already been handled, further check if it's a
    // sprite layer/overlay layer, if so, we simply bypass this layer.
    if (layer->compositionType == HWC_OVERLAY) {
        IntelDisplayPlane *plane = mLayerList->getPlane(index);
        if (plane) {
            switch (plane->getPlaneType()) {
            case IntelDisplayPlane::DISPLAY_PLANE_PRIMARY:
                // detach plane & re-check it
                mLayerList->detachPlane(index, plane);
                layer->compositionType = HWC_FRAMEBUFFER;
                layer->hints = 0;
                break;
            case IntelDisplayPlane::DISPLAY_PLANE_OVERLAY:
            case IntelDisplayPlane::DISPLAY_PLANE_RGB_OVERLAY:
            case IntelDisplayPlane::DISPLAY_PLANE_SPRITE:
            default:
                return false;
            }
        }
    }

    // check whether all other layers were handled by HWC
    for (size_t i = 0; i < (size_t)mLayerList->getLayersCount(); i++) {
        if ((i != size_t(index)) &&
            (list->hwLayers[i].compositionType != HWC_OVERLAY))
            return false;
    }

    return isSpriteLayer(list, index, layer, flags);
}


// TODO: re-implement this function after video interface
// is ready.
// Currently, LayerTS::setGeometry will set compositionType
// to HWC_OVERLAY. HWC will change it to HWC_FRAMEBUFFER
// if HWC found this layer was NOT a overlay layer (can NOT
// be handled by hardware overlay)
bool IntelMIPIDisplayDevice::isOverlayLayer(hwc_display_contents_1_t *list,
                                     int index,
                                     hwc_layer_1_t *layer,
                                     int& flags)
{
    bool needClearFb = false;
    bool forceOverlay = false;
    bool useOverlay = false;

    if (!list || !layer)
        return false;

    IMG_native_handle_t *grallocHandle =
        (IMG_native_handle_t*)layer->handle;

    if (!grallocHandle)
        return false;

    // clear hints
    layer->hints = 0;

    // check format
    if (mLayerList->getLayerType(index) != IntelHWComposerLayer::LAYER_TYPE_YUV) {
        useOverlay = false;
        goto out_check;
    }

    // force to use overlay in video extend mode
    if (mDrm->getDisplayMode() == OVERLAY_EXTEND)
        forceOverlay = true;

    // check buffer usage
    if ((grallocHandle->usage & GRALLOC_USAGE_PROTECTED) || isForceOverlay(layer)) {
        ALOGD_IF(ALLOW_HWC_PRINT, "isOverlayLayer: protected video/force Overlay");
        mDrm->setDisplayIed(true);
        forceOverlay = true;
    } else if (mVideoSeekingActive) {
        useOverlay = false;
        forceOverlay = false;
        goto out_check;
    }

    // check blending, overlay cannot support blending
    if (layer->blending != HWC_BLENDING_NONE) {
        useOverlay = false;
        goto out_check;
    }

    // fall back if HWC_SKIP_LAYER was set, if forced to use
    // overlay skip this check
    if (!forceOverlay && (layer->flags & HWC_SKIP_LAYER)) {
        ALOGD_IF(ALLOW_HWC_PRINT, "isOverlayLayer: skip layer was set");
        useOverlay = false;
        goto out_check;
    }

    // check visible regions
    if (layer->visibleRegionScreen.numRects > 1) {
        useOverlay = false;
        goto out_check;
    }

    // TODO: not support OVERLAY_CLONE_MIPI0
    if (mDrm->getDisplayMode() == OVERLAY_CLONE_MIPI0) {
        useOverlay = false;
        goto out_check;
    }

    // fall back if YUV Layer is in the middle of
    // other layers and covers the layers under it.
    if (!forceOverlay && index > 0 && index < (mLayerList->getLayersCount()-1)) {
        for (int i = index - 1; i >= 0; i--) {
            if (areLayersIntersecting(layer, &list->hwLayers[i])) {
                useOverlay = false;
                goto out_check;
            }
        }
    }

    // check whether layer are covered by layers above it
    // if layer is covered by a layer which needs blending,
    // clear corresponding region in frame buffer
    for (size_t i = index + 1; i < (size_t)mLayerList->getLayersCount(); i++) {
        if (areLayersIntersecting(&list->hwLayers[i], layer)) {
            ALOGD_IF(ALLOW_HWC_PRINT,
                "%s: overlay %d is covered by layer %d\n", __func__, index, i);
                if (list->hwLayers[i].blending !=  HWC_BLENDING_NONE)
                    mLayerList->setNeedClearup(index, true);
        }
    }

    useOverlay = true;
    needClearFb = true;
out_check:
    if (forceOverlay) {
        // clear HWC_SKIP_LAYER flag so that force to use overlay
        ALOGD_IF(ALLOW_HWC_PRINT, "isOverlayLayer: force to use overlay");
        layer->flags &= ~HWC_SKIP_LAYER;
        mLayerList->setForceOverlay(index, true);
        layer->compositionType = HWC_OVERLAY;
        useOverlay = true;
        needClearFb = true;
#ifdef INTEL_EXT_SF_ANIMATION_HINT
        layer->hints |= HWC_HINT_DISABLE_ANIMATION;
#endif
    }

    // check if frame buffer clear is needed
    if (useOverlay) {
        ALOGD_IF(ALLOW_HWC_PRINT, "isOverlayLayer: got an overlay layer");
        layer->compositionType = HWC_OVERLAY;
        if (needClearFb)
            ALOGD_IF(ALLOW_HWC_PRINT, "isOverlayLayer: clear fb");
    }

    flags = 0;
    return useOverlay;
}

// A layer can be handled by a RGB overlay when:
// 1) the layer is the most top layer & no blending is needed
// 2) the layer is NOT the top layer but has no intersection with other layers
bool IntelMIPIDisplayDevice::isRGBOverlayLayer(hwc_display_contents_1_t *list,
                                               unsigned int index,
                                               hwc_layer_1_t *layer,
                                               int& flags)
{
    bool useRGBOverlay = false;
    int srcWidth;
    int srcHeight;
    int dstWidth;
    int dstHeight;
    drmModeFBPtr fbInfo;

    if (!list || !layer)
        return false;

    IMG_native_handle_t *grallocHandle =
        (IMG_native_handle_t*)layer->handle;

    if (!grallocHandle)
        return false;

    // If there is only one layer, we can always display it by h/w
    if (list->numHwLayers < 3)
        return false;

    // Don't use it when:
    // 1) HDMI is connected
    // 2) there are YUV layers
    // 3) video starts to playing
    if ((mDrm->isHdmiConnected()) ||
        (mDrm->isVideoPrepared()) ||
        (mLayerList->getYUVLayerCount())) {
        useRGBOverlay = false;
        goto out_check;
    }

    if ((layer->flags & HWC_SKIP_LAYER)) {
        useRGBOverlay = false;
        goto out_check;
    }

    if (layer->transform) {
        useRGBOverlay = false;
        goto out_check;
    }

    // check format
    if (mLayerList->getLayerType(index) != IntelHWComposerLayer::LAYER_TYPE_RGB) {
        useRGBOverlay = false;
        goto out_check;
    }

    // check scaling
    srcWidth = grallocHandle->iWidth;
    srcHeight = grallocHandle->iHeight;
    dstWidth = layer->displayFrame.right - layer->displayFrame.left;
    dstHeight = layer->displayFrame.bottom - layer->displayFrame.top;

    if ((srcWidth != dstWidth) || (srcHeight != dstHeight)) {
        useRGBOverlay = false;
        goto out_check;
    }

    // check src size, if it's too big (large then 1/8 screen size),not worth it
    fbInfo = IntelHWComposerDrm::getInstance().getOutputFBInfo(OUTPUT_MIPI0);
    if ((srcWidth * srcHeight) > ((fbInfo->width * fbInfo->height) >> 3)) {
        useRGBOverlay = false;
        goto out_check;
    }

    if (index != list->numHwLayers - 2 && index != list->numHwLayers - 3) {
        useRGBOverlay = false;
        goto out_check;
    }

    if (index == list->numHwLayers -2 &&
        layer->blending == HWC_BLENDING_NONE) {
        useRGBOverlay = true;
        goto out_check;
    }

    // check whether this layer has intersection with other layers
    for (size_t i = 0; i < list->numHwLayers - 1; i++) {
        if (i == index)
            continue;
        if (areLayersIntersecting(&list->hwLayers[i], layer)) {
            useRGBOverlay = false;
            goto out_check;
        }
    }

    useRGBOverlay = true;
out_check:
    if (useRGBOverlay) {
        ALOGD_IF(ALLOW_HWC_PRINT, "isRGBOverlayLayer: got an RGB overlay layer");
        layer->compositionType = HWC_OVERLAY;
    }

    flags = 0;
    return useRGBOverlay;
}


// When the geometry changed, we need
// 0) reclaim all allocated planes, reclaimed planes will be disabled
//    on the start of next frame. A little bit tricky, we cannot disable the
//    planes right after geometry is changed since there's no data in FB now,
//    so we need to wait FB is update then disable these planes.
// 1) build a new layer list for the changed hwc_layer_list
// 2) attach planes to these layers which can be handled by HWC
void IntelMIPIDisplayDevice::onGeometryChanged(hwc_display_contents_1_t *list)
{
    ALOGD_IF(ALLOW_HWC_PRINT, "%s\n", __func__);
    // reclaim all planes
    bool ret = mLayerList->invalidatePlanes();
    if (!ret) {
        ALOGE("%s: failed to reclaim allocated planes\n", __func__);
        return;
    }

    // update layer list with new list
    mLayerList->updateLayerList(list);

    // TODO: uncomment it to print out layer list info
    // dumpLayerList(list);

    if (isScreenshotActive(list)) {
        ALOGD_IF(ALLOW_HWC_PRINT, "%s: Screenshot Active!\n", __func__);
        goto out_check;
    }

    mVideoSentToWidi = false;

    for (size_t i = 0; list && i < (size_t)mLayerList->getLayersCount(); i++) {
        // check whether a layer can be handled in general
        if (!isHWCLayer(&list->hwLayers[i]))
            continue;

        if (list->hwLayers[i].compositionType != HWC_BACKGROUND &&
            mExtendedModeInfo->widiExtHandle != NULL &&
            mExtendedModeInfo->widiExtHandle == (IMG_native_handle_t*)list->hwLayers[i].handle)
        {
            if(mVideoSeekingActive)
                list->hwLayers[i].compositionType = HWC_FRAMEBUFFER;
            else
                list->hwLayers[i].compositionType = HWC_OVERLAY;

            list->hwLayers[i].hints |= HWC_HINT_DISABLE_ANIMATION;
            mVideoSentToWidi = true;
            continue;
        }

        // further check whether a layer can be handle by overlay/sprite
        int flags = 0;
        //bool hasOverlay = mPlaneManager->hasFreeOverlays();
        bool hasRGBOverlay = mPlaneManager->hasFreeRGBOverlays();
        bool hasSprite = mPlaneManager->hasFreeSprites();

        if (/*hasOverlay &&*/ isOverlayLayer(list, i, &list->hwLayers[i], flags)) {
            ret = overlayPrepare(i, &list->hwLayers[i], flags);
            if (!ret) {
                ALOGE("%s: failed to prepare overlay\n", __func__);
                list->hwLayers[i].compositionType = HWC_FRAMEBUFFER;
                list->hwLayers[i].hints = 0;
            }
        } else if (hasRGBOverlay &&
                   isRGBOverlayLayer(list, i, &list->hwLayers[i], flags)) {
            ret = rgbOverlayPrepare(i, &list->hwLayers[i], flags);
            if (!ret) {
                LOGE("%s: failed to prepare RGB overlay\n", __func__);
                list->hwLayers[i].compositionType = HWC_FRAMEBUFFER;
                list->hwLayers[i].hints = 0;
            }
        } else if (hasSprite && isSpriteLayer(list, i, &list->hwLayers[i], flags)) {
            ret = spritePrepare(i, &list->hwLayers[i], flags);
            if (!ret) {
                ALOGE("%s: failed to prepare sprite\n", __func__);
                list->hwLayers[i].compositionType = HWC_FRAMEBUFFER;
                list->hwLayers[i].hints = 0;
            }
        } else {
            list->hwLayers[i].compositionType = HWC_FRAMEBUFFER;
        }
    }

out_check:
    // revisit each layer, make sure protected layers were handled by hwc,
    // and check if we can make use of primary plane
    revisitLayerList(list, true);

    // disable reclaimed planes
    mPlaneManager->disableReclaimedPlanes(IntelDisplayPlane::DISPLAY_PLANE_SPRITE);
    mPlaneManager->disableReclaimedPlanes(IntelDisplayPlane::DISPLAY_PLANE_PRIMARY);
}

bool IntelMIPIDisplayDevice::prepare(hwc_display_contents_1_t *list)
{
    ALOGD_IF(ALLOW_HWC_PRINT, "%s\n", __func__);

    if (!initCheck()) {
        ALOGE("%s: failed to initialize HWComposer\n", __func__);
        return false;
    }

    if (mIsBlank) {
        //ALOGW("%s: HWC is blank, bypass", __func__);
        return false;
    }

    // FIXME: it is tricky here to wait for cycles to disable reclaim overlay;
    // As processFlip cmd schedule in SGX maybe delayed more than one cycle,
    // especially under the case of high quality video rotation.
    static int cnt = 0;
    if (mPlaneManager->hasReclaimedOverlays() && (!mIsScreenshotActive || ++cnt == 3)) {
        mPlaneManager->disableReclaimedPlanes(IntelDisplayPlane::DISPLAY_PLANE_OVERLAY);
        mPlaneManager->disableReclaimedPlanes(IntelDisplayPlane::DISPLAY_PLANE_RGB_OVERLAY);
        cnt = 0;
    }

    // clear force swap buffer flag
    mForceSwapBuffer = false;

    int index = checkTrickMode(list, mVideoSentToWidi);
    bool forceCheckingList = ((index >= 0) != mVideoSeekingActive);
    mVideoSeekingActive = (index >= 0);

    // check whether video player status changed and then
    // determine whether traversing the layer list and
    // disable or enable RGBOverlay
    static bool isVideoPrepared = false;
    bool isPlayerStatusChanged =
                (isVideoPrepared != mDrm->isVideoPrepared()) ? true : false;

    if (isPlayerStatusChanged)
        isVideoPrepared = mDrm->isVideoPrepared();

    // handle geometry changing. attach display planes to layers
    // which can be handled by HWC.
    // plane control information (e.g. position) will be set here
    if (!list || (list->flags & HWC_GEOMETRY_CHANGED) ||
            mHotplugEvent || forceCheckingList || isPlayerStatusChanged) {
        onGeometryChanged(list);
        mHotplugEvent = false;
        mExtendedModeInfo->videoSentToWidi = mVideoSentToWidi;

        if (list && (list->flags & HWC_GEOMETRY_CHANGED) &&
            (mVideoSentToWidi || mDrm->getDisplayMode() == OVERLAY_EXTEND))
        {
            bool hasSkipLayer = false;
            ALOGD_IF(ALLOW_HWC_PRINT,
                    "layers num:%d", mLayerList->getLayersCount());
            for (size_t j = 0 ; j < (size_t)mLayerList->getLayersCount(); j++) {
                if (list->hwLayers[j].flags & HWC_SKIP_LAYER) {
                hasSkipLayer = true;
                break;
                }
            }

            if (!hasSkipLayer) {
                mDrm->notifyWidi(mVideoSentToWidi);
                if (mLayerList->getLayersCount() == 1 && mLayerList->getYUVLayerCount())
                    mDrm->notifyMipi(false);
                else
                    mDrm->notifyMipi(true);
            }
        }
    }

    // handle buffer changing. setup data buffer.
    if (list && !updateLayersData(list)) {
        ALOGD_IF(ALLOW_HWC_PRINT, "prepare: revisiting layer list\n");
        // TODO: We shouldn't change any layer to HWC_OVERLAY in the following
        // revisit because there is no chance to update the layer's data.
        revisitLayerList(list, false);
    }

    handleSmartComposition(list);

    if (list && (list->flags & HWC_GEOMETRY_CHANGED))
	    memset(&mPrevFlipHandles[0], 0, sizeof(mPrevFlipHandles));

    return true;
}

void IntelMIPIDisplayDevice::handleSmartComposition(hwc_display_contents_1_t *list)
{
    int i;
    bool bDirty = false;

    if (!list) return;

    // when geometry change, set layer dirty to switch out smart composition,
    // also update GLES composition status for later using.
    // BZ96412: Video layer compositionType is maybe changed in non-geometry
    // prepare, exclude it from smart composition.
    if (list->flags & HWC_GEOMETRY_CHANGED) {
        bDirty = true;
        mHasGlesComposition = false;
        mHasSkipLayer = false;
        for (i = 0; i < list->numHwLayers - 1; i++) {
            mHasGlesComposition = mHasGlesComposition ||
                (list->hwLayers[i].compositionType == HWC_FRAMEBUFFER &&
                 mLayerList->getLayerType(i) != IntelHWComposerLayer::LAYER_TYPE_YUV);
            mHasSkipLayer = mHasSkipLayer ||
                (list->hwLayers[i].flags & HWC_SKIP_LAYER);
        }
    }

    // Only check if there is YUV overlay, has composition on framebuffer
    // and the layer number is limited.
    if (mYUVOverlay < 0 || !mHasGlesComposition || mHasSkipLayer ||
        list->numHwLayers > 6 || list->numHwLayers < 4){
        ALOGD_IF(mSkipComposition, "Leave smart composition mode");
        mSkipComposition = false;
        return;
    }

    // check layer update except FB_TARGET and YUV overlay
    for (i = 0; i < list->numHwLayers - 1; i++) {
        bDirty = bDirty ||
            (i != mYUVOverlay && list->hwLayers[i].handle != mLastHandles[i]);
        mLastHandles[i] = list->hwLayers[i].handle;
    }

    // Smart composition state change
    if (mSkipComposition == bDirty) {
        mSkipComposition = !mSkipComposition;

        if (mSkipComposition)
            ALOGD_IF(ALLOW_HWC_PRINT, "Enter smart composition mode");
        else
            ALOGD_IF(ALLOW_HWC_PRINT, "Leave smart composition mode");

        // Update compositeType
        for (i = 0; i < list->numHwLayers - 1; i++) {
            if (i != mYUVOverlay)
                list->hwLayers[i].compositionType =
                        mSkipComposition ? HWC_OVERLAY : HWC_FRAMEBUFFER;
        }
    }
}

bool IntelMIPIDisplayDevice::commit(hwc_display_contents_1_t *list,
                                    buffer_handle_t *bh,
                                    int* acquireFenceFd,
                                    int** releaseFenceFd,
                                    int &numBuffers)
{
    ALOGD_IF(ALLOW_HWC_PRINT, "%s\n", __func__);

    if (!initCheck()) {
        ALOGE("%s: failed to initialize HWComposer\n", __func__);
        return false;
    }

    if (mIsBlank) {
        //ALOGW("%s: HWC is blank, bypass", __func__);
        return false;
    }

    // if hotplug was happened & didn't be handled skip the flip
    if (mHotplugEvent)
        return true;

    void *context = mPlaneManager->getPlaneContexts();
    if (!context) {
        ALOGE("%s: invalid plane contexts\n", __func__);
        return false;
    }
    // need check whether eglSwapBuffers is necessary
    bool needSwapBuffer = false;

    // in video ext mode ,where there is only on video layer which would be ignored
    // in the mipi device, we should not swap fb target
#if 0 
    // if all layers were attached with display planes then we don't need
    // swap buffers.
    if (!mLayerList->getLayersCount() ||
        mLayerList->getLayersCount() != mLayerList->getAttachedPlanesCount() ||
        mForceSwapBuffer) {
        ALOGD_IF(ALLOW_HWC_PRINT,
               "%s: mForceSwapBuffer: %d, layer count: %d, attached plane:%d\n",
               __func__, mForceSwapBuffer, mLayerList->getLayersCount(),
               mLayerList->getAttachedPlanesCount());

        // FIXME: it might be a surface flinger bug
        // surface flinger failed to render a layer to FB sometimes
	// because screen dirty region was unchanged, in this case
        // we don't to swap buffers.
#ifdef INTEL_EXT_SF_NEED_SWAPBUFFER
        if (!list || (list->flags & HWC_NEED_SWAPBUFFERS))
#endif
            needSwapBuffer = true;
    }

#else

    for (size_t i = 0 ; list && (i < list->numHwLayers - 1) ; i++) {
        needSwapBuffer = needSwapBuffer || 
            (list->hwLayers[i].compositionType == HWC_FRAMEBUFFER);
    }
#endif
    // if primary plane is in use, skip eglSwapBuffers
    if (!mPlaneManager->primaryAvailable(0)) {
        ALOGD_IF(ALLOW_HWC_PRINT, "%s: primary plane in use\n", __func__);
        needSwapBuffer = false;
    }

    {
        buffer_handle_t *bufferHandles = bh;

        if (!list)
            return false;

        // setup primary plane contexts if swap buffers is needed
        hwc_layer_1_t* fb_layer = &list->hwLayers[list->numHwLayers-1];
        if ((needSwapBuffer || mSkipComposition) &&
            mLayerList->getLayersCount() > 0 &&
            fb_layer->handle &&
            fb_layer->compositionType == HWC_FRAMEBUFFER_TARGET) {
            flipFramebufferContexts(context, fb_layer);
            acquireFenceFd[numBuffers] = fb_layer->acquireFenceFd;
            releaseFenceFd[numBuffers] = &fb_layer->releaseFenceFd;
            bufferHandles[numBuffers++] = fb_layer->handle;
        }

        // Call plane's flip for each layer in hwc_layer_list, if a plane has
        // been attached to a layer
        // First post RGB layers, then overlay layers.
        for (size_t i=0 ; list && i<(size_t)mLayerList->getLayersCount(); i++) {
            IntelDisplayPlane *plane = mLayerList->getPlane(i);
            int flags = mLayerList->getFlags(i);

            if (!plane)
                continue;
            if (list->hwLayers[i].flags & HWC_SKIP_LAYER)
                continue;
            if (list->hwLayers[i].compositionType != HWC_OVERLAY)
                continue;

            ALOGD_IF(ALLOW_HWC_PRINT, "%s: flip plane %d, flags: 0x%x\n",
                __func__, i, flags);

            bool ret = plane->flip(context, flags);
            if (!ret)
                ALOGW("%s: failed to flip plane %d context !\n", __func__, i);
            else {
                int planeType = plane->getPlaneType();
                buffer_handle_t handle =
                    (buffer_handle_t)plane->getDataBufferHandle();

                if ((planeType == IntelDisplayPlane::DISPLAY_PLANE_PRIMARY ||
                     planeType == IntelDisplayPlane::DISPLAY_PLANE_SPRITE) &&
                    (i < 10 && handle == mPrevFlipHandles[i])) {
                    // If we post two consecutive same buffers through
                    // primary plane, don't pass buffer handle to avoid
                    // SGX deadlock issue
                    ALOGD_IF(ALLOW_HWC_PRINT, "same RGB buffer handle %p", handle);
                } else if (plane->getDataBufferHandle() == 0) {
                    // check if plane data buffer is NULL, which may
                    // happen when updateLayerData failed.
                    ALOGW("layer [%d]: plane handle is NULL!", i);
                } else {
                    acquireFenceFd[numBuffers] = list->hwLayers[i].acquireFenceFd;
                    releaseFenceFd[numBuffers] = &list->hwLayers[i].releaseFenceFd;
                    bufferHandles[numBuffers++] =
                        (buffer_handle_t)plane->getDataBufferHandle();
                    if (i < 10) {
                        mPrevFlipHandles[i] =
			    (buffer_handle_t)plane->getDataBufferHandle();
                    }
                }
            }

            // clear flip flags, except for DELAY_DISABLE
            mLayerList->setFlags(i, flags & IntelDisplayPlane::DELAY_DISABLE);

            // remove clear fb hints
            list->hwLayers[i].hints &= ~HWC_HINT_CLEAR_FB;
        }
    }

#if 0
        //make sure all flips were finished
        for (size_t i=0 ; list && i<list->numHwLayers-1 ; i++) {
            IntelDisplayPlane *plane = mLayerList->getPlane(i);
            int flags = mLayerList->getFlags(i);
            if (!plane)
                continue;
            if (list->hwLayers[i].flags & HWC_SKIP_LAYER)
                continue;
            if (list->hwLayers[i].compositionType != HWC_OVERLAY)
                continue;

            plane->waitForFlipCompletion();
        }
#endif

    return true;
}

bool IntelMIPIDisplayDevice::isForceOverlay(hwc_layer_1_t *layer)
{
    if (!layer)
        return false;

    IMG_native_handle_t *grallocHandle =
        (IMG_native_handle_t*)layer->handle;

    if (!grallocHandle)
        return false;

    if (grallocHandle->iFormat != HAL_PIXEL_FORMAT_INTEL_HWC_NV12_VED &&
        grallocHandle->iFormat != HAL_PIXEL_FORMAT_INTEL_HWC_NV12_TILE)
        return false;

    // get payload buffer
    IntelPayloadBuffer buffer(mGrallocBufferManager, grallocHandle->fd[1]);

    intel_gralloc_payload_t *payload =
            (intel_gralloc_payload_t*)buffer.getCpuAddr();
    if (!payload) {
        ALOGE("%s: invalid address\n", __func__);
        return false;
    }

    bool ret = (payload->force_output_method == OUTPUT_FORCE_OVERLAY) ? true : false;
    return ret;
}



bool IntelMIPIDisplayDevice::dump(char *buff,
                           int buff_len, int *cur_len)
{
    IntelDisplayPlane *plane = NULL;
    bool ret = true;
    int i;

    mDumpBuf = buff;
    mDumpBuflen = buff_len;
    mDumpLen = (int) (*cur_len);

    if (mLayerList) {
       dumpPrintf("------------ MIPI Totally %d layers -------------\n",
                                     mLayerList->getLayersCount());
       for (i = 0; i < mLayerList->getLayersCount(); i++) {
           plane = mLayerList->getPlane(i);

           if (plane) {
               int planeType = plane->getPlaneType();
               dumpPrintf("   # layer %d attached to %s plane \n", i,
                            (planeType == 3) ? "overlay" : "sprite");
           } else
               dumpPrintf("   # layer %d goes through eglswapbuffer\n ", i);
       }

       dumpPrintf("-------------MIPI runtime parameters -------------\n");
       dumpPrintf("  + mHotplugEvent: %d \n", mHotplugEvent);
       dumpPrintf("  + mForceSwapBuffer: %d \n", mForceSwapBuffer);
       dumpPrintf("  + mForceSwapBuffer: %d \n", mForceSwapBuffer);
       dumpPrintf("  + Display Mode: %d \n", mDrm->getDisplayMode());
    }

    *cur_len = mDumpLen;
    return ret;
}

bool IntelMIPIDisplayDevice::getDisplayConfig(uint32_t* configs,
                                        size_t* numConfigs)
{
    if (!numConfigs || !numConfigs[0])
        return false;

    if (mDrm->getOutputConnection(mDisplayIndex) != DRM_MODE_CONNECTED)
        return false;

    *numConfigs = 1;
    configs[0] = 0;

    return true;
}

bool IntelMIPIDisplayDevice::getDisplayAttributes(uint32_t config,
            const uint32_t* attributes, int32_t* values)
{
    char val[PROPERTY_VALUE_MAX];

    if (config != 0)
        return false;

    if (!attributes || !values)
        return false;

    if (mDrm->getOutputConnection(mDisplayIndex) != DRM_MODE_CONNECTED)
        return false;

    drmModeModeInfoPtr mode = mDrm->getOutputMode(mDisplayIndex);

    while (*attributes != HWC_DISPLAY_NO_ATTRIBUTE) {
        switch (*attributes) {
        case HWC_DISPLAY_VSYNC_PERIOD:
            *values = 1e9 /  mode->vrefresh;
            break;
        case HWC_DISPLAY_WIDTH:
            *values = mode->hdisplay;
            break;
        case HWC_DISPLAY_HEIGHT:
            *values = mode->vdisplay;
            break;
        case HWC_DISPLAY_DPI_X:
            property_get("panel.physicalWidthmm", val, "47");
            *values = (atoi(val) == 0) ? 144000.0f :
                (mode->hdisplay * 25000.4f / atoi(val));
            break;
        case HWC_DISPLAY_DPI_Y:
            property_get("panel.physicalHeightmm", val, "82");
            *values = (atoi(val) == 0) ? 144000.0f :
                (mode->vdisplay * 25000.4f / atoi(val));
            break;
        default:
            break;
        }
        attributes ++;
        values ++;
    }

    return true;
}



bool IntelMIPIDisplayDevice::overlayPrepare(int index, hwc_layer_1_t *layer, int flags)
{

    ALOGD_IF(ALLOW_HWC_PRINT, "%s\n", __func__);

    if (!layer) {
        ALOGE("%s: Invalid layer\n", __func__);
        return false;
    }
    if(shouldHide(layer)){
        layer->compositionType = HWC_OVERLAY;
        return true;
    }

    //external not prepared

    bool hasOverlay = mPlaneManager->hasFreeOverlays();
    if(!hasOverlay){
        return false;
    }

    // has overlay and haveOverlay.
    // allocate overlay plane
    IntelDisplayPlane *plane = mPlaneManager->getOverlayPlane();
    if (!plane) {
        ALOGE("%s: failed to create overlay plane\n", __func__);
        return false;
    }

    int dstLeft = layer->displayFrame.left;
    int dstTop = layer->displayFrame.top;
    int dstRight = layer->displayFrame.right;
    int dstBottom = layer->displayFrame.bottom;

    IntelOverlayPlane *overlayP =
        reinterpret_cast<IntelOverlayPlane *>(plane);

    // setup plane parameters
    overlayP->setPosition(dstLeft, dstTop, dstRight, dstBottom);

    overlayP->setPipe(PIPE_MIPI0);

    // attach plane to hwc layer
    mLayerList->attachPlane(index, overlayP, flags);

    return true;
}

/**
* should hide this layer?
*
*/
bool IntelMIPIDisplayDevice::shouldHide(hwc_layer_1_t *layer){
     //TODO: hack for extended_mode
    bool result = false;
    if(mDrm->getDisplayMode() == OVERLAY_EXTEND){
        result = true;
    }
    ALOGD_IF((ALLOW_HWC_PRINT) && result, "Hide this layer");
    return result;
}

