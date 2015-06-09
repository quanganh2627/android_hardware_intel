/* Modem Manager - client list source file
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
#include <sys/types.h>
#include <sys/socket.h>
#include "client.h"
#include "client_cnx.h"
#include "errors.h"
#include "logs.h"
#include "data_to_msg.h"
#include "msg_format.h"
#include <time.h>

#define NEW_CLIENT_NAME "unknown"

typedef e_mmgr_errors_t (*set_msg) (msg_t *, mmgr_cli_event_t *);

typedef struct client {
    char name[CLIENT_NAME_LEN + 1];
    int fd;
    struct timespec time;
    e_mmgr_requests_t request;
    uint32_t subscription;
    /* These flags are used to store client ACKs */
    e_cnx_requests_t cnx;
    set_msg *set_data;
} client_t;

typedef struct client_list {
    int list_size;
    int connected;
    client_t *list;
    set_msg set_data[E_MMGR_NUM_EVENTS];
} client_list_t;

static const char const *g_mmgr_events[] = {
#undef X
#define X(a) #a
    MMGR_EVENTS
};

static e_mmgr_errors_t client_close(client_list_t *clients);

/**
 * Check if the client is fully registered
 *
 * @param [in] h client handle
 *
 * @return false if client is NULL
 * @return current client status
 */
bool client_is_registered(const client_hdle_t *h)
{
    bool registered = false;
    client_t *client = (client_t *)h;

    if (client)
        registered = (client->cnx & E_CNX_NAME) &&
                     (client->cnx & E_CNX_FILTER);

    return registered;
}

/**
 * Check all clients acknowledgement for those registered to the event
 *
 * @private
 *
 * @param [in] h clients list handle
 * @param [in] filter specify which element to check
 * @param [in] ev user should be registered to this event. If not,
 *             MMGR will take into account its ACK.
 * @param [in] print
 *
 * @return false if h is NULL or ev is incorrect
 * @return false if at least one client has not acknowledge
 * @return true if all clients have acknowledge
 */
static bool check_all_clients_ack(const clients_hdle_t *h,
                                  e_cnx_requests_t filter, e_mmgr_events_t ev,
                                  e_print_t print)
{
    bool answer = true;
    client_list_t *clients = (client_list_t *)h;

    ASSERT(clients != NULL);

    if (ev > E_MMGR_NUM_EVENTS) {
        answer = false;
    } else {
        for (int i = 0; i < clients->list_size; i++) {
            if (clients->list[i].fd != CLOSED_FD) {
                if ((clients->list[i].subscription & (0x1 << ev)) &&
                    ((clients->list[i].cnx & filter) != filter)) {
                    answer = false;
                    if (E_PRINT == print)
                        LOG_DEBUG("client (%s) did not ack to %s",
                                  clients->list[i].name, g_mmgr_events[ev]);
                    else
                        break;
                }
            }
        }
    }

    return answer;
}

/**
 * Reset all clients acknowledgement
 *
 * @private
 *
 * @param [in] h clients list handle
 * @param [in] filter specify which element to check
 *
 * @return E_ERR_FAILED if at least one client has not acknowledge
 * @return E_ERR_SUCCESS if successful
 */
static inline e_mmgr_errors_t reset_ack(clients_hdle_t *h,
                                        e_cnx_requests_t filter)
{
    client_list_t *clients = (client_list_t *)h;

    ASSERT(clients != NULL);

    for (int i = 0; i < clients->list_size; i++) {
        if (clients->list[i].fd != CLOSED_FD)
            clients->list[i].cnx &= ~filter;
    }

    return E_ERR_SUCCESS;
}

/**
 * init current client
 *
 * @private
 *
 * @param [in] client current client
 * @param [in] fd client file descriptor
 *
 * @return E_ERR_FAILED if at least one client has not acknowledge
 * @return E_ERR_SUCCESS if successful
 */
static inline void init_client(client_t *client, int fd)
{
    ASSERT(client != NULL);

    client->fd = fd;
    client->cnx = E_CNX_RESOURCE_RELEASED;
    /* users should be registered to these events */
    client->subscription = (0x1 << E_MMGR_ACK) | (0x1 << E_MMGR_NACK);
    strncpy(client->name, NEW_CLIENT_NAME, CLIENT_NAME_LEN);
    clock_gettime(CLOCK_MONOTONIC, &client->time);
}

/**
 * remove client from client's list
 *
 * @private
 *
 * @param [in,out] clients list of clients
 * @param [in] fd
 *
 * @return E_ERR_FAILED if client not found
 * @return E_ERR_SUCCESS if successful
 */
static e_mmgr_errors_t remove_from_list(client_list_t *clients, int fd)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;

    ASSERT(clients != NULL);

    if (CLOSED_FD != fd) {
        for (int i = 0; i < clients->list_size; i++) {
            if (fd == clients->list[i].fd) {
                clients->connected--;
                LOG_INFO("client (fd=%d name=%s) removed. still connected: %d",
                         clients->list[i].fd, clients->list[i].name,
                         clients->connected);
                clients->list[i].fd = CLOSED_FD;
                ret = E_ERR_SUCCESS;
                break;
            }
        }
    }

    return ret;
}

/**
 * initialize client list structure
 *
 * @param [in] list_size size of clients
 *
 * @return a valid clients_hdle_t pointer
 * @return NULL otherwise
 */
clients_hdle_t *clients_init(int list_size)
{
    client_list_t *clients = NULL;

    clients = calloc(1, sizeof(client_list_t));
    if (!clients)
        goto err;

    clients->list = calloc(list_size, sizeof(client_t));
    if (!clients->list)
        goto err;

    clients->connected = 0;
    clients->list_size = list_size;
    for (int i = 0; i < list_size; i++) {
        init_client(&clients->list[i], CLOSED_FD);
        clients->list[i].set_data = clients->set_data;
    }

    for (int i = 0; i < E_MMGR_NUM_EVENTS; i++)
        clients->set_data[i] = msg_set_empty;
    clients->set_data[E_MMGR_RESPONSE_MODEM_HW_ID] = set_msg_modem_hw_id;
    clients->set_data[E_MMGR_RESPONSE_FUSE_INFO] = set_msg_fuse_info;
    clients->set_data[E_MMGR_NOTIFY_AP_RESET] = set_msg_ap_reset;
    clients->set_data[E_MMGR_NOTIFY_CORE_DUMP_COMPLETE] = set_msg_core_dump;
    clients->set_data[E_MMGR_NOTIFY_TFT_EVENT] = set_msg_tft_event;
    clients->set_data[E_MMGR_RESPONSE_MODEM_FW_RESULT] =
        set_msg_modem_fw_result;
    clients->set_data[E_MMGR_RESPONSE_MODEM_NVM_RESULT] =
        set_msg_modem_nvm_result;
    return (clients_hdle_t *)clients;

err:
    clients_dispose((clients_hdle_t *)clients);
    return NULL;
}

/**
 * Disposes clients module
 *
 * @param [in] h handle
 *
 * @return E_ERR_SUCCESS otherwise
 */
e_mmgr_errors_t clients_dispose(clients_hdle_t *h)
{
    client_list_t *clients = (client_list_t *)h;

    /* do not use ASSERT in dispose function */

    if (clients) {
        client_close(clients);
        if (clients->list)
            free(clients->list);
        free(clients);
    }

    return E_ERR_SUCCESS;
}

/**
 * add new client to list
 *
 * @param [in,out] h list of clients
 * @param [in] fd client file descriptor
 *
 * @return E_ERR_FAILED no space
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t client_add(clients_hdle_t *h, int fd)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    client_list_t *clients = (client_list_t *)h;

    ASSERT(clients != NULL);

    for (int i = 0; i < clients->list_size; i++) {
        if (clients->list[i].fd == CLOSED_FD) {
            init_client(&clients->list[i], fd);
            clients->connected++;
            LOG_DEBUG("client (fd=%d) added. connected: %d",
                      fd, clients->connected);
            ret = E_ERR_SUCCESS;
            break;
        }
    }

    return ret;
}

/**
 * close connexion, remove connexion on epoll and remove client from list
 *
 * @param [in] h clients list handle
 * @param [in] fd
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t client_remove(clients_hdle_t *h, int fd)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;
    client_list_t *clients = (client_list_t *)h;

    ASSERT(clients != NULL);

    /* No needs to unsubscribe the fd from epoll list. It's automatically done
     * when the fd is closed. See epoll man page. As remove_from_list set fd to
     * CLOSED_FD, do a backup to close it */
    ret = remove_from_list(clients, fd);
    cnx_close(&fd);

    return ret;
}

/**
 * Set client name
 *
 * @param [in,out] h client handle
 * @param [in] name client name
 * @param [in] len name length
 *
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t client_set_name(client_hdle_t *h, const char *name, size_t len)
{
    client_t *client = (client_t *)h;

    ASSERT(client != NULL);
    ASSERT(name != NULL);

    if (len > CLIENT_NAME_LEN) {
        LOG_ERROR("client name too long");
        len = CLIENT_NAME_LEN;
    }
    strncpy(client->name, name, len);
    client->name[len] = '\0';
    LOG_DEBUG("client with fd=%d is called \"%s\"", client->fd, client->name);
    client->cnx |= E_CNX_NAME;

    return E_ERR_SUCCESS;
}

/**
 * set client filter events
 *
 * @param [in,out] h client handle
 * @param [in] subscription client filter param
 *
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t client_set_filter(client_hdle_t *h, uint32_t subscription)
{
    client_t *client = (client_t *)h;

    ASSERT(client != NULL);

    client->subscription = subscription;
    client->cnx |= E_CNX_FILTER;
    LOG_DEBUG("client (fd=%d name=%s) filter=0x%.8X", client->fd, client->name,
              client->subscription);

    return E_ERR_SUCCESS;
}

/**
 * find the client on client list
 *
 * @param [in] h list of clients
 * @param [in] fd client's file descriptor
 *
 * @return NULL if h is NULL or client not found
 * @return a valid client_hdle_t pointer
 */
client_hdle_t *client_find(const clients_hdle_t *h, int fd)
{
    client_list_t *clients = (client_list_t *)h;
    client_t *client = NULL;

    ASSERT(clients != NULL);

    if (fd != CLOSED_FD) {
        for (int i = 0; i < clients->list_size; i++) {
            if (fd == clients->list[i].fd) {
                client = &clients->list[i];
                break;
            }
        }
    }

    return (client_hdle_t *)client;
}


/**
 * send message with data to client
 *
 * @param [in] h client to inform
 * @param [in] state state to provide
 * @param [in] data data to send
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t client_inform(const client_hdle_t *h, e_mmgr_events_t state,
                              void *data)
{
    size_t size;
    size_t write_size;
    e_mmgr_errors_t ret = E_ERR_SUCCESS;
    mmgr_cli_event_t event = { .id = state, .data = data };
    msg_t msg = { .data = NULL };
    client_t *client = (client_t *)h;

    ASSERT(client != NULL);
    ASSERT(client->set_data[state] != NULL);

    /* do not check data because it can be NULL on purpose */
    client->set_data[state] (&msg, &event);

    size = SIZE_HEADER + msg.hdr.len;
    write_size = size;
    if ((0x01 << state) & client->subscription) {
        if ((ret = cnx_write(client->fd, msg.data, &write_size)) ==
            E_ERR_SUCCESS) {
            if (size != write_size) {
                LOG_ERROR("send failed for client (fd=%d name=%s) send=%d/%d",
                          client->fd, client->name, write_size, size);
                ret = E_ERR_FAILED;
            } else {
                LOG_DEBUG("Client (fd=%d name=%s) informed of: %s", client->fd,
                          client->name, g_mmgr_events[state]);
            }
        }
    } else {
        LOG_DEBUG("Client (fd=%d name=%s) NOT informed of: %s",
                  client->fd, client->name, g_mmgr_events[state]);
    }

    msg_delete(&msg);
    return ret;
}

/**
 * inform all clients of modem state
 *
 * @param [in] h clients list handle
 * @param [in] state current modem state
 * @param [in] data additional data to send
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t clients_inform_all(const clients_hdle_t *h,
                                   e_mmgr_events_t state, void *data)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;
    static bool down_state = false;
    client_list_t *clients = (client_list_t *)h;

    if (state == E_MMGR_EVENT_MODEM_DOWN) {
        if (down_state)
            goto out;
        else
            down_state = true;
    } else if (state == E_MMGR_EVENT_MODEM_UP) {
        down_state = false;
    }

    ASSERT(clients != NULL);
    /* do not check data because it can be NULL on purpose */

    for (int i = 0; i < clients->list_size; i++) {
        if (clients->list[i].fd != CLOSED_FD)
            ret = client_inform((client_hdle_t *)&clients->list[i], state,
                                data);
    }

out:
    return ret;
}

/**
 * close all connexion's client fd
 *
 * @param [in,out] clients list of clients
 *
 * @return E_ERR_SUCCESS if successful
 */
static e_mmgr_errors_t client_close(client_list_t *clients)
{
    ASSERT(clients != NULL);

    for (int i = 0; i < clients->list_size; i++) {
        if (clients->list[i].fd != CLOSED_FD) {
            LOG_DEBUG("i=%d fd=%d", i, clients->list[i].fd);
            cnx_close(&clients->list[i].fd);
        }
    }

    return E_ERR_SUCCESS;
}

/**
 * check if all clients have acknowledge the modem cold request
 *
 * @param [in,out] h clients list handle
 * @param [in] print
 *
 * @return false if h is NULL
 * @return true if all clients have acknowledge
 */
bool clients_has_ack_cold(const clients_hdle_t *h, e_print_t print)
{
    return check_all_clients_ack(h, E_CNX_COLD_RESET,
                                 E_MMGR_NOTIFY_MODEM_COLD_RESET, print);
}

/**
 * check if all clients have acknowledge the shutdown request
 *
 * @param [in,out] h clients list handle
 * @param [in] print
 *
 * @return false if h is NULL
 * @return true if all clients have acknowledge
 */
bool clients_has_ack_shtdwn(const clients_hdle_t *h, e_print_t print)
{
    return check_all_clients_ack(h, E_CNX_MODEM_SHUTDOWN,
                                 E_MMGR_NOTIFY_MODEM_SHUTDOWN, print);
}

/**
 * check if all clients have released the resource
 *
 * @param [in,out] h list of clients
 * @param [in] print
 *
 * @return false if h is NULL
 * @return true if at least one client has the resource
 */
bool clients_has_resource(const clients_hdle_t *h, e_print_t print)
{
    bool answer = false;
    client_list_t *clients = (client_list_t *)h;

    ASSERT(clients != NULL);

    for (int i = 0; i < clients->list_size; i++) {
        if (clients->list[i].fd != CLOSED_FD) {
            if ((clients->list[i].cnx & E_CNX_RESOURCE_RELEASED)
                != E_CNX_RESOURCE_RELEASED) {
                answer = true;
                if (E_PRINT == print)
                    LOG_DEBUG("client (%s) did not release",
                              clients->list[i].name);
                else
                    break;
            }
        }
    }

    return answer;
}

/**
 * reset cold ack flag for all connected clients
 *
 * @param [in,out] h list of clients
 *
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t clients_reset_ack_cold(clients_hdle_t *h)
{
    return reset_ack(h, E_CNX_COLD_RESET);
}

/**
 * reset shutdown ack flag for all connected clients
 *
 * @param [in,out] h list of clients
 *
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t clients_reset_ack_shtdwn(clients_hdle_t *h)
{
    return reset_ack(h, E_CNX_MODEM_SHUTDOWN);
}

/**
 * @brief client_get_name Return client name
 * Returned pointer must not be freed.
 *
 * @param h
 *
 * @return client name
 */
const char *client_get_name(const client_hdle_t *h)
{
    client_t *client = (client_t *)h;

    ASSERT(client != NULL);

    return client->name;
}

/**
 * @brief client_get_fd Return client file descriptor
 *
 * @param h
 *
 * @return CLOSED_FD if h is NULL
 */
int client_get_fd(const client_hdle_t *h)
{
    int fd = CLOSED_FD;
    client_t *client = (client_t *)h;

    ASSERT(client != NULL);

    fd = client->fd;

    return fd;
}

/**
 * @brief client_set_request set client request
 *
 * @param h
 * @param req request
 *
 * @return E_ERR_SUCCESS
 */
e_mmgr_errors_t client_set_request(client_hdle_t *h, e_cnx_requests_t req)
{
    client_t *client = (client_t *)h;

    ASSERT(client != NULL);

    client->cnx |= req;

    return E_ERR_SUCCESS;
}

/**
 * @brief client_unset_request unset client request
 *
 * @param h
 * @param req request
 *
 * @return E_ERR_SUCCESS
 */
e_mmgr_errors_t client_unset_request(client_hdle_t *h, e_cnx_requests_t req)
{
    client_t *client = (client_t *)h;

    ASSERT(client != NULL);

    client->cnx &= ~req;

    return E_ERR_SUCCESS;
}

/**
 * @brief clients_get_connected Return number of connected clients
 *
 * @param h
 *
 * @return -1 if h is NULL
 * @return number of connected clients
 */
int clients_get_connected(const clients_hdle_t *h)
{
    int nb = -1;
    client_list_t *clients = (client_list_t *)h;

    ASSERT(clients != NULL);

    nb = clients->connected;

    return nb;
}

/**
 * @brief clients_get_allowed Return number of allowed clients
 *
 * @param h
 *
 * @return -1 if h is NULL
 * @return allowed clients
 */
int clients_get_allowed(const clients_hdle_t *h)
{
    int nb = -1;
    client_list_t *clients = (client_list_t *)h;

    ASSERT(clients != NULL);

    nb = clients->list_size;

    return nb;
}
