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
#ifndef __PVR_OVERLAY_HAL_H__
#define __PVR_OVERLAY_HAL_H__

#include <IPVRWsbm.h>
#include <psb_drm.h>
#include <pthread.h>
#include <pvr2d.h>

extern "C" {
#include "xf86drm.h"
#include "xf86drmMode.h"
}

#define PVR_DRM_DRIVER_NAME     "pvrsrvkm"

#define DRM_PSB_GTT_MAP         0x0F
#define DRM_PSB_GTT_UNMAP       0x10

typedef enum {
    PVR_OVERLAY_VSYNC_INIT,
    PVR_OVERLAY_VSYNC_DONE,
    PVR_OVERLAY_VSYNC_PENDING,
} eVsyncState;

typedef enum {
    PVR_OVERLAY_BUFFER_TYPE_TTM,
    PVR_OVERLAY_BUFFER_TYPE_PVR,
} PVROverlayBufferType;

enum {
    MDFLD_OVERLAY_A = 0,
    MDFLD_OVERLAY_C,
    MDFLD_OVERLAY_MAX,
};

typedef enum {
    MDFLD_PIPE_A = 0,
    MDFLD_PIPE_B,
    MDFLD_PIPE_C,
} IntelDCPipe;

typedef enum {
    MDFLD_OUTPUT_MIPI0 = 0,
    MDFLD_OUTPUT_MIPI1,
    MDFLD_OUTPUT_HDMI,
    MDFLD_OUTPUT_NUM,
} IntelDCOutput;

#define DRM_MODE_CONNECTOR_MIPI 15

typedef enum {
    MDFLD_OVERLAY_CLONE = 0,
    MDFLD_OVERLAY_EXTEND,
} IntelOverlayUsage;

typedef enum {
    MDFLD_OVERLAY_ROTATE_0 = 0,
    MDFLD_OVERLAY_ROTATE_90,
    MDFLD_OVERLAY_ROTATE_180,
    MDFLD_OVERLAY_ROTATE_270,
} IntelOverlayRotation;

typedef enum {
    MDFLD_OVERLAY_SINGLE_MIPI0 = 1,
    MDFLD_OVERLAY_SINGLE_MIPI1,
    MDFLD_OVERLAY_DUAL_MIPI,
    MDFLD_OVERLAY_HDMI_CONNECTED,
    MDFLD_OVERLAY_UNKNOWN,
} IntelOverlayDisplayMode;

typedef struct intel_overlay_position {
    int x;
    int y;
    uint32_t width;
    uint32_t height;
} IntelOverlayPosition;

typedef struct intel_overlay_size {
    uint32_t width;
    uint32_t height;
} IntelOverlaySize;

struct pvr_overlay_buffer_t {
    uint32_t width;
    uint32_t height;
    uint32_t yStride;
    uint32_t uvStride;
    uint32_t format;
    uint32_t align;
    uint32_t size;
    uint32_t dataSize;
    eVsyncState vsyncState;
    uint32_t gttOffset;
    uint32_t gttAlign;
    void * overlayCPUAddress;
    void * dataCPUAddress;
    void * overlayBuffer;
    void * dataBuffer;
    uint32_t overlayIndex;
};

typedef struct intel_drm_mode_info {
    drmModeConnection connections[MDFLD_OUTPUT_NUM];
    drmModeModeInfo modes[MDFLD_OUTPUT_NUM];
    bool initialized;
    IntelOverlayDisplayMode displayMode;
} IntelDrmModeInfo;

typedef struct intel_overlay_shared_context {
    pthread_mutex_t lock;
    pthread_mutexattr_t attr;
    volatile int32_t refCount;
    /*overlay-pipe mapping*/
    IntelDCPipe pipe[MDFLD_OVERLAY_MAX];
    IntelOverlayUsage usage[MDFLD_OVERLAY_MAX];
    /*overlay parameters*/
    IntelOverlayRotation rotation[MDFLD_OVERLAY_MAX];
    IntelOverlayPosition position[MDFLD_OVERLAY_MAX];
    IntelOverlaySize size[MDFLD_OVERLAY_MAX];
    IntelDrmModeInfo modeInfo;
    /*TODO: add more parameters here*/
} IntelOverlayHALContext;

/**
 * Class: Overlay HAL implementation
 * This is a singleton implementation of hardware overlay.
 * this object will be shared between mutiple overlay control/data devices.
 * FIXME: overlayHAL should contact to the h/w to track the overlay h/w
 * state.
 */
class PVROverlayHAL {
private:
    /*lock for hardware accessing*/
    pthread_mutex_t mLock;

    /*PVR2D handle*/
    PVR2DCONTEXTHANDLE mPVR2DHandle;

    /*DRM FD*/
    int drmFD;

    /*WSBM object instance*/
    PVRWsbm * mWsbm;

    static PVROverlayHAL *mInstance;

    int mSharedFd;
    int mSharedSize;
    IntelOverlayHALContext * mSharedContext;

    bool mDrmModeChanged;
    uint32_t mVideoBridgeIoctl;
private:
    PVROverlayHAL();
    void lock();
    void unlock();
    bool pvr2DInit();
    bool drmInit();
    void drmDestroy();
    void pvr2DDestroy();

    bool sharedContextLock();
    void sharedContextUnlock();

    uint32_t getBufferHandle(uint32_t device, uint32_t handle);
    bool  getVideoBridgeIoctl();
public:
    ~PVROverlayHAL();
    static PVROverlayHAL &Instance() {
        if (!mInstance) {
            mInstance = new PVROverlayHAL();
        }
        return *mInstance;
    }

    static PVROverlayHAL* getInstance() {
        return mInstance;
    }

    bool createSharedContext();
    void destroySharedContext(bool closeFd);
    bool openSharedContext(int fd, int size);
    int getSharedFd();
    int getSharedSize();
    bool initialize();
    bool allocatePVR2DBuffer(uint32_t size, uint32_t align, PVR2DMEMINFO ** buf);
    bool destroyPVR2DBuffer(PVR2DMEMINFO * buf);
    bool gttMap(PVR2DMEMINFO *buf, int *offset, uint32_t gttAlign);
    bool gttUnmap(PVR2DMEMINFO * buf);
    bool allocateOverlayBuffer(struct pvr_overlay_buffer_t * buf);
    bool destroyOverlayBuffer(struct pvr_overlay_buffer_t * buf);
    bool updateOverlay(struct pvr_overlay_buffer_t * controlBuffer,
                       struct pvr_overlay_control_block_t * controlBlk,
                       bool loadCoefficients);
    bool wrapExternalTTMBuffer(struct pvr_overlay_buffer_t *buf,
                               uint32_t device,
                               uint32_t handle);

    /*Gets & Sets of shared context*/
    void setPipe(int overlayIndex, int pipe);
    int getPipe(int overlayIndex, int *pipe);
    void setSize(int overlayIndex, uint32_t w, uint32_t h);
    int getSize(int overlayIndex, IntelOverlaySize *size);
    void setOverlayPipe(int overlayIndex, int pipe);
    void setOverlayUsage(int overlayIndex, int usage);
    int getOverlayPipe(int overlayIndex);
    int getOverlayUsage(int overlayIndex);
    void setPosition(int overlayIndex, int x, int y, int w, int h);
    int getPosition(int overlayIndex, int *x, int *y, int *w, int *h);
    bool detectDrmModeInfo();
    IntelOverlayDisplayMode getOverlayDisplayMode();
    void drmModeChanged(struct pvr_overlay_buffer_t * controlBuffer,
                        struct pvr_overlay_control_block_t * controlBlk);
    void setDrmModeChanged(bool changed);
};

#endif /*__PVR_OVERLAY_HAL_H__*/
