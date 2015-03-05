/*
 * Copyright (C) 2013 Han, He <he.han@intel.com>
 * Version: 1.0
 * Author: Han, He <he.han@intel.com>
 * Date: Aug 26th, 2013
 */

#ifndef _ACCELEROMETER_SIMPLE_CALIBRATION_H_
#define _ACCELEROMETER_SIMPLE_CALIBRATION_H_

#ifndef GRAVITY_EARTH
#define GRAVITY_EARTH           (9.80665f)
#endif

#define ACCEL_SIMP_CAL_SAMPLES_COUNT  6
#define ACCEL_SIMP_ZCAL_SAMPLES_COUNT 2

#ifdef __cplusplus
extern "C" {
#endif


enum accel_axis_t {
        ACCEL_AXIS_X = 0,
        ACCEL_AXIS_Y,
        ACCEL_AXIS_Z,
        ACCEL_AXIS_MAX,
};

struct accelerometer_simple_calibration_event_t {
        float data[ACCEL_AXIS_MAX];
};

struct accelerometer_simple_calibration_samples {
        float samples[ACCEL_SIMP_CAL_SAMPLES_COUNT][ACCEL_AXIS_MAX];
        int sampled[ACCEL_SIMP_CAL_SAMPLES_COUNT];
        int samples_collected;
        float factors[6];
        int factors_ready;
};

struct accelerometer_simple_zcalibration_samples {
        float models[ACCEL_SIMP_ZCAL_SAMPLES_COUNT];
        int sampled[ACCEL_SIMP_ZCAL_SAMPLES_COUNT];
        int samples_collected;
        float factors[2];
        int factors_ready;
};

int accel_simp_cal_calibration(struct accelerometer_simple_calibration_event_t* event, const char* configFile);
void accel_simp_cal_reset();
int accel_simp_zcal_calibration(struct accelerometer_simple_calibration_event_t* event);
void accel_simp_zcal_reset();

#ifdef __cplusplus
}
#endif

#endif
