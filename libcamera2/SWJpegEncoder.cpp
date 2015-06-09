/*
 * Copyright (C) 2011 The Android Open Source Project
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
#define LOG_TAG "Camera_SWJpegEncoder"

#include "SWJpegEncoder.h"
#include "ColorConverter.h"
#include "LogHelper.h"
#include <string.h>
#include "PlatformData.h"

namespace android {

SWJpegEncoder::SWJpegEncoder() :
    mJpegSize(-1)
    ,mTotalWidth(0)
    ,mTotalHeight(0)
    ,mDstBuf(NULL)
    ,mCPUCoresNum(1)
{
    LOG1("@%s, line:%d", __FUNCTION__, __LINE__);
}

SWJpegEncoder::~SWJpegEncoder()
{
    LOG1("@%s, line:%d", __FUNCTION__, __LINE__);
}

/**
 * encode jpeg by calling the Codec which is the libjpeg wrapper
 * the public interface of the SWJpegEncoder
 * Takes YUV data (NV12 or YUV420) and outputs JPEG encoded stream
 *
 * \param in: input buffer description
 * \param out: output param description
 * \return the jpeg size if encoding was successful
 * \return -1 if encoding failed
 */
int SWJpegEncoder::encode(const InputBuffer &in, const OutputBuffer &out)
{
    int status;
    nsecs_t startTime = systemTime();

    LOG1("@%s:\n\t IN  = {buf:%p, w:%u, h:%u, sz:%u, f:%s}" \
             "\n\t OUT = {buf:%p, w:%u, h:%u, sz:%u, q:%d}",
            __FUNCTION__,
            in.buf, in.width, in.height, in.size, v4l2Fmt2Str(in.fourcc),
            out.buf, out.width, out.height, out.size, out.quality);

    if (in.width == 0 || in.height == 0 || in.fourcc == 0) {
        ALOGE("Invalid input received!");
        mJpegSize = -1;
        goto exit;
    }

    mTotalWidth = in.width;
    mTotalHeight = in.height;
    mDstBuf = out.buf;

    status = isNeedMultiThreadEncoding(in.width, in.height)
                ? swEncodeMultiThread(in, out)
                : swEncode(in, out);
    if (status < 0)
        goto exit;

    LOG1("@%s encode, total consume:%ums", __FUNCTION__, (unsigned)((systemTime() - startTime) / 1000000));
    return mJpegSize;
exit:
    return (mJpegSize = -1);
}

/**
 *  This function will decide if we need to enable the multi thread jpeg encoding.
 *  currently, we have two conditions to use the old single jpeg encoding.
 *  one is that the resolution is smaller than the 1.3M
 *  the other is that the CPU number is 1
 *
 *  \param width: the Jpeg width
 *  \param height: the Jpeg height
 *
 *  \return false if we don't need multi thread Jpeg encoding
 *  \return true if we need multi thread Jpeg encoding
 */
bool SWJpegEncoder::isNeedMultiThreadEncoding(int width, int height)
{
    LOG1("@%s, line:%d, width:%d, height:%d", __FUNCTION__, __LINE__, width, height);
    bool ret = false;

    /* more conditions could be added to here by according to the request */
    if ((width < RESOLUTION_1_3MP_WIDTH && height < RESOLUTION_1_3MP_HEIGHT)
        || (1 == (mCPUCoresNum = PlatformData::getNumOfCPUCores())))
        ret = false;
    else
        ret = true;

    LOG1("@%s, line:%d, ret:%d", __FUNCTION__, __LINE__, ret);
    return ret;
}

/**
 * encode jpeg by calling the SWJpegEncoder which is the libjpeg wrapper
 * single thread.
 *
 * \param in: input buffer description
 * \param out: output param description
 * \return 0 if encoding was successful
 * \return -1 if encoding failed
 */
int SWJpegEncoder::swEncode(const InputBuffer &in, const OutputBuffer &out)
{
    LOG1("@%s, line:%d, use the libjpeg to do sw jpeg encoding", __FUNCTION__, __LINE__);
    int status = 0;
    Codec encoder(out.quality);

    encoder.init();
    encoder.setJpegQuality(out.quality);
    status = encoder.configEncoding(in.width, in.height, (JSAMPLE *)out.buf, out.size);
    if (status)
        goto exit;

    status = encoder.doJpegEncoding(in.buf, (in.buf + in.width * in.height), in.fourcc);
    if (status)
        goto exit;

exit:
    if (status)
        mJpegSize = -1;
    else
        encoder.getJpegSize(&mJpegSize);

    encoder.deInit();

    return (status ? -1 : 0);
}

/**
 * encode jpeg by calling the SWJpegEncoder which is the libjpeg wrapper
 * multi thread.
 * the thread number depends on the CPU number.
 *
 * \param in: input buffer description
 * \param out: output param description
 * \return 0 if encoding was successful
 * \return -1 if encoding failed
 */
int SWJpegEncoder::swEncodeMultiThread(const InputBuffer &in, const OutputBuffer &out)
{
    LOG1("@%s, line:%d, use the libjpeg to do sw jpeg encoding", __FUNCTION__, __LINE__);
    int status = 0;

    init(mCPUCoresNum);
    config(in, out);

    status = doJpegEncodingMultiThread();
    if (status)
        goto exit;

exit:
    mJpegSize = status ? -1 : mergeJpeg();
    deInit();

    return (status ? -1 : 0);
}

/**
 * Initialize for the multi thread jpeg encoding
 *
 * it will create n CodecWorkerThread by according to the thread number.
 */
void SWJpegEncoder::init(unsigned int threadNum)
{
    unsigned int num = CLIP(threadNum, MAX_THREAD_NUM, MIN_THREAD_NUM);
    LOG1("@%s, line:%d, thread number, pass:%d, real:%d", __FUNCTION__, __LINE__, threadNum, num);

    for (unsigned int i = 0; i < num; i++) {
        mSwJpegEncoder.push(new CodecWorkerThread);
    }
}

/**
 * deInit for the multi thread jpeg encoding
 *
 * it will release all n CodecWorkerThread
 */
void SWJpegEncoder::deInit(void)
{
    LOG1("@%s, line:%d", __FUNCTION__, __LINE__);
    for (unsigned int i = 0; i < mSwJpegEncoder.size(); i++) {
        mSwJpegEncoder.editItemAt(i).clear();
    }
    mSwJpegEncoder.removeItemsAt(0, mSwJpegEncoder.size());
    mSwJpegEncoder.clear();
}

/**
 * configue every thread for multi thread jpeg
 *
 * \param in: input buffer description
 * \param out: output param description
 */
void SWJpegEncoder::config(const InputBuffer &in, const OutputBuffer &out)
{
    LOG1("@%s, line:%d", __FUNCTION__, __LINE__);
    sp<CodecWorkerThread> encThread;
    CodecWorkerThread::CodecConfig cfg;

    for (unsigned int i = 0; i < mSwJpegEncoder.size(); i++) {
        cfg.width = in.width;
        /*
            for example, there are 4 threads.
            the first 3 threads must align to 16 which is NV12_MCU_SIZE
            but for the last thread, it doesn't have this request.
        */
        cfg.height = ALIGN16(in.height / mSwJpegEncoder.size());
        cfg.fourcc = in.fourcc;
        cfg.inBufY = in.buf + cfg.width * cfg.height * i;
        cfg.inBufUV = (cfg.fourcc == V4L2_PIX_FMT_NV12)
            ? (in.buf + in.width * in.height + cfg.width * cfg.height * i / 2)
            : NULL;
        cfg.quality = out.quality;
        cfg.outBufSize = (out.size - DEST_BUF_OFFSET) / mSwJpegEncoder.size();
        cfg.outBuf = out.buf + DEST_BUF_OFFSET + cfg.outBufSize * i;
        /* update the last thread's height */
        if (i == mSwJpegEncoder.size() - 1)
            cfg.height = in.height - cfg.height * (mSwJpegEncoder.size() - 1);

        encThread = mSwJpegEncoder.editItemAt(i);
        encThread->setConfig(cfg);
        LOG1("@%s, line:%d, the %d picture thread cfg", __FUNCTION__, __LINE__, i);
        LOG1("@%s, line:%d, cfg.width:%d, cfg.height:%d", __FUNCTION__, __LINE__, cfg.width, cfg.height);
        LOG1("@%s, line:%d, cfg.fourcc:%s 0x%x, cfg.quality:%d", __FUNCTION__, __LINE__, v4l2Fmt2Str(cfg.fourcc), cfg.fourcc, cfg.quality);
        LOG1("@%s, line:%d, cfg.inBufY:0x%p, cfg.inBufUV:0x%p", __FUNCTION__, __LINE__, cfg.inBufY, cfg.inBufUV);
        LOG1("@%s, line:%d, cfg.outBuf:0x%p, cfg.outBufSize:%d", __FUNCTION__, __LINE__, cfg.outBuf, cfg.outBufSize);
    }
}

/**
 * the function will trigger the multi jpeg encoding
 *
 * \return 0 if encoding was successful
 * \return -1 if encoding failed
 */
int SWJpegEncoder::doJpegEncodingMultiThread(void)
{
    LOG1("@%s, line:%d", __FUNCTION__, __LINE__);
    sp<CodecWorkerThread> encThread;
    status_t status = UNKNOWN_ERROR;
    String8 threadName("CamHAL_SWEncodeMultiThread");

    /* run all threads */
    for (unsigned int i = 0; i <mSwJpegEncoder.size(); i++) {
        threadName.appendFormat(":%d", i);
        LOG1("@%s, new sw jpeg thread name:%s", __FUNCTION__, threadName.string());
        encThread = mSwJpegEncoder.editItemAt(i);
        status = encThread->runThread(threadName.string());
        if (status != NO_ERROR) {
            ALOGE("@%s, line:%d, start jpeg thread fail, thread name:%s", __FUNCTION__, __LINE__, threadName.string());
            return -1;
        }
    }

    /* wait all threads to finish */
    for (unsigned int i = 0; i < mSwJpegEncoder.size(); i++) {
        LOG1("@%s, the %d sw jpeg encoder thread before join!", __FUNCTION__, i);
        encThread = mSwJpegEncoder.editItemAt(i);
        encThread->waitThreadFinish();
    }

    return 0;
}

/**
 * the function will merge all jpeg pictures which are generated in multi threads
 * to one jpeg picture
 *
 * \return int the merged jpeg size
 */
int SWJpegEncoder::mergeJpeg(void)
{
#define HEADER_TOTAL_LEN 623
#define HEADER_SOS_LEN 14
#define HEADER_EOI_LEN 2
#define HEADER_HEIGHT_POS 163
#define HEADER_WIDTH_POS 165
#define NV12_MCU_SIZE 16
    LOG1("@%s, line:%d", __FUNCTION__, __LINE__);
    int size = 0;
    CodecWorkerThread::CodecConfig cfg;
    nsecs_t startTime;
    sp<CodecWorkerThread> encThread = mSwJpegEncoder.editItemAt(0);
    encThread->getConfig(&cfg);

    /* Write the JPEG header */
    memcpy(mDstBuf, cfg.outBuf, HEADER_TOTAL_LEN - HEADER_SOS_LEN);
    size = HEADER_TOTAL_LEN - HEADER_SOS_LEN;

    /* Update the width and height info */
    mDstBuf[HEADER_HEIGHT_POS] = (mTotalHeight >> 8) & 0xFF;
    mDstBuf[HEADER_HEIGHT_POS+1] = mTotalHeight & 0xFF;
    mDstBuf[HEADER_WIDTH_POS] = (mTotalWidth >> 8) & 0xFF;
    mDstBuf[HEADER_WIDTH_POS+1] = mTotalWidth & 0xFF;

    /* Write the restarting interval */
    if (mSwJpegEncoder.size() > 1) {
        unsigned int MCUs = (cfg.height / NV12_MCU_SIZE) * (cfg.width / NV12_MCU_SIZE);
        mDstBuf[size++] = 0xFF;
        mDstBuf[size++] = 0xDD;
        mDstBuf[size++] = 0;
        mDstBuf[size++] = 4;
        mDstBuf[size++] = (MCUs >> 8) & 0xFF;
        mDstBuf[size++] = MCUs & 0xFF;
    }

    /* Write the SOS */
    memcpy((void *)((unsigned int)mDstBuf + size),
            (void *)((unsigned int)cfg.outBuf + HEADER_TOTAL_LEN - HEADER_SOS_LEN),
            HEADER_SOS_LEN);
    size += HEADER_SOS_LEN;

    /* Write coded segments */
    for (unsigned int i = 0; i <mSwJpegEncoder.size(); i++) {
        encThread = mSwJpegEncoder.editItemAt(i);
        startTime = systemTime();
        encThread->getConfig(&cfg);
        memmove((void *)((unsigned int)mDstBuf + size),
               (void *)((unsigned int)cfg.outBuf + HEADER_TOTAL_LEN),
               (encThread->getJpegDataSize() - HEADER_TOTAL_LEN - HEADER_EOI_LEN));
        LOG1("@%s, wr %d segments, size:%d, consume:%ums", __FUNCTION__, i, (encThread->getJpegDataSize()- HEADER_TOTAL_LEN - HEADER_EOI_LEN), (unsigned)((systemTime() - startTime) / 1000000));
        size += (encThread->getJpegDataSize() - HEADER_TOTAL_LEN - HEADER_EOI_LEN);

        if (i != (mSwJpegEncoder.size() - 1)) {
            mDstBuf[size++] = 0xFF;
            mDstBuf[size++] = (i & 0x7) | 0xD0;
        }
    }

    /* Write EOI */
    mDstBuf[size++]= 0xFF;
    mDstBuf[size++]= 0xD9;

    return size;
}

SWJpegEncoder::CodecWorkerThread::CodecWorkerThread() :
    mDataSize(-1)
{
    LOG1("@%s, line:%d", __FUNCTION__, __LINE__);
}

SWJpegEncoder::CodecWorkerThread::~CodecWorkerThread()
{
    LOG1("@%s, line:%d", __FUNCTION__, __LINE__);
}

/**
 * run one thread for multi thread jpeg encoding
 *
 * \param name: the thread name
 */
status_t SWJpegEncoder::CodecWorkerThread::runThread(const char *name)
{
    LOG1("@%s, line:%d", __FUNCTION__, __LINE__);
    return this->run(name);
}

/**
 * wait one thread until it has finished
 *
 */
void SWJpegEncoder::CodecWorkerThread::waitThreadFinish(void)
{
    LOG1("@%s, line:%d", __FUNCTION__, __LINE__);
    this->join();
    this->requestExitAndWait();
}

/**
 * get jpeg size which is done in one thread
 *
 * \return int the coded jpeg size
 */
int SWJpegEncoder::CodecWorkerThread::getJpegDataSize(void)
{
    LOG1("@%s, line:%d", __FUNCTION__, __LINE__);
    return mDataSize;
}

/**
 * the thread exe function for one jpeg thread
 * when the encoding has been done, it will return false to terminate the thread
 *
 * \return false
 */
bool SWJpegEncoder::CodecWorkerThread::threadLoop()
{
    LOG1("@%s, line:%d, in CodecWorkerThread", __FUNCTION__, __LINE__);
    nsecs_t startTime = systemTime();
    int ret = swEncode();
    LOG1("@%s one swEncode done!, consume:%ums, ret:%d", __FUNCTION__, (unsigned)((systemTime() - startTime) / 1000000), ret);

    return false;
}

/**
 * this function will call the SWJpegEncoder to encode one jpeg.
 * it's the main function of the threadLoop
 *
 * \return 0 if encoding was successful
 * \return -1 if encoding failed
 */
int SWJpegEncoder::CodecWorkerThread::swEncode(void)
{
    LOG1("@%s, line:%d, in CodecWorkerThread", __FUNCTION__, __LINE__);
    int status = 0;
    Codec encoder(mCfg.quality);

    encoder.init();
    encoder.setJpegQuality(mCfg.quality);
    status = encoder.configEncoding(mCfg.width, mCfg.height,
                            (JSAMPLE *)mCfg.outBuf, mCfg.outBufSize);
    if (status)
        goto exit;

    status = encoder.doJpegEncoding(mCfg.inBufY, mCfg.inBufUV, mCfg.fourcc);
    if (status)
        goto exit;

exit:
    if (status)
        mDataSize = -1;
    else
        encoder.getJpegSize(&mDataSize);

    encoder.deInit();

    return (status ? -1 : 0);
}

SWJpegEncoder::Codec::Codec(int quality) :
    mJpegQuality(CLIP(quality, 100, 1))
{
    LOG1("@%s", __FUNCTION__);
}

SWJpegEncoder::Codec::~Codec()
{
    LOG1("@%s", __FUNCTION__);
}

/**
 * Init the SW jpeg encoder
 *
 * It will init the libjpeg library
 */
void SWJpegEncoder::Codec::init(void)
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
void SWJpegEncoder::Codec::deInit(void)
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
void SWJpegEncoder::Codec::setJpegQuality(int quality)
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
int SWJpegEncoder::Codec::
configEncoding(int width, int height, void *jpegBuf, int jpegBufSize)
{
    LOG1("@%s", __FUNCTION__);

    mCInfo.input_components = 3;
    mCInfo.in_color_space = (J_COLOR_SPACE)SUPPORTED_FORMAT;
    mCInfo.image_width = width;
    mCInfo.image_height = height;

    if(setupJpegDestMgr(&mCInfo, (JSAMPLE *)jpegBuf, jpegBufSize) < 0) {
        ALOGE("@%s, line:%d, setupJpegDestMgr fail", __FUNCTION__, __LINE__);
        return -1;
    }

    jpeg_set_defaults(&mCInfo);
    jpeg_set_colorspace(&mCInfo, (J_COLOR_SPACE)SUPPORTED_FORMAT);
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
 * \param y_buf: the source buffer for Y data
 * \param uv_buf: the source buffer for UV data,
 * \it could be NULL if fourcc is V4L2_PIX_FMT_YUYV
 * \return 0 if the encoding is successful.
 * \return -1 if the encoding fails.
 */
int SWJpegEncoder::Codec::
doJpegEncoding(const void *y_buf, const void *uv_buf, int fourcc)
{
    LOG1("@%s, fourcc:%d, yuyv:%d, nv12:%d", __FUNCTION__, fourcc, V4L2_PIX_FMT_YUYV, V4L2_PIX_FMT_NV12);

    unsigned char *srcY = NULL;
    unsigned char *srcUV = NULL;
    unsigned char *p411 = NULL;
    JSAMPROW y[16],u[16],v[16];
    JSAMPARRAY data[3];
    int i, j, width, height;

    width = mCInfo.image_width;
    height = mCInfo.image_height;
    srcY = (unsigned char*)y_buf;
    srcUV = (unsigned char*)uv_buf;
    p411 = (unsigned char*)malloc(width * height * 3 / 2);
    if (NULL == p411) {
        ALOGE("@%s, line:%d, malloc fail", __FUNCTION__, __LINE__);
        return -1;
    }

    switch (fourcc) {
    case V4L2_PIX_FMT_YUYV:
        YUY2ToP411(width, height, srcY, p411);
        break;
    case V4L2_PIX_FMT_NV12:
        NV12ToP411Separate(width, height, srcY, srcUV, p411);
        break;
    case V4L2_PIX_FMT_NV21:
        NV21ToP411Separate(width, height, srcY, srcUV, p411);
        break;
    defaut:
        ALOGE("%s Unsupported fourcc %s 0x%x", __func__,v4l2Fmt2Str(fourcc), fourcc);
        return -1;
    }

    data[0] = y;
    data[1] = u;
    data[2] = v;
    for (i = 0; i < height; i += 16) {
        for (j = 0; j < 16 && (i + j) < height; j++) {
            y[j] = p411 + width * (j + i);
            if (j % 2 == 0) {
                u[j / 2] = p411 + width * height + width / 2 * ((j + i) / 2);
                v[j / 2] = p411 + width * height + width * height / 4 + width / 2 * ((j + i) / 2);
            }
        }
        jpeg_write_raw_data(&mCInfo, data, 16);
    }

    jpeg_finish_compress(&mCInfo);

    free(p411);
    p411 = NULL;

    return 0;
}

/**
 * Get the jpeg size.
 *
 * \param jpegSize: get the real jpeg size, it will be -1, if encoding fails
 */
void SWJpegEncoder::Codec::getJpegSize(int *jpegSize)
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
int SWJpegEncoder::Codec::
setupJpegDestMgr(j_compress_ptr cInfo, JSAMPLE *jpegBuf, int jpegBufSize)
{
    LOG1("@%s", __FUNCTION__);
    JpegDestMgrPtr dest;

    if (NULL == jpegBuf || jpegBufSize <= 0) {
        ALOGE("@%s, line:%d, jpegBuf:%p, jpegBufSize:%d", __FUNCTION__, __LINE__, jpegBuf, jpegBufSize);
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
void SWJpegEncoder::Codec::initDestination(j_compress_ptr cInfo)
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
boolean SWJpegEncoder::Codec::emptyOutputBuffer(j_compress_ptr cInfo)
{
    LOG1("@%s", __FUNCTION__);
    ALOGE("@%s, line:%d, buffer overflow!", __FUNCTION__, __LINE__);
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
void SWJpegEncoder::Codec::termDestination(j_compress_ptr cInfo)
{
    LOG1("@%s", __FUNCTION__);
    JpegDestMgrPtr dest = (JpegDestMgrPtr)cInfo->dest;

    dest->codedSize = dest->outJpegBufSize - dest->pub.free_in_buffer;
    LOG1("@%s, line:%d, codedSize:%d", __FUNCTION__, __LINE__, dest->codedSize);
}

}
