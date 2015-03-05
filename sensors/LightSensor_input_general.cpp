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

#include "LightSensor_input.h"

LightSensor::LightSensor(const sensor_platform_config_t *config)
    : SensorBase(config),
      mEnabled(0),
      mInputReader(4),
      mHasPendingEvent(false),
      inputDataOverrun(0)
{
    if (mConfig->handle != SENSORS_HANDLE_LIGHT)
        E("LightSensor: Incorrect sensor config");

    data_fd = SensorBase::openInputDev(mConfig->name);
    LOGE_IF(data_fd < 0, "can't open light input dev%s", mConfig->name);

    D("LightSensor open light input dev%s", mConfig->name);

    if (!config->priv_data)
        mGlassFactor = 1.0;
    else
        mGlassFactor =
		((union sensor_data_t *)config->priv_data)->light_glass_factor;

    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = SENSORS_HANDLE_LIGHT;
    mPendingEvent.type = SENSOR_TYPE_LIGHT;
    memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));
}

LightSensor::~LightSensor()
{
    if (mEnabled) {
        enable(0, 0);
    }
}

int LightSensor::enable(int32_t handle, int en)
{
    unsigned int flags = en ? 1 : 0;
    int fd;
    char buf[2] = { 0 };

    D("LightSensor -%s, flags = %d, mEnabled = %d", __func__, flags, mEnabled);
    if (flags == mEnabled)
        return 0;
    fd = open(mConfig->activate_path, O_RDWR);
    if (fd < 0) {
        E("LightSensor %s - open %s failed, errno=%d", __func__,
                                                mConfig->activate_path, errno);
        return -1;
    }

    if (flags) {
       buf[0] = '1';
    } else {
       buf[0] = '0';
    }
    int ret = write(fd, buf, sizeof(buf));
    close(fd);

    if (ret != sizeof(buf)) {
        E("LightSensor %s - write %s failed, errno=%d",
                __func__, mConfig->activate_path, errno);
        return -1;
    }
    mEnabled = flags;
    return 0;
}

bool LightSensor::hasPendingEvents() const
{
    return mHasPendingEvent;
}

int LightSensor::readEvents(sensors_event_t* data, int count)
{
    if (count < 1)
        return -EINVAL;

    ssize_t n = mInputReader.fill(data_fd);
    if (n < 0)
        return n;

    int numEventReceived = 0;
    input_event const* event;

    while (count && mInputReader.readEvent(&event)) {
        int type = event->type;

        D("LightSensor:%s, type = %d, code = %d, value = %d, count = %d",
                           __func__, type, event->code, event->value, count);

        if (type == EV_ABS && !inputDataOverrun) {
            float value = event->value;
            mPendingEvent.light = value * mGlassFactor;
        } else if (type == EV_SYN) {
            mPendingEvent.timestamp = timevalToNano(event->time);
            if (event->code == SYN_DROPPED) {
                LOGE("LightSensor: input event overrun, dropped event:drop");
                inputDataOverrun = 1;
            } else if (inputDataOverrun) {
                LOGE("LightSensor: input event overrun, dropped event:sync");
                inputDataOverrun = 0;
            } else {
                D("LightSensor::%s, in type = EV_SYN, mEnabled = %d", __func__, mEnabled);
                if (mEnabled) {
                    *data++ = mPendingEvent;
                    count--;
                    numEventReceived++;
                }
            }
        } else {
            LOGE("LightSensor: unknown event (type=%d, code=%d)", type, event->code);
        }
        mInputReader.next();
    }

    D("LightSensor::%s, return numEventReceived= %d", __func__, numEventReceived);
    return numEventReceived;
}
