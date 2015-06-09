/* Modem Manager - data to message source file
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

#include <stdlib.h>
#include <string.h>
#include "msg_to_data.h"
#include "logs.h"

/**
 * set client structure for empty messages
 *
 * @param [in] event data to send to client
 * @param [out] msg data to send
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t set_data_empty(msg_t *msg, mmgr_cli_event_t *event)
{
    ASSERT(msg != NULL);
    ASSERT(event != NULL);

    (void)msg;  /* unused */

    event->data = NULL;
    event->len = 0;

    return E_ERR_SUCCESS;
}

/**
 * set client structure for RESPONSE_FUSE_INFO message
 *
 * @param [in,out] msg data received
 * @param [in] request data to provide to MMGR client
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t set_data_fuse_info(msg_t *msg, mmgr_cli_event_t *request)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    size_t len;
    mmgr_cli_fuse_info_t *fuse = NULL;

    ASSERT(msg != NULL);
    ASSERT(request != NULL);

    memcpy(&len, &msg->hdr.len, sizeof(uint32_t));
    /* the buffer will be freed by the matching freed function */
    fuse = malloc(sizeof(mmgr_cli_fuse_info_t));
    if (fuse == NULL) {
        LOG_ERROR("memory allocation fails");
    } else {
        memcpy(fuse->id, msg->data, len);
        ret = E_ERR_SUCCESS;
    }

    request->data = fuse;
    return ret;
}

/**
 * set client structure for RESPONSE_MODEM_HW_ID message
 *
 * @param [in,out] msg data received
 * @param [in] request data to provide to MMGR client
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t set_data_hw_id(msg_t *msg, mmgr_cli_event_t *request)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    mmgr_cli_hw_id_t *hw = NULL;

    ASSERT(msg != NULL);
    ASSERT(request != NULL);

    /* calloc is used to be sure that id is NULL the buffer will be freed by
     * the matching freed function */
    hw = calloc(1, sizeof(mmgr_cli_hw_id_t));
    if (hw == NULL) {
        LOG_ERROR("memory allocation fails");
    } else {
        memcpy(&hw->len, &msg->hdr.len, sizeof(size_t));

        /* the buffer will be freed by the matching freed function */
        hw->id = malloc((hw->len + 1) * sizeof(char));
        if (hw->id == NULL) {
            LOG_ERROR("memory allocation fails");
        } else {
            memcpy(hw->id, msg->data, hw->len);
            memset(hw->id + hw->len, '\0', sizeof(char));
            ret = E_ERR_SUCCESS;
        }
    }

    request->data = hw;
    return ret;
}

/**
 * extract data from E_MMGR_NOTIFY_AP_RESET message
 *
 * @param [in] msg message received
 * @param [in] request
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t set_data_ap_reset(msg_t *msg, mmgr_cli_event_t *request)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    mmgr_cli_ap_reset_t *ap = NULL;
    mmgr_cli_recovery_cause_t *causes = NULL;
    char *msg_data = NULL, *name;
    size_t len_name, num_cause_entries = 0;

    ASSERT(msg != NULL);
    ASSERT(request != NULL);

    msg_data = msg->data;

    deserialize_size_t(&msg_data, &len_name);
    name = malloc((len_name + 1) * sizeof(char));
    if (name == NULL) {
        LOG_ERROR("memory allocation failed for name");
        goto out;
    }
    memcpy(name, msg_data, len_name);
    name[len_name] = '\0';

    msg_data += len_name;

    if (msg->hdr.len > (len_name + sizeof(size_t)))
        /* Extra information is present */
        deserialize_size_t(&msg_data, &num_cause_entries);

    ap = malloc(sizeof(mmgr_cli_ap_reset_t));
    if (ap == NULL) {
        LOG_ERROR("memory allocation failed for msg");
        free(name);
        goto out;
    }
    causes = malloc(num_cause_entries * sizeof(mmgr_cli_recovery_cause_t));
    if (causes == NULL) {
        LOG_ERROR("memory allocation failed for cause array");
        num_cause_entries = 0;
    }
    ap->len = len_name;
    ap->name = name;
    ap->num_causes = num_cause_entries;
    ap->recovery_causes = causes;

    for (size_t i = 0; i < num_cause_entries; i++) {
        deserialize_size_t(&msg_data, &ap->recovery_causes[i].len);
        ap->recovery_causes[i].cause =
            malloc((ap->recovery_causes[i].len + 1) * sizeof(char));
        if (ap->recovery_causes[i].cause == NULL) {
            LOG_ERROR("memory allocation failed for cause %d", i);
            ap->num_causes = i;
            break;
        }
        memcpy(ap->recovery_causes[i].cause, msg_data,
               ap->recovery_causes[i].len);
        ap->recovery_causes[i].cause[ap->recovery_causes[i].len] = '\0';
        msg_data += ap->recovery_causes[i].len;
    }

    ret = E_ERR_SUCCESS;

out:
    request->data = ap;
    return ret;
}

/**
 * extract data from E_MMGR_NOTIFY_CORE_DUMP_COMPLETE message
 *
 * @param [in] msg message received
 * @param [in] request
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t set_data_core_dump(msg_t *msg, mmgr_cli_event_t *request)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    mmgr_cli_core_dump_t *cd = NULL;
    char *msg_data = NULL;
    uint32_t tmp = 0;

    ASSERT(msg != NULL);
    ASSERT(request != NULL);

    /* this structure is composed of 4 elements: 3 integer and a string */
    if (msg->hdr.len < (3 * sizeof(uint32_t))) {
        LOG_ERROR("mandatory data is missing");
        goto out;
    }

    /* calloc is used to be sure that path is NULL the buffer will be freed by
     * the matching freed function */
    cd = calloc(1, sizeof(mmgr_cli_core_dump_t));
    if (cd == NULL) {
        LOG_ERROR("memory allocation failed");
        goto out;
    }
    msg_data = msg->data;
    deserialize_uint32(&msg_data, &tmp);
    memcpy(&cd->state, &tmp, sizeof(cd->state));
    deserialize_size_t(&msg_data, &cd->path_len);
    deserialize_size_t(&msg_data, &cd->reason_len);

    if ((cd->path_len + cd->reason_len) !=
        (msg->hdr.len - (msg_data - msg->data))) {
        LOG_ERROR("bad string length");
        goto out;
    }

    /* the buffer will be freed by the matching freed function */
    cd->path = malloc((cd->path_len + 1) * sizeof(char));
    if (cd->path == NULL) {
        LOG_ERROR("memory allocation failed");
        goto out;
    }
    memcpy(cd->path, msg_data, cd->path_len);
    memset(cd->path + cd->path_len, '\0', sizeof(char));

    if (cd->reason_len > 0) {
        cd->reason = malloc((cd->reason_len + 1) * sizeof(char));
        if (cd->reason == NULL) {
            LOG_ERROR("memory allocation failed");
            goto out;
        }
        memcpy(cd->reason, msg_data + cd->path_len, cd->reason_len);
        memset(cd->reason + cd->reason_len, '\0', sizeof(char));
    } else {
        cd->reason = NULL;
    }

    ret = E_ERR_SUCCESS;

out:
    request->data = cd;
    return ret;
}

/**
 * extract data from E_MMGR_NOTIFY_TFT_EVENT message
 *
 * @param [in] msg message received
 * @param [in] request
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t set_data_tft_event(msg_t *msg, mmgr_cli_event_t *request)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    mmgr_cli_tft_event_t *ev = NULL;
    mmgr_cli_tft_event_data_t *data = NULL;
    char *msg_data = NULL, *name;

    ASSERT(msg != NULL);
    ASSERT(request != NULL);

    /* this structure is composed at least of 5 elements: 4 integer and a string
    **/
    if (msg->hdr.len < (4 * sizeof(uint32_t))) {
        LOG_ERROR("mandatory data is missing");
        goto out;
    }

    /* calloc is used to be sure that name is NULL the buffer will be freed by
     * the matching freed function */
    ev = calloc(1, sizeof(mmgr_cli_tft_event_t));
    if (ev == NULL) {
        LOG_ERROR("memory allocation failed");
        goto out;
    }

    msg_data = msg->data;

    deserialize_int(&msg_data, (int *)&ev->type);
    deserialize_size_t(&msg_data, &ev->name_len);
    name = malloc((ev->name_len + 1) * sizeof(char));
    if (name == NULL) {
        LOG_ERROR("memory allocation failed for name");
        goto out;
    }
    memcpy(name, msg_data, ev->name_len);
    name[ev->name_len] = '\0';
    ev->name = name;

    msg_data += ev->name_len;

    deserialize_int(&msg_data, &ev->log);
    deserialize_size_t(&msg_data, &ev->num_data);

    if (ev->num_data > 0) {
        /* calloc is used to be sure that values are NULL the buffer will be
         * freed by
         * the matching freed function */
        data = calloc(ev->num_data, sizeof(mmgr_cli_tft_event_data_t));
        if (data == NULL) {
            LOG_ERROR("memory allocation failed for data array");
            goto out;
        }
        ev->data = data;

        for (size_t i = 0; i < ev->num_data; i++) {
            char *value;
            deserialize_size_t(&msg_data, &data[i].len);

            value = malloc((data[i].len + 1) * sizeof(char));
            if (value == NULL) {
                LOG_ERROR("memory allocation failed for data %d", i);
                goto out;
            }
            memcpy(value, msg_data, data[i].len);
            value[data[i].len] = '\0';
            data[i].value = value;
            msg_data += data[i].len;
        }
    }

    ret = E_ERR_SUCCESS;

out:
    request->data = ev;
    return ret;
}

/**
 * set client structure for RESPONSE_MODEM_FW_RESULT message
 *
 * @param [in,out] msg data received
 * @param [in] request data to provide to MMGR client
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t set_data_fw_result(msg_t *msg, mmgr_cli_event_t *request)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    size_t len;
    mmgr_cli_fw_update_result_t *result = NULL;
    uint32_t tmp = 0;
    char *msg_data = NULL;

    ASSERT(msg != NULL);
    ASSERT(request != NULL);

    memcpy(&len, &msg->hdr.len, sizeof(uint32_t));
    if (len != sizeof(e_modem_fw_error_t)) {
        LOG_ERROR("bad message size");
    } else {
        /* the buffer will be freed by the matching freed function */
        result = malloc(sizeof(mmgr_cli_fw_update_result_t));
        if (result == NULL) {
            LOG_ERROR("memory allocation fails");
        } else {
            msg_data = msg->data;
            deserialize_uint32(&msg_data, &tmp);
            memcpy(&result->id, &tmp, sizeof(e_modem_fw_error_t));
            ret = E_ERR_SUCCESS;
        }
    }

    request->data = result;
    return ret;
}

/**
 * set client structure for E_MMGR_RESPONSE_MODEM_NVM_RESULT message
 *
 * @param [in,out] msg data received
 * @param [in] request data to provide to MMGR client
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t set_data_nvm_result(msg_t *msg, mmgr_cli_event_t *request)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    size_t len;
    mmgr_cli_nvm_update_result_t *result = NULL;
    uint32_t tmp = 0;
    char *msg_data = NULL;

    ASSERT(msg != NULL);
    ASSERT(request != NULL);

    memcpy(&len, &msg->hdr.len, sizeof(uint32_t));
    if (len != (sizeof(e_modem_nvm_error_t) + sizeof(int))) {
        LOG_ERROR("bad message size");
    } else {
        /* the buffer will be freed by the matching freed function */
        result = malloc(sizeof(mmgr_cli_nvm_update_result_t));
        if (result == NULL) {
            LOG_ERROR("memory allocation fails");
        } else {
            msg_data = msg->data;
            deserialize_uint32(&msg_data, &tmp);
            memcpy(&result->id, &tmp, sizeof(e_modem_nvm_error_t));
            deserialize_int(&msg_data, &result->sub_error_code);
            ret = E_ERR_SUCCESS;
        }
    }

    request->data = result;
    return ret;
}

/**
 * free client structure for message with one element structure
 *
 * @param [in] request data to provide to MMGR client
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t free_one_element_struct(mmgr_cli_event_t *request)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;
    void *p = NULL;

    ASSERT(request != NULL);

    p = request->data;
    if (p != NULL) {
        free(p);
    } else {
        LOG_ERROR("failed to free memory");
        ret = E_ERR_FAILED;
    }

    return ret;
}

/**
 * free client structure for message RESPONSE_MODEM_HW_ID message
 *
 * @param [in] request data to delete
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t free_data_hw_id(mmgr_cli_event_t *request)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;
    mmgr_cli_hw_id_t *hw = NULL;

    ASSERT(request != NULL);

    hw = request->data;
    if (hw != NULL) {
        if (hw->id != NULL)
            free(hw->id);
        free(hw);
    } else {
        LOG_ERROR("failed to free memory");
        ret = E_ERR_FAILED;
    }

    return ret;
}

/**
 * free client structure for message CORE_DUMP_COMPLETE message
 *
 * @param [in] request data to delete
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t free_data_core_dump(mmgr_cli_event_t *request)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;
    mmgr_cli_core_dump_t *cd = NULL;

    ASSERT(request != NULL);

    cd = request->data;
    if (cd != NULL) {
        if (cd->path != NULL)
            free(cd->path);
        if (cd->reason != NULL)
            free(cd->reason);
        free(cd);
    } else {
        LOG_ERROR("failed to free memory");
        ret = E_ERR_FAILED;
    }

    return ret;
}

/**
 * free client structure for message NOTIFY_AP_RESET message
 *
 * @param [in] request data to delete
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t free_data_ap_reset(mmgr_cli_event_t *request)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;
    mmgr_cli_ap_reset_t *ap_rst = NULL;

    ASSERT(request != NULL);

    ap_rst = request->data;
    if (ap_rst != NULL) {
        free(ap_rst->name);
        if (ap_rst->recovery_causes) {
            for (size_t i = 0; i < ap_rst->num_causes; i++)
                free(ap_rst->recovery_causes[i].cause);
            free(ap_rst->recovery_causes);
        }
        free(ap_rst);
    } else {
        LOG_ERROR("failed to free memory");
        ret = E_ERR_FAILED;
    }

    return ret;
}

/**
 * free client structure for message NOTIFY_TFT_EVENT message
 *
 * @param [in] request data to delete
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t free_data_tft_event(mmgr_cli_event_t *request)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;
    mmgr_cli_tft_event_t *ev = NULL;

    ASSERT(request != NULL);

    ev = request->data;
    if (ev != NULL) {
        if (ev->name != NULL)
            free((char *)ev->name);
        if (ev->data != NULL) {
            for (size_t i = 0; i < ev->num_data; i++) {
                if (ev->data[i].value != NULL)
                    free((char *)ev->data[i].value);
            }
            free(ev->data);
        }
        free(ev);
    } else {
        LOG_ERROR("failed to free memory");
        ret = E_ERR_FAILED;
    }

    return ret;
}

/**
 * free client structure for message empty data message
 *
 * @param [in] event unused param
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t free_data_empty(mmgr_cli_event_t *event)
{
    (void)event;                /* unused */
    return E_ERR_SUCCESS;
}
