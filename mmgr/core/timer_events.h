/* Modem Manager - timer manager header file
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

#ifndef __MMGR_TIMER_HEADER__
#define __MMGR_TIMER_HEADER__

#include "client.h"
#include "errors.h"
#include "tcs_mmgr.h"
#include "states.h"

typedef void *timer_handle_t;

#define TIMER \
    X(COLD_RESET_ACK), \
    X(MODEM_SHUTDOWN_ACK), \
    X(WAIT_FOR_IPC_READY), \
    X(WAIT_FOR_BUS_READY), \
    X(REBOOT_MODEM_DELAY), \
    X(CORE_DUMP_IPC_RESET), \
    X(WAIT_CORE_DUMP_READY), \
    X(MDM_FLASHING), \
    X(CORE_DUMP_READING), \
    X(FMMO), \
    X(NUM)

typedef enum e_timer_type {
#undef X
#define X(a) E_TIMER_ ## a
    TIMER
} e_timer_type_t;

/* Defines for bitfield describing current context (up to 32 bits):
 *    bit   value
 *    0     reset: 1 if MMGR should reset the modem
 *    1     start_mdm_off: 1 if MMGR should start the modem shutdown
 *    2     finalize_mdm_off: 1 if MMGR should finalize the modem shutdown
 *    3     cd_err: 1 if MMGR should restart cd IPC
 *    4     cancel_flashing flashing thread timeout. 1 to cancel the operation
 *    5     stop_mcdr: 1 to stop mcdr thread
 *    6     streamline: 1 if tlv application is on going
 */
typedef enum  e_timer_mdm_action {
    E_TIMER_NO_ACTION = 0x0,
    E_TIMER_RESET = 0x1,
    E_TIMER_START_MDM_OFF = 0x1 << 1,
        E_TIMER_FINALIZE_MDM_OFF = 0x1 << 2,
        E_TIMER_CD_ERR = 0x1 << 3,
        E_TIMER_CANCEL_FLASHING = 0x1 << 4,
        E_TIMER_STOP_MCDR = 0x1 << 5,
        E_TIMER_STREAMLINE = 0x1 << 6
} e_timer_mdm_action_t;

timer_handle_t *timer_init(const mmgr_recovery_t *recov,
                           const mmgr_timings_t *timings,
                           const mcdr_info_t *mcdr,
                           const clients_hdle_t *clients);
e_mmgr_errors_t timer_dispose(timer_handle_t *h);

e_mmgr_errors_t timer_start(timer_handle_t *h, e_timer_type_t type);
e_mmgr_errors_t timer_stop(timer_handle_t *h, e_timer_type_t type);
e_mmgr_errors_t timer_event(timer_handle_t *h, e_mmgr_state_t state,
                            bool core_dump_signal_hw_working,
                            unsigned int *context);
e_mmgr_errors_t timer_stop_all(timer_handle_t *h);

int timer_get_timeout(timer_handle_t *h);
long timer_get_value(timer_handle_t *h, e_timer_type_t type);

#endif                          /* __MMGR_TIMER_HEADER__ */
