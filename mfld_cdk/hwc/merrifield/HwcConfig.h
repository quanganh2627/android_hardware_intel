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
#ifndef HWCCONFIG_H_
#define HWCCONFIG_H_

#include <utils/Singleton.h>

namespace android {
namespace intel {

// change the default configuration here
#define HWC_CONFIG_DEFAULT_LOG_LEVEL    "2"
#define HWC_CONFIG_DEFAULT_VIDEO_MODE_SUPPORT "1"

#define HWC_CONFIG_DEBUG_LOG_LEVEL        "hwc.debug.log.level"
#define HWC_CONFIG_FEATURE_EXTEND_VIDEO   "hwc.feature.extend.video"

class HwcConfig : public Singleton<HwcConfig> {
public:
    // debug log levels
    enum {
        DEBUG_LOG_VERBOSE = 0,
        DEBUG_LOG_DEBUG,
        DEBUG_LOG_INFO,
        DEBUG_LOG_WARNING,
        DEBUG_LOG_ERROR,
    };

    // feature
    enum {
        UNSUPPORTED = 0,
        SUPPORTED,
    };
public:
    HwcConfig();
public:
    void debugLogLevel(int& value);
    void extendVideo(int& value);
};

} // namespace intel
} // namespace android


#endif /* HWCCONFIG_H_ */
