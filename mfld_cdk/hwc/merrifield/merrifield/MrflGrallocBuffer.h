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
#ifndef MRFLGRALLOCBUFFER_H_
#define MRFLGRALLOCBUFFER_H_

#include <IDataBuffer.h>

namespace android {
namespace intel {

enum {
    SUB_BUFFER0 = 0,
    SUB_BUFFER1,
    SUB_BUFFER2,
    SUB_BUFFER_MAX,
};

struct IMGGrallocBuffer{
    native_handle_t base;
    int syncFD;
    int fd[SUB_BUFFER_MAX];
    unsigned long long ui64Stamp;
    int usage;
    int width;
    int height;
    int format;
    int bpp;
}__attribute__((aligned(sizeof(int)),packed));

struct PayloadBuffer {
    // transform made by clients (clients to hwc)
    int client_transform;
    int metadata_transform;
    int rotated_width;
    int rotated_height;
    int surface_protected;
    int force_output_method;
    uint32_t rotated_buffer_handle;
    uint32_t renderStatus;
    unsigned int used_by_widi;
    int bob_deinterlace;
    uint32_t width;
    uint32_t height;
    uint32_t luma_stride;
    uint32_t chroma_u_stride;
    uint32_t chroma_v_stride;
    uint32_t format;
    uint32_t khandle;
    int64_t  timestamp;

    uint32_t rotate_luma_stride;
    uint32_t rotate_chroma_u_stride;
    uint32_t rotate_chroma_v_stride;
    void *native_window;
};

// force output method values
enum {
    FORCE_OUTPUT_INVALID = 0,
    FORCE_OUTPUT_GPU,
    FORCE_OUTPUT_OVERLAY,
};

class MrflGrallocBuffer : public IDataBuffer {
public:
    MrflGrallocBuffer(uint32_t handle);
    ~MrflGrallocBuffer();

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

    // gralloc buffer operation
    uint64_t getStamp() const { return mStamp; }
    uint32_t getUsage() const { return mUsage; };
private:
    uint32_t mHandle;
    stride_t mStride;
    crop_t mCrop;
    uint32_t mFormat;
    uint32_t mWidth;
    uint32_t mHeight;

    uint64_t mStamp;
    uint32_t mUsage;
};

} // namespace intel
} // namespace android


#endif /* MRFLGRALLOCBUFFER_H_ */
