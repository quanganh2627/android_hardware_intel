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

#include "ProximitySensor.h"

/*****************************************************************************/

ProximitySensor::ProximitySensor()
    : SensorBase("proximity"),
      mEnabled(0),
      mHasPendingEvent(false)
{
    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = SENSORS_HANDLE_PROXIMITY;
    mPendingEvent.type = SENSOR_TYPE_PROXIMITY;
    memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));
}

ProximitySensor::ProximitySensor(int handle)
    : SensorBase("proximity", handle),
      mEnabled(0),
      mHasPendingEvent(false)
{
    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = idHandle;
    mPendingEvent.type = SENSOR_TYPE_PROXIMITY;
    memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));
}

ProximitySensor::~ProximitySensor() {
    if (mEnabled)
        enable(0, 0);
}

int ProximitySensor::enable(int32_t, int en) {
    int flags = en ? 1 : 0;
    int ret;

    D("ProximitySensor - %s - enable=%d", __FUNCTION__, en);

    if (mHandle == NULL)
        return -1;

    if (mEnabled == flags)
        return 0;

    if (flags == 0) {
        ret = psh_stop_streaming(mHandle);
        if (ret != 0) {
            E("ProximitySensor - %s, failed to stop streaming, ret = %d",
                                                     __FUNCTION__, ret);
            return -1;
        }
    } else if (flags == 1) {
        ret = psh_start_streaming_with_flag(mHandle, 1, 0, NO_STOP_WHEN_SCREEN_OFF);
        if (ret != 0) {
            E("ProximitySensor - %s, failed to start streaming, ret = %d",
                                                     __FUNCTION__, ret);
            return -1;
        }
     }

     mEnabled = flags;
     return 0;
}

bool ProximitySensor::hasPendingEvents() const {
    return mHasPendingEvent;
}

int ProximitySensor::readEvents(sensors_event_t* data, int count)
{
    int val = -1;
    struct timespec t;
    int size, numEventReceived = 0;
    char buf[512];
    struct ps_phy_data *p_ps_phy_data;
    int unit_size = sizeof(struct ps_phy_data);

    D("ProximitySensor - %s", __FUNCTION__);
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
    p_ps_phy_data = (struct ps_phy_data *)buf;

    while (size > 0) {
        if (p_ps_phy_data->near == 1)
            mPendingEvent.distance = (float)0;
        else
            mPendingEvent.distance = (float)5;

        mPendingEvent.timestamp = getTimestamp();

        if (mEnabled == 1) {
           *data++ = mPendingEvent;
            numEventReceived++;
        }

        size = size - unit_size;
        p = p + unit_size;
        p_ps_phy_data = (struct ps_phy_data *)p;
    }

    D("ProximitySensor - %s read data %f", __FUNCTION__,
                                mPendingEvent.distance);
    return numEventReceived;
}
