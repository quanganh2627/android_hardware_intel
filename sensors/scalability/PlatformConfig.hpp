#ifndef _PLATFORM_CONFIG_HPP_
#define _PLATFORM_CONFIG_HPP_
#include <string>
#include <map>
#include <vector>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include "SensorDevice.hpp"
#include "ConfigData.hpp"


class PlatformConfig {
        std::map<int, struct PlatformData> configs;
        std::vector<SensorDevice> devices;
        int count;
        bool initialized;
        bool initXML(xmlNodePtr node);
        bool addPlatformData(xmlNodePtr node, std::string type);
        bool addSensorDevice(xmlNodePtr node, std::string type, std::string category);
        int getType(std::string type);
        sensor_category_t getCategory(std::string category);
        sensors_event_property_t getEventProperty(int type);
        sensor_driver_node_type getDriverNodeType(std::string nodeType);
        int getAxis(std::string axis);
        float getUnit(std::string unitName);
public:
        PlatformConfig();
        unsigned int size() { return devices.size(); }
        bool getPlatformData(int id, struct PlatformData &data);
        bool getSensorDevice(int id, SensorDevice &device);
};
#endif
