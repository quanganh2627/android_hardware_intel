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
#include <sys/types.h>
#include <sys/stat.h>
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
#include "mdm_fw.h"
#include "reset_escalation.h"
#include "mux.h"
#include "tty.h"

#define AT_XLOG_GET "AT+XLOG=0\r"
#define AT_XLOG_RESET "AT+XLOG=2\r"
#define AT_XLOG_TIMEOUT 2500
#define AT_ANSWER_SIZE 254

#define AT_STREAMLINE_GET \
    "at@usbmwtestfw:usb_profile_get_nvm_configuration(USBPOW_ID_HSIC)\r"
#define AT_STREAMLINE_TIMEOUT 1000
#define AT_STREAMLINE_REPLY_SIZE 2048
#define AT_STREAMLINE_LOG_SPLIT 512
#define AT_STREAMLINE_LINE_SEPARATOR "\r\n"

#define AT_DEFAULT_TIMEOUT 2500
#define AT_DEFAULT_RESPONSE_SIZE 2048
#define AT_DEFAULT_LINE_SEPARATOR "\r\n"

/* switch to MUX timings */
#define STAT_DELAY 250          /* in milliseconds */
#define MAX_TIME_DELAY 4        /* in seconds */

typedef enum e_switch_to_mux_states {
    E_MUX_HANDSHAKE,
    E_MUX_XLOG,
    E_MUX_GET_STREAMLINE,
    E_MUX_AT_CMD,
    E_MUX_DRIVER,
} e_switch_to_mux_states_t;

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
            ALOGD("%s", data);
        }
    } while (read_size > 0);

    tcflush(fd_tty, TCIFLUSH);
    ret = send_at_retry(fd_tty, AT_XLOG_RESET, strlen(AT_XLOG_RESET),
                        max_retry, AT_ANSWER_SHORT_TIMEOUT);
out_xlog:
    return ret;
}

/**
 * Retrieve streamline configuration
 *
 * @param [in] fd_tty tty file descriptor
 *
 * @return E_ERR_TTY_BAD_FD
 * @return E_ERR_TTY_POLLHUP if a pollhup occurs
 * @return E_ERR_FAILED error during write
 * @return E_ERR_TTY_TIMEOUT no response from modem
 * @return E_ERR_SUCCESS if successful
 */
static e_mdm_wakeup_cfg_t get_wakeup_cfg(int fd_tty)
{
    e_mmgr_errors_t err = E_ERR_SUCCESS;
    struct timespec current, start;
    char data[AT_STREAMLINE_REPLY_SIZE + 1];
    size_t in_buffer = 0;
    e_mdm_wakeup_cfg_t wakeup_cfg = E_MDM_WAKEUP_UNKNOWN;

    tcflush(fd_tty, TCIOFLUSH);

    LOG_DEBUG("Sending %s", AT_STREAMLINE_GET);

    err = tty_write(fd_tty, AT_STREAMLINE_GET, strlen(AT_STREAMLINE_GET));
    if (err != E_ERR_SUCCESS)
        goto out_streamline;

    clock_gettime(CLOCK_BOOTTIME, &start);
    while (1) {
        int timeout;

        /* Wait for modem reply. */
        clock_gettime(CLOCK_BOOTTIME, &current);
        timeout = AT_STREAMLINE_TIMEOUT -
                  ((current.tv_sec - start.tv_sec) * 1000 +
                   ((current.tv_nsec - start.tv_nsec) / 1000000));
        if (timeout < 0)
            goto out_streamline;
        err = tty_wait_for_event(fd_tty, timeout);
        if (err != E_ERR_SUCCESS)
            goto out_streamline;

        while (1) {
            char *cr;

            int read_size = AT_STREAMLINE_REPLY_SIZE - (int)in_buffer;
            if (read_size <= 0) {
                LOG_ERROR("streamline reply bigger than allocated buffer "
                          "(%d bytes).\n", AT_STREAMLINE_REPLY_SIZE);
                goto out_streamline;
            }

            /* max_retries is set to 1 as retries are handled in this loop. */
            err = tty_read(fd_tty, &data[in_buffer], &read_size, 1);
            if (err != E_ERR_SUCCESS)
                goto out_streamline;
            if (read_size <= 0)
                break;

            data[in_buffer + read_size] = '\0';

            /* Parse modem reply line by line, with 'cr' pointing to the
             * beginning of a line (starting with 'data').
             */
            cr = data;
            while (1) {
                char *end_cr = strstr(cr, AT_STREAMLINE_LINE_SEPARATOR);
                if (end_cr != NULL) {
                    /* Found a complete line, parse it and quit the processing
                     * if OK or ERROR are found.
                     */
                    int base = 0;
                    int len;

                    /* Zero-terminate the just found complete line. */
                    *end_cr = '\0';
                    len = strlen(cr);

                    /* Debug output of the line (with splitting to not send too
                     * much data to Android's logging framework).
                     */
                    while (len > 0) {
                        const char *ellipsis = "";
                        char save = '\0';
                        if (len > AT_STREAMLINE_LOG_SPLIT) {
                            ellipsis = "...";
                            save = cr[base + AT_STREAMLINE_LOG_SPLIT];
                            cr[base + AT_STREAMLINE_LOG_SPLIT] = '\0';
                        }
                        LOG_INFO("received from modem: '%s'%s",
                                 &cr[base], ellipsis);
                        if (save != '\0')
                            cr[base + AT_STREAMLINE_LOG_SPLIT] = save;
                        base += AT_STREAMLINE_LOG_SPLIT;
                        len -= AT_STREAMLINE_LOG_SPLIT;
                    }

                    if ((!strcmp(cr, "OK")) || (!strcmp(cr, "ERROR"))) {
                        goto out_streamline;
                    } else if (strstr(cr, "current_configuration") != NULL) {
                        /* The line contains the USB configuration, search for
                         * INBAND vs OUTBAND.
                         */
                        if (strstr(cr, "NO_REMOTE_WAKEUP") != NULL) {
                            LOG_INFO("OUTBAND configuration detected");
                            wakeup_cfg = E_MDM_WAKEUP_OUTBAND;
                        } else {
                            LOG_INFO("INBAND configuration detected");
                            wakeup_cfg = E_MDM_WAKEUP_INBAND;
                        }
                    }

                    /* Line fully parsed, move to the beginning of the next
                     * one.
                     */
                    cr = end_cr + strlen(AT_STREAMLINE_LINE_SEPARATOR);
                } else {
                    /* As current line is not complete, move it to the beginning
                     * of the data buffer to append subsequent reads.
                     */
                    in_buffer = strlen(cr);
                    if ((in_buffer != 0) && (cr != data) &&
                        (in_buffer < sizeof(data)))   // Added for KW
                        memmove(data, cr, in_buffer);
                    break;
                }
            }
        }
    }

out_streamline:
    tcflush(fd_tty, TCIOFLUSH);

    return wakeup_cfg;
}

/**
 * This function builds a time stamp string.
 *
 * @param [out] buffer
 * @param [in] buffer size
 *
 * @return: true if successful.
 */
bool generate_timestamp(char *timestamp, int size)
{
    struct tm *tm;
    time_t curTime = time(NULL);
    bool ret = false;

    if (timestamp == NULL) {
        LOG_INFO("timestamp is NULL");
        return false;
    }

    if (curTime == -1) {
        LOG_ERROR("Invalid time");
    } else {
        tm = localtime(&curTime);
        if (tm == NULL) {
            LOG_ERROR("localtime() returned NULL");
        } else {
            snprintf(timestamp, size, "%4d%02d%02d%02d%02d%02d",
                     tm->tm_year + 1900, tm->tm_mon + 1,
                     tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
            ret = true;
        }
    }
    return ret;
}

/**
 * Read characters from tty fd then process these data by the given callback
 * function.
 * The fd_fs parameter is passed along to the callback function.
 *
 * @param [in] fd_tty Modem file descriptor
 * @param [in] fd_fs Core dump log file descriptor
 * @param [in] parseFct Callback function processing data received
 *
 * @return 0 All data have been parsed.
 * @return -1 Not all data have been read from fd_tty
 * @return -2 A critical error happened
 */
int read_cd_logs(int fd_tty, int fd_fs, PFN_PARSE_RESP parseFct)
{
    int ret = -2;
    char data[AT_DEFAULT_RESPONSE_SIZE + 1] = { 0 };
    int read_size = AT_DEFAULT_RESPONSE_SIZE;
    char *cr = NULL;
    char *pEndLine = NULL;
    size_t nOffsetBytes = 0;
    size_t totalLength = 0;

    if (fd_tty <= 0)
        goto out_read;

    LOG_DEBUG("WAITING DATA, time-out duration:%dms", AT_DEFAULT_TIMEOUT);

    do {
        /* Wait for modem reply. */
        if (tty_wait_for_event(fd_tty, AT_DEFAULT_TIMEOUT) != E_ERR_SUCCESS)
            goto out_read;

        read_size = AT_DEFAULT_RESPONSE_SIZE - nOffsetBytes;
        if (read_size <= 0) {
            LOG_ERROR("Modem reply bigger than allocated buffer "
                      "(%d bytes).\n", AT_DEFAULT_RESPONSE_SIZE);
            goto out_read;
        }

        if (tty_read(fd_tty, &data[nOffsetBytes], &read_size,
                     1) != E_ERR_SUCCESS) {
            LOG_ERROR("Failed reading data from TTY!");
            goto out_read;
        }

        LOG_DEBUG("Read %d bytes from tty", read_size);

        /* No data */
        if (read_size <= 0) {
            ret = -1;
            goto out_read;
        }

        /* Safe because buffer length is AT_DEFAULT_RESPONSE_SIZE+1 */
        data[nOffsetBytes + read_size] = '\0';
        totalLength = strlen(data);
        LOG_DEBUG("Data buffer length=%d bytes", totalLength);
        cr = data;
        /* Find pointer to end of line */
        while ((pEndLine = strstr(cr, AT_DEFAULT_LINE_SEPARATOR)) != NULL) {
            *pEndLine = '\0';
            size_t lineLen = strlen(cr);
            if (lineLen == 0) {
                /* Skip line separator characters */
                cr += strlen(AT_DEFAULT_LINE_SEPARATOR);
                continue;
            }
            /* Call parsing function with received data */
            int res = parseFct(&fd_fs, cr, &lineLen);

            LOG_DEBUG("Wrote %d bytes onto log file", lineLen);

            /* Check if end of data detected */
            if (res > 0) {
                LOG_INFO("Core Dump logs data end.");
                ret = 0;
                goto out_read;
            } else if (res == 0) {
                ret = -1;
            } else {
                goto out_read;
            }
            /* Go to next line */
            cr = pEndLine + strlen(AT_DEFAULT_LINE_SEPARATOR);
        }
        if (pEndLine == NULL) {
            /* Move the rest of data onto beginning of buffer */
            nOffsetBytes = strlen(cr);
            LOG_DEBUG("Processing remaining data:%d Bytes.", nOffsetBytes);
            if (nOffsetBytes > 0)
                if ((cr != data) && (nOffsetBytes < sizeof(data)))
                    memmove(data, cr, nOffsetBytes);
        }
        /* Loop  */
    } while (nOffsetBytes > 0);

out_read:
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
e_mmgr_errors_t switch_to_mux(int *fd_tty,
                              const char *mdm_bb_path,
                              e_link_t mdm_bb_type,
                              const mux_t *mux,
                              const char *sanity_check_dlc,
                              e_mdm_wakeup_cfg_t *wakeup_cfg)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    e_switch_to_mux_states_t state;
    struct timespec current, start;
    bool retry_bad_fd_done = false;

    ASSERT(fd_tty != NULL);
    ASSERT(mdm_bb_path != NULL);
    ASSERT(mux != NULL);
    ASSERT(sanity_check_dlc != NULL);
    ASSERT(wakeup_cfg != NULL);

    for (state = E_MUX_HANDSHAKE; state != E_MUX_DRIVER; /* none */) {
        switch (state) {
        case E_MUX_HANDSHAKE:
            ret = modem_handshake(*fd_tty, mux->retry);
            break;
        case E_MUX_XLOG:
            ret = run_at_xlog(*fd_tty, mux->retry);
            break;
        case E_MUX_GET_STREAMLINE:
            ret = E_ERR_SUCCESS;
            /* Only do the streamline step for USB based modems */
            if (E_LINK_USB == mdm_bb_type)
                *wakeup_cfg = get_wakeup_cfg(*fd_tty);
            else
                *wakeup_cfg = E_MDM_WAKEUP_UNKNOWN;
            break;
        case E_MUX_AT_CMD:
            ret = send_at_cmux(*fd_tty, mux);
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
            LOG_DEBUG("reopening tty device: %s", mdm_bb_path);
            retry_bad_fd_done = true;
            tty_close(fd_tty);
            if ((ret = tty_open(mdm_bb_path, fd_tty)) != E_ERR_SUCCESS)
                goto out;
        } else {
            ret = E_ERR_FAILED;
            goto out;
        }
    }

    ret = configure_cmux_driver(*fd_tty, mux->frame_size);
    if (ret != E_ERR_SUCCESS)
        goto out;

    /* Wait to be able to open a GSM TTY before sending MODEM_UP to clients
     * (this guarantees that the MUX control channel has been established with
     * the modem). Will retry for up to MAX_TIME_DELAY seconds. */
    LOG_DEBUG("looking for TTY %s", sanity_check_dlc);
    ret = E_ERR_FAILED;
    clock_gettime(CLOCK_MONOTONIC, &start);
    do {
        int tmp_fd;

        usleep(STAT_DELAY * 1000);
        if ((tmp_fd = open(sanity_check_dlc, O_RDWR)) >= 0) {
            close(tmp_fd);
            ret = E_ERR_SUCCESS;
            break;
        }

        clock_gettime(CLOCK_MONOTONIC, &current);
    } while ((current.tv_sec < (start.tv_sec + MAX_TIME_DELAY)) ||
             ((current.tv_sec == (start.tv_sec + MAX_TIME_DELAY)) &&
              (current.tv_nsec < start.tv_nsec)));

    if (ret != E_ERR_SUCCESS) {
        LOG_ERROR("was not able to open TTY %s", sanity_check_dlc);
    } else {
        LOG_DEBUG("TTY %s open success", sanity_check_dlc);
        /* It's necessary to reset the terminal configuration after MUX init */
        ret = tty_set_termio(*fd_tty);
    }

out:
    return ret;
}
