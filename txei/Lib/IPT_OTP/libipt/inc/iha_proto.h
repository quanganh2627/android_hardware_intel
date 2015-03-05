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

#ifndef _IHA_PROTO_H
#define _IHA_PROTO_H

#include <stdint.h>
#include "ipt_util.h"
#include "tee_types.h"

#pragma pack(1)

#define BYTES_TO_DWORDS(x)                  ((x + sizeof(uint32_t) - 1) / sizeof(uint32_t))
#define BYTES_TO_DWORD_ALIGNED_BYTES(x)     (BYTES_TO_DWORDS(x) * sizeof(uint32_t))

// Message type definitions
#define IHA_MSGTYPE_NULL_MSG                0
#define IHA_MSGTYPE_INIT_REQ                1
#define IHA_MSGTYPE_INIT_RESP               2
#define IHA_MSGTYPE_START_PROV_REQ          3
#define IHA_MSGTYPE_START_PROV_RESP         4
#define IHA_MSGTYPE_END_PROV_REQ            5
#define IHA_MSGTYPE_END_PROV_RESP           6
#define IHA_MSGTYPE_SEND_DATA_REQ           7
#define IHA_MSGTYPE_SEND_DATA_RESP          8
#define IHA_MSGTYPE_RECEIVE_DATA_REQ        9
#define IHA_MSGTYPE_RECEIVE_DATA_RESP       10
// #define IHA_MSGTYPE_CONTROL_REQ          11
// #define IHA_MSGTYPE_CONTROL_RESP         12
#define IHA_MSGTYPE_GET_OTP_REQ             13
#define IHA_MSGTYPE_GET_OTP_RESP            14
#define IHA_MSGTYPE_OTPS_STATUS_REQ         15
#define IHA_MSGTYPE_OTPS_STATUS_RESP        16
#define IHA_MSGTYPE_GET_CAPS_REQ            17
#define IHA_MSGTYPE_GET_CAPS_RESP           18
#define IHA_MSGTYPE_EPID_PROV_REQ           19
#define IHA_MSGTYPE_EPID_PROV_RESP          20
#define IHA_MSGTYPE_PROCESS_SVP_REQ         21
#define IHA_MSGTYPE_PROCESS_SVP_RESP        22
#define IHA_MSGTYPE_GET_SVP_REQ             23
#define IHA_MSGTYPE_GET_SVP_RESP            24
// IPT2.0 begin change
#define IHA_MSGTYPE_SAR_REQ                 25
#define IHA_MSGTYPE_SAR_RESP                26

// Cloud TEE extensions
#define IHA_MSGTYPE_CTEE_PREPARE            50
#define IHA_MSGTYPE_CTEE_PROCESS            51
// Message Types for WYS
#define IHA_MSGTYPE_WYS_SAR_SEND_REQ        100
#define IHA_MSGTYPE_WYS_SAR_SEND_RESP       101

// Data type definitions for WYS
#define IPT_WYS_DATATYPE_LOCAL_IMAGE        200 //0xFFFF0001 // same as DAL WYS_STANDARD_COMMAND_ID
#define IPT_WYS_DATATYPE_LOCAL_SUBMIT       201
#define IPT_WYS_DATATYPE_CHANNEL_SETUP      250
#define SEND_DATA_TYPE_ALGO_SELECT          500 //if Send Data is invoked for algo select
#define SAR_LEGACY_GET_OTP_WITH_TOKEN       501 // GetOtp request with token supplied
#define SAR_LEGACY_GET_OTP_WITHOUT_TOKEN    502 // GetOtp request without token
#define SAR_LEGACY_GET_OTP_STATUS           503 // GetOtp status request
#define SAR_GET_OTP_WITH_TOKEN              504 // New SAR based GetOtp request with token
#define SAR_GET_OTP_WITHOUT_TOKEN           505 // New SAR based GetOtp request w/o token
#define SAR_GET_OTP_STATUS                  506 // New SAR based GetOtpStatus request
#define SAR_WYS_STORE_PIN                   507 // SAR based WSYS store pin request
#define SAR_PROCESS_RESYNC_MSG              508 // Command to process a resync message from
                                                // the server

// IPT2.0 end change

// OTP API Version
#define IHA_API_VERSION_MAJOR  1
#define IHA_API_VERSION_MINOR  0
#define IHA_API_VERSION        ((IHA_API_VERSION_MAJOR << 16) | (IHA_API_VERSION_MINOR))

// Common message headers
struct s_msg_hdr
{
    uint32_t api_version;
    uint16_t message_type;
    uint16_t buf_length;            // length of the remainder of the message
};

// This is used for response validation
struct s_msg_hdr_resp_base
{
    struct s_msg_hdr header;
    uint32_t resp_status;
};

#define SIZE_OF_MSG_HDR_RESP_BASE    sizeof(struct s_msg_hdr_resp_base)

// Start Provisioning
struct s_msg_start_prov_req
{
    struct s_msg_hdr header;
};

struct s_msg_start_prov_resp
{
    struct s_msg_hdr header;
    uint32_t resp_status;
    uint32_t h_session_handle;
};

// End Provisioning
struct s_msg_end_prov_req
{
    struct s_msg_hdr header;
    uint32_t h_session_handle;
    uint16_t expected_enc_token_length;            // expected length of output encrypted Token
};

struct s_msg_end_prov_resp
{
    struct s_msg_hdr header;
    uint32_t resp_status;
    uint16_t enc_token_length;
    uint8_t enc_token[1];
};

// Process SVP Message
struct s_msg_process_svp_req
{
    struct s_msg_hdr header;
    uint32_t h_session_handle;
    uint16_t in_data_length;
    uint8_t in_data[OTP_MAX_PROCESS_SVP_SIZE];
};

#define SIZE_OF_PROCESS_SVP_REQ_HDR    (sizeof(struct s_msg_process_svp_req) - \
                                       OTP_MAX_PROCESS_SVP_SIZE)

struct s_msg_process_svp_resp
{
    struct s_msg_hdr header;
    uint32_t resp_status;
};

// Get SVP Message
struct s_msg_get_svp_req
{
    struct s_msg_hdr header;
    uint32_t h_session_handle;
    uint16_t expected_out_data_length;            // expected length of output data
};

struct s_msg_get_svp_resp
{
    struct s_msg_hdr header;
    uint32_t resp_status;
    uint16_t out_data_length;
    uint8_t out_data[OTP_MAX_GET_SVP_SIZE];
};

#define SIZE_OF_GET_SVP_RESP_HDR    (sizeof(struct s_msg_get_svp_resp) - OTP_MAX_GET_SVP_SIZE)

// Get OTP
struct s_msg_get_otp_req
{
    struct s_msg_hdr header;
    uint32_t h_session_handle;
    uint16_t expected_otp_length;            // expected length of output OTP
    uint16_t expected_enc_token_length;            // expected length of output encrypted Token
    uint16_t enc_token_length;
    uint16_t vendor_data_length;
    uint8_t data[1];     // encToken first, then vendorData
};

struct s_msg_get_otp_resp
{
    struct s_msg_hdr header;
    uint32_t resp_status;
    uint16_t otp_length;
    uint16_t enc_token_length;
    uint8_t data[1];     // otp comes first in this buffer
};

// Get Caps from OTPF
struct s_msg_get_caps_req
{
    struct s_msg_hdr header;
    uint16_t type;
    uint16_t expected_caps_length;            // expected length of output data
};

struct s_msg_get_caps_resp
{
    struct s_msg_hdr header;
    uint32_t resp_status;
    uint16_t caps_length;
    uint8_t caps[1];
};

// IPT 2.0 begin change
struct s_msg_sar_req
{
    struct s_msg_hdr header;
    uint32_t handle;
    uint16_t data_type;
    uint16_t expected_out_data_length;            // expected length of output data
    uint16_t in_data_length;
    uint8_t in_data[1];
};

struct s_msg_sar_resp
{
    struct s_msg_hdr header;
    uint32_t resp_status;
    uint16_t out_data_length;
    uint8_t out_data[1];
};
// IPT 2.0 end change

union s_msg_req
{
    struct s_msg_get_otp_req msg_get_otp_req;
    struct s_msg_get_svp_req msg_get_svp_req;
    struct s_msg_process_svp_req msg_process_svp_req;
    struct s_msg_end_prov_req msg_end_prov_req;
    struct s_msg_sar_req msg_sar_req;
    struct s_msg_get_caps_req msg_get_caps_req;
};

#define SIZE_OF_UNION_MSG_REQ    sizeof(union s_msg_req)
#define DWORD_ALIGNED_SIZE_OF_UNION_MSG_REQ    BYTES_TO_DWORD_ALIGNED_BYTES(SIZE_OF_UNION_MSG_REQ)

union s_msg_resp
{
    struct s_msg_hdr_resp_base msg_base_resp;
    struct s_msg_get_otp_resp msg_get_otp_resp;
    struct s_msg_start_prov_resp msg_start_prov_resp;
    struct s_msg_get_svp_resp msg_get_svp_resp;
    struct s_msg_process_svp_resp msg_process_svp_resp;
    struct s_msg_end_prov_resp msg_end_prov_resp;
    struct s_msg_sar_resp msg_sar_resp;
    struct s_msg_get_caps_resp msg_get_caps_resp;
};

#define SIZE_OF_UNION_MSG_RESP    sizeof(union s_msg_resp)
#define DWORD_ALIGNED_SIZE_OF_UNION_MSG_RESP    BYTES_TO_DWORD_ALIGNED_BYTES(SIZE_OF_UNION_MSG_RESP)

#endif // _IHA_PROTO_H
