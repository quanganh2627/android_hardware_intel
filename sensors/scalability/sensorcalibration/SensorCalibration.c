#include <hardware/sensors.h>
#include <cutils/log.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "SensorCalibration.h"
#include "CompassGenericCalibration/CompassGenericCalibration.h"

void CompassGenericCalibration(struct sensors_event_t* event, calibration_flag_t flag, const char* configFile)
{
        static int fd = -1;

        if (fd < 0)
                fd = open(configFile, O_RDWR | O_CREAT, S_IRWXU);

        if (fd < 0) {
                LOGE("%s line:%d unable to open %s", __FUNCTION__, __LINE__, configFile);
                return;
        }

        if (flag == READ_DATA) {
                struct flock lock;
                lock.l_type = F_WRLCK;
                lock.l_start = 0;
                lock.l_whence = SEEK_SET;
                lock.l_len = 0;
                if (fcntl(fd, F_SETLK, &lock) < 0)
                        LOGE("%s line:%d calibration file: %s lock fail!", __FUNCTION__, __LINE__, configFile);
                FILE * dataFile = fdopen(dup(fd), "r");
                CompassCal_init(dataFile);
                if (dataFile)
                        fclose(dataFile);
        }
        else if (flag == STORE_DATA) {
                struct flock lock;
                lock.l_type = F_UNLCK;
                lock.l_start = 0;
                lock.l_whence = SEEK_SET;
                lock.l_len = 0;
                if (fcntl(fd, F_SETLK, &lock) < 0)
                        LOGE("%s line:%d calibration file: %s unlock fail!", __FUNCTION__, __LINE__, configFile);
                FILE * dataFile = fdopen(dup(fd), "w");
                if (dataFile) {
                        rewind(dataFile);
                        CompassCal_storeResult(dataFile);
                        fclose(dataFile);
                }
                close(fd);
                fd = -1;
        }
        else if (flag == CALIBRATION_DATA) {
                long current_time_ms = (long)event->timestamp / 1000000;
                CompassCal_collectData(event->magnetic.x,
                                       event->magnetic.y, event->magnetic.z,
                                       current_time_ms);

                if (CompassCal_readyCheck()) {
                        CompassCal_computeCal(event->magnetic.x,
                                              event->magnetic.y, event->magnetic.z,
                                              &event->magnetic.x, &event->magnetic.y,
                                              &event->magnetic.z);
                        event->magnetic.status = SENSOR_STATUS_ACCURACY_HIGH;
                } else {
                        event->magnetic.status = SENSOR_STATUS_ACCURACY_LOW;
                }
        }
}

void GyroscopeGenericCalibration(struct sensors_event_t* event, calibration_flag_t flag, const char* configFile)
{
        int ret, fd = -1;
        char buf[50];
        static float calX = 0.0, calY = 0.0, calZ = 0.0;

        if (flag == STORE_DATA)
                return;

        if (flag == READ_DATA) {
                fd = open(configFile, O_RDONLY);

                if (fd < 0) {
                        LOGE("%s line:%d unable to open %s", __FUNCTION__, __LINE__, configFile);
                        return;
                }

                ret = pread(fd, buf, sizeof(buf), 0);
                if (ret > 0) {
                        ret = sscanf(buf, "%f %f %f\n", &calX, &calY, &calZ);
                        if (ret != 3)
                                calX = calY = calZ = 0.0;
                }
                close(fd);
        }
        else if (flag == CALIBRATION_DATA) {
                event->gyro.x -= calX;
                event->gyro.y -= calY;
                event->gyro.z -= calZ;
        }
}

#define APDS9XXX_PROXIMITY_INIT_DATA     0xFFFF
#define APDS9XXX_PROXIMITY_MAX_CROSSTALK 600
#define APDS9XXX_PROXIMITY_MIN_CROSSTALK 20
#define APDS9XXX_PROXIMITY_MAX_NUMBER    20

typedef struct {
        int number;
        int sum;
        int average;
        int head_offset;
        int crosstalk[APDS9XXX_PROXIMITY_MAX_NUMBER];
} apds9xxx_proximity_calibration_t;

void APDS9XXXProximityDriverGenericCalibration(struct sensors_event_t* event, calibration_flag_t flag, const char* configFile, const char* configNode)
{
        int config_fd, driver_fd, ret, threshold, raw_data = APDS9XXX_PROXIMITY_INIT_DATA;
        char buf[16] = { 0 };
        struct flock lock;
        apds9xxx_proximity_calibration_t cal_data = { 0, 0, 0, 0, { 0 } };

        if (flag != CALIBRATION_DATA)
                return;

        /* wait 50 milliseconds for hardware ready if needed */
        usleep(50000);
        driver_fd = open(configNode, O_RDWR);
        if (driver_fd < 0) {
                LOGE("%s line:%d cannot open file: %s", __FUNCTION__, __LINE__, configNode);
                return;
        }

        config_fd = open(configFile, O_RDWR | O_CREAT, S_IRWXU);
        if (config_fd < 0) {
                LOGE("%s line:%d cannot open file: %s", __FUNCTION__, __LINE__, configFile);
                goto error_open;
        }

        ret = read(driver_fd, buf, sizeof(buf) - 1);
        if (ret <= 0) {
                LOGE("%s line:%d read raw data error!", __FUNCTION__, __LINE__);
                goto error_read_raw;
        }

        raw_data = atoi(buf);

        if (raw_data == APDS9XXX_PROXIMITY_INIT_DATA) {
                LOGW("%s line:%d raw data is invalid: 0x%x", __FUNCTION__, __LINE__, raw_data);
        }

        lock.l_type = F_WRLCK;
        lock.l_start = 0;
        lock.l_whence = SEEK_SET;
        lock.l_len = 0;
        if (fcntl(config_fd, F_SETLK, &lock) < 0) {
                LOGE("%s line:%d file lock error: %s %s", __FUNCTION__, __LINE__, configFile, strerror(errno));
                goto error_lock;
        }

        ret = pread(config_fd, &cal_data, sizeof(cal_data), 0);
        if (ret < 0) {
                LOGE("%s line:%d file read error: %s %s", __FUNCTION__, __LINE__, configFile, strerror(errno));
                goto error_read_config;
        }
        else if (ret == 0) {
                LOGW("%s line:%d No calibration data", __FUNCTION__, __LINE__);
        }

        if (cal_data.number == 0 && raw_data == APDS9XXX_PROXIMITY_INIT_DATA)
                goto error_no_calibration;

        if (raw_data < APDS9XXX_PROXIMITY_MIN_CROSSTALK)
                raw_data += APDS9XXX_PROXIMITY_MIN_CROSSTALK;

        if (cal_data.number > APDS9XXX_PROXIMITY_MAX_NUMBER) {
                LOGE("%s line:%d number overflow", __FUNCTION__, __LINE__);
                goto error_overflow;
        }
        if (cal_data.number == 0) {
                if (raw_data > APDS9XXX_PROXIMITY_MAX_CROSSTALK)
                goto error_overflow;
                cal_data.average = raw_data;
                cal_data.sum = raw_data;
                cal_data.crosstalk[0] = raw_data;
                cal_data.number = 1;
        }
        else if (cal_data.number < APDS9XXX_PROXIMITY_MAX_NUMBER) {
                if (raw_data <= cal_data.average * 2) {
                        cal_data.crosstalk[cal_data.number++] = raw_data;
                        cal_data.sum += raw_data;
                        cal_data.average = cal_data.sum / cal_data.number;
                }
        }
        else {
                if (raw_data <= (cal_data.average * 15) / 10) {
                        cal_data.sum -= cal_data.crosstalk[cal_data.head_offset];
                        cal_data.sum += raw_data;
                        cal_data.average = cal_data.sum / cal_data.number;
                        cal_data.crosstalk[cal_data.head_offset] = raw_data;
                        if (cal_data.head_offset < cal_data.number - 1)
                                cal_data.head_offset++;
                        else
                                cal_data.head_offset = 0;
                }
        }

        ret = pwrite(config_fd, &cal_data, sizeof(cal_data), 0);
        if (ret < 0)
                LOGE("%s line:%d: Write config file %s failed, error %s",
                     __FUNCTION__, __LINE__, configFile, strerror(errno));

        threshold = cal_data.average;
        if (threshold < APDS9XXX_PROXIMITY_MIN_CROSSTALK)
                threshold = APDS9XXX_PROXIMITY_MIN_CROSSTALK;

        threshold = (threshold * 18) / 10;
        if (threshold > APDS9XXX_PROXIMITY_MAX_CROSSTALK)
                threshold = APDS9XXX_PROXIMITY_MAX_CROSSTALK;

        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf), "%d\n", threshold);
        ret = write(driver_fd, buf, strlen(buf));
        if (ret < 0) {
                LOGE("%s line:%d: Write config sysfs %s failed, error %s",
                     __FUNCTION__, __LINE__, configNode, strerror(errno));
        }

error_overflow:
error_no_calibration:
error_read_config:
        lock.l_type = F_UNLCK;
        if (fcntl(config_fd, F_SETLK, &lock) < 0)
                LOGE("%s line:%d file lock error: %s %s", __FUNCTION__, __LINE__, configFile, strerror(errno));
error_lock:
error_read_raw:
        close(config_fd);
error_open:
        close(driver_fd);
}
