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

#define LOG_NDEBUG 1
#define LOG_TAG "IntelVideoEditorH263Encoder"
#include <utils/Log.h>
#include "OMX_Video.h"
#include <media/stagefright/MediaBufferGroup.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/Utils.h>
#include <media/stagefright/foundation/ADebug.h>
#include "IntelVideoEditorH263Encoder.h"
#define INIT_BUF_FULLNESS_RATIO 0.8125
#define INITIAL_INTRA_PERIOD (mVideoFrameRate * 2 / 3)
#define NORMAL_INTRA_PERIOD (mVideoFrameRate * 3)

namespace android {

IntelVideoEditorH263Encoder::IntelVideoEditorH263Encoder(
        const sp<MediaSource>& source,
        const sp<MetaData>& meta)
    : mSource(source),
      mMeta(meta),
      mUseSyncMode(0),
      mStarted(false),
      mFirstFrame(true),
      mFrameCount(0),
      mVAEncoder(NULL),
      mOutBufGroup(NULL),
      mLastInputBuffer(NULL) {

    LOGV("Construct IntelVideoEditorH263Encoder");
}

IntelVideoEditorH263Encoder::~IntelVideoEditorH263Encoder() {
    LOGV("Destruct IntelVideoEditorH263Encoder");
    if (mStarted) {
        stop();
    }
}

status_t IntelVideoEditorH263Encoder::initCheck(const sp<MetaData>& meta) {
    LOGV("initCheck");

    Encode_Status   encStatus;

    sp<MetaData> sourceFormat = mSource->getFormat();

    CHECK(sourceFormat->findInt32(kKeyWidth, &mVideoWidth));
    CHECK(sourceFormat->findInt32(kKeyHeight, &mVideoHeight));
    CHECK(sourceFormat->findInt32(kKeyFrameRate, &mVideoFrameRate));
    CHECK(sourceFormat->findInt32(kKeyColorFormat, &mVideoColorFormat));

    CHECK(sourceFormat->findInt32(kKeyBitRate, &mVideoBitRate));
    LOGV("mVideoWidth = %d, mVideoHeight = %d, mVideoFrameRate = %d, mVideoColorFormat = %d, mVideoBitRate = %d",
        mVideoWidth, mVideoHeight, mVideoFrameRate, mVideoColorFormat, mVideoBitRate);
    if (mVideoColorFormat != OMX_COLOR_FormatYUV420SemiPlanar) {
        LOGE("Color format %d is not supported", mVideoColorFormat);
        return BAD_VALUE;
    }
    mFrameSize = mVideoHeight* mVideoWidth* 1.5;
    /*
     * SET PARAMS FOR THE ENCODER BASED ON THE METADATA
     * */

    encStatus = mVAEncoder->getParameters(&mEncParamsCommon);
    CHECK(encStatus == ENCODE_SUCCESS);
    LOGV("got encoder params");

    mEncParamsCommon.resolution.width = mVideoWidth;
    mEncParamsCommon.resolution.height= mVideoHeight;
    mEncParamsCommon.frameRate.frameRateNum = mVideoFrameRate;
    mEncParamsCommon.frameRate.frameRateDenom = 1;
    mEncParamsCommon.rcMode = RATE_CONTROL_VBR;
    mEncParamsCommon.rcParams.bitRate = mVideoBitRate;
    mEncParamsCommon.rawFormat =  RAW_FORMAT_NV12;

    // Set intra period to be a small value so that more IDR will be generated
    // at the beginning of encoding. After a certain period of time, change intra period
    // to be a bigger value, NORMAL_INTRA_PERIOD, in the rest of encoding.
    // This is to workaround that it may take long to show video after clone / extended
    // mode switching. During mode swithing, the current Widi stack sends RTSP command to
    // set adaptor jitter buffer size. Widi adaptor may miss the first IDR during adaptor
    // jitter buffer size setting. If the first IDR is missed, Widi adaptor must wait for
    // the next IDR to arrive so that decoding can be started. If intra period is long,
    // it will take long to show video.
    mEncParamsCommon.intraPeriod = INITIAL_INTRA_PERIOD;

    mEncParamsCommon.rcParams.minQP  = 1;
    mEncParamsCommon.rcParams.initQP = 24;

    mEncParamsCommon.syncEncMode = mUseSyncMode;
    mFrameCount = 0;

    encStatus = mVAEncoder->setParameters(&mEncParamsCommon);
    CHECK(encStatus == ENCODE_SUCCESS);
    LOGV("new encoder params set");

    VideoParamsHRD hrdParam;
    encStatus = mVAEncoder->getParameters(&hrdParam);
    CHECK(encStatus == ENCODE_SUCCESS);
    LOGV("got encoder hrd params ");

    hrdParam.bufferSize = mVideoBitRate;
    hrdParam.initBufferFullness = hrdParam.bufferSize * INIT_BUF_FULLNESS_RATIO;

    encStatus = mVAEncoder->setParameters(&hrdParam);
    CHECK(encStatus == ENCODE_SUCCESS);
    LOGV("new  encoder hard params set");

    mOutBufGroup = new MediaBufferGroup();
    CHECK(mOutBufGroup != NULL);

    return OK;
}

status_t IntelVideoEditorH263Encoder::start(MetaData *params) {
    LOGV("start");
    status_t ret = OK;

    if (mStarted) {
        LOGW("Call start() when encoder already started");
        return OK;
    }

    mSource->start(params);

    mVAEncoder = createVideoEncoder("video/h263");

    if (mVAEncoder == NULL) {
        LOGE("Fail to create video encoder");
        return NO_MEMORY;
    }
    mInitCheck = initCheck(mMeta);

    if (mInitCheck != OK) {
        return mInitCheck;
    }

    uint32_t maxSize;
    mVAEncoder->getMaxOutSize(&maxSize);

    LOGV("allocating output buffers of size %d",maxSize);
    for (int i = 0; i < OUTPUT_BUFFERS; i++ ) {
        mOutBufGroup->add_buffer(new MediaBuffer(maxSize));
    }

    if (OK != getSharedBuffers()) {
        LOGE("Failed to get the shared buffers from encoder ");
        return UNKNOWN_ERROR;
    }

    Encode_Status err;
    err = mVAEncoder->start();
    if (err!= ENCODE_SUCCESS) {
        LOGE("Failed to initialize the encoder: %d", err);

        /* We should exit the sharedbuffer mode, when failing to
        create the HW video encoder.
        */

        androidCreateThread(SBShutdownFunc,this);
        LOGI("Successfull create thread to exit shared buffer mode!");

        mSource->stop();

        sp<BufferShareRegistry> r = BufferShareRegistry::getInstance();
        err = r->encoderRequestToDisableSharingMode();
        LOGV("encoderRequestToDisableSharingMode returned %d\n", err);

        /* libsharedbuffer wants the source to call this after the encoder calls
         * encoderRequestToDisableSharingMode. Instead of doing complicated
         * synchronization, let's just call this ourselves on the source's
         * behalf.
         */

        err = r->sourceRequestToDisableSharingMode();
        LOGV("sourceRequestToDisableSharingMode returned %d\n", err);

        releaseVideoEncoder(mVAEncoder);
        mVAEncoder = NULL;

        return UNKNOWN_ERROR;
    }

    if (OK != setSharedBuffers()) {
        LOGE("Failed to setup the shared buffers");
        return UNKNOWN_ERROR;
    }

    mStarted = true;
    LOGV("start- DONE");
    return OK;
}

int IntelVideoEditorH263Encoder::SBShutdownFunc(void* arg)
{
    LOGV("IntelVideoEditorAVCEncoder::SBShutdownFunc begin()");
    sp<BufferShareRegistry> r = BufferShareRegistry::getInstance();
    int error = r->sourceExitSharingMode();
    LOGV("sourceExitSharingMode returns %d",error);
    return 0;
}

status_t IntelVideoEditorH263Encoder::stop() {
    LOGV("stop");
    if (!mStarted) {
        LOGW("Call stop() when encoder has not started");
        return OK;
    }

    if (mOutBufGroup) {
        delete mOutBufGroup;
        mOutBufGroup = NULL;
    }
    if (mLastInputBuffer!=NULL) {
        mLastInputBuffer->release();
    }
    mLastInputBuffer = NULL;

    /* call mSource->stop in a new thread, so the source
        can do its end of shared buffer shutdown */

    androidCreateThread(SBShutdownFunc,this);
    LOGV("Successfull create thread!");

    /* do encoder's buffer sharing shutdown */
    sp<BufferShareRegistry> r = BufferShareRegistry::getInstance();
    int err = r->encoderExitSharingMode();
    LOGV("encoderExitSharingMode returned %d\n", err);

    mSource->stop();

    err = r->encoderRequestToDisableSharingMode();
    LOGV("encoderRequestToDisableSharingMode returned %d\n", err);

    /* libsharedbuffer wants the source to call this after the encoder calls
     * encoderRequestToDisableSharingMode. Instead of doing complicated
     * synchronization, let's just call this ourselves on the source's
     * behalf. */
    err = r->sourceRequestToDisableSharingMode();
    LOGV("sourceRequestToDisableSharingMode returned %d\n", err);

    releaseVideoEncoder(mVAEncoder);
    mVAEncoder = NULL;

    mStarted = false;
    LOGV("stop - DONE");

    return OK;
}

sp<MetaData> IntelVideoEditorH263Encoder::getFormat() {
    LOGV("getFormat");

    sp<MetaData> format = new MetaData;
    format->setInt32(kKeyWidth, mVideoWidth);
    format->setInt32(kKeyHeight, mVideoHeight);
    format->setInt32(kKeyBitRate, mVideoBitRate);
    format->setInt32(kKeySampleRate, mVideoFrameRate);
    format->setInt32(kKeyColorFormat, mVideoColorFormat);
    format->setCString(kKeyMIMEType, MEDIA_MIMETYPE_VIDEO_H263);
    format->setCString(kKeyDecoderComponent, "IntelVideoEditorH263Encoder");
    return format;
}

status_t IntelVideoEditorH263Encoder::read(MediaBuffer **out, const ReadOptions *options) {

    status_t err;
    Encode_Status encRet;
    MediaBuffer *tmpIn;
    int64_t timestamp = 0;
    CHECK(!options);
    mReadOptions = options;
    *out = NULL;

    LOGV("IntelVideoEditorAVCEncoder::read start");

    do {
        err = mSource->read(&tmpIn, NULL);
        if (err == INFO_FORMAT_CHANGED) {
            stop();
            start(NULL);
        }
    } while (err == INFO_FORMAT_CHANGED);

    if (err == ERROR_END_OF_STREAM) {
        return err;
    }
    else if (err != OK) {
        LOGE("Failed to read input video frame: %d", err);
        return err;
    }

    VideoEncRawBuffer vaInBuf;
    vaInBuf.flag = 0;
    vaInBuf.type = FTYPE_UNKNOWN;

    vaInBuf.data = (uint8_t *)tmpIn->data();
    vaInBuf.size = tmpIn->size();

    tmpIn->meta_data()->findInt64(kKeyTime, (int64_t *)&(vaInBuf.timeStamp));
    LOGV("Encoding: buffer %p, size = %d, ts= %llu",vaInBuf.data, vaInBuf.size, vaInBuf.timeStamp);

    encRet = mVAEncoder->encode(&vaInBuf);
    if (encRet != ENCODE_SUCCESS) {
        LOGE("Failed to encode input video frame: %d", encRet);
        tmpIn->release();
        return UNKNOWN_ERROR;
    }

    if (mLastInputBuffer != NULL) {
        mLastInputBuffer->release();
        mLastInputBuffer = NULL;
    }
    mLastInputBuffer = tmpIn;

    LOGV("Encoding Done, getting output buffer 	");
    MediaBuffer *outputBuffer;

    CHECK(mOutBufGroup->acquire_buffer(&outputBuffer) == OK);
    LOGV("Waiting for outputbuffer");
    VideoEncOutputBuffer vaOutBuf;
    vaOutBuf.bufferSize = outputBuffer->size();
    vaOutBuf.dataSize = 0;
    vaOutBuf.data = (uint8_t *) outputBuffer->data();
    vaOutBuf.format = OUTPUT_EVERYTHING;

    vaOutBuf.format = OUTPUT_EVERYTHING;
    encRet = mVAEncoder->getOutput(&vaOutBuf);
    if (encRet != ENCODE_SUCCESS) {
        LOGE("Failed to retrieve encoded video frame: %d", encRet);
        outputBuffer->release();
        return UNKNOWN_ERROR;
    }
    if (vaOutBuf.flag & ENCODE_BUFFERFLAG_SYNCFRAME) {
        outputBuffer->meta_data()->setInt32(kKeyIsSyncFrame,true);
    }

    timestamp = vaInBuf.timeStamp;

    LOGV("Got it! data= %p, ts=%llu size =%d", vaOutBuf.data, timestamp, vaOutBuf.dataSize);

    outputBuffer->set_range(0, vaOutBuf.dataSize);
    outputBuffer->meta_data()->setInt64(kKeyTime,timestamp);
    *out = outputBuffer;
    LOGV("IntelVideoEditorAVCEncoder::read end");
    return OK;
}

status_t IntelVideoEditorH263Encoder::getSharedBuffers() {

    LOGV("getSharedBuffers begin");
    Encode_Status encRet;
    status_t ret = OK;

    sp<BufferShareRegistry> r = BufferShareRegistry::getInstance();

    if (r->encoderRequestToEnableSharingMode() == BS_SUCCESS) {
        LOGI("Shared buffer mode available\n");
    }
    else {
        LOGE("Request to enable sharing failed \n");
        return UNKNOWN_ERROR;
    }

    for(int i = 0; i < INPUT_SHARED_BUFFERS; i++) {
        VideoParamsUsrptrBuffer paramsUsrptrBuffer;
        paramsUsrptrBuffer.type = VideoParamsTypeUsrptrBuffer;
        paramsUsrptrBuffer.size =  sizeof(VideoParamsUsrptrBuffer);
        paramsUsrptrBuffer.expectedSize = mFrameSize;
        paramsUsrptrBuffer.format = STRING_TO_FOURCC("NV12");
        paramsUsrptrBuffer.width = mVideoWidth;
        paramsUsrptrBuffer.height = mVideoHeight;
#ifdef RUN_IN_MERRIFIELD
        if (mVideoWidth & 0x3f) {
            paramsUsrptrBuffer.width = (mVideoWidth + 0x3f) & ~0x3f;
            paramsUsrptrBuffer.expectedSize = paramsUsrptrBuffer.width * paramsUsrptrBuffer.height * 1.5;
        }
#endif
        LOGV("Share buffer request=");
        encRet = mVAEncoder->getParameters(&paramsUsrptrBuffer);
        if (encRet != ENCODE_SUCCESS  ) {
            LOGE("could not allocate input surface from the encoder %d", encRet);
            ret = NO_MEMORY;
            break;
        }
        mSharedBufs[i].allocatedSize = paramsUsrptrBuffer.actualSize;
        mSharedBufs[i].height = mVideoHeight;
        mSharedBufs[i].width = mVideoWidth;
        mSharedBufs[i].pointer = paramsUsrptrBuffer.usrPtr;
        mSharedBufs[i].stride = paramsUsrptrBuffer.stride;
    }
    LOGV("getSharedBuffers end");
    return ret;
}

status_t IntelVideoEditorH263Encoder::setSharedBuffers() {

    LOGV("setSharedBuffers");
    sp<BufferShareRegistry> r = BufferShareRegistry::getInstance();

    if (r->encoderSetSharedBuffer(mSharedBufs,INPUT_SHARED_BUFFERS) != BS_SUCCESS) {
        LOGE("encoderSetSharedBuffer failed \n");
        return UNKNOWN_ERROR;
    }

    if (r->encoderEnterSharingMode() != BS_SUCCESS) {
        LOGE("sourceEnterSharingMode failed\n");
        return UNKNOWN_ERROR;
    }
    return OK;
}

}
