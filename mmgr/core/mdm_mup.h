/* Modem Manager - modem mup header file
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

#ifndef __MDM_MUP_HEADER__
#define __MDM_MUP_HEADER__

#define MMGR_FW_OPERATIONS
#include <stdbool.h>
#include "errors.h"
#include "mmgr.h"
#include "link.h"
#include "security.h"
#include "tcs_config.h"

typedef void *mdm_mup_hdle_t;

mdm_mup_hdle_t *mdm_mup_init(const char *mdm_name, const char *streamline_dlc,
                             const char *rnd, const link_hdle_t *link,
                             const secure_handle_t *sec_hdle);
void mdm_mup_dispose(mdm_mup_hdle_t *hdle);

e_mmgr_errors_t mdm_mup_toggle_flashing(const mdm_mup_hdle_t *hdle, bool mode);

e_mmgr_errors_t mdm_mup_package(const mdm_mup_hdle_t *hdle,
                                const char *nvm_folder, const char *input,
                                const char *output);
e_modem_fw_error_t mdm_mup_push_fw(const mdm_mup_hdle_t *hdle, const char *fw,
                                   const char *eb_port, const char *fls_port);

mmgr_cli_nvm_update_result_t mdm_mup_push_tlv(const mdm_mup_hdle_t *hdle,
                                              const char *filename);

#endif
