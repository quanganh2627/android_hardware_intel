#include "InputEventSensor.hpp"
#include <sstream>
#include <cerrno>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#define EVENT_NAME_MAX  256

InputEventSensor::InputEventSensor(SensorDevice &mDevice, struct PlatformData &mData)
        :DirectSensor(mDevice, mData)
{
        inputDataOverrun = false;
}

int InputEventSensor::getPollfd()
{
        DIR *dir;
        struct dirent *entry;
        const std::string inputDir = "/dev/input/";
        std::string file;
        char name[EVENT_NAME_MAX];
        int fd;

        if (pollfd >= 0)
                return pollfd;

        dir = opendir(inputDir.c_str());
        if (dir == NULL) {
                LOGE("opendir error: %s: %s; error number: %d\n", inputDir.c_str(), strerror(errno), errno );
                return -1;
        }

        while ((entry = readdir(dir))) {
                if (entry->d_type != DT_CHR)
                        continue;

                file = inputDir + entry->d_name;
                fd = open(file.c_str(), O_RDWR);
                if (fd < 0) {
                        LOGE("open error: %s: %s; error number: %d\n", file.c_str(), strerror(errno), errno);
                        closedir( dir );
                        return -1;
                }

                name[sizeof(name) -1] = 0;
                if (ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name) < 1) {
                        close(fd);
                        continue;
                }

                if (data.name.compare(name) == 0) {
                        unsigned int clockId = CLOCK_MONOTONIC;
                        /* Set desired clock as monotonic to record timestamp of event
                         * Fixme: this ioctl is supported since Linux 3.4
	                 */
                        ioctl(fd, EVIOCSCLOCKID, &clockId);
                        pollfd = fd;
                        break;
                }

                close(fd);
        }
        closedir(dir);

        return pollfd;
}

int InputEventSensor::activate(int handle, int enabled) {
        static int activated = 0;
        int result;

        if (handle != device.getHandle()) {
                LOGE("%s: line: %d: %s handle not match! handle: %d required handle: %d",
                     __FUNCTION__, __LINE__, data.name.c_str(), device.getHandle(), handle);
                return -1;
        }

        enabled = enabled == 0 ? 0 : 1;

        if (Calibration != NULL) {
                if (enabled && !activated)
                        Calibration(&event, READ_DATA, data.calibrationFile.c_str());
                else if (!enabled && activated)
                        Calibration(&event, STORE_DATA, data.calibrationFile.c_str());
        }
        result =writeToFile(data.activateInterface, handle, static_cast<int64_t>(enabled));

        if (DriverCalibration != NULL && enabled != 0 && result == 0 && activated == 0)
                DriverCalibration(&event, CALIBRATION_DATA, data.driverCalibrationFile.c_str(), data.driverCalibrationInterface.c_str());

        activated = enabled;

        return result;
}

int InputEventSensor::setDelay(int handle, int64_t ns) {
        int64_t delay;
        int minDelay = device.getMinDelay() / US_TO_MS;

        delay = ns / NS_TO_MS;
        if (delay < minDelay)
                delay = minDelay;

        if (ns / 1000 == SENSOR_NOPOLL)
                delay = 0;

        /* If the sensor is interrupt mode */
        if (minDelay == 0 || data.setDelayInterface.length() == 0)
                return 0;

        return writeToFile(data.setDelayInterface, handle, delay);
}

int InputEventSensor::getData(std::queue<sensors_event_t> &eventQue) {
        struct input_event inputEvent[32];
        int count, ret;

        ret = read(pollfd, inputEvent, 32 * sizeof(struct input_event));
        if (ret < 0 || ret % sizeof(struct input_event)) {
                LOGE("Read input event error! ret: %d", ret);
                return -1;
        }

        count = ret / sizeof(struct input_event);

        for (int i = 0; i < count; i++) {
                if ((inputEvent[i].type == EV_REL || inputEvent[i].type == EV_ABS) && !inputDataOverrun) {
                        float value = static_cast<float>(inputEvent[i].value);
                        if ((inputEvent[i].type == EV_REL && inputEvent[i].code == REL_X) ||
                            (inputEvent[i].type == EV_ABS && inputEvent[i].code == ABS_X))
                                event.data[device.getMapper(AXIS_X)] = value * device.getScale(AXIS_X);
                        else if ((inputEvent[i].type == EV_REL && inputEvent[i].code == REL_Y) ||
                                 (inputEvent[i].type == EV_ABS && inputEvent[i].code == ABS_Y))
                                event.data[device.getMapper(AXIS_Y)] = value * device.getScale(AXIS_Y);
                        else if ((inputEvent[i].type == EV_REL && inputEvent[i].code == REL_Z) ||
                                 (inputEvent[i].type == EV_ABS && inputEvent[i].code == ABS_Z))
                                event.data[device.getMapper(AXIS_Z)] = value * device.getScale(AXIS_Z);
                }
                else if (inputEvent[i].type == EV_SYN) {
                        if (inputEvent[i].code == SYN_DROPPED) {
                                LOGE("input event overrun");
                                inputDataOverrun = true;
                        }
                        else if (inputDataOverrun) {
                                inputDataOverrun = false;
                        }
                        else {
                                event.timestamp = timevalToNano(inputEvent[i].time);
                                if (Calibration != NULL)
                                        Calibration(&event, CALIBRATION_DATA, data.calibrationFile.c_str());
                                else if (device.getEventProperty() == VECTOR)
                                        event.acceleration.status = SENSOR_STATUS_ACCURACY_MEDIUM;
                                eventQue.push(event);
                        }
                }
        }

        return 0;
}

bool InputEventSensor::selftest() {
        if (getPollfd() >= 0)
                return true;
        return false;
}

int InputEventSensor::openFile(std::string &pathset) {
        std::string path;
        size_t pos, start = 0;
        int fd;

        pos = pathset.find(';');
        if (pos == std::string::npos) {
                fd = open(pathset.c_str(), O_WRONLY);
        }
        else {
                while (pos != std::string::npos) {
                        path = pathset.substr(start, pos - start);
                        fd = open(path.c_str(), O_WRONLY);
                        if (fd >= 0)
                                break;
                        start = pos + 1;
                        pos = pathset.find(';', start);
                }
                if (start < pathset.size() && fd < 0) {
                        path = pathset.substr(start);
                        fd = open(path.c_str(), O_WRONLY);
                }
        }

        return fd;
}

int InputEventSensor::writeToFile(std::string &pathset, int handle, int64_t value) {
        int fd, ret;
        std::ostringstream ss;

        if (handle != device.getHandle()) {
                LOGE("%s: line: %d: %s handle not match! handle: %d required handle: %d",
                     __FUNCTION__, __LINE__, data.name.c_str(), device.getHandle(), handle);
                return -1;
        }

        fd = openFile(pathset);
        if (fd < 0) {
                LOGE("%s: line: %d: Cannot open the driver interface for activate: %s",
                     __FUNCTION__, __LINE__, data.activateInterface.c_str());
                return fd;
        }

        ss<<value;

        ret = write(fd, ss.str().c_str(), ss.str().size() + 1);

        if (ret != static_cast<int>(ss.str().size() + 1)) {
                LOGE("%s: line: %d: write activate error: %d",
                     __FUNCTION__, __LINE__, ret);
                close(fd);
                return -1;
        }

        close(fd);

        return 0;
}
