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

#ifndef ANDROID_SHAKE_SENSOR_H
#define ANDROID_SHAKE_SENSOR_H

#include <stdint.h>
#include <errno.h>
#include <sys/cdefs.h>
#include <sys/types.h>

#include "SensorBase.h"

/*****************************************************************************/

/*
 * Author: Zheng Huan <huan.zheng@intel.com> Wang Xun <xun.a.wang@intel.com>
 * Group: PSI, MCG
 * Virtual sensor that interacts with PSH to report shake events to sensor manager
 * The following gesture flick events will be reported
 *   Shake
 */

class ShakeSensor : public SensorBase {
    int mEnabled;
    sensors_event_t mPendingEvent;
    bool mHasPendingEvent;

public:
    ShakeSensor();
    virtual ~ShakeSensor();
    virtual int readEvents(sensors_event_t* data, int count);
    virtual bool hasPendingEvents() const;
    virtual int enable(int32_t handle, int enabled);
    virtual int setDelay(int32_t handle, int64_t ns);
private:
    int getShakeEvent(struct shaking_data* data) const;
};

/*****************************************************************************/
#endif  // ANDROID_SHAKE_SENSOR_H
