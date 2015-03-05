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
#include <IntelHWComposerDrm.h>
#include <IntelHWComposerCfg.h>
#include <IntelOverlayUtil.h>
#include <IntelOverlayHW.h>
#include <fcntl.h>
#include <errno.h>
#include <cutils/log.h>
#include <cutils/atomic.h>
#include <cutils/ashmem.h>
#include <sys/mman.h>

#ifdef TARGET_HAS_MULTIPLE_DISPLAY
#include "display/MultiDisplayType.h"
#endif

IntelHWComposerDrm *IntelHWComposerDrm::mInstance(0);

IntelHWComposerDrm:: ~IntelHWComposerDrm() {
    ALOGD_IF(ALLOW_MONITOR_PRINT, "%s: destroying overlay HAL...\n", __func__);

    drmDestroy();
    mInstance = NULL;
}

bool IntelHWComposerDrm::drmInit()
{
    int fd = open("/dev/card0", O_RDWR, 0);
    if (fd < 0) {
        ALOGE("%s: drmOpen failed. %s\n", __func__, strerror(errno));
        return false;
    }

    mDrmFd = fd;
    mHdmiConnector = NULL;
    ALOGD("%s: successfully. mDrmFd %d\n", __func__, fd);
    return true;
}

void IntelHWComposerDrm::drmDestroy()
{
    ALOGD_IF(ALLOW_MONITOR_PRINT, "%s: destroying...\n", __func__);

    if(mDrmFd > 0) {
        drmClose(mDrmFd);
        mDrmFd = -1;
    }
}

void IntelHWComposerDrm::setOutputConnection(const int output,
                                             drmModeConnection connection)
{
    if (output < 0 || output >= OUTPUT_MAX)
        return;

    mDrmOutputsState.connections[output] = connection;
}

drmModeConnection
IntelHWComposerDrm:: getOutputConnection(const int output)
{
    drmModeConnection connection = DRM_MODE_DISCONNECTED;

    if (output < 0 || output >= OUTPUT_MAX)
        return connection;

    connection = mDrmOutputsState.connections[output];

    return connection;
}

void IntelHWComposerDrm::setOutputMode(const int output,
                                       drmModeModeInfoPtr mode,
                                       int valid)
{
    if (output < 0 || output >= OUTPUT_MAX || !mode)
        return;

    if (valid) {
        memcpy(&mDrmOutputsState.modes[output],
               mode, sizeof(drmModeModeInfo));
        mDrmOutputsState.mode_valid[output] = true;
    } else
        mDrmOutputsState.mode_valid[output] = false;
}

drmModeModeInfoPtr IntelHWComposerDrm::getOutputMode(const int output)
{
    if (output < 0 || output >= OUTPUT_MAX)
        return 0;

    return &mDrmOutputsState.modes[output];
}

void IntelHWComposerDrm::setOutputFBInfo(const int output,
                                       drmModeFBPtr fbInfo)
{
    if (output < 0 || output >= OUTPUT_MAX || !fbInfo)
        return;

    memcpy(&mDrmOutputsState.fbInfos[output], fbInfo, sizeof(drmModeFB));
}

drmModeFBPtr IntelHWComposerDrm::getOutputFBInfo(const int output)
{
    if (output < 0 || output >= OUTPUT_MAX)
        return 0;

    return &mDrmOutputsState.fbInfos[output];
}

uint32_t IntelHWComposerDrm::getOutputFBId(const int output)
{
    if (output < 0 || output >= OUTPUT_MAX)
        return 0;

    return mDrmOutputsState.fbInfos[output].fb_id;
}

bool IntelHWComposerDrm::isValidOutputMode(const int output)
{
    if (output < 0 || output >= OUTPUT_MAX)
        return false;

    return mDrmOutputsState.mode_valid[output];
}

void IntelHWComposerDrm::setDisplayMode(intel_overlay_mode_t displayMode)
{
    mDrmOutputsState.old_display_mode = mDrmOutputsState.display_mode;
    mDrmOutputsState.display_mode = displayMode;
}

intel_overlay_mode_t IntelHWComposerDrm::getDisplayMode()
{
    return mDrmOutputsState.display_mode;
}

bool IntelHWComposerDrm::isVideoPlaying()
{
#ifdef TARGET_HAS_MULTIPLE_DISPLAY
    if (mMonitor != NULL)
        return mMonitor->isVideoPlaying();
#endif
    return false;
}

bool IntelHWComposerDrm::isVideoPrepared()
{
#ifdef TARGET_HAS_MULTIPLE_DISPLAY
    if (mMonitor != NULL)
        return mMonitor->isVideoPrepared();
#endif
    return false;
}

bool IntelHWComposerDrm::isOverlayOff()
{
#ifdef TARGET_HAS_MULTIPLE_DISPLAY
    if (mMonitor != NULL)
        return mMonitor->isOverlayOff();
#endif
    return false;
}

bool IntelHWComposerDrm::isHdmiConnected()
{
#ifdef TARGET_HAS_MULTIPLE_DISPLAY
    if (mMonitor != NULL)
        return mMonitor->isHdmiConnected();
#endif
    return false;
}

bool IntelHWComposerDrm::notifyWidi(bool on)
{
#ifdef TARGET_HAS_MULTIPLE_DISPLAY
    if (mMonitor != NULL)
        return mMonitor->notifyWidi(on);
#endif
    return false;
}

bool IntelHWComposerDrm::notifyMipi(bool on)
{
#ifdef TARGET_HAS_MULTIPLE_DISPLAY
    if (mMonitor != NULL)
        return mMonitor->notifyMipi(on);
#endif
    return false;
}

bool IntelHWComposerDrm::getVideoInfo(int *displayW, int *displayH, int *fps, int *isinterlace)
{
#ifdef TARGET_HAS_MULTIPLE_DISPLAY
    if (mMonitor != NULL)
        return mMonitor->getVideoInfo(displayW, displayH, fps, isinterlace);
#endif
    return false;
}

intel_overlay_mode_t IntelHWComposerDrm::getOldDisplayMode()
{
    return mDrmOutputsState.old_display_mode;
}

bool IntelHWComposerDrm::isDrmModeChanged(intel_display_mode_t* displayMode) {
    drmModeModeInfoPtr mode = getOutputMode(OUTPUT_HDMI);
    return isModeChanged(mode, displayMode);
}

bool IntelHWComposerDrm::detectMDSModeChange()
{

#ifdef TARGET_HAS_MULTIPLE_DISPLAY

    drmModeConnection hdmi = getOutputConnection(OUTPUT_HDMI);
    int mdsMode = 0;
    if (mMonitor != 0) {
        mdsMode = mMonitor->getDisplayMode();
        ALOGD_IF(ALLOW_MONITOR_PRINT, "%s: get MDS Mode %d", __func__, mdsMode);
        //TODO: overlay only support OVERLAY_EXTEND and OVERLAY_MIPI0
        if (mdsMode == OVERLAY_EXTEND && hdmi == DRM_MODE_CONNECTED)
            setDisplayMode(OVERLAY_EXTEND);
        else if (mdsMode == OVERLAY_CLONE_MIPI0)
            setDisplayMode(OVERLAY_CLONE_MIPI0);
        else
            setDisplayMode(OVERLAY_MIPI0);
    } else
#endif
    {
        setDisplayMode(OVERLAY_MIPI0);
    }

    return true;
}

bool IntelHWComposerDrm::initialize(IntelHWComposer *hwc)
{
    bool ret = false;

    ALOGD_IF(ALLOW_MONITOR_PRINT, "%s: init...\n", __func__);

    if (mDrmFd < 0) {
        ret = drmInit();
        if(ret == false) {
            ALOGE("%s: drmInit failed\n", __func__);
            return ret;
        }
    }

#ifdef TARGET_HAS_MULTIPLE_DISPLAY
    // create external display monitor
    mMonitor = new IntelExternalDisplayMonitor(hwc);
    if (mMonitor == 0) {
        ALOGE("%s: failed to create external display monitor\n", __func__);
        drmDestroy();
        return false;
    }
#endif

    // detect display mode
    ret = detectDrmModeInfo();
    if (ret == false)
        ALOGW("%s: failed to detect DRM modes\n", __func__);
    else
        ALOGD_IF(ALLOW_MONITOR_PRINT, "%s: finish successfully.\n", __func__);

    // set old display mode the same detect mode
    mDrmOutputsState.old_display_mode =  mDrmOutputsState.display_mode;
    mIsPresentation = false;
    mOnlyHdmiHasVideo = false;
    return true;
}
bool IntelHWComposerDrm::detectDrmModeInfo()
{
    ALOGD_IF(ALLOW_MONITOR_PRINT, "%s: detecting drm mode info...\n", __func__);

    if (mDrmFd < 0) {
        ALOGE("%s: invalid drm FD\n", __func__);
        return false;
    }

    /*try to get drm resources*/
    drmModeResPtr resources = drmModeGetResources(mDrmFd);
    if (!resources || !resources->connectors) {
        ALOGE("%s: fail to get drm resources. %s\n", __func__, strerror(errno));
        return false;
    }

    /*get mipi0 info*/
    drmModeConnectorPtr connector = NULL;
    drmModeEncoderPtr encoder = NULL;
    drmModeCrtcPtr crtc = NULL;
    drmModeConnectorPtr connectors[OUTPUT_MAX];
    drmModeModeInfoPtr mode = NULL;
    drmModeFBPtr fbInfo = NULL;

    for (int i = 0; i < resources->count_connectors; i++) {
        connector = drmModeGetConnector(mDrmFd, resources->connectors[i]);
        if (!connector) {
            ALOGW("%s: fail to get drm connector\n", __func__);
            continue;
        }

        int outputIndex = -1;
        if (connector->connector_type == DRM_MODE_CONNECTOR_MIPI ||
            connector->connector_type == DRM_MODE_CONNECTOR_LVDS) {
            ALOGD_IF(ALLOW_MONITOR_PRINT, "%s: got MIPI/LVDS connector\n", __func__);
            if (connector->connector_type_id == 1)
                outputIndex = OUTPUT_MIPI0;
            else if (connector->connector_type_id == 2)
                outputIndex = OUTPUT_MIPI1;
            else {
                ALOGW("%s: unknown connector type\n", __func__);
                outputIndex = OUTPUT_MIPI0;
            }
        } else if (connector->connector_type == DRM_MODE_CONNECTOR_DVID) {
            ALOGD_IF(ALLOW_MONITOR_PRINT, "%s: got HDMI connector\n", __func__);
            outputIndex = OUTPUT_HDMI;
        }

        /*update connection status*/
        setOutputConnection(outputIndex, connector->connection);

        /*get related encoder*/
        encoder = drmModeGetEncoder(mDrmFd, connector->encoder_id);
        if (!encoder) {
            ALOGD_IF(ALLOW_MONITOR_PRINT, "%s: fail to get drm encoder\n", __func__);
            drmModeFreeConnector(connector);
            setOutputConnection(outputIndex, DRM_MODE_DISCONNECTED);
            continue;
        }

        /*get related crtc*/
        crtc = drmModeGetCrtc(mDrmFd, encoder->crtc_id);
        if (!crtc) {
            ALOGD_IF(ALLOW_MONITOR_PRINT, "%s: fail to get drm crtc\n", __func__);
            drmModeFreeEncoder(encoder);
            drmModeFreeConnector(connector);
            setOutputConnection(outputIndex, DRM_MODE_DISCONNECTED);
            continue;
        }
        /*set crtc mode*/
        setOutputMode(outputIndex, &crtc->mode, crtc->mode_valid);

        // get fb info
        fbInfo = drmModeGetFB(mDrmFd, crtc->buffer_id);
        if (!fbInfo) {
            ALOGD("%s: fail to get fb info\n", __func__);
            drmModeFreeCrtc(crtc);
            drmModeFreeEncoder(encoder);
            drmModeFreeConnector(connector);
            setOutputConnection(outputIndex, DRM_MODE_DISCONNECTED);
            continue;
        }

        setOutputFBInfo(outputIndex, fbInfo);

        /*free all crtc/connector/encoder*/
        drmModeFreeFB(fbInfo);
        drmModeFreeCrtc(crtc);
        drmModeFreeEncoder(encoder);
        drmModeFreeConnector(connector);
    }

    drmModeFreeResources(resources);

    drmModeConnection mipi0 = getOutputConnection(OUTPUT_MIPI0);
    drmModeConnection mipi1 = getOutputConnection(OUTPUT_MIPI1);
    drmModeConnection hdmi = getOutputConnection(OUTPUT_HDMI);

    detectMDSModeChange();

    ALOGD_IF(ALLOW_MONITOR_PRINT,
           "%s: mipi/lvds %s, mipi1 %s, hdmi %s, displayMode %d\n",
        __func__,
        ((mipi0 == DRM_MODE_CONNECTED) ? "connected" : "disconnected"),
        ((mipi1 == DRM_MODE_CONNECTED) ? "connected" : "disconnected"),
        ((hdmi == DRM_MODE_CONNECTED) ? "connected" : "disconnected"),
        getDisplayMode());

    return true;
}

drmModeConnectorPtr
IntelHWComposerDrm::getConnector(int disp)
{
    if (mDrmFd < 0) {
        ALOGE("%s: invalid drm FD\n", __func__);
        return NULL;
    }
    uint32_t req_connector_type = 0;
    uint32_t req_connector_type_id = 1;

    switch (disp) {
        case OUTPUT_MIPI0:
        case OUTPUT_MIPI1:
            req_connector_type = DRM_MODE_CONNECTOR_MIPI;
            req_connector_type_id = disp ? 2 : 1;
            break;
        case OUTPUT_HDMI:
            req_connector_type = DRM_MODE_CONNECTOR_DVID;
            break;
        default:
            ALOGW("%s: invalid device number: %d\n", __func__, disp);
            return NULL;
    }

    drmModeResPtr resources = drmModeGetResources(mDrmFd);
    if (!resources || !resources->connectors) {
        ALOGE("%s: fail to get drm resources. %s\n", __func__, strerror(errno));
        return NULL;
    }

    drmModeConnectorPtr connector = NULL;
    // get requested connector type and id
    // search connector
    for (int i = 0; i < resources->count_connectors; i++) {
        connector = drmModeGetConnector(mDrmFd, resources->connectors[i]);
        if (!connector) {
            ALOGW("%s: fail to get drm connector\n", __func__);
            continue;
        }

        if (connector->connector_type == req_connector_type &&
            connector->connector_type_id == req_connector_type_id)
            break;

        drmModeFreeConnector(connector);
        connector = NULL;
    }

    drmModeFreeResources(resources);

    if (connector == NULL)
        ALOGW("%s: fail to get required connector\n", __func__);

    return connector;
}

void IntelHWComposerDrm::freeConnector(drmModeConnectorPtr connector)
{
    if (connector != NULL) {
        if (connector->connector_type == DRM_MODE_CONNECTOR_DVID)
            mHdmiConnector = NULL;

        drmModeFreeConnector(connector);
    }
}

drmModeConnectorPtr
IntelHWComposerDrm::getHdmiConnector() {
    if (mHdmiConnector == NULL)
        mHdmiConnector = getConnector(OUTPUT_HDMI);
    if (mHdmiConnector == NULL || mHdmiConnector->modes == NULL) {
        LOGE("%s: Failed to get HDMI connector", __func__);
        return NULL;
    }
    return mHdmiConnector;
}

drmModeEncoderPtr
IntelHWComposerDrm::getEncoder(int disp)
{
    if (mDrmFd < 0) {
        ALOGE("%s: invalid drm FD\n", __func__);
        return NULL;
    }

    uint32_t req_encoder_type = 0;

    switch (disp) {
        case OUTPUT_MIPI0:
        case OUTPUT_MIPI1:
            req_encoder_type = DRM_MODE_ENCODER_MIPI;
            break;
        case OUTPUT_HDMI:
            req_encoder_type = DRM_MODE_ENCODER_TMDS;
            break;
        default:
            ALOGW("%s: invalid device number: %d\n", __func__, disp);
            return NULL;
    }

    drmModeResPtr resources = drmModeGetResources(mDrmFd);
    if (!resources || !resources->encoders) {
        ALOGE("%s: fail to get drm resources. %s\n", __func__, strerror(errno));
        return NULL;
    }

    drmModeEncoderPtr encoder = NULL;
    for (int i = 0; i < resources->count_encoders; i++) {
        encoder = drmModeGetEncoder(mDrmFd, resources->encoders[i]);

        if (!encoder) {
            ALOGW("%s: Failed to get encoder\n", __func__);
            continue;
        }

        if (encoder->encoder_type == req_encoder_type)
            break;

        drmModeFreeEncoder(encoder);
        encoder = NULL;
    }
    drmModeFreeResources(resources);

    if (encoder == NULL)
        ALOGW("%s: fail to get required encoder\n", __func__);

    return encoder;
}

void IntelHWComposerDrm::freeEncoder(drmModeEncoderPtr encoder)
{
    if (encoder != NULL)
        drmModeFreeEncoder(encoder);
}

uint32_t IntelHWComposerDrm::getCrtcId(int disp)
{
    if (mDrmFd < 0) {
        ALOGE("%s: invalid drm FD\n", __func__);
        return 0;
    }

    drmModeEncoderPtr encoder = NULL;
    drmModeResPtr resources = NULL;
    drmModeCrtcPtr crtc = NULL;
    uint32_t crtc_id = 0;
    int i = 0;

    if ((encoder = getEncoder(disp)) == NULL)
        return 0;

    crtc_id = encoder->crtc_id;
    freeEncoder(encoder);

    if (crtc_id == 0) {
        /* Query an available crtc to use */
        if ((resources = drmModeGetResources(mDrmFd)) == NULL)
            return 0;

        if (!resources->crtcs)
            return 0;

        for (i = 0; i < resources->count_crtcs; i++) {
            crtc = drmModeGetCrtc(mDrmFd, resources->crtcs[i]);
            if (!crtc) {
                LOGE("%s: Failed to get crtc %d, error is %s",
                        __func__, resources->crtcs[i], strerror(errno));
                continue;
            }
            if (crtc->buffer_id == 0) {
                crtc_id = crtc->crtc_id;
                drmModeFreeCrtc(crtc);
                break;
            }
            drmModeFreeCrtc(crtc);
        }
    }

    return crtc_id;
}

// DISP separate functions
bool IntelHWComposerDrm::setMIPIDpms(drmModeConnectorPtr connector, bool on) {
    return true;
}

bool IntelHWComposerDrm::setHDMIDpms(bool on) {
    return true;
}

bool IntelHWComposerDrm::setMIPIScaling(int type) {
    return true;
}

bool IntelHWComposerDrm::setHDMIScaling(int type) {
    return true;
}

drmModeModeInfoPtr
IntelHWComposerDrm::selectDisplayDrmMode(int disp, intel_display_mode_t *displayMode)
{
    if (disp != OUTPUT_HDMI)
        return NULL;
    drmModeModeInfoPtr mode = getOutputMode(OUTPUT_HDMI);

    // If mode no change, return current mode
    if (displayMode && !isModeChanged(mode, displayMode))
        return mode;
    if (mHdmiConnector != NULL) {
        freeConnector(mHdmiConnector);
        mHdmiConnector = NULL;
    }
    drmModeConnectorPtr connector;
    connector = getHdmiConnector();
    if (!connector) {
        ALOGW("%s: fail to get drm connector\n", __func__);
        return NULL;
    }

    mode = getSelectMode(displayMode, connector);

    if (!mode) {
        ALOGW("%s: fail to get selected mode or any other mode! \n", __func__);
        return NULL;
    }

    // update current mode to be selected
    setOutputMode(OUTPUT_HDMI, mode, 1);

    //freeConnector(connector);

    mode = getOutputMode(OUTPUT_HDMI);
    LOGD("%s: mode is %dx%d@%dHz, 0x%x\n", __func__,
             mode->hdisplay, mode->vdisplay, mode->vrefresh, mode->flags);

    return mode;
}

// mode and Fb functions
drmModeModeInfoPtr
IntelHWComposerDrm::getSelectMode(intel_display_mode_t *displayMode,
                                  drmModeConnectorPtr connector)
{
    int index = -1;
    int i = 0;
    int preferred = -1;
    int max = 0;
    drmModeModeInfoPtr mode;

    // mode sort as big --> small
    for (i = 0; i < connector->count_modes; i++) {
        mode = &connector->modes[i];

        // set to requested mode if found
        if (displayMode &&
            displayMode->vrefresh == mode->vrefresh &&
            displayMode->hdisplay == mode->hdisplay &&
            displayMode->vdisplay == mode->vdisplay &&
            isDrmModeFlagsMatched(mode, displayMode)) {
            index = i;
            break;
        }
        // Get prefer mode
        if (mode->type & DRM_MODE_TYPE_PREFERRED)
            preferred = i;

        // Get the max timing
        if ((connector->modes[i].hdisplay > connector->modes[max].hdisplay) &&
            (connector->modes[i].vdisplay > connector->modes[max].vdisplay)) {
            max = i;
        } else if (
            (connector->modes[i].hdisplay == connector->modes[max].hdisplay) &&
            (connector->modes[i].vdisplay == connector->modes[max].vdisplay) &&
            (connector->modes[i].vrefresh > connector->modes[max].vrefresh)) {
            max = i;
        }
    }
    // could not find requested mode
    if (index == -1) {
        if (preferred != -1)
            index = preferred;
        else
            index = max;
    }
    LOGD("Get the timing, %d, %d, %d", preferred, max, index);
    return &connector->modes[index];
}

bool IntelHWComposerDrm::isModeChanged(drmModeModeInfoPtr mode,
                                       intel_display_mode_t *displayMode)
{
    // set to requested mode if found
    if (mode == NULL || displayMode == NULL) {
        return true;
    }

    if (displayMode->vrefresh == mode->vrefresh &&
        displayMode->hdisplay == mode->hdisplay &&
        displayMode->vdisplay == mode->vdisplay &&
        isDrmModeFlagsMatched(mode, displayMode)) {
        return false;
    } else {
        return true;
    }
}

bool IntelHWComposerDrm::isDrmModeFlagsMatched(drmModeModeInfoPtr mode, intel_display_mode_t* displayMode)
{
    if (mode == NULL || displayMode == NULL)
        return true;
    uint32_t flags = 0;
    if (displayMode->interlace)
        flags |= DRM_MODE_FLAG_INTERLACE;
    if (displayMode->ratio == 2)
        flags |= DRM_MODE_FLAG_PAR16_9;
    else if (displayMode->ratio == 1)
        flags |= DRM_MODE_FLAG_PAR4_3;

    LOGD("%s: 0x%x, 0x%x", __func__, flags, mode->flags);
    if (flags == 0)
        return true;

    if ((mode->flags & flags) == flags)
        return true;

    return false;
}

bool IntelHWComposerDrm::setupDrmFb(int disp,
                                    uint32_t fb_handler,
                                    drmModeModeInfoPtr mode)
{
    if (mDrmFd < 0) {
        ALOGE("%s: invalid drm FD\n", __func__);
        return false;
    }

    if (!mode) {
        ALOGW("%s: invalid mode !\n", __func__);
        return false;
    }

    int width = mode->hdisplay;
    int height = mode->vdisplay;
    int stride = align_to(width*4, 64);
    uint32_t fb_id = 0;

    // Drm add FB
    int ret = drmModeAddFB(mDrmFd, width, height, 24, 32,
                  stride, (uint32_t)(fb_handler), &fb_id);
    if (ret) {
        LOGE("%s: Failed to add fb !", __func__);
        return false;
    }

    // add to local output structure
    drmModeFBPtr fbInfo = drmModeGetFB(mDrmFd, fb_id);
    if (!fbInfo) {
        LOGE("%s: Failed to get fbInfo! ", __func__);
        return false;
    }
    setOutputFBInfo(disp, fbInfo);
    drmModeFreeFB(fbInfo);

    return true;
}

void IntelHWComposerDrm::deleteDrmFb(int disp)
{
    if (mDrmFd < 0) {
        ALOGE("%s: invalid drm FD\n", __func__);
        return;
    }

    drmModeFBPtr fbInfo = getOutputFBInfo(disp);

    if (fbInfo) {
        ALOGD_IF(ALLOW_MONITOR_PRINT, "%s: rm FB !\n", __func__);
        drmModeRmFB(mDrmFd, fbInfo->fb_id);
    }
}

bool IntelHWComposerDrm::handleDisplayDisConnection(int disp)
{
    if (disp == OUTPUT_HDMI) {
        if (mHdmiConnector != NULL) {
            drmModeFreeConnector(mHdmiConnector);
            mHdmiConnector = NULL;
        }
        drmModeModeInfo mode;
        drmModeConnection connection = DRM_MODE_DISCONNECTED;
        setOutputConnection(disp, connection);
        memset(&mode, 0, sizeof(drmModeModeInfo));
        setOutputMode(disp, &mode, 1);
    }

    return true;
}

// Connection and Mode setting
bool IntelHWComposerDrm::detectDisplayConnection(int disp)
{
    ALOGD_IF(ALLOW_MONITOR_PRINT,
              "%s: detecting display %d drm mode info...\n", __func__, disp);

    //get mipi0 info
    drmModeConnectorPtr connector = NULL;
    drmModeModeInfoPtr mode = NULL;
    uint32_t connector_type;

    connector = getConnector(disp);
    if (!connector) {
        ALOGW("%s: fail to get drm connector\n", __func__);
        return false;
    }

    //update connection status
    setOutputConnection(disp, connector->connection);

    if (connector->connection != DRM_MODE_CONNECTED) {
        freeConnector(connector);
        return false;
    }

    //update mode info
    mode = getSelectMode(NULL, connector);
    if (mode)
        setOutputMode(disp, mode, 1);

    freeConnector(connector);

    return true;
}

bool IntelHWComposerDrm::setDisplayDrmMode(int disp,
                                           uint32_t fb_handler,
                                           drmModeModeInfoPtr mode)
{
    drmModeConnectorPtr connector = NULL;
    uint32_t crtc_id = 0;
    uint32_t fb_id = 0;

    if (mDrmFd < 0) {
        ALOGE("%s: invalid drm FD\n", __func__);
        return false;
    }

    if (!fb_handler) {
        ALOGE("%s: invalid fb handler\n", __func__);
        return false;
    }
    if (disp != OUTPUT_HDMI)
        return false;
    connector = getHdmiConnector();
    if (!connector) {
        ALOGW("%s: fail to get drm connector\n", __func__);
        return false;
    }

    setOutputConnection(OUTPUT_HDMI, connector->connection);

    // re-check connection status
    if (connector->connection != DRM_MODE_CONNECTED) {
        freeConnector(connector);
        return false;
    }

    // get fb_id
    if (setupDrmFb(OUTPUT_HDMI, fb_handler, mode))
        fb_id = getOutputFBId(OUTPUT_HDMI);

    if (!fb_id) {
        ALOGW("%s: fail to get drm fb id\n", __func__);
        freeConnector(connector);
        return false;
    }

    // get crtc_id
    crtc_id = getCrtcId(OUTPUT_HDMI);
    if (!crtc_id) {
        ALOGW("%s: fail to get drm crtc id\n", __func__);
        freeConnector(connector);
        return false;
    }

    // crtc mode setting
    int ret = drmModeSetCrtc(mDrmFd, crtc_id, fb_id, 0, 0,
                   &connector->connector_id, 1, mode);
    if (ret) {
        ALOGW("drm Mode Set Crtc Error: 0x%x!\n", ret);
        freeConnector(connector);
        return false;
    }

    return true;
}

// DPMS
bool IntelHWComposerDrm::setDisplayDpms(int disp, bool blank)
{
    int ret=0;

    if (disp == OUTPUT_MIPI0) {
        // Set MIPI On/Off
        drmModeConnectorPtr connector;
        drmModePropertyPtr props = NULL;

        if ((connector = getConnector(disp)) == NULL) {
            ALOGW("%s: failed to get connector :%d!\n", __func__, disp);
            goto err;
        }

        if (connector->connection != DRM_MODE_CONNECTED) {
            ALOGW("%s: connector %d is not connected!\n", __func__, disp);
            freeConnector(connector);
            goto err;
        }

        for (int i = 0; i < connector->count_props; i++) {
            props = drmModeGetProperty(mDrmFd, connector->props[i]);
            if (!props) continue;

            if (!strcmp(props->name, "DPMS")) {
                ALOGD_IF(ALLOW_MONITOR_PRINT,
                         "%s: %s %u", __func__,
                         (blank == 0) ? "On" : "Off",
                         connector->connector_id);
                ret = drmModeConnectorSetProperty(mDrmFd,
                                connector->connector_id,
                                props->prop_id,
                                (blank==0) ? DRM_MODE_DPMS_ON : DRM_MODE_DPMS_OFF);
                drmModeFreeProperty(props);
                break;
            }
            drmModeFreeProperty(props);
        }
    } else if (disp == OUTPUT_HDMI) {
        // Set HDMI On/Off
        struct drm_psb_disp_ctrl dp_ctrl;

        ALOGD_IF(ALLOW_MONITOR_PRINT,
                 "%s: %s", __func__,
                 (blank == 0) ? "On" : "Off");
        memset(&dp_ctrl, 0, sizeof(dp_ctrl));
        dp_ctrl.cmd =
              (blank==0) ? DRM_PSB_DISP_PLANEB_ENABLE : DRM_PSB_DISP_PLANEB_DISABLE;

        ret = drmCommandWriteRead(mDrmFd, DRM_PSB_HDMI_FB_CMD, &dp_ctrl, sizeof(dp_ctrl));
    }

    if (ret != 0) {
        ALOGW("%s: connector %d dpms failed!\n", __func__, disp);
        goto err;
    }

    return true;
err:
    return false;
}

bool IntelHWComposerDrm::setHDMIPowerOff()
{
    int ret=0;
    struct drm_psb_disp_ctrl dp_ctrl;

    memset(&dp_ctrl, 0, sizeof(dp_ctrl));
    dp_ctrl.cmd = DRM_PSB_HDMI_OSPM_ISLAND_DOWN;

    ret = drmCommandWriteRead(mDrmFd,
            DRM_PSB_HDMI_FB_CMD, &dp_ctrl, sizeof(dp_ctrl));

    return (ret==0)?true:false;
}

// Vsync
bool IntelHWComposerDrm::setDisplayVsyncs(int disp, bool on) {
    return true;
}

// Scaling
bool IntelHWComposerDrm::setDisplayScaling(int disp, int type) {
    return true;
}

bool IntelHWComposerDrm::setDisplayIed(bool on)
{
    int ret;

    ret = drmCommandNone(mDrmFd, DRM_PSB_HDCP_DISPLAY_IED_ON);

    return (ret == 0) ? true : false;
}

void IntelHWComposerDrm::setPresentationMode(bool isPresentationMode) {
    mIsPresentation = isPresentationMode;
}

bool IntelHWComposerDrm::isPresentationMode() {
    ALOGD_IF(ALLOW_MONITOR_PRINT, "%s presentation mode", (mIsPresentation ? "Is" : "Isn't"));
    return mIsPresentation;
}

void IntelHWComposerDrm::setOnlyHdmiHasVideo(bool only) {
    mOnlyHdmiHasVideo = only;
}

bool IntelHWComposerDrm::onlyHdmiHasVideo() {
    if (mOnlyHdmiHasVideo)
        ALOGD_IF(ALLOW_MONITOR_PRINT, "Only HDMI has video layer");
    return mOnlyHdmiHasVideo;
}
