/* Modem Manager - at source file
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

#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>
#include <linux/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "at.h"
#include "errors.h"
#include "logs.h"
#include "tty.h"

#define AT_SIZE 20

/**
 * Send an AT command to a tty devices
 *
 * @param [in] fd tty file descriptor
 * @param [in] command at command to send
 * @param [in] command_size length of command
 * @param [in] timeout timeout for the command answer (in ms)
 *
 * @return E_ERR_SUCCESS command sends and 'OK' received
 * @return E_ERR_FAILED generic failure. Command to be resent
 * @return E_ERR_TTY_POLLHUP POLLHUP detected during read
 * @return E_ERR_TTY_BAD_FD if a bad file descriptor is provided
 */
static e_mmgr_errors_t send_at(int fd, const char *command, int command_size,
                               int timeout)
{
    e_mmgr_errors_t ret;
    int data_size = AT_SIZE;
    char data[AT_SIZE + 1];

    ASSERT(command != NULL);

    /* Send AT command to modem */
    LOG_DEBUG("Send of %s", command);
    tcflush(fd, TCIOFLUSH);
    ret = tty_write(fd, command, command_size);
    if (ret != E_ERR_SUCCESS)
        goto out;

    if (timeout == AT_ANSWER_NO_TIMEOUT) {
        LOG_DEBUG("No response needed from modem to %s", command);
        goto out;
    }

    LOG_DEBUG("Wait answer...");
    for (;; ) {
        /* Give time to receive response or POLLHUP. */
        ret = tty_wait_for_event(fd, timeout);
        if (ret != E_ERR_SUCCESS)
            goto out;

        /* Read response data but give up after AT_READ_MAX_RETRIES tries */
        ret = tty_read(fd, data, &data_size, AT_READ_MAX_RETRIES);
        if (ret != E_ERR_SUCCESS)
            goto out;

        data[data_size] = '\0';
        /* Did we get an "OK" back? */
        if (strstr(data, "OK")) {
            LOG_DEBUG("OK received");
            ret = E_ERR_SUCCESS;
            break;
        } else if (strstr(data, "PBREADY")) {
            LOG_DEBUG("PBREADY received");
            continue;
        } else {
            LOG_ERROR("Wrong anwser (%s)", data);
            ret = E_ERR_FAILED;
            break;
        }
    }                           /* Loop waiting answer */

    tcflush(fd, TCIFLUSH);

out:
    return ret;
}

/**
 * Try to send an AT command to modem
 *
 * @param [in] fd_tty tty file descriptor
 * @param [in] at_cmd AT command to send
 * @param [in] at_cmd_size AT command size
 * @param [in] retry number of retries to send the AT command
 * @param [in] timeout timeout for the command answer (in ms)
 *
 * @return E_ERR_SUCCESS command sends and 'OK' received
 * @return E_ERR_TTY_POLLHUP POLLHUP detected during read
 * @return E_ERR_FAILED failed to send command after all retries
 * @return E_ERR_TTY_BAD_FD bad file descriptor
 */
e_mmgr_errors_t send_at_retry(int fd_tty, const char *at_cmd, int at_cmd_size,
                              int retry, int timeout)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;

    ASSERT(at_cmd != NULL);

    /* Send AT until we get a valid response or after retry retries */
    for (; retry >= 0; retry--) {
        ret = send_at(fd_tty, at_cmd, at_cmd_size, timeout);
        if ((ret == E_ERR_TTY_BAD_FD) || (ret == E_ERR_TTY_POLLHUP) ||
            (ret == E_ERR_SUCCESS))
            break;
    }

    if (ret == E_ERR_FAILED)
        LOG_ERROR("Invalid or no response from modem");

    return ret;
}
