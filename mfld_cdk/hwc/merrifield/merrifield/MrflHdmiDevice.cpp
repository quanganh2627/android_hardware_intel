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
#include <MrflHdmiDevice.h>
#include <Hwcomposer.h>

namespace android {
namespace intel {

static Log& log = Log::getInstance();
MrflHdmiDevice::MrflHdmiDevice(Hwcomposer& hwc, DisplayPlaneManager& dpm)
    : DisplayDevice(DEVICE_EXTERNAL, hwc, dpm),
      mHotplugObserver(0)
{
    log.v("MrflHdmiDevice()");
}

MrflHdmiDevice::~MrflHdmiDevice()
{
    log.v("~MrflHdmiDevice");
}

bool MrflHdmiDevice::initialize()
{
    log.d("MrflHdmiDevice::initialize");

    // create hotplug observer
    mHotplugObserver = new HotplugEventObserver(*this);
    return DisplayDevice::initialize();
}

bool MrflHdmiDevice::commit(hwc_display_contents_1_t *display,
                             void *contexts,
                             int& count)
{
    bool ret;

    log.v("MrflHdmiDevice::commit");

    if (!display || !contexts) {
        log.e("MrflHdmiDevice::commit: invalid parameters");
        return false;
    }

    Mutex::Autolock _l(mLock);

    IMG_hwc_layer_t *imgLayerList = (IMG_hwc_layer_t*)contexts;

    for (size_t i = 0; i < display->numHwLayers; i++) {
        IDisplayPlane* plane = mLayerList->getPlane(i);
        if (!plane)
            continue;

        ret = plane->flip();
        if (ret == false) {
            log.w("MrflHdmiDevice::commit: failed to flip plane %d", i);
            continue;
        }

        IMG_hwc_layer_t *imgLayer = &imgLayerList[count++];
        // update IMG layer
        imgLayer->handle = display->hwLayers[i].handle;
        imgLayer->transform = display->hwLayers[i].transform;
        imgLayer->blending = display->hwLayers[i].blending;
        imgLayer->sourceCrop = display->hwLayers[i].sourceCrop;
        imgLayer->displayFrame = display->hwLayers[i].displayFrame;
        imgLayer->custom = (uint32_t)plane->getContext();
    }
    return true;
}

void MrflHdmiDevice::onHotplug(int connected)
{
    bool ret;

    log.d("MrflHdmiDevice::onHotplug: connected %d", connected);

    //Mutex::Autolock _l(mLock);

    // detect display configs
    ret = detectDisplayConfigs();
    if (ret == false) {
        log.d("MrflHdmiDevice::onHotplug: failed to detect display config");
        return;
    }

    {   // lock scope
        Mutex::Autolock _l(mLock);
        // delete device layer list
        if (!mConnection && mLayerList){
            delete mLayerList;
            mLayerList = 0;
        }
    }
    // notify hwcomposer
    mHwc.hotplug(mType, mConnection);
}

} // namespace intel
} // namespace android


