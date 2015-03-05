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
#include <PVROverlay.h>
#include <OverlayHALUtils.h>

#include <fcntl.h>
#include <errno.h>

#include <cutils/log.h>
#include <cutils/atomic.h>

PVROverlay::PVROverlay()
{
    LOGV("%s: creating a new PVR overlay\n", __func__);

    this->overlay_t::getHandleRef = getHandleRef;
    mHandle.version = sizeof(native_handle);
    mHandle.numFds = 1;
    mHandle.numInts = 9;
}

PVROverlay::PVROverlay(uint32_t w, uint32_t h, uint32_t format,
                       uint32_t yStride, uint32_t uvStride,
                       uint32_t size, uint32_t dataSize)
{
    LOGV("%s: creating a new overlay with parameters... mHandle %p\n",
         __func__, &mHandle);

    this->overlay_t::getHandleRef = getHandleRef;
    this->overlay_t::w = w;
    this->overlay_t::h = h;
    this->overlay_t::format = format;
    this->overlay_t::w_stride = yStride;
    this->overlay_t::h_stride = uvStride;

    mHandle.version = sizeof(native_handle);
    mHandle.numFds = 1;
    /*we have 9 extra Ints*/
    mHandle.numInts = 9;

    mHandle.width = w;
    mHandle.height = h;
    mHandle.format = format;
    mHandle.yStride = yStride;
    mHandle.uvStride = uvStride;
    mHandle.size = size;
    mHandle.dataSize = dataSize;
}

void PVROverlay::setSharedFd(int fd)
{
    mHandle.sharedFd = fd;
}

void PVROverlay::setSharedSize(int size)
{
    mHandle.sharedSize = size;
}

void PVROverlay::setOverlayIndex(uint32_t overlayIndex)
{
    mHandle.overlayIndex = overlayIndex;
}

uint32_t PVROverlay::getOverlayIndex()
{
    return mHandle.overlayIndex;
}

PVROverlay::~PVROverlay()
{
    LOGV("%s: destroying overlay\n", __FUNCTION__);
}
