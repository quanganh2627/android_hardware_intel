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

#include "ShakeSensor.h"

/*****************************************************************************/

#undef LOG_TAG
#define LOG_TAG "ShakeSensor"

#define SHAKING_BUF_SIZE            512
#define SHAKING_SAMPLE_RATE         20
#define SHAKING_BUF_DELAY           0
#define SHAKE_SEN_MEDIUM            1
#define PSH_SESSION_NOT_OPENED      NULL
#define SHAKING 1
#define INVALID_EVENT -1

ShakeSensor::ShakeSensor()
    : SensorBase("shake"),
      mEnabled(0),
      mHasPendingEvent(false)
{
    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = SENSORS_HANDLE_SHAKE;
    mPendingEvent.type = SENSOR_TYPE_SHAKE;
    memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));
}

ShakeSensor::~ShakeSensor()
{
    LOGI("~ShakeSensor %d\n", mEnabled);
    if (mEnabled == 1)
        enable(0, 0);
}

int ShakeSensor::enable(int32_t handle, int en)
{
    int flags = en ? 1 : 0;
    int ret;

    LOGI("ShakeSensor - %s - enable=%d", __FUNCTION__, en);

    if (mHandle == PSH_SESSION_NOT_OPENED)
        return -1;

    if (mEnabled == flags)
        return 0;

    if (flags == 0) {
        ret = psh_stop_streaming(mHandle);
        if (ret != 0) {
            E("ShakeSensor - %s, failed to stop streaming, ret = %d",
                                                     __FUNCTION__, ret);
            return -1;
        }
        LOGI("psh_stop_streaming succeeded");
    } else if (flags == 1) {
        int sensitivity = SHAKE_SEN_MEDIUM;
        if (psh_set_property(mHandle, PROP_SHAKING_SENSITIVITY, &sensitivity) != ERROR_NONE) {
            LOGE("psh_set_property failed.");
            return false;
        }
        ret = psh_start_streaming(mHandle, SHAKING_SAMPLE_RATE, SHAKING_BUF_DELAY);
        if (ret != 0) {
            E("ShakeSensor - %s, failed to start streaming, ret = %d",
                                                     __FUNCTION__, ret);
            return -1;
        }
        LOGI("psh_start_streaming succeeded!");
     }

     mEnabled = flags;
     return 0;
}

int ShakeSensor::setDelay(int32_t handle, int64_t ns)
{
    LOGI("setDelay - %s - %lld", __FUNCTION__, ns);
    return 0;
}

bool ShakeSensor::hasPendingEvents() const
{
    return mHasPendingEvent;
}

int ShakeSensor::getShakeEvent(struct shaking_data* data) const
{
    if (data->shaking == SHAKING) {
        return SENSOR_EVENT_TYPE_SHAKE;
    }
    return INVALID_EVENT;
}

int ShakeSensor::readEvents(sensors_event_t* data, int count)
{
    int size, numEventReceived = 0;
    char buf[512];
    int unit_size = sizeof(struct shaking_data);

    LOGI("ShakeSensor - %s", __FUNCTION__);
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
    struct shaking_data *p_shake_data;
    while (size > 0) {
        p_shake_data = (struct shaking_data *)p;
        int shake = getShakeEvent(p_shake_data);
        mPendingEvent.data[0] = (static_cast<float> (shake));

        mPendingEvent.timestamp = getTimestamp();

        if (mEnabled == 1 && shake != INVALID_EVENT) {
           *data++ = mPendingEvent;
            numEventReceived++;
        }

        size = size - unit_size;
        p = p + unit_size;
    }

    LOGI("ShakeSensor - %s read data %f", __FUNCTION__,
                                mPendingEvent.distance);
    return numEventReceived;
}
