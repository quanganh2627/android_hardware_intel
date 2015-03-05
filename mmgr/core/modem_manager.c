/* Modem Manager - main source file
**
** Copyright (C) Intel 2010
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
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "errors.h"
#include "logs.h"
#include "file.h"
#include "client_events.h"
#include "events_manager.h"
#include "modem_events.h"
#include "modem_info.h"
#include "property.h"
#include "timer_events.h"

#include "tcs.h"

#define USAGE \
    "Start "MODULE_NAME " Daemon.\n" \
    "Usage: "MODULE_NAME " [OPTION]...\n" \
    "-h\t\t: Show help options\n" \

#define TEL_STACK_PROPERTY "persist.service.telephony.off"

/* global values used to cleanup */
mmgr_data_t *g_mmgr = NULL;

/**
 * Clean MMGR before exit
 */
static void cleanup(void)
{
    events_dispose(g_mmgr);
    recov_dispose(g_mmgr->reset);
    timer_dispose(g_mmgr->timer);
    secure_stop(g_mmgr->secure);
    secure_dispose(g_mmgr->secure);
    mcdr_dispose(g_mmgr->mcdr);
    modem_info_dispose(&g_mmgr->info);
    client_events_dispose(g_mmgr);
    bus_ev_dispose(g_mmgr->events.bus_events);
    pm_dispose(g_mmgr->info.pm);
    ctrl_dispose(g_mmgr->info.ctrl);
    mdm_flash_dispose(g_mmgr->mdm_flash);
    LOG_VERBOSE("Exiting");
}

/**
 * Handle catched signals
 *
 * @param [in] sig signal handler id
 */
static void sig_handler(int sig)
{
    switch (sig) {
    case SIGUSR1:
        pthread_exit(NULL);
        break;
    case SIGHUP:
    case SIGTERM:
        /* nothing to do as cleanup will be called by exit */
        break;
    }
    exit(0);
}

/**
 * Set the handler needed to exit a thread
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED if failed
 */
static e_mmgr_errors_t set_signal_handler(void)
{
    struct sigaction sigact;
    int err = E_ERR_FAILED;

    memset(&sigact, 0, sizeof(struct sigaction));
    /* Signal handler */
    if (sigemptyset(&sigact.sa_mask) == -1)
        goto end_set_signal_handler;
    sigact.sa_flags = 0;
    sigact.sa_handler = sig_handler;

    if (sigaction(SIGUSR1, &sigact, NULL) == -1)
        goto end_set_signal_handler;
    if (sigaction(SIGHUP, &sigact, NULL) == -1)
        goto end_set_signal_handler;
    if (sigaction(SIGTERM, &sigact, NULL) == -1)
        goto end_set_signal_handler;

    /* configuration successful */
    err = E_ERR_SUCCESS;

end_set_signal_handler:
    return err;
}

/**
 * This function initialize all MMGR modules.
 * It reads the current platform configuration via TCS
 *
 * @param [in, out] mmgr
 *
 * @return void
 */
static void mmgr_init(mmgr_data_t *mmgr)
{
    tcs_handle_t *h = tcs_init();

    ASSERT(h != NULL);

    ASSERT(mmgr != NULL);

    tcs_cfg_t *cfg = tcs_get_config(h);
    mmgr_info_t *mmgr_cfg = tcs_get_mmgr_config(h);
    ASSERT(cfg != NULL);
    ASSERT(mmgr_cfg != NULL);

    tcs_print(h);

    ASSERT((mmgr->reset = recov_init(&mmgr_cfg->recov)) != NULL);

    ASSERT((mmgr->secure = secure_init(cfg->mdm_info.secured,
                                       &cfg->channels.secured)) != NULL);

    ASSERT((mmgr->mcdr = mcdr_init(&mmgr_cfg->mcdr)) != NULL);

    ASSERT(E_ERR_SUCCESS == modem_info_init(&cfg->mdm_info, &mmgr_cfg->com,
                                            &cfg->tlv,
                                            &mmgr_cfg->mdm_link,
                                            &cfg->channels,
                                            &mmgr_cfg->flash, &mmgr_cfg->mcd,
                                            &mmgr->info));

    ASSERT((mmgr->info.pm = pm_init(cfg->mdm_info.ipc_mdm,
                                    &mmgr_cfg->mdm_link.power,
                                    cfg->mdm_info.ipc_cd,
                                    &mmgr_cfg->mcdr.power)) != NULL);

    ASSERT((mmgr->info.ctrl = ctrl_init(cfg->mdm_info.ipc_mdm,
                                        &mmgr_cfg->mdm_link.ctrl,
                                        cfg->mdm_info.ipc_cd,
                                        &mmgr_cfg->mcdr.ctrl)) != NULL);

    ASSERT(E_ERR_SUCCESS == modem_events_init(mmgr));
    ASSERT(E_ERR_SUCCESS == client_events_init(mmgr_cfg->cli.max, mmgr));

    ASSERT((mmgr->timer = timer_init(&mmgr_cfg->recov, &mmgr_cfg->timings,
                                     &mmgr_cfg->mcdr, mmgr->clients)) != NULL);

    ASSERT((mmgr->events.bus_events =
                bus_ev_init(&mmgr_cfg->mdm_link.flash,
                            &mmgr_cfg->mdm_link.baseband,
                            &mmgr_cfg->mcdr.link)) != NULL);

    ASSERT(E_ERR_SUCCESS == events_init(mmgr_cfg->cli.max, mmgr));

    ASSERT((mmgr->mdm_flash = mdm_flash_init(&mmgr->info, mmgr->secure,
                                             mmgr->events.bus_events)));

    tcs_dispose(h);
}

static void disable_telephony(mmgr_data_t *mmgr)
{
    int disable_telephony = 1;

    property_get_int(TEL_STACK_PROPERTY, &disable_telephony);

    if (disable_telephony == 1) {
        LOG_DEBUG("telephony stack is disabled");
        /* Set MMGR state to MDM_RESET to call the recovery module and force
         * modem recovery to OOS. By doing so, MMGR will turn off the modem and
         * declare the modem OOS. Clients will not be able to turn on the modem
         */
        recov_force(mmgr->reset, E_FORCE_OOS);
        set_mmgr_state(mmgr, E_MMGR_MDM_RESET);
    } else {
        set_mmgr_state(mmgr, E_MMGR_MDM_OFF);
    }
}

/**
 * Modem Manager main function
 *
 * @param [in] argc number of arguments
 * @param [in] argv list of arguments
 *
 * @return EXIT_FAILURE if failed
 * @return EXIT_SUCCESS if successful
 */
int main(int argc, char *argv[])
{
    int err = 0;
    e_mmgr_errors_t ret = EXIT_SUCCESS;
    mmgr_data_t mmgr;

    /* Initialize the mmgr structure */
    memset(&mmgr, 0, sizeof(mmgr_data_t));
    g_mmgr = &mmgr;

    while (-1 != (err = getopt(argc, argv, "hv"))) {
        switch (err) {
        case 'h':
            puts(USAGE);
            goto out;
            break;

        case 'v':
            fprintf(stdout, "%s (last commit: \"%s\")\n", argv[0],
                    GIT_COMMIT_ID);
            goto out;
            break;

        default:
            puts(USAGE);
            goto out;
        }
    }
    LOG_DEBUG("Boot. last commit: \"%s\"", GIT_COMMIT_ID);

#ifndef GOCV_MMGR
    /* set default umask to have 664 as default value to new files */
    umask(MMGR_UMASK);
#endif

    if (set_signal_handler() == E_ERR_FAILED) {
        LOG_ERROR("Error during sigaction initialization. Exit");
        ret = EXIT_FAILURE;
        goto out;
    }

    if (atexit(cleanup) != 0) {
        LOG_ERROR("Exit configuration failed. Exit");
        ret = EXIT_FAILURE;
        goto out;
    }

    mmgr_init(&mmgr);
    disable_telephony(&mmgr);

    if (E_ERR_SUCCESS != events_start(&mmgr)) {
        LOG_ERROR("failed to start event module");
        ret = EXIT_FAILURE;
    } else {
        events_manager(&mmgr);
    }

out:
    /* @TODO: REMOVE EXIT. bogus? If returns is used, atexit function callback
     * is called but mmgr is deallocated... */
    exit(ret);
    return ret;
}
