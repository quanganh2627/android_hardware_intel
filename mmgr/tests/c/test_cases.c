/* Modem Manager (MMGR) test application - tests definition source file
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
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "common.h"
#include "errors.h"
#include "file.h"
#include "mmgr.h"
#include "property.h"
#include "test_cases.h"

#define MAX_MOVING_FILE_SLEEP 20
#define RIL_PROPERTY "persist.ril-daemon.disable"
#define MAX_RECOVERY_CAUSE 64

/**
 * Test modem reset without core dump
 *
 * @param [in] test test data
 *
 * @return E_ERR_FAILED test fails
 * @return E_ERR_OUT_OF_SERVICE test fails because MODEM is OUT
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t modem_self_reset(test_data_t *test)
{
    return at_self_reset(test);
}

/**
 * Test modem reset with core dump
 *
 * @param [in] ctx test data
 *
 * @return E_ERR_FAILED test fails
 * @return E_ERR_OUT_OF_SERVICE test fails because MODEM is OUT
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t reset_with_cd(test_data_t *ctx)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;

    ASSERT(ctx != NULL);

    ctx->monkey.nb_threads = 0;
    ret = at_core_dump(ctx);
    if (events_get(ctx) != E_EVENTS_SUCCEED)
        ret = E_ERR_FAILED;

    return ret;
}

e_mmgr_errors_t monkey_cd(test_data_t *ctx)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;

    ASSERT(ctx != NULL);

    if (!ctx->option_string) {
        ctx->monkey.nb_threads = 1;
    } else {
        char *endptr = NULL;
        errno = 0;
        ctx->monkey.nb_threads = strtol(ctx->option_string, &endptr, 10);
        if (errno) {
            LOG_ERROR("failed to get the number of threads");
            goto out;
        }
    }

    ret = at_core_dump(ctx);
    if (events_get(ctx) != E_EVENTS_SUCCEED)
        ret = E_ERR_FAILED;

out:
    return ret;
}


/**
 * Test modem reset with E_MMGR_REQUEST_MODEM_RECOVERY request
 *
 * @param [in] test test data
 *
 * @return E_ERR_FAILED test fails
 * @return E_ERR_OUT_OF_SERVICE test fails because MODEM is OUT
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t modem_recovery(test_data_t *test)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    int num_extra_params = 0;
    mmgr_cli_recovery_cause_t *causes = NULL;

    ASSERT(test != NULL);

    if (test->option_string != NULL)
        num_extra_params = atoi(test->option_string);
    else
        num_extra_params = -1;

    if (num_extra_params > 0) {
        int i;

        causes =
            malloc(num_extra_params * sizeof(mmgr_cli_recovery_cause_t));
        ASSERT(causes != NULL);

        for (i = 0; i < num_extra_params; i++) {
            causes[i].cause = malloc(MAX_RECOVERY_CAUSE);
            ASSERT(causes[i].cause != NULL);
            snprintf(causes[i].cause, MAX_RECOVERY_CAUSE - 1,
                     "Recovery cause number %d !", i);
            causes[i].cause[MAX_RECOVERY_CAUSE - 1] = '\0';
            causes[i].len = strlen(causes[i].cause);
        }
    } else if (num_extra_params == -1) {
        num_extra_params = 1;
        causes = malloc(sizeof(mmgr_cli_recovery_cause_t));
        ASSERT(causes != NULL);
        causes[0].cause = strdup("Requested by mmgr-test application");
        ASSERT(causes[0].cause != NULL);
        causes[0].len = strlen(causes[0].cause);
    } else {
        num_extra_params = 0;
    }

    ret = reset_by_client_request_with_data(test,
                                            E_MMGR_REQUEST_MODEM_RECOVERY,
                                            num_extra_params *
                                            sizeof(mmgr_cli_recovery_cause_t),
                                            causes,
                                            E_MMGR_NUM_EVENTS,
                                            E_MMGR_EVENT_MODEM_UP);

    if (causes != NULL) {
        int i;

        for (i = 0; i < num_extra_params; i++)
            free(causes[i].cause);
        free(causes);
    }

    if (events_get(test) != E_EVENTS_SUCCEED)
        ret = E_ERR_FAILED;

    return ret;
}

/**
 * Test modem reset with E_MMGR_REQUEST_MODEM_RESTART request
 *
 * @param [in] test test data
 *
 * @return E_ERR_FAILED test fails
 * @return E_ERR_OUT_OF_SERVICE test fails because MODEM is OUT
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t modem_restart(test_data_t *test)
{
    return reset_by_client_request(test, E_MMGR_REQUEST_MODEM_RESTART,
                                   E_MMGR_NOTIFY_MODEM_COLD_RESET,
                                   E_MMGR_EVENT_MODEM_UP);
}

/**
 * Test a complete reset escalation recovery
 *
 * @param [in] test test data
 *
 * @return E_ERR_FAILED test fails
 * @return E_ERR_OUT_OF_SERVICE test fails because MODEM is OUT
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t full_recovery(test_data_t *test)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    int i;
    int reboot;
    mmgr_cli_requests_t request;

    MMGR_CLI_INIT_REQUEST(request, E_MMGR_REQUEST_MODEM_RECOVERY);

    ASSERT(test != NULL);

    puts("\n*************************************************************\n"
         "To perform this test, you should have just boot your phone\n"
         "and performed no test. If a reset escalation has already been\n"
         "performed, the test will be declared FAILED\n"
         "*************************************************************\n\n");

    if (test->cfg.cold_reset > 0) {
        for (i = 1; i <= test->cfg.cold_reset; i++) {
            printf("\nCheck #%d COLD reset\n", i);
            pthread_mutex_lock(&test->mutex);
            test->events &= ~E_EVENTS_SUCCEED;
            pthread_mutex_unlock(&test->mutex);
            ret = reset_by_client_request(test,
                                          E_MMGR_REQUEST_MODEM_RECOVERY,
                                          E_MMGR_NOTIFY_MODEM_COLD_RESET,
                                          E_MMGR_EVENT_MODEM_UP);
            if ((ret != E_ERR_SUCCESS)
                || (events_get(test) != E_EVENTS_SUCCEED)) {
                ret = E_ERR_FAILED;
                goto out;
            }
        }
    }

    printf("\nCheck Reboot mechanism\n");
    property_get_int(PLATFORM_REBOOT_KEY, &reboot);
    if (mmgr_cli_send_msg(test->lib, &request) != E_ERR_CLI_SUCCEED) {
        ret = E_ERR_FAILED;
        goto out;
    }

    if (reboot >= test->cfg.reboot)
        ret = wait_for_state(test, E_MMGR_EVENT_MODEM_OUT_OF_SERVICE,
                             test->cfg.timeout_mdm_up);
    else
        ret = wait_for_state(test, E_MMGR_NOTIFY_PLATFORM_REBOOT,
                             test->cfg.timeout_mdm_up);

    if (ret == E_ERR_SUCCESS)
        ret = check_wakelock(false);

out:
    return ret;
}

/**
 * Resource acquire test
 *
 * @param [in] test test data
 *
 * @return E_ERR_FAILED test fails
 * @return E_ERR_OUT_OF_SERVICE test fails because MODEM is OUT
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t resource_acquire(test_data_t *test)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    mmgr_cli_requests_t request;

    MMGR_CLI_INIT_REQUEST(request, E_MMGR_RESOURCE_ACQUIRE);

    ASSERT(test != NULL);

    if (mmgr_cli_send_msg(test->lib, &request) == E_ERR_CLI_SUCCEED)
        ret = E_ERR_SUCCESS;

    return ret;
}

/**
 * Start the modem and never return. This is used to turn on the modem
 * and keep it alive when no client held the resource
 *
 * @param [in] test test data
 *
 * @return E_ERR_FAILED test fails
 * @return E_ERR_OUT_OF_SERVICE test fails because MODEM is OUT
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t start_modem(test_data_t *test)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    mmgr_cli_requests_t request;

    MMGR_CLI_INIT_REQUEST(request, E_MMGR_RESOURCE_ACQUIRE);

    ASSERT(test != NULL);

    if (mmgr_cli_send_msg(test->lib, &request) == E_ERR_CLI_SUCCEED)
        ret = E_ERR_SUCCESS;

    pause();

    return ret;
}

/**
 * Resource release test
 *
 * @param [in] test test data
 *
 * @return E_ERR_FAILED test fails
 * @return E_ERR_OUT_OF_SERVICE test fails because MODEM is OUT
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t resource_release(test_data_t *test)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    mmgr_cli_requests_t request;

    MMGR_CLI_INIT_REQUEST(request, E_MMGR_RESOURCE_RELEASE);

    ASSERT(test != NULL);

    if (mmgr_cli_send_msg(test->lib, &request) == E_ERR_CLI_SUCCEED)
        ret = E_ERR_SUCCESS;

    return ret;
}

/**
 * Test reset counter mechanism. Reboot counter shall be set to 0
 * and a cold modem reset shall be performed
 *
 * @param [in] test test data
 *
 * @return E_ERR_FAILED test fails
 * @return E_ERR_OUT_OF_SERVICE test fails because MODEM is OUT
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t reset_counter(test_data_t *test)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    int counter;

    ASSERT(test != NULL);

    property_set_int(PLATFORM_REBOOT_KEY, 123);
    LOG_DEBUG("waiting during %ds. Please, do not use your phone",
              test->cfg.reset_escalation_delay + 1);
    sleep(test->cfg.reset_escalation_delay + 1);

    ret = reset_by_client_request(test, E_MMGR_REQUEST_MODEM_RECOVERY,
                                  E_MMGR_NOTIFY_MODEM_COLD_RESET,
                                  E_MMGR_EVENT_MODEM_UP);

    if (ret == E_ERR_SUCCESS) {
        property_get_int(PLATFORM_REBOOT_KEY, &counter);
        if (counter != 0) {
            LOG_DEBUG("reset escalation not reseted");
            ret = E_ERR_FAILED;
        }
    }

    return ret;
}

/**
 * Turn on the modem
 *
 * @param [in] test test data
 *
 * @return E_ERR_FAILED test fails
 * @return E_ERR_OUT_OF_SERVICE test fails because MODEM is OUT
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t turn_on_modem(test_data_t *test)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    mmgr_cli_requests_t request;

    MMGR_CLI_INIT_REQUEST(request, E_MMGR_RESOURCE_ACQUIRE);

    ASSERT(test != NULL);

    ret = wait_for_state(test, E_MMGR_EVENT_MODEM_DOWN, MMGR_DELAY);
    if (ret != E_ERR_SUCCESS) {
        LOG_DEBUG("modem is up");
    } else {
        LOG_DEBUG("starting the RIL");
        ret = property_set_int(RIL_PROPERTY, 0);

        if (mmgr_cli_send_msg(test->lib, &request) != E_ERR_CLI_SUCCEED)
            ret = E_ERR_FAILED;
        else
            ret = wait_for_state(test, E_MMGR_EVENT_MODEM_UP,
                                 test->cfg.timeout_mdm_up);

        if (ret == E_ERR_SUCCESS)
            ret = check_wakelock(false);
    }

    return ret;
}

/**
 * This test checks the resource des/allocation mechanism
 *
 * @param [in] test test data
 *
 * @return E_ERR_FAILED if test fails
 * @return E_ERR_SUCCESS if test succes
 */
e_mmgr_errors_t resource_check(test_data_t *test)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    mmgr_cli_requests_t request;

    MMGR_CLI_INIT_REQUEST(request, E_MMGR_RESOURCE_ACQUIRE);

    ASSERT(test != NULL);

    puts("\n*************************************************************\n"
         "This test is only possible if no clients have acquire the \n"
         "resource...\n"
         "*************************************************************\n");

    ret = wait_for_state(test, E_MMGR_EVENT_MODEM_UP,
                         test->cfg.timeout_mdm_up);
    if (ret != E_ERR_SUCCESS)
        goto out;

    if (mmgr_cli_send_msg(test->lib, &request) != E_ERR_CLI_SUCCEED) {
        ret = E_ERR_FAILED;
        goto out;
    }

    sleep(1);

    request.id = E_MMGR_RESOURCE_RELEASE;
    if (mmgr_cli_send_msg(test->lib, &request) != E_ERR_CLI_SUCCEED) {
        ret = E_ERR_FAILED;
        goto out;
    }

    ret = wait_for_state(test, E_MMGR_NOTIFY_MODEM_SHUTDOWN, 5);
    if (ret != E_ERR_SUCCESS)
        goto out;

    ret = wait_for_state(test, E_MMGR_EVENT_MODEM_DOWN,
                         test->cfg.timeout_mdm_dwn);

    request.id = E_MMGR_RESOURCE_ACQUIRE;
    if (mmgr_cli_send_msg(test->lib, &request) != E_ERR_CLI_SUCCEED) {
        ret = E_ERR_FAILED;
        goto out;
    }

    ret = wait_for_state(test, E_MMGR_EVENT_MODEM_UP,
                         test->cfg.timeout_mdm_up);

    if (ret == E_ERR_SUCCESS)
        ret = check_wakelock(false);

out:
    return ret;
}

/**
 * lib mmgr client API stress test
 *
 * @param [in] test test data
 *
 * @return E_ERR_FAILED if test fails
 * @return E_ERR_SUCCESS if test succes
 */
e_mmgr_errors_t test_libmmgrcli_api(test_data_t *test)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    int line = -1;
    const char name[] = MODULE_NAME "_2";

    ASSERT(test != NULL);

    mmgr_cli_requests_t request;
    MMGR_CLI_INIT_REQUEST(request, E_MMGR_NUM_REQUESTS);
    mmgr_cli_handle_t *hdle;

    if (mmgr_cli_create_handle(NULL, name, NULL) != E_ERR_CLI_BAD_HANDLE) {
        line = __LINE__;
        goto out;
    }

    if (mmgr_cli_create_handle(&hdle, name, NULL) != E_ERR_CLI_BAD_HANDLE) {
        line = __LINE__;
        goto out;
    }

    hdle = NULL;
    if (mmgr_cli_create_handle(&hdle, NULL, NULL) != E_ERR_CLI_FAILED) {
        line = __LINE__;
        goto out;
    }

    if (mmgr_cli_create_handle(&hdle, name, NULL) != E_ERR_CLI_SUCCEED) {
        line = __LINE__;
        goto out;
    }

    if (mmgr_cli_delete_handle(NULL) != E_ERR_CLI_BAD_HANDLE) {
        line = __LINE__;
        goto out;
    }

    if (mmgr_cli_connect(hdle) != E_ERR_CLI_SUCCEED) {
        line = __LINE__;
        goto out;
    }

    if (mmgr_cli_delete_handle(hdle) != E_ERR_CLI_BAD_CNX_STATE) {
        line = __LINE__;
        goto out;
    }

    if (mmgr_cli_disconnect(hdle) != E_ERR_CLI_SUCCEED) {
        line = __LINE__;
        goto out;
    }

    if (mmgr_cli_delete_handle(hdle) != E_ERR_CLI_SUCCEED) {
        line = __LINE__;
        goto out;
    }

    if (mmgr_cli_subscribe_event(test->lib, generic_mmgr_evt,
                                 E_MMGR_ACK) != E_ERR_CLI_BAD_CNX_STATE) {
        line = __LINE__;
        goto out;
    }

    if (mmgr_cli_disconnect(test->lib) != E_ERR_CLI_SUCCEED) {
        line = __LINE__;
        goto out;
    }

    /* overwrites MODEM_DOWN event to test the bad callback function */
    if (mmgr_cli_unsubscribe_event(test->lib, E_MMGR_EVENT_MODEM_DOWN)
        != E_ERR_CLI_SUCCEED) {
        line = __LINE__;
        goto out;
    }
    if (mmgr_cli_subscribe_event(test->lib, bad_callback,
                                 E_MMGR_EVENT_MODEM_DOWN) !=
        E_ERR_CLI_SUCCEED) {
        line = __LINE__;
        goto out;
    }

    if (mmgr_cli_disconnect(NULL) != E_ERR_CLI_BAD_HANDLE) {
        line = __LINE__;
        goto out;
    }

    if (mmgr_cli_subscribe_event(test->lib, NULL, E_MMGR_ACK)
        != E_ERR_CLI_FAILED) {
        line = __LINE__;
        goto out;
    }

    if (mmgr_cli_subscribe_event(test->lib, NULL, E_MMGR_NACK)
        != E_ERR_CLI_FAILED) {
        line = __LINE__;
        goto out;
    }

    if (mmgr_cli_subscribe_event(test->lib, generic_mmgr_evt,
                                 E_MMGR_NUM_EVENTS) != E_ERR_CLI_FAILED) {
        line = __LINE__;
        goto out;
    }

    if (mmgr_cli_subscribe_event(NULL, generic_mmgr_evt, E_MMGR_ACK)
        != E_ERR_CLI_BAD_HANDLE) {
        line = __LINE__;
        goto out;
    }

    if (mmgr_cli_subscribe_event(test->lib, generic_mmgr_evt,
                                 E_MMGR_NOTIFY_MODEM_COLD_RESET) !=
        E_ERR_CLI_FAILED) {
        line = __LINE__;
        goto out;
    }

    if (mmgr_cli_unsubscribe_event(test->lib,
                                   E_MMGR_NACK) != E_ERR_CLI_FAILED) {
        line = __LINE__;
        goto out;
    }

    if (mmgr_cli_unsubscribe_event(test->lib,
                                   E_MMGR_NACK) != E_ERR_CLI_FAILED) {
        line = __LINE__;
        goto out;
    }

    if (mmgr_cli_unsubscribe_event(test->lib, E_MMGR_NUM_EVENTS)
        != E_ERR_CLI_FAILED) {
        line = __LINE__;
        goto out;
    }

    if (mmgr_cli_unsubscribe_event(NULL, E_MMGR_ACK) != E_ERR_CLI_BAD_HANDLE) {
        line = __LINE__;
        goto out;
    }

    if (mmgr_cli_lock(test->lib) != E_ERR_CLI_BAD_CNX_STATE) {
        line = __LINE__;
        goto out;
    }

    if (mmgr_cli_unlock(test->lib) != E_ERR_CLI_BAD_CNX_STATE) {
        line = __LINE__;
        goto out;
    }

    if (mmgr_cli_connect(test->lib) != E_ERR_CLI_SUCCEED) {
        line = __LINE__;
        goto out;
    }

    /* ack modem state. Otherwise the mmgr-test callback will block */
    pthread_mutex_trylock(&test->new_state_read);
    pthread_mutex_unlock(&test->new_state_read);

    if (mmgr_cli_connect(NULL) != E_ERR_CLI_BAD_HANDLE) {
        line = __LINE__;
        goto out;
    }

    if (mmgr_cli_lock(test->lib) != E_ERR_CLI_SUCCEED) {
        line = __LINE__;
        goto out;
    }

    if (mmgr_cli_lock(test->lib) != E_ERR_CLI_ALREADY_LOCK) {
        line = __LINE__;
        goto out;
    }

    if (mmgr_cli_unlock(test->lib) != E_ERR_CLI_SUCCEED) {
        line = __LINE__;
        goto out;
    }

    if (mmgr_cli_unlock(test->lib) != E_ERR_CLI_ALREADY_UNLOCK) {
        line = __LINE__;
        goto out;
    }

    if (mmgr_cli_send_msg(test->lib, NULL) != E_ERR_CLI_FAILED) {
        line = __LINE__;
        goto out;
    }

    if (mmgr_cli_send_msg(test->lib, &request) != E_ERR_CLI_FAILED) {
        line = __LINE__;
        goto out;
    }

    if (mmgr_cli_send_msg(NULL, &request) != E_ERR_CLI_BAD_HANDLE) {
        line = __LINE__;
        goto out;
    }

    if (mmgr_cli_unlock(NULL) != E_ERR_CLI_BAD_HANDLE) {
        line = __LINE__;
        goto out;
    }

    if (mmgr_cli_lock(NULL) != E_ERR_CLI_BAD_HANDLE) {
        line = __LINE__;
        goto out;
    }

    /* request MODEM_RESTART to check bad callback function */
    modem_state_set(test, E_MMGR_NUM_EVENTS);
    MMGR_CLI_INIT_REQUEST(request, E_MMGR_REQUEST_MODEM_RESTART);
    if (mmgr_cli_send_msg(test->lib, &request) != E_ERR_CLI_SUCCEED) {
        line = __LINE__;
        goto out;
    }

    wait_for_state(test, E_MMGR_EVENT_MODEM_UP, test->cfg.timeout_mdm_up);

    ret = E_ERR_SUCCESS;

out:
    if (ret != E_ERR_SUCCESS)
        LOG_ERROR("test failed at line %d", line);
    return ret;
}

e_mmgr_errors_t fake_modem_down(test_data_t *test)
{
    return request_fake_ev(test, E_MMGR_REQUEST_FAKE_DOWN,
                           E_MMGR_EVENT_MODEM_DOWN, false);
}

e_mmgr_errors_t fake_modem_up(test_data_t *test)
{
    return request_fake_ev(test, E_MMGR_REQUEST_FAKE_UP, E_MMGR_EVENT_MODEM_UP,
                           false);
}

e_mmgr_errors_t fake_modem_shtdwn(test_data_t *test)
{
    return request_fake_ev(test, E_MMGR_REQUEST_FAKE_MODEM_SHUTDOWN,
                           E_MMGR_NOTIFY_MODEM_SHUTDOWN, false);
}

e_mmgr_errors_t fake_modem_hs(test_data_t *test)
{
    return request_fake_ev(test, E_MMGR_REQUEST_FAKE_MODEM_OUT_OF_SERVICE,
                           E_MMGR_EVENT_MODEM_OUT_OF_SERVICE, false);
}

e_mmgr_errors_t fake_cd(test_data_t *test)
{
    return request_fake_ev(test, E_MMGR_REQUEST_FAKE_CORE_DUMP,
                           E_MMGR_NOTIFY_CORE_DUMP, false);
}

e_mmgr_errors_t fake_cd_complete(test_data_t *test)
{
    return request_fake_ev(test, E_MMGR_REQUEST_FAKE_CORE_DUMP_COMPLETE,
                           E_MMGR_NOTIFY_CORE_DUMP_COMPLETE, true);
}

e_mmgr_errors_t fake_tft_event(test_data_t *test)
{
    return request_fake_ev(test, E_MMGR_REQUEST_FAKE_TFT_EVENT,
                           E_MMGR_NOTIFY_TFT_EVENT, true);
}

e_mmgr_errors_t fake_ap_reset(test_data_t *test)
{
    return request_fake_ev(test, E_MMGR_REQUEST_FAKE_AP_RESET,
                           E_MMGR_NOTIFY_AP_RESET, true);
}

e_mmgr_errors_t fake_self_reset(test_data_t *test)
{
    return request_fake_ev(test, E_MMGR_REQUEST_FAKE_SELF_RESET,
                           E_MMGR_NOTIFY_SELF_RESET, false);
}

e_mmgr_errors_t fake_reboot(test_data_t *test)
{
    return request_fake_ev(test, E_MMGR_REQUEST_FAKE_PLATFORM_REBOOT,
                           E_MMGR_NOTIFY_PLATFORM_REBOOT, false);
}
