/*
 * Copyright (C) 2012 The Android Open Source Project
 * Copyright (c) 2012-2014 Intel Corporation
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
#include "AtomCommon.h"
#include <utils/Errors.h>
#include <utils/Vector.h>
#include <utils/threads.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "jpeglib.h"
#ifdef __cplusplus
}
#endif

namespace android {
/**
 * \class SWJpegEncoder
 *
 * This class is used for sw jpeg encoder.
 * It will use single or multi thread to do the sw jpeg encoding
 * It just support NV12 input currently.
 */
class SWJpegEncoder {
public:
    SWJpegEncoder();
    ~SWJpegEncoder();

    struct InputBuffer {
        unsigned char *buf;
        int width;
        int height;
        int fourcc;
        int size;

        void clear()
        {
            buf = NULL;
            width = 0;
            height = 0;
            fourcc = 0;
            size = 0;
        }
    };

    struct OutputBuffer {
        unsigned char *buf;
        int width;
        int height;
        int size;
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

    // Encoder functions
    int encode(const InputBuffer &in, const OutputBuffer &out);

// prevent copy constructor and assignment operator
private:
    SWJpegEncoder(const SWJpegEncoder& other);
    SWJpegEncoder& operator=(const SWJpegEncoder& other);

private:
    int mJpegSize;  /*!< it's used to store jpeg size */

    bool isNeedMultiThreadEncoding(int width, int height);
    int swEncode(const InputBuffer &in, const OutputBuffer &out);
    int swEncodeMultiThread(const InputBuffer &in, const OutputBuffer &out);

    int mTotalWidth;  /*!< the final jpeg width */
    int mTotalHeight;  /*!< the final jpeg height */
    unsigned char *mDstBuf;  /*!< the dest buffer to store the final jpeg */
    unsigned int mCPUCoresNum;  /*!< use to remember the CPU Cores number */

private:
    /**
     * \class CodecWorkerThread
     *
     * This class will create one thread to do one sw jpeg encoder.
     * It will call the SWJpegEncoderWrapper directly.
     */
    class CodecWorkerThread : private Thread, public virtual RefBase {
    public:
        struct CodecConfig {
            // input buffer configuration
            int width;
            int height;
            int fourcc;
            void *inBufY;
            void *inBufUV;
            // output buffer configuration
            int quality;
            void *outBuf;
            int outBufSize;
        };

        CodecWorkerThread();
        ~CodecWorkerThread();

        void setConfig(CodecConfig &cfg) {mCfg = cfg;}
        void getConfig(CodecConfig *cfg){*cfg = mCfg;}
        status_t runThread(const char *name);
        void waitThreadFinish(void);
        int getJpegDataSize(void);
    private:
        int mDataSize;  /*!< the jpeg data size in one thread */
        CodecConfig mCfg;  /*!< the cfg in one thread */
    private:
        virtual bool threadLoop();
        int swEncode(void);
    };

private:
    void init(unsigned int threadNum = 1);
    void deInit(void);
    void config(const InputBuffer &in, const OutputBuffer &out);
    int doJpegEncodingMultiThread(void);
    int mergeJpeg(void);

    Vector<sp<CodecWorkerThread> > mSwJpegEncoder;
    static const unsigned int MAX_THREAD_NUM = 8; /*!< the same as max jpeg restart time */
    static const unsigned int MIN_THREAD_NUM = 1;

    /*!< it's used to use one buffer to merge the multi jpeg data to one jpeg data */
    static const unsigned int DEST_BUF_OFFSET = 1024;

private:
    /**
     * \class Codec
     *
     * This class is used for sw jpeg encoder.
     * It will call the libjpeg directly.
     * It just support NV12 input currently.
     */
    class Codec {
    public:
        Codec(int quality);
        ~Codec();

        void init(void);
        void deInit(void);
        void setJpegQuality(int quality);
        int configEncoding(int width, int height, void *jpegBuf, int jpegBufSize);
        /*
            if fourcc is V4L2_PIX_FMT_NV12, y_buf and uv_buf must be passed
            if fourcc is V4L2_PIX_FMT_YUYV, y_buf must be passed, uv_buf could be NULL
        */
        int doJpegEncoding(const void* y_buf, const void* uv_buf = NULL, int fourcc = V4L2_PIX_FMT_NV12);
        void getJpegSize(int *jpegSize);

    // prevent copy constructor and assignment operator
    private:
        Codec(const Codec& other);
        Codec& operator=(const Codec& other);

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
        static const unsigned int SUPPORTED_FORMAT = JCS_YCbCr;

        int setupJpegDestMgr(j_compress_ptr cInfo, JSAMPLE *jpegBuf, int jpegBufSize);
        // the below three functions are for the dest buffer manager.
        static void initDestination(j_compress_ptr cInfo);
        static boolean emptyOutputBuffer(j_compress_ptr cInfo);
        static void termDestination(j_compress_ptr cInfo);
    };
};
}; // namespace android

#endif /* ANDROID_LIBCAMERA_SW_JPEG_ENCODER_H */
