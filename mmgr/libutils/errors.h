/* Modem Manager - errors header file
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

#ifndef __MMGR_ERRORS_HEADER__
#define __MMGR_ERRORS_HEADER__

#include <stdlib.h>

typedef enum e_mmgr_errors {
    /* General */
    E_ERR_SUCCESS = 0,
    E_ERR_FAILED,
    /* TTY errors */
    E_ERR_TTY_BAD_FD,
    E_ERR_TTY_POLLHUP,
    E_ERR_TTY_TIMEOUT,
    /* Client */
    E_ERR_DISCONNECTED,
} e_mmgr_errors_t;

#define CLOSED_FD -1

#define xstr(s) str(s)
#define str(s) #s

#define ASSERT(exp) do { \
        if (!(exp)) { \
            LOG_ERROR("%s:%d Assertion '" xstr(exp) "'", __FILE__, __LINE__); \
            abort(); \
        } \
} while (0)

#endif                          /* __MMGR_ERRORS_HEADER__ */
