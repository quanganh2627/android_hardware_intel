/*
 * Copyright (C) 2011 The Android Open Source Project
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
#ifndef M4XVSS_NV12_H
#define M4XVSS_NV12_H

M4OSA_ERR M4VSS3GPP_internalConvertAndResizeARGB8888toNV12(M4OSA_Void* pFileIn,
    M4OSA_FileReadPointer* pFileReadPtr, M4VIFI_ImagePlane* pImagePlanes,
    M4OSA_UInt32 width, M4OSA_UInt32 height);

M4OSA_ERR M4xVSS_PictureCallbackFct_NV12(M4OSA_Void* pPictureCtxt,
    M4VIFI_ImagePlane* pImagePlanes, M4OSA_Double*  pPictureDuration);

M4OSA_ERR M4VSS3GPP_externalVideoEffectColor_NV12(M4OSA_Void *pFunctionContext,
    M4VIFI_ImagePlane *PlaneIn, M4VIFI_ImagePlane *PlaneOut,
    M4VSS3GPP_ExternalProgress *pProgress, M4OSA_UInt32 uiEffectKind);

M4OSA_ERR M4VSS3GPP_externalVideoEffectFraming_NV12(M4OSA_Void *userData,
    M4VIFI_ImagePlane *PlaneIn, M4VIFI_ImagePlane *PlaneOut,
    M4VSS3GPP_ExternalProgress *pProgress, M4OSA_UInt32 uiEffectKind);

M4OSA_ERR M4xVSS_internalConvertRGBtoNV12(M4xVSS_FramingStruct* framingCtx);

M4OSA_ERR M4xVSS_AlphaMagic_NV12(M4OSA_Void *userData, M4VIFI_ImagePlane *PlaneIn1,
    M4VIFI_ImagePlane *PlaneIn2, M4VIFI_ImagePlane *PlaneOut,
    M4VSS3GPP_ExternalProgress *pProgress, M4OSA_UInt32 uiTransitionKind);

M4OSA_ERR M4xVSS_internalConvertAndResizeARGB8888toNV12(M4OSA_Void* pFileIn,
    M4OSA_FileReadPointer* pFileReadPtr, M4VIFI_ImagePlane* pImagePlanes,
    M4OSA_UInt32 width,M4OSA_UInt32 height);

M4OSA_ERR M4xVSS_internalConvertARGB888toNV12_FrammingEffect(M4OSA_Context pContext,
    M4VSS3GPP_EffectSettings* pEffect, M4xVSS_FramingStruct* framingCtx,
    M4VIDEOEDITING_VideoFrameSize OutputVideoResolution);

M4OSA_ERR M4xVSS_AlphaMagicBlending_NV12(M4OSA_Void *userData,
    M4VIFI_ImagePlane *PlaneIn1, M4VIFI_ImagePlane *PlaneIn2,
    M4VIFI_ImagePlane *PlaneOut, M4VSS3GPP_ExternalProgress *pProgress,
    M4OSA_UInt32 uiTransitionKind);

M4OSA_ERR M4xVSS_SlideTransition_NV12(M4OSA_Void *userData,
    M4VIFI_ImagePlane *PlaneIn1, M4VIFI_ImagePlane *PlaneIn2,
    M4VIFI_ImagePlane *PlaneOut, M4VSS3GPP_ExternalProgress *pProgress,
    M4OSA_UInt32 uiTransitionKind);

M4OSA_ERR M4xVSS_FadeBlackTransition_NV12(M4OSA_Void *userData,
    M4VIFI_ImagePlane *PlaneIn1, M4VIFI_ImagePlane *PlaneIn2,
    M4VIFI_ImagePlane *PlaneOut, M4VSS3GPP_ExternalProgress *pProgress,
    M4OSA_UInt32 uiTransitionKind);

#endif
