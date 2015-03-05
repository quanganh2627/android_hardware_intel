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

/* Contains changes by Wind River Systems, 2009-2010 */

#define LOG_TAG "GAID_light"

#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <hardware/sensors.h>

#include "sensors_gaid.h"

#define ALS_SYSFS_DIR   "/sys/class/i2c-adapter/i2c-5/5-0029/apds9802als/"
#define ALS_POWERON     "poweron"
#define ALS_DEV         "/dev/apds9802als"

static int fd_als = -1;
static int old_lux;

static int gaid_als_data_open(void)
{
    if (fd_als < 0) {
        fd_als = open(ALS_DEV, O_RDONLY);
        if (fd_als < 0) {
            E("%s dev file open failed: %s\n", __func__, strerror(errno));
        }
    }

    return fd_als;
}

static void gaid_als_data_close(void)
{
    if (fd_als >= 0) {
        close(fd_als);
        fd_als = -1;
    }
}

static void gaid_als_set_fd(fd_set *fds)
{
    FD_SET(fd_als, fds);
}

static int gaid_als_is_fd(fd_set *fds)
{
    return FD_ISSET(fd_als, fds);
}

static int gaid_als_data_read(sensors_event_t *data)
{
    struct timespec t;
    int ret;
    int lux;

    ret = pread(fd_als, &lux, sizeof(lux), 0);
    if (ret < 0) {
        E("%s read error\n", __func__);
        return ret;
    }

    clock_gettime(CLOCK_REALTIME, &t);

    data->timestamp = timespec_to_ns(&t);
    data->sensor = S_HANDLE_LIGHT;
    data->type = SENSOR_TYPE_LIGHT;
    data->version = sizeof(sensors_event_t);
    data->light = lux;

    D("lux = %d", lux);

    return 0;
}

static int gaid_als_activate(int enabled)
{
    int fd;
    int ret;
    char str[4];

    fd = open(ALS_SYSFS_DIR ALS_POWERON, O_WRONLY);
    if (fd < 0) {
        E("%s error open poweron: %s\n", __func__, strerror(errno));
        return -EINVAL;
    }

    str[0] = enabled ? '1' : '0';
    str[1] = '\0';
    ret = pwrite(fd, str, sizeof(str), 0);
    if (ret < 0) {
        E("%s error power on ALS dev: %s\n", __func__, strerror(errno));
        close(fd);
        return -EINVAL;
    }

    close(fd);
    return 0;
}

sensors_ops_t gaid_sensors_als = {
    .sensor_data_open   = gaid_als_data_open,
    .sensor_set_fd      = gaid_als_set_fd,
    .sensor_is_fd       = gaid_als_is_fd,
    .sensor_read        = gaid_als_data_read,
    .sensor_data_close  = gaid_als_data_close,
    .sensor_activate    = gaid_als_activate,
    .sensor_list        = {
        .name       = "APDS9802 Ambient Light Sensor",
        .vendor     = "Avago",
        .version    = 1,
        .handle     = S_HANDLE_LIGHT,
        .type       = SENSOR_TYPE_LIGHT,
        .maxRange   = 65535.0f,
        .resolution = 1.0f,
        .power      = 0.08f,
        .minDelay   = 0,
        .reserved   = {}
    },
};
