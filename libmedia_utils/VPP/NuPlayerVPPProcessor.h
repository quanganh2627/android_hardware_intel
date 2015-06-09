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

#ifndef NUPLAYER_VPPPROCESSOR_H_
#define NUPLAYER_VPPPROCESSOR_H_

#include "VPPBuffer.h"
#include "VPPProcThread.h"
#include "VPPSetting.h"
#include <media/stagefright/foundation/AHandler.h>
#include <media/stagefright/NativeWindowWrapper.h>
#include <media/stagefright/ACodec.h>
#include "VPPMds.h"
#ifdef USE_IVP
#include "ivp/VPPWorker.h"
#else
#include "VPPWorker.h"
#endif

#include "VPPMds.h"

namespace android {

struct ABuffer;
struct ACodec;
class VPPProcessorBase;
class VPPMDSListener;

struct NuPlayerVPPProcessor : public AHandler, VPPProcessorBase {
public:
    static NuPlayerVPPProcessor* getInstance(const sp<AMessage> &notify,
            const sp<NativeWindowWrapper> &nativeWindow = NULL);

    /*
     * initialize. including init buffers, VPPWorker, and start threads.
     * @param:
     *      codec: ACodec handle
     * @return:
     *      VPP_OK: success
     *      VPP_FAIL: fail
     */
    status_t init(sp<ACodec>& codec);

    /*
     * set video clip info to VPPProcessor
     * @param:
     *      videoInfo: video info need to set to vpp
     * @return:
     *      VPP_OK: success
     *      VPP_FAIL: fail
     */
    status_t validateVideoInfo(VPPVideoInfo *videoInfo);

    /*
     * invoke VPPProcThread
     */
    void invokeThreads();

    /*
     * check whether there are free buffer to put decoder buffer in
     * @return:
     *      VPP_OK:  can set decoder to vpp
     *      VPP_FAIL: cannot set decoder to vpp
     */
    status_t canSetBufferToVPP();

    /*
     * set decoder buffer to vpp
     * @param:
     *      buffer: decoder buffer
     *      notifyConsumed: message originally from decoder
     * @return:
     *      VPP_OK: successfully set buffer to vpp
     *      VPP_FAIL: set buffer to vpp fail
     */
    status_t setBufferToVPP(const sp<ABuffer> &buffer, const sp<AMessage> &notifyConsumed);

    /*
     * get vpp output buffer, set send this buffer to NuPlayerRenderer by message
     */
    void getBufferFromVPP();

    /*
     * seek and flush buffers
     */
    void seek();

    /*
     * indicate video stream has reached to end
     */
    void setEOS();

    /*
     * When NuPlayer is releasing, we need to flush NuPlayerVPPProcessor also,
     * in this function, quit thread first, then flush all buffers.
     */
    void flushShutdown();

    /*
     * get VPP output frame rate.
     */
    uint32_t getVppOutputFps();

    /* set display information to VPP
     */
    void setDisplayMode(int32_t mode);

    /* config enable/disable VPP frame rate conversion for HDMI
     */
    status_t configFrc4Hdmi(bool enable);

public:
    // number of extra input buffer needed by VPP
    uint32_t mInputBufferNum;
    // number of output buffer needed by VPP
    uint32_t mOutputBufferNum;

    enum {
        kWhatUpdateVppOutput = 'quvO',
        kWhatUpdateVppInput  = 'quvI',
    };

protected:
    virtual ~NuPlayerVPPProcessor();
    virtual void onMessageReceived(const sp<AMessage> &msg);

private:
    enum {
        kWhatFreeBuffer     = 'freB',
    };

    static NuPlayerVPPProcessor* mNuPlayerVPPProcessor;
    // buffer info for VPP input
    VPPBuffer mInput[VPPBuffer::MAX_VPP_BUFFER_NUMBER];
    // buffer info for VPP output
    VPPBuffer mOutput[VPPBuffer::MAX_VPP_BUFFER_NUMBER];
    // input load point
    uint32_t mInputLoadPoint;
    // output load point
    uint32_t mOutputLoadPoint;
    // total input count
    uint32_t mInputCount;

    // vpp notify
    sp<AMessage> mNotify;
    // VPPProceThread
    sp<VPPProcThread> mProcThread;
    bool mThreadRunning;
    VPPWorker * mWorker;
    sp<NativeWindowWrapper> mNativeWindow;
    Vector<ACodec::BufferInfo> * mBufferInfos;
    bool mEOS;
    int64_t mLastInputTimeUs;

    sp<ACodec> mACodec;
    sp<VPPMDSListener> mMds;

private:
    NuPlayerVPPProcessor(const sp<AMessage> &notify,
            const sp<NativeWindowWrapper> &nativeWindow = NULL);
    // init inputBuffer and outBuffer
    status_t initBuffers();
    // completely release all buffers
    void releaseBuffers();
    // clear input buffer array
    void postAndResetInput(uint32_t index);
    // find buffer info by buffer id
    ACodec::BufferInfo * findBufferByID(IOMX::buffer_id bufferID);
    // find buffer info by graphic buffer handle
    ACodec::BufferInfo * findBufferByGraphicBuffer(sp<GraphicBuffer> graphicBuffer);
    // dequeue BufferInfo from native window
    ACodec::BufferInfo * dequeueBufferFromNativeWindow();
    status_t cancelBufferToNativeWindow(ACodec::BufferInfo *info);
    // free buffer when receiving kWhatFreeBuffer message
    void onFreeBuffer(const sp<AMessage> &msg);
    int64_t getBufferTimestamp(sp<ABuffer> buffer);
    void printBuffers();
    void quitThread();
    // flush buffers without quit threads
    void flushNoShutdown();
    // bofore flush
    bool hasProcessingBuffer();

    DISALLOW_EVIL_CONSTRUCTORS(NuPlayerVPPProcessor);
};

}  // namespace android

#endif  // NUPLAYER_VPPPROCESSOR_H_
