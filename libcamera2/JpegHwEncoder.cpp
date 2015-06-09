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
#define LOG_TAG "Camera_JpegHwEncoder"

#include "LogHelper.h"
#include "JpegHwEncoder.h"

namespace android {

JpegHwEncoder::JpegHwEncoder() :
    mBuffer2SurfaceId(ERROR_POINTER_NOT_FOUND),
    mHWInitialized(false),
    mContextRestoreNeeded(false),
    mFirstImageSeq(ERROR_POINTER_NOT_FOUND),
    mDynamicImageSeq(ERROR_POINTER_NOT_FOUND),
    mMaxOutJpegBufSize(0)
{
    LOG1("@%s", __FUNCTION__);

    mHwImageEncoder = new IntelImageEncoder();
    if (mHwImageEncoder == NULL) {
        ALOGE("mHwImageEncoder allocation failed");
    }
    mBufferAttribute.clear();
}

JpegHwEncoder::~JpegHwEncoder()
{
    LOG1("@%s", __FUNCTION__);
    if (mHWInitialized)
        deInit();

    delete mHwImageEncoder;
    mHwImageEncoder = NULL;
}

/**
 * Initialize the HW encoder
 *
 * Initializes the IntelImageEncoder.
 * It could fail in cases when libmix JPEG encoder had been initialized or initialize libva failed.
 * This is handled normally by the PictureThread falls back to the SW encoder
 *
 * \return 0 success
 * \return -1 failure
 */
int JpegHwEncoder::init(void)
{
    int status = 0;
    LOG1("@%s", __FUNCTION__);

    status = mHwImageEncoder->initializeEncoder();
    if (status != 0) {
        ALOGE("mHwImageEncoder initializeEncoder failed");
        delete mHwImageEncoder;
        mHwImageEncoder = NULL;
        return -1;
    }

    mBuffer2SurfaceId.clear();
    mBufferAttribute.clear();
    mFirstImageSeq = ERROR_POINTER_NOT_FOUND;
    mDynamicImageSeq = ERROR_POINTER_NOT_FOUND;
    mHWInitialized = true;
    return 0;
}

/**
 * deInit the HW encoder
 *
 * de-initializes the IntelImageEncoder
 */
int JpegHwEncoder::deInit(void)
{
    LOG1("@%s", __FUNCTION__);
    if (mHwImageEncoder) {
        mHwImageEncoder->deinitializeEncoder();
        LOG1("@%s: release mHwImageEncoder", __FUNCTION__);
    }
    mHWInitialized = false;
    return 0;
}

/**
 *  Configure pre-allocated input buffer
 *
 *  Prepares the encoder to use a set of pre-allocated input buffers
 *  if an encode command comes with a pointer belonging to this set
 *  the encoding process is faster.
 * \param inputBuffersArray: a set of pre-allocated input buffers
 * \param inputBuffersNum: buffer number
 * \return 0 success
 * \return -1 failure
 */
status_t JpegHwEncoder::setInputBuffers(AtomBuffer* inputBuffersArray, int inputBuffersNum)
{
    LOG1("@%s", __FUNCTION__);

    status_t status = NO_ERROR;
    int imageSeq = 0;
    unsigned int vaFmt = 0;

    if (isInitialized())
        deInit();

    /**
     * if we want to create and destroy the IntelImageEncoder for each capture we may be
     * configured like this. This happens in video mode where the video encoder
     * context also needs to exist
     */
    if (inputBuffersNum == 0) {
        LOG1("HW encoder configured with 0 pre-allocated buffers");
        return NO_ERROR;
    }

    if (inputBuffersArray[0].bpl % SIZE_OF_WIDTH_ALIGNMENT || inputBuffersArray[0].height % 2) {
        ALOGW("@%s, line:%d, bpl:%d or height:%d, we can't support", __FUNCTION__, __LINE__, inputBuffersArray[0].bpl, inputBuffersArray[0].height);
        return BAD_VALUE;
    }

    if (V4L2Fmt2VAFmt(inputBuffersArray[0].fourcc, vaFmt) < 0) {
        ALOGW("@%s, unsupport format, do not use the hw jpeg encoder", __FUNCTION__);
        return BAD_VALUE;
    }

    if (init() < 0) {
        ALOGE("HW encoder failed to initialize when setting the input buffers");
        return UNKNOWN_ERROR;
    }

    for (int i = 0;i < inputBuffersNum;i++) {
        /*create surface
        *buffer had been mapped when allocated by gralloc, so use SURFACE_TYPE_USER_PTR for GraphicBuffer.
        */
        status = mHwImageEncoder->createSourceSurface(SURFACE_TYPE_USER_PTR, inputBuffersArray[i].dataPtr, inputBuffersArray[i].width, inputBuffersArray[i].height, inputBuffersArray[i].bpl, vaFmt, &imageSeq);
        if (status != 0) {
            ALOGE("create source surface failed");
            return UNKNOWN_ERROR;
        }
        mBuffer2SurfaceId.add(inputBuffersArray[i].dataPtr, imageSeq);
        if (i == 0)
           mFirstImageSeq = imageSeq;
    }

    //record default buffer attributes
    mBufferAttribute.width  = inputBuffersArray[0].width;
    mBufferAttribute.height = inputBuffersArray[0].height;
    mBufferAttribute.bpl    = inputBuffersArray[0].bpl;
    mBufferAttribute.fourcc = inputBuffersArray[0].fourcc;

    //create context
    status = mHwImageEncoder->createContext(mFirstImageSeq, &mMaxOutJpegBufSize);
    if (status != 0) {
        ALOGE("createContext failed");
        return UNKNOWN_ERROR;
    }

    return NO_ERROR;
}

/**
 * Encodes the input buffer placing the  resulting JPEG bitstream in the
 * output buffer. The encoding operation is synchronous
 *
 * \param in: input buffer descriptor, including buffer ponter
 * \param out: output buffer descriptor, including output quality
 * \return 0 if encoding was successful
 * \return -1 if encoding failed
 */
int JpegHwEncoder::encode(const InputBuffer &in, OutputBuffer &out)
{
    LOG1("@%s", __FUNCTION__);

    int status = 0;
    int imgSeq = -1;
    unsigned int vaFmt = 0;

    if (mHwImageEncoder == NULL) {
        ALOGW("JPEG HW encoding failed, falling back to SW");
        return -1;
    }

    if (mContextRestoreNeeded == true) {
        ALOGW("@%s Not allowed to call in parallel, this should not happen", __FUNCTION__);
    }

    if ((in.width <= MIN_HW_ENCODING_WIDTH && in.height <= MIN_HW_ENCODING_HEIGHT)
        || V4L2Fmt2VAFmt(in.fourcc, vaFmt) < 0) {
         ALOGW("@%s, line:%d, do not use the hw jpeg encoder", __FUNCTION__, __LINE__);
         return -1;
    }

    imgSeq = mBuffer2SurfaceId.valueFor((void*)in.buf);
    if (imgSeq == ERROR_POINTER_NOT_FOUND) {
        status = handleDynamicBuffer(in, imgSeq);
        if (status) {
            ALOGE("failed to handle dynamic buffer, falling back to SW");
            finalizeDynamicBuffer();
            return -1;
        }
    }
    if (mHwImageEncoder->encode(imgSeq, CLIP(out.quality, 100, 1)) == 0) {
    } else {
        ALOGW("JPEG HW encoding failed, falling back to SW");
        return -1;
    }
    //according to libmix Jpegencoder function call flow, getCodedSize must be called before getCoded
    if (mHwImageEncoder->getCodedSize(&out.size) < 0) {
        ALOGE("Could not get coded JPEG size!");
        return -1;
    }

    if (mHwImageEncoder->getCoded((void*)out.buf, out.size) < 0) {
        ALOGE("Could not encode picture stream!");
        status = -1;
    }
    if (mDynamicImageSeq != ERROR_POINTER_NOT_FOUND) {
        status = finalizeDynamicBuffer();
        if (status != 0) {
            ALOGW("finalizeDynamicBuffer failed");
        }
    }

    return status;
}

/**
 * Starts the HW encoding process.
 * After it returns the JPEG is not encoded yet
 * The following steps are:
 * - getOutput()
 *
 * \param in: input buffer descriptor, including buffer ponter
 * \param out: output buffer descriptor, including output quality
 * \param mMaxCodedSize: max size of coded jpeg
 */
int JpegHwEncoder::encodeAsync(const InputBuffer &in, OutputBuffer &out, int &mMaxCodedSize)
{
    LOG1("@%s", __FUNCTION__);
    int imgSeq = -1;
    int status = 0;
    void* dataPtr = (void*)in.buf;

    if (mContextRestoreNeeded == true) {
        ALOGW("@%s Not allowed to call in parallel, this should not happen", __FUNCTION__);
    }

    imgSeq = mBuffer2SurfaceId.valueFor(dataPtr);
    if (imgSeq == ERROR_POINTER_NOT_FOUND) {
        status = handleDynamicBuffer(in, imgSeq);
        if (status) {
            ALOGE("failed to handle dynamic buffer, falling back to SW");
            finalizeDynamicBuffer();
            return -1;
        }
    }
    if (mHwImageEncoder &&
        mHwImageEncoder->encode(imgSeq, CLIP(out.quality, 100, 1)) == 0) {
    } else {
        ALOGW("JPEG HW encoding failed, falling back to SW");
        return -1;
    }
    mMaxCodedSize = mMaxOutJpegBufSize;
    return 0;
}

/**
 *  Retrieve the encoded JPEG size
 *
 *  Part of the asynchronous encoding process.
 *
 *  \param outSize: actual size of jpeg
 */
int JpegHwEncoder::getOutputSize(unsigned int& outSize)
{
    LOG1("@%s", __FUNCTION__);
    int status = 0;

    if (mHwImageEncoder->getCodedSize(&outSize) < 0) {
        ALOGE("Could not get coded size!");
        status = -1;
    }
    return status;
}

/**
 *  Retrieve the encoded bitstream
 *
 *  Part of the asynchronous encoding process.
 *  Copies the jpeg bistream from the internal buffer allocated by libVA
 *  to the one provided inside the outputBuffer struct
 *
 *  \param outBuf: output buffer of the encoding process
 *  \param outSize: actual size of jpeg
 */
int JpegHwEncoder::getOutput(void* outBuf, unsigned int& outSize)
{
    LOG1("@%s", __FUNCTION__);
    int status = 0;

    if (mHwImageEncoder->getCoded(outBuf, outSize) < 0) {
        ALOGE("Could not encode picture stream!");
        status = -1;
    }

    if (mDynamicImageSeq != ERROR_POINTER_NOT_FOUND) {
        status = finalizeDynamicBuffer();
        if (status != 0) {
            ALOGW("finalizeDynamicBuffer failed");
        }
    }
    return status;
}

/*
 * convert V4L2 Format to VA Format
*/
int JpegHwEncoder::V4L2Fmt2VAFmt(unsigned int v4l2Fmt, unsigned int &vaFmt)
{
    int status = 0;

    switch(v4l2Fmt) {
    case V4L2_PIX_FMT_NV12:
         vaFmt = VA_RT_FORMAT_YUV420;
         break;
    case V4L2_PIX_FMT_YUV422P:
         vaFmt = VA_RT_FORMAT_YUV422;
         break;
    default:
         ALOGE("@%s Unknown / unsupported preview pixel format: fatal error",__FUNCTION__);
         status = -1;
         break;
    }
    return status;
}

/**
 * Generally we have a group of default buffers registered and the input buffer
 * should be in it. This method is going to handle an exceptional case that
 * the requested input buffer is a new one which is not in default group.
 * For this situation, if the new buffer has the same attribute with default
 * buffer, just needs to create a new surface for this buffer. Otherwise, we have
 * to destroy the old context and create a new one for the dynamic surface.
 *
 * A call to finalizeDynamicBuffer is needed to fallback to default
 *
 * \param in Input image buffer descriptor
 * \param image sequence id created in the new context
 *
 * \return NO_ERROR on success
 */
status_t JpegHwEncoder::handleDynamicBuffer(const InputBuffer &in, int &imgSeq)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    unsigned int vaFmt = 0;

    if (in.height % 2 || in.bpl % SIZE_OF_WIDTH_ALIGNMENT) {
        ALOGW("@%s, line:%d, width:%d or height:%d, we can't support", __FUNCTION__, __LINE__, in.width, in.height);
        return BAD_VALUE;
    }
    if (V4L2Fmt2VAFmt(in.fourcc, vaFmt) < 0) {
        ALOGW("@%s, unsupport format, do not use the hw jpeg encoder", __FUNCTION__);
        return BAD_VALUE;
    }
    if (! isInitialized()) {
        if (init() < 0) {
            ALOGE("HW encoder failed to initialize when setting the input buffers");
            return NO_INIT;
        }
    }


    // create surface for the dynamic buffer
    status = mHwImageEncoder->createSourceSurface(SURFACE_TYPE_USER_PTR, in.buf, in.width, in.height, in.bpl, vaFmt, &imgSeq);
    if (status != NO_ERROR) {
        ALOGE("create source surface failed");
        return UNKNOWN_ERROR;
    }
    mDynamicImageSeq = imgSeq;

    // no need to reset context for same attribute buffer
    if (mBufferAttribute.sameAttrWith(in)) {
        LOG1("create new surface :%d for dynamic buffer, keep the old context", mDynamicImageSeq);
        return status;
    }

    // destroy context first
    status = mHwImageEncoder->destroyContext();
    if (status != NO_ERROR) {
        ALOGW("destroy context failed, try to create new context");
    }
    // create new context
    status = mHwImageEncoder->createContext(imgSeq, &mMaxOutJpegBufSize);
    if (status != NO_ERROR) {
        ALOGE("failed to creat new context");
        return UNKNOWN_ERROR;
    }
    mContextRestoreNeeded = true;

    return status;
}

/**
 * Finalize the previous dynamic buffer handling.
 * This method must be called if we hanlded a dynamic buffer before.
 * The condition could be if mDynamicImageSeq is valid.
 * Restore the context if needed.
 *
 *  \return NO_ERROR on success
 */
status_t JpegHwEncoder::finalizeDynamicBuffer()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    // destroy surface
    if (mDynamicImageSeq != ERROR_POINTER_NOT_FOUND) {
        status = mHwImageEncoder->destroySourceSurface(mDynamicImageSeq);
        if (status != NO_ERROR) {
            ALOGW("failed to destroy surface:%d", mDynamicImageSeq);
        }
        mDynamicImageSeq = ERROR_POINTER_NOT_FOUND;
    }

    if (!mContextRestoreNeeded) {
        LOG1("%s no need to restore context", __FUNCTION__);
        return status;
    }

    mContextRestoreNeeded = false;
    // destroy temporary context
    status = mHwImageEncoder->destroyContext();
    if (status != NO_ERROR) {
        ALOGW("destroy context failed");
    }
    // create context if needed
    if (mFirstImageSeq != ERROR_POINTER_NOT_FOUND) {
        status = mHwImageEncoder->createContext(mFirstImageSeq, &mMaxOutJpegBufSize);
        if (status != 0) {
            ALOGE("createContext failed");
            return -1;
        }
    }
    return status;
}

} // namespace android
