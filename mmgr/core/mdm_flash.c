/* Modem Manager - modem flashing source file
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

#define MMGR_FW_OPERATIONS
#include "mdm_flash.h"
#include "errors.h"
#include "logs.h"
#include "modem_specific.h"
#include "pm.h"
#include <pthread.h>

#define WRITE 1
#define READ 0

typedef struct mdm_flash_ctx {
    const modem_info_t *mdm_info;
    const secure_handle_t *secure;
    const bus_ev_hdle_t *bus_ev;
    bool ch_hw_sw;
    e_modem_fw_error_t result;
    int fd_pipe[2];
    pthread_t id;
    pthread_mutex_t mtx;
    int attempts;
} mdm_flash_ctx_t;

static e_modem_fw_error_t get_verdict(mdm_flash_ctx_t *ctx)
{
    ASSERT(ctx != NULL);
    pthread_mutex_lock(&ctx->mtx);
    e_modem_fw_error_t result = ctx->result;
    pthread_mutex_unlock(&ctx->mtx);
    return result;
}

static void set_verdict(mdm_flash_ctx_t *ctx, e_modem_fw_error_t result)
{
    ASSERT(ctx != NULL);

    pthread_mutex_lock(&ctx->mtx);
    ctx->result = result;
    pthread_mutex_unlock(&ctx->mtx);
}

static void mdm_flash(mdm_flash_ctx_t *ctx)
{
    e_modem_fw_error_t verdict = E_MODEM_FW_ERROR_UNSPECIFIED;
    char msg = 0;
    const char *flashing_interface = NULL;

    ASSERT(ctx != NULL);

    LOG_DEBUG("[SLAVE-FLASH] start modem flashing");
    set_verdict(ctx, verdict);

    if (ctx->mdm_info->mdm_link == E_LINK_HSI)
        flashing_interface = "/dev/ttyIFX1";
    else
        flashing_interface = bus_ev_get_flash_interface(ctx->bus_ev);

    toggle_flashing_mode(ctx->mdm_info, true);
    pm_on_mdm_flash(ctx->mdm_info->pm);

    mdm_push_fw(ctx->mdm_info, flashing_interface, ctx->ch_hw_sw,
                ctx->secure, &verdict);

    toggle_flashing_mode(ctx->mdm_info, false);

    set_verdict(ctx, verdict);

    LOG_DEBUG("[SLAVE-FLASH] flashing done. Notify main thread");
    write(ctx->fd_pipe[WRITE], &msg, sizeof(msg));
}

/**
 * Returns modem flashing verdict
 *
 * @param [in] hdle modem flashing handle
 *
 * @return verdict
 */
e_modem_fw_error_t mdm_flash_get_verdict(mdm_flash_handle_t *hdle)
{
    return get_verdict((mdm_flash_ctx_t *)hdle);
}

/**
 * Initialize modem flashing module
 * The function will assert in case of error
 *
 * @param [in] mdm_info
 * @param [in] secure
 * @param [in] bus_ev
 *
 * @return a valid handle or NULL if modem is flashbased
 */
mdm_flash_handle_t *mdm_flash_init(const modem_info_t *mdm_info,
                                   const secure_handle_t *secure,
                                   const bus_ev_hdle_t *bus_ev)
{
    mdm_flash_ctx_t *ctx = NULL;

    if (mdm_info->is_flashless) {
        ctx = calloc(1, sizeof(mdm_flash_ctx_t));

        ASSERT(ctx != NULL);

        ctx->mdm_info = mdm_info;
        ctx->secure = secure;
        ctx->bus_ev = bus_ev;
        pthread_mutex_init(&ctx->mtx, NULL);
        ctx->attempts = 0;

        if (mdm_info->mdm_link == E_LINK_HSI)
            ctx->ch_hw_sw = true;
        else if (mdm_info->mdm_link == E_LINK_USB)
            ctx->ch_hw_sw = false;

        ASSERT(pipe(ctx->fd_pipe) == 0);
    }

    return (mdm_flash_handle_t *)ctx;
}

/**
 * Start fhe flashing thread
 *
 * @param [in] hdle flashing module
 *
 * @return E_ERR_SUCCESS
 * @return E_ERR_FAILED if thread is already running
 */
e_mmgr_errors_t mdm_flash_start(mdm_flash_handle_t *hdle)
{
    mdm_flash_ctx_t *ctx = (mdm_flash_ctx_t *)hdle;
    e_mmgr_errors_t ret = E_ERR_SUCCESS;

    ASSERT(ctx != NULL);

    if (!ctx->id) {
        ctx->attempts++;
        pthread_create(&ctx->id, NULL, (void *)mdm_flash, (void *)ctx);
    } else {
        ret = E_ERR_FAILED;
        LOG_ERROR("thread already running");
    }

    return ret;
}

/**
 * Finalize the flashing operation. It joins the thread
 *
 * @param [in] hdle flashing module
 *
 */
void mdm_flash_finalize(mdm_flash_handle_t *hdle)
{
    mdm_flash_ctx_t *ctx = (mdm_flash_ctx_t *)hdle;

    ASSERT(ctx != NULL);

    if (ctx->id) {
        pthread_join(ctx->id, NULL);
        ctx->id = 0;
        LOG_DEBUG("[MASTER] flashing thread is stopped");
    }
}

/**
 * Returns the fd used by the module to notify events
 *
 * @param [in] hdle flashing module
 *
 * @return a valid fd or CLOSED_FD
 */
int mdm_flash_get_fd(mdm_flash_handle_t *hdle)
{
    mdm_flash_ctx_t *ctx = (mdm_flash_ctx_t *)hdle;
    int fd = CLOSED_FD;

    if (ctx)
        fd = ctx->fd_pipe[READ];

    return fd;
}

/**
 * Cancel flashing operation. This function will be called
 * when the flashing operation reaches timeout.
 * Because it isn't possible to properly stop the flashing
 * thread, MMGR is stopped here in order to be re-launched by the
 * Android framework, because it is a persistent service.
 *
 * @param [in] hdle flashing module
 *
 */
void mdm_flash_cancel(mdm_flash_handle_t *hdle)
{
    ASSERT(hdle != NULL);

    exit(EXIT_FAILURE);
}

/**
 * Free the module memory
 *
 * @param [in] hdle flashing module
 *
 */
void mdm_flash_dispose(mdm_flash_handle_t *hdle)
{
    mdm_flash_ctx_t *ctx = (mdm_flash_ctx_t *)hdle;

    if (ctx) {
        close(ctx->fd_pipe[READ]);
        close(ctx->fd_pipe[WRITE]);

        free(ctx);
    }
}

/**
 * Returns the number of flashing attempts
 *
 * @param [in] hdle flashing module
 *
 * @return the number of flashing attempts
 */
int mdm_flash_get_attempts(mdm_flash_handle_t *hdle)
{
    mdm_flash_ctx_t *ctx = (mdm_flash_ctx_t *)hdle;

    ASSERT(ctx != NULL);

    return ctx->attempts;
}

/**
 * Reset the number of flashing attempts
 *
 * @param [in] hdle flashing module
 *
 */
void mdm_flash_reset_attempts(mdm_flash_handle_t *hdle)
{
    mdm_flash_ctx_t *ctx = (mdm_flash_ctx_t *)hdle;

    ASSERT(ctx != NULL);

    ctx->attempts = 0;
}
