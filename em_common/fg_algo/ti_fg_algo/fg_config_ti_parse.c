#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <cutils/log.h>

#include "../fg_algo_iface.h"
#include "libfg/android_fg.h"

/* Time threshold for a target that is in power off state */
#define POWER_OFF_TIME_THRESHOLD 86400 /* in secs = 1 day */

/* TI Algo specific save restore structure definition */
/* Size of the OCV Lookup table */
#define OCV_TABLE_SIZE		21

struct edv_t {
	unsigned int voltage;
	unsigned int percent;
};
struct ti_fg_config_data {
	char battid[8];

	/* Save data validity & timestamp related params */
	int validity;
	int time_stamp_1;

	/* runtime data to be saved */
	int fcc;
	int nac;
	int rmc;
	int soc;
	int overcharge_q;
	int cycle_count;
	int cycle_q;
	unsigned int learned_cycle;
	unsigned int c_min_rate;

	/* cell config */
	int cc_voltage;
	int cc_current;
	int cc_term_voltage;
	int cc_q;
	unsigned int seq_cc;
	unsigned int design_capacity;
	unsigned int r_sense;
	unsigned int fcc_adjust;
	unsigned int max_impedance;
	unsigned int max_overcharge;  /* charge after CC, UNIT = % of fcc */
	unsigned int max_fcc_delta; /* UNIT = % of fcc */
	unsigned int low_temp;
	unsigned int deep_dsg_voltage;
	unsigned int light_load; /* valid dsg rate, UNIT = C, fcc/light_load */
	unsigned int near_full;  /* valid dsg learning cycle, UNIT = % of fcc */
	unsigned int recharge;  /* CC clear range, UNIT = % of fcc */
	unsigned int mode_switch_capacity;

	/* OCV Config */
	unsigned int voltage_diff;
	unsigned int current_diff;
	unsigned int sleep_enter_current;
	unsigned int sleep_enter_samples;
	unsigned int sleep_exit_current;
	unsigned int sleep_exit_samples;
	unsigned int long_sleep_current;
	unsigned int ocv_period;
	unsigned int relax_period;
	unsigned int flat_zone_low;
	unsigned int flat_zone_high;
	int c_factor;
	int z_factor;
	unsigned int max_delta;
	unsigned int fcc_update_valid_min;
	unsigned int fcc_update_valid_max;
	unsigned int table[OCV_TABLE_SIZE];
	/* EDV Config Data */
	unsigned int averaging;
	unsigned int seq_edv;
	unsigned int overload_current;
	unsigned int term_voltage;
	unsigned int edv_z_factor;
	struct edv_t edv[3];
};


static struct ti_fg_config_data parse_ti_cfg_data;
/*
 * BYT-CR 5120mAhr Battery config data.
 */
/* FG Calibration */
const struct cal_config cal_volt = {
	.offset = 0,
	.slope = 0x8000,
};

const struct cal_config cal_cur = {
	.offset = 0,
	.slope = 0x8000,
};

const struct cal_config cal_temp = {
	.offset = 0,
	.slope = 0x8000,
};
/* EDV Configuration */
static struct edv_config edv_cfg = {
	.averaging = true,
	/* seq_edv: number of edv samples */
	.seq_edv = 10,
	.overload_current = 2000,  /* 2A */
	/* system termination voltage */
	.term_voltage = 3200,
	.z_factor = 200,
	.edv = { /* need to change */
		{3500, 0},
		{3600, 5},
		{3660, 10},
	},
};
#define UNIT_U  1000
#define OCV_TABLE_SIZE 21

static struct rate_config rate_cfg = {
	.max_rate = 400,
	.min_rate = 50,
	.chg_gain = 5,
	.dsg_gain = 5,
};
/* OCV Configuration */
static struct ocv_config ocv_cfg = {
	.voltage_diff = 75,
	.current_diff = 30,

	.sleep_enter_current = 60,
	.sleep_enter_samples = 3,

	.sleep_exit_current = 80,
	.sleep_exit_samples = 3,

	.long_sleep_current = 200,
	.ocv_period = 600,
	.relax_period = 1800,

	.flat_zone_low = 3715,
	.flat_zone_high = 3777,

	/* New values */
	.c_factor = 0,
	.z_factor = 100,
	.fcc_update_valid_min = 10,
	.fcc_update_valid_max = 90,
	.table_size = OCV_TABLE_SIZE,
	/* %, max SOC correction by OCV */
	.max_delta = 1,

	.table = {
		3500, 3600, 3660, 3690, 3715,
		3727, 3741, 3758, 3777, 3800,
		3826, 3860, 3904, 3950, 4000,
		4050, 4100, 4155, 4210, 4265,
		4320
	},
};
/* General Battery Cell Configuration */
struct cell_config cell_cfg =  {
	.cc_voltage = 4250, /* need to change */
	.cc_current = 260, /* can be changed, if needed */
	.cc_q = 300, /* uAh, 18mA*60sec */
	.seq_cc = 3, /* polling interval * n, max 80secs */
	.cc_term_voltage = 4380, /* max bat charging term voltage */

	.ocv_below_edv1 = false,

	.design_capacity = 4600 * UNIT_U, /* uAh - need to change */
	.r_sense = 10, /* 20mohm - need to change*/
	.fcc_adjust = 2 * UNIT_U, /* uAh */
	.max_impedance = 300, /* mohm - no chnage*/

	.max_overcharge = 10, /* % of fcc */

	.max_fcc_delta = 2, /* %, fcc*0.02 */

	.low_temp = 119,
	.light_load = 20,  /* current = fcc/light_load, C/17*/
	.near_full = 90, /* %,  fcc*0.9 */

	.recharge = 93, /* for CC clear %, fcc*0.93 */ /*80 * UNIT_U */

	.mode_switch_capacity = 5 * UNIT_U, /* uAh */

	.ocv = &ocv_cfg,
	.edv = &edv_cfg,
	.rate = &rate_cfg,
	.cal_volt = &cal_volt,
	.cal_cur = &cal_cur,
	.cal_temp = &cal_temp,
};

struct cell_state ti_fg_cell_cfg = {
	.config = &cell_cfg,
};

static int update_ti_fg_conf(unsigned char *cfg,
				struct ti_fg_config_data *parse_ti_cfg_data)
{
	unsigned int i, j;

	memcpy(parse_ti_cfg_data, cfg, sizeof(struct ti_fg_config_data));

	SLOGD("\n printing the bin");

	SLOGD("\n validity = %d", parse_ti_cfg_data->validity);
	SLOGD("\n time_stamp_1 = %d", parse_ti_cfg_data->time_stamp_1);
	SLOGD("\n fcc = %d", parse_ti_cfg_data->fcc);
	SLOGD("\n nac = %d", parse_ti_cfg_data->nac);
	SLOGD("\n rmc = %d", parse_ti_cfg_data->rmc);
	SLOGD("\n soc = %d", parse_ti_cfg_data->soc);
	SLOGD("\n overcharge_q = %d", parse_ti_cfg_data->overcharge_q);
	SLOGD("\n cycle_count = %d", parse_ti_cfg_data->cycle_count);
	SLOGD("\n cycle_q = %d", parse_ti_cfg_data->cycle_q);
	SLOGD("\n learned_cycle = %d", parse_ti_cfg_data->learned_cycle);
	SLOGD("\n c_min_rate = %d", parse_ti_cfg_data->c_min_rate);
	SLOGD("\n cc_voltage = %d", parse_ti_cfg_data->cc_voltage);
	SLOGD("\n cc_current = %d", parse_ti_cfg_data->cc_current);
	SLOGD("\n cc_term_voltage = %d", parse_ti_cfg_data->cc_term_voltage);
	SLOGD("\n cc_q = %d", parse_ti_cfg_data->cc_q);
	SLOGD("\n seq_cc = %d", parse_ti_cfg_data->seq_cc);
	SLOGD("\n design_capacity = %d", parse_ti_cfg_data->design_capacity);
	SLOGD("\n r_sense = %d", parse_ti_cfg_data->r_sense);
	SLOGD("\n fcc_adjust = %d", parse_ti_cfg_data->fcc_adjust);
	SLOGD("\n max_impedance = %d", parse_ti_cfg_data->max_impedance);
	SLOGD("\n max_overcharge = %d", parse_ti_cfg_data->max_overcharge);
	SLOGD("\n max_fcc_delta = %d", parse_ti_cfg_data->max_fcc_delta);
	SLOGD("\n low_temp = %d", parse_ti_cfg_data->low_temp);
	SLOGD("\n deep_dsg_voltage = %d", parse_ti_cfg_data->deep_dsg_voltage);
	SLOGD("\n light_load = %d", parse_ti_cfg_data->light_load);
	SLOGD("\n near_full = %d", parse_ti_cfg_data->near_full);
	SLOGD("\n recharge = %d", parse_ti_cfg_data->recharge);
	SLOGD("\n mode_switch_capacity = %d",
				parse_ti_cfg_data->mode_switch_capacity);
	SLOGD("\n voltage_diff = %d", parse_ti_cfg_data->voltage_diff);
	SLOGD("\n current_diff = %d", parse_ti_cfg_data->current_diff);
	SLOGD("\n sleep_enter_cur = %d", parse_ti_cfg_data->sleep_enter_current);
	SLOGD("\n sleep_enter_smp = %d", parse_ti_cfg_data->sleep_enter_samples);
	SLOGD("\n sleep_exit_cur = %d", parse_ti_cfg_data->sleep_exit_current);
	SLOGD("\n sleep_exit_smp = %d", parse_ti_cfg_data->sleep_exit_samples);
	SLOGD("\n long_sleep_cur = %d", parse_ti_cfg_data->long_sleep_current);
	SLOGD("\n ocv_period = %d", parse_ti_cfg_data->ocv_period);
	SLOGD("\n relax_period = %d", parse_ti_cfg_data->relax_period);
	SLOGD("\n flat_zone_low = %d", parse_ti_cfg_data->flat_zone_low);
	SLOGD("\n flat_zone_high = %d", parse_ti_cfg_data->flat_zone_high);
	SLOGD("\n c_factor = %d", parse_ti_cfg_data->c_factor);
	SLOGD("\n z_factor = %d", parse_ti_cfg_data->z_factor);
	SLOGD("\n max_delta = %d", parse_ti_cfg_data->max_delta);
	SLOGD("\n fcc_update_valid_min = %d",
				parse_ti_cfg_data->fcc_update_valid_min);
	SLOGD("\n fcc_update_valid_max = %d",
				parse_ti_cfg_data->fcc_update_valid_max);
	SLOGD("\n table[0] = %d", parse_ti_cfg_data->table[0]);
	SLOGD("\n EDV");
	SLOGD("\n averaging = %d", parse_ti_cfg_data->averaging);
	SLOGD("\n seq_edv = %d", parse_ti_cfg_data->seq_edv);
	SLOGD("\n overload_current = %d", parse_ti_cfg_data->overload_current);
	SLOGD("\n term_voltage = %d", parse_ti_cfg_data->term_voltage);
	SLOGD("\n edv_z_factor = %d", parse_ti_cfg_data->edv_z_factor);
	SLOGD("\n edv[0][0] = %d, %d", parse_ti_cfg_data->edv[0].voltage,
				parse_ti_cfg_data->edv[0].percent);
	SLOGD("\n edv[1][1] = %d, %d", parse_ti_cfg_data->edv[1].voltage,
				parse_ti_cfg_data->edv[1].percent);
	SLOGD("\n edv[2][2] = %d, %d", parse_ti_cfg_data->edv[2].voltage,
				parse_ti_cfg_data->edv[2].percent);

	return 0;
}

int update_fg_conf(unsigned char *cfg, void *parse_cfg)
{

	SLOGD("Entered update_fg_conf\n");

	parse_cfg = (struct ti_fg_config_data *)&parse_ti_cfg_data;

	update_ti_fg_conf(cfg, parse_cfg);

	return 0;
}

void populate_config_data(struct cell_state *cfg)
{
	int i;

	SLOGD("\n Populating the data from xml\n");

	cfg->config->cc_voltage = parse_ti_cfg_data.cc_voltage;
	cfg->config->cc_current = parse_ti_cfg_data.cc_current;
	cfg->config->cc_term_voltage = parse_ti_cfg_data.cc_term_voltage;
	cfg->config->cc_q = parse_ti_cfg_data.cc_q;
	cfg->config->seq_cc = parse_ti_cfg_data.seq_cc;
	cfg->config->design_capacity = parse_ti_cfg_data.design_capacity;
	cfg->config->r_sense = parse_ti_cfg_data.r_sense;
	cfg->config->fcc_adjust = parse_ti_cfg_data.fcc_adjust;
	cfg->config->max_impedance = parse_ti_cfg_data.max_impedance;
	cfg->config->max_overcharge = parse_ti_cfg_data.max_overcharge;
	cfg->config->max_fcc_delta = parse_ti_cfg_data.max_fcc_delta;
	cfg->config->low_temp = parse_ti_cfg_data.low_temp;
	cfg->config->recharge = parse_ti_cfg_data.recharge;
	cfg->config->light_load = parse_ti_cfg_data.light_load;
	cfg->config->near_full = parse_ti_cfg_data.near_full;
	cfg->config->mode_switch_capacity = parse_ti_cfg_data.mode_switch_capacity;
	cfg->config->deep_dsg_voltage = parse_ti_cfg_data.deep_dsg_voltage;


	cfg->config->edv->averaging = parse_ti_cfg_data.averaging;
	cfg->config->edv->seq_edv = parse_ti_cfg_data.seq_edv;
	cfg->config->edv->overload_current = parse_ti_cfg_data.overload_current;
	cfg->config->edv->term_voltage = parse_ti_cfg_data.term_voltage;
	cfg->config->edv->z_factor = parse_ti_cfg_data.edv_z_factor;
	cfg->config->edv->edv[0].voltage = parse_ti_cfg_data.edv[0].voltage;
	cfg->config->edv->edv[0].percent = parse_ti_cfg_data.edv[0].percent;
	cfg->config->edv->edv[1].voltage = parse_ti_cfg_data.edv[1].voltage;
	cfg->config->edv->edv[1].percent = parse_ti_cfg_data.edv[1].percent;
	cfg->config->edv->edv[2].voltage = parse_ti_cfg_data.edv[2].voltage;
	cfg->config->edv->edv[2].percent = parse_ti_cfg_data.edv[2].percent;


	cfg->config->ocv->voltage_diff = parse_ti_cfg_data.voltage_diff;
	cfg->config->ocv->current_diff = parse_ti_cfg_data.current_diff;
	cfg->config->ocv->sleep_enter_current = parse_ti_cfg_data.sleep_enter_current;
	cfg->config->ocv->sleep_enter_samples = parse_ti_cfg_data.sleep_enter_samples;
	cfg->config->ocv->sleep_exit_current = parse_ti_cfg_data.sleep_exit_current;
	cfg->config->ocv->sleep_exit_samples = parse_ti_cfg_data.sleep_exit_samples;
	cfg->config->ocv->long_sleep_current = parse_ti_cfg_data.long_sleep_current;
	cfg->config->ocv->ocv_period = parse_ti_cfg_data.ocv_period;
	cfg->config->ocv->relax_period = parse_ti_cfg_data.relax_period;
	cfg->config->ocv->flat_zone_low = parse_ti_cfg_data.flat_zone_low;
	cfg->config->ocv->flat_zone_high = parse_ti_cfg_data.flat_zone_high;
	cfg->config->ocv->c_factor = parse_ti_cfg_data.c_factor;
	cfg->config->ocv->z_factor = parse_ti_cfg_data.z_factor;
	cfg->config->ocv->max_delta = parse_ti_cfg_data.max_delta;

	cfg->config->ocv->fcc_update_valid_min = parse_ti_cfg_data.fcc_update_valid_min;
	cfg->config->ocv->fcc_update_valid_max = parse_ti_cfg_data.fcc_update_valid_max;

	for (i = 0; i < OCV_TABLE_SIZE; i++)
		cfg->config->ocv->table[i] = parse_ti_cfg_data.table[i];

	SLOGD("After Populating config data\n");

}

int fg_algo_init(void *cfg, struct fg_in_params_t *fg_in_params,
				struct fg_out_params_t *fg_out_params)
{
	int ret = 0, saved_soc = 0;
	struct ti_fg_config_data *ti_data = (struct ti_fg_config_data *)cfg;
	struct timeval time;

	SLOGD("Enter fg_algo_init in config_ti_parse, validity = %d\n",
			parse_ti_cfg_data.validity);

	if (parse_ti_cfg_data.design_capacity)
		populate_config_data(&ti_fg_cell_cfg);

	/* Read the time and compare the time stamp */
	if (gettimeofday(&time, NULL) < 0) {
		SLOGE("Failed to get the time err = %d", errno);
		parse_ti_cfg_data.validity = 0;
	}

	if (abs(time.tv_sec - parse_ti_cfg_data.time_stamp_1) >
		POWER_OFF_TIME_THRESHOLD) {
		SLOGE("Target was in poweroff mode too long. don't restore the data");
		parse_ti_cfg_data.validity = 0;
	}

	/* Restore the nac and design capacity before fg_init */
	if (parse_ti_cfg_data.fcc) {
		ti_fg_cell_cfg.config->design_capacity = parse_ti_cfg_data.fcc;
		saved_soc = parse_ti_cfg_data.soc;
	}

	fg_init(&ti_fg_cell_cfg, fg_in_params->vboot/1000, 0);

	if ((abs(ti_fg_cell_cfg.soc - saved_soc) < 12) &&
			parse_ti_cfg_data.validity) {
		/* Restore the saved params from parse config */
		ti_fg_cell_cfg.fcc = parse_ti_cfg_data.fcc;
		ti_fg_cell_cfg.nac = parse_ti_cfg_data.nac;
		ti_fg_cell_cfg.rmc = parse_ti_cfg_data.rmc;
		ti_fg_cell_cfg.soc = parse_ti_cfg_data.soc;
		ti_fg_cell_cfg.overcharge_q = parse_ti_cfg_data.overcharge_q;
		ti_fg_cell_cfg.cycle_count = parse_ti_cfg_data.cycle_count;
		ti_fg_cell_cfg.cycle_q = parse_ti_cfg_data.cycle_q;
		ti_fg_cell_cfg.learned_cycle = parse_ti_cfg_data.learned_cycle;
		ti_fg_cell_cfg.c_min_rate = parse_ti_cfg_data.c_min_rate;
	}
	/*Copy the Algo Output and write it to the sysfs entries*/
	fg_out_params->capacity = ti_fg_cell_cfg.soc;
	fg_out_params->nac = ti_fg_cell_cfg.nac;
	fg_out_params->fcc = ti_fg_cell_cfg.fcc;

	cfg = (void *)&parse_ti_cfg_data;
	return ret;
}

int fg_algo_run(void *cfg, struct fg_in_params_t *fg_in_params,
				struct fg_out_params_t *fg_out_params)
{
	int ret = 0;

	/* If calibrate flag is set, reset the flag before fg_process */
	if (ti_fg_cell_cfg.calibrate)
		ti_fg_cell_cfg.calibrate = 0;

	/* Disable overcharge_q feature in the Algo */
	ti_fg_cell_cfg.overcharge_q = 0;

	fg_process(&ti_fg_cell_cfg, fg_in_params->delta_q,
	fg_in_params->vbatt/1000, fg_in_params->iavg/1000,
	fg_in_params->bat_temp/1000);

	/*Copy the Algo Output and write it to the sysfs entries*/
	fg_out_params->capacity = ti_fg_cell_cfg.soc;
	fg_out_params->nac = ti_fg_cell_cfg.nac;
	fg_out_params->fcc = ti_fg_cell_cfg.fcc;
	fg_out_params->cyc_cnt = ti_fg_cell_cfg.cycle_count;
	fg_out_params->cc_calib = ti_fg_cell_cfg.calibrate;

	/* Copy the data to secondary file */
	parse_ti_cfg_data.fcc = fg_out_params->fcc;
	parse_ti_cfg_data.nac = fg_out_params->nac;
	parse_ti_cfg_data.rmc = ti_fg_cell_cfg.rmc;
	parse_ti_cfg_data.soc = fg_out_params->capacity;
	parse_ti_cfg_data.overcharge_q = ti_fg_cell_cfg.overcharge_q;
	parse_ti_cfg_data.cycle_count = fg_out_params->cyc_cnt;
	parse_ti_cfg_data.cycle_q = ti_fg_cell_cfg.cycle_q;
	parse_ti_cfg_data.learned_cycle = ti_fg_cell_cfg.learned_cycle;
	parse_ti_cfg_data.c_min_rate = ti_fg_cell_cfg.c_min_rate;

	return ret;
}

int fg_algo_get_runtime_data(char *save_data, bool validity_flag)
{
	int ret;
	struct timeval time;

	if (gettimeofday(&time, NULL) < 0)
		SLOGE("Failed to get the time, errno = %d", errno);
	else
		parse_ti_cfg_data.time_stamp_1 = time.tv_sec;

	parse_ti_cfg_data.validity = (int)(validity_flag);

	memcpy(save_data, &parse_ti_cfg_data, sizeof(struct ti_fg_config_data));

	return 0;
}
