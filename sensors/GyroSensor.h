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

#ifndef ANDROID_GYRO_SENSOR_H
#define ANDROID_GYRO_SENSOR_H

#include "SensorBase.h"

class GyroSensor : public SensorBase {
public:
    GyroSensor(const sensor_platform_config_t *config);
    virtual ~GyroSensor();
    virtual int readEvents(sensors_event_t* data, int count);
    virtual bool hasPendingEvents() const;
    virtual int setDelay(int32_t handle, int64_t ns);
    virtual int enable(int32_t handle, int enabled);

private:
    float processRawData(int value, float scale);
    int mEnabled;
    InputEventCircularReader mInputReader;
    sensors_event_t mPendingEvent;
    sensors_event_t mCalEvent;
    bool mHasPendingEvent;
    int inputDataOverrun;
    int conf_fd;
};
#endif  // ANDROID_GYRO_SENSOR_H
