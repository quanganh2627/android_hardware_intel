/*
 * Copyright 2011 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FLASH_IFWI_H
#define FLASH_IFWI_H

#include <stdlib.h>
#include "capsule.h"

#ifdef MRFLD
int check_ifwi_file(void *data, unsigned size);
int update_ifwi_file(void *data, unsigned size);
int write_token_umip(void *data, size_t size);
#else
int update_ifwi_file(const char *dnx, const char *ifwi);
#endif

int update_ifwi_image(void *data, size_t size, unsigned reset_flag);

int flash_ulpmc(void *data, unsigned sz);

#endif
