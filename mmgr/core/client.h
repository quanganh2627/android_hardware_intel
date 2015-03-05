/* Modem Manager - client list header file
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

#ifndef __MMGR_CLIENT_HEADER__
#define __MMGR_CLIENT_HEADER__

#include "errors.h"
#include "mmgr_cli.h"
#include <stdbool.h>

typedef enum e_cnx_requests {
    E_CNX_NONE = 0,
    E_CNX_NAME = 0x01 << 0,
        E_CNX_FILTER = 0x01 << 1,
        E_CNX_COLD_RESET = 0x01 << 2,
        E_CNX_MODEM_SHUTDOWN = 0x01 << 3,
        E_CNX_RESOURCE_RELEASED = 0x01 << 4
} e_cnx_requests_t;

typedef enum e_print {
    E_PRINT,
    E_MUTE,
} e_print_t;

/* client handle: */
typedef void *client_hdle_t;

/* list of clients: */
typedef void *clients_hdle_t;

/* clients API: Those functions handle the list of clients */
clients_hdle_t *clients_init(int list_size);
e_mmgr_errors_t clients_dispose(clients_hdle_t *clients);

e_mmgr_errors_t client_add(clients_hdle_t *clients, int fd);
e_mmgr_errors_t client_remove(clients_hdle_t *clients, int fd);

int clients_get_connected(const clients_hdle_t *l);
int clients_get_allowed(const clients_hdle_t *l);

e_mmgr_errors_t clients_inform_all(const clients_hdle_t *l, e_mmgr_events_t ev,
                                   void *d);

bool clients_has_ack_cold(const clients_hdle_t *l, e_print_t print);
bool clients_has_ack_shtdwn(const clients_hdle_t *l, e_print_t print);
bool clients_has_resource(const clients_hdle_t *l, e_print_t print);

e_mmgr_errors_t clients_reset_ack_cold(clients_hdle_t *clients);
e_mmgr_errors_t clients_reset_ack_shtdwn(clients_hdle_t *clients);

/* client API: Those function handle a client */
bool client_is_registered(const client_hdle_t *client);
e_mmgr_errors_t client_set_name(client_hdle_t *client, const char *name,
                                size_t len);
e_mmgr_errors_t client_set_filter(client_hdle_t *client, uint32_t subscription);
e_mmgr_errors_t client_set_request(client_hdle_t *cli, e_cnx_requests_t req);
e_mmgr_errors_t client_unset_request(client_hdle_t *cli, e_cnx_requests_t req);

e_mmgr_errors_t client_inform(const client_hdle_t *l, e_mmgr_events_t ev,
                              void *data);

client_hdle_t *client_find(const clients_hdle_t *l, int fd);
const char *client_get_name(const client_hdle_t *client);
int client_get_fd(const client_hdle_t *client);

#endif                          /* __MMGR_CLIENT_HEADER__ */
