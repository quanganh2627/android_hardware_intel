/*
 * Copyright (C) 2013 Han, He <he.han@intel.com>
 * Version: 1.0
 * Author: Han, He <he.han@intel.com>
 * Date: Aug 26th, 2013
 */

#include "accelerometer_simple_calibration.h"
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <cutils/log.h>

#define ACCEL_SIMP_CAL_BUF_LENGTH       50
#define ACCEL_SIMP_CAL_NOISE_DENSITY    (0.025 * GRAVITY_EARTH)
#define ACCEL_SIMP_CAL_DIRECTION_OFFSET 3.2
#define ACCEL_SIMP_CAL_MODEL_OFFSET     1.2

struct accelerometer_simple_calibration_t {
        float buf[ACCEL_AXIS_MAX][ACCEL_SIMP_CAL_BUF_LENGTH];
        float average[ACCEL_AXIS_MAX];
        float sum[ACCEL_AXIS_MAX];
        float model;
        int index;
        int size;
        struct accelerometer_simple_calibration_samples samples;
        int samples_collected_this_time;
        int initialized;
};

static int need_recalibrate = 0;

static void accel_simp_cal_collect_data(struct accelerometer_simple_calibration_event_t* event, struct accelerometer_simple_calibration_t* asc)
{
        int i;

        if (asc->size < ACCEL_SIMP_CAL_BUF_LENGTH)
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
        if (asc->index >= ACCEL_SIMP_CAL_BUF_LENGTH)
                asc->index = 0;
}

static void accel_simp_cal_clear_data(struct accelerometer_simple_calibration_t* asc)
{
        int i;
        asc->index = 0;
        asc->size = 0;
        for (i = 0; i < ACCEL_AXIS_MAX; i++) {
                asc->average[i] = 0;
                asc->sum[i] = 0;
        }
}

static int accel_simp_cal_sample_out_range(struct accelerometer_simple_calibration_t* asc)
{
        int i, j;
        float noise[ACCEL_AXIS_MAX];

        if (asc->size < ACCEL_SIMP_CAL_BUF_LENGTH) {
                return 1;
        } else {
                if (asc->model < GRAVITY_EARTH - ACCEL_SIMP_CAL_MODEL_OFFSET || asc->model > GRAVITY_EARTH + ACCEL_SIMP_CAL_MODEL_OFFSET) {
                        LOGW("Incorrect asc->model: %f", asc->model);
                        return 1;
                }
                for (j = 0; j < ACCEL_SIMP_CAL_BUF_LENGTH; j++) {
                        for (i = 0; i < ACCEL_AXIS_MAX; i++) {
                                noise[i] = asc->buf[i][j] - asc->average[i];
                                if (noise[i] < -ACCEL_SIMP_CAL_NOISE_DENSITY || noise[i] > ACCEL_SIMP_CAL_NOISE_DENSITY) {
                                        LOGW("Noise too large: asc->index: %d coordinate: %d value: %lf asc->average: %lf noise: %lf",
                                             j, i, asc->buf[i][j], asc->average[i], noise[i]);
                                        return 1;
                                }
                        }
                }
        }
        return 0;
}

static void accel_simp_cal_sampling(struct accelerometer_simple_calibration_event_t* event, struct accelerometer_simple_calibration_t* asc)
{
        int i;

        if (accel_simp_cal_sample_out_range(asc)) {
                accel_simp_cal_collect_data(event, asc);
                return;
        }

        if (asc->average[ACCEL_AXIS_X] < GRAVITY_EARTH + ACCEL_SIMP_CAL_MODEL_OFFSET && asc->average[ACCEL_AXIS_X] > GRAVITY_EARTH - ACCEL_SIMP_CAL_DIRECTION_OFFSET) {
                LOGI("data x positive");
                if (asc->samples.sampled[ACCEL_AXIS_X]) {
                        LOGI("data x positive had collected");
                        accel_simp_cal_clear_data(asc);
                        return;
                }
                for (i = 0; i < ACCEL_AXIS_MAX; i++) {
                        asc->samples.samples[ACCEL_AXIS_X][i] = asc->average[i];
                }
                asc->samples.sampled[ACCEL_AXIS_X] = 1;
                asc->samples.samples_collected++;
                asc->samples_collected_this_time++;
                accel_simp_cal_clear_data(asc);
                LOGI("data x positive has collected");
        } else if (asc->average[ACCEL_AXIS_X] > -ACCEL_SIMP_CAL_MODEL_OFFSET - GRAVITY_EARTH && asc->average[ACCEL_AXIS_X] < ACCEL_SIMP_CAL_DIRECTION_OFFSET - GRAVITY_EARTH) {
                LOGI("data x negative");
                if (asc->samples.sampled[ACCEL_AXIS_Y]) {
                        LOGI("data x negative had collected");
                        accel_simp_cal_clear_data(asc);
                        return;
                }
                for (i = 0; i < ACCEL_AXIS_MAX; i++) {
                        asc->samples.samples[ACCEL_AXIS_Y][i] = asc->average[i];
                }
                asc->samples.sampled[ACCEL_AXIS_Y] = 1;
                asc->samples.samples_collected++;
                asc->samples_collected_this_time++;
                accel_simp_cal_clear_data(asc);
                LOGI("data x negative has collected");
        } else if (asc->average[ACCEL_AXIS_Y] < GRAVITY_EARTH + ACCEL_SIMP_CAL_MODEL_OFFSET && asc->average[ACCEL_AXIS_Y] > GRAVITY_EARTH - ACCEL_SIMP_CAL_DIRECTION_OFFSET) {
                LOGI("data y positive");
                if (asc->samples.sampled[ACCEL_AXIS_Z]) {
                        LOGI("data y positive had collected");
                        accel_simp_cal_clear_data(asc);
                        return;
                }
                for (i = 0; i < ACCEL_AXIS_MAX; i++) {
                        asc->samples.samples[2][i] = asc->average[i];
                }
                asc->samples.sampled[2] = 1;
                asc->samples.samples_collected++;
                asc->samples_collected_this_time++;
                accel_simp_cal_clear_data(asc);
                LOGI("data y positive has collected");
        } else if (asc->average[ACCEL_AXIS_Y] > -ACCEL_SIMP_CAL_MODEL_OFFSET - GRAVITY_EARTH && asc->average[ACCEL_AXIS_Y] < ACCEL_SIMP_CAL_DIRECTION_OFFSET - GRAVITY_EARTH) {
                LOGI("data y negative");
                if (asc->samples.sampled[ACCEL_AXIS_MAX]) {
                        LOGI("data y negative had collected");
                        accel_simp_cal_clear_data(asc);
                        return;
                }
                for (i = 0; i < ACCEL_AXIS_MAX; i++) {
                        asc->samples.samples[ACCEL_AXIS_MAX][i] = asc->average[i];
                }
                asc->samples.sampled[ACCEL_AXIS_MAX] = 1;
                asc->samples.samples_collected++;
                asc->samples_collected_this_time++;
                accel_simp_cal_clear_data(asc);
                LOGI("data y negative has collected");
        } else if (asc->average[2] < GRAVITY_EARTH + ACCEL_SIMP_CAL_MODEL_OFFSET && asc->average[2] > GRAVITY_EARTH - ACCEL_SIMP_CAL_DIRECTION_OFFSET) {
                LOGI("data z positive");
                if (asc->samples.sampled[4]) {
                        LOGI("data z positive had collected");
                        accel_simp_cal_clear_data(asc);
                        return;
                }
                for (i = 0; i < ACCEL_AXIS_MAX; i++) {
                        asc->samples.samples[4][i] = asc->average[i];
                }
                asc->samples.sampled[4] = 1;
                asc->samples.samples_collected++;
                asc->samples_collected_this_time++;
                accel_simp_cal_clear_data(asc);
                LOGI("data z positive has collected");
        } else if (asc->average[2] > -ACCEL_SIMP_CAL_MODEL_OFFSET - GRAVITY_EARTH && asc->average[2] < ACCEL_SIMP_CAL_DIRECTION_OFFSET - GRAVITY_EARTH) {
                LOGI("data z negative");
                if (asc->samples.sampled[5]) {
                        LOGI("data z negative had collected");
                        accel_simp_cal_clear_data(asc);
                        return;
                }
                for (i = 0; i < ACCEL_AXIS_MAX; i++) {
                        asc->samples.samples[5][i] = asc->average[i];
                }
                asc->samples.sampled[5] = 1;
                asc->samples.samples_collected++;
                asc->samples_collected_this_time++;
                accel_simp_cal_clear_data(asc);
                LOGI("data z negative has collected");
        } else {
                LOGI("Other data %lf %lf %lf", asc->average[ACCEL_AXIS_X], asc->average[ACCEL_AXIS_Y], asc->average[2]);
                accel_simp_cal_collect_data(event, asc);
        }

}

static int accel_simp_cal_read_samples(struct accelerometer_simple_calibration_samples* samples, const char* configFile)
{
        int fd, ret;

        fd = open(configFile, O_RDONLY);
        if (fd < 0) {
                LOGE("%s line:%d, open %s error: %s", __FUNCTION__, __LINE__, configFile, strerror(errno));
                return fd;
        }

        ret = read(fd, samples, sizeof(struct accelerometer_simple_calibration_samples));
        if (ret < 0) {
                LOGE("%s line:%d, read %s error: %s", __FUNCTION__, __LINE__, configFile, strerror(errno));
                close(fd);
                return ret;
        } else if (ret != sizeof(struct accelerometer_simple_calibration_samples)) {
                LOGE("%s line:%d, invalid config file: %s", __FUNCTION__, __LINE__, configFile);
                close(fd);
                return -EINVAL;
        }

        close(fd);
        return 0;
}

static int accel_simp_cal_store_samples(struct accelerometer_simple_calibration_samples* samples, const char* configFile)
{
        int fd, ret;

        fd = open(configFile, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
        if (fd < 0) {
                LOGE("%s line:%d, open %s error: %s", __FUNCTION__, __LINE__, configFile, strerror(errno));
                return fd;
        }

        ret = write(fd, samples, sizeof(struct accelerometer_simple_calibration_samples));
        if (ret < 0) {
                LOGE("%s line:%d, write %s error: %s", __FUNCTION__, __LINE__, configFile, strerror(errno));
                close(fd);
                return ret;
        } else if (ret != sizeof(struct accelerometer_simple_calibration_samples)) {
                LOGE("%s line:%d, write config file: %s error", __FUNCTION__, __LINE__, configFile);
                close(fd);
                return -EINVAL;
        }

        close(fd);
        return 0;
}

static int accel_simp_cal_matrix_transformation(float (*p)[ACCEL_SIMP_CAL_SAMPLES_COUNT], int start)
{
	int i, j;
	float value;

        if (start >= ACCEL_SIMP_CAL_SAMPLES_COUNT) {
                LOGE("%s line:%d error matrix position:%d", __FUNCTION__, __LINE__, start);
                return -EOVERFLOW;
        }

	value = p[start][start];
        if (value == 0.0) {
                LOGE("%s line:%d invalid matrix:%d", __FUNCTION__, __LINE__, start);
                return -EINVAL;
        }

	for (i = start; i < ACCEL_SIMP_CAL_SAMPLES_COUNT; i++) {
		p[start][i] /= value;
	}

	for (i = start + 1; i < ACCEL_SIMP_CAL_SAMPLES_COUNT - 1; i++) {
		value = p[i][start];
		for (j = start; j < ACCEL_SIMP_CAL_SAMPLES_COUNT; j++) {
			p[i][j] -= value * p[start][j];
		}
	}

        return 0;
}

static int accel_simp_cal_calculate_factors(struct accelerometer_simple_calibration_samples* samples)
{
        int i, j, ret;
        float v1, v2;
        float matrix[ACCEL_SIMP_CAL_SAMPLES_COUNT -1][ACCEL_SIMP_CAL_SAMPLES_COUNT] = { { 0 } };
        float roots[ACCEL_SIMP_CAL_SAMPLES_COUNT] = { 0 };
        float factors[ACCEL_SIMP_CAL_SAMPLES_COUNT], scale;

        for(i = 0; i < ACCEL_SIMP_CAL_SAMPLES_COUNT -1; i++) {
                for (j = ACCEL_AXIS_X; j < ACCEL_AXIS_MAX; j++) {
                        matrix[i][2*j] = samples->samples[i][j] * samples->samples[i][j] - samples->samples[i+1][j] * samples->samples[i+1][j];
                        matrix[i][2*j+1] = 2.0 * (samples->samples[i][j] - samples->samples[i+1][j]);
                }
        }

	for (i = 0; i < ACCEL_SIMP_CAL_SAMPLES_COUNT -1; i++) {
                ret = accel_simp_cal_matrix_transformation(matrix, i);
                if (ret < 0) {
                        LOGE("%s line:%d matrix_transformation failed", __FUNCTION__, __LINE__);
                        return ret;
                }
		LOGI("%f %f %f %f %f %f", matrix[i][0], matrix[i][1], matrix[i][2], matrix[i][3], matrix[i][4], matrix[i][5]);
	}

        for (i = ACCEL_SIMP_CAL_SAMPLES_COUNT - 2; i >= 0; i--) {
		roots[i] = -matrix[i][ACCEL_SIMP_CAL_SAMPLES_COUNT-1];
		for (j = i + 1; j < ACCEL_SIMP_CAL_SAMPLES_COUNT - 1; j++) {
			roots[i] -= matrix[i][j] * roots[j];
		}
		LOGI("root[%d]: %f", i, roots[i]);
	}
	roots[ACCEL_SIMP_CAL_SAMPLES_COUNT-1] = 1.0;

        if (roots[0] < 0.0) {
                for (i = 0; i < ACCEL_SIMP_CAL_SAMPLES_COUNT; i++) {
                        roots[i] *= -1.0;
                }
        }

	for (i = 0; i < ACCEL_SIMP_CAL_SAMPLES_COUNT; i += 2) {
		factors[i] = sqrt(roots[i]);
		factors[i + 1] = roots[i + 1] / factors[i];
		LOGI("%f\t%f", factors[i], factors[i + 1]);
	}

	scale = GRAVITY_EARTH * GRAVITY_EARTH / ((factors[0] * samples->samples[0][ACCEL_AXIS_X] + factors[1]) * (factors[0] * samples->samples[0][ACCEL_AXIS_X] + factors[1]) +
                                                 (factors[2] * samples->samples[0][ACCEL_AXIS_Y] + factors[3]) * (factors[2] * samples->samples[0][ACCEL_AXIS_Y] + factors[3]) +
                                                 (factors[4] * samples->samples[0][ACCEL_AXIS_Z] + factors[5]) * (factors[4] * samples->samples[0][ACCEL_AXIS_Z] + factors[5]));
	scale = sqrt(scale);

	for (i = 0; i < 6; i++) {
		samples->factors[i] = factors[i] * scale;
		LOGI("factors[%d]: %f", i, samples->factors[i]);
	}

        return 0;
}

static void accel_simp_cal_calibrate(struct accelerometer_simple_calibration_event_t* event, struct accelerometer_simple_calibration_samples* samples)
{
        int i;

        if (!event)
                return;

        for (i = 0; i < ACCEL_AXIS_MAX; i++)
                event->data[i] = samples->factors[2*i] * event->data[i] + samples->factors[2*i+1];
}

int accel_simp_cal_calibration(struct accelerometer_simple_calibration_event_t* event, const char* configFile)
{
        int i, j;
        int ret;
        static struct accelerometer_simple_calibration_t asc;

        if (configFile == NULL)
                configFile = "/data/accel_simple_cal_default.conf";

        if (!asc.initialized) {
                ret = accel_simp_cal_read_samples(&asc.samples, configFile);
                if (ret < 0) {
                        LOGW("%s line:%d, read samples error!", __FUNCTION__, __LINE__);
                        memset(&asc.samples, 0, sizeof(asc.samples));
                } else {
                        for (i = 0; i < ACCEL_SIMP_CAL_SAMPLES_COUNT; i++)
                                LOGI("%lf\t%lf\t%lf", asc.samples.samples[i][ACCEL_AXIS_X], asc.samples.samples[i][ACCEL_AXIS_Y], asc.samples.samples[i][ACCEL_AXIS_Z]);
                }
                asc.initialized = 1;
        }

        if (need_recalibrate == 1) {
                LOGW("%s line:%d, need recalibrate!", __FUNCTION__, __LINE__);
                memset(&asc.samples, 0, sizeof(asc.samples));
                need_recalibrate = 0;
        }

        if (asc.samples.factors_ready) {
                if (event) {
                        accel_simp_cal_calibrate(event, &asc.samples);
                        return 0;
                }
        } else if (asc.samples.samples_collected >= ACCEL_SIMP_CAL_SAMPLES_COUNT) {
                LOGI("All data collected. %d", asc.samples.samples_collected);
                for (i = 0; i < ACCEL_SIMP_CAL_SAMPLES_COUNT; i++)
                        LOGI("%f\t%f\t%f", asc.samples.samples[i][ACCEL_AXIS_X], asc.samples.samples[i][ACCEL_AXIS_Y], asc.samples.samples[i][ACCEL_AXIS_Z]);
                ret = accel_simp_cal_calculate_factors(&asc.samples);
                if (ret < 0) {
                        LOGE("%s line:%d, calculate factors error!", __FUNCTION__, __LINE__);
                        memset(&asc.samples, 0, sizeof(asc.samples));
                        accel_simp_cal_clear_data(&asc);
                        return -EAGAIN;
                }
                asc.samples.factors_ready = 1;

                ret = accel_simp_cal_store_samples(&asc.samples, configFile);

                if (event) {
                        accel_simp_cal_calibrate(event, &asc.samples);
                        return 0;
                }
                return -EAGAIN;
        }

        if (event)
                accel_simp_cal_sampling(event, &asc);
        else
                accel_simp_cal_store_samples(&asc.samples, configFile);

        return -EAGAIN;
}

void accel_simp_cal_reset()
{
        need_recalibrate = 1;
}
