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
#ifndef __INTEL_HWCOMPOSER_CPP__
#define __INTEL_HWCOMPOSER_CPP__

#include <EGL/egl.h>
#include <hardware/hwcomposer.h>
#include <utils/Vector.h>

#include <Log.h>
#include <IDisplayDevice.h>

namespace android {
namespace intel {

class Hwcomposer : public hwc_composer_device_1_t {
public:
    Hwcomposer();
    virtual ~Hwcomposer();

    // callbacks implementation
    virtual bool prepare(size_t numDisplays,
                           hwc_display_contents_1_t** displays);
    virtual bool commit(size_t numDisplays,
                           hwc_display_contents_1_t** displays);
    virtual bool vsyncControl(int disp, int enabled);
    virtual bool release();
    virtual bool dump(char *buff, int buff_len, int *cur_len);
    virtual void registerProcs(hwc_procs_t const *procs);

    virtual bool blank(int disp, int blank);
    virtual bool getDisplayConfigs(int disp,
                                       uint32_t *configs,
                                       size_t *numConfigs);
    virtual bool getDisplayAttributes(int disp,
                                          uint32_t config,
                                          const uint32_t *attributes,
                                          int32_t *values);
    virtual bool compositionComplete(int disp);

    // callbacks
    virtual void vsync(int disp, int64_t timestamp);
    virtual void hotplug(int disp, int connected);

    virtual bool initCheck() const;
    virtual bool initialize();
protected:
    virtual DisplayPlaneManager* createDisplayPlaneManager() = 0;
    virtual IDisplayDevice* createDisplayDevice(int disp,
                                                 DisplayPlaneManager& dpm) = 0;
    virtual void* getContexts() = 0;
    virtual bool commitContexts(void *context, int count) = 0;
protected:
    hwc_procs_t const *mProcs;
    DisplayPlaneManager *mPlaneManager;
    Vector<IDisplayDevice*> mDisplayDevices;
    IMG_framebuffer_device_public_t *mFBDev;
    Mutex mLock;
    bool mInitialized;
};

} // namespace intel
}

#endif /*__INTEL_HWCOMPOSER_CPP__*/
