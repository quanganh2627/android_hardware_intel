/* Modem Manager - modem events source file
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "at.h"
#include "common.h"
#include "errors.h"
#include "mdm_flash.h"
#include "file.h"
#include "java_intent.h"
#include "logs.h"
#include "modem_events.h"
#include "modem_specific.h"
#include "mux.h"
#include "security.h"
#include "timer_events.h"
#include "tty.h"
#include "mmgr.h"
#include "pm.h"

/* AT command to shutdown modem */
#define POWER_OFF_MODEM "AT+CFUN=0\r"

#define READ_SIZE 64
#define AT_CFUN_RETRY 0

static e_mmgr_errors_t pre_modem_out_of_service(mmgr_data_t *mmgr);

static inline void mdm_close_fds(mmgr_data_t *mmgr)
{
    secure_stop(mmgr->secure);
    tty_close(&mmgr->fd_tty);
}

static e_mmgr_errors_t notify_core_dump(clients_hdle_t *clients,
                                        mcdr_handle_t *mcdr_hdle,
                                        e_core_dump_state_t state)
{
    mmgr_cli_core_dump_t cd;
    e_mmgr_errors_t ret = E_ERR_FAILED;

    ASSERT(clients != NULL);

    memset(&cd, 0, sizeof(cd));

    cd.state = state;
    if (mcdr_hdle != NULL) {
        const char *path = mcdr_get_path(mcdr_hdle);
        const char *filename = mcdr_get_filename(mcdr_hdle);
        cd.path_len = strnlen(path, PATH_MAX) + strnlen(filename, PATH_MAX) + 2;

        cd.path = malloc(sizeof(char) * cd.path_len);
        if (cd.path == NULL) {
            LOG_ERROR("memory allocation fails");
            goto out;
        }
        snprintf(cd.path, cd.path_len, "%s/%s", path, filename);

        if (E_CD_TIMEOUT == state) {
            cd.reason = "Timeout. Operation aborted";
            cd.reason_len = strlen(cd.reason);
        } else if (E_CD_SUCCEED != state) {
            cd.reason = (char *)mcdr_get_error_reason(mcdr_hdle);
            cd.reason_len = strlen(cd.reason);
        } else {
            cd.reason = NULL;
            cd.reason_len = 0;
        }
    }
    ret = clients_inform_all(clients, E_MMGR_NOTIFY_CORE_DUMP_COMPLETE, &cd);
    free(cd.path);

out:
    return ret;
}

/**
 * Checks if the modem flashing has succeed and informs MMGR clients
 *
 * @param [in] mmgr
 * @param [in] verdict
 *
 * @return none
 */
void flash_verdict(mmgr_data_t *mmgr, e_modem_fw_error_t verdict)
{
    mmgr_cli_fw_update_result_t fw_result = { .id = verdict };
    static const char *const ev_type = "TFT_STAT_FLASH";

    ASSERT(mmgr != NULL);

    clients_inform_all(mmgr->clients, E_MMGR_RESPONSE_MODEM_FW_RESULT,
                       &fw_result);

    switch (fw_result.id) {
    case E_MODEM_FW_BAD_FAMILY: {
        broadcast_msg(E_MSG_INTENT_MODEM_FW_BAD_FAMILY);

        static const char *const msg = "Modem FW bad family";
        mmgr_cli_tft_event_data_t data[] = { { strlen(msg), msg } };
        mmgr_cli_tft_event_t ev =
        { E_EVENT_STATS, strlen(ev_type), ev_type, 0, 1, data };
        clients_inform_all(mmgr->clients, E_MMGR_NOTIFY_TFT_EVENT, &ev);

        /* Set MMGR state to MDM_RESET to call the recovery module and
         * force modem recovery to OOS. By doing so, MMGR will turn off the
         * modem and declare the modem OOS. Clients will not be able to turn
         * on the modem */
        recov_force(mmgr->reset, E_FORCE_OOS);
        set_mmgr_state(mmgr, E_MMGR_MDM_RESET);
        break;
    }

    case E_MODEM_FW_SUCCEED: {
        set_mmgr_state(mmgr, E_MMGR_MDM_CONF_ONGOING);
        /* @TODO: fix that into flash_modem/modem_specific */
        if (mmgr->info.mdm_link == E_LINK_USB)
            timer_start(mmgr->timer, E_TIMER_WAIT_FOR_BUS_READY);
        timer_start(mmgr->timer, E_TIMER_WAIT_FOR_IPC_READY);

        static const char *const msg = "Modem flashing succeed";
        mmgr_cli_tft_event_data_t data[] =
        { { strlen(msg), msg }, { 0, NULL }, { 0, NULL } };
        mmgr_cli_tft_event_t ev =
        { E_EVENT_STATS, strlen(ev_type), ev_type, 0, 3, data };

        data[1].value = calloc(MMGR_CLI_MAX_TFT_EVENT_DATA_LEN, sizeof(char));
        if (data[1].value != NULL) {
            data[1].len = snprintf((char *)data[1].value,
                                   MMGR_CLI_MAX_TFT_EVENT_DATA_LEN, "%d",
                                   mdm_flash_get_attempts(mmgr->mdm_flash));

            data[2].value =
                calloc(MMGR_CLI_MAX_TFT_EVENT_DATA_LEN, sizeof(char));
            if (data[2].value != NULL) {
                data[2].len = snprintf((char *)data[2].value,
                                       MMGR_CLI_MAX_TFT_EVENT_DATA_LEN,
                                       "%ld",
                                       timer_get_value(mmgr->timer,
                                                       E_TIMER_MDM_FLASHING));
            } else {
                LOG_DEBUG("Memory allocation failed for data 2. Data 2 not set.");
                ev.num_data = 2;
            }
        } else {
            LOG_DEBUG(
                "Memory allocation failed for data 1. Data 1 and 2 not set.");
            ev.num_data = 1;
        }

        clients_inform_all(mmgr->clients, E_MMGR_NOTIFY_TFT_EVENT, &ev);
        mdm_flash_reset_attempts(mmgr->mdm_flash);

        if (data[1].value != NULL)
            free((char *)data[1].value);
        if (data[2].value != NULL)
            free((char *)data[2].value);

        break;
    }

    case E_MODEM_FW_SW_CORRUPTED: {
        static const char *const msg = "Modem FW corrupted";
        mmgr_cli_tft_event_data_t data[] = { { strlen(msg), msg } };
        mmgr_cli_tft_event_t ev =
        { E_EVENT_STATS, strlen(ev_type), ev_type, 0, 1, data };
        clients_inform_all(mmgr->clients, E_MMGR_NOTIFY_TFT_EVENT, &ev);

        /* Set MMGR state to MDM_RESET to call the recovery module and
         * force modem recovery to OOS. By doing so, MMGR will turn off the
         * modem and declare the modem OOS. Clients will not be able to turn
         * on the modem */
        recov_force(mmgr->reset, E_FORCE_OOS);
        set_mmgr_state(mmgr, E_MMGR_MDM_RESET);
        break;
    }

    case E_MODEM_FW_ERROR_UNSPECIFIED: {
        static const char *const msg = "Modem FW unspecified error";
        mmgr_cli_tft_event_data_t data[] = { { strlen(msg), msg } };
        mmgr_cli_tft_event_t ev =
        { E_EVENT_STATS, strlen(ev_type), ev_type, MMGR_CLI_TFT_AP_LOG_MASK, 1,
          data };
        clients_inform_all(mmgr->clients, E_MMGR_NOTIFY_TFT_EVENT, &ev);

        set_mmgr_state(mmgr, E_MMGR_MDM_RESET);
        break;
    }

    case E_MODEM_FW_READY_TIMEOUT: {
        /* This error is already reported: "IPC ready not received" */
        set_mmgr_state(mmgr, E_MMGR_MDM_RESET);
        break;
    }

    case E_MODEM_FW_OUTDATED: {
        broadcast_msg(E_MSG_INTENT_MODEM_FW_OUTDATED);
        static const char *const msg = "Modem FW outdated";
        mmgr_cli_tft_event_data_t data[] = { { strlen(msg), msg } };
        mmgr_cli_tft_event_t ev =
        { E_EVENT_STATS, strlen(ev_type), ev_type, 0, 1, data };
        clients_inform_all(mmgr->clients, E_MMGR_NOTIFY_TFT_EVENT, &ev);

        /* Set MMGR state to MDM_RESET to call the recovery module and
         * force modem recovery to OOS. By doing so, MMGR will turn off the
         * modem and declare the modem OOS. Clients will not be able to turn
         * on the modem */
        recov_force(mmgr->reset, E_FORCE_OOS);
        set_mmgr_state(mmgr, E_MMGR_MDM_RESET);
        break;
    }

    case E_MODEM_FW_SECURITY_CORRUPTED: {
        broadcast_msg(E_MSG_INTENT_MODEM_FW_SECURITY_CORRUPTED);
        static const char *const msg = "Modem FW security corrupted";
        mmgr_cli_tft_event_data_t data[] = { { strlen(msg), msg } };
        mmgr_cli_tft_event_t ev =
        { E_EVENT_STATS, strlen(ev_type), ev_type, 0, 1, data };
        clients_inform_all(mmgr->clients, E_MMGR_NOTIFY_TFT_EVENT, &ev);

        /* Set MMGR state to MDM_RESET to call the recovery module and
         * force modem recovery to OOS. By doing so, MMGR will turn off the
         * modem and declare the modem OOS. Clients will not be able to turn
         * on the modem */
        recov_force(mmgr->reset, E_FORCE_OOS);
        set_mmgr_state(mmgr, E_MMGR_MDM_RESET);
        break;
    }

    case E_MODEM_FW_NUM:
        /* nothing to do */
        break;
    }
}

/**
 * Apply TLV update. A TLV update is applied only if one TLV file exists in the
 * folder specified by TCS.
 *
 * @param [in,out] mmgr mmgr context
 *
 * @return true if a TLV file has been applied
 * @return false otherwise
 */
static bool apply_tlv(mmgr_data_t *mmgr)
{
    bool applied = false;
    mmgr_cli_nvm_update_result_t nvm_result =
    { .id = E_MODEM_NVM_ERROR_UNSPECIFIED };
    static const char *const ev_type = "TFT_ERROR_TLV";

    ASSERT(mmgr != NULL);

    if (mmgr->info.is_flashless) {
        char *files[2];
        int found = file_find(mmgr->info.tlv_path, ".tlv", files,
                              ARRAY_SIZE(files));
        if (found == 0) {
            nvm_result.id = E_MODEM_NVM_NO_NVM_PATCH;
            LOG_DEBUG("no TLV file found at %s; skipping nvm update",
                      mmgr->info.tlv_path);
        } else if (found == 1) {
            LOG_DEBUG("TLV file to apply: %s", files[0]);
            if (E_ERR_SUCCESS != flash_modem_nvm(&mmgr->info,
                                                 mmgr->info.mdm_custo_dlc,
                                                 files[0],
                                                 &nvm_result.id,
                                                 &nvm_result.sub_error_code)) {
                static const char *const msg =
                    "TLV failure: failed to apply TLV";
                LOG_ERROR("%s", msg);

                mmgr_cli_tft_event_data_t data[] =
                { { strlen(msg), msg }, { strlen(files[0]), files[0] } };
                mmgr_cli_tft_event_t ev = { E_EVENT_ERROR,
                                            strlen(ev_type), ev_type,
                                            MMGR_CLI_TFT_AP_LOG_MASK |
                                            MMGR_CLI_TFT_BP_LOG_MASK,
                                            2, data };
                clients_inform_all(mmgr->clients, E_MMGR_NOTIFY_TFT_EVENT, &ev);
            } else {
                applied = true;
            }
        } else {
            static const char *const msg = "TLV failure: too many files found";
            LOG_ERROR("%s. Skipping NVM update", msg);
            mmgr_cli_tft_event_data_t data[] = { { strlen(msg), msg },
                                                 { strlen(files[0]), files[0] },
                                                 { strlen(files[1]),
                                                   files[1] } };
            mmgr_cli_tft_event_t ev = { E_EVENT_ERROR,
                                        strlen(ev_type), ev_type,
                                        MMGR_CLI_TFT_AP_LOG_MASK,
                                        3, data };
            clients_inform_all(mmgr->clients, E_MMGR_NOTIFY_TFT_EVENT, &ev);
        }

        int i;
        for (i = 0; i < found; i++)
            free(files[i]);

        clients_inform_all(mmgr->clients, E_MMGR_RESPONSE_MODEM_NVM_RESULT,
                           &nvm_result);
    }

    return applied;
}

static void core_dump_prepare(mmgr_data_t *mmgr)
{
    ASSERT(mmgr != NULL);

    if (mcdr_is_enabled(mmgr->mcdr)) {
        timer_stop(mmgr->timer, E_TIMER_CORE_DUMP_IPC_RESET);
        timer_stop(mmgr->timer, E_TIMER_WAIT_CORE_DUMP_READY);
        timer_start(mmgr->timer, E_TIMER_CORE_DUMP_READING);

        pm_on_mdm_cd(mmgr->info.pm);
        mcdr_start(mmgr->mcdr);
    } else {
        timer_stop_all(mmgr->timer);
        set_mmgr_state(mmgr, E_MMGR_MDM_RESET);
    }
}

void core_dump_finalize(mmgr_data_t *mmgr, e_core_dump_state_t state)
{
    ASSERT(mmgr != NULL);

    pm_on_mdm_cd_complete(mmgr->info.pm);

    broadcast_msg(E_MSG_INTENT_CORE_DUMP_COMPLETE);
    notify_core_dump(mmgr->clients, mmgr->mcdr, state);

    mmgr->info.polled_states |= MDM_CTRL_STATE_IPC_READY;
    mmgr->info.polled_states &= ~MDM_CTRL_STATE_WARM_BOOT;
    set_mcd_poll_states(&mmgr->info);

    if (mmgr->info.mdm_link == E_LINK_USB)
        mmgr->events.link_state = E_MDM_LINK_NONE;

    /* The modem will be reset. No need to launch
     * a timer */
    set_mmgr_state(mmgr, E_MMGR_MDM_RESET);
}

/**
 * add new tty file descriptor to epoll
 *
 * @param [in,out] mmgr mmgr context
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
static e_mmgr_errors_t update_modem_tty(mmgr_data_t *mmgr)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;

    ASSERT(mmgr != NULL);

    ret = tty_listen_fd(mmgr->epollfd, mmgr->fd_tty, EPOLLIN);
    if (ret == E_ERR_SUCCESS) {
        mmgr->info.polled_states = MDM_CTRL_STATE_COREDUMP | MDM_CTRL_STATE_OFF;
        mmgr->info.polled_states |=
            MDM_CTRL_STATE_WARM_BOOT | MDM_CTRL_STATE_COLD_BOOT;

        ret = set_mcd_poll_states(&mmgr->info);
    }

    return ret;
}

/**
 * handle E_EL_MODEM_COLD_RESET pre reset escalation state
 *
 * @param [in,out] mmgr mmgr context
 *
 * @return E_ERR_FAILED if reset not performed
 * @return E_ERR_SUCCESS if successful
 */
static e_mmgr_errors_t pre_mdm_cold_reset(mmgr_data_t *mmgr)
{
    static bool wait_operation = true;

    ASSERT(mmgr != NULL);

    if (clients_get_connected(mmgr->clients) == 0) {
        recov_set_state(mmgr->reset, E_OPERATION_CONTINUE);
        wait_operation = false;
    } else if (wait_operation) {
        LOG_DEBUG("need to ack all clients");

        wait_operation = false;
        recov_set_state(mmgr->reset, E_OPERATION_WAIT);

        clients_inform_all(mmgr->clients, E_MMGR_EVENT_MODEM_DOWN, NULL);
        clients_inform_all(mmgr->clients, E_MMGR_NOTIFY_MODEM_COLD_RESET,
                           NULL);
        set_mmgr_state(mmgr, E_MMGR_WAIT_COLD_ACK);
        timer_start(mmgr->timer, E_TIMER_COLD_RESET_ACK);
    } else {
        wait_operation = true;
        recov_set_state(mmgr->reset, E_OPERATION_CONTINUE);

        broadcast_msg(E_MSG_INTENT_MODEM_COLD_RESET);
        clients_reset_ack_cold(mmgr->clients);
        mmgr->request.accept_request = false;
        if ((mmgr->info.mdm_link == E_LINK_USB) && mmgr->info.is_flashless)
            set_mmgr_state(mmgr, E_MMGR_MDM_START);
        else
            set_mmgr_state(mmgr, E_MMGR_MDM_CONF_ONGOING);

        timer_stop(mmgr->timer, E_TIMER_COLD_RESET_ACK);
    }

    return E_ERR_SUCCESS;
}

/**
 * handle E_EL_PLATFORM_REBOOT pre reset escalation state
 *
 * @param [in,out] mmgr mmgr context
 *
 * @return E_ERR_SUCCESS
 */
static e_mmgr_errors_t pre_platform_reboot(mmgr_data_t *mmgr)
{
    int reboot_counter = recov_get_reboot();

    ASSERT(mmgr != NULL);

    recov_set_state(mmgr->reset, E_OPERATION_CONTINUE);
    if (reboot_counter >= recov_get_retry_allowed(mmgr->reset)) {
        /* go to next level */
        LOG_INFO("Reboot cancelled. Max value reached");
        recov_next(mmgr->reset);
        pre_modem_out_of_service(mmgr);
    } else {
        recov_set_reboot(++reboot_counter);

        clients_inform_all(mmgr->clients, E_MMGR_NOTIFY_PLATFORM_REBOOT, NULL);
        broadcast_msg(E_MSG_INTENT_PLATFORM_REBOOT);

        /* pretend that the modem is OOS to reject all clients' requests */
        set_mmgr_state(mmgr, E_MMGR_MDM_OOS);
    }

    return E_ERR_SUCCESS;
}

/**
 * handle E_EL_MODEM_OUT_OF_SERVICE pre reset escalation state
 *
 * @param [in,out] mmgr mmgr context
 *
 * @return E_ERR_SUCCESS
 */
static e_mmgr_errors_t pre_modem_out_of_service(mmgr_data_t *mmgr)
{
    ASSERT(mmgr != NULL);

    LOG_INFO("MODEM OUT OF SERVICE");
    clients_inform_all(mmgr->clients, E_MMGR_EVENT_MODEM_OUT_OF_SERVICE, NULL);
    broadcast_msg(E_MSG_INTENT_MODEM_OUT_OF_SERVICE);

    set_mmgr_state(mmgr, E_MMGR_MDM_OOS);
    recov_set_state(mmgr->reset, E_OPERATION_CONTINUE);

    return E_ERR_SUCCESS;
}

/**
 * starts the modem shutdown
 *
 * @param [in,out] mmgr mmgr context
 *
 * @return E_ERR_FAILED if reset not performed
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t mdm_start_shtdwn(mmgr_data_t *mmgr)
{
    int fd = CLOSED_FD;
    e_mmgr_errors_t ret = E_ERR_SUCCESS;

    ASSERT(mmgr != NULL);

    mmgr->info.polled_states = MDM_CTRL_STATE_WARM_BOOT |
                               MDM_CTRL_STATE_COREDUMP;
    set_mcd_poll_states(&mmgr->info);

    tty_open(mmgr->info.shtdwn_dlc, &fd);
    if (fd < 0) {
        LOG_ERROR("operation FAILED");
        ret = E_ERR_FAILED;
    } else {
        send_at_retry(fd, POWER_OFF_MODEM, strlen(POWER_OFF_MODEM),
                      AT_CFUN_RETRY, AT_ANSWER_NO_TIMEOUT);
        tty_close(&fd);
    }

    return ret;
}

e_mmgr_errors_t mdm_finalize_shtdwn(mmgr_data_t *mmgr)
{
    ASSERT(mmgr != NULL);
    clients_inform_all(mmgr->clients, E_MMGR_EVENT_MODEM_DOWN, NULL);

    mdm_close_fds(mmgr);
    return mdm_down(&mmgr->info);
}

/**
 * try fo perform a modem escalation recovery
 *
 * @param [in,out] mmgr mmgr context
 *
 * @return E_ERR_FAILED if reset not performed
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t reset_modem(mmgr_data_t *mmgr)
{
    e_escalation_level_t level = E_EL_UNKNOWN;
    e_reset_operation_state_t state = E_OPERATION_UNKNOWN;

    ASSERT(mmgr != NULL);

    /* Do pre-process operation */
    recov_start(mmgr->reset);
    level = recov_get_level(mmgr->reset);
    ASSERT(mmgr->hdler_pre_mdm[level] != NULL);
    mmgr->hdler_pre_mdm[level] (mmgr);

    state = recov_get_state(mmgr->reset);
    if (state == E_OPERATION_SKIP) {
        mdm_close_fds(mmgr);
        goto out_mdm_ev;
    } else if (state == E_OPERATION_WAIT) {
        goto out;
    }

    /* Keep only CORE DUMP state */
    mmgr->info.polled_states = MDM_CTRL_STATE_COREDUMP;
    set_mcd_poll_states(&mmgr->info);
    timer_stop_all(mmgr->timer);

    /* initialize modules */
    mdm_close_fds(mmgr);
    if ((level != E_EL_PLATFORM_REBOOT) &&
        (level != E_EL_MODEM_OUT_OF_SERVICE)) {
        if (E_ERR_SUCCESS != mdm_prepare(&mmgr->info)) {
            LOG_ERROR("modem fw is corrupted. Declare modem OOS");
            /* Set MMGR state to MDM_RESET to call the recovery module and
             * force modem recovery to OOS. By doing so, MMGR will turn off the
             * modem and declare the modem OOS. Clients will not be able to turn
             * on the modem */
            recov_force(mmgr->reset, E_FORCE_OOS);
            return reset_modem(mmgr);
        }
    }

    /* restart modem */
    mdm_prepare_link(&mmgr->info);

    /* The level can change between the pre operation and the operation in a
     * specific case: if we are in PLATFORM_REBOOT state and we reached the
     * maximum allowed attempts */
    level = recov_get_level(mmgr->reset);
    ASSERT(mmgr->hdler_mdm[level] != NULL);
    mmgr->hdler_mdm[level] (&mmgr->info);

    /* configure events handling */
    if ((level == E_EL_PLATFORM_REBOOT) || (level == E_EL_MODEM_OUT_OF_SERVICE))
        goto out;

out_mdm_ev:
    recov_done(mmgr->reset);

    mdm_subscribe_start_ev(&mmgr->info);
    if (!mmgr->info.is_flashless && mmgr->info.ipc_ready_present)
        timer_start(mmgr->timer, E_TIMER_WAIT_FOR_IPC_READY);
    if (mmgr->info.mdm_link == E_LINK_USB) {
        timer_start(mmgr->timer, E_TIMER_WAIT_FOR_BUS_READY);
        mmgr->events.link_state = E_MDM_LINK_NONE;
    }

out:
    return E_ERR_SUCCESS;
}

/**
 * open TTY and configure MUX
 *
 * @param [in,out] mmgr mmgr context
 *
 * @return E_ERR_SUCCESS if successful
 */
static e_mmgr_errors_t configure_modem(mmgr_data_t *mmgr)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;

    ASSERT(mmgr != NULL);

    ret = tty_open(mmgr->info.mdm_ipc_path, &mmgr->fd_tty);
    if (ret != E_ERR_SUCCESS) {
        LOG_ERROR("open fails");
        set_mmgr_state(mmgr, E_MMGR_MDM_RESET);
        goto out;
    }

    ret = switch_to_mux(&mmgr->fd_tty, &mmgr->info);
    if (ret == E_ERR_SUCCESS) {
        LOG_VERBOSE("Switch to MUX succeed");
    } else {
        static const char *const msg = "MUX configuration failed";
        LOG_ERROR("%s. reason=%d", msg, ret);
        static const char *const ev_type = "TFT_ERROR_IPC";
        mmgr_cli_tft_event_data_t data[] = { { strlen(msg), msg } };
        mmgr_cli_tft_event_t ev = { E_EVENT_ERROR,
                                    strlen(ev_type), ev_type,
                                    MMGR_CLI_TFT_AP_LOG_MASK |
                                    MMGR_CLI_TFT_BP_LOG_MASK,
                                    1, data };
        clients_inform_all(mmgr->clients, E_MMGR_NOTIFY_TFT_EVENT, &ev);
        set_mmgr_state(mmgr, E_MMGR_MDM_RESET);
        goto out;
    }
    set_mmgr_state(mmgr, E_MMGR_MDM_UP);
    update_modem_tty(mmgr);

    return ret;

out:
    return ret;
}

static e_mmgr_errors_t cleanup_ipc_event(mmgr_data_t *mmgr)
{
    size_t data_size = READ_SIZE;
    char data[READ_SIZE];
    ssize_t read_size;

    ASSERT(mmgr != NULL);

    /* clean event by reading data */
    do
        read_size = read(mmgr->fd_tty, data, data_size);
    while (read_size > 0);

    mdm_close_fds(mmgr);

    return E_ERR_SUCCESS;
}

/**
 * handle ipc events
 *
 * @param [in,out] mmgr mmgr context
 *
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t ipc_event(mmgr_data_t *mmgr)
{
    static const char *const msg = "IPC hang-up";

    ASSERT(mmgr != NULL);
    cleanup_ipc_event(mmgr);

    clients_inform_all(mmgr->clients, E_MMGR_EVENT_MODEM_DOWN, NULL);
    static const char *const ev_type = "TFT_ERROR_IPC";
    mmgr_cli_tft_event_data_t data[] = { { strlen(msg), msg } };
    mmgr_cli_tft_event_t ev = { E_EVENT_ERROR,
                                strlen(ev_type), ev_type,
                                MMGR_CLI_TFT_AP_LOG_MASK,
                                1, data };
    clients_inform_all(mmgr->clients, E_MMGR_NOTIFY_TFT_EVENT, &ev);

    set_mmgr_state(mmgr, E_MMGR_MDM_RESET);

    return E_ERR_SUCCESS;
}

static e_mmgr_errors_t launch_secur(mmgr_data_t *mmgr)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;
    int fd;

    ASSERT(mmgr != NULL);

    if ((ret = secure_register(mmgr->secure, &fd) == E_ERR_SUCCESS)) {
        if (fd != CLOSED_FD) {
            tty_listen_fd(mmgr->epollfd, fd, EPOLLIN);
            ret = secure_start(mmgr->secure);
        }
    }

    return ret;
}

static e_mmgr_errors_t do_configure(mmgr_data_t *mmgr)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;

    ASSERT(mmgr != NULL);

    if ((ret = configure_modem(mmgr)) == E_ERR_SUCCESS) {
        if (apply_tlv(mmgr))
            timer_start(mmgr->timer, E_TIMER_REBOOT_MODEM_DELAY);
        else
            ret = launch_secur(mmgr);
        clients_inform_all(mmgr->clients, E_MMGR_EVENT_MODEM_UP, NULL);
        pm_on_mdm_up(mmgr->info.pm);
    }

    return ret;
}

/**
 * handle modem control event
 *
 * @param [in,out] mmgr mmgr context
 *
 * @return E_ERR_TTY_BAD_FD failed to open tty. perform a modem reset
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t modem_control_event(mmgr_data_t *mmgr)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;
    e_modem_events_type_t mcd_state;

    ASSERT(mmgr != NULL);

    mdm_get_state(mmgr->info.fd_mcd, &mcd_state);

    if (mcd_state & E_EV_FW_DOWNLOAD_READY) {
        /* manage fw update request */
        LOG_DEBUG("current state: E_EV_FW_DOWNLOAD_READY");
        mmgr->events.link_state |= E_MDM_LINK_FW_DL_READY;
        mmgr->events.link_state &= ~E_MDM_LINK_IPC_READY;
        mmgr->info.polled_states &= ~MDM_CTRL_STATE_FW_DOWNLOAD_READY;
        mmgr->info.polled_states |= MDM_CTRL_STATE_IPC_READY;
        set_mcd_poll_states(&mmgr->info);

        if (((mmgr->info.mdm_link == E_LINK_USB) &&
             mmgr->events.link_state & E_MDM_LINK_FLASH_READY) ||
            (mmgr->info.mdm_link == E_LINK_HSI)) {
            ret = mdm_flash_start(mmgr->mdm_flash);
            timer_start(mmgr->timer, E_TIMER_MDM_FLASHING);
        }
    } else if (mcd_state & E_EV_IPC_READY) {
        LOG_DEBUG("current state: E_EV_IPC_READY");
        timer_stop(mmgr->timer, E_TIMER_WAIT_FOR_IPC_READY);
        mmgr->events.link_state |= E_MDM_LINK_IPC_READY;
        mmgr->info.polled_states &= ~MDM_CTRL_STATE_IPC_READY;
        mmgr->events.link_state &= ~E_MDM_LINK_FW_DL_READY;
        set_mcd_poll_states(&mmgr->info);
        if ((mmgr->events.link_state & E_MDM_LINK_BB_READY) &&
            (mmgr->state == E_MMGR_MDM_CONF_ONGOING))
            ret = do_configure(mmgr);
    } else if (mcd_state & E_EV_CORE_DUMP) {
        LOG_DEBUG("current state: E_EV_CORE_DUMP");
        set_mmgr_state(mmgr, E_MMGR_MDM_CORE_DUMP);
        timer_stop_all(mmgr->timer);

        if (mmgr->fd_tty != CLOSED_FD) {
            clients_inform_all(mmgr->clients, E_MMGR_EVENT_MODEM_DOWN, NULL);
            mdm_close_fds(mmgr);
        }

        mmgr->events.link_state |= E_MDM_LINK_CORE_DUMP_READY;
        mmgr->info.polled_states &= ~MDM_CTRL_STATE_COREDUMP;
        set_mcd_poll_states(&mmgr->info);

        LOG_DEBUG("start timer for core dump ready");
        timer_start(mmgr->timer, E_TIMER_CORE_DUMP_IPC_RESET);
        timer_start(mmgr->timer, E_TIMER_WAIT_CORE_DUMP_READY);

        clients_inform_all(mmgr->clients, E_MMGR_NOTIFY_CORE_DUMP, NULL);
        broadcast_msg(E_MSG_INTENT_CORE_DUMP_WARNING);

        if ((mmgr->info.mdm_link == E_LINK_USB) &&
            !(mmgr->events.link_state & E_MDM_LINK_CORE_DUMP_READ_READY))
            LOG_DEBUG("waiting for bus enumeration");
        else
            core_dump_prepare(mmgr);
    } else if (mcd_state & E_EV_MODEM_SELF_RESET) {
        /* Deregister to WARM boot event or MMGR will receive endlessly
         * this event */
        mmgr->info.polled_states &= ~MDM_CTRL_STATE_WARM_BOOT;
        set_mcd_poll_states(&mmgr->info);

        if (E_MMGR_MDM_CORE_DUMP == mmgr->state) {
            if (E_CD_SUCCEED != mcdr_get_result(mmgr->mcdr)) {
                notify_core_dump(mmgr->clients, NULL, E_CD_SELF_RESET);
                set_mmgr_state(mmgr, E_MMGR_MDM_RESET);
            }
        } else if (E_MMGR_MDM_PREPARE_OFF == mmgr->state) {
            LOG_DEBUG("FMMO: modem is down");
            timer_stop(mmgr->timer, E_TIMER_FMMO);
            mdm_finalize_shtdwn(mmgr);
            set_mmgr_state(mmgr, E_MMGR_MDM_OFF);
        } else {
            clients_inform_all(mmgr->clients, E_MMGR_EVENT_MODEM_DOWN, NULL);
            clients_inform_all(mmgr->clients, E_MMGR_NOTIFY_SELF_RESET,
                               NULL);
            set_mmgr_state(mmgr, E_MMGR_MDM_RESET);
        }
    } else if (mcd_state & E_EV_MODEM_OFF) {
        LOG_DEBUG("modem off. Nothing to do");
    }

    return ret;
}

e_mmgr_errors_t bus_events(mmgr_data_t *mmgr)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;

    ASSERT(mmgr != NULL);

    bus_ev_read(mmgr->events.bus_events);
    if (bus_ev_hdle_events(mmgr->events.bus_events) != E_ERR_SUCCESS) {
        LOG_INFO("bus_ev_hdle_events undefined event");
        goto out;
    }
    if ((bus_ev_get_state(mmgr->events.bus_events) & MDM_BB_READY) &&
        (mmgr->state == E_MMGR_MDM_CONF_ONGOING)) {
        LOG_DEBUG("ready to configure modem");
        timer_stop(mmgr->timer, E_TIMER_WAIT_FOR_BUS_READY);
        mmgr->events.link_state &= ~E_MDM_LINK_FLASH_READY;
        mmgr->events.link_state |= E_MDM_LINK_BB_READY;
        if ((mmgr->events.link_state & E_MDM_LINK_IPC_READY) ||
            (!mmgr->info.ipc_ready_present))
            ret = do_configure(mmgr);
    } else if ((bus_ev_get_state(mmgr->events.bus_events) & MDM_FLASH_READY) &&
               (mmgr->state == E_MMGR_MDM_START)) {
        LOG_DEBUG("ready to flash modem");
        timer_stop(mmgr->timer, E_TIMER_WAIT_FOR_BUS_READY);
        mmgr->events.link_state |= E_MDM_LINK_FLASH_READY;
        mmgr->events.link_state &= ~E_MDM_LINK_BB_READY;
        if (mmgr->events.link_state & E_MDM_LINK_FW_DL_READY) {
            timer_start(mmgr->timer, E_TIMER_MDM_FLASHING);
            mdm_flash_start(mmgr->mdm_flash);
        }
    } else if ((bus_ev_get_state(mmgr->events.bus_events) & MDM_CD_READY) &&
               (mmgr->state == E_MMGR_MDM_CORE_DUMP)) {
        LOG_DEBUG("ready to read a core dump");
        if (mmgr->fd_tty != CLOSED_FD) {
            clients_inform_all(mmgr->clients, E_MMGR_EVENT_MODEM_DOWN, NULL);
            mdm_close_fds(mmgr);
        }

        mmgr->events.link_state |= E_MDM_LINK_CORE_DUMP_READ_READY;
        if (mmgr->events.link_state & E_MDM_LINK_CORE_DUMP_READY)
            core_dump_prepare(mmgr);
    } else {
        LOG_DEBUG("Unhandled usb event");
    }

out:
    return ret;
}

/**
 * initialize modem events handler
 *
 * @param [in,out] mmgr mmgr context
 *
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t modem_events_init(mmgr_data_t *mmgr)
{
    ASSERT(mmgr != NULL);

    mmgr->hdler_pre_mdm[E_EL_MODEM_COLD_RESET] = pre_mdm_cold_reset;
    mmgr->hdler_pre_mdm[E_EL_PLATFORM_REBOOT] = pre_platform_reboot;
    mmgr->hdler_pre_mdm[E_EL_MODEM_OUT_OF_SERVICE] = pre_modem_out_of_service;

    mmgr->hdler_mdm[E_EL_MODEM_COLD_RESET] = mdm_cold_reset;
    mmgr->hdler_mdm[E_EL_PLATFORM_REBOOT] = platform_reboot;
    mmgr->hdler_mdm[E_EL_MODEM_OUT_OF_SERVICE] = out_of_service;

    return E_ERR_SUCCESS;
}
