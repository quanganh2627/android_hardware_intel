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
#include <Drm.h>
#include <Hwcomposer.h>
#include <IDisplayDevice.h>
#include <Log.h>

namespace android {
namespace intel {

static Log& log = Log::getInstance();

DisplayDevice::DisplayDevice(uint32_t type, Hwcomposer& hwc, DisplayPlaneManager& dpm)
    : mType(type),
      mHwc(hwc),
      mDisplayPlaneManager(dpm),
      mActiveDisplayConfig(-1),
      mVsyncObserver(0),
      mLayerList(0),
      mPrimaryPlane(0),
      mConnection(DEVICE_DISCONNECTED),
      mInitialized(false)
{
    log.v("DisplayDevice()");

    switch (type) {
    case DEVICE_PRIMARY:
        mName = "Primary";
        break;
    case DEVICE_EXTERNAL:
        mName = "External";
        break;
    case DEVICE_VIRTUAL:
        mName = "Virtual";
        break;
    default:
        mName = "Unknown";
    }

    mDisplayConfigs.setCapacity(DEVICE_COUNT);
}

DisplayDevice::~DisplayDevice()
{
    log.v("~DisplayDevice()");
}

void DisplayDevice::onGeometryChanged(hwc_display_contents_1_t *list)
{
    log.v("DisplayDevice::onGeometryChanged, disp %d", mType);
    // create a new layer list
    mLayerList = new HwcLayerList(list,
                                  mDisplayPlaneManager,
                                  mPrimaryPlane,
                                  mType);
    if (!mLayerList)
        log.w("onGeometryChanged: failed to create layer list");
}

void DisplayDevice::prePrepare(hwc_display_contents_1_t *display)
{
    log.v("DisplayDevice::prepare");

    if (!initCheck())
        return;

    Mutex::Autolock _l(mLock);

    if (mConnection != DEVICE_CONNECTED)
        return;

    // for a null list, delete hwc list
    if (!display) {
        delete mLayerList;
        mLayerList = 0;
        return;
    }

    // check if geometry is changed, if changed delete list
    if ((display->flags & HWC_GEOMETRY_CHANGED) && mLayerList) {
        delete mLayerList;
        mLayerList = 0;
    }
}

bool DisplayDevice::prepare(hwc_display_contents_1_t *display)
{

    log.v("DisplayDevice::prepare");

    if (!initCheck())
        return false;

    Mutex::Autolock _l(mLock);

    if (mConnection != DEVICE_CONNECTED)
        return true;

    if (!display)
        return true;

    // check if geometry is changed
    if (display->flags & HWC_GEOMETRY_CHANGED)
        onGeometryChanged(display);

    if (!mLayerList) {
        log.e("prepare: null HWC layer list");
        return false;
    }

    // update list with new list
    return mLayerList->update(display);
}

bool DisplayDevice::commit(hwc_display_contents_1_t *display,
                            void *context,
                            int& count)
{
    log.v("commit");
    return true;
}

bool DisplayDevice::vsyncControl(int enabled)
{
    struct drm_psb_vsync_set_arg arg;
    Drm& drm(Drm::getInstance());
    bool ret;

    log.v("vsyncControl");

    if (!initCheck())
        return false;

    //Mutex::Autolock _l(mLock);

    if (mConnection != DEVICE_CONNECTED)
        return false;

    memset(&arg, 0, sizeof(struct drm_psb_vsync_set_arg));
    arg.vsync.pipe = mType;
    if (enabled)
        arg.vsync_operation_mask = VSYNC_ENABLE;
    else
        arg.vsync_operation_mask = VSYNC_DISABLE;

    log.v("DisplayDevice::vsyncControl: disp %d, enabled %d", mType, enabled);

    ret = drm.writeReadIoctl(DRM_PSB_VSYNC_SET, &arg, sizeof(arg));
    if (ret == false) {
        log.e("DisplayDevice::vsyncControl: failed set vsync");
        return false;
    }

    mVsyncObserver->control(enabled);
    return true;
}

bool DisplayDevice::blank(int blank)
{
    log.v("blank");

    if (!initCheck())
        return false;

    //Mutex::Autolock _l(mLock);

    if (mConnection != DEVICE_CONNECTED)
        return false;

    return true;
}

bool DisplayDevice::getDisplayConfigs(uint32_t *configs,
                                         size_t *numConfigs)
{
    log.v("getDisplayConfigs");

    if (!initCheck())
        return false;

    //Mutex::Autolock _l(mLock);

    if (mConnection != DEVICE_CONNECTED)
        return false;

    if (!configs || !numConfigs) {
        log.e("getDisplayConfigs: invalid parameters");
        return false;
    }

    *configs = 0;
    *numConfigs = mDisplayConfigs.size();

    return true;
}

bool DisplayDevice::getDisplayAttributes(uint32_t configs,
                                            const uint32_t *attributes,
                                            int32_t *values)
{
    log.v("getDisplayAttributes");

    if (!initCheck())
        return false;

    //Mutex::Autolock _l(mLock);

    if (mConnection != DEVICE_CONNECTED)
        return false;

    if (!attributes || !values) {
        log.e("getDisplayAttributes: invalid parameters");
        return false;
    }

    DisplayConfig *config = mDisplayConfigs.itemAt(mActiveDisplayConfig);
    if  (!config) {
        log.e("getDisplayAttributes: failed to get display config");
        return false;
    }

    int i = 0;
    while (attributes[i] != HWC_DISPLAY_NO_ATTRIBUTE) {
        switch (attributes[i]) {
        case HWC_DISPLAY_VSYNC_PERIOD:
            values[i] = 1e9 / config->getRefreshRate();
            break;
        case HWC_DISPLAY_WIDTH:
            values[i] = config->getWidth();
            break;
        case HWC_DISPLAY_HEIGHT:
            values[i] = config->getHeight();
            break;
        case HWC_DISPLAY_DPI_X:
            values[i] = config->getDpiX();
            break;
        case HWC_DISPLAY_DPI_Y:
            values[i] = config->getDpiY();
            break;
        default:
            log.e("getDisplayAttributes: unknown attribute %d", attributes[i]);
            break;
        }
        i++;
    }

    return true;
}

bool DisplayDevice::compositionComplete()
{
    log.v("compositionComplete");

    if (!initCheck())
        return false;

    //Mutex::Autolock _l(mLock);

    if (mConnection != DEVICE_CONNECTED)
        return true;

    return true;
}

void DisplayDevice::removeDisplayConfigs()
{
    for (size_t i = 0; i < mDisplayConfigs.size(); i++) {
        DisplayConfig *config = mDisplayConfigs.itemAt(i);
        delete config;
    }

    mDisplayConfigs.clear();
    mActiveDisplayConfig = -1;
}

bool DisplayDevice::updateDisplayConfigs(struct Output *output)
{
    drmModeConnectorPtr drmConnector;
    drmModeCrtcPtr drmCrtc;
    drmModeModeInfoPtr drmMode;
    drmModeModeInfoPtr drmPreferredMode;
    drmModeFBPtr drmFb;
    int drmModeCount;
    float physWidthInch;
    float physHeightInch;
    int dpiX, dpiY;

    log.d("updateDisplayConfigs()");

    Mutex::Autolock _l(mLock);

    drmConnector = output->connector;
    if (!drmConnector) {
        log.e("DisplayDevice::updateDisplayConfigs:Output has no connector");
        return false;
    }

    physWidthInch = (float)drmConnector->mmWidth * 0.039370f;
    physHeightInch = (float)drmConnector->mmHeight * 0.039370f;

    drmModeCount = drmConnector->count_modes;
    drmPreferredMode = 0;

    // reset display configs
    removeDisplayConfigs();

    // reset the number of display configs
    mDisplayConfigs.setCapacity(drmModeCount + 1);

    log.v("updateDisplayConfigs: mode count %d", drmModeCount);

    // find preferred mode of this display device
    for (int i = 0; i < drmModeCount; i++) {
        drmMode = &drmConnector->modes[i];
        dpiX = drmMode->hdisplay / physWidthInch;
        dpiY = drmMode->vdisplay / physHeightInch;

        if ((drmMode->type & DRM_MODE_TYPE_PREFERRED)) {
            drmPreferredMode = drmMode;
            continue;
        }

        log.v("updateDisplayConfigs: adding new config %dx%d %d\n",
              drmMode->hdisplay,
              drmMode->vdisplay,
              drmMode->vrefresh);

        // if not preferred mode add it to display configs
        DisplayConfig *config = new DisplayConfig(drmMode->vrefresh,
                                                  drmMode->hdisplay,
                                                  drmMode->vdisplay,
                                                  dpiX, dpiY);
        mDisplayConfigs.add(config);
    }

    // update device connection status
    mConnection = (output->connected) ? DEVICE_CONNECTED : DEVICE_DISCONNECTED;

    // if device is connected, continue checking current mode
    if (mConnection != DEVICE_CONNECTED)
        goto use_preferred_mode;
    else if (mConnection == DEVICE_CONNECTED) {
        // use active fb and mode
        drmCrtc = output->crtc;
        drmFb = output->fb;
        if (!drmCrtc || !drmFb) {
            log.e("updateDisplayConfigs: impossible");
            goto use_preferred_mode;
        }

        drmMode = &drmCrtc->mode;
        if (!drmCrtc->mode_valid)
            goto use_preferred_mode;

        log.v("updateDisplayConfigs: using current mode %dx%d %d\n",
              drmMode->hdisplay,
              drmMode->vdisplay,
              drmMode->vrefresh);

        // use current drm mode, likely it's preferred mode
        dpiX = drmMode->hdisplay / physWidthInch;
        dpiY = drmMode->vdisplay / physHeightInch;
        // use active fb dimension as config width/height
        DisplayConfig *config = new DisplayConfig(drmMode->vrefresh,
                                                  //drmFb->width,
                                                  //drmFb->height,
                                                  drmMode->hdisplay,
                                                  drmMode->vdisplay,
                                                  dpiX, dpiY);
        // add it to the front of other configs
        mDisplayConfigs.push_front(config);
    }

    // init the active display config
    mActiveDisplayConfig = 0;

    return true;
use_preferred_mode:
    if (drmPreferredMode) {
        dpiX = drmPreferredMode->hdisplay / physWidthInch;
        dpiY = drmPreferredMode->vdisplay / physHeightInch;
        DisplayConfig *config = new DisplayConfig(drmPreferredMode->vrefresh,
                                                  drmPreferredMode->hdisplay,
                                                  drmPreferredMode->vdisplay,
                                                  dpiX, dpiY);
        if (!config) {
            log.e("updateDisplayConfigs: failed to allocate display config");
            return false;
        }

        log.v("updateDisplayConfigs: using preferred mode %dx%d %d\n",
              drmPreferredMode->hdisplay,
              drmPreferredMode->vdisplay,
              drmPreferredMode->vrefresh);

        // add it to the front of other configs
        mDisplayConfigs.push_front(config);
    }

    // init the active display config
    mActiveDisplayConfig = 0;
    return true;
}

bool DisplayDevice::detectDisplayConfigs()
{
    int outputIndex = -1;
    struct Output *output;
    bool ret;
    Drm& drm(Drm::getInstance());

    log.d("detectDisplayConfigs");

    // detect drm objects
    switch (mType) {
    case DEVICE_PRIMARY:
        outputIndex = Drm::OUTPUT_MIPI0;
        break;
    case DEVICE_EXTERNAL:
        outputIndex = Drm::OUTPUT_HDMI;
        break;
    case DEVICE_VIRTUAL:
        return true;
    }

    if (outputIndex < 0) {
        log.w("detectDisplayConfigs(): failed to detect Drm objects");
        return false;
    }

    // detect
    ret = drm.detect();
    if (ret == false) {
        log.e("detectDisplayConfigs(): Drm detection failed");
        return false;
    }

    // get output
    output = drm.getOutput(outputIndex);
    if (!output) {
        log.e("detectDisplayConfigs(): failed to get output");
        return false;
    }


    // update display configs
    return updateDisplayConfigs(output);
}

bool DisplayDevice::initialize()
{
    bool ret;

    log.v("DisplayDevice::initialize");

    // detect display configs
    ret = detectDisplayConfigs();
    if (ret == false) {
        log.e("initialize(): failed to detect display config");
        return false;
    }

    // get primary plane of this device
    mPrimaryPlane = mDisplayPlaneManager.getPrimaryPlane(mType);
    if (!mPrimaryPlane) {
        log.e("initialize(): failed to get primary plane");
        goto prim_err;
    }

    // create vsync event observer
    mVsyncObserver = new VsyncEventObserver(*this);
    mInitialized = true;

    return true;
prim_err:
    removeDisplayConfigs();
    mInitialized = false;
    return false;
}

bool DisplayDevice::isConnected() const
{
    if (!initCheck())
        return false;

    return (mConnection == DEVICE_CONNECTED) ? true : false;
}

const char* DisplayDevice::getName() const
{
    return mName;
}

int DisplayDevice::getType() const
{
    return mType;
}

void DisplayDevice::onHotplug(int connected)
{
    log.v("DisplayDevice::onHotplug");
}

void DisplayDevice::onVsync(int64_t timestamp)
{
    log.v("DisplayDevice::timestamp");

    if (!initCheck())
        return;

    //Mutex::Autolock _l(mLock);

    if (mConnection != DEVICE_CONNECTED)
        return;

    // notify hwc
    mHwc.vsync(mType, timestamp);
}

void DisplayDevice::dump(Dump& d)
{
    d.append("-------------------------------------------------------------\n");
    d.append("Device Name: %s (%s)\n", mName,
            mConnection ? "connected" : "disconnected");
    d.append("Display configs (count = %d):\n", mDisplayConfigs.size());
    d.append(" CONFIG | VSYNC_PERIOD | WIDTH | HEIGHT | DPI_X | DPI_Y \n");
    d.append("--------+--------------+-------+--------+-------+-------\n");
    for (size_t i = 0; i < mDisplayConfigs.size(); i++) {
        DisplayConfig *config = mDisplayConfigs.itemAt(i);
        if (config) {
            d.append("%s %2d   |     %4d     | %5d |  %4d  |  %3d  |  %3d  \n",
                     (i == (size_t)mActiveDisplayConfig) ? "* " : "  ",
                     i,
                     config->getRefreshRate(),
                     config->getWidth(),
                     config->getHeight(),
                     config->getDpiX(),
                     config->getDpiY());
        }
    }
    // dump layer list
    if (mLayerList)
        mLayerList->dump(d);
}

} // namespace intel
} // namespace android
