#include "DirectSensor.hpp"
#include <dlfcn.h>

DirectSensor::DirectSensor(SensorDevice &mDevice, struct PlatformData &mData)
        :Sensor(mDevice),
         data(mData)
{
        Calibration = NULL;
        DriverCalibration = NULL;
        calibrationMethodsHandle = NULL;
        if (data.calibrationFunc.length() > 0 || (data.driverCalibrationFunc.length() > 0 && data.driverCalibrationInterface.length() > 0)) {
                calibrationMethodsHandle = dlopen("/system/lib/libsensorcalibration.so", RTLD_LAZY);
                if (calibrationMethodsHandle == NULL) {
                        LOGE("dlopen: /system/lib/libsensorcalibration.so error!");
                        return;
                }

                if (data.calibrationFunc.length() > 0 && Calibration == NULL) {
                        Calibration = reinterpret_cast<void (*)(struct sensors_event_t*, calibration_flag_t, const char*)>(dlsym(calibrationMethodsHandle, data.calibrationFunc.c_str()));
                        if (Calibration == NULL) {
                                LOGE("%s line: %d dlsym: %s error!", __FUNCTION__, __LINE__, data.calibrationFunc.c_str());
                        }
                }


                if (data.driverCalibrationFunc.length() > 0  && data.driverCalibrationInterface.length() > 0 && DriverCalibration == NULL) {
                        DriverCalibration = reinterpret_cast<void (*)(struct sensors_event_t*, calibration_flag_t, const char*, const char*)>(dlsym(calibrationMethodsHandle, data.driverCalibrationFunc.c_str()));
                        if (DriverCalibration == NULL) {
                                LOGE("%s line: %d dlsym: %s error!", __FUNCTION__, __LINE__, data.calibrationFunc.c_str());
                        }
                }
        }
}

DirectSensor::~DirectSensor()
{
        if (calibrationMethodsHandle != NULL) {
                int err = dlclose(calibrationMethodsHandle);
                if (err != 0) {
                        LOGE("%s line:%d dlclose error! %d", __FUNCTION__, __LINE__, err);
                }
        }
}
