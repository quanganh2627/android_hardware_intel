/*
* Copyright (c) 2009-2011 Intel Corporation.  All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#ifndef JPEGDEC_H
#define JPEGDEC_H

#include <utils/KeyedVector.h>
#include <utils/threads.h>
#include <hardware/gralloc.h>
#include "JPEGCommon.h"
#include <va/va.h>
#include <va/va_drmcommon.h>
#include <va/va_vpp.h>
#include <va/va_android.h>
#include <va/va_tpi.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

using namespace android;

struct CJPEGParse;
class JpegBitstreamParser;
class JpegBlitter;
typedef void* BlitEvent;

extern int generateHandle();

// Non thread-safe
class JpegDecoder
{
friend class JpegBlitter;
public:
    typedef unsigned long MapHandle;
    JpegDecoder(VADisplay display = NULL, VAConfigID vpCfgId = VA_INVALID_ID, VAContextID vpCtxId = VA_INVALID_ID, bool use_blitter = false);
    virtual ~JpegDecoder();
    static JpegDecodeStatus preInit(VADisplay display);
    virtual JpegDecodeStatus init(int width, int height, RenderTarget **targets, int num);
    virtual void deinit();
    virtual JpegDecodeStatus parse(JpegInfo &jpginfo);
    virtual JpegDecodeStatus decode(JpegInfo &jpginfo, const RenderTarget &target);
    virtual JpegDecodeStatus sync(RenderTarget &target);
    virtual bool busy(RenderTarget &target) const;
    virtual JpegDecodeStatus blit(RenderTarget &src, RenderTarget &dst, int scale_factor);
    virtual JpegDecodeStatus getRgbaTile(RenderTarget &src,
                                         uint8_t *sysmem,
                                         int left, int top, int width, int height, int scale_factor);
    virtual JpegDecodeStatus blitToLinearRgba(RenderTarget &src,
                                              uint8_t *sysmem,
                                              uint32_t width, uint32_t height,
                                              BlitEvent &event, int scale_factor);
    virtual JpegDecodeStatus blitToCameraSurfaces(RenderTarget &src,
                                                   buffer_handle_t dst_nv12,
                                                   buffer_handle_t dst_yuy2,
                                                   uint8_t *dst_nv21,
                                                   uint8_t *dst_yv12,
                                                   uint32_t width, uint32_t height,
                                                   BlitEvent &event);
    virtual void syncBlit(BlitEvent &event);
    virtual MapHandle mapData(RenderTarget &target, void ** data, uint32_t * offsets, uint32_t * pitches);
    virtual void unmapData(RenderTarget &target, MapHandle maphandle);
    virtual VASurfaceID getSurfaceID(const RenderTarget &target) const;
    virtual JpegDecodeStatus createSurfaceFromRenderTarget(const RenderTarget &target, VASurfaceID *surf_id);
    virtual JpegDecodeStatus destroySurface(RenderTarget &target);
    virtual JpegDecodeStatus destroySurface(VASurfaceID surf_id);
protected:
    bool mInitialized;
    mutable Mutex mLock;
    VADisplay mDisplay;
    VAConfigID mConfigId;
    VAContextID mContextId;
    CJPEGParse *mParser;
    JpegBitstreamParser *mBsParser;
    JpegBlitter *mBlitter;
    bool mDispCreated;
    KeyedVector<buffer_handle_t, VASurfaceID> mGrallocSurfaceMap;
    KeyedVector<unsigned long, VASurfaceID> mDrmSurfaceMap;
    KeyedVector<int, VASurfaceID> mNormalSurfaceMap;
    KeyedVector<int, VASurfaceID> mUserptrSurfaceMap;
    virtual JpegDecodeStatus parseHeader(JpegInfo &jpginfo);
    virtual JpegDecodeStatus parseTableData(JpegInfo &jpginfo);
    virtual bool jpegColorFormatSupported(JpegInfo &jpginfo) const;
    virtual JpegDecodeStatus createSurfaceInternal(int width, int height, uint32_t fourcc, int handle, VASurfaceID *surf_id);
    virtual JpegDecodeStatus createSurfaceUserptr(int width, int height, uint32_t fourcc, uint8_t* ptr, VASurfaceID *surf_id);
    virtual JpegDecodeStatus createSurfaceDrm(int width, int height, uint32_t fourcc, unsigned long boname, int stride, VASurfaceID *surf_id);
    virtual JpegDecodeStatus createSurfaceGralloc(int width, int height, uint32_t fourcc, buffer_handle_t handle, int stride, VASurfaceID *surf_id);
};


#endif

