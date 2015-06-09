/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_LIBCAMERA_ATOMACC
#define ANDROID_LIBCAMERA_ATOMACC

#include "ia_types.h"
#include "sys/types.h"

/*
 * These functions are C wrappers for the acceleration methods in AtomISP.
 */

namespace android {

#if defined(__cplusplus)
extern "C" {
#endif

void *open_firmware(const char *fw_path, unsigned *size);

/* for standalone mode : used in HDR */
int load_firmware(void *isp, void *fw, unsigned size, unsigned *handle);

/* for extension mode : used in FA */
int load_firmware_pipe(void *isp, void *fw, unsigned size, unsigned *handle);

int load_firmware_ext(void *isp, void *fw, unsigned size, unsigned *handle, int dst);

int unload_firmware(void *isp, unsigned handle);

int map_firmware_arg (void *isp, void *val, size_t size, unsigned long *ptr);

int unmap_firmware_arg (void *isp, unsigned long val, size_t size);

int set_firmware_arg(void *isp, unsigned handle, unsigned num, void *val, size_t size);

int set_mapped_arg(void *isp, unsigned handle, unsigned mem, unsigned long val, size_t size);

int start_firmware(void *isp, unsigned handle);

int wait_for_firmware(void *isp, unsigned handle);

int abort_firmware(void *isp, unsigned handle, unsigned timeout);

int set_stage_state(void *isp, unsigned int handle, bool state);

int wait_stage_update(void *isp, unsigned int handle);

#if defined(__cplusplus)
} // extern "C"
#endif

} // namespace android

#endif // ANDROID_LIBCAMERA_ATOMACC
