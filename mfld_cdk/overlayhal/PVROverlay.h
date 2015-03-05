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
#ifndef __PVR_OVERLAY_H__
#define __PVR_OVERLAY_H__

#include <hardware/hardware.h>
#include <hardware/overlay.h>

/*Legacy formats*/
enum {
    OVERLAY_FORMAT_YCbYCr_420_I = HAL_PIXEL_FORMAT_YCbCr_420_I,
    OVERLAY_FORMAT_YCbCr_420_SP = HAL_PIXEL_FORMAT_YCbCr_420_SP,
};

struct pvr_overlay_handle_t : public native_handle {
    int sharedFd;
    int sharedSize;
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint32_t yStride;
    uint32_t uvStride;
    uint32_t size;
    uint32_t dataSize;
    uint32_t overlayIndex;
};

class PVROverlay : public overlay_t {
private:
    struct pvr_overlay_handle_t mHandle;

    static overlay_handle_t getHandleRef(struct overlay_t * overlay) {
        return &(static_cast<PVROverlay *>(overlay)->mHandle);
    }
public:
    PVROverlay();
    PVROverlay(uint32_t w, uint32_t h, uint32_t format,
               uint32_t yStride, uint32_t uvStride,
               uint32_t size, uint32_t dataSize);
    void setSharedFd(int fd);
    void setSharedSize(int size);
    void setOverlayIndex(uint32_t overlayIndex);
    uint32_t getOverlayIndex();
    ~PVROverlay();
};

#endif
