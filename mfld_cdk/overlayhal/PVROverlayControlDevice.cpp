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

#include <PVROverlayControlDevice.h>
#include <PVROverlayHAL.h>
#include <OverlayHALUtils.h>

#include <cutils/log.h>
#include <cutils/atomic.h>

PVROverlayControlDevice::PVROverlayControlDevice()
{
    LOGV("%s creating control device\n", __func__);

    pthread_mutex_init(&mLock, NULL);

    mFreeOverlays = (1 << MDFLD_OVERLAY_A) | (1 << MDFLD_OVERLAY_C);

    /*create shared context*/
    bool ret = PVROverlayHAL::Instance().createSharedContext();
    if (ret == false) {
        LOGE("%s: failed to create shared context\n", __func__);
    }

    ret =  PVROverlayHAL::Instance().detectDrmModeInfo();
    if (ret == false) {
        LOGW("%s: failed to detect DRM info\n", __func__);
    }
}

PVROverlayControlDevice::~PVROverlayControlDevice()
{
    LOGV("%s: destroying control device\n", __func__);

    this->lock();

    /*destroy shared context*/
    PVROverlayHAL::Instance().destroySharedContext(true);

    this->unlock();

    /*destroy device lock*/
    pthread_mutex_destroy(&mLock);
}

PVROverlay *PVROverlayControlDevice::getOverlay(uint32_t w, uint32_t h,
                                                uint32_t format,
                                                uint32_t yStride,
                                                uint32_t uvStride,
                                                uint32_t size,
                                                uint32_t dataSize)
{
    LOGV("%s: getting free overlay...\n", __func__);

    this->lock();

    if (!mFreeOverlays) {
        LOGI("%s: No free overlay found\n", __func__);
        this->unlock();
        return NULL;
    }

    /*get a free overlay*/
    for (int i=0; i<MDFLD_OVERLAY_MAX; i++) {
        if (mFreeOverlays & (1 << i)) {

            /*found free overlay*/
            PVROverlay *overlay = new PVROverlay(w, h, format,
                                                 yStride,
                                                 uvStride,
                                                 size,
                                                 dataSize);
            if (!overlay) {
                LOGE("%s: Oops, no overlay\n", __func__);

                this->unlock();
                return NULL;
            }

            /*update overlay parameters*/
            overlay->setSharedFd(getSharedFd());
            overlay->setSharedSize(getSharedSize());
            overlay->setOverlayIndex(i);

            /*remove this overlay from free overlays*/
            mFreeOverlays &= ~(1 << i);

            LOGV("%s: overlay %s is allocated\n", __func__,
                                                 i ? "C" : "A");
            this->unlock();
            return overlay;
        }
    }


    this->unlock();

    /*get here where there's error*/
    LOGE("%s: check the code again\n", __func__);

    return NULL;
}

void PVROverlayControlDevice::putOverlay(PVROverlay *overlay)
{
    if (!overlay)
        return;

    uint32_t overlayIndex = overlay->getOverlayIndex();

    LOGV("%s: release overlay %s\n", __func__, overlayIndex ? "C" : "A");

    this->lock();

    delete overlay;

    mFreeOverlays |= (1 << overlayIndex);

    this->unlock();
}

int PVROverlayControlDevice::getSharedFd()
{
    return PVROverlayHAL::Instance().getSharedFd();
}

int PVROverlayControlDevice::getSharedSize()
{
    return PVROverlayHAL::Instance().getSharedSize();
}

void PVROverlayControlDevice::lock()
{
    pthread_mutex_lock(&mLock);
}

void PVROverlayControlDevice::unlock()
{
    pthread_mutex_unlock(&mLock);
}

void PVROverlayControlDevice::setPosition(uint32_t overlayIndex,
                                          int x,
                                          int y,
                                          int w,
                                          int h)
{
    PVROverlayHAL::Instance().setPosition(overlayIndex,x, y, w, h);
}
