/* Modem Manager client library - utils header file
**
** Copyright (C) Intel 2012
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
*/

#ifndef __MMGR_CLI_UTILS_H__
#define __MMGR_CLI_UTILS_H__

#include "common.h"
#define DEF_MMGR_RESPONSIVE_TIMEOUT 20  /* in seconds */

typedef enum e_send_method {
    E_SEND_SINGLE,
    E_SEND_THREADED,
} e_send_method_t;

bool is_connected(mmgr_lib_context_t *p_lib);
e_err_mmgr_cli_t send_msg(mmgr_lib_context_t *p_lib,
                          const mmgr_cli_requests_t *request,
                          e_send_method_t method, int timeout);
e_err_mmgr_cli_t read_events(mmgr_lib_context_t *p_lib);

e_err_mmgr_cli_t cli_connect(mmgr_lib_context_t *p_lib);
e_err_mmgr_cli_t cli_disconnect(mmgr_lib_context_t *p_lib);

e_mmgr_events_t is_request_rejected(mmgr_lib_context_t *p_lib);

e_err_mmgr_cli_t handle_disconnection(mmgr_lib_context_t *p_lib);

#endif                          /* __MMGR_CLI_UTILS_H__ */
