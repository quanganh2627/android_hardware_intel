/*
 * Copyright (c) 2014 Intel Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#define LOG_TAG "Camera_SensorHWExtIsp"

#include <linux/media.h>
#include <linux/v4l2-subdev.h>
#include <assert.h>
#include "AtomCommon.h"
#include "v4l2device.h"
#include "PerformanceTraces.h"
#include "SensorHWExtIsp.h"

namespace android {

SensorHWExtIsp::SensorHWExtIsp(int cameraId)
    :SensorHW(cameraId)
{

}

SensorHWExtIsp::~SensorHWExtIsp()
{
    // Base class handles destruction
}

int SensorHWExtIsp::setAfMode(int mode)
{
    LOG2("@%s: %d", __FUNCTION__, mode);
    // Fixed focus camera -> no effect
    if (PlatformData::isFixedFocusCamera(mCameraId))
        return -1;

    int ret = -1;
    ret = extIspIoctl(EXT_ISP_CID_FOCUS_MODE, mode);
    return ret;
}

int SensorHWExtIsp::getAfMode(int *mode)
{
    LOG2("@%s", __FUNCTION__);

    if (PlatformData::isFixedFocusCamera(mCameraId))
        return -1;

    int data = -1;
    int retval =  extIspIoctl(EXT_ISP_CID_GET_AF_MODE, data);
    *mode = data;

    return retval;
}

int SensorHWExtIsp::setAfEnabled(bool enable)
{
    LOG2("@%s: en: %d ", __FUNCTION__, enable);

    if (PlatformData::isFixedFocusCamera(mCameraId))
        return -1;

    // start running the AF
    int data = enable ? EXT_ISP_FOCUS_SEARCH : EXT_ISP_FOCUS_STOP;

    int retval = extIspIoctl(EXT_ISP_CID_FOCUS_EXECUTION, data);
    return retval;
}

int SensorHWExtIsp::setAfWindows(const CameraWindow *windows, int numWindows)
{
    LOG2("@%s", __FUNCTION__);

    status_t retX = -1, retY = -1;

    if (PlatformData::isFixedFocusCamera(mCameraId))
        return -1;

    if (windows == NULL || numWindows <= 0) {
        return NO_ERROR;
    }

    // TODO: Support multiple windows?

    // Accroding to m10mo spec, the touch AF coordinate is the upper
    // left corner of the touch area. Thus, using only that point here.
    int x = windows[0].x_left;
    int y = windows[0].y_top;
    retX = extIspIoctl(EXT_ISP_CID_TOUCH_POSX, x);
    retY = extIspIoctl(EXT_ISP_CID_TOUCH_POSY, y);

    if (retX != NO_ERROR || retY != NO_ERROR) {
        ALOGW("Failed setting AF windows, retvals %d, %d", retX, retY);
        return UNKNOWN_ERROR;
    }

    return NO_ERROR;
}

int SensorHWExtIsp::setAeFlashMode(int mode)
{
    LOG2("@%s: %d", __FUNCTION__, mode);
    int ret = 0;

    // use our custom control for external ISP, as standard
    // V4L2 value range lacks "auto" mode.
    // TODO: can we use existing V4L2 std control instead? Would not need this hack then...
    ret = extIspIoctl(EXT_ISP_CID_FLASH_MODE, mode);
    if (ret != 0) {
        ALOGW("Failed setting flash mode, ret: %d", ret);
        return UNKNOWN_ERROR;
    }

    return ret;
}

/**
 * Helper function for sending the extension xioctl()
 */
int SensorHWExtIsp::extIspIoctl(int id, int &data)
{
    struct atomisp_ext_isp_ctrl cmd;
    CLEAR(cmd);

    cmd.id = id;
    cmd.data = data;

    return mDevice->xioctl(ATOMISP_IOC_EXT_ISP_CTRL, &cmd);
}

} // namespace android
