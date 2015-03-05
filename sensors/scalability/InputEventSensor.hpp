#ifndef _INPUT_EVENT_SENSOR_HPP_
#define _INPUT_EVENT_SENSOR_HPP_

#include "DirectSensor.hpp"

class InputEventSensor : public DirectSensor {
        int openFile(std::string &pathset);
        int writeToFile(std::string &pathset, int handle, int64_t value);
        bool inputDataOverrun;
public:
        InputEventSensor(SensorDevice &mDevice, struct PlatformData &mData);
        ~InputEventSensor()
        {
                if (pollfd >= 0)
                        close(pollfd);
        }
        int getPollfd();
        int activate(int handle, int enabled);
        int setDelay(int handle, int64_t ns);
        int getData(std::queue<sensors_event_t> &eventQue);
        bool selftest();
};

#endif
