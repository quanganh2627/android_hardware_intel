#ifndef _SENSOR_HPP_
#define _SENSOR_HPP_
#include "SensorDevice.hpp"
#include "utils.hpp"
#include <queue>

#define SENSOR_NOPOLL   0x7fffffff
#define NS_TO_MS 1000000
#define US_TO_MS 1000
#define ACCEL_FILTER 0.1

class Sensor {
protected:
        SensorDevice device;
        sensors_event_t event;
        int pollfd;
public:
        Sensor();
        Sensor(SensorDevice &device);
        virtual ~Sensor() {}
        SensorDevice& getDevice() { return device; }
        void resetEventHandle();
        virtual int getPollfd() = 0;
        virtual int activate(int handle, int enabled) { return 0; }
        virtual int setDelay(int handle, int64_t ns) { return 0; }
        virtual int getData(std::queue<sensors_event_t> &eventQue) = 0;
        virtual bool selftest() = 0;
};

#endif
