/*
 * Copyright (C) 2012 The Android Open Source Project
 * Copyright (c) 2012 Intel Corporation. All Rights Reserved.
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
 *\file SWJpegEncoder.h
 *
 * Abstracts the SW jpeg encoder
 *
 * This class calls the libjpeg ditectly. And libskia's performance is poor.
 * The SW jpeg encoder is used for the thumbnail encoding mainly.
 * But When the HW jpeg encoding fails, it will use the SW jpeg encoder also.
 *
 */

#ifndef ANDROID_LIBCAMERA_SW_JPEG_ENCODER_H
#define ANDROID_LIBCAMERA_SW_JPEG_ENCODER_H

#include <stdio.h>
//#include "AtomCommon.h"
#include "CameraCommon.h"
#include <utils/Errors.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "jpeglib.h"
#ifdef __cplusplus
}
#endif
#define CLIP(Number, Max, Min)    ((Number) > (Max) ? (Max) : ((Number) < (Min) ? (Min) : (Number)))

namespace android {

/**
 * \class SWJpegEncoder
 *
 * This class is used for sw jpeg encoder.
 * It will call the libjpeg directly.
 * It just support NV12 input currently.
 */
class SWJpegEncoder {
public:
    SWJpegEncoder();
    ~SWJpegEncoder();

    void init(void);
    void deInit(void);
    void setJpegQuality(int quality);
    int configEncoding(int width, int height, void *jpegBuf, int jpegBufSize);
    int doJpegEncoding(const void* yuv_buf, int format);
    void getJpegSize(int *jpegSize);

// prevent copy constructor and assignment operator
private:
    SWJpegEncoder(const SWJpegEncoder& other);
    SWJpegEncoder& operator=(const SWJpegEncoder& other);

private:
    typedef struct {
        struct jpeg_destination_mgr pub;
        JSAMPLE *outJpegBuf;  /*!< jpeg output buffer */
        int outJpegBufSize;  /*!< jpeg output buffer size */
        int codedSize;  /*!< the final encoded out jpeg size */
        bool encodeSuccess;  /*!< if buffer overflow, it will be set to false */
    } JpegDestMgr, *JpegDestMgrPtr;

    struct jpeg_compress_struct mCInfo;
    struct jpeg_error_mgr mJErr;
    int mJpegQuality;
    static const unsigned int mSupportedFormat = JCS_YCbCr;
    static const int mDefaultJpegQuality = 90;

    int setupJpegDestMgr(j_compress_ptr cInfo, JSAMPLE *jpegBuf, int jpegBufSize);
    // the below three functions are for the dest buffer manager.
    static void initDestination(j_compress_ptr cInfo);
    static boolean emptyOutputBuffer(j_compress_ptr cInfo);
    static void termDestination(j_compress_ptr cInfo);
};

}; // namespace android

#endif /* ANDROID_LIBCAMERA_SW_JPEG_ENCODER_H */
