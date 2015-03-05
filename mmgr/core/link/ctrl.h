/* Modem Manager - link control management source file
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

#ifndef __MMGR_IPC_CTRL_HEADER__
#define __MMGR_IPC_CTRL_HEADER__

#include "errors.h"
#include "tcs_mmgr.h"

typedef void *ctrl_handle_t;

ctrl_handle_t ctrl_init(e_link_t mdm_type, link_ctrl_t *mdm_ctrl,
                        e_link_t cd_type, link_ctrl_t *cd_ctrl);

e_mmgr_errors_t ctrl_dispose(ctrl_handle_t *h);

e_mmgr_errors_t ctrl_on_mdm_down(ctrl_handle_t *h);
e_mmgr_errors_t ctrl_on_mdm_up(ctrl_handle_t *h);
e_mmgr_errors_t ctrl_on_mdm_flash(ctrl_handle_t *h);
e_mmgr_errors_t ctrl_on_cd_ipc_failure(ctrl_handle_t *h);

#endif                          /* __MMGR_IPC_CTRL_HEADER__ */
