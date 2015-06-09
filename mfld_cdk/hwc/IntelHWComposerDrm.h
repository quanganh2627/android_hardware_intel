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
#ifndef __INTEL_HWCOMPOSER_DRM_H__
#define __INTEL_HWCOMPOSER_DRM_H__

#include <IntelBufferManager.h>
#include <IntelHWCUEventObserver.h>
#include <linux/psb_drm.h>
#include <pthread.h>
#include <pvr2d.h>
#ifdef TARGET_HAS_MULTIPLE_DISPLAY
#include <IntelExternalDisplayMonitor.h>
#endif

extern "C" {
#include "xf86drm.h"
#include "xf86drmMode.h"
}

#define PVR_DRM_DRIVER_NAME     "pvrsrvkm"

#define DRM_PSB_GTT_MAP         0x0F
#define DRM_PSB_GTT_UNMAP       0x10

#define DRM_MODE_CONNECTOR_MIPI 15

typedef enum {
    PVR_OVERLAY_VSYNC_INIT,
    PVR_OVERLAY_VSYNC_DONE,
    PVR_OVERLAY_VSYNC_PENDING,
} eVsyncState;

typedef enum {
    OUTPUT_MIPI0 = 0,
    OUTPUT_HDMI,
    OUTPUT_MIPI1,
    OUTPUT_MAX,
} intel_drm_output_t;

typedef enum {
    OVERLAY_MIPI0 = 0,
    OVERLAY_CLONE_MIPI0,
    OVERLAY_CLONE_MIPI1,
    OVERLAY_CLONE_DUAL,
    OVERLAY_EXTEND,
    OVERLAY_UNKNOWN,
} intel_overlay_mode_t;

typedef struct {
    drmModeConnection connections[OUTPUT_MAX];
    drmModeModeInfo modes[OUTPUT_MAX];
    drmModeFB fbInfos[OUTPUT_MAX];
    bool mode_valid[OUTPUT_MAX];
    intel_overlay_mode_t display_mode;
    intel_overlay_mode_t old_display_mode;
} intel_drm_output_state_t;

// this structure must match MDSHDMITiming
typedef struct {
    uint32_t vrefresh;
    uint32_t hdisplay;
    uint32_t vdisplay;
    uint32_t interlace;
    uint32_t ratio;
} intel_display_mode_t;

class IntelHWComposer;

/**
 * Class: Overlay HAL implementation
 * This is a singleton implementation of hardware overlay.
 * this object will be shared between mutiple overlay control/data devices.
 * FIXME: overlayHAL should contact to the h/w to track the overlay h/w
 * state.
 */
class IntelHWComposerDrm {
private:
    int mDrmFd;
    drmModeConnectorPtr mHdmiConnector;
    intel_drm_output_state_t mDrmOutputsState;
    static IntelHWComposerDrm *mInstance;
    bool mIsPresentation;
    bool mOnlyHdmiHasVideo;
#ifdef TARGET_HAS_MULTIPLE_DISPLAY
    android::sp<IntelExternalDisplayMonitor> mMonitor;
#endif
private:
    IntelHWComposerDrm()
        : mDrmFd(-1)
#ifdef TARGET_HAS_MULTIPLE_DISPLAY
        , mMonitor(0)
#endif
    {
        memset(&mDrmOutputsState, 0, sizeof(intel_drm_output_state_t));
    }
    IntelHWComposerDrm(const IntelHWComposerDrm&);
    bool drmInit();
    void drmDestroy();

private:
     // basic function set
    drmModeConnectorPtr getConnector(int disp);
    drmModeConnectorPtr getHdmiConnector();
    void freeConnector(drmModeConnectorPtr connector);
    drmModeEncoderPtr getEncoder(int disp);
    void freeEncoder(drmModeEncoderPtr encoder);
    uint32_t getCrtcId(int disp);

    // DISP separate functions
    bool setMIPIDpms(drmModeConnectorPtr connector, bool on);
    bool setHDMIDpms(bool on);
    bool setMIPIScaling(int type);
    bool setHDMIScaling(int type);

    // mode and Fb functions
    bool isModeChanged(drmModeModeInfoPtr mode, intel_display_mode_t *displayMode);
    drmModeModeInfoPtr getSelectMode(intel_display_mode_t *displayMode,
                                    drmModeConnectorPtr connector);
    bool setupDrmFb(int disp, uint32_t fb_handler, drmModeModeInfoPtr mode);

public:
    ~IntelHWComposerDrm();
    static IntelHWComposerDrm& getInstance() {
        IntelHWComposerDrm *instance = mInstance;
        if (instance == 0) {
            instance = new IntelHWComposerDrm();
            mInstance = instance;
        }
        return *instance;
    }
    bool initialize(IntelHWComposer *hwc);
    int getDrmFd() const { return mDrmFd; }

    // default detect only called by initialize
    bool detectDrmModeInfo();

    // Connection and Mode setting
    bool detectDisplayConnection(int disp);
    drmModeModeInfoPtr selectDisplayDrmMode(int disp, intel_display_mode_t *displayMode);
    bool setDisplayDrmMode(int disp, uint32_t fb_handler, drmModeModeInfoPtr mode);
    bool handleDisplayDisConnection(int disp);
    bool detectMDSModeChange();

    // DPMS
    bool setDisplayDpms(int disp, bool blank);
    bool setHDMIPowerOff();
    // Vsync
    bool setDisplayVsyncs(int disp, bool on);
    // Scaling
    bool setDisplayScaling(int disp, int type);

    // DRM output states
    void setOutputConnection(const int output, drmModeConnection connection);
    drmModeConnection getOutputConnection(const int output);
    void setOutputMode(const int output, drmModeModeInfoPtr mode, int valid);
    drmModeModeInfoPtr getOutputMode(const int output);
    void setOutputFBInfo(const int output, drmModeFBPtr fbInfo);
    drmModeFBPtr getOutputFBInfo(const int output);
    uint32_t getOutputFBId(const int output);
    bool isValidOutputMode(const int output);
    void setDisplayMode(intel_overlay_mode_t displayMode);
    intel_overlay_mode_t getDisplayMode();
    intel_overlay_mode_t getOldDisplayMode();
    bool isVideoPlaying();
    bool isVideoPrepared();
    bool isOverlayOff();
    bool isHdmiConnected();
    bool notifyMipi(bool);
    bool notifyWidi(bool);
    bool getVideoInfo(int *displayW, int *displayH, int *fps, int *isinterlace);
    bool isDrmModeChanged(intel_display_mode_t* displayMode);
    bool isDrmModeFlagsMatched(drmModeModeInfoPtr mode, intel_display_mode_t* displayMode);
    void deleteDrmFb(int disp);
    bool setDisplayIed(bool on);
    void setPresentationMode(bool isPresentationMode);
    bool isPresentationMode();
    void setOnlyHdmiHasVideo(bool only);
    bool onlyHdmiHasVideo();
};

#endif /*__INTEL_HWCOMPOSER_DRM_H__*/
