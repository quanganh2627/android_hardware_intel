/* Modem Manager - modem info header file
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
#ifndef _MMGR_MODEM_INFO_HEADER__
#define _MMGR_MODEM_INFO_HEADER__

#include <stdbool.h>
#include "modem_update.h"
#include "tcs_mmgr.h"
#include "core_dump.h"

#define HANDSHAKE_AFTER_CD_RETRIES_COUNT 12

typedef enum e_mdm_wakeup_cfg {
    E_MDM_WAKEUP_UNKNOWN,
    E_MDM_WAKEUP_INBAND,
    E_MDM_WAKEUP_OUTBAND,
} e_mdm_wakeup_cfg_t;

/* Callback used to parse an AT response string */
typedef int (*PFN_PARSE_RESP) (void *ctx, const char *pszResp, size_t *len);

int read_cd_logs(int fd_tty, int fd_fs, PFN_PARSE_RESP parseFct);
bool generate_timestamp(char *timestamp, int size);

e_mmgr_errors_t switch_to_mux(int *fd_tty, const char *mdm_bb_path,
                              e_link_t mdm_bb_type, const mux_t *mux,
                              const char *sanity_check_dlc,
                              e_mdm_wakeup_cfg_t *wakeup_cfg);

#endif                          /* _MMGR_MODEM_INFO_HEADER__ */
