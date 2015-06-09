/* Modem Manager client library - utils source file
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

#include <cutils/sockets.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include "utils.h"
#include "tty.h"
#include "msg_format.h"
#include "client_cnx.h"

const char const *g_mmgr_events[] = {
#undef X
#define X(a) #a
    MMGR_EVENTS
};

static const char const *g_mmgr_requests[] = {
#undef X
#define X(a) #a
    MMGR_REQUESTS
};

static inline e_mmgr_events_t get_ack(mmgr_lib_context_t *p_lib)
{
    e_mmgr_events_t ack;

    pthread_mutex_lock(&p_lib->mtx);
    ack = p_lib->ack;
    pthread_mutex_unlock(&p_lib->mtx);

    return ack;
}

static inline void set_ack(mmgr_lib_context_t *p_lib, e_mmgr_events_t ack)
{
    pthread_mutex_lock(&p_lib->mtx);
    p_lib->ack = ack;
    pthread_mutex_unlock(&p_lib->mtx);
}

static e_err_mmgr_cli_t ev_ack(mmgr_lib_context_t *p_lib, e_mmgr_events_t id)
{
    e_err_mmgr_cli_t ret = E_ERR_CLI_FAILED;

    CHECK_CLI_PARAM(p_lib, ret, out);

    set_ack(p_lib, id);

    pthread_mutex_lock(&p_lib->mtx_signal);
    pthread_cond_signal(&p_lib->cond);
    pthread_mutex_unlock(&p_lib->mtx_signal);
    ret = E_ERR_CLI_SUCCEED;
out:
    return ret;
}

static e_err_mmgr_cli_t call_cli_callback(mmgr_lib_context_t *p_lib, msg_t *msg)
{
    e_err_mmgr_cli_t ret = E_ERR_CLI_FAILED;
    mmgr_cli_event_t event = { .context = p_lib->cli_ctx };
    e_mmgr_events_t id;
    struct timespec start, end;
    int err = 1;

    memcpy(&id, &msg->hdr.id, sizeof(e_mmgr_events_t));

    if (id < E_MMGR_NUM_EVENTS) {
        LOG_DEBUG("event (%s) received", p_lib, g_mmgr_events[id]);
        if ((id == E_MMGR_ACK) || (id == E_MMGR_NACK)) {
            ret = ev_ack(p_lib, id);
        } else if (p_lib->func[id] != NULL) {
            event.id = id;
            p_lib->set_data[id] (msg, &event);

            pthread_mutex_lock(&p_lib->mtx);
            p_lib->tid = gettid();
            pthread_mutex_unlock(&p_lib->mtx);

            clock_gettime(CLOCK_BOOTTIME, &start);
            err = p_lib->func[id] (&event);
            clock_gettime(CLOCK_BOOTTIME, &end);

            LOG_VERBOSE("callback for event (%s) handled in %ld ms",
                        p_lib, g_mmgr_events[id],
                        ((end.tv_sec - start.tv_sec) * 1000) +
                        ((end.tv_nsec - start.tv_nsec) / 1000000));

            p_lib->free_data[id] (&event);
            ret = E_ERR_CLI_SUCCEED;
        } else {
            LOG_ERROR("func is NULL", p_lib);
        }

        if ((err == 0) && ((id == E_MMGR_NOTIFY_MODEM_COLD_RESET) ||
                           (id == E_MMGR_NOTIFY_MODEM_SHUTDOWN))) {
            mmgr_cli_requests_t request;
            memset(&request, 0, sizeof(request));
            if (id == E_MMGR_NOTIFY_MODEM_COLD_RESET)
                request.id = E_MMGR_ACK_MODEM_COLD_RESET;
            else
                request.id = E_MMGR_ACK_MODEM_SHUTDOWN;

            send_msg(p_lib, &request, E_SEND_SINGLE,
                     DEF_MMGR_RESPONSIVE_TIMEOUT);
        }
    } else {
        LOG_DEBUG("unkwnown event received (0x%.2X)", p_lib, msg->hdr.id);
    }
    return ret;
}

/**
 * Read a message in the socket
 *
 * @param [in] p_lib
 * @param [out] msg It's user responsability to free this buffer
 *
 * @return E_ERR_CLI_FAILED
 * @return E_ERR_CLI_BAD_CNX_STATE if client is disconnected
 * @return E_ERR_CLI_SUCCEED
 */
static e_err_mmgr_cli_t read_msg(mmgr_lib_context_t *p_lib, msg_t *msg)
{
    e_err_mmgr_cli_t ret = E_ERR_CLI_FAILED;
    e_mmgr_errors_t err = E_ERR_FAILED;

    CHECK_CLI_PARAM(p_lib, ret, out);
    CHECK_CLI_PARAM(msg, ret, out);

    memset(msg, 0, sizeof(msg_t));

    /* read msg data */
    err = msg_get_header(p_lib->fd_socket, &msg->hdr);
    if (err == E_ERR_DISCONNECTED) {
        LOG_DEBUG("connection closed by MMGR", p_lib);

        pthread_mutex_lock(&p_lib->mtx);
        p_lib->connected = E_CNX_RECONNECT;
        pthread_mutex_unlock(&p_lib->mtx);
        ret = E_ERR_CLI_BAD_CNX_STATE;
    } else {
        e_mmgr_events_t id = E_MMGR_NUM_EVENTS;
        size_t size = 0;

        memcpy(&id, &msg->hdr.id, sizeof(e_mmgr_events_t));
        memcpy(&size, &msg->hdr.len, sizeof(size_t));
        if (size != 0) {
            msg->data = calloc(size, sizeof(char));
            if (msg->data == NULL) {
                LOG_ERROR("memory allocation fails", p_lib);
            } else {
                size_t read_size = size;
                err = cnx_read(p_lib->fd_socket, msg->data, &read_size);
                if ((err != E_ERR_SUCCESS) || (read_size != size))
                    LOG_ERROR("Read error. Size: %d/%d", p_lib, read_size,
                              size);
                else
                    ret = E_ERR_CLI_SUCCEED;
            }
        } else {
            ret = E_ERR_CLI_SUCCEED;
        }
    }

out:
    return ret;
}

/**
 * function to handle cnx event
 *
 * @private
 *
 * @param [in] p_lib library handle
 *
 * @return E_ERR_CLI_BAD_HANDLE if p_lib is invalid
 * @return E_ERR_CLI_FAILED if not connected or invalid request id
 * @return E_ERR_CLI_BAD_CNX_STATE if client is disconnected
 * @return E_ERR_CLI_SUCCEED
 */
static e_err_mmgr_cli_t handle_cnx_event(mmgr_lib_context_t *p_lib)
{
    e_err_mmgr_cli_t ret = E_ERR_CLI_FAILED;
    msg_t msg = { .data = NULL };

    CHECK_CLI_PARAM(p_lib, ret, out);

    ret = read_msg(p_lib, &msg);
    if (ret == E_ERR_CLI_SUCCEED)
        ret = call_cli_callback(p_lib, &msg);

    msg_delete(&msg);

out:
    return ret;
}

/**
 * handle events provided by select
 *
 * @private
 *
 * @param [in] p_lib private structure
 * @param [in] rfds read events
 *
 * @return E_ERR_CLI_FAILED
 * @return E_ERR_CLI_BAD_CNX_STATE
 * @return E_ERR_CLI_SUCCEED
 */
static e_err_mmgr_cli_t handle_events(mmgr_lib_context_t *p_lib, fd_set *rfds)
{
    e_err_mmgr_cli_t ret = E_ERR_CLI_FAILED;
    char buffer[PIPE_BUF];

    CHECK_CLI_PARAM(p_lib, ret, out);
    CHECK_CLI_PARAM(rfds, ret, out);

    if (FD_ISSET(p_lib->fd_pipe[READ], rfds)) {
        read(p_lib->fd_pipe[READ], buffer, PIPE_BUF);
        LOG_DEBUG("stopping thread", p_lib);
        ret = E_ERR_CLI_BAD_CNX_STATE;
    } else if (FD_ISSET(p_lib->fd_socket, rfds)) {
        ret = handle_cnx_event(p_lib);
    } else {
        LOG_DEBUG("event not handled", p_lib);
    }
out:
    return ret;
}

/**
 * handle client disconnection
 *
 * @private
 *
 * @param [in] p_lib library handle
 *
 * @return E_ERR_CLI_FAILED if client is disconnected
 * @return E_ERR_CLI_BAD_HANDLE
 * @return E_ERR_CLI_SUCCEED if client is reconnected
 */
e_err_mmgr_cli_t handle_disconnection(mmgr_lib_context_t *p_lib)
{
    e_err_mmgr_cli_t ret = E_ERR_CLI_FAILED;
    cnx_state_t state;
    msg_t msg = { .data = NULL };

    CHECK_CLI_PARAM(p_lib, ret, out);

    pthread_mutex_lock(&p_lib->mtx);
    state = p_lib->connected;
    p_lib->connected = E_CNX_DISCONNECTED;
    pthread_mutex_unlock(&p_lib->mtx);

    if (state == E_CNX_RECONNECT) {
        LOG_DEBUG("notify fake modem down event", p_lib);
        msg.hdr.id = E_MMGR_EVENT_MODEM_DOWN;
        ret = call_cli_callback(p_lib, &msg);

        LOG_DEBUG("notify fake modem cold reset event", p_lib);
        msg.hdr.id = E_MMGR_NOTIFY_MODEM_COLD_RESET;
        ret = call_cli_callback(p_lib, &msg);

        LOG_DEBUG("try to reconnect", p_lib);
        do {
            /* endlessly try to reconnect to MMGR */
            sleep(1);

            pthread_mutex_lock(&p_lib->mtx);
            if (p_lib->fd_socket != CLOSED_FD)
                cnx_close(&p_lib->fd_socket);
            pthread_mutex_unlock(&p_lib->mtx);

            ret = cli_connect(p_lib);
        } while (ret != E_ERR_CLI_SUCCEED);

        if (p_lib->lock) {
            mmgr_cli_requests_t request = { .id = E_MMGR_RESOURCE_ACQUIRE };
            /* restore the context */
            do
                ret = send_msg(p_lib, &request, E_SEND_SINGLE,
                               DEF_MMGR_RESPONSIVE_TIMEOUT);
            while (ret != E_ERR_CLI_SUCCEED);
            LOG_DEBUG("context restored", p_lib);
        }

        pthread_mutex_lock(&p_lib->mtx);
        p_lib->connected = E_CNX_CONNECTED;
        pthread_mutex_unlock(&p_lib->mtx);
    } else {
        pthread_mutex_lock(&p_lib->mtx);
        if (p_lib->fd_socket != CLOSED_FD)
            cnx_close(&p_lib->fd_socket);
        if (p_lib->fd_pipe[READ] != CLOSED_FD) {
            close(p_lib->fd_pipe[READ]);
            p_lib->fd_pipe[READ] = CLOSED_FD;
        }
        if (p_lib->fd_pipe[WRITE] != CLOSED_FD) {
            close(p_lib->fd_pipe[WRITE]);
            p_lib->fd_pipe[WRITE] = CLOSED_FD;
        }
        pthread_mutex_unlock(&p_lib->mtx);

        LOG_DEBUG("disconnected", p_lib);
    }
out:
    return ret;
}

/**
 * check if client is connected or not. This is a sensitive data
 *
 * @param [in] ctx private structure
 *
 * @private
 *
 * @return false if ctx is NULL
 * @return connection state otherwise
 */
bool is_connected(mmgr_lib_context_t *ctx)
{
    bool answer = false;

    if (ctx) {
        pthread_mutex_lock(&ctx->mtx);
        answer = (ctx->connected == E_CNX_CONNECTED);
        pthread_mutex_unlock(&ctx->mtx);
    }

    return answer;
}

/**
 * send registration sequence to MMGR
 *
 * @param [in] p_lib
 *
 * @private
 *
 * @return E_ERR_CLI_BAD_HANDLE if handle is invalid
 * @return E_ERR_CLI_FAILED if timeout (20s) or MMGR not responsive
 * @return E_ERR_CLI_SUCCEED
 */
static e_err_mmgr_cli_t register_client(mmgr_lib_context_t *p_lib)
{
    e_err_mmgr_cli_t ret = E_ERR_CLI_FAILED;
    mmgr_cli_requests_t request[2];
    int timeout = DEF_MMGR_RESPONSIVE_TIMEOUT;
    struct timespec start, ts;

    CHECK_CLI_PARAM(p_lib, ret, out);

    request[0].id = E_MMGR_SET_NAME;
    request[0].len = strnlen(p_lib->cli_name, CLIENT_NAME_LEN);
    request[0].data = &p_lib->cli_name;

    request[1].id = E_MMGR_SET_EVENTS;
    request[1].len = sizeof(uint32_t);
    request[1].data = &p_lib->events;

    clock_gettime(CLOCK_REALTIME, &start);
    for (int i = 0; i < 2; i++) {
        if ((ret = send_msg(p_lib, &request[i], E_SEND_SINGLE, timeout)) !=
            E_ERR_CLI_SUCCEED)
            break;

        clock_gettime(CLOCK_REALTIME, &ts);
        timeout = DEF_MMGR_RESPONSIVE_TIMEOUT - (ts.tv_sec - start.tv_sec);
    }

    if (ret == E_ERR_CLI_SUCCEED)
        LOG_DEBUG("connected successfully", p_lib);
    else
        LOG_ERROR("failed to connect", p_lib);
out:
    return ret;
}

/**
 * send an mmgr request. This function uses the reader thread to wait for MMGR's
 * answer or wait for an event on the link. It depends of the choosen method.
 *
 * @param [in] p_lib library handle
 * @param [in] request request to send to the mmgr
 * @param [in] method If this function is called under the reader thread, you
 *        should use the E_SEND_SINGLE, E_SEND_THREADED should be used otherwise
 * @param [in] timeout (in seconds)
 *
 * @return E_ERR_CLI_BAD_HANDLE if handle is invalid
 * @return E_ERR_CLI_TIMEOUT if MMGR is not responsive or after a timeout of
 *         20s
 * @return E_ERR_CLI_REJECTED if this function is called under the callback
 * @return E_ERR_CLI_FAILED if request is NULL or invalid request id, timeout
 * @return E_ERR_CLI_SUCCEED if message accepted (ACK received)
 */
e_err_mmgr_cli_t send_msg(mmgr_lib_context_t *p_lib,
                          const mmgr_cli_requests_t *request,
                          e_send_method_t method, int timeout)
{
    e_err_mmgr_cli_t ret = E_ERR_CLI_FAILED;
    e_mmgr_errors_t err = E_ERR_SUCCESS;
    msg_t msg = { .data = NULL };
    size_t size = 0;
    struct timespec start, ts;
    int sleep_duration = 1;

    if (request == NULL) {
        LOG_ERROR("request is NULL", p_lib);
        goto out;
    }

    if (request->id >= E_MMGR_NUM_REQUESTS) {
        LOG_ERROR("bad request", p_lib);
        goto out;
    }

    if (timeout <= 0)
        goto timeout;

    set_ack(p_lib, E_MMGR_NUM_EVENTS);
    p_lib->set_msg[request->id] (&msg, (void *)request);

    clock_gettime(CLOCK_REALTIME, &start);
    /* The loop ends after MMGR approval or timeout */
    while (true) {
        /* Lock the mutex before sending the request. Otherwise, the answer can
         * be handled before waiting for the signal */
        pthread_mutex_lock(&p_lib->mtx_signal);

        size = SIZE_HEADER + msg.hdr.len;
        err = cnx_write(p_lib->fd_socket, msg.data, &size);
        if ((err != E_ERR_SUCCESS) || (size != (SIZE_HEADER + msg.hdr.len))) {
            LOG_ERROR("write failed", p_lib);
            break;
        }

        LOG_DEBUG("request (%s) sent successfully", p_lib,
                  g_mmgr_requests[request->id]);

        if ((request->id == E_MMGR_ACK_MODEM_COLD_RESET) ||
            (request->id == E_MMGR_ACK_MODEM_SHUTDOWN)) {
            ret = E_ERR_CLI_SUCCEED;
            break;
        }

        LOG_DEBUG("Waiting for answer", p_lib);

        if (method == E_SEND_SINGLE) {
            err = tty_wait_for_event(p_lib->fd_socket, timeout * 1000);
            if (err == E_ERR_TTY_TIMEOUT)
                break;
            msg_t answer;
            memset(&answer, 0, sizeof(msg_t));
            if (read_msg(p_lib, &answer) == E_ERR_CLI_SUCCEED)
                set_ack(p_lib, answer.hdr.id);
            msg_delete(&answer);
        } else {
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += timeout;
            if (pthread_cond_timedwait(&p_lib->cond, &p_lib->mtx_signal,
                                       &ts) == ETIMEDOUT) {
                err = E_ERR_TTY_TIMEOUT;
                break;
            } else {
                err = E_ERR_SUCCESS;
            }
        }

        if (get_ack(p_lib) == E_MMGR_ACK) {
            ret = E_ERR_CLI_SUCCEED;
            break;
        }
        pthread_mutex_unlock(&p_lib->mtx_signal);

        clock_gettime(CLOCK_REALTIME, &ts);
        timeout = DEF_MMGR_RESPONSIVE_TIMEOUT - (ts.tv_sec - start.tv_sec);
        if ((timeout > 0) && (++sleep_duration <= timeout))
            sleep(sleep_duration);
        else
            break;              /* timeout expired */
    }

timeout:
    if (err == E_ERR_TTY_TIMEOUT) {
        /* This happens if: MMGR is not responsive OR if client's callback
         * takes too much time (E_SEND_THREAD only). Indeed, the callback is
         * called by the consumer thread. */
        LOG_DEBUG("timeout for request (%s)", p_lib,
                  g_mmgr_requests[request->id]);
        ret = E_ERR_CLI_TIMEOUT;
    }

out:
    /* when we break the do{}while loop, the mutex is not ALWAYS unlocked. To
     * be safe, try to lock it before unlocking it */
    pthread_mutex_trylock(&p_lib->mtx_signal);
    pthread_mutex_unlock(&p_lib->mtx_signal);
    msg_delete(&msg);
    return ret;
}

/**
 * handle mmgr events and dispatch them
 *
 * @param [in] p_lib private structure
 *
 * @return E_ERR_CLI_BAD_HANDLE if p_lib is NULL
 * @return E_ERR_CLI_SUCCEED at the end
 */
e_err_mmgr_cli_t read_events(mmgr_lib_context_t *p_lib)
{
    e_err_mmgr_cli_t ret = E_ERR_CLI_SUCCEED;
    fd_set rfds;
    int fd_max;

    CHECK_CLI_PARAM(p_lib, ret, out);

    if (p_lib->fd_socket > p_lib->fd_pipe[READ])
        fd_max = p_lib->fd_socket;
    else
        fd_max = p_lib->fd_pipe[READ];

    do {
        FD_ZERO(&rfds);
        FD_SET(p_lib->fd_pipe[READ], &rfds);
        FD_SET(p_lib->fd_socket, &rfds);

        if (select(fd_max + 1, &rfds, NULL, NULL, NULL) < 0) {
            LOG_ERROR("select failed (%s)", p_lib, strerror(errno));
            break;
        }

        ret = handle_events(p_lib, &rfds);
        if (ret == E_ERR_CLI_BAD_CNX_STATE)
            ret = handle_disconnection(p_lib);
    } while (ret != E_ERR_CLI_FAILED);

out:
    pthread_exit(&ret);
    return ret;
}

/**
 * connect the client to mmgr
 *
 * @param [in] p_lib library handle
 *
 * @return E_ERR_CLI_FAILED
 * @return E_ERR_CLI_BAD_HANDLE
 * @return E_ERR_CLI_SUCCEED
 */
e_err_mmgr_cli_t cli_connect(mmgr_lib_context_t *p_lib)
{
    e_err_mmgr_cli_t ret = E_ERR_CLI_FAILED;
    int fd = CLOSED_FD;

    fd = socket_local_client(p_lib->cnx_name,
                             ANDROID_SOCKET_NAMESPACE_RESERVED, SOCK_STREAM);
    if (fd < 0) {
        LOG_ERROR("failed to open socket", p_lib);
    } else {
        pthread_mutex_lock(&p_lib->mtx);
        p_lib->fd_socket = fd;
        pthread_mutex_unlock(&p_lib->mtx);

        ret = register_client(p_lib);
    }

    return ret;
}

/**
 * disconnect the client
 *
 * @param p_lib library handle
 *
 * @return E_ERR_CLI_BAD_HANDLE
 * @return E_ERR_CLI_SUCCEED
 * @return E_ERR_CLI_FAILED if already disconnected
 */
e_err_mmgr_cli_t cli_disconnect(mmgr_lib_context_t *p_lib)
{
    e_err_mmgr_cli_t ret = E_ERR_CLI_FAILED;
    char msg = 0;
    ssize_t size;

    if (is_connected(p_lib)) {
        LOG_DEBUG("writing signal", p_lib);
        if ((size = write(p_lib->fd_pipe[WRITE], &msg, sizeof(msg))) < -1)
            LOG_ERROR("write failed (%s)", p_lib, strerror(errno));
    }

    LOG_DEBUG("waiting for end of reading thread", p_lib);
    if (p_lib->thr_id != -1) {
        pthread_join(p_lib->thr_id, NULL);
        p_lib->thr_id = -1;
    }

    LOG_DEBUG("reading thread stopped", p_lib);

    if (!is_connected(p_lib)) {
        LOG_DEBUG("is disconnected", p_lib);
        ret = E_ERR_CLI_SUCCEED;
    } else {
        LOG_ERROR("failed to disconnect", p_lib);
    }

    return ret;
}
