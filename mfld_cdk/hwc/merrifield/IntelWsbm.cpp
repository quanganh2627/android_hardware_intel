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
#include <IntelWsbm.h>
#include <cutils/log.h>
#include <cutils/atomic.h>

/***********************************************************************
 *  WSBM Object Implementation
 ***********************************************************************/
IntelWsbm::IntelWsbm(int drmFD)
{
    LOGV("%s: creating a new wsbm object...\n", __func__);
    mDrmFD = drmFD;
}

IntelWsbm::~IntelWsbm()
{
    pvrWsbmTakedown();
}

bool IntelWsbm::initialize()
{
    int ret = pvrWsbmInitialize(mDrmFD);
    if(ret) {
        LOGE("%s: wsbm initialize failed\n", __FUNCTION__);
        return false;
    }

    return true;
}

bool IntelWsbm::allocateTTMBuffer(uint32_t size, uint32_t align, void ** buf)
{
    int ret = pvrWsbmAllocateTTMBuffer(size, align, buf);
    if(ret) {
        LOGE("%s: Allocate buffer failed\n", __func__);
        return false;
    }

    return true;
}

bool IntelWsbm::destroyTTMBuffer(void * buf)
{
    int ret = pvrWsbmDestroyTTMBuffer(buf);
    if(ret) {
        LOGE("%s: destroy buffer failed\n", __func__);
        return false;
    }

    return true;
}

void * IntelWsbm::getCPUAddress(void * buf)
{
    return pvrWsbmGetCPUAddress(buf);
}

uint32_t IntelWsbm::getGttOffset(void * buf)
{
    return pvrWsbmGetGttOffset(buf);
}

bool IntelWsbm::wrapTTMBuffer(uint32_t handle, void **buf)
{
    int ret = pvrWsbmWrapTTMBuffer(handle, buf);
    if (ret) {
        LOGE("%s: wrap buffer failed\n", __func__);
        return false;
    }

    return true;
}

bool IntelWsbm::unreferenceTTMBuffer(void *buf)
{
    int ret = pvrWsbmUnReference(buf);
    if (ret) {
        LOGE("%s: unreference buffer failed\n", __func__);
        return false;
    }

    return true;
}

uint32_t IntelWsbm::getKBufHandle(void *buf)
{
    return pvrWsbmGetKBufHandle(buf);
}

bool IntelWsbm::waitIdleTTMBuffer(void *buf)
{
    int ret = pvrWsbmWaitIdle(buf);
    if (ret) {
        LOGE("%s: wait ttm buffer idle failed\n", __func__);
        return false;
    }

    return true;
}
