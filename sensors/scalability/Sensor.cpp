#include "Sensor.hpp"

Sensor::Sensor()
{
        pollfd = -1;
        memset(&event, 0, sizeof(sensors_event_t));
        event.version = sizeof(sensors_event_t);
}

Sensor::Sensor(SensorDevice &mDevice)
{
        pollfd = -1;
        device = mDevice;
        memset(&event, 0, sizeof(sensors_event_t));
        event.version = sizeof(sensors_event_t);
        event.sensor = device.getHandle();
        event.type = device.getType();
}

void Sensor::resetEventHandle()
{
        event.sensor = device.getHandle();
}
