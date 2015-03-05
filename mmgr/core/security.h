/* Modem Manager - secure header file
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

#ifndef __MMGR_SECURITY_HEADER__
#define __MMGR_SECURITY_HEADER__

#include "errors.h"
#include "tcs_mmgr.h"

typedef void *secure_handle_t;

typedef int (*secure_cb_t) (uint32_t *type, uint32_t *length, uint8_t **data);

secure_handle_t *secure_init(bool enabled, channel_t *ch);
e_mmgr_errors_t secure_dispose(secure_handle_t *secur);

e_mmgr_errors_t secure_register(secure_handle_t *secur, int *fd);
e_mmgr_errors_t secure_start(secure_handle_t *secur);
e_mmgr_errors_t secure_stop(secure_handle_t *secur);
e_mmgr_errors_t secure_event(secure_handle_t *secur);
secure_cb_t secure_get_callback(const secure_handle_t *secur);
int secure_get_fd(secure_handle_t *h);
const char *secure_get_error(secure_handle_t *h);

#endif
