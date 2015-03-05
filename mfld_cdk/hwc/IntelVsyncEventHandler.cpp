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
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/queue.h>
#include <linux/netlink.h>
#include "IntelHWComposer.h"
#include "IntelVsyncEventHandler.h"

IntelVsyncEventHandler::IntelVsyncEventHandler(IntelHWComposer *hwc, int fd) :
    mComposer(hwc), mDrmFd(fd), mNextFakeVSync(0), mActiveVsyncs(0)
{
    ALOGV("Vsync Event Handler created");
    mRefreshPeriod = nsecs_t(1e9 / 60);
}

IntelVsyncEventHandler::~IntelVsyncEventHandler()
{

}

void IntelVsyncEventHandler::handleVsyncEvent(const char *msg, int msgLen)
{
    uint64_t ts = 0;
    int pipe = 0;
    const char *vsync_msg, *pipe_msg;

    if (strcmp(msg, "change@/devices/pci0000:00/0000:00:02.0/drm/card0"))
        return;
    msg += strlen(msg) + 1;

    vsync_msg = pipe_msg = 0;

    do {
        if (!strncmp(msg, "VSYNC=", strlen("VSYNC=")))
            vsync_msg = msg;
        else if (!strncmp(msg, "PIPE=", strlen("PIPE=")))
            pipe_msg = msg;
        if (vsync_msg && pipe_msg)
            break;
        msg += strlen(msg) + 1;
    } while (*msg);

    // report vsync event
    if (vsync_msg && pipe_msg) {
        ts = strtoull(vsync_msg + strlen("VSYNC="), NULL, 0);
        pipe = strtol(pipe_msg + strlen("PIPE="), NULL, 0);

        ALOGV("%s: vsync timestamp %lld, pipe %d\n", __func__, ts, pipe);
        mComposer->vsync(ts, pipe);
    }
}

void IntelVsyncEventHandler::setActiveVsyncs(uint32_t activeVsyncs)
{
    android::Mutex::Autolock _l(mLock);
    mActiveVsyncs = activeVsyncs;
    mCondition.signal();
}

bool IntelVsyncEventHandler::threadLoop()
{
    struct drm_psb_vsync_set_arg arg;
    int i = 0;
    int ret = 0;

#define HW_VSYNC_MASK ((1 << VSYNC_SRC_HDMI) | (1 << VSYNC_SRC_MIPI))
    { // scope for lock
        android::Mutex::Autolock _l(mLock);
        while (!(mActiveVsyncs & HW_VSYNC_MASK)) {
            mCondition.wait(mLock);
        }
    }

    memset(&arg, 0, sizeof(struct drm_psb_vsync_set_arg));
    arg.vsync_operation_mask = VSYNC_WAIT;

    for (i = 0; i <= VSYNC_SRC_HDMI; i++) {
	    if ((1 << i) & mActiveVsyncs) {
		    // pipe select
		    if (i == VSYNC_SRC_HDMI)
			    arg.vsync.pipe = 1;
		    else
			    arg.vsync.pipe = 0;

		    ret = drmCommandWriteRead(mDrmFd, DRM_PSB_VSYNC_SET, &arg, sizeof(arg));
		    if (ret) {
			    ALOGW("%s: failed to wait vsync, error = %d\n", __func__, ret);
			    return false;
		    }

		    mComposer->vsync((uint64_t)arg.vsync.timestamp, arg.vsync.pipe);
	    }
    }

    return true;
}

android::status_t IntelVsyncEventHandler::readyToRun()
{
    return android::NO_ERROR;
}

void IntelVsyncEventHandler::onFirstRef()
{
    ALOGV("Vsync Event Handler onFirstRef");
    run("HWC Vsync Event Handler", android::PRIORITY_URGENT_DISPLAY);
}
