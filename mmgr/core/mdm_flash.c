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

#include <pthread.h>
#include <stdlib.h>
#include <openssl/md5.h>

#define MMGR_FW_OPERATIONS
#include "mdm_flash.h"

#include "errors.h"
#include "file.h"
#include "folder.h"
#include "logs.h"
#include "mdm_fw.h"
#include "mdm_mup.h"
#include "property.h"
#include "tcs_config.h"
#include "tty.h"

#define WRITE 1
#define READ 0

#define MD5_HASH_SIZE 16
#define HASH_SIZE (2 * MD5_HASH_SIZE + 1)

#if HASH_SIZE > PROPERTY_VALUE_MAX
#error "HASH_SIZE cannot exceed PROPERTY_VALUE_MAX"
#endif

typedef struct hash_property {
    const char *key;
    char value[HASH_SIZE];
} hash_property_t;

typedef struct mdm_flash_ctx {
    mdm_mup_hdle_t mup;

    const mdm_fw_hdle_t *fw;
    const secure_handle_t *secure;
    const link_hdle_t *link;

    bool flashless;

    bool update;
    bool flash_ongoing;
    int attempts;
    e_modem_fw_error_t flash_err;
    mdm_flash_upgrade_err_t upgrade_err;

    pthread_t id;
    int fd_pipe[2];

    const char *fw_file;
    /* @TODO: This should be removed once the MUP API is updated to package and
     * push directly a modem fw */
    const char *fw_packaged;

    hash_property_t blob_hash;
    hash_property_t cfg_hash;
} mdm_flash_ctx_t;

static inline void mdm_flash_set_upgrade_err(mdm_flash_ctx_t *flash,
                                             mdm_flash_upgrade_err_t upgrade_err)
{
    flash->upgrade_err |= upgrade_err;
}

static inline bool mdm_flash_is_property_equal(const char *key,
                                               const char *value)
{
    char read[PROPERTY_VALUE_MAX];

    property_get_string(key, read);
    return 0 == strcmp(read, value);
}

static inline bool mdm_flash_are_property_hashes_empty(mdm_flash_ctx_t *flash)
{
    return mdm_flash_is_property_equal(flash->blob_hash.key, "")
           && mdm_flash_is_property_equal(flash->cfg_hash.key, "");
}

static void mdm_flash_compute_cfg_hash(mdm_flash_ctx_t *flash, const char *fw)
{
    MD5_CTX md5_ctx;
    unsigned char md5[MD5_HASH_SIZE];

    ASSERT(flash != NULL);
    ASSERT(fw != NULL);

    const tlvs_info_t *tlvs = mdm_fw_get_tlvs(flash->fw);

    MD5_Init(&md5_ctx);

    for (size_t i = 0; i < tlvs->nb; i++)
        MD5_Update(&md5_ctx, tlvs->tlv[i].filename,
                   strlen(tlvs->tlv[i].filename));

    MD5_Update(&md5_ctx, fw, strlen(fw));

    MD5_Final(md5, &md5_ctx);

    /* convert in hexa */
    for (size_t i = 0; i < sizeof(md5); i++)
        sprintf(&flash->cfg_hash.value[i * 2], "%02x", md5[i]);
}

/**
 * Detects if both hashes (blob_hash and config_hash) have been updated.
 *
 * @param [in] flash module context
 * @param [in] fw_path
 *
 * @return true if an update is detected
 */
static bool mdm_flash_is_hash_changed(mdm_flash_ctx_t *flash,
                                      const char *fw_path)
{
    bool ret = false;

    ASSERT(flash != NULL);

    if (!mdm_flash_is_property_equal(flash->blob_hash.key,
                                     flash->blob_hash.value)) {
        LOG_INFO("Change in firmware folder detected");
        ret = true;
    }

    mdm_flash_compute_cfg_hash(flash, fw_path);
    if (!mdm_flash_is_property_equal(flash->cfg_hash.key,
                                     flash->cfg_hash.value)) {
        LOG_INFO("Change in configuration detected");
        ret = true;
    }

    return ret;
}

static e_mmgr_errors_t mdm_flash_init_hash(mdm_flash_ctx_t *flash,
                                           const key_hdle_t *keys)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;

    ASSERT(flash != NULL);
    ASSERT(keys != NULL);

    flash->blob_hash.key = key_get_blob(keys);
    flash->cfg_hash.key = key_get_cfg(keys);

    /* Blob hash is only updated during a phone update (OTA or fastboot). That
     * is why, this value is read during the init */
    if (E_ERR_SUCCESS != file_read(mdm_fw_get_blob_hash_path(flash->fw),
                                   flash->blob_hash.value,
                                   sizeof(flash->blob_hash.value))) {
        LOG_ERROR("failed to read blob hash content");
        ret = E_ERR_FAILED;
    }

    return ret;
}

static void mdm_flash_push(mdm_flash_ctx_t *ctx)
{
    char msg = 0;

    ASSERT(ctx != NULL);
    ASSERT(ctx->fw_file != NULL);

    const char *ebl_port = link_get_flash_ebl_interface(ctx->link);
    const char *fw_port = link_get_flash_fw_interface(ctx->link);

    LOG_DEBUG("[SLAVE-FLASH] start modem flashing");
    ctx->flash_err = mdm_mup_push_fw(ctx->mup, ctx->fw_file, ebl_port, fw_port);

    LOG_DEBUG("[SLAVE-FLASH] flashing done. Notify main thread");
    write(ctx->fd_pipe[WRITE], &msg, sizeof(msg));
}

static void mdm_flash_remove_fw_packaged(mdm_flash_ctx_t *flash)
{
    ASSERT(flash != NULL);

    if (flash->fw_file) {
        if (flash->flashless)
            unlink(flash->fw_file);
        flash->fw_file = NULL;
    }
}

/**
 * Returns modem flashing state
 *
 * @param [in] hdle modem flashing handle
 *
 * @return flashing state
 */
e_modem_fw_error_t mdm_flash_get_flashing_err(mdm_flash_handle_t *hdle)
{
    mdm_flash_ctx_t *ctx = (mdm_flash_ctx_t *)hdle;

    ASSERT(ctx != NULL);

    return ctx->flash_err;
}

/**
 * Returns the provisioning error status
 *
 * @param hdle module handle
 *
 * @return mdm_flash_upgrade_err_t
 */
mdm_flash_upgrade_err_t mdm_flash_get_upgrade_err(const mdm_flash_handle_t *hdle)
{
    const mdm_flash_ctx_t *flash = (mdm_flash_ctx_t *)hdle;

    ASSERT(flash != NULL);

    return flash->upgrade_err;
}

/**
 * Initializes modem flashing module. The function will assert in case of error
 *
 * @param [in] mdm_info modem info
 * @param [in] fw pointer to fw module
 * @param [in] secure pointer to secure module
 * @param [in] link modem links module
 *
 * @return a valid handle. Must be freed by calling mdm_flash_dispose
 */
mdm_flash_handle_t *mdm_flash_init(const mdm_info_t *mdm_info,
                                   const mdm_fw_hdle_t *fw,
                                   const secure_handle_t *secure,
                                   const key_hdle_t *keys,
                                   link_hdle_t *link)
{
    mdm_flash_ctx_t *ctx = calloc(1, sizeof(mdm_flash_ctx_t));

    ASSERT(ctx != NULL);
    ASSERT(mdm_info != NULL);
    ASSERT(fw != NULL);
    ASSERT(secure != NULL);
    ASSERT(link != NULL);
    ASSERT(keys != NULL);

    ctx->fw = fw;
    ctx->link = link;
    ctx->secure = secure;
    ctx->flashless = mdm_info->core.flashless;
    ctx->upgrade_err = MDM_UPDATE_ERR_NONE;
    ctx->flash_err = E_MODEM_FW_ERROR_UNSPECIFIED;
    ctx->fw_packaged = mdm_fw_get_fw_package_path(fw);
    ctx->flash_ongoing = false;
    ctx->attempts = 0;

    ctx->mup = mdm_mup_init(mdm_info->core.name,
                            mdm_info->chs.ch->mmgr.mdm_custo.device,
                            mdm_fw_get_rnd_path(fw), link, secure);
    ASSERT(ctx->mup != NULL);

    ASSERT(E_ERR_SUCCESS == mdm_flash_init_hash(ctx, keys));

    /* After a factory reset, hash properties are empty. Dynamic NVM file and
     * miu provisioning files must be deleted */
    if (mdm_flash_are_property_hashes_empty(ctx)) {
        LOG_DEBUG("factory reset detected");
        errno = 0;
        if (!unlink(mdm_fw_get_nvm_dyn_path(fw)))
            LOG_INFO("dynamic NVM file removed");
        else
            LOG_DEBUG("dynamic file not removed: %s", strerror(errno));
        if (!folder_remove(mdm_fw_dbg_get_miu_folder(fw)))
            LOG_INFO("RnD provisioning folder removed");
    }

    ASSERT(pipe(ctx->fd_pipe) == 0);

    return (mdm_flash_handle_t *)ctx;
}

/**
 * Prepares modem fw. It packages the fw with the NVM data
 *
 * @param [in] hdle module handle
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t mdm_flash_prepare(mdm_flash_handle_t *hdle)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    mdm_flash_ctx_t *flash = (mdm_flash_ctx_t *)hdle;

    ASSERT(flash != NULL);

    flash->upgrade_err = MDM_UPDATE_ERR_NONE;

    const char *input = mdm_fw_get_fw_path(flash->fw);
    if (input) {
        flash->update = mdm_flash_is_hash_changed(flash, input);
        if (flash->flashless) {
            mdm_mup_package(flash->mup, mdm_fw_get_runtime_path(flash->fw),
                            input, flash->fw_packaged);
            flash->fw_file = flash->fw_packaged;
        } else {
            flash->fw_file = input;
        }

        if (flash->fw_file && file_exist(flash->fw_file)) {
            LOG_INFO("Modem firmware ready: %s", flash->fw_file);
            ret = E_ERR_SUCCESS;
        } else {
            flash->fw_file = NULL;
        }
    }

    if (E_ERR_SUCCESS != ret)
        mdm_flash_set_upgrade_err(flash, MDM_UPDATE_ERR_FLASH);

    return ret;
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
    mdm_flash_ctx_t *flash = (mdm_flash_ctx_t *)hdle;
    e_mmgr_errors_t ret = E_ERR_SUCCESS;

    ASSERT(flash != NULL);

    if (!flash->flash_ongoing) {
        flash->attempts++;
        flash->flash_err = E_MODEM_FW_ERROR_UNSPECIFIED;

        link_on_mdm_flash(flash->link);
        pthread_create(&flash->id, NULL, (void *)mdm_flash_push, (void *)flash);
        flash->flash_ongoing = true;
    } else {
        ret = E_ERR_FAILED;
        LOG_ERROR("thread already running");
    }

    return ret;
}

/**
 * Finalizes the flashing operation. It joins the thread
 *
 * @param [in] hdle flashing module
 *
 */
void mdm_flash_finalize(mdm_flash_handle_t *hdle)
{
    mdm_flash_ctx_t *flash = (mdm_flash_ctx_t *)hdle;

    ASSERT(flash != NULL);

    if (flash->flash_ongoing) {
        pthread_join(flash->id, NULL);
        flash->flash_ongoing = false;
        mdm_flash_remove_fw_packaged(flash);
        link_on_mdm_flash_complete(flash->link);
        LOG_DEBUG("[MASTER] flashing thread is stopped");
    }
}

/**
 * Pushes all streamline updates
 *
 * @param [in] hdle module handle
 * @param [out] err provides an NVM status
 *
 * @return NULL in case of success
 * @return the TLV file creating the error. This pointer must NOT be freed
 */
const char *mdm_flash_streamline(mdm_flash_handle_t *hdle,
                                 mmgr_cli_nvm_update_result_t *err)
{
    mdm_flash_ctx_t *flash = (mdm_flash_ctx_t *)hdle;
    char *filename = NULL;

    ASSERT(flash != NULL);
    ASSERT(err != NULL);


    if (!flash->update) {
        LOG_INFO("No streamline update");
        err->id = E_MODEM_NVM_NO_NVM_PATCH;
        err->sub_error_code = E_MODEM_NVM_SUCCEED;
    } else {
        /* If this function is called after a flashing failure, we have a logic
         * issue */
        ASSERT(E_MODEM_FW_SUCCEED == mdm_flash_get_flashing_err(hdle));
        const tlvs_info_t *tlvs = mdm_fw_get_tlvs(flash->fw);

        for (size_t i = 0; i < tlvs->nb; i++) {
            *err = mdm_mup_push_tlv(flash->mup, tlvs->tlv[i].filename);
            if (E_MODEM_NVM_SUCCEED != err->id) {
                mdm_flash_set_upgrade_err(flash, MDM_UPDATE_ERR_TLV);
                filename = tlvs->tlv[i].filename;
                break;
            }
        }
        if (E_MODEM_NVM_SUCCEED == err->id)
            LOG_INFO("Streamline updates successfully applied");
        flash->update = false;
    }

    /* Properties are updated after a successful modem flashing and TLV
     * application */
    if ((E_MODEM_NVM_NO_NVM_PATCH == err->id) ||
        (E_MODEM_NVM_SUCCEED == err->id)) {
        property_set(flash->blob_hash.key, flash->blob_hash.value);
        property_set(flash->cfg_hash.key, flash->cfg_hash.value);
    }

    return (const char *)filename;
}

/**
 * Returns the fd used by the module to notify events
 *
 * @param [in] hdle flashing module
 *
 * @return a valid fd or CLOSED_FD
 */
int mdm_flash_get_fd(const mdm_flash_handle_t *hdle)
{
    const mdm_flash_ctx_t *ctx = (mdm_flash_ctx_t *)hdle;
    int fd = CLOSED_FD;

    if (ctx)
        fd = ctx->fd_pipe[READ];

    return fd;
}

/**
 * Cancels flashing operation. This function will be called
 * when the flashing operation reaches timeout.
 * Because it isn't possible to properly stop the flashing
 * thread, MMGR is stopped here in order to be re-launched by the
 * Android framework, because it is a persistent service.
 *
 * @param [in] hdle flashing module
 */
void mdm_flash_cancel(mdm_flash_handle_t *hdle)
{
    mdm_flash_remove_fw_packaged((mdm_flash_ctx_t *)hdle);
    exit(EXIT_FAILURE);
}

/**
 * Frees the module memory
 *
 * @param [in] hdle flashing module
 */
void mdm_flash_dispose(mdm_flash_handle_t *hdle)
{
    mdm_flash_ctx_t *ctx = (mdm_flash_ctx_t *)hdle;

    if (ctx) {
        mdm_mup_dispose(ctx->mup);
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
int mdm_flash_get_attempts(const mdm_flash_handle_t *hdle)
{
    const mdm_flash_ctx_t *ctx = (mdm_flash_ctx_t *)hdle;

    ASSERT(ctx != NULL);

    return ctx->attempts;
}

/**
 * Resets the number of flashing attempts
 *
 * @param [in] hdle flashing module
 */
void mdm_flash_reset_attempts(mdm_flash_handle_t *hdle)
{
    mdm_flash_ctx_t *ctx = (mdm_flash_ctx_t *)hdle;

    ASSERT(ctx != NULL);

    ctx->attempts = 0;
}

/**
 * Detects if a modem firmware upload is needed
 *
 * @param [in] hdle module handle
 *
 * @return true if a modem upload is needed
 */
bool mdm_flash_is_required(const mdm_flash_handle_t *hdle)
{
    mdm_flash_ctx_t *flash = (mdm_flash_ctx_t *)hdle;

    ASSERT(flash != NULL);

    return flash->flashless || flash->update;
}
