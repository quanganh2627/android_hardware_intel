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

#ifndef INTELVIDEOEDITORH263ENCODER_H
#define INTELVIDEOEDITORH263ENCODER_H

#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MediaBufferGroup.h>
#include <media/stagefright/MediaSource.h>
#include <utils/Vector.h>
#include "va/va.h"
#include "VideoEncoderHost.h"
#include <IntelBufferSharing.h>

namespace android {
struct IntelVideoEditorH263Encoder :  public MediaSource {
    IntelVideoEditorH263Encoder(const sp<MediaSource> &source,
            const sp<MetaData>& meta);

    virtual status_t start(MetaData *params);
    virtual status_t stop();

    virtual sp<MetaData> getFormat();

    virtual status_t read(MediaBuffer **buffer, const ReadOptions *options);


protected:
    virtual ~IntelVideoEditorH263Encoder();

private:
    sp<MediaSource> mSource;
    sp<MetaData>    mMeta;

    int32_t  mVideoWidth;
    int32_t  mVideoHeight;
    int32_t  mFrameSize;
    int32_t  mVideoFrameRate;
    int32_t  mVideoBitRate;
    int32_t  mVideoColorFormat;
    int32_t  mUseSyncMode;
    status_t mInitCheck;
    bool     mStarted;
    bool     mFirstFrame;
    int32_t  mFrameCount;
    static const int OUTPUT_BUFFERS = 6;
    static const int INPUT_SHARED_BUFFERS = 8;
    IVideoEncoder         *mVAEncoder;
    VideoParamsCommon     mEncParamsCommon;
    SharedBufferType      mSharedBufs[INPUT_SHARED_BUFFERS];
    const ReadOptions     *mReadOptions;
    MediaBufferGroup      *mOutBufGroup;   /* group of output buffers*/
    MediaBuffer           *mLastInputBuffer;

private:
    status_t initCheck(const sp<MetaData>& meta);
    int32_t calcBitrate(int width, int height);
    status_t getSharedBuffers();
    status_t setSharedBuffers();
    static int SBShutdownFunc(void* arg);

    IntelVideoEditorH263Encoder(const IntelVideoEditorH263Encoder &);
    IntelVideoEditorH263Encoder &operator=(const IntelVideoEditorH263Encoder &);
};
};
#endif

