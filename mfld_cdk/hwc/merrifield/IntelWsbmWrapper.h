/*
 * Copyright © 2012 Intel Corporation
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Jackie Li <yaodong.li@intel.com>
 *
 */
#ifndef __INTEL_WSBM_WRAPPER_H__
#define __INTEL_WSBM_WRAPPER_H__

#include <cutils/atomic.h>

#if defined(__cplusplus)
extern "C" {
#endif

extern int pvrWsbmInitialize(int drmFD);
extern void pvrWsbmTakedown();
extern int pvrWsbmAllocateTTMBuffer(uint32_t size, uint32_t align,void ** buf);
extern int pvrWsbmDestroyTTMBuffer(void * buf);
extern void * pvrWsbmGetCPUAddress(void * buf);
extern uint32_t pvrWsbmGetGttOffset(void * buf);
extern int pvrWsbmWrapTTMBuffer(uint32_t handle, void **buf);
extern int pvrWsbmUnReference(void *buf);
extern int pvrWsbmWaitIdle(void *buf);
uint32_t pvrWsbmGetKBufHandle(void *buf);

#if defined(__cplusplus)
}
#endif

#endif /*__INTEL_WSBM_WRAPPER_H__*/
