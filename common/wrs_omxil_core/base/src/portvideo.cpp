/*
 * portvideo.cpp, port class for video
 *
 * Copyright (c) 2009-2010 Wind River Systems, Inc.
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

#include <stdlib.h>
#include <string.h>

#include <OMX_Core.h>
#include <OMX_Component.h>

#include <componentbase.h>
#include <portvideo.h>

#ifdef ANDROID
#define LOG_TAG "portvideo"
#include <utils/Log.h>
#endif

PortVideo::PortVideo()
{
    memset(&videoparam, 0, sizeof(videoparam));
    ComponentBase::SetTypeHeader(&videoparam, sizeof(videoparam));

    videoparam.nIndex = 0;
    videoparam.eCompressionFormat = OMX_VIDEO_CodingUnused;
    videoparam.eColorFormat = OMX_COLOR_FormatYUV420Planar;
    videoparam.xFramerate = 15 << 16;

    memset(&bitrateparam, 0, sizeof(bitrateparam));
    ComponentBase::SetTypeHeader(&bitrateparam, sizeof(bitrateparam));

    bitrateparam.eControlRate = OMX_Video_ControlRateConstant;
    bitrateparam.nTargetBitrate = 64000;

    memset(&privateinfoparam, 0, sizeof(privateinfoparam));
    ComponentBase::SetTypeHeader(&privateinfoparam, sizeof(privateinfoparam));

    privateinfoparam.nCapacity = 0;
    privateinfoparam.nHolder = NULL;

    mbufsharing = OMX_FALSE;
}

//PortVideo::~PortVideo()
//{
//    if(privateinfoparam.nHolder != NULL) {
//        free(privateinfoparam.nHolder);
//        privateinfoparam.nHolder = NULL;
//    }
//}

OMX_ERRORTYPE PortVideo::SetPortVideoParam(
    const OMX_VIDEO_PARAM_PORTFORMATTYPE *p, bool internal)
{
    if (!internal) {
        OMX_ERRORTYPE ret;

        ret = ComponentBase::CheckTypeHeader((void *)p, sizeof(*p));
        if (ret != OMX_ErrorNone)
            return ret;
        if (videoparam.nPortIndex != p->nPortIndex)
            return OMX_ErrorBadPortIndex;
    }
    else
        videoparam.nPortIndex = p->nPortIndex;

    videoparam.nIndex = p->nIndex;
    videoparam.eCompressionFormat = p->eCompressionFormat;
    videoparam.eColorFormat = p->eColorFormat;
    videoparam.xFramerate = p->xFramerate;

    return OMX_ErrorNone;
}

const OMX_VIDEO_PARAM_PORTFORMATTYPE *PortVideo::GetPortVideoParam(void)
{
    return &videoparam;
}

OMX_ERRORTYPE PortVideo::SetPortBitrateParam(
    const OMX_VIDEO_PARAM_BITRATETYPE *p, bool internal)
{
    if (!internal) {
        OMX_ERRORTYPE ret;

        ret = ComponentBase::CheckTypeHeader((void *)p, sizeof(*p));
        if (ret != OMX_ErrorNone)
            return ret;
        if (bitrateparam.nPortIndex != p->nPortIndex)
            return OMX_ErrorBadPortIndex;
    }
    else
        bitrateparam.nPortIndex = p->nPortIndex;

    bitrateparam.eControlRate = p->eControlRate;
    bitrateparam.nTargetBitrate = p->nTargetBitrate;

    return OMX_ErrorNone;
}

const OMX_VIDEO_PARAM_BITRATETYPE *PortVideo::GetPortBitrateParam(void)
{
    return &bitrateparam;
}

OMX_ERRORTYPE PortVideo::SetPortBufferSharingInfo(OMX_BOOL isbufsharing)
{
    mbufsharing = isbufsharing;

    return OMX_ErrorNone;
}

const OMX_BOOL *PortVideo::GetPortBufferSharingInfo(void)
{
    return &mbufsharing;
}


OMX_ERRORTYPE PortVideo::SetPortPrivateInfoParam(
    const OMX_VIDEO_CONFIG_PRI_INFOTYPE *p, bool internal)
{
    if (!internal) {
        OMX_ERRORTYPE ret;

        ret = ComponentBase::CheckTypeHeader((void *)p, sizeof(*p));
        if (ret != OMX_ErrorNone)
            return ret;
        if (privateinfoparam.nPortIndex != p->nPortIndex)
            return OMX_ErrorBadPortIndex;
    }
    else
        privateinfoparam.nPortIndex = p->nPortIndex;

    const OMX_BOOL *isbufsharing = GetPortBufferSharingInfo();
    if(*isbufsharing) {
        //if(privateinfoparam.nHolder != NULL) {
        //    free(privateinfoparam.nHolder);
        //    privateinfoparam.nHolder = NULL;
        //}
        if(p->nHolder != NULL) {
            privateinfoparam.nCapacity = p->nCapacity;
            privateinfoparam.nHolder = (OMX_PTR)malloc(sizeof(OMX_U32) * (p->nCapacity));
            memcpy(privateinfoparam.nHolder, p->nHolder, sizeof(OMX_U32) * (p->nCapacity));
        } else {
            privateinfoparam.nCapacity = 0;
            privateinfoparam.nHolder = NULL;
        }
    }
    
    return OMX_ErrorNone;
}

const OMX_VIDEO_CONFIG_PRI_INFOTYPE *PortVideo::GetPortPrivateInfoParam(void)
{
    return &privateinfoparam;
}


/* end of PortVideo */

PortAvc::PortAvc()
{
    OMX_VIDEO_PARAM_PORTFORMATTYPE videoparam;

    memcpy(&videoparam, GetPortVideoParam(), sizeof(videoparam));
    videoparam.eCompressionFormat = OMX_VIDEO_CodingAVC;
    videoparam.eColorFormat = OMX_COLOR_FormatUnused;
    videoparam.xFramerate = 15 << 16;
    SetPortVideoParam(&videoparam, false);

    memset(&avcparam, 0, sizeof(avcparam));
    memset(&avcprofilelevel, 0, sizeof(avcprofilelevel));

    avcparam.eProfile = OMX_VIDEO_AVCProfileVendorStartUnused;
    avcparam.eLevel = OMX_VIDEO_AVCLevelVendorStartUnused;
   
//set buffer sharing mode
#ifdef COMPONENT_SUPPORT_BUFFER_SHARING
    SetPortBufferSharingInfo(OMX_TRUE);
#endif

    ComponentBase::SetTypeHeader(&avcparam, sizeof(avcparam));
    ComponentBase::SetTypeHeader(&avcprofilelevel, sizeof(avcprofilelevel));
}

OMX_ERRORTYPE PortAvc::SetPortAvcParam(
    const OMX_VIDEO_PARAM_AVCTYPE *p, bool overwrite_readonly)
{
    if (!overwrite_readonly) {
        OMX_ERRORTYPE ret;

        ret = ComponentBase::CheckTypeHeader((void *)p, sizeof(*p));
        if (ret != OMX_ErrorNone)
            return ret;
        if (avcparam.nPortIndex != p->nPortIndex)
            return OMX_ErrorBadPortIndex;
    }
    else {
        OMX_VIDEO_PARAM_PORTFORMATTYPE videoparam;

        memcpy(&videoparam, GetPortVideoParam(), sizeof(videoparam));
        videoparam.nPortIndex = p->nPortIndex;
        SetPortVideoParam(&videoparam, true);

        avcparam.nPortIndex = p->nPortIndex;
    }

    avcparam.nSliceHeaderSpacing = p->nSliceHeaderSpacing;
    avcparam.nPFrames = p->nPFrames;
    avcparam.nBFrames = p->nBFrames;
    avcparam.bUseHadamard = p->bUseHadamard;
    avcparam.nRefFrames = p->nRefFrames;
    avcparam.nRefIdx10ActiveMinus1 = p->nRefIdx10ActiveMinus1;
    avcparam.nRefIdx11ActiveMinus1 = p->nRefIdx11ActiveMinus1;
    avcparam.bEnableUEP = p->bEnableUEP;
    avcparam.bEnableFMO = p->bEnableFMO;
    avcparam.bEnableASO = p->bEnableASO;
    avcparam.bEnableRS = p->bEnableRS;
    avcparam.nAllowedPictureTypes = p->nAllowedPictureTypes;
    avcparam.bFrameMBsOnly = p->bFrameMBsOnly;
    avcparam.bMBAFF = p->bMBAFF;
    avcparam.bEntropyCodingCABAC = p->bEntropyCodingCABAC;
    avcparam.bWeightedPPrediction = p->bWeightedPPrediction;
    avcparam.nWeightedBipredicitonMode = p->nWeightedBipredicitonMode;
    avcparam.bconstIpred = p->bconstIpred;
    avcparam.bDirect8x8Inference = p->bDirect8x8Inference;
    avcparam.bDirectSpatialTemporal = p->bDirectSpatialTemporal;
    avcparam.nCabacInitIdc = p->nCabacInitIdc;
    avcparam.eLoopFilterMode = p->eLoopFilterMode;

#ifdef COMPONENT_SUPPORT_OPENCORE
#ifdef COMPONENT_SUPPORT_BUFFER_SHARING
// sepcial case ,not change default profile and level for opencore buffer sharing.
#else
    avcparam.eProfile = p->eProfile;
    avcparam.eLevel = p->eLevel;
#endif
#else
    avcparam.eProfile = p->eProfile;
    avcparam.eLevel = p->eLevel;
#endif
    return OMX_ErrorNone;
}

const OMX_VIDEO_PARAM_AVCTYPE *PortAvc::GetPortAvcParam(void)
{
    return &avcparam;
}

const OMX_VIDEO_PARAM_PROFILELEVELTYPE *PortAvc::GetPortAvcProfileLevel(void)
{
    return &avcprofilelevel;
}
/* end of PortAvc */

PortMpeg4::PortMpeg4()
{
    OMX_VIDEO_PARAM_PORTFORMATTYPE videoparam;

    memcpy(&videoparam, GetPortVideoParam(), sizeof(videoparam));
    videoparam.eCompressionFormat = OMX_VIDEO_CodingMPEG4;
    videoparam.eColorFormat = OMX_COLOR_FormatUnused;
    videoparam.xFramerate = 15 << 16;
    SetPortVideoParam(&videoparam, false);

    memset(&mpeg4param, 0, sizeof(mpeg4param));
    ComponentBase::SetTypeHeader(&mpeg4param, sizeof(mpeg4param));

    mpeg4param.eProfile = OMX_VIDEO_MPEG4ProfileVendorStartUnused;
    mpeg4param.eLevel = OMX_VIDEO_MPEG4LevelVendorStartUnused;

#ifdef COMPONENT_SUPPORT_BUFFER_SHARING
    SetPortBufferSharingInfo(OMX_TRUE);
#endif
}

OMX_ERRORTYPE PortMpeg4::SetPortMpeg4Param(
    const OMX_VIDEO_PARAM_MPEG4TYPE *p, bool overwrite_readonly)
{
    if (!overwrite_readonly) {
        OMX_ERRORTYPE ret;

        ret = ComponentBase::CheckTypeHeader((void *)p, sizeof(*p));
        if (ret != OMX_ErrorNone)
            return ret;
        if (mpeg4param.nPortIndex != p->nPortIndex)
            return OMX_ErrorBadPortIndex;
    }
    else {
        OMX_VIDEO_PARAM_PORTFORMATTYPE videoparam;

        memcpy(&videoparam, GetPortVideoParam(), sizeof(videoparam));
        videoparam.nPortIndex = p->nPortIndex;
        SetPortVideoParam(&videoparam, true);

        mpeg4param.nPortIndex = p->nPortIndex;
    }

    mpeg4param.nSliceHeaderSpacing = p->nSliceHeaderSpacing;
    mpeg4param.bSVH = p->bSVH;
    mpeg4param.bGov = p->bGov;
    mpeg4param.nPFrames = p->nPFrames;
    mpeg4param.nBFrames = p->nBFrames;
    mpeg4param.nIDCVLCThreshold = p->nIDCVLCThreshold;
    mpeg4param.bACPred = p->bACPred;
    mpeg4param.nMaxPacketSize = p->nMaxPacketSize;
    mpeg4param.nTimeIncRes = p->nTimeIncRes;
#ifdef COMPONENT_SUPPORT_OPENCORE
#ifdef COMPONENT_SUPPORT_BUFFER_SHARING
// sepcial case ,not change default profile and level for opencore buffer sharing.
#else
    mpeg4param.eProfile = p->eProfile;
    mpeg4param.eLevel = p->eLevel;
#endif
#else
    mpeg4param.eProfile = p->eProfile;
    mpeg4param.eLevel = p->eLevel;
#endif
    mpeg4param.nAllowedPictureTypes = p->nAllowedPictureTypes;
    mpeg4param.nHeaderExtension = p->nHeaderExtension;
    mpeg4param.bReversibleVLC = p->bReversibleVLC;

    return OMX_ErrorNone;
}

const OMX_VIDEO_PARAM_MPEG4TYPE *PortMpeg4::GetPortMpeg4Param(void)
{
    return &mpeg4param;
}

/* end of PortMpeg4 */

PortH263::PortH263()
{
    OMX_VIDEO_PARAM_PORTFORMATTYPE videoparam;

    memcpy(&videoparam, GetPortVideoParam(), sizeof(videoparam));
    videoparam.eCompressionFormat = OMX_VIDEO_CodingH263;
    videoparam.eColorFormat = OMX_COLOR_FormatUnused;
    videoparam.xFramerate = 15 << 16;
    SetPortVideoParam(&videoparam, false);

    memset(&h263param, 0, sizeof(h263param));
    ComponentBase::SetTypeHeader(&h263param, sizeof(h263param));

    //set buffer sharing mode
    h263param.eProfile = OMX_VIDEO_H263ProfileVendorStartUnused;
    h263param.eLevel = OMX_VIDEO_H263LevelVendorStartUnused;

#ifdef COMPONENT_SUPPORT_BUFFER_SHARING
    SetPortBufferSharingInfo(OMX_TRUE);
#endif
	
}

OMX_ERRORTYPE PortH263::SetPortH263Param(
    const OMX_VIDEO_PARAM_H263TYPE *p, bool overwrite_readonly)
{
    if (!overwrite_readonly) {
        OMX_ERRORTYPE ret;

        ret = ComponentBase::CheckTypeHeader((void *)p, sizeof(*p));
        if (ret != OMX_ErrorNone)
            return ret;
        if (h263param.nPortIndex != p->nPortIndex)
            return OMX_ErrorBadPortIndex;
    }
    else {
        OMX_VIDEO_PARAM_PORTFORMATTYPE videoparam;

        memcpy(&videoparam, GetPortVideoParam(), sizeof(videoparam));
        videoparam.nPortIndex = p->nPortIndex;
        SetPortVideoParam(&videoparam, true);

        h263param.nPortIndex = p->nPortIndex;
    }

    h263param.nPFrames = p->nPFrames;
    h263param.nBFrames = p->nBFrames;
#ifdef COMPONENT_SUPPORT_OPENCORE
#ifdef COMPONENT_SUPPORT_BUFFER_SHARING
#else
    h263param.eProfile = p->eProfile;
    h263param.eLevel = p->eLevel;
#endif
#else
    h263param.eProfile = p->eProfile;
    h263param.eLevel = p->eLevel;
#endif
    h263param.bPLUSPTYPEAllowed        = p->bPLUSPTYPEAllowed;
    h263param.nAllowedPictureTypes     = p->nAllowedPictureTypes;
    h263param.bForceRoundingTypeToZero = p->bForceRoundingTypeToZero;
    h263param.nPictureHeaderRepetition = p->nPictureHeaderRepetition;
    h263param.nGOBHeaderInterval       = p->nGOBHeaderInterval;

    return OMX_ErrorNone;
}

const OMX_VIDEO_PARAM_H263TYPE *PortH263::GetPortH263Param(void)
{
    return &h263param;
}

/* end of PortH263 */


