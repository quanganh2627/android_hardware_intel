/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_LIBCAMERA_JPEG_COMPRESSOR_H
#define ANDROID_LIBCAMERA_JPEG_COMPRESSOR_H

#include <stdio.h>
#include "CameraCommon.h"
#include <utils/Errors.h>

namespace android {
class SWJpegEncoder;

class JpegCompressor {
public:
    JpegCompressor();
    ~JpegCompressor();


    struct InputBuffer {
        unsigned char *buf;
        int width;
        int height;
        int format;
        int size;
        int stride;
        int alignHeight;

        void clear()
        {
            buf = NULL;
            width = 0;
            height = 0;
            format = 0;
            size = 0;
        }
    };

    struct OutputBuffer {
        unsigned char *buf;
        int width;
        int height;
        int size;
        int quality;

        void clear()
        {
            buf = NULL;
            width = 0;
            height = 0;
            size = 0;
            quality = 0;
        }
    };

    // Encoder functions
    int encode(const InputBuffer &in, const OutputBuffer &out);
private:
    int mJpegSize;
    unsigned char* midbuf;
    SWJpegEncoder *mSWEncoder;
    int swEncode(const InputBuffer &in, const OutputBuffer &out);

};

}; // namespace android

#endif // ANDROID_LIBCAMERA_JPEG_COMPRESSOR_H
