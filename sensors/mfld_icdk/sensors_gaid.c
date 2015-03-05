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

/* this implements a sensors hardware library for the Android emulator.
 * the following code should be built as a shared library that will be
 * placed into /system/lib/hw/sensors.goldfish.so
 *
 * it will be loaded by the code in hardware/libhardware/hardware.c
 * which is itself called from com_android_server_SensorService.cpp
 */

#define LOG_TAG "GAID_Sensors"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <cutils/log.h>
#include <cutils/native_handle.h>
#include <hardware/sensors.h>
#include <pthread.h>

#include "sensors_gaid.h"

#define DEFAULT_TIMEOUT 1000000 /* us */

static struct {
    int handle;
    int fd;
    sensors_ops_t* ops;
} _sensorIds[] =
{
    GAID_SENSORS_DATA
};

#define MAX_NUM_SENSORS ((int)(sizeof(_sensorIds)/sizeof(_sensorIds[0])))

/** SENSORS POLL DEVICE
 **
 ** This one is used to read sensor data from the hardware.
 ** We implement this by simply reading the data from the
 ** emulator through the QEMUD channel.
 **/

typedef struct SensorPoll {
    struct sensors_poll_device_t  device;
    sensors_event_t     sensors[MAX_NUM_SENSORS];
    int                 active[MAX_NUM_SENSORS];
    int                 delay[MAX_NUM_SENSORS];
    int                 mindelay;
    int                 max_fd;
    unsigned int        pending_mask;
    pthread_cond_t      activated;
    pthread_mutex_t     mutex;
} SensorPoll;

static int fill_data(SensorPoll* data,sensors_event_t* values, int maxcount)
{
    int i;
    int count = 0;

    for (i = 0; i < MAX_NUM_SENSORS; i++) {
        if (count > maxcount)
            break;

        if (data->pending_mask & (1 << i)) {
            memcpy(values, &data->sensors[i],sizeof(sensors_event_t));
            data->pending_mask &= ~(1 << i);
            count++;
            values++;
        }
    }

    return count;
}

static void wait_for_sensor_activated(SensorPoll *polldev)
{
    int i;

    pthread_mutex_lock(&polldev->mutex);
    for (i = 0; i < MAX_NUM_SENSORS; i++) {
        if (polldev->active[i] == 1)
            goto out;
    }
    pthread_cond_wait(&polldev->activated, &polldev->mutex);
    D("at least one sensor activated");

out:
    pthread_mutex_unlock(&polldev->mutex);
}

static int icdk_sensor_poll(struct sensors_poll_device_t *dev,
                            sensors_event_t* data, int count)
{
    fd_set fds;
    struct timeval t;
    int timeout;
    int ret;
    SensorPoll *polldev = (void*)dev;
    int i, change;

    D("%s: dev=%p", __FUNCTION__, dev);

    if (polldev->pending_mask)
         return fill_data(polldev, data, count);


    timeout = polldev->mindelay;

    t.tv_sec = timeout / 1000000;
    t.tv_usec = timeout % 1000000;

    while (1) {
        wait_for_sensor_activated(polldev);

	FD_ZERO(&fds);
	for (i = 0; i < MAX_NUM_SENSORS; i++) {
		if (_sensorIds[i].fd < 0)
			continue;
		_sensorIds[i].ops->sensor_set_fd(&fds);
	}

        ret = select(polldev->max_fd + 1, &fds, NULL, NULL, &t);
        if (ret < 0) {
            D("%s: select error %d", __FUNCTION__, ret);
            break;
        }

        /* timeout */
        if (ret == 0)
            continue;

        for (i = 0; i < MAX_NUM_SENSORS; i++) {
            if (_sensorIds[i].fd < 0 || !polldev->active[i])
                continue;

            if (_sensorIds[i].ops->sensor_is_fd(&fds)) {
                ret = _sensorIds[i].ops->sensor_read(&polldev->sensors[i]);
                if (ret < 0) {
                    D("%s %d: read sensor error %d", __FUNCTION__, i, ret);
                    continue;
                }
                polldev->pending_mask |= (1 << i);
            }
        }

        if (!polldev->pending_mask) {
            usleep(timeout);
            continue;
	}

        ret = fill_data(polldev, data, count);
        break;
    }

    return ret;
}

/** SENSORS POLL DEVICE FUNCTIONS **/

static int icdk_sensor_close(struct hw_device_t *dev)
{
    int i;
    SensorPoll *polldev = (SensorPoll *)dev;

    D("%s: dev=%p", __FUNCTION__, dev);

    if (!polldev)
        return 0;

    for (i = 0; i < MAX_NUM_SENSORS; i++) {
        if (_sensorIds[i].fd >= 0)
            _sensorIds[i].ops->sensor_data_close();
    }

    pthread_cond_destroy(&polldev->activated);
    pthread_mutex_destroy(&polldev->mutex);

    free(polldev);

    return 0;
}

static int icdk_sensor_open(struct hw_device_t *dev)
{
    int i, ret;
    SensorPoll *polldev = (SensorPoll *)dev;

    D("%s: dev=%p", __FUNCTION__, dev);

    if (!polldev)
        return 0;

    for (i = 0; i < MAX_NUM_SENSORS; i++) {
        ret = _sensorIds[i].ops->sensor_data_open();
        if (ret < 0) {
            _sensorIds[i].fd = -1;
        } else {
            _sensorIds[i].fd = ret;
            if (ret > polldev->max_fd)
                polldev->max_fd = ret;
        }
    }

    return 0;
}

static int get_min_delay(SensorPoll *polldev)
{
    int i;
    int delay = DEFAULT_TIMEOUT;

    for (i = 0; i < MAX_NUM_SENSORS; i++) {
        if (!polldev->active[i])
            continue;

        if (polldev->delay[i] < delay)
            delay = polldev->delay[i];
    }

    D("%s min delay %d us", __FUNCTION__, delay);

    return delay;
}

static int
icdk_sensor_activate(struct sensors_poll_device_t *dev, int handle, int enabled)
{
    int ret = 0;
    int i;
    SensorPoll *polldev = (void *)dev;

    D("%s: dev=%p handle=%x enable=%d ", __FUNCTION__, dev, handle, enabled);

    for (i = 0; i < MAX_NUM_SENSORS; i++) {
        if (_sensorIds[i].ops->sensor_list.handle != handle)
            continue;

        if (_sensorIds[i].ops->sensor_activate)
            _sensorIds[i].ops->sensor_activate(enabled);
        polldev->active[i] = enabled;
    }

    if (enabled == 0)
        polldev->mindelay = get_min_delay(polldev);

    pthread_mutex_lock(&polldev->mutex);
    for (i = 0; i < MAX_NUM_SENSORS; i++) {
        if (polldev->active[i] == 1) {
            pthread_cond_signal(&polldev->activated);
            break;
        }
    }
    pthread_mutex_unlock(&polldev->mutex);

    return ret;
}

static int
icdk_sensor_set_delay(struct sensors_poll_device_t *dev, int handle, int64_t ns)
{
    int i, hwdelay;
    SensorPoll *polldev = (void *)dev;

    D("%s: handle=%x delay=%lld", __FUNCTION__, handle, ns);

    for (i = 0; i < MAX_NUM_SENSORS; i++) {
        if (_sensorIds[i].ops->sensor_list.handle != handle)
            continue;

        hwdelay = _sensorIds[i].ops->sensor_list.minDelay;
	if (hwdelay == 0)
            break;

        polldev->delay[i] = ns / 1000;

        if (polldev->delay[i] < hwdelay * 1000)
            polldev->delay[i] = hwdelay * 1000;

        if (_sensorIds[i].ops->sensor_set_delay)
            _sensorIds[i].ops->sensor_set_delay(polldev->delay[i]);
    }

    polldev->mindelay = get_min_delay(polldev);

    return 0;
}

/** MODULE REGISTRATION SUPPORT
 **
 ** This is required so that hardware/libhardware/hardware.c
 ** will dlopen() this library appropriately.
 **/

/*
 * the following is the list of all supported sensors.
 * this table is used to build sSensorList declared below
 * according to which hardware sensors are reported as
 * available from the emulator (see get_sensors_list below)
 *
 * note: numerical values for maxRange/resolution/power were
 *       taken from the reference AK8976A implementation
 */
static struct sensor_t sensor_list[MAX_NUM_SENSORS];

static int sensors_get_sensors_list(struct sensors_module_t* module,
                                    struct sensor_t const** list)
{
    int i;

    for (i = 0; i < MAX_NUM_SENSORS; i++) {
        memcpy(&sensor_list[i],&(_sensorIds[i].ops->sensor_list),
               sizeof(struct sensor_t));
    }

    *list = sensor_list;
    return MAX_NUM_SENSORS;
}

static int open_sensors(const struct hw_module_t* module,
                        const char *name, struct hw_device_t **device)
{
    int status = 0;
    int i;

    D("%s: name=%s", __FUNCTION__, name);

    if (!strcmp(name, SENSORS_HARDWARE_POLL)) {
        SensorPoll *dev = malloc(sizeof(*dev));
        if (dev == NULL) {
            E("%s: fail to alloc SensorPoll", __FUNCTION__);
            return -ENOMEM;
        }

        memset(dev, 0, sizeof(*dev));

        dev->device.common.tag     = HARDWARE_DEVICE_TAG;
        dev->device.common.version = 0;
        dev->device.common.module  = (struct hw_module_t*) module;
        dev->device.common.close   = icdk_sensor_close;
        dev->device.poll           = icdk_sensor_poll;
        dev->device.activate       = icdk_sensor_activate;
        dev->device.setDelay       = icdk_sensor_set_delay;

        for (i = 0; i < MAX_NUM_SENSORS; i++)
            dev->delay[i] = DEFAULT_TIMEOUT;

        *device = &dev->device.common;

        icdk_sensor_open(*device);
        pthread_cond_init(&dev->activated, NULL);
        pthread_mutex_init(&dev->mutex, NULL);

        status = 0;
    }
    return status;
}

static struct hw_module_methods_t sensors_module_methods = {
    .open = open_sensors
};

struct sensors_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .version_major = 1,
        .version_minor = 0,
        .id = SENSORS_HARDWARE_MODULE_ID,
        .name = "Intel iCDK SENSORS Module",
        .author = "Intel Corporation",
        .methods = &sensors_module_methods,
    },
  .get_sensors_list = sensors_get_sensors_list
};
