/*
* Copyright (c) 2014 Intel Corporation.  All rights reserved.
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
/*
 * Copyright 2007 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SKIMAGEDECODER_LIBMIX_H
#define SKIMAGEDECODER_LIBMIX_H

#include "SkImageDecoder_libjpeg_turbo.h"
#include <utils/Log.h>
#include <hardware/gralloc.h>
#include <utils/threads.h>
#include <utils/Timers.h>
#include <cutils/properties.h>
#include <utils/Vector.h>
#include <pthread.h>

class JpegDecoder;
class JpegBlitter;

class SkJPEGMixImageDecoder : public SkJPEGTurboImageDecoder {
public:
    SkJPEGMixImageDecoder(): mDecoder(NULL) {}
    virtual ~SkJPEGMixImageDecoder() {}
    virtual Format getFormat() const {
        return kJPEG_Format;
    }

protected:
    virtual bool onBuildTileIndex(SkStreamRewindable *stream, int *width, int *height) SK_OVERRIDE;
    virtual bool onDecodeSubset(SkBitmap* bitmap, const SkIRect& rect) SK_OVERRIDE;
    virtual bool onDecode(SkStream* stream, SkBitmap* bm, Mode) SK_OVERRIDE;

private:
    JpegDecoder *mDecoder;
};

class SkJPEGMixImageEncoder : public SkJPEGTurboImageEncoder {
protected:
    virtual bool onEncode(SkWStream* stream, const SkBitmap& bm, int quality);
};


#endif
