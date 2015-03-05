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

#include "GyroSensor.h"

#define CONVERT_GYRO    ((70.0f / 1000.0f) * ((float)M_PI / 180.0f))

GyroSensor::GyroSensor(const sensor_platform_config_t *config)
    : SensorBase(config),
      mEnabled(0),
      mInputReader(4),
      mHasPendingEvent(false),
      inputDataOverrun(0)

{
    if (mConfig->handle != SENSORS_HANDLE_GYROSCOPE)
        E("GyroSensor: Incorrect sensor config");

    data_fd = SensorBase::openInputDev(mConfig->name);
    LOGE_IF(data_fd < 0, "can't open gyro input dev");

    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = SENSORS_HANDLE_GYROSCOPE;
    mPendingEvent.type = SENSOR_TYPE_GYROSCOPE;
    memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));
    mPendingEvent.gyro.status = SENSOR_STATUS_ACCURACY_HIGH;
}

GyroSensor::~GyroSensor()
{
    if (mEnabled)
        enable(0, 0);

    if (conf_fd > -1)
        close(conf_fd);
}

int GyroSensor::enable(int32_t handle, int en)
{
    int fd, ret = 0;
    int flags = en ? 1 : 0;
    char buf[50];

    D("GyroSensor: en %d", en);

    if (flags == mEnabled)
        return 0;

    if ((fd = SensorBase::openFile(mConfig->activate_path, O_RDWR)) < 0) {
        E("GyroSensor: Open device file failed, possible path: %s!",
                                                mConfig->activate_path);
        return -1;
    }

    if (flags == 1 && mEnabled == 0) {
        conf_fd = open(mConfig->config_path, O_RDONLY);
        memset(mCalEvent.data, 0, sizeof(mCalEvent.data));
        if (conf_fd > -1) {
            ret = pread(conf_fd, buf, sizeof(buf), 0);
            if (ret > 0) {
                ret = sscanf(buf, "%f %f %f\n",
                      &mCalEvent.data[0], &mCalEvent.data[1], &mCalEvent.data[2]);
                if (ret != 3)
                    mCalEvent.data[0] = mCalEvent.data[1] = mCalEvent.data[2] = 0;
                D("GyroSensor cal value = [%f, %f, %f]\n", mCalEvent.data[0],
                    mCalEvent.data[1], mCalEvent.data[2]);
            }
            close(conf_fd);
        }
    }

    buf[1] = 0;
    buf[0] = flags ? '1' : '0';
    ret = write(fd, buf, sizeof(buf));
    if (ret > 0) {
        mEnabled = flags;
        ret = 0;
    }
    close(fd);

    return ret;
}

bool GyroSensor::hasPendingEvents() const
{
    return mHasPendingEvent;
}

int GyroSensor::setDelay(int32_t handle, int64_t delay_ns)
{
    int fd;
    unsigned long delay_ms;
    char buf[10] = { 0 };


    if ((fd = SensorBase::openFile(mConfig->poll_path, O_RDWR)) < 0) {
        E("GyroSensor: Open device file failed, possible path: %s!",
                                                mConfig->poll_path);
        return -1;
    }

    if (delay_ns < mConfig->min_delay)
        delay_ns = mConfig->min_delay;

    D("GyroSensor::%s, delay_ns=%lld", __func__, delay_ns);
    delay_ms = delay_ns / 1000000;
    snprintf(buf, sizeof(buf), "%ld", delay_ms);
    write(fd, buf, sizeof(buf));
    close(fd);

    return 0;
}

float GyroSensor::processRawData(int value, float scale)
{
    return (float)value * CONVERT_GYRO * scale;
}

int GyroSensor::readEvents(sensors_event_t* data, int count)
{
    D("GyroSensor::%s", __func__);

    if (count < 1)
        return -EINVAL;

    ssize_t n = mInputReader.fill(data_fd);
    if (n < 0)
        return n;

    int numEventReceived = 0;
    input_event const* event;

    while (count && mInputReader.readEvent(&event)) {
        int type = event->type;
        if (type == EV_REL && !inputDataOverrun) {
            float value = event->value;
           if (event->code == REL_X) {
                mPendingEvent.data[mConfig->mapper[AXIS_X]] =
                    processRawData(value, mConfig->scale[AXIS_X])
                    - mCalEvent.data[mConfig->mapper[AXIS_X]];
            } else if (event->code == REL_Y) {
                mPendingEvent.data[mConfig->mapper[AXIS_Y]] =
                    processRawData(value, mConfig->scale[AXIS_Y])
                    - mCalEvent.data[mConfig->mapper[AXIS_Y]];
            } else if (event->code == REL_Z) {
                mPendingEvent.data[mConfig->mapper[AXIS_Z]] =
                    processRawData(value, mConfig->scale[AXIS_Z])
                    - mCalEvent.data[mConfig->mapper[AXIS_Z]];
            }
        } else if (type == EV_SYN) {
            /* drop input event overrun data */
            if (event->code == SYN_DROPPED) {
                E("GyroSensor: input event overrun, dropped event:drop");
                inputDataOverrun = 1;
            } else if (inputDataOverrun) {
                E("GyroSensor: input event overrun, dropped event:sync");
                inputDataOverrun = 0;
            } else {
                mPendingEvent.timestamp = timevalToNano(event->time);
                if (mEnabled) {
                    *data++ = mPendingEvent;
                    count--;
                    numEventReceived++;
                    D("gyro = [%f, %f, %f]\n", mPendingEvent.data[0],
                        mPendingEvent.data[1], mPendingEvent.data[2]);
                }
            }
        } else {
            E("GyroSensor: unknown event (type=%d, code=%d)",
                    type, event->code);
        }
        mInputReader.next();
    }

    return numEventReceived;
}
