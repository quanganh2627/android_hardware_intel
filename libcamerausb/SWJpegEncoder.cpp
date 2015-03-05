/*
 * Copyright (C) 2011 The Android Open Source Project
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
#define LOG_TAG "Camera_SWJpegEncoder"

#include "SWJpegEncoder.h"
#include "ColorConverter.h"
#include "LogHelper.h"
#include <string.h>

namespace android {

SWJpegEncoder::SWJpegEncoder() :
    mJpegQuality(mDefaultJpegQuality)
{
    LOG1("@%s", __FUNCTION__);
}

SWJpegEncoder::~SWJpegEncoder()
{
    LOG1("@%s", __FUNCTION__);
}

/**
 * Init the SW jpeg encoder
 *
 * It will init the libjpeg library
 */
void SWJpegEncoder::init(void)
{
    LOG1("@%s", __FUNCTION__);
    memset(&mCInfo, 0, sizeof(mCInfo));
    mCInfo.err = jpeg_std_error(&mJErr);
    jpeg_create_compress(&mCInfo);
}

/**
 * deInit the SW jpeg encoder
 *
 * It will deinit the libjpeg library
 */
void SWJpegEncoder::deInit(void)
{
    LOG1("@%s", __FUNCTION__);
    jpeg_destroy_compress(&mCInfo);
}

/**
 * Set the jpeg quality
 *
 * \param quality: one value from 0 to 100
 *
 */
void SWJpegEncoder::setJpegQuality(int quality)
{
    LOG1("@%s, quality:%d", __FUNCTION__, quality);
    mJpegQuality = CLIP(quality, 100, 1);
}

/**
 * Config the SW jpeg encoder.
 *
 * mainly, it will set the destination buffer manager, color space, quality.
 *
 * \param width: the width of the jpeg dimentions.
 * \param height: the height of the jpeg dimentions.
 * \param jpegBuf: the dest buffer to store the jpeg data
 * \param jpegBufSize: the size of jpegBuf buffer
 *
 * \return 0 if the configuration is right.
 * \return -1 if the configuration fails.
*/
int SWJpegEncoder::configEncoding(int width, int height, void *jpegBuf, int jpegBufSize)
{
    LOG1("@%s", __FUNCTION__);

    mCInfo.input_components = 3;
    mCInfo.in_color_space = (J_COLOR_SPACE)mSupportedFormat;
    mCInfo.image_width = width;
    mCInfo.image_height = height;

    if(setupJpegDestMgr(&mCInfo, (JSAMPLE *)jpegBuf, jpegBufSize) < 0) {
        LOGE("@%s, line:%d, setupJpegDestMgr fail", __FUNCTION__, __LINE__);
        return -1;
    }

    jpeg_set_defaults(&mCInfo);
    jpeg_set_colorspace(&mCInfo, (J_COLOR_SPACE)mSupportedFormat);
    jpeg_set_quality(&mCInfo, mJpegQuality, TRUE);
    mCInfo.raw_data_in = TRUE;
    mCInfo.dct_method = JDCT_ISLOW;
    mCInfo.comp_info[0].h_samp_factor = 2;
    mCInfo.comp_info[0].v_samp_factor = 2;
    mCInfo.comp_info[1].h_samp_factor = 1;
    mCInfo.comp_info[1].v_samp_factor = 1;
    mCInfo.comp_info[2].h_samp_factor = 1;
    mCInfo.comp_info[2].v_samp_factor = 1;
    jpeg_start_compress(&mCInfo, TRUE);

    return 0;
}

/**
 * Do the SW jpeg encoding.
 *
 * it will convert the YUV data to P411 and then do jpeg encoding.
 *
 * \param yuv_buf: the source buffer for YUV data
 * \return 0 if the encoding is successful.
 * \return -1 if the encoding fails.
 */
int SWJpegEncoder::doJpegEncoding(const void* yuv_buf, int format)
{
    LOG1("@%s", __FUNCTION__);

    unsigned char * srcY = NULL;
    unsigned char * srcU = NULL;
    unsigned char * srcV = NULL;
    JSAMPROW y[16],u[16],v[16];
    JSAMPARRAY data[3];
    int i, j, width, height;

    width= mCInfo.image_width;
    height=mCInfo.image_height;
    srcY = (unsigned char *)yuv_buf;
    srcV = (unsigned char *)yuv_buf + width * height;
    srcU = (unsigned char *)yuv_buf + width * height + width * height / 4;
    data[0] = y;
    data[1] = u;
    data[2] = v;
    for (i = 0; i < height; i += 16) {
        for (j = 0; j < 16 && (i + j) < height; j++) {
            y[j] = srcY + width * (j + i);
            if (j % 2 == 0) {
                u[j / 2] = srcU + width / 2 * ((j + i) / 2);
                v[j / 2] = srcV + width / 2 * ((j + i) / 2);
            }
        }
        jpeg_write_raw_data(&mCInfo, data, 16);
    }

    jpeg_finish_compress(&mCInfo);

    return 0;
}

/**
 * Get the jpeg size.
 *
 * \param jpegSize: get the real jpeg size, it will be -1, if encoding fails
 */
void SWJpegEncoder::getJpegSize(int *jpegSize)
{
    LOG1("@%s", __FUNCTION__);

    JpegDestMgrPtr dest = (JpegDestMgrPtr)mCInfo.dest;

    *jpegSize = (false == dest->encodeSuccess) ? -1 : dest->codedSize;
}

/**
 * Setup the jpeg destination buffer manager
 *
 * it will convert the YUV data to P411 and then do jpeg encoding.
 *
 * \param cInfo: the compress pointer
 * \param jpegBuf: the buffer pointer for jpeg data
 * \param jpegBufSize: the jpegBuf buffer's size
 * \return 0 if it's successful.
 * \return -1 if it fails.
 */
int SWJpegEncoder::
setupJpegDestMgr(j_compress_ptr cInfo, JSAMPLE *jpegBuf, int jpegBufSize)
{
    LOG1("@%s", __FUNCTION__);
    JpegDestMgrPtr dest;

    if (NULL == jpegBuf || jpegBufSize <= 0) {
        LOGE("@%s, line:%d, jpegBuf:%p, jpegBufSize:%d", __FUNCTION__, __LINE__, jpegBuf, jpegBufSize);
        return -1;
    }

    if (cInfo->dest == NULL) {
        cInfo->dest = (struct jpeg_destination_mgr *)
                        (*cInfo->mem->alloc_small)((j_common_ptr)cInfo,
                            JPOOL_PERMANENT, sizeof(JpegDestMgr));
        memset(cInfo->dest, 0, sizeof(JpegDestMgr));
    }
    dest = (JpegDestMgrPtr)cInfo->dest;

    dest->pub.init_destination = initDestination;
    dest->pub.empty_output_buffer = emptyOutputBuffer;
    dest->pub.term_destination = termDestination;
    dest->outJpegBuf = jpegBuf;
    dest->outJpegBufSize = jpegBufSize;

    return 0;
}

/**
 * Init the destination
 *
 * It's the first function which be called
 * among initDestination, emptyOutputBuffer and termDestination
 *
 * \param cInfo: the compress pointer
 */
void SWJpegEncoder::initDestination(j_compress_ptr cInfo)
{
    LOG1("@%s", __FUNCTION__);
    JpegDestMgrPtr dest = (JpegDestMgrPtr)cInfo->dest;

    dest->pub.next_output_byte = dest->outJpegBuf;
    dest->pub.free_in_buffer = dest->outJpegBufSize;
    dest->encodeSuccess = true;
}

/**
 * Empty the output buffer
 *
 * The function should not be called,
 * because we should allocate enough memory for the jpeg destination buffer
 * If we return FALSE, the libjpeg will terminate, so return TRUE always.
 * But when the function is called, the encoding failing will be recorded.
 *
 * \param cInfo: the compress pointer
 * \return TRUE if it is successful.
 * \return FALSE if something is wrong
 */
boolean SWJpegEncoder::emptyOutputBuffer(j_compress_ptr cInfo)
{
    LOG1("@%s", __FUNCTION__);
    LOGE("@%s, line:%d, buffer overflow!", __FUNCTION__, __LINE__);
    JpegDestMgrPtr dest = (JpegDestMgrPtr)cInfo->dest;

    /* re-cfg the buffer info */
    dest->pub.next_output_byte = dest->outJpegBuf;
    dest->pub.free_in_buffer = dest->outJpegBufSize;
    dest->encodeSuccess = false;

    return TRUE; /* if return FALSE, the total taking picture will fail */
}

/**
 * Terminate the destination
 *
 * The function will be called as the last function,
 * among initDestination, emptyOutputBuffer and termDestination.
 * We can get the encoded jpeg size from it.
 *
 * \param cInfo: the compress pointer
 */
void SWJpegEncoder::termDestination(j_compress_ptr cInfo)
{
    LOG1("@%s", __FUNCTION__);
    JpegDestMgrPtr dest = (JpegDestMgrPtr)cInfo->dest;

    dest->codedSize = dest->outJpegBufSize - dest->pub.free_in_buffer;
    LOG1("@%s, line:%d, codedSize:%d", __FUNCTION__, __LINE__, dest->codedSize);
}

}
