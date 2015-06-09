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
#define LOG_TAG "VPPProcThread"
#include "VPPProcThread.h"
#include <utils/Log.h>

namespace android {

VPPProcThread::VPPProcThread(bool canCallJava, VPPWorker* vppWorker,
                VPPBuffer *inputBuffer, const uint32_t inputBufferNum,
                VPPBuffer *outputBuffer, const uint32_t outputBufferNum):
    Thread(canCallJava),
    mWait(false), mError(false), mEOS(false), mSeek(false),
    mThreadId(NULL),
    mVPPWorker(vppWorker),
    mInput(inputBuffer),
    mOutput(outputBuffer),
    mInputBufferNum(inputBufferNum),
    mOutputBufferNum(outputBufferNum),
    mInputProcIdx(0),
    mOutputProcIdx(0),
    mNumTaskInProcesing(0),
    mFirstInputFrame(true),
    mInputFillIdx(0),
    mOutputFillIdx(0),
    mbFlushPipelineInProcessing(false),
    mFrcChange(false),
    mNeedCheckFrc(false) {
}

VPPProcThread::~VPPProcThread() {
    ALOGV("VPPProcThread is deleted");
}

status_t VPPProcThread::readyToRun() {
    mThreadId = androidGetThreadId();
    //do init ops here
    return Thread::readyToRun();
}


bool VPPProcThread::getBufForFirmwareOutput(Vector< sp<GraphicBuffer> > *fillBufList,uint32_t *fillBufNum){
    uint32_t i = 0;
    bool  bRet =true;
    // output buffer number for filling
    *fillBufNum = 0;

    uint32_t needFillNum = 0;

    //output data available
    needFillNum = mVPPWorker->getFillBufCount();
    if ((needFillNum == 0) || (needFillNum > 4))
       return false;

    if (mOutput[mOutputFillIdx].mStatus == VPP_BUFFER_END_FLAG) {
       *fillBufNum = 1;
       sp<GraphicBuffer> fillBuf = mOutput[mOutputFillIdx].mGraphicBuffer.get();
       fillBufList->push_back(fillBuf);
       ALOGV("End flag %d", __LINE__);
       return true;
    }

    for (i = 0; i < needFillNum; i++) {
        uint32_t fillPos = (mOutputFillIdx + i) % mOutputBufferNum;
        if (mOutput[fillPos].mStatus == VPP_BUFFER_PROCESSING) {
            sp<GraphicBuffer> fillBuf = mOutput[fillPos].mGraphicBuffer.get();
            fillBufList->push_back(fillBuf);
        } else {
            ALOGV(" buffer status error %d line %d ...", mOutput[fillPos].mStatus, __LINE__);
            break;
        }
    }

    *fillBufNum  = i;
    if (i < needFillNum) {
        ALOGE("falied to get fill buffer, status mOutPutFillIdx %d i %d, fillBufNum %d", mOutputFillIdx, i, needFillNum);
        bRet = false;
    }

    return bRet;
}


int VPPProcThread::updateFirmwareOutputBufStatus(Vector< sp<GraphicBuffer> > fillBufList,
                                        uint32_t fillBufNum) {
    int64_t timeUs;
    bool bPipelineFlushCompleted = false;

    if (mFirstInputFrame) {
        mFirstInputFrame = false;
    } else {
        mInput[mInputFillIdx].mStatus = VPP_BUFFER_READY;
        mInputFillIdx = (mInputFillIdx + 1) % mInputBufferNum;
    }

    if (mOutput[mOutputFillIdx].mStatus == VPP_BUFFER_END_FLAG) {
       mOutput[mOutputFillIdx].mStatus = VPP_BUFFER_FREE;
       mFirstInputFrame = true;
       bPipelineFlushCompleted = true;

       if (mEOS || mSeek) {
           mInputFillIdx = 0;
           mOutputFillIdx = 0;
           mInputProcIdx = 0;
           mOutputProcIdx = 0;
       }
       ALOGV("End flag  finished %d", __LINE__);
    } else if (mOutput[mOutputFillIdx].mStatus == VPP_BUFFER_PROCESSING) {
        for(uint32_t i = 0; i < fillBufNum; i++) {
            uint32_t outputVppPos = (mOutputFillIdx + i) % mOutputBufferNum;
            mOutput[outputVppPos].mStatus = VPP_BUFFER_READY;
            if (fillBufNum > 1) {
                // frc is enabled, output fps is 60, change timeStamp
                timeUs = mOutput[outputVppPos].mTimeUs;
                timeUs -= 1000000ll * (fillBufNum - i - 1) / 60;
                mOutput[outputVppPos].mTimeUs = timeUs;
            }
        }
        mOutputFillIdx = (mOutputFillIdx + fillBufNum) % mOutputBufferNum;
   } else {
          ALOGE("Shoud not be here");
   }

   return bPipelineFlushCompleted;
}


bool VPPProcThread::getBufForFirmwareInput(Vector< sp<GraphicBuffer> > *procBufList,
                                   sp<GraphicBuffer> *inputBuf,
                                   bool bFlushPipeline,
                                   uint32_t *procBufNum) {
    uint32_t needProcNum = 0;
    bool bGetBufSuccess = true;


    *procBufNum = 0;
    needProcNum = mVPPWorker->getProcBufCount();
    if ((needProcNum == 0) || (needProcNum > 4)) {
       return false;
    }

    if (!bFlushPipeline) {
        *inputBuf = mInput[mInputProcIdx].mGraphicBuffer.get();
    } else {
        needProcNum = 1;
        *inputBuf = NULL;
    }

    // output buffer number for processing
    uint32_t i;
    uint32_t procPos;
    i = 0;
    do {
        procPos = (mOutputProcIdx + i) % mOutputBufferNum;
        if (mOutput[procPos].mStatus == VPP_BUFFER_FREE) {
            sp<GraphicBuffer> procBuf = mOutput[procPos].mGraphicBuffer.get();
            procBufList->push_back(procBuf);
            i++;
        } else {
            ALOGV("mOutputProcIdx %d i %d buf status %d", mOutputProcIdx,
                      i, mOutput[procPos].mStatus);
        }

        if (mOutput[mOutputProcIdx].mStatus == VPP_BUFFER_END_FLAG) {
              ALOGE("End flag should not be here");
        }
    } while ((i < needProcNum) && (mOutput[procPos].mStatus == VPP_BUFFER_FREE));

    *procBufNum = i;
    bGetBufSuccess = (*procBufNum  == needProcNum);
    ALOGV("bGetBuf %d mOutputProcIdx %d i %d buf status %d", bGetBufSuccess,mOutputProcIdx, i, mOutput[procPos].mStatus);

    return bGetBufSuccess;
}


int VPPProcThread::updateFirmwareInputBufStatus(Vector< sp<GraphicBuffer> > procBufList,
                                         uint32_t procBufNum, int64_t timeUs,
                                         bool bFlushPipeline) {
     if (!bFlushPipeline) {
         mInput[mInputProcIdx].mStatus = VPP_BUFFER_PROCESSING;
         mInputProcIdx = (mInputProcIdx + 1) % mInputBufferNum;

         for(uint32_t i = 0; i < procBufNum; i++) {
             uint32_t procPos = (mOutputProcIdx + i) % mOutputBufferNum;
             mOutput[procPos].mStatus = VPP_BUFFER_PROCESSING;
             // set output buffer timestamp as the same as input
             mOutput[procPos].mTimeUs = timeUs;
         }
         mOutputProcIdx = (mOutputProcIdx + procBufNum) % mOutputBufferNum;
     } else {
         mOutput[mOutputProcIdx].mStatus = VPP_BUFFER_END_FLAG;
     }

     return 0;
}


bool VPPProcThread::isReadytoRun() {

    bool bInputReady = (mInput[mInputProcIdx].mStatus == VPP_BUFFER_LOADED) ? true : false;
    bool bOutBufFree = isOutputBufFree();

    if ((bInputReady && bOutBufFree) || (mNumTaskInProcesing > 0))
       return true;
    else
       return false;
}


bool VPPProcThread::isOutputBufFree() {
    uint32_t procPos;
    uint32_t i = 0;
    uint32_t needOutputBufNum = mVPPWorker->getProcBufCount();
    bool bOutBufFree;

    do {
        procPos = (mOutputProcIdx + i) % mOutputBufferNum;
        bOutBufFree = (mOutput[procPos].mStatus == VPP_BUFFER_FREE);
        if (bOutBufFree)
           i++;
    } while (bOutBufFree && i < (needOutputBufNum + 1));

    //Reserved one free buffer for flushpipeline
    return (i >= (needOutputBufNum + 1));
}

bool VPPProcThread::threadLoop() {
    uint32_t procBufNum = 0, fillBufNum = 0;
    sp<GraphicBuffer> inputBuf;
    Vector< sp<GraphicBuffer> > procBufList;

    Vector< sp<GraphicBuffer> > fillBufList;
    int64_t timeUs = 0ll;
    bool bInputReady = false;
    bool bOutputBufFree = true;
    uint32_t flags = 0;
    bool bPendingOnFirmware = false;
    bool bFlushPipeline = false;
    bool bGetBufSuccess = true;

    Mutex::Autolock autoLock(mLock);

    if (mNeedCheckFrc) {
        bool frcOn;
        FRC_RATE frcRate;
        mNeedCheckFrc = false;
        mVPPWorker->calculateFrc(&frcOn, &frcRate);
        if ((frcOn != mVPPWorker->mFrcOn) || (frcRate != mVPPWorker->mFrcRate)) {
            ALOGI("mNumTaskInProcesing %d", mNumTaskInProcesing);
            mFrcChange = true;
            mVPPWorker->mUpdatedFrcRate = frcRate;
            mVPPWorker->mUpdatedFrcOn = frcOn;
        } else {
            ALOGI("No FRC change needed. Keep current FRC");
        }
    }

    ALOGV("mNumTaskInProcesing %d", mNumTaskInProcesing);
    while ((mNumTaskInProcesing > 0 && (!bPendingOnFirmware || mbFlushPipelineInProcessing)) && bGetBufSuccess ) {
        fillBufList.clear();
        bGetBufSuccess = getBufForFirmwareOutput(&fillBufList, &fillBufNum);
        ALOGV("bGetOutput %d, buf num %d", bGetBufSuccess, fillBufNum);
        if (bGetBufSuccess) {
            status_t ret = mVPPWorker->fill(fillBufList, fillBufNum);
            if (ret == STATUS_OK) {
                mNumTaskInProcesing--;
                ALOGV("mNumTaskInProcesing: %d ...", mNumTaskInProcesing);
                bool bPipelineFlusheCompleted = updateFirmwareOutputBufStatus(fillBufList, fillBufNum);
                if (bPipelineFlusheCompleted) {
                    ALOGI("bPipelineFlusheCompleted");
                    mSeek = false;
                    mEOS = false;
                    if (mFrcChange &&
                        ((mVPPWorker->mFrcOn != mVPPWorker->mUpdatedFrcOn) ||
                          (mVPPWorker->mFrcRate != mVPPWorker->mUpdatedFrcRate))) {
                        mVPPWorker->mFrcRate = mVPPWorker->mUpdatedFrcRate;
                        mVPPWorker->mFrcOn = mVPPWorker->mUpdatedFrcOn;
                    }
                    mFrcChange = false;

                    mbFlushPipelineInProcessing = false;
                    mVPPWorker->reset();
                    Mutex::Autolock endLock(mEndLock);
                    ALOGV("send end signal, mInputFillIdx = %d",mInputFillIdx);
                    mEndCond.signal();
                }
            } else if (ret == STATUS_DATA_RENDERING) {
                bPendingOnFirmware = true;
                if (mSeek || mEOS || mFrcChange) {
                    ALOGI("PendingOnFirmware %d, TaskInProcesing %d, flushing %d, Eos/Seek/Frc %d/%d/%d",
                              bPendingOnFirmware ,mNumTaskInProcesing, mbFlushPipelineInProcessing,
                              mEOS, mSeek, mFrcChange);
                    mRunCond.waitRelative(mLock, 2000000);
                }
            } else {
              mError = true;
              ALOGE("VPP read firmware data error! Thread EXIT...");
              return false;
            }
        }
    }
    ALOGV("after mNumTaskInProcesing %d ...", mNumTaskInProcesing);

    bInputReady = (mInput[mInputProcIdx].mStatus == VPP_BUFFER_LOADED) ? true : false;
    bOutputBufFree = isOutputBufFree();
    bFlushPipeline = ((!bInputReady && (mEOS || mSeek)) || mFrcChange) && (!mbFlushPipelineInProcessing);

    mWait = (!bInputReady || !bOutputBufFree) && (!mSeek && !mEOS && !mFrcChange);
    if (mWait) {
        ALOGV("wait for input/outpu ...");
        mRunCond.wait(mLock);
        ALOGV("wake up from mLock ...");
    }

    ALOGV("before send: bInputReady %d flush: %d flushinProcess %d", bInputReady, bFlushPipeline, mbFlushPipelineInProcessing);

    if (((bInputReady && bOutputBufFree) || bFlushPipeline) && !mbFlushPipelineInProcessing ) {
        bool bGetInBuf = getBufForFirmwareInput(&procBufList, &inputBuf, bFlushPipeline, &procBufNum);
        if (bGetInBuf) {
            if (!bFlushPipeline) {
                flags = mInput[mInputProcIdx].mFlags;
                // get input buffer timestamp
                timeUs = mInput[mInputProcIdx].mTimeUs;
            }
            status_t ret = mVPPWorker->process(inputBuf, procBufList, procBufNum, bFlushPipeline, flags);
            if (ret == STATUS_OK) {
                mNumTaskInProcesing++;
                if (bFlushPipeline) {
                    mbFlushPipelineInProcessing = true;
                    ALOGI("Vpp FlushPipeline set to driver");
                }
                updateFirmwareInputBufStatus(procBufList, procBufNum, timeUs, bFlushPipeline);
            } else {
                ALOGE("process error %d ...", __LINE__);
            }
        }
    }

    ALOGV("Process End: bInputReady %d  tasks %d outbufFree %d", bInputReady, mNumTaskInProcesing, bOutputBufFree);

   return true;
}


bool VPPProcThread::isCurrentThread() const {
    return mThreadId == androidGetThreadId();
}

void VPPProcThread::notifyCheckFrc() {
    mNeedCheckFrc = true;
}

} /* namespace android */
