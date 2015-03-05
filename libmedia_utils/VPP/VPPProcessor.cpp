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
#include "VPPProcessor.h"

#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MetaData.h>
#include <ui/GraphicBuffer.h>
#include <utils/Log.h>

#if defined (TARGET_HAS_MULTIPLE_DISPLAY) && !defined (USE_MDS_LEGACY)
#include <display/MultiDisplayService.h>
using namespace android::intel;
#endif

#define LOG_TAG "VPPProcessor"

namespace android {

VPPProcessor::VPPProcessor(const sp<ANativeWindow> &native, OMXCodec *codec)
        :mInputBufferNum(0), mOutputBufferNum(0),
         mInputLoadPoint(0), mOutputLoadPoint(0),
         mLastRenderBuffer(NULL),
         mNativeWindow(native), mCodec(codec),
         mBufferInfos(NULL),
         mThreadRunning(false), mEOS(false),
         mTotalDecodedCount(0), mInputCount(0), mVPPProcCount(0), mVPPRenderCount(0),
         mVppOutputFps(0) {
    LOGI("construction");
    memset(mInput, 0, VPPBuffer::MAX_VPP_BUFFER_NUMBER * sizeof(VPPBuffer));
    memset(mOutput, 0, VPPBuffer::MAX_VPP_BUFFER_NUMBER * sizeof(VPPBuffer));

    mWorker = VPPWorker::getInstance(mNativeWindow);
}

VPPProcessor::~VPPProcessor() {
    quitThread();
    mThreadRunning = false;

    if (mWorker != NULL) {
        delete mWorker;
        mWorker == NULL;
    }

    releaseBuffers();
    mVPPProcessor = NULL;
    LOGI("VPPProcessor is deleted");
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

//static
bool VPPProcessor::isVppOn() {
#if defined (TARGET_HAS_MULTIPLE_DISPLAY) && !defined (USE_MDS_LEGACY)
    if (!VPPSetting::isVppOn())
        return false;

    sp<IServiceManager> sm = defaultServiceManager();
    if (sm == NULL) {
        LOGE("%s: Failed to get service manager", __func__);
        return false;
    }
    sp<IMDService> mds = interface_cast<IMDService>(
            sm->getService(String16(INTEL_MDS_SERVICE_NAME)));
    if (mds == NULL) {
        LOGE("%s: Failed to get MDS service", __func__);
        return false;
    }
    sp<IMultiDisplayInfoProvider> mdsInfoProvider = mds->getInfoProvider();
    if (mdsInfoProvider == NULL) {
        LOGE("%s: Failed to get info provider", __func__);
        return false;
    }
    return mdsInfoProvider->getVppState();
#else
    return VPPSetting::isVppOn();
#endif
}

status_t VPPProcessor::init() {
    LOGV("init");
    if (mCodec == NULL || mWorker == NULL)
        return VPP_FAIL;

    // set BufferInfo from decoder
    if (mBufferInfos == NULL) {
        mBufferInfos = &mCodec->mPortBuffers[mCodec->kPortIndexOutput];
        if(mBufferInfos == NULL)
            return VPP_FAIL;
        uint32_t size = mBufferInfos->size();
        LOGI("mBufferInfo size is %d",size);
        if (mInputBufferNum == 0 || mOutputBufferNum == 0
                || size <= mInputBufferNum + mOutputBufferNum
                || mInputBufferNum > VPPBuffer::MAX_VPP_BUFFER_NUMBER
                || mOutputBufferNum > VPPBuffer::MAX_VPP_BUFFER_NUMBER) {
            LOGE("input or output buffer number is invalid");
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
                LOGE("set graphic buffer config to VPPWorker failed");
                return VPP_FAIL;
            }
        }
    }

    if (initBuffers() != STATUS_OK)
        return VPP_FAIL;

    // init VPPWorker
    if(mWorker->init() != STATUS_OK)
        return VPP_FAIL;

    return createThread();
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

void VPPProcessor::printBuffers() {
    MediaBuffer *mediaBuffer = NULL;
    for (uint32_t i = 0; i < mInputBufferNum; i++) {
        mediaBuffer = findMediaBuffer(mInput[i]);
        LOGV("input %d.   %p,  status = %d, time = %lld", i, mediaBuffer, mInput[i].mStatus, mInput[i].mTimeUs);
    }
    LOGV("======================================= ");
    for (uint32_t i = 0; i < mOutputBufferNum; i++) {
        mediaBuffer = findMediaBuffer(mOutput[i]);
        LOGV("output %d.   %p,  status = %d, time = %lld", i, mediaBuffer, mOutput[i].mStatus, mOutput[i].mTimeUs);
    }

}

void VPPProcessor::printRenderList() {
    List<MediaBuffer*>::iterator it;
    for (it = mRenderList.begin(); it != mRenderList.end(); it++) {
        LOGV("renderList: %p, timestamp = %lld", *it, (*it) ? getBufferTimestamp(*it) : 0);
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
        if (!mEOS) {
            // no buffer ready to render
            return VPP_BUFFER_NOT_READY;
        }
        LOGI("GOT END OF STREAM!!!");
        *buffer = NULL;

        LOGD("======mTotalDecodedCount=%d, mInputCount=%d, mVPPProcCount=%d, mVPPRenderCount=%d======",
            mTotalDecodedCount, mInputCount, mVPPProcCount, mVPPRenderCount);
        mEOS = false;
        return ERROR_END_OF_STREAM;
    }

    *buffer = *(mRenderList.begin());
    mLastRenderBuffer = *buffer;
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
    LOGI("seek");
    /* invoke thread if it is waiting */
    if (mThreadRunning) {
        {
            Mutex::Autolock procLock(mProcThread->mLock);
            LOGV("got proc lock");
            if (!hasProcessingBuffer()) {
                LOGI("seek done");
                return;
             }
             mProcThread->mSeek = true;
             LOGV("set proc seek ");
             mProcThread->mRunCond.signal();
             LOGI("wake up proc thread");
        }
        LOGI("try to get mEnd lock");
        Mutex::Autolock endLock(mProcThread->mEndLock);
        LOGI("waiting mEnd lock(seek finish) ");
        mProcThread->mEndCond.wait(mProcThread->mEndLock);
        LOGI("wake up proc thread");
        flush();
        LOGI("seek done");
    }
}

status_t VPPProcessor::reset() {
    LOGW("Error happens in VSP and VPPProcessor need to reset");
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
    LOGI("quitThread");
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
    LOGI("releaseBuffers");
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
    LOGI("hasProcBuffer %d", hasProcBuffer);
    return hasProcBuffer;
}

void VPPProcessor::flush() {
    LOGV("flush");
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
    LOGV("flush end");
}

status_t VPPProcessor::clearInput() {
    // release useless input buffer
    for (uint32_t i = 0; i < mInputBufferNum; i++) {
        if (mInput[i].mStatus == VPP_BUFFER_READY) {
            MediaBuffer *mediaBuffer = findMediaBuffer(mInput[i]);
            if (mediaBuffer != NULL && mediaBuffer->refcount() > 0) {
                LOGV("clearInput: mediaBuffer = %p, refcount = %d",mediaBuffer, mediaBuffer->refcount());
                mediaBuffer->release();
            }
            mInput[i].resetBuffer(NULL);
        }
    }
    return VPP_OK;
}

status_t VPPProcessor::updateRenderList() {
    LOGV("updateRenderList");
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
            LOGV("1. vpp output comes too late, drop it, timeBuffer = %lld", timeBuffer);
            //vpp output comes too late, drop it
            if (buff->refcount() > 0)
                buff->release();
        } else if (timeBuffer == timeRenderList) {
            LOGV("2. timeBuffer = %lld, timeRenderList = %lld, going to erase %p, insert %p", timeBuffer, timeRenderList, *it, buff);
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
            LOGV("3. timeBuffer = %lld, timeRenderList = %lld", timeBuffer, timeRenderList);
            //x.5 frame, just insert it
            mVPPRenderCount ++;
            mRenderList.insert(it, buff);
            mOutput[mOutputLoadPoint].mStatus = VPP_BUFFER_RENDERING;
        } else {
            LOGE("Vpp: SHOULD NOT BE HERE");
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
    LOGV("VPPProcessor::cancelBufferToNativeWindow buffer = %p", buff);
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
    LOGV("VPPProcessor::dequeueBufferFromNativeWindow");
    if (mNativeWindow == NULL || mBufferInfos == NULL)
        return NULL;

    ANativeWindowBuffer *buff;
    OMXCodec::BufferInfo *info = NULL;
    //int err = mNativeWindow->dequeueBuffer(mNativeWindow.get(), &buff);
    while (1) {
        int err = native_window_dequeue_buffer_and_wait(mNativeWindow.get(), &buff);
        if (err != 0) {
            LOGE("dequeueBuffer from native window failed");
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
    LOGV("VPPProcessor::signalBufferReturned, buff = %p", buff);
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
    if (videoInfo == NULL || mWorker == NULL)
        return VPP_FAIL;
    if (mWorker->configFilters(videoInfo->width, videoInfo->height, videoInfo->fps, slowMotionFactor) != VPP_OK)
        return VPP_FAIL;
    mInputBufferNum = mWorker->mNumForwardReferences + 3;
    /* reserve one buffer in VPPProcThread, so add one more buffer here */
    mOutputBufferNum = 1 + (mWorker->mNumForwardReferences + 2) * mWorker->mFrcRate;
    if (mInputBufferNum > VPPBuffer::MAX_VPP_BUFFER_NUMBER 
            || mOutputBufferNum > VPPBuffer::MAX_VPP_BUFFER_NUMBER) {
        LOGE("buffer number needed are exceeded limitation");
        return VPP_FAIL;
    }

    // mFrcRate is 1 if FRC is disabled or input FPS is not changed by VPP
    if (mWorker->mFrcRate == FRC_RATE_2_5X) {
        mVppOutputFps = videoInfo->fps * 5 / 2;
    } else {
        mVppOutputFps = videoInfo->fps * mWorker->mFrcRate;
    }

    return VPP_OK;
}

void VPPProcessor::setEOS()
{
    LOGI("setEOS");
    mEOS = true;
    mProcThread->mEOS = true;
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

uint32_t VPPProcessor::getVppOutputFps()
{
  return mVppOutputFps;
}
} /* namespace android */
