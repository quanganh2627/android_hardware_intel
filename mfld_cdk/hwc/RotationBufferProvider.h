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
 *    Li Zeng <li.zeng@intel.com>
 *    Jian Sun <jianx.sun@intel.com>
 */

#ifndef __ROTATIONO_BUFFER_PROVIDER_H__
#define __ROTATIONO_BUFFER_PROVIDER_H__

#include <va/va.h>
#include <sys/time.h>
#include <va/va_tpi.h>
#include <va/va_vpp.h>
#include "IntelWsbm.h"
#include <utils/Timers.h>
#include <va/va_android.h>
#include "IntelBufferManager.h"

#define Display unsigned int
typedef void* VADisplay;
typedef int VAStatus;

class RotationBufferProvider {

public:
    RotationBufferProvider(IntelWsbm* wsbm);
    ~RotationBufferProvider();

    bool initialize();
    void deinitialize();
    bool setupRotationBuffer(intel_gralloc_payload_t *payload, int transform);

private:
    bool startVA(intel_gralloc_payload_t *payload, int transform);
    void stopVA();
    bool isContextChanged(int width, int height, int transform);
    int transFromHalToVa(int transform);
    uint32_t createWsbmBuffer(int width, int height, void **buf);
    int getStride(bool isTarget, int width);
    bool createVaSurface(intel_gralloc_payload_t *payload, int transform, bool isTarget);
    void freeVaSurfaces();
    inline uint32_t getMilliseconds();

private:
    enum {
        MAX_SURFACE_NUM = 4
    };

    IntelWsbm* mWsbm;
    bool mVaInitialized;
    VADisplay mVaDpy;
    VAConfigID mVaCfg;
    VAContextID mVaCtx;
    VABufferID mVaBufFilter;
    VASurfaceID mSourceSurface;
    Display mDisplay;

    // rotation config variables
    int mWidth;
    int mHeight;
    int mTransform;

    int mRotatedWidth;
    int mRotatedHeight;
    int mRotatedStride;

    int mTargetIndex;
    int mKhandles[MAX_SURFACE_NUM];
    VASurfaceID mRotatedSurfaces[MAX_SURFACE_NUM];
    void *mDrmBuf[MAX_SURFACE_NUM];
};

#endif
