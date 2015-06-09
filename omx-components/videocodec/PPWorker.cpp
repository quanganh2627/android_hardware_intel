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
#include <cutils/properties.h>
#include <OMX_Core.h>
#include <OMX_IVCommon.h>
#include <OMX_VPP.h>
#include <system/graphics.h>
#include <system/window.h>

//#define LOG_NDEBUG 0
#define LOG_TAG "PPWorker"

#include "PPWorker.h"
#include "VPPSetting.h"
#if defined (TARGET_HAS_MULTIPLE_DISPLAY)
#include <display/MultiDisplayService.h>
using namespace android::intel;
#endif

#define CHECK_VASTATUS(str) \
    do { \
        if (vaStatus != VA_STATUS_SUCCESS) { \
                LOGE("%s failed\n", str); \
                return STATUS_ERROR;}   \
        }while(0);

enum STRENGTH {
    STRENGTH_LOW = 0,
    STRENGTH_MEDIUM,
    STRENGTH_HIGH
};

#define DENOISE_DEBLOCK_STRENGTH STRENGTH_MEDIUM
#define COLOR_STRENGTH STRENGTH_MEDIUM
#ifdef TARGET_VPP_USE_GEN
#define COLOR_NUM 4
#else
#define COLOR_NUM 2
#endif

#define MAX_FRC_OUTPUT 4 /*for frcx4*/
#define DEINTERLACE_NUM 1

#define QVGA_AREA (320 * 240)
#define VGA_AREA (640 * 480)
#define HD1080P_AREA (1920 * 1080)

#define DEFAULT_FRAME_RATE 30
#define DEFAULT_DENOISE_LEVEL 64
#define MIN_DENOISE_LEVEL 0
#define MAX_DENOISE_LEVEL 64
#define DEFAULT_SHARPNESS_LEVEL 32
#define MIN_SHARPNESS_LEVEL 0
#define MAX_SHARPNESS_LEVEL 64
#define DEFAULT_COLORBALANCE_LEVEL 32
#define MIN_COLORBALANCE_LEVEL 0
#define MAX_COLORBALANCE_LEVEL 64


VPPWorker::VPPWorker()
    :   mVPStarted(false),
        mPort(NULL),
        mFilters(0),
        mPreDisplayMode(0),
        mDisplayMode(0),
        mDebug(0),
        mVppOn(false),
        mVPPSettingUpdated(false),
        mMds(NULL),
        m3PReconfig(false) {
    memset(&mFilterParam, 0, sizeof(mFilterParam));
#ifdef TARGET_HAS_MULTIPLE_DISPLAY
    mMds = new VPPMDSListener(this);
#endif
}

//static
VPPWorker* VPPWorker::mVPPWorker = NULL;

//static
VPPWorker* VPPWorker::getInstance() {
    if (mVPPWorker == NULL)
        mVPPWorker = new VPPWorker();
    return mVPPWorker;
}

status_t VPPWorker::init(PortVideo *port) {
    if (!mVPStarted) {
        iVP_status status = iVP_create_context(&mVPContext, 1200, 800, 1);
        if (status == IVP_STATUS_SUCCESS)
            mVPStarted = true;
        else
            return STATUS_ERROR;
    }

    if (port == NULL) {
        ALOGE("%s: invalid output port", __func__);
        return STATUS_ERROR;
    } else
        mPort = port;

    char propValueString[PROPERTY_VALUE_MAX];

    property_get("media.3p.debug", propValueString, "0");
    mDebug = atoi(propValueString);
#ifdef TARGET_HAS_MULTIPLE_DISPLAY
    if (mMds->init() != STATUS_OK)
        return STATUS_ERROR;
#endif

    //update vpp status here
    isVppOn();

    //set to default value
    mFilterParam.frameRate = DEFAULT_FRAME_RATE;
    mFilterParam.denoiseLevel = DEFAULT_DENOISE_LEVEL;
    mFilterParam.sharpnessLevel = DEFAULT_SHARPNESS_LEVEL;
    mFilterParam.colorBalanceLevel = DEFAULT_COLORBALANCE_LEVEL;

    return STATUS_OK;
}

status_t VPPWorker::configFilters(uint32_t* filters,
                                  const FilterParam* filterParam,
                                  const uint32_t flags) {
    if (!filterParam) {
        ALOGE("%s: invalid filterParam", __func__);
        return STATUS_ERROR;
    }

    //need to update vpp status if VPP settings has been modified.
    if (mVPPSettingUpdated) {
        if(isVppOn())
            m3PReconfig = true;
        mVPPSettingUpdated = false;
    }

    // reset filter type to 0
    mFilters = 0;
    mFilterParam.srcWidth = filterParam->srcWidth;
    mFilterParam.srcHeight = filterParam->srcHeight;
    mFilterParam.dstWidth = filterParam->dstWidth;
    mFilterParam.dstHeight = filterParam->dstHeight;
    mFilterParam.scalarType = filterParam->scalarType;
    mFilterParam.deinterlaceType = filterParam->deinterlaceType;

    if (mVppOn) {
        mFilterParam.frameRate = filterParam->frameRate;
        mFilterParam.hasEncoder = filterParam->hasEncoder;

        if (filterParam->denoiseLevel >= MIN_DENOISE_LEVEL
                && filterParam->denoiseLevel <= MAX_DENOISE_LEVEL)
            mFilterParam.denoiseLevel = filterParam->denoiseLevel;
        if (filterParam->sharpnessLevel >= MIN_SHARPNESS_LEVEL
                && filterParam->sharpnessLevel <= MAX_SHARPNESS_LEVEL)
            mFilterParam.sharpnessLevel = filterParam->sharpnessLevel;
        if (filterParam->colorBalanceLevel >= MIN_COLORBALANCE_LEVEL
                && filterParam->colorBalanceLevel <= MAX_COLORBALANCE_LEVEL)
            mFilterParam.colorBalanceLevel = filterParam->colorBalanceLevel;

        uint32_t area = filterParam->srcWidth * filterParam->srcHeight;

        if (area <= VGA_AREA)
            mFilters |= OMX_INTEL_ImageFilterDenoise;

        if (area <= HD1080P_AREA)
            mFilters |= OMX_INTEL_ImageFilterSharpness;

        if ((mDisplayMode & MDS_HDMI_CONNECTED) == 0 &&
                (mDisplayMode & MDS_WIDI_ON) == 0 &&
                (mFilterParam.hasEncoder == 0)) {
            mFilters |= OMX_INTEL_ImageFilter3P;
            ALOGV("%s: Enable 3P filter.", __func__);
        }
    }

    if ((flags & OMX_BUFFERFLAG_TFF) != 0 ||
            (flags & OMX_BUFFERFLAG_BFF) != 0)
        mFilters |= OMX_INTEL_ImageFilterDeinterlace;

    *filters = mFilters;

    return STATUS_OK;
}

status_t VPPWorker::process(buffer_handle_t inputBuffer,
                            buffer_handle_t outputBuffer,
                            uint32_t outputCount, bool isEOS, uint32_t flags) {
    ALOGV("process: outputCount=%d, mInputIndex=%d", outputCount, mInputIndex);
    iVP_layer_t   primarySurf;
    iVP_layer_t   outSurf[DEINTERLACE_NUM];
    int angle = 0, i = 0;
    INTEL_PRIVATE_VIDEOINFO videoInfo;

    videoInfo.value = flags;

    if (isEOS) {
        ALOGI("%s: EOS flag is detected", __func__);
        return STATUS_OK;
    }

    if (outputCount < 1 || outputCount > DEINTERLACE_NUM) {
       ALOGE("%s: invalid outputCount", __func__);
       return STATUS_ERROR;
    }

    memset(&primarySurf,0,sizeof(iVP_layer_t));
    memset(&outSurf,0,sizeof(iVP_layer_t));

    iVP_rect_t priSrect;
    priSrect.left  = 0;
    priSrect.top   = 0;
    priSrect.width = mFilterParam.srcWidth;
    priSrect.height  = mFilterParam.srcHeight;

    iVP_rect_t priDrect;
    priDrect.left  = 0;
    priDrect.top   = 0;
    priDrect.width = mFilterParam.dstWidth;
    priDrect.height  = mFilterParam.dstHeight;

    primarySurf.srcRect = &priSrect;
    primarySurf.destRect = &priDrect;
    primarySurf.bufferType    = IVP_GRALLOC_HANDLE; //TODO: it only support buffer_handle_t now
    primarySurf.rotation = (iVP_rotation_t)(angle/90);
    primarySurf.gralloc_handle = inputBuffer;
    primarySurf.filter = mFilterParam.scalarType;

    // add VP filter to primarySurf : DI
    if (mFilters & OMX_INTEL_ImageFilterDeinterlace) {
        primarySurf.VPfilters |= FILTER_DEINTERLACE;
        primarySurf.iDeinterlaceMode = mFilterParam.deinterlaceType;
    }
    // add VP filter to primarySurf : DN
    if (mFilters & OMX_INTEL_ImageFilterDenoise) {
        primarySurf.VPfilters |= FILTER_DENOISE;
        primarySurf.fDenoiseFactor = mFilterParam.denoiseLevel;
    }
    // add VP filter to primarySurf : Sharpness
    if (mFilters & OMX_INTEL_ImageFilterSharpness) {
        primarySurf.VPfilters |= FILTER_SHARPNESS;
        primarySurf.fSharpnessFactor = mFilterParam.sharpnessLevel;
    }
    // add VP filter to primarySurf : Color balance
    if (mFilters & OMX_INTEL_ImageFilterColorBalance) {
        primarySurf.VPfilters |= FILTER_COLORBALANCE;
        primarySurf.fColorBalanceBrightness = 0;
        primarySurf.fColorBalanceContrast = 1;
        primarySurf.fColorBalanceHue = 0;
        primarySurf.fColorBalanceSaturation = 1;
    }
    // add VP filter to primarySurf: 3p

    if (mFilters & OMX_INTEL_ImageFilter3P) {
        primarySurf.VPfilters |= FILTER_3P;
        primarySurf.st3pInfo.bEnable3P = true;
        switch (videoInfo.videoinfo.eVideoSource) {
            case INTEL_VideoSourceCamera:
                primarySurf.st3pInfo.stStreamType = IVP_STREAM_TYPE_CAMERA;
                break;
            //FIX ME: to add more stream type for video editor, video conf, e.g.
            case INTEL_VideoSourceVideoEditor:
            case INTEL_VideoSourceTranscode:
            case INTEL_VideoSourceVideoConf:
            default:
                primarySurf.st3pInfo.stStreamType = IVP_STREAM_TYPE_NORMAL;
                break;
        }

        primarySurf.st3pInfo.fFrameRate = mFilterParam.frameRate * 1.0;
        primarySurf.st3pInfo.eKernelDumpBitmap.value = 0;
        if (mDebug == 1) {
            char propValueString[PROPERTY_VALUE_MAX];

            property_get("media.3p.reconfigure", propValueString, "0");
            int reconfigure = atoi(propValueString);
            if (reconfigure == 1) {
                m3PReconfig = true;
                property_set("media.3p.reconfigure", "0");
            }
            property_get("media.3p.kernelruntimedump", propValueString, "0");
            int krtDump = atoi(propValueString);
            primarySurf.st3pInfo.eKernelDumpBitmap.value = krtDump;
        }

        if (m3PReconfig) {
            primarySurf.st3pInfo.bReconfig = true;
            m3PReconfig = false;
        }
    }

    iVP_rect_t outSrect;
    outSrect.left  = 0;
    outSrect.top   = 0;
    outSrect.width = mFilterParam.srcWidth;
    outSrect.height  = mFilterParam.srcHeight;

    iVP_rect_t outDrect;
    outDrect.left  = 0;
    outDrect.top   = 0;
    outDrect.width = mFilterParam.srcWidth;
    outDrect.height  = mFilterParam.srcHeight;

    for (i = 0; i < outputCount; i++) {
        outSurf[i].srcRect			= &outSrect;
        outSurf[i].destRect		= &outDrect;
        outSurf[i].bufferType		= IVP_GRALLOC_HANDLE; //TODO: it only support buffer_handle_t now
        outSurf[i].gralloc_handle   = outputBuffer;

        if (flags & (OMX_BUFFERFLAG_TFF))
            primarySurf.sample_type = (i == 0) ? IVP_SAMPLETYPE_TOPFIELD : IVP_SAMPLETYPE_BOTTOMFIELD;
        else if (flags & (OMX_BUFFERFLAG_BFF))
            primarySurf.sample_type = (i == 0) ? IVP_SAMPLETYPE_BOTTOMFIELD : IVP_SAMPLETYPE_TOPFIELD;
        else
            primarySurf.sample_type = IVP_SAMPLETYPE_PROGRESSIVE;

        iVP_exec(&mVPContext, &primarySurf, NULL, 0, &outSurf[i], true);
    }

    ALOGV("process, exit");
    return STATUS_OK;
}

VPPWorker::~VPPWorker() {
    if (mVPStarted) {
        iVP_destroy_context(&mVPContext);
        mVPStarted = false;
    }
#ifdef TARGET_HAS_MULTIPLE_DISPLAY
    if (mMds != NULL) {
        mMds->deInit();
        mMds = NULL;
    }
#endif
    memset(&mFilterParam, 0, sizeof(mFilterParam));
    mVPPWorker = NULL;
    mPort = NULL;
}

status_t VPPWorker::reset() {
    ALOGV("reset");

    if (mVPStarted)
        iVP_destroy_context(&mVPContext);

    iVP_status status = iVP_create_context(&mVPContext, 1200, 800, 1);
    if (status != IVP_STATUS_SUCCESS)
        return STATUS_ERROR;

    memset(&mFilterParam, 0, sizeof(mFilterParam));
    mVPStarted = true;
    return STATUS_OK;
}

void VPPWorker::setDisplayMode(int mode) {
    if (mode & MDS_VPP_CHANGED)
        mVPPSettingUpdated = true;

    //only care HDMI/WIDI status
    mDisplayMode = mode & (MDS_HDMI_CONNECTED | MDS_WIDI_ON);
    if (mPreDisplayMode == mDisplayMode) {
        ALOGV("%s: HDMI/WIDI status no change", __func__);
        return;
    }

    //HDMI
    if ((mDisplayMode & MDS_HDMI_CONNECTED) == 0 &&
            (mPreDisplayMode & MDS_HDMI_CONNECTED) != 0) {
        m3PReconfig = true;
        if (mPort)
            mPort->ReportEvent((OMX_EVENTTYPE)OMX_INTEL_EventIntelResumeVpp);
    } else if ((mDisplayMode & MDS_HDMI_CONNECTED) != 0 &&
            (mPreDisplayMode & MDS_HDMI_CONNECTED) == 0) {
        if (mPort)
            mPort->ReportEvent((OMX_EVENTTYPE)OMX_INTEL_EventIntelSkipVpp);
    }

    //WIDI
    if ((mDisplayMode & MDS_WIDI_ON) == 0 &&
            (mPreDisplayMode & MDS_WIDI_ON) != 0) {
        m3PReconfig = true;
        if (mPort)
            mPort->ReportEvent((OMX_EVENTTYPE)OMX_INTEL_EventIntelResumeVpp);
    } else if ((mDisplayMode & MDS_WIDI_ON) != 0 &&
            (mPreDisplayMode & MDS_WIDI_ON) == 0) {
        if(mPort)
            mPort->ReportEvent((OMX_EVENTTYPE)OMX_INTEL_EventIntelSkipVpp);
    }

    mPreDisplayMode = mDisplayMode;
    return;
}

bool VPPWorker::isVppOn() {
    sp<IServiceManager> sm = defaultServiceManager();
    if (sm == NULL) {
        ALOGE("%s: Failed to get service manager", __func__);
        return false;
    }
    sp<IMDService> mds = interface_cast<IMDService>(
            sm->getService(String16(INTEL_MDS_SERVICE_NAME)));
    if (mds == NULL) {
        ALOGE("%s: Failed to get MDS service", __func__);
        return false;
    }
    sp<IMultiDisplayInfoProvider> mdsInfoProvider = mds->getInfoProvider();
    if (mdsInfoProvider == NULL) {
        ALOGE("%s: Failed to get info provider", __func__);
        return false;
    }

    uint32_t vppStatus = mdsInfoProvider->getVppState();
    mVppOn = (vppStatus & VPP_COMMON_ON) != 0;
    ALOGI("%s: vppon %d", __func__, mVppOn);

    return mVppOn;
}
