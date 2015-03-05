/*
 * Copyright (C) 2008 The Android Open Source Project
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

#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/select.h>

#include <cutils/log.h>

#include "SimpleTappingSensor.h"

/*****************************************************************************/

#undef LOG_TAG
#define LOG_TAG "SimpleTappingSensor"

#define STAP_BUF_SIZE            512
#define STAP_SAMPLE_RATE         20
#define STAP_BUF_DELAY           0
#define STAP_DEFAULT_LEVEL       0
#define PSH_SESSION_NOT_OPENED      NULL
#define DOUBLE_TAPPING 2
#define INVALID_EVENT -1

SimpleTappingSensor::SimpleTappingSensor()
    : SensorBase("simpletapping"),
      mEnabled(0),
      mHasPendingEvent(false)
{
    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = SENSORS_HANDLE_SIMPLE_TAPPING;
    mPendingEvent.type = SENSOR_TYPE_SIMPLE_TAPPING;
    memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));
}

SimpleTappingSensor::~SimpleTappingSensor()
{
    LOGI("~SimpleTappingSensor %d\n", mEnabled);
    if (mEnabled == 1)
        enable(0, 0);
}

int SimpleTappingSensor::enable(int32_t handle, int en)
{
    int flags = en ? 1 : 0;
    int ret;

    LOGI("SimpleTappingSensor - %s - enable=%d", __FUNCTION__, en);

    if (mHandle == PSH_SESSION_NOT_OPENED)
        return -1;

    if (mEnabled == flags)
        return 0;

    if (flags == 0) {
        ret = psh_stop_streaming(mHandle);
        if (ret != 0) {
            E("SimpleTappingSensor - %s, failed to stop streaming, ret = %d",
                                                     __FUNCTION__, ret);
            return -1;
        }
        LOGI("psh_stop_streaming succeeded");
    } else if (flags == 1) {
        int level = STAP_DEFAULT_LEVEL;
        if (psh_set_property(mHandle, PROP_STAP_LEVEL, &level) != ERROR_NONE) {
            LOGE("psh_set_property failed.");
            return false;
        }
        ret = psh_start_streaming(mHandle, STAP_SAMPLE_RATE, STAP_BUF_DELAY);
        if (ret != 0) {
            E("SimpleTappingSensor - %s, failed to start streaming, ret = %d",
                                                     __FUNCTION__, ret);
            return -1;
        }
        LOGI("psh_start_streaming succeeded!");
     }

     mEnabled = flags;
     return 0;
}

int SimpleTappingSensor::setDelay(int32_t handle, int64_t ns)
{
    LOGI("setDelay - %s - %lld", __FUNCTION__, ns);
    return 0;
}

bool SimpleTappingSensor::hasPendingEvents() const
{
    return mHasPendingEvent;
}

int SimpleTappingSensor::getSimpleTappingEvent(struct stap_data* data) const
{
    if (data->stap == DOUBLE_TAPPING) {
        return SENSOR_EVENT_TYPE_SIMPLE_TAPPING_DOUBLE_TAPPING;
    }
    return INVALID_EVENT;
}

int SimpleTappingSensor::readEvents(sensors_event_t* data, int count)
{
    int size, numEventReceived = 0;
    char buf[512];
    int unit_size = sizeof(struct stap_data);

    LOGI("SimpleTappingSensor - %s", __FUNCTION__);
    if (count < 1)
        return -EINVAL;

    if (mHandle == PSH_SESSION_NOT_OPENED)
        return 0;

    if ((unit_size * count) <= 512)
        size = unit_size * count;
    else
        size = (512 / unit_size) * unit_size;

    size = read(data_fd, buf, size);

    char *p = buf;
    struct stap_data *p_stap_data;
    while (size > 0) {
        p_stap_data = (struct stap_data *)p;
        int stap = getSimpleTappingEvent(p_stap_data);
        mPendingEvent.data[0] = (static_cast<float> (stap));

        mPendingEvent.timestamp = getTimestamp();

        if (mEnabled == 1 && stap != INVALID_EVENT) {
           *data++ = mPendingEvent;
            numEventReceived++;
        }

        size = size - unit_size;
        p = p + unit_size;
    }

    LOGI("SimpleTappingSensor - %s read data %f", __FUNCTION__,
                                mPendingEvent.distance);
    return numEventReceived;
}
