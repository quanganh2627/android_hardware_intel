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
#ifndef SPRITEPLANE_H_
#define SPRITEPLANE_H_

#include <utils/KeyedVector.h>

#include <IDisplayPlane.h>

namespace android {
namespace intel {

class SpritePlane : public IDisplayPlane {
protected:
    enum {
        PLANE_FORMAT_BGRX565  = 0x14000000UL,
        PLANE_PIXEL_FORMAT_BGRX8888 = 0x18000000UL,
        PLANE_PIXEL_FORMAT_BGRA8888 = 0x1c000000UL,
        PLANE_PIXEL_FORMAT_RGBX8888 = 0x38000000UL,
        PALEN_PIXEL_FORMAT_RGBA8888 = 0x3c000000UL,
    };
public:
    SpritePlane(int index, int pipe);
    virtual ~SpritePlane();
public:
    virtual int getIndex() const;
    virtual int getType() const;
    virtual bool initCheck() const { return mInitialized; }

    // data destination
    virtual void setPosition(int x, int y, int w, int h);
    virtual void setSourceCrop(int x, int y, int w, int h);
    virtual void setTransform(int transform);

    // data source
    virtual bool isValidBuffer(uint32_t handle);
    virtual bool isValidTransform(uint32_t trans);
    virtual bool isValidBlending(uint32_t blending);
    virtual bool isValidScaling(hwc_rect_t& src, hwc_rect_t& dest);
    virtual bool setDataBuffer(uint32_t handle);
    virtual void invalidateBufferCache();

    // display device
    virtual bool assignToPipe(uint32_t pipe);

    virtual void setZOrderConfig(ZOrderConfig& config);

    // hardware operations
    virtual bool reset();
    virtual bool flip();
    virtual bool enable();
    virtual bool disable();

    virtual void* getContext() const;
protected:
    virtual bool initialize();
protected:
    class BufferCache : public IBufferCache {
    public:
        BufferCache(int size);
        virtual ~BufferCache();
        // add a new mapper into buffer cache
        virtual bool addMapper(uint64_t handle, IBufferMapper* mapper);
        // get a buffer mapper
        virtual IBufferMapper* getMapper(uint64_t handle);

        virtual void clear();
    private:
        android::KeyedVector<uint64_t, IBufferMapper*> mBufferPool;
    };
protected:
    int mIndex;
    int mType;
    bool mInitialized;
    // gralloc data buffer cache
    BufferCache *mGrallocBufferCache;
    IMG_gralloc_module_public_t *mGrallocModule;
    PlanePosition mPosition;
    crop_t mSrcCrop;
    int mTransform;
    int mPipe;
};

} // namespace intel
} // namespace android

#endif /* SPRITEPLANE_H_ */
