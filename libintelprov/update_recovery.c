/*
 * Copyright (C) 2011-2014 Intel Corporation
 * Copyright (C) 2008 The Android Open Source Project
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

#define LOG_TAG "recovery"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>

#include <cutils/log.h>
#include <mincrypt/sha.h>
#include <applypatch.h>

#include <bootimg.h>

#include "flash_image.h"
#include "update_osip.h"
#include "util.h"
#include "gpt/partlink/partlink.h"

/* Needs to agree with ota_from_target_files.MakeRecoveryPatch() */
#define OSIP_SIG_SIZE		480
#define ANDROID_SIG_SIZE	2048

#define LOGPERROR(x)	LOGE("%s failed: %s", x, strerror(errno))

#define TGT_SIZE_MAX    (LBA_SIZE * OS_MAX_LBA)

static struct OSIP_header osip;
static int recovery_index;

static int read_recovery_signature(void **buf)
{
	int fd = -1;
	int sig_size;
	int fgpt = full_gpt();

	sig_size = fgpt ? ANDROID_SIG_SIZE : OSIP_SIG_SIZE;

	*buf = malloc(sig_size);
	if (!*buf) {
		LOGE("Failed to allocate signature buffer\n");
		return -1;
	}

	if (fgpt) {
		struct boot_img_hdr hdr;
		ssize_t img_size;

		fd = open_bootimage(RECOVERY_OS_NAME);
		if (fd < 0) {
			LOGPERROR("open");
			goto err;
		}

		img_size = bootimage_size(fd, &hdr, false);
		if (img_size <= 0) {
			error("Invalid image\n");
			goto err;
		}

		sig_size = hdr.sig_size;

		if (lseek(fd, img_size, SEEK_SET) < 0) {
			LOGPERROR("lseek");
			goto err;
		}
	} else {
		int offset;
		sig_size = OSIP_SIG_SIZE;
		fd = open(MMC_DEV_POS, O_RDONLY);
		if (fd < 0) {
			LOGPERROR("open");
			goto err;
		}

		offset = osip.desc[recovery_index].logical_start_block * LBA_SIZE;
		if (lseek(fd, offset, SEEK_SET) < 0) {
			LOGPERROR("lseek");
			goto err;
		}
	}

	if (safe_read(fd, *buf, sig_size)) {
		LOGPERROR("read");
		goto err;
	}

	close(fd);
	return sig_size;

err:
	if (fd != -1)
		close(fd);
	free(*buf);

	return -1;
}

/* Compare the first SIG_SIZE bytes of the current recovery.img
 * with the SHA1 passed in, to determine if we need to update it.
 * These last bytes have the digital signature from LFSTK and would
 * not be the same. This is much faster than hashing the entire image */
static int check_recovery_header(const char *tgt_sha1, int *needs_patching)
{
	int sig_size;
	void *buf;
	uint8_t src_digest[SHA_DIGEST_SIZE];
	uint8_t tgt_digest[SHA_DIGEST_SIZE];

	if (ParseSha1(tgt_sha1, tgt_digest)) {
		LOGE("Bad SHA1 digest passed in");
		return -1;
	}

	sig_size = read_recovery_signature(&buf);
	if (sig_size == -1) {
		LOGE("Failed to read boot signature\n");
		return -1;
	}
	SHA_hash(buf, sig_size, src_digest);
	free(buf);

	*needs_patching = memcmp(src_digest, tgt_digest, SHA_DIGEST_SIZE);
	return 0;
}

static int check_recovery_image(const char *tgt_sha1, int *needs_patching)
{
	void *data;
	int sz;
	uint8_t tgt_digest[SHA_DIGEST_SIZE];
	uint8_t expected_tgt_digest[SHA_DIGEST_SIZE];

	if (ParseSha1(tgt_sha1, expected_tgt_digest)) {
		LOGE("Bad SHA1 digest passed in");
		return -1;
	}

	if ((sz = read_image(RECOVERY_OS_NAME, &data)) == -1) {
		LOGE("failed to read recovery image");
		*needs_patching = 1;
		return 0;
	}
	SHA_hash(data, sz, tgt_digest);

	*needs_patching = memcmp(tgt_digest, expected_tgt_digest,
			SHA_DIGEST_SIZE);
	free(data);
	return 0;
}

typedef struct {
	unsigned char* buffer;
	ssize_t size;
	ssize_t pos;
} MemorySinkInfo;

static ssize_t MemorySink(unsigned char* data, ssize_t len, void* token) {
	MemorySinkInfo* msi = (MemorySinkInfo*)token;
	if (msi->size - msi->pos < len) {
		return -1;
	}
	memcpy(msi->buffer + msi->pos, data, len);
	msi->pos += len;
	return len;
}


static int patch_recovery(const char *src_sha1, const char *tgt_sha1,
		unsigned int tgt_size, const char *patchfile)
{
	MemorySinkInfo msi;
	void *src_data;
	int src_size;
	uint8_t expected_src_digest[SHA_DIGEST_SIZE];
	uint8_t src_digest[SHA_DIGEST_SIZE];
	uint8_t expected_tgt_digest[SHA_DIGEST_SIZE];
	uint8_t tgt_digest[SHA_DIGEST_SIZE];
	SHA_CTX ctx;

	Value patchval;

	msi.buffer = NULL;
	src_data = NULL;
	patchval.data = NULL;
	SHA_init(&ctx);

	if (ParseSha1(src_sha1, expected_src_digest)) {
		LOGE("Bad SHA1 src SHA1 digest %s passed in", src_sha1);
		return -1;
	}
	if (ParseSha1(tgt_sha1, expected_tgt_digest)) {
		LOGE("Bad SHA1 tgt SHA1 digest %s passed in", tgt_sha1);
		return -1;
	}

	src_size = read_image(ANDROID_OS_NAME, &src_data);
	if (src_size == -1) {
		LOGE("Failed to read image %s\n", ANDROID_OS_NAME);
		return -1;
	}

	SHA_hash(src_data, src_size, src_digest);
	if (memcmp(src_digest, expected_src_digest, SHA_DIGEST_SIZE)) {
		LOGE("boot image digests don't match!");
		goto out;
	}

	msi.pos = 0;
        if (tgt_size > TGT_SIZE_MAX){
                LOGE("tgt_size is too big!");
                goto out;
	}
	msi.size = tgt_size;
	msi.buffer = malloc(tgt_size);
	if (!msi.buffer) {
		LOGPERROR("malloc");
		goto out;
	}

	if (file_read(patchfile, (void **)&patchval.data, (size_t *)&patchval.size)) {
		LOGE("Coudln't read patch data");
		goto out;
	}
	patchval.type = VAL_BLOB;

	if (ApplyImagePatch(src_data, src_size, &patchval,
				MemorySink, &msi, &ctx, NULL)) {
		LOGE("Patching process failed");
		goto out;
	}
	SHA_hash(msi.buffer, msi.size, tgt_digest);
	if (memcmp(tgt_digest, expected_tgt_digest, SHA_DIGEST_SIZE)) {
		LOGE("output recovery image digest mismatch");
		goto out;
	}
	if (flash_recovery_kernel(msi.buffer, msi.size)) {
		LOGE("error writing patched recovery image");
		goto out;
	}
	LOGI("Recovery sucessfully patched from boot");

out:
	free(src_data);
	free(patchval.data);
	free(msi.buffer);
	return 0;
}

static void usage(void)
{
	printf("-s | --src-sha1     Expected sha1 of boot image\n");
	printf("-t | --tgt-sha1     Expected sha1 of patched recovery image\n");
	printf("-z | --tgt-size     Size of patched recovery image\n");
	printf("-c | --check-sha1   Expected SHA1 of signature of recovery\n");
	printf("                    If the actual value does not match, patch it\n");
	printf("-p | --patch        Patch file, applied to boot, which creates recovery\n");
	printf("-h | --help         Show this message\n");
}

static struct option long_options[] = {
	{"src-sha1",   1, NULL, 's'},
	{"tgt-sha1",   1, NULL, 't'},
	{"tgt-size",   1, NULL, 'z'},
	{"check-sha1", 1, NULL, 'c'},
	{"patch",      1, NULL, 'p'},
	{"help",       0, NULL, 'h'},
	{0, 0, 0, 0}
};

int main(int argc, char **argv)
{
	int c;
	char *src_sha1 = NULL;
	char *tgt_sha1 = NULL;
	char *check_sha1 = NULL;
	char *patch_file = NULL;
	unsigned int tgt_size = 0;
	int needs_patching;
	int unsigned_image;

	while (1) {
		int option_index = 0;
		c = getopt_long(argc, argv, "s:t:z:c:p:h", long_options,
				&option_index);
		if (c == -1)
			break;

		switch (c) {
		case 's':
			src_sha1 = strdup(optarg);
			break;
		case 't':
			tgt_sha1 = strdup(optarg);
			break;
		case 'z':
			tgt_size = atoi(optarg);
			break;
		case 'c':
			check_sha1 = strdup(optarg);
			break;
		case 'p':
			patch_file = strdup(optarg);
			break;
		case 'h':
			usage();
			exit(0);
		default:
			usage();
			exit(1);
		}
	}

	if (full_gpt()) {
		struct boot_img_hdr hdr;
		int fd = open(BASE_PLATFORM_INTEL_LABEL"/recovery", O_RDONLY);
		if (fd < 0) {
			LOGPERROR("open");
			exit(EXIT_FAILURE);
		}
		if (safe_read(fd, &hdr, sizeof(hdr))) {
			LOGPERROR("read");
			close(fd);
			exit(EXIT_FAILURE);
		}
		close(fd);
		unsigned_image = hdr.sig_size == 0;
	} else {
		if (read_OSIP(&osip)) {
			LOGE("Can't read the OSIP");
			exit(EXIT_FAILURE);
		}
		recovery_index = get_named_osii_index(RECOVERY_OS_NAME);
		if (recovery_index < 0) {
			LOGE("Can't find recovery console in the OSIP");
			exit(EXIT_FAILURE);
		}
		unsigned_image = osip.desc[recovery_index].attribute & ATTR_UNSIGNED_KERNEL;
	}

	if (unsigned_image || !check_sha1) {
		/* We can't do any quick checks if unsigned images
		 * are used. Examine the whole image */
		LOGI("Checking full recovery image");
		if (tgt_sha1 != NULL){
			if (check_recovery_image(tgt_sha1, &needs_patching)) {
				LOGE("Can't examine current recovery console SHA1");
				exit(EXIT_FAILURE);
			}
		}else{
			LOGE("SHA1 option was not detected correctly");
			exit(EXIT_FAILURE);
		}
	} else {
		LOGI("Checking recovery image's signature");
		if (check_recovery_header(check_sha1, &needs_patching)) {
			LOGE("Can't compare recovery console SHA1 sums");
			exit(EXIT_FAILURE);
		}
	}

	if ((src_sha1 != NULL) && (tgt_sha1 != NULL) && (patch_file != NULL) && (tgt_size!= 0))
	{
		if (needs_patching) {
			LOGI("Installing new recovery image");
			if (patch_recovery(src_sha1, tgt_sha1, tgt_size, patch_file)) {
				LOGE("Couldn't patch recovery image");
				exit(EXIT_FAILURE);
			}
		} else {
			LOGI("Recovery image already installed");
		}
	}else{
		LOGI("Update options were not successfully detected");
	}
	return 0;
}

