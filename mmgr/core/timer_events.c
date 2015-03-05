/* Modem Manager - timer manager source file
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

#define MMGR_FW_OPERATIONS
#include "events_manager.h"
#include "modem_specific.h"
#include "file.h"
#include "logs.h"
#include "timer_events.h"
#include "modem_info.h"

static const char const *g_type_str[] = {
#undef X
#define X(a) #a
    TIMER
};

#define TIMER_INFINITE -1       /* wait indefinitely */

typedef struct mmgr_timer {
    e_timer_type_t type;
    int timeout[E_TIMER_NUM]; /* in seconds */
    struct timespec end[E_TIMER_NUM];
    const clients_hdle_t *clients;
} mmgr_timer_t;

/**
 * Returns remaining time (in seconds) between current time
 * and end timer
 */
static inline long timer_get_elapsed(struct timespec cur, struct timespec end)
{
    return ((end.tv_sec - cur.tv_sec) * 1000) +
           ((end.tv_nsec - cur.tv_nsec) / 1000000);
}

/**
 * Checks if timer has elapsed for a specific timer
 */
static inline bool timer_is_elapsed(mmgr_timer_t *t, e_timer_type_t type,
                                    struct timespec *cur)
{
    return (t->type & (0x1 << type)) &&
           (timer_get_elapsed(*cur, t->end[type]) <= 0);
}

/**
 * @brief timer_get_timeout Return current timeout value
 *
 * @param h timer module handle
 *
 * @return current timeout value otherwise (in milliseconds)
 */
int timer_get_timeout(timer_handle_t *h)
{
    int min = TIMER_INFINITE;
    mmgr_timer_t *t = (mmgr_timer_t *)h;

    ASSERT(t != NULL);

    if (t->type != 0x0) {
        int i = 0;
        struct timespec cur;
        clock_gettime(CLOCK_BOOTTIME, &cur);

        for (i = 0; i < E_TIMER_NUM; i++) {
            if (t->type & (0x1 << i)) {
                long diff = timer_get_elapsed(cur, t->end[i]);
                if (diff < 0)
                    diff = 0;

                if (min == TIMER_INFINITE)
                    min = diff;
                else if (min > diff)
                    min = diff;
            }
        }
    }

    LOG_DEBUG("timeout: %d ms", min);
    return min;
}

/**
 * start a timer for a specific event
 *
 * @param [in] h timer module handle
 * @param [in] type type of event
 *
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t timer_start(timer_handle_t *h, e_timer_type_t type)
{
    struct timespec current;
    mmgr_timer_t *timer = (mmgr_timer_t *)h;

    ASSERT(timer != NULL);

    clock_gettime(CLOCK_BOOTTIME, &current);

    if (timer->timeout[type] != 0) {
        LOG_DEBUG("start timer for event: %s", g_type_str[type]);
        timer->type |= 0x01 << type;
        timer->end[type] = current;
        timer->end[type].tv_sec += timer->timeout[type];
    }

    return E_ERR_SUCCESS;
}

/**
 * @brief stop_all_timers stop all running timers
 *
 * @param [in] h timer module handle
 *
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t timer_stop_all(timer_handle_t *h)
{
    mmgr_timer_t *timer = (mmgr_timer_t *)h;

    ASSERT(timer != NULL);

    timer->type = 0x0;
    LOG_DEBUG("timer stopped");

    return E_ERR_SUCCESS;
}

/**
 * stop a timer for a specific event
 *
 * @param [in] h timer module handle
 * @param [in] type type of event
 *
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t timer_stop(timer_handle_t *h, e_timer_type_t type)
{
    mmgr_timer_t *timer = (mmgr_timer_t *)h;

    ASSERT(timer != NULL);

    LOG_DEBUG("stop timer for event: %s", g_type_str[type]);
    timer->type &= ~(0x01 << type);

    if (timer->type == 0x0)
        LOG_DEBUG("All timers stopped");

    return E_ERR_SUCCESS;
}

/**
 * handle timeout cases
 *
 * @param [in] h timer module handle
 * @param [out] reset true if MMGR should reset the modem
 * @param [out] start_mdm_off true if MMGR should start the modem shutdown
 * @param [out] finalize_mdm_off true if MMGR should finalize the modem shutdown
 * @param [out] cd_err true if MMGR should restart cd IPC
 * @param [out] cancel_flashing flashing thread timeout. cancel the operation
 * @param [out] stop_mcdr stop mcdr thread.
 *
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t timer_event(timer_handle_t *h, bool *reset,
                            bool *start_mdm_off, bool *finalize_mdm_off,
                            bool *cd_err, bool *cancel_flashing,
                            bool *stop_mcdr)
{
    struct timespec cur;
    mmgr_timer_t *t = (mmgr_timer_t *)h;
    bool handled = false;

    ASSERT(t != NULL);
    ASSERT(reset != NULL);
    ASSERT(start_mdm_off != NULL);
    ASSERT(finalize_mdm_off != NULL);
    ASSERT(cd_err != NULL);
    ASSERT(stop_mcdr != NULL);

    *reset = false;
    *start_mdm_off = false;
    *finalize_mdm_off = false;
    *cd_err = false;
    *stop_mcdr = false;

    clock_gettime(CLOCK_BOOTTIME, &cur);

    if (timer_is_elapsed(t, E_TIMER_COLD_RESET_ACK, &cur)) {
        handled = true;
        clients_has_ack_cold(t->clients, E_PRINT);
        timer_stop(h, E_TIMER_COLD_RESET_ACK);
        *reset = true;
    }

    if (timer_is_elapsed(t, E_TIMER_MODEM_SHUTDOWN_ACK, &cur)) {
        handled = true;
        clients_has_ack_shtdwn(t->clients, E_PRINT);
        timer_stop(h, E_TIMER_MODEM_SHUTDOWN_ACK);
        *reset = true;
        *start_mdm_off = true;
    }

    if (timer_is_elapsed(t, E_TIMER_WAIT_FOR_IPC_READY, &cur)) {
        mmgr_cli_fw_update_result_t result = { .id = E_MODEM_FW_READY_TIMEOUT };
        static const char *const msg = "IPC READY not received";

        LOG_DEBUG("%s. Force modem reset", msg);
        clients_inform_all(t->clients, E_MMGR_RESPONSE_MODEM_FW_RESULT,
                           &result);
        static const char *const ev_type = "TFT_ERROR_IPC";
        mmgr_cli_tft_event_data_t data[] = { { strlen(msg), msg } };
        mmgr_cli_tft_event_t ev = { E_EVENT_ERROR,
                                    strlen(ev_type), ev_type,
                                    MMGR_CLI_TFT_AP_LOG_MASK,
                                    1, data };
        clients_inform_all(t->clients, E_MMGR_NOTIFY_TFT_EVENT, &ev);

        handled = true;
        timer_stop(h, E_TIMER_WAIT_FOR_IPC_READY);
        *reset = true;
    }

    if (timer_is_elapsed(t, E_TIMER_WAIT_FOR_BUS_READY, &cur)) {
        static const char *const msg = "BUS READY not received";

        LOG_DEBUG("%s. Force modem reset", msg);
        static const char *const ev_type = "TFT_ERROR_USB";
        mmgr_cli_tft_event_data_t data[] = { { strlen(msg), msg } };
        mmgr_cli_tft_event_t ev = { E_EVENT_ERROR,
                                    strlen(ev_type), ev_type,
                                    MMGR_CLI_TFT_AP_LOG_MASK,
                                    1, data };
        clients_inform_all(t->clients, E_MMGR_NOTIFY_TFT_EVENT, &ev);

        handled = true;
        timer_stop(h, E_TIMER_WAIT_FOR_BUS_READY);
        *reset = true;
    }

    if (timer_is_elapsed(t, E_TIMER_REBOOT_MODEM_DELAY, &cur)) {
        handled = true;
        timer_stop(h, E_TIMER_REBOOT_MODEM_DELAY);
        *reset = true;
    }

    if (timer_is_elapsed(t, E_TIMER_CORE_DUMP_IPC_RESET, &cur)) {
        handled = true;
        timer_stop(h, E_TIMER_CORE_DUMP_IPC_RESET);
        LOG_DEBUG("Timeout while waiting for core dump IPC. Reset IPC");
        *cd_err = true;
    }

    if (timer_is_elapsed(t, E_TIMER_WAIT_CORE_DUMP_READY, &cur)) {
        handled = true;
        LOG_DEBUG("timeout while waiting for core dump ipc. reset modem");
        timer_stop(h, E_TIMER_WAIT_CORE_DUMP_READY);
        *reset = true;

        mmgr_cli_core_dump_t cd = { .state = E_CD_LINK_ERROR };
        cd.reason = "modem enumeration failure";
        cd.reason_len = strlen(cd.reason);
        clients_inform_all(t->clients, E_MMGR_NOTIFY_CORE_DUMP_COMPLETE,
                           &cd);
    }

    if (timer_is_elapsed(t, E_TIMER_MDM_FLASHING, &cur)) {
        handled = true;
        timer_stop(h, E_TIMER_MDM_FLASHING);
        *cancel_flashing = true;
    }

    if (timer_is_elapsed(t, E_TIMER_CORE_DUMP_READING, &cur)) {
        handled = true;
        timer_stop(h, E_TIMER_CORE_DUMP_READING);
        *stop_mcdr = true;
    }

    if (timer_is_elapsed(t, E_TIMER_FMMO, &cur)) {
        handled = true;
        timer_stop(h, E_TIMER_FMMO);
        *finalize_mdm_off = true;
    }

    if (!handled)
        LOG_ERROR("timeout not handled");

    return E_ERR_SUCCESS;
}

/**
 * initialize timer module
 *
 * @param [in] recov recovery configuration
 * @param [in] timings mmgr timings
 * @param [in] clients list
 *
 * @return a valid timer_handle_t pointer
 * @return NULL otherwise
 */
timer_handle_t *timer_init(const mmgr_recovery_t *recov,
                           const mmgr_timings_t *timings,
                           const mcdr_info_t *mcdr,
                           const clients_hdle_t *clients)

{
    mmgr_timer_t *timer = NULL;

    ASSERT(recov != NULL);
    ASSERT(timings != NULL);
    ASSERT(clients != NULL);

    timer = calloc(1, sizeof(mmgr_timer_t));
    if (timer) {
        timer->clients = clients;
        timer->type = 0;

        timer->timeout[E_TIMER_COLD_RESET_ACK] = recov->cold_timeout;
        timer->timeout[E_TIMER_MODEM_SHUTDOWN_ACK] = recov->shtdwn_timeout;
        timer->timeout[E_TIMER_WAIT_FOR_IPC_READY] = timings->ipc_ready;
        timer->timeout[E_TIMER_WAIT_FOR_BUS_READY] = timings->ipc_ready;
        timer->timeout[E_TIMER_REBOOT_MODEM_DELAY] = timings->ipc_ready;
        timer->timeout[E_TIMER_CORE_DUMP_IPC_RESET] = timings->cd_ipc_reset;
        timer->timeout[E_TIMER_WAIT_CORE_DUMP_READY] =
            timings->cd_ipc_ready;
        timer->timeout[E_TIMER_MDM_FLASHING] = timings->mdm_flash;
        timer->timeout[E_TIMER_CORE_DUMP_READING] = mcdr->gnl.timeout;
        timer->timeout[E_TIMER_FMMO] = timings->fmmo;
    }

    return (timer_handle_t *)timer;
}

/**
 * dispose timer module
 *
 * @param [in] h timer module handle
 *
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t timer_dispose(timer_handle_t *h)
{
    mmgr_timer_t *timer = (mmgr_timer_t *)h;

    /* do not use ASSERT in dispose function */

    free(timer);

    return E_ERR_SUCCESS;
}

/**
 * Returns the time elapsed since starting of the timer for a specific event
 *
 * @param [in] h timer module handle
 * @param [in] type type of event
 *
 * @return time elapsed since starting in milliseconds
 */
long timer_get_value(timer_handle_t *h, e_timer_type_t type)
{
    struct timespec current;
    mmgr_timer_t *t = (mmgr_timer_t *)h;

    ASSERT(t != NULL);

    clock_gettime(CLOCK_BOOTTIME, &current);

    return t->timeout[type] * 1000 - timer_get_elapsed(current, t->end[type]);
}
