/* Modem Manager - keys source file
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


#include <cutils/properties.h>
#include <stdio.h>
#include <stdlib.h>
#include "errors.h"
#include "key.h"
#include "logs.h"

#define MDM_VERSION_KEY "gsm.version.baseband"

typedef struct key_ctx {
    char blob[PROPERTY_KEY_MAX];
    char cfg[PROPERTY_KEY_MAX];
    char amtl[PROPERTY_KEY_MAX];
    char reboot[PROPERTY_KEY_MAX];
    char mdm_version[PROPERTY_KEY_MAX];
} key_ctx_t;

key_hdle_t *key_init(size_t inst_id)
{
    key_ctx_t *key = calloc(1, sizeof(key_ctx_t));

    ASSERT(key != NULL);

    snprintf(key->blob, sizeof(key->blob), "persist.sys.mmgr%d.blob_hash",
             inst_id);
    snprintf(key->cfg, sizeof(key->cfg), "persist.sys.mmgr%d.config_hash",
             inst_id);
    snprintf(key->reboot, sizeof(key->reboot), "persist.sys.mmgr%d.reboot",
             inst_id);

    snprintf(key->amtl, sizeof(key->amtl), "service.amtl%d.cfg", inst_id);

    if (1 == inst_id)
        snprintf(key->mdm_version, sizeof(key->mdm_version), "%s",
                 MDM_VERSION_KEY);
    else
        snprintf(key->mdm_version, sizeof(key->mdm_version), "%s%d",
                 MDM_VERSION_KEY, inst_id);

    return (key_hdle_t *)key;
}

void key_dispose(key_hdle_t *hdle)
{
    free(hdle);
}

const char *key_get_blob(const key_hdle_t *hdle)
{
    key_ctx_t *key = (key_ctx_t *)hdle;

    ASSERT(key != NULL);

    return key->blob;
}

const char *key_get_cfg(const key_hdle_t *hdle)
{
    key_ctx_t *key = (key_ctx_t *)hdle;

    ASSERT(key != NULL);

    return key->cfg;
}

const char *key_get_reboot_counter(const key_hdle_t *hdle)
{
    key_ctx_t *key = (key_ctx_t *)hdle;

    ASSERT(key != NULL);

    return key->reboot;
}

const char *key_get_amtl(const key_hdle_t *hdle)
{
    key_ctx_t *key = (key_ctx_t *)hdle;

    ASSERT(key != NULL);

    return key->amtl;
}

const char *key_get_mdm_version(const key_hdle_t *hdle)
{
    key_ctx_t *key = (key_ctx_t *)hdle;

    ASSERT(key != NULL);

    return key->mdm_version;
}
