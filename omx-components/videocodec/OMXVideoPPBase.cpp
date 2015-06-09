/*
* Copyright (c) 2009-2011 Intel Corporation.  All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

//#define LOG_NDEBUG 0
#define LOG_TAG "OMXVideoPPBase"
#include <utils/Log.h>
#include <OMX_VPP.h>
#include "OMXVideoPPBase.h"

using namespace android;

static const char* VA_RAW_MIME_TYPE = "video/x-raw-va";

OMXVideoPPBase::OMXVideoPPBase()
    : mOMXInBufferHeaderTypePtrNum(0),
      mOMXOutBufferHeaderTypePtrNum(0),
      mErrorReportEnabled (false),
      mFilters(0) {
    memset(&mGraphicBufferParam, 0, sizeof(mGraphicBufferParam));
    memset(&mFilterParam, 0, sizeof(mFilterParam));
    mVPP = VPPWorker::getInstance();
    if (!mVPP) {
        ALOGE("%s: create mVPP fails", __func__);
    }
    BuildHandlerList();
    ALOGI("OMXVideoPPBase constructs..");
}

OMXVideoPPBase::~OMXVideoPPBase() {

    if (this->ports) {
        if (this->ports[INPORT_INDEX]) {
            delete this->ports[INPORT_INDEX];
            this->ports[INPORT_INDEX] = NULL;
        }

        if (this->ports[OUTPORT_INDEX]) {
            delete this->ports[OUTPORT_INDEX];
            this->ports[OUTPORT_INDEX] = NULL;
        }
    }

    if (mVPP != NULL) {
        delete mVPP;
        mVPP = NULL;
    }
}

OMX_ERRORTYPE OMXVideoPPBase::InitInputPort(void) {
    this->ports[INPORT_INDEX] = new PortVideo;
    if (this->ports[INPORT_INDEX] == NULL) {
        return OMX_ErrorInsufficientResources;
    }

    PortVideo *port = static_cast<PortVideo *>(this->ports[INPORT_INDEX]);

    // OMX_PARAM_PORTDEFINITIONTYPE
    OMX_PARAM_PORTDEFINITIONTYPE paramPortDefinitionInput;
    memset(&paramPortDefinitionInput, 0, sizeof(paramPortDefinitionInput));
    SetTypeHeader(&paramPortDefinitionInput, sizeof(paramPortDefinitionInput));
    paramPortDefinitionInput.nPortIndex = INPORT_INDEX;
    paramPortDefinitionInput.eDir = OMX_DirInput;
    paramPortDefinitionInput.nBufferCountActual = INPORT_ACTUAL_BUFFER_COUNT;
    paramPortDefinitionInput.nBufferCountMin = INPORT_MIN_BUFFER_COUNT;
    paramPortDefinitionInput.nBufferSize = INPORT_BUFFER_SIZE;
    paramPortDefinitionInput.bEnabled = OMX_TRUE;
    paramPortDefinitionInput.bPopulated = OMX_FALSE;
    paramPortDefinitionInput.eDomain = OMX_PortDomainVideo;
    paramPortDefinitionInput.format.video.cMIMEType = (OMX_STRING)VA_RAW_MIME_TYPE;
    paramPortDefinitionInput.format.video.pNativeRender = NULL;
    paramPortDefinitionInput.format.video.nFrameWidth = 176;
    paramPortDefinitionInput.format.video.nFrameHeight = 144;
    paramPortDefinitionInput.format.video.nStride = 0;
    paramPortDefinitionInput.format.video.nSliceHeight = 0;
    paramPortDefinitionInput.format.video.nBitrate = 64000;
    paramPortDefinitionInput.format.video.xFramerate = 15 << 16;
    // TODO: check if we need to set bFlagErrorConcealment  to OMX_TRUE
    paramPortDefinitionInput.format.video.bFlagErrorConcealment = OMX_FALSE;
    paramPortDefinitionInput.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused; // to be overridden
    paramPortDefinitionInput.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    paramPortDefinitionInput.format.video.pNativeWindow = NULL;
    paramPortDefinitionInput.bBuffersContiguous = OMX_FALSE;
    paramPortDefinitionInput.nBufferAlignment = 0;

    // Derived class must implement this interface and override any field if needed.
    // eCompressionFormat and and cMIMEType must be overridden
    InitInputPortFormatSpecific(&paramPortDefinitionInput);

    port->SetPortDefinition(&paramPortDefinitionInput, true);

    // OMX_VIDEO_PARAM_PORTFORMATTYPE
    OMX_VIDEO_PARAM_PORTFORMATTYPE paramPortFormat;
    memset(&paramPortFormat, 0, sizeof(paramPortFormat));
    SetTypeHeader(&paramPortFormat, sizeof(paramPortFormat));
    paramPortFormat.nPortIndex = INPORT_INDEX;
    paramPortFormat.nIndex = 0;
    paramPortFormat.eCompressionFormat = paramPortDefinitionInput.format.video.eCompressionFormat;
    paramPortFormat.eColorFormat = paramPortDefinitionInput.format.video.eColorFormat;
    paramPortFormat.xFramerate = paramPortDefinitionInput.format.video.xFramerate;

    port->SetPortVideoParam(&paramPortFormat, true);

    return OMX_ErrorNone;
}


OMX_ERRORTYPE OMXVideoPPBase::InitOutputPort(void) {
    this->ports[OUTPORT_INDEX] = new PortVideo;
    if (this->ports[OUTPORT_INDEX] == NULL) {
        return OMX_ErrorInsufficientResources;
    }

    PortVideo *port = static_cast<PortVideo *>(this->ports[OUTPORT_INDEX]);

    // OMX_PARAM_PORTDEFINITIONTYPE
    OMX_PARAM_PORTDEFINITIONTYPE paramPortDefinitionOutput;

    memset(&paramPortDefinitionOutput, 0, sizeof(paramPortDefinitionOutput));
    SetTypeHeader(&paramPortDefinitionOutput, sizeof(paramPortDefinitionOutput));

    paramPortDefinitionOutput.nPortIndex = OUTPORT_INDEX;
    paramPortDefinitionOutput.eDir = OMX_DirOutput;
    paramPortDefinitionOutput.nBufferCountActual = OUTPORT_ACTUAL_BUFFER_COUNT;
    paramPortDefinitionOutput.nBufferCountMin = OUTPORT_MIN_BUFFER_COUNT;
    //paramPortDefinitionOutput.nBufferSize = sizeof(VideoRenderBuffer);

    paramPortDefinitionOutput.bEnabled = OMX_TRUE;
    paramPortDefinitionOutput.bPopulated = OMX_FALSE;
    paramPortDefinitionOutput.eDomain = OMX_PortDomainVideo;
    paramPortDefinitionOutput.format.video.cMIMEType = (OMX_STRING)VA_RAW_MIME_TYPE;
    paramPortDefinitionOutput.format.video.pNativeRender = NULL;
    paramPortDefinitionOutput.format.video.nFrameWidth = 176;
    paramPortDefinitionOutput.format.video.nFrameHeight = 144;
    paramPortDefinitionOutput.format.video.nStride = 176;
    paramPortDefinitionOutput.format.video.nSliceHeight = 144;
    paramPortDefinitionOutput.format.video.nBitrate = 64000;
    paramPortDefinitionOutput.format.video.xFramerate = 15 << 16;
    paramPortDefinitionOutput.format.video.bFlagErrorConcealment = OMX_FALSE;
    paramPortDefinitionOutput.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    //paramPortDefinitionOutput.format.video.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
    paramPortDefinitionOutput.format.video.eColorFormat = OMX_COLOR_FormatYUV422PackedSemiPlanar;
    paramPortDefinitionOutput.format.video.pNativeWindow = NULL;
    paramPortDefinitionOutput.bBuffersContiguous = OMX_FALSE;
    paramPortDefinitionOutput.nBufferAlignment = 0;

    // no format specific to initialize output port
    InitOutputPortFormatSpecific(&paramPortDefinitionOutput);

    //should put in specific
    paramPortDefinitionOutput.format.video.cMIMEType = (OMX_STRING)"video/pp"; // to be overridden

    port->SetPortDefinition(&paramPortDefinitionOutput, true);

    // OMX_VIDEO_PARAM_PORTFORMATTYPE
    OMX_VIDEO_PARAM_PORTFORMATTYPE paramPortFormat;
    SetTypeHeader(&paramPortFormat, sizeof(paramPortFormat));
    paramPortFormat.nPortIndex = OUTPORT_INDEX;
    paramPortFormat.nIndex = 0;
    paramPortFormat.eCompressionFormat = paramPortDefinitionOutput.format.video.eCompressionFormat;
    paramPortFormat.eColorFormat = paramPortDefinitionOutput.format.video.eColorFormat;
    paramPortFormat.xFramerate = paramPortDefinitionOutput.format.video.xFramerate;

    port->SetPortVideoParam(&paramPortFormat, true);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoPPBase::InitInputPortFormatSpecific(OMX_PARAM_PORTDEFINITIONTYPE *paramPortDefinitionInput) {
    // no format specific to initialize output port
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoPPBase::InitOutputPortFormatSpecific(OMX_PARAM_PORTDEFINITIONTYPE *paramPortDefinitionOutput) {
    // no format specific to initialize output port
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoPPBase::ProcessorInit(void) {
    ALOGV("ProcessInit");
    OMX_ERRORTYPE ret;
    ret = OMXComponentCodecBase::ProcessorInit();
    CHECK_RETURN_VALUE("OMXComponentCodecBase::ProcessorInit");

    if (mVPP == NULL) {
        ALOGE("ProcessorInit: Video VPP is not created.");
        return OMX_ErrorDynamicResourcesUnavailable;
    }

    PortVideo *port = static_cast<PortVideo *>(this->ports[OUTPORT_INDEX]);
    status_t status = mVPP->init(port);
    if (status != STATUS_OK) {
        ALOGE("ProcessorInit: VPP initialize failed.");
        return OMX_ErrorUndefined;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoPPBase::ProcessorReset(void) {
    ALOGV("ProcessReset");

    if (mVPP == NULL) {
        ALOGE("ProcessorInit: Video VPP is not created.");
        return OMX_ErrorDynamicResourcesUnavailable;
    }

    status_t status = mVPP->reset();
    if (status != STATUS_OK) {
        ALOGE("ProcessorReset: VPP reset failed.");
        return OMX_ErrorUndefined;
    }
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoPPBase::ProcessorDeinit(void) {
    ALOGV("ProcessDeinit");

    if (mVPP == NULL) {
        ALOGE("ProcessorInit: Video VPP is not created.");
        return OMX_ErrorDynamicResourcesUnavailable;
    }
    mOMXInBufferHeaderTypePtrNum = 0;
    mOMXOutBufferHeaderTypePtrNum = 0;
    memset(&mFilterParam, 0, sizeof(mFilterParam));
    memset(&mGraphicBufferParam, 0, sizeof(mGraphicBufferParam));

    return OMXComponentCodecBase::ProcessorDeinit();
}

OMX_ERRORTYPE OMXVideoPPBase::ProcessorStart(void) {
    ALOGV("ProcessStart");
    return OMXComponentCodecBase::ProcessorStart();
}

OMX_ERRORTYPE OMXVideoPPBase::ProcessorProcess(
    OMX_BUFFERHEADERTYPE ***pBuffers,
    buffer_retain_t *retains,
    OMX_U32 numberBuffers) {
    ALOGV("ProcessorProcess");
    OMX_BUFFERHEADERTYPE *pOutputBuffer = *pBuffers[OUTPORT_INDEX];
    OMX_BUFFERHEADERTYPE *pInputBuffer = *pBuffers[INPORT_INDEX];

    mVPP->configFilters((uint32_t*)&mFilters, &mFilterParam, pInputBuffer->nFlags);

    if (pInputBuffer->pBuffer == NULL || pInputBuffer->nFilledLen == 0) {
        ALOGE("%s: Buffer for VPP input is empty.", __func__);
        return OMX_ErrorBadParameter;
    }

    ALOGV("%s: VPP input 0x%x, output 0x%x, width %d, height %d", __func__,
            pInputBuffer->pBuffer, pOutputBuffer->pBuffer,
            mGraphicBufferParam.graphicBufferWidth,
            mGraphicBufferParam.graphicBufferHeight);
    mVPP->process((buffer_handle_t)pInputBuffer->pBuffer, (buffer_handle_t)pOutputBuffer->pBuffer, 1, false, pInputBuffer->nFlags);

    pOutputBuffer->nFilledLen = sizeof(OMX_U8*);
    pOutputBuffer->nTimeStamp = pInputBuffer->nTimeStamp;
    *retains = BUFFER_RETAIN_NOT_RETAIN;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoPPBase::ProcessorStop(void) {
    ALOGV("ProcessorStop");
    return OMXComponentCodecBase::ProcessorStop();
}

OMX_ERRORTYPE OMXVideoPPBase::ProcessorPause(void) {
    ALOGV("ProcessorPause");
    return OMXComponentCodecBase::ProcessorPause();
}

OMX_ERRORTYPE OMXVideoPPBase::ProcessorResume(void) {
    ALOGV("ProcessorResume");
    return OMXComponentCodecBase::ProcessorResume();
}

OMX_ERRORTYPE OMXVideoPPBase::ProcessorFlush(OMX_U32 portIndex) {
    ALOGV("ProcessorFlush");
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoPPBase::GetNativeBuffer(OMX_PTR pStructure) {
    return OMX_ErrorBadParameter;
}

OMX_ERRORTYPE OMXVideoPPBase::SetNativeBuffer(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    UseAndroidNativeBufferParams *param = (UseAndroidNativeBufferParams*)pStructure;
    CHECK_TYPE_HEADER(param);

    ALOGI("setNativeBuffer for handle %p on port %d", pStructure, param->nPortIndex);

    //handle input port and output port both
    OMX_BUFFERHEADERTYPE **buf_hdr = NULL;

    if (param->nPortIndex == INPORT_INDEX) {
        mOMXInBufferHeaderTypePtrNum++;
        if (mOMXInBufferHeaderTypePtrNum > MAX_GRAPHIC_BUFFER_NUM)
            return OMX_ErrorOverflow;

        buf_hdr = &mOMXInBufferHeaderTypePtrArray[mOMXInBufferHeaderTypePtrNum-1];
    } else if (param->nPortIndex == OUTPORT_INDEX) {
        mOMXOutBufferHeaderTypePtrNum++;
        if (mOMXOutBufferHeaderTypePtrNum > MAX_GRAPHIC_BUFFER_NUM)
            return OMX_ErrorOverflow;

        buf_hdr = &mOMXOutBufferHeaderTypePtrArray[mOMXOutBufferHeaderTypePtrNum-1];
    }

    ret = this->ports[param->nPortIndex]->UseBuffer(buf_hdr, param->nPortIndex, param->pAppPrivate, sizeof(OMX_U8*),
                                      const_cast<OMX_U8*>(reinterpret_cast<const OMX_U8*>(param->nativeBuffer->handle)));
    if (ret != OMX_ErrorNone)
        return ret;

    if (mOMXOutBufferHeaderTypePtrNum == 1 && param->nPortIndex == OUTPORT_INDEX) {
         mGraphicBufferParam.graphicBufferColorFormat = param->nativeBuffer->format;
         mGraphicBufferParam.graphicBufferStride = param->nativeBuffer->stride;
         mFilterParam.dstWidth = mGraphicBufferParam.graphicBufferWidth = param->nativeBuffer->width;
         mFilterParam.dstHeight = mGraphicBufferParam.graphicBufferHeight = param->nativeBuffer->height;
         ALOGI("dest w = %d, h = %d", mFilterParam.dstWidth, mFilterParam.dstHeight);
    }

    if (mOMXInBufferHeaderTypePtrNum == 1 && param->nPortIndex == INPORT_INDEX) {
        mFilterParam.srcWidth = param->nativeBuffer->width;
        mFilterParam.srcHeight = param->nativeBuffer->height;
        ALOGI("src w = %d, h = %d", mFilterParam.srcWidth, mFilterParam.srcHeight);
    }

    *(param->bufferHeader) = *buf_hdr;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoPPBase::GetFilterTypes(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_INTEL_CONFIG_FILTERTYPE * param = (OMX_INTEL_CONFIG_FILTERTYPE *) pStructure;

    CHECK_TYPE_HEADER(param);
    //FIX ME: need to query filter caps from driver.
    param->nFilterType = (OMX_INTEL_ImageFilterDenoise
            | OMX_INTEL_ImageFilterDeinterlace
            | OMX_INTEL_ImageFilterSharpness
            | OMX_INTEL_ImageFilterScale
            | OMX_INTEL_ImageFilterColorBalance
            | OMX_INTEL_ImageFilter3P);
     return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoPPBase::SetFilterTypes(OMX_PTR pStructure) {
    //Don't use this interface. Component own which filters on or off. 
#if 0
    CHECK_SET_PARAM_STATE();

    OMX_ERRORTYPE ret;
    OMX_U32 *param = (OMX_U32 *) pStructure;
    CHECK_TYPE_HEADER(param);
    mFilters = *param;

    return OMX_ErrorNone;
#endif
    return OMX_ErrorBadParameter;
}

OMX_ERRORTYPE OMXVideoPPBase::GetDenoiseLevel(OMX_PTR pStructure) {
    return OMX_ErrorBadParameter;
}

OMX_ERRORTYPE OMXVideoPPBase::SetDenoiseLevel(OMX_PTR pStructure) {
    CHECK_SET_CONFIG_STATE();

    OMX_ERRORTYPE ret;
    OMX_INTEL_CONFIG_DENOISETYPE *param = (OMX_INTEL_CONFIG_DENOISETYPE *) pStructure;
    CHECK_TYPE_HEADER(param);
    mFilterParam.denoiseLevel = param->nLevel;

    return OMX_ErrorNone;
}


OMX_ERRORTYPE OMXVideoPPBase::GetDeinterlaceLevel(OMX_PTR pStructure) {
    return OMX_ErrorBadParameter;
}

OMX_ERRORTYPE OMXVideoPPBase::SetDeinterlaceLevel(OMX_PTR pStructure) {
    CHECK_SET_CONFIG_STATE();

    OMX_ERRORTYPE ret;
    OMX_INTEL_CONFIG_DEINTERLACETYPE *param = (OMX_INTEL_CONFIG_DEINTERLACETYPE *) pStructure;
    CHECK_TYPE_HEADER(param);
    switch (param->eDeinterlace) {
        case OMX_INTEL_DeinterlaceADI:
            mFilterParam.deinterlaceType = IVP_DEINTERLACE_ADI;
            break;
        case OMX_INTEL_DeinterlaceBob:
        default:
            mFilterParam.deinterlaceType = IVP_DEINTERLACE_BOB;
            break;
    }
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoPPBase::GetColorBalanceLevel(OMX_PTR pStructure) {
    return OMX_ErrorBadParameter;
}

OMX_ERRORTYPE OMXVideoPPBase::SetColorBalanceLevel(OMX_PTR pStructure) {
    CHECK_SET_CONFIG_STATE();

    OMX_ERRORTYPE ret;
    OMX_INTEL_CONFIG_COLORBALANCETYPE *param = (OMX_INTEL_CONFIG_COLORBALANCETYPE *) pStructure;
    CHECK_TYPE_HEADER(param);
    mFilterParam.colorBalanceLevel = param->nLevel;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoPPBase::GetSharpnessLevel(OMX_PTR pStructure) {
    return OMX_ErrorBadParameter;
}

OMX_ERRORTYPE OMXVideoPPBase::SetSharpnessLevel(OMX_PTR pStructure) {
    CHECK_SET_CONFIG_STATE();

    OMX_ERRORTYPE ret;
    OMX_INTEL_CONFIG_SHARPNESSTYPE *param = (OMX_INTEL_CONFIG_SHARPNESSTYPE *) pStructure;
    CHECK_TYPE_HEADER(param);
    mFilterParam.sharpnessLevel = param->nLevel;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoPPBase::GetScaleLevel(OMX_PTR pStructure) {
    return OMX_ErrorBadParameter;
}

OMX_ERRORTYPE OMXVideoPPBase::SetScaleLevel(OMX_PTR pStructure) {
    CHECK_SET_CONFIG_STATE();

    OMX_ERRORTYPE ret;
    OMX_INTEL_CONFIG_SCALARTYPE *param = (OMX_INTEL_CONFIG_SCALARTYPE *) pStructure;
    CHECK_TYPE_HEADER(param);
    switch (param->eScalar) {
        case OMX_INTEL_ScalarBilinear:
            mFilterParam.scalarType = IVP_FILTER_FAST;
            break;
        case OMX_INTEL_ScalarAvs:
        default:
            mFilterParam.scalarType = IVP_FILTER_HQ;
            break;
    }
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoPPBase::GetIntel3PLevel(OMX_PTR pStructure) {
    return OMX_ErrorBadParameter;
}

OMX_ERRORTYPE OMXVideoPPBase::SetIntel3PLevel(OMX_PTR pStructure) {
    CHECK_SET_CONFIG_STATE();

    OMX_ERRORTYPE ret;
    OMX_INTEL_CONFIG_INTEL3PTYPE *param = (OMX_INTEL_CONFIG_INTEL3PTYPE *) pStructure;
    CHECK_TYPE_HEADER(param);

    mFilterParam.frameRate = param->nFrameRate;
    mFilterParam.hasEncoder = param->nHasEncoder;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoPPBase::BuildHandlerList(void) {
    OMXComponentCodecBase::BuildHandlerList();
    AddHandler(static_cast<OMX_INDEXTYPE>(OMX_IndexExtUseNativeBuffer), GetNativeBuffer, SetNativeBuffer);
    AddHandler(static_cast<OMX_INDEXTYPE>(OMX_INTEL_IndexConfigFilterType), GetFilterTypes, SetFilterTypes);
    AddHandler(static_cast<OMX_INDEXTYPE>(OMX_INTEL_IndexConfigDenoiseLevel), GetDenoiseLevel, SetDenoiseLevel);
    AddHandler(static_cast<OMX_INDEXTYPE>(OMX_INTEL_IndexConfigDeinterlaceLevel), GetDeinterlaceLevel, SetDeinterlaceLevel);
    AddHandler(static_cast<OMX_INDEXTYPE>(OMX_INTEL_IndexConfigColorBalanceLevel), GetColorBalanceLevel, SetColorBalanceLevel);
    AddHandler(static_cast<OMX_INDEXTYPE>(OMX_INTEL_IndexConfigSharpnessLevel), GetSharpnessLevel, SetSharpnessLevel);
    AddHandler(static_cast<OMX_INDEXTYPE>(OMX_INTEL_IndexConfigScaleLevel), GetScaleLevel, SetScaleLevel);
    AddHandler(static_cast<OMX_INDEXTYPE>(OMX_INTEL_IndexConfigIntel3PLevel), GetIntel3PLevel, SetIntel3PLevel);

    return OMX_ErrorNone;
}

DECLARE_OMX_COMPONENT("OMX.Intel.Video.PostProcess", "video.postprocess", OMXVideoPPBase);

