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
#define LOG_TAG "IntelVideoEditorAVCEncoder"
#include <utils/Log.h>
#include "OMX_Video.h"
#include <media/stagefright/MediaBufferGroup.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/Utils.h>
#include "IntelVideoEditorAVCEncoder.h"
#include <media/stagefright/foundation/ADebug.h>

#define INIT_BUF_FULLNESS_RATIO 0.6
#define MIN_INTRA_PERIOD        30
#define SHORT_INTRA_PERIOD (mVideoFrameRate)
#define MEDIUM_INTRA_PERIOD (2*mVideoFrameRate)
#define LONG_INTRA_PERIOD (4*mVideoFrameRate)
#define LOW_QUALITY_BITRATE         2000000
#define MEDIUM_QUALITY_BITRATE      5000000
#define BITRATE_1M                  1000000
#define BITRATE_2M                  2000000
#define BITRATE_4M                  4000000
#define BITRATE_5M                  5000000

namespace android {

IntelVideoEditorAVCEncoder::IntelVideoEditorAVCEncoder(
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
      mLastInputBuffer(NULL),
      mCallEncode(0) {

    LOGV("Construct IntelVideoEditorAVCEncoder");
}

IntelVideoEditorAVCEncoder::~IntelVideoEditorAVCEncoder() {
    LOGV("Destruct IntelVideoEditorAVCEncoder");
    if (mStarted) {
        stop();
    }
}

status_t IntelVideoEditorAVCEncoder::initCheck(const sp<MetaData>& meta) {
    LOGV("initCheck");

    Encode_Status   encStatus;
    uint32_t        disableFrameSkip = 0;
    sp<MetaData> sourceFormat = mSource->getFormat();

    CHECK(sourceFormat->findInt32(kKeyWidth, &mVideoWidth));
    CHECK(sourceFormat->findInt32(kKeyHeight, &mVideoHeight));
    CHECK(sourceFormat->findInt32(kKeyFrameRate, &mVideoFrameRate));
    CHECK(sourceFormat->findInt32(kKeyColorFormat, &mVideoColorFormat));

    CHECK(sourceFormat->findInt32(kKeyBitRate, &mVideoBitRate));

    // Tune the output bitrates to improve the quality
    if (mVideoBitRate < BITRATE_1M) {
        mVideoBitRate = BITRATE_1M;
        if (mVideoHeight > 720) {
            mVideoBitRate = BITRATE_4M;
        }
        else if (mVideoHeight > 480)
        {
            mVideoBitRate = BITRATE_2M;
        }
    }
    else if (mVideoBitRate < BITRATE_4M) {
        if (mVideoHeight > 720) {
            mVideoBitRate = BITRATE_5M;
        }
        else if (mVideoHeight > 480) {
            mVideoBitRate = BITRATE_4M;
        }
    }

    LOGI("mVideoWidth = %d, mVideoHeight = %d, mVideoFrameRate = %d, mVideoColorFormat = %d, mVideoBitRate = %d",
        mVideoWidth, mVideoHeight, mVideoFrameRate, mVideoColorFormat, mVideoBitRate);

    // disable frame skip for low bitrate clips
    if (mVideoBitRate < BITRATE_2M) {
        LOGI("Frameskip is disabled for low bitrate clips");
        disableFrameSkip = 1;
    }

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
    mEncParamsCommon.profile = VAProfileH264Baseline;
    mEncParamsCommon.resolution.width = mVideoWidth;
    mEncParamsCommon.resolution.height= mVideoHeight;
    mEncParamsCommon.frameRate.frameRateNum = mVideoFrameRate;
    mEncParamsCommon.frameRate.frameRateDenom = 1;
    mEncParamsCommon.rcMode = RATE_CONTROL_VBR;
    mEncParamsCommon.rcParams.bitRate = mVideoBitRate;
    mEncParamsCommon.rcParams.disableFrameSkip = disableFrameSkip;
    mEncParamsCommon.rawFormat =  RAW_FORMAT_NV12;

    mEncParamsCommon.rcParams.minQP  = 0;
    mEncParamsCommon.rcParams.maxQP = 0;
    mEncParamsCommon.rcParams.initQP = 0;
    mEncParamsCommon.rcParams.I_minQP = 0;
    mEncParamsCommon.rcParams.I_maxQP = 0;
    mEncParamsCommon.rcParams.enableIntraFrameQPControl = 0;

    if (mVideoBitRate < LOW_QUALITY_BITRATE) {
        mEncParamsCommon.intraPeriod = LONG_INTRA_PERIOD;
    }
    else if (mVideoBitRate < MEDIUM_QUALITY_BITRATE) {
        mEncParamsCommon.intraPeriod = MEDIUM_INTRA_PERIOD;
    }
    else {
        mEncParamsCommon.intraPeriod = SHORT_INTRA_PERIOD;
    }
    if (mEncParamsCommon.intraPeriod < MIN_INTRA_PERIOD) {
        mEncParamsCommon.intraPeriod = MIN_INTRA_PERIOD;
    }

    mEncParamsCommon.syncEncMode = mUseSyncMode;
    mFrameCount = 0;

    // All luma and chroma block edges of the slice are filtered
    mEncParamsCommon.disableDeblocking = 0;

    encStatus = mVAEncoder->setParameters(&mEncParamsCommon);
    CHECK(encStatus == ENCODE_SUCCESS);
    LOGV("new encoder params set");

    encStatus = mVAEncoder->getParameters(&mEncParamsH264);
    CHECK(encStatus == ENCODE_SUCCESS);
    LOGV("got H264 encoder params ");

    mEncParamsH264.ipPeriod = 1;
    mEncParamsH264.idrInterval = 1;
    mEncParamsH264.sliceNum.iSliceNum = 2;
    mEncParamsH264.sliceNum.pSliceNum = 2;

    // If the bitrate is low, we set the slice number to 1 in one frame to avoid visible boundary
    if (mVideoBitRate < LOW_QUALITY_BITRATE) {
        mEncParamsH264.sliceNum.iSliceNum = 1;
        mEncParamsH264.sliceNum.pSliceNum = 1;
    }
    mEncParamsH264.VUIFlag = 0;

    encStatus = mVAEncoder->setParameters(&mEncParamsH264);
    CHECK(encStatus == ENCODE_SUCCESS);
    LOGV("new  H264 encoder params set");

    if (disableFrameSkip) {
        VideoConfigBitRate configBitrate;
        encStatus = mVAEncoder->getConfig(&configBitrate);
        CHECK(encStatus == ENCODE_SUCCESS);
        LOGV("got encoder config set");

        configBitrate.rcParams.disableFrameSkip = 1;
        encStatus = mVAEncoder->setConfig(&configBitrate);
        CHECK(encStatus == ENCODE_SUCCESS);
        LOGV("got encoder frame skip/bits stuffing set");
    }

    // Only CBR support HRD feature
    if (mEncParamsCommon.rcMode == RATE_CONTROL_CBR) {
        VideoParamsHRD hrdParam;
        encStatus = mVAEncoder->getParameters(&hrdParam);
        CHECK(encStatus == ENCODE_SUCCESS);
        LOGV("got encoder hrd params ");

        hrdParam.bufferSize = mVideoBitRate;
        hrdParam.initBufferFullness = hrdParam.bufferSize * INIT_BUF_FULLNESS_RATIO;

        encStatus = mVAEncoder->setParameters(&hrdParam);
        CHECK(encStatus == ENCODE_SUCCESS);
        LOGV("new  encoder hrd params set");
    }

    mOutBufGroup = new MediaBufferGroup();
    CHECK(mOutBufGroup != NULL);

    return OK;
}

status_t IntelVideoEditorAVCEncoder::start(MetaData *params) {
    LOGV("start");
    status_t ret = OK;

    if (mStarted) {
        LOGW("Call start() when encoder already started");
        return OK;
    }

    mSource->start(params);

    mVAEncoder = createVideoEncoder(MEDIA_MIMETYPE_VIDEO_AVC);

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
         * behalf. */
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

int IntelVideoEditorAVCEncoder::SBShutdownFunc(void* arg)
{
    LOGV("IntelVideoEditorAVCEncoder::SBShutdownFunc begin()");
    sp<BufferShareRegistry> r = BufferShareRegistry::getInstance();
    int error = r->sourceExitSharingMode();
    LOGV("sourceExitSharingMode returns %d",error);
    return 0;
}

status_t IntelVideoEditorAVCEncoder::stop() {
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

sp<MetaData> IntelVideoEditorAVCEncoder::getFormat() {

    sp<MetaData> format = new MetaData;
    format->setInt32(kKeyWidth, mVideoWidth);
    format->setInt32(kKeyHeight, mVideoHeight);
    format->setInt32(kKeyBitRate, mVideoBitRate);
    format->setInt32(kKeySampleRate, mVideoFrameRate);
    format->setInt32(kKeyColorFormat, mVideoColorFormat);
    format->setCString(kKeyMIMEType, MEDIA_MIMETYPE_VIDEO_AVC);
    format->setCString(kKeyDecoderComponent, "IntelVideoEditorAVCEncoder");
    return format;
}

status_t IntelVideoEditorAVCEncoder::read(MediaBuffer **out, const ReadOptions *options) {

    status_t err;
    Encode_Status encRet;
    MediaBuffer *tmpIn = NULL, *tmpIn2 = NULL;
    CHECK(!options);
    mReadOptions = options;
    *out = NULL;
    MediaBuffer *outputBuffer;
    VideoEncOutputBuffer vaOutBuf;

    LOGV("IntelVideoEditorAVCEncoder::read start");

    do {
        err = mSource->read(&tmpIn, NULL);
        if (err == INFO_FORMAT_CHANGED) {
            stop();
            start(NULL);
        }
    } while (err == INFO_FORMAT_CHANGED);

    if (err == ERROR_END_OF_STREAM) {
        if (mCallEncode > 0) {
            LOGV("Get the last encoded frame");
            CHECK(mOutBufGroup->acquire_buffer(&outputBuffer) == OK);
            vaOutBuf.bufferSize = outputBuffer->size();
            vaOutBuf.dataSize = 0;
            vaOutBuf.data = (uint8_t *) outputBuffer->data();
            vaOutBuf.format = OUTPUT_EVERYTHING;

            encRet = mVAEncoder->getOutput(&vaOutBuf);
            if (encRet != ENCODE_SUCCESS) {
                LOGE("Failed to retrieve encoded video frame: %d", encRet);
                outputBuffer->release();
                return UNKNOWN_ERROR;
            }
            mCallEncode--;

            if (vaOutBuf.flag & ENCODE_BUFFERFLAG_SYNCFRAME) {
                outputBuffer->meta_data()->setInt32(kKeyIsSyncFrame,true);
            }

            outputBuffer->set_range(0, vaOutBuf.dataSize);
            outputBuffer->meta_data()->setInt64(kKeyTime,vaOutBuf.timeStamp);
            *out = outputBuffer;
        }

        if (mLastInputBuffer != NULL) {
            mLastInputBuffer->release();
            mLastInputBuffer = NULL;
        }
        return err;
    }
    else if (err != OK) {
        LOGE("Failed to read input video frame: %d", err);
        return err;
    }

    // For encode async mode
    if (mFirstFrame) {
        do {
            err = mSource->read(&tmpIn2, NULL);
            if (err == INFO_FORMAT_CHANGED) {
                stop();
                start(NULL);
            }
        } while (err == INFO_FORMAT_CHANGED);

        if (err == ERROR_END_OF_STREAM) {
            if (tmpIn == NULL)
                return err;
        }
        else if (err != OK) {
            LOGE("Failed to read input video frame: %d", err);
            return err;
        }
    }

    VideoEncRawBuffer vaInBuf, vaInBuf2;
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
    mCallEncode++;

    if (mFirstFrame && tmpIn2) {
        vaInBuf2.flag = 0;
        vaInBuf2.type = FTYPE_UNKNOWN;

        vaInBuf2.data = (uint8_t *)tmpIn2->data();
        vaInBuf2.size = tmpIn2->size();

        tmpIn2->meta_data()->findInt64(kKeyTime, (int64_t *)&(vaInBuf2.timeStamp));
        LOGV("For async mode encoding: buffer %p, size = %d, ts= %llu",vaInBuf2.data, vaInBuf2.size, vaInBuf2.timeStamp);

        encRet = mVAEncoder->encode(&vaInBuf2);
        if (encRet != ENCODE_SUCCESS) {
            LOGE("Failed to encode input video frame: %d", encRet);
            tmpIn2->release();
            return UNKNOWN_ERROR;
        }
        mCallEncode++;
    }

    LOGV("Encoding Done, getting output buffer 	");

    CHECK(mOutBufGroup->acquire_buffer(&outputBuffer) == OK);
    LOGV("Waiting for outputbuffer");

    vaOutBuf.bufferSize = outputBuffer->size();
    vaOutBuf.dataSize = 0;
    vaOutBuf.data = (uint8_t *) outputBuffer->data();
    vaOutBuf.format = OUTPUT_EVERYTHING;

    if (mFirstFrame) {
        LOGV("mFirstFrame\n");
        encRet = mVAEncoder->getOutput(&vaOutBuf);
        if (encRet != ENCODE_SUCCESS) {
            LOGE("Failed to retrieve encoded video frame: %d", encRet);
            outputBuffer->release();
            return UNKNOWN_ERROR;
        }
        outputBuffer->meta_data()->setInt32(kKeyIsCodecConfig,true);
        outputBuffer->meta_data()->setInt32(kKeyIsSyncFrame,true);
        if (tmpIn != NULL) {
            tmpIn->release();
            tmpIn = NULL;
        }
        mLastInputBuffer = tmpIn2;
        mFirstFrame = false;
    } else {
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
    }

    if (mLastInputBuffer != tmpIn2) {
        if (mLastInputBuffer != NULL) {
            mLastInputBuffer->release();
            mLastInputBuffer = NULL;
        }
        mLastInputBuffer = tmpIn;
    } else {
        tmpIn2 = NULL;
    }

    LOGV("Got it! data= %p, ts=%llu size =%d", vaOutBuf.data, vaOutBuf.timeStamp, vaOutBuf.dataSize);

    mCallEncode--;
    outputBuffer->set_range(0, vaOutBuf.dataSize);
    outputBuffer->meta_data()->setInt64(kKeyTime,vaOutBuf.timeStamp);
    *out = outputBuffer;

    LOGV("IntelVideoEditorAVCEncoder::read end");
    return OK;
}

status_t IntelVideoEditorAVCEncoder::getSharedBuffers() {

    LOGV("getSharedBuffers begin");
    Encode_Status encRet;
    status_t ret = OK;

    sp<BufferShareRegistry> r = BufferShareRegistry::getInstance();

    if (r->encoderRequestToEnableSharingMode() == BS_SUCCESS) {
        LOGV("Shared buffer mode available\n");
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

status_t IntelVideoEditorAVCEncoder::setSharedBuffers() {
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
