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

/**
 * Our header */
#include "M4VSS3GPP_API.h"
#include "M4VSS3GPP_InternalTypes.h"
#include "M4VSS3GPP_InternalFunctions.h"
#include "M4VSS3GPP_InternalConfig.h"
#include "M4VSS3GPP_ErrorCodes.h"

// StageFright encoders require %16 resolution
#include "M4ENCODER_common.h"
/**
 * OSAL headers */
#include "M4OSA_Memory.h" /**< OSAL memory management */
#include "M4OSA_Debug.h"  /**< OSAL debug management */

#include "M4AIR_API_NV12.h"
#include "VideoEditorToolsNV12.h"

#define M4xVSS_ABS(a) ( ( (a) < (0) ) ? (-(a)) : (a) )
#define Y_PLANE_BORDER_VALUE    0x00
#define UV_PLANE_BORDER_VALUE   0x80

/**
******************************************************************************
* M4OSA_ERR M4VSS3GPP_internalConvertAndResizeARGB8888toNV12(M4OSA_Void* pFileIn,
*                                            M4OSA_FileReadPointer* pFileReadPtr,
*                                               M4VIFI_ImagePlane* pImagePlanes,
*                                               M4OSA_UInt32 width,
*                                               M4OSA_UInt32 height);
* @brief    It Coverts and resizes a ARGB8888 image to NV12
* @note
* @param    pFileIn         (IN) The ARGB888 input file
* @param    pFileReadPtr    (IN) Pointer on filesystem functions
* @param    pImagePlanes    (IN/OUT) Pointer on NV12 output planes allocated by the user.
*                           ARGB8888 image  will be converted and resized to output
*                           NV12 plane size
* @param width       (IN) width of the ARGB8888
* @param height      (IN) height of the ARGB8888
* @return   M4NO_ERROR: No error
* @return   M4ERR_ALLOC: memory error
* @return   M4ERR_PARAMETER: At least one of the function parameters is null
******************************************************************************
*/

M4OSA_ERR M4VSS3GPP_internalConvertAndResizeARGB8888toNV12(M4OSA_Void* pFileIn,
    M4OSA_FileReadPointer* pFileReadPtr, M4VIFI_ImagePlane* pImagePlanes,
    M4OSA_UInt32 width, M4OSA_UInt32 height)
{
    M4OSA_Context pARGBIn;
    M4VIFI_ImagePlane rgbPlane1 ,rgbPlane2;
    M4OSA_UInt32 frameSize_argb = width * height * 4;
    M4OSA_UInt32 frameSize_rgb888 = width * height * 3;
    M4OSA_UInt32 i = 0,j= 0;
    M4OSA_ERR err = M4NO_ERROR;

    M4OSA_UInt8 *pArgbPlane =
        (M4OSA_UInt8*) M4OSA_32bitAlignedMalloc(frameSize_argb,
                                                M4VS, (M4OSA_Char*)"argb data");
    if (pArgbPlane == M4OSA_NULL) {
        M4OSA_TRACE1_0("M4VSS3GPP_internalConvertAndResizeARGB8888toNV12: \
            Failed to allocate memory for ARGB plane");
        return M4ERR_ALLOC;
    }

    /* Get file size */
    err = pFileReadPtr->openRead(&pARGBIn, pFileIn, M4OSA_kFileRead);
    if (err != M4NO_ERROR) {
        M4OSA_TRACE1_2("M4VSS3GPP_internalConvertAndResizeARGB8888toNV12 : \
            Can not open input ARGB8888 file %s, error: 0x%x\n",pFileIn, err);
        free(pArgbPlane);
        pArgbPlane = M4OSA_NULL;
        goto cleanup;
    }

    err = pFileReadPtr->readData(pARGBIn,(M4OSA_MemAddr8)pArgbPlane,
                                 &frameSize_argb);
    if (err != M4NO_ERROR) {
        M4OSA_TRACE1_2("M4VSS3GPP_internalConvertAndResizeARGB8888toNV12 \
            Can not read ARGB8888 file %s, error: 0x%x\n",pFileIn, err);
        pFileReadPtr->closeRead(pARGBIn);
        free(pArgbPlane);
        pArgbPlane = M4OSA_NULL;
        goto cleanup;
    }

    err = pFileReadPtr->closeRead(pARGBIn);
    if(err != M4NO_ERROR) {
        M4OSA_TRACE1_2("M4VSS3GPP_internalConvertAndResizeARGB8888toNV12 \
            Can not close ARGB8888  file %s, error: 0x%x\n",pFileIn, err);
        free(pArgbPlane);
        pArgbPlane = M4OSA_NULL;
        goto cleanup;
    }

    rgbPlane1.pac_data =
        (M4VIFI_UInt8*)M4OSA_32bitAlignedMalloc(frameSize_rgb888,
                                            M4VS, (M4OSA_Char*)"RGB888 plane1");
    if(rgbPlane1.pac_data == M4OSA_NULL) {
        M4OSA_TRACE1_0("M4VSS3GPP_internalConvertAndResizeARGB8888toNV12 \
            Failed to allocate memory for rgb plane1");
        free(pArgbPlane);
        return M4ERR_ALLOC;
    }
    rgbPlane1.u_height = height;
    rgbPlane1.u_width = width;
    rgbPlane1.u_stride = width*3;
    rgbPlane1.u_topleft = 0;


    /** Remove the alpha channel */
    for (i=0, j = 0; i < frameSize_argb; i++) {
        if ((i % 4) == 0) continue;
        rgbPlane1.pac_data[j] = pArgbPlane[i];
        j++;
    }
    free(pArgbPlane);

    /**
     * Check if resizing is required with color conversion */
    if(width != pImagePlanes->u_width || height != pImagePlanes->u_height) {

        frameSize_rgb888 = pImagePlanes->u_width * pImagePlanes->u_height * 3;
        rgbPlane2.pac_data =
            (M4VIFI_UInt8*)M4OSA_32bitAlignedMalloc(frameSize_rgb888, M4VS,
                                                   (M4OSA_Char*)"rgb Plane2");
        if(rgbPlane2.pac_data == M4OSA_NULL) {
            M4OSA_TRACE1_0("Failed to allocate memory for rgb plane2");
            free(rgbPlane1.pac_data);
            return M4ERR_ALLOC;
        }
        rgbPlane2.u_height =  pImagePlanes->u_height;
        rgbPlane2.u_width = pImagePlanes->u_width;
        rgbPlane2.u_stride = pImagePlanes->u_width*3;
        rgbPlane2.u_topleft = 0;

        /* Resizing */
        err = M4VIFI_ResizeBilinearRGB888toRGB888(M4OSA_NULL,
                                                  &rgbPlane1, &rgbPlane2);
        free(rgbPlane1.pac_data);
        if(err != M4NO_ERROR) {
            M4OSA_TRACE1_1("error resizing RGB888 to RGB888: 0x%x\n", err);
            free(rgbPlane2.pac_data);
            return err;
        }

        /*Converting Resized RGB888 to NV12 */
        err = M4VIFI_RGB888toNV12(M4OSA_NULL, &rgbPlane2, pImagePlanes);
        free(rgbPlane2.pac_data);
        if(err != M4NO_ERROR) {
            M4OSA_TRACE1_1("error converting from RGB888 to NV12: 0x%x\n", err);
            return err;
        }
    } else {
        err = M4VIFI_RGB888toNV12(M4OSA_NULL, &rgbPlane1, pImagePlanes);
        if(err != M4NO_ERROR) {
            M4OSA_TRACE1_1("error when converting from RGB to NV12: 0x%x\n", err);
        }
        free(rgbPlane1.pac_data);
    }
cleanup:
    M4OSA_TRACE3_0("M4VSS3GPP_internalConvertAndResizeARGB8888toNV12 exit");
    return err;
}


M4OSA_ERR M4VSS3GPP_intApplyRenderingMode_NV12(M4VSS3GPP_InternalEditContext *pC,
    M4xVSS_MediaRendering renderingMode, M4VIFI_ImagePlane* pInplane,
    M4VIFI_ImagePlane* pOutplane)
{

    M4OSA_ERR err = M4NO_ERROR;
    M4AIR_Params airParams;
    M4VIFI_ImagePlane pImagePlanesTemp[2];
    M4OSA_UInt32 i = 0;

    M4OSA_TRACE1_0("M4VSS3GPP_intApplyRenderingMode_NV12 begin");

    if (renderingMode == M4xVSS_kBlackBorders) {
        memset((void *)pOutplane[0].pac_data, Y_PLANE_BORDER_VALUE,
               (pOutplane[0].u_height*pOutplane[0].u_stride));
        memset((void *)pOutplane[1].pac_data, UV_PLANE_BORDER_VALUE,
               (pOutplane[1].u_height*pOutplane[1].u_stride));
    }

    if (renderingMode == M4xVSS_kResizing) {
        /**
        * Call the resize filter.
        * From the intermediate frame to the encoder image plane */
        err = M4VIFI_ResizeBilinearNV12toNV12(M4OSA_NULL,
                                                  pInplane, pOutplane);
        if (M4NO_ERROR != err) {
            M4OSA_TRACE1_1("M4VSS3GPP_intApplyRenderingMode: \
                M4ViFilResizeBilinearNV12toNV12 returns 0x%x!", err);
            return err;
        }
    } else {
        M4VIFI_ImagePlane* pPlaneTemp = M4OSA_NULL;
        M4OSA_UInt8* pOutPlaneY =
            pOutplane[0].pac_data + pOutplane[0].u_topleft;
        M4OSA_UInt8* pOutPlaneUV =
            pOutplane[1].pac_data + pOutplane[1].u_topleft;

        M4OSA_UInt8* pInPlaneY = M4OSA_NULL;
        M4OSA_UInt8* pInPlaneUV = M4OSA_NULL;

        /* To keep media aspect ratio*/
        /* Initialize AIR Params*/
        airParams.m_inputCoord.m_x = 0;
        airParams.m_inputCoord.m_y = 0;
        airParams.m_inputSize.m_height = pInplane->u_height;
        airParams.m_inputSize.m_width = pInplane->u_width;
        airParams.m_outputSize.m_width = pOutplane->u_width;
        airParams.m_outputSize.m_height = pOutplane->u_height;
        airParams.m_bOutputStripe = M4OSA_FALSE;
        airParams.m_outputOrientation = M4COMMON_kOrientationTopLeft;

        /**
        Media rendering: Black borders*/
        if (renderingMode == M4xVSS_kBlackBorders) {
            pImagePlanesTemp[0].u_width = pOutplane[0].u_width;
            pImagePlanesTemp[0].u_height = pOutplane[0].u_height;
            pImagePlanesTemp[0].u_stride = pOutplane[0].u_width;
            pImagePlanesTemp[0].u_topleft = 0;

            pImagePlanesTemp[1].u_width = pOutplane[1].u_width;
            pImagePlanesTemp[1].u_height = pOutplane[1].u_height;
            pImagePlanesTemp[1].u_stride = pOutplane[1].u_width;
            pImagePlanesTemp[1].u_topleft = 0;


            /**
             * Allocates plan in local image plane structure */
            pImagePlanesTemp[0].pac_data =
                (M4OSA_UInt8*)M4OSA_32bitAlignedMalloc(
                    pImagePlanesTemp[0].u_width * pImagePlanesTemp[0].u_height,
                    M4VS, (M4OSA_Char *)"pImagePlaneTemp Y") ;
            if (pImagePlanesTemp[0].pac_data == M4OSA_NULL) {
                M4OSA_TRACE1_0("M4VSS3GPP_intApplyRenderingMode_NV12: Alloc Error");
                return M4ERR_ALLOC;
            }
            pImagePlanesTemp[1].pac_data =
                (M4OSA_UInt8*)M4OSA_32bitAlignedMalloc(
                    pImagePlanesTemp[1].u_width * pImagePlanesTemp[1].u_height,
                    M4VS, (M4OSA_Char *)"pImagePlaneTemp UV") ;
            if (pImagePlanesTemp[1].pac_data == M4OSA_NULL) {
                M4OSA_TRACE1_0("M4VSS3GPP_intApplyRenderingMode_NV12: Alloc Error");
                free(pImagePlanesTemp[0].pac_data);
                return M4ERR_ALLOC;
            }

            pInPlaneY = pImagePlanesTemp[0].pac_data ;
            pInPlaneUV = pImagePlanesTemp[1].pac_data ;

            memset((void *)pImagePlanesTemp[0].pac_data, Y_PLANE_BORDER_VALUE,
                (pImagePlanesTemp[0].u_height*pImagePlanesTemp[0].u_stride));
            memset((void *)pImagePlanesTemp[1].pac_data, UV_PLANE_BORDER_VALUE,
                (pImagePlanesTemp[1].u_height*pImagePlanesTemp[1].u_stride));

            M4OSA_UInt32 height =
                (pInplane->u_height * pOutplane->u_width) /pInplane->u_width;

            if (height <= pOutplane->u_height) {
                /**
                 * Black borders will be on the top and the bottom side */
                airParams.m_outputSize.m_width = pOutplane->u_width;
                airParams.m_outputSize.m_height = height;
                /**
                 * Number of lines at the top */
                pImagePlanesTemp[0].u_topleft =
                    (M4xVSS_ABS((M4OSA_Int32)(pImagePlanesTemp[0].u_height -
                      airParams.m_outputSize.m_height)>>1)) *
                      pImagePlanesTemp[0].u_stride;
                pImagePlanesTemp[0].u_height = airParams.m_outputSize.m_height;
                pImagePlanesTemp[1].u_topleft =
                    (M4xVSS_ABS((M4OSA_Int32)(pImagePlanesTemp[1].u_height -
                     (airParams.m_outputSize.m_height>>1)))>>1) *
                     pImagePlanesTemp[1].u_stride;
                pImagePlanesTemp[1].u_topleft = ((pImagePlanesTemp[1].u_topleft>>1)<<1);
                pImagePlanesTemp[1].u_height =
                    airParams.m_outputSize.m_height>>1;

            } else {
                /**
                 * Black borders will be on the left and right side */
                airParams.m_outputSize.m_height = pOutplane->u_height;
                airParams.m_outputSize.m_width =
                    (M4OSA_UInt32)((pInplane->u_width * pOutplane->u_height)/pInplane->u_height);

                pImagePlanesTemp[0].u_topleft =
                    (M4xVSS_ABS((M4OSA_Int32)(pImagePlanesTemp[0].u_width -
                     airParams.m_outputSize.m_width)>>1));
                pImagePlanesTemp[0].u_width = airParams.m_outputSize.m_width;
                pImagePlanesTemp[1].u_topleft =
                    (M4xVSS_ABS((M4OSA_Int32)(pImagePlanesTemp[1].u_width -
                     airParams.m_outputSize.m_width))>>1);
                pImagePlanesTemp[1].u_topleft = ((pImagePlanesTemp[1].u_topleft>>1)<<1);
                pImagePlanesTemp[1].u_width = airParams.m_outputSize.m_width;
            }

            /**
             * Width and height have to be even */
            airParams.m_outputSize.m_width =
                (airParams.m_outputSize.m_width>>1)<<1;
            airParams.m_outputSize.m_height =
                (airParams.m_outputSize.m_height>>1)<<1;
            airParams.m_inputSize.m_width =
                (airParams.m_inputSize.m_width>>1)<<1;
            airParams.m_inputSize.m_height =
                (airParams.m_inputSize.m_height>>1)<<1;
            pImagePlanesTemp[0].u_width =
                (pImagePlanesTemp[0].u_width>>1)<<1;
            pImagePlanesTemp[1].u_width =
                (pImagePlanesTemp[1].u_width>>1)<<1;
            pImagePlanesTemp[0].u_height =
                (pImagePlanesTemp[0].u_height>>1)<<1;
            pImagePlanesTemp[1].u_height =
                (pImagePlanesTemp[1].u_height>>1)<<1;

            /**
             * Check that values are coherent */
            if (airParams.m_inputSize.m_height ==
                   airParams.m_outputSize.m_height) {
                airParams.m_inputSize.m_width =
                    airParams.m_outputSize.m_width;
            } else if (airParams.m_inputSize.m_width ==
                          airParams.m_outputSize.m_width) {
                airParams.m_inputSize.m_height =
                    airParams.m_outputSize.m_height;
            }
            pPlaneTemp = pImagePlanesTemp;
        }

        /**
         * Media rendering: Cropping*/
        if (renderingMode == M4xVSS_kCropping) {
            airParams.m_outputSize.m_height = pOutplane->u_height;
            airParams.m_outputSize.m_width = pOutplane->u_width;
            if ((airParams.m_outputSize.m_height *
                 airParams.m_inputSize.m_width)/airParams.m_outputSize.m_width <
                  airParams.m_inputSize.m_height) {
                /* Height will be cropped */
                airParams.m_inputSize.m_height =
                    (M4OSA_UInt32)((airParams.m_outputSize.m_height *
                     airParams.m_inputSize.m_width)/airParams.m_outputSize.m_width);
                airParams.m_inputSize.m_height =
                    (airParams.m_inputSize.m_height>>1)<<1;
                airParams.m_inputCoord.m_y =
                    (M4OSA_Int32)((M4OSA_Int32)((pInplane->u_height -
                     airParams.m_inputSize.m_height))>>1);
            } else {
                /* Width will be cropped */
                airParams.m_inputSize.m_width =
                    (M4OSA_UInt32)((airParams.m_outputSize.m_width *
                     airParams.m_inputSize.m_height)/airParams.m_outputSize.m_height);
                airParams.m_inputSize.m_width =
                    (airParams.m_inputSize.m_width>>1)<<1;
                airParams.m_inputCoord.m_x =
                    (M4OSA_Int32)((M4OSA_Int32)((pInplane->u_width -
                     airParams.m_inputSize.m_width))>>1);
            }
            pPlaneTemp = pOutplane;
        }
           /**
        * Call AIR functions */
        if (M4OSA_NULL == pC->m_air_context) {
            err = M4AIR_create_NV12(&pC->m_air_context, M4AIR_kNV12P);
            if(err != M4NO_ERROR) {
                M4OSA_TRACE1_1("M4VSS3GPP_intApplyRenderingMode_NV12: \
                    M4AIR_create returned error 0x%x", err);
                goto cleanUp;
            }
        }

        err = M4AIR_configure_NV12(pC->m_air_context, &airParams);
        if (err != M4NO_ERROR) {
            M4OSA_TRACE1_1("M4VSS3GPP_intApplyRenderingMode_NV12: \
                Error when configuring AIR: 0x%x", err);
            M4AIR_cleanUp_NV12(pC->m_air_context);
            goto cleanUp;
        }

        err = M4AIR_get_NV12(pC->m_air_context, pInplane, pPlaneTemp);
        if (err != M4NO_ERROR) {
            M4OSA_TRACE1_1("M4VSS3GPP_intApplyRenderingMode_NV12: \
                Error when getting AIR plane: 0x%x", err);
            M4AIR_cleanUp_NV12(pC->m_air_context);
            goto cleanUp;
        }

        if (renderingMode == M4xVSS_kBlackBorders) {
            for (i=0; i<pOutplane[0].u_height; i++) {
                memcpy((void *)pOutPlaneY, (void *)pInPlaneY,
                        pOutplane[0].u_width);
                pInPlaneY += pOutplane[0].u_width;
                pOutPlaneY += pOutplane[0].u_stride;
            }
            for (i=0; i<pOutplane[1].u_height; i++) {
                memcpy((void *)pOutPlaneUV, (void *)pInPlaneUV,
                        pOutplane[1].u_width);
                pInPlaneUV += pOutplane[1].u_width;
                pOutPlaneUV += pOutplane[1].u_stride;
            }

        }
    }

    M4OSA_TRACE1_0("M4VSS3GPP_intApplyRenderingMode_NV12 end");
cleanUp:
    if (renderingMode == M4xVSS_kBlackBorders) {
        for (i=0; i<2; i++) {
            if (pImagePlanesTemp[i].pac_data != M4OSA_NULL) {
                free(pImagePlanesTemp[i].pac_data);
                pImagePlanesTemp[i].pac_data = M4OSA_NULL;
            }
        }
    }
    return err;
}


M4OSA_ERR M4VSS3GPP_intSetNv12PlaneFromARGB888(
    M4VSS3GPP_InternalEditContext *pC, M4VSS3GPP_ClipContext* pClipCtxt)
{

    M4OSA_ERR err= M4NO_ERROR;

    // Allocate memory for NV12 plane
    pClipCtxt->pPlaneYuv =
     (M4VIFI_ImagePlane*)M4OSA_32bitAlignedMalloc(
        2*sizeof(M4VIFI_ImagePlane), M4VS,
        (M4OSA_Char*)"pPlaneYuv");

    if (pClipCtxt->pPlaneYuv == M4OSA_NULL) {
        return M4ERR_ALLOC;
    }

    pClipCtxt->pPlaneYuv[0].u_height =
        pClipCtxt->pSettings->ClipProperties.uiStillPicHeight;
    pClipCtxt->pPlaneYuv[0].u_width =
        pClipCtxt->pSettings->ClipProperties.uiStillPicWidth;
    pClipCtxt->pPlaneYuv[0].u_stride = pClipCtxt->pPlaneYuv[0].u_width;
    pClipCtxt->pPlaneYuv[0].u_topleft = 0;

    pClipCtxt->pPlaneYuv[0].pac_data =
        (M4VIFI_UInt8*)M4OSA_32bitAlignedMalloc(
        pClipCtxt->pPlaneYuv[0].u_height*
        pClipCtxt->pPlaneYuv[0].u_width * 1.5,
        M4VS, (M4OSA_Char*)"imageClip YUV data");
    if (pClipCtxt->pPlaneYuv[0].pac_data == M4OSA_NULL) {
        free(pClipCtxt->pPlaneYuv);
        return M4ERR_ALLOC;
    }

    pClipCtxt->pPlaneYuv[1].u_height = pClipCtxt->pPlaneYuv[0].u_height >>1;
    pClipCtxt->pPlaneYuv[1].u_width = pClipCtxt->pPlaneYuv[0].u_width;
    pClipCtxt->pPlaneYuv[1].u_stride = pClipCtxt->pPlaneYuv[1].u_width;
    pClipCtxt->pPlaneYuv[1].u_topleft = 0;
    pClipCtxt->pPlaneYuv[1].pac_data = (M4VIFI_UInt8*)(
        pClipCtxt->pPlaneYuv[0].pac_data +
        pClipCtxt->pPlaneYuv[0].u_height *
        pClipCtxt->pPlaneYuv[0].u_width);


    err = M4VSS3GPP_internalConvertAndResizeARGB8888toNV12 (
        pClipCtxt->pSettings->pFile,
        pC->pOsaFileReadPtr,
        pClipCtxt->pPlaneYuv,
        pClipCtxt->pSettings->ClipProperties.uiStillPicWidth,
        pClipCtxt->pSettings->ClipProperties.uiStillPicHeight);
    if (M4NO_ERROR != err) {
        free(pClipCtxt->pPlaneYuv[0].pac_data);
        free(pClipCtxt->pPlaneYuv);
        return err;
    }

    // Set the YUV data to the decoder using setoption
    err = pClipCtxt->ShellAPI.m_pVideoDecoder->m_pFctSetOption (
        pClipCtxt->pViDecCtxt,
        M4DECODER_kOptionID_DecYuvData,
        (M4OSA_DataOption)pClipCtxt->pPlaneYuv);  // FIXME: not sure when call this
    if (M4NO_ERROR != err) {
        free(pClipCtxt->pPlaneYuv[0].pac_data);
        free(pClipCtxt->pPlaneYuv);
        return err;
    }

    pClipCtxt->pSettings->ClipProperties.bSetImageData = M4OSA_TRUE;

    // Allocate Yuv plane with effect
    pClipCtxt->pPlaneYuvWithEffect =
        (M4VIFI_ImagePlane*)M4OSA_32bitAlignedMalloc(
        2*sizeof(M4VIFI_ImagePlane), M4VS,
        (M4OSA_Char*)"pPlaneYuvWithEffect");
    if (pClipCtxt->pPlaneYuvWithEffect == M4OSA_NULL) {
        free(pClipCtxt->pPlaneYuv[0].pac_data);
        free(pClipCtxt->pPlaneYuv);
        return M4ERR_ALLOC;
    }

    pClipCtxt->pPlaneYuvWithEffect[0].u_height = pC->ewc.uiVideoHeight;
    pClipCtxt->pPlaneYuvWithEffect[0].u_width = pC->ewc.uiVideoWidth;
    pClipCtxt->pPlaneYuvWithEffect[0].u_stride = pC->ewc.uiVideoWidth;
    pClipCtxt->pPlaneYuvWithEffect[0].u_topleft = 0;

    pClipCtxt->pPlaneYuvWithEffect[0].pac_data =
        (M4VIFI_UInt8*)M4OSA_32bitAlignedMalloc(
        pC->ewc.uiVideoHeight * pC->ewc.uiVideoWidth * 1.5,
        M4VS, (M4OSA_Char*)"imageClip YUV data");
    if (pClipCtxt->pPlaneYuvWithEffect[0].pac_data == M4OSA_NULL) {
        free(pClipCtxt->pPlaneYuv[0].pac_data);
        free(pClipCtxt->pPlaneYuv);
        free(pClipCtxt->pPlaneYuvWithEffect);
        return M4ERR_ALLOC;
    }

    pClipCtxt->pPlaneYuvWithEffect[1].u_height =
        pClipCtxt->pPlaneYuvWithEffect[0].u_height >>1;
    pClipCtxt->pPlaneYuvWithEffect[1].u_width =
        pClipCtxt->pPlaneYuvWithEffect[0].u_width;
    pClipCtxt->pPlaneYuvWithEffect[1].u_stride =
        pClipCtxt->pPlaneYuvWithEffect[1].u_width;
    pClipCtxt->pPlaneYuvWithEffect[1].u_topleft = 0;
    pClipCtxt->pPlaneYuvWithEffect[1].pac_data = (M4VIFI_UInt8*)(
        pClipCtxt->pPlaneYuvWithEffect[0].pac_data +
        pClipCtxt->pPlaneYuvWithEffect[0].u_height *
        pClipCtxt->pPlaneYuvWithEffect[0].u_width);

    err = pClipCtxt->ShellAPI.m_pVideoDecoder->m_pFctSetOption(
        pClipCtxt->pViDecCtxt, M4DECODER_kOptionID_YuvWithEffectContiguous,
        (M4OSA_DataOption)pClipCtxt->pPlaneYuvWithEffect);
    if (M4NO_ERROR != err) {
        free(pClipCtxt->pPlaneYuv[0].pac_data);
        free(pClipCtxt->pPlaneYuv);
        free(pClipCtxt->pPlaneYuvWithEffect);
        return err;
    }

    return M4NO_ERROR;
}


M4OSA_ERR M4VSS3GPP_intRotateVideo_NV12(M4VIFI_ImagePlane* pPlaneIn,
    M4OSA_UInt32 rotationDegree)
{
    M4OSA_ERR err = M4NO_ERROR;
    M4VIFI_ImagePlane outPlane[2];

    if (rotationDegree != 180) {
        // Swap width and height of in plane
        outPlane[0].u_width = pPlaneIn[0].u_height;
        outPlane[0].u_height = pPlaneIn[0].u_width;
        outPlane[0].u_stride = outPlane[0].u_width;
        outPlane[0].u_topleft = 0;
        outPlane[0].pac_data = (M4OSA_UInt8 *)M4OSA_32bitAlignedMalloc(
            (outPlane[0].u_stride*outPlane[0].u_height), M4VS,
            (M4OSA_Char*)("out Y plane for rotation"));
        if (outPlane[0].pac_data == M4OSA_NULL) {
            return M4ERR_ALLOC;
        }

        outPlane[1].u_width = outPlane[0].u_width;
        outPlane[1].u_height = outPlane[0].u_height >> 1;
        outPlane[1].u_stride = outPlane[1].u_width;
        outPlane[1].u_topleft = 0;
        outPlane[1].pac_data = (M4OSA_UInt8 *)M4OSA_32bitAlignedMalloc(
            (outPlane[1].u_stride*outPlane[1].u_height), M4VS,
            (M4OSA_Char*)("out U plane for rotation"));
        if (outPlane[1].pac_data == M4OSA_NULL) {
            free((void *)outPlane[0].pac_data);
            return M4ERR_ALLOC;
        }
    }

    switch(rotationDegree) {
        case 90:
            M4VIFI_Rotate90RightNV12toNV12(M4OSA_NULL, pPlaneIn, outPlane);
            break;

        case 180:
            // In plane rotation, so planeOut = planeIn
            M4VIFI_Rotate180NV12toNV12(M4OSA_NULL, pPlaneIn, pPlaneIn);
            break;

        case 270:
            M4VIFI_Rotate90LeftNV12toNV12(M4OSA_NULL, pPlaneIn, outPlane);
            break;

        default:
            M4OSA_TRACE1_1("invalid rotation param %d", (int)rotationDegree);
            err = M4ERR_PARAMETER;
            break;
    }

    if (rotationDegree != 180) {
        memset((void *)pPlaneIn[0].pac_data, 0,
            (pPlaneIn[0].u_width*pPlaneIn[0].u_height));
        memset((void *)pPlaneIn[1].pac_data, 0,
            (pPlaneIn[1].u_width*pPlaneIn[1].u_height));

        // Copy Y, U and V planes
        memcpy((void *)pPlaneIn[0].pac_data, (void *)outPlane[0].pac_data,
            (pPlaneIn[0].u_width*pPlaneIn[0].u_height));
        memcpy((void *)pPlaneIn[1].pac_data, (void *)outPlane[1].pac_data,
            (pPlaneIn[1].u_width*pPlaneIn[1].u_height));

        free((void *)outPlane[0].pac_data);
        free((void *)outPlane[1].pac_data);

        // Swap the width and height of the in plane
        uint32_t temp = 0;
        temp = pPlaneIn[0].u_width;
        pPlaneIn[0].u_width = pPlaneIn[0].u_height;
        pPlaneIn[0].u_height = temp;
        pPlaneIn[0].u_stride = pPlaneIn[0].u_width;

        pPlaneIn[1].u_width = pPlaneIn[0].u_width;
        pPlaneIn[1].u_height = pPlaneIn[0].u_height >> 1;
        pPlaneIn[1].u_stride = pPlaneIn[1].u_width;

    }

    return err;
}

M4OSA_ERR M4VSS3GPP_intSetNV12Plane(M4VIFI_ImagePlane* planeIn,
                                      M4OSA_UInt32 width, M4OSA_UInt32 height) {

    M4OSA_ERR err = M4NO_ERROR;

    if (planeIn == M4OSA_NULL) {
        M4OSA_TRACE1_0("NULL in plane, error");
        return M4ERR_PARAMETER;
    }

    planeIn[0].u_width = width;
    planeIn[0].u_height = height;
    planeIn[0].u_stride = planeIn[0].u_width;

    planeIn[1].u_width = width;
    planeIn[1].u_height = height/2;
    planeIn[1].u_stride = planeIn[1].u_width;

    return err;
}

