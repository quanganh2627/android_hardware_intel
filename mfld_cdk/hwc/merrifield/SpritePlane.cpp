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
#include <SpritePlane.h>

namespace android {
namespace intel {

static Log& log = Log::getInstance();

SpritePlane::SpritePlane(int index, int pipe)
    : mIndex(index),
      mType(PLANE_SPRITE),
      mInitialized(false),
      mTransform(PLANE_TRANSFORM_0),
      mPipe(pipe)
{
    log.v("SpritePlane");

    mPosition.x = 0;
    mPosition.y = 0;
    mPosition.w = 0;
    mPosition.h = 0;

    // initialize
    initialize();
}

SpritePlane::~SpritePlane()
{
    log.v("~SpritePlane");
}

bool SpritePlane::initialize()
{
    log.v("SpritePlane::initialize");

    // create buffer cache
    mGrallocBufferCache = new BufferCache(5);
    if (!mGrallocBufferCache) {
        LOGE("failed to create gralloc buffer cache\n");
        goto cache_err;
    }

    // init gralloc module
    hw_module_t const* module;
    if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module)) {
        LOGE("failed to get gralloc module\n");
        goto gralloc_err;
    }

    mGrallocModule = (IMG_gralloc_module_public_t*)module;

    mInitialized = true;
    return true;
gralloc_err:
    delete mGrallocBufferCache;
cache_err:
    mInitialized = false;
    return false;
}

int SpritePlane::getIndex() const
{
    log.v("SpritePlane::getIndex");

    return mIndex;
}

int SpritePlane::getType() const
{
    log.v("SpritePlane::getType");
    return mType;
}

void SpritePlane::setPosition(int x, int y, int w, int h)
{
    log.v("SpritePlane::setPosition: %d, %d - %dx%d", x, y, w, h);

    mPosition.x = x;
    mPosition.y = y;
    mPosition.w = w;
    mPosition.h = h;
}

void SpritePlane::setSourceCrop(int x, int y, int w, int h)
{
    log.v("setSourceCrop: %d, %d - %dx%d", x, y, w, h);

    if (!initCheck()) {
        log.e("SpritePlane::setSourceCrop: plane hasn't been initialized");
        return;
    }

    mSrcCrop.x = x;
    mSrcCrop.y = y;
    mSrcCrop.w = w;
    mSrcCrop.h = h;
}

void SpritePlane::setTransform(int trans)
{
    log.v("SpritePlane::setTransform: %d", trans);

    if (!initCheck()) {
        log.e("SpritePlane::setTransform: plane hasn't been initialized");
        return;
    }

    switch (trans) {
    case PLANE_TRANSFORM_90:
    case PLANE_TRANSFORM_180:
    case PLANE_TRANSFORM_270:
        mTransform = trans;
        break;
    default:
        mTransform = PLANE_TRANSFORM_0;
    }
}

bool SpritePlane::isValidTransform(uint32_t trans)
{
    log.v("SpritePlane::isValidTransform: trans %d", trans);

    if (!initCheck()) {
        log.e("SpritePlane::isValidTransform: plane hasn't been initialized");
        return false;
    }

    return trans ? false : true;
}

bool SpritePlane::isValidBuffer(uint32_t handle)
{
    log.v("SpritePlane::isValidBuffer: handle = 0x%x", handle);
    return false;
}

bool SpritePlane::isValidBlending(uint32_t blending)
{
    log.v("SpritePlane::isValidBlending: blending = 0x%x", blending);

    if (!initCheck()) {
        log.e("SpritePlane::isValidBlending: plane hasn't been initialized");
        return false;
    }

    // support premultipled & none blanding
    switch (blending) {
    case PLANE_BLENDING_NONE:
    case PLANE_BLENDING_PREMULT:
        return true;
    default:
        log.v("SpritePlane::isValidBlending: unsupported blending 0x%x",
              blending);
    }

    return false;
}

bool SpritePlane::isValidScaling(hwc_rect_t& src, hwc_rect_t& dest)
{
    int srcW, srcH;
    int dstW, dstH;

    log.v("SpritePlane::isValidScaling");

    if (!initCheck()) {
        log.e("SpritePlane::isValidScaling: plane hasn't been initialized");
        return false;
    }

    srcW = src.right - src.left;
    srcH = src.bottom - src.top;
    dstW = dest.right - dest.left;
    dstH = dest.bottom - dest.top;

    log.v("SpritePlane::isValidScaling: (%dx%d) v.s. (%dx%d)",
            srcW, srcH, dstW, dstH);
    // no scaling is supported
    return ((srcW == dstW) && (srcH == dstH)) ? true : false;
}

bool SpritePlane::setDataBuffer(uint32_t handle)
{
    log.v("SpritePlane::setDataBuffer: handle = 0x%x", handle);
    return false;
}

void SpritePlane::invalidateBufferCache()
{
    log.v("SpritePlane::invalidateBufferCache");

    if (!initCheck()) {
        log.e("SpritePlane:invalidateBufferCache: plane hasn't been initialized");
        return;
    }

    mGrallocBufferCache->clear();
}

bool SpritePlane::assignToPipe(uint32_t pipe)
{
    log.v("SpritePlane::assignToPipe: pipe = %d", pipe);
    return false;
}

void SpritePlane::setZOrderConfig(ZOrderConfig& config)
{
    log.v("SpritePlane::assignToPipe");
}

bool SpritePlane::reset()
{
    log.v("SpritePlane::reset");
    return false;
}

bool SpritePlane::flip()
{
    log.v("SpritePlane::flip");
    return false;
}

bool SpritePlane::enable()
{
    log.v("SpritePlane::enable");
    return false;
}

bool SpritePlane::disable()
{
    log.v("SpritePlane::disable");
    return false;
}

void* SpritePlane::getContext() const
{
    log.v("SpritePlane::getContext");
    return 0;
}

//------------------------------------------------------------------------------
SpritePlane::BufferCache::BufferCache(int size)
{
    mBufferPool.setCapacity(size);
    mBufferPool.clear();
}

SpritePlane::BufferCache::~BufferCache()
{

}

bool SpritePlane::BufferCache::addMapper(uint64_t handle,
                                              IBufferMapper* mapper)
{
    ssize_t index = mBufferPool.indexOfKey(handle);
    if (index >= 0) {
        log.e("addMapper: buffer 0x%llx exists\n", handle);
        return false;
    }

    // add mapper
    mBufferPool.add(handle, mapper);
    return true;
}

IBufferMapper* SpritePlane::BufferCache::getMapper(uint64_t handle)
{
    ssize_t index = mBufferPool.indexOfKey(handle);
    if (index < 0)
        return 0;
    return mBufferPool.valueAt(index);
}

void SpritePlane::BufferCache::clear()
{
    for (size_t i = 0; i <mBufferPool.size(); i++) {
        IBufferMapper* mapper = mBufferPool.valueAt(i);
        if (mapper)
            mapper->unmap();
        delete mapper;
    }

    // clear buffer pool
    mBufferPool.clear();
}

} // namespace intel
} // namespace android
