/* Modem Manager - modem info source file
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
#include <ctype.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <linux/hsi_ffl_tty.h>
#include <linux/mdm_ctrl.h>
#include <time.h>
#include "at.h"
#include "errors.h"
#include "logs.h"
#include "modem_info.h"
#include "modem_specific.h"
#include "reset_escalation.h"
#include "mux.h"
#include "tty.h"

#define AT_XLOG_GET "AT+XLOG=0\r"
#define AT_XLOG_RESET "AT+XLOG=2\r"
#define AT_XLOG_TIMEOUT 2500
#define AT_ANSWER_SIZE 254

/* switch to MUX timings */
#define STAT_DELAY 250          /* in milliseconds */
#define MAX_TIME_DELAY 4        /* in seconds */

typedef enum e_switch_to_mux_states {
    E_MUX_HANDSHAKE,
    E_MUX_XLOG,
    E_MUX_AT_CMD,
    E_MUX_DRIVER,
} e_switch_to_mux_states_t;

/**
 * initialize modem info structure and mcdr
 *
 * @param [in] mdm_info mmgr config
 * @param [in] com
 * @param [in] tlv
 * @param [in] mdm_link
 * @param [in] ch channel
 * @param [in] flash
 * @param [in,out] info modem info
 *
 * @return E_ERR_FAILED if mcdr init fails
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t modem_info_init(mdm_info_t *mdm_info, mmgr_com_t *com,
                                tlv_info_t *tlv,
                                mmgr_mdm_link_t *mdm_link, channels_t *ch,
                                mmgr_flashless_t *flash, mmgr_mcd_t *mcd,
                                modem_info_t *info)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;

    ASSERT(mdm_info != NULL);
    ASSERT(com != NULL);
    ASSERT(tlv != NULL);
    ASSERT(mdm_link != NULL);
    ASSERT(info != NULL);

    info->polled_states = MDM_CTRL_STATE_COREDUMP;
    info->is_flashless = mdm_info->flashless;
    info->mux = com->mux;
    info->ipc_ready_present = mcd->ipc_ready_present;

    info->cd_link = mdm_info->ipc_cd;
    info->mdm_link = mdm_info->ipc_mdm;
    switch (mdm_link->baseband.type) {
    case E_LINK_HSI:
        strncpy(info->mdm_ipc_path, mdm_link->baseband.hsi.device,
                sizeof(info->mdm_ipc_path));
        break;
    case E_LINK_USB:
        strncpy(info->mdm_ipc_path, mdm_link->baseband.usb.device,
                sizeof(info->mdm_ipc_path));
        break;
    default:
        LOG_ERROR("type not handled");
        ret = E_ERR_FAILED;
        goto out;
    }

    /* @TODO: if not DLC, this code should be updated */
    strncpy(info->sanity_check_dlc, ch->sanity_check.device,
            sizeof(info->sanity_check_dlc) - 1);
    strncpy(info->mdm_custo_dlc, ch->mdm_custo.device,
            sizeof(info->mdm_custo_dlc) - 1);
    strncpy(info->shtdwn_dlc, ch->shutdown.device,
            sizeof(info->shtdwn_dlc) - 1);

    if (!strncmp(info->sanity_check_dlc, "", sizeof(info->sanity_check_dlc)) ||
        !strncmp(info->mdm_custo_dlc, "", sizeof(info->mdm_custo_dlc)) ||
        !strncmp(info->shtdwn_dlc, "", sizeof(info->shtdwn_dlc))) {
        LOG_ERROR("empty DLC");
        ret = E_ERR_FAILED;
        goto out;
    }

    strncpy(info->mdm_name, mdm_info->name, sizeof(info->mdm_name) - 1);
    if (tlv->folder[0] == '\0') {
        LOG_ERROR("TLV path not set");
        ret = E_ERR_FAILED;
        goto out;
    } else {
        strncpy(info->tlv_path, tlv->folder, sizeof(info->tlv_path) - 1);
    }

    info->fl_conf = *flash;
    info->fd_mcd = open(MBD_DEV, O_RDWR);
    if (info->fd_mcd == -1) {
        LOG_DEBUG("failed to open Modem Control Driver interface: %s",
                  strerror(errno));
        ret = E_ERR_FAILED;
        goto out;
    }

    /* REVERT ME: 7260 Enumeration Bug: BZ 166282 */
    info->need_warmreset = (strstr(mdm_info->name, "7260") != NULL &&
                            strcmp(mdm_info->hw_revision, "20") == 0 &&
                            strcmp(mdm_info->sw_revision, "1.0") == 0);

    ret = mdm_specific_init(info);
out:
    return ret;
}

/**
 * This functions disposes the modem info module
 * @TODO: update this module to use getters/setters
 *
 * @param [in] info
 *
 * @return E_ERR_FAILED if mcdr init fails
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t modem_info_dispose(modem_info_t *info)
{
    /* do not use ASSERT in dispose function */

    if (info && info->fd_mcd != CLOSED_FD) {
        close(info->fd_mcd);
        info->fd_mcd = CLOSED_FD;
    }

    return mdm_specific_dispose(info);
}

/**
 * Log the self reset reason
 *
 * @param [in] fd_tty tty file descriptor
 * @param [in] max_retry
 *
 * @return E_ERR_TTY_BAD_FD
 * @return E_ERR_TTY_POLLHUP if a pollhup occurs
 * @return E_ERR_FAILED error during write
 * @return E_ERR_TTY_TIMEOUT no response from modem
 * @return E_ERR_SUCCESS if successful
 */
static e_mmgr_errors_t run_at_xlog(int fd_tty, int max_retry)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;
    char data[AT_ANSWER_SIZE + 1];
    int read_size = 0;
    char *p = NULL;

    tcflush(fd_tty, TCIOFLUSH);

    LOG_DEBUG("Sending %s", AT_XLOG_GET);

    ret = tty_write(fd_tty, AT_XLOG_GET, strlen(AT_XLOG_GET));
    if (ret != E_ERR_SUCCESS)
        goto out_xlog;

    memset(data, 0, AT_ANSWER_SIZE + 1);

    ret = tty_wait_for_event(fd_tty, AT_XLOG_TIMEOUT);
    if (ret != E_ERR_SUCCESS)
        goto out_xlog;

    do {
        read_size = AT_ANSWER_SIZE;
        ret = tty_read(fd_tty, data, &read_size, AT_READ_MAX_RETRIES);
        data[read_size] = '\0';
        if (ret != E_ERR_SUCCESS)
            goto out_xlog;

        if (read_size > 0) {
            data[read_size] = '\0';
            /* erasing \r characters to improve display */
            while ((p = strchr(data, '\r')) != NULL)
                *p = ' ';
            /* to improve display, do not use LOG_DEBUG macro here */
            LOGD("%s", data);
        }
    } while (read_size > 0);

    tcflush(fd_tty, TCIFLUSH);
    ret = send_at_retry(fd_tty, AT_XLOG_RESET, strlen(AT_XLOG_RESET),
                        max_retry, AT_ANSWER_SHORT_TIMEOUT);
out_xlog:
    return ret;
}

/**
 * Activate the MUX
 *
 * To activate the MUX this function will do the following step
 *      1. Ping modem port and wait good response
 *      2. Send AT+XLOG to print modem info and get panic id if available
 *      3. Send AT+CMUX command
 *      4. Configure the MUX driver
 *      5. Wait creation of all MUX tty ports
 *
 * @param [in,out] fd_tty modem file descriptor
 * @param [in] info modem information
 *
 * @return E_ERR_TTY_BAD_FD bad file descriptor
 * @return E_ERR_TTY_POLLHUP POLLHUP detected during read
 * @return E_ERR_FAILED bad driver configuration or error during write
 * @return E_ERR_TTY_TIMEOUT no response from modem
 * @return E_ERR_SUCCESS
 */
e_mmgr_errors_t switch_to_mux(int *fd_tty, modem_info_t *info)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    e_switch_to_mux_states_t state;
    struct timespec current, start;
    bool retry_bad_fd_done = false;

    ASSERT(fd_tty != NULL);
    ASSERT(info != NULL);

    for (state = E_MUX_HANDSHAKE; state != E_MUX_DRIVER; /* none */) {
        switch (state) {
        case E_MUX_HANDSHAKE:
            ret = modem_handshake(*fd_tty, info->mux.retry);
            break;
        case E_MUX_XLOG:
            ret = run_at_xlog(*fd_tty, info->mux.retry);
            break;
        case E_MUX_AT_CMD:
            ret = send_at_cmux(*fd_tty, &info->mux);
            break;
        case E_MUX_DRIVER:
            /* nothing to do here */
            break;
        default:
            LOG_ERROR("bad state");
            ret = E_ERR_FAILED;
            goto out;
        }

        if (ret == E_ERR_SUCCESS) {
            /* states are ordered. go to next one */
            state++;
        } else if ((ret == E_ERR_TTY_BAD_FD) && (retry_bad_fd_done == false)) {
            LOG_DEBUG("reopening tty device: %s", info->mdm_ipc_path);
            retry_bad_fd_done = true;
            tty_close(fd_tty);
            if ((ret = tty_open(info->mdm_ipc_path, fd_tty)) != E_ERR_SUCCESS)
                goto out;
        } else {
            ret = E_ERR_FAILED;
            goto out;
        }
    }

    ret = configure_cmux_driver(*fd_tty, info->mux.frame_size);
    if (ret != E_ERR_SUCCESS)
        goto out;

    /* Wait to be able to open a GSM TTY before sending MODEM_UP to clients
     * (this guarantees that the MUX control channel has been established with
     * the modem). Will retry for up to MAX_TIME_DELAY seconds. */
    LOG_DEBUG("looking for TTY %s", info->sanity_check_dlc);
    ret = E_ERR_FAILED;
    clock_gettime(CLOCK_MONOTONIC, &start);
    do {
        int tmp_fd;

        usleep(STAT_DELAY * 1000);
        if ((tmp_fd = open(info->sanity_check_dlc, O_RDWR)) >= 0) {
            close(tmp_fd);
            ret = E_ERR_SUCCESS;
            break;
        }

        clock_gettime(CLOCK_MONOTONIC, &current);
    } while ((current.tv_sec < (start.tv_sec + MAX_TIME_DELAY)) ||
             ((current.tv_sec == (start.tv_sec + MAX_TIME_DELAY)) &&
              (current.tv_nsec < start.tv_nsec)));

    if (ret != E_ERR_SUCCESS) {
        LOG_ERROR("was not able to open TTY %s", info->sanity_check_dlc);
    } else {
        LOG_DEBUG("TTY %s open success", info->sanity_check_dlc);
        /* It's necessary to reset the terminal configuration after MUX init */
        ret = tty_set_termio(*fd_tty);
    }

out:
    return ret;
}
