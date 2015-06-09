/*++
   INTEL CONFIDENTIAL
   Copyright (c) 2013 Intel Corporation. All Rights Reserved.

   The source code contained or described herein and all documents related
   to the source code ("Material") are owned by Intel Corporation or its
   suppliers or licensors. Title to the Material remains with Intel Corporation
   or its suppliers and licensors. The Material contains trade secrets and
   proprietary and confidential information of Intel or its suppliers and
   licensors. The Material is protected by worldwide copyright and trade secret
   laws and treaty provisions. No part of the Material may be used, copied,
   reproduced, modified, published, uploaded, posted, transmitted, distributed,
   or disclosed in any way without Intel's prior express written permission.

   No license under any patent, copyright, trade secret or other intellectual
   property right is granted to or conferred upon you by disclosure or delivery
   of the Materials, either expressly, by implication, inducement, estoppel or
   otherwise. Any license under such intellectual property rights must be
   express and approved by Intel in writing.
--*/
/**
 * @file acds_cmd_structs.h
 */

#ifndef _ACDS_CMD_STRUCTS_H_
#define _ACDS_CMD_STRUCTS_H_

typedef uint32_t UINT32;
typedef uint16_t UINT16;
typedef uint8_t UINT8;
typedef int32_t INT32;

typedef struct acds_hdr_req
{
    //UINT32 version;
    UINT16  main_opcode;
    UINT16 sub_opcode;
}ACDS_MESSAGE_REQUEST_HEADER;

typedef struct acds_hdr_resp
{
    //UINT32 version;
    UINT16  main_opcode;
    UINT16 sub_opcode;
    INT32 status;
}ACDS_MESSAGE_RESPONSE_HEADER;


#endif






