/* Modem Manager client library - interface source file
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

#include "utils.h"
#include "mmgr_cli.h"
#include "msg_to_data.h"
#include "data_to_msg.h"
#include "msg_format.h"

#define DEFAULT_TID 1

#define CHECK_EVENT(ctx, id, err, out) do { \
        if (id >= E_MMGR_NUM_EVENTS) { \
            LOG_ERROR("unknown event", ctx); \
            ret = E_ERR_CLI_FAILED; \
            goto out; \
        } \
} while (0)

inline e_mmgr_events_t is_request_rejected(mmgr_lib_context_t *p_lib)
{
    e_err_mmgr_cli_t ret = E_ERR_CLI_SUCCEED;

    pthread_mutex_lock(&p_lib->mtx);
    if (p_lib->tid == gettid())
        ret = E_ERR_CLI_REJECTED;
    pthread_mutex_unlock(&p_lib->mtx);

    return ret;
}

inline bool is_lock(mmgr_lib_context_t *ctx)
{
    bool answer;

    pthread_mutex_lock(&ctx->mtx);
    answer = ctx->lock;
    pthread_mutex_unlock(&ctx->mtx);

    return answer;
}

inline void set_lock(mmgr_lib_context_t *ctx, bool state)
{
    pthread_mutex_lock(&ctx->mtx);
    ctx->lock = state;
    pthread_mutex_unlock(&ctx->mtx);
}

/**
 * @see mmgr_cli.h
 */
e_err_mmgr_cli_t mmgr_cli_send_msg(mmgr_cli_handle_t *handle,
                                   const mmgr_cli_requests_t *request)
{
    e_err_mmgr_cli_t ret = E_ERR_CLI_FAILED;
    mmgr_lib_context_t *p_lib = (mmgr_lib_context_t *)handle;

    CHECK_CLI_PARAM(handle, ret, out);

    if (!is_connected(p_lib)) {
        ret = E_ERR_CLI_BAD_CNX_STATE;
        LOG_ERROR("request not sent", p_lib);
    } else {
        ret = is_request_rejected(p_lib);
        if (ret != E_ERR_CLI_REJECTED)
            ret = send_msg(p_lib, request, E_SEND_THREADED,
                           DEF_MMGR_RESPONSIVE_TIMEOUT);
    }

out:
    return ret;
}

/**
 * @see mmgr_cli.h
 */
e_err_mmgr_cli_t mmgr_cli_create_handle(mmgr_cli_handle_t **handle,
                                        const char *client_name, void *context)
{
    e_err_mmgr_cli_t ret = E_ERR_CLI_SUCCEED;
    mmgr_lib_context_t *p_lib = NULL;
    int i;

    CHECK_CLI_PARAM(handle, ret, out);

    if (*handle != NULL) {
        LOGE("*handle is not NULL");
        ret = E_ERR_CLI_BAD_HANDLE;
        goto out;
    }

    if (client_name == NULL) {
        LOGE("client_name is NULL");
        ret = E_ERR_CLI_FAILED;
        goto out;
    }

    p_lib = calloc(1, sizeof(mmgr_lib_context_t));
    if (p_lib == NULL) {
        LOGE("failed to allocate");
        ret = E_ERR_CLI_FAILED;
        goto out;
    }

    pthread_mutex_init(&p_lib->mtx, NULL);
    pthread_mutex_init(&p_lib->mtx_signal, NULL);
    pthread_cond_init(&p_lib->cond, NULL);
    p_lib->events = (0x1 << E_MMGR_ACK) | (0x1 << E_MMGR_NACK);
    p_lib->lock = false;
    p_lib->cli_ctx = context;
    p_lib->fd_socket = CLOSED_FD;
    p_lib->fd_pipe[READ] = CLOSED_FD;
    p_lib->fd_pipe[WRITE] = CLOSED_FD;
    p_lib->connected = E_CNX_DISCONNECTED;
    p_lib->thr_id = -1;
    p_lib->tid = DEFAULT_TID;
    p_lib->ack = E_MMGR_NUM_REQUESTS;
    strncpy(p_lib->cli_name, client_name, CLIENT_NAME_LEN - 1);

    for (i = 0; i < E_MMGR_NUM_REQUESTS; i++)
        p_lib->set_msg[i] = msg_set_empty;

    for (i = 0; i < E_MMGR_NUM_EVENTS; i++) {
        p_lib->set_data[i] = set_data_empty;
        p_lib->free_data[i] = free_data_empty;
    }

    p_lib->set_msg[E_MMGR_SET_NAME] = set_msg_name;
    p_lib->set_msg[E_MMGR_SET_EVENTS] = set_msg_filter;
    p_lib->set_msg[E_MMGR_REQUEST_MODEM_RESTART] = set_msg_restart;
    p_lib->set_msg[E_MMGR_REQUEST_MODEM_RECOVERY] = set_msg_recovery;

    p_lib->set_data[E_MMGR_RESPONSE_MODEM_HW_ID] = set_data_hw_id;
    p_lib->free_data[E_MMGR_RESPONSE_MODEM_HW_ID] = free_data_hw_id;

    p_lib->set_data[E_MMGR_RESPONSE_FUSE_INFO] = set_data_fuse_info;
    p_lib->free_data[E_MMGR_RESPONSE_FUSE_INFO] = free_one_element_struct;

    p_lib->set_data[E_MMGR_NOTIFY_AP_RESET] = set_data_ap_reset;
    p_lib->free_data[E_MMGR_NOTIFY_AP_RESET] = free_data_ap_reset;

    p_lib->set_data[E_MMGR_NOTIFY_CORE_DUMP_COMPLETE] = set_data_core_dump;
    p_lib->free_data[E_MMGR_NOTIFY_CORE_DUMP_COMPLETE] = free_data_core_dump;

    p_lib->set_data[E_MMGR_NOTIFY_TFT_EVENT] = set_data_tft_event;
    p_lib->free_data[E_MMGR_NOTIFY_TFT_EVENT] = free_data_tft_event;

    p_lib->set_data[E_MMGR_RESPONSE_MODEM_FW_RESULT] = set_data_fw_result;
    p_lib->free_data[E_MMGR_RESPONSE_MODEM_FW_RESULT] = free_one_element_struct;

    *handle = (mmgr_cli_handle_t *)p_lib;
    LOG_DEBUG("handle created successfully", p_lib);
out:
    return ret;
}

/**
 * @see mmgr_cli.h
 */
e_err_mmgr_cli_t mmgr_cli_delete_handle(mmgr_cli_handle_t *handle)
{
    e_err_mmgr_cli_t ret = E_ERR_CLI_SUCCEED;
    mmgr_lib_context_t *p_lib = (mmgr_lib_context_t *)handle;

    CHECK_CLI_PARAM(p_lib, ret, out);

    if (!is_connected(p_lib)) {
        free(p_lib);
        LOGD("handle freed successfully");
    } else {
        ret = E_ERR_CLI_BAD_CNX_STATE;
        LOG_ERROR("handle not freed", p_lib);
    }

out:
    return ret;
}

/**
 * @see mmgr_cli.h
 */
e_err_mmgr_cli_t mmgr_cli_subscribe_event(mmgr_cli_handle_t *handle,
                                          event_handler func,
                                          e_mmgr_events_t id)
{
    e_err_mmgr_cli_t ret = E_ERR_CLI_SUCCEED;
    mmgr_lib_context_t *p_lib = (mmgr_lib_context_t *)handle;

    CHECK_CLI_PARAM(p_lib, ret, out);

    if (is_connected(p_lib)) {
        LOG_ERROR("Wrong connection state. subscription has been rejected",
                  p_lib);
        ret = E_ERR_CLI_BAD_CNX_STATE;
        goto out;
    }

    CHECK_EVENT(p_lib, id, ret, out);
    if ((id == E_MMGR_ACK) || (id == E_MMGR_NACK)) {
        ret = E_ERR_CLI_FAILED;
        goto out;
    }

    if (func == NULL) {
        LOG_ERROR("function is NULL", p_lib);
        ret = E_ERR_CLI_FAILED;
        goto out;
    }

    pthread_mutex_lock(&p_lib->mtx);
    if (p_lib->func[id] != NULL) {
        ret = E_ERR_CLI_FAILED;
    } else {
        p_lib->events |= (0x01 << id);
        p_lib->func[id] = func;
    }
    pthread_mutex_unlock(&p_lib->mtx);

    if (ret == E_ERR_CLI_SUCCEED)
        LOG_DEBUG("event (%s) subscribed successfully", p_lib,
                  g_mmgr_events[id]);
    else
        LOG_ERROR("event (%s) already configured", p_lib, g_mmgr_events[id]);

out:
    return ret;
}

/**
 * @see mmgr_cli.h
 */
e_err_mmgr_cli_t mmgr_cli_unsubscribe_event(mmgr_cli_handle_t *handle,
                                            e_mmgr_events_t id)
{
    e_err_mmgr_cli_t ret = E_ERR_CLI_SUCCEED;
    mmgr_lib_context_t *p_lib = (mmgr_lib_context_t *)handle;

    CHECK_CLI_PARAM(p_lib, ret, out);

    if (is_connected(p_lib)) {
        LOG_ERROR("Wrong connection state. Request rejected", p_lib);
        ret = E_ERR_CLI_BAD_CNX_STATE;
        goto out;
    }

    CHECK_EVENT(p_lib, id, ret, out);
    if ((id == E_MMGR_ACK) || (id == E_MMGR_NACK)) {
        ret = E_ERR_CLI_FAILED;
        goto out;
    }

    pthread_mutex_lock(&p_lib->mtx);
    p_lib->events &= ~(0x01 << id);
    p_lib->func[id] = NULL;
    pthread_mutex_unlock(&p_lib->mtx);

    LOG_DEBUG("event (%s) unsubscribed successfully", p_lib, g_mmgr_events[id]);
out:
    return ret;
}

/**
 * @see mmgr_cli.h
 */
e_err_mmgr_cli_t mmgr_cli_connect(mmgr_cli_handle_t *handle)
{
    e_err_mmgr_cli_t ret = E_ERR_CLI_SUCCEED;
    mmgr_lib_context_t *p_lib = (mmgr_lib_context_t *)handle;
    int err = 0;

    CHECK_CLI_PARAM(p_lib, ret, out);

    if (is_connected(p_lib)) {
        LOG_ERROR("already connected", p_lib);
        goto out;
    }

    if (pipe(p_lib->fd_pipe) < 0) {
        LOG_ERROR("failed to create pipe (%s)", p_lib, strerror(errno));
        ret = E_ERR_CLI_FAILED;
    } else {
        ret = cli_connect(p_lib);
    }

    if (ret == E_ERR_CLI_SUCCEED) {
        err = pthread_create(&p_lib->thr_id, NULL, (void *)read_events, p_lib);
        if (err != 0) {
            LOG_ERROR("failed to create the reader thread. "
                      "Disconnect the client", p_lib);
            ret = E_ERR_CLI_FAILED;
        } else {
            pthread_mutex_lock(&p_lib->mtx);
            p_lib->connected = E_CNX_CONNECTED;
            pthread_mutex_unlock(&p_lib->mtx);
        }
    }

out:
    if (ret != E_ERR_CLI_SUCCEED)
        handle_disconnection(p_lib); // Clean all currently used resources

    return ret;
}

/**
 * @see mmgr_cli.h
 */
e_err_mmgr_cli_t mmgr_cli_disconnect(mmgr_cli_handle_t *handle)
{
    e_err_mmgr_cli_t ret = E_ERR_CLI_FAILED;
    mmgr_lib_context_t *p_lib = (mmgr_lib_context_t *)handle;

    CHECK_CLI_PARAM(p_lib, ret, out);

    if (is_connected(p_lib))
        ret = cli_disconnect(p_lib);
    else
        ret = E_ERR_CLI_BAD_CNX_STATE;

out:
    return ret;
}

/**
 * @see mmgr_cli.h
 */
e_err_mmgr_cli_t mmgr_cli_lock(mmgr_cli_handle_t *handle)
{
    e_err_mmgr_cli_t ret = E_ERR_CLI_SUCCEED;
    mmgr_lib_context_t *p_lib = (mmgr_lib_context_t *)handle;
    mmgr_cli_requests_t request = { .id = E_MMGR_RESOURCE_ACQUIRE };

    CHECK_CLI_PARAM(p_lib, ret, out);

    if (!is_connected(p_lib)) {
        ret = E_ERR_CLI_BAD_CNX_STATE;
        LOG_ERROR("not connected", p_lib);
        goto out;
    }

    ret = is_request_rejected(p_lib);
    if (ret == E_ERR_CLI_REJECTED)
        goto out;

    if (is_lock(p_lib)) {
        LOG_ERROR("Already locked", p_lib);
        ret = E_ERR_CLI_ALREADY_LOCK;
    } else {
        ret = send_msg(p_lib, &request, E_SEND_THREADED,
                       DEF_MMGR_RESPONSIVE_TIMEOUT);
        if (ret == E_ERR_CLI_SUCCEED)
            set_lock(p_lib, true);
        else
            ret = E_ERR_FAILED;
    }
out:
    return ret;
}

/**
 * @see mmgr_cli.h
 */
e_err_mmgr_cli_t mmgr_cli_unlock(mmgr_cli_handle_t *handle)
{
    e_err_mmgr_cli_t ret = E_ERR_CLI_SUCCEED;
    mmgr_lib_context_t *p_lib = (mmgr_lib_context_t *)handle;
    mmgr_cli_requests_t request = { .id = E_MMGR_RESOURCE_RELEASE };

    CHECK_CLI_PARAM(p_lib, ret, out);

    if (!is_connected(p_lib)) {
        ret = E_ERR_CLI_BAD_CNX_STATE;
        LOG_ERROR("not connected", p_lib);
        goto out;
    }

    ret = is_request_rejected(p_lib);
    if (ret == E_ERR_CLI_REJECTED)
        goto out;

    if (!is_lock(p_lib)) {
        LOG_ERROR("Already unlocked", p_lib);
        ret = E_ERR_CLI_ALREADY_UNLOCK;
    } else {
        ret = send_msg(p_lib, &request, E_SEND_THREADED,
                       DEF_MMGR_RESPONSIVE_TIMEOUT);
        if (ret == E_ERR_CLI_SUCCEED)
            set_lock(p_lib, false);
        else
            ret = E_ERR_FAILED;
    }
out:
    return ret;
}

/**
 * @see mmgr_cli.h
 */
int mmgr_cli_get_fd(mmgr_cli_handle_t *hdle)
{
    int fd = CLOSED_FD;
    mmgr_lib_context_t *ctx = (mmgr_lib_context_t *)hdle;

    if (ctx)
        fd = ctx->fd_socket;

    return fd;
}
