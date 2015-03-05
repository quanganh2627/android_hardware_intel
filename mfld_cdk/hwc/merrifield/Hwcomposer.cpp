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
#include <cutils/log.h>
#include <cutils/atomic.h>

#include <Hwcomposer.h>
#include <Dump.h>

namespace android {
namespace intel {

static Log& log = Log::getInstance();

Hwcomposer::Hwcomposer()
    : mProcs(0),
      mPlaneManager(0),
      mFBDev(0),
      mInitialized(false)
{
    log.d("Hwcomposer");

    mDisplayDevices.setCapacity(IDisplayDevice::DEVICE_COUNT);
}

Hwcomposer::~Hwcomposer()
{
    log.d("~Hwcomposer");
}

bool Hwcomposer::initCheck() const
{
    return mInitialized;
}

bool Hwcomposer::prepare(size_t numDisplays,
                          hwc_display_contents_1_t** displays)
{
    bool ret = true;

    //Mutex::Autolock _l(mLock);

    log.v("prepare display count %d\n", numDisplays);

    if (!initCheck())
        return false;

    if (!numDisplays || !displays) {
        log.e("prepare: invalid parameters");
        return false;
    }

    // disable reclaimed planes
    if (mPlaneManager)
        mPlaneManager->disableReclaimedPlanes();

    // reclaim all allocated planes if possible
    for (size_t i = 0; i < numDisplays; i++) {
        IDisplayDevice *device = mDisplayDevices.itemAt(i);
        if (!device) {
            log.v("prepare: device %d doesn't exist", i);
            continue;
        }

        if (!device->isConnected()) {
            log.v("prepare: device %d is disconnected", i);
            continue;
        }

        device->prePrepare(displays[i]);
    }

    for (size_t i = 0; i < numDisplays; i++) {
        IDisplayDevice *device = mDisplayDevices.itemAt(i);
        if (!device) {
            log.v("prepare: device %d doesn't exist", i);
            continue;
        }

        if (!device->isConnected()) {
            log.v("prepare: device %d is disconnected", i);
            continue;
        }

        ret = device->prepare(displays[i]);
        if (ret == false) {
            log.e("prepare: failed to do prepare for device %d", i);
            continue;
        }
    }

    return ret;
}

bool Hwcomposer::commit(size_t numDisplays,
                         hwc_display_contents_1_t **displays)
{
    bool ret = true;

    log.v("commit display count %d\n", numDisplays);

    //Mutex::Autolock _l(mLock);

    if (!initCheck())
        return false;

    if (!numDisplays || !displays) {
        log.e("commit: invalid parameters");
        return false;
    }

    void *hwContexts = getContexts();
    int count = 0;
    if (!hwContexts) {
        log.e("Hwcomposer::commit: invalid hwContexts");
        return false;
    }

    for (size_t i = 0; i < numDisplays; i++) {
        IDisplayDevice *device = mDisplayDevices.itemAt(i);
        if (!device) {
            log.v("commit: device %d doesn't exist", i);
            continue;
        }

        if (!device->isConnected()) {
            log.v("commit: device %d is disconnected", i);
            continue;
        }

        ret = device->commit(displays[i], hwContexts, count);
        if (ret == false) {
            log.e("commit: failed to do commit for device %d", i);
            continue;
        }
    }

    // commit hwContexts to hardware
    ret = commitContexts(hwContexts, count);
    if (ret == false) {
        log.e("Hwcomposer::commit: failed to commit hwContexts");
        return false;
    }

    return ret;
}

bool Hwcomposer::vsyncControl(int disp, int enabled)
{
    log.v("vsyncControl: disp %d, enabled %d", disp, enabled);

    if (!initCheck())
        return false;

    if (disp < 0 || disp >= IDisplayDevice::DEVICE_COUNT) {
        log.e("vsyncControl: invalid disp %d", disp);
        return false;
    }

    IDisplayDevice *device = mDisplayDevices.itemAt(disp);
    if (!device) {
        log.e("vsyncControl: no device found");
        return false;
    }

    return device->vsyncControl(enabled);
}

bool Hwcomposer::blank(int disp, int blank)
{
    log.v("blank: disp %d, blank %d", disp, blank);

    if (!initCheck())
        return false;

    if (disp < 0 || disp >= IDisplayDevice::DEVICE_COUNT) {
        log.e("blank: invalid disp %d", disp);
        return false;
    }

    IDisplayDevice *device = mDisplayDevices.itemAt(disp);
    if (!device) {
        log.e("blank: no device found");
        return false;
    }

    return device->blank(blank);
}

bool Hwcomposer::getDisplayConfigs(int disp,
                                      uint32_t *configs,
                                      size_t *numConfigs)
{
    log.v("getDisplayConfig");

    if (!initCheck())
        return false;

    if (disp < 0 || disp >= IDisplayDevice::DEVICE_COUNT) {
        log.e("getDisplayConfigs: invalid disp %d", disp);
        return false;
    }

    IDisplayDevice *device = mDisplayDevices.itemAt(disp);
    if (!device) {
        log.e("getDisplayConfigs: no device %d found", disp);
        return false;
    }

    return device->getDisplayConfigs(configs, numConfigs);
}

bool Hwcomposer::getDisplayAttributes(int disp,
                                         uint32_t config,
                                         const uint32_t *attributes,
                                         int32_t *values)
{
    log.v("getDisplayAttributes");

    if (!initCheck())
        return false;

    if (disp < 0 || disp >= IDisplayDevice::DEVICE_COUNT) {
        log.e("getDisplayAttributes: invalid disp %d", disp);
        return false;
    }

    IDisplayDevice *device = mDisplayDevices.itemAt(disp);
    if (!device) {
        log.e("getDisplayAttributes: no device found");
        return false;
    }

    return device->getDisplayAttributes(config, attributes, values);
}

bool Hwcomposer::compositionComplete(int disp)
{
    log.v("compositionComplete");

    if (!initCheck())
        return false;

    // complete fb device first
    if (mFBDev) {
        mFBDev->base.compositionComplete(&mFBDev->base);
    }

    if (disp < 0 || disp >= IDisplayDevice::DEVICE_COUNT) {
        log.e("compositionComplete: invalid disp %d", disp);
        return false;
    }

    IDisplayDevice *device = mDisplayDevices.itemAt(disp);
    if (!device) {
        log.e("compositionComplete: no device found");
        return false;
    }

    return device->compositionComplete();
}

void Hwcomposer::vsync(int disp, int64_t timestamp)
{
    //Mutex::Autolock _l(mLock);

    if (!initCheck())
        return;

    if (mProcs && mProcs->vsync) {
        log.v("report vsync disp %d timestamp %llu", disp, timestamp);
        mProcs->vsync(const_cast<hwc_procs_t*>(mProcs), disp, timestamp);
    }
}

void Hwcomposer::hotplug(int disp, int connected)
{
    //Mutex::Autolock _l(mLock);

    if (!initCheck())
        return;

    if (mProcs && mProcs->hotplug) {
        log.v("report hotplug disp %d connected %d", disp, connected);
        mProcs->hotplug(const_cast<hwc_procs_t*>(mProcs), disp, connected);
    }
}

bool Hwcomposer::release()
{
    log.d("release");

    if (!initCheck())
        return false;

    return true;
}

bool Hwcomposer::dump(char *buff, int buff_len, int *cur_len)
{
    log.d("dump");

    if (!initCheck())
        return false;

    Dump d(buff, buff_len);

    // dump composer status
    d.append("Intel Hardware Composer state:\n");
    // dump device status
    for (size_t i= 0; i < mDisplayDevices.size(); i++) {
        IDisplayDevice *device = mDisplayDevices.itemAt(i);
        if (device)
            device->dump(d);
    }

    // dump plane manager status
    if (mPlaneManager)
        mPlaneManager->dump(d);

    return true;
}

void Hwcomposer::registerProcs(hwc_procs_t const *procs)
{
    log.d("registerProcs");

    if (!procs)
        log.w("registerProcs: procs is NULL");

    mProcs = procs;
}

bool Hwcomposer::initialize()
{
    int err;
    log.d("initialize");

    // open frame buffer device
    hw_module_t const* module;
    err = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module);
    if (err) {
        log.e("Hwcomposer::initialize: failed to load gralloc module, %d", err);
        return false;
    }

    IMG_gralloc_module_public_t *imgGrallocModule;
    imgGrallocModule = (IMG_gralloc_module_public_t*)module;

    // open frame buffer device
    err = framebuffer_open(module, (framebuffer_device_t**)&mFBDev);
    if (err) {
        log.e("Hwcomposer::initialize: failed to open frame buffer device, %d",
              err);
        return false;
    }

    mFBDev->bBypassPost = 1;

    // create display plane manager
    mPlaneManager = createDisplayPlaneManager();
    if (!mPlaneManager || !mPlaneManager->initialize()) {
        log.e("initialize: failed to create display plane manager");
        goto dpm_create_err;
    }

    // create display device
    for (int i = 0; i < IDisplayDevice::DEVICE_COUNT; i++) {
        IDisplayDevice *device = createDisplayDevice(i, *mPlaneManager);
        if (!device || !device->initialize()) {
            log.e("initialize: failed to create device %d", i);
            continue;
        }
        // add this device
        mDisplayDevices.insertAt(device, i, 1);
    }

    mInitialized = true;
    return true;
device_create_err:
    for (size_t i = 0; i < mDisplayDevices.size(); i++) {
        IDisplayDevice *device = mDisplayDevices.itemAt(i);
        delete device;
    }
dpm_create_err:
    framebuffer_close((framebuffer_device_t*)mFBDev);
    mInitialized = false;
    return false;
}

} // namespace intel
} // namespace android
