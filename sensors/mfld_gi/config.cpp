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

#include "../sensors.h"

#include "../LightSensor.h"
#include "../ProximitySensor.h"
#include "../AccelSensor.h"
#include "../CompassSensor.h"

#define RANGE_A                     (2*GRAVITY_EARTH)
#define RESOLUTION_A                (GRAVITY_EARTH / 1000)

#define RANGE_L                     (10000.0f)
#define RESOLUTION_L                (0.1f)

#define RANGE_P                     (6.0f)
#define RESOLUTION_P                (6.0f)

#define RESOLUTION_M                (0.15f)
#define RANGE_M                     (250.0f)

#define RANGE_G                     (2000.0f*(float)M_PI/180.0f)
#define CONVERT_GYRO                ((70.0f / 1000.0f) * ((float)M_PI / 180.0f))
#define RESOLUTION_G                ((70.0f / 1000.0f) * ((float)M_PI / 180.0f))

#define RANGE_PRESSURE              (1260.0f)
#define RESOLUTION_PRESSURE         (0.24f)

/* Sequence of sensor type in sensor_configs and sensor_list should me the same */
static const union sensor_data_t compass_data = {
    compass_filter_en:    1,
};
static const sensor_platform_config_t sensor_configs[] = {
/* accel */
    {
        handle:         SENSORS_HANDLE_ACCELEROMETER,
        name:           "accel",
        activate_path:  "/sys/bus/i2c/devices/5-0019/lis3dh/enable",
        poll_path:      "/sys/bus/i2c/devices/5-0019/lis3dh/poll",
        data_path:      NULL,
        config_path:    NULL,
        mapper:         { AXIS_Y, AXIS_X, AXIS_Z },
        scale:          { -1.0, -1.0, 1.0 },
        range:          { 0 },
        min_delay:      0,
        priv_data:      0,
    },
/* light */
    {
        handle:         SENSORS_HANDLE_LIGHT,
        name:           "light",
        activate_path:  "/dev/apds990x_lsensor",
        poll_path:      NULL,
        data_path:      "/dev/apds990x_lsensor",
        config_path:    NULL,
        mapper:         { 0 },
        scale:          { 0 },
        range:          { 0, 10000.0 },
        min_delay:      0,
        priv_data:      0,
    },
/* proximity */
    {
        handle:         SENSORS_HANDLE_PROXIMITY,
        name:           "proximity",
        activate_path:  "/dev/apds990x_psensor",
        poll_path:      NULL,
        data_path:      "/sys/bus/i2c/devices/5-0039/prox0_raw",
        config_path:    "/sys/bus/i2c/devices/5-0039/prox0_thresh_above_value",
        mapper:         { 0 },
        scale:          { 0 },
        range:          { 0 },
        min_delay:      0,
        priv_data:      0,
    },
/* compass */
    {
        handle:         SENSORS_HANDLE_MAGNETIC_FIELD,
        name:           "lsm303cmp",
        activate_path:  "/sys/bus/i2c/devices/5-001e/lsm303cmp/enable",
        poll_path:      "/sys/bus/i2c/devices/5-001e/lsm303cmp/poll",
        data_path:      NULL,
        config_path:    "/data/compass.conf",
        mapper:         { AXIS_Y, AXIS_X, AXIS_Z },
        scale:          { 670, -670, 600 },
        range:          { 0 },
        min_delay:      0,
        priv_data:      &compass_data,
    },
};

static const struct sensor_t sensor_list[] = {
    { "MODEL_LSM303DLHC 3-axis Accelerometer",
      "STMicroelectronics",
      1, SENSORS_HANDLE_ACCELEROMETER,
      SENSOR_TYPE_ACCELEROMETER, RANGE_A, RESOLUTION_A, 0.11f, 10000, { } },
    { "Avago APDS-9900 Digital Ambient Light Sensor",
      "Avago",
      1, SENSORS_HANDLE_LIGHT,
      SENSOR_TYPE_LIGHT, RANGE_L, RESOLUTION_L, 0.25f, 0, { } },
    { "Avago APDS-9900 Digital Proximity Sensor",
      "Avago",
      1, SENSORS_HANDLE_PROXIMITY,
      SENSOR_TYPE_PROXIMITY, RANGE_P, RESOLUTION_P, 0.25f, 0, { } },
    { "MODEL_LSM303DLHC 3-axis Magnetic field sensor",
      "STMicroelectronics",
      1, SENSORS_HANDLE_MAGNETIC_FIELD,
      SENSOR_TYPE_MAGNETIC_FIELD, RANGE_M, RESOLUTION_M, 0.1f, 20000, { } },
};

static SensorBase* platform_sensors[ARRAY_SIZE(sensor_list)];

const struct sensor_t* get_platform_sensor_list(int *sensor_num)
{
    int num = ARRAY_SIZE(sensor_list);
    *sensor_num = 0;

    if (ARRAY_SIZE(sensor_list) != ARRAY_SIZE(sensor_configs))
        return NULL;

    for (int i = 0; i < num; i++) {
        if (sensor_list[i].handle != sensor_configs[i].handle) {
            E("Sensor config sequence error\n");
            return NULL;
        }
    }
    *sensor_num = num;

    return sensor_list;
}

SensorBase **get_platform_sensors()
{
    int num = ARRAY_SIZE(sensor_list);

    for (int i = 0; i < num; i++) {
        int handle = sensor_list[i].handle;
        switch (handle) {
        case SENSORS_HANDLE_LIGHT:
            platform_sensors[i] = new LightSensor(&sensor_configs[i]);
            break;
        case SENSORS_HANDLE_PROXIMITY:
            platform_sensors[i] = new ProximitySensor(&sensor_configs[i]);
            break;
        case SENSORS_HANDLE_ACCELEROMETER:
            platform_sensors[i] = new AccelSensor(&sensor_configs[i]);
            break;
        case SENSORS_HANDLE_MAGNETIC_FIELD:
            platform_sensors[i] = new CompassSensor(&sensor_configs[i]);
            break;
        default:
            E("Error, no Sensor ID handle %d found\n", handle);
            return NULL;
        }
    }

    return platform_sensors;
}
