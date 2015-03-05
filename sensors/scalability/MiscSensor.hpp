#ifndef _MISC_SENSOR_HPP_
#define _MISC_SENSOR_HPP_

#include "DirectSensor.hpp"

class MiscSensor : public DirectSensor {
public:
        MiscSensor(SensorDevice &mDevice, struct PlatformData &mData) :DirectSensor(mDevice, mData) {}
        ~MiscSensor()
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
