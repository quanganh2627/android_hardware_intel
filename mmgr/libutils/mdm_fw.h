/* Modem Manager - modem fw header file
**
** ** Copyright (C) Intel 2014
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

#ifndef __MODEM_FW_HEADER__
#define __MODEM_FW_HEADER__

#include "errors.h"
#include "tcs_mmgr.h"

typedef void *mdm_fw_hdle_t;

mdm_fw_hdle_t *mdm_fw_init(int inst_id, const mdm_info_t *mdm,
                           const mmgr_fw_t *fw_cfg);

void mdm_fw_dispose(mdm_fw_hdle_t *hdle);

e_mmgr_errors_t mdm_fw_create_folders(const mdm_fw_hdle_t *hdle);
const char *mdm_fw_get_fw_path(const mdm_fw_hdle_t *hdle);
const char *mdm_fw_get_fw_package_path(const mdm_fw_hdle_t *hdle);
const char *mdm_fw_get_rnd_path(const mdm_fw_hdle_t *hdle);
const char *mdm_fw_get_blob_hash_path(const mdm_fw_hdle_t *hdle);
const char *mdm_fw_get_runtime_path(const mdm_fw_hdle_t *hdle);
const char *mdm_fw_get_factory_folder(const mdm_fw_hdle_t *hdle);
const char *mdm_fw_get_nvm_dyn_path(const mdm_fw_hdle_t *hdle);
const char *mdm_fw_get_nvm_sta_path(const mdm_fw_hdle_t *hdle);

const char *mdm_fw_dbg_get_miu_folder(const mdm_fw_hdle_t *hdle);
const char *mdm_fw_dbg_get_miu_fw_path(const mdm_fw_hdle_t *hdle);

e_mmgr_errors_t mdm_fw_backup_calib(const mdm_fw_hdle_t *hdle);

const tlvs_info_t *mdm_fw_get_tlvs(const mdm_fw_hdle_t *hdle);

#endif /* __MODEM_FW_HEADER__ */
