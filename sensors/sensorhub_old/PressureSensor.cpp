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

#include "PressureSensor.h"

/*****************************************************************************/

PressureSensor::PressureSensor()
    : SensorBase("pressure"),
      mEnabled(0),
      mHasPendingEvent(false)
{
    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = SENSORS_HANDLE_PRESSURE;
    mPendingEvent.type = SENSOR_TYPE_PRESSURE;
    memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));
}

PressureSensor::PressureSensor(int handle)
    : SensorBase("pressure", handle),
      mEnabled(0),
      mHasPendingEvent(false)
{
    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = idHandle;
    mPendingEvent.type = SENSOR_TYPE_PRESSURE;
    memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));
}

PressureSensor::~PressureSensor() {
    if (mEnabled)
        enable(0, 0);
}

int PressureSensor::setInitialState() {
    return 0;
}

int PressureSensor::enable(int32_t handle, int en) {
    int flags = en ? 1 : 0;
    int ret;

    D("PressureSensor - %s, flags = %d, mEnabled = %d, data_fd = %d",
                                __FUNCTION__, flags, mEnabled, data_fd);

    if (mHandle == NULL)
        return -1;

    if ((flags != mEnabled) && (flags == 0)) {
        ret = psh_stop_streaming(mHandle);
        if (ret != 0) {
            E("PressureSensor - %s, failed to stop streaming, ret = %d",
                                                     __FUNCTION__, ret);
            return -1;
        }
    }

    mEnabled = flags;
    return 0;
}

#define BARO_MIN_DELAY 20000000
int PressureSensor::setDelay(int32_t handle, int64_t delay_ns)
{
    int fd, ms, data_rate, ret;
    char buf[10] = { 0 };

    LOGD("PressureSensor: %s delay_ns=%lld", __FUNCTION__, delay_ns);

    if (mHandle == NULL)
        return -1;

    if (mEnabled == 0)
        return 0;

    if (delay_ns < BARO_MIN_DELAY)
        delay_ns = BARO_MIN_DELAY;

    ms = delay_ns / 1000000;
    data_rate = 1000 / ms;

    last_timestamp = getTimestamp();

    ret = psh_start_streaming(mHandle, data_rate, 0);
    if (ret != 0) {
        E("psh_start_streaming failed, ret is %d", ret);
        return -1;
    }

    mEnabled = 1;

    return 0;
}

bool PressureSensor::hasPendingEvents() const {
    return mHasPendingEvent;
}

int PressureSensor::readEvents(sensors_event_t* data, int count)
{
    static unsigned long D1 = 0;
    static unsigned long D2 = 0;
    static unsigned int coef[8] = { 0 };

    int size, numEventReceived = 0;
    char buf[512];
    struct baro_raw_data *p_baro_raw_data;
    int unit_size = sizeof(struct baro_raw_data);
    int64_t current_timestamp;
    int64_t step;

    D("PressureSensor::%s", __FUNCTION__);

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
    p_baro_raw_data = (struct baro_raw_data *)buf;

    while (size > 0) {
        mPendingEvent.pressure = p_baro_raw_data->p / 1000.0;
        mPendingEvent.timestamp = last_timestamp + step * (numEventReceived + 1);

        if (mEnabled == 1) {
           *data++ = mPendingEvent;
            numEventReceived++;
        }

        size = size - unit_size;
        p = p + unit_size;
        p_baro_raw_data = (struct baro_raw_data *)p;
    }

    D("PressureSensor::%s, numEventReceived is %d", __FUNCTION__,
                                                numEventReceived);

    last_timestamp = current_timestamp;

    return numEventReceived;
}
