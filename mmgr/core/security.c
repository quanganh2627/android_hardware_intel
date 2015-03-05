/* Modem Manager - secure source file
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

#include "at.h"
#include "logs.h"
#include "security.h"
#include "tty.h"
#include <dlfcn.h>
#include <resolv.h>

#define AT_SEC_RETRY 4
#define AT_SIZE 1024
#define SECUR_LIB "libdx_cc7.so"
#define SECUR_CALLBACK "secure_channel_callback"

/* @TODO: avoid this dupplication */
#define SECURE_CH_DATA_FREE_RETURN_DATA   0

#define MSG_TYPE_INDX 0
#define MSG_LEN_INDX 2
#define MSG_DATA_INDX 4
#define MSGS_MAX 6
#define AT_SECUR "xsecchannel"
#define MSG_START_STR "+"AT_SECUR ":"
#define MSG_ANSWER_START "+"AT_SECUR "="
#define MAX_TLV_LEN 2 * 1024

typedef struct sec_msg {
    char *message; //buffer containing at+xsecchannel command
    int length;    //length of the at+xsecchannel command
    bool err;      //response to command was ERROR
}sec_msg_t;

typedef struct secure {
    bool enable;
    char dlc[PATH_MAX];
    int fd;
    void *hdle;
    secure_cb_t callback;
    sec_msg_t send_queue[MSGS_MAX];
    int nb_of_msgs;
    const char *err_msg;
} secure_t;

typedef enum {
    OK,
    ERROR,
    NONE
} modem_rsp;

/**
 * read the message provided by the modem
 *
 * @param [in] fd file descriptor
 * @param [in] received buffer containing the message read from modem.
 * @param [out] message buffer containing data from modem
 *                                      should always be freed by the user
 * @param [out] send_id send message id
 * @param [out] req_id request id
 * @param [out] len received length
 *
 * @return E_ERR_FAILED
 * @return E_ERR_SUCCESS
 */
static e_mmgr_errors_t retrieve_data(int fd, char *received, char **message,
                                     int *send_id, int *req_id, int *len)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    int remain = 0;
    int tmp = 0;
    int data_len = 0;
    char *end = NULL;

    ASSERT(received != NULL);
    ASSERT(message != NULL);
    ASSERT(send_id != NULL);
    ASSERT(req_id != NULL);
    ASSERT(len != NULL);

    /* extract sender, request IDs and length. If the pattern is not present in
     * the chunk it will fail. But this should not happen as we should receive
     * it in one chunk */
    received += strlen(MSG_START_STR);
    if (sscanf(received, "%d,%d,%d,", send_id, req_id, len) != 3) {
        LOG_ERROR("Extraction of sender, request IDs and length failed");
        goto out;
    }

    if (*len > MAX_TLV_LEN) {
        LOG_ERROR("TLV size is too high");
        goto out;
    }

    /* extract the data. it begins with " */
    received = strstr(received, "\"");
    if (!received)
        goto out;
    /* remove " character */
    received++;

    /* add +3 (\r, \n, and ") in case we haven't read everything yet */
    *message = malloc(*len + 3);
    if (!*message) {
        LOG_ERROR("memory allocation failed");
        goto out;
    }

    /* check for end of modem message */
    end = strstr(received, "\r");
    if (end)
        data_len = end - received;
    else
        data_len = strlen(received);

    if (data_len > *len)
        data_len = *len;

    memcpy(*message, received, data_len);

    if (data_len != *len) {
        remain = *len - data_len + 3;
        tmp = remain;
        ret = tty_read(fd, *message + data_len, &tmp, AT_READ_MAX_RETRIES);
        if ((ret != E_ERR_SUCCESS) || (tmp != remain))
            goto out;
    }

    memset(*message + *len, '\0', 1);

    ret = E_ERR_SUCCESS;

out:
    return ret;
}

/**
 * decode a message received by the modem
 *
 * @param [in] received data to decode
 * @param [in] rec_len size of received buffer
 * @param [out] type message type
 * @param [out] length length of the converted data
 * @param [out] conv converted data. should be freed by the user
 *
 * @return E_ERR_FAILED
 * @return E_ERR_SUCCESS
 */
static e_mmgr_errors_t decode_data(char *received, int rec_len, uint32_t *type,
                                   uint32_t *length, uint8_t **conv)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    uint8_t *tmp = NULL;
    int len = 0;

    ASSERT(received != NULL);
    ASSERT(type != NULL);
    ASSERT(length != NULL);
    ASSERT(conv != NULL);

    *conv = NULL;
    tmp = calloc(rec_len, sizeof(uint8_t));
    if (!tmp)
        goto out;

    if ((len = b64_pton(received, tmp, rec_len)) < 0)
        goto out;

    /* extract type and length */
    memcpy(length, &len, sizeof(*length));
    memset(type, 0, sizeof(*type));
    memcpy(type, &tmp[MSG_TYPE_INDX], 2);

    *conv = malloc(sizeof(uint8_t) * *length);
    if (*conv) {
        memcpy(*conv, tmp, *length);
        ret = E_ERR_SUCCESS;
    }

out:
    if (ret != E_ERR_SUCCESS)
        LOG_ERROR("operation failed");
    if (tmp)
        free(tmp);
    return ret;
}

/**
 * encode a message received by the modem
 *
 * @param [in] send_id message id
 * @param [in] req_id request id
 * @param [in] src data received by the secur lib
 * @param [in] src_len src length
 * @param [out] send message encoded. should be freed by the user
 * @param [out] send_len message length
 *
 * @return E_ERR_FAILED
 * @return E_ERR_SUCCESS
 */
static e_mmgr_errors_t encode_data(int send_id, int req_id, const uint8_t *src,
                                   uint32_t src_len, char **send, int *send_len)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    char *conv = NULL;
    size_t data_len = 0;
    int conv_len = 0;

    ASSERT(src != NULL);
    ASSERT(send != NULL);

    if (src_len <= 0)
        goto out;

    *send = NULL;
    data_len = src_len * sizeof(char);
    conv_len = data_len * 4 / 3 + 4;

    conv = calloc(conv_len, sizeof(char));
    if (!conv)
        goto out;

    /* convert data */
    conv_len = b64_ntop(src, data_len, conv, conv_len);
    if (conv_len < 0) {
        LOG_ERROR("conversion has failed");
        goto out;
    }

    /* send_id / req_id are on 16 bits they should not use more than 5 bytes
     * conv_len will not be more than 12 bytes */
    *send_len = conv_len + (5 * 2) + 12 +
                (sizeof(char) * (10 + strlen(MSG_ANSWER_START)));
    *send = malloc(sizeof(char) * *send_len);
    if (!*send)
        goto out;

    *send_len = snprintf(*send, *send_len, "at%s%d,%d,%d,\"%s\"\r\n",
                         MSG_ANSWER_START, send_id, req_id, conv_len, conv);
    ret = E_ERR_SUCCESS;

out:
    if (conv)
        free(conv);
    if (ret != E_ERR_SUCCESS)
        LOG_ERROR("operation failed");
    return ret;
}

/**
 * Send message to modem
 *
 * @param [in] secure security handler
 *
 * @return E_ERR_FAILED if failed
 * @return E_ERR_SUCCESS if successful
 */
static e_mmgr_errors_t send_sec_msg(secure_t *secure)
{
    int i = 0;
    e_mmgr_errors_t ret = E_ERR_FAILED;

    ASSERT(secure != NULL);

    for (i = 0; i < AT_SEC_RETRY; i++) {
        if ((ret =
                 tty_write(secure->fd, secure->send_queue[0].message,
                           secure->send_queue[0].length)) == E_ERR_SUCCESS)
            break;
    }

    if (ret != E_ERR_SUCCESS)
        LOG_ERROR("secure channel: Error when sending command to modem");

    return ret;
}

/**
 * Push message to the send queue
 *
 * @param [in] secure security handler
 * @param [in] message "AT" command to be pushed in queue
 * @param [in] len length of command
 *
 * @return E_ERR_FAILED if failed
 * @return E_ERR_SUCCESS if successful
 */
static e_mmgr_errors_t push_msg(secure_t *secure, char *message, int len)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;

    ASSERT(message != NULL);
    ASSERT(secure != NULL);

    if (secure->nb_of_msgs < MSGS_MAX) {
        secure->send_queue[secure->nb_of_msgs].message = malloc(len + 1);
        ASSERT(secure->send_queue[secure->nb_of_msgs].message);
        if (memcpy(secure->send_queue[secure->nb_of_msgs].message,
                   message, len + 1) != NULL) {
            secure->send_queue[secure->nb_of_msgs].length = len;
            secure->send_queue[secure->nb_of_msgs].err = false;
            secure->nb_of_msgs++;
            ret = E_ERR_SUCCESS;
        }
    } else {
        LOG_ERROR("Message can't be added to send list");
    }

    /* Send only the first at+xsecchannel command without waiting for a
     * modem response */
    if (secure->nb_of_msgs == 1) {
        if (send_sec_msg(secure) != E_ERR_SUCCESS) {
            free(secure->send_queue[0].message);
            secure->nb_of_msgs--;
        }
    }

    free(message);

    return ret;
}

/**
 * Remove message from the send queue
 *
 * @param [in] secure security handler
 *
 * @return E_ERR_FAILED if failed
 * @return E_ERR_SUCCESS if successful
 */
static e_mmgr_errors_t pull_msg(secure_t *secure)
{
    ASSERT(secure != NULL);

    e_mmgr_errors_t ret = E_ERR_FAILED;

    if (secure->send_queue[0].message != NULL) {
        free(secure->send_queue[0].message);
        secure->nb_of_msgs--;
        if (secure->nb_of_msgs > 0) {
            int i = 0;
            for (i = 0; i < secure->nb_of_msgs; i++)
                secure->send_queue[i] = secure->send_queue[i + 1];
            send_sec_msg(secure);
        }
        ret = E_ERR_SUCCESS;
    }

    return ret;
}

/**
 * Check modem message for response to previous command
 *
 * @param [in] buffer modem message to be checked
 *
 * @return OK if "OK" string is present in the buffer
 * @return ERROR if "ERROR" string is present in the buffer
 * @return NONE if no modem response in the buffer
 */
static inline modem_rsp check_response(char *buffer)
{
    if (strstr(buffer, "OK")) {
        LOG_INFO("secure channel: received OK response ");
        return OK;
    } else if (strstr(buffer, "ERROR")) {
        LOG_INFO("secure channel: received ERROR response ");
        return ERROR;
    } else {
        return NONE;
    }
}

/**
 * Handle message from modem
 *
 * @param [in] secure security handler
 * @param [in] buffer modem message
 * @param [out] send buffer containing "at+xsecchannel" command for modem
 * @param [out] send_len length of "at+xsecchannel" command
 *
 * @return E_ERR_FAILED if failed
 * @return E_ERR_SUCCESS if successful
 */
static e_mmgr_errors_t handle_message(secure_t *secure, char *buffer,
                                      char **send, int *send_len)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    char *data = NULL;
    uint32_t type = 0;
    uint32_t length = 0;
    uint8_t *conv = NULL;
    uint8_t *p_conv = NULL;
    int rec_len = 0;
    int send_id = 0;
    int req_id = 0;
    int err = 0;

    ASSERT(secure != NULL);
    ASSERT(buffer != NULL);
    ASSERT(send != NULL);
    ASSERT(send_len != NULL);

    ret = retrieve_data(secure->fd, buffer, &data, &send_id, &req_id,
                        &rec_len);
    if ((ret != E_ERR_SUCCESS) || (data == NULL)) {
        LOG_ERROR("secure channel: data retrieval error.");
        goto out;
    }

    ret = decode_data(data, rec_len, &type, &length, &conv);
    if ((ret != E_ERR_SUCCESS) || (conv == NULL)) {
        LOG_ERROR("secure channel: data decode error.");
        goto out;
    }

    /* the secure library will overwrite the pointer. Save the pointer
     * to be able to free the memory */
    p_conv = conv;
    if ((err = secure->callback(&type, &length, &conv)) < 0) {
        LOG_ERROR("secure channel failed with err=%d", err);
        goto out;
    }

    ret = encode_data(send_id, req_id, conv, length, send, send_len);
    if ((ret != E_ERR_SUCCESS) || (send == NULL)) {
        LOG_ERROR("secure channel: data encode error.");
        goto out;
    }

    /* free memory allocation on secure lib side */
    type = SECURE_CH_DATA_FREE_RETURN_DATA;
    secure->callback(&type, NULL, NULL);

    ret = E_ERR_SUCCESS;

out:
    if (data != NULL)
        free(data);
    if (p_conv != NULL)
        free(p_conv);
    return ret;
}

/**
 * Handle event on the security channel
 *
 * @param [in] h security handler
 *
 * @return E_ERR_FAILED if failed
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t secure_event(secure_handle_t *h)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;
    char buffer[AT_SIZE + 1];
    char *message = NULL;
    char *msg_data = NULL;
    int read_size = AT_SIZE;
    int len_data = 0;
    secure_t *secur = (secure_t *)h;

    ASSERT(secur != NULL);

    if (!secur->enable)
        goto out;

    secur->err_msg = NULL;

    for (;; ) {
        read_size = AT_SIZE;
        ret = tty_read(secur->fd, buffer, &read_size, AT_READ_MAX_RETRIES);

        if (ret != E_ERR_SUCCESS) {
            LOG_ERROR(
                "secure channel: Error in tty read");
            break;
        }

        if (read_size > AT_SIZE) {
            LOG_ERROR(
                "secure channel: Modem message bigger than %d bytes",
                AT_SIZE);
            ret = E_ERR_FAILED;
            break;
        } else if (read_size == 0) {
            break;
        }


        buffer[read_size] = '\0';

        LOG_DEBUG("Received: %s", buffer);

        /* Retrieve +XSECCHANNEL data from the modem message and push it into
         * the queue */
        message = strcasestr(buffer, MSG_START_STR);
        while (message != NULL) {
            ret = handle_message(secur, message, &msg_data, &len_data);
            if (ret != E_ERR_FAILED) {
                if (push_msg(secur, msg_data, len_data) != E_ERR_SUCCESS)
                    LOG_ERROR(
                        "secure channel: Error pushing command to send queue");
            } else {
                LOG_ERROR(
                    "secure channel: Error retrieving data from modem message");
            }
            message =
                strcasestr(message + strlen(MSG_START_STR), MSG_START_STR);
        }

        /* Check the modem message for the response to a previous command */
        if (secur->nb_of_msgs > 0) {
            if (check_response(buffer) == OK) {
                pull_msg(secur);
            } else if (check_response(buffer) == ERROR) {
                if (secur->send_queue[0].err == false) {
                    /* If the modem response is "ERROR", the command will be
                     * re-sent one time */
                    LOG_DEBUG("secure channel: Resending the command");
                    send_sec_msg(secur);
                    secur->send_queue[0].err = true;
                } else {
                    /* If the modem response is still "ERROR", inform crashlog
                     * and remove the command from the list */
                    secur->err_msg = "AT+XSECCHANNEL command rejected";
                    LOG_ERROR("secure channel: AT+XSECCHANNEL command rejected");
                    pull_msg(secur);
                }
            }
        }
    }

out:
    return ret;
}

/**
 * register the secur module. It returns the file descriptor
 *
 * @param [in] h security handler
 * @param [out] fd file descriptor to return
 *
 * @return E_ERR_FAILED if failed
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t secure_register(secure_handle_t *h, int *fd)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;
    secure_t *secur = (secure_t *)h;

    ASSERT(secur != NULL);

    secur->fd = CLOSED_FD;
    if (secur->enable)
        ret = tty_open(secur->dlc, &secur->fd);

    *fd = secur->fd;

    return ret;
}

/**
 * Start the security module
 *
 * @param [in] h security handler
 *
 * @return E_ERR_FAILED if failed
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t secure_start(secure_handle_t *h)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;
    static const char const at_cmd[] = "at+" AT_SECUR "?\r";
    secure_t *secur = (secure_t *)h;

    ASSERT(secur != NULL);

    if (secur->enable) {
        LOG_DEBUG("Send of: %s", at_cmd);
        /* The modem doesn't answer OK to this AT command. That's why this
         * function is used */
        ret = tty_write(secur->fd, at_cmd, strlen(at_cmd));
        secur->nb_of_msgs = 0;
    }

    return ret;
}

/**
 * Stop the security module
 *
 * @param [in] h security handler
 *
 * @return E_ERR_FAILED if failed
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t secure_stop(secure_handle_t *h)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;
    secure_t *secur = (secure_t *)h;

    ASSERT(secur != NULL);

    if (secur->enable)
        ret = tty_close(&secur->fd);

    return ret;
}

/**
 * Initialize the security module
 *
 * @param [in] enabled enable or disable the secured channel feature
 * @param [in] ch which channel to use
 *
 * @return a valid secure_handle_t pointer
 * @return NULL otherwise
 */
secure_handle_t *secure_init(bool enabled, channel_t *ch)
{
    char *p = NULL;
    secure_t *secur = NULL;

    ASSERT(ch != NULL);

    secur = calloc(1, sizeof(secure_t));
    if (!secur) {
        LOG_ERROR("memory allocation failed");
        goto err;
    }

    secur->fd = CLOSED_FD;
    secur->enable = enabled;

    if (enabled) {
        /* @TODO: This code should be udpated if the channel is not a DLC */
        strncpy(secur->dlc, ch->device, sizeof(secur->dlc) - 1);

        secur->hdle = dlopen(SECUR_LIB, RTLD_LAZY);
        if (secur->hdle == NULL) {
            LOG_ERROR("failed to open library");
            dlerror();
            goto err;
        }

        secur->callback = dlsym(secur->hdle, SECUR_CALLBACK);

        p = (char *)dlerror();
        if (p != NULL) {
            LOG_ERROR("An error ocurred during symbol resolution");
            dlclose(secur->hdle);
            secur->hdle = NULL;
            goto err;
        }
    } else {
        secur->hdle = NULL;
    }

    return (secure_handle_t *)secur;

err:
    secure_dispose((secure_handle_t *)secur);
    return NULL;
}

/**
 * Dispose the security module
 *
 * @param [in] h security handler
 *
 * @return E_ERR_FAILED if failed
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t secure_dispose(secure_handle_t *h)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;
    secure_t *secur = (secure_t *)h;

    /* do not use ASSERT in dispose function */
    if (secur && secur->enable) {
        if (secur->hdle != NULL) {
            dlclose(secur->hdle);
            secur->hdle = NULL;
        } else {
            ret = E_ERR_FAILED;
        }
        if (secur->nb_of_msgs > 0) {
            int i = 0;
            for (i = 0; i < secur->nb_of_msgs; i++)
                free(secur->send_queue[i].message);
        }
    }

    free(secur);

    return ret;
}

/**
 * Provide the secur callback function
 *
 * @param [in] h security handler
 *
 * @return a valid secure_cb_t
 * @return NULL otherwise
 */
secure_cb_t secure_get_callback(const secure_handle_t *h)
{
    secure_t *secur = (secure_t *)h;
    secure_cb_t callback = NULL;

    ASSERT(secur != NULL);

    if (secur->enable)
        callback = secur->callback;

    return callback;
}

/**
 * Return file descriptor used by the secure module
 *
 * @param [in] h security handler
 *
 * @return valid fd
 */
int secure_get_fd(secure_handle_t *h)
{
    secure_t *secur = (secure_t *)h;

    ASSERT(secur != NULL);

    return secur->fd;
}

/**
 * Return security error message
 *
 * @param [in] h security handler
 *
 * @return valid err_msg
 */
const char *secure_get_error(secure_handle_t *h)
{
    secure_t *secur = (secure_t *)h;

    ASSERT(secur != NULL);

    return secur->err_msg;
}
