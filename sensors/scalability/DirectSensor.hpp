#ifndef _DIRECT_SENSOR_HPP_
#define _DIRECT_SENSOR_HPP_
#include <map>
#include <string>
#include "Sensor.hpp"
#include "ConfigData.hpp"
#include "sensorcalibration/SensorCalibration.h"

class DirectSensor : public Sensor {
protected:
        struct PlatformData data;
        void* calibrationMethodsHandle;
        void (*Calibration)(struct sensors_event_t* event, calibration_flag_t flag, const char* configFile);
        void (*DriverCalibration)(struct sensors_event_t* event, calibration_flag_t flag, const char* configFile, const char* configNode);
public:
        DirectSensor(SensorDevice &mDevice, struct PlatformData &mData);
        ~DirectSensor();
        struct PlatformData& getPlatformData() { return data; }
        virtual int getPollfd() = 0;
        virtual int activate(int handle, int enabled) { return 0;}
        virtual int setDelay(int handle, int64_t ns) { return 0; }
        virtual int getData(std::queue<sensors_event_t> &eventQue) = 0;
        virtual bool selftest() = 0;
};

#endif
