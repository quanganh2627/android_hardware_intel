/* Modem Manager - tty source file
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

#include <sys/epoll.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include "logs.h"
#include "tty.h"

#define MAX_OPEN_RETRY 5
#define DELAY_BETWEEN_SUCCESSIVE_READ 20000     /* in microseconds */

/**
 * add fd to epoll
 *
 * @param [out] epollfd epoll fd
 * @param [in] fd file descriptor
 * @param [in] events events to catch
 *
 * @return E_ERR_FAILED initialization fails
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t tty_listen_fd(int epollfd, int fd, int events)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;
    struct epoll_event ev;

    ev.events = events;
    ev.data.fd = fd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        LOG_ERROR("Failed to add fd: (%s)", strerror(errno));
        ret = E_ERR_FAILED;
    }

    return ret;
}

e_mmgr_errors_t tty_init_listener(int *epollfd)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;

    ASSERT(epollfd != NULL);

    *epollfd = epoll_create(1);
    if (*epollfd == CLOSED_FD) {
        LOG_ERROR("epoll initialization failed");
        ret = E_ERR_FAILED;
    }

    return ret;
}

/**
 * wait for an event on tty
 *
 * @param [in] fd file descriptor
 * @param [in] timeout timeout (in milliseconds)
 *
 * @return E_ERR_TTY_TIMEOUT if any event occurs
 * @return E_ERR_TTY_POLLHUP if a pollhup occurs
 * @return E_ERR_FAILED if epoll create failed or unexpected event occurs
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t tty_wait_for_event(int fd, int timeout)
{
    struct epoll_event ev;
    int epollfd = CLOSED_FD;
    int err;
    e_mmgr_errors_t ret = E_ERR_SUCCESS;

    if ((ret = tty_init_listener(&epollfd)) != E_ERR_SUCCESS)
        goto out;

    ret = tty_listen_fd(epollfd, fd, EPOLLIN | EPOLLHUP);
    if (ret != E_ERR_SUCCESS)
        goto out;

    err = epoll_wait(epollfd, &ev, 1, timeout);
    if (err > 0) {
        if (ev.events & EPOLLHUP) {
            LOG_VERBOSE("POLLHUP received");
            ret = E_ERR_TTY_POLLHUP;
        } else if (ev.events & EPOLLIN) {
            LOG_VERBOSE("Received response data");
        } else {
            LOG_ERROR("Unexpected event (%d)", ev.events);
            ret = E_ERR_FAILED;
        }
    } else if (err == 0) {
        LOG_ERROR("WAIT ANSWER TIMEOUT");
        ret = E_ERR_TTY_TIMEOUT;
    } else {
        LOG_ERROR("Poll failed (%s)", strerror(errno));
        ret = E_ERR_FAILED;
    }

out:
    if (epollfd != CLOSED_FD)
        close(epollfd);
    return ret;
}

/**
 * read data from tty
 *
 * @param [in] fd file descriptor
 * @param [out] data buffer
 * @param [in,out] data_size available buffer size, updated with read size
 * @param [in] max_retries max read retries
 *
 * @return E_ERR_SUCCESS if successful,
 * @return E_ERR_TTY_BAD_FD if a bad fd is provided,
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t tty_read(int fd, char *data, int *data_size,
                         int max_retries)
{
    int i;
    int err;
    int read_size = 0;
    e_mmgr_errors_t ret = E_ERR_SUCCESS;

    ASSERT(data != NULL);
    ASSERT(data_size != NULL);

    memset(data, 0, *data_size);
    for (i = 0; i < max_retries; i++) {
        err = read(fd, data + read_size, *data_size - read_size);
        if (err < 0) {
            LOG_ERROR("Read failed (%s)", strerror(errno));
            if (errno == EBADF)
                ret = E_ERR_TTY_BAD_FD;
            else
                ret = E_ERR_FAILED;
            goto failure;
        } else if (err == 0) {
            if (read_size > 0)
                /* We have read nothing but a data was read before */
                break;
            else
                usleep(DELAY_BETWEEN_SUCCESSIVE_READ);
        } else {
            read_size += err;
        }
    }

    *data_size = read_size;

failure:
    return ret;
}

/**
 * write data to a tty device
 *
 * @param [in] fd file descriptor
 * @param [in] data to be written
 * @param [in] data_size data size
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED if nothing has been written
 * @return E_ERR_TTY_BAD_FD if write fails
 */
e_mmgr_errors_t tty_write(int fd, const char *data, int data_size)
{
    int err = 0;
    int cur = 0;
    e_mmgr_errors_t ret = E_ERR_SUCCESS;

    do {
        err = write(fd, data + cur, data_size - cur);
        cur += err;

        if (err == 0) {
            LOG_ERROR("write nothing (%s) fd=%d", strerror(errno), fd);
            ret = E_ERR_FAILED;
            break;
        } else if (err < 0) {
            LOG_ERROR("write error (%s) fd=%d", strerror(errno), fd);
            ret = E_ERR_TTY_BAD_FD;
            break;
        }
    } while (cur < data_size);

    return ret;
}

/**
 * Set tty configuration
 *
 * @param [in] fd tty file descriptor
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED if nothing has been written
 */
e_mmgr_errors_t tty_set_termio(int fd)
{
    struct termios newtio;
    e_mmgr_errors_t ret = E_ERR_SUCCESS;

    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
        LOG_ERROR("fcntl failed (%s)", strerror(errno));
        ret = E_ERR_FAILED;
        goto out;
    }

    memset(&newtio, 0, sizeof(newtio));

    /* disable postprocess output characters */
    /* *INDENT-OFF* */
    newtio.c_oflag &= ~OPOST;
    newtio.c_lflag &= ~(ECHO /* disable echo input characters */
                        | ECHONL /* disable echo new line */
                        | ICANON /* disable erase, kill, werase, and */
                        /* print special characters */
                        | ISIG /* disable interrupt, quit, and suspend */
                        /* special characters */
                        | IEXTEN); /* disable non-POSIX special characters */

    newtio.c_cflag &= ~(CSIZE /* no size */
                        | PARENB /* disable parity bit */
                        | CBAUD /* clear current baud rate */
                        | CBAUDEX); /* clear current buad rate */
    newtio.c_cflag |= CS8; /* character size 8 bits */
    newtio.c_cflag |= CLOCAL | CREAD; /* Ignore modem control lines */
    newtio.c_cflag |= B115200; /* baud rate 115200 */
    /* *INDENT-ON* */

    tcflush(fd, TCIFLUSH);
    tcsetattr(fd, TCSANOW, &newtio);

out:
    return ret;
}

/**
 * Open a TTY device and set the terminal configuration
 *
 * @param [in] tty_name tty path
 * @param [out] fd tty file descriptor
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_TTY_BAD_FD if open fails
 */
e_mmgr_errors_t tty_open(const char *tty_name, int *fd)
{
    e_mmgr_errors_t ret = E_ERR_TTY_BAD_FD;
    int count;

    ASSERT(tty_name != NULL);
    ASSERT(fd != NULL);

    if (*fd != CLOSED_FD)
        goto out;

    LOG_DEBUG("trying to open tty device: %s", tty_name);
    for (count = 0; count < MAX_OPEN_RETRY; count++) {
        *fd = open(tty_name, O_RDWR);
        if (*fd > 0) {
            break;
        } else if ((errno == ENOENT) || (errno == EAGAIN) ||
                   (errno == EACCES)) {
            sleep(1);
            LOG_DEBUG("retry to open %s due to (%s) failure",
                      tty_name, strerror(errno));
        } else {
            break;
        }
    }

    if (*fd < 0) {
        LOG_ERROR("open of %s failed (%s)", tty_name, strerror(errno));
    } else if (tty_set_termio(*fd) != E_ERR_SUCCESS) {
        LOG_ERROR("Failed to set discipline");
        close(*fd);
        *fd = CLOSED_FD;
    } else {
        ret = E_ERR_SUCCESS;
        LOG_DEBUG("done");
    }

out:
    return ret;
}

/**
 * Close a TTY devices
 *
 * @param [in,out] fd file descriptor to close
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED if nothing has been written
 */
e_mmgr_errors_t tty_close(int *fd)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;

    ASSERT(fd != NULL);

    LOG_DEBUG("trying to close tty");
    if (*fd > CLOSED_FD) {
        close(*fd);
        *fd = CLOSED_FD;
        ret = E_ERR_SUCCESS;
        LOG_DEBUG("closed");
    }

    return ret;
}
