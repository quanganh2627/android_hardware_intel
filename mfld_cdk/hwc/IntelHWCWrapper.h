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

#ifndef INTEL_HWC_WRAPPER_H
#define INTEL_HWC_WRAPPER_H

#define HWC_REMOVE_DEPRECATED_VERSIONS 1

#include <hardware/hwcomposer.h>
#include <hardware/hardware.h>
#include <system/graphics.h>
#include <hal_public.h>

#ifdef ENABLE_DISPLAY_PIPE_C
#define INTEL_HW_OVERLAY_COUNT 2
#else
#define INTEL_HW_OVERLAY_COUNT 1
#endif
#define INTEL_HW_SPRIT_COUNT  1
#define INTEL_HW_MAX_PLANE_COUNT (INTEL_HW_OVERLAY_COUNT + INTEL_HW_SPRIT_COUNT)
#define YUV_BUFFER_COUNT      3

enum {
    HWC_SUPPORT_SYS_NAV_BAR = 0x00000001,
    HWC_SUPPORT_SYS_STATUS_BAR = 0x00000002,
    HWC_SUPPORT_SYS_WALLPAPER = 0x000000004,

    HWC_SKIP_SYS_DISPLAY = 0x10000000,
    HWC_BLIT_SYS_ALWAYS = 0x20000000,
    HWC_SUPPORT_MERGED_LAYER = 0x40000000,
};

typedef struct sys_layer
{
    int index;
    int width;
    int height;
    int yuv_index;
    buffer_handle_t rgb;
    buffer_handle_t yuv[YUV_BUFFER_COUNT];
} sys_layer_t;

class IntelHWCWrapper
{
private:
    bool mInitialized;
    int  mDebugFlags;
    IMG_gralloc_module_public_t* mGrallocModule;
    alloc_device_t* mGrallocDevice;
    size_t mNumLayers;
    size_t mNumYUV;
    size_t mNumSys;
    size_t mFbWidth;
    size_t mFbHeight;
    sys_layer_t mNavBar;
    sys_layer_t mStatusBar;

    // all sys layers are in one overlay, use source color key.
    bool mUseMergedLayer;
    sys_layer_t mMergedLayer;

    bool isYUVLayer(hwc_layer_1_t* hwcl);
    bool isIntersectRect(hwc_rect_t* r1, hwc_rect_t* r2);
    bool isOverlappedLayer(hwc_display_contents_1_t *list,
                           hwc_layer_1_t* hwcl);

    bool checkLayersSupport(hwc_display_contents_1_t *list);
    bool checkSysLayer(sys_layer_t* sl,
                       hwc_display_contents_1_t* list,
                       hwc_layer_1_t* hwcl);
    bool updateSysLayer(sys_layer_t* sl,
                        hwc_display_contents_1_t* list,
                        hwc_layer_1_t* hwcl);
public:
    IntelHWCWrapper();
    ~IntelHWCWrapper();

    bool initialize();
    bool pre_prepare(hwc_display_contents_1_t *list);
    bool post_prepare(hwc_display_contents_1_t *list);
    bool pre_commit(hwc_display_t dpy,
                    hwc_surface_t sur,
                    hwc_display_contents_1_t *list);
    bool post_commit(hwc_display_t dpy,
                     hwc_surface_t sur,
                     hwc_display_contents_1_t *list);

};

#endif //INTEL_HWC_WRAPPER_H
