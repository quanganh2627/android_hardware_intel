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
#include "key.h"
#include "client_events.h"
#include "events_manager.h"
#include "modem_events.h"
#include "modem_info.h"
#include "property.h"
#include "timer_events.h"
#include "client_cnx.h"

#include "tcs.h"

#define USAGE \
    "Start "MODULE_NAME " Daemon.\n" \
    "Usage: "MODULE_NAME " [OPTION]...\n" \
    "-h\t\t: Show help options\n" \
    "-v\t\t: show "MODULE_NAME " version\n" \
    "-i\t\t: specify instance number\n" \


/* global values used to cleanup */
static mmgr_data_t *g_mmgr = NULL;

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
    client_events_dispose(g_mmgr);
    bus_ev_dispose(g_mmgr->events.bus_events);
    mdm_mcd_dispose(g_mmgr->mcd);
    mdm_flash_dispose(g_mmgr->flash);
    mdm_fw_dispose(g_mmgr->fw);
    mdm_dlc_dispose(g_mmgr->mdm_dlc);
    link_dispose(g_mmgr->link);
    key_dispose(g_mmgr->keys);
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
 * Set an Android property used by AMTL to know which configuration file
 * to use
 *
 * @param [in] cfg TCS configuration
 * @param [in] keys
 * @param [in] mdm_id modem id in TCS
 *
 */
static void set_amtl_cfg(tcs_cfg_t *cfg, const key_hdle_t *keys, size_t mdm_id)
{
    char amtl_cfg[PROPERTY_VALUE_MAX] = { "" };

    ASSERT(cfg != NULL);
    ASSERT(keys != NULL);

    const char *amtl_key = key_get_amtl(keys);

    property_get_string(amtl_key, amtl_cfg);
    if (amtl_cfg[0] == '\0') {
        char platform[PROPERTY_VALUE_MAX] = { "" };

        LOG_DEBUG("amtl property not set");
        property_get_string(key_get_platform(keys), platform);
        snprintf(amtl_cfg, sizeof(amtl_cfg), "%s_XMM_%s", platform,
                 cfg->mdm[mdm_id].core.name);
        property_set(amtl_key, amtl_cfg);
    }
}

/**
 * This function initialize all MMGR modules.
 * It reads the current platform configuration via TCS
 *
 * @param [in, out] mmgr
 * @param [in] inst_id MMGR instance id
 *
 * @return void
 */
static void mmgr_init(mmgr_data_t *mmgr, int inst_id)
{
    tcs_handle_t *h = tcs_init();
    int mdm_id = inst_id - 1;

    ASSERT(h != NULL);
    ASSERT(mmgr != NULL);

    tcs_cfg_t *cfg = tcs_get_config(h);
    ASSERT(cfg != NULL);
    ASSERT(cfg->nb >= 1);
    ASSERT((size_t)mdm_id < cfg->nb);
    ASSERT(mdm_id >= 0);
    ASSERT(cfg->mdm != NULL);
    ASSERT(cfg->mdm[mdm_id].tlvs.nb >= 1);
    ASSERT(cfg->mdm[mdm_id].tlvs.tlv != NULL);
    ASSERT(cfg->mdm[mdm_id].chs.nb >= 1);
    ASSERT(cfg->mdm[mdm_id].chs.ch != NULL);

    mmgr_info_t *mmgr_cfg = tcs_get_mmgr_config(h, &cfg->mdm[mdm_id]);
    ASSERT(mmgr_cfg != NULL);

    tcs_print(h);

    mmgr->keys = key_init(inst_id);
    ASSERT(mmgr->keys != NULL);

    if (cfg->nb == 2) {
        LOG_INFO("DSDA platform");
        mmgr->dsda = true;
    } else {
        mmgr->dsda = false;
    }

    /* SSIC power on work around */
    bool ssic_hack = (strstr(mmgr_cfg->mdm_link.ctrl.device, "ssic") != NULL);
    if (ssic_hack)
        LOG_DEBUG("SSIC Power on sequence used");

    mmgr->reset = recov_init(&mmgr_cfg->recov, mmgr->keys);
    ASSERT(mmgr->reset != NULL);

    ASSERT((mmgr->secure =
                secure_init(cfg->mdm[mdm_id].core.secured,
                            &cfg->mdm[mdm_id].chs.ch[0].mmgr.secured)) != NULL);

    ASSERT((mmgr->mcdr = mcdr_init(&mmgr_cfg->mcdr)) != NULL);

    ASSERT(E_ERR_SUCCESS == modem_events_init(mmgr));
    ASSERT(E_ERR_SUCCESS == client_events_init(mmgr_cfg->cli.max, mmgr));

    ASSERT((mmgr->timer = timer_init(&mmgr_cfg->recov, &mmgr_cfg->timings,
                                     &mmgr_cfg->mcdr, mmgr->clients)) != NULL);

    ASSERT((mmgr->events.bus_events =
                bus_ev_init(&mmgr_cfg->mdm_link.flash_ebl,
                            &mmgr_cfg->mdm_link.baseband,
                            &mmgr_cfg->mdm_link.reconfig_usb,
                            &mmgr_cfg->mcdr.link)) != NULL);

    ASSERT((mmgr->link = link_init(&mmgr_cfg->mdm_link, &mmgr_cfg->mcdr,
                                   mmgr->events.bus_events, ssic_hack))
           != NULL);

    ASSERT((mmgr->mdm_dlc = mdm_dlc_init(&mmgr_cfg->com,
                                         &cfg->mdm[mdm_id].chs.ch[0].mmgr))
           != NULL);

    ASSERT(E_ERR_SUCCESS == events_init(mmgr_cfg->cli.max, mmgr));

    ASSERT((mmgr->mcd = mdm_mcd_init(&mmgr_cfg->mcd, &cfg->mdm[mdm_id].core,
                                     mmgr->link, !mmgr->dsda, ssic_hack))
           != NULL);

    ASSERT((mmgr->fw = mdm_fw_init(inst_id, &cfg->mdm[mdm_id], &mmgr_cfg->fw))
           != NULL);

    ASSERT(E_ERR_SUCCESS == mdm_fw_create_folders(mmgr->fw));

    ASSERT((mmgr->flash = mdm_flash_init(&cfg->mdm[mdm_id], mmgr->fw,
                                         mmgr->secure,
                                         mmgr->keys, mmgr->link)) != NULL);

    set_amtl_cfg(cfg, mmgr->keys, mdm_id);

    tcs_dispose(h);
}

static void disable_telephony(mmgr_data_t *mmgr)
{
    int disable_telephony = 1;

    property_get_int(key_get_telephony_state(mmgr->keys), &disable_telephony);

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
    int inst_id = DEFAULT_INST_ID;
    e_mmgr_errors_t ret = EXIT_SUCCESS;
    mmgr_data_t mmgr;

    /* Initialize the mmgr structure */
    memset(&mmgr, 0, sizeof(mmgr_data_t));
    g_mmgr = &mmgr;

    while (-1 != (err = getopt(argc, argv, "hvi:"))) {
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

        case 'i': {
            char *end_ptr = NULL;
            inst_id = strtol(optarg, &end_ptr, 10);
            LOG_DEBUG("instance number: %d", inst_id);
            break;
        }
        default:
            puts(USAGE);
            goto out;
        }
    }
    logs_init(inst_id);
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

    mmgr_init(&mmgr, inst_id);
    disable_telephony(&mmgr);

    if (E_ERR_SUCCESS != events_start(&mmgr, inst_id)) {
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
