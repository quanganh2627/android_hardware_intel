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

#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/select.h>
#include <dlfcn.h>
#include <cutils/log.h>

#include "MagneticSensor.h"

#define HMC_SENSOR_DATA_NAME	"compass"

MagneticSensor::MagneticSensor()
: SensorBase(HMC_SENSOR_DATA_NAME),
      mEnabled(0)
{
    D("add: magnetic Sensor");
    mMagneticEvent.version = sizeof(sensors_event_t);
    mMagneticEvent.sensor = idHandle;
    mMagneticEvent.type = SENSOR_TYPE_MAGNETIC_FIELD;
    mMagneticEvent.magnetic.status = SENSOR_STATUS_ACCURACY_LOW;
}

MagneticSensor::MagneticSensor(int handle)
: SensorBase(HMC_SENSOR_DATA_NAME, handle),
      mEnabled(0)
{
    D("add: magnetic Sensor");
    mMagneticEvent.version = sizeof(sensors_event_t);
    mMagneticEvent.sensor = idHandle;
    mMagneticEvent.type = SENSOR_TYPE_MAGNETIC_FIELD;
    mMagneticEvent.magnetic.status = SENSOR_STATUS_ACCURACY_LOW;
}

MagneticSensor::~MagneticSensor()
{
    if (mEnabled)
        enable(0, 0);
}

int MagneticSensor::enable(int32_t handle, int en)
{
    unsigned int flags = en ? 1 : 0;
    char buf[128];
    int ret;
    D("MagneticSensor - %s, flags = %d, mEnabled = %d", __FUNCTION__, flags,
                                                             mEnabled);

    if (mHandle == NULL)
        return -1;

    if ((flags != mEnabled) && (flags == 0)) {
        ret = psh_stop_streaming(mHandle);
        if (ret != 0) {
            E("CompSensor - %s, failed to stop streaming, ret = %d",
                                                 __FUNCTION__, ret);
            return -1;
        }
    }

    mEnabled = flags;
    return 0;
}

#define COMPASS_MIN_DELAY 10000000

int MagneticSensor::setDelay(int32_t handle, int64_t ns)
{
    int fd, ms, data_rate = 0, ret;

    D("%s setDelay ns = %lld\n", __func__, ns);

    if (mHandle == NULL)
        return -1;

    if (mEnabled == 0)
        return 0;

    if (ns < COMPASS_MIN_DELAY)
        ns = COMPASS_MIN_DELAY;

    ms = ns / 1000 / 1000;

    if (ms != 0)
        data_rate = 1000/ms;
    D("psh_start_streaming(), data_rate is %d", data_rate);

    if (data_rate == 0)
        return -1;

    last_timestamp = getTimestamp();

    ret = psh_start_streaming(mHandle, data_rate, 0);

    if (ret != 0) {
        E("psh_start_streaming failed, ret is %d", ret);
        return -1;
    }

    return 0;
}

int MagneticSensor::readEvents(sensors_event_t* data, int count)
{
    int size, numEventReceived = 0;
    char buf[512];
    struct compass_raw_data *p_compass_raw_data;
    int unit_size = sizeof(struct compass_raw_data);
    int64_t current_timestamp;
    int64_t step;

    if (count < 1)
        return -EINVAL;

    if (mHandle == NULL)
        return 0;

    D("%s count = %d ", __func__, count);

    if ((unit_size * count) <= 512)
        size = unit_size * count;
    else
        size = (512 / unit_size) * unit_size;

    size = read(data_fd, buf, size);
    count = size / unit_size;

    current_timestamp = getTimestamp();
    step = (current_timestamp - last_timestamp) / count;

    char *p = buf;
    p_compass_raw_data = (struct compass_raw_data *)buf;

    while (size > 0) {
        processEvent(EVENT_TYPE_M_O_X, p_compass_raw_data->x);
        processEvent(EVENT_TYPE_M_O_Y, p_compass_raw_data->y);
        processEvent(EVENT_TYPE_M_O_Z, p_compass_raw_data->z);

        if (p_compass_raw_data->accuracy)
            mMagneticEvent.magnetic.status = SENSOR_STATUS_ACCURACY_HIGH;
        else
            mMagneticEvent.magnetic.status = SENSOR_STATUS_ACCURACY_LOW;

        mMagneticEvent.timestamp = last_timestamp + step * (numEventReceived + 1);

        if (mEnabled) {
            *data++ = mMagneticEvent;
            numEventReceived++;
        }

        size = size - unit_size;
        p = p + unit_size;
        p_compass_raw_data = (struct compass_raw_data *)p;
    }

    last_timestamp = current_timestamp;

    return numEventReceived;
}

void MagneticSensor::processEvent(int code, float value)
{
    float data;

    data = value/10;
    switch (code) {
        case EVENT_TYPE_M_O_X:
            mMagneticEvent.magnetic.x = data;
            break;
        case EVENT_TYPE_M_O_Y:
            mMagneticEvent.magnetic.y = data;
            break;
        case EVENT_TYPE_M_O_Z:
            mMagneticEvent.magnetic.z = data;
	    break;
        default:
            break;
    }
    return;
}
