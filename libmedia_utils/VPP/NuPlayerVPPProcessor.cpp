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
#define LOG_TAG "NuPlayerVPPProcessor"
#include <utils/Log.h>

#include "NuPlayerVPPProcessor.h"

#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/MediaDefs.h>

#if defined (TARGET_HAS_MULTIPLE_DISPLAY) && !defined (USE_MDS_LEGACY)
#include <display/MultiDisplayService.h>
using namespace android::intel;
#endif

namespace android {

NuPlayerVPPProcessor::NuPlayerVPPProcessor(
        const sp<AMessage> &notify,
        const sp<NativeWindowWrapper> &nativeWindow)
    : mNotify(notify),
      mNativeWindow(nativeWindow),
      mBufferInfos(NULL),
      mInputBufferNum(0),
      mOutputBufferNum(0),
      mThreadRunning(false),
      mInputLoadPoint(0),
      mOutputLoadPoint(0),
      mInputCount(0),
      mACodec(NULL),
      mEOS(false),
      mLastInputTimeUs(-1) {

    mWorker = VPPWorker::getInstance(mNativeWindow->getNativeWindow());
}


NuPlayerVPPProcessor::~NuPlayerVPPProcessor() {
    quitThread();

    if (mWorker != NULL) {
        delete mWorker;
        mWorker = NULL;
    }

    if (mThreadRunning == false) {
        releaseBuffers();
    }
    mNuPlayerVPPProcessor = NULL;
    LOGI("===== VPPInputCount = %d  =====", mInputCount);
}

//static
NuPlayerVPPProcessor* NuPlayerVPPProcessor::mNuPlayerVPPProcessor = NULL;

//static
NuPlayerVPPProcessor* NuPlayerVPPProcessor::getInstance(
        const sp<AMessage> &notify,
        const sp<NativeWindowWrapper> &nativeWindow) {
    if (mNuPlayerVPPProcessor == NULL) {
        // If no instance is existing, create one
        mNuPlayerVPPProcessor = new NuPlayerVPPProcessor(notify, nativeWindow);
        if (mNuPlayerVPPProcessor != NULL && mNuPlayerVPPProcessor->mWorker == NULL) {
            // If VPPWorker instance is not got successfully, delete VPPProcessor
            delete mNuPlayerVPPProcessor;
            mNuPlayerVPPProcessor = NULL;
        }
    } else if (mNuPlayerVPPProcessor->mWorker != NULL &&
            !mNuPlayerVPPProcessor->mWorker->validateNativeWindow(nativeWindow->getNativeWindow()))
        // If one instance is existing, check if the caller share the same NativeWindow handle
        return NULL;

    return mNuPlayerVPPProcessor;
}

void NuPlayerVPPProcessor::invokeThreads() {
    if (!mThreadRunning)
        return;

    // invoke thread as many as possible
    mProcThread->mRunCond.signal();
}

status_t NuPlayerVPPProcessor::canSetBufferToVPP() {
    if (!mThreadRunning)
        return VPP_FAIL;

    if (!mEOS && mInput[mInputLoadPoint].mStatus == VPP_BUFFER_FREE) {
        return VPP_OK;
    }

    return VPP_FAIL;
}


status_t NuPlayerVPPProcessor::setBufferToVPP(const sp<ABuffer> &buffer, const sp<AMessage> &notifyConsumed) {
    if (!mThreadRunning)
        return VPP_OK;

    if (buffer == NULL || notifyConsumed == NULL)
        return VPP_FAIL;

    /*
     * mLastInputTimeUs >= timeBuf, means this incoming buffer is abnormal,
     * so we never send it to VPP, and mark it as notVppInput
     */
    int64_t timeBuf = getBufferTimestamp(buffer);
    if (mLastInputTimeUs >= timeBuf) {
        notifyConsumed->setInt32("notVppInput", true);
        return VPP_OK;
    }

    IOMX::buffer_id bufferID;
    CHECK(notifyConsumed->findPointer("buffer-id", &bufferID));

    ACodec::BufferInfo *info = findBufferByID(bufferID);
    CHECK(info != NULL);
    sp<GraphicBuffer> graphicBuffer = info->mGraphicBuffer;

    mInput[mInputLoadPoint].mFlags = 0;
    mInput[mInputLoadPoint].mGraphicBuffer = graphicBuffer;
    mInput[mInputLoadPoint].mTimeUs = timeBuf;
    notifyConsumed->setInt32("vppInput", true);
    mInput[mInputLoadPoint].mCodecMsg = notifyConsumed;
    mInput[mInputLoadPoint].mStatus = VPP_BUFFER_LOADED;

    mInputLoadPoint = (mInputLoadPoint + 1) % mInputBufferNum;
    mInputCount ++;
    mLastInputTimeUs = timeBuf;

    return VPP_OK;
}

void NuPlayerVPPProcessor::getBufferFromVPP() {
    if (!mThreadRunning)
        return;

//    printBuffers();

    while (mOutput[mOutputLoadPoint].mStatus == VPP_BUFFER_READY) {

        sp<AMessage> notifyConsumed = new AMessage(ACodec::kWhatOutputBufferDrained, mACodec->id());
        ACodec::BufferInfo *info = findBufferByGraphicBuffer(mOutput[mOutputLoadPoint].mGraphicBuffer);
        CHECK(info != NULL);

        info->mData->meta()->setInt64("timeUs", mOutput[mOutputLoadPoint].mTimeUs);
        notifyConsumed->setPointer("buffer-id", info->mBufferID);
        notifyConsumed->setInt32("vppOutput", true);

        sp<AMessage> vppNotifyConsumed = new AMessage(kWhatFreeBuffer, id());
        vppNotifyConsumed->setInt32("index", mOutputLoadPoint);
        notifyConsumed->setMessage("vppNotifyConsumed", vppNotifyConsumed);

        sp<AMessage> vppNotify = mNotify->dup();
        vppNotify->setInt32("what", kWhatUpdateVppOutput);
        vppNotify->setMessage("notifyConsumed", notifyConsumed);
        vppNotify->setBuffer("buffer", info->mData);
        vppNotify->post();

        LOGV("vpp output buffer index = %d, buffer = %p, timeUs = %lld",
                mOutputLoadPoint, mOutput[mOutputLoadPoint].mGraphicBuffer.get(), mOutput[mOutputLoadPoint].mTimeUs);

        mOutput[mOutputLoadPoint].mStatus = VPP_BUFFER_RENDERING;
        mOutputLoadPoint = (mOutputLoadPoint + 1) % mOutputBufferNum;
    }

    for (uint32_t i = 0; i < mInputBufferNum; i++) {
        if (mInput[i].mStatus == VPP_BUFFER_READY)
            postAndResetInput(i);
    }
}

void NuPlayerVPPProcessor::onMessageReceived(const sp<AMessage> &msg) {
    switch(msg->what()) {
        case kWhatFreeBuffer:
        {
            onFreeBuffer(msg);
            break;
        }

        default:
        {
            TRESPASS();
            break;
        }
    }
}

void NuPlayerVPPProcessor::onFreeBuffer(const sp<AMessage> &msg) {
    int32_t reuse = 0, index = 0;
    CHECK(msg->findInt32("reuse", &reuse));
    CHECK(msg->findInt32("index", &index));

    /*
     * flushShutdown quits threads
     * and there are still some rendering buffer not processed, process here
     */
    if (!mThreadRunning) {
        ACodec::BufferInfo * info = findBufferByGraphicBuffer(mOutput[index].mGraphicBuffer);
        if (info != NULL) {
            LOGV("cancel buffer after thread quit ; graphicBuffer = %p", mOutput[index].mGraphicBuffer.get());
            cancelBufferToNativeWindow(info);
            mOutput[index].resetBuffer(NULL);
        }
    } else {
        if (reuse == 1) {
            mOutput[index].resetBuffer(mOutput[index].mGraphicBuffer);
        } else if (reuse == 0) {
            ACodec::BufferInfo *info = dequeueBufferFromNativeWindow();
            CHECK(info != NULL);
            mOutput[index].resetBuffer(info->mGraphicBuffer);
        }
    }

}

int64_t NuPlayerVPPProcessor::getBufferTimestamp(sp<ABuffer> buffer) {
    int64_t timeUs;
    CHECK(buffer->meta()->findInt64("timeUs", &timeUs));
    return timeUs;
}

status_t NuPlayerVPPProcessor::validateVideoInfo(VPPVideoInfo *videoInfo){
    if (videoInfo == NULL || mWorker == NULL)
        return VPP_FAIL;
    if (mWorker->configFilters(videoInfo->width, videoInfo->height, videoInfo->fps) != VPP_OK)
        return VPP_FAIL;
    mInputBufferNum = mWorker->mNumForwardReferences + 3;
    /* reserve one buffer in VPPProcThread, on add one more buffer here */
    mOutputBufferNum = 1 + (mWorker->mNumForwardReferences + 2) * mWorker->mFrcRate;
    if (mInputBufferNum > VPPBuffer::MAX_VPP_BUFFER_NUMBER
            || mOutputBufferNum > VPPBuffer::MAX_VPP_BUFFER_NUMBER) {
        LOGE("buffer number needed are exceeded limitation");
        return VPP_FAIL;
    }
    return VPP_OK;
}

// static
bool NuPlayerVPPProcessor::isVppOn() {
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

ACodec::BufferInfo * NuPlayerVPPProcessor::findBufferByID(IOMX::buffer_id bufferID) {
    if (mBufferInfos == NULL)
        return NULL;

    for (size_t i = 0; i < mBufferInfos->size(); ++i) {
        ACodec::BufferInfo *info = &mBufferInfos->editItemAt(i);

        if (info != NULL && info->mBufferID == bufferID) {
            return info;
        }
    }

    return NULL;
}

ACodec::BufferInfo * NuPlayerVPPProcessor::findBufferByGraphicBuffer(sp<GraphicBuffer> graphicBuffer) {
    if (mBufferInfos == NULL)
        return NULL;

    for (size_t i = 0; i < mBufferInfos->size(); ++i) {
        ACodec::BufferInfo *info = &mBufferInfos->editItemAt(i);

        if (info != NULL && info->mGraphicBuffer == graphicBuffer) {
            return info;
        }
    }

    return NULL;
}

status_t NuPlayerVPPProcessor::init(sp<ACodec> &codec) {
    LOGI("init");
    if (codec == NULL || mWorker == NULL)
        return VPP_FAIL;

    mACodec = codec;

    // set BufferInfo from decoder
    if (mBufferInfos == NULL) {
        mBufferInfos = &codec->mBuffers[codec->kPortIndexOutput];
        if(mBufferInfos == NULL)
            return VPP_FAIL;
        uint32_t size = mBufferInfos->size();
        if (mInputBufferNum == 0 || mOutputBufferNum == 0
                || size <= mInputBufferNum + mOutputBufferNum
                || mInputBufferNum > VPPBuffer::MAX_VPP_BUFFER_NUMBER
                || mOutputBufferNum > VPPBuffer::MAX_VPP_BUFFER_NUMBER) {
            LOGE("input or output buffer number is invalid");
            return VPP_FAIL;
        }
        for (uint32_t i = 0; i < size; i++) {
            ACodec::BufferInfo *info = &mBufferInfos->editItemAt(i);
            GraphicBuffer* graphicBuffer = info->mGraphicBuffer.get();
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

status_t NuPlayerVPPProcessor::initBuffers() {
    ACodec::BufferInfo *buf = NULL;
    uint32_t i;
    for (i = 0; i < mInputBufferNum; i++) {
        mInput[i].resetBuffer(NULL);
    }

    for (i = 0; i < mOutputBufferNum; i++) {
        buf = dequeueBufferFromNativeWindow();
        if (buf == NULL)
            return VPP_FAIL;

        mOutput[i].resetBuffer(buf->mGraphicBuffer);
    }
    return VPP_OK;
}

ACodec::BufferInfo * NuPlayerVPPProcessor::dequeueBufferFromNativeWindow() {
    ANativeWindowBuffer *buf;
    if (native_window_dequeue_buffer_and_wait(mNativeWindow->getNativeWindow().get(), &buf) != 0) {
        LOGE("dequeueBuffer failed.");
        return NULL;
    }

    for (size_t i = mBufferInfos->size(); i-- > 0;) {
        ACodec::BufferInfo *info = &mBufferInfos->editItemAt(i);

        if (info->mGraphicBuffer->handle == buf->handle) {
            CHECK_EQ((int)info->mStatus,
                     (int)ACodec::BufferInfo::OWNED_BY_NATIVE_WINDOW);

            info->mStatus = ACodec::BufferInfo::OWNED_BY_VPP;
            LOGV("dequeueBufferFromNativeWindow graphicBuffer = %p", info->mGraphicBuffer.get());

            return info;
        }
    }

    return NULL;
}

void NuPlayerVPPProcessor::releaseBuffers() {
    LOGI("releaseBuffers");
    for (uint32_t i = 0; i < mInputBufferNum; i++) {
        mInput[i].resetBuffer(NULL);
    }

    for (uint32_t i = 0; i < mOutputBufferNum; i++) {
        ACodec::BufferInfo * info = findBufferByGraphicBuffer(mOutput[i].mGraphicBuffer);
        if (info != NULL) {
            cancelBufferToNativeWindow(info);
            mOutput[i].resetBuffer(NULL);
        }
    }

    mInputLoadPoint = 0;
    mOutputLoadPoint = 0;
    mLastInputTimeUs = -1;
}

status_t NuPlayerVPPProcessor::cancelBufferToNativeWindow(ACodec::BufferInfo *info) {
    CHECK((info->mStatus == ACodec::BufferInfo::OWNED_BY_VPP)
            || (info->mStatus == ACodec::BufferInfo::OWNED_BY_NATIVE_WINDOW));

    if (info->mStatus == ACodec::BufferInfo::OWNED_BY_VPP) {
        LOGV("cancelBuffer on buffer %p", info->mGraphicBuffer.get());

        int err = mNativeWindow->getNativeWindow()->cancelBuffer(
                mNativeWindow->getNativeWindow().get(), info->mGraphicBuffer.get(), -1);

        CHECK_EQ(err, 0);

        info->mStatus = ACodec::BufferInfo::OWNED_BY_NATIVE_WINDOW;
    }
    return OK;
}

void NuPlayerVPPProcessor::printBuffers() {
    for (uint32_t i = 0; i < mInputBufferNum; i++) {
        LOGI("input %d.   graphicBuffer = %p,  status = %d, time = %lld", i, mInput[i].mGraphicBuffer.get(), mInput[i].mStatus, mInput[i].mTimeUs);
    }
    for (uint32_t i = 0; i < mOutputBufferNum; i++) {
        LOGI("output %d.   graphicBuffer = %p,  status = %d, time = %lld", i, mOutput[i].mGraphicBuffer.get(), mOutput[i].mStatus, mOutput[i].mTimeUs);
    }

}

void NuPlayerVPPProcessor::postAndResetInput(uint32_t index) {
    if (mInput[index].mGraphicBuffer != NULL) {
        // release useless input buffer
        sp<AMessage> vppNotify = mNotify->dup();
        vppNotify->setInt32("what", kWhatUpdateVppInput);
        vppNotify->setMessage("notifyConsumed", mInput[index].mCodecMsg);
        //vppNotify->setBuffer("buffer", buffer);
        vppNotify->post();
    }

    mInput[index].resetBuffer(NULL);
}

void NuPlayerVPPProcessor::quitThread() {
    LOGI("quitThread");
    if (mThreadRunning) {
        mProcThread->requestExit();
        {
            Mutex::Autolock autoLock(mProcThread->mLock);
            mProcThread->mRunCond.signal();
        }
        mProcThread->requestExitAndWait();
        mProcThread.clear();
    }
    mThreadRunning = false;
    return;
}


void NuPlayerVPPProcessor::seek() {

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
            LOGV("wake up proc thread");
        }
        Mutex::Autolock endLock(mProcThread->mEndLock);
        LOGI("waiting proc thread mEnd lock");
        mProcThread->mEndCond.wait(mProcThread->mEndLock);
        LOGI("wake up from proc thread");
        flushNoShutdown();
        mLastInputTimeUs = -1;
        LOGI("seek done");
    }
}


bool NuPlayerVPPProcessor::hasProcessingBuffer() {
    LOGV("before hasProcessingBuffer");
    //printBuffers();
    bool hasProcBuffer = false;
    for (uint32_t i = 0; i < mInputBufferNum; i++) {
        if (mInput[i].mStatus == VPP_BUFFER_PROCESSING)
            hasProcBuffer = true;
        if (mInput[i].mStatus != VPP_BUFFER_PROCESSING && mInput[i].mStatus != VPP_BUFFER_FREE) {
            postAndResetInput(i);
        }
    }
    for (uint32_t i = 0; i < mOutputBufferNum; i++) {
        if ((mOutput[i].mStatus != VPP_BUFFER_PROCESSING) && (mOutput[i].mStatus != VPP_BUFFER_FREE) && (mOutput[i].mStatus != VPP_BUFFER_END_FLAG)) {
            mOutput[i].resetBuffer(mOutput[i].mGraphicBuffer);
        }
    }
    mInputLoadPoint = 0;
    mOutputLoadPoint = 0;
    LOGV("after hasProcessingBuffer");
    LOGI("hasProcessingBuffer %d", hasProcBuffer);
    //printBuffers();
    return hasProcBuffer;
}

void NuPlayerVPPProcessor::flushNoShutdown() {
    LOGV("before flushNoShutdown");
    //printBuffers();
    for (uint32_t i = 0; i < mInputBufferNum; i++) {
        CHECK(mInput[i].mStatus != VPP_BUFFER_PROCESSING);
        if (mInput[i].mStatus != VPP_BUFFER_FREE) {
            postAndResetInput(i);
        }
    }

    for (uint32_t i = 0; i < mOutputBufferNum; i++) {
        CHECK(mOutput[i].mStatus != VPP_BUFFER_PROCESSING);
        if (mOutput[i].mStatus != VPP_BUFFER_FREE
                    /*&& mOutput[i].mStatus != VPP_BUFFER_RENDERING*/) {
            mOutput[i].resetBuffer(mOutput[i].mGraphicBuffer);
        }
    }
    LOGV("after flushNoShutdown");
    printBuffers();
}

void NuPlayerVPPProcessor::setEOS() {
    if (!mThreadRunning)
        return;
    LOGI("set eos");
    mEOS = true;
    mProcThread->mEOS = true;
}

void NuPlayerVPPProcessor::flushShutdown() {
    LOGV("flushShutdown");

    if(!mThreadRunning)
        return;

    quitThread();

    if (mWorker != NULL) {
        delete mWorker;
        mWorker = NULL;
    }

    printBuffers();

    if (!mThreadRunning) {
        for (uint32_t i = 0; i < mInputBufferNum; i++) {
            if(mInput[i].mStatus != VPP_BUFFER_FREE) {
                postAndResetInput(i);
            }
        }

        for (uint32_t i = 0; i < mOutputBufferNum; i++) {
            if (mOutput[i].mStatus != VPP_BUFFER_RENDERING) {
                ACodec::BufferInfo * info = findBufferByGraphicBuffer(mOutput[i].mGraphicBuffer);
                if (info != NULL) {
                    cancelBufferToNativeWindow(info);
                    mOutput[i].resetBuffer(NULL);
                }
            }
        }
    }
    mInputLoadPoint = 0;
    mOutputLoadPoint = 0;
}

} //namespace android
