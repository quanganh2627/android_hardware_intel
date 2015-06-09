/* Modem Manager - core dump source file
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
#include <dlfcn.h>
#include <pthread.h>
#include "core_dump.h"
#include "errors.h"
#include "logs.h"

#define WRITE 1
#define READ 0

typedef struct mcdr_lib {
    void *hdle;
    void (*read)(mcdr_data_t *);
    void (*cleanup)(void);
    mcdr_status_t (*get_state)(void);
    char *(*get_reason)(void);
} mcdr_lib_t;

typedef struct mcdr_ctx {
    bool enabled;
    bool running;
    int fd_pipe[2];
    pthread_t id;
    mcdr_lib_t mcdr;
    mcdr_data_t data;
} mcdr_ctx_t;

#ifdef GOCV_MMGR
#define MCDR_LIBRARY_NAME "libmcdr-gcov.so"
#else
#define MCDR_LIBRARY_NAME "libmcdr.so"
#endif

#define MCDR_GET_CORE_DUMP "mcdr_get_core_dump"
#define MCDR_CLEANUP "mcdr_cleanup"
#define MCDR_GET_STATE "mcdr_get_state"
#define MCDR_GET_REASON "mcdr_get_reason"

/**
 * initialize core dump. If the libmcdr is available, mcdr will be enabled
 * disabled if not.
 *
 * @param [in] cfg
 *
 * @return a valid mcdr_handle_t pointer
 * @return NULL otherwise
 */
mcdr_handle_t *mcdr_init(const mcdr_info_t *cfg)
{
    mcdr_ctx_t *ctx = calloc(1, sizeof(mcdr_ctx_t));;

    ASSERT(cfg != NULL);
    ASSERT(ctx != NULL);

    if (!cfg->gnl.enable) {
        ctx->enabled = false;
        LOG_VERBOSE("MCDR is disabled");
    } else {
        ctx->mcdr.hdle = dlopen(MCDR_LIBRARY_NAME, RTLD_LAZY);
        if (ctx->mcdr.hdle == NULL) {
            ctx->enabled = false;
            LOG_VERBOSE("failed to load the library");
            dlerror();
        } else {
            ctx->enabled = true;
            ctx->data.mcdr_info = *cfg;
            ctx->running = false;

            ctx->mcdr.read = dlsym(ctx->mcdr.hdle, MCDR_GET_CORE_DUMP);
            ctx->mcdr.cleanup = dlsym(ctx->mcdr.hdle, MCDR_CLEANUP);
            ctx->mcdr.get_state = dlsym(ctx->mcdr.hdle, MCDR_GET_STATE);
            ctx->mcdr.get_reason = dlsym(ctx->mcdr.hdle, MCDR_GET_REASON);

            ASSERT(dlerror() == NULL);
            ASSERT(pipe(ctx->fd_pipe) == 0);
        }
    }

    return (mcdr_handle_t *)ctx;
}

/**
 * @brief mcdr_dispose
 *
 * @param h module handle
 *
 * @return E_ERR_SUCCESS if succeed
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t mcdr_dispose(mcdr_handle_t *h)
{
    mcdr_ctx_t *ctx = (mcdr_ctx_t *)h;
    e_mmgr_errors_t ret = E_ERR_SUCCESS;

    if (ctx) {
        if (ctx->mcdr.hdle != NULL) {
            dlclose(ctx->mcdr.hdle);
            ctx->mcdr.hdle = NULL;
        }
        close(ctx->fd_pipe[READ]);
        close(ctx->fd_pipe[WRITE]);
        free(ctx);
    } else {
        ret = E_ERR_FAILED;
    }

    return ret;
}

/**
 * function calling the mcdr lib. This function will send a signal once the
 * the operation is ended
 *
 * @param [in,out] cd_reader core dump thread struct
 */
static void mcdr_read(mcdr_ctx_t *ctx)
{
    char msg = 0;

    ASSERT(ctx != NULL);

    ctx->mcdr.read(&ctx->data);
    LOG_DEBUG("[SLAVE-MCDR] Core dump retrieved. Notify main thread");
    write(ctx->fd_pipe[WRITE], &msg, sizeof(msg));
}

/**
 * Start core dump reader program.
 *
 * @param [in] h module handle
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t mcdr_start(mcdr_handle_t *h)
{
    mcdr_ctx_t *ctx = (mcdr_ctx_t *)h;
    e_mmgr_errors_t ret = E_ERR_SUCCESS;

    ASSERT(ctx != NULL);

    if (!ctx->running) {
        ctx->running = true;
        pthread_create(&ctx->id, NULL, (void *)mcdr_read, (void *)ctx);
    } else {
        ret = E_ERR_FAILED;
        LOG_ERROR("thread already running");
    }

    return ret;
}

/**
 * Finalize core dump retrieval. It joins the thread
 *
 * @param [in] h module handle
 *
 */
void mcdr_finalize(mcdr_handle_t *h)
{
    mcdr_ctx_t *ctx = (mcdr_ctx_t *)h;

    ASSERT(ctx != NULL);

    if (ctx->running) {
        ctx->running = false;
        pthread_join(ctx->id, NULL);
        LOG_DEBUG("[MASTER] MCDR thread is stopped");
    }
}

/**
 * Sop the core dump retrieval
 *
 * @param [in] h module handle
 *
 */
void mcdr_cancel(mcdr_handle_t *h)
{
    mcdr_ctx_t *ctx = (mcdr_ctx_t *)h;

    ASSERT(ctx != NULL);

    if (ctx->running) {
        if (MCDR_ARCHIVE_IN_PROGRESS == ctx->mcdr.get_state()) {
            LOG_DEBUG("Archiving still on-going");
        } else {
            LOG_ERROR("Core dump retrieval stopped. Aborting");
            ctx->mcdr.cleanup();
        }
        mcdr_finalize(h);
    }
}

/**
 * Returns the fd used by core dump module to raise events
 *
 * @param [in] h module handle
 *
 * @return a valid fd or CLOSED_FD
 */
int mcdr_get_fd(mcdr_handle_t *h)
{
    mcdr_ctx_t *ctx = (mcdr_ctx_t *)h;
    int fd = CLOSED_FD;

    if (ctx)
        fd = ctx->fd_pipe[READ];

    return fd;
}

/**
 * Returns the path where core dump is stored
 * The returned value must not be freed
 *
 * @param [in] h module handle
 *
 * @return path
 */
const char *mcdr_get_path(mcdr_handle_t *h)
{
    mcdr_ctx_t *ctx = (mcdr_ctx_t *)h;

    ASSERT(ctx != NULL);

    return ctx->data.mcdr_info.gnl.path;
}

/**
 * Returns the modem baseband version of which core dump has occured
 * The returned value must not be freed
 *
 * @param [in] h module handle
 *
 * @return modem baseband version
 */
const char *mcdr_get_mdm_version(mcdr_handle_t *h)
{
    mcdr_ctx_t *ctx = (mcdr_ctx_t *)h;

    ASSERT(ctx != NULL);

    return ctx->data.mdm_version;
}

/**
 * Sets the modem baseband version of which core dump has occured
 * no return value
 *
 * @param [in] h module handle
 * @param [in] pmdm_version modem version string
 *
 * @void
 */
void mcdr_set_mdm_version(mcdr_handle_t *h, char *pmdm_version)
{
    mcdr_ctx_t *ctx = (mcdr_ctx_t *)h;

    ASSERT(ctx != NULL);
    ASSERT(pmdm_version != NULL);

    snprintf(ctx->data.mdm_version, sizeof(ctx->data.mdm_version), "%s",
             pmdm_version);
}

/**
 * Returns the filename where core dump is stored
 * The returned value must not be freed
 *
 * @param [in] h module handle
 *
 * @return filename
 */
const char *mcdr_get_filename(mcdr_handle_t *h)
{
    mcdr_ctx_t *ctx = (mcdr_ctx_t *)h;

    ASSERT(ctx != NULL);

    return ctx->data.coredump_file;
}

/**
 * Returns the error reason
 * The returned value must not be freed
 *
 * @param [in] h module handle
 *
 * @return error reason
 */
const char *mcdr_get_error_reason(mcdr_handle_t *h)
{
    mcdr_ctx_t *ctx = (mcdr_ctx_t *)h;

    ASSERT(ctx != NULL);

    return ctx->mcdr.get_reason();
}

/**
 * Returns if core dump feature is enabled
 *
 * @param [in] h module handle
 *
 * @return false by default
 * @return valid state otherwise
 */
bool mcdr_is_enabled(mcdr_handle_t *h)
{
    bool enable = false;
    mcdr_ctx_t *ctx = (mcdr_ctx_t *)h;

    if (ctx)
        enable = ctx->enabled;

    return enable;
}

/**
 * Returns true if core dump logs feature is enabled
 *
 * @param [in] h module handle
 *
 * @return false by default
 * @return valid state otherwise
 */
bool mcdr_log_is_enabled(mcdr_handle_t *h)
{
    bool enable = false;
    mcdr_ctx_t *ctx = (mcdr_ctx_t *)h;

    if (ctx)
        enable = ctx->data.mcdr_info.gnl.enable_cd_log;

    return enable;
}

/**
 * Return core dump retrieval operation status
 *
 * @param [in] h module handle
 *
 * @return e_core_dump_state_t. E_CD_OTHER if core dump is disabled
 */
e_core_dump_state_t mcdr_get_result(mcdr_handle_t *h)
{
    mcdr_ctx_t *ctx = (mcdr_ctx_t *)h;
    e_core_dump_state_t state = E_CD_OTHER;

    ASSERT(ctx != NULL);

    LOG_DEBUG("MCDR state:%d", ctx->mcdr.get_state());

    switch (ctx->mcdr.get_state()) {
    case MCDR_ARCHIVE_IN_PROGRESS:
    case MCDR_SUCCEED:
        state = E_CD_SUCCEED;
        break;

    case MCDR_CLEANING_UP:
    case MCDR_FS_ERROR:
    case MCDR_INIT_ERROR:
    case MCDR_IO_ERROR:
        state = E_CD_LINK_ERROR;
        break;

    case MCDR_START_PROT_ERR:
    case MCDR_PROT_ERROR:
        state = E_CD_PROTOCOL_ERROR;
        break;

    default:
        /* nothing to do */
        break;
    }

    return state;
}
