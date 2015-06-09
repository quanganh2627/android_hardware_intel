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

//#define LOG_NDEBUG 0
#define LOG_TAG "VPPWorker"

#include "VPPSetting.h"
#include "VPPWorker.h"
#define CHECK_VASTATUS(str) \
    do { \
        if (vaStatus != VA_STATUS_SUCCESS) { \
                ALOGE("%s failed\n", str); \
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

#define QVGA_AREA (320 * 240)
#define VGA_AREA (640 * 480)
#define HD1080P_AREA (1920 * 1080)

namespace android {

VPPWorker::VPPWorker(const sp<ANativeWindow> &nativeWindow)
    :mGraphicBufferNum(0),
        mWidth(0), mHeight(0), mInputFps(FRAME_RATE_0),
        mVAStarted(false), mVAContext(VA_INVALID_ID),
        mDisplay(NULL), mVADisplay(NULL), mVAConfig(VA_INVALID_ID),
        mNumSurfaces(0), mSurfaces(NULL), mVAExtBuf(NULL),
        mNumForwardReferences(3), mForwardReferences(NULL), mPrevInput(0),
        mNumFilterBuffers(0),
        mDeblockOn(false), mDenoiseOn(false), mDeinterlacingOn(false),
        mSharpenOn(false), mColorOn(false),
        mFrcRate(FRC_RATE_1X), mFrcOn(false),
        mUpdatedFrcRate(FRC_RATE_1X), mUpdatedFrcOn(false),
        mInputIndex(0), mOutputIndex(0), mDisplayMode(0),
        mEnableFrc4Hdmi(false), hdmiTimingList(NULL), hdmiListCount(0), mVPPOn(0) {
    memset(&mFilterBuffers, 0, VAProcFilterCount * sizeof(VABufferID));
    memset(&mGraphicBufferConfig, 0, sizeof(GraphicBufferConfig));
    memset(&currHdmiTiming, 0, sizeof(MDSHdmiTiming));
}

//static
VPPWorker* VPPWorker::mVPPWorker = NULL;
//static
sp<ANativeWindow> VPPWorker::mNativeWindow = NULL;

//static
VPPWorker* VPPWorker::getInstance(const sp<ANativeWindow> &nativeWindow) {
    if (mVPPWorker == NULL) {
        mVPPWorker = new VPPWorker(nativeWindow);
        mNativeWindow = nativeWindow;
    } else if (mNativeWindow != nativeWindow)
        return NULL;
    return mVPPWorker;
}

//static
bool VPPWorker::validateNativeWindow(const sp<ANativeWindow> &nativeWindow) {
    if (mNativeWindow == nativeWindow)
        return true;
    else
        return false;
}

status_t VPPWorker::init() {
    status_t ret = STATUS_OK;

    if (!mVAStarted) {
        ret = setupVA();
        if (ret != STATUS_OK)
            return ret;
    }

    if (mNumFilterBuffers == 0) {
        ret = setupFilters();
        if(ret != STATUS_OK)
            return ret;
    }

    return setupPipelineCaps();
}

bool VPPWorker::isSupport() const {
    bool support = false;

    int num_entrypoints = vaMaxNumEntrypoints(mVADisplay);
    VAEntrypoint * entrypoints = (VAEntrypoint *)malloc(num_entrypoints * sizeof(VAEntrypoint));
    if (entrypoints == NULL) {
        ALOGE("failed to malloc entrypoints array\n");
        return false;
    }

    // check if it contains VPP entry point VAEntrypointVideoProc
    VAStatus vaStatus = vaQueryConfigEntrypoints(mVADisplay, VAProfileNone, entrypoints, &num_entrypoints);
    if (vaStatus != VA_STATUS_SUCCESS) {
        ALOGE("vaQueryConfigEntrypoints failed");
        return false;
    }
    for (int i = 0; !support && i < num_entrypoints; i++) {
        support = entrypoints[i] == VAEntrypointVideoProc;
    }
    free(entrypoints);
    entrypoints = NULL;

    return support;
}

status_t VPPWorker::setGraphicBufferConfig(sp<GraphicBuffer> graphicBuffer) {
    if (graphicBuffer == NULL || mGraphicBufferNum >= MAX_GRAPHIC_BUFFER_NUMBER)
        return STATUS_ERROR;

    ANativeWindowBuffer * nativeBuffer = graphicBuffer->getNativeBuffer();
    if (nativeBuffer == NULL)
        return STATUS_ERROR;
    // assign below config data for the 1st time
    if (!mGraphicBufferNum) {
        mGraphicBufferConfig.colorFormat = nativeBuffer->format;
        mGraphicBufferConfig.stride = nativeBuffer->stride;
        mGraphicBufferConfig.width = nativeBuffer->width;
        mGraphicBufferConfig.height = nativeBuffer->height;
    }
    mGraphicBufferConfig.buffer[mGraphicBufferNum++] = (uint32_t)nativeBuffer->handle;
    return STATUS_OK;
}

VASurfaceID VPPWorker::mapBuffer(sp<GraphicBuffer> graphicBuffer) {
    if (graphicBuffer == NULL || mSurfaces == NULL || mVAExtBuf == NULL)
        return VA_INVALID_SURFACE;
    ANativeWindowBuffer * nativeBuffer = graphicBuffer->getNativeBuffer();
    for (uint32_t i = 0; i < mNumSurfaces; i++) {
        if (mGraphicBufferConfig.buffer[i] == (uint32_t)nativeBuffer->handle)
            return mSurfaces[i];
    }
    return VA_INVALID_SURFACE;
}

uint32_t VPPWorker::getOutputBufCount(uint32_t index) {
    uint32_t bufCount = 1;
    if (mFrcOn && index > 0)
            bufCount = mFrcRate - (((mFrcRate == FRC_RATE_2_5X) ? (index & 1): 0));
    return bufCount;
}

uint32_t VPPWorker::getProcBufCount() {
    return getOutputBufCount(mInputIndex);
}

uint32_t VPPWorker::getFillBufCount() {
        return getOutputBufCount(mOutputIndex);
}

status_t VPPWorker::setupVA() {
    ALOGV("setupVA");
    if (mVAStarted)
        return STATUS_OK;

    if (mDisplay != NULL) {
        ALOGE("VA is particially started");
        return STATUS_ERROR;
    }
    mDisplay = new Display;
    *mDisplay = ANDROID_DISPLAY_HANDLE;

    mVADisplay = vaGetDisplay(mDisplay);
    if (mVADisplay == NULL) {
        ALOGE("vaGetDisplay failed");
        return STATUS_ERROR;
    }

    int majorVersion, minorVersion;
    VAStatus vaStatus = vaInitialize(mVADisplay, &majorVersion, &minorVersion);
    CHECK_VASTATUS("vaInitialize");

    // Check if VPP entry point is supported
    if (!isSupport()) {
        ALOGE("VPP is not supported on current platform");
        return STATUS_NOT_SUPPORT;
    }

    // Find out the format for the target
    VAConfigAttrib attrib;
    attrib.type = VAConfigAttribRTFormat;
    vaStatus = vaGetConfigAttributes(mVADisplay, VAProfileNone, VAEntrypointVideoProc, &attrib, 1);
    CHECK_VASTATUS("vaGetConfigAttributes");

    if ((attrib.value & VA_RT_FORMAT_YUV420) == 0) {
        ALOGE("attribute is %x vs wanted %x", attrib.value, VA_RT_FORMAT_YUV420);
        return STATUS_NOT_SUPPORT;
    }

    ALOGV("ready to create config");
    // Create the configuration
    vaStatus = vaCreateConfig(mVADisplay, VAProfileNone, VAEntrypointVideoProc, &attrib, 1, &mVAConfig);
    CHECK_VASTATUS("vaCreateConfig");

    // Create VASurfaces
    mNumSurfaces = mGraphicBufferNum;
    mSurfaces = new VASurfaceID[mNumSurfaces];
    if (mSurfaces == NULL) {
        return STATUS_ALLOCATION_ERROR;
    }

    mVAExtBuf = new VASurfaceAttribExternalBuffers;
    if(mVAExtBuf == NULL) {
        return STATUS_ALLOCATION_ERROR;
    }
    VASurfaceAttrib attribs[3];
    int supportedMemType = 0;

    //check whether it support create surface from external buffer
    unsigned int num = 0;
    VASurfaceAttrib* outAttribs = NULL;
    //get attribs number
    vaStatus = vaQuerySurfaceAttributes(mVADisplay, mVAConfig, NULL, &num);
    CHECK_VASTATUS("vaQuerySurfaceAttributes");
    if (num == 0)
        return STATUS_NOT_SUPPORT;

    //get attributes
    outAttribs = new VASurfaceAttrib[num];
    if (outAttribs == NULL) {
        return STATUS_ALLOCATION_ERROR;
    }
    vaStatus = vaQuerySurfaceAttributes(mVADisplay, mVAConfig, outAttribs, &num);
    if (vaStatus != VA_STATUS_SUCCESS) {
        ALOGE("vaQuerySurfaceAttributs fail!");
        delete []outAttribs;
        return STATUS_ERROR;
    }

    for(int i = 0; i < num; i ++) {
        if (outAttribs[i].type == VASurfaceAttribMemoryType) {
            supportedMemType = outAttribs[i].value.value.i;
            break;
        }
    }
    delete []outAttribs;

    if (supportedMemType & VA_SURFACE_ATTRIB_MEM_TYPE_ANDROID_GRALLOC == 0)
        return VA_INVALID_SURFACE;

    mVAExtBuf->pixel_format = VA_FOURCC_NV12;
    mVAExtBuf->width = mGraphicBufferConfig.width;
    mVAExtBuf->height = mGraphicBufferConfig.height;
    mVAExtBuf->data_size = mGraphicBufferConfig.stride * mGraphicBufferConfig.height * 1.5;
    mVAExtBuf->num_buffers = mNumSurfaces;
    mVAExtBuf->num_planes = 3;
    mVAExtBuf->pitches[0] = mGraphicBufferConfig.stride;
    mVAExtBuf->pitches[1] = mGraphicBufferConfig.stride;
    mVAExtBuf->pitches[2] = mGraphicBufferConfig.stride;
    mVAExtBuf->pitches[3] = 0;
    mVAExtBuf->offsets[0] = 0;
    mVAExtBuf->offsets[1] = mGraphicBufferConfig.stride * mGraphicBufferConfig.height;
    mVAExtBuf->offsets[2] = mVAExtBuf->offsets[1];
    mVAExtBuf->offsets[3] = 0;
    mVAExtBuf->flags = VA_SURFACE_ATTRIB_MEM_TYPE_ANDROID_GRALLOC;
    if (mGraphicBufferConfig.colorFormat == OMX_INTEL_COLOR_FormatYUV420PackedSemiPlanar_Tiled) {
        ALOGV("set TILING flag");
        mVAExtBuf->flags |= VA_SURFACE_EXTBUF_DESC_ENABLE_TILING;
    }
    mVAExtBuf->private_data = mNativeWindow.get(); //pass nativeWindow through private_data

    mVAExtBuf->buffers= (long unsigned int *)malloc(sizeof(long unsigned int)*mNumSurfaces);
    if (mVAExtBuf->buffers == NULL) {
        return STATUS_ALLOCATION_ERROR;
    }
    for (uint32_t i = 0; i < mNumSurfaces; i++) {
        mVAExtBuf->buffers[i] = (uint32_t)mGraphicBufferConfig.buffer[i];
    }

    attribs[0].type = (VASurfaceAttribType)VASurfaceAttribMemoryType;
    attribs[0].flags = VA_SURFACE_ATTRIB_SETTABLE;
    attribs[0].value.type = VAGenericValueTypeInteger;
    attribs[0].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_ANDROID_GRALLOC;

    attribs[1].type = (VASurfaceAttribType)VASurfaceAttribExternalBufferDescriptor;
    attribs[1].flags = VA_SURFACE_ATTRIB_SETTABLE;
    attribs[1].value.type = VAGenericValueTypePointer;
    attribs[1].value.value.p = (void *)mVAExtBuf;

    attribs[2].type = (VASurfaceAttribType)VASurfaceAttribUsageHint;
    attribs[2].flags = VA_SURFACE_ATTRIB_SETTABLE;
    attribs[2].value.type = VAGenericValueTypeInteger;
    attribs[2].value.value.i = VA_SURFACE_ATTRIB_USAGE_HINT_VPP_READ;

    int width, height;
#ifdef TARGET_VPP_USE_GEN
    width = mWidth;
    height = mHeight;
#else
    width = mVAExtBuf->width;
    height = mVAExtBuf->height;
#endif

    vaStatus = vaCreateSurfaces(mVADisplay, VA_RT_FORMAT_YUV420, width,
                                 height, mSurfaces, mNumSurfaces, attribs, 3);
    CHECK_VASTATUS("vaCreateSurfaces");

    // Create Context
    ALOGV("ready to create context");
    vaStatus = vaCreateContext(mVADisplay, mVAConfig, mWidth, mHeight, 0, mSurfaces, mNumSurfaces, &mVAContext);
    CHECK_VASTATUS("vaCreateContext");

    mVAStarted = true;
    ALOGV("VA has been successfully started");
    return STATUS_OK;
}

status_t VPPWorker::terminateVA() {
    if (mVAExtBuf) {
        if (mVAExtBuf->buffers) {
            free(mVAExtBuf->buffers);
            mVAExtBuf->buffers = NULL;
        }
        delete mVAExtBuf;
        mVAExtBuf = NULL;
    }

    if (mSurfaces) {
        vaDestroySurfaces(mVADisplay, mSurfaces, mNumSurfaces);
        delete [] mSurfaces;
        mSurfaces = NULL;
    }

    for (int i = 0; i < mNumFilterBuffers; i++) {
        vaDestroyBuffer(mVADisplay, mFilterBuffers[i]);
    }

    if (mVAContext != VA_INVALID_ID) {
         vaDestroyContext(mVADisplay, mVAContext);
         mVAContext = VA_INVALID_ID;
    }

    if (mVAConfig != VA_INVALID_ID) {
        vaDestroyConfig(mVADisplay, mVAConfig);
        mVAConfig = VA_INVALID_ID;
    }

    if (mVADisplay) {
        vaTerminate(mVADisplay);
        mVADisplay = NULL;
    }

    if (mDisplay) {
        delete mDisplay;
        mDisplay = NULL;
    }

    mVAStarted = false;
    return STATUS_OK;
}

status_t VPPWorker::configFilters(const uint32_t width, const uint32_t height, const uint32_t fps, const uint32_t slowMotionFactor, const uint32_t flags) {
    mWidth = width;
    mHeight = height;
    mInputFps = fps;
    uint32_t area = mWidth * mHeight;

    //initialize vpp status here
    isVppOn();

    ALOGE("mVPPOn = %d, VPP_COMMON_ON = %d, VPP_FRC_ON = %d", mVPPOn, VPP_COMMON_ON, VPP_FRC_ON);
#ifdef TARGET_VPP_USE_GEN
    if (mVPPOn & VPP_COMMON_ON) {
        ALOGV("vpp is on in settings");
        if (area <= VGA_AREA) {
            mDenoiseOn = true;
        }
        //mColorOn = true;
        mDeinterlacingOn = true;

        return STATUS_OK;
    } else {
        ALOGW("all the filters are off, do not do VPP");
        return STATUS_NOT_SUPPORT;
    }
#endif
    // limit resolution that VPP supported for **Merifield/Moorefield**: <QCIF or >1080P
    if ((mHeight < 144) || (mHeight > 1080) || (area > HD1080P_AREA)) {
        ALOGW("unspported resolution %d x %d, limit (176x144 - 1920x1080)", mWidth, mHeight);
        return STATUS_NOT_SUPPORT;
    }

    if (mVPPOn & VPP_COMMON_ON) {
        ALOGV("vpp is on in settings");

        // QCIF to QVGA
        if (area <= QVGA_AREA) {
            mDeblockOn = true;
            mSharpenOn = true;
            mColorOn = true;
        }
        // QVGA to VGA
        else if (area <= VGA_AREA) {
            mDenoiseOn = true;
            mSharpenOn = true;
            mColorOn = true;
        }
        // VGA to 1080P
        else if (area <= HD1080P_AREA) {
            mSharpenOn = true;
        }
    }

    if (slowMotionFactor == 2 || slowMotionFactor == 4) {
        // slow motion mode, only do FRC according to slow motion factor
        mFrcOn = true;
        mInputFps = fps / slowMotionFactor;

        if (fps == FRAME_RATE_24) {
            mFrcRate = FRC_RATE_2_5X;
        } else if (fps == FRAME_RATE_30 || fps == FRAME_RATE_60) {
            mFrcRate = FRC_RATE_2X;
        }

    } else if (mVPPOn & VPP_FRC_ON) {
        ALOGV("FRC is on in Settings");
        calcFrcByInputFps(&mFrcOn, &mFrcRate);
        ALOGV("FRC enable %d, FrcRate %d", mFrcOn, mFrcRate);
    }

    // enable sharpen always while FRC is on
    if (mFrcOn)
        mSharpenOn = true;

    ALOGI("width=%d, height=%d, fps=%d, slowmotion=%d, \
            mDeblockOn=%d, mDenoiseOn=%d, mSharpenOn=%d, mColorOn=%d, mFrcOn=%d, mFrcRate=%d",
          mWidth, mHeight, fps, slowMotionFactor, mDeblockOn, mDenoiseOn, mSharpenOn, mColorOn, mFrcOn, mFrcRate);

    if (!mDeblockOn && !mDenoiseOn && !mSharpenOn && !mColorOn && !mFrcOn) {
        ALOGW("all the filters are off, do not do VPP, either FRC");
        return STATUS_NOT_SUPPORT;
    }
    return STATUS_OK;
}

status_t VPPWorker::calcFrcByInputFps(bool *FrcOn, FRC_RATE *FrcRate) {
    if (!FrcRate || !FrcOn) {
        return STATUS_ERROR;
    }

    if ((mInputFps == FRAME_RATE_15) || (mInputFps == FRAME_RATE_25) || (mInputFps == FRAME_RATE_30)) {
        *FrcOn = true;
        *FrcRate = FRC_RATE_2X;
    } else if (mInputFps == FRAME_RATE_24) {
        *FrcOn = true;
        *FrcRate = FRC_RATE_2_5X;
    } else {
        *FrcOn = false;
        *FrcRate = FRC_RATE_1X;
        ALOGI("Unsupported input frame rate %d. VPP FRC is OFF.", mInputFps);
    }

    return STATUS_OK;
}

status_t VPPWorker::calcFrcByMatchHdmiCap(bool *FrcOn, FRC_RATE *FrcRate) {
    int32_t fpsSet[HDMI_TIMING_MAX];
    int32_t fpsCnt = 0;

    if (!FrcRate || !FrcOn || !hdmiTimingList || (hdmiListCount <= 0)) {
        return STATUS_ERROR;
    }
    if ((currHdmiTiming.width <= 0) || (currHdmiTiming.height <=0)) {
        ALOGW("Current HDMI time set is invalid wxh: %d x %d ", currHdmiTiming.width, currHdmiTiming.height);
        return STATUS_ERROR;
    }

    const int32_t width = currHdmiTiming.width;
    const int32_t height = currHdmiTiming.height;
    ALOGI("current HDMI setting %d x %d ", currHdmiTiming.width, currHdmiTiming.height);
    ALOGI("hdmi curretn setting supported fps (in current resolution)");
    //get hdmi supported FPS by of current resolution
    for (int32_t i = 0; i < hdmiListCount; i++) {
        if((width == hdmiTimingList[i].width) && (height == hdmiTimingList[i].height)) {
              fpsSet[fpsCnt] = hdmiTimingList[i].refresh;
              ALOGI("%d", fpsSet[fpsCnt]);
              fpsCnt++;
         }
    }

    *FrcRate = FRC_RATE_1X;
    *FrcOn = false;
    switch (mInputFps) {
        case FRAME_RATE_15:
        case FRAME_RATE_25:
        case FRAME_RATE_30:
            if (isFpsSupport(mInputFps * 2, fpsSet, fpsCnt)) {
                *FrcOn = true;
                *FrcRate = FRC_RATE_2X;
            }
            break;
        case FRAME_RATE_24:
            if(isFpsSupport(FRAME_RATE_60, fpsSet, fpsCnt)) {
                *FrcOn = true;
                *FrcRate = FRC_RATE_2_5X;
            }
            break;
        defalt:
            ALOGI("VPP FRC output cannot match HDMI capability. VPP FRC is OFF.Input %d", mInputFps);
            break;
    }

    ALOGI("VPP FRC for HDMI frcOn %d, rate %d ", *FrcOn, *FrcRate);

    return STATUS_OK;
}

bool VPPWorker::isFpsSupport(int32_t fps, int32_t *fpsSet, int32_t fpsSetCnt) {
    bool ret = false;
    for (int32_t i = 0; i < fpsSetCnt; i++) {
        if (fps == fpsSet[i]) {
            ret = true;
            break;
        }
    }

    return ret;
}

status_t VPPWorker::setupFilters() {
    ALOGV("setupFilters");
    VAProcFilterParameterBuffer deblock, denoise, sharpen;
    VAProcFilterParameterBufferDeinterlacing deint;
    VAProcFilterParameterBufferColorBalance color[COLOR_NUM];
    VAProcFilterParameterBufferFrameRateConversion frc;
    VABufferID deblockId, denoiseId, deintId, sharpenId, colorId, frcId;
    uint32_t numCaps;
    VAProcFilterCap deblockCaps, denoiseCaps, sharpenCaps, frcCaps;
    VAProcFilterCapDeinterlacing deinterlacingCaps[VAProcDeinterlacingCount];
    VAProcFilterCapColorBalance colorCaps[COLOR_NUM];
    VAStatus vaStatus;
    uint32_t numSupportedFilters = VAProcFilterCount;
    VAProcFilterType supportedFilters[VAProcFilterCount];

    // query supported filters
    vaStatus = vaQueryVideoProcFilters(mVADisplay, mVAContext, supportedFilters, &numSupportedFilters);
    CHECK_VASTATUS("vaQueryVideoProcFilters");

    // create filter buffer for each filter
    for (uint32_t i = 0; i < numSupportedFilters; i++) {
        switch (supportedFilters[i]) {
            case VAProcFilterDeblocking:
                if (mDeblockOn) {
                    // check filter caps
                    numCaps = 1;
                    vaStatus = vaQueryVideoProcFilterCaps(mVADisplay, mVAContext,
                            VAProcFilterDeblocking,
                            &deblockCaps,
                            &numCaps);
                    CHECK_VASTATUS("vaQueryVideoProcFilterCaps for deblocking");
                    // create parameter buffer
                    deblock.type = VAProcFilterDeblocking;
                    deblock.value = deblockCaps.range.min_value + DENOISE_DEBLOCK_STRENGTH * deblockCaps.range.step;
                    vaStatus = vaCreateBuffer(mVADisplay, mVAContext,
                        VAProcFilterParameterBufferType, sizeof(deblock), 1,
                        &deblock, &deblockId);
                    CHECK_VASTATUS("vaCreateBuffer for deblocking");
                    mFilterBuffers[mNumFilterBuffers] = deblockId;
                    mNumFilterBuffers++;
                }
                break;
            case VAProcFilterNoiseReduction:
                if(mDenoiseOn) {
                    // check filter caps
                    numCaps = 1;
                    vaStatus = vaQueryVideoProcFilterCaps(mVADisplay, mVAContext,
                            VAProcFilterNoiseReduction,
                            &denoiseCaps,
                            &numCaps);
                    CHECK_VASTATUS("vaQueryVideoProcFilterCaps for denoising");
                    // create parameter buffer
                    denoise.type = VAProcFilterNoiseReduction;
#ifdef TARGET_VPP_USE_GEN
                    char propValueString[PROPERTY_VALUE_MAX];

                    // placeholder for vpg driver: can't support denoise factor auto adjust, so leave config to user.
                    property_get("vpp.filter.denoise.factor", propValueString, "64.0");
                    denoise.value = atof(propValueString);
                    denoise.value = (denoise.value < 0.0f) ? 0.0f : denoise.value;
                    denoise.value = (denoise.value > 64.0f) ? 64.0f : denoise.value;
#else
                    denoise.value = denoiseCaps.range.min_value + DENOISE_DEBLOCK_STRENGTH * denoiseCaps.range.step;
#endif
                    vaStatus = vaCreateBuffer(mVADisplay, mVAContext,
                        VAProcFilterParameterBufferType, sizeof(denoise), 1,
                        &denoise, &denoiseId);
                    CHECK_VASTATUS("vaCreateBuffer for denoising");
                    mFilterBuffers[mNumFilterBuffers] = denoiseId;
                    mNumFilterBuffers++;
                }
                break;
            case VAProcFilterDeinterlacing:
                if (mDeinterlacingOn) {
                    numCaps = VAProcDeinterlacingCount;
                    vaStatus = vaQueryVideoProcFilterCaps(mVADisplay, mVAContext,
                            VAProcFilterDeinterlacing,
                            &deinterlacingCaps[0],
                            &numCaps);
                    CHECK_VASTATUS("vaQueryVideoProcFilterCaps for deinterlacing");
                    for (int i = 0; i < numCaps; i++)
                    {
                        VAProcFilterCapDeinterlacing * const cap = &deinterlacingCaps[i];
                        if (cap->type != VAProcDeinterlacingBob) // desired Deinterlacing Type
                            continue;

                        deint.type = VAProcFilterDeinterlacing;
                        deint.algorithm = VAProcDeinterlacingBob;
                        vaStatus = vaCreateBuffer(mVADisplay,
                                mVAContext,
                                VAProcFilterParameterBufferType,
                                sizeof(deint), 1,
                                &deint, &deintId);
                        CHECK_VASTATUS("vaCreateBuffer for deinterlacing");
                        mFilterBuffers[mNumFilterBuffers] = deintId;
                        mNumFilterBuffers++;
                    }
                }
                break;
            case VAProcFilterSharpening:
                if(mSharpenOn) {
                    // check filter caps
                    numCaps = 1;
                    vaStatus = vaQueryVideoProcFilterCaps(mVADisplay, mVAContext,
                            VAProcFilterSharpening,
                            &sharpenCaps,
                            &numCaps);
                    CHECK_VASTATUS("vaQueryVideoProcFilterCaps for sharpening");
                    // create parameter buffer
                    sharpen.type = VAProcFilterSharpening;
                    sharpen.value = sharpenCaps.range.default_value;
                    vaStatus = vaCreateBuffer(mVADisplay, mVAContext,
                        VAProcFilterParameterBufferType, sizeof(sharpen), 1,
                        &sharpen, &sharpenId);
                    CHECK_VASTATUS("vaCreateBuffer for sharpening");
                    mFilterBuffers[mNumFilterBuffers] = sharpenId;
                    mNumFilterBuffers++;
                }
                break;
            case VAProcFilterColorBalance:
                if(mColorOn) {
                    uint32_t featureCount = 0;
                    // check filter caps
                    // FIXME: it's not used at all!
                    numCaps = COLOR_NUM;
                    vaStatus = vaQueryVideoProcFilterCaps(mVADisplay, mVAContext,
                            VAProcFilterColorBalance,
                            colorCaps,
                            &numCaps);
                    CHECK_VASTATUS("vaQueryVideoProcFilterCaps for color balance");
                    // create parameter buffer
                    for (uint32_t i = 0; i < numCaps; i++) {
                        if (colorCaps[i].type == VAProcColorBalanceAutoSaturation) {
                            color[i].type = VAProcFilterColorBalance;
                            color[i].attrib = VAProcColorBalanceAutoSaturation;
                            color[i].value = colorCaps[i].range.min_value + COLOR_STRENGTH * colorCaps[i].range.step;
                            featureCount++;
                        }
                        else if (colorCaps[i].type == VAProcColorBalanceAutoBrightness) {
                            color[i].type = VAProcFilterColorBalance;
                            color[i].attrib = VAProcColorBalanceAutoBrightness;
                            color[i].value = colorCaps[i].range.min_value + COLOR_STRENGTH * colorCaps[i].range.step;
                            featureCount++;
                        }
                    }
#ifdef TARGET_VPP_USE_GEN
                    //TODO: VPG need to support check input value by colorCaps.
                    enum {kHue = 0, kSaturation, kBrightness, kContrast};
                    char propValueString[PROPERTY_VALUE_MAX];
                    color[kHue].type = VAProcFilterColorBalance;
                    color[kHue].attrib = VAProcColorBalanceHue;

                    // placeholder for vpg driver: can't support auto color balance, so leave config to user.
                    property_get("vpp.filter.procamp.hue", propValueString, "0.0");
                    color[kHue].value = atof(propValueString);
                    color[kHue].value = (color[kHue].value < -180.0f) ? -180.0f : color[kHue].value;
                    color[kHue].value = (color[kHue].value > 180.0f) ? 180.0f : color[kHue].value;
                    featureCount++;

                    color[kSaturation].type   = VAProcFilterColorBalance;
                    color[kSaturation].attrib = VAProcColorBalanceSaturation;
                    property_get("vpp.filter.procamp.saturation", propValueString, "1.0");
                    color[kSaturation].value = atof(propValueString);
                    color[kSaturation].value = (color[kSaturation].value < 0.0f) ? 0.0f : color[kSaturation].value;
                    color[kSaturation].value = (color[kSaturation].value > 10.0f) ? 10.0f : color[kSaturation].value;
                    featureCount++;

                    color[kBrightness].type   = VAProcFilterColorBalance;
                    color[kBrightness].attrib = VAProcColorBalanceBrightness;
                    property_get("vpp.filter.procamp.brightness", propValueString, "0.0");
                    color[kBrightness].value = atof(propValueString);
                    color[kBrightness].value = (color[kBrightness].value < -100.0f) ? -100.0f : color[kBrightness].value;
                    color[kBrightness].value = (color[kBrightness].value > 100.0f) ? 100.0f : color[kBrightness].value;
                    featureCount++;

                    color[kContrast].type   = VAProcFilterColorBalance;
                    color[kContrast].attrib = VAProcColorBalanceContrast;
                    property_get("vpp.filter.procamp.contrast", propValueString, "1.0");
                    color[kContrast].value = atof(propValueString);
                    color[kContrast].value = (color[kContrast].value < 0.0f) ? 0.0f : color[kContrast].value;
                    color[kContrast].value = (color[kContrast].value > 10.0f) ? 10.0f : color[kContrast].value;
                    featureCount++;
#endif
                    vaStatus = vaCreateBuffer(mVADisplay, mVAContext,
                        VAProcFilterParameterBufferType, sizeof(*color), featureCount,
                        color, &colorId);
                    CHECK_VASTATUS("vaCreateBuffer for color balance");
                    mFilterBuffers[mNumFilterBuffers] = colorId;
                    mNumFilterBuffers++;
                }
                break;
            case VAProcFilterFrameRateConversion:
                if(mFrcOn) {
                    frc.type = VAProcFilterFrameRateConversion;
                    frc.input_fps = mInputFps;
                    switch (mFrcRate){
                        case FRC_RATE_1X:
                            frc.output_fps = frc.input_fps;
                            break;
                        case FRC_RATE_2X:
                            frc.output_fps = frc.input_fps * 2;
                            break;
                        case FRC_RATE_2_5X:
                            frc.output_fps = frc.input_fps * 5/2;
                            break;
                        case FRC_RATE_4X:
                            frc.output_fps = frc.input_fps * 4;
                            break;
                    }
                    vaStatus = vaCreateBuffer(mVADisplay, mVAContext,
                        VAProcFilterParameterBufferType, sizeof(frc), 1,
                        &frc, &frcId);
                    CHECK_VASTATUS("vaCreateBuffer for frc");
                    mFilterBuffers[mNumFilterBuffers] = frcId;
                    mNumFilterBuffers++;
                    mFilterFrc = frcId;
                }
                break;
            default:
                ALOGE("Not supported filter\n");
                break;
        }
    }
    return STATUS_OK;
}

status_t VPPWorker::setupPipelineCaps() {
    ALOGV("setupPipelineCaps");
    //TODO color standards
    VAProcPipelineCaps pipelineCaps;
    VAStatus vaStatus;
    pipelineCaps.input_color_standards = in_color_standards;
    pipelineCaps.num_input_color_standards = VAProcColorStandardCount;
    pipelineCaps.output_color_standards = out_color_standards;
    pipelineCaps.num_output_color_standards = VAProcColorStandardCount;

    vaStatus = vaQueryVideoProcPipelineCaps(mVADisplay, mVAContext,
        mFilterBuffers, mNumFilterBuffers,
        &pipelineCaps);
    CHECK_VASTATUS("vaQueryVideoProcPipelineCaps");

    mNumForwardReferences = pipelineCaps.num_forward_references;
    if (mNumForwardReferences > 0) {
        mForwardReferences = (VASurfaceID*)malloc(mNumForwardReferences * sizeof(VASurfaceID));
        if (mForwardReferences == NULL)
            return STATUS_ALLOCATION_ERROR;
        memset(mForwardReferences, 0, mNumForwardReferences * sizeof(VASurfaceID));
    }
    return STATUS_OK;
}

status_t VPPWorker::process(sp<GraphicBuffer> inputGraphicBuffer,
                             Vector< sp<GraphicBuffer> > outputGraphicBuffer,
                             uint32_t outputCount, bool isEOS, uint32_t flags) {
    ALOGV("process: outputCount=%d, mInputIndex=%d", outputCount, mInputIndex);
    VASurfaceID input;
    VASurfaceID output[MAX_FRC_OUTPUT];
    VABufferID pipelineId;
    VAProcPipelineParameterBuffer *pipeline;
    VAProcFilterParameterBufferFrameRateConversion *frc;
    VAStatus vaStatus;
    uint32_t i;

    if (outputCount < 1) {
       ALOGE("invalid outputCount");
       return STATUS_ERROR;
    }
    // map GraphicBuffer to VASurface
    input = mapBuffer(inputGraphicBuffer);
    if (input == VA_INVALID_SURFACE && !isEOS) {
        ALOGE("invalid input buffer");
        return STATUS_ERROR;
    }
    for (i = 0; i < outputCount; i++) {
        output[i] = mapBuffer(outputGraphicBuffer[i]);
        if (output[i] == VA_INVALID_SURFACE) {
            ALOGE("invalid output buffer");
            return STATUS_ERROR;
        }
    }

    // reference frames setting
    if (mNumForwardReferences > 0) {
        /* add previous frame into reference array*/
        for (i = 1; i < mNumForwardReferences; i++) {
            mForwardReferences[i - 1] = mForwardReferences[i];
        }

        //make last reference to input
        mForwardReferences[mNumForwardReferences - 1] = mPrevInput;
    }

    mPrevInput = input;
    // create pipeline parameter buffer
    vaStatus = vaCreateBuffer(mVADisplay,
            mVAContext,
            VAProcPipelineParameterBufferType,
            sizeof(*pipeline),
            1,
            NULL,
            &pipelineId);
    CHECK_VASTATUS("vaCreateBuffer for VAProcPipelineParameterBufferType");

    ALOGV("before vaBeginPicture");
    vaStatus = vaBeginPicture(mVADisplay, mVAContext, output[0]);
    CHECK_VASTATUS("vaBeginPicture");

    // map pipeline paramter buffer
    vaStatus = vaMapBuffer(mVADisplay, pipelineId, (void**)&pipeline);
    CHECK_VASTATUS("vaMapBuffer for pipeline parameter buffer");

    // frc pamameter setting
    if (mFrcOn) {
        vaStatus = vaMapBuffer(mVADisplay, mFilterFrc, (void **)&frc);
        CHECK_VASTATUS("vaMapBuffer for frc parameter buffer");
        if (isEOS)
            frc->num_output_frames = 0;
        else
            frc->num_output_frames = outputCount - 1;
        frc->output_frames = output + 1;
    }

    // pipeline parameter setting
    VARectangle dst_region;
    dst_region.x = 0;
    dst_region.y = 0;
    dst_region.width = mWidth;
    dst_region.height = mHeight;

    VARectangle src_region;
    src_region.x = 0;
    src_region.y = 0;
    src_region.width = mWidth;
    src_region.height = mHeight;

    if (isEOS) {
        pipeline->surface = 0;
        pipeline->pipeline_flags = VA_PIPELINE_FLAG_END;
    }
    else {
        pipeline->surface = input;
        pipeline->pipeline_flags = 0;
    }
#ifdef TARGET_VPP_USE_GEN
    pipeline->surface_region = &src_region;
    pipeline->output_region = &dst_region;
    pipeline->surface_color_standard = VAProcColorStandardBT601;
    pipeline->output_color_standard = VAProcColorStandardBT601;
#else
    pipeline->surface_region = NULL;
    pipeline->output_region = NULL;//&output_region;
    pipeline->surface_color_standard = VAProcColorStandardNone;
    pipeline->output_color_standard = VAProcColorStandardNone;
    /* real rotate state will be decided in psb video */
    pipeline->rotation_state = 0;
#endif
    /* FIXME: set more meaningful background color */
    pipeline->output_background_color = 0;
    pipeline->filters = mFilterBuffers;
    pipeline->num_filters = mNumFilterBuffers;
    pipeline->forward_references = mForwardReferences;
    pipeline->num_forward_references = mNumForwardReferences;
    pipeline->backward_references = NULL;
    pipeline->num_backward_references = 0;

    //currently, we only transfer TOP field to frame, no frame rate change.
    if (flags & (OMX_BUFFERFLAG_TFF | OMX_BUFFERFLAG_BFF)) {
        pipeline->filter_flags = VA_TOP_FIELD;
    } else {
        pipeline->filter_flags = VA_FRAME_PICTURE;
    }

    if (mFrcOn) {
        vaStatus = vaUnmapBuffer(mVADisplay, mFilterFrc);
        CHECK_VASTATUS("vaUnmapBuffer for frc parameter buffer");
    }

    vaStatus = vaUnmapBuffer(mVADisplay, pipelineId);
    CHECK_VASTATUS("vaUnmapBuffer for pipeline parameter buffer");

    ALOGV("before vaRenderPicture");
    // Send parameter to driver
    vaStatus = vaRenderPicture(mVADisplay, mVAContext, &pipelineId, 1);
    CHECK_VASTATUS("vaRenderPicture");
    ALOGV("before vaEndPicture");
    vaStatus = vaEndPicture(mVADisplay, mVAContext);
    CHECK_VASTATUS("vaEndPicture");

    mInputIndex++;
    ALOGV("process, exit");
    return STATUS_OK;
}

status_t VPPWorker::fill(Vector< sp<GraphicBuffer> > outputGraphicBuffer, uint32_t outputCount) {
    ALOGV("fill, outputCount=%d, mOutputIndex=%d",outputCount, mOutputIndex);
    // get output surface
    VASurfaceID output[MAX_FRC_OUTPUT];
    VAStatus vaStatus;
    VASurfaceStatus surStatus;

    if (outputCount < 1)
        return STATUS_ERROR;
    // map GraphicBuffer to VASurface
    for (uint32_t i = 0; i < outputCount; i++) {

        output[i] = mapBuffer(outputGraphicBuffer[i]);
        if (output[i] == VA_INVALID_SURFACE) {
            ALOGE("invalid output buffer");
            return STATUS_ERROR;
        }

        vaStatus = vaQuerySurfaceStatus(mVADisplay, output[i],&surStatus);
        CHECK_VASTATUS("vaQuerySurfaceStatus");
        if (surStatus == VASurfaceRendering) {
            ALOGV("Rendering %d", i);
            /* The behavior of driver is: all output of one process task are return in one interruption.
               The whole outputs of one FRC task are all ready or none of them is ready.
               If the behavior changed, it hurts the performance.
            */
            if (0 != i) {
                ALOGW("*****Driver behavior changed. The performance is hurt.");
                ALOGW("Please check driver behavior: all output of one task return in one interruption.");
            }
            vaStatus = STATUS_DATA_RENDERING;
            break;
        }
        if ((surStatus != VASurfaceRendering) && (surStatus != VASurfaceReady)) {
            ALOGE("surface statu Error %d", surStatus);
            vaStatus = STATUS_ERROR;
        }

        vaStatus = vaSyncSurface(mVADisplay, output[i]);
        CHECK_VASTATUS("vaSyncSurface");
        vaStatus = STATUS_OK;
        //dumpYUVFrameData(output[i]);
    }

    if (vaStatus == STATUS_OK)
        mOutputIndex++;

    ALOGV("fill, exit");
    return vaStatus;
}

VPPWorker::~VPPWorker() {
    if (mForwardReferences != NULL) {
        free(mForwardReferences);
        mForwardReferences = NULL;
    }

    if (hdmiTimingList != NULL) {
        delete [] hdmiTimingList;
        hdmiTimingList = NULL;
    }

    if (mVAStarted) {
        terminateVA();
    }
    mVPPWorker = NULL;
    mNativeWindow.clear();
}

// Debug only
#define FRAME_OUTPUT_FILE_NV12 "/storage/sdcard0/vpp_output.nv12"
status_t VPPWorker::dumpYUVFrameData(VASurfaceID surfaceID) {
    status_t ret;
    if (surfaceID == VA_INVALID_SURFACE)
        return STATUS_ERROR;

    VAStatus vaStatus;
    VAImage image;
    unsigned char *data_ptr;

    vaStatus = vaDeriveImage(mVADisplay,
            surfaceID,
            &image);
    CHECK_VASTATUS("vaDeriveImage");

    vaStatus = vaMapBuffer(mVADisplay, image.buf, (void **)&data_ptr);
    CHECK_VASTATUS("vaMapBuffer");

    ret = writeNV12(mWidth, mHeight, data_ptr, image.pitches[0], image.pitches[1]);
    if (ret != STATUS_OK) {
        AALOGV("writeNV12 error");
        return STATUS_ERROR;
    }

    vaStatus = vaUnmapBuffer(mVADisplay, image.buf);
    CHECK_VASTATUS("vaUnMapBuffer");

    vaStatus = vaDestroyImage(mVADisplay,image.image_id);
    CHECK_VASTATUS("vaDestroyImage");

    return STATUS_OK;
}

status_t VPPWorker::reset() {
    status_t ret;
    ALOGD("reset");
    mInputIndex = 0;
    mOutputIndex = 0;
    mNumFilterBuffers = 0;
    if (mForwardReferences != NULL) {
        free(mForwardReferences);
        mForwardReferences = NULL;
    }
    if (mVAContext != VA_INVALID_ID) {
         vaDestroyContext(mVADisplay, mVAContext);
         mVAContext = VA_INVALID_ID;
    }
    VAStatus vaStatus = vaCreateContext(mVADisplay, mVAConfig, mWidth, mHeight, 0, mSurfaces, mNumSurfaces, &mVAContext);
    CHECK_VASTATUS("vaCreateContext");
    if (mNumFilterBuffers == 0) {
        ret = setupFilters();
        if(ret != STATUS_OK)
            return ret;
    }
    return setupPipelineCaps();
}

uint32_t VPPWorker::getVppOutputFps() {
    uint32_t outputFps;
    //mFrcRate is 1 if FRC is disabled or input FPS is not changed by VPP.
    if (FRC_RATE_2_5X == mFrcRate) {
        outputFps = mInputFps * 5 / 2;
    } else {
        outputFps = mInputFps * mFrcRate;
    }

    ALOGI("vpp is on in settings %d %d %d", outputFps,  mInputFps, mFrcRate);
    return outputFps;
}

//set display mode
void VPPWorker::setDisplayMode (int32_t mode) {
    mDisplayMode = mode & MDS_HDMI_CONNECTED;
}

int32_t VPPWorker::getDisplayMode () {
    return mDisplayMode;
}

bool VPPWorker::isHdmiConnected() {
    return (mDisplayMode & MDS_HDMI_CONNECTED);
}

status_t VPPWorker::getHdmiData() {
    if (!(isHdmiConnected())) {
        ALOGW("HDMI is NOT connected. Cannot get HDMI data");
        return STATUS_ERROR;
    }
    if ((mMds == NULL) || (hdmiTimingList == NULL)) {
        ALOGW("Error. Input parameter is invalid.");
        return STATUS_ERROR;
    }
    status_t ret = (*mMds)->getHdmiMetaData(&currHdmiTiming, hdmiTimingList,&hdmiListCount);
    ALOGI("HDMI setting: %d x %d @ %d, count %d", currHdmiTiming.width, currHdmiTiming.height,
              currHdmiTiming.refresh, hdmiListCount);

    return ret;
}

//set display mode
status_t VPPWorker::configFrc4Hdmi(bool enableFrc4Hdmi, sp<VPPMDSListener>* pmds) {
    status_t status = STATUS_OK;

    mEnableFrc4Hdmi = enableFrc4Hdmi;
    if (mEnableFrc4Hdmi) {
        if (pmds == NULL) {
            mEnableFrc4Hdmi = false;
            status = STATUS_ERROR;
            ALOGE("MDS is NULL");
        }

        mMds = pmds;
        if (hdmiTimingList != NULL)
            delete []hdmiTimingList;
        hdmiTimingList = new MDSHdmiTiming[HDMI_TIMING_MAX];
        if (hdmiTimingList == NULL) {
            status = STATUS_ERROR;
            ALOGE("failed to allocat memory for VPP FRC for HDMI ");
        }
    }

    return status;
}

status_t VPPWorker::calculateFrc(bool *pFrcOn, FRC_RATE *pFrcRate) {
    status_t status;
    if (!pFrcRate || !pFrcOn) {
        return STATUS_ERROR;
    }

    if (!(mVPPOn & VPP_FRC_ON)) {
        *pFrcOn = false;
        return STATUS_OK;
    }

    ALOGV("EnableFrc4Hdmi %d  hdmiConnected %d", mEnableFrc4Hdmi, isHdmiConnected());
    if (mEnableFrc4Hdmi && (isHdmiConnected())) {
        status = getHdmiData();
        if (status != STATUS_ERROR) {
            //FRC policy for HDMI
            status = calcFrcByMatchHdmiCap(pFrcOn,pFrcRate);
        }
    } else {
        //FRC policy for MIPI
        status = calcFrcByInputFps(pFrcOn, pFrcRate);
    }

    ALOGI("FrcRate %d  mFrcOn %d", *pFrcRate, *pFrcOn);
    return status;
}

status_t VPPWorker::writeNV12(int width, int height, unsigned char *out_buf, int y_pitch, int uv_pitch) {
    size_t result;
    int frame_size;
    unsigned char *y_start, *uv_start;
    int h;

    FILE *ofile = fopen(FRAME_OUTPUT_FILE_NV12, "ab");
    if(ofile == NULL) {
        ALOGE("Open %s failed!", FRAME_OUTPUT_FILE_NV12);
        return STATUS_ERROR;
    }

    if (out_buf == NULL)
    {
        fclose(ofile);
        return STATUS_ERROR;
    }
    if ((width % 2) || (height % 2))
    {
        fclose(ofile);
        return STATUS_ERROR;
    }
    // Set frame size
    frame_size = height * width * 3/2;

    /* write y */
    y_start = out_buf;
    for (h = 0; h < height; ++h) {
        result = fwrite(y_start, sizeof(unsigned char), width, ofile);
        y_start += y_pitch;
    }

    /* write uv */
    uv_start = out_buf + uv_pitch * height;
    for (h = 0; h < height / 2; ++h) {
        result = fwrite(uv_start, sizeof(unsigned char), width, ofile);
        uv_start += uv_pitch;
    }
    // Close file
    fclose(ofile);
    return STATUS_OK;
}

uint32_t VPPWorker::isVppOn() {
    ALOGE("VPPWorkder::isVppOn");
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

    mVPPOn = mdsInfoProvider->getVppState();
    ALOGE("VPPWorkder::isVppOn, mVPPOn = %d", mVPPOn);

    return mVPPOn;
}
} //namespace Anroid
