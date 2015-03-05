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

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <bootimg.h>
#include "util.h"
#include "flash_image.h"
#include "gpt/partlink/partlink.h"
#include "update_osip.h"

int full_gpt(void)
{
	struct stat buf;

	return (stat(BASE_PLATFORM_INTEL_LABEL"/fastboot", &buf) == 0
		&& S_ISBLK(buf.st_mode));
}

char *get_gpt_path(const char *name)
{
	char *block_dev = malloc(BUFSIZ);
	char base[] = BASE_PLATFORM_INTEL_LABEL"/";
	struct stat buf;

	if (!name) {
		error("%s: Passed name is empty.\n", __func__);
		goto error;
	}

	if (strlen(name) > BUFSIZ - sizeof(base)) {
		error("%s: Buffer is not large enough to build block device path.\n", __func__);
		goto error;
	}

	if (!block_dev) {
		error("%s: Failed to allocate mem for block dev.\n", __func__);
		goto error;
	}

	strncpy(block_dev, base, sizeof(base));
	strncpy(block_dev + sizeof(base) - 1, name, strlen(name) + 1);

	if (stat(block_dev, &buf) != 0 || !S_ISBLK(buf.st_mode))
		goto error;

	return block_dev;
error:
	if (block_dev)
		free(block_dev);
	return NULL;
}

int flash_image(void *data, unsigned sz, const char *name)
{
	if (full_gpt()) {
		char *block_dev;
		int ret;

		block_dev = get_gpt_path(name);
		if (!block_dev)
			return -1;

		ret = file_write(block_dev, data, sz);
		free(block_dev);
		return ret;
	} else {
		int index = get_named_osii_index(name);

		if (index < 0) {
			error("Can't find OSII index!!\n");
			return -1;
		}

		return write_stitch_image(data, sz, index);
	}
}

static int pages(struct boot_img_hdr *hdr, int blob_size)
{
        return (blob_size + hdr->page_size - 1) / hdr->page_size;
}

int open_bootimage(const char *name)
{
	char *block_dev;
	int fd = -1;

	block_dev = get_gpt_path(name);
	if (!block_dev)
		goto out;

	fd =  open(block_dev, O_RDONLY);
	if (fd < 0)
		error("Failed to open %s: %s\n", block_dev, strerror(errno));

	free(block_dev);
out:
	return fd;
}

/* Fill hdr with bootimage's header and return image's size */
ssize_t bootimage_size(int fd, struct boot_img_hdr *hdr, bool include_sig)
{
	ssize_t size = -1;

	if (safe_read(fd, hdr, sizeof(*hdr))) {
		error("Failed to read image header: %s\n", strerror(errno));
		goto out;
	}

	if (memcmp(hdr->magic, BOOT_MAGIC, sizeof(hdr->magic))) {
		error("Image is corrupted (bad magic)\n");
		goto out;
	}

	size = (1 + pages(hdr, hdr->kernel_size) +
	       pages(hdr, hdr->ramdisk_size) +
	       pages(hdr, hdr->second_size)) * hdr->page_size;

	if (include_sig)
		size += pages(hdr, hdr->sig_size) * hdr->page_size;

out:
	return size;
}

static int read_image_full_gpt(const char *name, void **data)
{
	ssize_t size;
	struct boot_img_hdr hdr;
	int ret = -1;
	int fd;

	fd = open_bootimage(name);
	if (fd < 0) {
		error("Failed to open %s image\n", name);
		goto out;
	}

	size = bootimage_size(fd, &hdr, true);
	if (size <= 0) {
		error("Invalid %s image\n", name);
		goto out;
	}

	if (lseek(fd, 0, SEEK_SET) < 0) {
		error("Seek to beginning of file failed: %s\n", strerror(errno));
		goto out;
	}

	*data = malloc(size);
	if (!*data) {
		error("Memory allocation failure\n");
		goto close;
	}

	ret = safe_read(fd, *data, size);
	if (ret)
		free(*data);
	else
		ret = size;
close:
	close(fd);
out:
	return ret;
}

int read_image(const char *name, void **data)
{
	size_t size;

	if (full_gpt())
		return read_image_full_gpt(name, data);

	int index;
	index = get_named_osii_index(name);
	if (index < 0) {
		error("Can't find image %s in the OSIP\n", name);
		return -1;
	}

	if (read_osimage_data(data, &size, index)) {
		error("Failed to read OSIP entry\n");
		return -1;
	}
	return size;
}

int flash_android_kernel(void *data, unsigned sz)
{
	return flash_image(data, sz, ANDROID_OS_NAME);
}

int flash_recovery_kernel(void *data, unsigned sz)
{
	return flash_image(data, sz, RECOVERY_OS_NAME);
}

int flash_fastboot_kernel(void *data, unsigned sz)
{
	return flash_image(data, sz, FASTBOOT_OS_NAME);
}

int flash_splashscreen_image(void *data, unsigned sz)
{
	return flash_image(data, sz, SPLASHSCREEN_NAME);
}

int flash_esp(void *data, unsigned sz)
{
	return flash_image(data, sz, ESP_PART_NAME);
}
