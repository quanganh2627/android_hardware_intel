/*
 * Copyright (c) 2008-2012, Intel Corporation. All rights reserved.
 *
 * Redistribution.
 * Redistribution and use in binary form, without modification, are
 * permitted provided that the following conditions are met:
 *  * Redistributions must reproduce the above copyright notice and
 * the following disclaimer in the documentation and/or other materials
 * provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its
 * suppliers may be used to endorse or promote products derived from
 * this software without specific  prior written permission.
 *  * No reverse engineering, decompilation, or disassembly of this
 * software is permitted.
 *
 * Limited patent license.
 * Intel Corporation grants a world-wide, royalty-free, non-exclusive
 * license under patents it now or hereafter owns or controls to make,
 * have made, use, import, offer to sell and sell ("Utilize") this
 * software, but solely to the extent that any such patent is necessary
 * to Utilize the software alone, or in combination with an operating
 * system licensed under an approved Open Source license as listed by
 * the Open Source Initiative at http://opensource.org/licenses.
 * The patent license shall not apply to any other combinations which
 * include this software. No hardware per se is licensed hereunder.
 *
 * DISCLAIMER.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors:
 *    Jackie Li <yaodong.li@intel.com>
 *
 */
#ifndef __INTEL_HWCOMPOSER_CPP__
#define __INTEL_HWCOMPOSER_CPP__

#define HWC_REMOVE_DEPRECATED_VERSIONS 1

#include <EGL/egl.h>
#include <hardware/hwcomposer.h>

#include <IntelHWComposerDrm.h>
#include <IntelBufferManager.h>
#include <IntelHWComposerLayer.h>
#include <IntelHWComposerDump.h>
#include <IntelVsyncEventHandler.h>
#include <IntelFakeVsyncEvent.h>
#include <IntelDisplayDevice.h>
#ifdef INTEL_RGB_OVERLAY
#include <IntelHWCWrapper.h>
#endif
class IntelHWComposer : public hwc_composer_device_1_t, public IntelHWCUEventObserver, public IntelHWComposerDump  {
public:
    enum {
        VSYNC_SRC_MIPI = 0,
        VSYNC_SRC_HDMI,
        VSYNC_SRC_FAKE,
        VSYNC_SRC_NUM,
    };
    enum {
        DISPLAY_NUM = 3,
    };
private:
    IMG_gralloc_module_public_t* mGrallocModule;
    IntelHWComposerDrm *mDrm;
    IntelBufferManager *mBufferManager;
    IntelBufferManager *mGrallocBufferManager;
    IntelBufferManager *mCursorBufferManager;
    IntelDisplayBuffer *cursorDataBuffer;
    IntelDisplayPlaneManager *mPlaneManager;
    IntelDisplayDevice *mDisplayDevice[DISPLAY_NUM];
    hwc_procs_t const *mProcs;
    android::sp<IntelVsyncEventHandler> mVsync;
    android::sp<IntelFakeVsyncEvent> mFakeVsync;
    nsecs_t mLastVsync;
    struct hdmi_fb_handler {
        uint32_t umhandle;
        uint32_t kmhandle;
        uint32_t size;
    } mHDMIFBHandle;
    WidiExtendedModeInfo mExtendedModeInfo;

    android::Mutex mLock;
    bool mInitialized;
    uint32_t mActiveVsyncs;
    uint32_t mVsyncsEnabled;
    uint32_t mVsyncsCount;
    nsecs_t mVsyncsTimestamp;

    mutable android::Mutex mHpdLock;
    android::Condition mHpdCondition;
    bool mHpdCompletion;
#ifdef INTEL_RGB_OVERLAY
    IntelHWCWrapper mWrapper;
#endif
private:
    bool handleHotplugEvent(int hdp, void *data);
    bool handleDisplayModeChange();
    bool handleDynamicModeSetting(void *data);
    uint32_t disableUnusedVsyncs(uint32_t target);
    uint32_t enableVsyncs(uint32_t target);
    uint32_t getTargetVsync();
    bool needSwitchVsyncSrc();
    bool vsyncControl_l(int enabled);
    void signalHpdCompletion();
    void waitForHpdCompletion();
    static IMG_native_handle_t *findVideoHandle(hwc_display_contents_1_t* list);

    bool mForceDumpPostBuffer;
    int dumpPost2Buffers(int num, buffer_handle_t* buffer);
    int dumpLayerLists(size_t numDisplays, hwc_display_contents_1_t** displays);
    bool checkPresentationMode(hwc_display_contents_1_t*, hwc_display_contents_1_t*);
public:
    bool onUEvent(int msgType, void* msg, int msgLen);
    void vsync(int64_t timestamp, int pipe);
public:
    bool initCheck() { return mInitialized; }
    bool initialize();
    bool vsyncControl(int enabled);
    bool release();
    bool dump(char *buff, int buff_len, int *cur_len);
    bool dumpDisplayStat();
    void registerProcs(hwc_procs_t const *procs) { mProcs = procs; }
#ifdef INTEL_RGB_OVERLAY
    IntelHWCWrapper* getWrapper() { return &mWrapper; }
#endif
    bool prepareDisplays(size_t numDisplays, hwc_display_contents_1_t** displays);
    bool commitDisplays(size_t numDisplays, hwc_display_contents_1_t** displays);
    bool blankDisplay(int disp, int blank);
    bool getDisplayConfigs(int disp, uint32_t* configs, size_t* numConfigs);
    bool getDisplayAttributes(int disp, uint32_t config,
            const uint32_t* attributes, int32_t* values);
    bool setFramecount(int cmd, int count, int x, int y);
    IntelHWComposer()
        : IntelHWCUEventObserver(), IntelHWComposerDump(),
          mDrm(0), mBufferManager(0), mGrallocBufferManager(0),
          mCursorBufferManager(0), cursorDataBuffer(0),
          mPlaneManager(0),mProcs(0), mVsync(0), mFakeVsync(0),
          mLastVsync(0), mInitialized(false),
          mActiveVsyncs(0), mHpdCompletion(true), mForceDumpPostBuffer(false) {}
    ~IntelHWComposer();
};

#endif /*__INTEL_HWCOMPOSER_CPP__*/
