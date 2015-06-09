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

#ifndef JPEG_BLITTER_H
#define JPEG_BLITTER_H

#include "JPEGCommon.h"
#include <utils/threads.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <hardware/gralloc.h>

using namespace android;

class JpegDecoder;
typedef void* BlitEvent;

class JpegBlitter
{
public:
    JpegBlitter(VADisplay display, VAConfigID vpCfgId, VAContextID vpCtxId);
    virtual ~JpegBlitter();
    static JpegDecodeStatus preInit(VADisplay display);
    virtual void init(JpegDecoder &dec);
    virtual void deinit();
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
private:
    mutable Mutex mLock;
    JpegDecoder *mDecoder;
    VADisplay mDisplay;
    VAConfigID mConfigId;
    VAContextID mContextId;
    void *mPrivate;
    bool mInitialized;
};

#endif
