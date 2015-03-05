/* Modem Manager - events manager source file
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
#include <errno.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include "at.h"
#include "client_events.h"
#include "client_cnx.h"
#include "errors.h"
#include "events_manager.h"
#include "logs.h"
#include "modem_events.h"
#include "mmgr.h"
#include "property.h"
#include "security.h"
#include "timer_events.h"
#include "tty.h"
#include "modem_specific.h"

#include "hardware_legacy/power.h"

#define FIRST_EVENT -1

static const char const *g_mmgr_st[] = {
#undef X
#define X(a) #a
    MMGR_STATE
};

inline void set_mmgr_state(mmgr_data_t *mmgr, e_timer_type_t state)
{
    mmgr->state = state;
    LOG_VERBOSE("new STATE: %s", g_mmgr_st[mmgr->state]);
}

static e_mmgr_errors_t security_event(mmgr_data_t *mmgr)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;

    ret = secure_event(mmgr->secure);

    const char *err_msg = secure_get_error(mmgr->secure);
    if (err_msg) {
        static const char *const ev_type = "TFT_ERROR_MISC";
        mmgr_cli_tft_event_data_t data[] = { { strlen(err_msg), err_msg } };
        mmgr_cli_tft_event_t ev = { E_EVENT_ERROR,
                                    strlen(ev_type), ev_type,
                                    MMGR_CLI_TFT_AP_LOG_MASK |
                                    MMGR_CLI_TFT_BP_LOG_MASK,
                                    1, data };
        clients_inform_all(mmgr->clients, E_MMGR_NOTIFY_TFT_EVENT, &ev);
    }

    return ret;
}

/**
 * close modem tty and cnxs
 *
 * @param [in,out] mmgr mmgr context
 *
 * @return E_ERR_SUCCESS
 */
e_mmgr_errors_t events_dispose(mmgr_data_t *mmgr)
{
    ASSERT(mmgr != NULL);

    free(mmgr->events.ev);
    release_wake_lock(MODULE_NAME);
    if (mmgr->fd_tty != CLOSED_FD)
        tty_close(&mmgr->fd_tty);
    if (mmgr->fd_cnx != CLOSED_FD)
        cnx_close(&mmgr->fd_cnx);
    if (mmgr->epollfd != CLOSED_FD)
        close(mmgr->epollfd);

    return E_ERR_SUCCESS;
}

/**
 * initialize events module
 *
 * @param [in] nb_client
 * @param [in,out] mmgr mmgr context
 *
 * @return E_ERR_FAILED if failed
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t events_init(int nb_client, mmgr_data_t *mmgr)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;

    ASSERT(mmgr != NULL);

    /* initialize all fds */
    mmgr->fd_tty = CLOSED_FD;
    mmgr->fd_cnx = CLOSED_FD;
    mmgr->epollfd = CLOSED_FD;

    mmgr->events.nfds = 0;
    mmgr->events.ev = malloc(sizeof(struct epoll_event) * (nb_client + 1));
    if (mmgr->events.ev == NULL) {
        LOG_ERROR("memory allocation failed");
        ret = E_ERR_FAILED;
    } else {
        mmgr->events.cur_ev = FIRST_EVENT;
        mmgr->request.accept_request = true;
    }

    return ret;
}

/**
 * start events handler
 *
 * @param [in,out] mmgr mmgr context
 *
 * @return E_ERR_FAILED if failed
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t events_start(mmgr_data_t *mmgr)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;

    ASSERT(mmgr != NULL);

    if ((ret = mdm_prepare(&mmgr->info)) != E_ERR_SUCCESS)
        goto out;

    ret = cnx_open(&mmgr->fd_cnx);
    if (ret != E_ERR_SUCCESS)
        goto out;

    if ((ret = tty_init_listener(&mmgr->epollfd)) != E_ERR_SUCCESS)
        goto out;

    ret = tty_listen_fd(mmgr->epollfd, mmgr->fd_cnx, EPOLLIN);
    if (ret != E_ERR_SUCCESS)
        goto out;

    ret = tty_listen_fd(mmgr->epollfd, mmgr->info.fd_mcd, EPOLLIN);
    if (ret != E_ERR_SUCCESS)
        goto out;

    ret = tty_listen_fd(mmgr->epollfd, mdm_flash_get_fd(mmgr->mdm_flash),
                        EPOLLIN);
    if (ret != E_ERR_SUCCESS)
        goto out;

    if (mcdr_is_enabled(mmgr->mcdr)) {
        ret = tty_listen_fd(mmgr->epollfd, mcdr_get_fd(mmgr->mcdr), EPOLLIN);
        if (ret != E_ERR_SUCCESS)
            goto out;
    }

    ret = set_mcd_poll_states(&mmgr->info);
    LOG_DEBUG("MCD driver added to poll list");

    if (mmgr->info.mdm_link == E_LINK_USB) {
        if (E_ERR_SUCCESS != (ret = bus_ev_start(mmgr->events.bus_events)))
            goto out;

        int wd_fd = bus_ev_get_fd(mmgr->events.bus_events);
        ret = tty_listen_fd(mmgr->epollfd, wd_fd, EPOLLIN);
        if (ret != E_ERR_SUCCESS)
            goto out;
        LOG_DEBUG("bus event fd added to poll list");

        /* handle the first events after discovery */
        if (bus_ev_get_state(mmgr->events.bus_events) & MDM_BB_READY) {
            /* ready to configure modem */
            mmgr->events.link_state &= ~E_MDM_LINK_FLASH_READY;
            mmgr->events.link_state |= E_MDM_LINK_BB_READY;
        } else if (bus_ev_get_state(mmgr->events.bus_events) &
                   MDM_FLASH_READY) {
            /* ready to flash modem */
            mmgr->events.link_state |= E_MDM_LINK_FLASH_READY;
            mmgr->events.link_state &= ~E_MDM_LINK_BB_READY;
        } else if (!mmgr->info.is_flashless) {
            timer_start(mmgr->timer, E_TIMER_WAIT_FOR_BUS_READY);
        }
    } else {
        mmgr->events.link_state |= E_MDM_LINK_BB_READY;
    }

out:
    return ret;
}

/**
 * this function is an event dispatcher. it waits for a new event if event list
 * is empty. otherwise, it sets the current event type
 *
 * @param [in,out] mmgr mmgr context
 *
 * @return E_ERR_FAILED if epoll_wait fails
 * E_ERR_SUCCESS: if successful
 */
static e_mmgr_errors_t wait_for_event(mmgr_data_t *mmgr)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;
    int fd;

    ASSERT(mmgr != NULL);

    if (mmgr->events.cur_ev + 1 >= mmgr->events.nfds) {
        do {
            mmgr->events.cur_ev = FIRST_EVENT;
            LOG_INFO("Waiting for a new event");
            mmgr->events.nfds = epoll_wait(mmgr->epollfd, mmgr->events.ev,
                                           clients_get_allowed(mmgr->clients)
                                           + 1,
                                           timer_get_timeout(mmgr->timer));
            if (mmgr->events.nfds == -1) {
                if ((errno == EBADF) || (errno == EINVAL)) {
                    LOG_ERROR("Bad configuration");
                    ret = E_ERR_FAILED;
                    goto out;
                }
            }
        } while (mmgr->events.nfds == -1);
    }

    mmgr->events.cur_ev++;
    if (mmgr->events.nfds == 0) {
        mmgr->events.state = E_EVENT_TIMEOUT;
    } else {
        fd = mmgr->events.ev[mmgr->events.cur_ev].data.fd;
        if (fd == mmgr->fd_cnx)
            mmgr->events.state = E_EVENT_NEW_CLIENT;
        else if (fd == mmgr->fd_tty)
            mmgr->events.state = E_EVENT_IPC;
        else if (fd == mmgr->info.fd_mcd)
            mmgr->events.state = E_EVENT_MCD;
        else if (fd == bus_ev_get_fd(mmgr->events.bus_events))
            mmgr->events.state = E_EVENT_BUS;
        else if (fd == secure_get_fd(mmgr->secure))
            mmgr->events.state = E_EVENT_SECUR;
        else if (fd == mdm_flash_get_fd(mmgr->mdm_flash))
            mmgr->events.state = E_EVENT_FLASHING;
        else if (fd == mcdr_get_fd(mmgr->mcdr))
            mmgr->events.state = E_EVENT_MCDR;
        else
            mmgr->events.state = E_EVENT_CLIENT;
    }

out:
    return ret;
}

static inline void flush_pipe(int fd)
{
    char msg;

    read(fd, &msg, sizeof(msg));
}

/**
 * events manager: manage modem and cnx events
 * Instead of a state machine, an event dispatcher is used here.
 * A state machine is not usefull here as the protocol
 * is stateless.
 *
 * @param [in,out] mmgr mmgr context
 *
 * @return E_ERR_SUCCESS
 */
e_mmgr_errors_t events_manager(mmgr_data_t *mmgr)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    bool wakelock = false;
    static const char const *events_str[] = {
#undef X
#define X(a) #a
        EVENTS
    };

    ASSERT(mmgr != NULL);

    for (;; ) {
        if (mmgr->events.cli_req & E_CLI_REQ_OFF) {
            clients_reset_ack_shtdwn(mmgr->clients);
            timer_start(mmgr->timer, E_TIMER_FMMO);
            mdm_start_shtdwn(mmgr);
            set_mmgr_state(mmgr, E_MMGR_MDM_PREPARE_OFF);
            mmgr->events.cli_req &= ~E_CLI_REQ_OFF;
        } else if (mmgr->state == E_MMGR_MDM_RESET) {
            LOG_DEBUG("restoring modem");
            reset_modem(mmgr);
            mmgr->events.cli_req &= ~E_CLI_REQ_RESET;
        }

        if ((mmgr->state == E_MMGR_MDM_OFF) || (mmgr->state == E_MMGR_MDM_UP) ||
            (mmgr->state == E_MMGR_MDM_OOS)) {
            wakelock = false;
            release_wake_lock(MODULE_NAME);
        }
        if ((ret = wait_for_event(mmgr)) != E_ERR_SUCCESS)
            goto out;
        if (wakelock == false) {
            wakelock = true;
            acquire_wake_lock(PARTIAL_WAKE_LOCK, MODULE_NAME);
        }

        LOG_DEBUG("event type: %s", events_str[mmgr->events.state]);
        switch (mmgr->events.state) {
        case E_EVENT_IPC:
            ret = ipc_event(mmgr);
            break;
        case E_EVENT_MCD:
            ret = modem_control_event(mmgr);
            break;
        case E_EVENT_BUS:
            ret = bus_events(mmgr);
            break;
        case E_EVENT_NEW_CLIENT:
            ret = new_client(mmgr);
            break;
        case E_EVENT_CLIENT:
            ret = known_client(mmgr);
            break;
        case E_EVENT_SECUR:
            ret = security_event(mmgr);
            break;
        case E_EVENT_TIMEOUT: {
            bool reset = false;
            bool start_mdm_off = false;
            bool finalize_mdm_off = false;
            bool cd_ipc = false;
            bool cancel_flashing = false;
            bool stop_mcdr = false;

            ret = timer_event(mmgr->timer, &reset, &start_mdm_off,
                              &finalize_mdm_off, &cd_ipc,
                              &cancel_flashing, &stop_mcdr);
            if (reset)
                set_mmgr_state(mmgr, E_MMGR_MDM_RESET);
            if (cd_ipc)
                ctrl_on_cd_ipc_failure(mmgr->info.ctrl);
            if (cancel_flashing) {
                static const char *const msg = "Timeout during modem flashing. "
                                               "Operation cancelled";
                static const char *const ev_type = "TFT_STAT_FLASH";
                mmgr_cli_tft_event_data_t data[] = { { strlen(msg), msg } };
                mmgr_cli_tft_event_t ev =
                { E_EVENT_STATS, strlen(ev_type), ev_type, 0, 1, data };
                clients_inform_all(mmgr->clients, E_MMGR_NOTIFY_TFT_EVENT, &ev);

                LOG_INFO("%s", msg);
                mdm_flash_cancel(mmgr->mdm_flash);
                set_mmgr_state(mmgr, E_MMGR_MDM_RESET);
            }
            if (stop_mcdr) {
                mcdr_cancel(mmgr->mcdr);
                core_dump_finalize(mmgr, E_CD_TIMEOUT);
            }
            if (start_mdm_off) {
                mmgr->events.cli_req = E_CLI_REQ_OFF;
            } else if (finalize_mdm_off) {
                static const char *const msg = "Timeout during FMMO. Force "
                                               "modem shutdown";
                static const char *const ev_type = "TFT_ERROR_MISC";
                mmgr_cli_tft_event_data_t data[] = { { strlen(msg), msg } };
                mmgr_cli_tft_event_t ev = { E_EVENT_ERROR,
                                            strlen(ev_type), ev_type,
                                            MMGR_CLI_TFT_AP_LOG_MASK,
                                            1, data };
                clients_inform_all(mmgr->clients, E_MMGR_NOTIFY_TFT_EVENT, &ev);
                LOG_INFO("%s", msg);
                mdm_finalize_shtdwn(mmgr);
                set_mmgr_state(mmgr, E_MMGR_MDM_OFF);
            }

            break;
        }
        case E_EVENT_FLASHING:
            flush_pipe(mdm_flash_get_fd(mmgr->mdm_flash));
            timer_stop(mmgr->timer, E_TIMER_MDM_FLASHING);
            mdm_flash_finalize(mmgr->mdm_flash);
            flash_verdict(mmgr, mdm_flash_get_verdict(mmgr->mdm_flash));
            break;
        case E_EVENT_MCDR:
            flush_pipe(mcdr_get_fd(mmgr->mcdr));
            timer_stop(mmgr->timer, E_TIMER_CORE_DUMP_READING);

            mcdr_finalize(mmgr->mcdr);
            core_dump_finalize(mmgr, mcdr_get_result(mmgr->mcdr));
            break;
        }
    }

out:
    /* if the wakelock is set here, it will be removed by events_cleanup
     * function */
    return ret;
}
