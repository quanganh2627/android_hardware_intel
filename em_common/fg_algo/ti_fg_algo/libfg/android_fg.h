/*
 * android_fg.h
 *
 * TI Fuel Gauge header
 *
 * Copyright (C) 2014 Texas Instruments, Inc.
 * Author: Texas Instruments, Inc.
 *
 */

#ifndef __ANDROID_FG_H__
#define __ANDROID_FG_H__

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>

#include <cutils/log.h>   /* for SLOGE */
#include <cutils/klog.h>

#define do_gettimeofday(a) gettimeofday(a, NULL)
#define DEBUG
/*
 * Divide positive or negative dividend by positive divisor and round
 * to closest integer. Result is undefined for negative divisors and
 * for negative dividends if the divisor variable type is unsigned.
 */
#define DIV_ROUND_CLOSEST(x, divisor)(              \
{                                                   \
	typeof(x) __x = x;                              \
	typeof(divisor) __d = divisor;                  \
	(((typeof(x))-1) > 0 ||                         \
	((typeof(divisor))-1) > 0 || (__x) > 0) ?       \
	(((__x) + ((__d) / 2)) / (__d)) :               \
	(((__x) - ((__d) / 2)) / (__d));                \
}                                                   \
)

#ifdef DEBUG
#define LOG_TAG "LIBFG:"
#define DEBUG_TAG "fg_algo_iface"
#define LIB_FG_KLOG_LEVEL 3
#define KLOGE(x...) do { KLOG_ERROR(DEBUG_TAG, x); } while (0)

#define dev_dbg(dev, format, ...)		\
{						\
	SLOGE(format, ##__VA_ARGS__);		\
	KLOGE(format, ##__VA_ARGS__);		\
}
#else
#define dev_dbg(dev, format, ...)	NULL
#endif

/* Fuel Gauge Constatnts */
#define MAX_PERCENTAGE		100

/* Num, cycles with no Learning, after this many cycles, the gauge
   start adjusting FCC, based on Estimated Cell Degradation */
#define NO_LEARNING_CYCLES	25

/* Size of the OCV Lookup table */
#define OCV_TABLE_SIZE		21

/* OCV Configuration */
struct ocv_config {
	unsigned char voltage_diff;
	unsigned char current_diff;

	unsigned short sleep_enter_current;
	unsigned char sleep_enter_samples;

	unsigned short sleep_exit_current;
	unsigned char sleep_exit_samples;

	unsigned short long_sleep_current;

	unsigned int ocv_period;
	unsigned int relax_period;

	unsigned short flat_zone_low;
	unsigned short flat_zone_high;

	short c_factor;
	short z_factor;

	unsigned char max_delta;
	unsigned short fcc_update_valid_min;
	unsigned short fcc_update_valid_max;

	unsigned char table_size;
	unsigned short table[OCV_TABLE_SIZE];
};

/* EDV Point */
struct edv_point {
	short voltage;
	char percent;
};

/* Calibration */
struct cal_config {
	short offset;
	unsigned short slope;
};

/* EDV Point tracking data */
struct edv_state {
	short voltage;
	char percent;
	unsigned int min_capacity;
};

/* EDV Configuration */
struct edv_config {
	bool averaging;
	unsigned char seq_edv;
	short overload_current;
	short term_voltage;
	unsigned short z_factor;
	struct edv_point edv[3];
};

/* C_rate adaptation configration */
struct rate_config {
	unsigned short max_rate;
	unsigned short min_rate;
	short chg_gain;
	short dsg_gain;
};

/* General Battery Cell Configuration */
struct cell_config {
	bool cc_polarity;
	bool cc_out;
	bool ocv_below_edv1;

	short cc_voltage;
	short cc_current;
	short cc_term_voltage;
	int cc_q;
	unsigned char seq_cc;

	unsigned int design_capacity;

	unsigned char r_sense;

	unsigned short fcc_adjust;
	unsigned short max_impedance;
	unsigned char max_overcharge;  /* charge after CC, UNIT = % of fcc */

	unsigned char max_fcc_delta; /* UNIT = % of fcc */

	unsigned char low_temp;
	unsigned short deep_dsg_voltage;

	unsigned char light_load; /* valid dsg rate, UNIT = C, fcc/light_load */
	unsigned char near_full;  /* valid dsg learning cycle, UNIT = % of fcc */
	unsigned char recharge;  /* CC clear range, UNIT = % of fcc */

	unsigned int mode_switch_capacity;

	struct ocv_config *ocv;
	struct edv_config *edv;
	const struct rate_config *rate;
	const struct cal_config *cal_volt;
	const struct cal_config *cal_cur;
	const struct cal_config *cal_temp;
};

/* Cell State */
struct cell_state {
	short soc;

	int fcc;
	int nac;
	int rmc;
	int delta_q;

	short voltage;
	short av_voltage;
	short cur;
	short av_current;
	short boot_voltage;
	short impedance;

	short temperature;
	short cycle_count;

	bool sleep;
	bool relax;

	bool chg;
	bool dsg;

	bool edv0;
	bool edv1;
	bool edv2;
	bool ocv;
	bool cc;
	bool full;

	bool vcq;
	bool vdq;
	bool fcc_update_valid;
	bool init;
	bool init2;

	struct timeval last_correction;
	struct timeval last_ocv;
	struct timeval sleep_timer;
/*	struct timeval el_timer;*/
/*	unsigned int cumulative_sleep; */

	short prev_soc;
	int learn_q;
	unsigned short dod_eoc;
	int learn_offset;
	unsigned short learned_cycle;
	unsigned int new_fcc;
	int negative_q;
	int overcharge_q;
	int charge_cycle_q;
	int discharge_cycle_q;
	int cycle_q;
	int cc_q;

	unsigned char seq_cc;
	unsigned char sleep_samples;
	unsigned char seq_edvs;

	int c_rate;
	unsigned short c_min_rate;

	struct edv_state edv;

	bool updated;
	bool calibrate;

	struct cell_config *config;
	struct device *dev;

	int *charge_status;
};

void fg_init(struct cell_state *cell, short voltage, short curr);
void fg_process(struct cell_state *cell, int delta_q, short voltage,
		short cur, short temperature);

#endif
