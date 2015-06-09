/* Modem Manager - modem events header file
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

#ifndef __MMGR_MODEM_EVENTS_HEADER__
#define __MMGR_MODEM_EVENTS_HEADER__

#include "bus_events.h"
#include "events_manager.h"
#include "mdm_flash.h"

e_mmgr_errors_t modem_events_init(mmgr_data_t *mmgr);
e_mmgr_errors_t ipc_event(mmgr_data_t *mmgr);
e_mmgr_errors_t modem_control_event(mmgr_data_t *mmgr);
e_mmgr_errors_t bus_events(mmgr_data_t *mmgr);
e_mmgr_errors_t reset_modem(mmgr_data_t *mmgr);
e_mmgr_errors_t mdm_start_shtdwn(mmgr_data_t *mmgr);
e_mmgr_errors_t mdm_finalize_shtdwn(mmgr_data_t *mmgr);

void inform_flash_err(const clients_hdle_t *clients,
                      e_modem_fw_error_t flash_err, int attempts, long timer);
void inform_upgrade_err(clients_hdle_t *clients, mdm_flash_upgrade_err_t err);
void core_dump_finalize(mmgr_data_t *mmgr, e_core_dump_state_t state);

#endif                          /* __MMGR_MODEM_EVENTS_HEADER__ */
