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
#include <string.h>
#include "logs.h"
#include "client_cnx.h"
#include "msg_format.h"

/**
 * deserialize uint32 data from buffer
 *
 * @param [in,out] buffer data received. the address is shifted of read size
 * @param [out] value data extracted
 *
 * @return none
 */
void deserialize_uint32(char **buffer, uint32_t *value)
{
    memcpy(value, *buffer, sizeof(uint32_t));
    *value = ntohl(*value);
    *buffer += sizeof(uint32_t);
}

/**
 * deserialize int data from buffer
 *
 * @param [in,out] buffer data received. the address is shifted of read size
 * @param [out] value data extracted
 *
 * @return none
 */
void deserialize_int(char **buffer, int *value)
{
    uint32_t tmp = 0;

    deserialize_uint32(buffer, &tmp);
    memcpy(value, &tmp, sizeof(int));
}

/**
 * deserialize size_t data from buffer
 *
 * @param [in,out] buffer data received. the address is shifted of read size
 * @param [out] value data extracted
 *
 * @return none
 */
void deserialize_size_t(char **buffer, size_t *value)
{
    uint32_t tmp = 0;

    deserialize_uint32(buffer, &tmp);
    memcpy(value, &tmp, sizeof(size_t));
}


/**
 * serialize uint32_t data to buffer
 *
 * @param [out] buffer output buffer
 * @param [in] value size_t value to serialize
 *
 * @return none
 */
void serialize_uint32(char **buffer, uint32_t value)
{
    value = htonl(value);
    memcpy(*buffer, &value, sizeof(uint32_t));
    *buffer += sizeof(uint32_t);
}

/**
 * serialize int data to buffer
 *
 * @param [out] buffer output buffer
 * @param [in] value size_t value to serialize
 *
 * @return none
 */
void serialize_int(char **buffer, int value)
{
    uint32_t tmp;

    memcpy(&tmp, &value, sizeof(int));
    serialize_uint32(buffer, tmp);
}

/**
 * serialize size_t data to buffer
 *
 * @param [out] buffer output buffer
 * @param [in] value size_t value to serialize
 *
 * @return none
 */
void serialize_size_t(char **buffer, size_t value)
{
    uint32_t tmp;

    memcpy(&tmp, &value, sizeof(size_t));
    serialize_uint32(buffer, tmp);
}

/**
 * set header
 *
 * @param [in,out] msg received message
 *
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t msg_set_header(msg_t *msg)
{
    struct timeval ts;
    uint32_t tmp;
    char *msg_data = NULL;

    ASSERT(msg != NULL);

    msg_data = msg->data;

    /* setting id */
    serialize_uint32(&msg_data, msg->hdr.id);

    /* setting timestamp */
    gettimeofday(&ts, NULL);
    memcpy(&tmp, &ts.tv_sec, sizeof(ts.tv_sec));
    serialize_uint32(&msg_data, tmp);

    /* setting size */
    serialize_uint32(&msg_data, msg->hdr.len);

    return E_ERR_SUCCESS;
}

/**
 * read data from cnx and extract header
 *
 * @param [in] fd cnx file descriptor
 * @param [out] hdr message header
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_DISCONNECTED if client is disconnected
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t msg_get_header(int fd, msg_hdr_t *hdr)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    char buffer[SIZE_HEADER];
    size_t len = SIZE_HEADER;
    char *p = buffer;

    ASSERT(hdr != NULL);

    if ((ret = cnx_read(fd, buffer, &len)) != E_ERR_SUCCESS)
        goto out;

    if (len == 0) {
        ret = E_ERR_DISCONNECTED;
        LOG_DEBUG("client disconnected");
        goto out;
    } else if (len < SIZE_HEADER) {
        ret = E_ERR_FAILED;
        LOG_ERROR("Invalid message. Header is missing");
        goto out;
    }

    /* extract request id */
    deserialize_uint32(&p, &hdr->id);

    /* extract timestamp */
    deserialize_uint32(&p, &hdr->ts);

    /* extract data len */
    deserialize_uint32(&p, &hdr->len);

    if (hdr->len < (len - SIZE_HEADER))
        LOG_ERROR("Invalid message. Bad buffer len");
    else
        ret = E_ERR_SUCCESS;

out:
    return ret;
}

/**
 * free message data
 *
 * @param [in] msg data to send
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t msg_delete(msg_t *msg)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;

    ASSERT(msg != NULL);

    if (msg->data != NULL)
        free(msg->data);
    else
        ret = E_ERR_FAILED;

    return ret;
}

/**
 * set buffer to send empty message
 *
 * @param [out] msg data to send
 * @param [in] request request to send
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t msg_set_empty(msg_t *msg, mmgr_cli_event_t *request)
{
    size_t size = 0;
    char *msg_data = NULL;

    ASSERT(msg != NULL);
    ASSERT(request != NULL);

    return msg_prepare(msg, &msg_data, request->id, &size);
}

/**
 * handle message allocation memory and set message header
 *
 * @private
 *
 * @param [in,out] msg data to send
 * @param [in] msg_data data to send
 * @param [in] id message id
 * @param [in] size data size
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t msg_prepare(msg_t *msg, char **msg_data,
                            e_mmgr_events_t id, size_t *size)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    size_t len;

    ASSERT(msg != NULL);
    ASSERT(msg_data != NULL);

    len = SIZE_HEADER + *size;
    msg->data = calloc(len, sizeof(char));
    if (msg->data == NULL) {
        LOG_ERROR("memory allocation fails");
    } else {
        memcpy(&msg->hdr.id, &id, sizeof(id));
        memcpy(&msg->hdr.len, size, sizeof(size_t));
        ret = msg_set_header(msg);
        *size = len;
        *msg_data = msg->data + SIZE_HEADER;
    }

    return ret;
}
