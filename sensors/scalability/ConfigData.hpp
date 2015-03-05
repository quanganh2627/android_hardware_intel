#ifndef _CONFIG_DATA_HPP_
#define _CONFIG_DATA_HPP_
#include <string>

typedef enum {
        INPUT_EVENT = 0,
        MISC
} sensor_driver_node_type;

struct PlatformData {
        std::string name;
        std::string activateInterface;
        std::string setDelayInterface;
        std::string dataInterface;
        std::string calibrationFile;
        std::string calibrationFunc;
        std::string driverCalibrationInterface;
        std::string driverCalibrationFile;
        std::string driverCalibrationFunc;
        sensor_driver_node_type driverNodeType;
        bool filterLength;
};

#endif
