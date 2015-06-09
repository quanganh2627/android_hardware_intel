/* Modem Manager - events manager header file
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

#ifndef __MMGR_STATES_HEADER__
#define __MMGR_STATES_HEADER__

#define MMGR_STATE \
    X(MDM_OFF), \
    X(MDM_RESET), \
    X(MDM_START), \
    X(MDM_CONF_ONGOING), \
    X(MDM_UP), \
    X(MDM_OOS), \
    X(WAIT_COLD_ACK), \
    X(WAIT_SHT_ACK), \
    X(MDM_CORE_DUMP), \
    X(MDM_PREPARE_OFF), \
    X(MDM_LINK_USB_DISC), \
    X(NUM)

typedef enum e_mmgr_state {
#undef X
#define X(a) E_MMGR_ ## a
    MMGR_STATE
} e_mmgr_state_t;

#endif                          /* __MMGR_STATES_HEADER__ */
