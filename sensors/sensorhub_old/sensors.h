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

#include <stdint.h>
#include <errno.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <utils/Log.h>
#include <linux/input.h>

#include <hardware/hardware.h>
#include <hardware/sensors.h>

__BEGIN_DECLS


/*****************************************************************************/

//#define SENSOR_DBG

#ifdef SENSOR_DBG
#define  D(...)  LOGD(__VA_ARGS__)
#else
#define  D(...)  ((void)0)
#endif

#define  E(...)  LOGE(__VA_ARGS__)

/*****************************************************************************/
/*
 * SENSORS_HANDLE_xxx should greater than SENSORS_HANDLE_BASE and must be unique.
*/
#define SENSORS_HANDLE_LIGHT            	1
#define SENSORS_HANDLE_PROXIMITY        	2
#define SENSORS_HANDLE_ACCELEROMETER     	3
#define SENSORS_HANDLE_MAGNETIC_FIELD   	4
#define SENSORS_HANDLE_GYROSCOPE        	5
#define SENSORS_HANDLE_PRESSURE         	6
#define SENSORS_HANDLE_GRAVITY			7
#define SENSORS_HANDLE_LINEAR_ACCELERATION 	8
#define SENSORS_HANDLE_ROTATION_VECTOR  	9
#define SENSORS_HANDLE_ORIENTATION		10
#define SENSORS_HANDLE_GESTURE_FLICK		11
#define SENSORS_HANDLE_GESTURE			12
#define SENSORS_HANDLE_TERMINAL			13
#define SENSORS_HANDLE_PHYSICAL_ACTIVITY	14
#define SENSORS_HANDLE_PEDOMETER		15
#define SENSORS_HANDLE_AUDIO_CLASSIFICATION	16
#define SENSORS_HANDLE_AMBIENT_TEMPERATURE	17
#define SENSORS_HANDLE_SHAKE			18
#define SENSORS_HANDLE_SIMPLE_TAPPING		19
#define SENSORS_HANDLE_LIGHT_SEC            	20
#define SENSORS_HANDLE_PROXIMITY_SEC     	21
#define SENSORS_HANDLE_ACCELEROMETER_SEC     	22
#define SENSORS_HANDLE_MAGNETIC_FIELD_SEC   	23
#define SENSORS_HANDLE_GYROSCOPE_SEC        	24
#define SENSORS_HANDLE_PRESSURE_SEC         	25
#define SENSORS_HANDLE_MAX              	25

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

/*
 * The SENSORS Module
 */

#define NSEC_PER_SEC    1000000000L
static inline int64_t timespec_to_ns(const struct timespec *ts)
{
    return ((int64_t) ts->tv_sec * NSEC_PER_SEC) + ts->tv_nsec;
}

/*****************************************************************************/
#define GRAVITY 9.80665f

#define EVENT_TYPE_M_O_X			REL_X
#define EVENT_TYPE_M_O_Y			REL_Y
#define EVENT_TYPE_M_O_Z			REL_Z

// conversion of acceleration data to SI units (m/s^2)
#define RANGE_A                 (2*GRAVITY_EARTH)
#define CONVERT_A               (2*GRAVITY_EARTH / 4096)
#define CONVERT_A_X             (CONVERT_A)
#define CONVERT_A_Y             (CONVERT_A)
#define CONVERT_A_Z             (CONVERT_A)

// conversion of magnetic data to uT units
#define CONVERT_M               (0.5f)
#define CONVERT_M_X             (-CONVERT_M)
#define CONVERT_M_Y             (-CONVERT_M)
#define CONVERT_M_Z             (-CONVERT_M)

// conversion of gyro data to SI units (radian/sec)
#define RANGE_GYRO              (2000.0f*(float)M_PI/180.0f)
#define CONVERT_GYRO            ((1.0f / 10.0f) * ((float)M_PI / 180.0f))
#define CONVERT_GYRO_X          (CONVERT_GYRO)
#define CONVERT_GYRO_Y          (-CONVERT_GYRO)
#define CONVERT_GYRO_Z          (CONVERT_GYRO)

/*****************************************************************************/

/*****************************************************************************/
// Definition for virtual sensors, it's one-one map to the user space library
// Sensor types
#define SENSOR_TYPE_GESTURE_FLICK           100
#define SENSOR_TYPE_GESTURE                 101
#define SENSOR_TYPE_PHYSICAL_ACTIVITY       102
#define SENSOR_TYPE_TERMINAL                103
#define SENSOR_TYPE_AUDIO_CLASSIFICATION    104
#define SENSOR_TYPE_PEDOMETER               105
#define SENSOR_TYPE_SHAKE                   106
#define SENSOR_TYPE_SIMPLE_TAPPING          108

// Sensor event types
#define SHIFT_GESTURE_FLICK         4
#define SHIFT_GESTURE               5
#define SHIFT_PHYSICAL_ACTIVITY     6
#define SHIFT_TERMINAL              7
#define SHIFT_AUDIO_CLASSIFICATION  8
#define SHIFT_SHAKE_TITLT           9
#define SHIFT_SIMPLE_TAPPING        10

#define SENSOR_EVENT_TYPE_GESTURE_LEFT_FLICK        (1 << SHIFT_GESTURE_FLICK | 1)
#define SENSOR_EVENT_TYPE_GESTURE_RIGHT_FLICK       (1 << SHIFT_GESTURE_FLICK | 2)
#define SENSOR_EVENT_TYPE_GESTURE_UP_FLICK          (1 << SHIFT_GESTURE_FLICK | 3)
#define SENSOR_EVENT_TYPE_GESTURE_DOWN_FLICK        (1 << SHIFT_GESTURE_FLICK | 4)
#define SENSOR_EVENT_TYPE_GESTURE_LEFT_FLICK_TWICE  (1 << SHIFT_GESTURE_FLICK | 5)
#define SENSOR_EVENT_TYPE_GESTURE_RIGHT_FLICK_TWICE (1 << SHIFT_GESTURE_FLICK | 6)
#define SENSOR_EVENT_TYPE_GESTURE_NO_FLICK          (1 << SHIFT_GESTURE_FLICK | 7)

#define SENSOR_EVENT_TYPE_GESTURE_NUMBER_ONE            (1 << SHIFT_GESTURE | 1)
#define SENSOR_EVENT_TYPE_GESTURE_NUMBER_TWO            (1 << SHIFT_GESTURE | 2)
#define SENSOR_EVENT_TYPE_GESTURE_NUMBER_THREE          (1 << SHIFT_GESTURE | 3)
#define SENSOR_EVENT_TYPE_GESTURE_NUMBER_FOUR           (1 << SHIFT_GESTURE | 4)
#define SENSOR_EVENT_TYPE_GESTURE_NUMBER_FIVE           (1 << SHIFT_GESTURE | 5)
#define SENSOR_EVENT_TYPE_GESTURE_NUMBER_SIX            (1 << SHIFT_GESTURE | 6)
#define SENSOR_EVENT_TYPE_GESTURE_NUMBER_SEVEN          (1 << SHIFT_GESTURE | 7)
#define SENSOR_EVENT_TYPE_GESTURE_NUMBER_EIGHT          (1 << SHIFT_GESTURE | 8)
#define SENSOR_EVENT_TYPE_GESTURE_NUMBER_NINE           (1 << SHIFT_GESTURE | 9)
#define SENSOR_EVENT_TYPE_GESTURE_NUMBER_ZERO           (1 << SHIFT_GESTURE | 10)
#define SENSOR_EVENT_TYPE_GESTURE_NUMBER_EAR_TOUCH      (1 << SHIFT_GESTURE | 11)
#define SENSOR_EVENT_TYPE_GESTURE_NUMBER_EAR_TOUCH_BACK (1 << SHIFT_GESTURE | 12)

#define SENSOR_EVENT_TYPE_PHYSICAL_ACTIVITY_WALKING     (1 << SHIFT_PHYSICAL_ACTIVITY | 1)
#define SENSOR_EVENT_TYPE_PHYSICAL_ACTIVITY_RUNNING     (1 << SHIFT_PHYSICAL_ACTIVITY | 2)
#define SENSOR_EVENT_TYPE_PHYSICAL_ACTIVITY_SEDENTARY   (1 << SHIFT_PHYSICAL_ACTIVITY | 3)
#define SENSOR_EVENT_TYPE_PHYSICAL_ACTIVITY_RANDOM      (1 << SHIFT_PHYSICAL_ACTIVITY | 4)
#define SENSOR_EVENT_TYPE_PHYSICAL_ACTIVITY_BIKING      (1 << SHIFT_PHYSICAL_ACTIVITY | 5)
#define SENSOR_EVENT_TYPE_PHYSICAL_ACTIVITY_DRIVING     (1 << SHIFT_PHYSICAL_ACTIVITY | 6)

#define SENSOR_EVENT_TYPE_TERMINAL_FACE_UP              (1 << SHIFT_TERMINAL | 1)
#define SENSOR_EVENT_TYPE_TERMINAL_FACE_DOWN            (1 << SHIFT_TERMINAL | 2)
#define SENSOR_EVENT_TYPE_TERMINAL_PORTRAIT_UP          (1 << SHIFT_TERMINAL | 3)
#define SENSOR_EVENT_TYPE_TERMINAL_PORTRAIT_DOWN        (1 << SHIFT_TERMINAL | 4)
#define SENSOR_EVENT_TYPE_TERMINAL_HORIZONTAL_UP        (1 << SHIFT_TERMINAL | 5)
#define SENSOR_EVENT_TYPE_TERMINAL_HORIZONTAL_DOWN      (1 << SHIFT_TERMINAL | 6)
#define SENSOR_EVENT_TYPE_TERMINAL_UNKNOWN              (1 << SHIFT_TERMINAL | 7)

#define SENSOR_EVENT_TYPE_AUDIO_CLASSIFICATION_CROWD            (1 << SHIFT_AUDIO_CLASSIFICATION | 1)
#define SENSOR_EVENT_TYPE_AUDIO_CLASSIFICATION_SOFT_MUSIC       (1 << SHIFT_AUDIO_CLASSIFICATION | 2)
#define SENSOR_EVENT_TYPE_AUDIO_CLASSIFICATION_MECHANICAL       (1 << SHIFT_AUDIO_CLASSIFICATION | 3)
#define SENSOR_EVENT_TYPE_AUDIO_CLASSIFICATION_MOTION           (1 << SHIFT_AUDIO_CLASSIFICATION | 4)
#define SENSOR_EVENT_TYPE_AUDIO_CLASSIFICATION_MALE_SPEECH      (1 << SHIFT_AUDIO_CLASSIFICATION | 5)
#define SENSOR_EVENT_TYPE_AUDIO_CLASSIFICATION_FEMALE_SPEECH    (1 << SHIFT_AUDIO_CLASSIFICATION | 6)
#define SENSOR_EVENT_TYPE_AUDIO_CLASSIFICATION_SILENT           (1 << SHIFT_AUDIO_CLASSIFICATION | 7)
#define SENSOR_EVENT_TYPE_AUDIO_CLASSIFICATION_UNKNOWN          (1 << SHIFT_AUDIO_CLASSIFICATION | 8)

#define SENSOR_EVENT_TYPE_SHAKE         (1 << SHIFT_SHAKE_TITLT | 1)
#define SENSOR_EVENT_TYPE_SIMPLE_TAPPING_DOUBLE_TAPPING         (1 << SHIFT_SIMPLE_TAPPING | 1)

// Sensor delay types
#define SENSOR_DELAY_TYPE_PHYSICAL_ACTIVITY_INSTANT     (((1 << 3) + 1) * 1000)
#define SENSOR_DELAY_TYPE_PHYSICAL_ACTIVITY_QUICK       (((1 << 3) + 2) * 1000)
#define SENSOR_DELAY_TYPE_PHYSICAL_ACTIVITY_NORMAL      (((1 << 3) + 3) * 1000)
#define SENSOR_DELAY_TYPE_PHYSICAL_ACTIVITY_STATISTIC   (((1 << 3) + 4) * 1000)

#define SENSOR_DELAY_TYPE_AUDIO_CLASSIFICATION_INSTANT  (((1 << 3) + 5) * 1000)
#define SENSOR_DELAY_TYPE_AUDIO_CLASSIFICATION_SHORT    (((1 << 3) + 6) * 1000)
#define SENSOR_DELAY_TYPE_AUDIO_CLASSIFICATION_MEDIUM   (((1 << 3) + 7) * 1000)
#define SENSOR_DELAY_TYPE_AUDIO_CLASSIFICATION_LONG     (((1 << 3) + 8) * 1000)

#define SENSOR_DELAY_TYPE_PEDOMETER_INSTANT         (((1 << 3) + 9) * 1000)
#define SENSOR_DELAY_TYPE_PEDOMETER_QUICK           (((1 << 3) + 10) * 1000)
#define SENSOR_DELAY_TYPE_PEDOMETER_NORMAL          (((1 << 3) + 11) * 1000)
#define SENSOR_DELAY_TYPE_PEDOMETER_STATISTIC       (((1 << 3) + 12) * 1000)

/*****************************************************************************/

class SensorBase;
const struct sensor_t* get_platform_sensor_list(int *sensor_num);
SensorBase **get_platform_sensors();
extern void (* sensor_platform_finalize)();

__END_DECLS

#endif  // ANDROID_SENSORS_H
