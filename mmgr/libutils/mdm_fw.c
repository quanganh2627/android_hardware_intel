/* Modem Manager - modem fw source file
**
** ** Copyright (C) Intel 2014
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

#include <libgen.h>
#include <sys/stat.h>

#include "common.h"
#include "errors.h"
#include "file.h"
#include "folder.h"
#include "logs.h"
#include "mdm_fw.h"
#include "tcs_config.h"

#define FLS_FILE_PERMISSION (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
#define REGEXP_SIZE 128

/**
 * This macro is used to disable the debug code of this module in production
 * (build user).
 *
 * Functions prefixed by mdm_fw_dbg are debug ones. Thanks to the macro,
 * for build user, those functions will be empty and then GCC will remove
 * them */
#ifdef MMGR_DEBUG
#define MDM_FW_DEBUG 1
#else
#define MDM_FW_DEBUG 0
#endif

typedef struct mdm_dbg_fw {
    char *miu_folder;
    char *miu_fw;
    tlvs_info_t tlvs;
} mdm_dbg_fw_t;

typedef struct mdm_fw {
    int id; /* MMGR instance id */
    tlvs_info_t tlvs;
    mmgr_fw_t cfg;
    char fw_regexp[REGEXP_SIZE];
    bool flashless;
    /* A pointer is used here to reduce memory usage in production */
    mdm_dbg_fw_t *dbg;

    char *blob_path;
    char *fw_file;
    char *fw_packaged_path;
    char *rnd_path;
    char *nvm_dyn_path;
    char *nvm_sta_path;
} mdm_fw_t;

static char *mdm_fw_compute_path(const char *folder, const char *filename)
{
    ASSERT(folder != NULL);
    ASSERT(filename != NULL);

    size_t len = strlen(folder) + strlen(filename) + 2;
    char *path = malloc(sizeof(char) * len);
    ASSERT(path != NULL);

    snprintf(path, len, "%s/%s", folder, filename);

    return path;
}

static void mdm_fw_dbg_init(mdm_fw_t *fw)
{
    if (MDM_FW_DEBUG) {
        ASSERT(fw != NULL);
        fw->dbg = calloc(1, sizeof(mdm_dbg_fw_t));
        ASSERT(fw->dbg != NULL);
        char subfolder[10];
        snprintf(subfolder, sizeof(subfolder), "miu_%d", fw->id);
        fw->dbg->miu_folder = mdm_fw_compute_path(fw->cfg.folders.input,
                                                  subfolder);
        fw->dbg->miu_fw = mdm_fw_compute_path(fw->dbg->miu_folder, "fw.fls");
        fw->rnd_path =
            mdm_fw_compute_path(fw->dbg->miu_folder, fw->cfg.rnd.cert);
    }
}

static void mdm_fw_dbg_dispose(mdm_fw_t *fw)
{
    if (MDM_FW_DEBUG) {
        ASSERT(fw != NULL);
        free(fw->dbg->tlvs.tlv);
        free(fw->dbg->miu_folder);
        free(fw->dbg->miu_fw);
        free(fw->dbg);
    }
}

static void mdm_fw_dbg_get_tlvs(mdm_fw_t *fw, tlvs_info_t **tlvs)
{
    if (MDM_FW_DEBUG) {
        ASSERT(fw != NULL);
        ASSERT(tlvs != NULL);
        ASSERT(fw->dbg != NULL);

        size_t found;
        char **files = file_find_ext(fw->dbg->miu_folder, "tlv", &found);
        if (found > 0) {
            ASSERT(files != NULL);
            size_t nb = fw->tlvs.nb + found;

            if (!fw->dbg->tlvs.tlv) {
                fw->dbg->tlvs.tlv = malloc(sizeof(tlv_info_t) * nb);
                ASSERT(fw->dbg->tlvs.tlv != NULL);

                /* copy default and overlay tlvs */
                memcpy(fw->dbg->tlvs.tlv, fw->tlvs.tlv,
                       (fw->tlvs.nb * sizeof(fw->tlvs.tlv[0])));
            } else {
                fw->dbg->tlvs.tlv = realloc(fw->dbg->tlvs.tlv,
                                            sizeof(tlv_info_t) * nb);
                ASSERT(fw->dbg->tlvs.tlv != NULL);
            }

            for (size_t i = 0; i < found; i++) {
                LOG_DEBUG("streamline added by miu: %s", files[i]);
                snprintf(fw->dbg->tlvs.tlv[i + fw->tlvs.nb].filename,
                         sizeof(fw->dbg->tlvs.tlv[i + fw->tlvs.nb].filename),
                         "%s", files[i]);
                free(files[i]);
            }
            free(files);

            fw->dbg->tlvs.nb = nb;
            *tlvs = &fw->dbg->tlvs;
        }
    }
}

/**
 * Returns the miu firmware path
 *
 * @param hdle module handle
 *
 * @return a string containing the fw path. Returns NULL in production
 */
const char *mdm_fw_dbg_get_miu_folder(const mdm_fw_hdle_t *hdle)
{
    mdm_fw_t *fw = (mdm_fw_t *)hdle;
    char *miu_path = NULL;

    if (MDM_FW_DEBUG) {
        ASSERT(fw != NULL);
        ASSERT(fw->dbg != NULL);
        miu_path = fw->dbg->miu_folder;
    }

    return miu_path;
}

/**
 * Returns the miu firmware path
 *
 * @param hdle module handle
 *
 * @return a string containing the fw path. Returns NULL in production
 */
const char *mdm_fw_dbg_get_miu_fw_path(const mdm_fw_hdle_t *hdle)
{
    mdm_fw_t *fw = (mdm_fw_t *)hdle;
    char *miu_fw = NULL;

    if (MDM_FW_DEBUG) {
        ASSERT(fw != NULL);
        ASSERT(fw->dbg != NULL);
        miu_fw = fw->dbg->miu_fw;
    }

    return miu_fw;
}

/**
 * Create fls regexp used to detect the right FLS in the filesystem
 *
 * @param [out] regexp
 * @param [in] mdm_name
 * @param [in] flashless
 * @param [in] hw_revision
 * @param [in] sw_revision
 *
 * @return E_ERR_SUCCESS
 * @return E_ERR_FAILED
 */
static void get_fw_regexp(char *regexp, const char *mdm_name, bool flashless,
                          const char *hw_revision, const char *sw_revision)
{
    static const char const *fw_type[] = { "NAND", "FLASHLESS" };

    ASSERT(regexp != NULL);
    ASSERT(mdm_name != NULL);
    ASSERT(hw_revision != NULL);
    ASSERT(sw_revision != NULL);

    if (!strcmp(mdm_name, "6360")) {
        /* This is a 6360 modem. This modem has a different convention naming */
        snprintf(regexp, REGEXP_SIZE, "^XMM_%s_.*%s.*\\.fls$", mdm_name,
                 fw_type[flashless]);
    } else {
        const char *hw = hw_revision;
        const char *sw = sw_revision;

        if (!strcmp(hw_revision, "NA"))
            hw = "[[:digit:]]{2}";
        if (!strcmp(sw_revision, "NA"))
            sw = "[[:digit:]]{1}\\.[[:digit:]]{1}";

        snprintf(regexp, REGEXP_SIZE,
                 "^XMM_%s_REV%s_[[:digit:]]{4}\\.[[:digit:]]{2}_V%s.*%s.*\\.fls$",
                 mdm_name, hw, sw, fw_type[flashless]);
    }
}

static inline void mdm_fw_get_calib_files(const mdm_fw_t *fw, char *run,
                                          char *bkup, size_t size)
{
    snprintf(run, size, "%s/%s", fw->cfg.folders.runtime,
             fw->cfg.nvm.cal);
    snprintf(bkup, size, "%s/%s", fw->cfg.folders.factory,
             fw->cfg.nvm.cal);
}

static void mdm_fw_prepare_calib(mdm_fw_t *fw)
{
    char run[MY_PATH_MAX];
    char bkup[MY_PATH_MAX];

    ASSERT(fw != NULL);

    mdm_fw_get_calib_files(fw, run, bkup, MY_PATH_MAX);

    /* Restore calibration file from factory if missing */
    if (!file_exist(run)) {
        if (E_ERR_SUCCESS != file_copy(bkup, run, FLS_FILE_PERMISSION))
            /* This is not a blocking error case because this can happen in
             * production when first calib is about to be done. Just raise a
             * warning. */
            LOG_INFO("Device must be re-calibrated. backup is missing (%s)",
                     bkup);
        else
            LOG_INFO("Calibration file restored from %s", bkup);
    }
}

/**
 * Returns runtime path
 *
 * @param hdle module handle
 *
 * @return a string containing the path. The pointer must NOT be freed
 */
const char *mdm_fw_get_runtime_path(const mdm_fw_hdle_t *hdle)
{
    mdm_fw_t *fw = (mdm_fw_t *)hdle;

    ASSERT(fw != NULL);

    return fw->cfg.folders.runtime;
}

/**
 * Returns the hash path
 *
 * @param hdle module handle
 *
 * @return a string containing the blob path
 */
const char *mdm_fw_get_blob_hash_path(const mdm_fw_hdle_t *hdle)
{
    mdm_fw_t *fw = (mdm_fw_t *)hdle;

    ASSERT(fw != NULL);

    return fw->blob_path;
}

/**
 * Returns the RnD certificate path
 *
 * @param hdle module handle
 *
 * @return a string containing the RnD path or an empty path in production
 */
const char *mdm_fw_get_rnd_path(const mdm_fw_hdle_t *hdle)
{
    mdm_fw_t *fw = (mdm_fw_t *)hdle;

    ASSERT(fw != NULL);

    return fw->rnd_path;
}

/**
 * Returns the NVM dynamic path
 *
 * @param hdle module handle
 *
 * @return a string containing the RnD path
 */
const char *mdm_fw_get_nvm_dyn_path(const mdm_fw_hdle_t *hdle)
{
    mdm_fw_t *fw = (mdm_fw_t *)hdle;

    ASSERT(fw != NULL);

    return fw->nvm_dyn_path;
}

/**
 * Returns the NVM static path
 *
 * @param hdle module handle
 *
 * @return a string containing the RnD path
 */
const char *mdm_fw_get_nvm_sta_path(const mdm_fw_hdle_t *hdle)
{
    mdm_fw_t *fw = (mdm_fw_t *)hdle;

    ASSERT(fw != NULL);

    return fw->nvm_sta_path;
}


/**
 * Returns the package firmware path
 *
 * @TODO: This function should be removed once the MUP API is updated to package
 * and push directly a modem fw
 *
 * @param hdle module handle
 *
 * @return a string containing the fw package path
 */
const char *mdm_fw_get_fw_package_path(const mdm_fw_hdle_t *hdle)
{
    mdm_fw_t *fw = (mdm_fw_t *)hdle;

    ASSERT(fw != NULL);

    return fw->fw_packaged_path;
}

/**
 * Returns the factory folder
 *
 * @param hdle module handle
 *
 * @return a string containing the fw path.
 */
const char *mdm_fw_get_factory_folder(const mdm_fw_hdle_t *hdle)
{
    mdm_fw_t *fw = (mdm_fw_t *)hdle;

    ASSERT(fw != NULL);

    return fw->cfg.folders.factory;
}

/**
 * Returns the firmware path
 *
 * @param hdle module handle
 *
 * @return a string containing the fw path
 */
const char *mdm_fw_get_fw_path(const mdm_fw_hdle_t *hdle)
{
    mdm_fw_t *fw = (mdm_fw_t *)hdle;

    ASSERT(fw != NULL);

    /* free previous buffer */
    free(fw->fw_file);
    fw->fw_file = NULL;

    if (fw->dbg && fw->dbg->miu_fw && file_exist(fw->dbg->miu_fw)) {
        fw->fw_file = strdup(fw->dbg->miu_fw);
        LOG_DEBUG("miu fw detected. fw: %s", fw->fw_file);
    } else {
        size_t found;
        char **files = file_find(fw->cfg.folders.input, fw->fw_regexp, &found);
        if (found == 1) {
            ASSERT(files != NULL);
            fw->fw_file = files[0];
            files[0] = NULL;
            LOG_DEBUG("fw: %s", fw->fw_file);
        } else if (found == 0) {
            LOG_ERROR("no modem firmware. The phone must be reflashed");
        } else {
            LOG_ERROR("more than one firmware is available. Provisioning "
                      "folder is corrupted. The phone must be reflashed");
        }

        for (size_t i = 0; i < found; i++)
            free(files[i]);
        free(files);
    }

    return fw->fw_file;
}

/**
 * Returns the list of TLV to be applied
 *
 * @param hdle module handle
 *
 * @return a structure listing all TLVs to be applied. This pointer must NOT
 *         be freed.
 */
const tlvs_info_t *mdm_fw_get_tlvs(const mdm_fw_hdle_t *hdle)
{
    mdm_fw_t *fw = (mdm_fw_t *)hdle;

    ASSERT(fw != NULL);

    tlvs_info_t *tlvs = &fw->tlvs;
    mdm_fw_dbg_get_tlvs(fw, &tlvs);

    return tlvs;
}

/**
 * Backup calib.nvm file in factory. Existing file will be overwritten.
 *
 * @param hdle module handle
 *
 * @return E_ERR_SUCCESS
 * @return E_ERR_FAILED
 */
e_mmgr_errors_t mdm_fw_backup_calib(const mdm_fw_hdle_t *hdle)
{
    mdm_fw_t *fw = (mdm_fw_t *)hdle;
    char run[MY_PATH_MAX];
    char bkup[MY_PATH_MAX];

    ASSERT(fw != NULL);

    mdm_fw_get_calib_files(fw, run, bkup, MY_PATH_MAX);

    return file_copy(run, bkup, FLS_FILE_PERMISSION);
}

/**
 * Creates the needed folders
 *
 * @param hdle module handle
 *
 * @return E_ERR_SUCCESS
 * @return E_ERR_FAILED
 */
e_mmgr_errors_t mdm_fw_create_folders(const mdm_fw_hdle_t *hdle)
{
    mdm_fw_t *fw = (mdm_fw_t *)hdle;
    e_mmgr_errors_t ret = E_ERR_SUCCESS;

    ASSERT(fw != NULL);

    if (fw->flashless) {
        if (!folder_create(fw->cfg.folders.runtime))
            LOG_INFO("runtime directory: %s", fw->cfg.folders.runtime);
        else
            ret = E_ERR_FAILED;

        if (!folder_create(fw->cfg.folders.injection))
            LOG_INFO("injection directory: %s", fw->cfg.folders.injection);
        else
            ret = E_ERR_FAILED;

        mdm_fw_prepare_calib(fw);
    }

    return ret;
}

/**
 * Initializes the modem firmware module
 *
 * @param [in] inst_id MMGR instance ID
 * @param [in] mdm modme info
 * @param [in] fw_cfg firmware configuration
 *
 * @return NULL in case of error
 * @return a valid pointer
 */
mdm_fw_hdle_t *mdm_fw_init(int inst_id, const mdm_info_t *mdm,
                           const mmgr_fw_t *fw_cfg)
{
    mdm_fw_t *fw = calloc(1, sizeof(mdm_fw_t));

    ASSERT(fw != NULL);
    ASSERT(mdm != NULL);
    ASSERT(fw_cfg != NULL);

    fw->id = inst_id;
    fw->flashless = mdm->core.flashless;
    fw->cfg = *fw_cfg;

    get_fw_regexp(fw->fw_regexp, mdm->core.name, fw->flashless,
                  mdm->core.hw_revision, mdm->core.sw_revision);

    /* update runtime and injection folders: add instance ID sub-directory */
    if (fw->flashless) {
        char subfolder[10];
        snprintf(subfolder, sizeof(subfolder), "/%d/", inst_id);

        strncat(fw->cfg.folders.injection, subfolder,
                sizeof(fw->cfg.folders.injection) -
                strlen(fw->cfg.folders.injection) - 1);
        /* To ensure a backward compatibility, calib.nvm of first MMGR instance
         * is stored at the root of factory telephony folder */
        if (inst_id != 1)
            strncat(fw->cfg.folders.factory, subfolder,
                    sizeof(fw->cfg.folders.factory) -
                    strlen(fw->cfg.folders.factory) - 1);
        strncat(fw->cfg.folders.runtime, subfolder,
                sizeof(fw->cfg.folders.runtime) -
                strlen(fw->cfg.folders.runtime) - 1);
    }

    if (!folder_exist(fw->cfg.folders.input)) {
        LOG_ERROR("Input folder (%s) does not exist",
                  fw->cfg.folders.input);
        goto err;
    }

    if (mdm->tlvs.nb > 0) {
        fw->tlvs.nb = mdm->tlvs.nb;
        fw->tlvs.tlv = malloc(sizeof(tlv_info_t) * mdm->tlvs.nb);
        ASSERT(fw->tlvs.tlv != NULL);
        for (size_t i = 0; i < mdm->tlvs.nb; i++)
            snprintf(fw->tlvs.tlv[i].filename, sizeof(fw->tlvs.tlv[i].filename),
                     "%s/%s", fw->cfg.folders.input, mdm->tlvs.tlv[i].filename);

    }

    fw->blob_path = mdm_fw_compute_path(fw->cfg.folders.input, "hash");
    fw->fw_packaged_path = mdm_fw_compute_path(fw->cfg.folders.injection,
                                               "fw.fls");
    fw->nvm_dyn_path = mdm_fw_compute_path(fw->cfg.folders.runtime,
                                           fw->cfg.nvm.dyn);
    fw->nvm_sta_path = mdm_fw_compute_path(fw->cfg.folders.runtime,
                                           fw->cfg.nvm.sta);

    mdm_fw_dbg_init(fw);

    return (mdm_fw_hdle_t *)fw;

err:
    mdm_fw_dispose((mdm_fw_hdle_t *)fw);
    return NULL;
}

/**
 * Disposes the module
 *
 * @param hdle module handle
 */
void mdm_fw_dispose(mdm_fw_hdle_t *hdle)
{
    mdm_fw_t *fw = (mdm_fw_t *)hdle;

    ASSERT(fw != NULL);

    mdm_fw_dbg_dispose(fw);
    free(fw->tlvs.tlv);
    free(fw->blob_path);
    free(fw->fw_file);
    free(fw->fw_packaged_path);
    free(fw->rnd_path);
    free(fw->nvm_dyn_path);
    free(fw->nvm_sta_path);
    free(fw);
}
