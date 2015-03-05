#ifndef _SENSOR_DEVICE_H_
#define _SENSOR_DEVICE_H_
#include <utils/Log.h>
#include <hardware/sensors.h>
#include <cstring>

typedef enum {
        AXIS_X = 0,
        AXIS_Y,
        AXIS_Z,
        AXIS_W,
        AXIS_MAX,
        AXIS_OTHER = 255
} axis_t;

typedef enum {
        LINUX_DRIVER = 0,
        LIBSENSORHUB
} sensor_category_t;

typedef enum {
        OTHER = 0,
        VECTOR,
        SCALAR
} sensors_event_property_t;

typedef enum {
        PRIMARY = 0,
        SECONDARY = 1,
} sensors_subname;

class SensorDevice {
        struct sensor_t dev;
        int id; //start from 0
        sensor_category_t category; // 1 for PSH sensor, 0 for direct sensor
        sensors_event_property_t eventProperty;
        sensors_subname subname;
        int mapper[4];
        float scale[4];
public:
        SensorDevice()
        {
                std::memset(&dev, 0, sizeof(dev));
                id = 0;
                category = LINUX_DRIVER;
                subname = PRIMARY;
                for (int i = 0; i < AXIS_MAX; i++) {
                        mapper[i] = i;
                        scale[i] = 1.0;
                }
        }
        SensorDevice(const SensorDevice &device);
        ~SensorDevice() {
                if (dev.name != NULL) {
                        delete[] dev.name;
                }
                if (dev.vendor != NULL) {
                        delete[] dev.vendor;
                }
        }
        SensorDevice& operator=(const SensorDevice &device);
        int getId() { return id; }
        void setId(const int &new_id) { id = new_id; }
        static int idToHandle(int id) { return id + 1; }
        static int handleToId(int handle) { return handle - 1 ; }
        sensor_category_t getCategory() { return category; }
        void setCategory(const sensor_category_t &new_category) { category = new_category; }
        sensors_event_property_t getEventProperty() { return eventProperty; }
        void setEventProperty(const sensors_event_property_t &newEventProperty) { eventProperty = newEventProperty; }
        sensors_subname getSubname() { return subname; }
        void setSubname(const sensors_subname &new_subname) { subname = new_subname; }
        int getMapper(unsigned int index) { return index < AXIS_MAX ? mapper[index] : -1; }
        void setMapper(unsigned int index, int value) { if (index < AXIS_MAX) mapper[index] = value; }
        float getScale(unsigned int index) { return index < AXIS_MAX ? scale[index] : 1; }
        void setScale(unsigned int index, float value) { if (index < AXIS_MAX) scale[index] = value; }
        const char* getName() { return dev.name; }
        void setName(const char* name);
        const char* getVendor() { return dev.vendor; }
        void setVendor(const char* vendor);
        int getVersion() { return dev.version; }
        void setVersion(int version) { dev.version = version; }
        int getHandle() { return dev.handle; }
        void setHandle(int handle) { dev.handle = handle; }
        int getType() { return dev.type; }
        void setType(int type) { dev.type = type; }
        float getMaxRange() { return dev.maxRange; }
        void setMaxRange(float maxRange) { dev.maxRange = maxRange; }
        float getResolution() { return dev.resolution; }
        void setResolution(float resolution) { dev.resolution = resolution; }
        float getPower() { return dev.power; }
        void setPower(float power) { dev.power = power; }
        int32_t getMinDelay() { return dev.minDelay; }
        void setMinDelay(int32_t minDelay) { dev.minDelay = minDelay; }
        void copyItem(struct sensor_t* item) { memcpy(item, &dev, sizeof(struct sensor_t)); }
};

#endif
