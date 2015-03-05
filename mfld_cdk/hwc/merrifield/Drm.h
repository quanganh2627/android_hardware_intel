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
#ifndef __DRM_H__
#define __DRM_H__

#include <utils/Singleton.h>
#include <psb_drm.h>

extern "C" {
#include "xf86drm.h"
#include "xf86drmMode.h"
}

namespace android {
namespace intel {

struct Output {
    drmModeConnectorPtr connector;
    drmModeEncoderPtr encoder;
    drmModeCrtcPtr crtc;
    drmModeFBPtr fb;
    int connected;
};

class Drm : public android::Singleton<Drm> {
public:
    // DRM object index
    enum {
        OUTPUT_MIPI0 = 0,
        OUTPUT_HDMI,
        OUTPUT_MIPI1,
        OUTPUT_MAX,
    };
public:
    Drm();
public:
    // mode setting
    bool detect();
    bool setMode(int output, drmModeModeInfoPtr mode);

    bool writeReadIoctl(unsigned long cmd, void *data,
		                  unsigned long size);
    bool writeIoctl(unsigned long cmd, void *data,
                      unsigned long size);

    struct Output* getOutput(int output);
    bool outputConnected(int output);
    int getDrmFd() const;
private:
    int mDrmFd;
    struct Output mOutputs[OUTPUT_MAX];
    Mutex mLock;
};

} // namespace intel
} // namespace android



#endif /* __DRM_H__ */
