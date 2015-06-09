/* Modem Manager - modem mcd header file
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

#ifndef __MMGR_MDM_MCD__
#define __MMGR_MDM_MCD__

#include <stdbool.h>
#include "errors.h"
#include "link.h"
#include "tcs.h"

typedef void *mdm_mcd_hdle_t;

typedef enum e_modem_events_type {
    E_EV_NONE = 0x0,
    E_EV_MODEM_SELF_RESET = 0x1 << 0,
        E_EV_CORE_DUMP = 0x1 << 1,
        E_EV_IPC_READY = 0x1 << 2,
        E_EV_FW_DOWNLOAD_READY = 0x1 << 3,
        E_EV_MODEM_OFF = 0x1 << 4,
        E_EV_CONF_FAILED = 0x1 << 5,
} e_modem_events_type_t;

mdm_mcd_hdle_t *mdm_mcd_init(const mmgr_mcd_t *mcd_cfg,
                             const mdm_core_t *mdm_core, link_hdle_t *link,
                             bool off_allowed, bool ssic_hack);

void mdm_mcd_dispose(mdm_mcd_hdle_t *hdle);

e_mmgr_errors_t mdm_mcd_prepare_link(const mdm_mcd_hdle_t *hdle);
void mdm_mcd_finalize_link(const mdm_mcd_hdle_t *hdle);

e_mmgr_errors_t mdm_mcd_unregister(mdm_mcd_hdle_t *hdle, int events);
e_mmgr_errors_t mdm_mcd_register(mdm_mcd_hdle_t *hdle, int events, bool force);

e_modem_events_type_t mdm_mcd_get_state(const mdm_mcd_hdle_t *hdle);

e_mmgr_errors_t mdm_mcd_up(const mdm_mcd_hdle_t *hdle);
e_mmgr_errors_t mdm_mcd_down(const mdm_mcd_hdle_t *hdle);
e_mmgr_errors_t mdm_mcd_off(const mdm_mcd_hdle_t *hdle);
e_mmgr_errors_t mdm_mcd_cold_reset(const mdm_mcd_hdle_t *hdle);

int mdm_mcd_get_fd(const mdm_mcd_hdle_t *hdle);

bool mdm_mcd_is_ipc_ready_present(const mdm_mcd_hdle_t *hdle);

#endif
