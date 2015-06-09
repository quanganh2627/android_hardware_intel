/******************************************************************************
 * Intel Management Engine Interface (Intel MEI) Linux driver
 * Intel MEI Interface Header
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2012 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110,
 * USA
 *
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.GPL.
 *
 * Contact Information:
 *	Intel Corporation.
 *	linux-mei@linux.intel.com
 *	http://www.intel.com
 *
 * BSD LICENSE
 *
 * Copyright(c) 2003 - 2012 Intel Corporation. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>

#include <linux/mei-mm.h>
#include <meimm.h>

#define MEI_MM_STATUS_INIT      1UL
#define MEI_MM_STATUS_ALLOCATED 2UL

/*****************************************************************************
 * Intel Management Enginin Interface
 *****************************************************************************/
#ifdef ANDROID
#define LOG_TAG "meimm"
#include <cutils/log.h>
#define meimm_msg(_me, fmt, ARGS...) ALOGV_IF(_me->verbose, fmt, ##ARGS)
#define meimm_err(_me, fmt, ARGS...) ALOGE_IF(_me->verbose, fmt, ##ARGS)
#else
#define meimm_msg(_mm, fmt, ARGS...) do {         \
	if ((_mm)->verbose)                       \
		fprintf(stderr, "meimm: " fmt, ##ARGS);	\
} while (0)

#define meimm_err(_mm, fmt, ARGS...) do {         \
	fprintf(stderr, "meimm: error: " fmt, ##ARGS); \
} while (0)
#endif /* ANDROID */

#ifndef MMEDEV
#define MMDEV "/dev/meimm"
#endif /* MMDEV */

void meimm_verbose(struct meimm *mm, bool verbose)
{
	if (!mm)
		return;
	mm->verbose = true;
}

void meimm_deinit(struct meimm *mm)
{
	if (!mm)
		return;

	if (mm->fd >= 0)
		close(mm->fd);
	mm->fd = -1;
	mm->status = 0;
}

int meimm_init(struct meimm *mm, bool verbose)
{
	if (!mm)
		return EINVAL;
	mm->fd = -1;

	mm->verbose = verbose;
	mm->status = MEI_MM_STATUS_INIT;

	return 0;
}


struct meimm *meimm_alloc(size_t size, bool verbose)
{
	struct meimm *mm = malloc(sizeof(struct meimm));

	if (!mm)
		return NULL;

	if (meimm_init(mm, verbose)) {
		free(mm);
		mm = NULL;
	}
	return mm;
}

void meimm_free(struct meimm *mm)
{
	if (!mm)
		return;
	meimm_deinit(mm);
	free(mm);
}

int meimm_alloc_map_memory(struct meimm *mm, size_t size)
{
	struct mei_mm_data data = {
		.size = size,
	};
	int ret;

	mm->fd = open(MMDEV, O_RDWR);
	if (mm->fd == -1) {
		meimm_err(mm, "cannot open %s: %s\n", MMDEV, strerror(errno));
		ret = errno;
		goto err;
	}
	meimm_msg(mm, "device %s opened with fd=%d\n", MMDEV, mm->fd);


	ret = ioctl(mm->fd, IOCTL_MEI_MM_ALLOC, &data);
	if (ret) {
		meimm_err(mm, "IOCTL_MEI_MM_ALLOC receive message. err=%d\n", ret);
		ret = errno;
		goto err;
	}
	meimm_msg(mm, "Allocated: vaddr=0x%0llX paddr=0x%0llX size=%llu\n",
			data.vaddr, data.paddr, data.size);

	/* pa_offset = addr & ~(sysconf(_SC_PAGE_SIZE) - 1); */
	mm->ptr = mmap(NULL, data.size, PROT_READ | PROT_WRITE, MAP_SHARED, mm->fd, 0);
	if (mm->ptr == MAP_FAILED)  {
		meimm_err(mm, "memmap failed err=%d\n", errno);
		goto err;
	}
	mm->size = data.size;
	mm->status = MEI_MM_STATUS_ALLOCATED;
	meimm_msg(mm, "got user space ptr = %p\n", mm->ptr);
	return 0;
err:
	meimm_deinit(mm);
	return ret;
}

int meimm_free_memory(struct meimm *mm)
{
	struct mei_mm_data data;
	
	int ret;
	if (mm->status != MEI_MM_STATUS_ALLOCATED)
		return 0;

	if (mm->ptr == NULL)
		return 0;

	ret = msync(mm->ptr, mm->size, MS_ASYNC);
	if (ret) {
		meimm_err(mm, "msync fialed %s\n", strerror(errno));
		goto out;
	}
	ret = munmap(mm->ptr, mm->size);
	if (ret) {
		meimm_err(mm, "munmap fialed %s\n", strerror(errno));
		goto out;
	}

	memset(&data, 0, sizeof(data));
	data.size = mm->size;

	ret = ioctl(mm->fd, IOCTL_MEI_MM_FREE, &data);
out:
	mm->status = MEI_MM_STATUS_INIT;
	return ret;
}

void *meimm_get_addr(struct meimm *mm)
{
	return (mm && mm->status == MEI_MM_STATUS_ALLOCATED) ? mm->ptr :  NULL;
}

ssize_t meimm_get_size(struct meimm *mm)
{
	return (mm && mm->status == MEI_MM_STATUS_ALLOCATED) ? mm->size : -1;
}

void *meimm_memcpy(struct meimm *mm, off_t offset, const void *buf, size_t len)
{
	unsigned char *ptr;
	int ret;
	if (!mm || mm->status != MEI_MM_STATUS_ALLOCATED || !mm->ptr)
		return NULL;

	if (offset + len > mm->size)
		return NULL;

	ptr = memcpy((unsigned char *)mm->ptr + offset, buf, len);
	ret = msync(mm->ptr, mm->size, MS_ASYNC);
	if (ret)
		meimm_err(mm, "msync fialed %s\n", strerror(errno));

	return ptr;
}

