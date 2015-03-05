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

#ifndef __VPP_PROCESSOR_H
#define __VPP_PROCESSOR_H
#include "VPPBuffer.h"
#include "VPPProcThread.h"
#include "VPPWorker.h"
#include "VPPSetting.h"

#include <stdint.h>

#include <android/native_window.h>
#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/OMXCodec.h>

namespace android {

struct MediaBuffer;
struct MediaBufferObserver;
struct OMXCodec;
class VPPProcThread;

class VPPProcessor : public MediaBufferObserver {
public:
    /* Single instance
     * Only create VPPProcessor once and return handle to client that construct it
     */
    static VPPProcessor* getInstance(const sp<ANativeWindow> &native, OMXCodec* codec);
    virtual ~VPPProcessor();

    /* Set video clip info and calculate buffer number needed
     * @param
     *      info: video clip info
     *      slowMotionFactor: slow motion factor, 1 mean no slow motin effect
     * @return:
     *      VPP_OK: success
     *      VPP_FAIL: fail
     */
    status_t validateVideoInfo(VPPVideoInfo* info, uint32_t slowMotionFactor = 1);

    /*
     * Get VPP on/off status from VppSettings
     * @return:
     *      true: vpp on
     *      false: vpp off
     */
    static bool isVppOn();

    /* In this init() function, firstly, bufferInfo will be set as OMXCodec's,
     * and then VPPWorker will be initialized. After both steps succeed,
     * VPPThread starts to run.
     * @return:
     *      VPP_OK: success
     *      VPP_FAIL:fail
     */
    status_t init();

    /*
     * Set VPPWorker::mSeek flag to true, send run signal to VPPThread to
     * make sure VPPThread is activated, and then wait for VPPThread's ready
     * signal to reset all input and output buffers.
     */
    void seek();

    /*
     * Check whether there is empty input buffer to put decoder buffer in,
     * or RenderList is empty. Input buffer, output buffer and RenderList
     * will also be updated in it.
     * @return:
     *     true: need to set data into VPP
     *     false: NO need to set data into VPP
     */
    bool canSetDecoderBufferToVPP();

    /*
     * Set video decoder buffer to VPPProcesor, this buffer will be inserted
     * into RenderList, as well as input buffer if there is a room.
     * @param:
     *      buff: video decoder buffer
     * @return:
     *      VPP_OK: success
     *      VPP_FAIL: fail
     */
    status_t setDecoderBufferToVPP(MediaBuffer *buff);

    /*
     * Read buffer out from RenderList for rendering
     * @param:
     *      buffer: the buffers from Renderlist
     * @return:
     *      VPP_OK: success
     *      VPP_FAIL: fail
     *      VPP_BUFFER_NOT_READY: no buffer available to render
     *      ERROR_END_OF_STREAM: got end of stream
     */
    virtual status_t read(MediaBuffer **buffer);

    /*
     * Callback function for release MediaBuffer
     * (This is the virtual function of MediaBufferObserver)
     * @param:
     *      buffer: the buffer is releasing
     */
    virtual void signalBufferReturned(MediaBuffer *buffer);

     /*
      * indicate video stream has reached to end
      */
     void setEOS();

     /* return VPP output video frame rate.
      * 1. If Vpp FRC is on, Output FPS is after converted, which is different from input.
      *    If VPP FRC is off, output fps is the same as input fps.
      *
      * 2. If VPP is off, input is the same as output.
      * 3. unsupported video size, neithere VPP nor the method are not available for APP.
      */
     uint32_t getVppOutputFps();

public:
    // number of extra input buffer needed by VPP
    uint32_t mInputBufferNum;
    // number of output buffer needed by VPP
    uint32_t mOutputBufferNum;

private:
    // construction
    VPPProcessor(const sp<ANativeWindow> &native, OMXCodec* codec);
    // init inputBuffer and outBuffer
    status_t initBuffers();
    // completely release all buffers
    void releaseBuffers();
    // before seek
    bool hasProcessingBuffer();
    // reset
    status_t reset();
    // flush buffers and renderlist for seek
    void flush();
    // create threads and run
    status_t createThread();
    // stop thread if needed
    void quitThread();
    // return the BufferInfo accordingly to MediaBuffer
    OMXCodec::BufferInfo *findBufferInfo(MediaBuffer *buff);
    // cancel MediaBuffer to native window
    status_t cancelBufferToNativeWindow(MediaBuffer *buff);
    // dequeue MediaBuffer from native window
    MediaBuffer * dequeueBufferFromNativeWindow();
    // get MediaBuffer's time stamp from meta data field
    int64_t getBufferTimestamp(MediaBuffer * buff);
    // release useless input buffers as well
    status_t clearInput();
    // add output buffer into Renderlist
    status_t updateRenderList();
    // return MediaBuffer according to VPPBuffer
    MediaBuffer * findMediaBuffer(VPPBuffer &buff);
    // debug only
    void printBuffers();
    void printRenderList();

    VPPProcessor(const VPPProcessor &);
    VPPProcessor &operator=(const VPPProcessor &);

private:
    // VPPProcessor instance
    static VPPProcessor* mVPPProcessor;
    // buffer info for VPP input
    VPPBuffer mInput[VPPBuffer::MAX_VPP_BUFFER_NUMBER];
    // buffer info for VPP output
    VPPBuffer mOutput[VPPBuffer::MAX_VPP_BUFFER_NUMBER];
    // mRenderList is used to render
    List<MediaBuffer *> mRenderList;
    // input load point
    uint32_t mInputLoadPoint;
    // output load to RenderList point
    uint32_t mOutputLoadPoint;

    MediaBuffer* mLastRenderBuffer;

    sp<VPPProcThread> mProcThread;
    friend class VPPProcThread;
    VPPWorker* mWorker;

    sp<ANativeWindow> mNativeWindow;
    OMXCodec* mCodec;
    // mBufferInfos is all buffer Infos allocated by OMXCodec
    Vector<OMXCodec::BufferInfo> * mBufferInfos;
    bool mThreadRunning;
    bool mEOS;
    uint32_t mTotalDecodedCount, mInputCount, mVPPProcCount, mVPPRenderCount;
    uint32_t mVppOutputFps;
};

} /* namespace android */

#endif /* __VPP_PROCESSOR_H */
