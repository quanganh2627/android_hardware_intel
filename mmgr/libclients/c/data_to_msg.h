/* Modem Manager - data to message header file
**
** Copyright (C) Intel 2013
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

#ifndef __MMGR_DATA_TO_MSG_HEADER__
#define __MMGR_DATA_TO_MSG_HEADER__

#define MMGR_FW_OPERATIONS
#include "mmgr_cli.h"
#include "msg_format.h"
#include "errors.h"

e_mmgr_errors_t set_msg_name(msg_t *msg, mmgr_cli_event_t *request);
e_mmgr_errors_t set_msg_filter(msg_t *msg, mmgr_cli_event_t *request);
e_mmgr_errors_t set_msg_restart(msg_t *msg, mmgr_cli_event_t *request);
e_mmgr_errors_t set_msg_recovery(msg_t *msg, mmgr_cli_event_t *request);

#endif                          /* __MMGR_DATA_TO_MSG_HEADER__ */
