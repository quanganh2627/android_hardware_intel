/*
 * Copyright (C) 2012 The Android Open Source Project
 * Copyright (c) 2012 Intel Corporation. All Rights Reserved.
 *
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

/**
 *\file JpegHwEncoder.h
 *
 * Abstracts the HW accelerated JPEG encoder
 *
 * It allow synchronous and asynchronous encoding
 * It interfaces with imageencoder in libmix to access the HW that encoder in JPEG
 *
 *
 */

#ifndef JPEGHWENCODER_H_
#define JPEGHWENCODER_H_

#include "AtomCommon.h"

#ifdef USE_INTEL_JPEG
#include <ImageEncoder.h>
#endif

namespace android {


/**
 * \class JpegHwEncoder
 *
 * This class offers HW accelerated JPEG encoding.
 * Since actual encoding is done in HW it offers synchronous and
 * asynchronous interfaces to the encoding
 */
class JpegHwEncoder {
public:
    struct InputBuffer {
        unsigned char *buf;
        int width;
        int height;
        int bpl;   //stride
        int fourcc;
        int size;

        void clear()
        {
            buf = NULL;
            width = 0;
            height = 0;
            bpl = 0;
            fourcc = 0;
            size = 0;
        }
    };

    struct OutputBuffer {
        unsigned char *buf;
        int width;
        int height;
        unsigned int size;
        int quality;
        int length;     /*>! amount of the data actually written to the buffer. Always smaller than size field*/

        void clear()
        {
            buf = NULL;
            width = 0;
            height = 0;
            size = 0;
            quality = 0;
            length = 0;
        }
    };
// prevent copy constructor and assignment operator
private:
    JpegHwEncoder(const JpegHwEncoder& other);
    JpegHwEncoder& operator=(const JpegHwEncoder& other);

#ifdef USE_INTEL_JPEG
public:
    JpegHwEncoder();
    virtual ~JpegHwEncoder();

    int init(void);
    int deInit(void);
    bool isInitialized() {return mHWInitialized;};
    status_t setInputBuffers(AtomBuffer* inputBuffersArray, int inputBuffersNum);
    int encode(const InputBuffer &in, OutputBuffer &out);
    /* Async encode */
    int encodeAsync(const InputBuffer &in, OutputBuffer &out, int &mMaxCodedSize);
    int getOutputSize(unsigned int& outSize);
    int getOutput(void* outBuf, unsigned int& outSize);

private:
    int V4L2Fmt2VAFmt(unsigned int v4l2Fmt, unsigned int &vaFmt);
    int resetContext(const InputBuffer &in, int &imgSeq);
    int restoreContext();

private:
    /**
      * \define
      * size of the JPEG encoder should be aligned in width
      */
    #define SIZE_OF_WIDTH_ALIGNMENT 64
    /**
      * \define ERROR_POINTER_NOT_FOUND
      * \brief default value used to detect that a buffer address does not have an
      * \assigned VA Surface
      */
    #define ERROR_POINTER_NOT_FOUND 0x1EADBEEF

    IntelImageEncoder* mHwImageEncoder;
    DefaultKeyedVector<void*, int> mBuffer2SurfaceId;      /*!> Vector where to store the buffer and surface id */

    // If the picture dimension is <= the below w x h
    // We should use the software jpeg encoder
    static const int MIN_HW_ENCODING_WIDTH = 640;
    static const int MIN_HW_ENCODING_HEIGHT = 480;

    bool            mHWInitialized;
    bool            mContextRestoreNeeded;       /*!< flags the need for a context restore */
    int firstImageSeq;      /*!< record the first image seq for multi buffer*/
    int singelSeq;          /*!< record the image seq for the singel buffer*/
    unsigned int mMaxOutJpegBufSize; /*!< the max JPEG Buffer Size. This is initialized to
                                      the size of the input YUV buffer*/
#else  //USE_INTEL_JPEG
//Stub implementation if HW encoder is disabled
public:
    JpegHwEncoder(){};
    virtual ~JpegHwEncoder(){};

    int init(void){return -1;};
    int deInit(void){return -1;};
    bool isInitialized() {return false;};
    int setInputBuffers(AtomBuffer* inputBuffersArray, int inputBuffersNum){return -1;};
    int encode(const InputBuffer &in, OutputBuffer &out){return -1;};
    int encodeAsync(const InputBuffer &in, OutputBuffer &out, int &mMaxCodedSize){return -1;};
    int getOutputSize(unsigned int& outSize) {return -1;};
    int getOutput(void* outBuf, unsigned int& outSize){return -1;};
#endif
};
}; // namespace android

#endif /* JPEGHWENCODER_H_ */
