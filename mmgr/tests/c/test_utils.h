/* Modem Manager (MMGR) test application - utils header file
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

#ifndef __MMGR_TEST_UTILS_FILE__
#define __MMGR_TEST_UTILS_FILE__

#include <pthread.h>
#include <stdbool.h>
#include <semaphore.h>
#include "key.h"
#include "logs.h"
#include "mmgr.h"
#include "mmgr_cli.h"
#include "tcs_mmgr.h"

#define FILENAME_SIZE 256
#define MMGR_DELAY 5        /* in seconds */
#define MDM_CONFIGURATION 5 /* in seconds */
#define WRITE 1
#define READ 0

typedef enum e_events {
    E_EVENTS_NONE = 0x0,
    E_EVENTS_ERROR_OCCURED = 0x1,
    E_EVENTS_SUCCEED = 0x1 << 1,
        E_EVENTS_MODEM_OOS = 0x1 << 2,
} e_events_t;

typedef struct test_cfg {
    int timeout_cd_detection;
    int timeout_cd_complete;
    int timeout_mdm_up;
    int timeout_mdm_dwn;
    int cold_reset;
    int reboot;
    int reset_escalation_delay;
    char shtdwn_dlc[PATH_MAX];
    char *key_reboot;
    char build_type[PROPERTY_VALUE_MAX];
} test_cfg_t;

typedef struct monkey_ctx {
    pthread_mutex_t mtx;
    bool state;
    int nb_threads;
    pthread_t *ids;
} monkey_ctx_t;

typedef struct test_data {
    int fd_pipe[2];
    pthread_mutex_t mutex;
    sem_t sem;
    e_mmgr_events_t waited_state;
    e_mmgr_events_t modem_state;
    mmgr_cli_handle_t *lib;
    e_events_t events;
    test_cfg_t cfg;
    const char *option_string;
    monkey_ctx_t monkey;
} test_data_t;

void modem_state_set(test_data_t *test_data, e_mmgr_events_t state);
e_events_t events_get(test_data_t *test_data);
e_mmgr_errors_t compare_file_content(const char *path, const char *data,
                                     int len);
e_mmgr_errors_t wait_for_state(test_data_t *thread_data, int state,
                               int timeout);

e_mmgr_errors_t cleanup_modemcrash_dir(const char *path);
e_mmgr_errors_t configure_client_library(test_data_t *data, int id);
e_mmgr_errors_t cleanup_client_library(test_data_t *data);
int generic_mmgr_evt(mmgr_cli_event_t *ev);
int bad_callback(mmgr_cli_event_t *ev);

e_mmgr_errors_t reset_by_client_request(test_data_t *events_data,
                                        e_mmgr_requests_t request,
                                        size_t data_len, void *data,
                                        e_mmgr_events_t notification,
                                        e_mmgr_events_t final_state);

e_mmgr_errors_t at_self_reset(test_data_t *events_data);
e_mmgr_errors_t at_core_dump(test_data_t *events_data);
e_mmgr_errors_t request_fake_ev(test_data_t *test, e_mmgr_requests_t id,
                                e_mmgr_events_t answer, bool check_result);
e_mmgr_errors_t check_wakelock(bool state);

#endif                          /* __MMGR_TEST_UTILS_FILE__ */
