/*
 * Copyright (C) 2011 The Android Open Source Project
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
#define LOG_TAG "Camera_JpegCompressor"

#define JPEG_BLOCK_SIZE 4096

#include "JpegCompressor.h"
#include "ColorConverter.h"
#include "SWJpegEncoder.h"
#include "LogHelper.h"
#include <string.h>

namespace android {
JpegCompressor::JpegCompressor() :
    mSWEncoder(NULL)
{
    LOG1("@%s", __FUNCTION__);

    mSWEncoder = new SWJpegEncoder();
    mJpegSize = -1;
}

JpegCompressor::~JpegCompressor()
{
    LOG1("@%s", __FUNCTION__);
    if (mSWEncoder != NULL) {
        LOG1("Deleting JPEG encoder...");
        delete mSWEncoder;
    }
}

int JpegCompressor::swEncode(const InputBuffer &in, const OutputBuffer &out)
{
    LOG1("@%s, use the libjpeg to do sw jpeg encoding", __FUNCTION__);
    int status = 0;

    if (NULL == mSWEncoder) {
        LOGE("@%s, line:%d, mSWEncoder is NULL", __FUNCTION__, __LINE__);
        mJpegSize = -1;
        return -1;
    }

    mSWEncoder->init();
    mSWEncoder->setJpegQuality(out.quality);
    status = mSWEncoder->configEncoding(in.width, in.height, (JSAMPLE *)out.buf, out.size);
    if (status)
        goto exit;

    status = mSWEncoder->doJpegEncoding(in.buf, in.format);
    if (status)
        goto exit;

exit:
    if (status)
        mJpegSize = -1;
    else
        mSWEncoder->getJpegSize(&mJpegSize);

    mSWEncoder->deInit();

    return (status ? -1 : 0);
}

// Takes YUV data (NV12 or YUV420) and outputs JPEG encoded stream
int JpegCompressor::encode(const InputBuffer &in, const OutputBuffer &out)
{
    InputBuffer mid;
    LOG1("@%s:\n\t IN  = {buf:%p, w:%u, h:%u, sz:%u, f:%s}" \
             "\n\t OUT = {buf:%p, w:%u, h:%u, sz:%u, q:%d}",
            __FUNCTION__,
            in.buf, in.width, in.height, in.size, v4l2Fmt2Str(in.format),
            out.buf, out.width, out.height, out.size, out.quality);

    // For HW path
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];

    if (in.width == 0 || in.height == 0 || in.format == 0) {
        ALOGE("Invalid input received!");
        mJpegSize = -1;
        goto exit;
    }
    midbuf = (unsigned char*)malloc(in.width * in.height * 3/2);//yuv420
    if(midbuf == NULL)
    {
        ALOGE("alloc memory failed");
        mJpegSize = -1;
        goto exit;
    }
    {
        // Choose Skia
        LOG1("Choosing SWJpegEncoder for JPEG encoding");
        if (mSWEncoder == NULL) {
            ALOGE("Skia JpegEncoder not created, cannot encode to JPEG!");
            mJpegSize = -1;
            goto exit;
        }
        RepaddingYV12(out.width,out.height,in.stride,out.width,in.alignHeight,in.buf, out.buf,0);
        memcpy(midbuf,out.buf,out.width * out.height *3/2);
        mid.buf = midbuf;
        mid.format = in.format;
        mid.height = in.height;
        mid.size = in.size;
        mid.stride = in.stride;
        mid.width = in.width;
        if (swEncode(mid, out) < 0)
        goto exit;
        if(midbuf != NULL)
        {
            free(midbuf);
        }
        return mJpegSize;
    }
exit:
    if(midbuf != NULL)
    {
        free(midbuf);
    }
    return mJpegSize;
}

}
