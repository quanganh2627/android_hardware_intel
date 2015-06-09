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

#include "PerformanceTraces.h"
#include "PictureThread.h"
#include "LogHelper.h"
#include "Callbacks.h"
#include "CallbacksThread.h"
#include "ImageScaler.h"
#include "MemoryUtils.h"
#include "PlatformData.h"
#include <utils/Timers.h>
#include "SWJpegEncoder.h"

namespace android {

static const unsigned char JPEG_MARKER_SOI[2] = {0xFF, 0xD8}; // JPEG StartOfImage marker
static const unsigned char JPEG_MARKER_EOI[2] = {0xFF, 0xD9}; // JPEG EndOfImage marker
static const int SIZE_OF_APP0_MARKER = 18;      /* Size of the JFIF App0 marker
                                                 * This is introduced by SW encoder after
                                                 * SOI. And sometimes needs to be removed
                                                 */
PictureThread::PictureThread(I3AControls *aaaControls, sp<ScalerService> scaler,
                             sp<CallbacksThread> callbacksThread, Callbacks *callbacks) :
    Thread(true) // callbacks may call into java
    ,mMessageQueue("PictureThread", MESSAGE_ID_MAX)
    ,mThreadRunning(false)
    ,mCallbacks(callbacks)
    ,mCallbacksThread(callbacksThread)
    ,mHwCompressor(NULL)
    ,mExifMaker(NULL)
    ,mExifBuf(AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_SNAPSHOT_JPEG))
    ,mOutBuf(AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_SNAPSHOT_JPEG))
    ,mThumbBuf(AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_POSTVIEW))
    ,mScaledPic(AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_SNAPSHOT))
    ,mPictureQuality(80)
    ,mThumbnailQuality(50)
    ,mInputBufferArray(NULL)
    ,mInputBuffDataArray(NULL)
    ,mInputBuffers(0)
    ,mPostviewBufferArray(NULL)
    ,mPostviewBuffers(0)
    ,mScaler(scaler)
    ,mMaxOutJpegBufSize(0)
    ,m3AControls(aaaControls)
{
    LOG1("@%s", __FUNCTION__);

    mHwCompressor = new JpegHwEncoder();
    if (mHwCompressor == NULL) {
        ALOGE("HwCompressor allocation failed");
    }

    // TODO: Remove the EXIFMaker's dependency on aaaControls
    mExifMaker = new EXIFMaker(aaaControls);
    if (mExifMaker == NULL) {
        ALOGE("ExifMaker allocation failed");
    }
}

PictureThread::~PictureThread()
{
    LOG1("@%s", __FUNCTION__);

    ALOGD("@%s: release mOutBuf", __FUNCTION__);
    MemoryUtils::freeAtomBuffer(mOutBuf);

    ALOGD("@%s: release mExifBuf", __FUNCTION__);
    MemoryUtils::freeAtomBuffer(mExifBuf);

    ALOGD("@%s: release mThumbBuf", __FUNCTION__);
    MemoryUtils::freeAtomBuffer(mThumbBuf);

    ALOGD("@%s: release mScaledPic", __FUNCTION__);
    MemoryUtils::freeAtomBuffer(mScaledPic);


    ALOGD("@%s: release InputBuffers", __FUNCTION__);
    freeInputBuffers();

    ALOGD("@%s: release postview buffers", __FUNCTION__);
    freePostviewBuffers();

    if (mHwCompressor) {
        ALOGD("@%s: release mHwCompressor", __FUNCTION__);
        delete mHwCompressor;
        mHwCompressor = NULL;
    }

    if (mExifMaker) {
        ALOGD("@%s: release mExifMaker", __FUNCTION__);
        delete mExifMaker;
        mExifMaker = NULL;
    }
}

/*
 * encodeToJpeg: encodes the given buffer and creates the final JPEG file
 * It allocates the memory for the final JPEG that contains EXIF(with thumbnail)
 * plus main picture
 * Input:  mainBuf  - buffer containing the main picture image
 *         thumbBuf - buffer containing the thumbnail image (optional, can be NULL)
 * Output: destBuf  - buffer containing the final JPEG image including EXIF header
 *         Note that, if present, thumbBuf will be included in EXIF header
 */
status_t PictureThread::encodeToJpeg(AtomBuffer *mainBuf, AtomBuffer *thumbBuf, AtomBuffer *destBuf)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    nsecs_t startTime = systemTime();
    bool failback = false;

    size_t bufferSize = (mainBuf->width * mainBuf->height * 2);
    if (mOutBuf.dataPtr != NULL && bufferSize != (size_t) mOutBuf.size) {
        MemoryUtils::freeAtomBuffer(mOutBuf);
    }

    if (mOutBuf.dataPtr == NULL) {
        mCallbacks->allocateMemory(&mOutBuf, bufferSize);
    }
    if (mExifBuf.dataPtr == NULL) {
        mCallbacks->allocateMemory(&mExifBuf, EXIF_SIZE_LIMITATION + sizeof(JPEG_MARKER_SOI));
    }
    if (mOutBuf.dataPtr == NULL) {
        ALOGE("Could not allocate memory for temp buffer!");
        return NO_MEMORY;
    }
    LOG1("Out buffer: @%p (%d bytes)", mOutBuf.dataPtr, mOutBuf.size);
    LOG1("Exif buffer: @%p (%d bytes)", mExifBuf.dataPtr, mExifBuf.size);

    status = scaleMainPic(mainBuf);
    if (status == NO_ERROR) {
       mainBuf = &mScaledPic;
    }

    // Start encoding main picture using HW encoder
    if (mainBuf->type != ATOM_BUFFER_PANORAMA) {
        status = startHwEncoding(mainBuf);
        if(status != NO_ERROR)
            failback = true;
    } else
        failback = true;

    // Convert and encode the thumbnail, if present and EXIF maker is initialized
    if (mExifMaker->isInitialized())
    {
        encodeExif(thumbBuf);
    }

    if (failback) {  // Encode main picture with SW encoder
        status = doSwEncode(mainBuf, destBuf);
    } else {
        status = completeHwEncode(mainBuf, destBuf);
    }

    if (status != NO_ERROR)
        ALOGE("Error while encoding JPEG");
    else {
        /* Update the fields in the AtomBuffer structure */
        destBuf->width = mainBuf->width;
        destBuf->height = mainBuf->height;
        destBuf->fourcc = V4L2_PIX_FMT_JPEG;
    }

    PERFORMANCE_TRACES_BREAKDOWN_STEP_PARAM("frameEncoded", mainBuf->frameCounter);
    LOG1("Total JPEG size: %d (time to encode: %ums)", destBuf->size, (unsigned)((systemTime() - startTime) / 1000000));
    return status;
}

status_t PictureThread::encode(MetaData &metaData, AtomBuffer *snaphotBuf, AtomBuffer *postviewBuf)
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_ENCODE;
    msg.data.encode.metaData = metaData;
    msg.data.encode.snaphotBuf = *snaphotBuf;
    mCallbacksThread->rawFrameDone(snaphotBuf);
    if (postviewBuf) {
        msg.data.encode.postviewBuf = *postviewBuf;
        mCallbacksThread->postviewFrameDone(postviewBuf);
    } else {
        // thumbnail is optional
        LOG1("@%s, encoding without Thumbnail", __FUNCTION__);
        msg.data.encode.postviewBuf.buff = NULL;
        msg.data.encode.postviewBuf.dataPtr = NULL;
    }
    return mMessageQueue.send(&msg);
}

void PictureThread::getDefaultParameters(CameraParameters *params)
{
    LOG1("@%s", __FUNCTION__);
    if (!params) {
        ALOGE("null params");
        return;
    }

    params->set(CameraParameters::KEY_ROTATION, "0");
    params->setPictureFormat(CameraParameters::PIXEL_FORMAT_JPEG);
    params->set(CameraParameters::KEY_SUPPORTED_PICTURE_FORMATS,
            CameraParameters::PIXEL_FORMAT_JPEG);
    params->set(CameraParameters::KEY_JPEG_QUALITY, "80");
    params->set(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY, "50");
}

status_t PictureThread::initialize(const CameraParameters &params, int zoomRatio)
{
    LOG1("@%s", __FUNCTION__);

    Message msg;
    msg.id = MESSAGE_ID_INITIALIZE;
    msg.data.param.params = &params;
    msg.data.param.zoomRatio = zoomRatio;

    return mMessageQueue.send(&msg, MESSAGE_ID_INITIALIZE);
}

status_t PictureThread::handleMessageInitialize(MessageParam *msg)
{
    LOG1("@%s", __FUNCTION__);
    CameraParameters params = *msg->params;
    int zoomRatio = msg->zoomRatio;

    mExifMaker->initialize(params, zoomRatio);
    int q = params.getInt(CameraParameters::KEY_JPEG_QUALITY);
    if (q != 0)
        mPictureQuality = q;
    q = params.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY);
    if (q != 0)
        mThumbnailQuality = q;

    mThumbBuf.fourcc = PlatformData::getPreviewPixelFormat();
    mThumbBuf.width = params.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH);
    mThumbBuf.height = params.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT);
    mThumbBuf.bpl = pixelsToBytes(mThumbBuf.fourcc, mThumbBuf.width);
    mThumbBuf.size = frameSize(mThumbBuf.fourcc, mThumbBuf.width, mThumbBuf.height);
    if (mThumbBuf.dataPtr != NULL) {
        MemoryUtils::freeAtomBuffer(mThumbBuf);
    }

    params.getPictureSize(&mScaledPic.width, &mScaledPic.height);
    mScaledPic.bpl = mScaledPic.width;
    mScaledPic.size = frameSize(mScaledPic.fourcc, bytesToPixels(mScaledPic.fourcc, mScaledPic.bpl), mScaledPic.height);

    mMessageQueue.reply(MESSAGE_ID_INITIALIZE, NO_ERROR);
    return NO_ERROR;
}

status_t PictureThread::allocSnapshotBuffers(const AtomBuffer& formatDescriptor,
                                             int sharedBuffersNum,
                                             Vector<AtomBuffer> *bufs,
                                             bool registerToScaler)
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_ALLOC_BUFS;
    msg.data.alloc.formatDesc = formatDescriptor;
    msg.data.alloc.numBufs = sharedBuffersNum;
    msg.data.alloc.bufs = bufs;
    msg.data.alloc.registerToScaler = registerToScaler;

    return mMessageQueue.send(&msg, MESSAGE_ID_ALLOC_BUFS);
}

status_t PictureThread::allocPostviewBuffers(const AtomBuffer& formatDescriptor,
                                             int sharedBuffersNum,
                                             Vector<AtomBuffer> *bufs,
                                             bool registerToScaler)
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_ALLOC_POSTVIEW_BUFS;
    msg.data.alloc.formatDesc = formatDescriptor;
    msg.data.alloc.numBufs = sharedBuffersNum;
    msg.data.alloc.bufs = bufs;
    msg.data.alloc.registerToScaler = registerToScaler;

    return mMessageQueue.send(&msg, MESSAGE_ID_ALLOC_POSTVIEW_BUFS);
}

status_t PictureThread::wait()
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_WAIT;
    return mMessageQueue.send(&msg, MESSAGE_ID_WAIT);
}

status_t PictureThread::flushBuffers()
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_FLUSH;

    // we own the dynamically allocated MetaData, so free
    // data of pending message before flushing them
    Vector<Message> pending;
    mMessageQueue.remove(MESSAGE_ID_ENCODE, &pending);
    Vector<Message>::iterator it;
    for(it = pending.begin(); it != pending.end(); ++it) {
      it->data.encode.metaData.free(m3AControls);
    }

    return mMessageQueue.send(&msg, MESSAGE_ID_FLUSH);
}

status_t PictureThread::handleMessageExit()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    mThreadRunning = false;
    return status;
}

/**
 * Frees resources tired to metaData object.
 */
void PictureThread::MetaData::free(I3AControls *aaaControls)
{
    if (ia3AMkNote)
        aaaControls->put3aMakerNote(ia3AMkNote);

    if (atomispMkNote) {
        delete atomispMkNote;
        atomispMkNote = NULL;
    }

    if (aeConfig) {
        delete aeConfig;
        aeConfig = NULL;
    }
}

/**
 * Passes the picture metadata to EXIFMaker.
 */
void PictureThread::setupExifWithMetaData(const PictureThread::MetaData &metaData)
{
    mExifMaker->pictureTaken();
    if (metaData.atomispMkNote)
        mExifMaker->setDriverData(*metaData.atomispMkNote);
    if (metaData.ia3AMkNote)
        mExifMaker->setMakerNote(*metaData.ia3AMkNote);
    if (metaData.aeConfig)
        mExifMaker->setSensorAeConfig(*metaData.aeConfig);
    if (metaData.flashFired)
        mExifMaker->enableFlash();
}

status_t PictureThread::handleMessageEncode(MessageEncode *msg)
{
    LOG1("@%s: snapshot ID = %d", __FUNCTION__, msg->snaphotBuf.id);
    status_t status = NO_ERROR;
    AtomBuffer jpegBuf = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_SNAPSHOT_JPEG);

    if (msg->snaphotBuf.width == 0 ||
        msg->snaphotBuf.height == 0 ||
        msg->snaphotBuf.fourcc == 0) {
        ALOGE("Picture information not set yet!");
        return UNKNOWN_ERROR;
    }

    // prepare EXIF data
    setupExifWithMetaData(msg->metaData);

    // Encode the image
    AtomBuffer *postviewBuf;
    if (msg->postviewBuf.dataPtr == NULL)
        postviewBuf = NULL;
    else
        postviewBuf = &msg->postviewBuf;

    // Mirror snapshot and postview buffers if requested
    if (msg->metaData.saveMirrored) {
        mirrorBuffer(&msg->snaphotBuf, msg->metaData.currentOrientation, msg->metaData.cameraOrientation);
        if (postviewBuf)
            mirrorBuffer(postviewBuf, msg->metaData.currentOrientation, msg->metaData.cameraOrientation);
    }

    status = encodeToJpeg(&msg->snaphotBuf, postviewBuf, &jpegBuf);
    if (status != NO_ERROR) {
        ALOGE("Error generating JPEG image!");
        LOG1("Releasing jpegBuf @%p", jpegBuf.dataPtr);
        MemoryUtils::freeAtomBuffer(jpegBuf);
    }

    jpegBuf.frameCounter = msg->snaphotBuf.frameCounter;

    mCallbacksThread->compressedFrameDone(&jpegBuf, &msg->snaphotBuf, &msg->postviewBuf);

    // ownership was transferred to us from ControlThread, so we need
    // to free resources here after encoding
    msg->metaData.free(m3AControls);

    return status;
}

status_t PictureThread::handleMessageAllocBufs(MessageAllocBufs *msg)
{
    LOG1("@%s: width = %d, height = %d, fourcc = %s, numBufs = %d",
            __FUNCTION__,
            msg->formatDesc.width,
            msg->formatDesc.height,
            v4l2Fmt2Str(msg->formatDesc.fourcc),
            msg->numBufs);
    status_t status = NO_ERROR;
    size_t bufferSize = frameSize(msg->formatDesc.fourcc, msg->formatDesc.width, msg->formatDesc.height);

    /* check if re-allocation is needed */
    if( (mInputBufferArray != NULL) &&
        (mInputBuffers == msg->numBufs) &&
        (mInputBufferArray[0].width == msg->formatDesc.width) &&
        (mInputBufferArray[0].height == msg->formatDesc.height) &&
        (mInputBufferArray[0].fourcc == msg->formatDesc.fourcc)) {
        LOG1("Trying to allocate same number of buffers with same resolution... skipping");
        goto skip;
    }

    /* Free old buffers if already allocated */
    if (bufferSize != (size_t) mOutBuf.size) {
        MemoryUtils::freeAtomBuffer(mOutBuf);
    }

    /* Allocate Output buffer : JPEG and EXIF */
    if (mOutBuf.dataPtr == NULL) {
        mCallbacks->allocateMemory(&mOutBuf, bufferSize);
    }
    if (mExifBuf.dataPtr == NULL) {
        mCallbacks->allocateMemory(&mExifBuf, EXIF_SIZE_LIMITATION + sizeof(JPEG_MARKER_SOI));
    }
    if (mOutBuf.dataPtr == NULL || mExifBuf.dataPtr == NULL) {
        ALOGE("Could not allocate memory for output buffers!");
        status = NO_MEMORY;
        goto exit_fail;
    }

    /* re-allocates array of input buffers into mInputBufferArray */
    freeInputBuffers();
    status = allocateInputBuffers(msg->formatDesc, msg->numBufs, msg->registerToScaler);
    if (status != NO_ERROR)
        goto exit_fail;

    /* Now let the encoder know about the new buffers for the surfaces*/
    if (mHwCompressor) {
        status = mHwCompressor->setInputBuffers(mInputBufferArray, mInputBuffers);
        if (status) {
            ALOGW("HW Encoder cannot use pre-allocate buffers");
            status = NO_ERROR; // this is not critical, we still return some buffers
        }
    }

skip:
    for (int i = 0; i < mInputBuffers; ++i) {
        msg->bufs->push(mInputBufferArray[i]);
    }

exit_fail:
    mMessageQueue.reply(MESSAGE_ID_ALLOC_BUFS, status);
    return status;
}

status_t PictureThread::handleMessageAllocPostviewBufs(MessageAllocBufs *msg)
{
    LOG1("@%s: width = %d, height = %d, fourcc = %s, numBufs = %d",
            __FUNCTION__,
            msg->formatDesc.width,
            msg->formatDesc.height,
            v4l2Fmt2Str(msg->formatDesc.fourcc),
            msg->numBufs);

    status_t status = NO_ERROR;

    /* check if re-allocation is needed */
    if( (mPostviewBufferArray != NULL) &&
        (mPostviewBuffers == msg->numBufs) &&
        (mPostviewBufferArray[0].width == msg->formatDesc.width) &&
        (mPostviewBufferArray[0].height == msg->formatDesc.height) &&
        (mPostviewBufferArray[0].fourcc == msg->formatDesc.fourcc)) {
        LOG1("Trying to allocate same number of postview buffers with same resolution... skipping");
        goto skip;
    }

    freePostviewBuffers();
    status = allocatePostviewBuffers(msg->formatDesc, msg->numBufs, msg->registerToScaler);
    if (status != NO_ERROR)
        goto exit_fail;

skip:
    for (int i = 0; i < mInputBuffers; ++i) {
        msg->bufs->push(mPostviewBufferArray[i]);
    }

exit_fail:
    mMessageQueue.reply(MESSAGE_ID_ALLOC_POSTVIEW_BUFS, status);
    return status;
}

status_t PictureThread::allocateInputBuffers(AtomBuffer& formatDescriptor, int numBufs, bool registerToScaler)
{
    LOG1("@%s size (%dx%d) num %d fourcc %s", __FUNCTION__, formatDescriptor.width,
         formatDescriptor.height, numBufs, v4l2Fmt2Str(formatDescriptor.fourcc));
    // temporary workaround until CSS supports buffers with different bpls
    // until then we need to align all buffers to display subsystem bpl
    // requirements.... even the snapshot buffers that do not go to screen
    int bpl = SGXandDisplayBpl(formatDescriptor.fourcc, formatDescriptor.width);
    LOG1("@%s bpl %d", __FUNCTION__, bpl);

    formatDescriptor.bpl = bpl;
    formatDescriptor.size = frameSize(formatDescriptor.fourcc,
                                      bytesToPixels(formatDescriptor.fourcc, bpl),
                                      formatDescriptor.height);

    if(numBufs == 0)
        return NO_ERROR;

    mInputBufferArray = new AtomBuffer[numBufs];
    mInputBuffDataArray = new char*[numBufs];
    if((mInputBufferArray == NULL) || mInputBuffDataArray == NULL)
        goto bailout;

    mInputBuffers = numBufs;

    for (int i = 0; i < mInputBuffers; i++) {
        mInputBufferArray[i] = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_SNAPSHOT);
        /*
         * For some use cases there is not enough graphic memory to allocate the snapshot
         * buffers. We limit the graphic allocation only when we need. This
         * is signaled by the boolean registerToScaler. In other cases allocate
         * from HEAP as usual
         */
        if (registerToScaler)
            MemoryUtils::allocateGraphicBuffer(mInputBufferArray[i], formatDescriptor);
        else
            MemoryUtils::allocateAtomBuffer(mInputBufferArray[i], formatDescriptor, mCallbacks);

        if (mInputBufferArray[i].dataPtr == NULL) {
            mInputBuffers = i;
            goto bailout;
        }

        mInputBufferArray[i].status = FRAME_STATUS_OK;
        mInputBuffDataArray[i] = (char *) mInputBufferArray[i].dataPtr;
        if (registerToScaler)
            mScaler->registerBuffer(mInputBufferArray[i], ScalerService::SCALER_OUTPUT);

        LOG2("Snapshot buffer[%d] allocated, ptr = %p",i,mInputBufferArray[i].dataPtr);

    }
    return NO_ERROR;

bailout:
    ALOGE("Error allocating input buffers");
    freeInputBuffers();
    return NO_MEMORY;
}

void PictureThread::freeInputBuffers()
{
    LOG1("@%s", __FUNCTION__);

    if (mInputBufferArray != NULL) {
       unregisterFromGpuScalerAndFree(mInputBufferArray, mInputBuffers);
       delete [] mInputBufferArray;
       mInputBufferArray = NULL;
       mInputBuffers = 0;
    }

    if (mInputBuffDataArray != NULL) {
       delete [] mInputBuffDataArray;
       mInputBuffDataArray = NULL;
    }
}

void PictureThread::unregisterFromGpuScalerAndFree(AtomBuffer bufferArray[], int numBuffs)
{
    LOG1("@%s", __FUNCTION__);
    for (int i = 0; i < numBuffs; ++i) {
        if (bufferArray[i].gfxInfo.scalerId != -1) {
            mScaler->unRegisterBuffer(bufferArray[i], ScalerService::SCALER_OUTPUT);
            bufferArray[i].gfxInfo.scalerId = -1;
        }
        MemoryUtils::freeAtomBuffer(bufferArray[i]);
    }
}

status_t PictureThread::allocatePostviewBuffers(const AtomBuffer &formatDescriptor,
                                                int numBufs, bool registerToScaler)
{
    LOG1("@%s", __FUNCTION__);

    status_t status = NO_ERROR;

    if (numBufs == 0)
        return NO_ERROR;

    mPostviewBufferArray = new AtomBuffer[numBufs];

    if (mPostviewBufferArray == NULL) {
        return NO_MEMORY;
    }

    mPostviewBuffers = numBufs;
    AtomBuffer postv = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_POSTVIEW);

    for (int i = 0; i < numBufs; ++i) {
        ALOGD("allocating postview %d", i+1);
        postv.buff = NULL;
        postv.size = 0;
        postv.dataPtr = NULL;

        if (registerToScaler) {
            MemoryUtils::allocateGraphicBuffer(postv, formatDescriptor);
        } else {
            mCallbacks->allocateMemory(&postv, formatDescriptor.size);
        }

        if (postv.dataPtr == NULL) {
            status = NO_MEMORY;
        } else if (status == NO_ERROR) {
            if (registerToScaler)
                mScaler->registerBuffer(postv, ScalerService::SCALER_OUTPUT);
            ALOGD("postview dataPtr %p", postv.dataPtr);
            mPostviewBufferArray[i] = postv;
        } else {
            status = UNKNOWN_ERROR;
        }

        if (status != NO_ERROR) {
            ALOGE("Error in postview allocation (%d)", status);
            mPostviewBuffers = i;
            freePostviewBuffers();
            break;
        }
    }

    return status;
}

void PictureThread::freePostviewBuffers()
{
    LOG1("@%s", __FUNCTION__);

    if (mPostviewBufferArray != NULL) {
        unregisterFromGpuScalerAndFree(mPostviewBufferArray, mPostviewBuffers);

        delete[] mPostviewBufferArray;
        mPostviewBufferArray = NULL;
        mPostviewBuffers = 0;
    }
}


/**
 * Encode Thumbnail picture into mOutBuf
 *
 * It encodes the Exif data into buffer exifDst
 *
 * returns encoded size if successful or
 * zero if encoding failed
 */
int PictureThread::encodeExifAndThumbnail(AtomBuffer *thumbBuf, unsigned char* exifDst)
{
    LOG1("@%s", __FUNCTION__);
    int size = 0;
    int exifSize = 0;
    nsecs_t endTime;
    SWJpegEncoder swEncoder;
    SWJpegEncoder::InputBuffer inBuf;
    SWJpegEncoder::OutputBuffer outBuf;

    if (thumbBuf == NULL || exifDst == NULL)
        goto exit;

    // Size 0x0 is not an error, handled as thumbnail off
    if (thumbBuf->width == 0 &&
        thumbBuf->height == 0)
        goto exit;

    if (thumbBuf->dataPtr == NULL) {
        ALOGW("Emtpy buffer was sent for thumbnail");
        goto exit;
    } else {
        inBuf.buf = (unsigned char*)thumbBuf->dataPtr;
    }

    // setup the SWJpegEncoder input and output buffers
    inBuf.width = thumbBuf->width;
    inBuf.height = thumbBuf->height;
    inBuf.fourcc = thumbBuf->fourcc;
    inBuf.size = frameSize(thumbBuf->fourcc, thumbBuf->width, thumbBuf->height);

    outBuf.buf = (unsigned char*)mOutBuf.dataPtr;
    outBuf.width = thumbBuf->width;
    outBuf.height = thumbBuf->height;
    outBuf.quality = mThumbnailQuality;
    outBuf.size = mOutBuf.size;

    // Set Exif data
    if (!mExifMakerName.isEmpty())
        mExifMaker->setMaker(mExifMakerName.string());

    if (!mExifModelName.isEmpty())
        mExifMaker->setModel(mExifModelName.string());

    if (!mExifSoftwareName.isEmpty())
        mExifMaker->setSoftware(mExifSoftwareName.string());

    do {
        endTime = systemTime();
        size = swEncoder.encode(inBuf, outBuf);
        LOG1("Thumbnail JPEG size: %d (time to encode: %ums)", size, (unsigned)((systemTime() - endTime) / 1000000));
        if (size > 0) {
            mExifMaker->setThumbnail(outBuf.buf, size);
        } else {
            // This is not critical, we can continue with main picture image
            ALOGE("Could not encode thumbnail stream!");
        }

        exifSize = mExifMaker->makeExif(&exifDst);
        outBuf.quality = outBuf.quality - 5;

    } while (exifSize > 0 && size > 0 && outBuf.quality > 0  && !mExifMaker->isThumbnailSet());
exit:
    return exifSize;
}

status_t PictureThread::handleMessageWait()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    mMessageQueue.reply(MESSAGE_ID_WAIT, status);
    return status;
}

status_t PictureThread::handleMessageFlush()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    // Now, flush the queued JPEG buffers from CallbacksThread
    status = mCallbacksThread->flushPictures();
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

        case MESSAGE_ID_ALLOC_BUFS:
            status = handleMessageAllocBufs(&msg.data.alloc);
            break;

        case MESSAGE_ID_ALLOC_POSTVIEW_BUFS:
            status = handleMessageAllocPostviewBufs(&msg.data.alloc);
            break;

        case MESSAGE_ID_WAIT:
            status = handleMessageWait();
            break;

        case MESSAGE_ID_FLUSH:
            status = handleMessageFlush();
            break;

        case MESSAGE_ID_INITIALIZE:
            status = handleMessageInitialize(&msg.data.param);
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

    return false;
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

/**
 * Start asynchronously the HW encoder.
 *
 * This may fail in that case we should failback to SW encoding
 *
 * \param mainBuf buffer containing the full resolution snapshot
 *
 */
status_t PictureThread::startHwEncoding(AtomBuffer* mainBuf)
{
    JpegHwEncoder::InputBuffer inBuf;
    JpegHwEncoder::OutputBuffer outBuf;
    nsecs_t endTime;
    status_t status = NO_ERROR;

    PERFORMANCE_TRACES_BREAKDOWN_STEP_PARAM("In",mainBuf->frameCounter);
    inBuf.clear();
    if (mainBuf->shared) {
        inBuf.buf = (unsigned char *) *((char **)mainBuf->dataPtr);
    } else {
        inBuf.buf = (unsigned char *) mainBuf->dataPtr;
    }

    inBuf.width = mainBuf->width;
    inBuf.height = mainBuf->height;
    inBuf.fourcc = mainBuf->fourcc;
    inBuf.bpl = mainBuf->bpl;
    inBuf.size = frameSize(mainBuf->fourcc, mainBuf->width, mainBuf->height);
    outBuf.clear();
    outBuf.width = mainBuf->width;
    outBuf.height = mainBuf->height;
    outBuf.quality = mPictureQuality;
    endTime = systemTime();
    if (mHwCompressor &&
        mHwCompressor->encodeAsync(inBuf, outBuf, mMaxOutJpegBufSize) == 0) {
        LOG1("Picture JPEG (time to start encode: %ums)", (unsigned)((systemTime() - endTime) / 1000000));
    } else {
        ALOGW("JPEG HW encoding failed, falling back to SW");
        status = INVALID_OPERATION;
    }

    return status;
}

/**
 * Generates the EXIF header for the final JPEG into the mExifBuf
 *
 * This will be finally pre-appended to the main JPEG
 * In case a thumbnail frame is passed it will be scaled to fit the thumbnail
 * resolution required and then compressed to JPEG and added to the EXIF data
 *
 * If no thumbnail is passed only the exif information is stored in mExfBuf
 *
 * \param thumbBuf buffer storing the thumbnail image.
 *
 */
void PictureThread::encodeExif(AtomBuffer *thumbBuf)
{
   LOG1("Encoding EXIF with thumb : %p", thumbBuf);

    // Downscale postview 2 thumbnail when needed
    if (mThumbBuf.size == 0) {
        // thumbnail off, postview gets discarded
        thumbBuf = &mThumbBuf;
    } else if (thumbBuf &&
        (mThumbBuf.width < thumbBuf->width ||
         mThumbBuf.height < thumbBuf->height ||
         mThumbBuf.width < thumbBuf->bpl)) {
        int srcHeighByThumbAspect = thumbBuf->width * mThumbBuf.height / mThumbBuf.width;
        mThumbBuf.fourcc = thumbBuf->fourcc;
        mThumbBuf.bpl = pixelsToBytes(mThumbBuf.fourcc, mThumbBuf.width);
        mThumbBuf.size = frameSize(mThumbBuf.fourcc, mThumbBuf.width, mThumbBuf.height);

        LOG1("Downscaling postview2thumbnail : %dx%d (%d) -> %dx%d (%d)",
                thumbBuf->width, thumbBuf->height, thumbBuf->bpl,
                mThumbBuf.width, mThumbBuf.height, mThumbBuf.bpl);
        if (mThumbBuf.dataPtr == NULL)
            mCallbacks->allocateMemory(&mThumbBuf,mThumbBuf.size);

        if (mThumbBuf.dataPtr == NULL) {
            ALOGE("Could not allocate memory for ThumbBuf buffers!");
            mThumbBuf.size = 0;
            mThumbBuf.width = 0;
            mThumbBuf.height = 0;
        } else if (thumbBuf->height > srcHeighByThumbAspect) {
            // Support cropping 16:9 out from 4:3
            int skipLines = (thumbBuf->height - srcHeighByThumbAspect) / 2;
            ALOGW("Thumbnail cropped to match requested aspect ratio");
            thumbBuf->height = srcHeighByThumbAspect;
            ImageScaler::downScaleImage(thumbBuf, &mThumbBuf, skipLines, skipLines);
        } else {
            ImageScaler::downScaleImage(thumbBuf, &mThumbBuf);
        }
        thumbBuf = &mThumbBuf;
    }

    unsigned char* currentPtr = (unsigned char*)mExifBuf.dataPtr;
    mExifBuf.size = 0;

    // Copy the SOI marker
    memcpy(currentPtr, JPEG_MARKER_SOI, sizeof(JPEG_MARKER_SOI));
    mExifBuf.size += sizeof(JPEG_MARKER_SOI);
    currentPtr += sizeof(JPEG_MARKER_SOI);

    // Set Exif data
    if (!mExifMakerName.isEmpty())
        mExifMaker->setMaker(mExifMakerName.string());

    if (!mExifModelName.isEmpty())
        mExifMaker->setModel(mExifModelName.string());

    if (!mExifSoftwareName.isEmpty())
        mExifMaker->setSoftware(mExifSoftwareName.string());

    // Encode thumbnail as JPEG and exif into mExifBuf
    int tmpSize = encodeExifAndThumbnail(thumbBuf, currentPtr);
    if (tmpSize == 0) {
        // This is not critical, we can continue with main picture image
        ALOGI("Exif created without thumbnail stream!");
        tmpSize = mExifMaker->makeExif(&currentPtr);
    }
    mExifBuf.size += tmpSize;
    currentPtr += mExifBuf.size;
}

/**
 * Does the encoding of the main picture using the SW encoder
 *
 * This is used in the failback scenario in case the HW encoder fails
 *
 * \param mainBuf the AtomBuffer with the full resolution snapshot
 * \param destBuf AtomBuffer where the final JPEG is stored
 *
 * This method allocates the memory for the final JPEG, that will be freed
 * in the CallbackThread once the jpeg has been given to the user.
 *
 * The final JPEG contains the EXIF header stored in mExifBuf plus the
 * JPEG bitstream for the full resolution snapshot
 */
status_t PictureThread::doSwEncode(AtomBuffer *mainBuf, AtomBuffer* destBuf)
{
    status_t status= NO_ERROR;
    nsecs_t endTime;
    SWJpegEncoder swEncoder;
    SWJpegEncoder::InputBuffer inBuf;
    SWJpegEncoder::OutputBuffer outBuf;
    int finalSize = 0;

    PERFORMANCE_TRACES_BREAKDOWN_STEP_PARAM("In",mainBuf->frameCounter);
    inBuf.clear();
    if (mainBuf->shared) {
        inBuf.buf = (unsigned char *) *((char **)mainBuf->dataPtr);
    } else {
        inBuf.buf = (unsigned char *) mainBuf->dataPtr;
    }

    int realWidth = (mainBuf->bpl > mainBuf->width)? mainBuf->bpl:
                                                       mainBuf->width;
    inBuf.width = realWidth;
    inBuf.height = mainBuf->height;
    inBuf.fourcc = mainBuf->fourcc;
    inBuf.size = frameSize(mainBuf->fourcc, mainBuf->width, mainBuf->height);
    outBuf.clear();
    outBuf.buf = (unsigned char*)mOutBuf.dataPtr;
    outBuf.width = realWidth;
    outBuf.height = mainBuf->height;
    outBuf.quality = mPictureQuality;
    outBuf.size = mOutBuf.size;
    endTime = systemTime();
    int mainSize = swEncoder.encode(inBuf, outBuf) - sizeof(JPEG_MARKER_SOI) - SIZE_OF_APP0_MARKER;
    LOG1("Picture JPEG size: %d (time to encode: %ums)", mainSize, (unsigned)((systemTime() - endTime) / 1000000));
    if (mainSize > 0) {
        finalSize = mExifBuf.size + mainSize;
    } else {
        ALOGE("Could not encode picture stream!");
        status = UNKNOWN_ERROR;
    }

    if (status == NO_ERROR) {
        mCallbacks->allocateMemory(destBuf, finalSize);
        if (destBuf->dataPtr == NULL) {
            ALOGE("No memory for final JPEG file!");
            status = NO_MEMORY;
        }
    }
    if (status == NO_ERROR) {
        destBuf->size = finalSize;
        // Copy EXIF (it will also have the SOI markers)
        memcpy(destBuf->dataPtr, mExifBuf.dataPtr, mExifBuf.size);
        // Copy the final /
        // JPEG stream into the final destination buffer
        // avoid the copying the SOI and APP0 but copy EOI marker
        char *copyTo = (char*)destBuf->dataPtr + mExifBuf.size;
        char *copyFrom = (char*)mOutBuf.dataPtr + sizeof(JPEG_MARKER_SOI) + SIZE_OF_APP0_MARKER;
        memcpy(copyTo, copyFrom, mainSize);

        destBuf->id = mainBuf->id;
    }

    return status;
}

/**
 * Waits for the HW encode to complete the JPEG encoding and completes the final
 * JPEG with the EXIF header
 *
 * \param mainBuf input, full resolution snapshot
 * \param destBuf output, jpeg encoded buffer
 *
 * The memory for the encoded jpeg will be allocated in this meethod. It will be
 * freed by the CallbackThread once the JPEG has been delivered to the client
 */
status_t PictureThread::completeHwEncode(AtomBuffer *mainBuf, AtomBuffer *destBuf)
{
    status_t status= NO_ERROR;
    unsigned int mainSize = 0;
    int finalSize = 0;
    unsigned char* dstPtr = NULL;

    //get JPEG size
    if (mHwCompressor->getOutputSize(mainSize) < 0) {
        ALOGE("Could not get Coded size!");
        return UNKNOWN_ERROR;
    }

    finalSize = mExifBuf.size + mainSize - sizeof(JPEG_MARKER_SOI);
    //allocate JPEG buffer base on the actual coded JPEG size
    mCallbacks->allocateMemory(destBuf, finalSize);
    if (destBuf->dataPtr == NULL) {
        ALOGE("No memory for final JPEG file!");
        status = NO_MEMORY;
    } else {
        destBuf->id = mainBuf->id;

        //get the JPEG data
        /*Since the jpeg got from libmix JPEG encoder start with SOI marker, and EXIF also have the SOI marker
         *so need to remove SOI marker fom jpeg data
         */
        dstPtr = (unsigned char*)destBuf->dataPtr + mExifBuf.size - sizeof(JPEG_MARKER_SOI);
        if (mHwCompressor->getOutput(dstPtr, mainSize) < 0) {
            ALOGE("Could not encode picture stream!");
            status = UNKNOWN_ERROR;
        } else {
            //Copy EXIF (it will also have the SOI marker)
            memcpy(destBuf->dataPtr, mExifBuf.dataPtr, mExifBuf.size);

            char *copyTo = (char*)destBuf->dataPtr + finalSize - sizeof(JPEG_MARKER_EOI);
            memcpy(copyTo, (void*)JPEG_MARKER_EOI, sizeof(JPEG_MARKER_EOI));
        }
    }

    return status;
}

/**
 * Scales the main picture to the resolution setup to the mScaledPic buffer
 * in case both resolutions are the same no scaling is done
 * mScaledPic resolution is setup during initialize()
 * The scled image is stored in the local buffer mScaledPic
 *
 * \param  mainBuf snapshot buffer to be scaled
 *
 * \return NO_ERROR in case the scale was done and successful
 * \return INVALID_OPERATION in case there was no need to scale
 * \return NO_MEMORY in case it could not allocate the scaled buffer.
 *
 */
status_t PictureThread::scaleMainPic(AtomBuffer *mainBuf)
{
    LOG1("%s",__FUNCTION__);
    status_t status = NO_ERROR;

    if ((mainBuf->width > mScaledPic.width) ||
        (mainBuf->height > mScaledPic.height) ||
        (mainBuf->bpl > mScaledPic.width)) {
        LOG1("Need to scale or trim from (%dx%d) s(%d)--> (%d,%d) s(%d)",mainBuf->width, mainBuf->height,mainBuf->bpl,
                                                      mScaledPic.width, mScaledPic.height, mScaledPic.bpl);

        MemoryUtils::freeAtomBuffer(mScaledPic);

        mCallbacks->allocateMemory(&mScaledPic, mScaledPic.size);
        if (mScaledPic.dataPtr == NULL)
            goto exit;

        ImageScaler::downScaleImage(mainBuf, &mScaledPic);
    } else {
        LOG1("No need to scale");
        status = INVALID_OPERATION;
    }
exit:
    return status;
}

void PictureThread::setExifMaker(const String8& data)
{
    LOG1("%s: name = %s",__FUNCTION__, data.string());
    mExifMakerName = data;
}

void PictureThread::setExifModel(const String8& data)
{
    LOG1("%s: name = %s",__FUNCTION__, data.string());
    mExifModelName = data;
}

void PictureThread::setExifSoftware(const String8& data)
{
    LOG1("%s: name = %s",__FUNCTION__, data.string());
    mExifSoftwareName = data;
}

} // namespace android
