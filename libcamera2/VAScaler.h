/*
 * Copyright (c) 2007-2013 Intel Corporation. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/**
 *\file VAScaler.h
 * TBF
 */
#ifndef VA_SCALER_H_
#define VA_SCALER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <utils/KeyedVector.h>
#include <ui/GraphicBuffer.h>
#include "IHWScaler.h"
#include "VideoVPPBase.h"
#include "AtomCommon.h"

#define MAX_NUM_BUFFER_STORE 32
#define BufferID int
#define NO_ZOOM  1.0

#define HAL_PIXEL_FORMAT_NV12 HAL_PIXEL_FORMAT_NV12_LINEAR_PACKED_INTEL

namespace android {

class VAScaler :
    public IHWScaler {
// constructor destructor
public:
    VAScaler();
    ~VAScaler();

// public methods
public:
    void setZoomFactor(float zf);
    int  processFrame(int inputBufferId, int outputBufferId);
    int  addOutputBuffer(buffer_handle_t *pBufHandle, int width, int height, int stride, int format);
    int  addInputBuffer(buffer_handle_t *pBufHandle, int width, int height, int stride, int format);
    void removeInputBuffer(int bufferId);
    void removeOutputBuffer(int bufferId);

// prevent copy constructor and assignment operator
private:
    VAScaler(const VAScaler& other);
    VAScaler& operator=(const VAScaler& other);

// private methods
private:
    status_t init();
    status_t deInit();
    status_t mapGraphicFmtToVAFmt(int &vaRTFormat, int &vaFourcc, int graphicFormat);
    void     setZoomRegion (VARectangle &region, int w, int h, float zoom);
    void     clearBuffers(KeyedVector<BufferID , RenderTarget *> &buffers);

// private data
private:
    bool    mInitialized;
    VideoVPPBase *mVA;
    VPParameters *mVPP;
    KeyedVector<BufferID , RenderTarget *> mIBuffers;
    KeyedVector<BufferID , RenderTarget *> mOBuffers;
    BufferID mIIDKey;
    BufferID mOIDKey;
    float   mZoomFactor;
}; // class vaImgScaler

}; // namespace android

#endif //VA_SCALER_H_
