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

#ifndef ANDROID_LIGHT_SENSOR_APDS9300_H
#define ANDROID_LIGHT_SENSOR_APDS9300_H

#include "SensorBase.h"

class LightSensor : public SensorBase {
    int mEnabled;
    sensors_event_t mPendingEvent;
    bool mHasPendingEvent;

private:
    float getLux(unsigned int, unsigned int);
    static const int alsScale = 37177;	/* 5047 (13.7ms), 37177 (100ms), 65535 (400ma) */
    static const int alsGain = 16;	/* 1 or 16 */
    static const float glassFactor = 5.94;

public:
    LightSensor(const sensor_platform_config_t *config);
    virtual ~LightSensor();
    virtual int readEvents(sensors_event_t* data, int count);
    virtual bool hasPendingEvents() const;
    virtual int enable(int32_t handle, int enabled);
};

#endif  // ANDROID_LIGHT_SENSOR_APDS9300_H
