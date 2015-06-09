/* Modem Manager - modem link header file
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

#ifndef __MMGR_LINK_HEADER__
#define __MMGR_LINK_HEADER__

#include <stdbool.h>
#include "bus_events.h"
#include "errors.h"
#include "tcs_mmgr.h"

typedef void *link_hdle_t;

link_hdle_t *link_init(const mmgr_mdm_link_t *links, const mcdr_info_t *mcdr,
                       const bus_ev_hdle_t *bus_ev, bool ssic_hack);
void link_dispose(link_hdle_t *hdle);

e_mmgr_errors_t link_on_mdm_down(const link_hdle_t *hdle);
e_mmgr_errors_t link_on_mdm_reset(const link_hdle_t *hdle, int delay);
e_mmgr_errors_t link_on_mdm_flash(const link_hdle_t *hdle);
e_mmgr_errors_t link_on_mdm_flash_complete(const link_hdle_t *hdle);
e_mmgr_errors_t link_on_mdm_up(const link_hdle_t *hdle);

e_mmgr_errors_t link_on_cd(const link_hdle_t *hdle);
e_mmgr_errors_t link_on_cd_failure(const link_hdle_t *hdle);
e_mmgr_errors_t link_on_cd_complete(const link_hdle_t *hdle);

const char *link_get_flash_ebl_interface(const link_hdle_t *hdle);
const char *link_get_flash_fw_interface(const link_hdle_t *hdle);
const char *link_get_bb_interface(const link_hdle_t *hdle);

e_link_t link_get_flash_ebl_type(const link_hdle_t *hdle);
e_link_t link_get_flash_fw_type(const link_hdle_t *hdle);
e_link_t link_get_bb_type(const link_hdle_t *hdle);
e_link_t link_get_cd_type(const link_hdle_t *hdle);

int link_get_ebl_baudrate(const link_hdle_t *hdle);

#endif /* __MMGR_LINK_HEADER__ */
