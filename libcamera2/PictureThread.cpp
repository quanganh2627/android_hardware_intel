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
                             sp<CallbacksThread> callbacksThread, Callbacks *callbacks,
                             ICallbackPicture *pictureDone,
                             int cameraId) :
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
    ,mFirstPartBuf(AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_SNAPSHOT))
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
    ,mPictureDoneCallback(pictureDone)
    ,mCameraId(cameraId)
{
    LOG1("@%s", __FUNCTION__);

    mHwCompressor = new JpegHwEncoder();
    if (mHwCompressor == NULL) {
        LOGE("HwCompressor allocation failed");
    }

    // TODO: Remove the EXIFMaker's dependency on aaaControls
    mExifMaker = new EXIFMaker(aaaControls);
    if (mExifMaker == NULL) {
        LOGE("ExifMaker allocation failed");
    }
}

PictureThread::~PictureThread()
{
    LOG1("@%s", __FUNCTION__);

    LOG2("@%s: release mOutBuf", __FUNCTION__);
    MemoryUtils::freeAtomBuffer(mOutBuf);

    LOG2("@%s: release mExifBuf", __FUNCTION__);
    MemoryUtils::freeAtomBuffer(mExifBuf);

    LOG2("@%s: release mThumbBuf", __FUNCTION__);
    MemoryUtils::freeAtomBuffer(mThumbBuf);

    LOG2("@%s: release mScaledPic", __FUNCTION__);
    MemoryUtils::freeAtomBuffer(mScaledPic);

    LOG2("@%s: release mCapturePostViewBufList", __FUNCTION__);
    List<AtomBuffer>::iterator it = mCapturePostViewBufList.begin();
    while (it != mCapturePostViewBufList.end()) {
        MemoryUtils::freeAtomBuffer(*it);
        it = mCapturePostViewBufList.erase(it); // returns pointer to next item in list
    }

    LOG2("@%s: release InputBuffers", __FUNCTION__);
    freeInputBuffers();

    LOG2("@%s: release postview buffers", __FUNCTION__);
    freePostviewBuffers();

    if (mHwCompressor) {
        LOG2("@%s: release mHwCompressor", __FUNCTION__);
        delete mHwCompressor;
        mHwCompressor = NULL;
    }

    if (mExifMaker) {
        LOG2("@%s: release mExifMaker", __FUNCTION__);
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
 *         dataHasBeenFlushed - bool indicating whether data has been flushed from CPU cache
 * Output: destBuf  - buffer containing the final JPEG image including EXIF header
 *         Note that, if present, thumbBuf will be included in EXIF header
 */
status_t PictureThread::encodeToJpeg(AtomBuffer *mainBuf, AtomBuffer *thumbBuf, AtomBuffer *destBuf, bool dataHasBeenFlushed)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    nsecs_t startTime = systemTime();
    bool swFallback = false;

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
        LOGE("Could not allocate memory for temp buffer!");
        return NO_MEMORY;
    }
    LOG1("Out buffer: @%p (%d bytes)", mOutBuf.dataPtr, mOutBuf.size);
    LOG1("Exif buffer: @%p (%d bytes)", mExifBuf.dataPtr, mExifBuf.size);

    status = scaleMainPic(mainBuf);
    if (status == NO_ERROR) {
       mainBuf = &mScaledPic;
    }

    PERFORMANCE_TRACES_BREAKDOWN_STEP_PARAM("frameEncode starting", mainBuf->frameCounter);

    // TODO: Revert: This is a workaround to overcome
    // JPEG HW encoder limitation of properly encoding
    // pictures only with heights aligned to 8. So for non-8-aligned
    // we use SW encoder.
    const int JPEG_HEIGHT_ALIGN = 8;
    bool isAligned = (mainBuf->height % JPEG_HEIGHT_ALIGN == 0);

    // Start encoding main picture using HW encoder (except for panorama, which
    // often has resolution which the HW-encoder can't handle
    if (mainBuf->type != ATOM_BUFFER_PANORAMA && isAligned) {
        if (!dataHasBeenFlushed)
            MemoryUtils::flushMemory((char *)mainBuf->dataPtr, mainBuf->size);

        status = startHwEncoding(mainBuf);
        if(status != NO_ERROR) {
            LOG1("@%s, hw encoding failed", __FUNCTION__);
            swFallback = true;
        }
    } else {
        LOG1("@%s, using sw encoding", __FUNCTION__);
        swFallback = true;
    }

    // Convert and encode the thumbnail, if present and EXIF maker is initialized
    if (mExifMaker->isInitialized())
    {
        encodeExif(thumbBuf);
    }

    if (swFallback) {  // Encode main picture with SW encoder
        status = doSwEncode(mainBuf, destBuf);
    } else {
        status = completeHwEncode(mainBuf, destBuf);
    }

    if (status != NO_ERROR)
        LOGE("Error while encoding JPEG");
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

status_t PictureThread::encode(MetaData &metaData, AtomBuffer *snapshotBuf, AtomBuffer *postviewBuf, bool dataHasBeenFlushed)
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_ENCODE;
    msg.data.encode.metaData = metaData;
    msg.data.encode.snapshotBuf = *snapshotBuf;
    msg.data.encode.dataHasBeenFlushed = dataHasBeenFlushed;
    mCallbacksThread->rawFrameDone(snapshotBuf);
    if (postviewBuf) {
        msg.data.encode.postviewBuf = *postviewBuf;
        mCallbacksThread->postviewFrameDone(postviewBuf);
    } else {
        // thumbnail is optional
        LOG1("@%s, encoding without Thumbnail", __FUNCTION__);
        msg.data.encode.postviewBuf = AtomBufferFactory::createAtomBuffer(); // init fields
        msg.data.encode.postviewBuf.buff = NULL;
        msg.data.encode.postviewBuf.dataPtr = NULL;
    }
    return mMessageQueue.send(&msg);
}

void PictureThread::getDefaultParameters(CameraParameters *params, int cameraId)
{
    LOG1("@%s", __FUNCTION__);
    if (!params) {
        LOGE("null params");
        return;
    }

    params->set(CameraParameters::KEY_ROTATION, "0");
    params->setPictureFormat(CameraParameters::PIXEL_FORMAT_JPEG);
    params->set(CameraParameters::KEY_SUPPORTED_PICTURE_FORMATS,
            CameraParameters::PIXEL_FORMAT_JPEG);
    mPictureQuality = PlatformData::defaultJpegQuality(cameraId);
    params->set(CameraParameters::KEY_JPEG_QUALITY, mPictureQuality);
    mThumbnailQuality = PlatformData::defaultJpegThumbnailQuality(cameraId);
    params->set(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY, mThumbnailQuality);
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

    mThumbBuf.fourcc = PlatformData::getPreviewPixelFormat(mCameraId);
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
        LOG1("@%s gives buffers back to owner", __FUNCTION__);
        mPictureDoneCallback->pictureDone(&it->data.encode.snapshotBuf, &it->data.encode.postviewBuf);
    }

    mCapturePostViewBufListLock.lock();
    List<AtomBuffer>::iterator itPostView = mCapturePostViewBufList.begin();
    while (itPostView != mCapturePostViewBufList.end()) {
        MemoryUtils::freeAtomBuffer(*itPostView);
        itPostView = mCapturePostViewBufList.erase(itPostView); // returns pointer to next item in list
    }
    mCapturePostViewBufListLock.unlock();

    Vector<Message> pendingCapture;
    mMessageQueue.remove(MESSAGE_ID_CAPTURE, &pendingCapture);
    for(it = pendingCapture.begin(); it != pendingCapture.end(); ++it) {
        LOG1("@%s returning capture buffers back to owner", __FUNCTION__);
        it->data.capture.captureBuf.owner->returnBuffer(&it->data.capture.captureBuf);
    }

    if (mFirstPartBuf.owner) {
        LOG1("@%s returning stored capture buffers back to owner", __FUNCTION__);
        mFirstPartBuf.owner->returnBuffer(&mFirstPartBuf);
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

bool PictureThread::atomIspNotify(IAtomIspObserver::Message *msg, const ObserverState state)
{
    LOG2("@%s", __FUNCTION__);
    // if this frame is identified to be carrying the thumbnail, the preview copy is
    // done here, lock protected, in the caller (observer) context, to ensure
    // that the preview frame is not returned and overwritten by the sensor before
    // creating the thumbnail

    if (!msg) {
        LOG1("Received observer state change");
        // We are currently not receiving MESSAGE_ID_END_OF_STREAM when stream
        // stops. Observer gets paused when device is about to be stopped and
        // after pausing, we no longer receive new frames for the same session.
        // Reset frame counter based on any observer state change
        return false;
    }

    // send to processing, if there is a connected capture buffer with the preview
    if (msg->id == MESSAGE_ID_FRAME && msg->data.frameBuffer.buff.auxBuf) {
        // copy frame for thumbnail when it is needed
        unsigned char *jpeginfo = ((unsigned char*)msg->data.frameBuffer.buff.auxBuf->dataPtr) + JPEG_INFO_START;
        unsigned char *nv12meta = ((unsigned char*)msg->data.frameBuffer.buff.auxBuf->dataPtr) + NV12_META_START;
        uint32_t thumbnailFrameId(getU32fromFrame(jpeginfo, JPEG_INFO_THUMBNAIL_FRAME_ID_ADDR));
        uint32_t nv12metaFrameCount(getU32fromFrame(nv12meta, NV12_META_FRAME_COUNT_ADDR));
        LOG2("@%s: thumbnailFrameId = %d, nv12metaFrameCount = %d", __FUNCTION__, thumbnailFrameId , nv12metaFrameCount);
        if (thumbnailFrameId == nv12metaFrameCount && thumbnailFrameId != 0) {
            LOG1("@%s: Copy frame for thumbnail. thumbnailFrameId = %d", __FUNCTION__, thumbnailFrameId);
            mPictureDoneCallback->atPostviewPresent();
            AtomBuffer capturePostViewBuf(AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_POSTVIEW));
            capturePostViewBuf.fourcc = msg->data.frameBuffer.buff.fourcc;
            capturePostViewBuf.width = msg->data.frameBuffer.buff.width;
            capturePostViewBuf.height = msg->data.frameBuffer.buff.height;
            capturePostViewBuf.bpl = msg->data.frameBuffer.buff.bpl;
            capturePostViewBuf.id = thumbnailFrameId;
            mCallbacks->allocateMemory(&capturePostViewBuf, msg->data.frameBuffer.buff.size);
            if (capturePostViewBuf.dataPtr == NULL) {
                LOGE("Failed to allocate memory for capturePostViewBuf");
            } else {
                memcpy(capturePostViewBuf.dataPtr, msg->data.frameBuffer.buff.dataPtr, capturePostViewBuf.size);
                Mutex::Autolock lock(mCapturePostViewBufListLock);
                mCapturePostViewBufList.push_back(capturePostViewBuf);
            }
        }

        // send message
        Message mesg;
        mesg.id = MESSAGE_ID_CAPTURE;
        mesg.data.capture.captureBuf = *msg->data.frameBuffer.buff.auxBuf;
        mMessageQueue.send(&mesg);
    }
    return false;
}

status_t PictureThread::handleMessageCapture(MessageCapture *msg)
{
    LOG2("@%s", __FUNCTION__);

    /*******
     * TODO: Remove when capture-related bugs are fixed
     *
     * temporary logging of jpeginfo and JPEG metainfo
     */
    char array[16];
    unsigned char *meta = ((unsigned char*)msg->captureBuf.dataPtr) + JPEG_INFO_START;
    strncpy(array, (char*) meta, 15);
    array[15] = 0;
    uint32_t yuvFrameIdDebug  = *(uint32_t*)(meta+0x17);
    yuvFrameIdDebug = be32toh(yuvFrameIdDebug);
    meta += 15;

    LOG2("@%s JPEGINFO '%s' yuvID(%d) mode: %02hhx count: %02hhx reserved: %02hhx %02hhx length: %02hhx %02hhx %02hhx %02hhx yuvID: %02hhx %02hhx %02hhx %02hhx thumbID: %02hhx %02hhx %02hhx %02hhx", __FUNCTION__, array, yuvFrameIdDebug,
         meta[0] , meta[1], meta[2], meta[3], meta[4], meta[5], meta[6], meta[7], meta[8] , meta[9], meta[10], meta[11], meta[12], meta[13], meta[14], meta[15]);

    meta = ((unsigned char*)msg->captureBuf.dataPtr) + NV12_META_START;
    strncpy(array, (char *) meta, 14);
    array[14] = 0;
    meta += 14;
    uint32_t frameCount = *(uint32_t*)(meta);
    frameCount = be32toh(frameCount);
    LOG2("@%s NV12METAINFO '%s' framecount(%d) %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx ", __FUNCTION__, array, frameCount,
         meta[0] , meta[1], meta[2], meta[3], meta[4], meta[5], meta[6], meta[7], meta[8] , meta[9], meta[10], meta[11], meta[12], meta[13], meta[14], meta[15]);
    /******* end temporary logging */

    unsigned char *jpeginfo = ((unsigned char*)msg->captureBuf.dataPtr) + JPEG_INFO_START;
    unsigned char *nv12meta = ((unsigned char*)msg->captureBuf.dataPtr) + NV12_META_START;

    // Check markers
    if (strncmp((char*) &jpeginfo[JPEG_INFO_START_MARKER_ADDR], JPEG_INFO_START_MARKER, sizeof(JPEG_INFO_START_MARKER) - 1) != 0 ||
        strncmp((char*) &jpeginfo[JPEG_INFO_END_MARKER_ADDR], JPEG_INFO_END_MARKER, sizeof(JPEG_INFO_END_MARKER) - 1) != 0 ||
        strncmp((char*) &nv12meta[NV12_META_START_MARKER_ADDR], NV12_META_START_MARKER, sizeof(NV12_META_START_MARKER) - 1) != 0 ||
        strncmp((char*) &nv12meta[NV12_META_END_MARKER_ADDR], NV12_META_END_MARKER, sizeof(NV12_META_END_MARKER) - 1) != 0) {
        LOGE("jpeg info or nv12 meta marker not found in frame. skip frame.");
        // return the buffer to isp -> putSnapshot
        msg->captureBuf.owner->returnBuffer(&msg->captureBuf);
        return NO_ERROR;
    }

    uint32_t yuvFrameId(getU32fromFrame(jpeginfo, JPEG_INFO_YUV_FRAME_ID_ADDR));
    uint32_t thumbnailFrameId(getU32fromFrame(jpeginfo, JPEG_INFO_THUMBNAIL_FRAME_ID_ADDR));
    uint32_t nv12metaFrameCount(getU32fromFrame(nv12meta, NV12_META_FRAME_COUNT_ADDR));
    // TODO: remove AF state debug along with the above "temporary" debug trace.
    uint16_t afState(getU16fromFrame(nv12meta, NV12_META_AF_STATE_ADDR));

    uint16_t qValue(getU16fromFrame(jpeginfo, JPEG_INFO_Q_VALUE_ADDR));
    uint32_t jpegSizeQValue(getU32fromFrame(jpeginfo, JPEG_INFO_JPEG_SIZE_Q_VALUE_ADDR));

    LOG2("@%s: yuvFrameId = %d, thumbnailFrameId = %d, nv12metaFrameCount = %d, nv12AfState = 0x%x", __FUNCTION__, yuvFrameId, thumbnailFrameId , nv12metaFrameCount, afState);

    switch (jpeginfo[JPEG_INFO_MODE_ADDR]) {
    case JPEG_FRAME_TYPE_META:
        // this is the default preview + metadata case. We just return the buffer.
        LOG2("@%s: normal preview with metadata.", __FUNCTION__);
        // return the buffer to isp -> putSnapshot
        msg->captureBuf.owner->returnBuffer(&msg->captureBuf);
        break;

    case JPEG_FRAME_TYPE_FULL:
        LOG1("@%s: full jpeg", __FUNCTION__);
        LOG2("@%s: qValue = %d, jpegSizeQValue = %d", __FUNCTION__,  qValue, jpegSizeQValue);
        assembleJpeg(&msg->captureBuf, NULL);
        // return the buffer to isp -> putSnapshot
        msg->captureBuf.owner->returnBuffer(&msg->captureBuf);
        LOG1("@%s: full jpeg done", __FUNCTION__);
        break;

    case JPEG_FRAME_TYPE_SPLIT:
        LOG1("@%s: split jpeg", __FUNCTION__);
        LOG2("@%s: qValue = %d, jpegSizeQValue = %d", __FUNCTION__,  qValue, jpegSizeQValue);
        switch (jpeginfo[JPEG_INFO_COUNT_ADDR]) {
        case 0x00:
            LOG1("@%s: split jpeg first part", __FUNCTION__);
            mFirstPartBuf = msg->captureBuf;
            break;
        case 0x01:
            LOG1("@%s: split jpeg second part", __FUNCTION__);
            assembleJpeg(&mFirstPartBuf, &msg->captureBuf);
            // return the buffer to isp -> putSnapshot
            mFirstPartBuf.owner->returnBuffer(&mFirstPartBuf);
            mFirstPartBuf.owner = NULL;
            msg->captureBuf.owner->returnBuffer(&msg->captureBuf);
            LOG1("@%s: split jpeg done", __FUNCTION__);
            break;
         default:
             LOGE("Unknown jpeg count!");
             // return the buffer to isp -> putSnapshot
             msg->captureBuf.owner->returnBuffer(&msg->captureBuf);
             return UNKNOWN_ERROR;
             break;
        }
        break;

    default:
        LOGE("Unknown jpeg mode!");
        // return the buffer to isp -> putSnapshot
        msg->captureBuf.owner->returnBuffer(&msg->captureBuf);
        return UNKNOWN_ERROR;
        break;
    }

    return NO_ERROR;
}

/*
 * Get jpeg data size from continuous jpeg capture frame
 */
uint32_t PictureThread::getJpegDataSize(const void* framePtr) const
{
    uint32_t result = *((uint32_t*)((uint8_t*)framePtr + JPEG_INFO_START + JPEG_INFO_SIZE_ADDR));

    //conversion from big-endia
    result = be32toh(result);

    LOG2("@%s: frame jpeg data size = %d", __FUNCTION__, result);

    return result;
}

void PictureThread::setupExifWithNv12Meta(AtomBuffer *mainBuf)
{
    LOG1("@%s", __FUNCTION__);
    unsigned char *nv12meta = (unsigned char*)mainBuf->dataPtr + NV12_META_START;
    uint32_t nv12metaFrameCount(getU32fromFrame(nv12meta, NV12_META_FRAME_COUNT_ADDR));
    LOG2("@%s: frame count = %d", __FUNCTION__, nv12metaFrameCount);
    uint32_t iso(getU32fromFrame(nv12meta, NV12_META_ISO_ADDR));
    uint32_t exposureBias(getU32fromFrame(nv12meta, NV12_META_EXPOSURE_BIAS_ADDR));
    uint32_t tv(getU32fromFrame(nv12meta, NV12_META_TV_ADDR));
    uint32_t bv(getU32fromFrame(nv12meta, NV12_META_BV_ADDR));
    uint32_t exposureTimeDenominator(getU32fromFrame(nv12meta, NV12_META_EXPOSURE_TIME_DENOMINATOR_ADDR));
    uint32_t flash(getU32fromFrame(nv12meta, NV12_META_FLASH_ADDR));
    uint16_t av(getU16fromFrame(nv12meta, NV12_META_AV_ADDR));

    LOG2("@%s: ISO = %d", __FUNCTION__, iso);
    LOG2("@%s: exposureBias = %d", __FUNCTION__, exposureBias);
    LOG2("@%s: TV = %d", __FUNCTION__, tv);
    LOG2("@%s: BV = %d", __FUNCTION__, bv);
    LOG2("@%s: exposureTimeDenominator = %d", __FUNCTION__, exposureTimeDenominator);
    LOG2("@%s: flash = %d", __FUNCTION__, flash);
    LOG2("@%s: av = %d", __FUNCTION__, av);

    mExifMaker->setExtIspAeConfig(iso, exposureBias, tv, bv, exposureTimeDenominator, av);

    if (flash != 0)
        mExifMaker->enableFlash();
}

status_t PictureThread::assembleJpeg(AtomBuffer *mainBuf, AtomBuffer *mainBuf2)
{
    LOG1("@%s", __FUNCTION__);
    status_t status(NO_ERROR);
    int finalSize(0);
    int mainSize(0);
    int mainSize1(0);
    int mainSize2(0);
    char *copyTo(0);

    AtomBuffer destBuf = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_SNAPSHOT_JPEG);
    AtomBuffer postviewBuf = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_POSTVIEW);

    uint32_t thumbnailId(getU32fromFrame((uint8_t*)mainBuf->dataPtr,
                                         JPEG_INFO_START + JPEG_INFO_YUV_FRAME_ID_ADDR));

    if (mExifBuf.dataPtr == NULL) {
        mCallbacks->allocateMemory(&mExifBuf, EXIF_SIZE_LIMITATION + sizeof(JPEG_MARKER_SOI));
    }

    /* Allocate Output buffer for thumbnail */
    if (mOutBuf.dataPtr == NULL) {
        mCallbacks->allocateMemory(&mOutBuf, EXIF_SIZE_LIMITATION);
    }

    // prepare EXIF data (focal length etc)
    mExifMaker->setDriverData(mMakerInfo);
    // Read exif info from META data
    setupExifWithNv12Meta(mainBuf);

    mCapturePostViewBufListLock.lock();
    while (!mCapturePostViewBufList.empty()) {
        List<AtomBuffer>::iterator it = mCapturePostViewBufList.begin();
        uint32_t postviewBufId = (it)->id;
        if (thumbnailId == postviewBufId) {
            postviewBuf = *(it);
            mCapturePostViewBufList.erase(it);
            break;
        } else if (thumbnailId > postviewBufId) {
            LOGW("Old thumbnail(%u) buffer in queue (expect %u)", postviewBufId, thumbnailId);
            mCapturePostViewBufList.erase(it);
        } else {
            LOGW("Could not find thumbnail (%u) for capture.", thumbnailId);
            break;
        }
    }
    mCapturePostViewBufListLock.unlock();

    if (postviewBuf.dataPtr != NULL) {
        LOG2("@%s: Thumbnail id, %u - %u", __FUNCTION__, postviewBuf.id,
             getU32fromFrame((uint8_t*)mainBuf->dataPtr, JPEG_INFO_START + JPEG_INFO_YUV_FRAME_ID_ADDR));
        encodeExif(&postviewBuf);
    } else {
        LOGW("No thumbnail available during JPEG assemble.");
        encodeExif(NULL);
    }

    MemoryUtils::freeAtomBuffer(postviewBuf);

    LOG2("@%s: part one JPEG frame count = %u", __FUNCTION__,
         getU32fromFrame((uint8_t*) mainBuf->dataPtr, JPEG_META_START + JPEG_META_FRAME_COUNT_ADDR));

    // skip SOI MARKER start of JPEG data because it is already in EXIF
    mainSize1 = getJpegDataSize(mainBuf->dataPtr) - sizeof(JPEG_MARKER_SOI);
    if (mainBuf2 == NULL) {
        mainSize = mainSize1;
    } else {
        mainSize2 = getJpegDataSize(mainBuf2->dataPtr);
        mainSize = mainSize1 + mainSize2;
        LOG2("@%s: part two JPEG frame count = %u", __FUNCTION__,
             getU32fromFrame((uint8_t*) mainBuf2->dataPtr, JPEG_META_START + JPEG_META_FRAME_COUNT_ADDR));
    }

    finalSize = mExifBuf.size + mainSize;
    //allocate JPEG buffer base on the actual coded JPEG size
    mCallbacks->allocateMemory(&destBuf, finalSize);
    if (destBuf.dataPtr == NULL) {
        LOGE("No memory for final JPEG file!");
        status = NO_MEMORY;
        return status;
    }

    //Copy EXIF (it will also have the SOI marker)
    memcpy(destBuf.dataPtr, mExifBuf.dataPtr, mExifBuf.size);

    // Copy Jpeg data
    if (mainBuf2 == NULL) {
        copyTo = (char*)destBuf.dataPtr + mExifBuf.size;
        memcpy(copyTo, (char*)mainBuf->dataPtr + JPEG_DATA_START + sizeof(JPEG_MARKER_SOI), mainSize1);
    } else {
        copyTo = (char*)destBuf.dataPtr + mExifBuf.size;
        memcpy(copyTo, (char*)mainBuf->dataPtr + JPEG_DATA_START + sizeof(JPEG_MARKER_SOI), mainSize1);
        copyTo = (char*)destBuf.dataPtr + mExifBuf.size + mainSize1;
        memcpy(copyTo, (char*)mainBuf2->dataPtr + JPEG_DATA_START, mainSize2);
    }

    /* Update the fields in the AtomBuffer structure */
    destBuf.width = mainBuf->width;
    destBuf.height = mainBuf->height;
    destBuf.fourcc = V4L2_PIX_FMT_JPEG;
    destBuf.frameCounter = mainBuf->frameCounter;
    destBuf.id = thumbnailId;

    mCallbacksThread->rawFrameDone(NULL);
    mCallbacksThread->compressedFrameDone(&destBuf, NULL, NULL);

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

    if (faceState.faces) {
        delete[] faceState.faces;
        faceState.faces = NULL;
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
    if (metaData.faceState.num_faces > 0)
        mExifMaker->setFaceData(metaData.faceState);
}

status_t PictureThread::handleMessageEncode(MessageEncode *msg)
{
    LOG1("@%s: snapshot ID = %d", __FUNCTION__, msg->snapshotBuf.id);
    status_t status = NO_ERROR;
    AtomBuffer jpegBuf = AtomBufferFactory::createAtomBuffer(ATOM_BUFFER_SNAPSHOT_JPEG);

    if (msg->snapshotBuf.width == 0 ||
        msg->snapshotBuf.height == 0 ||
        msg->snapshotBuf.fourcc == 0) {
        LOGE("Picture information not set yet!");
        return UNKNOWN_ERROR;
    }

    // prepare EXIF data
    setupExifWithMetaData(msg->metaData);

    // Encode the image
    AtomBuffer *postviewBuf;
    if (msg->postviewBuf.dataPtr == NULL || msg->postviewBuf.status == FRAME_STATUS_SKIPPED)
        postviewBuf = NULL;
    else
        postviewBuf = &msg->postviewBuf;

    // Mirror snapshot and postview buffers if requested
    if (msg->metaData.saveMirrored) {
        mirrorBuffer(&msg->snapshotBuf, msg->metaData.currentOrientation, msg->metaData.cameraOrientation);
        if (postviewBuf)
            mirrorBuffer(postviewBuf, msg->metaData.currentOrientation, msg->metaData.cameraOrientation);
    }

    status = encodeToJpeg(&msg->snapshotBuf, postviewBuf, &jpegBuf, msg->dataHasBeenFlushed);
    if (status != NO_ERROR) {
        LOGE("Error generating JPEG image!");
        LOG1("Releasing jpegBuf @%p", jpegBuf.dataPtr);
        MemoryUtils::freeAtomBuffer(jpegBuf);
    }

    jpegBuf.frameCounter = msg->snapshotBuf.frameCounter;

    mCallbacksThread->compressedFrameDone(&jpegBuf, &msg->snapshotBuf, &msg->postviewBuf);

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
        LOGE("Could not allocate memory for output buffers!");
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
            LOGW("HW Encoder cannot use pre-allocate buffers");
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
    int bpl = SGXandDisplayBpl(formatDescriptor.fourcc, formatDescriptor.bpl);
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
    LOGE("Error allocating input buffers");
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
        LOG1("allocating postview %d", i+1);
        postv.buff = NULL;
        postv.size = 0;
        postv.dataPtr = NULL;

        if (registerToScaler) {
            MemoryUtils::allocateGraphicBuffer(postv, formatDescriptor);
        } else {
            MemoryUtils::allocateAtomBuffer(postv, formatDescriptor, mCallbacks);
        }

        if (postv.dataPtr == NULL) {
            status = NO_MEMORY;
        } else if (status == NO_ERROR) {
            if (registerToScaler)
                mScaler->registerBuffer(postv, ScalerService::SCALER_OUTPUT);
            LOG1("postview dataPtr %p", postv.dataPtr);
            mPostviewBufferArray[i] = postv;
        } else {
            status = UNKNOWN_ERROR;
        }

        if (status != NO_ERROR) {
            LOGE("Error in postview allocation (%d)", status);
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
        LOGW("Emtpy buffer was sent for thumbnail");
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
            LOGE("Could not encode thumbnail stream!");
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

void PictureThread::setMakerNote(atomisp_makernote_info makerNote)
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_SET_MAKERNOTE;
    msg.data.maker.makerNote = makerNote;
    mMessageQueue.send(&msg);
}

status_t PictureThread::handleMessageSetMakernote(MessageSetMakernote *msg)
{
    LOG1("@%s", __FUNCTION__);
    mMakerInfo = msg->makerNote;
    return OK;
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

        case MESSAGE_ID_SET_MAKERNOTE:
            status = handleMessageSetMakernote(&msg.data.maker);
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

        case MESSAGE_ID_CAPTURE:
            status = handleMessageCapture(&msg.data.capture);
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

    inBuf.buf = (unsigned char *) mainBuf->dataPtr;
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
        LOGW("JPEG HW encoding failed, falling back to SW");
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
            LOGE("Could not allocate memory for ThumbBuf buffers!");
            mThumbBuf.size = 0;
            mThumbBuf.width = 0;
            mThumbBuf.height = 0;
        } else if (thumbBuf->height > srcHeighByThumbAspect) {
            // Support cropping 16:9 out from 4:3
            int skipLines = (thumbBuf->height - srcHeighByThumbAspect) / 2;
            LOGW("Thumbnail cropped to match requested aspect ratio");
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
        LOGI("Exif created without thumbnail stream!");
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

    int realWidth = (mainBuf->bpl > mainBuf->width)? mainBuf->bpl:
                                                       mainBuf->width;
    inBuf.buf = (unsigned char *) mainBuf->dataPtr;
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
        LOGE("Could not encode picture stream!");
        status = UNKNOWN_ERROR;
    }

    if (status == NO_ERROR) {
        mCallbacks->allocateMemory(destBuf, finalSize);
        if (destBuf->dataPtr == NULL) {
            LOGE("No memory for final JPEG file!");
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
        LOGE("Could not get Coded size!");
        return UNKNOWN_ERROR;
    }

    finalSize = mExifBuf.size + mainSize - sizeof(JPEG_MARKER_SOI);
    //allocate JPEG buffer base on the actual coded JPEG size
    mCallbacks->allocateMemory(destBuf, finalSize);
    if (destBuf->dataPtr == NULL) {
        LOGE("No memory for final JPEG file!");
        status = NO_MEMORY;
    } else {
        destBuf->id = mainBuf->id;

        //get the JPEG data
        /*Since the jpeg got from libmix JPEG encoder start with SOI marker, and EXIF also have the SOI marker
         *so need to remove SOI marker fom jpeg data
         */
        dstPtr = (unsigned char*)destBuf->dataPtr + mExifBuf.size - sizeof(JPEG_MARKER_SOI);
        if (mHwCompressor->getOutput(dstPtr, mainSize) < 0) {
            LOGE("Could not encode picture stream!");
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
