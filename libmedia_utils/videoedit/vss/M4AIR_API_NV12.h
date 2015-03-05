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
*************************************************************************
 * @file   M4AIR_API_NV12.h
 * @brief  Area of Interest Resizer  API
 * @note
*************************************************************************
*/

#ifndef M4AIR_API_NV12_H
#define M4AIR_API_NV12_H
/******************************* INCLUDES *******************************/
#include "M4OSA_Types.h"
#include "M4OSA_Error.h"
#include "M4OSA_CoreID.h"
#include "M4OSA_Mutex.h"
#include "M4OSA_Memory.h"
#include "M4VIFI_FiltersAPI.h"
#include "M4Common_types.h"
#include "M4AIR_API.h"


/**
 ******************************************************************************
 * M4OSA_ERR M4AIR_create_NV12(M4OSA_Context* pContext,M4AIR_InputFormatType inputFormat);
 * @brief     This function initialize an instance of the AIR.
 * @param     pContext:    (IN/OUT) Address of the context to create
 * @param     inputFormat:    (IN) input format type.
 * @return    M4NO_ERROR: there is no error
 * @return    M4ERR_PARAMETER: pContext is M4OSA_NULL (debug only). Invalid formatType
 * @return    M4ERR_ALLOC: No more memory is available
 ******************************************************************************
*/
M4OSA_ERR M4AIR_create_NV12(M4OSA_Context* pContext,M4AIR_InputFormatType inputFormat);


/**
 ******************************************************************************
 * M4OSA_ERR M4AIR_cleanUp_NV12(M4OSA_Context pContext)
 * @brief     This function destroys an instance of the AIR component
 * @param     pContext:    (IN) Context identifying the instance to destroy
 * @return    M4NO_ERROR: there is no error
 * @return    M4ERR_PARAMETER: pContext is M4OSA_NULL (debug only).
 * @return    M4ERR_STATE: Internal state is incompatible with this function call.
 ******************************************************************************
*/
M4OSA_ERR M4AIR_cleanUp_NV12(M4OSA_Context pContext);


/**
 ******************************************************************************
 * M4OSA_ERR M4AIR_configure_NV12(M4OSA_Context pContext, M4AIR_Params* pParams)
 * @brief    This function will configure the AIR.
 * @note    It will set the input and output coordinates and sizes,
 *            and indicates if we will proceed in stripe or not.
 *            In case a M4AIR_get in stripe mode was on going, it will cancel this previous
 *            processing and reset the get process.
 * @param    pContext:                (IN) Context identifying the instance
 * @param    pParams->m_bOutputStripe:(IN) Stripe mode.
 * @param    pParams->m_inputCoord:    (IN) X,Y coordinates of the first valid pixel in input.
 * @param    pParams->m_inputSize:    (IN) input ROI size.
 * @param    pParams->m_outputSize:    (IN) output size.
 * @return    M4NO_ERROR: there is no error
 * @return    M4ERR_ALLOC: No more memory space to add a new effect.
 * @return    M4ERR_PARAMETER: pContext is M4OSA_NULL (debug only).
 * @return    M4ERR_AIR_FORMAT_NOT_SUPPORTED: the requested input format is not supported.
 ******************************************************************************
*/
M4OSA_ERR M4AIR_configure_NV12(M4OSA_Context pContext, M4AIR_Params* pParams);


/**
 ******************************************************************************
 * M4OSA_ERR M4AIR_get_NV12(M4OSA_Context pContext, M4VIFI_ImagePlane* pIn, M4VIFI_ImagePlane* pOut)
 * @brief    This function will provide the requested resized area of interest according to
 *            settings provided in M4AIR_configure.
 * @note    In case the input format type is JPEG, input plane(s)
 *            in pIn is not used. In normal mode, dimension specified in output plane(s) structure
 *            must be the same than the one specified in M4AIR_configure. In stripe mode, only
 *            the width will be the same, height will be taken as the stripe height (typically 16).
 *            In normal mode, this function is call once to get the full output picture. In stripe
 *            mode, it is called for each stripe till the whole picture has been retrieved,and
 *            the position of the output stripe in the output picture is internally incremented
 *            at each step.
 *            Any call to M4AIR_configure during stripe process will reset this one to the
 *              beginning of the output picture.
 * @param    pContext:    (IN) Context identifying the instance
 * @param    pIn:            (IN) Plane structure containing input Plane(s).
 * @param    pOut:        (IN/OUT)  Plane structure containing output Plane(s).
 * @return    M4NO_ERROR: there is no error
 * @return    M4ERR_ALLOC: No more memory space to add a new effect.
 * @return    M4ERR_PARAMETER: pContext is M4OSA_NULL (debug only).
 ******************************************************************************
*/
M4OSA_ERR M4AIR_get_NV12(M4OSA_Context pContext, M4VIFI_ImagePlane* pIn, M4VIFI_ImagePlane* pOut);

#endif

