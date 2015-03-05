/* Modem Manager - mux source file
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

#include <errno.h>
#include <string.h>
#include <termios.h>
#include <cutils/sockets.h>
#include <linux/gsmmux.h>
#include <linux/hsi_ffl_tty.h>
#include <linux/ioctl.h>
#include "at.h"
#include "errors.h"
#include "logs.h"
#include "mux.h"

/* AT commands */
#define AT_PING_CMD "ATE0\r"
/* Maximum size for configuration string key */
#define MAX_SIZE_CONF_KEY 100

/* GSM0710 line discipline code */
#define N_GSM0710 21
/* AT MUX configuration */
#define AT_MUX_CMD_SIZE 128
#define AT_MUX_MODE 0
#define AT_MUX_SUBSET 0
#define AT_MUX_PORT_SPEED
#define AT_MUX_T1 10
#define AT_MUX_N2 3
#define AT_MUX_T2 30
#define AT_MUX_T3
#define AT_MUX_K

/**
 * check that modem is alive by sending PING request
 *
 * @param [in] fd_tty modem file descriptor
 * @param [in] retry retries to send command
 *
 * @return E_ERR_TTY_BAD_FD bad file descriptor
 * @return E_ERR_TTY_POLLHUP POLLHUP detected during read
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t modem_handshake(int fd_tty, int retry)
{
    LOG_VERBOSE("sending PING to modem");
    return send_at_retry(fd_tty, AT_PING_CMD, strlen(AT_PING_CMD), retry,
                         AT_ANSWER_SHORT_TIMEOUT);
}

/**
 * configure CMUX driver
 *
 * @param [in] fd_tty modem file descriptor
 * @param [in] max_frame_size maximum frame size
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t configure_cmux_driver(int fd_tty, int max_frame_size)
{
    struct gsm_config cfg;
    int err;
    int ldisc = N_GSM0710;
    e_mmgr_errors_t ret = E_ERR_FAILED;

    LOG_VERBOSE("attach mux ld to initial tty");
    err = ioctl(fd_tty, TIOCSETD, &ldisc);
    if (err < 0) {
        LOG_ERROR("set ioctl failed (%s)", strerror(errno));
        goto out;
    }

    err = ioctl(fd_tty, TIOCGETD, &ldisc);
    if (err < 0) {
        LOG_ERROR("get ioctl failed (%s)", strerror(errno));
        goto out;
    }

    if (ldisc != N_GSM0710) {
        LOG_ERROR("Unable to set line discipline");
        goto out;
    }

    /* configure mux */
    memset(&cfg, 0, sizeof(struct gsm_config));

    err = ioctl(fd_tty, GSMIOC_GETCONF, &cfg);
    if (err < 0) {
        LOG_ERROR("get config ioctl failed (%s)", strerror(errno));
        goto out;
    }
    LOG_VERBOSE("Default configuration\n"
                PRINT_INTEGER
                PRINT_INTEGER
                PRINT_INTEGER
                PRINT_INTEGER
                PRINT_INTEGER
                PRINT_INTEGER
                PRINT_INTEGER
                PRINT_INTEGER
                PRINT_INTEGER
                PRINT_INTEGER
                PRINT_INTEGER
                PRINT_INTEGER
                PRINT_INTEGER,
                "adaption", cfg.adaption,
                "encapsulation", cfg.encapsulation,
                "initiator", cfg.initiator,
                "t1", cfg.t1,
                "t2", cfg.t2,
                "t3", cfg.t3,
                "n2", cfg.n2,
                "mru", cfg.mru,
                "mtu", cfg.mtu,
                "k", cfg.k,
                "i", cfg.i, "clocal", cfg.clocal, "burst", cfg.burst);

    /* encoding -- set to basic */
    cfg.encapsulation = 0;
    /* we are starting side */
    cfg.initiator = 1;
    /* In specification 3GPP TS 27.010, 5.7.2 set same size as MUX
     * configuration */
    cfg.mru = max_frame_size;
    cfg.mtu = max_frame_size;
    /* Disable burst mode (not supported by IMC modem) */
    cfg.burst = 0;

    LOG_VERBOSE("write config ioctl to mux");
    err = ioctl(fd_tty, GSMIOC_SETCONF, &cfg);
    if (err < 0)
        LOG_ERROR("config ioctl problem (%s)", strerror(errno));
    else
        ret = E_ERR_SUCCESS;

out:
    return ret;
}

/**
 * send cmux command
 *
 * @param [in] fd_tty modem file descriptor
 * @param [in] mux mux configuration
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_TTY_POLLHUP POLLHUP detected during read
 * @return E_ERR_TTY_BAD_FD bad file descriptor
 * @return E_ERR_FAILED if AT+CMUX creation command failed
 */
e_mmgr_errors_t send_at_cmux(int fd_tty, mux_t *mux)
{
    char at_cmux_config[AT_MUX_CMD_SIZE];
    e_mmgr_errors_t ret = E_ERR_FAILED;

    ASSERT(mux != NULL);

    /* NB: Add AT_MUX_PORT_SPEED, AT_MUX_T3, AT_MUX_K if needed */
    if (snprintf(at_cmux_config, AT_MUX_CMD_SIZE,
                 "AT+CMUX=%d,%d,,%d,%d,%d,%d,,\r", AT_MUX_MODE, AT_MUX_SUBSET,
                 mux->frame_size, AT_MUX_T1, AT_MUX_N2, AT_MUX_T2) < 0) {
        LOG_ERROR("AT+CMUX command creation failed");
    } else {
        LOG_VERBOSE("sending AT+CMUX");
        ret = send_at_retry(fd_tty, at_cmux_config,
                            strnlen(at_cmux_config, AT_MUX_CMD_SIZE),
                            mux->retry, AT_ANSWER_SHORT_TIMEOUT);
        if (ret != E_ERR_SUCCESS)
            LOG_ERROR("AT+CMUX not sent successfully");
    }

    return ret;
}
