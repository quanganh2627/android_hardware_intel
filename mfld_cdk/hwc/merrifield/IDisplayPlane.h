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
#ifndef __IDISPLAY_PLANE_H__
#define __IDISPLAY_PLANE_H__

#include <IBufferMapper.h>

namespace android {
namespace intel {

typedef struct {
    int x;
    int y;
    int w;
    int h;
} PlanePosition;

typedef struct {
   void *data;
   int size;
} PlaneContext;

enum {
    // support up to 4 overlays
    MAX_OVERLAY_COUNT = 4,
};

typedef struct {
    int layerCount;
    int planeCount;
    int overlayCount;
    int overlayIndexes[MAX_OVERLAY_COUNT];
    int primaryIndex;
} ZOrderConfig;

class IDisplayPlane {
public:
    // transform
    enum {
        PLANE_TRANSFORM_0 = 0,
        PLANE_TRANSFORM_90 = HWC_TRANSFORM_ROT_90,
        PLANE_TRANSFORM_180 = HWC_TRANSFORM_ROT_180,
        PLANE_TRANSFORM_270 = HWC_TRANSFORM_ROT_270,
    };

    // blending
    enum {
        PLANE_BLENDING_NONE = HWC_BLENDING_NONE,
        PLANE_BLENDING_PREMULT = HWC_BLENDING_PREMULT,
    };
    // pipe
    enum {
        PIPE_MIPI0 = 0,
        PIPE_HDMI,
        PIPE_MIPI1,
    };

    enum {
        PLANE_SPRITE = 1,
        PLANE_OVERLAY,
        PLANE_PRIMARY,
    };
public:
    IDisplayPlane() {}
    virtual ~IDisplayPlane() {}
public:
    virtual int getIndex() const = 0;
    virtual int getType() const = 0;

    // data destination
    virtual void setPosition(int x, int y, int w, int h) = 0;
    virtual void setSourceCrop(int x, int y, int w, int h) = 0;
    virtual void setTransform(int transform) = 0;

    // data source
    virtual bool isValidBuffer(uint32_t handle) = 0;
    virtual bool isValidTransform(uint32_t trans) = 0;
    virtual bool isValidBlending(uint32_t blending) = 0;
    virtual bool isValidScaling(hwc_rect_t& src, hwc_rect_t& dest) = 0;
    virtual bool setDataBuffer(uint32_t handle) = 0;
    virtual void invalidateBufferCache() = 0;

    // display device
    virtual bool assignToPipe(uint32_t pipe) = 0;

    // hardware operations
    virtual bool reset() = 0;
    virtual bool flip() = 0;
    virtual bool enable() = 0;
    virtual bool disable() = 0;

    // set z order config
    virtual void setZOrderConfig(ZOrderConfig& config) = 0;

    virtual void* getContext() const = 0;

    class IBufferCache {
    public:
        IBufferCache() {}
        virtual ~IBufferCache() {}
        // add a new mapper into buffer cache
        virtual bool addMapper(uint64_t handle, IBufferMapper* mapper) = 0;
        // get a buffer mapper
        virtual IBufferMapper* getMapper(uint64_t handle) = 0;
        // clear cache
        virtual void clear() = 0;
    };
};

} // namespace intel
} // namespace android

#endif /* __IDISPLAY_PLANE_H__ */
