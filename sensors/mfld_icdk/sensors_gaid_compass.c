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

#define LOG_TAG "GAID_compass"

#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <hardware/sensors.h>
#include <sys/stat.h>

#include "sensors_gaid.h"

#define COMPASS_SYSFS_DIR       "/sys/class/i2c-adapter/i2c-5/5-000f/ak8974/"
#define COMPASS_DATA            "curr_pos"
#define COMPASS_CONFIG_DIR      "/data/compass/"
#define COMPASS_CONFIG_FILE     "ak8974.conf"

#define RESOLUTION 0.3f /* 0.3 uT per LSB */
#define CALIBRATION_MIN_RANGE 100

static int fd_pos = -1;
static int fd_config = -1;
static int minX, maxX;
static int minY, maxY;
static int minZ, maxZ;

static int gaid_compass_data_open(void)
{
    char buf[32];
    int ret = 0;

    if (fd_pos < 0) {
        fd_pos = open(COMPASS_SYSFS_DIR COMPASS_DATA, O_RDONLY);
        if (fd_pos < 0)
            E("%s dev file open failed", __func__);

        fd_config = open(COMPASS_CONFIG_DIR COMPASS_CONFIG_FILE,
                         O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO);
        if (fd_config >= 0) {
            ret = pread(fd_config, buf, sizeof(buf), 0);

            if (ret > 0)
                sscanf(buf, "%d,%d,%d,%d,%d,%d\n",
                       &minX, &maxX, &minY, &maxY, &minZ, &maxZ);
            else
                minX = maxX = minY = maxY = minZ = maxZ = 0;
        }
    }

    return fd_pos;
}

static void gaid_compass_data_close(void)
{
    if (fd_pos >= 0) {
        close(fd_pos);
        fd_pos = -1;
    }

    if (fd_config >= 0) {
        close(fd_config);
        fd_config = -1;
    }
}

static void gaid_compass_set_fd(fd_set *fds)
{
    FD_SET(fd_pos, fds);
}

static int gaid_compass_is_fd(fd_set *fds)
{
    return FD_ISSET(fd_pos, fds);
}

static void gaid_compass_calibration(int* x, int* y, int* z)
{
    int offsetX, offsetY, offsetZ;

    if (maxX == minX && maxX == 0)
        maxX = minX = *x;
    if (maxY == minY && maxY == 0)
        maxY = minY = *y;
    if (maxZ == minZ && maxZ == 0)
        maxZ = minZ = *z;

    if (*x < minX)
        minX = *x;
    if (*x > maxX)
        maxX = *x;
    offsetX = (minX + maxX) / 2;
    *x = *x - offsetX;

    if (*y < minY)
        minY = *y;
    if (*y > maxY)
        maxY = *y;
    offsetY = (minY + maxY) / 2;
    *y = *y - offsetY;

    if (*z < minZ)
        minZ = *z;
    if (*z > maxZ)
        maxZ = *z;
    offsetZ = (minZ + maxZ) / 2;
    *z = *z - offsetZ;

    if ((maxX - minX < CALIBRATION_MIN_RANGE) ||
        (maxY - minY < CALIBRATION_MIN_RANGE) ||
        (maxZ - minZ < CALIBRATION_MIN_RANGE))
        *x = *y = *z = 0;
}

static void gaid_compass_save_calibration()
{
    static int oldminX = 0, oldmaxX = 0;
    static int oldminY = 0, oldmaxY = 0;
    static int oldminZ = 0, oldmaxZ = 0;
    char buf[128];

    if (fd_config >= 0) {
        if ((oldminX != minX) || (oldmaxX != maxX) ||
            (oldminY != minX) || (oldmaxY != maxY) ||
            (oldminZ != minZ) || (oldmaxZ != maxZ)) {
            snprintf(buf, sizeof(buf), "%d,%d,%d,%d,%d,%d\n",
                    minX, maxX, minY, maxY, minZ, maxZ);
            sscanf(buf, "%d,%d,%d,%d,%d,%d\n",
                   &oldminX, &oldmaxX, &oldminY, &oldmaxY, &oldminZ, &oldmaxZ);

            pwrite(fd_config, buf, sizeof(buf), 0);
        }
    }
}

#define BUFSIZE    100
static int gaid_compass_data_read(sensors_event_t *data)
{
    struct timespec t;
    char buf[BUFSIZE];
    int x,y,z;
    int ret;
    float heading;

    ret = pread(fd_pos, buf, sizeof(buf), 0);
    if (ret < 0) {
        E("%s read error\n", __func__);
        return ret;
    }
    sscanf(buf,"(%d,%d,%d)", &x, &y, &z);
    D("compass raw data: x = %d, y = %d, z = %d", x, y, z);

    gaid_compass_calibration(&x, &y, &z);
    gaid_compass_save_calibration();

    clock_gettime(CLOCK_REALTIME, &t);

    data->timestamp = (timespec_to_ns(&t));
    data->sensor = S_HANDLE_MAGNETIC_FIELD;
    data->type = SENSOR_TYPE_MAGNETIC_FIELD;
    data->version = sizeof(sensors_event_t);
    data->magnetic.x = y * RESOLUTION * -1.0;
    data->magnetic.y = x * RESOLUTION;
    data->magnetic.z = z * RESOLUTION;
    data->magnetic.status = SENSOR_STATUS_ACCURACY_HIGH;

    return 0;
}

sensors_ops_t gaid_sensors_compass = {
    .sensor_data_open   = gaid_compass_data_open,
    .sensor_set_fd      = gaid_compass_set_fd,
    .sensor_is_fd       = gaid_compass_is_fd,
    .sensor_read        = gaid_compass_data_read,
    .sensor_data_close  = gaid_compass_data_close,
    .sensor_list        = {
        .name       = "AK8974 Compass",
        .vendor     = "AsahiKASEI",
        .version    = 1,
        .handle     = S_HANDLE_MAGNETIC_FIELD,
        .type       = SENSOR_TYPE_MAGNETIC_FIELD,
        .maxRange   = 614.0f,
        .resolution = 0.3f,
        .minDelay   = 50,
        .power      = 1.0f,
        .reserved   = {}
    },
};
