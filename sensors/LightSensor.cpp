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

#include "LightSensor.h"

LightSensor::LightSensor(const sensor_platform_config_t *config)
    : SensorBase(config),
      mEnabled(0),
      mHasPendingEvent(false)
{
    if (mConfig->handle != SENSORS_HANDLE_LIGHT)
        E("LightSensor: Incorrect sensor config");

    data_fd = open(mConfig->data_path, O_RDONLY);
    LOGE_IF(data_fd < 0, "can't open %s", mConfig->data_path);

    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = SENSORS_HANDLE_LIGHT;
    mPendingEvent.type = SENSOR_TYPE_LIGHT;
    memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));
}

LightSensor::~LightSensor()
{
    if (mEnabled)
        enable(0, 0);
}

int LightSensor::enable(int32_t handle, int en)
{
    D("LightSensor - %s - enable=%d", __func__, en);

    if (ioctl(data_fd, 0, en)) {
        E("LightSensor: ioctl enable failed!");
        return -1;
    }
    return 0;
}

bool LightSensor::hasPendingEvents() const
{
    return mHasPendingEvent;
}

int LightSensor::readEvents(sensors_event_t* data, int count)
{
    unsigned int val = -1;
    struct timespec t;

    D("LightSensor - %s", __func__);

    if (count < 1 || data == NULL || data_fd < 0)
        return -EINVAL;

    *data = mPendingEvent;
    data->timestamp = getTimestamp();
    read(data_fd, &val, sizeof(unsigned int));
    data->light = (float)val;
    data->light = data->light < mConfig->range[1] ? data->light : mConfig->range[1];
    D("LightSensor - read data val = %f ",data->light);

    return 1;
}
