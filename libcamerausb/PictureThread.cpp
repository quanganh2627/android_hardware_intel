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
#define LOG_TAG "Camera_PictureThread"

#include "CameraBufferAllocator.h"
#include "ColorConverter.h"
#include "PictureThread.h"
#include "LogHelper.h"
#include "Callbacks.h"
#include "DumpImage.h"
#include "VAConvertor.h"

#include <utils/Timers.h>
#include <assert.h>
namespace android {

static const int MAX_EXIF_SIZE = 0xFFFF;
static const unsigned char JPEG_MARKER_SOI[2] = {0xFF, 0xD8}; // JPEG StartOfImage marker
static const unsigned char JPEG_MARKER_EOI[2] = {0xFF, 0xD9}; // JPEG EndOfImage marker

PictureThread::PictureThread() :
    Thread(true) // callbacks may call into java
    ,mMessageQueue("PictureThread", MESSAGE_ID_MAX)
    ,mThreadRunning(false)
    ,mCallbacks(NULL)
    ,mOutData(NULL)
    ,mExifBuf(NULL)
    ,mVaConvertor(new VAConvertor())
    ,mInputFormat(V4L2_PIX_FMT_YUV422P)
{
    LOG1("@%s", __FUNCTION__);
}

PictureThread::~PictureThread()
{
    LOG1("@%s", __FUNCTION__);
    if (mOutData != NULL) {
        delete[] mOutData;
    }
    if (mExifBuf != NULL) {
        delete[] mExifBuf;
    }
    if (mVaConvertor !=NULL)
        delete mVaConvertor;
    if (mCallbacks.get())
        mCallbacks.clear();
}

/*
 * encodeToJpeg: encodes the given buffer and creates the final JPEG file
 * Input:  mainBuf  - buffer containing the main picture image
 *         thumbBuf - buffer containing the thumbnail image (optional, can be NULL)
 * Output: destBuf  - buffer containing the final JPEG image including EXIF header
 *         Note that, if present, thumbBuf will be included in EXIF header
 * picture_stride: stride for src main image
 * thumbnail_stride: stride for src thumbnail image
 * alignPicHeight: main picture height
 * alignThumHeight: thumbnail picture height
 */

status_t PictureThread::encodeToJpeg(void *mainBuf, int mainSize, void *thumbBuf, CameraBuffer *destBuf,int picture_stride,int thumbnail_stride,int alignPicHeight,int alignThumHeight)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    char *copyFrom = NULL;

    nsecs_t startTime = systemTime();
    nsecs_t endTime = 0;

    // Convert and encode the thumbnail, if present and EXIF maker is initialized

    if (mConfig.exif.enableThumb) {

        LOG1("Encoding thumbnail");

        // setup the JpegCompressor input and output buffers
        mEncoderInBuf.clear();
        mEncoderInBuf.buf = (unsigned char*)thumbBuf;
        mEncoderInBuf.width = mConfig.thumbnail.width;
        mEncoderInBuf.height = mConfig.thumbnail.height;
        mEncoderInBuf.format = mConfig.thumbnail.format;
        mEncoderInBuf.stride = thumbnail_stride;
        mEncoderInBuf.alignHeight = alignThumHeight;
        mEncoderInBuf.size = frameSize(mConfig.thumbnail.format,
                thumbnail_stride,
                mConfig.thumbnail.height);
        mEncoderOutBuf.clear();
        mEncoderOutBuf.buf = mOutData;
        mEncoderOutBuf.width = mConfig.thumbnail.width;
        mEncoderOutBuf.height = mConfig.thumbnail.height;
        mEncoderOutBuf.quality = mConfig.thumbnail.quality;
        mEncoderOutBuf.size = mMaxOutDataSize;
        endTime = systemTime();
        int size = compressor.encode(mEncoderInBuf, mEncoderOutBuf);
        LOG1("Thumbnail JPEG size: %d (time to encode: %ums)", size, (unsigned)((systemTime() - endTime) / 1000000));
        if (size > 0) {
            encoder.setThumbData(mEncoderOutBuf.buf, size);
        } else {
            // This is not critical, we can continue with main picture image
            ALOGE("Could not encode thumbnail stream!");
        }
    } else {
        LOG1("Skipping thumbnail");
    }
    int totalSize = 0;
    unsigned int exifSize = 0;
    // Copy the SOI marker
    unsigned char* currentPtr = mExifBuf;
    memcpy(currentPtr, JPEG_MARKER_SOI, sizeof(JPEG_MARKER_SOI));
    totalSize += sizeof(JPEG_MARKER_SOI);
    currentPtr += sizeof(JPEG_MARKER_SOI);
    if (encoder.makeExif(currentPtr, &mConfig.exif, &exifSize, false) != JPG_SUCCESS)
        ALOGE("Error making EXIF");
    currentPtr += exifSize;
    totalSize += exifSize;
    // Copy the EOI marker
    memcpy(currentPtr, (void*)JPEG_MARKER_EOI, sizeof(JPEG_MARKER_EOI));
    totalSize += sizeof(JPEG_MARKER_EOI);
    exifSize = totalSize;

    if(!mConfig.jpegfromdriver) {
    // Convert and encode the main picture image
    // setup the JpegCompressor input and output buffers
        mEncoderInBuf.clear();
        mEncoderInBuf.buf = (unsigned char *) mainBuf;

        mEncoderInBuf.width = mConfig.picture.width;
        mEncoderInBuf.height = mConfig.picture.height;
        mEncoderInBuf.format = mConfig.picture.format;
        mEncoderInBuf.stride = picture_stride;
        mEncoderInBuf.alignHeight = alignPicHeight;
        mEncoderInBuf.size = frameSize(mConfig.picture.format,
                            picture_stride,
                            mConfig.picture.height);
        mEncoderOutBuf.clear();
        mEncoderOutBuf.buf = (unsigned char*)mOutData;
        mEncoderOutBuf.width = mConfig.picture.width;
        mEncoderOutBuf.height = mConfig.picture.height;
        mEncoderOutBuf.quality = mConfig.picture.quality;
        mEncoderOutBuf.size = mMaxOutDataSize;
        endTime = systemTime();
        mainSize = compressor.encode(mEncoderInBuf, mEncoderOutBuf);
    }
    LOG1("Picture JPEG size: %d (time to encode: %ums)", mainSize, (unsigned)((systemTime() - endTime) / 1000000));
    if (mainSize > 0) {
        // We will skip SOI marker from final file
        totalSize += (mainSize - sizeof(JPEG_MARKER_SOI));
    } else {
        ALOGE("Could not encode picture stream!");
        status = UNKNOWN_ERROR;
    }

    if (status == NO_ERROR) {
        CameraMemoryAllocator::instance()->allocateMemory(destBuf,totalSize,mCallbacks.get());
        if (destBuf->getData() == 0) {
            ALOGE("No memory for final JPEG file!");
            status = NO_MEMORY;
        }
    }
    if (status == NO_ERROR) {
        // Copy EXIF (it will also have the SOI and EOI markers
        memcpy(destBuf->getData(), mExifBuf, exifSize);
        // Copy the final JPEG stream into the final destination buffer, but exclude the SOI marker
        char *copyTo = (char*)destBuf->getData() + exifSize;
        if(!mConfig.jpegfromdriver) {
            copyFrom = (char*)mOutData + sizeof(JPEG_MARKER_SOI);
        } else {
            copyFrom = (char*)mainBuf + sizeof(JPEG_MARKER_SOI);
        }
        memcpy(copyTo, copyFrom, mainSize - sizeof(JPEG_MARKER_SOI));
    }
    LOG1("Total JPEG size: %d (time to encode: %ums)", totalSize, (unsigned)((systemTime() - startTime) / 1000000));
    return status;
}


status_t PictureThread::encode(CameraBuffer *snaphotBuf,CameraBuffer *interBuf, CameraBuffer *postviewBuf)
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_ENCODE;
    msg.data.encode.snaphotBuf = snaphotBuf;
    msg.data.encode.interBuf = interBuf;
    msg.data.encode.postviewBuf = postviewBuf;
    status_t ret = INVALID_OPERATION;
    if (snaphotBuf != 0)
        snaphotBuf->incrementProcessor();
    if (interBuf != 0)
        interBuf->incrementProcessor();
    if (postviewBuf != 0)
        postviewBuf->incrementProcessor();
    if ((ret = mMessageQueue.send(&msg)) != NO_ERROR) {
        if (snaphotBuf != 0)
            snaphotBuf->decrementProcessor();
        if (interBuf != 0)
            interBuf->decrementProcessor();
        if (postviewBuf != 0)
            postviewBuf->decrementProcessor();
    }
    return ret;
}

void PictureThread::getDefaultParameters(CameraParameters *params)
{
    LOG1("@%s", __FUNCTION__);
    if (!params) {
        ALOGE("null params");
        return;
    }

    params->setPictureFormat(CameraParameters::PIXEL_FORMAT_JPEG);
    params->set(CameraParameters::KEY_SUPPORTED_PICTURE_FORMATS,
            CameraParameters::PIXEL_FORMAT_JPEG);
    params->set(CameraParameters::KEY_JPEG_QUALITY, "80");
    params->set(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY, "50");
}

void PictureThread::setConfig(Config *config)
{
    mConfig = *config;
    if(mOutData != NULL)
        delete []mOutData;
    mMaxOutDataSize = (mConfig.picture.width * mConfig.picture.height * 3/2);
    if(mMaxOutDataSize != 0)
        mOutData = new unsigned char[mMaxOutDataSize];

    if (mExifBuf != NULL)
        delete mExifBuf;
    mExifBuf = new unsigned char[MAX_EXIF_SIZE];
}

status_t PictureThread::flushBuffers()
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_FLUSH;
    mMessageQueue.remove(MESSAGE_ID_ENCODE);
    return mMessageQueue.send(&msg, MESSAGE_ID_FLUSH);
}

status_t PictureThread::handleMessageExit()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    mThreadRunning = false;
    return status;
}

status_t PictureThread::handleMessageEncode(MessageEncode *msg)
{
    LOG1("@%s: snapshot ID = %d", __FUNCTION__, msg->snaphotBuf->getID());
    status_t status = NO_ERROR;
    CameraBuffer jpegBuf;
    void *snapshotbuff[3];
    void *thumbnailbuff[3];
    void *mainbuf = NULL;
    int mainSize = 0;
    int size=0;
    int alignPicHeight=0;
    int alignThumbnailHeight=0;

    if (mConfig.picture.width == 0 ||
        mConfig.picture.height == 0 ||
        mConfig.picture.format == 0) {
        ALOGE("Picture information not set yet!");
        if (msg->snaphotBuf != NULL)
        {
           msg->snaphotBuf->decrementProcessor();
           msg->interBuf->decrementProcessor();
        }
        return UNKNOWN_ERROR;
    }
    if((msg->snaphotBuf == NULL) || (msg->interBuf == NULL))
    {
        ALOGE("snaphotBuf or interBuf is NULL!");
        return UNKNOWN_ERROR;
    }

    // Encode the image
    alignPicHeight = msg->interBuf->GetRenderTargetHandle()->height;
    if(!mConfig.jpegfromdriver) {
        mVaConvertor->VPPBitBlit(msg->snaphotBuf->GetRenderTargetHandle(),msg->interBuf->GetRenderTargetHandle());
    }
    if(mConfig.exif.enableThumb)
    {
         if(msg->postviewBuf == NULL)
         {
             ALOGE("postviewBuf is NULL!");
             return UNKNOWN_ERROR;
         }
         alignThumbnailHeight = mConfig.thumbnail.height;
         mVaConvertor->VPPBitBlit(msg->interBuf->GetRenderTargetHandle(),msg->postviewBuf->GetRenderTargetHandle());
         msg->postviewBuf->LockGrallocData(thumbnailbuff,&size);
         if(!mConfig.jpegfromdriver) {
             msg->interBuf->LockGrallocData(snapshotbuff,&size);
             mainbuf = snapshotbuff[0];
             mainSize = size;
         } else {
             mainbuf = msg->snaphotBuf->getData();
             mainSize = msg->snaphotBuf->getDataSize();
         }
         if ((status = encodeToJpeg(mainbuf, mainSize, thumbnailbuff[0], &jpegBuf,msg->interBuf->GetGraStride(),msg->postviewBuf->GetGraStride(),alignPicHeight,alignThumbnailHeight)) == NO_ERROR) {
               mCallbacks->compressedRawFrameDone(msg->snaphotBuf);
               mCallbacks->compressedFrameDone(&jpegBuf);
         } else {
           ALOGE("Error generating JPEG image!");
         }
         msg->postviewBuf->UnLockGrallocData();
         if(!mConfig.jpegfromdriver) {
             msg->interBuf->UnLockGrallocData();
         }
    }
    else
    {
         if(!mConfig.jpegfromdriver) {
            msg->interBuf->LockGrallocData(snapshotbuff,&size);
            mainbuf = snapshotbuff[0];
            mainSize = size;
         } else {
            mainbuf = msg->snaphotBuf->getData();
            mainSize = msg->snaphotBuf->getDataSize();
         }
         if ((status = encodeToJpeg(mainbuf, mainSize, NULL, &jpegBuf,msg->interBuf->GetGraStride(),0,alignPicHeight,0)) == NO_ERROR) {
               mCallbacks->compressedRawFrameDone(msg->snaphotBuf);
               mCallbacks->compressedFrameDone(&jpegBuf);
         } else {
           ALOGE("Error generating JPEG image!");
         }
         if(!mConfig.jpegfromdriver) {
             msg->interBuf->UnLockGrallocData();
         }
    }
    // When the encoding is done, send back the buffers to camera
    if (msg->snaphotBuf != NULL)
        msg->snaphotBuf->decrementProcessor();
    if (msg->interBuf!= NULL)
        msg->interBuf->decrementProcessor();
    if (msg->postviewBuf!= NULL)
        msg->postviewBuf->decrementProcessor();

    LOG1("Releasing jpegBuf @%p", jpegBuf.getData());
    jpegBuf.releaseMemory();

    return status;
}

status_t PictureThread::handleMessageFlush()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    if(mVaConvertor)
       mVaConvertor->stop();

    mMessageQueue.reply(MESSAGE_ID_FLUSH, status);
    return status;
}

status_t PictureThread::waitForAndExecuteMessage()
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    Message msg;
    mMessageQueue.receive(&msg);

    switch (msg.id) {

        case MESSAGE_ID_EXIT:
            status = handleMessageExit();
            break;

        case MESSAGE_ID_ENCODE:
            status = handleMessageEncode(&msg.data.encode);
            break;

        case MESSAGE_ID_FLUSH:
            status = handleMessageFlush();
            break;

        default:
            status = BAD_VALUE;
            break;
    };
    return status;
}

bool PictureThread::threadLoop()
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    mThreadRunning = true;
    while (mThreadRunning)
        status = waitForAndExecuteMessage();

    return status;
}

status_t PictureThread::requestExitAndWait()
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_EXIT;

    // tell thread to exit
    // send message asynchronously
    mMessageQueue.send(&msg);

    // propagate call to base class
    return Thread::requestExitAndWait();
}

} // namespace android
