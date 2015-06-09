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
#include <cutils/log.h>
#include <cutils/atomic.h>
#include <cutils/properties.h>

#include <IntelHWComposer.h>
#include <IntelOverlayUtil.h>
#include <IntelHWComposerCfg.h>
#include <IntelUtility.h>

#ifdef INTEL_WIDI
#include <WidiDisplayDevice.h>
#endif

IntelHWComposer::~IntelHWComposer()
{
    ALOGD_IF(ALLOW_HWC_PRINT, "%s\n", __func__);

    delete mPlaneManager;
    delete mBufferManager;
    delete mGrallocBufferManager;
    delete mDrm;

    for (size_t i=0; i<DISPLAY_NUM; i++) {
        delete mDisplayDevice[i];
     }
    // stop uevent observer
    stopObserver();
}

void IntelHWComposer::signalHpdCompletion()
{
    android::Mutex::Autolock _l(mHpdLock);
    if (mHpdCompletion == false) {
        mHpdCompletion = true;
        mHpdCondition.signal();
        ALOGD("%s: send out hpd completion signal\n", __func__);
    }
}

void IntelHWComposer::waitForHpdCompletion()
{
    android::Mutex::Autolock _l(mHpdLock);

    // time out for 300ms
    nsecs_t reltime = 300000000;
    mHpdCompletion = false;
    while(!mHpdCompletion) {
        mHpdCondition.waitRelative(mHpdLock, reltime);
    }

    ALOGD("%s: receive hpd completion signal: %d\n", __func__, mHpdCompletion?1:0);
}

bool IntelHWComposer::handleDisplayModeChange()
{
    android::Mutex::Autolock _l(mLock);
    ALOGD_IF(ALLOW_HWC_PRINT, "handleDisplayModeChange");

    if (!mDrm) {
         ALOGW("%s: mDrm is not intilized!\n", __func__);
         return false;
    }

    mDrm->detectMDSModeChange();

    if (needSwitchVsyncSrc())
        vsyncControl_l(1);

    for (size_t i=0; i<DISPLAY_NUM; i++)
        if (mDisplayDevice[i])
            mDisplayDevice[i]->onHotplugEvent(true);

    return true;
}

bool IntelHWComposer::handleHotplugEvent(int hpd, void *data)
{
    bool ret = false;

    ALOGD_IF(ALLOW_HWC_PRINT, "handleHotplugEvent");

    if (!mDrm) {
        ALOGW("%s: mDrm is not intialized!\n", __func__);
        return false;
    }

    if (hpd) {
        // get display mode
        intel_display_mode_t *s_mode = (intel_display_mode_t *)data;
        drmModeModeInfoPtr mode;
        mode = mDrm->selectDisplayDrmMode(OUTPUT_HDMI, s_mode);
        if (!mode)
            return false;

        // alloc buffer;
        mHDMIFBHandle.size = mode->vdisplay * align_to(mode->hdisplay * 4, 64);
        ret = mGrallocBufferManager->alloc(mHDMIFBHandle.size,
                                      &mHDMIFBHandle.umhandle,
                                      &mHDMIFBHandle.kmhandle);
        if (!ret)
            return false;

        // mode setting;
        ret = mDrm->setDisplayDrmMode(OUTPUT_HDMI, mHDMIFBHandle.kmhandle, mode);
        if (!ret)
            return false;

        ALOGD("%s: detected hdmi hotplug event:%s\n", __func__, hpd?"IN":"OUT");
        handleDisplayModeChange();

        /* hwc_dev->procs is set right after the device is opened, but there is
         * still a race condition where a hotplug event might occur after the open
         * but before the procs are registered. */
        if (mProcs && mProcs->vsync) {
            mProcs->hotplug(mProcs, HWC_DISPLAY_EXTERNAL, hpd);
        }
    } else {
        ret = mDrm->handleDisplayDisConnection(OUTPUT_HDMI);
        if (!ret)
            return false;

        ALOGD("%s: detected hdmi hotplug event:%s\n", __func__, hpd?"IN":"OUT");
        handleDisplayModeChange();

        /* hwc_dev->procs is set right after the device is opened, but there is
         * still a race condition where a hotplug event might occur after the open
         * but before the procs are registered. */
        if (mProcs && mProcs->vsync) {
            mProcs->hotplug(mProcs, HWC_DISPLAY_EXTERNAL, hpd);
        }
        // TODO: here we need to wait for the plug-out take effect.
        waitForHpdCompletion();
        // rm FB
        mDrm->deleteDrmFb(OUTPUT_HDMI);
        // release buffer;
        ret = mGrallocBufferManager->dealloc(mHDMIFBHandle.umhandle);
        if (!ret)
            return false;

        memset(&mHDMIFBHandle, 0, sizeof(mHDMIFBHandle));
    }
    return true;
}

bool IntelHWComposer::handleDynamicModeSetting(void *data)
{
    bool ret = false;

    ALOGD_IF(ALLOW_HWC_PRINT, "%s: handle Dynamic mode setting!\n", __func__);
    // check HDMI timing
    if (!mDrm->isDrmModeChanged((intel_display_mode_t*)data)){
        ALOGD("Same HDMI timing, ignore this setting");
        return true;
    }
    // send plug-out to SF for mode changing on the same device
    // otherwise SF will bypass the plug-in message as there is
    // no connection change;
    ret = handleHotplugEvent(0, NULL);
    if (!ret) {
        ALOGW("%s: send fake unplug event failed!\n", __func__);
        goto out;
    }

    // then change the mode and send plug-in to SF
    ret = handleHotplugEvent(1, data);
    if (!ret) {
        ALOGW("%s: send plug in event failed!\n", __func__);
        goto out;
    }
out:
    return ret;
}

bool IntelHWComposer::onUEvent(int msgType, void* msg, int msgLen)
{
    bool ret = false;
#ifdef TARGET_HAS_MULTIPLE_DISPLAY
    if (msgType == IntelExternalDisplayMonitor::MSG_TYPE_MDS)
        ret = handleDisplayModeChange();

    // handle hdmi plug in;
    if (msgType == IntelExternalDisplayMonitor::MSG_TYPE_MDS_HOTPLUG_IN)
        ret = handleHotplugEvent(1, msg);

    // handle hdmi plug out;
    if (msgType == IntelExternalDisplayMonitor::MSG_TYPE_MDS_HOTPLUG_OUT)
        ret = handleHotplugEvent(0, NULL);

    // handle dynamic mode setting
    if (msgType == IntelExternalDisplayMonitor::MSG_TYPE_MDS_TIMING_DYNAMIC_SETTING)
        ret = handleDynamicModeSetting(msg);

    return ret;
#endif

    char* szMsg = (char*)msg;
    if (strcmp(szMsg, "change@/devices/pci0000:00/0000:00:02.0/drm/card0"))
        return true;
    szMsg += strlen(szMsg) + 1;

    do {
        if (!strncmp(szMsg, "HOTPLUG_IN=1", strlen("HOTPLUG_IN=1"))) {
            ALOGD("%s: detected hdmi hotplug event:%s\n", __func__, szMsg);
            ret = handleHotplugEvent(1, NULL);
            break;
        } else if (!strncmp(szMsg, "HOTPLUG_OUT=1", strlen("HOTPLUG_OUT=1"))) {
            ret = handleHotplugEvent(0, NULL);
            break;
        }

        szMsg += strlen(szMsg) + 1;
    } while (*szMsg);

    return ret;
}

void IntelHWComposer::vsync(int64_t timestamp, int pipe)
{
    if (mProcs && mProcs->vsync) {
        ALOGV("%s: report vsync timestamp %llu, pipe %d, active 0x%x", __func__,
             timestamp, pipe, mActiveVsyncs);
        if ((1 << pipe) & mActiveVsyncs)
            mProcs->vsync(const_cast<hwc_procs_t*>(mProcs), 0, timestamp);
    }
    mLastVsync = timestamp;
}

uint32_t IntelHWComposer::disableUnusedVsyncs(uint32_t target)
{
    uint32_t unusedVsyncs = mActiveVsyncs & (~target);
    struct drm_psb_vsync_set_arg arg;
    uint32_t vsync;
    int i, ret;

    ALOGV("disableVsync: unusedVsyncs 0x%x\n", unusedVsyncs);

    if (!unusedVsyncs)
        goto disable_out;

    /*disable unused vsyncs*/
    for (i = 0; i < VSYNC_SRC_NUM; i++) {
        vsync = (1 << i);
        if (!(vsync & unusedVsyncs))
            continue;

        /*disable vsync*/
        if (i == VSYNC_SRC_FAKE)
            mFakeVsync->setEnabled(false, mLastVsync);
        else {
            memset(&arg, 0, sizeof(struct drm_psb_vsync_set_arg));
            arg.vsync_operation_mask = VSYNC_DISABLE | GET_VSYNC_COUNT;

            // pipe select
            if (i == VSYNC_SRC_HDMI)
                arg.vsync.pipe = 1;
            else
                arg.vsync.pipe = 0;

            ret = drmCommandWriteRead(mDrm->getDrmFd(), DRM_PSB_VSYNC_SET,
                                      &arg, sizeof(arg));
            if (ret) {
                ALOGW("%s: failed to enable/disable vsync %d\n", __func__, ret);
                continue;
            }
            mVsyncsEnabled = 0;
            mVsyncsCount = arg.vsync.vsync_count;
            mVsyncsTimestamp = arg.vsync.timestamp;
        }

        /*disabled successfully, remove it from unused vsyncs*/
        unusedVsyncs &= ~vsync;
    }
disable_out:
    return unusedVsyncs;
}

uint32_t IntelHWComposer::enableVsyncs(uint32_t target)
{
    uint32_t enabledVsyncs = 0;
    struct drm_psb_vsync_set_arg arg;
    uint32_t vsync;
    int i, ret;

    ALOGV("enableVsyn: enable vsyncs 0x%x\n", target);

    if (!target) {
        enabledVsyncs = 0;
        goto enable_out;
    }

    // remove all active vsyncs from target
    target &= ~mActiveVsyncs;
    if (!target) {
        enabledVsyncs = mActiveVsyncs;
        goto enable_out;
    }

    // enable vsyncs which is currently inactive
    for (i = 0; i < VSYNC_SRC_NUM; i++) {
        vsync = (1 << i);
        if (!(vsync & target))
            continue;

        /*enable vsync*/
        if (i == VSYNC_SRC_FAKE)
            mFakeVsync->setEnabled(true, mLastVsync);
        else {
            memset(&arg, 0, sizeof(struct drm_psb_vsync_set_arg));
            arg.vsync_operation_mask = VSYNC_ENABLE | GET_VSYNC_COUNT;

            // pipe select
            if (i == VSYNC_SRC_HDMI)
                arg.vsync.pipe = 1;
            else
                arg.vsync.pipe = 0;

            ret = drmCommandWriteRead(mDrm->getDrmFd(), DRM_PSB_VSYNC_SET,
                                      &arg, sizeof(arg));
            if (ret) {
                ALOGW("%s: failed to enable vsync %d\n", __func__, ret);
                continue;
            }
            mVsyncsEnabled = 1;
            mVsyncsCount = arg.vsync.vsync_count;
            mVsyncsTimestamp = arg.vsync.timestamp;
        }

        /*enabled successfully*/
        enabledVsyncs |= vsync;
    }
enable_out:
    return enabledVsyncs;
}

uint32_t IntelHWComposer::getTargetVsync()
{
    uint32_t targetVsyncs = 0;

    if (OVERLAY_EXTEND == mDrm->getDisplayMode())
        targetVsyncs |= (1 << VSYNC_SRC_HDMI);
    else if (mExtendedModeInfo.videoSentToWidi && mDrm->isVideoPlaying()) {
        targetVsyncs |= (1 << VSYNC_SRC_FAKE);
    } else {
        targetVsyncs |= (1 << VSYNC_SRC_MIPI);
    }

    return targetVsyncs;
}

bool IntelHWComposer::needSwitchVsyncSrc()
{
    uint32_t targetVsyncs;

    if (!mActiveVsyncs)
        return false;

    targetVsyncs = getTargetVsync();
    if (targetVsyncs != mActiveVsyncs)
        return true;

    return false;
}

bool IntelHWComposer::vsyncControl(int enabled)
{
    android::Mutex::Autolock _l(mLock);
    return vsyncControl_l(enabled);
}

bool IntelHWComposer::vsyncControl_l(int enabled)
{
    uint32_t targetVsyncs = 0;
    uint32_t activeVsyncs = 0;
    uint32_t enabledVsyncs = 0;

    ALOGV("vsyncControl, enabled %d\n", enabled);

    if (enabled != 0 && enabled != 1)
        return false;

    // for disable vsync request, disable all active vsyncs
    if (!enabled) {
        targetVsyncs = 0;
        goto disable_vsyncs;
    }
    targetVsyncs = getTargetVsync();

    // enable selected vsyncs
    enabledVsyncs = enableVsyncs(targetVsyncs);

disable_vsyncs:
    // disable unused vsyncs
    activeVsyncs = disableUnusedVsyncs(targetVsyncs);

    // update active vsyncs
    mActiveVsyncs = enabledVsyncs | activeVsyncs;
    mVsync->setActiveVsyncs(mActiveVsyncs);

    ALOGV("vsyncControl: activeVsyncs 0x%x\n", mActiveVsyncs);
    return true;
}

bool IntelHWComposer::release()
{
    ALOGD("hwcomposer release");

    if (!initCheck())
        return false;

    android::Mutex::Autolock _l(mLock);
    // disable all devices
    for (size_t i=0 ; i<DISPLAY_NUM ; i++) {
        if (mDisplayDevice[i])
            mDisplayDevice[i]->release();
    }

    return true;
}

bool IntelHWComposer::dumpDisplayStat()
{
    struct drm_psb_register_rw_arg arg;
    struct drm_psb_vsync_set_arg vsync_arg;
    int ret;

    // dump vsync info
    memset(&vsync_arg, 0, sizeof(struct drm_psb_vsync_set_arg));
    vsync_arg.vsync_operation_mask = GET_VSYNC_COUNT;
    vsync_arg.vsync.pipe = 0;

    ret = drmCommandWriteRead(mDrm->getDrmFd(), DRM_PSB_VSYNC_SET,
                               &vsync_arg, sizeof(vsync_arg));
    if (ret) {
        ALOGW("%s: failed to dump vsync info %d\n", __func__, ret);
        goto out;
    }

    dumpPrintf("-------------Display Stat -------------------\n");
    dumpPrintf("  + last vsync count: %d, timestamp %d ms \n",
                     mVsyncsCount, mVsyncsTimestamp/1000000);
    dumpPrintf("  + current vsync count: %d, timestamp %d ms \n",
                     vsync_arg.vsync.vsync_count,
                     vsync_arg.vsync.timestamp/1000000);

    // Read pipe stat register
    memset(&arg, 0, sizeof(struct drm_psb_register_rw_arg));
    arg.display_read_mask = REGRWBITS_PIPEASTAT;

    ret = drmCommandWriteRead(mDrm->getDrmFd(), DRM_PSB_REGISTER_RW,
                              &arg, sizeof(arg));
    if (ret) {
        ALOGW("%s: failed to dump display registers %d\n", __func__, ret);
        goto out;
    }

    dumpPrintf("  + PIPEA STAT: 0x%x \n", arg.display.pipestat_a);

    // Read interrupt mask register
    memset(&arg, 0, sizeof(struct drm_psb_register_rw_arg));
    arg.display_read_mask = REGRWBITS_INT_MASK;

    ret = drmCommandWriteRead(mDrm->getDrmFd(), DRM_PSB_REGISTER_RW,
                              &arg, sizeof(arg));
    if (ret) {
        ALOGW("%s: failed to dump display registers %d\n", __func__, ret);
        goto out;
    }

    dumpPrintf("  + INT_MASK_REG: 0x%x \n", arg.display.int_mask);

    // Read interrupt enable register
    memset(&arg, 0, sizeof(struct drm_psb_register_rw_arg));
    arg.display_read_mask = REGRWBITS_INT_ENABLE;

    ret = drmCommandWriteRead(mDrm->getDrmFd(), DRM_PSB_REGISTER_RW,
                              &arg, sizeof(arg));
    if (ret) {
        ALOGW("%s: failed to dump display registers %d\n", __func__, ret);
        goto out;
    }

    dumpPrintf("  + INT_ENABLE_REG: 0x%x \n", arg.display.int_enable);

    // open this if need to dump all display registers.
#if 1
    // dump all display regs in driver
    memset(&arg, 0, sizeof(struct drm_psb_register_rw_arg));
    arg.display_read_mask = REGRWBITS_DISPLAY_ALL;

    ret = drmCommandWriteRead(mDrm->getDrmFd(), DRM_PSB_REGISTER_RW,
                              &arg, sizeof(arg));
    if (ret) {
        ALOGW("%s: failed to dump display registers %d\n", __func__, ret);
        goto out;
    }
#endif

out:
    return (ret == 0) ? true : false;
}

bool IntelHWComposer::dump(char *buff,
                           int buff_len, int *cur_len)
{
    IntelDisplayPlane *plane = NULL;
    bool ret = true;
    int i;

    mDumpBuf = buff;
    mDumpBuflen = buff_len;
    mDumpLen = 0;

    dumpDisplayStat();

    for (size_t i=0 ; i<DISPLAY_NUM ; i++) {
        if (mDisplayDevice[i])
            mDisplayDevice[i]->dump(mDumpBuf,  mDumpBuflen, &mDumpLen);
    }

    mPlaneManager->dump(mDumpBuf,  mDumpBuflen, &mDumpLen);

    return ret;
}

bool IntelHWComposer::initialize()
{
    bool ret = true;

    //TODO: replace the hard code buffer type later
    int bufferType = IntelBufferManager::TTM_BUFFER;

    ALOGD_IF(ALLOW_HWC_PRINT, "%s\n", __func__);
    if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
            (const hw_module_t**)&mGrallocModule) != 0) {
        ALOGE("%s: failed to open IMG GRALLOC module\n", __func__);
        goto err;
    }

    //create new DRM object if not exists
    if (!mDrm) {
        mDrm = &IntelHWComposerDrm::getInstance();
        if (!mDrm) {
            ALOGE("%s: Invalid DRM object\n", __func__);
            goto err;
        }

        ret = mDrm->initialize(this);
        if (ret == false) {
            ALOGE("%s: failed to initialize DRM instance\n", __func__);
            goto drm_err;
        }
    }

    //create Vsync Event Handler
    mVsync = new IntelVsyncEventHandler(this, mDrm->getDrmFd());

    mFakeVsync = new IntelFakeVsyncEvent(this);

    //create new buffer manager and initialize it
    if (!mBufferManager) {
        //mBufferManager = new IntelTTMBufferManager(mDrm->getDrmFd());
        mBufferManager = new IntelBCDBufferManager(mDrm->getDrmFd());
        if (!mBufferManager) {
            ALOGE("%s: Failed to create buffer manager\n", __func__);
            goto drm_err;
        }
        // do initialization
        ret = mBufferManager->initialize();
        if (ret == false) {
            ALOGE("%s: Failed to initialize buffer manager\n", __func__);
            goto bm_err;
        }
    }

    // create buffer manager for gralloc buffer
    if (!mGrallocBufferManager) {
        //mGrallocBufferManager = new IntelPVRBufferManager(mDrm->getDrmFd());
        mGrallocBufferManager = new IntelGraphicBufferManager(mDrm->getDrmFd());
        if (!mGrallocBufferManager) {
            ALOGE("%s: Failed to create Gralloc buffer manager\n", __func__);
            goto bm_err;
        }

        ret = mGrallocBufferManager->initialize();
        if (ret == false) {
            ALOGE("%s: Failed to initialize Gralloc buffer manager\n", __func__);
            goto gralloc_bm_err;
        }
    }

    // create new display plane manager
    if (!mPlaneManager) {
        mPlaneManager =
            new IntelDisplayPlaneManager(mDrm->getDrmFd(),
                                         mBufferManager, mGrallocBufferManager);
        if (!mPlaneManager) {
            ALOGE("%s: Failed to create plane manager\n", __func__);
            goto gralloc_bm_err;
        }
    }

    // create display devices
    memset(mDisplayDevice, 0, sizeof(mDisplayDevice));
    for (size_t i=0; i<DISPLAY_NUM; i++) {
         if (i == HWC_DISPLAY_PRIMARY)
             mDisplayDevice[i] =
                 new IntelMIPIDisplayDevice(mBufferManager, mGrallocBufferManager,
                                       mPlaneManager, mDrm, &mExtendedModeInfo, i);
#ifdef TARGET_HAS_MULTIPLE_DISPLAY
         else if (i == HWC_DISPLAY_EXTERNAL)
             mDisplayDevice[i] =
                 new IntelHDMIDisplayDevice(mBufferManager, mGrallocBufferManager,
                                       mPlaneManager, mDrm, i);
#endif
#ifdef INTEL_WIDI
         else if (i == HWC_DISPLAY_VIRTUAL)
             mDisplayDevice[i] =
                 new WidiDisplayDevice(mBufferManager, mGrallocBufferManager,
                                       mPlaneManager, mDrm, &mExtendedModeInfo, i);
#endif
         else
             continue;

         if (!mDisplayDevice[i]) {
             ALOGE("%s: Failed to create plane manager\n", __func__);
             goto device_err;
         }
    }

    // init mHDMIBuffers
    memset(&mHDMIFBHandle, 0, sizeof(mHDMIFBHandle));
    memset(&mExtendedModeInfo, 0, sizeof(mExtendedModeInfo));

    // do mode setting in HWC if HDMI is connected when boot up
    if (mDrm->detectDisplayConnection(OUTPUT_HDMI))
        handleHotplugEvent(1, NULL);

    char value[PROPERTY_VALUE_MAX];
    property_get("hwcomposer.debug.dumpPost2", value, "0");
    if (atoi(value))
        mForceDumpPostBuffer = true;

    // startObserver();
    mInitialized = true;

    ALOGD_IF(ALLOW_HWC_PRINT, "%s: successfully\n", __func__);
    return true;

device_err:
    for (size_t i=0; i<DISPLAY_NUM; i++) {
        if (mDisplayDevice[i])
            delete mDisplayDevice[i];
        mDisplayDevice[i] = 0;
    }

pm_err:
    if (mPlaneManager)
        delete mPlaneManager;
    mPlaneManager = 0;

gralloc_bm_err:
    if (mGrallocBufferManager)
        delete mGrallocBufferManager;
    mGrallocBufferManager = 0;

bm_err:
    if (mBufferManager)
        delete mBufferManager;
    mBufferManager = 0;

drm_err:
    if (mDrm)
        delete mDrm;
    mDrm = 0;

err:
    return false;
}

IMG_native_handle_t *IntelHWComposer::findVideoHandle(hwc_display_contents_1_t* list)
{
    IMG_native_handle_t *foundHandle = NULL;

    if (list) {
        for (size_t i = 0; i < list->numHwLayers-1; i++) {
            hwc_layer_1_t& layer = list->hwLayers[i];
            if (layer.compositionType != HWC_BACKGROUND && layer.handle) {
                IMG_native_handle_t *grallocHandle = (IMG_native_handle_t*)layer.handle;
                if (grallocHandle->iFormat == HAL_PIXEL_FORMAT_INTEL_HWC_NV12 ||
                    grallocHandle->iFormat == HAL_PIXEL_FORMAT_INTEL_HWC_NV12_VED ||
                    grallocHandle->iFormat == HAL_PIXEL_FORMAT_INTEL_HWC_NV12_TILE)
                {
                    foundHandle = grallocHandle;
                }
            }
        }
    }

    return foundHandle;
}

bool IntelHWComposer::checkPresentationMode(hwc_display_contents_1_t* primary_list,
                                                hwc_display_contents_1_t* secondary_list)
{
    bool isPresentation = false;
    if (primary_list != NULL && secondary_list != NULL) {
        if (primary_list->numHwLayers != secondary_list->numHwLayers) {
            isPresentation = true;
        } else {
            for (size_t i = 0; i < primary_list->numHwLayers-1; i++) {
                hwc_layer_1_t& primary_layer = primary_list->hwLayers[i];
                hwc_layer_1_t& secondary_layer = secondary_list->hwLayers[i];
                if (primary_layer.handle != secondary_layer.handle) {
                    isPresentation = true;
                    break;
                }
            }
        }
    }
    if (mDrm)
        mDrm->setPresentationMode(isPresentation);
    return isPresentation;
}

bool IntelHWComposer::prepareDisplays(size_t numDisplays,
                                      hwc_display_contents_1_t** displays)
{
    android::Mutex::Autolock _l(mLock);

    mExtendedModeInfo.widiExtHandle = NULL;

#ifdef HWC_DEBUG_DUMP_LAYERS
    IntelUtility u(numDisplays, displays);
    u.dumpLayers(NULL);
#endif
    // Presentation mode checking for HDMI
    if (numDisplays > HWC_DISPLAY_EXTERNAL && displays[HWC_DISPLAY_EXTERNAL]) {
        checkPresentationMode(displays[HWC_DISPLAY_PRIMARY], displays[HWC_DISPLAY_EXTERNAL]);
        bool onlyHdmiHasVideo = false;
        // HDMI has video layer but primary device hasn't
        IMG_native_handle_t *videoHandleMipi = findVideoHandle(displays[HWC_DISPLAY_PRIMARY]);
        IMG_native_handle_t *videoHandleHdmi = findVideoHandle(displays[HWC_DISPLAY_EXTERNAL]);
        if (videoHandleMipi == NULL && videoHandleHdmi != NULL)
            onlyHdmiHasVideo = true;
        mDrm->setOnlyHdmiHasVideo(onlyHdmiHasVideo);
    }
    if (numDisplays >= HWC_DISPLAY_VIRTUAL && displays[HWC_DISPLAY_VIRTUAL])
    {
        IMG_native_handle_t *videoHandleMipi = findVideoHandle(displays[HWC_DISPLAY_PRIMARY]);
        IMG_native_handle_t *videoHandleWidi = findVideoHandle(displays[HWC_DISPLAY_VIRTUAL]);
        if (videoHandleMipi == videoHandleWidi)
            mExtendedModeInfo.widiExtHandle = videoHandleMipi;

        // call prepare for widi out of order since it may cancel extended mode
        if (mDisplayDevice[HWC_DISPLAY_VIRTUAL]) {
            mDisplayDevice[HWC_DISPLAY_VIRTUAL]->prepare(displays[HWC_DISPLAY_VIRTUAL]);
        }
    }

    for (size_t disp = 0; disp < numDisplays; disp++) {
        if (disp >= DISPLAY_NUM)
            break;

        hwc_display_contents_1_t *list = displays[disp];
        if (list && mDisplayDevice[disp] && disp != HWC_DISPLAY_VIRTUAL)
            mDisplayDevice[disp]->prepare(list);

        if (disp == HWC_DISPLAY_EXTERNAL && !list)
            signalHpdCompletion();
    }

    return true;
}

bool IntelHWComposer::commitDisplays(size_t numDisplays,
                                     hwc_display_contents_1_t** displays)
{
    if (!initCheck()) {
        ALOGE("%s: failed to initialize HWComposer\n", __func__);
        return false;
    }

    android::Mutex::Autolock _l(mLock);

    mPlaneManager->resetPlaneContexts();

    size_t disp;
    buffer_handle_t bufferHandles[INTEL_DISPLAY_PLANE_NUM];
    int acquireFenceFd[INTEL_DISPLAY_PLANE_NUM];
    int* releaseFenceFd[INTEL_DISPLAY_PLANE_NUM]={0};
    int i,j, numBuffers = 0;
    bool ret = true;
 
    for (disp = 0; disp < numDisplays && disp < DISPLAY_NUM; disp++) {
        hwc_display_contents_1_t *list = displays[disp];

        if (list && mDisplayDevice[disp]) {
            for (int i = 0; i < list->numHwLayers; i++) {
                list->hwLayers[i].releaseFenceFd = -1;
            }
            mDisplayDevice[disp]->commit(list, bufferHandles,
                acquireFenceFd, releaseFenceFd, numBuffers);
        }
     }

    void *context = mPlaneManager->getPlaneContexts();

    // commit plane contexts
    if (numBuffers) {
        ALOGD_IF(ALLOW_HWC_PRINT, "%s: commits %d buffers\n", __func__, numBuffers);
        int err = mGrallocModule->PostBuffers(mGrallocModule,
                                              bufferHandles,
                                              acquireFenceFd,
                                              releaseFenceFd,
                                              numBuffers,
                                              context,
                                              mPlaneManager->getContextLength());
        if (err) {
            ALOGE("%s: Post2 failed with errno %d\n", __func__, err);
            ret = false;
        }
    }

    for (disp = 0; disp < numDisplays && disp < DISPLAY_NUM; disp++) {
        hwc_display_contents_1_t *list = displays[disp];
        if (list) {
            for (i = 0; i < list->numHwLayers; i++) {
                if (list->hwLayers[i].releaseFenceFd ==
                    LAYER_SAME_RGB_BUFFER_SKIP_RELEASEFENCEFD){
                    for(j = 0; j < INTEL_DISPLAY_PLANE_NUM; j++){
                        if(!releaseFenceFd[j])
                            continue;

                        if(*releaseFenceFd[j] >= 0){
                            //every layer relase fence fd dup from a same fd
                            list->hwLayers[i].releaseFenceFd = dup(*releaseFenceFd[j]);
                            break;
                        }
                    }
                }
                if (list->hwLayers[i].acquireFenceFd >= 0)
                    close(list->hwLayers[i].acquireFenceFd);
                    list->hwLayers[i].acquireFenceFd = -1;
            }

            if (list->outbufAcquireFenceFd != -1) {
                close(list->outbufAcquireFenceFd);
                list->outbufAcquireFenceFd = -1;
            }
        }
    }

    if ( ret == false || mForceDumpPostBuffer) {
        dumpPost2Buffers(numBuffers, bufferHandles);
        dumpLayerLists(numDisplays, displays);
    }

    return ret;
}

bool IntelHWComposer::blankDisplay(int disp, int blank)
{
    if ((disp<DISPLAY_NUM) && mDisplayDevice[disp]) {
        mDisplayDevice[disp]->blank(blank);
        android::Mutex::Autolock _l(mLock);
        if (blank == 1)
            mDisplayDevice[disp]->release();
    }

    return true;
}

bool IntelHWComposer::getDisplayConfigs(int disp, uint32_t* configs, 
                                        size_t* numConfigs)
{
    if (disp >= DISPLAY_NUM || !mDisplayDevice[disp]) {
        ALOGW("%s: invalid disp num %d\n", __func__, disp);
        return false;
    }

    if (!mDisplayDevice[disp]->getDisplayConfig(configs, numConfigs)) {
        return false;
    }

    return true;
}

bool IntelHWComposer::getDisplayAttributes(int disp, uint32_t config,
            const uint32_t* attributes, int32_t* values)
{
    if (disp >= DISPLAY_NUM || !mDisplayDevice[disp]) {
        ALOGW("%s: invalid disp num %d\n", __func__, disp);
        return false;
    }

    if (!mDisplayDevice[disp]->getDisplayAttributes(config, attributes, values)) {
        return false;
    }

    return true;
}

bool IntelHWComposer::setFramecount(int cmd, int count, int x, int y)
{
   struct drm_psb_register_rw_arg arg;
   struct psb_gtt_mapping_arg gttarg;
   uint32_t fb_size = 0;
   uint8_t* pDatabuff = NULL;
   uint8_t w = 128;
   uint8_t h = 128;
   int ret = 0;
   void *virtAddr;
   uint32_t size;
   PVR2D_ULONG uFlags = 0;
   PVR2DERROR err;
   switch(cmd){
      case 0:
         if (mCursorBufferManager) {
           if (cursorDataBuffer) {
              ret = mCursorBufferManager->updateCursorReg(count, cursorDataBuffer, 0, 0, w, h, true);
              if (ret == false) {
                 ALOGE("%s: Failed to update Cursor content\n", __func__);
                 return false;
              }
           }
         }
         break;
      case 1:
         if (!mCursorBufferManager) {
            mCursorBufferManager = new IntelPVRBufferManager(mDrm->getDrmFd());
            if (!mCursorBufferManager) {
               ALOGE("%s: Failed to create Cursor buffer manager\n", __func__);
               ret = false;
               goto gralloc_bm_err;
            }
            ret = mCursorBufferManager->initialize();
            if (ret == false) {
               ALOGE("%s: Failed to initialize Cursor buffer manager\n", __func__);
               goto gralloc_bm_err;
            }
            cursorDataBuffer = mCursorBufferManager->curAlloc(w, h);
            if (!cursorDataBuffer) {
               ALOGE("%s: Failed to alloc Cursor buffer memory\n", __func__);
               ret = false;
               goto gralloc_bm_err;
            }
            ret = mCursorBufferManager->updateCursorReg(0, cursorDataBuffer, x, y, w, h, true);
            if (ret == false) {
               ALOGE("%s: Failed to update Cursor content\n", __func__);
               return false;
            }
         }
         break;
      case 2:
         if (mCursorBufferManager) {
           if (cursorDataBuffer) {
               ret = mCursorBufferManager->updateCursorReg(0, cursorDataBuffer, x, y, w, h, false);
               if (ret == false) {
                  ALOGE("%s: Failed to update Cursor content\n", __func__);
                  return false;
               }
           }
           mCursorBufferManager->curFree(cursorDataBuffer);
           mCursorBufferManager = 0;
           delete mCursorBufferManager;
         }
         break;
    }
    return ret;
    gralloc_bm_err:
       mCursorBufferManager = 0;
       delete mCursorBufferManager;
       return false;
}

int IntelHWComposer::dumpLayerLists(size_t numDisplays,
                                     hwc_display_contents_1_t** displays)
{
    int disp, i;
    for (disp = 0; disp < numDisplays; disp++)
    {
        if (displays[disp]) {
            ALOGD("%d hwc_layers in display %d", displays[disp]->numHwLayers, disp);
            for (i = 0; i < displays[disp]->numHwLayers; i++) {
                IMG_native_handle_t* handle =
                       (IMG_native_handle_t*)displays[disp]->hwLayers[i].handle;
		if (handle) {
                    ALOGD("handle=%p type=%d format=%x usage=%x stamp=%llu fd[0]=%d",
			    handle,
                            displays[disp]->hwLayers[i].compositionType,
			    handle->iFormat,
			    handle->usage,
			    handle->ui64Stamp,
			    handle->fd[0]
			    );
                }
            }
        }
    }
    return 0;
}

int IntelHWComposer::dumpPost2Buffers(int num, buffer_handle_t* buffer)
{
    int i;

    ALOGD("%d buffer handle in Post2", num);
    for (i = 0; i < num; i++) {
        IMG_native_handle_t* handle =
                (IMG_native_handle_t*)buffer[i];
        if (handle) {
            ALOGD("handle=%p format=%x usage=%x stamp=%d fd[0]=%d",
                handle,
                handle->iFormat,
                handle->usage,
                handle->ui64Stamp,
                handle->fd[0]
                );
        }
    }

    return 0;
}
