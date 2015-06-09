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
#include <Log.h>
#include <MrflGrallocBuffer.h>

namespace android {
namespace intel {

static Log& log = Log::getInstance();
MrflGrallocBuffer::MrflGrallocBuffer(uint32_t handle)
{
    struct IMGGrallocBuffer *grallocHandle = (struct IMGGrallocBuffer*)handle;
    int bpp;
    int yStride, uvStride;

    log.v("MrflGrallocBuffer: handle 0x%x\n");

    memset(&mCrop, 0, sizeof(crop_t));

    if (!grallocHandle)
        goto invalid_handle;

    mHandle = handle;
    mFormat = grallocHandle->format;
    mWidth = grallocHandle->width;
    mHeight = grallocHandle->height;
    mStamp = grallocHandle->ui64Stamp;
    mUsage = grallocHandle->usage;
    bpp = grallocHandle->bpp;

    // setup stride
    switch (mFormat) {
    case FORMAT_YV12:
    case FORMAT_I420:
        yStride = align_to(align_to(mWidth, 32), 64);
        uvStride = align_to(yStride >> 1, 64);
        mStride.yuv.yStride = yStride;
        mStride.yuv.uvStride = uvStride;
        break;
    case FORMAT_NV12_VED:
        yStride = align_to(align_to(mWidth, 32), 64);
        uvStride = yStride;
        mStride.yuv.yStride = yStride;
        mStride.yuv.uvStride = uvStride;
        break;
    case FORMAT_YUY2:
    case FORMAT_UYVY:
        yStride = align_to((align_to(mWidth, 32) << 1), 64);
        uvStride = 0;
        mStride.yuv.yStride = yStride;
        mStride.yuv.uvStride = uvStride;
        break;
    default:
        mStride.rgb.stride = align_to(((bpp >> 3) * align_to(mWidth, 32)), 64);
        break;
    }

    return;
invalid_handle:
    log.e("MrflGrallocBuffer: invalid gralloc handle");
    mHandle = 0;
    mFormat = FORMAT_INVALID;
    mWidth = 0;
    mHeight = 0;
    mStamp = 0;
    mUsage = 0;
}

MrflGrallocBuffer::~MrflGrallocBuffer()
{

}

}
}
