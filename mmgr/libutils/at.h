/* Modem Manager - at header file
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

#ifndef __MMGR_AT_HEADER__
#define __MMGR_AT_HEADER__

#define AT_READ_MAX_RETRIES 4

#define AT_ANSWER_NO_TIMEOUT 0
#define AT_ANSWER_SHORT_TIMEOUT 2500
#define AT_ANSWER_LONG_TIMEOUT 30000

#include "errors.h"

e_mmgr_errors_t send_at_retry(int fd_tty, const char *at_cmd, int at_cmd_size,
                              int retry, int timeout);

#endif                          /* __MMGR_AT_HEADER__ */
