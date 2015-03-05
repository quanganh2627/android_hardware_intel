/**
 * Copyright 2013 - 2014 (c) Intel Corporation. All rights reserved.

 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at

 * http://www.apache.org/licenses/LICENSE-2.0

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>

#include <sys/types.h>
#include <cutils/android_reboot.h>

#include <thermald.h>

/* Temperatures are in milli degree celcius */
static struct thermal_zone thermal_zones[] = {
    {"skin0", -1, 65000},
    {"skin1", -1, 70000},
    {"SYSTHERM0", -1, 64000},
    {"SYSTHERM1", -1, 74000},
    {"max17047_battery", -1, 60000},
    {"byt_battery", -1, 64000},
    {"skin0_1", -1, 65000},
};

int read_sysfs_type(const char *path, char *buf, size_t sz)
{
    int fd;
    size_t cnt;

    fd = open(path, O_RDONLY, 0);
    if (fd < 0)
        goto err;

    cnt = read(fd, buf, sz - 1);
    if (cnt <= 0)
        goto err;
    if (buf[cnt - 1] == '\n')
        cnt--;

    buf[cnt] = '\0';
    close(fd);
    return cnt;

err:
    if (fd >= 0)
        close(fd);
    return -1;
}

int read_sysfs_temp(const char *path, int *val)
{
    char buf[32];
    int ret;
    int tmp;
    char *end;

    ret = read_sysfs_type(path, buf, sizeof(buf));
    if (ret < 0)
        return -1;

    tmp = strtol(buf, &end, 0);
    if (end == buf ||
        ((end < buf+sizeof(buf)) && (*end != '\n' && *end != '\0')))
        return -1;

    *val = tmp;
    return 0;
}

int get_zone_temp(int zone_index)
{
    char buffer[THERMAL_SYSFS_LENGTH];
    int temp, status;

    snprintf(buffer, sizeof(buffer), TEMP_PATH, zone_index);
    status = read_sysfs_temp(buffer, &temp);

    if (status) {
        LOGE("Error in reading temperature for zone index %d", zone_index);
        temp = 0;
    }
    return temp;
}

int get_zone_index(char * zone_name)
{
    char buffer[THERMAL_NAME_LENGTH];
    char path[THERMAL_SYSFS_LENGTH];
    int zone_index = -1, i;

    for (i = 0; i < sizeof(thermal_zones)/sizeof(struct thermal_zone); i++) {
        snprintf(path, sizeof(path), TYPE_PATH, i);
        read_sysfs_type(path, buffer, THERMAL_NAME_LENGTH);
        if (!strncmp(buffer, zone_name, sizeof(buffer))) {
            zone_index = i;
            break;
        }
    }

    return zone_index;
}

static void temperature_check()
{
    int zone_temp, i;

    for (i = 0; i < sizeof(thermal_zones)/sizeof(struct thermal_zone); i++) {
        if (thermal_zones[i].index == -1) continue;
        zone_temp = get_zone_temp(thermal_zones[i].index);
        if (zone_temp >= thermal_zones[i].critical_temp) {
            LOGI("Temperature(%d) of %s skin is higher than threshold(%d)",
                zone_temp, thermal_zones[i].name, thermal_zones[i].critical_temp);
            system("echo 1 > /sys/module/intel_mid_osip/parameters/force_shutdown_occured");
            LOGW("Powering off the platform by Thermal daemon due to critical temperature");
            android_reboot(ANDROID_RB_POWEROFF, 0, NULL);
        }
    }
}

static void thermal_loop()
{
    LOGI("Thermal daemon started.");
    while(1) {
        temperature_check();
        sleep(5);
    }
}

static void thermal_init()
{
    int i;
    for (i = 0; i < sizeof(thermal_zones)/sizeof(struct thermal_zone); i++) {
        thermal_zones[i].index = get_zone_index(thermal_zones[i].name);
    }
}

int main(int argc, char **argv)
{
    thermal_init();
    thermal_loop();
    return 0;
}
