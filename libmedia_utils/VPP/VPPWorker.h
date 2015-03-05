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

#include <android/native_window.h>

namespace android {

typedef enum _FRC_RATE {
    FRC_RATE_1X = 1,
    FRC_RATE_2X,
    FRC_RATE_2_5X,
    FRC_RATE_4X
}FRC_RATE;

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
        status_t configFilters(const uint32_t width, const uint32_t height, const uint32_t fps, const uint32_t slowMotionFactor = 1);

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

        // Get output buffer needed based on input index
        uint32_t getOutputBufCount(uint32_t index);

        // Debug only
        // Dump YUV frame
        status_t dumpYUVFrameData(VASurfaceID surfaceID);
        status_t writeNV12(int width, int height, unsigned char *out_buf, int y_pitch, int uv_pitch);

        VPPWorker(const VPPWorker &);
        VPPWorker &operator=(const VPPWorker &);

    public:
        uint32_t mNumForwardReferences;
        FRC_RATE mFrcRate;
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
        bool mFrcOn;
        VABufferID mFilterFrc;

        // status
        uint32_t mInputIndex;
        uint32_t mOutputIndex;

        // FIXME: not very sure how to check color standard
        VAProcColorStandardType in_color_standards[VAProcColorStandardCount];
        VAProcColorStandardType out_color_standards[VAProcColorStandardCount];
};

}
#endif //VPPWorker_H_
