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
#include <Log.h>
#include <MrflHwcomposer.h>
#include <MrflDisplayPlaneManager.h>
#include <MrflMipiDevice.h>
#include <MrflHdmiDevice.h>

namespace android {
namespace intel {

static Log& log = Log::getInstance();
MrflHwcomposer::MrflHwcomposer()
    : Hwcomposer()
{

}

MrflHwcomposer::~MrflHwcomposer()
{

}

// implement createDisplayPlaneManager()
DisplayPlaneManager* MrflHwcomposer::createDisplayPlaneManager()
{
    log.v("MrflHwcomposer::createDisplayPlaneManager");
    return (new MrflDisplayPlaneManager());
}

IDisplayDevice* MrflHwcomposer::createDisplayDevice(int disp,
                                                     DisplayPlaneManager& dpm)
{
    log.v("MrflHwcomposer::createDisplayDevice");

    switch (disp) {
    case IDisplayDevice::DEVICE_PRIMARY:
        return (new MrflMipiDevice(*this, dpm));
    case IDisplayDevice::DEVICE_EXTERNAL:
        return (new MrflHdmiDevice(*this, dpm));
    default:
        log.e("MrflHwcomposer::createDisplayDevice: unsupported device %d",
              disp);
        return 0;
    }
}

void* MrflHwcomposer::getContexts()
{
    log.v("MrflHwcomposer::getContexts");
    return (void *)mImgLayers;
}

bool MrflHwcomposer::commitContexts(void *contexts, int count)
{
    log.v("MrflHwcomposer::commitContexts: contexts = 0x%x, count = %d",
          contexts, count);

    // nothing need to be submitted
    if (!count)
        return true;

    if (!contexts) {
        log.e("MrflHwcomposer::commitContexts: invalid parameters");
        return false;
    }

    if (mFBDev) {
        int err = mFBDev->Post2(&mFBDev->base, mImgLayers, count);
        if (err) {
            log.e("MrflHwcomposer::commitContexts: Post2 failed err = %d", err);
            return false;
        }
    }

    return true;
}

} //namespace intel
} //namespace android
