/*
 * Copyright 2014 Intel Corporation
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

#ifndef _FLASH_IMAGE_H_
#define _FLASH_IMAGE_H_

#include <bootimg.h>

int flash_image(void *data, unsigned sz, const char *name);
int read_image(const char *name, void **data);
int flash_android_kernel(void *data, unsigned sz);
int flash_recovery_kernel(void *data, unsigned sz);
int flash_fastboot_kernel(void *data, unsigned sz);
int flash_splashscreen_image(void *data, unsigned sz);
int flash_esp(void *data, unsigned sz);
int full_gpt(void);
ssize_t bootimage_size(int fd, struct boot_img_hdr *hdr, bool include_sig);
int open_bootimage(const char *name);

#endif	/* _FLASH_IMAGE_H_ */
