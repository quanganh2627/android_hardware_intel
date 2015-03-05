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
#include <dlfcn.h>
#include <sys/time.h>
#include <cutils/log.h>

#include "OrientationSensor.h"

/*****************************************************************************/

OrientationSensor::OrientationSensor()
: SensorBase("orientation"),
      mEnabled(0)
{
    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = SENSORS_HANDLE_ORIENTATION;
    mPendingEvent.type = SENSOR_TYPE_ORIENTATION;
    memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));
}

OrientationSensor::~OrientationSensor()
{
    if (mEnabled)
        enable(0, 0);
}

int OrientationSensor::enable(int32_t handle, int en)
{
    unsigned int flags = en ? 1 : 0;
    int ret;

    D("OrientationSensor - %s, flags = %d, mEnabled = %d, data_fd = %d",
                          __FUNCTION__, flags, mEnabled, data_fd);

    if (mHandle == NULL)
        return -1;

    if ((flags != mEnabled) && (flags == 0)) {
        ret = psh_stop_streaming(mHandle);
        if (ret != 0) {
            E("OrientationSensor - %s, failed to stop streaming, ret = %d",
                                                  __FUNCTION__, ret);
            return -1;
        }
    }

    mEnabled = flags;
    return 0;
}

#define MAX_DATA_RATE 100

int OrientationSensor::setDelay(int32_t handle, int64_t ns)
{
    int data_rate = 0,delay_ms, ret;

    if (mHandle == NULL)
        return -1;

    if (mEnabled == 0)
        return 0;

    D("psh_start_streaming(), data_rate is %d", data_rate);

    delay_ms = ns / 1000000;
    data_rate = 1000 / delay_ms;

    if (data_rate == 0)
        return -1;

    if (data_rate > MAX_DATA_RATE)
        data_rate = MAX_DATA_RATE;

    last_timestamp = getTimestamp();

    ret = psh_start_streaming(mHandle, data_rate, 0);
    if (ret != 0) {
        E("psh_start_streaming failed, ret is %d", ret);
        return -1;
    }

    return 0;
}

int OrientationSensor::readEvents(sensors_event_t* data, int count)
{
    int size, numEventReceived = 0;
    char buf[512];
    struct orientation_data *p_data;
    int unit_size = sizeof(struct orientation_data);
    int64_t current_timestamp;
    int64_t step;

    if (count < 1)
        return -EINVAL;

    D("OrientationSensor::%s, count is %d", __FUNCTION__, count);

    if (mHandle == NULL)
        return 0;

    if ((unit_size * count) <= 512)
        size = unit_size * count;
    else
        size = (512 / unit_size) * unit_size;

    size = read(data_fd, buf, size);
    count = size / unit_size;

    D("OrientationSensor::readEvents read size is %d", size);

    current_timestamp = getTimestamp();
    step = (current_timestamp - last_timestamp) / count;

    char *p = buf;

    p_data = (struct orientation_data *)buf;
    while (size > 0) {

        mPendingEvent.data[0] = (float)p_data->azimuth/65536;
        mPendingEvent.data[1] = (float)p_data->pitch/65536;
        mPendingEvent.data[2] = (float)p_data->roll/65536;

        mPendingEvent.timestamp = last_timestamp + step * (numEventReceived + 1);

        if (mEnabled == 1) {
           *data++ = mPendingEvent;
            numEventReceived++;
        }

        size = size - unit_size;
        p = p + unit_size;
        p_data = (struct orientation_data *)p;
    }

    D("OrientationSensor::%s, numEventReceived is %d", __FUNCTION__,
                                            numEventReceived);

    last_timestamp = current_timestamp;

    return numEventReceived;
}
