/*************************************************************************
**     Copyright (C) 2012 Intel Corporation. All rights reserved.      **
**                                                                     **
** Permission is hereby granted, free of charge, to any person         **
** obtaining a copy of this software and associated documentation      **
** files (the "Software"), to deal in the Software without             **
** restriction, including without limitation the rights to use, copy,  **
** modify, merge, publish, distribute, sublicense, and/or sell copies  **
** of the Software, and to permit persons to whom the Software is      **
** furnished to do so, subject to the following conditions:            **
**                                                                     **
** The above copyright notice and this permission notice shall be      **
** included in all copies or substantial portions of the Software.     **
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,     **
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF  **
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND               **
** NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS **
** BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN  **
** ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN   **
** CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE    **
** SOFTWARE.                                                           **
**                                                                     **
*************************************************************************/

#ifndef _IPTF_INTERNAL_H_
#define _IPTF_INTERNAL_H_

#include "tee_types.h"

#pragma pack(1)

#define SVP_VERSION_1                1
#define SVP_VERSION_2                2
#define SVP_MSG_M1                   1
#define SVP_MSG_M2                   2
#define SVP_MSG_M3                   3
#define SVP_MSG_M4                   4
#define SVP_NONCE_LENGTH             10

#define MIN_S1_LEN                   104

// SVC state machine definition
enum svcstate
{
    SVC_INIT_STATE = 01,
    SVC_M1_STATE,
    SVC_M2_STATE,
    SVC_M3_STATE,
    SVC_M4_STATE,
    SVC_STATE_FINISH
};

enum svcmsg
{
    SVC_M1_MESSAGE = 01,
    SVC_M2_MESSAGE,
    SVC_M3_MESSAGE,
    SVC_M4_MESSAGE,
    SERVER_RESYNC_MESSAGE     //not technically a SVC message, but uses the same message formats
};

enum tlvtype
{
    TLV_TYPE_SIGMA           = 01,
    TLV_TYPE_VENDOR_DATA_SIG,
    TLV_TYPE_VENDOR_DATA_ENC,
    TLV_TYPE_CLIENT_NONCE,
    TLV_TYPE_SIGNATURE,
    TLV_TYPE_OEM_ID,
    TLV_TYPE_IPTMK
};

// SVC message definitions
struct svc_header
{
    uint8_t svc_version;
    uint8_t svc_msg_type;
    uint16_t svc_length;
};

#define SIZE_OF_SVC_HDR    sizeof(struct svc_header)

struct tlv_header
{
    uint8_t tlv_type;
    uint16_t tlv_length;
};

#define SIZE_OF_TLV_HDR    sizeof(struct tlv_header)

#endif //_IPTF_INTERNAL_H_
