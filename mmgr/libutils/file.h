/* Modem Manager - file header file
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

#ifndef __MMGR_FILE_HEADER__
#define __MMGR_FILE_HEADER__

#include <sys/stat.h>
#include <stdbool.h>
#include "errors.h"

#define OPEN_MODE_RW_UGO (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | \
                          S_IWOTH)
#define SYSFS_OPEN_MODE S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP
#define MMGR_UMASK (S_IXUSR | S_IXGRP | S_IWOTH | S_IXOTH)

e_mmgr_errors_t file_write(const char *path, unsigned long mode, char *value,
                           size_t size);

e_mmgr_errors_t file_read(const char *path, char *value, size_t size);

bool file_exist(const char *path);
char **file_find(const char *folder, const char *regexp, size_t *found);
char **file_find_ext(const char *folder, const char *extension, size_t *nb);

e_mmgr_errors_t file_copy(const char *src, const char *dst, mode_t dst_mode);

#endif                          /* __MMGR_FILE_HEADER__ */
