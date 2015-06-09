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

typedef enum {
    SEP_KEYMASTER_SUCCESS = 0,
    SEP_KEYMASTER_BAD_PARAMETER,
    SEP_KEYMASTER_CMD_BUFFER_TOO_BIG,
    SEP_KEYMASTER_RSP_BUFFER_TOO_SMALL,
    SEP_KEYMASTER_CC_INIT_FAILURE,
    SEP_KEYMASTER_FAILURE
} sep_keymaster_return_t;

#ifdef __cplusplus
extern "C" {
#endif

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
sep_keymaster_return_t sep_keymaster_send_cmd(const uint8_t * const cmd_buffer,
                                              uint32_t cmd_length,
                                              uint8_t * const rsp_buffer,
                                              uint32_t * const rsp_length);

#ifdef __cplusplus
}
#endif

#endif /* __LIBKEYMASTER_API_H__ */
