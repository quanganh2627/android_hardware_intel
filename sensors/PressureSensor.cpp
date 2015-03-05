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

#include "PressureSensor.h"

PressureSensor::PressureSensor(const sensor_platform_config_t *config)
    : SensorBase(config),
      mEnabled(0),
      mInputReader(32),
      mHasPendingEvent(false),
      inputDataOverrun(0)
{
    if (mConfig->handle != SENSORS_HANDLE_PRESSURE)
        E("PressureSensor: Incorrect sensor config");

    data_fd = SensorBase::openInputDev(mConfig->name);
    LOGE_IF(data_fd < 0, "can't open pressure input dev");

    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = SENSORS_HANDLE_PRESSURE;
    mPendingEvent.type = SENSOR_TYPE_PRESSURE;
    memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));
}

PressureSensor::~PressureSensor()
{
    if (mEnabled)
        enable(0, 0);
}

int PressureSensor::enable(int32_t handle, int en)
{
    int flags = en ? 1 : 0;

    if (flags != mEnabled) {
        int fd;
        fd = open(mConfig->activate_path, O_RDWR);
        if (fd >= 0) {
            char buf[2];
            buf[1] = 0;
            if (flags) {
                buf[0] = '1';
            } else {
                buf[0] = '0';
            }
            int ret = write(fd, buf, sizeof(buf));
            close(fd);
            if (ret == sizeof(buf)) {
                mEnabled = flags;
                return 0;
            }
        }
        return -1;
    }
    return 0;
}

int PressureSensor::setDelay(int32_t handle, int64_t delay_ns)
{

    int fd, ms;
    char buf[10] = { 0 };

    D("PressureSensor: %s delay_ns=%lld", __FUNCTION__, delay_ns);
    if ((fd = open(mConfig->poll_path, O_RDWR)) < 0) {
        E("PressureSensor: Open %s failed!", mConfig->poll_path);
        return -1;
    }

    ms = delay_ns / 1000000;
    snprintf(buf, sizeof(buf), "%d", ms);
    write(fd, buf, sizeof(buf));
    close(fd);

    return 0;
}

bool PressureSensor::hasPendingEvents() const
{
    return mHasPendingEvent;
}

int PressureSensor::readEvents(sensors_event_t* data, int count)
{
    unsigned long pressure = 0;

    if (count < 1)
        return -EINVAL;

    ssize_t n = mInputReader.fill(data_fd);
    if (n < 0)
        return n;

    int numEventReceived = 0;
    input_event const* event;

    while (count && mInputReader.readEvent(&event)) {
        int type = event->type;
        if ((type == EV_ABS || type == EV_REL) && !inputDataOverrun) {
           switch (event->code) {
            case EVENT_TYPE_PRESSURE:
                pressure = event->value;
                break;
            case EVENT_TYPE_TEMPERATURE:
                /* ignore temperature data from sensor */
                break;
            default:
                E("PressureSensor: unknown event (type=%d, code=%d)",
                     type, event->code);
            }
        } else if (type == EV_SYN) {
            /* drop input event overrun data */
            if (event->code == SYN_DROPPED) {
                E("PressureSensor: input event overrun, dropped event:drop");
                inputDataOverrun = 1;
            } else if (inputDataOverrun) {
                E("PressureSensor: input event overrun, dropped event:sync");
                inputDataOverrun = 0;
            } else {
                mPendingEvent.pressure = (float)pressure / 4096;
                mPendingEvent.timestamp = timevalToNano(event->time);
                if (mEnabled) {
                    *data++ = mPendingEvent;
                    count -= 1;
                    numEventReceived += 1;
                }
            }
        } else {
            E("PressureSensor: unknown event (type=%d, code=%d)",
                 type, event->code);
        }
        mInputReader.next();
    }

    return numEventReceived;
}
