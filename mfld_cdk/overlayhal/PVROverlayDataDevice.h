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
#ifndef __PVR_OVERLAY_DATA_DEVICE_H__
#define __PVR_OVERLAY_DATA_DEVICE_H__

#include <pthread.h>

#include <hardware/hardware.h>
#include <hardware/overlay.h>

#include <IOverlayDevice.h>
#include <PVROverlayHAL.h>
#include <PVROverlay.h>

/**
 * Class: Overlay Data Device
 * overlay clients will creat one or more data device object throw which
 * overlay clients could setup the overlay by calling overlay HAL singleton
 * object.
 */
class PVROverlayDataDevice : public overlay_data_device_t {
private:
    pthread_mutex_t mLock;
    pthread_cond_t mHasBuffer;

    struct pvr_overlay_control_block_t * mControlBlock;
    struct pvr_overlay_buffer_t mControlBlkBuffer;

    /*init following members during initializing*/
    struct pvr_overlay_buffer_t mDataBuffers[PVR_OVERLAY_BUFFER_NUM];
    int mNumFreeBuffers;

    /*wrapped external buffer*/
    struct pvr_overlay_buffer_t mExternalBuffer;
    bool mUsingExternalBuffer;

    /*setting as (0, 0) - (width, height) by default*/
    int dstX;
    int dstY;
    int dstWidth;
    int dstHeight;

    /*TODO: add source crop later*/

    /*Overlay binding to this data device*/
    uint32_t mOverlayIndex;
private:
    bool controlBlockInit();
    void bufferOffsetSetup(struct pvr_overlay_buffer_t * buf);
    uint32_t calculateSWidthSW(uint32_t offset, uint32_t width);
    void coordinateSetup(struct pvr_overlay_buffer_t * buf);
    bool setCoeffRegs(double *coeff, int mantSize, coeffPtr pCoeff, int pos);
    void updateCoeff(int taps, double fCutoff, bool isHoriz, bool isY,
                    coeffPtr pCoeff);
    void scalingSetup(uint32_t srcWidth, uint32_t srcHeight,
            uint32_t dstWidth, uint32_t dstHeight);
    bool formatOverlayBuffer(struct pvr_overlay_buffer_t * buffer);
public:
    PVROverlayDataDevice();
    ~PVROverlayDataDevice();
    bool initialize(struct pvr_overlay_handle_t * overlay);
    void lock();
    void unlock();
    bool post(struct pvr_overlay_buffer_t * buffer);
    bool postExternal(uint32_t device, uint32_t handle);
    struct pvr_overlay_buffer_t * getBuffer();
    bool putBuffer(struct pvr_overlay_buffer_t * buffer);
    void signal();
    void waitBuffer();
    bool setCrop(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
    bool getCrop(uint32_t *x, uint32_t *y, uint32_t *w, uint32_t *h);
    void setOverlayPipe(int pipe);
    void setOverlayUsage(int usage);
    void setStride(int yStride);
    void resetOverlay();
    void setDrmModeChanged(bool changed);
};

#endif /*__PVR_OVERLAY_DATA_DEVICE_H__*/
