#include "MiscSensor.hpp"
#include <sstream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define IO_CMD_ACTIVATE 0
#define IO_CMD_SETDELAY 1

typedef struct {
        union {
                int value;
                int64_t timestamp;
        };
        axis_t axis;
} sensors_misc_event_t;

int MiscSensor::getPollfd()
{
        if (pollfd >= 0)
                return pollfd;

        pollfd = open(data.dataInterface.c_str(), O_RDONLY);
        if (pollfd < 0)
                LOGE("%s: line: %d: open %s error!", __FUNCTION__, __LINE__, data.activateInterface.c_str());

        return pollfd;
}

int MiscSensor::activate(int handle, int enabled) {
        static int activated = 0;
        int fd, ret;

        if (handle != device.getHandle()) {
                LOGE("%s: line: %d: %s handle not match! handle: %d required handle: %d",
                     __FUNCTION__, __LINE__, data.name.c_str(), device.getHandle(), handle);
                return -1;
        }

        enabled = enabled == 0 ? 0 : 1;

        fd = getPollfd();
        if (fd < 0) {
                LOGE("%s: line: %d: %s getPollfd error!", __FUNCTION__, __LINE__, data.name.c_str());
                return -1;
        }

        if (Calibration != NULL) {
                if (enabled && !activated)
                        Calibration(&event, READ_DATA, data.calibrationFile.c_str());
                else if (!enabled && activated)
                        Calibration(&event, STORE_DATA, data.calibrationFile.c_str());
        }

        ret = ioctl(fd, IO_CMD_ACTIVATE, enabled);
        if (ret) {
                LOGE("%s: line: %d: %s enable error! arg: %d", __FUNCTION__, __LINE__, data.name.c_str(), enabled);
                return -1;
        }

        if (DriverCalibration != NULL && enabled != 0 && activated == 0)
                DriverCalibration(&event, CALIBRATION_DATA, data.driverCalibrationFile.c_str(), data.driverCalibrationInterface.c_str());

        activated = enabled;

        return 0;
}

int MiscSensor::setDelay(int handle, int64_t ns)
{
        int fd, ret;
        int64_t delay;
        int minDelay = device.getMinDelay() / US_TO_MS;

        if (handle != device.getHandle()) {
                LOGE("%s: line: %d: %s handle not match! handle: %d required handle: %d",
                     __FUNCTION__, __LINE__, data.name.c_str(), device.getHandle(), handle);
                return -1;
        }

        fd = getPollfd();
        if (fd < 0) {
                LOGE("%s: line: %d: %s getPollfd error!", __FUNCTION__, __LINE__, data.name.c_str());
                return -1;
        }

        if (ns / 1000 == SENSOR_NOPOLL)
                delay = 0;
        else
                delay = ns / NS_TO_MS;

        if (delay < minDelay)
                delay = minDelay;

        /* If the sensor is interrupt mode */
        if (minDelay == 0 || data.setDelayInterface.length() == 0)
                return 0;

        ret = ioctl(fd, IO_CMD_SETDELAY, delay);
        if (ret) {
                LOGE("%s: line: %d: %s set delay error! arg: %lld", __FUNCTION__, __LINE__, data.name.c_str(), ns);
                return -1;
        }

        return 0;
}

int MiscSensor::getData(std::queue<sensors_event_t> &eventQue) {
        int count, ret;
        sensors_misc_event_t miscEvent[32];

        ret = read(pollfd, miscEvent, sizeof(miscEvent));

        if (ret == sizeof(int)) {
                event.data[0] = static_cast<float>(miscEvent[0].value) * device.getScale(0);
                event.timestamp = getTimestamp();
                if (Calibration != NULL)
                        Calibration(&event, CALIBRATION_DATA, NULL);
                eventQue.push(event);
                return 0;
        }
        else if (ret % sizeof(sensors_misc_event_t) != 0) {
                LOGE("%s line: %d, name: %s ret: %d",
                     __FUNCTION__, __LINE__, data.name.c_str(), ret);
                return -1;
        }

        count = ret / sizeof(sensors_misc_event_t);
        for (int i = 0; i < count; i++) {
                float value = static_cast<float>(miscEvent[i].value);
                switch(miscEvent[i].axis) {
                case AXIS_X:
                        event.data[device.getMapper(AXIS_X)] = value * device.getScale(AXIS_X);
                        break;
                case AXIS_Y:
                        event.data[device.getMapper(AXIS_Y)] = value * device.getScale(AXIS_Y);
                        break;
                case AXIS_Z:
                        event.data[device.getMapper(AXIS_Z)] = value * device.getScale(AXIS_Z);
                        break;
                case AXIS_OTHER:
                        event.timestamp = miscEvent[i].timestamp;
                        if (Calibration != NULL)
                                Calibration(&event, CALIBRATION_DATA, NULL);
                        else if (device.getEventProperty() == VECTOR)
                                event.acceleration.status = SENSOR_STATUS_ACCURACY_MEDIUM;
                        eventQue.push(event);
                default:
                        LOGW("%s line: %d unknown axis: %d", __FUNCTION__, __LINE__, miscEvent[i].axis);
                        break;
                }
        }

        return 0;
}

bool MiscSensor::selftest() {
        if (getPollfd() >= 0)
                return true;
        return false;
}
