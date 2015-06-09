/* Modem Manager - modem mup source file
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
#include <dlfcn.h>

#include "mdm_mup.h"
#include "file.h"
#include "logs.h"
#include "modem_update.h"

#include "tcs_mmgr.h"
#include "security.h"

#define MUP_LIB "libmodemupdate.so"
#define MUP_FUNC_INIT "mup_initialize"
#define MUP_FUNC_OPEN "mup_open_device"
#define MUP_FUNC_UP_FW "mup_update_fw"
#define MUP_FUNC_UP_NVM "mup_update_nvm"
#define MUP_FUNC_DISPOSE "mup_dispose"
#define MUP_FUNC_FW_VERSION "mup_check_fw_version"
#define MUP_FUNC_CONFIG_SECUR "mup_configure_secur_channel"
#define MUP_FUNC_GEN_FLS "mup_gen_fls"

typedef struct mup_op {
    e_mup_err_t (*initialize)(mup_interface_t **handle,
                              mup_ap_log_callback_t ap_log_callback);
    e_mup_err_t (*open_device)(mup_fw_update_params_t *params);
    e_mup_err_t (*update_fw)(mup_fw_update_params_t *params);
    e_mup_err_t (*update_nvm)(mup_nvm_update_params_t *params);
    e_mup_err_t (*check_fw_version)(const char *fw_path, const char *version);
    e_mup_err_t (*dispose)(mup_interface_t *handle);

    e_mup_err_t (*config_secur_channel)(mup_interface_t *handle, void *func,
                                        const char *rnd_path);
    e_mup_err_t (*gen_fls)(const char *in, const char *out, const char *dir,
                           const char *certificate, const char *secur);
} mup_op_t;

typedef struct mup_cfg {
    char *mdm_name;
    char *streamline_dlc;
    char *rnd;
    bool channel_switching;
    int baudrate;
} mup_cfg_t;

typedef struct mdm_mup_ctx {
    void *hdle;
    mup_op_t ops;
    mup_cfg_t cfg;
    const secure_handle_t *sec_hdle;
} mdm_mup_ctx_t;

/**
 * callback to log MUP messages in aplog
 *
 * @param [in] msg message to display
 */
static void mup_log(const char *msg, ...)
{
    if (msg != NULL) {
        char buff[256];
        va_list ap;

        va_start(ap, msg);
        vsnprintf(buff, sizeof(buff), msg, ap);
        LOG_DEBUG("%s", buff);
        va_end(ap);
    }
}

/**
 * Pushes the modem firmware
 *
 * @param [in] hdle module handle
 * @param [in] fw modem firmware
 * @param [in] eb_port port to use to push the EB (bootloader)
 * @param [in] fls_port port to use to push the FW
 *
 * @return E_ERR_SUCCESS
 * @return E_ERR_FAILED
 */
e_modem_fw_error_t mdm_mup_push_fw(const mdm_mup_hdle_t *hdle, const char *fw,
                                   const char *eb_port, const char *fls_port)
{
    const mdm_mup_ctx_t *mup = (mdm_mup_ctx_t *)hdle;
    mup_interface_t *mup_ctx = NULL;
    e_modem_fw_error_t verdict = E_MODEM_FW_ERROR_UNSPECIFIED;

    ASSERT(mup != NULL);
    ASSERT(fw != NULL);
    ASSERT(eb_port != NULL);
    ASSERT(fls_port != NULL);
    ASSERT(mup->ops.initialize != NULL);
    ASSERT(mup->ops.check_fw_version != NULL);
    ASSERT(mup->ops.open_device != NULL);
    ASSERT(mup->ops.config_secur_channel != NULL);
    ASSERT(mup->ops.update_fw != NULL);
    ASSERT(mup->cfg.rnd != NULL);

    if (E_MUP_SUCCEED != mup->ops.initialize(&mup_ctx, mup_log)) {
        LOG_ERROR("modem updater initialization failed");
        goto out;
    }

    if (mup->ops.check_fw_version(fw, mup->cfg.mdm_name) != E_MUP_SUCCEED) {
        LOG_ERROR("Bad modem family");
        verdict = E_MODEM_FW_BAD_FAMILY;
        goto out;
    }

    mup_fw_update_params_t params = {
        .handle = mup_ctx,
        .mdm_eb_port = eb_port,
        .mdm_fls_port = fls_port,
        .channel_hw_sw = mup->cfg.channel_switching,
        .fw_file_path = fw,
        .fw_file_path_len = strlen(fw),
        .erase_all = false,
        .baudrate = mup->cfg.baudrate,
    };

    if (E_MUP_SUCCEED != mup->ops.open_device(&params)) {
        LOG_ERROR("failed to open device");
        goto out;
    }

    const char *rnd = NULL;

    if (file_exist(mup->cfg.rnd)) {
        rnd = mup->cfg.rnd;
    }

    if (E_MUP_SUCCEED !=
        mup->ops.config_secur_channel(mup_ctx,
                                      secure_get_callback(mup->sec_hdle),
                                      rnd)) {
        LOG_ERROR("failed to configure the secured channel");
        goto out;
    }

    e_mup_err_t err = mup->ops.update_fw(&params);
    static const char const *verdict_str[] = {
#undef X
#define X(a) #a
        MUP_STATE
    };
    LOG_DEBUG("verdict: %s", verdict_str[err]);

    switch (err) {
    case E_MUP_FW_RESTRICTED:
    case E_MUP_SUCCEED:
        verdict = E_MODEM_FW_SUCCEED;
        break;
    case E_MUP_FAILED:
        /* nothing to do */
        break;
    case E_MUP_FW_OUTDATED:
        verdict = E_MODEM_FW_OUTDATED;
        break;
    case E_MUP_FW_CORRUPTED:
        verdict = E_MODEM_FW_SECURITY_CORRUPTED;
        break;
    case E_MUP_BAD_PARAMETER:
        /* nothing to do */
        break;
    }

out:
    if (mup_ctx)
        if (E_MUP_SUCCEED != mup->ops.dispose(mup_ctx))
            LOG_ERROR("modem updater disposal fails");

    return verdict;
}

/**
 * Packages the modem firmware (FW+NVM files)
 *
 * @param [in] hdle module handle
 * @param [in] nvm_folder folder where NVM files are stored
 * @param [in] input modem firmware
 * @param [out] output modem packaged
 *
 * @return E_ERR_SUCCESS
 * @return E_ERR_FAILED
 */
e_mmgr_errors_t mdm_mup_package(const mdm_mup_hdle_t *hdle,
                                const char *nvm_folder, const char *input,
                                const char *output)
{
    const mdm_mup_ctx_t *mup = (mdm_mup_ctx_t *)hdle;
    e_mmgr_errors_t ret = E_ERR_SUCCESS;

    ASSERT(mup != NULL);
    ASSERT(nvm_folder != NULL);
    ASSERT(input != NULL);
    ASSERT(output != NULL);
    ASSERT(mup->ops.gen_fls != NULL);

    /* @TODO: add certificate and secur files */
    if (E_MUP_SUCCEED != mup->ops.gen_fls(input, output, nvm_folder, "", "")) {
        LOG_ERROR("Modem firmware packaging has failed");
        ret = E_ERR_FAILED;
    }

    return ret;
}

/**
 * Push a streamline update (tlv scripts)
 *
 * @param [in] hdle module handle
 * @param [in] tlv streamline file to apply
 *
 * @return mmgr_cli_nvm_update_result_t
 */
mmgr_cli_nvm_update_result_t mdm_mup_push_tlv(const mdm_mup_hdle_t *hdle,
                                              const char *filename)

{
    const mdm_mup_ctx_t *mup = (mdm_mup_ctx_t *)hdle;
    mup_interface_t *mup_ctx = NULL;
    mmgr_cli_nvm_update_result_t ret = {
        .id = E_MODEM_NVM_NO_NVM_PATCH,
        .sub_error_code = E_MUP_FAILED,
    };

    ASSERT(mup != NULL);
    ASSERT(filename != NULL);
    ASSERT(mup->ops.initialize != NULL);
    ASSERT(mup->ops.update_nvm != NULL);
    ASSERT(mup->ops.dispose != NULL);
    ASSERT(mup->cfg.streamline_dlc != NULL);

    if (E_MUP_SUCCEED != mup->ops.initialize(&mup_ctx, mup_log)) {
        LOG_ERROR("modem updater initialization failed");
    } else {
        mup_nvm_update_params_t params = {
            .handle = mup_ctx,
            .mdm_com_port = mup->cfg.streamline_dlc,
            .nvm_file_path = (char *)filename,
            .nvm_file_path_len = strlen(filename),
        };

        ret.sub_error_code = mup->ops.update_nvm(&params);
        if (E_MUP_SUCCEED == ret.sub_error_code) {
            ret.id = E_MODEM_NVM_SUCCEED;
            LOG_INFO("TLV file successfuly applied: %s", filename);
        } else {
            LOG_ERROR("failed to apply: %s", filename);
            ret.id = E_MODEM_NVM_FAIL;
        }

        if (E_MUP_SUCCEED != mup->ops.dispose(mup_ctx))
            LOG_ERROR("modem updater disposal failed");
    }

    return ret;
}

/**
 * Initiliazes mup module
 *
 * @param [in] mdm_name modem name
 * @param [in] streamline_dlc DLC to use to push streamline updates
 * @param [in] rnd RnD certificate path
 * @param [in] link link module
 * @param [in] sec_hdle secure module
 *
 * @return a valid pointer. must be freed by caller by calling mdm_mup_dispose
 */
mdm_mup_hdle_t *mdm_mup_init(const char *mdm_name, const char *streamline_dlc,
                             const char *rnd, const link_hdle_t *link,
                             const secure_handle_t *sec_hdle)
{
    mdm_mup_ctx_t *mup = calloc(1, sizeof(mdm_mup_ctx_t));

    ASSERT(mdm_name != NULL);
    ASSERT(streamline_dlc != NULL);
    ASSERT(rnd != NULL);
    ASSERT(sec_hdle != NULL);
    ASSERT(mup != NULL);

    mup->hdle = dlopen(MUP_LIB, RTLD_LAZY);
    ASSERT(mup->hdle != NULL);

    mup->ops.initialize = dlsym(mup->hdle, MUP_FUNC_INIT);
    mup->ops.open_device = dlsym(mup->hdle, MUP_FUNC_OPEN);
    mup->ops.update_fw = dlsym(mup->hdle, MUP_FUNC_UP_FW);
    mup->ops.update_nvm = dlsym(mup->hdle, MUP_FUNC_UP_NVM);
    mup->ops.dispose = dlsym(mup->hdle, MUP_FUNC_DISPOSE);
    mup->ops.config_secur_channel = dlsym(mup->hdle, MUP_FUNC_CONFIG_SECUR);
    mup->ops.check_fw_version = dlsym(mup->hdle, MUP_FUNC_FW_VERSION);
    mup->ops.gen_fls = dlsym(mup->hdle, MUP_FUNC_GEN_FLS);

    if (dlerror() != NULL) {
        LOG_ERROR("An error occurred during symbol resolution");
        mdm_mup_dispose((mdm_mup_hdle_t *)mup);
        mup = NULL;
    } else {
        mup->cfg.mdm_name = strdup(mdm_name);
        mup->cfg.streamline_dlc = strdup(streamline_dlc);
        mup->cfg.rnd = strdup(rnd);
        mup->sec_hdle = sec_hdle;
        mup->cfg.baudrate = link_get_ebl_baudrate(link);

        if (E_LINK_USB == link_get_flash_ebl_type(link))
            mup->cfg.channel_switching = false;
        else
            mup->cfg.channel_switching = true;
    }

    return (mdm_mup_hdle_t *)mup;
}

/**
 * Disposes the module
 *
 * @param [in] hdle module handle
 */
void mdm_mup_dispose(mdm_mup_hdle_t *hdle)
{
    mdm_mup_ctx_t *mup = (mdm_mup_ctx_t *)hdle;

    ASSERT(mup != NULL);

    dlclose(mup->hdle);
    mup->hdle = NULL;
    free(mup->cfg.mdm_name);
    free(mup->cfg.streamline_dlc);
    free(mup->cfg.rnd);
    free(mup);
}
