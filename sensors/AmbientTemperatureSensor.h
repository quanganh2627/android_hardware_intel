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

#ifndef ANDROID_AMBTEMP_SENSOR_H
#define ANDROID_AMBTEMP_SENSOR_H

#include "SensorBase.h"

#define MAX_SIZE 	128
#define MAX_ROW 	200

class AmbTempSensor : public SensorBase {
public:
    AmbTempSensor(const sensor_platform_config_t *config);
    virtual ~AmbTempSensor();

    virtual int enable(int32_t handle, int enabled);
    virtual int readEvents(sensors_event_t* data, int count);
    virtual int setDelay(int32_t handle, int64_t delay_ns);

private:
    int getTable(const char *config_path);
    float processRawData(int value);
    uint32_t mEnabled;
    sensors_event_t mPendingEvent;
    uint32_t mTempBase;
    uint32_t mTableRow;
    int mTable[MAX_ROW];
};

#endif  // ANDROID_AMBTEMP_SENSOR_H
