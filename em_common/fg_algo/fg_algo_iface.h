#include <stdbool.h>

static struct fg_in_params_t {
	int vbatt;
	int vboot;
	int vocv;
	int ibatt;
	int ibat_boot;
	int iavg;
	int bat_temp;
	int delta_q;
} fg_in_params;

static struct fg_out_params_t {

	int capacity;
	int nac;
	int fcc;
	int cyc_cnt;
	int cc_calib;
} fg_out_params;

int fg_algo_init(void *cfg, struct fg_in_params_t *fg_in_params,
					struct fg_out_params_t *fg_out_params);
int fg_algo_run(void *cfg, struct fg_in_params_t *fg_in_params,
					struct fg_out_params_t *fg_out_params);

/* Function routine which parses the binary file into config data */
int update_fg_conf(unsigned char *cfg, void *parse_cfg);

/* Function routine which populates the data to be saved */
int fg_algo_get_runtime_data(char *save_data, bool validity_flag);
