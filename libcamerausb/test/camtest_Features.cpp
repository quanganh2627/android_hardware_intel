/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <camera.h>
#include <cutils/properties.h>
#include <gtest/gtest.h>
#include <linux/videodev2.h>

#define LOG_TAG "CameraFeatures"
#include <utils/Log.h>

namespace android {

static const char *PROP_PREFIX          = "ro.camera";
static const char *PROP_NUMBER          = "number";
static const char *PROP_DEVNAME         = "devname";
static const char *PROP_FACING          = "facing";
static const char *PROP_ORIENTATION     = "orientation";
static const char *PROP_FACING_FRONT    = "front";
static const char *PROP_FACING_BACK     = "back";

class CameraFeatures : public testing::Test {
protected:

    struct CameraSensor {
        char devName[PROPERTY_VALUE_MAX];       // device node's name, e.g. /dev/video0
        struct camera_info info;                // camera info defined by Android
        int fd;                                 // the file descriptor of device at run time
    };

    // Initializes information about each camera needed for test.
    // Camera information comes from build properties defined in BoardConfig.mk
    virtual void SetUp()
    {
        ALOGD("%s", __FUNCTION__);

        char propKey[PROPERTY_KEY_MAX];
        char propVal[PROPERTY_VALUE_MAX];

        mNumCameras = 0;
        mCameras = NULL;

        // get total number of cameras
        snprintf(propKey, sizeof(propKey), "%s.%s", PROP_PREFIX, PROP_NUMBER);
        ASSERT_NE(property_get(propKey, propVal, 0), 0)
            << "Failed to get number of cameras from prop.";

        mNumCameras = atoi(propVal);
        ASSERT_GE(mNumCameras, 0)
            << "Invalid number of cameras " << mNumCameras;

        mCameras = new CameraSensor[mNumCameras];
        memset(mCameras, 0, sizeof(CameraSensor) * mNumCameras);
        for (int i = 0; i < mNumCameras; i++) {
            mCameras[i].fd = -1;

            // device name
            snprintf(propKey, sizeof(propKey), "%s.%d.%s", PROP_PREFIX, i, PROP_DEVNAME);
            ASSERT_NE(property_get(propKey, mCameras[i].devName, 0), 0)
                << "Failed to get name of camera " << i << " from prop";

            // facing info
            snprintf(propKey, sizeof(propKey), "%s.%d.%s", PROP_PREFIX, i, PROP_FACING);
            ASSERT_NE(property_get(propKey, propVal, 0), 0)
                << "Failed to get facing of camera" << i << " from prop";

            if (!strncmp(PROP_FACING_FRONT, propVal, strlen(PROP_FACING_FRONT))) {
                mCameras[i].info.facing = CAMERA_FACING_FRONT;
            } else if (!strncmp(PROP_FACING_BACK, propVal, strlen(PROP_FACING_BACK))) {
                mCameras[i].info.facing = CAMERA_FACING_BACK;
            } else {
                FAIL() << "Invalid facing of camera " << i << " from prop";
            }

            // orientation
            snprintf(propKey, sizeof(propKey), "%s.%d.%s",
                    PROP_PREFIX, i, PROP_ORIENTATION);
            ASSERT_NE(property_get(propKey, propVal, 0), 0)
                << "Failed to get orientation of camera " << i << " from prop";

            mCameras[i].info.orientation = atoi(propVal);
            ASSERT_GE(mCameras[i].info.orientation, 0);

            ALOGD("%s Camera id=%d, device=%s, facing=%s, orientation=%d",
                    __FUNCTION__, i, mCameras[i].devName,
                    mCameras[i].info.facing == CAMERA_FACING_FRONT ? "front" : "back",
                    mCameras[i].info.orientation);
        }
    }

    // Cleans up data structures
    virtual void TearDown()
    {
        ALOGD("%s", __FUNCTION__);

        if (mCameras)
            delete [] mCameras;
    }

    int mNumCameras;
    CameraSensor *mCameras;
};

///////////////////////////////////////////////////////////////////////////////
// Test description:
//      Loops over all camera devices and gets supported camera sensor feature
//      information such as resolution, frame rate, format, and supported
//      controls.
// Expected result:
//      1. Doesn't crash
//      2. Able to open devices successfully
//      3. Information retrieved is valid
// Misc:
//      All supported camera sensor features are printed out using Android
//      LOG* macros. Run 'adb logcat' to see such information.
///////////////////////////////////////////////////////////////////////////////
TEST_F(CameraFeatures, Features)
{

#define LOG_FEATURE(fmt, ...) ALOGD("CameraFeature " fmt, ##__VA_ARGS__)

    ASSERT_GT(mNumCameras, 0)
        << "No cameras detected";

    // loop over each camera sensor
    for (int i = 0; i < mNumCameras; i++) {
        int fd = open(mCameras[i].devName, O_RDWR);

        ASSERT_GE(fd, 0) << "unable to open camera fd=" << fd << " name=" << mCameras[i].devName;

        LOG_FEATURE("------------------------ %s (%s) ------------------------",
                mCameras[i].devName,
                mCameras[i].info.facing == CAMERA_FACING_FRONT ? "front" : "back");

        // log all of the supported frame sizes and resolutions for each supported format
        // frame sizes and resolutions may vary depending on format
        struct v4l2_fmtdesc fmt;
        int idx = 0;
        while (true) {
            memset(&fmt, 0, sizeof(fmt));
            fmt.index = idx++;
            fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            fmt.flags = 0;
            if (ioctl(fd, VIDIOC_ENUM_FMT, &fmt) < 0) {
                break;
            }

            struct v4l2_frmsizeenum framesize;
            int j = 0;
            float fps = 0;
            while (true) {
                memset(&framesize, 0, sizeof(framesize));
                framesize.index = j++;
                framesize.pixel_format = fmt.pixelformat;

                if (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &framesize) < 0) {
                    break;
                }

                struct v4l2_frmivalenum frm_interval;
                memset(&frm_interval, 0, sizeof(frm_interval));
                frm_interval.pixel_format = framesize.pixel_format;
                frm_interval.width = framesize.discrete.width;;
                frm_interval.height = framesize.discrete.height;
                fps = -1.0;

                int ret = ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frm_interval);
                ASSERT_GE(ret, 0) << "Error enumerating frame intervals for " << mCameras[i].devName;

                assert(0 != frm_interval.discrete.denominator);

                fps = 1.0 / (1.0 * frm_interval.discrete.numerator / frm_interval.discrete.denominator);

                LOG_FEATURE("Frame info: format=%s, size=%ux%u, fps=%d",
                        fmt.description,
                        framesize.discrete.width,
                        framesize.discrete.height,
                        (int)fps);
            }
        }

        // log all of the supported IOCTL controls and extended controls
        struct v4l2_queryctrl ctrl;
        struct v4l2_querymenu menu;
        memset(&ctrl, 0, sizeof(ctrl));
        ctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
        while (ioctl(fd, VIDIOC_QUERYCTRL, &ctrl) == 0) {
            if (!(ctrl.flags & V4L2_CTRL_FLAG_DISABLED)) {
                int readonly = (int) ((ctrl.flags & V4L2_CTRL_FLAG_READ_ONLY) != 0);
                LOG_FEATURE("Control: %s (id=%d) readonly=%d",
                        ctrl.name, ctrl.id, readonly);
                switch (ctrl.type) {
                    case V4L2_CTRL_TYPE_INTEGER64:
                        LOG_FEATURE("  - type=INTEGER64");
                        break;
                    case V4L2_CTRL_TYPE_INTEGER:
                        LOG_FEATURE("  - type=INTEGER");
                        break;
                    case V4L2_CTRL_TYPE_BOOLEAN:
                        LOG_FEATURE("  - type=BOOLEAN");
                        break;
                    case V4L2_CTRL_TYPE_MENU:
                        LOG_FEATURE("  - type=MENU");
                        memset(&menu, 0, sizeof(menu));
                        menu.id = ctrl.id;
                        for (menu.index = ctrl.minimum; menu.index <= (unsigned int) ctrl.maximum; menu.index++) {
                            ioctl(fd, VIDIOC_QUERYMENU, &menu);
                            LOG_FEATURE("    - menu index=%d name=%s", menu.index, menu.name);
                        };
                        break;
                    case V4L2_CTRL_TYPE_BUTTON:
                        LOG_FEATURE("  - type=BUTTON");
                        break;
                    default:
                        LOG_FEATURE("  - type=???");
                        break;
                }
                LOG_FEATURE("  - minimum=%d", ctrl.minimum);
                LOG_FEATURE("  - maximum=%d", ctrl.maximum);
                LOG_FEATURE("  - step=%d", ctrl.step);
                LOG_FEATURE("  - default_value=%d", ctrl.default_value);
                LOG_FEATURE("  - flags=0x%x", ctrl.flags);
            }
            ctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
        }

        close(fd);
    }
}

} // namespace android
