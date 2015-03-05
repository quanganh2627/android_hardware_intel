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
//#define ALOG_NDEBUG 0
#include <IntelHWComposerCfg.h>
#include <binder/IServiceManager.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/queue.h>
#include <linux/netlink.h>
#include <pthread.h>

#include "IntelExternalDisplayMonitor.h"
#include "IntelHWComposer.h"

using namespace android;

#define UNKNOWN_MDS_MODE 0 // status of video/widi/HDMI/HDCP/MIPI is unknown initially, it can be set indirectly through onMdsMessage event or directly set by invoking mMDClient->getMode

IntelExternalDisplayMonitor::IntelExternalDisplayMonitor(IntelHWComposer *hwc) :
    mMDClient(NULL),
    mActiveDisplayMode(UNKNOWN_MDS_MODE),
    mVideoPrepareState(MDS_VIDEO_UNPREPARED),
    mWidiOn(false),
    mInitialized(false),
    mComposer(hwc),
    mLastMsg(MSG_TYPE_MDS_UNDEFINED)
{
    ALOGD_IF(ALLOW_MONITOR_PRINT, "External display monitor created");
    initialize();
}

IntelExternalDisplayMonitor::~IntelExternalDisplayMonitor()
{
    ALOGD_IF(ALLOW_MONITOR_PRINT, "External display monitor Destroyed");
    if (mMDClient) {
        mMDClient->unregisterListener();
        delete mMDClient;
        mMDClient = NULL;
    }
    mInitialized = false;
    mComposer = 0;
}

void IntelExternalDisplayMonitor::initialize()
{
    ALOGD_IF(ALLOW_MONITOR_PRINT, "External display monitor initialized");
    mInitialized = true;
}

int IntelExternalDisplayMonitor::onMdsMessage(int msg, void *data, int size)
{
    bool ret = true;

    ALOGD_IF(ALLOW_MONITOR_PRINT, "onMdsMessage: External display monitor onMdsMessage, %d size :%d ", msg, size);
    if (msg == MDS_MODE_CHANGE) {
        int mode = *((int*)data);

        if (checkMode(mActiveDisplayMode, MDS_HDMI_CONNECTED) &&
            !checkMode(mode, MDS_HDMI_CONNECTED)) {
            ALOGD_IF(ALLOW_MONITOR_PRINT, "%s: HDMI is plugged out %d", __func__, mode);
            mLastMsg = MSG_TYPE_MDS_HOTPLUG_OUT;
            ret = mComposer->onUEvent(mLastMsg, NULL, 0);
        }
    } else if (msg == MDS_SET_TIMING) {
        MDSHDMITiming* timing = (MDSHDMITiming*)data;
        intel_display_mode_t* mode = NULL;
        if (mLastMsg == MSG_TYPE_MDS_HOTPLUG_OUT) {
            ALOGD_IF(ALLOW_MONITOR_PRINT, "%s: HDMI is plugged in, should set HDMI timing", __func__);
            mLastMsg = MSG_TYPE_MDS_HOTPLUG_IN;
        } else {
            ALOGD_IF(ALLOW_MONITOR_PRINT, "%s: Setting HDMI timing", __func__);
            mLastMsg = MSG_TYPE_MDS_TIMING_DYNAMIC_SETTING;
        }

        ALOGD_IF(ALLOW_MONITOR_PRINT, "%s: Setting HDMI timing %dx%d@%dx%dx%d", __func__,
                timing->width, timing->height,
                timing->refresh, timing->ratio, timing->interlace);
        return mComposer->onUEvent(mLastMsg, data, size);
    } else if (msg == MDS_SET_VIDEO_STATUS) {
        mVideoPrepareState = *((int *)data);
    }

    if (msg == MDS_MODE_CHANGE) {
        mActiveDisplayMode = *(int *)data;
        ret = mComposer->onUEvent(MSG_TYPE_MDS, data, size);
    }
    return ret ? 0 : (-1);
}

int IntelExternalDisplayMonitor::getDisplayMode()
{
    int mode = OVERLAY_MIPI0;
    if (checkMode(mActiveDisplayMode, MDS_HDMI_ON)) {
        if (checkMode(mActiveDisplayMode, MDS_HDMI_VIDEO_EXT)) {
            if (!checkMode(mActiveDisplayMode, MDS_OVERLAY_OFF))
                mode = OVERLAY_EXTEND;
        } else if (checkMode(mActiveDisplayMode, MDS_HDMI_CLONE))
                mode = OVERLAY_CLONE_MIPI0;
    }
    ALOGD_IF(ALLOW_MONITOR_PRINT, "Get MultiDisplay mode %#x, %d", mActiveDisplayMode, mode);
    return mode;
}

bool IntelExternalDisplayMonitor::isVideoPlaying()
{
    if (checkMode(mActiveDisplayMode, MDS_VIDEO_PLAYING))
       return true;
    return false;
}

bool IntelExternalDisplayMonitor::isHdmiConnected()
{
    if (checkMode(mActiveDisplayMode, MDS_HDMI_CONNECTED))
       return true;
    return false;
}

bool IntelExternalDisplayMonitor::isVideoPrepared()
{
    if (mVideoPrepareState <= MDS_VIDEO_PREPARED) {
        return true;
    }

    return false;
}

bool IntelExternalDisplayMonitor::isOverlayOff()
{
    if (checkMode(mActiveDisplayMode, MDS_OVERLAY_OFF)) {
       ALOGD_IF(ALLOW_MONITOR_PRINT, "Overlay is off, %#x", mActiveDisplayMode);
       return true;
    }
    return false;
}

void IntelExternalDisplayMonitor::binderDied(const wp<IBinder>& who)
{
    ALOGD_IF(ALLOW_MONITOR_PRINT, "External display monitor binderDied");
}

bool IntelExternalDisplayMonitor::notifyWidi(bool on)
{
    ALOGD_IF(ALLOW_MONITOR_PRINT, "Exteranal display notify the MDS widi's state");
    // TODO: remove mWideOn. MultiDisplay Service maintains the state machine.
    if ((mMDClient != NULL) && (mWidiOn != on)) {
        mWidiOn = on;
        return mMDClient->notifyWidi(on);
    }
    return false;
}


bool IntelExternalDisplayMonitor::notifyMipi(bool on)
{
    ALOGD_IF(ALLOW_MONITOR_PRINT,
            "Exteranal display notify the MDS that Mipi should be turned on/off");
    if ((mMDClient != NULL) &&
        ((mActiveDisplayMode & MDS_HDMI_VIDEO_EXT) ||
         (mWidiOn))) {
        return mMDClient->notifyMipi(on);
    }
    return false;
}

bool IntelExternalDisplayMonitor::getVideoInfo(int *displayW, int *displayH, int *fps, int *isinterlace)
{
    if (mMDClient != NULL) {
        int ret = mMDClient->getVideoInfo(displayW, displayH, fps, isinterlace);
        return (ret == MDS_NO_ERROR);
    }
    return false;
}

bool IntelExternalDisplayMonitor::threadLoop()
{
    //ALOGD_IF(ALLOW_MONITOR_PRINT, "External display monitor thread loop");

    if (mMDClient !=  0) {
        ALOGD_IF(ALLOW_MONITOR_PRINT, "threadLoop: found MDS, threadLoop will exit.");
        requestExit();
        return true;
    }

    // if no MDS service available, fall back uevent
    struct pollfd fds;
    int nr;

    fds.fd = mUeventFd;
    fds.events = POLLIN;
    fds.revents = 0;
    nr = poll(&fds, 1, -1);

    if(nr > 0 && fds.revents == POLLIN) {
        int count = recv(mUeventFd, mUeventMessage, UEVENT_MSG_LEN - 2, 0);
        if (count > 0)
            mComposer->onUEvent(MSG_TYPE_UEVENT, mUeventMessage, UEVENT_MSG_LEN - 2);
    }

    return true;
}

status_t IntelExternalDisplayMonitor::readyToRun()
{
    ALOGD_IF(ALLOW_MONITOR_PRINT, "External display monitor ready to run");

    // get multi-display manager service, retry 10 seconds
    int retry = 10;
    do {
        sp<IBinder> service = NULL;
        sp<IServiceManager> serviceManager = defaultServiceManager();
        service = serviceManager->getService(String16(INTEL_MDS_SERVICE_NAME));
        if (service != 0) {
            mMDClient = new MultiDisplayClient();
            if (mMDClient == NULL) {
                ALOGE("Fail to create a multidisplay client");
            } else {
                ALOGD_IF(ALLOW_MONITOR_PRINT, "Create a MultiDisplay client at HWC");
                bool bWait = true;
                mActiveDisplayMode = mMDClient->getDisplayMode(bWait);
                mLastMsg = checkMode(mActiveDisplayMode, MDS_HDMI_CONNECTED) ?
                    MSG_TYPE_MDS_HOTPLUG_IN : MSG_TYPE_MDS_HOTPLUG_OUT;
                int mode = getDisplayMode();
                if (mode == OVERLAY_CLONE_MIPI0)
                    IntelHWComposerDrm::getInstance().setDisplayMode(OVERLAY_CLONE_MIPI0);
                else if (mode == OVERLAY_EXTEND) {
                    IntelHWComposerDrm::getInstance().setDisplayMode(OVERLAY_EXTEND);
                    ALOGE("Impossible be here. Only clone mode or single mode is valid initialized state.");
                } else
                    IntelHWComposerDrm::getInstance().setDisplayMode(OVERLAY_MIPI0);
            }
            service->linkToDeath(this);
            break;
        }
        ALOGW("Failed to get %s service.try again...\n", INTEL_MDS_SERVICE_NAME);
    } while(--retry);

    if (!retry && mMDClient == NULL) {
        ALOGW("Failed to get service %s, fall back uevent\n", INTEL_MDS_SERVICE_NAME);
        struct sockaddr_nl addr;
        int sz = 64*1024;

        memset(&addr, 0, sizeof(addr));
        addr.nl_family = AF_NETLINK;
        addr.nl_pid =  pthread_self() | getpid();
        addr.nl_groups = 0xffffffff;

        mUeventFd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
        if(mUeventFd < 0) {
            ALOGD("%s: failed to open uevent socket\n", __func__);
            return TIMED_OUT;
        }

        setsockopt(mUeventFd, SOL_SOCKET, SO_RCVBUFFORCE, &sz, sizeof(sz));

        if(bind(mUeventFd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
            close(mUeventFd);
            return TIMED_OUT;
        }

        memset(mUeventMessage, 0, UEVENT_MSG_LEN);
    } else {
        ALOGD_IF(ALLOW_MONITOR_PRINT, "Got MultiDisplay Service\n");
        if (mMDClient != NULL)
            mMDClient->registerListener(this, const_cast<char *>("HWComposer"),
                    MDS_MODE_CHANGE | MDS_SET_TIMING | MDS_SET_VIDEO_STATUS);
    }

    return NO_ERROR;
}

void IntelExternalDisplayMonitor::onFirstRef()
{
    ALOGD_IF(ALLOW_MONITOR_PRINT, "External display monitor onFirstRef");
    run("HWC external display monitor", PRIORITY_URGENT_DISPLAY);
}
