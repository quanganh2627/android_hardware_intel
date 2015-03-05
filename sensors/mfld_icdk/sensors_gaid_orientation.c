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

#define LOG_TAG "icdk_orientation"

#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <math.h>
#include <hardware/sensors.h>

#include "sensors_gaid.h"

#define COMPASS_SYSFS_DIR	"/sys/class/i2c-adapter/i2c-5/5-000f/ak8974/"
#define COMPASS_DATA		"curr_pos"

#define PI 3.1415926f

static int fd_pos = -1;

static int gaid_orientation_data_open(void)
{
    if (fd_pos < 0) {
        fd_pos = open(COMPASS_SYSFS_DIR COMPASS_DATA, O_RDONLY);
        if (fd_pos < 0)
            E("%s dev file open failed", __func__);
    }

    return fd_pos;
}

static void gaid_orientation_data_close(void)
{
    if (fd_pos >= 0) {
        close(fd_pos);
        fd_pos = -1;
    }
}

static void gaid_orientation_set_fd(fd_set *fds)
{
    FD_SET(fd_pos, fds);
}

static int gaid_orientation_is_fd(fd_set *fds)
{
    return FD_ISSET(fd_pos, fds);
}

static float calc_azimuth(int x, int y)
{
    float ret = 0.0;

    if (x == 0) {
        if (y < 0)
            ret = 90.0;
        else if (y > 0)
            ret = 270.0;
    } else {
        if (x < 0)
            ret = 180 - atan((float)y / x) * 180 / PI;
        else {
            if (y < 0)
                ret = -atan((float)y / x) * 180 / PI;
            else if (y > 0)
                ret = 360 - atan((float)y / x) * 180 / PI;
        }
    }

    return ret;
}

#define BUFSIZE    100
static int gaid_orientation_data_read(sensors_event_t *data)
{
    struct timespec t;
    char buf[BUFSIZE];
    int x,y,z;
    int ret;
    float azimuth;

    ret = pread(fd_pos, buf, sizeof(buf), 0);
    if (ret < 0) {
        E("%s read error\n", __func__);
        return ret;
    }
    sscanf(buf,"(%d,%d,%d)", &x, &y, &z);
    D("orientation raw data: x = %d, y = %d, z = %d", x, y, z);

    azimuth = calc_azimuth(x, -y);
    clock_gettime(CLOCK_REALTIME, &t);

    data->timestamp = (timespec_to_ns(&t));
    data->sensor = S_HANDLE_ORIENTATION,
    data->type = SENSOR_TYPE_ORIENTATION,
    data->version = sizeof(sensors_event_t);
    data->orientation.azimuth = azimuth;
    data->orientation.pitch = 0;
    data->orientation.roll = 0;
    data->orientation.status = SENSOR_STATUS_ACCURACY_HIGH;

    return 0;
}

sensors_ops_t gaid_sensors_orientation = {
    .sensor_data_open   = gaid_orientation_data_open,
    .sensor_set_fd      = gaid_orientation_set_fd,
    .sensor_is_fd       = gaid_orientation_is_fd,
    .sensor_read        = gaid_orientation_data_read,
    .sensor_data_close  = gaid_orientation_data_close,
    .sensor_list        = {
        .name       = "AK8974 Compass",
        .vendor     = "AsahiKASEI",
        .version    = 1,
        .handle     = S_HANDLE_ORIENTATION,
        .type       = SENSOR_TYPE_ORIENTATION,
        .maxRange   = 360.0f,
        .resolution = 0.1f,
        .minDelay   = 50,
        .power      = 1.0f,
        .reserved   = {}
    },
};
