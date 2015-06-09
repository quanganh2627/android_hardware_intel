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
#include <hardware/hardware.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <cutils/log.h>
#include <cutils/atomic.h>

#include <MrflHwcomposer.h>
#include <Log.h>

namespace android {
namespace intel {

static Log& log = Log().getInstance();

static int hwc_prepare(struct hwc_composer_device_1 *dev,
                          size_t numDisplays,
                          hwc_display_contents_1_t** displays)
{
    int status = 0;

    log.v("hwc_prepare");

    Hwcomposer *hwc = static_cast<Hwcomposer*>(dev);

    if (!hwc) {
        log.e("hwc_prepare: Invalid HWC device\n");
        status = -EINVAL;
        goto prepare_out;
    }

    if (hwc->prepare(numDisplays, displays) == false) {
        status = -EINVAL;
        goto prepare_out;
    }
prepare_out:
    return 0;
}

static int hwc_set(struct hwc_composer_device_1 *dev,
                     size_t numDisplays,
                     hwc_display_contents_1_t **displays)
{
    int status = 0;

    log.v("hwc_set\n");

    Hwcomposer *hwc = static_cast<Hwcomposer*>(dev);

    if (!hwc) {
        log.e("hwc_set: Invalid HWC device\n");
        status = -EINVAL;
        goto set_out;
    }

    if (hwc->commit(numDisplays, displays) == false) {
        log.e("hwc_set: failed to commit\n");
        status = HWC_EGL_ERROR;
        goto set_out;
    }

set_out:
    return 0;
}

static void hwc_dump(struct hwc_composer_device_1 *dev,
                       char *buff,
                       int buff_len)
{
    Hwcomposer *hwc = static_cast<Hwcomposer*>(dev);

    if (hwc)
       hwc->dump(buff, buff_len, 0);
}

void hwc_registerProcs(struct hwc_composer_device_1 *dev,
                          hwc_procs_t const *procs)
{
    log.v("hwc_registerProcs\n");

    Hwcomposer *hwc = static_cast<Hwcomposer*>(dev);

    if (!hwc) {
        log.e("hwc_registerProcs: Invalid HWC device\n");
        return;
    }

    hwc->registerProcs(procs);
}

static int hwc_device_close(struct hw_device_t *dev)
{
    return 0;
}

static int hwc_query(struct hwc_composer_device_1 *dev,
                       int what,
                       int* value)
{
    log.v("hwc_query: what %d\n", what);
    return -EINVAL;
}

static int hwc_eventControl(struct hwc_composer_device_1 *dev,
                                int disp,
                                int event,
                                int enabled)
{
    int err = 0;
    bool ret;

    log.v("hwc_eventControl: event %d, enabled %d\n", event, enabled);
    Hwcomposer *hwc = static_cast<Hwcomposer*>(dev);

    if (!hwc) {
        log.e("hwc_eventControl: Invalid HWC device\n");
        return -EINVAL;
    }

    switch (event) {
    case HWC_EVENT_VSYNC:
        ret = hwc->vsyncControl(disp, enabled);
        if (ret == false) {
            log.e("hwc_eventControl: failed to enable/disable vsync\n");
            err = -EINVAL;
        }
        break;
    default:
        log.e("hwc_eventControl: unsupported event %d\n", event);
    }

    return err;
}

static int hwc_blank(hwc_composer_device_1_t *dev, int disp, int blank)
{
    Hwcomposer *hwc = static_cast<Hwcomposer*>(dev);
    bool ret;

    if (!hwc) {
        log.e("hwc_blank: invalid HWC device");
        return -EINVAL;
    }

    ret = hwc->blank(disp, blank);
    if (ret == false) {
        log.e("hwc_blank: failed to blank disp %d, blank %d", disp, blank);
        return -EINVAL;
    }

    return 0;
}

static int hwc_getDisplayConfigs(hwc_composer_device_1_t *dev,
                                     int disp,
                                     uint32_t *configs,
                                     size_t *numConfigs)
{
    Hwcomposer *hwc = static_cast<Hwcomposer*>(dev);
    bool ret;

    if (!hwc) {
        log.e("hwc_getDisplayConfigs: invalid HWC device");
        return -EINVAL;
    }

    ret = hwc->getDisplayConfigs(disp, configs, numConfigs);
    if (ret == false) {
        log.e("hwc_getDisplayConfigs: failed to get configs of disp %d", disp);
        return -EINVAL;
    }

    return 0;
}

static int hwc_getDisplayAttributes(hwc_composer_device_1_t *dev,
                                        int disp,
                                        uint32_t config,
                                        const uint32_t *attributes,
                                        int32_t *values)
{
    Hwcomposer *hwc = static_cast<Hwcomposer*>(dev);
    bool ret;

    if (!hwc) {
        log.e("hwc_getDisplayAttributes: invalid HWC device");
        return -EINVAL;
    }

    ret = hwc->getDisplayAttributes(disp, config, attributes, values);
    if (ret == false) {
        log.e("hwc_getDisplayAttributes: failed to get attributes of disp %d",
              disp);
        return -EINVAL;
    }

    return 0;
}

static int hwc_compositionComplete(hwc_composer_device_1_t *dev, int disp)
{
    Hwcomposer *hwc = static_cast<Hwcomposer*>(dev);
    bool ret;

    if (!hwc) {
        log.e("hwc_compositionComplete: invalid HWC device");
        return -EINVAL;
    }

    ret = hwc->compositionComplete(disp);
    if (ret == false) {
        log.e("hwc_compositionComplete: faild for disp %d", disp);
        return -EINVAL;
    }

    return 0;
}

//------------------------------------------------------------------------------

static int hwc_device_open(const struct hw_module_t* module,
                              const char* name,
                              struct hw_device_t** device)
{
    int status = -EINVAL;

    log.d("hwc_device_open: open device %s", name);

    if (!strcmp(name, HWC_HARDWARE_COMPOSER)) {
        Hwcomposer *hwc = new MrflHwcomposer();
        if (!hwc) {
            log.e("hwc_device_open: No memory\n");
            status = -ENOMEM;
            goto hwc_init_out;
        }

        /* initialize our state here */
        if (hwc->initialize() == false) {
            log.e("hwc_device_open: failed to intialize HWCompower\n");
            status = -EINVAL;
            goto hwc_init_out;
        }

        /* initialize the procs */
        hwc->hwc_composer_device_1_t::common.tag = HARDWARE_DEVICE_TAG;
        hwc->hwc_composer_device_1_t::common.version = HWC_DEVICE_API_VERSION_1_1;
        hwc->hwc_composer_device_1_t::common.module =
            const_cast<hw_module_t*>(module);
        hwc->hwc_composer_device_1_t::common.close = hwc_device_close;

        hwc->hwc_composer_device_1_t::prepare = hwc_prepare;
        hwc->hwc_composer_device_1_t::set = hwc_set;
        hwc->hwc_composer_device_1_t::dump = hwc_dump;
        hwc->hwc_composer_device_1_t::registerProcs = hwc_registerProcs;
        hwc->hwc_composer_device_1_t::query = hwc_query;

        hwc->hwc_composer_device_1_t::blank = hwc_blank;
        hwc->hwc_composer_device_1_t::eventControl = hwc_eventControl;
        hwc->hwc_composer_device_1_t::getDisplayConfigs = hwc_getDisplayConfigs;
        hwc->hwc_composer_device_1_t::getDisplayAttributes = hwc_getDisplayAttributes;

        // This is used to hack FBO switch flush issue in SurfaceFlinger.
        hwc->hwc_composer_device_1_t::reserved_proc[0] = (void*)hwc_compositionComplete;

        *device = &hwc->hwc_composer_device_1_t::common;

        status = 0;
    }
hwc_init_out:
    return status;
}

} // namespace intel
} // namespace android

static struct hw_module_methods_t hwc_module_methods = {
    open: android::intel::hwc_device_open
};

hwc_module_t HAL_MODULE_INFO_SYM = {
    common: {
        tag: HARDWARE_MODULE_TAG,
        version_major: 1,
        version_minor: 1,
        id: HWC_HARDWARE_MODULE_ID,
        name: "Intel Hardware Composer",
        author: "Intel UMSE",
        methods: &hwc_module_methods,
    }
};
