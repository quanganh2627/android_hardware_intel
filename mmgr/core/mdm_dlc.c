/* Modem Manager - modem DLC source file
**
** Copyright (C) Intel 2014
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
#include "mdm_dlc.h"
#include "errors.h"
#include "logs.h"

typedef struct mdm_dlc {
    char sanity_check[PATH_MAX];
    char shtdwn[PATH_MAX];
    char custom[PATH_MAX];
    char cd_logs[PATH_MAX];
    mux_t mux;
} mdm_dlc_t;

/**
 * Gets shutdown DLC
 *
 * @param [in] hdle
 *
 * @return DLC. must not be freed by caller
 */
const char *mdm_dlc_get_shutdown(const mdm_dlc_hdlt_t *hdle)
{
    mdm_dlc_t *mdm_dlc = (mdm_dlc_t *)hdle;

    ASSERT(mdm_dlc != NULL);

    return mdm_dlc->shtdwn;
}

/**
 * Gets streamline injection DLC
 *
 * @param [in] hdle
 *
 * @return DLC. must not be freed by caller
 */
const char *mdm_dlc_get_streamline(const mdm_dlc_hdlt_t *hdle)
{
    mdm_dlc_t *mdm_dlc = (mdm_dlc_t *)hdle;

    ASSERT(mdm_dlc != NULL);

    return mdm_dlc->custom;
}

/**
 * Gets sanity DLC
 *
 * @param [in] hdle
 *
 * @return DLC. must not be freed by caller
 */
const char *mdm_dlc_get_sanity(const mdm_dlc_hdlt_t *hdle)
{
    mdm_dlc_t *mdm_dlc = (mdm_dlc_t *)hdle;

    ASSERT(mdm_dlc != NULL);

    return mdm_dlc->sanity_check;
}

/**
 * Gets core dump logs DLC
 *
 * @param [în] hdle
 *
 * @return DLC. must not be freed by caller
 */
const char *mdm_dlc_get_cd_logs(const mdm_dlc_hdlt_t *hdle)
{
    mdm_dlc_t *mdm_dlc = (mdm_dlc_t *)hdle;

    ASSERT(mdm_dlc != NULL);

    return mdm_dlc->cd_logs;
}

/**
 * Gets mux configuration
 *
 * @param [in] hdle
 *
 * @return mux configuration. Must not be freed by caller
 */
const mux_t *mdm_dlc_get_mux_cfg(const mdm_dlc_hdlt_t *hdle)
{
    mdm_dlc_t *mdm_dlc = (mdm_dlc_t *)hdle;

    ASSERT(mdm_dlc != NULL);

    return &mdm_dlc->mux;
}

/**
 * Initializes DLC module
 *
 * @param [in] com
 * @param [in] ch channel
 *
 * @return valid pointer. Must be freed by calling mdm_dlc_dipose.
 */
mdm_dlc_hdlt_t *mdm_dlc_init(mmgr_com_t *com, channels_mmgr_t *ch)
{
    mdm_dlc_t *mdm_dlc = calloc(1, sizeof(mdm_dlc_t));

    ASSERT(mdm_dlc != NULL);
    ASSERT(com != NULL);

    mdm_dlc->mux = com->mux;

    /* @TODO: if not DLC, this code should be updated */
    strncpy(mdm_dlc->sanity_check, ch->sanity_check.device,
            sizeof(mdm_dlc->sanity_check) - 1);
    strncpy(mdm_dlc->shtdwn, ch->shutdown.device,
            sizeof(mdm_dlc->shtdwn) - 1);
    strncpy(mdm_dlc->custom, ch->mdm_custo.device,
            sizeof(mdm_dlc->custom) - 1);
    strncpy(mdm_dlc->cd_logs, ch->cd_logs.device,
            sizeof(mdm_dlc->cd_logs) - 1);

    if (mdm_dlc->sanity_check[0] == '\0' ||
        mdm_dlc->shtdwn[0] == '\0' ||
        mdm_dlc->cd_logs[0] == '\0' ||
        mdm_dlc->custom[0] == '\0') {
        LOG_ERROR("empty DLC");
        mdm_dlc_dispose((mdm_dlc_hdlt_t *)mdm_dlc);
        mdm_dlc = NULL;
    }

    return (mdm_dlc_hdlt_t *)mdm_dlc;
}

/**
 * Disposes DLC module
 *
 * @param [in] hdle DLC module handler
 */
void mdm_dlc_dispose(mdm_dlc_hdlt_t *hdle)
{
    free(hdle);
}
