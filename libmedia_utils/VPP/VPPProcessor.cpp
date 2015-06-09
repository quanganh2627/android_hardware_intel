/*
 * Copyright (C) 2012 Intel Corporation.  All rights reserved.
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
 *
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "VPPProcessor"
#include "VPPProcessor.h"

#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MetaData.h>
#include <ui/GraphicBuffer.h>
#include <utils/Log.h>

#if defined TARGET_HAS_MULTIPLE_DISPLAY
#include <display/MultiDisplayService.h>
using namespace android::intel;
#endif


namespace android {

VPPProcessor::VPPProcessor(const sp<ANativeWindow> &native, OMXCodec *codec)
        :mInputBufferNum(0), mOutputBufferNum(0),
         mInputLoadPoint(0), mOutputLoadPoint(0),
         mNativeWindow(native), mCodec(codec),
         mBufferInfos(NULL),
         mThreadRunning(false), mEOS(false), mIsEosRead(false),
         mTotalDecodedCount(0), mInputCount(0), mVPPProcCount(0), mVPPRenderCount(0),
         mMds(NULL) {
    ALOGI("construction");
    memset(mInput, 0, VPPBuffer::MAX_VPP_BUFFER_NUMBER * sizeof(VPPBuffer));
    memset(mOutput, 0, VPPBuffer::MAX_VPP_BUFFER_NUMBER * sizeof(VPPBuffer));

#ifdef TARGET_HAS_MULTIPLE_DISPLAY
    mMds = new VPPMDSListener(this);
#endif
    mWorker = VPPWorker::getInstance(mNativeWindow);
}

VPPProcessor::~VPPProcessor() {
    quitThread();
    mThreadRunning = false;

    if (mWorker != NULL) {
        delete mWorker;
        mWorker = NULL;
    }

#ifdef TARGET_HAS_MULTIPLE_DISPLAY
    if (mMds != NULL) {
        mMds->deInit();
        mMds = NULL;
    }
#endif
    releaseBuffers();
    mVPPProcessor = NULL;
    ALOGI("VPPProcessor is deleted");
}

//static
VPPProcessor* VPPProcessor::mVPPProcessor = NULL;

//static
VPPProcessor* VPPProcessor::getInstance(const sp<ANativeWindow> &native, OMXCodec* codec) {
    if (mVPPProcessor == NULL) {
        // If no instance is existing, create one
        mVPPProcessor = new VPPProcessor(native, codec);
        if (mVPPProcessor != NULL && mVPPProcessor->mWorker == NULL) {
            // If VPPWorker instance is not got successfully, delete VPPProcessor
            delete mVPPProcessor;
            mVPPProcessor = NULL;
        }
    } else if (mVPPProcessor->mWorker != NULL && !mVPPProcessor->mWorker->validateNativeWindow(native))
        // If one instance is existing, check if the caller share the same NativeWindow handle
        return NULL;

    return mVPPProcessor;
}

status_t VPPProcessor::init() {
    ALOGV("init");
    if (mCodec == NULL || mWorker == NULL)
        return VPP_FAIL;

    // set BufferInfo from decoder
    if (mBufferInfos == NULL) {
        mBufferInfos = &mCodec->mPortBuffers[mCodec->kPortIndexOutput];
        if(mBufferInfos == NULL)
            return VPP_FAIL;
        uint32_t size = mBufferInfos->size();
        ALOGI("mBufferInfo size is %d",size);
        if (mInputBufferNum == 0 || mOutputBufferNum == 0
                || size <= mInputBufferNum + mOutputBufferNum
                || mInputBufferNum > VPPBuffer::MAX_VPP_BUFFER_NUMBER
                || mOutputBufferNum > VPPBuffer::MAX_VPP_BUFFER_NUMBER) {
            ALOGE("input or output buffer number is invalid");
            return VPP_FAIL;
        }
        for (uint32_t i = 0; i < size; i++) {
            MediaBuffer* mediaBuffer = mBufferInfos->editItemAt(i).mMediaBuffer;
            if(mediaBuffer == NULL)
                return VPP_FAIL;
            GraphicBuffer* graphicBuffer = mediaBuffer->graphicBuffer().get();
            // set graphic buffer config to VPPWorker
            if(mWorker->setGraphicBufferConfig(graphicBuffer) == STATUS_OK)
                continue;
            else {
                ALOGE("set graphic buffer config to VPPWorker failed");
                return VPP_FAIL;
            }
        }
    }

    if (initBuffers() != STATUS_OK)
        return VPP_FAIL;
#ifndef USE_IVP
    // init VPPWorker
    if(mWorker->init() != STATUS_OK)
        return VPP_FAIL;
#endif
    return createThread();
}

status_t VPPProcessor::configFrc4Hdmi(bool enableFrc4Hdmi) {

    status_t status = STATUS_OK;
    ALOGI("configFrc4Hdmi %d", enableFrc4Hdmi);
#ifdef TARGET_HAS_MULTIPLE_DISPLAY
    if (enableFrc4Hdmi) {
        status_t status = mWorker->configFrc4Hdmi(enableFrc4Hdmi, &mMds);
        if (status != STATUS_OK) {
            ALOGE("failed to enable FRC for HDMI");
            return status;
        }

        //Recalculate VPP FRC for HDMI
        bool frcOn = false;
        FRC_RATE frcRate = FRC_RATE_1X;
        status = mWorker->calculateFrc(&frcOn, &frcRate);
        /* Apply new FRC to VPP here.
         * VPP FRC is configured before VPP thread start
         */
        bool newFrcSet = (frcOn != false) || (frcRate != FRC_RATE_1X);
        if (newFrcSet && !mThreadRunning) {
            mWorker->mFrcOn = frcOn;
            mWorker->mFrcRate = frcRate;
        } else if (mThreadRunning) {
            ALOGW("Configure VPP FRC for HDMI too late");
        }
    }
#endif
    return status;
}

bool VPPProcessor::canSetDecoderBufferToVPP() {
    if (!mThreadRunning)
        return true;
    // put VPP output which still in output array to RenderList
    CHECK(updateRenderList() == VPP_OK);

    // invoke VPPProcThread as many as possible
    mProcThread->mRunCond.signal();

    // release obsolete input buffers
    clearInput();

    if (countBuffersWeOwn() > (mOutputBufferNum + mInputBufferNum)) {
        return false;
    }
    // in non-EOS status, if input vectors has free position or
    // we have no frame to render, then set Decoder buffer in
    if (!mEOS && (mRenderList.empty() || mInput[mInputLoadPoint].mStatus == VPP_BUFFER_FREE))
        return true;
    return false;
}

status_t VPPProcessor::setDecoderBufferToVPP(MediaBuffer *buff) {
    if (buff != NULL) {
        mRenderList.push_back(buff);
        mTotalDecodedCount ++;
        // put buff in inputBuffers when there is empty buffer
        if (mInput[mInputLoadPoint].mStatus == VPP_BUFFER_FREE) {
            mIsEosRead = false;
            buff->add_ref();

            OMXCodec::BufferInfo *info = findBufferInfo(buff);
            if (info == NULL) return VPP_FAIL;
            mInput[mInputLoadPoint].mFlags = info->mFlags;
            mInput[mInputLoadPoint].mGraphicBuffer = buff->graphicBuffer();
            mInput[mInputLoadPoint].mTimeUs = getBufferTimestamp(buff);
            mInput[mInputLoadPoint].mStatus = VPP_BUFFER_LOADED;
            mInputLoadPoint = (mInputLoadPoint + 1) % mInputBufferNum;
            mInputCount ++;
            return VPP_OK;
        }
    }
    return VPP_FAIL;
}

int32_t VPPProcessor::countBuffersWeOwn() {
    int32_t n = 0;
    List<MediaBuffer*>::iterator it;
    for (it = mRenderList.begin(); it != mRenderList.end(); it++) {
        n++;
    }

    for (uint32_t i = 0; i < mOutputBufferNum; i++) {
        if (mOutput[i].mGraphicBuffer != NULL)
           n++;
    }

    return n;
}

void VPPProcessor::printBuffers() {
    MediaBuffer *mediaBuffer = NULL;
    for (uint32_t i = 0; i < mInputBufferNum; i++) {
        mediaBuffer = findMediaBuffer(mInput[i]);
        ALOGV("input %d.   %p,  status = %d, time = %lld", i, mediaBuffer, mInput[i].mStatus, mInput[i].mTimeUs);
    }
    ALOGV("======================================= ");
    for (uint32_t i = 0; i < mOutputBufferNum; i++) {
        mediaBuffer = findMediaBuffer(mOutput[i]);
        ALOGV("output %d.   %p,  status = %d, time = %lld", i, mediaBuffer, mOutput[i].mStatus, mOutput[i].mTimeUs);
    }

}

void VPPProcessor::printRenderList() {
    List<MediaBuffer*>::iterator it;
    for (it = mRenderList.begin(); it != mRenderList.end(); it++) {
        ALOGV("renderList: %p, timestamp = %lld", *it, (*it) ? getBufferTimestamp(*it) : 0);
    }
}

status_t VPPProcessor::read(MediaBuffer **buffer) {
    printBuffers();
    printRenderList();
    if (mProcThread->mError) {
        if (reset() != VPP_OK)
            return VPP_FAIL;
    }
    if (mRenderList.empty()) {
        if (!mEOS && !mIsEosRead) {
            // no buffer ready to render
            return VPP_BUFFER_NOT_READY;
        }
        ALOGI("GOT END OF STREAM!!!");
        *buffer = NULL;

        ALOGD("======mTotalDecodedCount=%d, mInputCount=%d, mVPPProcCount=%d, mVPPRenderCount=%d======",
            mTotalDecodedCount, mInputCount, mVPPProcCount, mVPPRenderCount);
        mEOS = false;
        mIsEosRead = true;
        return ERROR_END_OF_STREAM;
    }

    *buffer = *(mRenderList.begin());
    mRenderList.erase(mRenderList.begin());

    OMXCodec::BufferInfo *info = findBufferInfo(*buffer);
    if (info == NULL) return VPP_FAIL;
    info->mStatus = OMXCodec::OWNED_BY_CLIENT;

    return VPP_OK;
}

int64_t VPPProcessor::getBufferTimestamp(MediaBuffer * buff) {
    if (buff == NULL) return -1;
    int64_t timeUs;
    if (!buff->meta_data()->findInt64(kKeyTime, &timeUs))
        return -1;
    return timeUs;
}

void VPPProcessor::seek() {
    ALOGI("seek");
    /* invoke thread if it is waiting */
    if (mThreadRunning) {
        {
            Mutex::Autolock procLock(mProcThread->mLock);
            ALOGV("got proc lock");
            if (!hasProcessingBuffer()) {
                ALOGI("seek done");
                return;
             }
             mProcThread->mSeek = true;
             ALOGV("set proc seek ");
             mProcThread->mRunCond.signal();
             ALOGI("wake up proc thread");
        }
        ALOGI("try to get mEnd lock");
        Mutex::Autolock endLock(mProcThread->mEndLock);
        ALOGI("waiting mEnd lock(seek finish) ");
        mProcThread->mEndCond.wait(mProcThread->mEndLock);
        ALOGI("wake up proc thread");
        flush();
        ALOGI("seek done");
    }
}

status_t VPPProcessor::reset() {
    ALOGW("Error happens in VSP and VPPProcessor need to reset");
    quitThread();
    flush();
    if (mWorker->reset() != STATUS_OK)
        return VPP_FAIL;
    return createThread();
}

status_t VPPProcessor::createThread() {
    // VPPThread starts to run
    mProcThread = new VPPProcThread(false, mWorker,
            mInput, mInputBufferNum,
            mOutput, mOutputBufferNum);
    if (mProcThread == NULL)
        return VPP_FAIL;
    mProcThread->run("VPPProcThread", ANDROID_PRIORITY_NORMAL);
    mThreadRunning = true;
    return VPP_OK;
}

void VPPProcessor::quitThread() {
    ALOGI("quitThread");
    if(mThreadRunning) {
        mProcThread->requestExit();
        {
            Mutex::Autolock autoLock(mProcThread->mLock);
            mProcThread->mRunCond.signal();
        }
        mProcThread->requestExitAndWait();
        mProcThread.clear();
    }
    return;
}

void VPPProcessor::releaseBuffers() {
    ALOGI("releaseBuffers");
    for (uint32_t i = 0; i < mInputBufferNum; i++) {
        MediaBuffer *mediaBuffer = findMediaBuffer(mInput[i]);
        if (mediaBuffer != NULL && mediaBuffer->refcount() > 0)
            mediaBuffer->release();
        mInput[i].resetBuffer(NULL);
    }

    for (uint32_t i = 0; i < mOutputBufferNum; i++) {
        MediaBuffer *mediaBuffer = findMediaBuffer(mOutput[i]);
        if (mediaBuffer != NULL && mediaBuffer->refcount() > 0) {
        OMXCodec::BufferInfo *info = findBufferInfo(mediaBuffer);
        if (info != NULL && info->mStatus != OMXCodec::OWNED_BY_CLIENT)
            mediaBuffer->release();
        }
    }

    mInputLoadPoint = 0;
    mOutputLoadPoint = 0;

    if (!mRenderList.empty()) {
        List<MediaBuffer*>::iterator it;
        for (it = mRenderList.begin(); it != mRenderList.end(); it++) {
            if (*it == NULL) break;
            MediaBuffer* renderBuffer = *it;
            if (renderBuffer->refcount() > 0)
                renderBuffer->release();
        }
        mRenderList.clear();
    }
}

bool VPPProcessor::hasProcessingBuffer() {
    bool hasProcBuffer = false;
    for (uint32_t i = 0; i < mInputBufferNum; i++) {
        if (mInput[i].mStatus == VPP_BUFFER_PROCESSING)
            hasProcBuffer = true;
        if (mInput[i].mStatus != VPP_BUFFER_PROCESSING &&
            mInput[i].mStatus != VPP_BUFFER_FREE) {
            MediaBuffer *mediaBuffer = findMediaBuffer(mInput[i]);
            if (mediaBuffer != NULL && mediaBuffer->refcount() > 0)
                mediaBuffer->release();
            mInput[i].resetBuffer(NULL);
        }
    }
    for (uint32_t i = 0; i < mOutputBufferNum; i++) {
        if ((mOutput[i].mStatus != VPP_BUFFER_PROCESSING) &&
            (mOutput[i].mStatus != VPP_BUFFER_FREE) && (mOutput[i].mStatus != VPP_BUFFER_END_FLAG)) {
            MediaBuffer *mediaBuffer = findMediaBuffer(mOutput[i]);
            if (mediaBuffer != NULL && mediaBuffer->refcount() > 0) {
                OMXCodec::BufferInfo *info = findBufferInfo(mediaBuffer);
                if (info != NULL && info->mStatus != OMXCodec::OWNED_BY_CLIENT)
                    mediaBuffer->release();
            }
        }
    }
    mInputLoadPoint = 0;
    mOutputLoadPoint = 0;
    ALOGI("hasProcBuffer %d", hasProcBuffer);
    return hasProcBuffer;
}

void VPPProcessor::flush() {
    ALOGV("flush");
    // flush all input buffers
    for (uint32_t i = 0; i < mInputBufferNum; i++) {
        if (mInput[i].mStatus != VPP_BUFFER_FREE) {
            MediaBuffer *mediaBuffer = findMediaBuffer(mInput[i]);
            if (mediaBuffer != NULL && mediaBuffer->refcount() > 0)
                mediaBuffer->release();
            mInput[i].resetBuffer(NULL);
        }
    }

    // flush all output buffers
    for (uint32_t i = 0; i < mOutputBufferNum; i++) {
        if (mOutput[i].mStatus != VPP_BUFFER_FREE) {
            MediaBuffer *mediaBuffer = findMediaBuffer(mOutput[i]);
            if (mediaBuffer != NULL && mediaBuffer->refcount() > 0) {
                OMXCodec::BufferInfo *info = findBufferInfo(mediaBuffer);
                if (info != NULL && info->mStatus != OMXCodec::OWNED_BY_CLIENT)
                    mediaBuffer->release();
            }
        }
    }

    // flush render list
    if (!mRenderList.empty()) {
        List<MediaBuffer*>::iterator it;
        for (it = mRenderList.begin(); it != mRenderList.end(); it++) {
            if (*it == NULL) break;
            MediaBuffer* renderBuffer = *it;
            if (renderBuffer->refcount() > 0)
                renderBuffer->release();
        }
        mRenderList.clear();
    }
    mInputLoadPoint = 0;
    mOutputLoadPoint = 0;
    ALOGV("flush end");
}

status_t VPPProcessor::clearInput() {
    // release useless input buffer
    for (uint32_t i = 0; i < mInputBufferNum; i++) {
        if (mInput[i].mStatus == VPP_BUFFER_READY) {
            MediaBuffer *mediaBuffer = findMediaBuffer(mInput[i]);
            if (mediaBuffer != NULL && mediaBuffer->refcount() > 0) {
                ALOGV("clearInput: mediaBuffer = %p, refcount = %d",mediaBuffer, mediaBuffer->refcount());
                mediaBuffer->release();
            }
            mInput[i].resetBuffer(NULL);
        }
    }
    return VPP_OK;
}

status_t VPPProcessor::updateRenderList() {
    ALOGV("updateRenderList");
    while (mOutput[mOutputLoadPoint].mStatus == VPP_BUFFER_READY) {
        MediaBuffer* buff = findMediaBuffer(mOutput[mOutputLoadPoint]);
        if (buff == NULL) return VPP_FAIL;

        int64_t timeBuffer = mOutput[mOutputLoadPoint].mTimeUs;
        if (timeBuffer == -1)
            return VPP_FAIL;
        //set timestamp from VPPBuffer to MediaBuffer
        buff->meta_data()->setInt64(kKeyTime, timeBuffer);

        List<MediaBuffer*>::iterator it;
        int64_t timeRenderList = 0;
        for (it = mRenderList.begin(); it != mRenderList.end(); it++) {
            if (*it == NULL) break;

            timeRenderList = getBufferTimestamp(*it);
            if (timeRenderList == -1)
                return VPP_FAIL;

            if ((mWorker->mFrcRate > FRC_RATE_1X && timeBuffer <= timeRenderList) ||
                    (mWorker->mFrcRate == FRC_RATE_1X && timeBuffer == timeRenderList)) {
                break;
            }
        }
        if (mRenderList.empty() || it == mRenderList.end() || (it == mRenderList.begin() && timeBuffer < timeRenderList)) {
            ALOGV("1. vpp output comes too late, drop it, timeBuffer = %lld", timeBuffer);
            //vpp output comes too late, drop it
            if (buff->refcount() > 0)
                buff->release();
        } else if (timeBuffer == timeRenderList) {
            ALOGV("2. timeBuffer = %lld, timeRenderList = %lld, going to erase %p, insert %p", timeBuffer, timeRenderList, *it, buff);
            //same timestamp, use vpp output to replace the input
            MediaBuffer* renderBuff = *it;
            if (renderBuff->refcount() > 0)
                renderBuff->release();
            List<MediaBuffer*>::iterator erase = mRenderList.erase(it);
            mRenderList.insert(erase, buff);
            mOutput[mOutputLoadPoint].mStatus = VPP_BUFFER_RENDERING;
            mVPPProcCount ++;
            mVPPRenderCount ++;
        } else if (timeBuffer < timeRenderList) {
            ALOGV("3. timeBuffer = %lld, timeRenderList = %lld", timeBuffer, timeRenderList);
            //x.5 frame, just insert it
            mVPPRenderCount ++;
            mRenderList.insert(it, buff);
            mOutput[mOutputLoadPoint].mStatus = VPP_BUFFER_RENDERING;
        } else {
            ALOGE("Vpp: SHOULD NOT BE HERE");
            if (buff->refcount() > 0)
                buff->release();
        }
        mOutputLoadPoint = (mOutputLoadPoint + 1) % mOutputBufferNum;
    }
    return VPP_OK;
}

OMXCodec::BufferInfo* VPPProcessor::findBufferInfo(MediaBuffer *buff) {
    OMXCodec::BufferInfo *info = NULL;
    for (uint32_t i = 0; i < mBufferInfos->size(); i++) {
        if (mBufferInfos->editItemAt(i).mMediaBuffer == buff) {
            info = &mBufferInfos->editItemAt(i);
            break;
        }
    }
    return info;
}

status_t VPPProcessor::cancelBufferToNativeWindow(MediaBuffer *buff) {
    ALOGV("VPPProcessor::cancelBufferToNativeWindow buffer = %p", buff);
    int err = mNativeWindow->cancelBuffer(mNativeWindow.get(), buff->graphicBuffer().get(), -1);
    if (err != 0)
        return err;

    OMXCodec::BufferInfo *info = findBufferInfo(buff);
    if (info == NULL) return VPP_FAIL;

    if ((info->mStatus != OMXCodec::OWNED_BY_VPP)
            && info->mStatus != OMXCodec::OWNED_BY_CLIENT)
        return VPP_FAIL;
    info->mStatus = OMXCodec::OWNED_BY_NATIVE_WINDOW;
    info->mMediaBuffer->setObserver(NULL);

    return VPP_OK;
}

MediaBuffer * VPPProcessor::dequeueBufferFromNativeWindow() {
    ALOGV("VPPProcessor::dequeueBufferFromNativeWindow");
    if (mNativeWindow == NULL || mBufferInfos == NULL)
        return NULL;

    ANativeWindowBuffer *buff;
    OMXCodec::BufferInfo *info = NULL;
    //int err = mNativeWindow->dequeueBuffer(mNativeWindow.get(), &buff);
    while (1) {
        int err = native_window_dequeue_buffer_and_wait(mNativeWindow.get(), &buff);
        if (err != 0) {
            ALOGE("dequeueBuffer from native window failed");
            return NULL;
        }

        for (uint32_t i = 0; i < mBufferInfos->size(); i++) {
            sp<GraphicBuffer> graphicBuffer = mBufferInfos->itemAt(i).mMediaBuffer->graphicBuffer();
            if (graphicBuffer->handle == buff->handle) {
                info = &mBufferInfos->editItemAt(i);
                break;
            }
        }
        if (info == NULL) return NULL;

        sp<MetaData> metaData = info->mMediaBuffer->meta_data();
        metaData->setInt32(kKeyRendered, 0);

        if (info->mStatus == OMXCodec::OWNED_BY_CLIENT)
            continue;

        CHECK_EQ((int)info->mStatus, (int)OMXCodec::OWNED_BY_NATIVE_WINDOW);
        info->mMediaBuffer->add_ref();
        info->mStatus = OMXCodec::OWNED_BY_VPP;
        info->mMediaBuffer->setObserver(this);
        break;
    }
    return info->mMediaBuffer;
}

status_t VPPProcessor::initBuffers() {
    MediaBuffer *buf = NULL;
    uint32_t i;
    for (i = 0; i < mInputBufferNum; i++) {
        mInput[i].resetBuffer(NULL);
    }

    for (i = 0; i < mOutputBufferNum; i++) {
        buf = dequeueBufferFromNativeWindow();
        if (buf == NULL)
            return VPP_FAIL;

        mOutput[i].resetBuffer(buf->graphicBuffer());
    }
    return VPP_OK;
}

void VPPProcessor::signalBufferReturned(MediaBuffer *buff) {
    // Only called by client
    ALOGV("VPPProcessor::signalBufferReturned, buff = %p", buff);
    if (buff == NULL) return;

    int32_t rendered = 0;
    sp<MetaData> metaData = buff->meta_data();
    if (! metaData->findInt32(kKeyRendered, &rendered)) {
        rendered = 0;
    }

    OMXCodec::BufferInfo *info = findBufferInfo(buff);
    if (info == NULL) return;

    if (mThreadRunning) {
        if (info->mStatus == OMXCodec::OWNED_BY_CLIENT && rendered) {
            // Buffer has been rendered and returned to NativeWindow
            metaData->setInt32(kKeyRendered, 0);
            buff->setObserver(NULL);
            info->mStatus = OMXCodec::OWNED_BY_NATIVE_WINDOW;

            MediaBuffer * mediaBuffer = dequeueBufferFromNativeWindow();
            if (mediaBuffer == NULL) return;

            for (uint32_t i = 0; i < mOutputBufferNum; i++) {
                if (buff->graphicBuffer() == mOutput[i].mGraphicBuffer) {
                    mOutput[i].resetBuffer(mediaBuffer->graphicBuffer());
                    break;
                }
            }
        } else {
            // Reuse buffer
            buff->add_ref();
            info->mStatus = OMXCodec::OWNED_BY_VPP;
            for (uint32_t i = 0; i < mOutputBufferNum; i++) {
                if (buff->graphicBuffer() == mOutput[i].mGraphicBuffer) {
                    mOutput[i].resetBuffer(mOutput[i].mGraphicBuffer);
                    break;
                }
            }
        }
    } else { //!mThreadRunning
        if (!(info->mStatus == OMXCodec::OWNED_BY_CLIENT && rendered)) {
            // Cancel buffer back to NativeWindow as long as it's not rendered
            status_t err = cancelBufferToNativeWindow(buff);
            if (err != VPP_OK) return;
        }

        buff->setObserver(NULL);
        info->mStatus = OMXCodec::OWNED_BY_NATIVE_WINDOW;

        for (uint32_t i = 0; i < mOutputBufferNum; i++) {
            if (buff->graphicBuffer() == mOutput[i].mGraphicBuffer) {
                mOutput[i].resetBuffer(NULL);
                break;
            }
        }
    }

    return;
}

status_t VPPProcessor::validateVideoInfo(VPPVideoInfo * videoInfo, uint32_t slowMotionFactor)
{
#ifdef TARGET_HAS_MULTIPLE_DISPLAY
    if (mMds != NULL && mMds->init() != STATUS_OK)
        return VPP_FAIL;
#endif
#ifdef USE_IVP
    // init VPPWorker
    if(mWorker->init() != STATUS_OK)
        return VPP_FAIL;
#endif
    if (videoInfo == NULL || mWorker == NULL)
        return VPP_FAIL;
    if (mWorker->configFilters(videoInfo->width, videoInfo->height, videoInfo->fps, slowMotionFactor, 0) != VPP_OK)
        return VPP_FAIL;
    mInputBufferNum = mWorker->mNumForwardReferences + 3;
    /* reserve one buffer in VPPProcThread, so add one more buffer here */
    mOutputBufferNum = 1 + (mWorker->mNumForwardReferences + 2) * mWorker->mFrcRate;
    if (mInputBufferNum > VPPBuffer::MAX_VPP_BUFFER_NUMBER 
            || mOutputBufferNum > VPPBuffer::MAX_VPP_BUFFER_NUMBER) {
        ALOGE("buffer number needed are exceeded limitation");
        return VPP_FAIL;
    }

    return VPP_OK;
}

void VPPProcessor::setEOS()
{
    if (mIsEosRead)
        return;

    ALOGI("setEOS");
    mEOS = true;

    if ((mProcThread != NULL) && mThreadRunning) {
        mProcThread->mEOS = true;
    } else {
        ALOGW("VPP processs thread is not running");
    }
}

MediaBuffer * VPPProcessor::findMediaBuffer(VPPBuffer &buff) {
    if (!mBufferInfos)
        return NULL;

    MediaBuffer* mediaBuffer = NULL;
    for (uint32_t i = 0; i < mBufferInfos->size(); i++) {
        mediaBuffer = mBufferInfos->editItemAt(i).mMediaBuffer;
        if (mediaBuffer->graphicBuffer() == buff.mGraphicBuffer) {
            return mediaBuffer;
        }
    }
    return NULL;
}

uint32_t VPPProcessor::getVppOutputFps() {
    return mWorker->getVppOutputFps();
}

void VPPProcessor::setDisplayMode(int32_t mode) {
    if (mWorker != NULL) {
#ifndef TARGET_VPP_USE_GEN
        //check if frame rate conversion needed if HDMI connection status changed
        ALOGV("old/new/connect_Bit mode %d %d %d",
                 mWorker->getDisplayMode(),  mode, MDS_HDMI_CONNECTED);
        if ((mWorker->getDisplayMode() != mode) && (mProcThread != NULL)) {
            mProcThread->notifyCheckFrc();
            ALOGI("NeedCheckFrc change");
        }
#endif
        mWorker->setDisplayMode(mode);
    }
}

} /* namespace android */
