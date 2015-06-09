/* Modem Manager client library - common header
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

#ifndef __MMGR_CLI_COMMON_H__
#define __MMGR_CLI_COMMON_H__

#define MMGR_FW_OPERATIONS
#include <stdbool.h>
#include "mmgr_cli.h"
#include "msg_format.h"
#include "client_cnx.h"

#define LOG_NDEBUG 0
#define LOG_TAG MODULE_NAME
#include <utils/Log.h>

#define LOG_ERROR(format, ctx, args ...) \
    do { ALOGE("%s - (fd:%d name:%s) - " format, __FUNCTION__, \
               ctx->fd_socket, ctx->cli_name, ## args); } while (0)
#define LOG_DEBUG(format, ctx, args ...) \
    do { ALOGD("%s - (fd:%d name:%s) - " format, __FUNCTION__, \
               ctx->fd_socket, ctx->cli_name, ## args); } while (0)
#define LOG_VERBOSE(format, ctx, args ...) \
    do { ALOGV("%s - (fd:%d name:%s) - " format, __FUNCTION__, \
               ctx->fd_socket, ctx->cli_name, ## args); } while (0)

#define CNX_STATES \
    X(DISCONNECTED), \
    X(CONNECTED), \
    X(RECONNECT)

typedef enum cnx_state {
#undef X
#define X(a) E_CNX_ ## a
    CNX_STATES
} cnx_state_t;

typedef e_mmgr_errors_t (*msg_handler) (msg_t *, mmgr_cli_event_t *);
typedef e_mmgr_errors_t (*free_handler) (mmgr_cli_event_t *);

/**
 * internal structure for mmgr_cli
 *
 * @private
 */
typedef struct mmgr_lib_context {
    uint32_t events;
    pthread_t thr_id;
    pthread_mutex_t mtx;
    void *cli_ctx;
    cnx_state_t connected;
    int fd_socket;
    int fd_pipe[2];
    event_handler func[E_MMGR_NUM_EVENTS];
    char cli_name[CLIENT_NAME_LEN];
    bool lock;
    msg_handler set_msg[E_MMGR_NUM_REQUESTS];
    msg_handler set_data[E_MMGR_NUM_EVENTS];
    free_handler free_data[E_MMGR_NUM_EVENTS];
    /* variables used for sync op: */
    pthread_mutex_t mtx_signal;
    pthread_cond_t cond;
    e_mmgr_events_t ack;
    pid_t tid;
    char cnx_name[MMGR_SOCKET_LEN];
} mmgr_lib_context_t;

#define CLOSED_FD -1

#define xstr(s) str(s)
#define str(s) #s

#define CHECK_CLI_PARAM(handle, err, out) do { \
        if (handle == NULL) { \
            ALOGE(xstr(handle) "%s - is NULL", __FUNCTION__); \
            err = E_ERR_CLI_BAD_HANDLE; \
            goto out; \
        } \
} while (0)

extern const char *g_mmgr_events[];

#define READ 0
#define WRITE 1

#endif                          /* __MMGR_CLI_COMMON_H__ */
