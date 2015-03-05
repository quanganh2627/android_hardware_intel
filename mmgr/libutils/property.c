/* Modem Manager - Android property source file
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
#include <stdio.h>
#include <string.h>
#include "errors.h"
#include "logs.h"
#include "property.h"

/**
 * Store the value in an Android property
 *
 * @param [in] key property key
 * @param [in] value value to set
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED if not
 */
e_mmgr_errors_t property_set_int(const char *key, int value)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    char write_value[PROPERTY_VALUE_MAX];

    ASSERT(key != NULL);

    snprintf(write_value, sizeof(write_value), "%d", value);

    if (property_set(key, write_value) == 0) {
        LOG_DEBUG("%s: %s", key, write_value);
        ret = E_ERR_SUCCESS;
    } else {
        LOG_ERROR("Set property failed %s ", strerror(errno));
    }

    return ret;
}

/**
 * Get the value from an Android property
 * If the key doesn't exist, the default value is returned: 0
 *
 * @param [in] key property key
 * @param [out] value read value
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED if not
 */
e_mmgr_errors_t property_get_int(const char *key, int *value)
{
    char read_value[PROPERTY_VALUE_MAX];
    e_mmgr_errors_t ret = E_ERR_SUCCESS;

    ASSERT(key != NULL);
    ASSERT(value != NULL);

    property_get(key, read_value, "0");
    if (sscanf(read_value, "%d", value) != 1) {
        LOG_ERROR("conversion failed. Set to default value: 0");
        *value = 0;
        ret = E_ERR_FAILED;
    }

    LOG_DEBUG("%s: %d", key, *value);

    return ret;
}

/**
 * Get the value from an Android property
 * If the key doesn't exist, the default value is returned: ""
 *
 * @param [in] key property key
 * @param [out] value read value
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED if not
 */
e_mmgr_errors_t property_get_string(const char *key, char *value)
{
    char read_value[PROPERTY_VALUE_MAX];
    int len;

    ASSERT(key != NULL);
    ASSERT(value != NULL);

    memset(read_value, 0, PROPERTY_VALUE_MAX);
    property_get(key, read_value, "");
    len = strnlen(read_value, PROPERTY_VALUE_MAX - 1);
    strncpy(value, read_value, len);
    value[len] = '\0';

    LOG_DEBUG("%s: %s", key, value);

    return E_ERR_SUCCESS;
}
