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

#include "data_to_msg.h"
#include "logs.h"

/**
 * handle E_MMGR_RESPONSE_MODEM_HW_ID message allocation
 *
 * @param [in,out] msg data to send
 * @param [in] request data to send
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t set_msg_modem_hw_id(msg_t *msg, mmgr_cli_event_t *request)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    size_t size;
    mmgr_cli_hw_id_t *hw = NULL;
    char *msg_data = NULL;

    ASSERT(msg != NULL);
    ASSERT(request != NULL);

    hw = request->data;

    /* msg->hdr.len is used to provide string lengh */
    size = sizeof(char) * hw->len;
    ret = msg_prepare(msg, &msg_data, E_MMGR_RESPONSE_MODEM_HW_ID, &size);
    if (ret == E_ERR_SUCCESS)
        memcpy(msg_data, hw->id, sizeof(char) * hw->len);

    return ret;
}

/**
 * handle E_MMGR_RESPONSE_FUSE_INFO message allocation
 *
 * @param [in,out] msg data to send
 * @param [in] request data to send
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t set_msg_fuse_info(msg_t *msg, mmgr_cli_event_t *request)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    size_t size;
    mmgr_cli_fuse_info_t *fuse = NULL;
    char *msg_data = NULL;

    ASSERT(msg != NULL);
    ASSERT(request != NULL);

    fuse = request->data;

    /* msg->hdr.len is used to provide string lengh */
    size = sizeof(char) * FUSE_LEN;
    ret = msg_prepare(msg, &msg_data, E_MMGR_RESPONSE_FUSE_INFO, &size);
    if (ret == E_ERR_SUCCESS)
        memcpy(msg_data, fuse->id, sizeof(char) * FUSE_LEN);

    return ret;
}

/**
 * handle E_MMGR_NOTIFY_AP_RESET message allocation
 *
 * @param [in,out] msg data to send
 * @param [in] request data to send
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t set_msg_ap_reset(msg_t *msg, mmgr_cli_event_t *request)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    size_t size;
    mmgr_cli_internal_ap_reset_t *ap = NULL;
    char *msg_data = NULL;

    ASSERT(msg != NULL);
    ASSERT(request != NULL);

    ap = request->data;

    size = sizeof(uint32_t) + ap->len + ap->extra_len;
    ret = msg_prepare(msg, &msg_data, E_MMGR_NOTIFY_AP_RESET, &size);
    if (ret == E_ERR_SUCCESS) {
        serialize_size_t(&msg_data, ap->len);
        memcpy(msg_data, ap->name, sizeof(char) * ap->len);
        if (ap->extra_len)
            /* Note that 'extra_data' is already serialized (as it's the raw
             * data from the client recovery request) so can be copied 'as is'
             */
            memcpy(msg_data + ap->len, ap->extra_data,
                   sizeof(char) * ap->extra_len);
    }

    return ret;
}

/**
 * handle E_MMGR_RESPONSE_MODEM_FW_RESULT message allocation
 *
 * @param [in,out] msg data to send
 * @param [in] request data to send
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t set_msg_modem_fw_result(msg_t *msg, mmgr_cli_event_t *request)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    uint32_t tmp;
    size_t size;
    mmgr_cli_fw_update_result_t *result = NULL;
    char *msg_data = NULL;

    ASSERT(msg != NULL);
    ASSERT(request != NULL);

    result = request->data;

    size = sizeof(uint32_t);
    ret = msg_prepare(msg, &msg_data, E_MMGR_RESPONSE_MODEM_FW_RESULT, &size);
    if (ret == E_ERR_SUCCESS) {
        memcpy(&tmp, &result->id, sizeof(e_modem_fw_error_t));
        serialize_uint32(&msg_data, tmp);
    }

    return ret;
}

/**
 * handle E_MMGR_RESPONSE_MODEM_NVM_RESULT message allocation
 *
 * @param [in,out] msg data to send
 * @param [in] request data to send
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t set_msg_modem_nvm_result(msg_t *msg, mmgr_cli_event_t *request)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    uint32_t tmp;
    size_t size;
    mmgr_cli_nvm_update_result_t *result = NULL;
    char *msg_data = NULL;

    ASSERT(msg != NULL);
    ASSERT(request != NULL);

    result = request->data;

    size = sizeof(uint32_t) + sizeof(int);
    ret = msg_prepare(msg, &msg_data, E_MMGR_RESPONSE_MODEM_NVM_RESULT, &size);
    if (ret == E_ERR_SUCCESS) {
        memcpy(&tmp, &result->id, sizeof(e_modem_fw_error_t));
        serialize_uint32(&msg_data, tmp);
        serialize_int(&msg_data, result->sub_error_code);
    }

    return ret;
}

/**
 * handle E_MMGR_NOTIFY_CORE_DUMP_COMPLETE message allocation
 *
 * @param [in,out] msg data to send
 * @param [in] request data to send
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t set_msg_core_dump(msg_t *msg, mmgr_cli_event_t *request)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    uint32_t tmp;
    size_t size;
    mmgr_cli_core_dump_t *cd = NULL;
    char *msg_data = NULL;

    ASSERT(msg != NULL);
    ASSERT(request != NULL);

    cd = request->data;

    /* this structure is composed of 5 elements: 3 integers and two string */
    size = 3 * sizeof(uint32_t) +
           sizeof(char) * (cd->path_len + cd->reason_len);
    ret = msg_prepare(msg, &msg_data, E_MMGR_NOTIFY_CORE_DUMP_COMPLETE, &size);
    if (ret == E_ERR_SUCCESS) {
        memcpy(&tmp, &cd->state, sizeof(cd->state));
        serialize_uint32(&msg_data, tmp);
        serialize_size_t(&msg_data, cd->path_len);
        serialize_size_t(&msg_data, cd->reason_len);
        memcpy(msg_data, cd->path, sizeof(char) * cd->path_len);
        if (cd->reason_len > 0)
            memcpy(msg_data + cd->path_len, cd->reason,
                   sizeof(char) * cd->reason_len);
    }

    return ret;
}

/**
 * handle E_MMGR_NOTIFY_TFT_EVENT message allocation
 *
 * @param [in,out] msg data to send
 * @param [in] request data to send
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t set_msg_tft_event(msg_t *msg, mmgr_cli_event_t *request)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    size_t size;
    mmgr_cli_tft_event_t *ev = NULL;
    char *msg_data = NULL;

    ASSERT(msg != NULL);
    ASSERT(request != NULL);

    ev = request->data;

    /* this structure is composed of 5 elements: 4 integers and a string
     * an array of a structure composed of one integer and a string can be added
     */
    size = 4 * sizeof(uint32_t) + sizeof(char) * ev->name_len;
    for (size_t i = 0; i < ev->num_data; i++) {
        size += sizeof(uint32_t) + sizeof(char) * ev->data[i].len;
    }

    ret = msg_prepare(msg, &msg_data, E_MMGR_NOTIFY_TFT_EVENT, &size);
    if (ret == E_ERR_SUCCESS) {
        serialize_int(&msg_data, ev->type);
        serialize_size_t(&msg_data, ev->name_len);
        memcpy(msg_data, ev->name, sizeof(char) * ev->name_len);
        msg_data += ev->name_len;
        serialize_int(&msg_data, ev->log);
        serialize_size_t(&msg_data, ev->num_data);

        ASSERT(ev->num_data <= MMGR_CLI_MAX_TFT_EVENT_DATA);

        for (size_t i = 0; i < ev->num_data; i++) {
            ASSERT(ev->data[i].len < MMGR_CLI_MAX_RECOVERY_CAUSE_LEN);
            serialize_size_t(&msg_data, ev->data[i].len);
            memcpy(msg_data, ev->data[i].value, sizeof(char) * ev->data[i].len);
            msg_data += ev->data[i].len;
        }
    }

    return ret;
}
