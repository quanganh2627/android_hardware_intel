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
#include <fcntl.h>
#include <errno.h>

#include <Drm.h>
#include <Log.h>
#include <cutils/log.h>

namespace android {

using namespace intel;
ANDROID_SINGLETON_STATIC_INSTANCE(Drm);

namespace intel {

static Log& log = Log::getInstance();

Drm::Drm() : Singleton<Drm>()
{
    int fd = open("/dev/card0", O_RDWR, 0);
    if (fd < 0) {
        log.e("Drm(): drmOpen failed. %s", strerror(errno));
    }

    mDrmFd = fd;
    memset(&mOutputs, 0, sizeof(mOutputs));

    LOGD("Drm(): successfully. mDrmFd %d", fd);
}

bool Drm::detect()
{
    Mutex::Autolock _l(mLock);

    if (mDrmFd < 0) {
        log.e("detect(): invalid Fd");
        return false;
    }

    // try to get drm resources
    drmModeResPtr resources = drmModeGetResources(mDrmFd);
    if (!resources || !resources->connectors) {
        log.e("detect(): fail to get drm resources. %s\n", strerror(errno));
        return false;
    }

    drmModeConnectorPtr connector = NULL;
    drmModeEncoderPtr encoder = NULL;
    drmModeCrtcPtr crtc = NULL;
    drmModeConnectorPtr connectors[OUTPUT_MAX];
    drmModeModeInfoPtr mode = NULL;
    drmModeFBPtr fbInfo = NULL;
    struct Output *output = NULL;

    for (int i = 0; i < resources->count_connectors; i++) {
        connector = drmModeGetConnector(mDrmFd, resources->connectors[i]);
        if (!connector) {
            log.e("detect(): fail to get drm connector\n");
            continue;
        }

        int outputIndex = -1;
        if (connector->connector_type == DRM_MODE_CONNECTOR_MIPI ||
            connector->connector_type == DRM_MODE_CONNECTOR_LVDS) {
            log.v("detect(): got MIPI/LVDS connector\n");
            if (connector->connector_type_id == 1)
                outputIndex = OUTPUT_MIPI0;
            else if (connector->connector_type_id == 2)
                outputIndex = OUTPUT_MIPI1;
            else {
                log.w("detect(): unknown connector type\n");
                // FIXME: sure?
                outputIndex = OUTPUT_MIPI0;
            }
        } else if (connector->connector_type == DRM_MODE_CONNECTOR_DVID) {
            log.v("detect(): got HDMI connector\n");
            outputIndex = OUTPUT_HDMI;
        }

        if (outputIndex < 0)
            continue;

        // update output, free the old objects first
        output = &mOutputs[outputIndex];

        output->connected =  0;

        if (output->connector) {
            drmModeFreeConnector(output->connector);
            output->connector = 0;
        }
        if (output->encoder) {
            drmModeFreeEncoder(output->encoder);
            output->encoder = 0;
        }

        if (output->crtc) {
            drmModeFreeCrtc(output->crtc);
            output->crtc = 0;
        }

        if (output->fb) {
            drmModeFreeFB(output->fb);
            output->fb = 0;
        }

        output->connector = connector;

        // get current encoder
        encoder = drmModeGetEncoder(mDrmFd, connector->encoder_id);
        if (!encoder) {
            log.e("detect(): fail to get drm encoder\n");
            continue;
        }

        output->encoder = encoder;

        // get crtc
        crtc = drmModeGetCrtc(mDrmFd, encoder->crtc_id);
        if (!crtc) {
            log.e("detect(): fail to get drm crtc\n");
            continue;
        }

        output->crtc = crtc;

        // get fb info
        fbInfo = drmModeGetFB(mDrmFd, crtc->buffer_id);
        if (!fbInfo) {
            log.e("detect(): fail to get fb info\n");
            continue;
        }

        output->fb = fbInfo;

        output->connected = (output->connector &&
                             output->connector->connection == DRM_MODE_CONNECTED &&
                             output->encoder &&
                             output->crtc &&
                             output->fb) ? 1 : 0;
    }

    drmModeFreeResources(resources);

    return true;
}

bool Drm::setMode(int output, drmModeModeInfoPtr mode)
{
    if (!mode) {
        log.e("Drm::setMode: invalid parameters");
        return false;
    }

    log.d("Drm::setMode: %dx%d %d",
          mode->hdisplay, mode->vdisplay, mode->vrefresh);

    return false;
}

bool Drm::writeReadIoctl(unsigned long cmd, void *data,
                           unsigned long size)
{
    int err;

    Mutex::Autolock _l(mLock);

    if (mDrmFd <= 0) {
        log.e("Drm is not initialized");
        return false;
    }

    if (!data || !size) {
        log.e("Invalid parameters");
        return false;
    }

    err = drmCommandWriteRead(mDrmFd, cmd, data, size);
    if (err) {
        log.e("Drm::Failed call 0x%x ioct with failure %d", cmd, err);
        return false;
    }

    return true;
}

bool Drm::writeIoctl(unsigned long cmd, void *data,
                       unsigned long size)
{
    int err;

    Mutex::Autolock _l(mLock);

    if (mDrmFd <= 0) {
        log.e("Drm is not initialized");
        return false;
    }

    if (!data || !size) {
        log.e("Invalid parameters");
        return false;
    }

    err = drmCommandWrite(mDrmFd, cmd, data, size);
    if (err) {
        log.e("Drm::Failed call 0x%x ioct with failure %d", cmd, err);
        return false;
    }

    return true;
}

int Drm::getDrmFd() const
{
    return mDrmFd;
}

struct Output* Drm::getOutput(int output)
{
    Mutex::Autolock _l(mLock);

    if (output < 0 || output >= OUTPUT_MAX) {
        log.e("getOutput(): invalid output %d", output);
        return 0;
    }

    return &mOutputs[output];
}

bool Drm::outputConnected(int output)
{
    Mutex::Autolock _l(mLock);

    if (output < 0 || output >= OUTPUT_MAX) {
        log.e("outputConnected(): invalid output %d", output);
        return false;
    }

    return mOutputs[output].connected ? true : false;
}

} // namespace intel
} // namespace android

