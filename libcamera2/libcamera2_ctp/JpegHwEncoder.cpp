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
    firstImageSeq(ERROR_POINTER_NOT_FOUND),
    singelSeq(ERROR_POINTER_NOT_FOUND),
    mMaxOutJpegBufSize(0)
{
    LOG1("@%s", __FUNCTION__);

    mHwImageEncoder = new IntelImageEncoder();
    if (mHwImageEncoder == NULL) {
        ALOGE("mHwImageEncoder allocation failed");
    }
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
    firstImageSeq = ERROR_POINTER_NOT_FOUND;
    singelSeq = ERROR_POINTER_NOT_FOUND;
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
           firstImageSeq = imageSeq;
    }
    //create context
    status = mHwImageEncoder->createContext(firstImageSeq, &mMaxOutJpegBufSize);
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

    if ((in.width <= MIN_HW_ENCODING_WIDTH && in.height <= MIN_HW_ENCODING_HEIGHT)
        || V4L2Fmt2VAFmt(in.fourcc, vaFmt) < 0) {
         ALOGW("@%s, line:%d, do not use the hw jpeg encoder", __FUNCTION__, __LINE__);
         return -1;
    }

    imgSeq = mBuffer2SurfaceId.valueFor((void*)in.buf);
    if (imgSeq == ERROR_POINTER_NOT_FOUND) {
        status = resetContext(in, imgSeq);
        mContextRestoreNeeded = true;
        if (status) {
            ALOGE("Encoder failed to reset the context, falling back to SW");
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

    if (mHwImageEncoder->getCoded((void*)out.buf, mMaxOutJpegBufSize) < 0) {
        ALOGE("Could not encode picture stream!");
        status = -1;
    }
    if (mContextRestoreNeeded) {
        status = restoreContext();
        if (status != 0) {
            ALOGW("restoreContext failed");
        }
        mContextRestoreNeeded = false;
    }
exit:
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

    mContextRestoreNeeded = false;

    imgSeq = mBuffer2SurfaceId.valueFor(dataPtr);
    if (imgSeq == ERROR_POINTER_NOT_FOUND) {
        status = resetContext(in, imgSeq);
        if (status) {
            ALOGE("Encoder failed to reset the context, falling back to SW");
            return -1;
        }
        mContextRestoreNeeded = true;
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

    if (mHwImageEncoder->getCoded(outBuf, mMaxOutJpegBufSize) < 0) {
        ALOGE("Could not encode picture stream!");
        status = -1;
    }
    if (mContextRestoreNeeded) {
        status = restoreContext();
        if (status != 0) {
            ALOGW("restoreContext failed");
        }
        mContextRestoreNeeded = false;
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
 *  Resets the current context and creates a new one with only 1 surface
 *  This is used when the encoder receives an input frame pointer to encode
 *  that is not mapped to a surface.
 *  A call to restoreContext is needed to revert this operation
 *
 *  \param in Input image buffer descriptor
 *  \param image sequence id created in the new context
 *
 *  \return 0 on success
 *  \return -1 on failure
 */
int JpegHwEncoder::resetContext(const InputBuffer &in, int &imgSeq)
{
    LOG1("@%s", __FUNCTION__);
    int status = 0;
    unsigned int vaFmt = 0;

    if (in.height % 2 || in.bpl % SIZE_OF_WIDTH_ALIGNMENT) {
        ALOGW("@%s, line:%d, width:%d or height:%d, we can't support", __FUNCTION__, __LINE__, in.width, in.height);
        return -1;
    }
    if (V4L2Fmt2VAFmt(in.fourcc, vaFmt) < 0) {
        ALOGW("@%s, unsupport format, do not use the hw jpeg encoder", __FUNCTION__);
        return -1;
    }
    if (! isInitialized()) {
        if (init() < 0) {
            ALOGE("HW encoder failed to initialize when setting the input buffers");
            return -1;
        }
    }

    /*create surface for the signel buffer
     * buffer had been mapped when allocated by GraphicBuffer, so use SURFACE_TYPE_USER_PTR for gralloc buffer.
     */
    status = mHwImageEncoder->createSourceSurface(SURFACE_TYPE_USER_PTR, in.buf, in.width, in.height, in.bpl, vaFmt, &imgSeq);
    if (status != 0) {
        ALOGE("create source surface failed");
        return -1;
    }
    singelSeq = imgSeq;
    //create context
    status = mHwImageEncoder->createContext(imgSeq, &mMaxOutJpegBufSize);
    if (status != 0) {
        ALOGE("createContext failed");
        return -1;
    }
    return 0;
}

/**
 *  Restores the context with the buffers originally allocated by PictureThread
 *  that were passed to encoder in setInputBuffers()
 *  This method is only needed if a context was reset. This is track by the boolean
 *  member mContextRestoreNeeded.
 *
 *  \return 0 on success
 *  \return -1 on failure
 */
int JpegHwEncoder::restoreContext()
{
    LOG1("@%s", __FUNCTION__);
    int status = 0;
    //destroy context if there is one before
    status = mHwImageEncoder->destroyContext();
    if (status != 0) {
        ALOGW("destroy context failed");
    }
    //destroy surface
    if (singelSeq != ERROR_POINTER_NOT_FOUND) {
        status = mHwImageEncoder->destroySourceSurface(singelSeq);
        if (status != 0) {
            ALOGW("destroy surface failed");
        }
    }
    //surfaces for multi buffer create in setInputBuffers aren't been destroyed, so there is no need to recreate.
    //create context
    if (firstImageSeq != ERROR_POINTER_NOT_FOUND) {
        status = mHwImageEncoder->createContext(firstImageSeq, &mMaxOutJpegBufSize);
        if (status != 0) {
            ALOGE("createContext failed");
            return -1;
        }
    }
    return status;
}


}
