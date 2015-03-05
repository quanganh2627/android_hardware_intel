/* full_rw.c
**
** Copyright 2013-2014 Intel Corporation
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
*/
#include "full_rw.h"

#include <unistd.h>
#include <errno.h>


ssize_t full_read(int fd, void *buf, size_t count)
{
    size_t total_read = 0;
    char *cbuf = buf;

    while (total_read != count) {

        ssize_t current_read = read(fd, cbuf + total_read, count - total_read);

        /* In case the reported error is EAGAIN or EINTR, just retry */
        if (current_read < 0 && errno != EAGAIN && errno != EINTR) {

            return -1;

        } else if (current_read == 0) {

            /* Zero means EOF or connection closed: full read is not possible and is reported
             * as error. errno is set to EIO: I/O  error
             */
            errno = EIO;
            return -1;
        }
        if (current_read > 0) {

            total_read += current_read;
        }
    }
    return count;
}

ssize_t full_write(int fd, const void *buf, size_t count)
{
    size_t total_write = 0;
    const char *cbuf = buf;

    while (total_write != count) {

        ssize_t current_write = write(fd, cbuf + total_write, count - total_write);

        /* In case the reported error is EAGAIN or EINTR, just retry */
        if (current_write < 0 && errno != EAGAIN && errno != EINTR) {

            return -1;
        }
        if (current_write > 0) {

            total_write += current_write;
        }
    }
    return count;
}
