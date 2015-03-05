/**********************************************************************
 * Copyright 2009 - 2012 (c) Intel Corporation. All rights reserved.
 * (Contributions by Discretix Technologies Ltd)

 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at

 * http://www.apache.org/licenses/LICENSE-2.0

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 **********************************************************************/
 
 #ifndef HOST_OP_CODE_H
#define HOST_OP_CODE_H
/*
   *  Object          : host_op_code.h
   *  State           :  %state%
   *  Creation date   :  Wed Nov 17 17:26:15 2004
   *  Last modified   :  %modify_time%
   */
  /** @file
   *  \brief this file containes the declerations of the HOST-SEP and vice versa op codes..
   *         
   *
   *  \version host_op_code.h#1:csrc:3
   *  \author avis
   */

/************* Include Files ****************/


/************************ Defines ******************************/


/************************ Enums ******************************/

typedef enum
{
    DX_SEP_HOST_SEP_PROTOCL_HOST_INIT_OP_CODE					= 0x1,

	/*CRYS*/
	DX_SEP_HOST_SEP_PROTOCOL_HOST_CRYS_FIRST_OP_CODE			= 0x2,
	DX_SEP_HOST_SEP_PROTOCOL_HOST_CRYS_LAST_OP_CODE				= 0x150,

	/*MNG*/
	DX_SEP_HOST_SEP_PROTOCOL_HOST_MNG_FIRST_OP_CODE				= 0x180,
	DX_SEP_HOST_SEP_PROTOCOL_HOST_MNG_LAST_OP_CODE				= 0x1FF,

    
	/* FLOW codes */
	DX_SEP_HOST_SEP_PROTOCOL_HOST_FLOW_FIRST_OP_CODE				= 0x200,
	DX_SEP_HOST_SEP_PROTOCOL_HOST_FLOW_LAST_OP_CODE				= 0x220,

	/* Private mng */
	DX_SEP_HOST_SEP_PROTOCOL_HOST_PRIVATE_PROJECT_FIRST_OP_CODE	= 0x400,
	DX_SEP_HOST_SEP_PROTOCOL_HOST_PRIVATE_PROJECT_LAST_OP_CODE	= 0x4ff,
    
    DX_SEP_HOST_SEP_PROTOCOL_HOST_SST_FIRST_OP_CODE				= 0x500,    /*1280*/
    DX_SEP_HOST_SEP_PROTOCOL_HOST_SST_LAST_OP_CODE				= 0x5ff,    /*1535*/
    
    DX_SEP_HOST_SEP_PROTOCOL_HOST_NVS_FIRST_OP_CODE             = 0x600,    /*1536*/
    DX_SEP_HOST_SEP_PROTOCOL_HOST_NVS_LAST_OP_CODE              = 0x60f,    /*1551*/

	DX_SEP_HOST_SEP_PROTOCOL_HOST_WMDRM_FIRST_OP_CODE			= 0x700,/*1792*/
	DX_SEP_HOST_SEP_PROTOCOL_HOST_WMDRM_LAST_OP_CODE			= 0x7FF,/*2047*/

    DX_SEP_HOST_SEP_PROTOCOL_HOST_CSI_FIRST_OP_CODE				= 0x800,
    DX_SEP_HOST_SEP_PROTOCOL_HOST_CSI_LAST_OP_CODE				= 0x8FF,

	DX_SEP_HOST_SEP_PROTOCOL_HOST_KMNG_FIRST_OP_CODE			= 0x900,
	DX_SEP_HOST_SEP_PROTOCOL_HOST_KMNG_LAST_OP_CODE				= 0x9FF,

	DX_SEP_HOST_SEP_PROTOCOL_HOST_TLK_ODRM_FIRST_OP_CODE		= 0xA01,    /*2561*/
    DX_SEP_HOST_SEP_PROTOCOL_HOST_TLK_ODRM_LAST_OP_CODE			= 0xA10,    /*2576*/

    DX_SEP_HOST_SEP_PROTOCOL_HOST_TLK_CERT_FIRST_OP_CODE		= 0xA11,    /*2577*/
    DX_SEP_HOST_SEP_PROTOCOL_HOST_TLK_CERT_LAST_OP_CODE			= 0xA30,    /*2892*/

    DX_SEP_HOST_SEP_PROTOCOL_HOST_TLK_SCLK_FIRST_OP_CODE		= 0xA31,    /*2609*/
    DX_SEP_HOST_SEP_PROTOCOL_HOST_TLK_SCLK_LAST_OP_CODE			= 0xA40,    /*2624*/

	DX_SEP_HOST_SEP_PROTOCOL_HOST_EXT_APP_FIRST_OP_CODE			= 0xB00,    /*2816*/
	DX_SEP_HOST_SEP_PROTOCOL_HOST_EXT_APP_LAST_OP_CODE			= 0xBFF,    /*3071*/

	DX_SEP_HOST_SEP_PROTOCOL_HOST_VOS_APP_FIRST_OP_CODE		= 0xC00,
	DX_SEP_HOST_SEP_PROTOCOL_HOST_VOS_APP_LAST_OP_CODE			= 0xC20,
	
	DX_SEP_HOST_SEP_PROTOCL_MAX_OP_CODE,
	DX_SEP_HOST_SEP_GENERAL_CODE = 0x7FFFFFFF 
    
}DX_SEP_HOST_SEP_OP_CODE_type;
/************************ Typedefs ******************************/


/************************ Global Data ******************************/


/************* Private function prototypes ****************/


#endif

