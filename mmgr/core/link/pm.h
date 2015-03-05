/* Modem Manager - link power management source file
**
** Copyright (C) Intel 2013
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

#ifndef __MMGR_IPC_PM_HEADER__
#define __MMGR_IPC_PM_HEADER__

#include "errors.h"
#include "tcs_mmgr.h"

typedef void *pm_handle_t;

pm_handle_t pm_init(e_link_t mdm_type, power_t *mdm_power, e_link_t cd_type,
                    power_t *cd_power);
e_mmgr_errors_t pm_dispose(pm_handle_t *h);

e_mmgr_errors_t pm_on_mdm_flash(pm_handle_t *h);
e_mmgr_errors_t pm_on_mdm_up(pm_handle_t *h);

e_mmgr_errors_t pm_on_mdm_oos(pm_handle_t *h);

e_mmgr_errors_t pm_on_mdm_cd(pm_handle_t *h);
e_mmgr_errors_t pm_on_mdm_cd_complete(pm_handle_t *h);

#endif                          /* __MMGR_IPC_PM_HEADER__ */
