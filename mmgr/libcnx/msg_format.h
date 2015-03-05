/* Modem Manager - message format header file
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

#ifndef __MSG_FORMAT_HEADER__
#define __MSG_FORMAT_HEADER__

#define MMGR_FW_OPERATIONS
#include "mmgr_cli.h"
#include "errors.h"

#define SIZE_HEADER (sizeof(uint32_t) * 3)

typedef struct msg_hdr {
    uint32_t id;
    uint32_t ts;
    uint32_t len;
} msg_hdr_t;

typedef struct msg {
    msg_hdr_t hdr;
    char *data;
} msg_t;

inline void deserialize_uint32(char **buffer, uint32_t *value);
inline void deserialize_int(char **buffer, int *value);
inline void deserialize_size_t(char **buffer, size_t *value);

inline void serialize_uint32(char **buffer, uint32_t value);
inline void serialize_int(char **buffer, int value);
inline void serialize_size_t(char **buffer, size_t value);

e_mmgr_errors_t msg_prepare(msg_t *msg, char **msg_data, e_mmgr_events_t id,
                            size_t *size);
e_mmgr_errors_t msg_get_header(int fd, msg_hdr_t *hdr);
e_mmgr_errors_t msg_delete(msg_t *msg);
e_mmgr_errors_t msg_set_empty(msg_t *msg, mmgr_cli_event_t *request);
e_mmgr_errors_t msg_set_header(msg_t *msg);

#endif                          /* __MSG_FORMAT_HEADER__ */
