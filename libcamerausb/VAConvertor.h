/*
* Copyright (c) 2007-2013 Intel Corporation. All Rights Reserved:
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
*\file VAConvert.h
* TBF
*/
#ifndef VA_CONVERT_H_
#define VA_CONVERT_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <utils/KeyedVector.h>
#include <ui/FramebufferNativeWindow.h>
#include <ui/GraphicBuffer.h>

#include <system/graphics.h>
#include <unistd.h>
#include <va/va.h>
#include <va/va_drmcommon.h>
#include <va/va_vpp.h>
#include <va/va_android.h>
#include <va/va_tpi.h>
#include <ufo/graphics.h>

#include <intel_bufmgr.h>
#include <i915_drm.h>
#include <drm_fourcc.h>
#include "CameraCommon.h"


#define MAX_NUM_BUFFER_STORE 32
#define BufferID int
#define NO_ZOOM  1.0


namespace android {

class VAConvertor{
// constructor destructor
public:
    VAConvertor();
    ~VAConvertor();

// public methods
public:
    status_t stop();
    int  processFrame(int inputBufferId, int outputBufferId);
    int  addInputBuffer(buffer_handle_t pBufHandle, int width, int height, int format);
    int  addOutputBuffer(buffer_handle_t pBufHandle, int width, int height, int format);
    void removeInputBuffer(int bufferId);
    void removeOutputBuffer(int bufferId);
    status_t VPPColorConverter(buffer_handle_t input_handle,buffer_handle_t output_handle,int mInWidth,int mInHeight,int mInputFormat,int mOutWidth,int mOutHeight,int mOutputFormat);
    status_t VPPBitBlit(RenderTarget *in,RenderTarget *out);
    status_t ConfigBuffer(RenderTarget *rt,buffer_handle_t pBufHandle, int width, int height, int format);
    status_t mapGraphicFmtToVAFmt(int &vaRTFormat, int &vaFourcc, int graphicFormat);
    status_t mapV4L2FmtToVAFmt(int &vaRTFormat, int &vaFourcc, int graphicFormat);

// private methods
private:
    status_t init();
    status_t deInit();

    void  clearBuffers(KeyedVector<BufferID , RenderTarget *> &buffers);
// private methods
private:
    bool  mInitialized;
    VideoVPPBase *mVA;
    VPParameters *mVPP;
    KeyedVector<BufferID , RenderTarget *> mIBuffers;
    KeyedVector<BufferID , RenderTarget *> mOBuffers;
    BufferID mIIDKey;
    BufferID mOIDKey;
    int mInWidth;
    int mInHeight;
    int mOutWidth;
    int mOutHeight;
    int mInFormat;
    int mOutFormat;
}; // class vaImgScaler

}; // namespace android

#endif //VA_CONVERT_H_

