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

#define ANDROID_DISPLAY_HANDLE 0x18C34078
#define MAX_GRAPHIC_BUFFER_NUMBER 64 // TODO: use GFX limitation first
#define Display unsigned int
#include <stdint.h>

#include <OMX_VPP.h>
#include <portvideo.h>
#include "iVP_api.h"
#include "VPPMds.h"

using namespace android;

class VPPMDSListener;

enum VPPWorkerStatus {
    STATUS_OK = 0,
    STATUS_NOT_SUPPORT,
    STATUS_ALLOCATION_ERROR,
    STATUS_ERROR,
    STATUS_DATA_RENDERING
};

typedef struct {
    uint32_t srcWidth;
    uint32_t srcHeight;
    uint32_t dstWidth;
    uint32_t dstHeight;
    uint32_t denoiseLevel;
    uint32_t sharpnessLevel;
    uint32_t colorBalanceLevel;
    iVP_deinterlace_t deinterlaceType;
    iVP_filter_t scalarType;
    uint32_t frameRate;
    uint32_t hasEncoder;
} FilterParam;

typedef enum _INTEL_SCAN_TYPE
{
    INTEL_SCAN_PROGRESSIV      = 0,  /* Progressive*/
    INTEL_SCAN_TOPFIELD        = 1,  /* Top field */
    INTEL_SCAN_BOTTOMFIELD     = 2   /* Bottom field */
} INTEL_SCAN_TYPE;

typedef enum _INTEL_VIDEOSOURCE_TYPE
{
    INTEL_VideoSourceUnKnown      = 0,/* unKnown stream */
    INTEL_VideoSourceCamera       = 1,/* Camera stream */
    INTEL_VideoSourceVideoEditor  = 2,/* Video Editor stream */
    INTEL_VideoSourceTranscode    = 3,/* Transcode stream */
    INTEL_VideoSourceVideoConf    = 4,/* Video Conference stream*/
    INTEL_VideoSourceMax          = 5 /* Reserve for future use*/
} INTEL_VIDEOSOURCE_TYPE;

typedef enum _INTEL_FRAME_TYPE
{
    INTEL_I_FRAME      = 0,  /* I frame */
    INTEL_P_FRAME      = 1,  /* P frame */
    INTEL_B_FRAME      = 2   /* B Frame */
} INTEL_FRAME_TYPE;

typedef union _INTEL_PRIVATE_VIDEOINFO{
    struct {
        unsigned int legacy       : 16;   /*reserved for legacy OMX usage*/
        INTEL_SCAN_TYPE eScanType : 2;    /*Progressive or interlace*/
        INTEL_VIDEOSOURCE_TYPE eVideoSource: 3 ; /*Camera,VideoEdtor, etc. */
        INTEL_FRAME_TYPE  ePictureType : 2; /*I/P/B*/
        unsigned int      nFrameRate   : 7; /*frame rate*/
        unsigned int      reserved     : 1; /*reserved for extension*/
    }videoinfo;
    unsigned int value;
}INTEL_PRIVATE_VIDEOINFO;

class VPPWorker {

    public:
        static VPPWorker* getInstance();

        // config filters on or off based on video info
        status_t configFilters(uint32_t* filters, const FilterParam* filterParam, const uint32_t flags);

        // Initialize: setupVA()->setupFilters()->setupPipelineCaps()
        status_t init(PortVideo *port);

        // Send input and output buffers to VSP to begin processing
        status_t process(buffer_handle_t input, buffer_handle_t output, uint32_t outputCount, bool isEOS, uint32_t flags);

        // reset index
        status_t reset();
        // set display mode
        void setDisplayMode(int mode);
        // update VPP status
        bool isVppOn();

        ~VPPWorker();

    private:
        VPPWorker();

        VPPWorker(const VPPWorker &);
        VPPWorker &operator=(const VPPWorker &);

    private:
        static VPPWorker* mVPPWorker;
        PortVideo *mPort;

        iVPCtxID mVPContext;
        bool mVPStarted;

        // VPP filter configuration
        uint32_t mFilters;
        FilterParam mFilterParam;

        //display mode
        int mPreDisplayMode;
        int mDisplayMode;
        //VPP status
        bool mVppOn;
        bool mVPPSettingUpdated;
        sp<VPPMDSListener> mMds;

        //3p reconfigure
        bool m3PReconfig;

        //debug flag
        int mDebug;

};

#endif //VPPWorker_H_
