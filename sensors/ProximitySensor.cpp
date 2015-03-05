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

#include "ProximitySensor.h"

ProximitySensor::ProximitySensor(const sensor_platform_config_t *config)
    : SensorBase(config),
      mEnabled(0),
      mHasPendingEvent(false)
{
    if (mConfig->handle != SENSORS_HANDLE_PROXIMITY)
        E("ProximitySensor: Incorrect sensor config");

    data_fd = open(mConfig->data_path, O_RDONLY);
    LOGE_IF(data_fd < 0, "can't open %s", mConfig->data_path);

    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = SENSORS_HANDLE_PROXIMITY;
    mPendingEvent.type = SENSOR_TYPE_PROXIMITY;
    memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));
}

ProximitySensor::~ProximitySensor()
{
    if (mEnabled)
        enable(0, 0);
}

int ProximitySensor::enable(int32_t, int en)
{
    D("ProximitySensor - %s - enable=%d", __func__, en);

    if (ioctl(data_fd, 0, en)) {
        E("ProximitySensor: ioctl set failed!");
        return -1;
    }
    return 0;
}

bool ProximitySensor::hasPendingEvents() const {
    return mHasPendingEvent;
}

int ProximitySensor::readEvents(sensors_event_t* data, int count)
{
    int val = -1;
    struct timespec t;

    D("ProximitySensor - %s", __func__);
    if (count < 1 || data == NULL || data_fd < 0)
        return -EINVAL;

    *data = mPendingEvent;
    data->timestamp = getTimestamp();
    read(data_fd, &val, sizeof(int));
    data->distance = (float)(val == 1 ? 0 : 6);
    LOGI("ProximitySensor - read data %f, %s",
            data->distance, data->distance > 0 ? "far" : "near");

    return 1;
}
