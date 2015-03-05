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
#ifndef __OVERLAY_PLANE_H__
#define __OVERLAY_PLANE_H__

#include <utils/KeyedVector.h>

#include <OverlayHW.h>
#include <IDisplayPlane.h>
#include <IBufferMapper.h>
#include <IntelWsbm.h>

namespace android {
namespace intel {

typedef struct {
    OverlayBackBufferBlk *buf;
    uint32_t gttOffsetInPage;
    uint32_t bufObject;
} OverlayBackBuffer;

class OverlayPlane : public IDisplayPlane {
    // flush flags
    enum {
        FLUSH_NEEDED     = 0x00000001UL,
        WAIT_VBLANK      = 0x00000002UL,
        UPDATE_COEF      = 0x00000004UL,
        BOB_DEINTERLACE  = 0x00000008UL,
        DELAY_DISABLE    = 0x00000010UL,
        WMS_NEEDED       = 0x00000020UL,
    };
public:
    OverlayPlane(int index, int pipe);
    ~OverlayPlane();

    int getIndex() const;
    int getType() const;

    virtual bool initialize();
    virtual bool initCheck() const { return mInitialized; }

    // plane context operations
    virtual void setPosition(int x, int y, int w, int h);
    virtual void setSourceCrop(int x, int y, int w, int h);
    virtual void setTransform(int transform);

    virtual bool isValidBuffer(uint32_t handle);
    virtual bool isValidTransform(uint32_t trans);
    virtual bool isValidBlending(uint32_t blending);
    virtual bool isValidScaling(hwc_rect_t& src, hwc_rect_t& dest);
    virtual bool setDataBuffer(uint32_t handle);
    virtual void invalidateBufferCache();

    virtual bool assignToPipe(uint32_t pipe);

    virtual void setZOrderConfig(ZOrderConfig& config);

    // plane operations
    virtual bool reset();
    virtual bool flip();
    virtual bool enable();
    virtual bool disable();

    virtual void* getContext() const;

protected:
    // generic overlay register flush
    virtual bool flush(uint32_t flags);
    virtual bool setDataBuffer(IBufferMapper& mapper);
    virtual bool bufferOffsetSetup(IBufferMapper& mapper);
    virtual uint32_t calculateSWidthSW(uint32_t offset, uint32_t width);
    virtual bool coordinateSetup(IBufferMapper& mapper);
    virtual bool setCoeffRegs(double *coeff, int mantSize,
                                 coeffPtr pCoeff, int pos);
    virtual void updateCoeff(int taps, double fCutoff,
                                bool isHoriz, bool isY,
                                coeffPtr pCoeff);
    virtual bool scalingSetup(IBufferMapper& mapper);
    virtual void checkPosition(int& x, int& y, int& w, int& h);
protected:
    // back buffer operations
    virtual OverlayBackBuffer* createBackBuffer();
    virtual void deleteBackBuffer();
    virtual void resetBackBuffer();
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
    // TTM data buffer cache
    BufferCache *mTTMBufferCache;
    // overlay back buffer
    OverlayBackBuffer *mBackBuffer;
    // overlay Gralloc buffer
    IMG_gralloc_module_public_t *mGrallocModule;
    // wsbm
    IntelWsbm *mWsbm;
    // plane position
    PlanePosition mPosition;
    crop_t mSrcCrop;
    // plane transform
    int mTransform;
    // pipe
    uint32_t mPipe;
};

} // namespace intel
} // namespace android

#endif /* __OVERLAY_PLANE_H__ */
