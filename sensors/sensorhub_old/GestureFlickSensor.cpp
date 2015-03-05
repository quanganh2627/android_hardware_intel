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

#include "GestureFlickSensor.h"

/*****************************************************************************/

#undef LOG_TAG
#define LOG_TAG "GestureFlickSensor"

#define GF_SAMPLE_RATE  20      /* gesture flick sampling rate, Hz */
#define GF_BUF_DELAY    0       /* gesture flick buffer delay, ms */
#define PSH_SESSION_NOT_OPENED NULL

typedef enum { /* definition flick gestures value */
    NO_FLICK = 0,
    LEFT_FLICK,
    RIGHT_FLICK,
    UP_FLICK,
    DOWN_FLICK,
    LEFT_TWICE,
    RIGHT_TWICE,
    FLICK_GESTURES_MAX = 0x7FFFFFFF
} FlickGestures;

GestureFlickSensor::GestureFlickSensor()
    : SensorBase("gestureflick"),
      mEnabled(0),
      mHasPendingEvent(false)
{
    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = SENSORS_HANDLE_GESTURE_FLICK;
    mPendingEvent.type = SENSOR_TYPE_GESTURE_FLICK;
    memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));
}

GestureFlickSensor::~GestureFlickSensor()
{
    LOGI("~GestureFlickSensor %d\n", mEnabled);
    if (mEnabled == 1)
        enable(0, 0);
}

int GestureFlickSensor::enable(int32_t handle, int en)
{
    int flags = en ? 1 : 0;
    int ret;

    LOGI("GestureFlickSensor - %s - enable=%d", __FUNCTION__, en);

    if (mHandle == PSH_SESSION_NOT_OPENED)
        return -1;

    if (mEnabled == flags)
        return 0;

    if (flags == 0) {
        ret = psh_stop_streaming(mHandle);
        if (ret != 0) {
            E("GestureFlickSensor - %s, failed to stop streaming, ret = %d",
                                                     __FUNCTION__, ret);
            return -1;
        }
        LOGI("psh_stop_streaming succeeded");
    } else if (flags == 1) {
        ret = psh_start_streaming(mHandle, GF_SAMPLE_RATE, GF_BUF_DELAY);
        if (ret != 0) {
            E("GestureFlickSensor - %s, failed to start streaming, ret = %d",
                                                     __FUNCTION__, ret);
            return -1;
        }
        LOGI("psh_start_streaming succeeded!");
     }

     mEnabled = flags;
     return 0;
}

int GestureFlickSensor::setDelay(int32_t handle, int64_t ns)
{
    LOGI("setDelay - %s - %lld", __FUNCTION__, ns);
    return 0;
}

bool GestureFlickSensor::hasPendingEvents() const
{
    return mHasPendingEvent;
}

int GestureFlickSensor::getGestureFlickEvent(struct gesture_flick_data* data) const
{
    switch ((FlickGestures)data->flick) {
    case LEFT_FLICK:
        return SENSOR_EVENT_TYPE_GESTURE_LEFT_FLICK;
    case RIGHT_FLICK:
        return SENSOR_EVENT_TYPE_GESTURE_RIGHT_FLICK;
    case LEFT_TWICE:
        return SENSOR_EVENT_TYPE_GESTURE_LEFT_FLICK_TWICE;
    case RIGHT_TWICE:
        return SENSOR_EVENT_TYPE_GESTURE_RIGHT_FLICK_TWICE;
    case UP_FLICK:
        return SENSOR_EVENT_TYPE_GESTURE_UP_FLICK;
    case DOWN_FLICK:
        return SENSOR_EVENT_TYPE_GESTURE_DOWN_FLICK;
    case NO_FLICK:
    default:
        return SENSOR_EVENT_TYPE_GESTURE_NO_FLICK;
    }
    return SENSOR_EVENT_TYPE_GESTURE_NO_FLICK;
}

int GestureFlickSensor::readEvents(sensors_event_t* data, int count)
{
    int size, numEventReceived = 0;
    char buf[512];
    int unit_size = sizeof(struct gesture_flick_data);

    LOGI("GestureFlickSensor - %s", __FUNCTION__);
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
    struct gesture_flick_data *p_gesture_data;
    while (size > 0) {
        p_gesture_data = (struct gesture_flick_data *)p;
        mPendingEvent.data[0] = (static_cast<float> (getGestureFlickEvent(p_gesture_data)));

        mPendingEvent.timestamp = getTimestamp();

        if (mEnabled == 1) {
           *data++ = mPendingEvent;
            numEventReceived++;
        }

        size = size - unit_size;
        p = p + unit_size;
    }

    LOGI("GestureFlickSensor - %s read data %f", __FUNCTION__,
                                mPendingEvent.distance);
    return numEventReceived;
}
