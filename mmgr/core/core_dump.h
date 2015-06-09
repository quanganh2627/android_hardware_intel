/* Modem Manager - core dump header file
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

#ifndef __CORE_DUMP_HEADER__
#define __CORE_DUMP_HEADER__


#include "errors.h"
#include "dumpreader.h"
#define MMGR_FW_OPERATIONS
#include "mmgr.h"
#include "tcs_mmgr.h"

typedef void *mcdr_handle_t;

mcdr_handle_t *mcdr_init(const mcdr_info_t *cfg);
e_mmgr_errors_t mcdr_dispose(mcdr_handle_t *h);

e_mmgr_errors_t mcdr_start(mcdr_handle_t *h);
void mcdr_finalize(mcdr_handle_t *h);
void mcdr_cancel(mcdr_handle_t *h);

int mcdr_get_fd(mcdr_handle_t *h);

const char *mcdr_get_path(mcdr_handle_t *h);
const char *mcdr_get_mdm_version(mcdr_handle_t *h);
void mcdr_set_mdm_version(mcdr_handle_t *h, char *pmdm_version);
const char *mcdr_get_filename(mcdr_handle_t *h);
const char *mcdr_get_error_reason(mcdr_handle_t *h);
bool mcdr_is_enabled(mcdr_handle_t *h);
bool mcdr_log_is_enabled(mcdr_handle_t *h);
e_core_dump_state_t mcdr_get_result(mcdr_handle_t *h);

#endif                          /* __CORE_DUMP_HEADER__ */
