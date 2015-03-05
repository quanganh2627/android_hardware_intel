#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <assert.h>
#include "../include/libsensorhub.h"
#include "../include/bist.h"

void sensor_name(int name)
{

	switch (name) {
		case SENSOR_ACCELEROMETER:
			printf("accel ");
			break;
		case SENSOR_GYRO:
			printf("gyro ");
			break;
		case SENSOR_COMP:
			printf("comp ");
			break;
		case SENSOR_BARO:
			printf("baro ");
			break;
		case SENSOR_ALS:
			printf("ALS ");
			break;
		case SENSOR_PROXIMITY:
			printf("PS ");
			break;
		case SENSOR_TC:
			printf("TC ");
			break;
		case SENSOR_LPE:
			printf("LPE ");
			break;
		default:
			printf("%d ", name);
			break;
	}
}

void sensor_status(int status)
{

	switch (status) {
		case BIST_RET_OK:
			printf("OK.\n");
			break;
		case BIST_RET_ERR_SAMPLE:
			printf("couldn't sample properly.\n");
			break;
		case BIST_RET_ERR_HW:
			printf("sesnor HW is wrong.\n");
			break;
		case BIST_RET_NOSUPPORT:
			printf("no such sensor HW on board.\n");
			break;
		default: break;
	}
}

int main (void)
{
	int i, ret = 0;

	struct bist_data bist;
	ret = run_bist(&bist);
	if (ret != ERROR_NONE) {
		printf("Can't get bist data!");
		exit(1);
	}
	for ( i = 0; i < PHY_SENSOR_MAX_NUM; i++) {
		sensor_name(i);
		printf("\tbist test result is\t");
		sensor_status(bist.result[i]);
	}
	return 0;
}
