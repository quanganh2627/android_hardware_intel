/*
 * Copyright (C) 2009 The Android Open Source Project
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

#define LOG_TAG "GAID_proximity"

#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <hardware/sensors.h>

#include "sensors_gaid.h"

#define PROXIMITY_SYSFS_DIR "/sys/class/i2c-adapter/i2c-5/5-0055/apds9802ps/"
#define PROXIMITY_POWERON "poweron"
#define PROXIMITY_DEV "/dev/apds9802ps"

static int fd_proximity = -1;

static int gaid_proximity_data_open(void)
{
    if (fd_proximity < 0) {
        fd_proximity = open(PROXIMITY_DEV, O_RDONLY);
        if (fd_proximity < 0) {
            E("%s dev file open failed: %s\n", __func__, strerror(errno));
        }
    }

    return fd_proximity;
}

static void gaid_proximity_data_close(void)
{
    if (fd_proximity >= 0) {
        close(fd_proximity);
        fd_proximity = -1;
    }
}

static void gaid_proximity_set_fd(fd_set *fds)
{
    FD_SET(fd_proximity, fds);
}

static int gaid_proximity_is_fd(fd_set *fds)
{
    return FD_ISSET(fd_proximity, fds);
}

#define OBJ_STATE_AWAY 1
#define OBJ_STATE_NEAR 2

static int gaid_proximity_data_read(sensors_event_t *data)
{
    struct timespec t;
    unsigned char state;
    int ret;
    float distance;

    ret = pread(fd_proximity, &state, sizeof(state), 0);
    if (ret < 0) {
        E("%s read error\n", __func__);
        return ret;
    }

    if (state == OBJ_STATE_AWAY)
        distance = 20.0;
    else if (state == OBJ_STATE_NEAR)
        distance = 2.0;
    else {
        E("%s read invalide data %d\n", __func__, state);
        return -EINVAL;
    }

    clock_gettime(CLOCK_REALTIME, &t);

    data->timestamp = timespec_to_ns(&t);
    data->sensor = S_HANDLE_PROXIMITY;
    data->type = SENSOR_TYPE_PROXIMITY;
    data->version = sizeof(sensors_event_t);
    data->distance = distance;
    D("distance = %f", data->distance);

    return 0;
}

static int gaid_proximity_activate(int enabled)
{
    int fd;
    int ret;
    char str[4];

    fd = open(PROXIMITY_SYSFS_DIR PROXIMITY_POWERON, O_WRONLY);
    if (fd < 0) {
        E("%s error open poweron: %s\n", __func__, strerror(errno));
        return -EINVAL;
    }

    str[0] = enabled ? '1' : '0';
    str[1] = '\0';
    ret = pwrite(fd, str, sizeof(str), 0);
    if (ret < 0) {
        E("%s error poweron PROXIMITY dev: %s\n", __func__, strerror(errno));
        close(fd);
        return -EINVAL;
    }

    close(fd);
    return 0;
}

sensors_ops_t gaid_sensors_proximity = {
    .sensor_data_open   = gaid_proximity_data_open,
    .sensor_set_fd      = gaid_proximity_set_fd,
    .sensor_is_fd       = gaid_proximity_is_fd,
    .sensor_read        = gaid_proximity_data_read,
    .sensor_data_close  = gaid_proximity_data_close,
    .sensor_activate    = gaid_proximity_activate,
    .sensor_list        = {
        .name       = "APDS9802 Proximity Sensor",
        .vendor     = "Avago",
        .version    = 1,
        .handle     = S_HANDLE_PROXIMITY,
        .type       = SENSOR_TYPE_PROXIMITY,
        .maxRange   = 20.0f,
        .resolution = 0.01f,
        .minDelay   = 0,
        .power      = 0.10f,
        .reserved   = {}
    },
};
