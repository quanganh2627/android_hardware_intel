/* Modem Manager - modem specific header file
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

#ifndef __MMGR_MODEM_SPECIFIC_HEADER__
#define __MMGR_MODEM_SPECIFIC_HEADER__

#define MMGR_FW_OPERATIONS
#include "mmgr.h"
#include "errors.h"
#include "modem_info.h"
#include "security.h"
#include <linux/mdm_ctrl.h>

e_mmgr_errors_t mdm_specific_init(modem_info_t *info);
e_mmgr_errors_t mdm_specific_dispose(modem_info_t *info);
e_mmgr_errors_t mdm_cold_reset(modem_info_t *info);
e_mmgr_errors_t mdm_down(modem_info_t *info);
e_mmgr_errors_t mdm_up(modem_info_t *info);
e_mmgr_errors_t mdm_get_state(int fd_mcd, e_modem_events_type_t *state);
e_mmgr_errors_t mdm_push_fw(const modem_info_t *info, const char *comport,
                            bool ch_sw, const secure_handle_t *sec_hdle,
                            e_modem_fw_error_t *verdict);
e_mmgr_errors_t flash_modem_nvm(modem_info_t *info, char *comport,
                                char *tlv_file, e_modem_nvm_error_t *verdict,
                                int *sub_error_code);
e_mmgr_errors_t set_mcd_poll_states(modem_info_t *info);
e_mmgr_errors_t toggle_flashing_mode(const modem_info_t *info,
                                     bool flashing_mode);

e_mmgr_errors_t mdm_prepare(modem_info_t *info);
e_mmgr_errors_t mdm_prepare_link(modem_info_t *info);

e_mmgr_errors_t mdm_subscribe_start_ev(modem_info_t *info);

e_mmgr_errors_t backup_prod_nvm(modem_info_t *info);

void mup_log(const char *msg, ...);

#endif                          /* __MMGR_MODEM_SPECIFIC_HEADER__ */
