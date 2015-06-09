/*
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
#include <IPVRWsbm.h>
#include <OverlayHALUtils.h>

#include <cutils/log.h>
#include <cutils/atomic.h>

/***********************************************************************
 *  WSBM Object Implementation
 ***********************************************************************/
PVRWsbm::PVRWsbm(int drmFD)
{
    LOGV("%s: creating a new wsbm object...\n", __func__);
    mDrmFD = drmFD;
}

PVRWsbm::~PVRWsbm()
{
    pvrWsbmTakedown();
}

bool PVRWsbm::initialize()
{
    int ret = pvrWsbmInitialize(mDrmFD);
    if(ret) {
        LOGE("%s: wsbm initialize failed\n", __FUNCTION__);
        return false;
    }

    return true;
}

bool PVRWsbm::allocateTTMBuffer(uint32_t size, uint32_t align, void ** buf)
{
    int ret = pvrWsbmAllocateTTMBuffer(size, align, buf);
    if(ret) {
        LOGE("%s: Allocate buffer failed\n", __func__);
        return false;
    }

    return true;
}

bool PVRWsbm::destroyTTMBuffer(void * buf)
{
    int ret = pvrWsbmDestroyTTMBuffer(buf);
    if(ret) {
        LOGE("%s: destroy buffer failed\n", __func__);
        return false;
    }

    return true;
}

void * PVRWsbm::getCPUAddress(void * buf)
{
    return pvrWsbmGetCPUAddress(buf);
}

uint32_t PVRWsbm::getGttOffset(void * buf)
{
    return pvrWsbmGetGttOffset(buf);
}

bool PVRWsbm::wrapTTMBuffer(uint32_t handle, void **buf)
{
    int ret = pvrWsbmWrapTTMBuffer(handle, buf);
    if (ret) {
        LOGE("%s: wrap buffer failed\n", __func__);
        return false;
    }

    return true;
}

bool PVRWsbm::unreferenceTTMBuffer(void *buf)
{
    int ret = pvrWsbmUnReference(buf);
    if (ret) {
        LOGE("%s: unreference buffer failed\n", __func__);
        return false;
    }

    return true;
}
