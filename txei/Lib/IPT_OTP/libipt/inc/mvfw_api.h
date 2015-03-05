/**********************************************************************
* Copyright (C) 2011 Intel Corporation. All rights reserved.

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

/*!
 * NOTE: Don't include this file. It is recommended to include sep_ipt.h
 */
#ifndef __MVFW_API_H__
#define __MVFW_API_H__

#include <inttypes.h>


/*!
 * Structs
 */
struct mvfw_req_header
{
    uint32_t api_version;
    uint16_t message_type;
    uint16_t buffer_length;
};

struct mvfw_resp_header
{
    uint32_t api_version;
    uint16_t message_type;
    uint16_t buffer_length;
    uint16_t status;
};

/*!
 * Functions
 */

/*!
 * @brief stub function included for legacy support
 */
uint32_t mvfw_sep_init();

/*!
 * @brief sends mv_init command to chaabi ipt applet
 */
uint32_t mvsepfw_initialize();

/*!
 * @brief sends mv_deinit command to chaabi ipt applet
 */
uint32_t mvsepfw_deinitialize();

/*!
 * @brief sends mv_connect commmand to chaabi ipt applet
 */
uint32_t mvsepfw_connect();

/*!
 * @brief sends mv_disconnect commmand to chaabi ipt applet
 */
uint32_t mvsepfw_disconnect();

/*!
 * @brief sends mediavault message to chaabi ipt applet
 */
uint32_t mvsepfw_sendmessage(const uint8_t  *in_data,
                             const uint32_t data_in_length,
                             uint8_t *data_out,
                             uint32_t *out_data_length);

#endif // __MVFW_API_H__
