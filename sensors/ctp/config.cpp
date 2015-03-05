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
#include "../GyroSensor.h"
#include "../CompassSensor.h"
#include "../PressureSensor.h"

#include <stdlib.h>
#include <string.h>
#include <cutils/properties.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

static sensor_platform_config_t *sensor_configs;
static struct sensor_t *sensor_list;
static int sensor_count;
void (* sensor_platform_finalize)();

static int sensor_config_init_xml();
static int sensor_config_init_sensors(xmlNodePtr node);
static int sensor_config_get_platform_data(xmlNodePtr node, sensor_platform_config_t * config, const xmlChar *type);
static int sensor_config_get_sensor(xmlNodePtr node, struct sensor_t *sensor_item, const xmlChar *type);
static int sensor_config_get_handle(const char *type);
static int sensor_config_get_type(const char *type);
static int sensor_config_get_axis(char *axis);
static float sensor_config_get_unit(char *unit_name);
static void sensor_config_finalize();
static char *generate_string(xmlChar *str);
static void dump();

static SensorBase **platform_sensors;

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

    for (int i = 0; i < sensor_count; i++) {
        if (sensor_list[i].handle != sensor_configs[i].handle) {
            E("Sensor config sequence error\n");
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
        case SENSORS_HANDLE_GYROSCOPE:
            platform_sensors[i] = new GyroSensor(&sensor_configs[i]);
            break;
        case SENSORS_HANDLE_PRESSURE:
            platform_sensors[i] = new PressureSensor(&sensor_configs[i]);
            break;
        default:
            E("Error, no Sensor ID handle %d found\n", handle);
            return NULL;
        }
    }

    return platform_sensors;
}

static int sensor_config_init_xml() {
    xmlDocPtr doc;
    xmlNodePtr root;
    char buf[PROPERTY_VALUE_MAX];
    char *file_prefix = "/system/etc/sensor_hal_config_";
    char file[PROPERTY_VALUE_MAX + 34];
    int ret = -1;

    ret = property_get("ro.sensors.mapper", buf, NULL);
    if (ret <= 0) {
        LOGE("Get sensors mapper property error! Use default config.\n");
	strcpy(buf, "default");
    }
    strcpy(file, file_prefix);
    strcat(file, buf);
    strcat(file, ".xml");
    D("config file: %s\n", file);

    doc = xmlReadFile(file, NULL, XML_PARSE_NOBLANKS);

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
    xmlChar *str = NULL;

    while (p != NULL) {
        count++;
        p = p->next;
    }

    sensor_count = count;

    sensor_configs = (sensor_platform_config_t *)malloc(sizeof(sensor_platform_config_t)*count);
    if (!sensor_configs) {
        LOGE("malloc error!\n");
        return -1;
    }

    sensor_list = (sensor_t *)malloc(sizeof(struct sensor_t)*count);
    if (!sensor_list) {
        LOGE("malloc error!\n");
        return -1;
    }

    platform_sensors = (SensorBase **)malloc(sizeof(SensorBase *)*count);
    if (!platform_sensors) {
        LOGE("malloc error!\n");
        return -1;
    }

    memset(sensor_configs, 0, sizeof(sensor_platform_config_t)*count);
    memset(sensor_list, 0, sizeof(struct sensor_t)*count);
    memset(platform_sensors, 0, sizeof(SensorBase *)*count);

    while (node != NULL) {
        D("node name: %s\n", node->name);
        p = node->xmlChildrenNode;
        while (p != NULL) {
            if ((!xmlStrcmp(p->name, (const xmlChar *)"platform_config")))
                sensor_config_get_platform_data(p->xmlChildrenNode, sensor_configs + i, node->name);
            else if ((!xmlStrcmp(p->name, (const xmlChar *)"sensor")))
                sensor_config_get_sensor(p->xmlChildrenNode, sensor_list + i, node->name);
            p = p->next;
        }
        i++;
        node = node->next;
    }

    D("Count: %d\n", count);
    return 0;
}


static int sensor_config_get_platform_data(xmlNodePtr node, sensor_platform_config_t * config, const xmlChar *type) {
    xmlChar *str = NULL;
    xmlChar *attr = NULL;
    union sensor_data_t *temp;
    xmlNodePtr p = node;

    if (p != NULL) {
        config->handle = sensor_config_get_handle((const char *)type);
    }

    while (p != NULL) {
        if (p->properties) {
            if ((!xmlStrcmp(p->name, (const xmlChar *)"mapper"))) {
                attr = xmlGetProp(p, (const xmlChar*)"axis_x");
                if (attr) {
                    config->mapper[0] = sensor_config_get_axis((char *)attr);
                    xmlFree(attr);
                }
                attr = xmlGetProp(p, (const xmlChar*)"axis_y");
                if (attr) {
                    config->mapper[1] = sensor_config_get_axis((char *)attr);
                    xmlFree(attr);
                }
                attr = xmlGetProp(p, (const xmlChar*)"axis_z");
                if (attr) {
                    config->mapper[2] = sensor_config_get_axis((char *)attr);
                    xmlFree(attr);
                }
            }
            else if ((!xmlStrcmp(p->name, (const xmlChar *)"scale"))) {
                attr = xmlGetProp(p, (const xmlChar*)"axis_x");
                if (attr) {
                    config->scale[0] = atof((char *)attr);
                    xmlFree(attr);
                }
                attr = xmlGetProp(p, (const xmlChar*)"axis_y");
                if (attr) {
                    config->scale[1] = atof((char *)attr);
                    xmlFree(attr);
                }
                attr = xmlGetProp(p, (const xmlChar*)"axis_z");
                if (attr) {
                    config->scale[2] = atof((char *)attr);
                    xmlFree(attr);
                }
            }
            else if ((!xmlStrcmp(p->name, (const xmlChar *)"range"))) {
                attr = xmlGetProp(p, (const xmlChar*)"min");
                if (attr) {
                    config->range[0] = atof((char *)attr);
                    xmlFree(attr);
                }
                attr = xmlGetProp(p, (const xmlChar*)"max");
                if (attr) {
                    config->range[1] = atof((char *)attr);
                    xmlFree(attr);
                }
            }
            else if ((!xmlStrcmp(p->name, (const xmlChar *)"priv_data"))) {
                attr = xmlGetProp(p, (const xmlChar*)"compass_filter_en");
                if (attr) {
                    temp = (sensor_data_t *)malloc(sizeof(union sensor_data_t));
                    if(!temp)
                        LOGE("malloc priv_data error!\n");
                    else
                        temp->compass_filter_en = atoi((char *)attr);
                    config->priv_data = temp;
                    xmlFree(attr);
                }
                else if ((attr = xmlGetProp(p, (const xmlChar*)"light_glass_factor"))) {
                    temp = (sensor_data_t *)malloc(sizeof(union sensor_data_t));
                    if(!temp)
                        LOGE("malloc priv_data error!\n");
                    else
                        temp->light_glass_factor = atof((char *)attr);
                    config->priv_data = temp;
                    xmlFree(attr);
                }
            }
            p = p->next;
            continue;
        }

        str = xmlNodeGetContent(p);
        if (str == NULL) {
            p = p->next;
            continue;
        }
	if ((!xmlStrcmp(str, (const xmlChar *)"0")) || (!xmlStrcmp(str, (const xmlChar *)""))) {
            xmlFree(str);
            p = p->next;
            continue;
        }

        if ((!xmlStrcmp(p->name, (const xmlChar *)"name"))) {
            config->name = generate_string(str);
        }
        else if ((!xmlStrcmp(p->name, (const xmlChar *)"activate_path"))) {
            config->activate_path = generate_string(str);
        }
        else if ((!xmlStrcmp(p->name, (const xmlChar *)"poll_path"))) {
            config->poll_path = generate_string(str);
        }
        else if ((!xmlStrcmp(p->name, (const xmlChar *)"data_path"))) {
            config->data_path = generate_string(str);
        }
        else if ((!xmlStrcmp(p->name, (const xmlChar *)"config_path"))) {
            config->config_path = generate_string(str);
        }
        else if ((!xmlStrcmp(p->name, (const xmlChar *)"min_delay"))) {
            config->min_delay = atoi((char *)str);
        }
        xmlFree(str);
        p = p->next;
    }
    return 0;
}

static int sensor_config_get_sensor(xmlNodePtr node, struct sensor_t *sensor_item, const xmlChar *type) {
    xmlChar *str = NULL;
    xmlChar *attr = NULL;
    float value = 0;
    xmlNodePtr p = node;

    if (p != NULL) {
        sensor_item->handle = sensor_config_get_handle((const char *)type);
        sensor_item->type = sensor_config_get_type((const char *)type);
    }
    while (p != NULL) {
        str = xmlNodeGetContent(p);
        if (str == NULL || (!xmlStrcmp(str, (const xmlChar *)"0")) || (!xmlStrcmp(str, (const xmlChar *)""))) {
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

static int sensor_config_get_handle(const char *type) {
    if (!type)
        return -1;
    if (!strcmp(type, "light"))
        return SENSORS_HANDLE_LIGHT;
    else if (!strcmp(type, "proximity"))
        return SENSORS_HANDLE_PROXIMITY;
    else if (!strcmp(type, "accelerometer"))
        return SENSORS_HANDLE_ACCELEROMETER;
    else if (!strcmp(type, "compass"))
        return SENSORS_HANDLE_MAGNETIC_FIELD;
    else if (!strcmp(type, "gyroscope"))
        return SENSORS_HANDLE_GYROSCOPE;
    else if (!strcmp(type, "pressure"))
        return SENSORS_HANDLE_PRESSURE;
    else if (!strcmp(type, "ambient_temperature"))
        return SENSORS_HANDLE_AMBIENT_TEMPERATURE;
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
    return -1;
}

static int sensor_config_get_axis(char *axis) {
    if (!axis)
        return -1;

    if (!strcmp(axis, "X"))
        return AXIS_X;
    if (!strcmp(axis, "Y"))
        return AXIS_Y;
    if (!strcmp(axis, "Z"))
        return AXIS_Z;

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

    if (sensor_configs) {
        for (i = 0; i < sensor_count; i++) {
            if ((sensor_configs + i)->name)
                free((void *)(sensor_configs + i)->name);
            if ((sensor_configs + i)->activate_path)
                free((void *)(sensor_configs + i)->activate_path);
            if ((sensor_configs + i)->poll_path)
                free((void *)(sensor_configs + i)->poll_path);
            if ((sensor_configs + i)->data_path)
                free((void *)(sensor_configs + i)->data_path);
            if ((sensor_configs + i)->priv_data)
                free((void *)(sensor_configs + i)->priv_data);
        }
        free(sensor_configs);
    }
    if (sensor_list) {
        for (i = 0; i < sensor_count; i++) {
            if ((sensor_list + i)->name)
                free((void *)(sensor_list + i)->name);
            if ((sensor_list + i)->vendor)
                free((void *)(sensor_list + i)->vendor);
        }
        free(sensor_list);
    }
    if (platform_sensors)
        free(platform_sensors);
}

static void dump() {
    int i = 0;
    for (i = 0; i < sensor_count; i++) {
        D("####################################################################\n");
        D("handle: %d\n", (sensor_configs + i)->handle);
        D("name %s\n", (sensor_configs + i)->name);
        D("activate_path: %s\n", (sensor_configs + i)->activate_path);
        D("poll_path: %s\n", (sensor_configs + i)->poll_path);
        D("data_path: %s\n", (sensor_configs + i)->data_path);
        D("config_path: %s\n", (sensor_configs + i)->config_path);
        D("mapper: %d %d %d\n", (sensor_configs + i)->mapper[0], (sensor_configs + i)->mapper[1], (sensor_configs + i)->mapper[2]);
        D("scale: %f %f %f\n", (sensor_configs + i)->scale[0], (sensor_configs + i)->scale[1], (sensor_configs + i)->scale[2]);
        D("range: %f %f\n", (sensor_configs + i)->range[0], (sensor_configs + i)->range[1]);
        D("min_delay: %d\n", (sensor_configs + i)->min_delay);
        D("priv_data: %x\n", (sensor_configs + i)->priv_data);
        if ((sensor_configs + i)->priv_data) {
            D("compass_filter_en: %d\n", (sensor_configs + i)->priv_data->compass_filter_en);
            D("light_glass_factor: %f\n", (sensor_configs + i)->priv_data->light_glass_factor);
        }

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
