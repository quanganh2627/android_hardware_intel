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

#include "LightSensor.h"

/*****************************************************************************/

LightSensor::LightSensor()
    : SensorBase("light"),
      mEnabled(0),
      mHasPendingEvent(false)
{
    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = idHandle;
    mPendingEvent.type = SENSOR_TYPE_LIGHT;
    memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));
}

LightSensor::LightSensor(int handle)
    : SensorBase("light", handle),
      mEnabled(0),
      mHasPendingEvent(false)
{
    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = idHandle;
    mPendingEvent.type = SENSOR_TYPE_LIGHT;
    memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));
}

LightSensor::~LightSensor() {
    if (mEnabled)
        enable(0, 0);
}

int LightSensor::enable(int32_t handle, int en)
{
    int flags = en ? 1 : 0;
    int ret;

    D("LightSensor - %s - enable=%d", __FUNCTION__, en);

    if (mHandle == NULL)
        return -1;

    if (mEnabled == flags)
        return 0;

    if (flags == 0) {
        ret = psh_stop_streaming(mHandle);
        if (ret != 0) {
            E("LightSensor - %s, failed to stop streaming, ret = %d",
                                                     __FUNCTION__, ret);
            return -1;
        }
    } else if (flags == 1) {
        /* data_rate 2 is decided after having discussion with Alek and Dong on Feb 7th, 2012 */
        ret = psh_start_streaming(mHandle, 2, 0);
        if (ret != 0) {
            E("LightSensor - %s, failed to start streaming, ret = %d",
                                                     __FUNCTION__, ret);
            return -1;
        }
    }

    mEnabled = flags;
    return 0;
}

bool LightSensor::hasPendingEvents() const {
    return mHasPendingEvent;
}

int LightSensor::readEvents(sensors_event_t* data, int count)
{
    unsigned int val = -1;
    struct timespec t;

    int size, numEventReceived = 0;
    char buf[512];
    struct als_raw_data *p_als_raw_data;
    int unit_size = sizeof(struct als_raw_data);

    D("LightSensor - %s", __FUNCTION__);
    if (count < 1)
        return -EINVAL;

    if (mHandle == NULL)
        return 0;

    if ((unit_size * count) <= 512)
        size = unit_size * count;
    else
        size = (512 / unit_size) * unit_size;

    size = read(data_fd, buf, size);

    char *p = buf;
    p_als_raw_data = (struct als_raw_data *)buf;

    while (size > 0) {
        mPendingEvent.light = (float)p_als_raw_data->lux/10;
        mPendingEvent.timestamp = getTimestamp();

        if (mEnabled == 1) {
           *data++ = mPendingEvent;
            numEventReceived++;
        }

        size = size - unit_size;
        p = p + unit_size;
        p_als_raw_data = (struct als_raw_data *)p;
    }

    D("LightSensor::%s, numEventReceived is %d", __FUNCTION__,
                                                numEventReceived);

    return numEventReceived;
}
