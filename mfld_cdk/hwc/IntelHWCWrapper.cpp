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
 */

#include <utils/Log.h>
#include "IntelHWCWrapper.h"
#include "IntelBufferManager.h"

IntelHWCWrapper::IntelHWCWrapper():
                 mInitialized(false),
                 mGrallocModule(0),
                 mGrallocDevice(0),
                 mNumLayers(0),
                 mNumYUV(0),
                 mNumSys(0),
                 mFbWidth(0),
                 mFbHeight(0),
                 mUseMergedLayer(false)
{
    memset(&mNavBar, 0, sizeof(mNavBar));
    memset(&mStatusBar, 0, sizeof(mStatusBar));
    memset(&mMergedLayer, 0, sizeof(mMergedLayer));
    mDebugFlags =
        HWC_SUPPORT_SYS_NAV_BAR |
        HWC_SUPPORT_SYS_STATUS_BAR |
        0;
}

IntelHWCWrapper::~IntelHWCWrapper()
{
    int i;

    for (i=0; i<YUV_BUFFER_COUNT; i++) {
        if (mNavBar.yuv[i]) {
            mGrallocDevice->free(mGrallocDevice, mNavBar.yuv[i]);
        }
        if (mStatusBar.yuv[i]) {
            mGrallocDevice->free(mGrallocDevice, mStatusBar.yuv[i]);
        }
        if (mMergedLayer.yuv[i]) {
            mGrallocDevice->free(mGrallocDevice, mMergedLayer.yuv[i]);
        }
    }
    gralloc_close(mGrallocDevice);
}

bool IntelHWCWrapper::initialize()
{
    hw_module_t const* module;

    if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module) == 0) {
        mGrallocModule = (IMG_gralloc_module_public_t*)module;
        gralloc_open(module, &mGrallocDevice);
        mFbWidth = mGrallocModule->psFrameBufferDevice->base.width;
        mFbHeight = mGrallocModule->psFrameBufferDevice->base.height;
        mInitialized = (mGrallocModule && mGrallocDevice &&
                        mFbWidth && mFbHeight) ? true : false;
    }
    return true;
}

bool IntelHWCWrapper::pre_prepare(hwc_display_contents_1_t *list)
{
    if (!mInitialized || !list)
        return false;

    if (!checkLayersSupport(list)) {
        return false;
    }

    if (!mUseMergedLayer) {
        if (mNavBar.index > 0) {
            updateSysLayer(&mNavBar, list,
                &list->hwLayers[mNavBar.index - 1]);
        }
        if (mStatusBar.index > 0) {
            updateSysLayer(&mStatusBar, list,
                &list->hwLayers[mStatusBar.index - 1]);
        }
    } else {
        if (!(mNavBar.index > 0 && mStatusBar.index > 0))
            return false;

        // Merged Layer support is not implemented yet.
        // blit all the layers if any of one is updated since they are in the
        // same back buffer.
        return false;
    }

#ifdef SKIP_DISPLAY_SYS_LAYER
    if (mNumSys) list->numHwLayers = mNumLayers - mNumSys;
#endif

    return true;
}

bool IntelHWCWrapper::post_prepare(hwc_display_contents_1_t *list)
{
    if (!mInitialized)
        return false;

#ifdef SKIP_DISPLAY_SYS_LAYER
    if (mNumSys) list->numHwLayers = mNumLayers;
#endif

    return true;
}

bool IntelHWCWrapper::pre_commit(hwc_display_t dpy,
                                 hwc_surface_t sur,
                                 hwc_display_contents_1_t *list)
{
    if (!mInitialized)
        return false;

#ifdef SKIP_DISPLAY_SYS_LAYER
    if (mNumSys) list->numHwLayers = mNumLayers - mNumSys;
#endif

    return true;
}

bool IntelHWCWrapper::post_commit(hwc_display_t dpy,
                                  hwc_surface_t sur,
                                  hwc_display_contents_1_t *list)
{
    if (!mInitialized)
        return false;

#ifdef SKIP_DISPLAY_SYS_LAYER
    if (mNumSys) list->numHwLayers = mNumLayers;
#endif

    return true;
}

bool IntelHWCWrapper::isYUVLayer(hwc_layer_1_t* hwcl)
{
    IMG_native_handle_t* grallocHandle =
        (IMG_native_handle_t*)hwcl->handle;

    return (grallocHandle &&
            (grallocHandle->iFormat == HAL_PIXEL_FORMAT_YV12 ||
             grallocHandle->iFormat == HAL_PIXEL_FORMAT_INTEL_HWC_NV12_VED ||
             grallocHandle->iFormat == HAL_PIXEL_FORMAT_INTEL_HWC_NV12 ||
             grallocHandle->iFormat == HAL_PIXEL_FORMAT_INTEL_HWC_YUY2 ||
             grallocHandle->iFormat == HAL_PIXEL_FORMAT_INTEL_HWC_UYVY ||
             grallocHandle->iFormat == HAL_PIXEL_FORMAT_INTEL_HWC_I420));
}

bool IntelHWCWrapper::isIntersectRect(hwc_rect_t* r1, hwc_rect_t* r2)
{
    return !(r1->left >= r2->right ||
             r1->right <= r2->left ||
             r1->top >= r2->bottom ||
             r1->bottom <= r2->top);
}

bool IntelHWCWrapper::isOverlappedLayer(hwc_display_contents_1_t *list,
                                        hwc_layer_1_t* hwcl)
{
    size_t i;
    hwc_rect_t* rect = &hwcl->displayFrame;

    for (i = 0; i < list->numHwLayers; i++) {
        if (hwcl == &list->hwLayers[i])
            continue;

        if (list->hwLayers[i].flags &
            (HWC_STATUS_BAR_LAYER | HWC_NAVIGATION_BAR_LAYER))
            continue;

        if (isIntersectRect(&list->hwLayers[i].displayFrame, rect))
            return true;
    }

    return false;
}

bool IntelHWCWrapper::checkLayersSupport(hwc_display_contents_1_t *list)
{
    hwc_layer_1_t* hwcl;
    int stride;
    size_t i;

    if (list->flags & HWC_GEOMETRY_CHANGED) {
        mNumLayers = list->numHwLayers;
        mNavBar.index = mStatusBar.index = 0;
        mUseMergedLayer = false;
        mNumYUV = mNumSys = 0;

        if (mNumLayers > INTEL_HW_MAX_PLANE_COUNT)
            goto ret_false;

        for (i = 0; i < list->numHwLayers; i++) {
            hwcl = &list->hwLayers[i];
            if (hwcl->flags & HWC_SKIP_LAYER)
                continue;

            if ((hwcl->flags & HWC_NAVIGATION_BAR_LAYER) &&
                checkSysLayer(&mNavBar, list, hwcl)) {
                mNavBar.index = i + 1;
                mNumSys ++;
            } else if ((hwcl->flags & HWC_STATUS_BAR_LAYER) &&
                checkSysLayer(&mStatusBar, list, hwcl)) {
                mStatusBar.index = i + 1;
                mNumSys ++;
            } else if (isYUVLayer(hwcl)) {
                mNumYUV ++;
            }
        }

        if (mNumYUV >= INTEL_HW_OVERLAY_COUNT) {
            goto ret_false;
        } else if (mNumYUV + mNumSys > INTEL_HW_OVERLAY_COUNT &&
            (mDebugFlags & HWC_SUPPORT_MERGED_LAYER)) {
            for (i=0; i<YUV_BUFFER_COUNT; i++) {
                if (!mMergedLayer.yuv[i]) {
                    if (mGrallocDevice->alloc(mGrallocDevice,
                        mFbWidth, mFbHeight,
                        HAL_PIXEL_FORMAT_INTEL_HWC_NV12,
                        GRALLOC_USAGE_HW_RENDER |
                        GRALLOC_USAGE_HW_TEXTURE |
                        GRALLOC_USAGE_HW_COMPOSER,
                        &mMergedLayer.yuv[i], &stride))
                    {
                        AALOGE("checkLayersSupport:Failed to alloc YUV buffer %d", i);
                        goto ret_false;
                    }
                }
            }
            mMergedLayer.width = mFbWidth;
            mMergedLayer.height = mFbHeight;
            mUseMergedLayer = true;
        }
    }

    return (mStatusBar.index > 0 || mNavBar.index > 0);

ret_false:
    mNavBar.index = mStatusBar.index = 0;
    mNavBar.rgb = mStatusBar.rgb = 0;
    return false;
}

bool IntelHWCWrapper::checkSysLayer(sys_layer_t* sl,
                                    hwc_display_contents_1_t* list,
                                    hwc_layer_1_t* hwcl)
{
    int width, height, stride;
    int i;

    width = (int)(hwcl->sourceCropf.right - hwcl->sourceCropf.left);
    height = (int)(hwcl->sourceCropf.bottom - hwcl->sourceCropf.top);

    // SourceCrop must the same size as displayFrame
    if (width != hwcl->displayFrame.right - hwcl->displayFrame.left ||
        height != hwcl->displayFrame.bottom - hwcl->displayFrame.top)
        return false;

    // We only handle small system bars.
    if ((width != (int)mFbWidth && height != (int)mFbHeight) ||
        (width > 128 && height > 128))
        return false;

    if ((hwcl->blending != HWC_BLENDING_NONE) &&
        isOverlappedLayer(list, hwcl))
        return false;

    if (width != sl->width || height != sl->height) {
        for (i=0; i<YUV_BUFFER_COUNT; i++) {
            if (sl->yuv[i]) {
                mGrallocDevice->free(mGrallocDevice, sl->yuv[i]);
            }
            if (mGrallocDevice->alloc(mGrallocDevice,
                    width, height,
                    HAL_PIXEL_FORMAT_INTEL_HWC_NV12,
                    GRALLOC_USAGE_HW_RENDER |
                    GRALLOC_USAGE_HW_TEXTURE |
                    GRALLOC_USAGE_HW_COMPOSER,
                    &sl->yuv[i], &stride))
            {
                AALOGE("checkSysLayer:Failed to alloc YUV buffer %d", i);
                return false;
            }
        }
        sl->width = width;
        sl->height = height;
        sl->rgb = 0;
    }

    return true;
}

bool IntelHWCWrapper::updateSysLayer(sys_layer_t* sl,
                                     hwc_display_contents_1_t* list,
                                     hwc_layer_1_t* hwcl)
{
    // If the layer is updated, blit the new data to yuv buffer.
    if (sl->rgb != hwcl->handle) {
        sl->yuv_index ++;
        sl->yuv_index %= YUV_BUFFER_COUNT;
        if (mGrallocModule->Blit2(mGrallocModule,
                hwcl->handle,
                sl->yuv[sl->yuv_index],
                sl->width, sl->height,
                (int)(hwcl->sourceCropf.left),
                (int)(hwcl->sourceCropf.top)))
        {
            AALOGE("updateSysLayer:Failed to blit to YUV buffer");
            sl->rgb = 0;
            return false;
        }
        sl->rgb = hwcl->handle;
    }

    // Replace the hwc layer handle to yuv buffer handle.
    hwcl->handle = sl->yuv[sl->yuv_index];

    // Set blending to none, it is used for status bar.
    hwcl->blending = HWC_BLENDING_NONE;

    return true;
}
