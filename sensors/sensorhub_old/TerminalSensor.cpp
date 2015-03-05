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

#include "TerminalSensor.h"

/*****************************************************************************/

#undef LOG_TAG
#define LOG_TAG "TerminalSensor"

#define TERM_RATE 20
#define TERM_DELAY (1000/TERM_RATE)

#define FACE_UNKNOWN 0
#define FACE_UP 1
#define FACE_DOWN 2

#define ORIENTATION_UNKNOWN 0
#define HORIZONTAL_UP 1
#define HORIZONTAL_DOWN 2
#define PORTRAIT_UP 3
#define PORTRAIT_DOWN 4

#define PSH_SESSION_NOT_OPENED NULL

TerminalSensor::TerminalSensor()
    : SensorBase("terminal"),
      mEnabled(0),
      mHasPendingEvent(false)
{
    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = SENSORS_HANDLE_TERMINAL;
    mPendingEvent.type = SENSOR_TYPE_TERMINAL;
    memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));
}

TerminalSensor::~TerminalSensor()
{
    LOGI("~~~TerminalSensor %d\n", mEnabled);
    if (mEnabled == 1)
        enable(0, 0);
}

int TerminalSensor::enable(int32_t handle, int en)
{
    int flags = en ? 1 : 0;
    int ret;

    LOGI("TerminalSensor - %s - enable=%d", __FUNCTION__, en);

    if (mHandle == PSH_SESSION_NOT_OPENED)
        return -1;

    if (mEnabled == flags)
        return 0;

    if (flags == 0) {
        ret = psh_stop_streaming(mHandle);
        if (ret != 0) {
            E("TerminalSensor - %s, failed to stop streaming, ret = %d",
                                                     __FUNCTION__, ret);
            return -1;
        }
        LOGI("psh_stop_streaming succeeded");
    } else if (flags == 1) {
        ret = psh_start_streaming(mHandle, TERM_RATE, TERM_DELAY);
        if (ret != 0) {
            E("TerminalSensor - %s, failed to start streaming, ret = %d",
                                                     __FUNCTION__, ret);
            return -1;
        }
        LOGI("psh_start_streaming succeeded!");
     }

     mEnabled = flags;
     return 0;
}

int TerminalSensor::setDelay(int32_t handle, int64_t ns)
{
    LOGI("setDelay - %s - %lld", __FUNCTION__, ns);
    return 0;
}

bool TerminalSensor::hasPendingEvents() const
{
    return mHasPendingEvent;
}

int TerminalSensor::getTerminalEvent(struct tc_data* data) const
{
    if (data->orien_z != FACE_UNKNOWN
        && data->orien_xy != ORIENTATION_UNKNOWN) {
        // This shouldn't happen
        return SENSOR_EVENT_TYPE_TERMINAL_UNKNOWN;
    }

    if (data->orien_xy == ORIENTATION_UNKNOWN) {
        // Face up/down
        switch (data->orien_z) {
            case FACE_UP:
                return SENSOR_EVENT_TYPE_TERMINAL_FACE_UP;
            case FACE_DOWN:
                return SENSOR_EVENT_TYPE_TERMINAL_FACE_DOWN;
            default:
                return SENSOR_EVENT_TYPE_TERMINAL_UNKNOWN;
        }
    }

    if (data->orien_z == FACE_UNKNOWN) {
        // Orientation up/down
        switch (data->orien_xy) {
            case PORTRAIT_UP:
                return SENSOR_EVENT_TYPE_TERMINAL_PORTRAIT_UP;
            case PORTRAIT_DOWN:
                return SENSOR_EVENT_TYPE_TERMINAL_PORTRAIT_DOWN;
            case HORIZONTAL_UP:
                return SENSOR_EVENT_TYPE_TERMINAL_HORIZONTAL_UP;
            case HORIZONTAL_DOWN:
                return SENSOR_EVENT_TYPE_TERMINAL_HORIZONTAL_DOWN;
            default:
                return SENSOR_EVENT_TYPE_TERMINAL_UNKNOWN;
        }
    }

    // Shouldn't reach here
    return SENSOR_EVENT_TYPE_TERMINAL_UNKNOWN;
}

int TerminalSensor::readEvents(sensors_event_t* data, int count)
{
    int size, numEventReceived = 0;
    char buf[512];
    int unit_size = sizeof(struct tc_data);

    LOGI("TerminalSensor - %s", __FUNCTION__);
    if (count < 1)
        return -EINVAL;

    if (mHandle == PSH_SESSION_NOT_OPENED)
        return 0;

    if ((unit_size * count) <= 512)
        size = unit_size * count;
    else
        size = (512 / unit_size) * unit_size;

    size = read(data_fd, buf, size);

    char *p = buf;
    struct tc_data *p_tc_data;
    while (size > 0) {
        p_tc_data = (struct tc_data *) p;
        mPendingEvent.data[0] = (static_cast<float> (getTerminalEvent(p_tc_data)));

        mPendingEvent.timestamp = getTimestamp();

        if (mEnabled == 1) {
           *data++ = mPendingEvent;
            numEventReceived++;
        }

        size = size - unit_size;
        p = p + unit_size;
    }

    LOGI("TerminalSensor - %s read data %f", __FUNCTION__,
                                mPendingEvent.distance);
    return numEventReceived;
}
