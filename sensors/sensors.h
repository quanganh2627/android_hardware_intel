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

#ifndef ANDROID_SENSORS_H
#define ANDROID_SENSORS_H

#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <math.h>
#include <dirent.h>
#include <ctype.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <utils/Log.h>
#include <linux/input.h>
#include <hardware/hardware.h>
#include <hardware/sensors.h>

#include "SensorBase.h"
#include "InputEventReader.h"

__BEGIN_DECLS

//#define SENSOR_DBG

#ifdef SENSOR_DBG
#define  D(...)  LOGD(__VA_ARGS__)
#else
#define  D(...)  ((void)0)
#endif

#define  E(...)  LOGE(__VA_ARGS__)
#define  I(...)  LOGI(__VA_ARGS__)

/* * SENSORS_HANDLE_xxx should greater than SENSORS_HANDLE_BASE
 * and must be unique.  */
#define SENSORS_HANDLE_LIGHT            1
#define SENSORS_HANDLE_PROXIMITY        2
#define SENSORS_HANDLE_ACCELEROMETER    3
#define SENSORS_HANDLE_MAGNETIC_FIELD   4
#define SENSORS_HANDLE_GYROSCOPE        5
#define SENSORS_HANDLE_PRESSURE         6
#define SENSORS_HANDLE_AMBIENT_TEMPERATURE      7
#define SENSORS_HANDLE_MAX              7

#define GRAVITY 9.80665f

#define EVENT_TYPE_ACCEL_X          REL_X
#define EVENT_TYPE_ACCEL_Y          REL_Y
#define EVENT_TYPE_ACCEL_Z          REL_Z

#define EVENT_TYPE_YAW              REL_RX
#define EVENT_TYPE_PITCH            REL_RY
#define EVENT_TYPE_ROLL             REL_RZ
#define EVENT_TYPE_ORIENT_STATUS    REL_WHEEL

#define EVENT_TYPE_MAGV_X           REL_DIAL
#define EVENT_TYPE_MAGV_Y           REL_HWHEEL
#define EVENT_TYPE_MAGV_Z           REL_MISC

#define EVENT_TYPE_M_O_X			REL_X
#define EVENT_TYPE_M_O_Y			REL_Y
#define EVENT_TYPE_M_O_Z			REL_Z

#define EVENT_TYPE_PROXIMITY        ABS_DISTANCE
#define EVENT_TYPE_LIGHT            ABS_MISC

#define EVENT_TYPE_GYRO_X           REL_RY
#define EVENT_TYPE_GYRO_Y           REL_RX
#define EVENT_TYPE_GYRO_Z           REL_RZ

#define EVENT_TYPE_PRESSURE         REL_X
#define EVENT_TYPE_TEMPERATURE      REL_Y

#define PATH_MAX_LEN                256
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define MIN(a, b) ((a) < (b) ? a : b)

#define NSEC_PER_SEC    1000000000L
static inline int64_t timespec_to_ns(const struct timespec *ts)
{
    return ((int64_t) ts->tv_sec * NSEC_PER_SEC) + ts->tv_nsec;
}

enum axis {
    AXIS_X = 0,
    AXIS_Y,
    AXIS_Z
};

union sensor_data_t {
    int compass_filter_en;
    float light_glass_factor;
};

class SensorBase;

const struct sensor_t* get_platform_sensor_list(int *num);
SensorBase** get_platform_sensors();
extern void (* sensor_platform_finalize)();

__END_DECLS
#endif  // ANDROID_SENSORS_H
