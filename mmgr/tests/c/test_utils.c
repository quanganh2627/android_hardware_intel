/* Modem Manager (MMGR) test application - utils source file
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
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/wait.h>
#include <cutils/sockets.h>
#include "at.h"
#include "common.h"
#include "errors.h"
#include "file.h"
#include "property.h"
#include "test_utils.h"
#include "tty.h"
#include "msg_format.h"

#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif

#define LEN_FILTER 10
#define WAKE_LOCK_SYSFS "/sys/power/wake_lock"

#define AT_SELF_RESET "AT+CFUN=15\r"
#define AT_CORE_DUMP "AT+XLOG=4\r"

#define BZ_MSG "\n\n*** YOU SHOULD RAISE A BZ ***\n"
#define WAKE_BUF_SIZE 1024

/* define used by monkey test: */
#define NB_REQUEST_MAX 128
#define RANDOM_TIMEOUT_VALUE ((random() % 10) * 100000)


const char *g_mmgr_requests[] = {
#undef X
#define X(a) #a
    MMGR_REQUESTS
};

const char *g_mmgr_events[] = {
#undef X
#define X(a) #a
    MMGR_EVENTS
};

static inline void set_monkey_state(monkey_ctx_t *ctx, bool state)
{
    pthread_mutex_lock(&ctx->mtx);
    ctx->state = state;
    pthread_mutex_unlock(&ctx->mtx);
}

static inline bool get_monkey_state(monkey_ctx_t *ctx)
{
    bool state = false;

    pthread_mutex_lock(&ctx->mtx);
    state = ctx->state;
    pthread_mutex_unlock(&ctx->mtx);

    return state;
}

static void monkey_client(monkey_ctx_t *ctx)
{
    int tid = gettid();
    char name[CLIENT_NAME_LEN];
    mmgr_cli_handle_t *lib = NULL;
    e_mmgr_requests_t ids[] = { E_MMGR_RESOURCE_ACQUIRE,
                                E_MMGR_RESOURCE_RELEASE,
                                E_MMGR_ACK_MODEM_COLD_RESET,
                                E_MMGR_ACK_MODEM_SHUTDOWN,
                                E_MMGR_REQUEST_MODEM_RESTART, };

    ASSERT(ctx != NULL);

    snprintf(name, sizeof(name), "monkey_tid_%d", tid);
    ASSERT(E_ERR_CLI_SUCCEED == mmgr_cli_create_handle(&lib, name, NULL));

    LOG_DEBUG("(tid:%d): start", tid);
    srandom(time(NULL));

    while (get_monkey_state(ctx)) {
        usleep(RANDOM_TIMEOUT_VALUE);
        while (E_ERR_CLI_SUCCEED != mmgr_cli_connect(lib)) ;
        LOG_DEBUG("(tid:%d) CONNECTED", tid);
        int fd = mmgr_cli_get_fd(lib);
        int nb_requests = random() % NB_REQUEST_MAX;

        for (int i = 0; (i < nb_requests) && get_monkey_state(ctx); i++) {
            msg_t msg = { .data = NULL };
            char *msg_data = NULL;
            size_t size = 0;
            e_mmgr_events_t id = ids[random() % ARRAY_SIZE(ids)];
            LOG_DEBUG("(tid:%d) request:%d", tid, id);
            msg_prepare(&msg, &msg_data, id, &size);
            write(fd, msg.data, size);
            msg_delete(&msg);
            usleep(RANDOM_TIMEOUT_VALUE);
        }
        ASSERT(E_ERR_CLI_SUCCEED == mmgr_cli_disconnect(lib));
        LOG_DEBUG("(tid:%d) DISCONNECTED", tid);
    }

    ASSERT(E_ERR_CLI_SUCCEED == mmgr_cli_delete_handle(lib));
    LOG_DEBUG("(tid:%d) end", tid);
}


static void start_monkey(monkey_ctx_t *ctx)
{
    ASSERT(ctx != NULL);

    LOG_DEBUG("nb threads: %d", ctx->nb_threads);
    ctx->ids = malloc(sizeof(pthread_t) * ctx->nb_threads);
    ASSERT(ctx->ids != NULL);

    pthread_mutex_init(&ctx->mtx, NULL);
    set_monkey_state(ctx, true);

    for (int i = 0; i < ctx->nb_threads; i++)
        pthread_create(&ctx->ids[i], NULL, (void *)monkey_client, ctx);
}

static void stop_monkey(monkey_ctx_t *ctx)
{
    ASSERT(ctx != NULL);

    set_monkey_state(ctx, false);

    for (int i = 0; i < ctx->nb_threads; i++)
        pthread_join(ctx->ids[i], NULL);

    free(ctx->ids);
}

/**
 * Update events variable
 *
 * @param [in,out] test_data thread handler
 * @param [in] state new test state
 *
 * @return E_ERR_SUCCESS if successful
 */
static e_mmgr_errors_t events_set(test_data_t *test_data, e_events_t state)
{
    ASSERT(test_data != NULL);

    pthread_mutex_lock(&test_data->mutex);
    test_data->events |= state;
    pthread_mutex_unlock(&test_data->mutex);

    return E_ERR_SUCCESS;
}

/**
 * Get events variable
 *
 * @param [in,out] test_data thread handler
 *
 * @return e_events_t
 */
e_events_t events_get(test_data_t *test_data)
{
    e_events_t ev = E_EVENTS_NONE;

    if (test_data == NULL) {
        ev = E_EVENTS_ERROR_OCCURED;
    } else {
        pthread_mutex_lock(&test_data->mutex);
        ev = test_data->events;
        pthread_mutex_unlock(&test_data->mutex);
    }
    return ev;
}

/**
 * Update modem_state variable
 *
 * @param [in,out] test_data thread handler
 * @param [in] state new modem state
 *
 * @return E_ERR_SUCCESS if successful
 */
void modem_state_set(test_data_t *test_data, e_mmgr_events_t state)
{
    ASSERT(test_data != NULL);

    LOG_DEBUG("modem_state_set: %s", g_mmgr_events[(int)state]);

    ASSERT(write(test_data->fd_pipe[WRITE], &state,
                 sizeof(e_mmgr_events_t)) == sizeof(e_mmgr_events_t));
}

/**
 * Get modem_state variable
 *
 * @param [in,out] test_data thread handler
 * @param [out] state current state
 *
 * @return E_ERR_SUCCESS if successful
 */
static e_mmgr_errors_t modem_state_get(test_data_t *test_data,
                                       e_mmgr_events_t *state)
{
    ASSERT(test_data != NULL);
    ASSERT(state != NULL);

    for (;; ) {
        int err =
            read(test_data->fd_pipe[READ], state, sizeof(e_mmgr_events_t));
        if (err == sizeof(e_mmgr_events_t)) {
            break;
        } else if ((err == -1) && (errno == EINTR)) {
            continue;
        } else {
            LOG_ERROR("event not received correctly (errno: %d)", errno);
            return E_ERR_FAILED;
        }
    }

    return E_ERR_SUCCESS;
}

/**
 * This function will send the command message to DLC
 *
 * @param [in] path dlc path to use
 * @param [in] command AT request
 * @param [in] command_size AT request size
 *
 * @return E_ERR_SUCCESS command sends and 'OK' received
 * @return E_ERR_TTY_POLLHUP POLLHUP detected during read
 * @return E_ERR_TTY_BAD_FD if a bad file descriptor is provided
 */
e_mmgr_errors_t send_at_cmd(char *path, char *command, int command_size)
{
    int fd_tty = CLOSED_FD;
    e_mmgr_errors_t ret = E_ERR_FAILED;

    ASSERT(command != NULL);

    tty_open(path, &fd_tty);
    if (fd_tty < 0) {
        LOG_ERROR("Failed to open %s", path);
    } else {
        ret = send_at_retry(fd_tty, command, command_size, 4, 2500);
        close(fd_tty);
    }

    return ret;
}

bool get_wakelock_state(void)
{
    bool state = false;
    int fd;
    int size = WAKE_BUF_SIZE;
    char data[WAKE_BUF_SIZE];

    while ((fd = open(WAKE_LOCK_SYSFS, O_RDONLY)) < 0) {
        sleep(1);
        LOG_DEBUG("retry to open %s", WAKE_LOCK_SYSFS);
    }

    memset(data, 0, size);
    size = read(fd, data, size);

    if ((size > 0) && (strstr(data, MODULE_NAME) != NULL))
        state = true;
    close(fd);

    return state;
}


/**
 * Check if wakelock state was reached
 *
 * @param [in] state wakelock state to reach
 *
 * @return E_ERR_SUCCESS if state reached
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t check_wakelock(bool state)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    bool wake_state = false;
    int remaining = 0;
    char *state_str[] = { "not ", "" };

    for (remaining = 0; remaining < 10; remaining++) {
        wake_state = get_wakelock_state();
        if (wake_state != state) {
            usleep(500 * 1000);
        } else {
            LOG_DEBUG("wakelock has reached correct state: %sset",
                      state_str[wake_state]);
            ret = E_ERR_SUCCESS;
            break;
        }
    }
    if (ret != E_ERR_SUCCESS)
        LOG_ERROR("*** wakelock is %sset and should be %sset ***",
                  state_str[wake_state], state_str[state]);

    return ret;
}

/**
 * Wait for modem state with timeout
 *
 * @param [in] test_data test_data
 * @param [in] state state to reach
 * @param [in] timeout timeout (in second)
 *
 * @return E_ERR_SUCCESS if state reached
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t wait_for_state(test_data_t *test_data, int state, int timeout)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    e_mmgr_events_t current_state = E_MMGR_NUM_EVENTS;
    struct timespec ts;
    struct timeval start;
    struct timeval current;
    int remaining = 0;

    ASSERT(test_data != NULL);

    pthread_mutex_lock(&test_data->mutex);
    test_data->waited_state = state;
    pthread_mutex_unlock(&test_data->mutex);

    LOG_DEBUG("waiting for state: %s. (during %ds max)", g_mmgr_events[state],
              timeout);

    gettimeofday(&start, NULL);

    do {
        gettimeofday(&current, NULL);
        ts.tv_sec = current.tv_sec;
        ts.tv_nsec = current.tv_usec * 1000;
        remaining = timeout - (current.tv_sec - start.tv_sec);
        if (remaining > 0)
            /*
             * A timeout of 1s is used here to save time in several UCs:
             * - if modem is OOS or plateform is shutdown
             * - if we have already reached the state
             */
            ts.tv_sec += 1;

        if (sem_timedwait(&test_data->sem, &ts) == 0) {
            if (modem_state_get(test_data, &current_state) == E_ERR_FAILED)
                break;
        } else {
            continue;
        }

        if (current_state == test_data->waited_state) {
            LOG_DEBUG("modem state: %s", g_mmgr_events[current_state]);
            ret = E_ERR_SUCCESS;
            break;
        } else if ((current_state == E_MMGR_EVENT_MODEM_OUT_OF_SERVICE) ||
                   (current_state == E_MMGR_NOTIFY_PLATFORM_REBOOT)) {
            LOG_DEBUG("modem state: %s", g_mmgr_events[current_state]);
            events_set(test_data, E_EVENTS_MODEM_OOS);
            break;
        }
    } while ((remaining > 1) && (current_state != test_data->waited_state));

    return ret;
}

/**
 * update modem state and send signal
 *
 * @param [in] id current event
 * @param [in] test_data test data
 *
 * @return E_ERR_SUCCESS
 */
static e_mmgr_errors_t set_and_notify(e_mmgr_events_t id,
                                      test_data_t *test_data)
{
    ASSERT(test_data != NULL);

    if (id < E_MMGR_NUM_EVENTS) {
        modem_state_set(test_data, id);

        ASSERT(sem_post(&test_data->sem) == 0);

        LOG_DEBUG("current state: %s", g_mmgr_events[id]);
    }

    return E_ERR_SUCCESS;
}

static int event_tft(mmgr_cli_event_t *ev)
{
    int ret = 1;
    e_mmgr_errors_t err = E_ERR_FAILED;
    test_data_t *data = NULL;
    mmgr_cli_tft_event_t *cli_ev = NULL;
    static const char *const ev_name = "TFT_EVENT_TEST";

    ASSERT(ev != NULL);

    data = (test_data_t *)ev->context;
    if (data == NULL)
        goto out;

    cli_ev = (mmgr_cli_tft_event_t *)ev->data;
    if (cli_ev == NULL) {
        LOG_ERROR("empty data");
        goto out;
    }
    LOG_DEBUG(
        "tft event {type:%d name_len:%d name:\"%s\" log:0x%X, num_data:%d}",
        cli_ev->type,
        cli_ev->name_len, cli_ev->name, cli_ev->log, cli_ev->num_data);
    for (size_t i = 0; i < cli_ev->num_data; i++) {
        LOG_DEBUG("data[%d] {len:%d value:\"%s\"}", i, cli_ev->data[i].len,
                  cli_ev->data[i].value);
    }
    if ((cli_ev->name_len == strlen(ev_name))
        && (strncmp(ev_name, cli_ev->name, cli_ev->name_len) == 0)
        && (cli_ev->num_data == MMGR_CLI_MAX_TFT_EVENT_DATA))
        events_set(data, E_EVENTS_SUCCEED);

    err = set_and_notify(ev->id, (test_data_t *)ev->context);
    if (err == E_ERR_SUCCESS)
        ret = 0;

out:
    if (ret == 1)
        events_set(data, E_EVENTS_ERROR_OCCURED);
    return ret;
}


/**
 * callback for ap reset
 *
 * @param [in] ev current info callback data
 *
 * @return 0 if successful
 * @return 1 otherwise
 */
static int event_ap_reset(mmgr_cli_event_t *ev)
{
    int ret = 1;
    e_mmgr_errors_t err = E_ERR_FAILED;
    test_data_t *data = NULL;
    mmgr_cli_ap_reset_t *ap = NULL;

    ASSERT(ev != NULL);

    data = (test_data_t *)ev->context;
    if (data == NULL)
        goto out;

    ap = (mmgr_cli_ap_reset_t *)ev->data;
    if (ap == NULL) {
        LOG_ERROR("empty data");
        goto out;
    }
    LOG_DEBUG("AP reset asked by: %s (len: %d)", ap->name, ap->len);
    if ((ap->len == strlen(MODULE_NAME)) &&
        (strncmp(MODULE_NAME, ap->name, ap->len) == 0))
        events_set(data, E_EVENTS_SUCCEED);

    err = set_and_notify(ev->id, (test_data_t *)ev->context);
    if (err == E_ERR_SUCCESS)
        ret = 0;

out:
    if (ret == 1)
        events_set(data, E_EVENTS_ERROR_OCCURED);
    return ret;
}

/**
 * callback for core dump
 *
 * @param [in] ev current info callback data
 *
 * @return 0 if successful
 * @return 1 otherwise
 */
static int event_core_dump(mmgr_cli_event_t *ev)
{
    int ret = 1;
    test_data_t *data = NULL;
    mmgr_cli_core_dump_t *cd = NULL;
    static const char const *cd_state[] = {
#undef X
#define X(a) #a
        CORE_DUMP_STATE
    };

    if (ev == NULL)
        goto out;

    data = (test_data_t *)ev->context;
    if (data == NULL)
        goto out;

    cd = (mmgr_cli_core_dump_t *)ev->data;
    if ((cd == NULL) || (cd->path == NULL)) {
        LOG_ERROR("empty data");
        goto out;
    }

    LOG_DEBUG("state: %s", cd_state[cd->state]);

    if (cd->state == E_CD_SUCCEED) {
        if (file_exist(cd->path)) {
            LOG_DEBUG("core dump found: %s", cd->path);
            ret = 0;
        } else {
            char *filename = basename(cd->path);
            char *folders[] = { "/mnt/shell/emulated/0/logs/", "/sdcard/" };
            for (size_t i = 0; i < ARRAY_SIZE(folders); i++) {
                LOG_DEBUG("look at: %s", folders[i]);
                size_t found;
                char **files = file_find(folders[i], filename, &found);
                if (found == 1) {
                    LOG_DEBUG("core dump found: %s", files[0]);
                    ret = 0;
                }

                for (size_t i = 0; i < found; i++)
                    free(files[i]);
                free(files);

                if (!ret)
                    break;
            }
        }
        if (ret)
            LOG_ERROR("core dump (%s) NOT found", basename(cd->path));
    } else {
        LOG_ERROR("core dump retrieval has failed with reason: (%s)",
                  cd->reason);
    }

out:
    set_and_notify(ev->id, (test_data_t *)ev->context);
    if (ret == 1)
        events_set(data, E_EVENTS_ERROR_OCCURED);
    else
        events_set(data, E_EVENTS_SUCCEED);
    return ret;
}

/**
 * handle the E_MMGR_RESPONSE_MODEM_FW_RESULT event
 *
 * @param [in] ev current info callback data
 *
 * @return 0 if successful
 * @return 1 otherwise
 */
static int event_fw_status(mmgr_cli_event_t *ev)
{
    int ret = 1;
    e_mmgr_errors_t err = E_ERR_FAILED;
    test_data_t *data = NULL;
    mmgr_cli_fw_update_result_t *fw = NULL;

    ASSERT(ev != NULL);

    data = (test_data_t *)ev->context;
    if (data == NULL)
        goto out;

    fw = (mmgr_cli_fw_update_result_t *)ev->data;
    if (fw == NULL) {
        LOG_ERROR("empty data");
        goto out;
    }
    LOG_DEBUG("fw id:%d", fw->id);
    err = set_and_notify(ev->id, (test_data_t *)ev->context);
    if (err == E_ERR_SUCCESS)
        ret = 0;

out:
    if (ret == 1)
        events_set(data, E_EVENTS_ERROR_OCCURED);
    return ret;
}

/**
 * generic callback event
 *
 * @param [in] ev current info callback data
 *
 * @return 0 if successful
 * @return 1 otherwise
 */
int generic_mmgr_evt(mmgr_cli_event_t *ev)
{
    int ret = 1;
    e_mmgr_errors_t err;
    test_data_t *test_data = NULL;

    ASSERT(ev != NULL);

    test_data = (test_data_t *)ev->context;
    if (test_data == NULL)
        goto out;

    err = set_and_notify(ev->id, (test_data_t *)ev->context);
    if (err == E_ERR_SUCCESS)
        ret = 0;

out:
    if (ret == 1)
        events_set(test_data, E_EVENTS_ERROR_OCCURED);
    return ret;
}

/**
 * buggy callback function. This function tries to send a message to MMGR.
 * this is forbidden by the API. The purpose of this test is to check that the
 * library rejects this call
 *
 * @param [in] ev current info callback data
 *
 * @return 0 if successful
 * @return 1 otherwise
 */
int bad_callback(mmgr_cli_event_t *ev)
{
    int ret = 1;
    test_data_t *test_data = NULL;
    mmgr_cli_requests_t request;

    MMGR_CLI_INIT_REQUEST(request, E_MMGR_REQUEST_FORCE_MODEM_SHUTDOWN);

    if (ev == NULL) {
        LOG_ERROR("ev is NULL");
        goto out;
    }

    test_data = (test_data_t *)ev->context;
    if (test_data == NULL)
        goto out;

    if (mmgr_cli_send_msg(test_data->lib, &request) != E_ERR_CLI_REJECTED) {
        events_set(test_data, E_EVENTS_ERROR_OCCURED);
    } else {
        ret = 0;
        LOG_DEBUG("request correctly rejected");
    }

out:
    if (ret == 1)
        events_set(test_data, E_EVENTS_ERROR_OCCURED);
    return ret;
}

/**
 * cleanup client library
 *
 * @param [in] test_data test data
 *
 * @return E_ERR_SUCCESS
 */
e_mmgr_errors_t cleanup_client_library(test_data_t *test_data)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;

    ASSERT(test_data != NULL);

    sem_destroy(&test_data->sem);
    close(test_data->fd_pipe[0]);
    close(test_data->fd_pipe[1]);

    if (mmgr_cli_disconnect(test_data->lib) != E_ERR_CLI_SUCCEED) {
        LOG_ERROR("failed to disconnect client");
        ret = E_ERR_FAILED;
        goto out;
    }

    if (mmgr_cli_delete_handle(test_data->lib) != E_ERR_CLI_SUCCEED) {
        LOG_ERROR("failed to free library");
        ret = E_ERR_FAILED;
    } else {
        test_data->lib = NULL;
    }

out:
    return ret;
}

/**
 * Handles the modem status
 *
 * @param [in,out] test_data test data
 * @param [in] id MMGR instance id
 *
 * @return E_ERR_FAILED if fails
 * @return E_ERR_SUCCESS if successsful
 */
e_mmgr_errors_t configure_client_library(test_data_t *test_data, int id)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    e_err_mmgr_cli_t err;

    ASSERT(test_data != NULL);

    err = mmgr_cli_create_handle(&test_data->lib, MODULE_NAME, test_data);
    if (err != E_ERR_CLI_SUCCEED) {
        LOG_ERROR("Get client handle failed");
        ret = E_ERR_FAILED;
        goto out;
    }

    mmgr_cli_set_instance(test_data->lib, id);

    if (mmgr_cli_subscribe_event(test_data->lib, generic_mmgr_evt,
                                 E_MMGR_EVENT_MODEM_DOWN) != E_ERR_CLI_SUCCEED)
        goto out;

    if (mmgr_cli_subscribe_event(test_data->lib, generic_mmgr_evt,
                                 E_MMGR_EVENT_MODEM_UP) != E_ERR_CLI_SUCCEED)
        goto out;

    if (mmgr_cli_subscribe_event(test_data->lib, generic_mmgr_evt,
                                 E_MMGR_EVENT_MODEM_OUT_OF_SERVICE) !=
        E_ERR_CLI_SUCCEED)
        goto out;

    if (mmgr_cli_subscribe_event(test_data->lib, generic_mmgr_evt,
                                 E_MMGR_NOTIFY_CORE_DUMP) != E_ERR_CLI_SUCCEED)
        goto out;

    if (mmgr_cli_subscribe_event(test_data->lib, generic_mmgr_evt,
                                 E_MMGR_NOTIFY_MODEM_COLD_RESET) !=
        E_ERR_CLI_SUCCEED)
        goto out;

    if (mmgr_cli_subscribe_event(test_data->lib, generic_mmgr_evt,
                                 E_MMGR_NOTIFY_MODEM_SHUTDOWN) !=
        E_ERR_CLI_SUCCEED)
        goto out;

    if (mmgr_cli_subscribe_event(test_data->lib, generic_mmgr_evt,
                                 E_MMGR_NOTIFY_PLATFORM_REBOOT) !=
        E_ERR_CLI_SUCCEED)
        goto out;

    if (mmgr_cli_subscribe_event(test_data->lib, event_core_dump,
                                 E_MMGR_NOTIFY_CORE_DUMP_COMPLETE) !=
        E_ERR_CLI_SUCCEED)
        goto out;

    if (mmgr_cli_subscribe_event(test_data->lib, event_ap_reset,
                                 E_MMGR_NOTIFY_AP_RESET) != E_ERR_CLI_SUCCEED)
        goto out;

    if (mmgr_cli_subscribe_event(test_data->lib, generic_mmgr_evt,
                                 E_MMGR_NOTIFY_SELF_RESET) != E_ERR_CLI_SUCCEED)
        goto out;

    if (mmgr_cli_subscribe_event(test_data->lib, event_tft,
                                 E_MMGR_NOTIFY_TFT_EVENT) != E_ERR_CLI_SUCCEED)
        goto out;

    if (mmgr_cli_subscribe_event(test_data->lib, event_fw_status,
                                 E_MMGR_RESPONSE_MODEM_FW_RESULT)
        != E_ERR_CLI_SUCCEED)
        goto out;

    if (mmgr_cli_connect(test_data->lib) == E_ERR_CLI_SUCCEED) {
        LOG_DEBUG("connection to MMGR (instance: %d) succeed", id);
        ret = E_ERR_SUCCESS;
        /* give some time to connect correctly */
        sleep(1);
    }

out:
    if (ret != E_ERR_SUCCESS)
        LOG_ERROR("failed to connect to MMGR (instance: %d)", id);
    return ret;
}

/**
 * perform a modem reset request via a socket request
 *
 * @param [in] data_test test data
 * @param [in] id request to send
 * @param [in] data_len length of the data to pass to MMGR in the request
 * @param [in] data pointer to the data to pass to MMGR in the request
 * @param [in] notification expected notification after AT command
 * @param [in] final_state final state expected
 *
 * @return E_ERR_FAILED test fails
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t reset_by_client_request(test_data_t *data_test,
                                        e_mmgr_requests_t id,
                                        size_t data_len,
                                        void *data,
                                        e_mmgr_events_t notification,
                                        e_mmgr_events_t final_state)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    mmgr_cli_requests_t request;
    static int iteration = 0;

    MMGR_CLI_INIT_REQUEST(request, id);

    ASSERT(data_test != NULL);

    /* Fill request with extra data information */
    request.len = data_len;
    request.data = data;

    /* Wait modem up only this is the first time running this test*/
    if (iteration == 0) {
        ret = wait_for_state(data_test, E_MMGR_EVENT_MODEM_UP, MMGR_DELAY);
        if (ret != E_ERR_SUCCESS) {
            LOG_DEBUG("modem is down");
            goto out;
        }
    }

    if (mmgr_cli_send_msg(data_test->lib, &request) != E_ERR_CLI_SUCCEED) {
        ret = E_ERR_FAILED;
        goto out;
    }

    if (notification != E_MMGR_NUM_EVENTS) {
        ret = wait_for_state(data_test, notification,
                             data_test->cfg.timeout_mdm_dwn);
        if (ret != E_ERR_SUCCESS)
            goto out;
    }

    /* Wait modem up during X seconds. Timeout provided by TCS */
    ret = wait_for_state(data_test, final_state,
                         data_test->cfg.timeout_mdm_up);
    if (ret == E_ERR_SUCCESS)
        ret = check_wakelock(false);

    iteration++;

out:
    return ret;
}

/**
 * Ask for a modem core dump
 *
 * @param [in] ctx test data
 *
 * @return E_ERR_FAILED test fails
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t at_core_dump(test_data_t *ctx)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;

    ASSERT(ctx != NULL);

    ret = wait_for_state(ctx, E_MMGR_EVENT_MODEM_UP, MMGR_DELAY);
    if (ret != E_ERR_SUCCESS) {
        LOG_DEBUG("modem is down");
        goto out;
    }

    int err = send_at_cmd(ctx->cfg.shtdwn_dlc, AT_CORE_DUMP,
                          strlen(AT_CORE_DUMP));
    if ((err != E_ERR_TTY_POLLHUP) && (err != E_ERR_SUCCESS)) {
        ret = E_ERR_FAILED;
        LOG_ERROR("send of AT commands fails ret=%d" BZ_MSG, ret);
    }

    ret = wait_for_state(ctx, E_MMGR_EVENT_MODEM_DOWN, MMGR_DELAY);
    if (ret != E_ERR_SUCCESS)
        goto out;

    ret = wait_for_state(ctx, E_MMGR_NOTIFY_CORE_DUMP,
                         ctx->cfg.timeout_cd_detection);
    if (ret != E_ERR_SUCCESS)
        goto out;

    if (ctx->monkey.nb_threads)
        start_monkey(&ctx->monkey);

    ret = wait_for_state(ctx, E_MMGR_NOTIFY_CORE_DUMP_COMPLETE,
                         ctx->cfg.timeout_cd_complete);
    if (ret != E_ERR_SUCCESS)
        goto out;

    /* Wait modem up during ctx->cfg.timeout_mdm_up seconds to end the
     * ctx */
    ret = wait_for_state(ctx, E_MMGR_EVENT_MODEM_UP,
                         ctx->cfg.timeout_mdm_up);

    if (ctx->monkey.nb_threads)
        stop_monkey(&ctx->monkey);

    if (ret == E_ERR_SUCCESS)
        ret = check_wakelock(false);
out:
    return ret;
}

/**
 * perform a modem self-reset request via an AT command
 *
 * @param [in] test test data
 *
 * @return E_ERR_FAILED test fails
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t at_self_reset(test_data_t *test)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    e_mmgr_errors_t err;

    ASSERT(test != NULL);

    /* Wait modem up */
    ret = wait_for_state(test, E_MMGR_EVENT_MODEM_UP, MMGR_DELAY);
    if (ret != E_ERR_SUCCESS) {
        LOG_DEBUG("modem is down");
        goto out;
    }

    /* Send reset command to modem */
    err = send_at_cmd(test->cfg.shtdwn_dlc, AT_SELF_RESET,
                      strlen(AT_SELF_RESET));
    if ((err != E_ERR_TTY_POLLHUP) && (err != E_ERR_SUCCESS)) {
        ret = E_ERR_FAILED;
        LOG_ERROR("send of AT commands fails ret=%d" BZ_MSG, ret);
    }

    ret = wait_for_state(test, E_MMGR_EVENT_MODEM_DOWN,
                         test->cfg.timeout_mdm_dwn);
    if (ret != E_ERR_SUCCESS)
        goto out;

    ret = wait_for_state(test, E_MMGR_NOTIFY_SELF_RESET,
                         test->cfg.timeout_mdm_up);
    if (ret != E_ERR_SUCCESS)
        goto out;

    /* Wait modem up during test->cfg.timeout_mdm_up seconds to end the
     * test */
    ret = wait_for_state(test, E_MMGR_EVENT_MODEM_UP,
                         test->cfg.timeout_mdm_up);

    if (ret == E_ERR_SUCCESS)
        ret = check_wakelock(false);
out:
    return ret;
}

static bool is_fake_events_allowed(test_data_t *test)
{
    ASSERT(test != NULL);

    return strcmp(test->cfg.build_type, FAKE_EVENTS_BUILD_TYPE) == 0;
}

e_mmgr_errors_t request_fake_ev(test_data_t *test, e_mmgr_requests_t id,
                                e_mmgr_events_t answer, bool check_result)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    e_err_mmgr_cli_t err = E_ERR_CLI_FAILED;
    mmgr_cli_requests_t request;

    MMGR_CLI_INIT_REQUEST(request, id);

    ASSERT(test != NULL);

    err = mmgr_cli_send_msg(test->lib, &request);

    if (!is_fake_events_allowed(test)) {
        if (err == E_ERR_CLI_SUCCEED) {
            LOG_ERROR("fake request APPROVED in production");
        } else {
            LOG_DEBUG("fake request REJECTED in production");
            ret = E_ERR_SUCCESS;
        }
    } else if (err == E_ERR_CLI_SUCCEED) {
        ret = wait_for_state(test, answer, MMGR_DELAY);

        if (check_result && (events_get(test) != E_EVENTS_SUCCEED))
            ret = E_ERR_FAILED;

        if (ret == E_ERR_SUCCESS)
            ret = check_wakelock(false);
    }

    return ret;
}
