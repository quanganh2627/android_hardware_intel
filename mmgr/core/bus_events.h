/* Modem Manager - bus events header file
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

#ifndef __MMGR_BUS_EV_MANAGER_HEADER__
#define __MMGR_BUS_EV_MANAGER_HEADER__

#include "errors.h"
#include "tcs_mmgr.h"

#define BUS_EV_CAPACITY 32
#define MDM_FLASH_READY 1
#define MDM_BB_READY 2
#define MDM_CD_READY 4

typedef void *bus_ev_hdle_t;

bus_ev_hdle_t *bus_ev_init(link_t *flash, link_t *bb, link_t *mcdr);
e_mmgr_errors_t bus_ev_dispose(bus_ev_hdle_t *h);
e_mmgr_errors_t bus_ev_start(bus_ev_hdle_t *h);

int bus_ev_get_state(bus_ev_hdle_t *h);
int bus_ev_get_fd(bus_ev_hdle_t *h);
int bus_ev_read(bus_ev_hdle_t *h);
e_mmgr_errors_t bus_ev_hdle_events(bus_ev_hdle_t *h);
const char *bus_ev_get_flash_interface(const bus_ev_hdle_t *h);

#endif                          /* __MMGR_BUS_EV_MANAGER_HEADER__ */
