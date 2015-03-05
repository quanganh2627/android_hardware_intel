/*
 * Copyright (c) 2008-2012, Intel Corporation. All rights reserved.
 *
 * Redistribution.
 * Redistribution and use in binary form, without modification, are
 * permitted provided that the following conditions are met:
 *  * Redistributions must reproduce the above copyright notice and
 * the following disclaimer in the documentation and/or other materials
 * provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its
 * suppliers may be used to endorse or promote products derived from
 * this software without specific  prior written permission.
 *  * No reverse engineering, decompilation, or disassembly of this
 * software is permitted.
 *
 * Limited patent license.
 * Intel Corporation grants a world-wide, royalty-free, non-exclusive
 * license under patents it now or hereafter owns or controls to make,
 * have made, use, import, offer to sell and sell ("Utilize") this
 * software, but solely to the extent that any such patent is necessary
 * to Utilize the software alone, or in combination with an operating
 * system licensed under an approved Open Source license as listed by
 * the Open Source Initiative at http://opensource.org/licenses.
 * The patent license shall not apply to any other combinations which
 * include this software. No hardware per se is licensed hereunder.
 *
 * DISCLAIMER.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors:
 *    Jackie Li <yaodong.li@intel.com>
 *
 */
#include <IntelWsbm.h>
#include <IntelOverlayUtil.h>
#include <cutils/log.h>
#include <cutils/atomic.h>

/***********************************************************************
 *  WSBM Object Implementation
 ***********************************************************************/
IntelWsbm::IntelWsbm(int drmFD)
{
    ALOGV("%s: creating a new wsbm object...\n", __func__);
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
        ALOGE("%s: wsbm initialize failed\n", __FUNCTION__);
        return false;
    }

    return true;
}

bool IntelWsbm::allocateTTMBuffer(uint32_t size, uint32_t align, void ** buf)
{
    int ret = pvrWsbmAllocateTTMBuffer(size, align, buf);
    if(ret) {
        ALOGE("%s: Allocate buffer failed\n", __func__);
        return false;
    }

    return true;
}

bool IntelWsbm::destroyTTMBuffer(void * buf)
{
    int ret = pvrWsbmDestroyTTMBuffer(buf);
    if(ret) {
        ALOGE("%s: destroy buffer failed\n", __func__);
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
        ALOGE("%s: wrap buffer failed\n", __func__);
        return false;
    }

    return true;
}

bool IntelWsbm::unreferenceTTMBuffer(void *buf)
{
    int ret = pvrWsbmUnReference(buf);
    if (ret) {
        ALOGE("%s: unreference buffer failed\n", __func__);
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
        ALOGE("%s: wait ttm buffer idle failed\n", __func__);
        return false;
    }

    return true;
}
