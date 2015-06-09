/* Modem Manager - data to message source file
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

#include <arpa/inet.h>
#include "logs.h"
#include "data_to_msg.h"

/**
 * handle SET_NAME message allocation
 *
 * @param [out] msg data to send
 * @param [in] request request to send
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t set_msg_name(msg_t *msg, mmgr_cli_event_t *request)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    size_t size;
    char *msg_data = NULL;

    ASSERT(msg != NULL);
    ASSERT(request != NULL);

    if (request->len <= 0) {
        LOG_ERROR("name is empty");
    } else {
        /* msg.hdr is used to store the string length */
        size = request->len;
        ret = msg_prepare(msg, &msg_data, request->id, &size);
        if (ret == E_ERR_SUCCESS)
            /* set name */
            memcpy(msg->data + SIZE_HEADER, request->data,
                   sizeof(char) * request->len);
    }

    return ret;
}

/**
 * handle SET_EVENTS message allocation
 *
 * @param [out] msg data to send
 * @param [in] request request to send
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t set_msg_filter(msg_t *msg, mmgr_cli_event_t *request)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    uint32_t tmp;
    size_t size;
    char *msg_data = NULL;

    ASSERT(msg != NULL);
    ASSERT(request != NULL);

    size = sizeof(uint32_t);
    ret = msg_prepare(msg, &msg_data, request->id, &size);
    if (ret == E_ERR_SUCCESS) {
        /* set filter */
        memcpy(&tmp, request->data, sizeof(int));
        serialize_uint32(&msg_data, tmp);
    }

    return ret;
}

/**
 * handle REQUEST_MODEM_RESTART message allocation
 *
 * @param [out] msg data to send
 * @param [in] request request to send
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t set_msg_restart(msg_t *msg, mmgr_cli_event_t *request)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    char *msg_data = NULL;
    size_t size = 0;

    ASSERT(msg != NULL);
    ASSERT(request != NULL);

    /* restart is optional */
    mmgr_cli_restart_t *restart = request->data;
    if (restart)
        size = sizeof(uint32_t);

    ret = msg_prepare(msg, &msg_data, request->id, &size);
    if (ret == E_ERR_SUCCESS)
        if (restart)
            serialize_uint32(&msg_data, restart->optional);

    return ret;
}

/**
 * handle REQUEST_MODEM_RECOVERY message allocation
 *
 * @param [out] msg data to send
 * @param [in] request request to send
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t set_msg_recovery(msg_t *msg, mmgr_cli_event_t *request)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    size_t size = 0, string_size = 0;
    char *msg_data = NULL;
    size_t req_size = 0;
    mmgr_cli_recovery_cause_t *req_extra_data = NULL;

    ASSERT(msg != NULL);
    ASSERT(request != NULL);

    req_extra_data = request->data;
    req_size = request->len;

    /* Some sanity checks */
    if ((req_size > (MMGR_CLI_MAX_RECOVERY_CAUSES *
                     sizeof(mmgr_cli_recovery_cause_t))) ||
        ((req_size % sizeof(mmgr_cli_recovery_cause_t)) != 0)) {
        LOG_ERROR("invalid extra data size (%d), ignoring", req_size);
        req_size = 0;
    }
    if ((req_size != 0) &&
        ((req_extra_data == NULL) ||
         ((((int)req_extra_data) %
           __alignof__(mmgr_cli_recovery_cause_t)) != 0))) {
        LOG_ERROR("invalid extra data pointer (%p), ignoring", req_extra_data);
        req_size = 0;
    }
    req_size /= sizeof(mmgr_cli_recovery_cause_t);
    for (size_t i = 0; i < req_size; i++) {
        if ((req_extra_data[i].len > MMGR_CLI_MAX_RECOVERY_CAUSE_LEN) ||
            (req_extra_data[i].cause == NULL)) {
            LOG_ERROR("invalid extra data entry (index %d, len %d, ptr %p)", i,
                      req_extra_data[i].len, req_extra_data[i].cause);
            req_size = 0;
            break;
        }
        string_size += req_extra_data[i].len;
    }

    if (req_size != 0)
        size = sizeof(size_t) + req_size * sizeof(size_t) + string_size;

    ret = msg_prepare(msg, &msg_data, request->id, &size);
    if (ret == E_ERR_SUCCESS) {
        /* Serialize client request */
        if (req_size != 0) {
            serialize_size_t(&msg_data, req_size);
            for (size_t i = 0; i < req_size; i++) {
                serialize_size_t(&msg_data, req_extra_data[i].len);
                memcpy(msg_data, req_extra_data[i].cause,
                       req_extra_data[i].len);
                msg_data += req_extra_data[i].len;
            }
        }
    }

    return ret;
}
