/*
* Copyright (C) 2011 The Android Open Source Project
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/



//#define LOG_NDEBUG 0
#define LOG_TAG "VAVideoDecoder"
#include <utils/Log.h>

#include <media/stagefright/MediaBufferGroup.h>
//#include <media/stagefright/MediaDebug.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/Utils.h>
#include <media/openmax/OMX_IVCommon.h>
#include "MetaDataExt.h"
#include "VAVideoDecoder.h"
#include "VideoDecoderInterface.h"
#include "VideoDecoderHost.h"
#include <media/stagefright/foundation/ADebug.h>
namespace android {

VAVideoDecoder::VAVideoDecoder(const sp<MediaSource> &source)
    : mSource(source),
      mStarted(false),
      mRawOutput(false),
      mInputBuffer(NULL),
      mTargetTimeUs(-1),
      mFrameIndex(0),
      mErrorCount(0),
      mDecodeMore(false),
      mDecoder(NULL) {

    const char *mime;
    CHECK(source->getFormat()->findCString(kKeyMIMEType, &mime));
    mDecoder = createVideoDecoder(mime);
    if (mDecoder == NULL) {
        ALOGE("Failed to create video decoder for %s", mime);
    }

    mFormat = new MetaData;
    mFormat->setCString(kKeyMIMEType, MEDIA_MIMETYPE_VIDEO_VA);

    int32_t width, height;
    CHECK(mSource->getFormat()->findInt32(kKeyWidth, &width));
    CHECK(mSource->getFormat()->findInt32(kKeyHeight, &height));
    mFormat->setInt32(kKeyWidth, width);
    mFormat->setInt32(kKeyHeight, height);
    mFormat->setInt32(kKeyColorFormat, OMX_INTEL_COLOR_FormatYUV420PackedSemiPlanar);
    mFormat->setCString(kKeyDecoderComponent, "VAVideoDecoder");

    int64_t durationUs;
    if (mSource->getFormat()->findInt64(kKeyDuration, &durationUs)) {
        mFormat->setInt64(kKeyDuration, durationUs);
    }

}

VAVideoDecoder::~VAVideoDecoder() {
    if (mStarted) {
        stop();
    }
    releaseVideoDecoder(mDecoder);
    mDecoder = NULL;
}

status_t VAVideoDecoder::start(MetaData *params) {
    CHECK(!mStarted);

    if (mDecoder == NULL) {
        ALOGE("Decoder is not created.");
        return UNKNOWN_ERROR;
    }

    uint32_t type;
    const void *data;
    size_t size;
    sp<MetaData> meta = mSource->getFormat();
    VideoConfigBuffer configBuffer;
    memset(&configBuffer, 0, sizeof(VideoConfigBuffer));

    if (meta->findData(kKeyConfigData, &type, &data, &size) ||
        meta->findData(kKeyESDS, &type, &data, &size) ||
        meta->findData(kKeyAVCC, &type, &data, &size)) {
        configBuffer.data = (uint8_t*)data;
        configBuffer.size = size;
    } else {
        ALOGW("No configuration data found!");
    }

    // A threshold is used here to avoid mediaserver allocate too big
    // memory (like 3GB) and crash in MPEG4Source::start. The
    // threshold is set to be input port size limit for Intel decoders.
    int32_t max_size;
    if (meta->findInt32(kKeyMaxInputSize, &max_size)) {
        if (max_size > MAXINPUTSIZE || max_size < 0) {
            ALOGE("Invalid kKeyMaxInputSize!");
            return ERROR_MALFORMED;
        }
    }
    configBuffer.flag |= WANT_RAW_OUTPUT;
    mFormat->setInt32(kKeyColorFormat, OMX_COLOR_FormatYUV420SemiPlanar);
    mFormat->setCString(kKeyMIMEType, MEDIA_MIMETYPE_VIDEO_RAW);
    mRawOutput = true;
    ALOGW("Decoder will output raw data.");

    mFormat->findInt32(kKeyWidth, &configBuffer.width);
    mFormat->findInt32(kKeyHeight, &configBuffer.height);

    Decode_Status res = mDecoder->start(&configBuffer);
    if (res != DECODE_SUCCESS) {
        ALOGE("Failed to start decoder. Error = %d", res);
        return UNKNOWN_ERROR;
    }

    // TODO: update format meta, including frame cropping information.

    // create MediaBuffer pool only when output is VASurface
    if (mRawOutput == false) {
        for (int32_t i = 0; i < NUM_OF_MEDIA_BUFFER; ++i) {
            MediaBuffer *buffer = new MediaBuffer(sizeof(VideoRenderBuffer));
            buffer->setObserver(this);
            // output is  unreadable VASurface
            buffer->meta_data()->setInt32(kKeyIsUnreadable, 1);
            buffer->meta_data()->setCString(kKeyMIMEType, MEDIA_MIMETYPE_VIDEO_VA);
            buffer->meta_data()->setInt32(kKeyColorFormat,OMX_COLOR_FormatYUV420SemiPlanar);
            mFrames.push(buffer);
        }
    }

    mSource->start();

    mFrameIndex = 0;
    mErrorCount = 0;
    mTargetTimeUs = -1;
    mStarted = true;
    return OK;
}


status_t VAVideoDecoder::stop() {
    CHECK(mStarted);

    if (mInputBuffer) {
        mInputBuffer->release();
        mInputBuffer = NULL;
    }

    for (size_t i = 0; i < mFrames.size(); ++i) {
         MediaBuffer *buffer = mFrames.editItemAt(i);
         buffer->setObserver(NULL);
         buffer->release();
    }
    mFrames.clear();
    mSource->stop();
    mDecoder->stop();

    mFrameIndex = 0;
    mErrorCount = 0;
    mRawOutput = false;
    mStarted = false;

    return OK;
}

sp<MetaData> VAVideoDecoder::getFormat() {
    return mFormat;
}

MediaBuffer *VAVideoDecoder::getOutputBuffer(bool bDraining) {
    const VideoRenderBuffer* buffer = mDecoder->getOutput(bDraining);
    if (buffer == NULL) {
        return NULL;
    }
    // indicate buffer is rendered
    buffer->renderDone = true;

    if (mTargetTimeUs >= 0) {
        CHECK(buffer->timeStamp <= mTargetTimeUs);
        if (buffer->timeStamp < mTargetTimeUs) {
            // We're still waiting for the frame with the matching
            // timestamp and we won't return the current one.
            ALOGV("skipping frame at %lld us", buffer->timeStamp);
            return NULL;
        } else {
            ALOGV("found target frame at %lld us", buffer->timeStamp);
            mTargetTimeUs = -1;
        }
    }

    MediaBuffer *mbuf = NULL;
    if (mRawOutput == false) {
        mbuf = mFrames.editItemAt(mFrameIndex);
        mFrameIndex++;
        if (mFrameIndex >= mFrames.size()) {
            mFrameIndex = 0;
        }
        memcpy(mbuf->data(), buffer, sizeof(VideoRenderBuffer));
        mbuf->meta_data()->setInt64(kKeyTime, buffer->timeStamp);
        mbuf->set_range(0, mbuf->size());
        mbuf->add_ref();
    } else {
        mbuf = new MediaBuffer(buffer->rawData->data, buffer->rawData->size);
        mbuf->meta_data()->setCString(kKeyMIMEType, MEDIA_MIMETYPE_VIDEO_RAW);
        mbuf->meta_data()->setInt32(kKeyColorFormat, OMX_COLOR_FormatYUV420SemiPlanar);
        mbuf->meta_data()->setInt64(kKeyTime, buffer->timeStamp);
    }

    return mbuf;
}

status_t VAVideoDecoder::read(MediaBuffer **out, const ReadOptions *options)  {
    *out = NULL;
    if (mDecoder == NULL) {
        ALOGE("Decoder is not created.");
        return UNKNOWN_ERROR;
    }

    int64_t seekTimeUs;
    ReadOptions::SeekMode mode;
    ReadOptions seekOptions;
    bool seeking = false;

    if (options && options->getSeekTo(&seekTimeUs, &mode)) {
        ALOGV("seek requested to %lld us (%.2f secs)", seekTimeUs, seekTimeUs / 1E6);

        if (seekTimeUs < 0) {
            ALOGE("Invalid seek time : %ld", (long int32_t)seekTimeUs);
            seekTimeUs = 0;
            //return ERROR_END_OF_STREAM;
        }
        //CHECK(seekTimeUs >= 0);

        seekOptions.setSeekTo(seekTimeUs, mode);
        mDecoder->flush();
        seeking = true;
    }

    for (;;) {
        status_t err = mSource->read(&mInputBuffer, &seekOptions);
        seekOptions.clearSeekTo();

        if (err != OK) {
            ALOGE("Failed to read buffer from source extractor.");
            // drain the output buffer when end of stream
            *out = getOutputBuffer(true);
            return (*out == NULL)  ? err : (status_t)OK;
        }

        CHECK(mInputBuffer != NULL);
        if (mInputBuffer->range_length() > 0) {
            break;
        }

        mInputBuffer->release();
        mInputBuffer = NULL;
    }

    if (mInputBuffer == NULL) {
        ALOGE("Unexpected NULL input buffer.");
        return ERROR_END_OF_STREAM;
    }

    if (seeking) {
        int64_t targetTimeUs;
        if (mInputBuffer->meta_data()->findInt64(kKeyTargetTime, &targetTimeUs) && targetTimeUs >= 0) {
            mTargetTimeUs = targetTimeUs;
        } else {
            mTargetTimeUs = -1;
        }
    }

    status_t err = UNKNOWN_ERROR;

    // prepare decoding buffer
    VideoDecodeBuffer decodeBuffer;
    memset(&decodeBuffer, 0, sizeof(decodeBuffer));
    decodeBuffer.data = (uint8_t*)mInputBuffer->data() + mInputBuffer->range_offset();
    decodeBuffer.size = mInputBuffer->range_length();
    decodeBuffer.flag = seeking ? (HAS_DISCONTINUITY | IS_SYNC_FRAME) : 0;
    mInputBuffer->meta_data()->findInt64(kKeyTime, &decodeBuffer.timeStamp);
    Decode_Status res = mDecoder->decode(&decodeBuffer);

    if (mDecodeMore) {
        mDecodeMore = false;
    } else {
        const VideoFormatInfo* formatInfo = mDecoder->getFormatInfo();
        if ((formatInfo->flags & IS_SINGLE_FIELD) == IS_SINGLE_FIELD) {
            mDecodeMore = true;
        }
    }

    if (res == DECODE_FORMAT_CHANGE) {
        ALOGW("Format changed.");
        // drain all the frames.
        MediaBuffer *mbuf = NULL;
        while ((mbuf = getOutputBuffer(true)) != NULL) {
            mbuf->release();
        }
        const VideoFormatInfo *info = mDecoder->getFormatInfo();
        uint32_t cropWidth, cropHeight;
        if (info != NULL) {
            cropWidth = info->width - (info->cropLeft + info->cropRight);
            cropHeight = info->height - (info->cropBottom + info->cropTop);

            mFormat->setInt32(kKeyWidth, cropWidth);
            mFormat->setInt32(kKeyHeight, cropHeight);

            res = mDecoder->decode(&decodeBuffer);
        } else {
            // TODO: handle format change
            err = INFO_FORMAT_CHANGED;
        }
    }

    if (res == DECODE_SUCCESS) {
        mErrorCount = 0;
        err = OK;
        if (mDecodeMore) {
            *out = new MediaBuffer(0);
             ALOGW("Current decoder buffer only contains one field!");
        } else {
            MediaBuffer *mbuf = getOutputBuffer(true);
            if (mbuf == NULL) {
                *out = new MediaBuffer(0);
            } else {
                *out = mbuf;
            }
        }
    } else {
        mErrorCount++;
        ALOGE("Failed to decode buffer (#%d). Error = %d", mErrorCount, res);
        if (checkFatalDecoderError(res)) {
            err = UNKNOWN_ERROR;
        } else {
            // For decoder errors that could be omitted,  not throw error and continue to decode.
            err = OK;
            *out = new MediaBuffer(0);
        }
    }

    mInputBuffer->release();
    mInputBuffer = NULL;
    return err;
}

}// namespace android

