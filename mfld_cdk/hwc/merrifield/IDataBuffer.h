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
#ifndef __IDATA_BUFFER_H__
#define __IDATA_BUFFER_H__

#include <hal_public.h>
//FIXME: remove dependency to OMX
#include <OMX_IVCommon.h>

namespace android {
namespace intel {

typedef struct crop {
    int x;
    int y;
    int w;
    int h;
} crop_t;

typedef struct stride {
    union {
        struct {
            uint32_t stride;
        } rgb;
        struct {
            uint32_t yStride;
            uint32_t uvStride;
        } yuv;
    };
}stride_t;

class IDataBuffer {
public:
    enum {
        // Invalid format
        FORMAT_INVALID = 0xffffffff,
        // RGB formats
        FORMAT_BGRA8888 = HAL_PIXEL_FORMAT_BGRA_8888,
        FORMAT_BGRX8888 = HAL_PIXEL_FORMAT_BGRX_8888,
        FORMAT_RGBX8888 = HAL_PIXEL_FORMAT_RGBX_8888,
        FORMAT_RGBA8888 = HAL_PIXEL_FORMAT_RGBA_8888,
        FORMAT_RGB565 = HAL_PIXEL_FORMAT_RGB_565,

        // YUV formats
        FORMAT_YV12 = HAL_PIXEL_FORMAT_YV12,
        FORMAT_I420 = HAL_PIXEL_FORMAT_I420,
        FORMAT_NV12_VED = OMX_INTEL_COLOR_FormatYUV420PackedSemiPlanar,
        FORMAT_YUY2 = HAL_PIXEL_FORMAT_YUY2,
        FORMAT_UYVY = HAL_PIXEL_FORMAT_UYVY,
    };

public:
    IDataBuffer() {}
    virtual ~IDataBuffer() {}

    // buffer source operations
    virtual uint32_t getHandle() const = 0;
    virtual void setStride(stride_t& stride) = 0;
    virtual stride_t& getStride () = 0;
    virtual void setWidth(uint32_t width) = 0;
    virtual uint32_t getWidth() const = 0;
    virtual void setHeight(uint32_t height) = 0;
    virtual uint32_t getHeight() const = 0;
    virtual void setCrop(int x, int y, int w, int h) = 0;
    virtual crop_t& getCrop() = 0;
    virtual void setFormat(uint32_t format) = 0;
    virtual uint32_t getFormat() const = 0;
};

// Data Buffer
class DataBuffer : public IDataBuffer {
public:
    DataBuffer(uint32_t handle) { mHandle = handle; }
    ~DataBuffer();
public:
    uint32_t getHandle() const { return mHandle; }
    void setStride(stride_t& stride) { mStride = stride; }
    stride_t& getStride() { return mStride; }
    void setWidth(uint32_t width) { mWidth = width; }
    uint32_t getWidth() const { return mWidth; }
    void setHeight(uint32_t height) { mHeight = height; }
    uint32_t getHeight() const { return mHeight; }
    void setCrop(int x, int y, int w, int h) {
        mCrop.x = x; mCrop.y = y; mCrop.w = w; mCrop.h = h; }
    crop_t& getCrop() { return mCrop; }
    void setFormat(uint32_t format) { mFormat = format; }
    uint32_t getFormat() const { return mFormat; }
private:
    uint32_t mHandle;
    stride_t mStride;
    crop_t mCrop;
    uint32_t mFormat;
    uint32_t mWidth;
    uint32_t mHeight;
};

} // namespace intel
} // namespace android

#endif /* __IDATA_BUFFER_H__ */
