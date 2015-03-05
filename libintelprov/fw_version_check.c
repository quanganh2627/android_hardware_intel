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

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdint.h>

#include "fw_version_check.h"

#define DEVICE_NAME	"/sys/devices/ipc/intel_fw_update.0/fw_info/fw_version"
#define DEVICE_NAME_ALT	"/sys/kernel/fw_update/fw_info/fw_version"
#define FIP_PATTERN	0x50494624
#define SCU_IPC_VERSION_LEN 16
#define SCU_IPC_VERSION_LEN_LONG 32
#define READ_SZ 256

#define SMIP_PATTERN	    0x50494D53
#define PTI_FIELD_OFFSET    0x030C

struct fip_version_block {
	uint8_t reserved;
	uint8_t minor;
	uint8_t major;
	uint8_t checksum;
};

struct Chaabi_rev {
	struct fip_version_block icache;
	struct fip_version_block resident;
	struct fip_version_block ext;
};

struct IFWI_rev {
	uint8_t minor;
	uint8_t major;
	uint16_t reserved;
};

struct FIP_header {
	uint32_t FIP_SIG;
	uint32_t header_info;
	struct fip_version_block ia32_rev;
	struct fip_version_block punit_rev;
	struct fip_version_block oem_rev;
	struct fip_version_block suppia32_rev;
	struct fip_version_block scu_rev;
	struct Chaabi_rev chaabi_rev;
	struct IFWI_rev ifwi_rev;
};

struct fip_version_block_long {
	uint16_t minor;
	uint16_t major;
	uint8_t  checksum;
	uint8_t  reserved8;
	uint16_t reserved16;
};

struct fip_version_block_chxx_long {
	uint16_t minor;
	uint16_t major;
	uint8_t  checksum;
	uint8_t  reserved8;
	uint16_t reserved16;
	uint16_t size;
	uint16_t dest;
};

struct FIP_header_long {
	uint32_t FIP_SIG;
	struct fip_version_block_long umip_rev;
	struct fip_version_block_long spat_rev;
	struct fip_version_block_long spct_rev;
	struct fip_version_block_long rpch_rev;
	struct fip_version_block_long ch00_rev;
	struct fip_version_block_long mipd_rev;
	struct fip_version_block_long mipn_rev;
	struct fip_version_block_long scuc_rev;
	struct fip_version_block_long hvm_rev;
	struct fip_version_block_long mia_rev;
	struct fip_version_block_long ia32_rev;
	struct fip_version_block_long oem_rev;
	struct fip_version_block_long ved_rev;
	struct fip_version_block_long vec_rev;
	struct fip_version_block_long mos_rev;
	struct fip_version_block_long pos_rev;
	struct fip_version_block_long cos_rev;
	struct fip_version_block_chxx_long ch01_rev;
	struct fip_version_block_chxx_long ch02_rev;
	struct fip_version_block_chxx_long ch03_rev;
	struct fip_version_block_chxx_long ch04_rev;
	struct fip_version_block_chxx_long ch05_rev;
	struct fip_version_block_chxx_long ch06_rev;
	struct fip_version_block_chxx_long ch07_rev;
	struct fip_version_block_chxx_long ch08_rev;
	struct fip_version_block_chxx_long ch09_rev;
	struct fip_version_block_chxx_long ch10_rev;
	struct fip_version_block_chxx_long ch11_rev;
	struct fip_version_block_chxx_long ch12_rev;
	struct fip_version_block_chxx_long ch13_rev;
	struct fip_version_block_chxx_long ch14_rev;
	struct fip_version_block_chxx_long ch15_rev;
	struct fip_version_block_long dnx_rev;
	struct fip_version_block_long reserved0_rev;
	struct fip_version_block_long reserved1_rev;
	struct fip_version_block_long ifwi_rev;
};

static int read_fw_revision(unsigned int *fw_revision, int len)
{
	int i, fw_info, ret;
	const char *sep = " ";
	char *p, *save;
	char buf[READ_SZ];

	fw_info = open(DEVICE_NAME, O_RDONLY);
	if (fw_info < 0) {
		fw_info = open(DEVICE_NAME_ALT, O_RDONLY);
		if (fw_info < 0) {
			fprintf(stderr, "failed to open %s or %s", DEVICE_NAME, DEVICE_NAME_ALT);
			return fw_info;
		}
	}

	ret = read(fw_info, buf, READ_SZ - 1);
	if (ret < 0) {
		fprintf(stderr, "failed to read fw_revision, ret = %d\n", ret);
		goto err;
	}

	buf[ret] = 0;
	p = strtok_r(buf, sep, &save);
	for(i = 0; p && i < len; i++) {
		ret = sscanf(p, "%x", &fw_revision[i]);
		if (ret != 1) {
			fprintf(stderr, "failed to parse fw_revision, ret = %d\n", ret);
			goto err;
		}
		p = strtok_r(NULL, sep, &save);
	}
	ret = 0;

err:
	close(fw_info);
	return ret;
}

/* Bytes in scu_ipc_version after the ioctl():
 * 00 SCU RT Firmware Minor Revision
 * 01 SCU RT Firmware Major Revision
 * 02 SCU ROM Firmware Minor Revision
 * 03 SCU ROM Firmware Major Revision 
 * 04 P-unit Microcode Minor Revision
 * 05 P-unit Microcode Major Revision
 * 06 IA-32 Firmware Minor Revision
 * 07 IA-32 Firmware Major Revision
 * 08 FTL Driver Minor Revision
 * 09 FTL Driver Major Revision
 * 10 Validation Hooks / OEM Minor Revision
 * 11 Validation Hooks / OEM Major Revision
 * 12 n/a
 * 13 n/a
 * 14 IFWI Minor Revision
 * 15 IFWI Major Revision
 */
int get_current_fw_rev(struct firmware_versions *v)
{
	int ret;
	unsigned int fw_revision[SCU_IPC_VERSION_LEN] = { 0 };

	ret = read_fw_revision(fw_revision, SCU_IPC_VERSION_LEN);
	if (ret)
		return ret;

	v->ifwi.major = fw_revision[15];
	v->ifwi.minor = fw_revision[14];
	v->scu.major = fw_revision[1];
	v->scu.minor = fw_revision[0];
	v->oem.major = fw_revision[11];
	v->oem.minor = fw_revision[10];
	v->punit.major = fw_revision[5];
	v->punit.minor = fw_revision[4];
	v->ia32.major = fw_revision[7];
	v->ia32.minor = fw_revision[6];
	v->supp_ia32.major = fw_revision[9];
	v->supp_ia32.minor = fw_revision[8];
	/* Can't read these from the SCU >:( */
	v->chaabi_icache.major = 0;
	v->chaabi_icache.minor = 0;
	v->chaabi_res.major = 0;
	v->chaabi_res.minor = 0;
	v->chaabi_ext.major = 0;
	v->chaabi_ext.minor = 0;

	return ret;
}

/* Bytes in scu_ipc_version after the ioctl():

 * 00 SCU Boot Strap Firmware Minor Revision Low
 * 01 SCU Boot Strap Firmware Minor Revision High
 * 02 SCU Boot Strap Firmware Major Revision Low
 * 03 SCU Boot Strap Firmware Major Revision High

 * 04 SCU Firmware Minor Revision Low
 * 05 SCU Firmware Minor Revision High
 * 06 SCU Firmware Major Revision Low
 * 07 SCU Firmware Major Revision High

 * 08 IA Firmware Minor Revision Low
 * 09 IA Firmware Minor Revision High
 * 10 IA Firmware Major Revision Low
 * 11 IA Firmware Major Revision High

 * 12 Validation Hooks Firmware Minor Revision Low
 * 13 Validation Hooks Firmware Minor Revision High
 * 14 Validation Hooks Firmware Major Revision Low
 * 15 Validation Hooks Firmware Major Revision High

 * 16 IFWI Firmware Minor Revision Low
 * 17 IFWI Firmware Minor Revision High
 * 18 IFWI Firmware Major Revision Low
 * 19 IFWI Firmware Major Revision High

 * 20 Chaabi Firmware Minor Revision Low
 * 21 Chaabi Firmware Minor Revision High
 * 22 Chaabi Firmware Major Revision Low
 * 23 Chaabi Firmware Major Revision High

 * 24 mIA Firmware Minor Revision Low
 * 25 mIA Firmware Minor Revision High
 * 26 mIA Firmware Major Revision Low
 * 27 mIA Firmware Major Revision High

 */
int get_current_fw_rev_long(struct firmware_versions_long *v)
{
	int ret;
	unsigned int fw_revision[SCU_IPC_VERSION_LEN_LONG] = { 0 };

	ret = read_fw_revision(fw_revision, SCU_IPC_VERSION_LEN_LONG);
	if (ret)
		return ret;

	v->scubootstrap.minor = fw_revision[1] << 8 | fw_revision[0];
	v->scubootstrap.major = fw_revision[3] << 8 | fw_revision[2];
	v->scu.minor = fw_revision[5]  << 8 | fw_revision[4];
	v->scu.major = fw_revision[7]  << 8 | fw_revision[6];
	v->ia32.minor = fw_revision[9]  << 8 | fw_revision[8];
	v->ia32.major = fw_revision[11] << 8 | fw_revision[10];
	v->valhooks.minor = fw_revision[13] << 8 | fw_revision[12];
	v->valhooks.major = fw_revision[15] << 8 | fw_revision[14];
	v->ifwi.minor = fw_revision[17] << 8 | fw_revision[16];
	v->ifwi.major = fw_revision[19] << 8 | fw_revision[18];
	v->chaabi.minor = fw_revision[21] << 8 | fw_revision[20];
	v->chaabi.major = fw_revision[23] << 8 | fw_revision[22];
	v->mia.minor = fw_revision[25] << 8 | fw_revision[24];
	v->mia.major = fw_revision[27] << 8 | fw_revision[26];

	return ret;
}

int fw_vercmp(struct firmware_versions *v1, struct firmware_versions *v2)
{
	uint16_t ver1 = (v1->ifwi.major << 8) + v1->ifwi.minor;
	uint16_t ver2 = (v2->ifwi.major << 8) + v2->ifwi.minor;

	if (ver1 < ver2)
		return -1;
	else if (ver1 > ver2)
		return 1;
	else
		return 0;
}

void dump_fw_versions(struct firmware_versions *v)
{
	printf("         ifwi: %02X.%02X\n", v->ifwi.major, v->ifwi.minor);
	printf("---- components ----\n");
	printf("          scu: %02X.%02X\n", v->scu.major, v->scu.minor);
	printf("        punit: %02X.%02X\n", v->punit.major, v->punit.minor);
	printf("    hooks/oem: %02X.%02X\n", v->oem.major, v->oem.minor);
	printf("         ia32: %02X.%02X\n", v->ia32.major, v->ia32.minor);
	printf("     suppia32: %02X.%02X\n", v->supp_ia32.major, v->supp_ia32.minor);
	printf("chaabi icache: %02X.%02X\n", v->chaabi_icache.major, v->chaabi_icache.minor);
	printf("   chaabi res: %02X.%02X\n", v->chaabi_res.major, v->chaabi_res.minor);
	printf("   chaabi ext: %02X.%02X\n", v->chaabi_ext.major, v->chaabi_ext.minor);
}

int get_image_fw_rev(void *data, unsigned sz, struct firmware_versions *v)
{
	struct FIP_header fip;
	unsigned char *databytes = (unsigned char *)data;
	int magic;
	int magic_found = 0;

	if (v == NULL) {
		fprintf(stderr, "Null pointer !\n");
		return -1;
	}
	else
		memset((void *)v, 0, sizeof(struct firmware_versions));

	while (sz >= sizeof(fip)){

		/* Scan for the FIP magic */
		while (sz >= sizeof(fip)) {
			memcpy(&magic, databytes, sizeof(magic));
			if (magic == FIP_PATTERN)
			{
				magic_found = 1;
				break;
			}
			databytes += sizeof(magic);
			sz -= sizeof(magic);
		}

		if (!magic_found) {
			fprintf(stderr, "Couldn't find FIP magic in image!\n");
			return -1;
		}
		if (sz < sizeof(fip)) {
			break;
		}

		memcpy(&fip, databytes, sizeof(fip));

		/* don't update if null */
		if (fip.ifwi_rev.major != 0)
			v->ifwi.major = fip.ifwi_rev.major;
		if (fip.ifwi_rev.minor != 0)
			v->ifwi.minor = fip.ifwi_rev.minor;
		if (fip.scu_rev.major != 0)
			v->scu.major = fip.scu_rev.major;
		if (fip.scu_rev.minor != 0)
			v->scu.minor = fip.scu_rev.minor;
		if (fip.oem_rev.major != 0)
			v->oem.major = fip.oem_rev.major;
		if (fip.oem_rev.minor != 0)
			v->oem.minor = fip.oem_rev.minor;
		if (fip.punit_rev.major != 0)
			v->punit.major = fip.punit_rev.major;
		if (fip.punit_rev.minor != 0)
			v->punit.minor = fip.punit_rev.minor;
		if (fip.ia32_rev.major != 0)
			v->ia32.major = fip.ia32_rev.major;
		if (fip.ia32_rev.minor != 0)
			v->ia32.minor = fip.ia32_rev.minor;
		if (fip.suppia32_rev.major != 0)
			v->supp_ia32.major = fip.suppia32_rev.major;
		if (fip.suppia32_rev.minor != 0)
			v->supp_ia32.minor = fip.suppia32_rev.minor;
		if (fip.chaabi_rev.icache.major != 0)
			v->chaabi_icache.major = fip.chaabi_rev.icache.major;
		if (fip.chaabi_rev.icache.minor != 0)
			v->chaabi_icache.minor = fip.chaabi_rev.icache.minor;
		if (fip.chaabi_rev.resident.major != 0)
			v->chaabi_res.major = fip.chaabi_rev.resident.major;
		if (fip.chaabi_rev.resident.minor != 0)
			v->chaabi_res.minor = fip.chaabi_rev.resident.minor;
		if (fip.chaabi_rev.ext.major != 0)
			v->chaabi_ext.major = fip.chaabi_rev.ext.major;
		if (fip.chaabi_rev.ext.minor != 0)
			v->chaabi_ext.minor = fip.chaabi_rev.ext.minor;

		databytes += sizeof(magic);
		sz -= sizeof(magic);
	}

	return 0;
}

int get_image_fw_rev_long(void *data, unsigned sz, struct firmware_versions_long *v)
{
	struct FIP_header_long fip;
	unsigned char *databytes = (unsigned char *)data;
	int magic;
	int magic_found = 0;

	if (v == NULL) {
		fprintf(stderr, "Null pointer !\n");
		return -1;
	}
	else
		memset((void *)v, 0, sizeof(struct firmware_versions_long));

	while (sz >= sizeof(fip)){

		/* Scan for the FIP magic */
		while (sz >= sizeof(fip)) {
			memcpy(&magic, databytes, sizeof(magic));
			if (magic == FIP_PATTERN)
			{
				magic_found = 1;
				break;
			}
			databytes += sizeof(magic);
			sz -= sizeof(magic);
		}

		if (!magic_found) {
			fprintf(stderr, "Couldn't find FIP magic in image!\n");
			return -1;
		}
		if (sz < sizeof(fip)) {
			break;
		}

		memcpy(&fip, databytes, sizeof(fip));

		/* not available in ifwi file */
		v->scubootstrap.minor = 0;
		v->scubootstrap.major = 0;

		/* don't update if null */
		if(fip.scuc_rev.minor != 0)
			v->scu.minor = fip.scuc_rev.minor;
		if(fip.scuc_rev.major != 0)
			v->scu.major = fip.scuc_rev.major;
		if(fip.ia32_rev.minor != 0)
			v->ia32.minor = fip.ia32_rev.minor;
		if(fip.ia32_rev.major != 0)
			v->ia32.major = fip.ia32_rev.major;
		if(fip.oem_rev.minor != 0)
			v->valhooks.minor = fip.oem_rev.minor;
		if(fip.oem_rev.major != 0)
			v->valhooks.major = fip.oem_rev.major;
		if(fip.ifwi_rev.minor != 0)
			v->ifwi.minor = fip.ifwi_rev.minor;
		if(fip.ifwi_rev.major != 0)
			v->ifwi.major = fip.ifwi_rev.major;
		if(fip.ch00_rev.minor != 0)
			v->chaabi.minor = fip.ch00_rev.minor;
		if(fip.ch00_rev.major != 0)
			v->chaabi.major = fip.ch00_rev.major;
		if(fip.mia_rev.minor != 0)
			v->mia.minor = fip.mia_rev.minor;
		if(fip.mia_rev.major != 0)
			v->mia.major = fip.mia_rev.major;

		databytes += sizeof(magic);
		sz -= sizeof(magic);
	}

	return 0;
}

int crack_update_fw(const char *fw_file, struct fw_version *ifwi_version){
	struct FIP_header fip;
	FILE *fd;
	int tmp = 0;
	int location;

	memset((void *)&fip, 0, sizeof(fip));

	if ((fd = fopen(fw_file, "rb")) == NULL) {
		fprintf(stderr, "fopen error: Unable to open file\n");
		return -1;
	}
	ifwi_version->major = 0;
	ifwi_version->minor = 0;

	while ((ifwi_version->minor == 0) && (ifwi_version->major == 0)){

		while (tmp != FIP_PATTERN) {
			int cur;
			fread(&tmp, sizeof(int), 1, fd);
			if (ferror(fd) || feof(fd)) {
				fprintf(stderr, "find FIP_pattern failed\n");
				fclose(fd);
				return -1;
			}
			cur = ftell(fd) - sizeof(int);
			fseek(fd, cur + sizeof(char), SEEK_SET);
		}
		location = ftell(fd) - sizeof(char);

		fseek(fd, location, SEEK_SET);
		fread((void *)&fip, sizeof(fip), 1, fd);
		if (ferror(fd) || feof(fd)) {
			fprintf(stderr, "read of FIP_header failed\n");
			fclose(fd);
			return -1;
		}
		ifwi_version->major = fip.ifwi_rev.major;
		ifwi_version->minor = fip.ifwi_rev.minor;
		tmp=0;

	}
	fclose(fd);

	return 0;
}

int crack_update_fw_pti_field(const char *fw_file, uint8_t *pti_field){
	FILE *fd;
	int tmp = 0;
	int location;
	uint8_t smip_data;

	if ((fd = fopen(fw_file, "rb")) == NULL) {
		fprintf(stderr, "fopen error: Unable to open file\n");
		return -1;
	}

	*pti_field = 0x00000000;

	/* Search for SMIP area. */
	while (tmp != SMIP_PATTERN) {
		int cur;
		fread(&tmp, sizeof(int), 1, fd);
		if (ferror(fd) || feof(fd)) {
			fprintf(stderr, "find SMIP_PATTERN failed\n");
			fclose(fd);
			return -1;
		}
		cur = ftell(fd) - sizeof(int);

		fseek(fd, cur + sizeof(char), SEEK_SET);
		if (ferror(fd) || feof(fd)) {
			fprintf(stderr, "reposition to SMIP_PATTERN failed\n");
			fclose(fd);
			return -1;
		}
	}
	location = ftell(fd) - sizeof(char);

	/* Move to the offset where the PIT field is located. */
	fseek(fd, location + PTI_FIELD_OFFSET, SEEK_SET);
	if (ferror(fd) || feof(fd)) {
		fprintf(stderr, "reposition to PTI field offset failed\n");
		fclose(fd);
		return -1;
	}

	fread(&smip_data, 1, 1, fd);
	if (ferror(fd) || feof(fd)) {
		fprintf(stderr, "read of FIP_header failed\n");
		fclose(fd);
		return -1;
	}

	/* Fill given param with found value. */
	*pti_field = smip_data;

	fclose(fd);
	return 0;
}

