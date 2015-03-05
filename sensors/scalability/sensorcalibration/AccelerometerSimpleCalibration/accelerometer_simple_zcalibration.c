/*
 * Copyright (C) 2013 Han, He <he.han@intel.com>
 * Version: 1.0
 * Author: Han, He <he.han@intel.com>
 * Date: Sep 17th, 2013
 */

#include "accelerometer_simple_calibration.h"
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <cutils/log.h>

#define ACCEL_SIMP_ZCAL_BUF_LENGTH       12
#define ACCEL_SIMP_ZCAL_NOISE_DENSITY    (0.025 * GRAVITY_EARTH)
#define ACCEL_SIMP_ZCAL_DIRECTION_SCALE  (0.970287525f)
#define ACCEL_SIMP_ZCAL_MODEL_OFFSET     1.2

struct accelerometer_simple_zcalibration_t {
        float buf[ACCEL_AXIS_MAX][ACCEL_SIMP_ZCAL_BUF_LENGTH];
        float average[ACCEL_AXIS_MAX];
        float sum[ACCEL_AXIS_MAX];
        float model;
        int index;
        int size;
        struct accelerometer_simple_zcalibration_samples samples;
        int samples_collected_this_time;
        int initialized;
};

static int need_rezcalibrate = 0;

static void accel_simp_zcal_collect_data(struct accelerometer_simple_calibration_event_t* event, struct accelerometer_simple_zcalibration_t* asc)
{
        int i;

        if (asc->size < ACCEL_SIMP_ZCAL_BUF_LENGTH)
                asc->size++;
        else
                for (i = 0; i < ACCEL_AXIS_MAX; i++) {
                        asc->sum[i] -= asc->buf[i][asc->index];
                }

        for (i = 0; i < ACCEL_AXIS_MAX; i++) {
                asc->buf[i][asc->index] = event->data[i];
                asc->sum[i] += event->data[i];
                asc->average[i] = asc->sum[i] / asc->size;
        }
        asc->model = sqrt(asc->average[ACCEL_AXIS_X] * asc->average[ACCEL_AXIS_X] + asc->average[ACCEL_AXIS_Y] * asc->average[ACCEL_AXIS_Y] + asc->average[ACCEL_AXIS_Z] * asc->average[ACCEL_AXIS_Z]);

        asc->index++;
        if (asc->index >= ACCEL_SIMP_ZCAL_BUF_LENGTH)
                asc->index = 0;
}

static void accel_simp_zcal_clear_data(struct accelerometer_simple_zcalibration_t* asc)
{
        int i;
        asc->index = 0;
        asc->size = 0;
        for (i = 0; i < ACCEL_AXIS_MAX; i++) {
                asc->average[i] = 0;
                asc->sum[i] = 0;
        }
}

static int accel_simp_zcal_sample_out_range(struct accelerometer_simple_zcalibration_t* asc)
{
        int i, j;
        float noise[ACCEL_AXIS_MAX];

        if (asc->size < ACCEL_SIMP_ZCAL_BUF_LENGTH) {
                return 1;
        } else {
                if (asc->model < GRAVITY_EARTH - ACCEL_SIMP_ZCAL_MODEL_OFFSET || asc->model > GRAVITY_EARTH + ACCEL_SIMP_ZCAL_MODEL_OFFSET) {
                        LOGW("Incorrect asc->model: %f", asc->model);
                        return 1;
                }
                for (j = 0; j < ACCEL_SIMP_ZCAL_BUF_LENGTH; j++) {
                        for (i = 0; i < ACCEL_AXIS_MAX; i++) {
                                noise[i] = asc->buf[i][j] - asc->average[i];
                                if (noise[i] < -ACCEL_SIMP_ZCAL_NOISE_DENSITY || noise[i] > ACCEL_SIMP_ZCAL_NOISE_DENSITY) {
                                        LOGW("Noise too large: asc->index: %d coordinate: %d value: %lf asc->average: %lf noise: %lf",
                                             j, i, asc->buf[i][j], asc->average[i], noise[i]);
                                        return 1;
                                }
                        }
                }
        }
        return 0;
}

static void accel_simp_zcal_sampling(struct accelerometer_simple_calibration_event_t* event, struct accelerometer_simple_zcalibration_t* asc)
{
        int i;

        if (accel_simp_zcal_sample_out_range(asc)) {
                accel_simp_zcal_collect_data(event, asc);
                return;
        }

        if (asc->average[ACCEL_AXIS_Z] > asc->model * ACCEL_SIMP_ZCAL_DIRECTION_SCALE) {
                LOGD("data z positive");
                if (asc->samples.sampled[0]) {
                        LOGD("data z positive had collected");
                        accel_simp_zcal_clear_data(asc);
                        return;
                }
                asc->samples.models[0] = asc->model;
                asc->samples.sampled[0] = 1;
                asc->samples.samples_collected++;
                accel_simp_zcal_clear_data(asc);
                LOGD("data z positive has collected");
        } else if (asc->average[ACCEL_AXIS_Z] < -asc->model * ACCEL_SIMP_ZCAL_DIRECTION_SCALE) {
                LOGD("data z negative");
                if (asc->samples.sampled[1]) {
                        LOGD("data z negative had collected");
                        accel_simp_zcal_clear_data(asc);
                        return;
                }
                asc->samples.models[1] = asc->model;
                asc->samples.sampled[1] = 1;
                asc->samples.samples_collected++;
                accel_simp_zcal_clear_data(asc);
                LOGD("data z negative has collected");

        } else {
                LOGD("Other data %lf %lf %lf", asc->average[ACCEL_AXIS_X], asc->average[ACCEL_AXIS_Y], asc->average[ACCEL_AXIS_Z]);
                accel_simp_zcal_collect_data(event, asc);
        }

}

static int accel_simp_zcal_zcalculate_factors(struct accelerometer_simple_zcalibration_samples* samples)
{
        samples->factors[0] = GRAVITY_EARTH * 2.0 / (samples->models[1] + samples->models[0]);
        samples->factors[1] = (samples->models[1] - samples->models[0]) / 2.0;
        LOGD("factors[0]: %f\tfactors[1]: %f", samples->factors[0], samples->factors[1]);

        return 0;
}

static void accel_simp_zcal_zcalibrate(struct accelerometer_simple_calibration_event_t* event, struct accelerometer_simple_zcalibration_samples* samples)
{
        int i;

        if (!event)
                return;

        event->data[ACCEL_AXIS_Z] = samples->factors[0] * event->data[ACCEL_AXIS_Z] + samples->factors[1];
}

int accel_simp_zcal_calibration(struct accelerometer_simple_calibration_event_t* event)
{
        int i, j;
        int ret = -EAGAIN;
        static struct accelerometer_simple_zcalibration_t asc;

        if (need_rezcalibrate == 1) {
                LOGW("%s line:%d, need rezcalibrate!", __FUNCTION__, __LINE__);
                memset(&asc.samples, 0, sizeof(asc.samples));
                need_rezcalibrate = 0;
        }

        if (asc.samples.samples_collected >= ACCEL_SIMP_ZCAL_SAMPLES_COUNT) {
                LOGD("All data collected. %d", asc.samples.samples_collected);
                for (i = 0; i < ACCEL_SIMP_ZCAL_SAMPLES_COUNT; i++)
                        LOGD("models[%d]: %f", i, asc.samples.models[i]);
                ret = accel_simp_zcal_zcalculate_factors(&asc.samples);
                if (ret < 0) {
                        LOGE("%s line:%d, zcalculate factors error!", __FUNCTION__, __LINE__);
                        memset(&asc.samples, 0, sizeof(asc.samples));
                        accel_simp_zcal_clear_data(&asc);
                        if (!asc.samples.factors_ready)
                                return -EAGAIN;
                }
                asc.samples.factors_ready = 1;
                asc.samples.samples_collected = 0;
                asc.samples.sampled[0] = 0;
                asc.samples.sampled[1] = 0;
        }

        if (event)
                accel_simp_zcal_sampling(event, &asc);

        if (asc.samples.factors_ready) {
                if (event) {
                        accel_simp_zcal_zcalibrate(event, &asc.samples);
                        ret = 0;
                }
        }

        return ret;
}

void accel_simp_zcal_reset()
{
        need_rezcalibrate = 1;
}
