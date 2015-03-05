/* Modem Manager - client events header file
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

#define MMGR_FW_OPERATIONS
#include <arpa/inet.h>
#include <errno.h>
#include "client.h"
#include "client_cnx.h"
#include "client_events.h"
#include "data_to_msg.h"
#include "common.h"
#include "errors.h"
#include "file.h"
#include "logs.h"
#include "modem_events.h"
#include "modem_specific.h"
#include "property.h"
#include "timer_events.h"
#include "reset_escalation.h"
#include "tty.h"
#include "msg_format.h"

/* This value is deliberately obfuscated. Otherwise, all clients
 * could declared the modem OOS */
#define CARE_CENTER 0xCA2CE7E2

static const char const *g_mmgr_requests[] = {
#undef X
#define X(a) #a
    MMGR_REQUESTS
};

#define RND_CERTIFICATE_FILE  "/logs/modem_rnd_certif.bin"

/**
 * handle REQUEST_MODEM_FUSE_INFO request if state is MDM_UP
 *
 * @private
 *
 * @param [in,out] mmgr mmgr context
 *
 * @return E_ERR_SUCCESS if successful
 */
static e_mmgr_errors_t request_modem_fuse_info(mmgr_data_t *mmgr)
{
    ASSERT(mmgr != NULL);

    /* @TODO: handle this request */
    return client_inform(mmgr->request.client, E_MMGR_NACK, NULL);
}

/**
 * handle REQUEST_MODEM_GET_HW_ID request if state is MDM_UP
 *
 * @private
 *
 * @param [in,out] mmgr mmgr context
 *
 * @return E_ERR_SUCCESS if successful
 */
static e_mmgr_errors_t request_modem_get_hw_id(mmgr_data_t *mmgr)
{
    ASSERT(mmgr != NULL);

    /* @TODO: handle this request */
    return client_inform(mmgr->request.client, E_MMGR_NACK, NULL);
}

/**
 * handle E_MMGR_SET_NAME request if state is MDM_UP
 *
 * @private
 *
 * @param [in,out] mmgr mmgr context
 *
 * @return E_ERR_SUCCESS if successful
 */
static e_mmgr_errors_t request_set_name(mmgr_data_t *mmgr)
{
    e_mmgr_errors_t ret;

    ASSERT(mmgr != NULL);

    ret = client_set_name(mmgr->request.client, mmgr->request.msg.data,
                          mmgr->request.msg.hdr.len);

    if (ret != E_ERR_SUCCESS)
        ret = E_ERR_DISCONNECTED;

    client_inform(mmgr->request.client, E_MMGR_ACK, NULL);

    return ret;
}

/**
 * handle E_MMGR_SET_EVENTS request
 *
 * @private
 *
 * @param [in,out] mmgr mmgr context
 *
 * @return E_ERR_SUCCESS if successful
 */
static e_mmgr_errors_t request_set_events(mmgr_data_t *mmgr)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    uint32_t filter;

    ASSERT(mmgr != NULL);

    if (mmgr->request.msg.hdr.len == sizeof(uint32_t)) {
        memcpy(&filter, mmgr->request.msg.data, sizeof(uint32_t));
        filter = ntohl(filter);
        ret = client_set_filter(mmgr->request.client, filter);

        /* inform client that connection has succeed */
        client_inform(mmgr->request.client, E_MMGR_ACK, NULL);

        /* client is registered and accepted. So, MMGR should provide the
         * current modem status if client has subsribed to it */
        e_mmgr_events_t notification = E_MMGR_NUM_EVENTS;
        switch (mmgr->state) {
        case E_MMGR_MDM_OOS:
            notification = E_MMGR_EVENT_MODEM_OUT_OF_SERVICE;
            break;
        case E_MMGR_MDM_START:
        case E_MMGR_MDM_CONF_ONGOING:
        case E_MMGR_MDM_RESET:
        case E_MMGR_MDM_OFF:
        case E_MMGR_MDM_PREPARE_OFF:
            notification = E_MMGR_EVENT_MODEM_DOWN;
            break;
        case E_MMGR_MDM_UP:
            notification = E_MMGR_EVENT_MODEM_UP;
            break;
        case E_MMGR_MDM_CORE_DUMP:
            notification = E_MMGR_NOTIFY_CORE_DUMP;
            break;
        case E_MMGR_WAIT_COLD_ACK:
            notification = E_MMGR_NOTIFY_MODEM_COLD_RESET;
            break;
        case E_MMGR_WAIT_SHT_ACK:
            notification = E_MMGR_NOTIFY_MODEM_SHUTDOWN;
            break;
        case E_MMGR_NUM:
            break;
        }
        client_inform(mmgr->request.client, notification, NULL);
    } else {
        LOG_ERROR("bad filter size");
        client_inform(mmgr->request.client, E_MMGR_NACK, NULL);
    }

    return ret;
}

/**
 * handle E_MMGR_RESOURCE_ACQUIRE request if state is MDM_OFF
 *
 * @private
 *
 * @param [in,out] mmgr mmgr context
 *
 * @return E_ERR_SUCCESS if successful
 */
static e_mmgr_errors_t resource_acquire_wakeup_modem(mmgr_data_t *mmgr)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;

    ASSERT(mmgr != NULL);

    client_inform(mmgr->request.client, E_MMGR_ACK, NULL);

    client_unset_request(mmgr->request.client, E_CNX_RESOURCE_RELEASED);
    /* the modem is off, then wake up the modem */
    LOG_DEBUG("wake up modem");

    mmgr->info.polled_states = MDM_CTRL_STATE_COREDUMP;
    if (mmgr->info.is_flashless)
        mmgr->info.polled_states |= MDM_CTRL_STATE_FW_DOWNLOAD_READY;
    else if (mmgr->info.ipc_ready_present)
        mmgr->info.polled_states |= MDM_CTRL_STATE_IPC_READY;
    set_mcd_poll_states(&mmgr->info);
    mmgr->events.link_state = E_MDM_LINK_NONE;

    if (E_ERR_SUCCESS != mdm_prepare(&mmgr->info)) {
        LOG_ERROR("modem fw is corrupted. Declare modem OOS");
        /* Set MMGR state to MDM_RESET to call the recovery module and
         * force modem recovery to OOS. By doing so, MMGR will turn off the
         * modem and declare the modem OOS. Clients will not be able to turn
         * on the modem */
        recov_force(mmgr->reset, E_FORCE_OOS);
        reset_modem(mmgr);
        ret = E_ERR_FAILED;
    } else if ((ret = mdm_up(&mmgr->info)) == E_ERR_SUCCESS) {
        if ((mmgr->info.mdm_link == E_LINK_USB) &&
            mmgr->info.is_flashless)
            set_mmgr_state(mmgr, E_MMGR_MDM_START);
        else
            set_mmgr_state(mmgr, E_MMGR_MDM_CONF_ONGOING);

        mmgr->events.cli_req = E_CLI_REQ_NONE;

        recov_reinit(mmgr->reset);
        if (!mmgr->info.is_flashless && mmgr->info.ipc_ready_present)
            timer_start(mmgr->timer, E_TIMER_WAIT_FOR_IPC_READY);

        /* if the modem is usb, add wait_for_bus_ready */
        /* @TODO: push that into modem_specific */
        if (mmgr->info.mdm_link == E_LINK_USB)
            timer_start(mmgr->timer, E_TIMER_WAIT_FOR_BUS_READY);
    }

    return ret;
}

/**
 * handle E_MMGR_RESOURCE_ACQUIRE request if state is MDM_UP
 *
 * @private
 *
 * @param [in,out] mmgr mmgr context
 *
 * @return E_ERR_SUCCESS if successful
 */
static e_mmgr_errors_t resource_acquire(mmgr_data_t *mmgr)
{
    ASSERT(mmgr != NULL);

    client_inform(mmgr->request.client, E_MMGR_ACK, NULL);
    return client_unset_request(mmgr->request.client, E_CNX_RESOURCE_RELEASED);
}

/**
 * handle E_MMGR_RESOURCE_RELEASE request if state is MDM_OFF
 *
 * @private
 *
 * @param [in,out] mmgr mmgr context
 *
 * @return E_ERR_SUCCESS if successful
 */
static e_mmgr_errors_t request_resource_release_mdm_off(mmgr_data_t *mmgr)
{
    ASSERT(mmgr != NULL);

    client_set_request(mmgr->request.client, E_CNX_RESOURCE_RELEASED);
    client_inform(mmgr->request.client, E_MMGR_ACK, NULL);

    return E_ERR_SUCCESS;
}

/**
 * handle E_MMGR_RESOURCE_RELEASE request if state not MDM_OOS
 *
 * @private
 *
 * @param [in,out] mmgr mmgr context
 *
 * @return E_ERR_SUCCESS if successful
 */
static e_mmgr_errors_t request_resource_release(mmgr_data_t *mmgr)
{
    ASSERT(mmgr != NULL);

    client_inform(mmgr->request.client, E_MMGR_ACK, NULL);
    client_set_request(mmgr->request.client, E_CNX_RESOURCE_RELEASED);

    if (!clients_has_resource(mmgr->clients, E_PRINT)) {
        LOG_INFO("notify clients that modem will be shutdown");
        clients_inform_all(mmgr->clients, E_MMGR_NOTIFY_MODEM_SHUTDOWN, NULL);
        /* if we have a current modem start procedure, stop all its timers */
        timer_stop_all(mmgr->timer);
        timer_start(mmgr->timer, E_TIMER_MODEM_SHUTDOWN_ACK);
        set_mmgr_state(mmgr, E_MMGR_WAIT_SHT_ACK);
    }

    return E_ERR_SUCCESS;
}

static e_mmgr_errors_t notify_ap_reset(mmgr_data_t *mmgr)
{
    mmgr_cli_internal_ap_reset_t ap_rst;
    e_mmgr_errors_t ret = E_ERR_FAILED;

    ASSERT(mmgr != NULL);

    const char *name = client_get_name(mmgr->request.client);
    ap_rst.len = strnlen(name, CLIENT_NAME_LEN);
    ap_rst.name = malloc(sizeof(char) * ap_rst.len);
    if (ap_rst.name == NULL) {
        LOG_ERROR("memory allocation fails");
    } else {
        if ((mmgr->request.msg.hdr.id == E_MMGR_REQUEST_MODEM_RECOVERY) &&
            (mmgr->request.msg.hdr.len != 0)) {
            ap_rst.extra_len = mmgr->request.msg.hdr.len;
            ap_rst.extra_data = mmgr->request.msg.data;
        } else {
            ap_rst.extra_len = 0;
            ap_rst.extra_data = NULL;
        }

        strncpy(ap_rst.name, name, ap_rst.len);
        ret = clients_inform_all(mmgr->clients, E_MMGR_NOTIFY_AP_RESET,
                                 &ap_rst);
        free(ap_rst.name);
    }

    return ret;
}

/**
 * handle E_MMGR_REQUEST_MODEM_RECOVERY request if state is MDM_UP
 *
 * @private
 *
 * @param [in,out] mmgr mmgr context
 *
 * @return E_ERR_SUCCESS if successful
 */
static e_mmgr_errors_t request_modem_recovery(mmgr_data_t *mmgr)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;
    int32_t sec;
    struct timeval ts;

    ASSERT(mmgr != NULL);

    client_inform(mmgr->request.client, E_MMGR_ACK, NULL);

    ts = recov_get_last_reset(mmgr->reset);
    memcpy(&sec, &mmgr->request.msg.hdr.ts, sizeof(uint32_t));
    if (sec > ts.tv_sec) {
        mmgr->events.cli_req = E_CLI_REQ_RESET;
        notify_ap_reset(mmgr);
        set_mmgr_state(mmgr, E_MMGR_MDM_RESET);
    } else {
        LOG_DEBUG("skipped. Request older than last recovery");
    }

    return ret;
}

/**
 * handle E_MMGR_REQUEST_MODEM_RESTART request if state is MDM_UP
 *
 * @private
 *
 * @param [in,out] mmgr mmgr context
 *
 * @return E_ERR_SUCCESS if successful
 */
static e_mmgr_errors_t request_modem_restart(mmgr_data_t *mmgr)
{
    uint32_t optional = 0;
    const char *name = NULL;

    ASSERT(mmgr != NULL);

    client_inform(mmgr->request.client, E_MMGR_ACK, NULL);

    if (mmgr->request.msg.hdr.len == sizeof(uint32_t)) {
        memcpy(&optional, mmgr->request.msg.data, sizeof(uint32_t));
        optional = ntohl(optional);
    }

    /* Only NVM server can declare the modem OOS by sending this request The
     * optional value is deliberately obfuscated */
    name = client_get_name(mmgr->request.client);
    if ((optional == CARE_CENTER) && name &&
        !(strncmp(name, "MMB_NVM", CLIENT_NAME_LEN))) {
        LOG_ERROR("NVM server has declared the modem unrecoverable");
        /* Set MMGR state to MDM_RESET to call the recovery module and force
         * OOS state. By doing so, MMGR will turn off the modem and declare the
         * modem OOS. Clients will not be able to turn on the modem */
        recov_force(mmgr->reset, E_FORCE_OOS);
    } else {
        mmgr->events.cli_req = E_CLI_REQ_RESET;
        recov_force(mmgr->reset, E_FORCE_NO_COUNT);
    }
    set_mmgr_state(mmgr, E_MMGR_MDM_RESET);

    return E_ERR_SUCCESS;
}

/**
 * handle REQUEST_MODEM_BACKUP_PRODUCTION request if state is MDM_UP
 *
 * @private
 *
 * @param [in,out] mmgr mmgr context
 *
 * @return E_ERR_SUCCESS if successful
 */
static e_mmgr_errors_t request_bkup_prod(mmgr_data_t *mmgr)
{
    ASSERT(mmgr != NULL);

    /* ack REQUEST_MODEM_BACKUP_PRODUCTION */
    client_inform(mmgr->request.client, E_MMGR_ACK, NULL);
    /* set backup_prod request */
    mmgr->events.cli_req |= E_CLI_REQ_PROD;
    /* do modem restart for NVM flush */
    recov_force(mmgr->reset, E_FORCE_NO_COUNT);
    set_mmgr_state(mmgr, E_MMGR_MDM_RESET);

    return E_ERR_SUCCESS;
}

/**
 * handle request ACK_COLD_RESET if state is WAIT_COLD_ACK
 *
 * @private
 *
 * @param [in,out] mmgr mmgr context
 *
 * @return E_ERR_SUCCESS if successful
 */
static e_mmgr_errors_t request_ack_cold_reset(mmgr_data_t *mmgr)
{
    ASSERT(mmgr != NULL);

    client_set_request(mmgr->request.client, E_CNX_COLD_RESET);
    if (clients_has_ack_cold(mmgr->clients, E_MUTE)) {
        LOG_DEBUG("All clients agreed cold reset");
        if (mmgr->events.cli_req & E_CLI_REQ_PROD) {
            /* backup nvm files from /config/telephony to /factory/telephony */
            if (backup_prod_nvm(&mmgr->info) == E_ERR_SUCCESS)
                clients_inform_all(mmgr->clients,
                                   E_MMGR_RESPONSE_MODEM_BACKUP_PRODUCTION,
                                   NULL);
        }
        mmgr->events.cli_req = E_CLI_REQ_RESET;
        set_mmgr_state(mmgr, E_MMGR_MDM_RESET);
    }

    return E_ERR_SUCCESS;
}

/**
 * handle request ACK_MODEM_SHUTDOWN if state is WAIT_SHT_ACK
 *
 * @private
 *
 * @param [in,out] mmgr mmgr context
 *
 * @return E_ERR_SUCCESS if successful
 */
static e_mmgr_errors_t request_ack_modem_shutdown(mmgr_data_t *mmgr)
{
    ASSERT(mmgr != NULL);

    client_set_request(mmgr->request.client, E_CNX_MODEM_SHUTDOWN);
    if (clients_has_ack_shtdwn(mmgr->clients, E_MUTE)) {
        LOG_DEBUG("All clients agreed modem shutdown");
        mmgr->events.cli_req = E_CLI_REQ_OFF;
        timer_stop(mmgr->timer, E_TIMER_MODEM_SHUTDOWN_ACK);
        set_mmgr_state(mmgr, E_MMGR_MDM_RESET);
    }

    return E_ERR_SUCCESS;
}

/**
 * handle E_MMGR_REQUEST_FORCE_MODEM_SHUTDOWN request if state is MDM_UP
 *
 * @private
 *
 * @param [in,out] mmgr mmgr context
 *
 * @return E_ERR_SUCCESS if successful
 */
static e_mmgr_errors_t request_force_modem_shutdown(mmgr_data_t *mmgr)
{
    ASSERT(mmgr != NULL);

    client_inform(mmgr->request.client, E_MMGR_ACK, NULL);
    clients_inform_all(mmgr->clients, E_MMGR_NOTIFY_MODEM_SHUTDOWN, NULL);
    timer_start(mmgr->timer, E_TIMER_MODEM_SHUTDOWN_ACK);
    set_mmgr_state(mmgr, E_MMGR_WAIT_SHT_ACK);

    return E_ERR_SUCCESS;
}

/**
 * handle client request
 *
 * @private
 *
 * @param [in,out] mmgr mmgr context
 *
 * @return E_ERR_FAILED if an error occurs
 * @return E_ERR_SUCCESS if successful
 */
static e_mmgr_errors_t client_request(mmgr_data_t *mmgr)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    size_t size;

    ASSERT(mmgr != NULL);

    mmgr->request.msg.data = NULL;
    size = mmgr->request.msg.hdr.len;

    if (size > 0) {
        mmgr->request.msg.data = calloc(size, sizeof(char));
        if (mmgr->request.msg.data == NULL)
            goto out;
        ret = cnx_read(client_get_fd(
                           mmgr->request.client), mmgr->request.msg.data,
                       &size);
        if ((ret != E_ERR_SUCCESS) || (size != mmgr->request.msg.hdr.len)) {
            LOG_ERROR("Client (fd=%d name=%s) Failed to read data",
                      client_get_fd(mmgr->request.client),
                      client_get_name(mmgr->request.client));
            goto out_free;
        }
    }

    if (mmgr->request.msg.hdr.id < E_MMGR_NUM_REQUESTS) {
        LOG_INFO("Request (%s) received from client (fd=%d name=%s)",
                 g_mmgr_requests[mmgr->request.msg.hdr.id],
                 client_get_fd(mmgr->request.client),
                 client_get_name(mmgr->request.client));

        if (!client_is_registered(mmgr->request.client) &&
            (mmgr->request.msg.hdr.id != E_MMGR_SET_NAME) &&
            (mmgr->request.msg.hdr.id != E_MMGR_SET_EVENTS)) {
            LOG_DEBUG("client not fully registered. Request rejected");
            client_inform(mmgr->request.client, E_MMGR_NACK, NULL);
        } else {
            if ((mmgr->state < E_MMGR_NUM) &&
                (mmgr->hdler_client[mmgr->state][mmgr->request.msg.hdr.id]))
                ret = mmgr->hdler_client[mmgr->state][mmgr->request.msg.hdr.id]
                          (mmgr);
        }
    }

out_free:
    if (mmgr->request.msg.data != NULL)
        free(mmgr->request.msg.data);
out:
    return ret;
}

/**
 * handle known client request
 *
 * @param [in,out] mmgr mmgr context
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED if client not found or cnx disconnection fails or
 *                       client banned
 */
e_mmgr_errors_t known_client(mmgr_data_t *mmgr)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;
    int fd = CLOSED_FD;
    client_hdle_t *client = NULL;

    ASSERT(mmgr != NULL);

    fd = mmgr->events.ev[mmgr->events.cur_ev].data.fd;
    client = client_find(mmgr->clients, fd);

    if (!client) {
        LOG_ERROR("failed to find client (fd=%d)", fd);
        /* close file descriptor to avoid fake events */
        close(fd);
    } else {
        const char *name = client_get_name(client);

        ret = msg_get_header(fd, &mmgr->request.msg.hdr);
        mmgr->request.client = client;
        if (ret == E_ERR_SUCCESS) {
            ret = client_request(mmgr);
        } else if (ret == E_ERR_DISCONNECTED) {
            /* client disconnection */
            LOG_DEBUG("Client (fd=%d name=%s) is disconnected", fd, name);

            /* client must release the locked resource, if any. handle this
             * resource release according to MMGR state */
            if ((mmgr->state < E_MMGR_NUM) &&
                (mmgr->hdler_client[mmgr->state][E_MMGR_RESOURCE_RELEASE]))
                ret = mmgr->hdler_client[mmgr->state][E_MMGR_RESOURCE_RELEASE]
                          (mmgr);
            ret = client_remove(mmgr->clients, fd);
        } else {
            LOG_ERROR("Client (fd=%d name=%s) bad message", fd, name);
        }
    }

    return ret;
}

/**
 * handle new cnx connection and add client in client list
 *
 * @param [in,out] mmgr mmgr context
 *
 * @return E_ERR_FAILED if cnx connection fails or client rejected
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t new_client(mmgr_data_t *mmgr)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    int conn_sock;
    int fd;

    ASSERT(mmgr != NULL);

    fd = mmgr->events.ev[mmgr->events.cur_ev].data.fd;

    if (clients_get_connected(mmgr->clients) <=
        clients_get_allowed(mmgr->clients)) {
        LOG_DEBUG("try to subscribe new client fd=%d", fd);
        conn_sock = cnx_accept(fd);
        if (conn_sock < 0) {
            LOG_ERROR("Error during accept (%s)", strerror(errno));
        } else if (tty_listen_fd(mmgr->epollfd, conn_sock,
                                 EPOLLIN) == E_ERR_SUCCESS) {
            ret = client_add(mmgr->clients, conn_sock);
            if (ret != E_ERR_SUCCESS)
                LOG_ERROR("failed to add new client");
            /* do not provide modem status as long as client has not
             * provided its name */
        }
    } else {
        LOG_INFO("client rejected: max client reached");
    }

    return ret;
}

static e_mmgr_errors_t request_fake_up(mmgr_data_t *mmgr)
{
    ASSERT(mmgr != NULL);

    client_inform(mmgr->request.client, E_MMGR_ACK, NULL);
    clients_inform_all(mmgr->clients, E_MMGR_EVENT_MODEM_UP, NULL);

    return E_ERR_SUCCESS;
}

static e_mmgr_errors_t request_fake_down(mmgr_data_t *mmgr)
{
    ASSERT(mmgr != NULL);

    client_inform(mmgr->request.client, E_MMGR_ACK, NULL);
    clients_inform_all(mmgr->clients, E_MMGR_EVENT_MODEM_DOWN, NULL);

    return E_ERR_SUCCESS;
}

static e_mmgr_errors_t request_fake_shtdwn(mmgr_data_t *mmgr)
{
    ASSERT(mmgr != NULL);

    client_inform(mmgr->request.client, E_MMGR_ACK, NULL);
    clients_inform_all(mmgr->clients, E_MMGR_NOTIFY_MODEM_SHUTDOWN, NULL);

    return E_ERR_SUCCESS;
}

static e_mmgr_errors_t request_fake_oos(mmgr_data_t *mmgr)
{
    ASSERT(mmgr != NULL);

    client_inform(mmgr->request.client, E_MMGR_ACK, NULL);
    clients_inform_all(mmgr->clients, E_MMGR_EVENT_MODEM_OUT_OF_SERVICE, NULL);

    return E_ERR_SUCCESS;
}

static e_mmgr_errors_t request_fake_cdd(mmgr_data_t *mmgr)
{
    ASSERT(mmgr != NULL);

    client_inform(mmgr->request.client, E_MMGR_ACK, NULL);
    clients_inform_all(mmgr->clients, E_MMGR_NOTIFY_CORE_DUMP, NULL);

    return E_ERR_SUCCESS;
}

static e_mmgr_errors_t request_fake_ptfrmreboot(mmgr_data_t *mmgr)
{
    ASSERT(mmgr != NULL);

    client_inform(mmgr->request.client, E_MMGR_ACK, NULL);
    clients_inform_all(mmgr->clients, E_MMGR_NOTIFY_PLATFORM_REBOOT, NULL);

    return E_ERR_SUCCESS;
}

static e_mmgr_errors_t request_fake_self_reset(mmgr_data_t *mmgr)
{
    ASSERT(mmgr != NULL);

    client_inform(mmgr->request.client, E_MMGR_ACK, NULL);
    clients_inform_all(mmgr->clients, E_MMGR_NOTIFY_SELF_RESET, NULL);

    return E_ERR_SUCCESS;
}

static e_mmgr_errors_t request_fake_cdd_complete(mmgr_data_t *mmgr)
{
    mmgr_cli_core_dump_t cd;
    char filename[PATH_MAX] = "";
    char data[1] = "";
    const char *path = NULL;

    ASSERT(mmgr != NULL);

    client_inform(mmgr->request.client, E_MMGR_ACK, NULL);

    path = mcdr_get_path(mmgr->mcdr);
    if (path) {
        snprintf(filename, PATH_MAX - 1, "%s/%s", path, FAKE_CD_FILENAME);
        file_write(filename, OPEN_MODE_RW_UGO, data, 0);
    }

    cd.state = E_CD_SUCCEED;
    cd.path = filename;
    cd.path_len = strnlen(filename, PATH_MAX);
    cd.reason = FAKE_CD_REASON;
    cd.reason_len = strlen(FAKE_CD_REASON);

    clients_inform_all(mmgr->clients, E_MMGR_NOTIFY_CORE_DUMP_COMPLETE, &cd);

    return E_ERR_SUCCESS;
}

static e_mmgr_errors_t request_fake_ap_reset(mmgr_data_t *mmgr)
{
    client_inform(mmgr->request.client, E_MMGR_ACK, NULL);
    return notify_ap_reset(mmgr);
}

static e_mmgr_errors_t request_fake_tft_event(mmgr_data_t *mmgr)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;
    static const char *const ev_name = "TFT_EVENT_TEST";
    mmgr_cli_tft_event_data_t data[MMGR_CLI_MAX_TFT_EVENT_DATA];
    mmgr_cli_tft_event_t ev = { E_EVENT_STATS, strlen(ev_name), ev_name,
                                MMGR_CLI_TFT_AP_LOG_MASK |
                                MMGR_CLI_TFT_BP_LOG_MASK,
                                MMGR_CLI_MAX_TFT_EVENT_DATA, data };
    int i;

    ASSERT(mmgr != NULL);

    for (i = 0; i < MMGR_CLI_MAX_TFT_EVENT_DATA; i++) {
        char *value;
        value = calloc(MMGR_CLI_MAX_TFT_EVENT_DATA_LEN, sizeof(char));
        if (value == NULL) {
            LOG_ERROR("Error during memory allocation for data %d", i);
            data[i].value = 0;
            data[i].len = 0;
        } else {
            data[i].len = sprintf(value, "Test data %d", i);
            data[i].value = value;
        }
    }

    client_inform(mmgr->request.client, E_MMGR_ACK, NULL);
    clients_inform_all(mmgr->clients, E_MMGR_NOTIFY_TFT_EVENT, &ev);

    for (i = 0; i < MMGR_CLI_MAX_TFT_EVENT_DATA; i++) {
        if (data[i].value != NULL)
            free((char *)data[i].value);
    }

    return ret;
}

e_mmgr_errors_t client_nack(mmgr_data_t *mmgr)
{
    ASSERT(mmgr != NULL);

    return client_inform(mmgr->request.client, E_MMGR_NACK, NULL);
}

/**
 * initialize the client events handlers
 *
 * @param [in] nb_client
 * @param [in,out] mmgr mmgr context
 *
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t client_events_init(int nb_client, mmgr_data_t *mmgr)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;
    int i, j;
    bool fake_requests = false;
    char build_type[PROPERTY_VALUE_MAX];

    ASSERT(mmgr != NULL);

    mmgr->clients = clients_init(nb_client);
    if (!mmgr->clients) {
        LOG_ERROR("Client list initialisation failed");
        ret = E_ERR_FAILED;
        goto out;
    }

    /* NB: Never accept client request during MDM_RESET or MDM_CONF_ONGOING
     * event */
    property_get_string(PROPERTY_BUILD_TYPE, build_type);
    mmgr->events.cli_req = E_CLI_REQ_NONE;

    /* Only enable fake requests for eng build */
    if (strncmp(build_type, FAKE_EVENTS_BUILD_TYPE, PROPERTY_VALUE_MAX) == 0)
        fake_requests = true;

    /* set default behavior */
    for (i = 0; i < E_MMGR_NUM; i++)
        for (j = 0; j < E_MMGR_NUM_REQUESTS; j++)
            mmgr->hdler_client[i][j] = client_nack;

    /* A client is ALWAYS able to establish a connection */
    for (i = 0; i < E_MMGR_NUM; i++) {
        mmgr->hdler_client[i][E_MMGR_SET_NAME] = request_set_name;
        mmgr->hdler_client[i][E_MMGR_SET_EVENTS] = request_set_events;
        if (fake_requests) {
            /* fake requests */
            mmgr->hdler_client[i][E_MMGR_REQUEST_FAKE_DOWN] = request_fake_down;
            mmgr->hdler_client[i][E_MMGR_REQUEST_FAKE_UP] = request_fake_up;
            mmgr->hdler_client[i][E_MMGR_REQUEST_FAKE_MODEM_SHUTDOWN] =
                request_fake_shtdwn;
            mmgr->hdler_client[i][E_MMGR_REQUEST_FAKE_MODEM_OUT_OF_SERVICE] =
                request_fake_oos;
            mmgr->hdler_client[i][E_MMGR_REQUEST_FAKE_CORE_DUMP] =
                request_fake_cdd;
            mmgr->hdler_client[i][E_MMGR_REQUEST_FAKE_PLATFORM_REBOOT] =
                request_fake_ptfrmreboot;
            mmgr->hdler_client[i][E_MMGR_REQUEST_FAKE_CORE_DUMP_COMPLETE] =
                request_fake_cdd_complete;
            mmgr->hdler_client[i][E_MMGR_REQUEST_FAKE_AP_RESET] =
                request_fake_ap_reset;
            mmgr->hdler_client[i][E_MMGR_REQUEST_FAKE_SELF_RESET] =
                request_fake_self_reset;
            mmgr->hdler_client[i][E_MMGR_REQUEST_FAKE_TFT_EVENT] =
                request_fake_tft_event;
        }
    }

    /* E_MMGR_RESOURCE_ACQUIRE */
    mmgr->hdler_client[E_MMGR_MDM_OFF][E_MMGR_RESOURCE_ACQUIRE] =
        resource_acquire_wakeup_modem;
    mmgr->hdler_client[E_MMGR_MDM_UP][E_MMGR_RESOURCE_ACQUIRE] =
        resource_acquire;
    mmgr->hdler_client[E_MMGR_MDM_START][E_MMGR_RESOURCE_ACQUIRE] =
        resource_acquire;
    mmgr->hdler_client[E_MMGR_MDM_CONF_ONGOING][E_MMGR_RESOURCE_ACQUIRE] =
        resource_acquire;
    mmgr->hdler_client[E_MMGR_WAIT_COLD_ACK][E_MMGR_RESOURCE_ACQUIRE] =
        resource_acquire;

    /* E_MMGR_RESOURCE_RELEASE */
    mmgr->hdler_client[E_MMGR_MDM_OFF][E_MMGR_RESOURCE_RELEASE] =
        request_resource_release_mdm_off;
    mmgr->hdler_client[E_MMGR_WAIT_SHT_ACK][E_MMGR_RESOURCE_RELEASE] =
        request_resource_release_mdm_off;
    mmgr->hdler_client[E_MMGR_WAIT_COLD_ACK][E_MMGR_RESOURCE_RELEASE] =
        request_resource_release;
    mmgr->hdler_client[E_MMGR_MDM_UP][E_MMGR_RESOURCE_RELEASE] =
        request_resource_release;

    /* E_MMGR_REQUEST_MODEM_RECOVERY */
    mmgr->hdler_client[E_MMGR_MDM_UP][E_MMGR_REQUEST_MODEM_RECOVERY] =
        request_modem_recovery;

    /* E_MMGR_REQUEST_MODEM_RESTART */
    mmgr->hdler_client[E_MMGR_MDM_UP][E_MMGR_REQUEST_MODEM_RESTART] =
        request_modem_restart;

    /* E_MMGR_REQUEST_FORCE_MODEM_SHUTDOWN */
    mmgr->hdler_client[E_MMGR_MDM_UP][E_MMGR_REQUEST_FORCE_MODEM_SHUTDOWN] =
        request_force_modem_shutdown;

    /* E_MMGR_ACK_MODEM_COLD_RESET */
    mmgr->hdler_client[E_MMGR_WAIT_COLD_ACK][E_MMGR_ACK_MODEM_COLD_RESET] =
        request_ack_cold_reset;

    /* E_MMGR_ACK_MODEM_SHUTDOWN */
    mmgr->hdler_client[E_MMGR_WAIT_SHT_ACK][E_MMGR_ACK_MODEM_SHUTDOWN] =
        request_ack_modem_shutdown;

    /* flashing API: */
    mmgr->hdler_client[E_MMGR_MDM_UP][E_MMGR_REQUEST_MODEM_FUSE_INFO] =
        request_modem_fuse_info;
    mmgr->hdler_client[E_MMGR_MDM_UP][E_MMGR_REQUEST_MODEM_GET_HW_ID] =
        request_modem_get_hw_id;
    mmgr->hdler_client[E_MMGR_MDM_UP][E_MMGR_REQUEST_MODEM_BACKUP_PRODUCTION] =
        request_bkup_prod;

out:
    return ret;
}

/**
 * This function disposes the client events module
 *
 * @param [in] mmgr
 *
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t client_events_dispose(mmgr_data_t *mmgr)
{
    ASSERT(mmgr != NULL);

    return clients_dispose(mmgr->clients);
}
