/* full_rw.h
 **
 ** Copyright 2013 Intel Corporation
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
#ifndef __FULL_RW__
#define __FULL_RW__

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>


/**
 * Attempts to read up to count bytes from file descriptor fd into the buffer starting at buf.
 * It internaly calls read() as many times as needed to read up to count bytes.
 * A retry will occur if the amount of red bytes is not yet equal to count, or each time the
 * reported error is EAGAIN or EINTR.
 * If read() reports an error (except EAGAIN and EINTR), it return -1 as error and errno is updated.
 * If read() returns 0, full_read() returns -1 as error with errno set to EIO.
 *
 * @param[in] fd file descriptor
 * @param[out] buf destination buffer
 * @param[in] count number of bytes to be read
 *
 * @return count in case of success, -1 otherwise with errno updated
 */
ssize_t full_read(int fd, void* buf, size_t count);

/**
 * Writes up to count bytes from the buffer pointed buf to the file referred to by the file
 * descriptor fd.
 * It internaly calls write() as many times as needed to write up to count bytes.
 * A retry will occur if the amount of red bytes is not yet equal to count, or each time the
 * reported error is EAGAIN or EINTR.
 * If write() reports an error (except EAGAIN and EINTR), it return -1 as error and errno is
 * updated.
 *
 * @param[in] fd file descriptor
 * @param[in] buf source buffer
 * @param[in] count number of bytes to be written
 *
 * @return count in case of success, -1 otherwise with errno updated
 */
ssize_t full_write(int fd, const void* buf, size_t count);

#ifdef __cplusplus
}
#endif
#endif
