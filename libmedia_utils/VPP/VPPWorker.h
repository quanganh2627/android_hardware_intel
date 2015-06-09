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

#ifndef VPPWorker_H_
#define VPPWorker_H_

#include <va/va.h>
#include <va/va_vpp.h>
#include <va/va_tpi.h>

#define ANDROID_DISPLAY_HANDLE 0x18C34078
#define MAX_GRAPHIC_BUFFER_NUMBER 64 // TODO: use GFX limitation first
#include "va/va_android.h"
#define Display unsigned int
#include <stdint.h>
#include "VPPMds.h"

#include <android/native_window.h>
//HDMI max timing is defined drm_hdmi.h
#define HDMI_TIMING_MAX (128)

#ifdef TARGET_HAS_MULTIPLE_DISPLAY
using namespace android::intel;
#endif

namespace android {
class VPPMDSListener;

typedef enum _FRC_RATE {
    FRC_RATE_1X = 1,
    FRC_RATE_2X,
    FRC_RATE_2_5X,
    FRC_RATE_4X
}FRC_RATE;

typedef enum _IINPUT_FRAME_RATE {
    FRAME_RATE_0 = 0,
    FRAME_RATE_15 = 15,
    FRAME_RATE_24 = 24,
    FRAME_RATE_25 = 25,
    FRAME_RATE_30 = 30,
    FRAME_RATE_50 = 50,
    FRAME_RATE_60 = 60
} IPNPUT_FRAME_RATE;

enum VPPWorkerStatus {
    STATUS_OK = 0,
    STATUS_NOT_SUPPORT,
    STATUS_ALLOCATION_ERROR,
    STATUS_ERROR,
    STATUS_DATA_RENDERING
};

struct GraphicBufferConfig {
    uint32_t colorFormat;
    uint32_t stride;
    uint32_t width;
    uint32_t height;
    uint32_t buffer[MAX_GRAPHIC_BUFFER_NUMBER];
};

class VPPWorker {

    public:
        static VPPWorker* getInstance(const sp<ANativeWindow> &nativeWindow);

        // config filters on or off based on video info
        status_t configFilters(const uint32_t width, const uint32_t height, const uint32_t fps, const uint32_t slowMotionFactor = 1, const uint32_t flags = 0);

        // Initialize: setupVA()->setupFilters()->setupPipelineCaps()
        status_t init();

        // Get output buffer number needed for processing
        uint32_t getProcBufCount();

        // Get output buffer number needed for filling
        uint32_t getFillBufCount();

        // Send input and output buffers to VSP to begin processing
        status_t process(sp<GraphicBuffer> input, Vector< sp<GraphicBuffer> > output, uint32_t outputCount, bool isEOS, uint32_t flags);

        // Fill output buffers given, it's a blocking call
        status_t fill(Vector< sp<GraphicBuffer> > outputGraphicBuffer, uint32_t outputCount);

        // Initialize graphic configuration buffer
        status_t setGraphicBufferConfig(sp<GraphicBuffer> graphicBuffer);

        // Check if the input NativeWindow handle is the same as the one when construction
        bool validateNativeWindow(const sp<ANativeWindow> &nativeWindow);
        // reset index
        status_t reset();

        // set video display mode
        void setDisplayMode(int32_t mode);

        // get video display mode
        int32_t getDisplayMode();

        // check HDMI connection status
        bool isHdmiConnected();

        /* config enable or disable VPP frame rate conversion for HDMI feature.
         * To enable this feature, MDS listener is MUST
         */
        status_t configFrc4Hdmi(bool enableFrc4Hdmi, sp<VPPMDSListener>* pmds);

        uint32_t getVppOutputFps();
        status_t calculateFrc(bool *frcOn, FRC_RATE *rate);

        ~VPPWorker();

    private:
        VPPWorker(const sp<ANativeWindow> &nativeWindow);
        // Check if VPP is supported
        bool isSupport() const;

        // Create VA context
        status_t setupVA();

        // Destroy VA context
        status_t terminateVA();

        // Check filter caps and create filter buffers
        status_t setupFilters();

        // Setup pipeline caps
        status_t setupPipelineCaps();

        // Map GraphicBuffer to VASurface
        VASurfaceID mapBuffer(sp<GraphicBuffer> graphicBuffer);

        /* calcualte VPP FRC rate according to input video frame rate.
         * Here set VPP output target fps as MIPI fresh rate, 60
         * input are limited to 24, 25, 30, and 60
         */
        status_t calcFrcByInputFps(bool *FrcOn, FRC_RATE *FrcRat);

        /* calcualte VPP FRC rate according to hdmi capability
         * Here set VPP output target fps to match HDMI supported.
         * If all VPP FRC output fps failed to match HDMI capability, disabled FRC.
         * input fps are limited to 24, 25, 30, and 60
         */
        status_t calcFrcByMatchHdmiCap(bool *FrcOn, FRC_RATE *FrcRate);

        /* read HDMI device capability and current HDMI setting
         */
        status_t getHdmiData();

        // Get output buffer needed based on input index
        uint32_t getOutputBufCount(uint32_t index);

        //check if the input fps is suportted in array fpsSet.
        bool isFpsSupport(int32_t fps, int32_t *fpsSet, int32_t fpsSetCnt);

        // Debug only
        // Dump YUV frame
        status_t dumpYUVFrameData(VASurfaceID surfaceID);
        status_t writeNV12(int width, int height, unsigned char *out_buf, int y_pitch, int uv_pitch);

        VPPWorker(const VPPWorker &);
        VPPWorker &operator=(const VPPWorker &);
        uint32_t isVppOn();

    public:
        uint32_t mNumForwardReferences;
        FRC_RATE mFrcRate;
        bool mFrcOn;
        //updated FrrRate used in VPP frc for HDMI only
        FRC_RATE mUpdatedFrcRate;
        bool mUpdatedFrcOn;
        bool bNeedCheckFrc;

    private:
        // Graphic buffer
        static VPPWorker* mVPPWorker;
        static sp<ANativeWindow> mNativeWindow;
        uint32_t mGraphicBufferNum;
        struct GraphicBufferConfig mGraphicBufferConfig;

        // video info
        uint32_t mWidth;
        uint32_t mHeight;
        uint32_t mInputFps;

        // VA common variables
        bool mVAStarted;
        VAContextID mVAContext;
        Display * mDisplay;
        VADisplay mVADisplay;
        VAConfigID mVAConfig;
        uint32_t mNumSurfaces;
        VASurfaceID *mSurfaces;
        VASurfaceAttribExternalBuffers *mVAExtBuf;

        // Forward References Surfaces
        VASurfaceID *mForwardReferences;
        VASurfaceID mPrevInput;

        // VPP Filters Buffers
        uint32_t mNumFilterBuffers;
        VABufferID mFilterBuffers[VAProcFilterCount];

        // VPP filter configuration
        bool mDeblockOn;
        bool mDenoiseOn;
        bool mDeinterlacingOn;
        bool mSharpenOn;
        bool mColorOn;
        VABufferID mFilterFrc;

        // status
        uint32_t mInputIndex;
        uint32_t mOutputIndex;
        //MDS Display mode
        int32_t mDisplayMode;
        bool mEnableFrc4Hdmi;

        sp<VPPMDSListener> *mMds;
        MDSHdmiTiming currHdmiTiming;
        MDSHdmiTiming *hdmiTimingList;
        int32_t hdmiListCount;

        uint32_t mVPPOn;

        // FIXME: not very sure how to check color standard
        VAProcColorStandardType in_color_standards[VAProcColorStandardCount];
        VAProcColorStandardType out_color_standards[VAProcColorStandardCount];
};

}
#endif //VPPWorker_H_
