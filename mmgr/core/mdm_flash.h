/* Modem Manager - modem flashing header file
**
** Copyright (C) Intel 2014
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

#ifndef __MMGR_MDM_FLASHING_HEADER__
#define __MMGR_MDM_FLASHING_HEADER__

#define MMGR_FW_OPERATIONS
#include "modem_info.h"
#include "security.h"
#include "bus_events.h"
#include "mmgr.h"

typedef void *mdm_flash_handle_t;

mdm_flash_handle_t *mdm_flash_init(const modem_info_t *mdm_info,
                                   const secure_handle_t *secure,
                                   const bus_ev_hdle_t *bus_ev);

e_mmgr_errors_t mdm_flash_start(mdm_flash_handle_t *hdle);
void mdm_flash_finalize(mdm_flash_handle_t *hdle);

int mdm_flash_get_fd(mdm_flash_handle_t *hdle);
e_modem_fw_error_t mdm_flash_get_verdict(mdm_flash_handle_t *hdle);
int mdm_flash_get_attempts(mdm_flash_handle_t *hdle);
void mdm_flash_reset_attempts(mdm_flash_handle_t *hdle);

void mdm_flash_cancel(mdm_flash_handle_t *hdle);
void mdm_flash_dispose(mdm_flash_handle_t *hdle);

#endif /* __MMGR_MDM_FLASHING_HEADER__ */
