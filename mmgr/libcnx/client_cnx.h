/* Modem Manager - cnx header file
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

#ifndef __MMGR_CNX_HEADER__
#define __MMGR_CNX_HEADER__

#include <sys/types.h>
#include "errors.h"

#define MMGR_SOCKET_BASE "mmgr"
#define MMGR_SOCKET_LEN 8

#define DEFAULT_INST_ID 1

e_mmgr_errors_t cnx_open(int *fd, const char *cnx_name);
e_mmgr_errors_t cnx_close(int *fd);
e_mmgr_errors_t cnx_accept(int fd);
e_mmgr_errors_t cnx_read(int fd, void *data, size_t *len);
e_mmgr_errors_t cnx_write(int fd, void *data, size_t *len);
e_mmgr_errors_t cnx_get_name(char *cnx_name, size_t len, int id);

#endif                          /* __MMGR_CNX_HEADER__ */
