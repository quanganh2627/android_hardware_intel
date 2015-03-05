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

#ifndef __IPT_API_H__
#define __IPT_API_H__

#include <tee_types.h>

/*!
 * @brief stub function included for legacy support
 */
uint32_t ipt_init();

/*!
 * @brief stub function included for legacy support
 */
uint32_t ipt_deinit();

/*!
 * @brief stub function included for legacy support
 */
uint32_t ipt_connect();

/*!
 * @brief stub function included for legacy support
 */
uint32_t ipt_disconnect();

/*!
 * @brief send data to chaabi ipt applet
 */
uint32_t ipt_send_message(uint16_t data_in_length,
                          uint8_t  *in_data,
                          uint16_t *data_out_length,
                          uint8_t  *out_data);

#endif // __IPT_API_H__
