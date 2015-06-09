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
#include <TTMBufferMapper.h>

namespace android {
namespace intel {

static Log& log = Log::getInstance();
TTMBufferMapper::TTMBufferMapper(IntelWsbm& wsbm, IDataBuffer& buffer)
    : mRefCount(0),
      mWsbm(wsbm),
      mBuffer(buffer),
      mBufferObject(0),
      mGttOffsetInPage(0),
      mCpuAddress(0),
      mSize(0)
{
    log.v("TTMBufferMapper::TTMBufferMapper");
}

TTMBufferMapper::~TTMBufferMapper()
{
    log.v("TTMBufferMapper::~TTMBufferMapper");
    // delete buffer
    delete &mBuffer;
}

bool TTMBufferMapper::map()
{
    void *wsbmBufferObject = 0;
    uint32_t handle;
    void *virtAddr;
    uint32_t gttOffsetInPage;

    log.v("TTMBufferMapper::map");

    handle = mBuffer.getHandle();

    bool ret = mWsbm.wrapTTMBuffer(handle, &wsbmBufferObject);
    if (ret == false) {
        log.e("TTMBufferMapper::map: failed to map TTM buffer");
        return false;
    }

    // TODO: review this later
    ret = mWsbm.waitIdleTTMBuffer(wsbmBufferObject);
    if (ret == false) {
        log.e("TTMBufferMapper::map: wait ttm buffer idle failed");
        return false;
    }

    virtAddr = mWsbm.getCPUAddress(wsbmBufferObject);
    gttOffsetInPage = mWsbm.getGttOffset(wsbmBufferObject);

    if (!gttOffsetInPage || !virtAddr) {
        log.w("TTMBufferMapper::map: %x Virtual addr: %p.",
              gttOffsetInPage, virtAddr);
        return false;
    }

    // update parameters
    mBufferObject = wsbmBufferObject;
    mGttOffsetInPage = gttOffsetInPage;
    mCpuAddress = virtAddr;
    mSize = 0;
    return true;
}

bool TTMBufferMapper::unmap()
{
    log.v("TTMBufferMapper::unmap");

    if (!mBufferObject)
        return false;

    mWsbm.unreferenceTTMBuffer(mBufferObject);

    mGttOffsetInPage = 0;
    mCpuAddress = 0;
    mSize = 0;
    return true;
}

int TTMBufferMapper::incRef()
{
    mRefCount++;

    return mRefCount;
}

int TTMBufferMapper::decRef()
{
    mRefCount--;
    return mRefCount;
}

stride_t& TTMBufferMapper::getStride () const
{
    return mBuffer.getStride();
}

uint32_t TTMBufferMapper::getWidth() const
{
    return mBuffer.getWidth();
}

uint32_t TTMBufferMapper::getHeight() const
{
    return mBuffer.getHeight();
}

crop_t& TTMBufferMapper::getCrop() const
{
    return mBuffer.getCrop();
}

uint32_t TTMBufferMapper::getFormat() const
{
    return mBuffer.getFormat();
}

bool TTMBufferMapper::waitIdle()
{
    return mWsbm.waitIdleTTMBuffer(mBufferObject);
}

} // namespace intel
} // namespace android


