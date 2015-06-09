/**********************************************************************
 * Copyright 2014 (c) Intel Corporation. All rights reserved.

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

#ifndef __LIBKEYMASTER_API_H__
#define __LIBKEYMASTER_API_H__

#include "intelkeymaster_firmware_api.h"
#include "android_heci_agent.h"

#define MAX_HECI_MSG_SIZE             3*1024	//TODO: determine value of this
typedef enum {
    SEP_KEYMASTER_SUCCESS = 0,
    SEP_KEYMASTER_BAD_PARAMETER,
    SEP_KEYMASTER_CMD_BUFFER_TOO_BIG,
    SEP_KEYMASTER_RSP_BUFFER_TOO_SMALL,
    SEP_KEYMASTER_HECI_CONNECT_FAILED,
    SEP_KEYMASTER_OUT_OF_MEMORY,
    SEP_KEYMASTER_HECI_SNDRCV_FAILED,
    SEP_KEYMASTER_FAILURE
} sep_keymaster_return_t;

/**
 * Send a keymaster command to sep, and waits for a response
 * @param cmd_buffer :		Pointer to command buffer
 * @param buffer_length :	Number of command bytes
 * @param rsp_buffer :		Pointer to response buffer
 * @param rsp_length :		Upon input, size of response buffer
 *                          Upon return, actual size of response
 * @return SEP_KEYMASTER_CMD_SUCCESS if command was sent and
 * response received; non zero value if there was a failure
 */
sep_keymaster_return_t sep_keymaster_send_cmd(const uint8_t * cmd_buffer,
        uint32_t cmd_length, uint8_t * rsp_buffer, uint32_t * rsp_length);

void swap_byte_order(uint8_t * buf, uint32_t buf_len);
void print_buf(char *prompt, uint8_t buf[], uint32_t size);

sep_keymaster_return_t
generate_keypair_cmd_buf(intel_keymaster_firmware_cmd_t * lib_cmd,
        ANDROID_HECI_KEYMASTER_CMD_RSA_GEN_KEY_REQUEST * fw_cmd);
sep_keymaster_return_t
import_keypair_cmd_buf(intel_keymaster_firmware_cmd_t * lib_cmd,
        ANDROID_HECI_KEYMASTER_CMD_RSA_IMPORT_KEY_REQUEST * fw_cmd);
sep_keymaster_return_t get_public_keypair_cmd_buf(
        intel_keymaster_firmware_cmd_t * lib_cmd,
        ANDROID_HECI_KEYMASTER_CMD_RSA_GET_PUBLIC_KEY_REQUEST * fw_cmd);
sep_keymaster_return_t sign_data_cmd_buf(
        intel_keymaster_firmware_cmd_t * lib_cmd,
        ANDROID_HECI_KEYMASTER_CMD_RSA_SIGN_DATA_NOPAD_REQUEST * fw_cmd);
sep_keymaster_return_t verify_data_cmd_buf(
        intel_keymaster_firmware_cmd_t * lib_cmd,
        ANDROID_HECI_KEYMASTER_CMD_RSA_VERIFY_DATA_NOPAD_REQUEST * fw_cmd);
sep_keymaster_return_t generate_cmd_buf(
        intel_keymaster_firmware_cmd_t * lib_cmd, uint8_t ** request,
        uint32_t * fw_request_len);
sep_keymaster_return_t get_caps();
sep_keymaster_return_t send_req_to_fw(const uint8_t * req,
        const uint32_t req_len, const uint8_t * resp, const uint32_t resp_len);
sep_keymaster_return_t set_response_cmd_id(const uint32_t rsp_id,
        uint32_t * fw_rsp_id);
sep_keymaster_return_t set_response_status_and_data(const uint8_t * response,
        intel_keymaster_firmware_rsp_t * caller_rsp, uint32_t * rsp_length,
        const uint32_t caller_rsp_data_buf_size);

#endif				/* __LIBKEYMASTER_API_H__ */
