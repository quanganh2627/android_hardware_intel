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
#include <termios.h>
#include <unistd.h>
#include <cutils/properties.h>
#include "at.h"
#include "common.h"
#include "errors.h"
#include "file.h"
#include "java_intent.h"
#include "logs.h"
#include "modem_events.h"
#include "mux.h"
#include "security.h"
#include "timer_events.h"
#include "tty.h"
#include "mmgr.h"
#include "link.h"
#include "modem_info.h"
#include "client_cnx.h"

/* AT command to shutdown modem */
#define POWER_OFF_MODEM             "AT+CFUN=0\r"
#define NB_CD_LOG_CMDS              4

#define TIMESTAMP_LEN   32
#define READ_SIZE       64
#define AT_CFUN_RETRY   0
#define MAX_TLV         30

/* At cmd to be sent to retrieve core dump logs */
#define CD_DUMPLOG_CMD "at@cdd:dumpLog()\r"

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
 *
 * @return none
 */
void inform_flash_err(const clients_hdle_t *clients,
                      e_modem_fw_error_t flash_err, int attempts, long timer)
{
    static const char *const ev_type = "TFT_STAT_FLASH";

    ASSERT(clients != NULL);

    mmgr_cli_fw_update_result_t fw_result = { .id = flash_err };

    clients_inform_all(clients, E_MMGR_RESPONSE_MODEM_FW_RESULT,
                       &fw_result);

    switch (fw_result.id) {
    case E_MODEM_FW_BAD_FAMILY: {
        static const char *const msg = "Modem FW bad family";
        mmgr_cli_tft_event_data_t data[] = { { strlen(msg), msg } };
        mmgr_cli_tft_event_t ev =
        { E_EVENT_STATS, strlen(ev_type), ev_type, 0, 1, data };
        clients_inform_all(clients, E_MMGR_NOTIFY_TFT_EVENT, &ev);

        broadcast_msg(E_MSG_INTENT_MODEM_FW_BAD_FAMILY);
        break;
    }

    case E_MODEM_FW_SUCCEED: {
        static const char *const msg = "Modem flashing succeed";
        mmgr_cli_tft_event_data_t data[] =
        { { strlen(msg), msg }, { 0, NULL }, { 0, NULL } };
        mmgr_cli_tft_event_t ev =
        { E_EVENT_STATS, strlen(ev_type), ev_type, 0, 3, data };

        data[1].value = calloc(MMGR_CLI_MAX_TFT_EVENT_DATA_LEN, sizeof(char));
        if (data[1].value != NULL) {
            data[1].len = snprintf((char *)data[1].value,
                                   MMGR_CLI_MAX_TFT_EVENT_DATA_LEN, "%d",
                                   attempts);

            data[2].value =
                calloc(MMGR_CLI_MAX_TFT_EVENT_DATA_LEN, sizeof(char));
            if (data[2].value != NULL) {
                data[2].len = snprintf((char *)data[2].value,
                                       MMGR_CLI_MAX_TFT_EVENT_DATA_LEN,
                                       "%ld", timer);
            } else {
                LOG_DEBUG("Memory allocation failed for data 2. Data 2 not set.");
                ev.num_data = 2;
            }
        } else {
            LOG_DEBUG(
                "Memory allocation failed for data 1. Data 1 and 2 not set.");
            ev.num_data = 1;
        }

        clients_inform_all(clients, E_MMGR_NOTIFY_TFT_EVENT, &ev);

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
        clients_inform_all(clients, E_MMGR_NOTIFY_TFT_EVENT, &ev);
        break;
    }

    case E_MODEM_FW_ERROR_UNSPECIFIED: {
        static const char *const msg = "Modem FW unspecified error";
        mmgr_cli_tft_event_data_t data[] = { { strlen(msg), msg } };
        mmgr_cli_tft_event_t ev =
        { E_EVENT_STATS, strlen(ev_type), ev_type, MMGR_CLI_TFT_AP_LOG_MASK, 1,
          data };
        clients_inform_all(clients, E_MMGR_NOTIFY_TFT_EVENT, &ev);
        break;
    }

    case E_MODEM_FW_OUTDATED: {
        static const char *const msg = "Modem FW outdated";
        mmgr_cli_tft_event_data_t data[] = { { strlen(msg), msg } };
        mmgr_cli_tft_event_t ev =
        { E_EVENT_STATS, strlen(ev_type), ev_type, 0, 1, data };
        clients_inform_all(clients, E_MMGR_NOTIFY_TFT_EVENT, &ev);
        broadcast_msg(E_MSG_INTENT_MODEM_FW_OUTDATED);
        break;
    }

    case E_MODEM_FW_SECURITY_CORRUPTED: {
        static const char *const msg = "Modem FW security corrupted";
        mmgr_cli_tft_event_data_t data[] = { { strlen(msg), msg } };
        mmgr_cli_tft_event_t ev =
        { E_EVENT_STATS, strlen(ev_type), ev_type, 0, 1, data };
        clients_inform_all(clients, E_MMGR_NOTIFY_TFT_EVENT, &ev);
        broadcast_msg(E_MSG_INTENT_MODEM_FW_SECURITY_CORRUPTED);
        break;
    }

    case E_MODEM_FW_READY_TIMEOUT: {
    case E_MODEM_FW_NUM:
        /* nothing to do */
        break;
    }
    }
}

/**
 * Check if FLS and TLV files update has failed and inform clients
 *
 * @param [in] mmgr
 *
 * @return none
 */
void inform_upgrade_err(clients_hdle_t *clients, mdm_flash_upgrade_err_t err)
{
    static const char *const ev_type = "TFT_MDM_UPDATE";
    mmgr_cli_tft_event_data_t data[] = { { 0, NULL }, { 0, NULL } };
    int elem_nb = 0;

    ASSERT(clients != NULL);

    if (err & MDM_UPDATE_ERR_FLASH) {
        broadcast_msg(E_MSG_INTENT_MODEM_FW_UPDATE_FAILURE);
        data[elem_nb].value = "FLS UPDATE FAILURE";
        data[elem_nb].len = strlen(data[elem_nb].value);
        elem_nb++;
    }

    if (err & MDM_UPDATE_ERR_TLV) {
        data[elem_nb].value = "TLV UPDATE FAILURE";
        data[elem_nb].len = strlen(data[elem_nb].value);
        elem_nb++;
    }

    mmgr_cli_tft_event_t ev =
    { E_EVENT_ERROR, strlen(ev_type), ev_type, MMGR_CLI_TFT_AP_LOG_MASK,
      elem_nb, data };

    clients_inform_all(clients, E_MMGR_NOTIFY_TFT_EVENT, &ev);
}

/**
 * Apply TLV update. All TLV's available in the folder specified by TCS will be
 * applied
 *
 * @param [in,out] mmgr mmgr context
 *
 * @return true if all TLV files have been applied successfuly
 * @return false otherwise
 */
static bool streamline(mmgr_data_t *mmgr)
{
    const char *filename = NULL;
    mmgr_cli_nvm_update_result_t err;

    ASSERT(mmgr != NULL);

    if ((filename = mdm_flash_streamline(mmgr->flash, &err)) != NULL) {
        static const char *const ev_type = "TFT_ERROR_TLV";
        static const char *const msg = "TLV failure: failed to apply TLV";
        mmgr_cli_tft_event_data_t data[] =
        { { strlen(msg), msg }, { strlen(filename), filename } };
        mmgr_cli_tft_event_t ev = { E_EVENT_ERROR, strlen(ev_type), ev_type,
                                    MMGR_CLI_TFT_AP_LOG_MASK |
                                    MMGR_CLI_TFT_BP_LOG_MASK,
                                    2, data };
        clients_inform_all(mmgr->clients, E_MMGR_NOTIFY_TFT_EVENT, &ev);

        recov_force(mmgr->reset, E_FORCE_OOS);
        set_mmgr_state(mmgr, E_MMGR_MDM_RESET);
    }

    clients_inform_all(mmgr->clients, E_MMGR_RESPONSE_MODEM_NVM_RESULT, &err);

    return E_MODEM_NVM_SUCCEED == err.id;
}

static void core_dump_prepare(mmgr_data_t *mmgr)
{
    char mdm_version[PROPERTY_VALUE_MAX];

    ASSERT(mmgr != NULL);

    /* Get the modem FW version. */
    property_get(key_get_mdm_version(mmgr->keys), mdm_version, "");

    mcdr_set_mdm_version(mmgr->mcdr, mdm_version);

    if (mcdr_is_enabled(mmgr->mcdr)) {
        timer_stop(mmgr->timer, E_TIMER_CORE_DUMP_IPC_RESET);
        timer_stop(mmgr->timer, E_TIMER_WAIT_CORE_DUMP_READY);
        timer_start(mmgr->timer, E_TIMER_CORE_DUMP_READING);

        link_on_cd(mmgr->link);
        mcdr_start(mmgr->mcdr);
        mmgr->cd_retrieved = true;
    } else {
        timer_stop_all(mmgr->timer);
        set_mmgr_state(mmgr, E_MMGR_MDM_RESET);
    }
}

void core_dump_finalize(mmgr_data_t *mmgr, e_core_dump_state_t state)
{
    ASSERT(mmgr != NULL);

    link_on_cd_complete(mmgr->link);

    broadcast_msg(E_MSG_INTENT_CORE_DUMP_COMPLETE);
    notify_core_dump(mmgr->clients, mmgr->mcdr, state);

    mdm_mcd_register(mmgr->mcd, MDM_CTRL_STATE_IPC_READY, false);
    mdm_mcd_unregister(mmgr->mcd, MDM_CTRL_STATE_WARM_BOOT);

    if (E_LINK_USB == link_get_bb_type(mmgr->link))
        mmgr->events.link_state = E_MDM_LINK_NONE;

    /* The modem will be reset. No need to launch
     * a timer */
    set_mmgr_state(mmgr, E_MMGR_MDM_RESET);
}

/**
 * Callback function used to write strings onto log file.
 * Newline separator characters are added at the end of received strings.
 *
 * @param [in] ctx Input argument, here it is the dest file fd.
 * @param [in] pszResp The string to write onto dest file.
 * @param [in/out] len The number of bytes to be written on dest file.
 *                     Updated with the number of bytes actually written onto
 *                     dest file.
 *
 * @return 0 If data were copied successfully onto file.
 * @return 1 If completion/termination string was found.
 * @return -1 If a critical error happened.
 */
int write_to_cd_log_file(void *ctx, const char *pszResp, size_t *len)
{
    int ret = 0;
    int fs_fd = -1;
    size_t written = 0;
    const char *cd_log_termination = "*** CDD log dump done ***";
    const char *line_separator = "\r\n";
    char *pTermination = NULL;

    ASSERT(pszResp != NULL);

    if ((ctx == NULL) || (len == NULL) || (pszResp == NULL)) {
        ret = -1;
        LOG_ERROR("Error bad arguments.");
        goto Exit;
    }

    pTermination = strstr(pszResp, cd_log_termination);
    if (pTermination != NULL) {
        // Only write data up to the end of termination string
        *len = pTermination - pszResp + strlen(cd_log_termination);
        ret = 1;
        LOG_INFO("Termination:%s found, END of core dump log detected!",
                 cd_log_termination);
    }

    LOG_DEBUG("Trying to write %d Bytes...", *len);

    fs_fd = *(int *)ctx;
    if (fs_fd > 0) {
        errno = 0;
        written = write(fs_fd, pszResp, *len);
        if (written > 0) {
            *len = written;
            // Add line separator onto file
            written = write(fs_fd, line_separator, strlen(line_separator));
            *len += written;
            LOG_INFO("Success to write %d Bytes onto CD log file.", *len);
        } else {
            LOG_ERROR("Failed to write %d Bytes onto CD log file. Error %d:%s",
                      *len, errno, strerror(errno));
            *len = 0;
            ret = -1;
            goto Exit;
        }
    } else {
        LOG_ERROR("Error with core dump log file descriptor: Bad value.");
        *len = 0;
        ret = -1;
        goto Exit;
    }

Exit:
    return ret;
}

/**
 * Initialize core dump logs
 *
 * @param [in,out] mmgr mmgr context
 *
 * @return E_ERR_FAILED if reset not performed
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t mdm_start_core_dump_logs(mmgr_data_t *mmgr)
{
    int fd = CLOSED_FD;
    e_mmgr_errors_t ret = E_ERR_FAILED;
    const char *path = NULL;
    const char *filename = NULL;
    char cd_log_basename[PATH_MAX] = { 0 };
    char cd_log_file[PATH_MAX] = { 0 };
    int dir_fd = -1;
    int fs_fd = -1;
    const char *mdm_version = NULL;
    const char *core_dump_log_port = NULL;
    char timestamp[TIMESTAMP_LEN] = { "00000000000000" };

    ASSERT(mmgr != NULL);

    mdm_version = mcdr_get_mdm_version(mmgr->mcdr);
    core_dump_log_port = mdm_dlc_get_cd_logs(mmgr->mdm_dlc);

    ASSERT(core_dump_log_port != NULL);

    ret = tty_open(core_dump_log_port, &fd);
    if (fd <= 0) {
        LOG_ERROR("Opening TTY FAILED, err:%d", ret);
        goto Exit;
    }

    if (!mmgr->mcdr) {
        LOG_ERROR("Failed to find mcdr handle.");
        goto Exit;
    }
    path = mcdr_get_path(mmgr->mcdr);
    if (path == NULL) {
        LOG_ERROR("Cannot retrieve core dump path.");
        goto Exit;
    }
    errno = 0;
    dir_fd = open(path, O_DIRECTORY);
    if (dir_fd <= 0) {
        LOG_ERROR("Cannot open core dump path (%s), errno:%s",
                  path, strerror(errno));
        goto Exit;
    }
    filename = mcdr_get_filename(mmgr->mcdr);
    if ((filename == NULL) || (strlen(filename) == 0)) {
        /* Core dump file was NOT generated, create cd debug log file name. */
        generate_timestamp(timestamp, TIMESTAMP_LEN);
        snprintf(cd_log_basename, PATH_MAX, "cd_debug_%s_%s",
                 mdm_version, timestamp);
    } else {
        /* Remove the '.tar.gz' extension characters */
        char *pExtension;
        snprintf(cd_log_basename, sizeof(cd_log_basename), "%s", filename);
        pExtension = strstr(cd_log_basename, ".tar.gz");
        if (pExtension != NULL)
            pExtension[0] = '\0';
    }

    /* Retrieve core dump log file */
    snprintf(cd_log_file, sizeof(cd_log_file), "%s.txt", cd_log_basename);
    fs_fd = openat(dir_fd, cd_log_file, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);

    if (fs_fd <= 0) {
        LOG_ERROR("Cannot open CD log file:%s", cd_log_file);
        goto Exit;
    }

    int read_status;
    do {
        errno = 0;
        ret = send_at_retry(fd, CD_DUMPLOG_CMD, strlen(CD_DUMPLOG_CMD),
                            0, AT_ANSWER_NO_TIMEOUT);
        if (ret != E_ERR_SUCCESS) {
            LOG_ERROR("Failed to send %s AT command, errno:%d:%s",
                      CD_DUMPLOG_CMD, errno, strerror(errno));
            goto Exit;
        }
        read_status = read_cd_logs(fd, fs_fd, write_to_cd_log_file);
    } while (read_status == -1);

    if (read_status) {
        LOG_ERROR("Failed to read core dump logs.");
        ret = E_ERR_FAILED;
        goto Exit;
    }
    /* Close/Sync core dump log file */
    if (fs_fd > 0) {
        fsync(fs_fd);
        close(fs_fd);
        fs_fd = -1;
    }
    /* Flush data in TTY */
    tcflush(fd, TCIOFLUSH);

    LOG_INFO("Core dump log %s retrieved.", cd_log_file);

Exit:
    if (fd > 0)
        tty_close(&fd);
    if (fs_fd > 0) {
        fsync(fs_fd);
        close(fs_fd);
    }
    if (dir_fd > 0)
        close(dir_fd);

    return ret;
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
    if (E_ERR_SUCCESS == ret)
        ret = mdm_mcd_register(mmgr->mcd, MDM_CTRL_STATE_COREDUMP |
                               MDM_CTRL_STATE_OFF | MDM_CTRL_STATE_WARM_BOOT |
                               MDM_CTRL_STATE_COLD_BOOT, true);

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
    ASSERT(mmgr != NULL);

    e_reset_operation_state_t current_state = recov_get_state(mmgr->reset);

    if ((current_state == E_OPERATION_NONE) &&
        (clients_get_connected(mmgr->clients) != 0)) {
        LOG_DEBUG("need to ack all clients");

        recov_set_state(mmgr->reset, E_OPERATION_WAIT);

        clients_inform_all(mmgr->clients, E_MMGR_EVENT_MODEM_DOWN, NULL);
        clients_inform_all(mmgr->clients, E_MMGR_NOTIFY_MODEM_COLD_RESET,
                           NULL);
        set_mmgr_state(mmgr, E_MMGR_WAIT_COLD_ACK);
        timer_start(mmgr->timer, E_TIMER_COLD_RESET_ACK);
    } else if ((current_state == E_OPERATION_WAIT) ||
               (clients_get_connected(mmgr->clients) == 0)) {
        recov_set_state(mmgr->reset, E_OPERATION_CONTINUE);

        if (recov_get_operation(mmgr->reset) != E_FORCE_NO_COUNT)
            broadcast_msg(E_MSG_INTENT_MODEM_COLD_RESET);

        if ((E_LINK_USB == link_get_bb_type(mmgr->link)) &&
            mdm_flash_is_required(mmgr->flash))
            set_mmgr_state(mmgr, E_MMGR_MDM_START);
        else
            set_mmgr_state(mmgr, E_MMGR_MDM_CONF_ONGOING);

        if (clients_get_connected(mmgr->clients) != 0) {
            clients_reset_ack_cold(mmgr->clients);
            timer_stop(mmgr->timer, E_TIMER_COLD_RESET_ACK);
        }
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
    int reboot_counter;

    ASSERT(mmgr != NULL);

    reboot_counter = recov_get_reboot(mmgr->reset);

    recov_set_state(mmgr->reset, E_OPERATION_CONTINUE);
    if (reboot_counter >= recov_get_retry_allowed(mmgr->reset)) {
        /* go to next level */
        LOG_INFO("Reboot cancelled. Max value reached");
        recov_next(mmgr->reset);
        pre_modem_out_of_service(mmgr);
    } else {
        recov_set_reboot(mmgr->reset, ++reboot_counter);

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

    mdm_mcd_register(mmgr->mcd, MDM_CTRL_STATE_WARM_BOOT |
                     MDM_CTRL_STATE_COREDUMP, true);

    tty_open(mdm_dlc_get_shutdown(mmgr->mdm_dlc), &fd);
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
    /* Reset state at this stage because we won't call recov_done anymore */
    recov_set_state(mmgr->reset, E_OPERATION_NONE);
    mdm_close_fds(mmgr);
    return mdm_mcd_down(mmgr->mcd);
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

    ASSERT(mmgr != NULL);

    if ((level != E_EL_PLATFORM_REBOOT) &&
        (level != E_EL_MODEM_OUT_OF_SERVICE)) {
        if (E_ERR_SUCCESS != mdm_flash_prepare(mmgr->flash)) {
            LOG_ERROR("modem firmware is corrupted");
            recov_force(mmgr->reset, E_FORCE_OOS);
        }
    }

    /* Do pre-process operation */
    recov_start(mmgr->reset);
    level = recov_get_level(mmgr->reset);
    ASSERT(mmgr->hdler_pre_mdm[level] != NULL);
    mmgr->hdler_pre_mdm[level] (mmgr);

    if (E_OPERATION_WAIT == recov_get_state(mmgr->reset))
        goto out;

    if ((level == E_EL_PLATFORM_REBOOT) || (level == E_EL_MODEM_OUT_OF_SERVICE))
        /* In PLATFORM_REBOOT or MODEM_OOS state, ignore MCD events */
        mdm_mcd_register(mmgr->mcd, MDM_CTRL_STATE_UNKNOWN, true);
    else
        /* Keep only CORE DUMP state */
        mdm_mcd_register(mmgr->mcd, MDM_CTRL_STATE_COREDUMP, true);
    timer_stop_all(mmgr->timer);

    /* initialize modules */
    mdm_close_fds(mmgr);

    link_on_mdm_reset(mmgr->link, 0);

    /* The level can change between the pre operation and the operation in a
     * specific case: if we are in PLATFORM_REBOOT state and we reached the
     * maximum allowed attempts */
    level = recov_get_level(mmgr->reset);
    ASSERT(mmgr->hdler_mdm[level] != NULL);
    mmgr->hdler_mdm[level] (mmgr->mcd);

    /* configure events handling */
    if ((level == E_EL_PLATFORM_REBOOT) || (level == E_EL_MODEM_OUT_OF_SERVICE))
        goto out;

    recov_done(mmgr->reset);

    if (mdm_flash_is_required(mmgr->flash))
        mdm_mcd_register(mmgr->mcd, MDM_CTRL_STATE_FW_DOWNLOAD_READY |
                         MDM_CTRL_STATE_COREDUMP, true);
    else
        mdm_mcd_register(mmgr->mcd, MDM_CTRL_STATE_IPC_READY |
                         MDM_CTRL_STATE_COREDUMP, true);

    if (!mdm_flash_is_required(mmgr->flash) &&
        mdm_mcd_is_ipc_ready_present(mmgr->mcd))
        timer_start(mmgr->timer, E_TIMER_WAIT_FOR_IPC_READY);
    if (E_LINK_USB == link_get_bb_type(mmgr->link)) {
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

    const char *mdm_bb_path = link_get_bb_interface(mmgr->link);
    ret = tty_open(mdm_bb_path, &mmgr->fd_tty);
    if (ret != E_ERR_SUCCESS) {
        LOG_ERROR("open fails");
        set_mmgr_state(mmgr, E_MMGR_MDM_RESET);
        goto out;
    }

    ret = switch_to_mux(&mmgr->fd_tty, mdm_bb_path,
                        link_get_bb_type(mmgr->link),
                        mdm_dlc_get_mux_cfg(mmgr->mdm_dlc),
                        mdm_dlc_get_sanity(mmgr->mdm_dlc),
                        &mmgr->wakeup_cfg);
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
    clients_inform_all(mmgr->clients, E_MMGR_EVENT_MODEM_DOWN, NULL);

    cleanup_ipc_event(mmgr);
    static const char *const ev_type = "TFT_ERROR_IPC";
    mmgr_cli_tft_event_data_t data[] = { { strlen(msg), msg },
                                         { 0, NULL } };
    mmgr_cli_tft_event_t ev = { E_EVENT_ERROR,
                                strlen(ev_type), ev_type,
                                MMGR_CLI_TFT_AP_LOG_MASK,
                                2, data };

    switch (mmgr->wakeup_cfg) {
    case E_MDM_WAKEUP_OUTBAND:
        data[1].value = "Streamline OUTBAND";
        break;
    case E_MDM_WAKEUP_INBAND:
        data[1].value = "Streamline INBAND";
        break;
    default:
        data[1].value = "Streamline UNKNOWN";
        break;
    }
    data[1].len = strlen(data[1].value);

    clients_inform_all(mmgr->clients, E_MMGR_NOTIFY_TFT_EVENT, &ev);

    if (E_MMGR_MDM_PREPARE_OFF == mmgr->state) {
        LOG_DEBUG("FMMO: modem down");
        timer_stop(mmgr->timer, E_TIMER_FMMO);
        mdm_finalize_shtdwn(mmgr);
        set_mmgr_state(mmgr, E_MMGR_MDM_OFF);
    } else {
        set_mmgr_state(mmgr, E_MMGR_MDM_RESET);
    }

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
        if (streamline(mmgr)) {
            clients_inform_all(mmgr->clients, E_MMGR_EVENT_MODEM_NFLUSH, NULL);
            timer_start(mmgr->timer, E_TIMER_REBOOT_MODEM_DELAY);
        } else {
            ret = launch_secur(mmgr);
            clients_inform_all(mmgr->clients, E_MMGR_EVENT_MODEM_UP, NULL);
        }

        LOG_DEBUG("state:%d, events state:%d, events link state:%d",
                  mmgr->state, mmgr->events.state, mmgr->events.link_state);

        if ((mmgr->mcdr) && (mmgr->cd_retrieved)) {
            mmgr->cd_retrieved = false;

            if (mcdr_log_is_enabled(mmgr->mcdr) &&
                (E_CD_SUCCEED != mcdr_get_result(mmgr->mcdr)))
                if (E_ERR_SUCCESS != mdm_start_core_dump_logs(mmgr))
                    LOG_ERROR("ERROR creating core dump log file.");
        }

        link_on_mdm_up(mmgr->link);
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

    ASSERT(mmgr != NULL);

    e_modem_events_type_t mcd_state = mdm_mcd_get_state(mmgr->mcd);

    if (mcd_state & E_EV_FW_DOWNLOAD_READY) {
        e_link_t ebl_type = link_get_flash_ebl_type(mmgr->link);
        /* manage fw update request */
        mmgr->events.link_state |= E_MDM_LINK_FW_DL_READY;
        mmgr->events.link_state &= ~E_MDM_LINK_IPC_READY;
        mdm_mcd_unregister(mmgr->mcd, MDM_CTRL_STATE_FW_DOWNLOAD_READY);

        if ((E_LINK_USB != ebl_type) ||
            (mmgr->events.link_state & E_MDM_LINK_FLASH_READY)) {
            if (E_ERR_SUCCESS == (ret = mdm_flash_start(mmgr->flash)))
                timer_start(mmgr->timer, E_TIMER_MDM_FLASHING);
            else
                set_mmgr_state(mmgr, E_MMGR_MDM_RESET);
        }
    } else if (mcd_state & E_EV_IPC_READY) {
        e_link_t bb_type = link_get_bb_type(mmgr->link);
        timer_stop(mmgr->timer, E_TIMER_WAIT_FOR_IPC_READY);
        mmgr->events.link_state |= E_MDM_LINK_IPC_READY;
        mmgr->events.link_state &= ~E_MDM_LINK_FW_DL_READY;
        mdm_mcd_unregister(mmgr->mcd, MDM_CTRL_STATE_IPC_READY);

        if ((mmgr->state == E_MMGR_MDM_CONF_ONGOING) &&
            ((E_LINK_USB != bb_type) ||
             (mmgr->events.link_state & E_MDM_LINK_BB_READY)))
            ret = do_configure(mmgr);
    } else if (mcd_state & E_EV_CORE_DUMP) {
        e_link_t cd_type = link_get_cd_type(mmgr->link);

        set_mmgr_state(mmgr, E_MMGR_MDM_CORE_DUMP);
        timer_stop_all(mmgr->timer);

        if (mmgr->fd_tty != CLOSED_FD) {
            clients_inform_all(mmgr->clients, E_MMGR_EVENT_MODEM_DOWN, NULL);
            mdm_close_fds(mmgr);
        }

        mmgr->events.link_state |= E_MDM_LINK_CORE_DUMP_READY;
        mdm_mcd_unregister(mmgr->mcd, MDM_CTRL_STATE_COREDUMP);

        LOG_DEBUG("start timer for core dump ready");
        timer_start(mmgr->timer, E_TIMER_CORE_DUMP_IPC_RESET);
        timer_start(mmgr->timer, E_TIMER_WAIT_CORE_DUMP_READY);

        clients_inform_all(mmgr->clients, E_MMGR_NOTIFY_CORE_DUMP, NULL);
        broadcast_msg(E_MSG_INTENT_CORE_DUMP_WARNING);

        if ((E_LINK_USB == cd_type) &&
            !(mmgr->events.link_state & E_MDM_LINK_CORE_DUMP_READ_READY))
            LOG_DEBUG("waiting for bus enumeration");
        else
            core_dump_prepare(mmgr);
    } else if (mcd_state & E_EV_MODEM_SELF_RESET) {
        /* Deregister to WARM boot event or MMGR will receive endlessly
         * this event */
        mdm_mcd_unregister(mmgr->mcd, MDM_CTRL_STATE_WARM_BOOT);

        if (E_MMGR_MDM_CORE_DUMP == mmgr->state) {
            if (E_CD_SUCCEED != mcdr_get_result(mmgr->mcdr)) {
                if (mcdr_is_enabled(mmgr->mcdr))
                    mcdr_cancel(mmgr->mcdr);
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
    static bool is_mdm_bb_ready = false;

    ASSERT(mmgr != NULL);

    bus_ev_read(mmgr->events.bus_events);
    if (bus_ev_hdle_events(mmgr->events.bus_events) != E_ERR_SUCCESS)
        goto out;

    if ((bus_ev_get_state(mmgr->events.bus_events) & MDM_BB_READY) &&
        (mmgr->state == E_MMGR_MDM_CONF_ONGOING)) {
        is_mdm_bb_ready = true;
        LOG_DEBUG("ready to configure modem");
        timer_stop(mmgr->timer, E_TIMER_WAIT_FOR_BUS_READY);
        mmgr->events.link_state &= ~E_MDM_LINK_FLASH_READY;
        mmgr->events.link_state |= E_MDM_LINK_BB_READY;
        if ((mmgr->events.link_state & E_MDM_LINK_IPC_READY) ||
            (!mdm_mcd_is_ipc_ready_present(mmgr->mcd)))
            ret = do_configure(mmgr);
    } else if ((bus_ev_get_state(mmgr->events.bus_events) & MDM_BB_READY) &&
               (mmgr->state == E_MMGR_MDM_LINK_USB_DISC)) {
        is_mdm_bb_ready = true;
        LOG_DEBUG("ready to configure modem which is not flashless");
        timer_stop(mmgr->timer, E_TIMER_WAIT_FOR_BUS_READY);
        timer_stop(mmgr->timer, E_TIMER_WAIT_CORE_DUMP_READY);
        mmgr->events.link_state |= E_MDM_LINK_BB_READY;
        if ((mmgr->events.link_state & E_MDM_LINK_IPC_READY) ||
            (!mdm_mcd_is_ipc_ready_present(mmgr->mcd)))
            ret = do_configure(mmgr);
    } else if ((bus_ev_get_state(mmgr->events.bus_events) & MDM_FLASH_READY) &&
               (mmgr->state == E_MMGR_MDM_START)) {
        LOG_DEBUG("ready to flash modem");
        timer_stop(mmgr->timer, E_TIMER_WAIT_FOR_BUS_READY);
        mmgr->events.link_state |= E_MDM_LINK_FLASH_READY;
        mmgr->events.link_state &= ~E_MDM_LINK_BB_READY;
        if ((mmgr->events.link_state & E_MDM_LINK_FW_DL_READY) ||
            (!mdm_mcd_is_ipc_ready_present(mmgr->mcd))) {
            if (E_ERR_SUCCESS == (ret = mdm_flash_start(mmgr->flash)))
                timer_start(mmgr->timer, E_TIMER_MDM_FLASHING);
            else
                set_mmgr_state(mmgr, E_MMGR_MDM_RESET);
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
    } else if ((bus_ev_get_state(mmgr->events.bus_events) & MDM_CD_READY) &&
               (mmgr->state == E_MMGR_MDM_LINK_USB_DISC)) {
        LOG_DEBUG("ready to read a core dump for modem whose "
                  "core dump hardware signal is absent or failed to work");
        set_mmgr_state(mmgr, E_MMGR_MDM_CORE_DUMP);
        timer_stop_all(mmgr->timer);

        if (mmgr->fd_tty != CLOSED_FD) {
            clients_inform_all(mmgr->clients, E_MMGR_EVENT_MODEM_DOWN, NULL);
            mdm_close_fds(mmgr);
        }

        timer_start(mmgr->timer, E_TIMER_CORE_DUMP_IPC_RESET);

        clients_inform_all(mmgr->clients, E_MMGR_NOTIFY_CORE_DUMP, NULL);
        broadcast_msg(E_MSG_INTENT_CORE_DUMP_WARNING);

        mmgr->events.link_state |= E_MDM_LINK_CORE_DUMP_READY;
        core_dump_prepare(mmgr);
    } else if (!(bus_ev_get_state(mmgr->events.bus_events) & MDM_BB_READY) &&
               is_mdm_bb_ready) {
        is_mdm_bb_ready = false;
        if ((mmgr->state == E_MMGR_MDM_UP) ||
            (mmgr->state == E_MMGR_WAIT_COLD_ACK) ||
            (mmgr->state == E_MMGR_WAIT_SHT_ACK) ||
            (mmgr->state == E_MMGR_MDM_CONF_ONGOING)) {
            LOG_DEBUG("found Modem base band USB disconnection");
            clients_inform_all(mmgr->clients, E_MMGR_EVENT_MODEM_DOWN, NULL);
            if (mmgr->fd_tty != CLOSED_FD)
                mdm_close_fds(mmgr);
            timer_stop(mmgr->timer, E_TIMER_WAIT_CORE_DUMP_READY);
            timer_start(mmgr->timer, E_TIMER_WAIT_CORE_DUMP_READY);
            set_mmgr_state(mmgr, E_MMGR_MDM_LINK_USB_DISC);
        }
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

    mmgr->hdler_mdm[E_EL_MODEM_COLD_RESET] = mdm_mcd_cold_reset;
    mmgr->hdler_mdm[E_EL_PLATFORM_REBOOT] = platform_reboot;
    mmgr->hdler_mdm[E_EL_MODEM_OUT_OF_SERVICE] = mdm_mcd_off;

    return E_ERR_SUCCESS;
}
