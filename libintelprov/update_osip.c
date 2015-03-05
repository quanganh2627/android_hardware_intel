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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <getopt.h>
#include <unistd.h>
#include <strings.h>

#include "update_osip.h"
#include "util.h"

#define ANDROID_OS_NUM      0
#define RECOVERY_OS_NUM     1
#define FASTBOOT_OS_NUM     2

#define UEFI_FW_IDX         0

#define pr_perror(x)	fprintf(stderr, "%s failed: %s\n", x, strerror(errno))

#define MAX(x, y)       ((x) > (y) ? (x) : (y))
#define MIN(x, y)       ((x) < (y) ? (x) : (y))

#define ARRAY_SIZE(a)	(sizeof(a) / sizeof(a[0]))

#define FW_START_OFFSET	50 /* give space for GPT */
#define FW_MAX_LBA	2000
const uint32_t fw_lba_slots[] = {
	(FW_START_OFFSET + (FW_MAX_LBA * 0)),
	(FW_START_OFFSET + (FW_MAX_LBA * 1))
};


#define FW_SLOTS	ARRAY_SIZE(fw_lba_slots)

#define OS_START_OFFSET	(FW_START_OFFSET + (FW_MAX_LBA * FW_SLOTS))
const uint32_t os_lba_slots[] = {
	(OS_START_OFFSET + (OS_MAX_LBA * 0)),
	(OS_START_OFFSET + (OS_MAX_LBA * 1)),
	(OS_START_OFFSET + (OS_MAX_LBA * 2)),
	(OS_START_OFFSET + (OS_MAX_LBA * 3)),
	(OS_START_OFFSET + (OS_MAX_LBA * 4)),
	(OS_START_OFFSET + (OS_MAX_LBA * 5)),
	(OS_START_OFFSET + (OS_MAX_LBA * 6)),
	(OS_START_OFFSET + (OS_MAX_LBA * 7)),
};
#define OS_SLOTS	ARRAY_SIZE(os_lba_slots)

#define CLEAR_BIT(x, b)		((x) &= ~(1 << b))

uint8_t get_osip_crc(struct OSIP_header *osip)
{
	size_t i;
	unsigned char *dat;
	uint8_t crc;

	dat = (unsigned char *)osip;
	crc = dat[0] ^ dat[1];
	for (i = 2; i < sizeof(*osip); i++)
		crc ^= dat[i];
	return crc;
}

static int fixup_osii(struct OSII *osii, unsigned int *slot_idx,
		unsigned max_slots, const uint32_t *slots)
{
	if (*slot_idx >= max_slots - 1) {
		fprintf(stderr, "too many images for attr %d",
				osii->attribute);
		return -1;
	}
	osii->logical_start_block = slots[*slot_idx];
	(*slot_idx)++;
	return 0;
}

int verify_osip_sizes(struct OSIP_header *osip)
{
	int i;
	for (i = 0; i < osip->num_pointers; i++) {
		uint8_t attr = osip->desc[i].attribute;
		unsigned max_size_lba;
		unsigned img_size = osip->desc[i].size_of_os_image;

		switch (attr&(~1)) {
		case ATTR_SIGNED_FW:
		case ATTR_UNSIGNED_FW:
			max_size_lba = FW_MAX_LBA;
			break;
		case ATTR_SIGNED_KERNEL:
		case ATTR_UNSIGNED_KERNEL:
                case ATTR_SIGNED_COS:
                case ATTR_SIGNED_POS:
                case ATTR_SIGNED_ROS:
                case ATTR_SIGNED_COMB:
			max_size_lba = OS_MAX_LBA;
			break;
		default:
			/* Don't care about other image types */
			continue;
		}
		if (img_size > max_size_lba) {
			fprintf(stderr, "OSIP entry %d bad size %u, max is %u\n",
				i, img_size * LBA_SIZE, max_size_lba * LBA_SIZE);
			return -1;
		}
	}
	return 0;
}

int fixup_osip(struct OSIP_header *osip, uint32_t ptn_lba)
{
	unsigned int fw_ctr = 0;
	unsigned int os_ctr = 0;
	unsigned int ptn_ctr = 0;
	int i;
	int ret = -1;

	for (i = 0; i < osip->num_pointers; i++) {
		uint8_t attr = osip->desc[i].attribute;

		switch (attr&(~1)) {
		case ATTR_SIGNED_FW:
		case ATTR_UNSIGNED_FW:
			ret = fixup_osii(&osip->desc[i], &fw_ctr,
					FW_SLOTS, fw_lba_slots);
			break;
		case ATTR_SIGNED_KERNEL:
		case ATTR_SIGNED_POS:
		case ATTR_SIGNED_COS:
		case ATTR_SIGNED_ROS:
		case ATTR_SIGNED_COMB:
		case ATTR_UNSIGNED_KERNEL:
			ret = fixup_osii(&osip->desc[i], &os_ctr,
					OS_SLOTS, os_lba_slots);
			break;
		case ATTR_FILESYSTEM:
			if (ptn_ctr > 0) {
				fprintf(stderr, "Multiple filesystems in OSIP"
						"not supported!");
				return -1;
			}
			osip->desc[i].logical_start_block = ptn_lba;
			ptn_ctr++;
			ret = 0;
			break;
		default:
			fprintf(stderr, "Unhandled attribute %d!\n", attr);
			return -1;
		}
		if (ret) {
			fprintf(stderr, "failed to update osip[%d]\n", i);
			return -1;
		}
	}

	osip->header_checksum = 0;
	osip->header_checksum = get_osip_crc(osip);
	return ret;
}


static uint32_t get_free_lba(const uint32_t *slots, int num_slots, int slot_size,
		struct OSIP_header *osip)
{
	int freeslot=0;
	int i, j;
        /* this algorithm allows transition to droidboot
           we allow the already flashed osip not to use the slots
	   we still make sure we progressively only use the predefined slots

	   We try to find a slot that is not already at least partly used
	   by already flashed image.
	*/
	for (j = 0; j < num_slots; j++) {
		freeslot = 1;
		for (i = 0; i < osip->num_pointers; i++) {
			uint32_t lba = osip->desc[i].logical_start_block;
			uint32_t endlba = lba + 1 + osip->desc[i].size_of_os_image;
			if ((lba >= slots[j] && lba < (slots[j] + slot_size)) ||
			    (endlba >= slots[j] && endlba < (slots[j] + slot_size))) {
				freeslot = 0;
                                fprintf(stderr, "slot %d used by osip %d\n", j,i);
				break;
			}
		}
		if (freeslot)
			break;
	}
	if (!freeslot)
		return 0; /* All in use!! */
	return slots[j];
}


static int get_free_fw_lba(struct OSIP_header *osip)
{
	return get_free_lba(fw_lba_slots, FW_SLOTS, FW_MAX_LBA, osip);
}


static int get_free_os_lba(struct OSIP_header *osip)
{
	return get_free_lba(os_lba_slots, OS_SLOTS, OS_MAX_LBA, osip);
}


static void dump_osip_index(struct OSIP_header *osip, int i)
{
	printf("OSII[%d]:\n", i);
	printf("   os_rev: %02x.%02x\n   LBA: %d\n",
	     osip->desc[i].os_rev_major, osip->desc[i].os_rev_minor,
	     osip->desc[i].logical_start_block);
	printf("   ddr_load_address: 0x%08x\n   entry_point: 0x%08x\n"
	       "   size_of_os_image (sectors): %d\n   attribute: 0x%02x\n",
	     osip->desc[i].ddr_load_address, osip->desc[i].entry_point,
	     osip->desc[i].size_of_os_image, osip->desc[i].attribute);
}

void dump_osip_header(struct OSIP_header *osip)
{
	int i;

	printf("OSIP:\n");
	printf("sig 0x%x, header_size %d, revision %02X.%02X\n",
	     osip->sig, osip->header_size, osip->header_rev_major,
	     osip->header_rev_minor);
	printf("header_checksum 0x%hhx, num_pointers %d, num_images %d\n",
	     osip->header_checksum, osip->num_pointers, osip->num_images);

	for (i = 0; i < osip->num_pointers; i++)
		dump_osip_index(osip, i);
}

void dump_OS_page(struct OSIP_header *osip, int os_index, int numpages)
{
	int i, j, fd;
	static uint8_t buffer[8192 + 1024];
	unsigned short *temp;

	dump_osip_index(osip, os_index);

	for (i = 0; i < numpages; i++) {
		fd = open(MMC_DEV_POS, O_RDONLY);
		if (fd < 0)
			return;
		lseek(fd,
		      osip->desc[os_index].logical_start_block
		      * LBA_SIZE, SEEK_SET);
		memset((void *)buffer, 0, sizeof(buffer));
		if (read(fd, (void *)buffer, sizeof(buffer)) <
		    (int)sizeof(buffer)) {
			fprintf(stderr, "read failed\n");
			close(fd);
			return;
		}
		close(fd);
		for (j = 0; j < LBA_SIZE / 0x10; j++) {
			temp = (uint16_t *) (buffer + j * 0x10);
			printf("%x %hx %hx %hx %hx %hx %hx %hx %hx\n",
			       osip->desc[os_index].logical_start_block *
			       LBA_SIZE + i * LBA_SIZE + j * 0x10, temp[0],
			       temp[1], temp[2], temp[3], temp[4], temp[5],
			       temp[6], temp[7]);
		}
	}

}

int read_OSIP(struct OSIP_header *osip)
{
	int fd;
	int ret = -1;

	memset((void *)osip, 0, sizeof(*osip));
	fd = open(MMC_DEV_POS, O_RDONLY);
	if (fd < 0) {
		pr_perror("open");
		return -1;
	}

	if (safe_read(fd, osip, sizeof(*osip)) < 0) {
		fprintf(stderr, "read of osip failed\n");
		goto out;
	}
	if (get_osip_crc(osip) != 0) {
		fprintf(stderr, "OSIP is corrupt!");
		goto out;
	}
	ret = 0;
out:
	close(fd);
	return ret;
}

int write_OSIP(struct OSIP_header *osip)
{
	int fd;
	int ret;
	const unsigned char *what = (const unsigned char *)osip;
	int sz;

	osip->header_checksum = 0;
	osip->header_checksum = get_osip_crc(osip);

        sz=sizeof(*osip);

        fd = open(MMC_DEV_POS, O_RDWR);
        if (fd < 0) {
                printf("write_OSIP: Can't open device %s: %s\n",
                                MMC_DEV_POS, strerror(errno));
                return -1;
        }

       while (sz) {
                ret = write(fd, what, sz);
                if (ret <= 0 && errno != EINTR) {
                        printf("write_OSIP: Failed to write to %s: %s\n",
                                        MMC_DEV_POS, strerror(errno));
                        close(fd);
                        return -1;
                }
                what += ret;
                sz -= ret;
        }
        fsync(fd);
        close(fd);
        return 0;
}

static int crack_stitched_image(void *data, struct OSII **rec, uint8_t **blob)
{
	struct OSIP_header *osip = (struct OSIP_header *)data;
	if (!data)
		return -1;
	if (((struct OSIP_header *)data)->num_pointers != 1) {
		fprintf(stderr,
			"too many osii records in stiched data, "
			"is this an osupdate.bin file?\n");
		return -1;
	}			// we only know how to deal with trivial OS packages.

	*rec = &osip->desc[0];
	*blob = (uint8_t *) data + LBA_SIZE;
	return 0;
}

/* Pull the OS image data off the NAND into a .osupdate.bin for application of
 * a bsdiff patch */
int read_osimage_data(void **data, size_t * size, int osii_index)
{
	struct OSIP_header osip;
	struct OSIP_header file_osip;
	struct OSII *osii, *file_osii;
	unsigned char *blob;
	size_t blob_size;
	int fd;
	size_t i;

	if (read_OSIP(&osip)) {
		fprintf(stderr, "read_OSIP fails\n");
		return -1;
	}

	osii = &osip.desc[osii_index];

	/* Add one LBA for the OSIP header */
	blob_size = (osii->size_of_os_image + 1) * LBA_SIZE;

	blob = malloc(blob_size);
	*size = blob_size;
	*data = blob;

	if (!blob) {
		pr_perror("malloc");
		return -1;
	}

	/* Set up the fake OSIP header and write it */
	memset(&file_osip, 0, sizeof(file_osip));
	file_osip.sig = OSIP_SIG;
	file_osip.header_rev_minor = 0;
	file_osip.header_rev_major = 0x1;
	file_osip.num_pointers = 1;
	file_osip.num_images = 1;
	file_osip.header_size = (file_osip.num_pointers * 0x18) + 0x20;
	file_osii = &file_osip.desc[0];
	memcpy(file_osii, osii, sizeof(*osii));
	file_osii->logical_start_block = 1;
	if (file_osii->attribute != ATTR_SIGNED_FW &&
			file_osii->attribute != ATTR_UNSIGNED_FW) {
		/* The OS image might have been invalidated.
		 * Restore the pointers */
		file_osii->entry_point = ENTRY_POINT;
		file_osii->ddr_load_address = DDR_LOAD_ADDX;
	}

	/* Create the checksum */
	file_osip.header_checksum = 0;
	file_osip.header_checksum = get_osip_crc(&file_osip);
	memcpy(blob, &file_osip, sizeof(file_osip));

	/* Write out the rest of block 0. There's a long string of 0xFF, some
	 * empty space, and the MBR magic cookie */
	blob += 0x38;
	blob_size -= 0x38;
	for(i = 0 ; i < 384; i++) {
		*blob = 0xFF;
		blob++;
		blob_size--;
	}
	blob += 70;
	blob_size -= 70;
	blob[0] = 0x55;
	blob[1] = 0xAA;
	blob += 2;
	blob_size -= 2;

	fd = open(MMC_DEV_POS, O_RDONLY);
	if (fd < 0) {
		pr_perror("open");
		goto out_error;
	}

	if (lseek(fd, osii->logical_start_block * LBA_SIZE, SEEK_SET) < 0) {
		pr_perror("lseek");
		close(fd);
		goto out_error;
	}

	if (safe_read(fd, blob, blob_size)) {
		close(fd);
		goto out_error;
	}
	close(fd);
	return 0;

out_error:
	free(*data);
	*data = NULL;
	return -1;
}

#define OSIP_BACKUP_OFFSET 0xE0

int destroy_the_osip_backup(void)
{
	int fd;
	uint32_t sig;
	fd = open(MMC_DEV_POS, O_RDWR);
	if (fd < 0) {
		printf("destroy_the_osip_backup: can't open file %s: %s\n",
				MMC_DEV_POS, strerror(errno));
		return -1;
	}
	lseek(fd, OSIP_BACKUP_OFFSET, SEEK_SET);
	if (safe_read(fd, &sig, sizeof(sig))) {
		close(fd);
		return -1;
	}
	if (sig == OSIP_SIG) {
		lseek(fd, OSIP_BACKUP_OFFSET, SEEK_SET);
		sig = (uint32_t)(~0x0);
		write(fd, &sig, sizeof(sig));
	}
	close(fd);
	return 0;
}

int write_stitch_image(void *data, size_t size, int osii_index)
{
	return write_stitch_image_ex(data, size, osii_index, 0);
}

int write_stitch_image_ex(void *data, size_t size, int osii_index, int large_image)
{
	struct OSIP_header osip;
	struct OSII *osii;
	uint8_t *blob;
	unsigned max_size_lba;
	int fd;

	if (osii_index < 0 || (unsigned int)osii_index >
			ARRAY_SIZE(osip.reserved)) {
		fprintf(stderr, "Bad OSII index %d\n", osii_index);
		return -1;
	}

	printf("Writing %zu byte image to osip[%d]\n", size, osii_index);
	if (crack_stitched_image(data, &osii, &blob)) {
		fprintf(stderr, "crack_stitched_image fails\n");
		return -1;
	}
	if ((osii->size_of_os_image * LBA_SIZE) != size - LBA_SIZE) {
		fprintf(stderr, "data format is not correct! \n");
		return -1;
	}
	if (read_OSIP(&osip)) {
		fprintf(stderr, "read_OSIP fails\n");
		return -1;
	}
	destroy_the_osip_backup();

	/* We have a set of designated LBAs to write images whose size
	 * is the number of images + 1. The new data gets written to
	 * the empty slot, and the OSIP is only updated once the data
	 * is completely written out. That way we don't brick the device
	 * if we cut the power in the middle of writing an OS image */
	switch (osii->attribute&(~1)) {
	case ATTR_SIGNED_KERNEL:
        case ATTR_SIGNED_POS:
        case ATTR_SIGNED_COS:
        case ATTR_SIGNED_ROS:
        case ATTR_SIGNED_COMB:
        case ATTR_SIGNED_SPLASHSCREEN:
	case ATTR_UNSIGNED_KERNEL:
		if(large_image) {
			osii->logical_start_block = OS_START_OFFSET;
			max_size_lba = OS_MAX_LBA * OS_SLOTS;
		} else {
			osii->logical_start_block = get_free_os_lba(&osip);
			max_size_lba = OS_MAX_LBA;
		}
		break;
	case ATTR_SIGNED_FW:
	case ATTR_UNSIGNED_FW:
		osii->logical_start_block = get_free_fw_lba(&osip);
		max_size_lba = FW_MAX_LBA;
		break;
	default:
		fprintf(stderr, "write_stitch_image: I can't handle "
				"attribute type %d!\n", osii->attribute);
		return -1;
	}
	if (osii->logical_start_block == 0) {
		fprintf(stderr, "Couldn't find a place to put the image!\n");
		return -1;
	}
	if (osii->size_of_os_image > max_size_lba) {
		fprintf(stderr, "Image is too large! image=%u"
				" max=%u (sectors)\n", osii->size_of_os_image,
				max_size_lba);
		return -1;
	}
        if (osii->logical_start_block == 0) {
		fprintf(stderr, "unable to find free slot in emmc for osimage!\n");
                return -1;
        }
	if (osii_index >= osip.num_pointers) {
		osip.num_pointers = osii_index + 1;
		osip.header_size = (osip.num_pointers * 0x18) + 0x20;
	} else {
		/* Preserve invalidation */
		if (osip.desc[osii_index].ddr_load_address == 0 &&
				osip.desc[osii_index].entry_point == 0) {
			osii->ddr_load_address = osip.desc[osii_index].ddr_load_address;
			osii->entry_point = osip.desc[osii_index].entry_point;
		}
	}
	switch (osip.desc[osii_index].attribute&(~1)) {
	case ATTR_SIGNED_KERNEL:
	case ATTR_SIGNED_POS:
	case ATTR_SIGNED_COS:
	case ATTR_SIGNED_ROS:
	case ATTR_SIGNED_COMB:
	case ATTR_UNSIGNED_KERNEL:
	case ATTR_NOTUSED&(~1):
		memcpy(&(osip.desc[osii_index]), osii, sizeof(struct OSII));
		break;
	default:
		if ((osip.desc[osii_index].attribute&(~1)) == (osii->attribute&(~1))) {
			//if the attribute is the same, then overwrite it.
			memcpy(&(osip.desc[osii_index]), osii, sizeof(struct OSII));
		} else {
			memcpy(&(osip.desc[osip.num_pointers]), &(osip.desc[osii_index]), sizeof(struct OSII));
			memcpy(&(osip.desc[osii_index]), osii, sizeof(struct OSII));
			osip.num_pointers++;
			osip.header_size = (osip.num_pointers * 0x18) + 0x20;
		}
	}

	/* Write the blob of data out to the disk */
	fd = open(MMC_DEV_POS, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "fail open %s\n", MMC_DEV_POS);
		return -1;
	}
	if (lseek(fd, osii->logical_start_block * LBA_SIZE, SEEK_SET) < 0) {
		pr_perror("lseek");
		close(fd);
		return -1;
	}

	size -= LBA_SIZE;
	while (size) {
		int ret = write(fd, blob, size);
		if (ret <= 0 && errno != EINTR) {
			pr_perror("write");
			close(fd);
			return -1;
		}
		blob += ret;
		size -= ret;
	}

	fsync(fd);
	close(fd);

	/* New data is written out completely, OK to update the OSIP with
	 * the new LBA values now */
	return write_OSIP(&osip);
}


int get_named_osii_index(const char *destination)
{
	int index;
	int offset;
	int attr;
	struct OSIP_header osip;

	if (destination == NULL) {
		fprintf(stderr, "destination is a NULL pointer\n");
		return -1;
	}

	if (!strcmp(destination, SPLASHSCREEN_NAME))
		return ATTR_SIGNED_SPLASHSCREEN;

	if (read_OSIP(&osip)) {
		fprintf(stderr, "Can't read OSIP!\n");
		return -1;
	}
	attr = osip.desc[0].attribute;
	if (attr == ATTR_SIGNED_FW || attr == ATTR_UNSIGNED_FW)
		offset = 1;
	else
		offset = 0;

	if (!strcmp(destination, ANDROID_OS_NAME)) {
		index = ANDROID_OS_NUM;
	} else if (!strcmp(destination, RECOVERY_OS_NAME)) {
		index = RECOVERY_OS_NUM;
	} else if (!strcmp(destination, FASTBOOT_OS_NAME)) {
		index = FASTBOOT_OS_NUM;
	} else if (!strcmp(destination, DROIDBOOT_OS_NAME)) {
		index = FASTBOOT_OS_NUM;
	} else {
		fprintf(stderr, "unknown destination %s\n", destination);
		return -1;
	}
	return index + offset;
}

int get_attribute_osii_index(int attr)
{
	int i;
	struct OSIP_header osip;

	if (read_OSIP(&osip)) {
		fprintf(stderr, "Can't read OSIP!\n");
		return -1;
	}
	for (i = 0; i < osip.num_pointers; i++)
		if ((osip.desc[i].attribute&(~1)) == attr)
			return i;
	return osip.num_pointers;
}

int update_osii(char *destination, int ddr_load_address, int entry_point) {
	int osii_index;
	struct OSIP_header osip;

	// destination parameter validity is tested in function get_named_osii_index
	osii_index = get_named_osii_index(destination);
	if (osii_index < 0 || (unsigned int)osii_index >
			ARRAY_SIZE(osip.reserved)) {
		fprintf(stderr, "Bad OSII index %d\n", osii_index);
		return -1;
	}

	if (read_OSIP(&osip)) {
		fprintf(stderr, "Can't read OSIP!\n");
		return -1;
	}

	/* Update the pointers of the OS image */
	osip.desc[osii_index].ddr_load_address = ddr_load_address;
	osip.desc[osii_index].entry_point = entry_point;

	return write_OSIP(&osip);
}

int invalidate_osii(char *destination) {
	/* Invalidate the pointers of the OS image */
	return update_osii(destination, 0, 0);
}

int restore_osii(char *destination) {
	/* The OS image might have been invalidated.
	 * Restore the pointers */
	return update_osii(destination, DDR_LOAD_ADDX, ENTRY_POINT);
}
