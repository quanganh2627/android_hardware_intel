#include "libsensorhub.h"

typedef enum {
	BIST_RET_OK = 0,
	BIST_RET_ERR_SAMPLE,
	BIST_RET_ERR_HW,
	BIST_RET_NOSUPPORT,
} bist_result_t;

#define PHY_SENSOR_MAX_NUM 10

struct bist_data {
	unsigned char result[PHY_SENSOR_MAX_NUM];
}__attribute__ ((packed));

/* the BIST results are returned in bist[]; use sensor_type
   as array index to get result of certain sensor type */
error_t run_bist(struct bist_data *bist);
