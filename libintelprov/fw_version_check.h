#ifndef IFWI_VERSION_CHECK_H
#define IFWI_VERSION_CHECK_H
/*
 * Copyright (C) 2011 Intel Corporation
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
#include <stdint.h>

struct fw_version {
	uint8_t major;
	uint8_t minor;
};

/* Note: Chaabi version numbers can be read from the FIP
 * but not from SCU IPC */
struct firmware_versions {
	struct fw_version ifwi;
	struct fw_version scu;
	struct fw_version oem;
	struct fw_version punit;
	struct fw_version ia32;
	struct fw_version supp_ia32;
	struct fw_version chaabi_icache;
	struct fw_version chaabi_res;
	struct fw_version chaabi_ext;
};

struct fw_version_long {
	uint16_t major;
	uint16_t minor;
};

struct firmware_versions_long {
	struct fw_version_long scubootstrap;
	struct fw_version_long scu;
	struct fw_version_long ia32;
	struct fw_version_long valhooks;
	struct fw_version_long ifwi;
	struct fw_version_long chaabi;
	struct fw_version_long mia;
};

/* Query the SCU for current firmware versions and populate
 * the fields in v. Returns nonzero on error */
int get_current_fw_rev(struct firmware_versions *v);
int get_current_fw_rev_long(struct firmware_versions_long *v);

/* Assuming data points to a blob of memory containing an IFWI
 * firmware image, inpsect the FIP header inside it and
 * populate the fields in v. Returns nonzero on error */
int get_image_fw_rev(void *data, unsigned sz, struct firmware_versions *v);
int get_image_fw_rev_long(void *data, unsigned sz, struct firmware_versions_long *v);

/* Dump all the firmware component versions to stdout */
void dump_fw_versions(struct firmware_versions *v);
void dump_fw_versions_long(struct firmware_versions_long *v);

/* Compare versions v1 and v2, and return -1, 1, or 0 if v1 is less than,
 * greater than, or equal to v2, respectively */
int fw_vercmp(struct firmware_versions *v1, struct firmware_versions *v2);

/* Crack ifwi firmware file */
int crack_update_fw(const char *fw_file, struct fw_version *ifwi_version);

/* Crack ifwi firmware file to get the PTI Field. */
int crack_update_fw_pti_field(const char *fw_file, uint8_t *pti_field);

#endif
