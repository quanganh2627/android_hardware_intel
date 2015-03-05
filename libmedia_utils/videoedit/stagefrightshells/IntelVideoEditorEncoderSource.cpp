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


//#define LOG_NDEBUG 0
#define LOG_TAG "IntelVideoEditorEncoderSource"
#include "utils/Log.h"

#include "IntelVideoEditorEncoderSource.h"
#include "utils/Vector.h"
#include <media/stagefright/MediaSource.h>
#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MediaBufferGroup.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MetaData.h>
#include <IntelBufferSharing.h>

namespace android {
sp<IntelVideoEditorEncoderSource> IntelVideoEditorEncoderSource::Create(
    const sp<MetaData> &format) {

    sp<IntelVideoEditorEncoderSource> aSource =
        new IntelVideoEditorEncoderSource(format);
    return aSource;
}

IntelVideoEditorEncoderSource::IntelVideoEditorEncoderSource(
    const sp<MetaData> &format):
        mGroup(NULL),
        mUseSharedBuffers(false),
        mFirstBufferLink(NULL),
        mLastBufferLink(NULL),
        mNbBuffer(0),
        mIsEOS(false),
        mState(CREATED),
        mEncFormat(format) {
    LOGV("IntelVideoEditorEncoderSource::IntelVideoEditorEncoderSource");
}

IntelVideoEditorEncoderSource::~IntelVideoEditorEncoderSource() {

    // Safety clean up
    if( STARTED == mState ) {
        stop();
    }
}

status_t IntelVideoEditorEncoderSource::start(MetaData *meta) {
    Mutex::Autolock autolock(mLock);
    status_t err = OK;

    LOGV("IntelVideoEditorEncoderSource::start() begin");

    if( CREATED != mState ) {
        LOGV("IntelVideoEditorEncoderSource::start: invalid state %d", mState);
        return UNKNOWN_ERROR;
    }
    mState = STARTED;
    sp<BufferShareRegistry> r = BufferShareRegistry::getInstance();
    if (r->sourceRequestToEnableSharingMode() == BS_SUCCESS) {
        LOGI("Shared buffer mode available\n");
        mUseSharedBuffers = true;
        mGroup = NULL;
    }
    else
    {
        LOGE("Shared buffer mode not available\n");
        return UNKNOWN_ERROR;
    }
    LOGV("IntelVideoEditorEncoderSource::start() END (0x%x)", err);
    return err;
}

status_t IntelVideoEditorEncoderSource::getSharedBuffers()
{
    Mutex::Autolock autolock(mLock);

    LOGV("IntelVideoEditorEncoderSource::getSharedBuffers begin");
    sp<BufferShareRegistry> r = BufferShareRegistry::getInstance();
    SharedBufferType *bufs = NULL;
    int buf_cnt = 0;

    if (r->sourceEnterSharingMode() != BS_SUCCESS) {
        LOGE("sourceEnterSharingMode failed\n");
        return UNKNOWN_ERROR;
    }

    if (r->sourceGetSharedBuffer(NULL, &buf_cnt) != BS_SUCCESS) {
        LOGE("sourceGetSharedBuffer failed, unable to get buffer count\n");
        return UNKNOWN_ERROR;
    }

    bufs = new SharedBufferType[buf_cnt];
    if (r->sourceGetSharedBuffer(bufs, &buf_cnt) != BS_SUCCESS) {
        LOGE("sourceGetSharedBuffer failed, unable to retrieve buffers\n");
        delete [] bufs;
        return UNKNOWN_ERROR;
    }

    mGroup = new MediaBufferGroup();

    for (int n = 0; n < buf_cnt; n++)
    {
        mGroup->add_buffer(new MediaBuffer(bufs[n].pointer, bufs[n].allocatedSize));
    }

    delete [] bufs;

    LOGV("IntelVideoEditorAVCEncoderSource::getSharedBuffers end");
    return OK;
}


status_t IntelVideoEditorEncoderSource::stop() {

    LOGV("IntelVideoEditorAVCEncoderSource::stop() begin");
    status_t err = OK;

    if( STARTED != mState ) {
        LOGV("IntelVideoEditorAVCEncoderSource::stop: invalid state %d", mState);
        return UNKNOWN_ERROR;
    }

    if (mUseSharedBuffers) {
        if (mGroup) {
            delete mGroup;
            mGroup = NULL;
        }
        mUseSharedBuffers = false;
    }

    int32_t i = 0;
    MediaBufferChain* tmpLink = NULL;
    while( mFirstBufferLink ) {
        i++;
        tmpLink = mFirstBufferLink;
        mFirstBufferLink = mFirstBufferLink->nextLink;
        delete tmpLink;
    }
    LOGV("IntelVideoEditorEncoderSource::stop : %d buffer remained", i);
    mFirstBufferLink = NULL;
    mLastBufferLink = NULL;
    mState = CREATED;

    LOGV("IntelVideoEditorEncoderSource::stop() END (0x%x)", err);
    return err;
}

sp<MetaData> IntelVideoEditorEncoderSource::getFormat() {

    LOGV("IntelVideoEditorEncoderSource::getFormat");
    return mEncFormat;
}

status_t IntelVideoEditorEncoderSource::read(MediaBuffer **buffer,
        const ReadOptions *options) {
    Mutex::Autolock autolock(mLock);

    LOGV("IntelVideoEditorEncoderSource::read() begin");

    MediaSource::ReadOptions readOptions;
    status_t err = OK;
    MediaBufferChain* tmpLink = NULL;

    if ( STARTED != mState ) {
        LOGV("IntelVideoEditorEncoderSource::read: invalid state %d", mState);
        return UNKNOWN_ERROR;
    }

    while (mFirstBufferLink == NULL && !mIsEOS) {
        LOGV("Wait for buffer in IntelVideoEditorEncoderSource::read()");
        mBufferCond.wait(mLock);
    }

    LOGV("Get the buffer in IntelVideoEditorEncoderSource::read()!");

    // End of stream?
    if (mFirstBufferLink == NULL) {
        *buffer = NULL;
        LOGV("IntelVideoEditorEncoderSource::read : EOS");
        return ERROR_END_OF_STREAM;
    }

    // Get a buffer from the chain
    *buffer = mFirstBufferLink->buffer;
    tmpLink = mFirstBufferLink;
    mFirstBufferLink = mFirstBufferLink->nextLink;

    if ( NULL == mFirstBufferLink) {
        mLastBufferLink = NULL;
    }
    delete tmpLink;
    mNbBuffer--;

    LOGV("IntelVideoEditorEncoderSource::read() END (0x%x)", err);
    return err;
}

int32_t IntelVideoEditorEncoderSource::storeBuffer(MediaBuffer *buffer) {
    Mutex::Autolock autolock(mLock);

    LOGV("IntelVideoEditorEncoderSource::storeBuffer() begin");

    status_t err = OK;

    if( NULL == buffer ) {
        LOGV("IntelVideoEditorEncoderSource::storeBuffer : reached EOS");
        mIsEOS = true;
    } else {
        MediaBufferChain* newLink = new MediaBufferChain;
        newLink->buffer = buffer;
        newLink->nextLink = NULL;
        if( NULL != mLastBufferLink ) {
            mLastBufferLink->nextLink = newLink;
        } else {
            mFirstBufferLink = newLink;
        }
        mLastBufferLink = newLink;
        mNbBuffer++;
    }
    mBufferCond.signal();
    LOGV("IntelVideoEditorEncoderSource::storeBuffer() end");
    return mNbBuffer;
}

int32_t IntelVideoEditorEncoderSource::requestBuffer(MediaBuffer **buffer) {
    status_t err = OK;
    LOGV("IntelVideoEditorEncoderSource::requestBuffer() begin");
    if (!mGroup && mUseSharedBuffers) {
        err = getSharedBuffers();
        if (err != OK) {
            LOGE("shared buffer setup failed\n");
            return err;
        }
    }

    if (mGroup) {
        err = mGroup->acquire_buffer(buffer);
    } else {
        LOGE("Fail to get media buffer group");
        return UNKNOWN_ERROR;
    }

    LOGV("requestBuffer buffer addr = 0x%p",(uint8_t *)(*buffer)->data());
    if (err != OK) {
        LOGE("Fail to get shared buffers");
        return UNKNOWN_ERROR;
    }
    LOGV("IntelVideoEditorEncoderSource::requestBuffer() end");
    return err;
}
}
