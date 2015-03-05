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

#define LOG_TAG "GAID_accelerometer"

#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <hardware/sensors.h>
#include <linux/input.h>
#include <dirent.h>

#include "sensors_gaid.h"

#define ACCEL_SYSFS_DIR    "/sys/devices/platform/lis3lv02d/"
#define ACCEL_POWERON      "poweron"
#define ACCEL_INPUT_POLL   "input/input2/poll"

#define INPUT_DIR       "/dev/input/"
#define ACCEL_NAME      "ST LIS3LV02DL Accelerometer"

static int fd_accel = -1;

#define INPUT_BUFFER_SIZE 64
static struct input_event input_buf[INPUT_BUFFER_SIZE];
static int input_buf_idx;
static int input_buf_cnt;
static float prev_x, prev_y, prev_z;

static int open_device(const char *devname)
{
    int fd;
    char name[80];

    fd = open(devname, O_RDWR);
    if (fd < 0) {
        E("could not open %s, error %s", devname, strerror(errno));
        return fd;
    }

    name[sizeof(name) - 1] = '\0';
    if (ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name) < 1)
        return -1;

    if (strncmp(name, ACCEL_NAME, sizeof(ACCEL_NAME)))
        return -1;

    return fd;
}

static int gaid_accelerometer_data_open(void)
{
    DIR *dir;
    struct dirent *de;
    int i = 0;
    char devname[PATH_MAX];
    char *filename;

    if (fd_accel >= 0)
        return fd_accel;

    dir = opendir(INPUT_DIR);
    if (dir == NULL)
        return -1;

    strcpy(devname, INPUT_DIR);
    filename = devname + strlen(devname);
    while ((de = readdir(dir))) {
        if (de->d_type != DT_CHR)
            continue;

        strcpy(filename, de->d_name);
        fd_accel = open_device(devname);
        if (fd_accel >= 0) {
            D("%s open file %s for accel data", __func__, devname);
            break;
        }
    }

    if (fd_accel < 0)
        E("%s dev file open failed", __FUNCTION__);

    closedir(dir);

    return fd_accel;
}

static void gaid_accelerometer_data_close(void)
{
    if (fd_accel >= 0) {
        close(fd_accel);
        fd_accel = -1;
    }
}

static void gaid_accelerometer_set_fd(fd_set *fds)
{
    FD_SET(fd_accel, fds);
}

static int gaid_accelerometer_is_fd(fd_set *fds)
{
    return FD_ISSET(fd_accel, fds) ? 1 : 0;
}

static int process_event(sensors_event_t *data, struct input_event *ev)
{
    int ret;
    struct timespec t;

    switch (ev->type) {
    case EV_ABS:
        switch (ev->code) {
        /*
         * data is in mG unit, need to convert it into m/s^2 required
         * by android flip axis X & Y to fit with Android platform
         */
        case ABS_X:
            data->acceleration.y = ((float)ev->value / 1000) * GRAVITY_EARTH;
            prev_y = data->acceleration.y;
            break;
        case ABS_Y:
            data->acceleration.x =
                        ((float)ev->value / 1000) * GRAVITY_EARTH * -1.0;
            prev_x = data->acceleration.x;
            break;
        case ABS_Z:
            data->acceleration.z = ((float)ev->value / 1000) * GRAVITY_EARTH;
            prev_z = data->acceleration.z;
            break;
        }
        ret = 0;
        break;
    case EV_SYN:
        clock_gettime(CLOCK_REALTIME, &t);

        data->timestamp = timespec_to_ns(&t);
        data->sensor = S_HANDLE_ACCELEROMETER;
        data->type = SENSOR_TYPE_ACCELEROMETER;
        data->version = sizeof(sensors_event_t);
        data->acceleration.status = SENSOR_STATUS_ACCURACY_HIGH;

        D("scaled data: x = %f, y = %f, z = %f\n",
          data->acceleration.x, data->acceleration.y, data->acceleration.z);
        ret = 1;
        break;
    }

    return ret;
}

static int gaid_accelerometer_data_read(sensors_event_t *data)
{
    int x,y,z;
    int size;
    struct input_event *iev;

    /*
     * the Linux input subsystem doesn't pass duplicate data to reduce
     * user-kernel memory copy. Here we remember the data from the
     * previous sample, and pass the previous value if it is not changed.
     */
    data->acceleration.x = prev_x;
    data->acceleration.y = prev_y;
    data->acceleration.z = prev_z;

    while (1) {
        while (input_buf_idx < input_buf_cnt) {
            iev = &input_buf[input_buf_idx++];
            if (process_event(data, iev))
                return 0;
        }

        size = read(fd_accel, input_buf, sizeof(input_buf));
        if (size < 0 || (size % sizeof(struct input_event)) != 0) {
            E("%s read error %s", __FUNCTION__, strerror(errno));
            return size;
        }

        input_buf_cnt = size / sizeof(input_buf[0]);
        input_buf_idx = 0;
    }

    return 0;
}

#define SENSOR_NO_POLL 0x7fffffff

static int gaid_accelerometer_set_delay(int us)
{
    int fd;
    int ret;
    char rate[16];

    fd = open(ACCEL_SYSFS_DIR ACCEL_INPUT_POLL, O_RDWR);
    if (fd < 0) {
        E("%s error open pollrate: %s", __func__, strerror(errno));
        return -1;
    }

    if (us == SENSOR_NO_POLL)
        us = 0;

    ret = snprintf(rate, sizeof(rate), "%d", us / 1000);
    if (ret < 0) {
        E("%s error convert rate to string: %s", __func__, strerror(errno));
        close(fd);
        return -1;
    }

    D("%s set delay to %s ms", __func__, rate);
    ret = pwrite(fd, rate, sizeof(rate), 0);
    if (ret < 0) {
        E("%s error write poll: %s ms %s", __func__, rate, strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

static int gaid_accelerometer_activate(int enabled)
{
    int fd;
    int ret;
    char str[4];

    fd = open(ACCEL_SYSFS_DIR ACCEL_POWERON, O_WRONLY);
    if (fd < 0) {
        E("%s error open poweron: %s", __func__, strerror(errno));
        return -1;
    }

    str[0] = enabled ? '1' : '0';
    str[1] = '\0';
    ret = pwrite(fd, str, sizeof(str), 0);
    if (ret < 0) {
        E("%s error write poweron: %s", __func__, strerror(errno));
        close(fd);
        return -1;
    }

    if (!enabled)
        gaid_accelerometer_set_delay(0);

    close(fd);
    return 0;
}

sensors_ops_t gaid_sensors_accelerometer = {
    .sensor_data_open  = gaid_accelerometer_data_open,
    .sensor_set_fd     = gaid_accelerometer_set_fd,
    .sensor_is_fd      = gaid_accelerometer_is_fd,
    .sensor_read       = gaid_accelerometer_data_read,
    .sensor_data_close = gaid_accelerometer_data_close,
    .sensor_activate   = gaid_accelerometer_activate,
    .sensor_set_delay  = gaid_accelerometer_set_delay,
    .sensor_list       = {
        .name       = "ST LIS3DH 3-axis Accelerometer",
        .vendor     = "ST",
        .version    = 1,
        .handle     = S_HANDLE_ACCELEROMETER,
        .type       = SENSOR_TYPE_ACCELEROMETER,
        .maxRange   = 2.0 * GRAVITY_EARTH,
        .resolution = GRAVITY_EARTH / 1000,
        .power      = 0.011f,
        .minDelay   = 20,
        .reserved   = {}
    },
};
