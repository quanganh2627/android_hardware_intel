#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <assert.h>
#include "../include/libsensorhub.h"


#define GET	(0 << 0)
#define SET	(1 << 0)
#define COMP	(0 << 1)
#define GYRO	(1 << 1)
#define ERR	(1 << 2)

static void dump_calibration_info(int gyro, struct cmd_calibration_param * param)
{
	printf ("calibration result: %u\n", param->calibrated);

	if (gyro) {
		printf ("x y z: %d %d %d\n", param->cal_param.gyro.x,
						param->cal_param.gyro.y,
						param->cal_param.gyro.z);
	} else {
		printf ("offset: %d %d %d\n", param->cal_param.compass.offset[0],
						param->cal_param.compass.offset[1],
						param->cal_param.compass.offset[2]);
		printf ("w: %d %d %d\n   %d %d %d\n   %d %d %d\n",
						param->cal_param.compass.w[0][0],
						param->cal_param.compass.w[0][1],
						param->cal_param.compass.w[0][2],
						param->cal_param.compass.w[1][0],
						param->cal_param.compass.w[1][1],
						param->cal_param.compass.w[1][2],
						param->cal_param.compass.w[2][0],
						param->cal_param.compass.w[2][1],
						param->cal_param.compass.w[2][2]);
		printf ("bfield: %d\n", param->cal_param.compass.bfield);
	}
}

static int get_set(int argc, char* argv[])
{
	static struct option opts[] = {
		{"COMP", 0, NULL, 'C'},
		{"GYRO", 0, NULL, 'G'},
		{"get", 0, NULL, 'g'},
		{"set", 0, NULL, 's'},
		{0, 0, NULL, 0},
	};

	static int req = 0;

	int i, index;

	while (1) {
		i = getopt_long(argc, argv, "CGgs", opts, &index);
		if (i == -1)
			break ;

		switch (i){
		case 'C':
			req |= COMP;
			break;
		case 'G':
			req |= GYRO;
			break;
		case 'g':
			req |= GET;
			break;
		case 's':
			req |= SET;
			break;
		default:
			req = ERR;
		}

	if (req == ERR)
		break;
	}

	return req;

}

static char* enum2str[] = {
	[COMP | GET] = "comp get",
	[COMP | SET] = "comp set",
	[GYRO | GET] = "gyro get",
	[GYRO | SET] = "gyro set",
	[ERR] = "err",
};

static void usage(void)
{
	printf ("\nUsage: compasscal [--COMP | --GYRO][--set | --get]\n");
	printf ("\t --COMP target is compass calibration.\n");
	printf ("\t --GYRO target is compass calibration.\n");
	printf ("\t --set reset calibration parameters.\n");
	printf ("\t --get dump current calibration parameters data.\n");
}

int main (int argc, char* argv[])
{
	int req;
	struct cmd_calibration_param param;
	handle_t cal;

	req =  get_set(argc, argv);
	printf("Your request is %s\n", enum2str[req]);
	if (req == ERR){
		usage();
		return 0;
	}

	memset(&param, 0, sizeof(struct cmd_calibration_param));

	if (req >> 1)
		cal = psh_open_session(SENSOR_CALIBRATION_GYRO);
	else
		cal = psh_open_session(SENSOR_CALIBRATION_COMP);
	assert(cal != NULL);

	if (req & SET){
		param.sub_cmd = SUBCMD_CALIBRATION_START;
		psh_set_calibration(cal, &param);
	} else {
		psh_get_calibration(cal, &param);
		dump_calibration_info(req >> 1, &param);
	}

	psh_close_session(cal);

	return 0;
}
