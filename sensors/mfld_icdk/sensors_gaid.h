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

#include<sys/select.h>

#include <cutils/log.h>

//#define SENSOR_DBG

#ifdef SENSOR_DBG
#define  D(...)  LOGD(__VA_ARGS__)
#else
#define  D(...)  ((void)0)
#endif

#define  E(...)  LOGE(__VA_ARGS__)

#define  ID_BASE           SENSORS_HANDLE_BASE
#define  ID_ACCELEROMETER  (ID_BASE+0)
#define  ID_MAGNETIC_FIELD (ID_BASE+1)
#define  ID_LIGHT          (ID_BASE+3)
#define  ID_PROXIMITY      (ID_BASE+4)
#define  ID_ORIENTATION    (ID_BASE+5)

#define S_HANDLE_ACCELEROMETER  (1<<ID_ACCELEROMETER)
#define S_HANDLE_MAGNETIC_FIELD (1<<ID_MAGNETIC_FIELD)
#define S_HANDLE_LIGHT          (1<<ID_LIGHT)
#define S_HANDLE_PROXIMITY      (1<<ID_PROXIMITY)
#define S_HANDLE_ORIENTATION    (1<<ID_ORIENTATION)

#define GAID_SENSORS_(x,y)  { S_HANDLE_##x , -1, &gaid_sensors_##y},

typedef struct {
    int handle;
    int (*sensor_data_open)(void);
    void (*sensor_set_fd)(fd_set *fds);
    int (*sensor_is_fd)(fd_set *fds);
    int (*sensor_read)(sensors_event_t *data);
    void (*sensor_data_close)(void);
    int (*sensor_activate)(int enabled);
    int (*sensor_set_delay)(int ms);
    struct sensor_t sensor_list;
} sensors_ops_t;

extern sensors_ops_t gaid_sensors_accelerometer;
extern sensors_ops_t gaid_sensors_thermal;
extern sensors_ops_t gaid_sensors_compass;
extern sensors_ops_t gaid_sensors_als;
extern sensors_ops_t gaid_sensors_proximity;
extern sensors_ops_t gaid_sensors_orientation;

#define  GAID_SENSORS_DATA  \
    GAID_SENSORS_(ACCELEROMETER,accelerometer) \
    GAID_SENSORS_(MAGNETIC_FIELD,compass) \
    GAID_SENSORS_(LIGHT,als) \
    GAID_SENSORS_(PROXIMITY,proximity) \
    GAID_SENSORS_(ORIENTATION,orientation)


#define NSEC_PER_SEC    1000000000L
static inline int64_t timespec_to_ns(const struct timespec *ts)
{
    return ((int64_t) ts->tv_sec * NSEC_PER_SEC) + ts->tv_nsec;
}

