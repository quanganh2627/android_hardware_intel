/* Modem Manager - modem folder header file
**
** ** Copyright (C) Intel 2014
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

#ifndef __MMGR_FOLDER_HEADER__
#define __MMGR_FOLDER_HEADER__

#include <stdbool.h>

int folder_create(const char *path);
int folder_remove(const char *path);
bool folder_exist(const char *path);

#endif /* __MMGR_FOLDER_HEADER__ */
