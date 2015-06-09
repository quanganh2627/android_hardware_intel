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
#include <hardware/hardware.h>

#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#include <cutils/log.h>
#include <cutils/atomic.h>

#include <IntelHWComposer.h>
#include <IntelHWComposerCfg.h>

/* global hwcomposer cfg info */
hwc_cfg cfg;

static void dump_layer(hwc_layer_1_t const* l)
{

}

static int hwc_prepare(struct hwc_composer_device_1 *dev,
                       size_t numDisplays, hwc_display_contents_1_t** displays)
{
    int status = 0;

    ALOGV("%s\n", __func__);

    IntelHWComposer *hwc = static_cast<IntelHWComposer*>(dev);

    if (!hwc) {
        ALOGE("%s: Invalid HWC device\n", __func__);
        status = -EINVAL;
        goto prepare_out;
    }

#ifdef INTEL_RGB_OVERLAY
    {
        IntelHWCWrapper* wrapper = hwc->getWrapper();
        wrapper->pre_prepare(list);
    }
#endif

    if (hwc->prepareDisplays(numDisplays, displays) == false) {
        status = -EINVAL;
        goto prepare_out;
    }
#ifdef INTEL_RGB_OVERLAY
#ifdef SKIP_DISPLAY_SYS_LAYER
    {
        IntelHWCWrapper* wrapper = hwc->getWrapper();
        wrapper->post_prepare(list);
    }
#endif
#endif

prepare_out:
    return 0;
}

static int hwc_set(struct hwc_composer_device_1 *dev,
                   size_t numDisplays, hwc_display_contents_1_t** displays)
{
    int status = 0;

    ALOGV("%s\n", __func__);

    IntelHWComposer *hwc = static_cast<IntelHWComposer*>(dev);

    if (!hwc) {
        ALOGE("%s: Invalid HWC device\n", __func__);
        status = -EINVAL;
        goto set_out;
    }

#ifdef INTEL_RGB_OVERLAY
#ifdef SKIP_DISPLAY_SYS_LAYER
    if (dpy && sur && list)
    {
        IntelHWCWrapper* wrapper = hwc->getWrapper();
        wrapper->pre_commit(dpy, sur, list);
    }
#endif
#endif

    if (hwc->commitDisplays(numDisplays, displays) == false) {
        ALOGE("%s: failed to commit\n", __func__);
        status = HWC_EGL_ERROR;
        goto set_out;
    }

#ifdef INTEL_RGB_OVERLAY
#ifdef SKIP_DISPLAY_SYS_LAYER
    if (dpy && sur && list)
    {
        IntelHWCWrapper* wrapper = hwc->getWrapper();
        wrapper->post_commit(dpy, sur, list);
    }
#endif
#endif

set_out:
    return 0;
}

static void hwc_dump(struct hwc_composer_device_1 *dev, char *buff, int buff_len)
{
    IntelHWComposer *hwc = static_cast<IntelHWComposer*>(dev);

    if (hwc)
       hwc->dump(buff, buff_len, 0);
}

void hwc_registerProcs(struct hwc_composer_device_1* dev,
                       hwc_procs_t const* procs)
{
    ALOGV("%s\n", __func__);

    IntelHWComposer *hwc = static_cast<IntelHWComposer*>(dev);

    if (!hwc) {
        ALOGE("%s: Invalid HWC device\n", __func__);
        return;
    }

    hwc->registerProcs(procs);
}

static int hwc_device_close(struct hw_device_t *dev)
{
#if 0
    IntelHWComposer *hwc = static_cast<IntelHWComposer*>(dev);

    ALOGD("%s\n", __func__);

    delete hwc;
#endif
    return 0;
}

static int hwc_get_cfg(hwc_cfg *cfg)
{
    FILE *fp = NULL;
    char cfg_string[CFG_STRING_LEN];
    char * pch;

    if (cfg == NULL) {
        ALOGE("%s: pass NULL parameter!\n", __func__);
        return -1;
    } else {
        /* set default cfg parameter */
        cfg->enable = 1;
        cfg->log_level = 0;
        cfg->bypasspost = 1;
    }

    fp = fopen(HWC_CFG_PATH, "r");
    if (fp != NULL) {
        memset(cfg_string, '\0', CFG_STRING_LEN);
        fread(cfg_string, 1, CFG_STRING_LEN-1, fp);

        ALOGD("%s: read config line %s!\n", __func__, cfg_string);

        pch = strstr(cfg_string, "enable");
        if (pch != NULL)
            cfg->enable = *(pch+strlen("enable=")) - '0';

        pch = strstr(cfg_string, "log_level");
        if (pch != NULL)
            cfg->log_level = atoi(pch+strlen("log_level="));

        pch = strstr(cfg_string, "bypasspost");
        if (pch != NULL)
            cfg->bypasspost = atoi(pch+strlen("bypasspost="));


        ALOGD("%s:get cfg parameter enable_hwc=%d,log_level=%d,bypasspost=%d!\n",
                     __func__, cfg->enable, cfg->log_level, cfg->bypasspost);
        fclose(fp);
    }

    return 0;
}

static int hwc_query(struct hwc_composer_device_1* dev,
                       int what,
                       int* value)
{
    ALOGD("%s: what %d\n", __func__, what);
    return -EINVAL;
}

static int hwc_eventControl(struct hwc_composer_device_1 *dev,
                             int disp,
                                int event,
                                int enabled)
{
    int err = 0;
    bool ret;

    ALOGV("%s: event %d, enabled %d\n", __func__, event, enabled);
    IntelHWComposer *hwc = static_cast<IntelHWComposer*>(dev);

    if (!hwc) {
        ALOGE("%s: Invalid HWC device\n", __func__);
        return -EINVAL;
    }

    switch (event) {
    case HWC_EVENT_VSYNC:
        ret = hwc->vsyncControl(enabled);
        if (ret == false) {
            ALOGE("%s: failed to enable/disable vsync\n", __func__);
            err = -EINVAL;
        }
        break;
    default:
        ALOGE("%s: unsupported event %d\n", __func__, event);
    }

    return err;
}

static int hwc_blank(hwc_composer_device_1_t* dev, int disp, int blank)
{
    IntelHWComposer *hwc = static_cast<IntelHWComposer*>(dev);

    if (hwc && hwc->blankDisplay(disp, blank))
        return 0;

    return -EINVAL;
}

static int hwc_getDisplayConfigs(hwc_composer_device_1_t* dev, int disp,
            uint32_t* configs, size_t* numConfigs)
{
    IntelHWComposer *hwc = static_cast<IntelHWComposer*>(dev);

    if (hwc && hwc->getDisplayConfigs(disp, configs, numConfigs))
        return 0;

    return -EINVAL;
}

static int hwc_getDisplayAttributes(hwc_composer_device_1_t* dev, int disp,
            uint32_t config, const uint32_t* attributes, int32_t* values)
{
    IntelHWComposer *hwc = static_cast<IntelHWComposer*>(dev);
    
    if (hwc && hwc->getDisplayAttributes(disp, config, attributes, values))
        return 0;

    return -EINVAL;
}

static int hwc_setFramecount(hwc_composer_device_1_t* dev, int cmd, int count, int x, int y)
{
    IntelHWComposer *hwc = static_cast<IntelHWComposer*>(dev);

    if (hwc && hwc->setFramecount(cmd, count, x, y))
        return 0;

    return -EINVAL;
}
/*****************************************************************************/

static int hwc_device_open(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device)
{
    int status = -EINVAL;

    ALOGV("%s: name %s\n", __func__, name);

    hwc_get_cfg(&cfg);

    if (cfg.enable == 0) {
        status = -EINVAL;
        goto hwc_init_out;
    }

    if (!strcmp(name, HWC_HARDWARE_COMPOSER)) {
        IntelHWComposer *hwc = new IntelHWComposer();
        if (!hwc) {
            ALOGE("%s: No memory\n", __func__);
            status = -ENOMEM;
            goto hwc_init_out;
        }

        /* initialize our state here */
        if (hwc->initialize() == false) {
            ALOGE("%s: failed to intialize HWCompower\n", __func__);
            status = -EINVAL;
            goto hwc_init_out;
        }

#ifdef INTEL_RGB_OVERLAY
        {
            IntelHWCWrapper* wrapper = hwc->getWrapper();
            if (wrapper->initialize() == false) {
                // Don't return error even failed to initialize wrapper.
                ALOGW("%s: failed to intialize HWCompowerWrapper\n", __func__);
            }
        }
#endif

        /* initialize the procs */
        hwc->hwc_composer_device_1_t::common.tag = HARDWARE_DEVICE_TAG;
        hwc->hwc_composer_device_1_t::common.version = HWC_DEVICE_API_VERSION_1_3;
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

        // Frame update debug interaction with SurfaceFlinger.
        hwc->hwc_composer_device_1_t::reserved_proc[0] = (void*)hwc_setFramecount;
        *device = &hwc->hwc_composer_device_1_t::common;
        status = 0;
    }
hwc_init_out:
    return status;
}

static struct hw_module_methods_t hwc_module_methods = {
    open: hwc_device_open
};

hwc_module_t HAL_MODULE_INFO_SYM = {
    common: {
        tag: HARDWARE_MODULE_TAG,
        version_major: 1,
        version_minor: 2,
        id: HWC_HARDWARE_MODULE_ID,
        name: "Intel Hardware Composer",
        author: "Intel MCG/PSI",
        methods: &hwc_module_methods,
        dso: 0,
        reserved: { 0 }
    }
};
