#ifndef _PSH_SENSOR_HPP_
#define _PSH_SENSOR_HPP_
#include "Sensor.hpp"
#include "SensorHubHelper.hpp"
#include "VirtualSensor.hpp"

struct sensor_hub_methods {
        handle_t (*psh_open_session)(psh_sensor_t sensor_type);
        void (*psh_close_session)(handle_t handle);
        int (*psh_get_fd)(handle_t handle);
        error_t (*psh_start_streaming)(handle_t handle, int data_rate, int buffer_delay);
        error_t (*psh_start_streaming_with_flag)(handle_t handle, int data_rate, int buffer_delay, streaming_flag flag);
        error_t (*psh_stop_streaming)(handle_t handle);
        error_t (*psh_set_property)(handle_t handle, property_type prop_type, void *value);
};

class PSHSensor : public Sensor {
protected:
        static struct sensor_hub_methods methods;
        void* methodsHandle;
        handle_t sensorHandle;
        bool activated;
private:
        bool SensorHubMethodsInitialize();
        bool SensorHubMethodsFinallize();
public:
        PSHSensor();
        PSHSensor(SensorDevice &mDevice);
        ~PSHSensor();
        virtual int getPollfd() = 0;
        virtual int activate(int handle, int enabled) { return 0; }
        virtual int setDelay(int handle, int64_t ns) { return 0; }
        virtual int getData(std::queue<sensors_event_t> &eventQue) = 0;
        virtual bool selftest() = 0;
};

#endif
