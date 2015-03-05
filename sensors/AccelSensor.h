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

#ifndef ANDROID_ACCEL_SENSOR_H
#define ANDROID_ACCEL_SENSOR_H

#include "SensorBase.h"

class AccelSensor : public SensorBase {
public:
    AccelSensor(const sensor_platform_config_t *config);
    virtual ~AccelSensor();

    virtual int setDelay(int32_t handle, int64_t ns);
    virtual int enable(int32_t handle, int enabled);
    virtual int readEvents(sensors_event_t* data, int count);

private:
    uint32_t mEnabled;
    InputEventCircularReader mInputReader;
    sensors_event_t mPendingEvent;
    int inputDataOverrun;
};

#endif  // ANDROID_AKM_SENSOR_H
