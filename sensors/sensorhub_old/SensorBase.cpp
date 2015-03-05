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
#define LOG_TAG "Sensors"

#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/select.h>

#include <cutils/log.h>

#include <linux/input.h>

#include "SensorBase.h"
#include "sensors.h"

/*****************************************************************************/

SensorBase::SensorBase(
        const char* data_name)
    : data_name(data_name),
      data_fd(-1),
      mHandle(NULL)
{
    if (data_name)
        openSession(data_name);

    if (mHandle)
        data_fd = psh_get_fd(mHandle);
}

SensorBase::SensorBase(const char* data_name, int handle)
    : data_name(data_name),
      data_fd(-1),
      idHandle(handle),
      mHandle(NULL)
{
    if (data_name)
        openSession(data_name);

    if (mHandle)
        data_fd = psh_get_fd(mHandle);
}

SensorBase::~SensorBase() {
    if (mHandle)
        psh_close_session(mHandle);

    data_fd = -1;
}

int SensorBase::getFd() const {
    return data_fd;
}

int SensorBase::setDelay(int32_t handle, int64_t ns) {
    return 0;
}

bool SensorBase::hasPendingEvents() const {
    return false;
}

int64_t SensorBase::getTimestamp() {
    struct timespec t;
    t.tv_sec = t.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return int64_t(t.tv_sec)*1000000000LL + t.tv_nsec;
}

void SensorBase::openSession(const char* inputName) {

    if(!strcmp(inputName, "accel")){
        if (idHandle == SENSORS_HANDLE_ACCELEROMETER)
            mHandle = psh_open_session(SENSOR_ACCELEROMETER);
        else
            mHandle = psh_open_session(SENSOR_ACCELEROMETER_SEC);
    }
    if(!strcmp(inputName, "pressure")){
        if (idHandle == SENSORS_HANDLE_PRESSURE)
            mHandle = psh_open_session(SENSOR_BARO);
        else
            mHandle = psh_open_session(SENSOR_BARO_SEC);
    }
    if(!strcmp(inputName, "compass")){
        if (idHandle == SENSORS_HANDLE_MAGNETIC_FIELD)
            mHandle = psh_open_session(SENSOR_COMP);
        else
            mHandle = psh_open_session(SENSOR_COMP_SEC);
    }
    if(!strcmp(inputName, "gyro")){
        if (idHandle == SENSORS_HANDLE_GYROSCOPE)
            mHandle = psh_open_session(SENSOR_GYRO);
        else
            mHandle = psh_open_session(SENSOR_GYRO_SEC);
    }
    if(!strcmp(inputName, "light")){
        if (idHandle == SENSORS_HANDLE_LIGHT)
            mHandle = psh_open_session(SENSOR_ALS);
        else
            mHandle = psh_open_session(SENSOR_ALS_SEC);
    }
    if(!strcmp(inputName, "proximity")){
        if (idHandle == SENSORS_HANDLE_PROXIMITY)
            mHandle = psh_open_session(SENSOR_PROXIMITY);
        else
            mHandle = psh_open_session(SENSOR_PROXIMITY_SEC);
    }
    if(!strcmp(inputName, "gravity")){
        mHandle = psh_open_session(SENSOR_GRAVITY);
    }
    if(!strcmp(inputName, "linearaccel")){
        mHandle = psh_open_session(SENSOR_LINEAR_ACCEL);
    }
    if(!strcmp(inputName, "rotate")){
        mHandle = psh_open_session(SENSOR_ROTATION_VECTOR);
    }
    if(!strcmp(inputName, "orientation")){
        mHandle = psh_open_session(SENSOR_ORIENTATION);
    }
    if(!strcmp(inputName, "terminal")){
        mHandle = psh_open_session(SENSOR_TC);
    }
    if(!strcmp(inputName, "gestureflick")){
        mHandle = psh_open_session(SENSOR_GESTURE_FLICK);
    }
    if(!strcmp(inputName, "shake")){
        mHandle = psh_open_session(SENSOR_SHAKING);
    }
    if(!strcmp(inputName, "simpletapping")){
        mHandle = psh_open_session(SENSOR_STAP);
    }

    if (mHandle == NULL) {
        E("can't open session for %s", inputName);
    }
}
