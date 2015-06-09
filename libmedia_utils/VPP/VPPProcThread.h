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

#ifndef __VPP_PROC_THREAD_H
#define __VPP_PROC_THREAD_H

#include <media/stagefright/MetaData.h>
#include "VPPBuffer.h"
#ifdef USE_IVP
#include "ivp/VPPWorker.h"
#else
#include "VPPWorker.h"
#endif
#include "VPPMds.h"

#include <utils/threads.h>
#include <utils/Errors.h>


namespace android {
class VPPWorker;

class VPPProcThread : public Thread {
    public:

        VPPProcThread(bool canCallJava, VPPWorker* vppWorker,
                VPPBuffer *inputBuffer, const uint32_t inputBufferNum,
                VPPBuffer *outputBuffer, const uint32_t outputBufferNum);
        virtual ~VPPProcThread();

        virtual status_t readyToRun();

        // Derived class must implement threadLoop(). The thread starts its life
        // here. There are two ways of using the Thread object:
        // 1) loop: if threadLoop() returns true, it will be called again if
        //          requestExit() wasn't called.
        // 2) once: if threadLoop() returns false, the thread will exit upon return.
        virtual bool threadLoop();
        bool isCurrentThread() const;

        bool isReadytoRun();
        void notifyCheckFrc();

    public:
        Mutex mLock;
        Mutex mEndLock;
        // main thread send this condition to VPPProcThread,
        // VPPProcThread wait this condition to run
        Condition mRunCond;
        Condition mEndCond;
        bool mWait;
        bool mError;
        bool mEOS;
        bool mSeek;

    private:
        VPPProcThread(const VPPProcThread &);
        VPPProcThread &operator=(const VPPProcThread &);
        bool getBufForFirmwareOutput(Vector< sp<GraphicBuffer> > *fillBufList,
                             uint32_t *fillBufNum);
        int updateFirmwareOutputBufStatus(Vector< sp<GraphicBuffer> > fillBufList,
                                    uint32_t fillBufNum);
        bool getBufForFirmwareInput(Vector< sp<GraphicBuffer> > *procBufList,
                            sp<GraphicBuffer> *inputBuf,
                            bool bFlushPipeline,
                            uint32_t *procBufNum );
        int updateFirmwareInputBufStatus(Vector< sp<GraphicBuffer> > procBufList,
                                   uint32_t procBufNum, int64_t timeUs,
                                   bool bFlushPipeline);
        bool isOutputBufFree();

    private:
        android_thread_id_t mThreadId;
        VPPWorker *mVPPWorker;
        VPPBuffer *mInput;
        VPPBuffer *mOutput;
        const uint32_t mInputBufferNum;
        const uint32_t mOutputBufferNum;
        uint32_t mInputProcIdx;
        uint32_t mOutputProcIdx;
        bool mFlagEnd;
        uint32_t mNumTaskInProcesing;
        bool mFirstInputFrame;
        uint32_t mInputFillIdx;
        uint32_t mOutputFillIdx;
        bool mbFlushPipelineInProcessing;
        bool mFrcChange;
        bool mNeedCheckFrc;
};

} /* END namespace android */
#endif /* __VPP_THREAD_H*/
