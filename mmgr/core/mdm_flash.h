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
#include <stdbool.h>
#include "security.h"
#include "key.h"
#include "link.h"
#include "mdm_fw.h"
#include "mmgr.h"

typedef enum mdm_flash_err {
    MDM_UPDATE_ERR_NONE = 0,
    MDM_UPDATE_ERR_FLASH = 0x1 << 0,
        MDM_UPDATE_ERR_TLV = 0x1 << 1,
} mdm_flash_upgrade_err_t;

typedef void *mdm_flash_handle_t;

mdm_flash_handle_t *mdm_flash_init(const mdm_info_t *mdm_info,
                                   const mdm_fw_hdle_t *fw,
                                   const secure_handle_t *secure,
                                   const key_hdle_t *keys, link_hdle_t *link);

void mdm_flash_dispose(mdm_flash_handle_t *hdle);

e_mmgr_errors_t mdm_flash_prepare(mdm_flash_handle_t *hdle);
e_mmgr_errors_t mdm_flash_start(mdm_flash_handle_t *hdle);
void mdm_flash_finalize(mdm_flash_handle_t *hdle);
void mdm_flash_cancel(mdm_flash_handle_t *hdle);

const char *mdm_flash_streamline(mdm_flash_handle_t *hdle,
                                 mmgr_cli_nvm_update_result_t *err);

int mdm_flash_get_fd(const mdm_flash_handle_t *hdle);

bool mdm_flash_is_required(const mdm_flash_handle_t *hdle);

e_modem_fw_error_t mdm_flash_get_flashing_err(mdm_flash_handle_t *hdle);
mdm_flash_upgrade_err_t mdm_flash_get_upgrade_err(const mdm_flash_handle_t *hdle);
int mdm_flash_get_attempts(const mdm_flash_handle_t *hdle);
void mdm_flash_reset_attempts(mdm_flash_handle_t *hdle);

#endif /* __MMGR_MDM_FLASHING_HEADER__ */
