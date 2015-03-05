/* Modem Manager - tty header file
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

#ifndef __MMGR_TTY_HEADER__
#define __MMGR_TTY_HEADER__

#include <sys/epoll.h>
#include "errors.h"

e_mmgr_errors_t tty_open(const char *tty_name, int *fd);
e_mmgr_errors_t tty_close(int *fd);
e_mmgr_errors_t tty_set_termio(int fd);
e_mmgr_errors_t tty_write(int fd, const char *data, int data_size);
e_mmgr_errors_t tty_listen_fd(int epollfd, int fd, int events);
e_mmgr_errors_t tty_init_listener(int *epollfd);
e_mmgr_errors_t tty_wait_for_event(int fd, int timeout);
e_mmgr_errors_t tty_read(int fd, char *data, int *data_size, int max_retries);

#endif                          /* __MMGR_TTY_HEADER__ */
