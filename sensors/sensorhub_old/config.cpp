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

#include "sensors.h"
#include "LightSensor.h"
#include "ProximitySensor.h"
#include "AccelSensor.h"
#include "GyroSensor.h"
#include "MagneticSensor.h"
#include "PressureSensor.h"
#include "LinearAccelSensor.h"
#include "GravitySensor.h"
#include "RotationVectorSensor.h"
#include "OrientationSensor.h"
#include "TerminalSensor.h"
#include "GestureFlickSensor.h"
#include "GestureSensor.h"
#include "PhysicalActivitySensor.h"
#include "PedometerSensor.h"
#include "ShakeSensor.h"
#include "SimpleTappingSensor.h"

#include <stdlib.h>
#include <string.h>
#include <cutils/properties.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#define XML_FILE "/system/etc/sensor_hal_config_default.xml"

static struct sensor_t *sensor_list;
static int sensor_count;
void (* sensor_platform_finalize)();

static int sensor_config_init_xml();
static int sensor_config_init_sensors(xmlNodePtr node);
static int sensor_config_get_sensor(xmlNodePtr node, struct sensor_t *sensor_item);
static int sensor_config_get_handle(const char *type, unsigned int instanceNumber);
static int sensor_config_get_type(const char *type);
static float sensor_config_get_unit(char *unit_name);
static int sensor_config_detect_sensor(int type, unsigned int instanceNumber);
static void sensor_config_finalize();
static char *generate_string(xmlChar *str);
static void dump();

static SensorBase *platform_sensors[SENSORS_HANDLE_MAX + 1];

const struct sensor_t* get_platform_sensor_list(int *sensor_num)
{
    int ret = -1;
    *sensor_num = 0;

    if (!sensor_list) {
        ret = sensor_config_init_xml();
        if (ret) {
            LOGE("Init config xml error\n");
            return NULL;
        }
    }
    *sensor_num = sensor_count;

    return sensor_list;
}

SensorBase **get_platform_sensors()
{
    int num = sensor_count;

    for (int i = 0; i < num; i++) {
        int handle = sensor_list[i].handle;
        switch (handle) {
        case SENSORS_HANDLE_LIGHT:
        case SENSORS_HANDLE_LIGHT_SEC:
            platform_sensors[handle] = new LightSensor(handle);
            break;
        case SENSORS_HANDLE_PROXIMITY:
        case SENSORS_HANDLE_PROXIMITY_SEC:
            platform_sensors[handle] = new ProximitySensor(handle);
            break;
        case SENSORS_HANDLE_ACCELEROMETER:
        case SENSORS_HANDLE_ACCELEROMETER_SEC:
            platform_sensors[handle] = new AccelSensor(handle);
            break;
        case SENSORS_HANDLE_MAGNETIC_FIELD:
        case SENSORS_HANDLE_MAGNETIC_FIELD_SEC:
            platform_sensors[handle] = new MagneticSensor(handle);
            break;
        case SENSORS_HANDLE_GYROSCOPE:
        case SENSORS_HANDLE_GYROSCOPE_SEC:
            platform_sensors[handle] = new GyroSensor(handle);
            break;
        case SENSORS_HANDLE_PRESSURE:
        case SENSORS_HANDLE_PRESSURE_SEC:
            platform_sensors[handle] = new PressureSensor(handle);
            break;
        case SENSORS_HANDLE_GRAVITY:
            platform_sensors[handle] = new GravitySensor();
            break;
        case SENSORS_HANDLE_LINEAR_ACCELERATION:
            platform_sensors[handle] = new LinearAccelSensor();
            break;
        case SENSORS_HANDLE_ROTATION_VECTOR:
            platform_sensors[handle] = new RotationVectorSensor();
            break;
        case SENSORS_HANDLE_ORIENTATION:
            platform_sensors[handle] = new OrientationSensor();
            break;
        case SENSORS_HANDLE_GESTURE_FLICK:
            platform_sensors[handle] = new GestureFlickSensor();
            break;
        case SENSORS_HANDLE_GESTURE:
            platform_sensors[handle] = new GestureSensor();
            break;
        case SENSORS_HANDLE_TERMINAL:
            platform_sensors[handle] = new TerminalSensor();
            break;
        case SENSORS_HANDLE_PHYSICAL_ACTIVITY:
            platform_sensors[handle] = new PhysicalActivitySensor();
            break;
        case SENSORS_HANDLE_PEDOMETER:
            platform_sensors[handle] = new PedometerSensor();
            break;
        case SENSORS_HANDLE_SHAKE:
            platform_sensors[handle] = new ShakeSensor();
            break;
        case SENSORS_HANDLE_SIMPLE_TAPPING:
            platform_sensors[handle] = new SimpleTappingSensor();
            break;
        default:
            LOGE("No Sensor id handle %d found\n", handle);
            continue;
        }
    }

    return platform_sensors;
}

static int sensor_config_init_xml() {
    xmlDocPtr doc;
    xmlNodePtr root;
    int ret = -1;

    doc = xmlReadFile(XML_FILE, NULL, XML_PARSE_NOBLANKS);

    if (doc == NULL ) {
        LOGE("XML Document not parsed successfully.\n");
        return -1;
    }

    root = xmlDocGetRootElement(doc);
    if (root == NULL) {
        LOGE("Empty XML document\n");
        xmlFreeDoc(doc);
        return -1;
    }

    if (xmlStrcmp(root->name, (const xmlChar *) "sensor_hal_config")) {
        LOGE("Wrong XML document, cannot find \"sensor_hal_config\" element!\n");
        xmlFreeDoc(doc);
        return -1;
    }

    ret = sensor_config_init_sensors(root->xmlChildrenNode);
    if (ret) {
        LOGE("Init Sensors Error!\n");
        return ret;
    }

    dump();
    xmlFreeDoc(doc);
    sensor_platform_finalize = sensor_config_finalize;
    return 0;
}

static int sensor_config_init_sensors(xmlNodePtr node) {
    xmlNodePtr p = node;
    int count = 0;
    int i = 0;
    int type;
    unsigned int instanceNumber = 0;
    xmlChar *str = NULL;
    xmlChar *attr = NULL;

    while (p != NULL) {
        attr = xmlGetProp(p, (const xmlChar*)"instance_number");
        if (attr) {
            instanceNumber = atoi((char *)attr);
            xmlFree(attr);
        }
        else
            instanceNumber = 0;

        type = sensor_config_get_type((const char*)p->name);
        if (sensor_config_detect_sensor(type, instanceNumber))
            count++;
        p = p->next;
    }

    sensor_count = count;

    sensor_list = (sensor_t *)malloc(sizeof(struct sensor_t)*count);
    if (!sensor_list) {
        LOGE("malloc error!\n");
        return -1;
    }

    memset(sensor_list, 0, sizeof(struct sensor_t)*count);

    while (node != NULL) {
        D("node name: %s\n", node->name);
        attr = xmlGetProp(node, (const xmlChar*)"instance_number");
        if (attr) {
            instanceNumber = atoi((char *)attr);
            xmlFree(attr);
        }
        else
            instanceNumber = 0;

        type = sensor_config_get_type((const char*)node->name);
        if (!sensor_config_detect_sensor(type, instanceNumber)) {
            node = node->next;
            continue;
        }

        p = node->xmlChildrenNode;
        if (p != NULL) {
            (sensor_list + i)->handle = sensor_config_get_handle((const char *)node->name, instanceNumber);
            (sensor_list + i)->type = type;
        }
        while (p != NULL) {
            if ((!xmlStrcmp(p->name, (const xmlChar *)"sensor")))
                sensor_config_get_sensor(p->xmlChildrenNode, sensor_list + i);
            p = p->next;
        }
        i++;
        node = node->next;
    }

    D("Count: %d\n", count);
    return 0;
}

static int sensor_config_get_sensor(xmlNodePtr node, struct sensor_t *sensor_item) {
    xmlChar *str = NULL;
    xmlChar *attr = NULL;
    float value = 0;
    xmlNodePtr p = node;

    while (p != NULL) {
        str = xmlNodeGetContent(p);
        if (str == NULL || (!xmlStrcmp(str, (const xmlChar *)"0"))
            || (!xmlStrcmp(str, (const xmlChar *)""))) {
            if (str != NULL)
                xmlFree(str);
            p = p->next;
            continue;
        }

        if ((!xmlStrcmp(p->name, (const xmlChar *)"name"))) {
            sensor_item->name = generate_string(str);
        }
        else if ((!xmlStrcmp(p->name, (const xmlChar *)"vendor"))) {
            sensor_item->vendor = generate_string(str);
        }
        else if ((!xmlStrcmp(p->name, (const xmlChar *)"version"))) {
            sensor_item->version = atoi((char *)str);
        }
        else if ((!xmlStrcmp(p->name, (const xmlChar *)"maxRange"))) {
            value = atof((char *)str);
            attr = xmlGetProp(p, (const xmlChar*)"unit");
            sensor_item->maxRange = value * sensor_config_get_unit((char *)attr);
        }
        else if ((!xmlStrcmp(p->name, (const xmlChar *)"resolution"))) {
            value = atof((char *)str);
            attr = xmlGetProp(p, (const xmlChar*)"unit");
            sensor_item->resolution = value * sensor_config_get_unit((char *)attr);
        }
        else if ((!xmlStrcmp(p->name, (const xmlChar *)"power"))) {
            sensor_item->power = atof((char *)str);
        }
        else if ((!xmlStrcmp(p->name, (const xmlChar *)"minDelay"))) {
            sensor_item->minDelay = atoi((char *)str);
        }
        xmlFree(str);
        p = p->next;
    }
    return 0;
}

static int sensor_config_get_handle(const char *type, unsigned int instanceNumber) {
    if (!type)
        return -1;
    if (!strcmp(type, "light")) {
        if (instanceNumber == 2)
            return SENSORS_HANDLE_LIGHT_SEC;
        else
            return SENSORS_HANDLE_LIGHT;
    }
    else if (!strcmp(type, "proximity")) {
        if (instanceNumber == 2)
            return SENSORS_HANDLE_PROXIMITY_SEC;
        else
            return SENSORS_HANDLE_PROXIMITY;
    }
    else if (!strcmp(type, "accelerometer")) {
        if (instanceNumber == 2)
            return SENSORS_HANDLE_ACCELEROMETER_SEC;
        else
            return SENSORS_HANDLE_ACCELEROMETER;
    }
    else if (!strcmp(type, "compass")) {
        if (instanceNumber == 2)
            return SENSORS_HANDLE_MAGNETIC_FIELD_SEC;
        else
            return SENSORS_HANDLE_MAGNETIC_FIELD;
    }
    else if (!strcmp(type, "gyroscope")) {
        if (instanceNumber == 2)
            return SENSORS_HANDLE_GYROSCOPE_SEC;
        else
            return SENSORS_HANDLE_GYROSCOPE;
    }
    else if (!strcmp(type, "pressure")) {
        if (instanceNumber == 2)
            return SENSORS_HANDLE_PRESSURE_SEC;
        else
            return SENSORS_HANDLE_PRESSURE;
    }
    else if (!strcmp(type, "ambient_temperature"))
        return SENSORS_HANDLE_AMBIENT_TEMPERATURE;
    else if (!strcmp(type, "gravity"))
        return SENSORS_HANDLE_GRAVITY;
    else if (!strcmp(type, "linear_acceleration"))
        return SENSORS_HANDLE_LINEAR_ACCELERATION;
    else if (!strcmp(type, "rotation_vector"))
        return SENSORS_HANDLE_ROTATION_VECTOR;
    else if (!strcmp(type, "orientation"))
        return SENSORS_HANDLE_ORIENTATION;
    else if (!strcmp(type, "gesture_flick"))
        return SENSORS_HANDLE_GESTURE_FLICK;
    else if (!strcmp(type, "gesture"))
        return SENSORS_HANDLE_GESTURE;
    else if (!strcmp(type, "terminal"))
        return SENSORS_HANDLE_TERMINAL;
    else if (!strcmp(type, "physical_activity"))
        return SENSORS_HANDLE_PHYSICAL_ACTIVITY;
    else if (!strcmp(type, "pedometer"))
        return SENSORS_HANDLE_PEDOMETER;
    else if (!strcmp(type, "shake"))
        return SENSORS_HANDLE_SHAKE;
    else if (!strcmp(type, "simple_tapping"))
        return SENSORS_HANDLE_SIMPLE_TAPPING;
    return -1;
}

static int sensor_config_get_type(const char *type) {
    if (!type)
        return -1;

    if (!strcmp(type, "light"))
        return SENSOR_TYPE_LIGHT;
    else if (!strcmp(type, "proximity"))
        return SENSOR_TYPE_PROXIMITY;
    else if (!strcmp(type, "accelerometer"))
        return SENSOR_TYPE_ACCELEROMETER;
    else if (!strcmp(type, "compass"))
        return SENSOR_TYPE_MAGNETIC_FIELD;
    else if (!strcmp(type, "gyroscope"))
        return SENSOR_TYPE_GYROSCOPE;
    else if (!strcmp(type, "pressure"))
        return SENSOR_TYPE_PRESSURE;
    else if (!strcmp(type, "ambient_temperature"))
        return SENSOR_TYPE_AMBIENT_TEMPERATURE;
    else if (!strcmp(type, "gravity"))
        return SENSOR_TYPE_GRAVITY;
    else if (!strcmp(type, "linear_acceleration"))
        return SENSOR_TYPE_LINEAR_ACCELERATION;
    else if (!strcmp(type, "rotation_vector"))
        return SENSOR_TYPE_ROTATION_VECTOR;
    else if (!strcmp(type, "orientation"))
        return SENSOR_TYPE_ORIENTATION;
    else if (!strcmp(type, "gesture_flick"))
        return SENSOR_TYPE_GESTURE_FLICK;
    else if (!strcmp(type, "gesture"))
        return SENSOR_TYPE_GESTURE;
    else if (!strcmp(type, "terminal"))
        return SENSOR_TYPE_TERMINAL;
    else if (!strcmp(type, "physical_activity"))
        return SENSOR_TYPE_PHYSICAL_ACTIVITY;
    else if (!strcmp(type, "pedometer"))
        return SENSOR_TYPE_PEDOMETER;
    else if (!strcmp(type, "shake"))
        return SENSOR_TYPE_SHAKE;
    else if (!strcmp(type, "simple_tapping"))
        return SENSOR_TYPE_SIMPLE_TAPPING;
    return -1;
}

static float sensor_config_get_unit(char *unit_name) {
    if (!unit_name)
        return 1.0;

    if (!strcmp(unit_name, "GRAVITY_EARTH"))
        return GRAVITY_EARTH;
    else if (!strcmp(unit_name, "M_PI"))
        return M_PI;

    return 1.0;
}

static int sensor_config_detect_sensor(int type, unsigned int instanceNumber) {
    psh_sensor_t psh_sensor_type;
    handle_t handle;

    if(type < 0)
        return 0;

    switch (type) {
    case SENSOR_TYPE_LIGHT:
        if (instanceNumber == 2)
            psh_sensor_type = SENSOR_ALS_SEC;
        else
            psh_sensor_type = SENSOR_ALS;
        break;
    case SENSOR_TYPE_PROXIMITY:
        if (instanceNumber == 2)
            psh_sensor_type = SENSOR_PROXIMITY_SEC;
        else
            psh_sensor_type = SENSOR_PROXIMITY;
        break;
    case SENSOR_TYPE_ACCELEROMETER:
        if (instanceNumber == 2)
            psh_sensor_type = SENSOR_ACCELEROMETER_SEC;
        else
            psh_sensor_type = SENSOR_ACCELEROMETER;
        break;
    case SENSOR_TYPE_MAGNETIC_FIELD:
        if (instanceNumber == 2)
            psh_sensor_type = SENSOR_COMP_SEC;
        else
            psh_sensor_type = SENSOR_COMP;
        break;
    case SENSOR_TYPE_GYROSCOPE:
        if (instanceNumber == 2)
            psh_sensor_type = SENSOR_GYRO_SEC;
        else
            psh_sensor_type = SENSOR_GYRO;
        break;
    case SENSOR_TYPE_PRESSURE:
        if (instanceNumber == 2)
            psh_sensor_type = SENSOR_BARO_SEC;
        else
            psh_sensor_type = SENSOR_BARO;
        break;
    case SENSOR_TYPE_GRAVITY:
        psh_sensor_type = SENSOR_GRAVITY;
        break;
    case SENSOR_TYPE_LINEAR_ACCELERATION:
        psh_sensor_type = SENSOR_LINEAR_ACCEL;
        break;
    case SENSOR_TYPE_ROTATION_VECTOR:
        psh_sensor_type = SENSOR_ROTATION_VECTOR;
        break;
    case SENSOR_TYPE_ORIENTATION:
        psh_sensor_type = SENSOR_ORIENTATION;
        break;
    case SENSOR_TYPE_GESTURE_FLICK:
        psh_sensor_type = SENSOR_GESTURE_FLICK;
        break;
    case SENSOR_TYPE_GESTURE:
        psh_sensor_type = SENSOR_GS;
        break;
    case SENSOR_TYPE_TERMINAL:
        psh_sensor_type = SENSOR_TC;
        break;
    case SENSOR_TYPE_PHYSICAL_ACTIVITY:
        psh_sensor_type = SENSOR_ACTIVITY;
        break;
    case SENSOR_TYPE_PEDOMETER:
        psh_sensor_type = SENSOR_PEDOMETER;
        break;
    case SENSOR_TYPE_SHAKE:
        psh_sensor_type = SENSOR_SHAKING;
        break;
    case SENSOR_TYPE_SIMPLE_TAPPING:
        psh_sensor_type = SENSOR_STAP;
        break;
    default:
        return 0;
    }

    handle = psh_open_session(psh_sensor_type);
    if (handle == NULL) {
        LOGE("%s psh_sensor_type %d", __FUNCTION__, psh_sensor_type);
        return 0;
    }

    psh_close_session(handle);

    return 1;
}

static char *generate_string(xmlChar *str) {
    char *new_str = NULL;

    new_str = (char *)malloc(strlen((char *)str) + 1);
    if (new_str) {
        strcpy(new_str, (char *)str);
    }
    return new_str;
}

static void sensor_config_finalize() {
    int i = 0;

    if (sensor_list) {
        for (i = 0; i < sensor_count; i++) {
            if ((sensor_list + i)->name)
                free((void *)(sensor_list + i)->name);
            if ((sensor_list + i)->vendor)
                free((void *)(sensor_list + i)->vendor);
        }
        free(sensor_list);
    }
}

static void dump() {
    int i = 0;
    for (i = 0; i < sensor_count; i++) {
        D("********************************************************************\n");
        D("name: %s\n", (sensor_list + i)->name);
        D("vendor: %s\n", (sensor_list + i)->vendor);
        D("version: %d\n", (sensor_list + i)->version);
        D("handle: %d\n", (sensor_list + i)->handle);
        D("type: %d\n", (sensor_list + i)->type);
        D("maxRange: %f\n", (sensor_list + i)->maxRange);
        D("resolution: %f\n", (sensor_list + i)->resolution);
        D("power: %f\n", (sensor_list + i)->power);
        D("minDelay: %d\n", (sensor_list + i)->minDelay);
    }
}
