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
#ifndef __PVR_OVERLAY_CONTROL_DEVICE_H__
#define __PVR_OVERLAY_CONTROL_DEVICE_H__

#include <pthread.h>

#include <hardware/hardware.h>
#include <hardware/overlay.h>

#include <PVROverlayHAL.h>
#include <PVROverlay.h>

/**
 * Class: Overlay Control Device
 * A DisplayHardware object will creat such an object which will be used
 * by surfaceflinger.
 */
class PVROverlayControlDevice : public overlay_control_device_t {
private:
    pthread_mutex_t mLock;
    uint8_t mFreeOverlays;

public:
    PVROverlayControlDevice();
    ~PVROverlayControlDevice();
    PVROverlay *getOverlay(uint32_t w, uint32_t h, uint32_t format,
                           uint32_t yStride, uint32_t uvStride,
                           uint32_t size, uint32_t dataSize);
    void putOverlay(PVROverlay *overlay);
    int getSharedFd();
    int getSharedSize();
    void lock();
    void unlock();
    void setPosition(uint32_t overlayIndex,
                     int x, int y, int w, int h);
};

#endif /*__PVR_OVERLAY_CONTROL_DEVICE_H__*/
