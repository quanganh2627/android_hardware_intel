/*
 * Copyright (C) 2010 The Android Open Source Project
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


#ifndef INTELVIDEOEDITORENCODERSOURCE_H
#define INTELVIDEOEDITORENCODERSOURCE_H

#include <media/stagefright/MediaSource.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/MediaBufferGroup.h>

namespace android {
struct IntelVideoEditorEncoderSource : public MediaSource {
    public:
        static sp<IntelVideoEditorEncoderSource> Create(
            const sp<MetaData> &format);
        virtual status_t start(MetaData *params = NULL);
        virtual status_t stop();
        virtual sp<MetaData> getFormat();
        virtual status_t read(MediaBuffer **buffer,
            const ReadOptions *options = NULL);
        virtual int32_t storeBuffer(MediaBuffer *buffer);
        virtual int32_t requestBuffer(MediaBuffer **buffer);

    protected:
        virtual ~IntelVideoEditorEncoderSource();

    private:
        status_t getSharedBuffers();
        MediaBufferGroup* mGroup;
        bool mUseSharedBuffers;

        struct MediaBufferChain {
            MediaBuffer* buffer;
            MediaBufferChain* nextLink;
        };
        enum State {
            CREATED,
            STARTED,
            ERROR
        };
        IntelVideoEditorEncoderSource(const sp<MetaData> &format);

        // Don't call me
        IntelVideoEditorEncoderSource(const IntelVideoEditorEncoderSource &);
        IntelVideoEditorEncoderSource &operator=(
                const IntelVideoEditorEncoderSource &);

        MediaBufferChain* mFirstBufferLink;
        MediaBufferChain* mLastBufferLink;
        int32_t           mNbBuffer;
        bool              mIsEOS;
        State             mState;
        sp<MetaData>      mEncFormat;
        Mutex             mLock;
        Condition         mBufferCond;
};
}
#endif
