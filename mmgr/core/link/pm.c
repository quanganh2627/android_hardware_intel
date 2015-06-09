/* Modem Manager - link power management source file
**
** Copyright (C) Intel 2013
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

#include "logs.h"
#include "pm.h"
#include "file.h"

#include <string.h>

typedef struct pm_link {
    power_t power;
    e_link_t type;
} pm_link_t;

typedef struct pm_data {
    pm_link_t mdm;
    pm_link_t cd;
} pm_ctx_t;

static inline void fill_link(pm_link_t *link, e_link_t type, const power_t *p)
{
    link->type = type;
    link->power = *p;
}

pm_handle_t pm_init(e_link_t mdm_type, const power_t *mdm_power,
                    e_link_t cd_type, const power_t *cd_power)
{
    pm_ctx_t *ctx = NULL;

    if (mdm_power && cd_power) {
        ctx = calloc(1, sizeof(pm_ctx_t));
        if (ctx) {
            fill_link(&ctx->mdm, mdm_type, mdm_power);
            fill_link(&ctx->cd, cd_type, cd_power);
        } else {
            LOG_ERROR("memory allocation failed");
            goto err;
        }
    }
    if ((mdm_type == E_LINK_USB) &&
        !strncmp(mdm_power->device, "", sizeof(mdm_power->device))) {
        LOG_ERROR("bad device to handle modem power");
        goto err;
    }
    if ((cd_type == E_LINK_UART) &&
        !strncmp(cd_power->device, "", sizeof(cd_power->device))) {
        LOG_ERROR("bad device to handle CD power");
        goto err;
    }

    return (pm_handle_t *)ctx;

err:
    pm_dispose((pm_handle_t *)ctx);
    return NULL;
}

e_mmgr_errors_t pm_dispose(pm_handle_t *h)
{
    pm_ctx_t *ctx = (pm_ctx_t *)h;

    /* do not use ASSERT in dispose function */

    free(ctx);

    return E_ERR_SUCCESS;
}

/**
 * This function sets the IPC power management
 *
 * @param [in] p link power
 * @param [in] state (true: power management is enabled)
 *
 * @return E_ERR_FAILED if it fails
 * @return E_ERR_SUCCESS if successful
 */
static e_mmgr_errors_t pm_set_state(power_t *p, bool state)
{
    ASSERT(p != NULL);

    char *cmd = p->on;
    if (!state)
        cmd = p->off;

    return file_write(p->device, SYSFS_OPEN_MODE, cmd, strlen(cmd));
}

/**
 * Perform the right power management operation when the modem is up
 *
 * @param [in] h power management handle
 *
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t pm_on_mdm_up(pm_handle_t *h)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;
    pm_ctx_t *ctx = (pm_ctx_t *)h;

    ASSERT(ctx != NULL);

    switch (ctx->mdm.type) {
    case E_LINK_HSI:
        /* Nothing to do */
        break;
    case E_LINK_USB:
        ret = pm_set_state(&ctx->mdm.power, true);
        break;
    case E_LINK_UART:
        /* Nothing to do */
        break;
    case E_LINK_SPI:
        /* Nothing to do */
        break;
    default:
        LOG_ERROR("type %d not handled", ctx->mdm.type);
        ret = E_ERR_FAILED;
        break;
    }

    return ret;
}

/**
 * Perform the right power management operation when a core dump is available
 *
 * @param [in] h power management handle
 *
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t pm_on_cd(pm_handle_t *h)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;
    pm_ctx_t *ctx = (pm_ctx_t *)h;

    ASSERT(ctx != NULL);

    switch (ctx->cd.type) {
    case E_LINK_HSI:
        /* Nothing to do */
        break;
    case E_LINK_USB:
        /* Nothing to do */
        break;
    case E_LINK_SPI:
        /* Nothing to do */
        break;
    case E_LINK_UART:
        ret = pm_set_state(&ctx->cd.power, false);
        break;
    default:
        LOG_ERROR("type %d not handled", ctx->cd.type);
        ret = E_ERR_FAILED;
        break;
    }

    return ret;
}

/**
 * Perform the right power management operation when CD operation is completed
 *
 * @param [in] h power management handle
 *
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t pm_on_cd_complete(pm_handle_t *h)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;
    pm_ctx_t *ctx = (pm_ctx_t *)h;

    ASSERT(ctx != NULL);

    switch (ctx->cd.type) {
    case E_LINK_HSI:
        /* Nothing to do */
        break;
    case E_LINK_USB:
        /* Nothing to do */
        break;
    case E_LINK_SPI:
        /* Nothing to do */
        break;
    case E_LINK_UART:
        ret = pm_set_state(&ctx->cd.power, true);
        break;
    default:
        LOG_ERROR("type %d not handled", ctx->cd.type);
        ret = E_ERR_FAILED;
        break;
    }

    return ret;
}
