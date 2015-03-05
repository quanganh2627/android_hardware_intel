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

#ifndef UPDATE_OSIP_H
#define UPDATE_OSIP_H
#include <stdlib.h>
#include <stdint.h>


#ifndef STORAGE_BASE_PATH
#define STORAGE_BASE_PATH "/dev/block/mmcblk0"
#endif

#ifndef STORAGE_PARTITION_FORMAT
#define STORAGE_PARTITION_FORMAT ""
#endif
#define DDR_LOAD_ADDX       0x01100000
#define ENTRY_POINT         0x01101000

#define ANDROID_OS_NAME     "boot"
#define RECOVERY_OS_NAME    "recovery"
#define FASTBOOT_OS_NAME    "fastboot"
#define DROIDBOOT_OS_NAME   "droidboot"
#define TEST_OS_NAME        "testos"
#define SPLASHSCREEN_NAME   "splashscreen"
#define ESP_PART_NAME       "ESP"
#define MAX_OSIP_DESC 	    7
/* mfld-structures section 2.7.1 mfld-fas v0.8*/
#define OSIP_SIG 0x24534f24	/* $OS$ */

struct OSII {			//os image identifier
	uint16_t os_rev_minor;
	uint16_t os_rev_major;
	uint32_t logical_start_block;	//units defined by get_block_size() if
	//reading/writing to/from nand, units of
	//512 bytes if cracking a stitched image
	uint32_t ddr_load_address;
	uint32_t entry_point;
	uint32_t size_of_os_image;	//units defined by get_page_size() if
	//reading/writing to/from nand, units of
	//512 bytes if cracking a stitched image
	uint8_t attribute;
	uint8_t reserved[3];
};

struct OSIP_header {		// os image profile
	uint32_t sig;
	uint8_t intel_reserved;	// was header_size;       // in bytes
	uint8_t header_rev_minor;
	uint8_t header_rev_major;
	uint8_t header_checksum;
	uint8_t num_pointers;
	uint8_t num_images;
	uint16_t header_size;	//was security_features;
	uint32_t reserved[5];

	struct OSII desc[MAX_OSIP_DESC];
};

uint8_t get_osip_crc(struct OSIP_header *osip);
int write_OSIP(struct OSIP_header *osip);
int read_OSIP(struct OSIP_header *osip);
void dump_osip_header(struct OSIP_header *osip);
void dump_OS_page(struct OSIP_header *osip, int os_index, int numpages);

int read_osimage_data(void **data, size_t *size, int osii_index);
int write_stitch_image(void *data, size_t size, int osii_index);
int write_stitch_image_ex(void *data, size_t size, int osii_index, int large_image);
int get_named_osii_index(const char *destination);
int invalidate_osii(char *destination);
int restore_osii(char *destination);
int get_attribute_osii_index(int attr);
int fixup_osip(struct OSIP_header *osip, uint32_t ptn_lba);
int verify_osip_sizes(struct OSIP_header *osip);

#define ATTR_SIGNED_KERNEL      0
#define ATTR_UNSIGNED_KERNEL    1
#define ATTR_SIGNED_COS		0x0A
#define ATTR_SIGNED_POS		0x0E
#define ATTR_SIGNED_ROS		0x0C
#define ATTR_SIGNED_COMB	0x10
#define ATTR_SIGNED_FW          8
#define ATTR_UNSIGNED_FW        9
#define ATTR_FILESYSTEM		3
#define ATTR_NOTUSED		(0xff)
#define ATTR_SIGNED_SPLASHSCREEN  0x04

#define LBA_SIZE	512
#define OS_MAX_LBA	32000
#define MMC_DEV_POS STORAGE_BASE_PATH

#endif
