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


#ifndef EDITVIDEO_NV12_H
#define EDITVIDEO_NV12_H

M4OSA_ERR M4VSS3GPP_intSetNv12PlaneFromARGB888(
    M4VSS3GPP_InternalEditContext *pC, M4VSS3GPP_ClipContext* pClipCtxt);

M4OSA_ERR M4VSS3GPP_intRotateVideo_NV12(M4VIFI_ImagePlane* pPlaneIn,
    M4OSA_UInt32 rotationDegree);

M4OSA_ERR M4VSS3GPP_intApplyRenderingMode_NV12(M4VSS3GPP_InternalEditContext *pC,
    M4xVSS_MediaRendering renderingMode, M4VIFI_ImagePlane* pInplane,
    M4VIFI_ImagePlane* pOutplane);

M4OSA_ERR M4VSS3GPP_intSetNV12Plane(M4VIFI_ImagePlane* planeIn,
    M4OSA_UInt32 width, M4OSA_UInt32 height);

unsigned char M4VFL_modifyLumaWithScale_NV12(M4ViComImagePlane *plane_in,
    M4ViComImagePlane *plane_out, unsigned long lum_factor,
    void *user_data);

unsigned char M4VIFI_ImageBlendingonNV12 (void *pUserData,
    M4ViComImagePlane *pPlaneIn1, M4ViComImagePlane *pPlaneIn2,
    M4ViComImagePlane *pPlaneOut, UInt32 Progress);

#endif
