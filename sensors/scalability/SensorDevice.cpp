#include "SensorDevice.hpp"
SensorDevice::SensorDevice(const SensorDevice &device)
{
        char* str;
        int size;

        memset(&dev, 0, sizeof(dev));
        if (device.dev.name != NULL) {
                size = strlen(device.dev.name);
                str = new char[size + 1];
                strcpy(str, device.dev.name);
                dev.name = str;
        }
        if (device.dev.vendor != NULL) {
                size = strlen(device.dev.vendor);
                str = new char[size + 1];
                strcpy(str, device.dev.vendor);
                dev.vendor = str;
        }
        dev.version = device.dev.version;
        dev.handle = device.dev.handle;
        dev.type = device.dev.type;
        dev.maxRange = device.dev.maxRange;
        dev.resolution = device.dev.resolution;
        dev.power = device.dev.power;
        dev.minDelay = device.dev.minDelay;
        id = device.id;
        category = device.category;
        subname = device.subname;
        eventProperty = device.eventProperty;
        for (int i = 0; i < AXIS_MAX; i++) {
                mapper[i] = device.mapper[i];
                scale[i] = device.scale[i];
        }
}

SensorDevice& SensorDevice::operator=(const SensorDevice &device)
{
        char* str;
        int size;
        if (this == &device)
                return *this;

        if (dev.name != NULL)
                delete[] dev.name;
        if (dev.vendor != NULL)
                delete[] dev.vendor;

        memset(&dev, 0, sizeof(dev));
        if (device.dev.name != NULL) {
                size = strlen(device.dev.name);
                str = new char[size + 1];
                strcpy(str, device.dev.name);
                dev.name = str;
        }
        if (device.dev.vendor != NULL) {
                size = strlen(device.dev.vendor);
                str = new char[size + 1];
                strcpy(str, device.dev.vendor);
                dev.vendor = str;
        }
        dev.version = device.dev.version;
        dev.handle = device.dev.handle;
        dev.type = device.dev.type;
        dev.maxRange = device.dev.maxRange;
        dev.resolution = device.dev.resolution;
        dev.power = device.dev.power;
        dev.minDelay = device.dev.minDelay;
        id = device.id;
        category = device.category;
        subname = device.subname;
        eventProperty = device.eventProperty;
        for (int i = 0; i < AXIS_MAX; i++) {
                mapper[i] = device.mapper[i];
                scale[i] = device.scale[i];
        }
        return *this;
}

void SensorDevice::setName(const char* name) {
        if (name != NULL) {
                if (dev.name != NULL)
                        delete[] dev.name;
                int size = strlen(name);
                char* str = new char[size + 1];
                strcpy(str, name);
                dev.name = str;
        }
}

void SensorDevice::setVendor(const char* vendor)
{
        if (vendor != NULL) {
                if (dev.vendor != NULL)
                        delete[] dev.vendor;
                int size = strlen(vendor);
                char* str = new char[size + 1];
                strcpy(str, vendor);
                dev.vendor = str;
        }
}


