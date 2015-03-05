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

#include "LightSensor_apds9300.h"

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

float LightSensor::getLux(unsigned int clear, unsigned int ir)
{
    double luxValue=0.0;
    double ch1ch0Ratio;
    unsigned int ch0Data, ch1Data, chScale;

    chScale = 1;
    if(alsScale == 5047)
        chScale =  ((322*1024)/11);
    else if(alsScale == 37177)
        chScale = ((322*1024)/81);
    else
        chScale = (1*1024);

    if(alsGain == 1)
        chScale = chScale << 4;

    ch0Data = (clear * chScale)/1024;
    ch1Data = (ir * chScale)/1024;

    ch1ch0Ratio = (double)ch1Data / (double)ch0Data;
    if(ch1ch0Ratio > 1.30)
        luxValue = 0.0;
    else if(ch1ch0Ratio > 0.80)
        luxValue = (0.00338 * (double)ch0Data) - (0.00260 * (double)ch1Data);
    else if(ch1ch0Ratio > 0.65)
        luxValue = (0.0157 * (double)ch0Data) - (0.0180 * (double)ch1Data);
    else if(ch1ch0Ratio > 0.52)
        luxValue = (0.0229 * (double)ch0Data) - (0.0291 * (double)ch1Data);
    else if(ch1ch0Ratio > 0)
        luxValue = (0.0315 * (double)ch0Data) - (0.0593 * (double)ch0Data * pow(ch1ch0Ratio, 1.4));
    else
        luxValue = 0.0;

    return (float)(luxValue * glassFactor);
}

int LightSensor::readEvents(sensors_event_t* data, int count)
{
    unsigned int val = -1;
    unsigned int buf[2];
    struct timespec t;

    D("LightSensor - %s", __func__);

    if (count < 1 || data == NULL || data_fd < 0)
        return -EINVAL;

    *data = mPendingEvent;
    data->timestamp = getTimestamp();
    read(data_fd, buf, sizeof(unsigned int) * 2);
    data->light = getLux(buf[0], buf[1]);
    data->light = data->light < mConfig->range[1] ? data->light : mConfig->range[1];
    D("LightSensor - read data val = %f ",data->light);

    return 1;
}
