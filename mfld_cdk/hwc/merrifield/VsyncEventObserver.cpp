/*
 * Copyright © 2012 Intel Corporation
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
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

#include <Log.h>
#include <Drm.h>
#include <VsyncEventObserver.h>
#include <IDisplayDevice.h>

namespace android {
namespace intel {

static Log& log = Log::getInstance();

VsyncEventObserver::VsyncEventObserver(IDisplayDevice& disp)
    : mDisplayDevice(disp),
      mEnabled(0)
{
    log.v("VsyncEventObserver()");
}

VsyncEventObserver::~VsyncEventObserver()
{

}

void VsyncEventObserver::control(int enabled)
{
    log.v("control: enabled %s", enabled ? "True" : "False");

    Mutex::Autolock _l(mLock);
    mEnabled = enabled;
    mCondition.signal();
}

bool VsyncEventObserver::threadLoop()
{
    { // scope for lock
        Mutex::Autolock _l(mLock);
        while (!mEnabled) {
            mCondition.wait(mLock);
        }
    }

    struct drm_psb_vsync_set_arg arg;
    memset(&arg, 0, sizeof(struct drm_psb_vsync_set_arg));
    arg.vsync_operation_mask = VSYNC_WAIT;
    if (mDisplayDevice.getType() == IDisplayDevice::DEVICE_PRIMARY)
        arg.vsync.pipe = 0;
    else if (mDisplayDevice.getType() == IDisplayDevice::DEVICE_EXTERNAL)
        arg.vsync.pipe = 1;

    bool ret = Drm::getInstance().writeReadIoctl(DRM_PSB_VSYNC_SET,
                                                 &arg,
                                                 sizeof(arg));
    if (ret == false) {
        log.w("threadLoop: failed to wait for vsync, check vsync enabling...");
        return true;
    }

    // notify device
    mDisplayDevice.onVsync((int64_t)arg.vsync.timestamp);
    return true;
}

status_t VsyncEventObserver::readyToRun()
{
    log.v("VsyncEventObserver: readyToRun. disp %d", mDisplayDevice.getType());
    return NO_ERROR;
}

void VsyncEventObserver::onFirstRef()
{
    log.v("VsyncEventObserver: onFirstRef. disp %d", mDisplayDevice.getType());
    run("VsyncEventObserver", PRIORITY_URGENT_DISPLAY);
}

} // namespace intel
} // namesapce android
