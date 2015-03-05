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
#include <sys/stat.h>

#include "GyroSensor.h"
/*****************************************************************************/

GyroSensor::GyroSensor()
    : SensorBase("gyro"),
      mEnabled(0)
{
    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = idHandle;
    mPendingEvent.type = SENSOR_TYPE_GYROSCOPE;
    memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));
}

GyroSensor::GyroSensor(int handle)
    : SensorBase("gyro", handle),
      mEnabled(0)
{
    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = idHandle;
    mPendingEvent.type = SENSOR_TYPE_GYROSCOPE;
    memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));
}

GyroSensor::~GyroSensor() {
    if (mEnabled)
        enable(0, 0);
}

int GyroSensor::setInitialState() {
    return 0;
}

int GyroSensor::enable(int32_t handle, int en) {
    int flags = en ? 1 : 0;
    int ret;
    char buf[50];

    D("GyroSensor - %s, flags = %d, mEnabled = %d, data_fd = %d",
                         __FUNCTION__, flags, mEnabled, data_fd);

    if (mHandle == NULL)
        return -1;

    if ((flags != mEnabled) && (flags == 0)) {
        ret = psh_stop_streaming(mHandle);
        if (ret != 0) {
            E("AccelSensor - %s, failed to stop streaming, ret = %d",
                                                  __FUNCTION__, ret);
            return -1;
        }
    }

    mEnabled = flags;
    return 0;
}

#define GYRO_MIN_DELAY 10000000

int GyroSensor::setDelay(int32_t handle, int64_t delay_ns)
{
    int fd, data_rate, ret;
    unsigned long delay_ms;

    D("GyroSensor::%s, delay_ns=%lld", __FUNCTION__, delay_ns);

    if (mHandle == NULL)
	return -1;

    if (mEnabled == 0)
        return 0;

    if (delay_ns < GYRO_MIN_DELAY)
        delay_ns = GYRO_MIN_DELAY;

    delay_ms = delay_ns / 1000000;
    data_rate = 1000 / delay_ms;

    last_timestamp = getTimestamp();

    ret = psh_start_streaming(mHandle, data_rate, 0);
    if (ret != 0) {
        E("psh_start_streaming failed, ret is %d", ret);
        return -1;
    }

    mEnabled = 1;

    return 0;
}

float processRawData(int value)
{
    return (float)value * CONVERT_GYRO;
}

int GyroSensor::readEvents(sensors_event_t* data, int count)
{
    int size, numEventReceived = 0;
    char buf[512];
    struct gyro_raw_data *p_gyro_raw_data;
    int unit_size = sizeof(struct gyro_raw_data);
    int64_t current_timestamp;
    int64_t step;

    D("GyroSensor::%s", __FUNCTION__);

    if (count < 1)
        return -EINVAL;

    if (mHandle == NULL)
        return 0;

    if ((unit_size * count) <= 512)
        size = unit_size * count;
    else
        size = (512 / unit_size) * unit_size;

    size = read(data_fd, buf, size);
    count = size / unit_size;

    current_timestamp = getTimestamp();
    step = (current_timestamp - last_timestamp) / count;

    char *p = buf;
    p_gyro_raw_data = (struct gyro_raw_data *)buf;

    while (size > 0) {
        mPendingEvent.data[0] = processRawData(p_gyro_raw_data->x);
        mPendingEvent.data[1] = processRawData(p_gyro_raw_data->y);
        mPendingEvent.data[2] = processRawData(p_gyro_raw_data->z);

        mPendingEvent.timestamp = last_timestamp + step * (numEventReceived + 1);

        if (mEnabled == 1) {
           *data++ = mPendingEvent;
            numEventReceived++;
        }

        size = size - unit_size;
        p = p + unit_size;
        p_gyro_raw_data = (struct gyro_raw_data *)p;

    }

    D("GyroSensor::%s, numEventReceived is %d", __FUNCTION__,
                                           numEventReceived);

    last_timestamp = current_timestamp;

    return numEventReceived;
}
