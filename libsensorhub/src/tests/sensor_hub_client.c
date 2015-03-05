#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <cutils/log.h>

#include "../include/libsensorhub.h"
#include "../include/bist.h"

static void dump_accel_data(int fd)
{
	char buf[512];
	int size = 0;
	struct accel_data *p_accel_data;

	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;

		p_accel_data = (struct accel_data *)buf;
		while (size > 0) {
			printf("x, y, z is: %d, %d, %d, size is %d \n",
					p_accel_data->x, p_accel_data->y,
					p_accel_data->z, size);
			size = size - sizeof(struct accel_data);
			p = p + sizeof(struct accel_data);
			p_accel_data = (struct accel_data *)p;
		}
	}
}

static void dump_gyro_data(int fd)
{
	char buf[512];
	int size = 0;
	struct gyro_raw_data *p_gyro_raw_data;

	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;

		p_gyro_raw_data = (struct gyro_raw_data *)buf;
		while (size > 0) {
		printf("x, y, z is: %d, %d, %d, size is %d \n",
					p_gyro_raw_data->x, p_gyro_raw_data->y,
					p_gyro_raw_data->z, size);
			size = size - sizeof(struct gyro_raw_data);
			p = p + sizeof(struct gyro_raw_data);
			p_gyro_raw_data = (struct gyro_raw_data *)p;
		}
	}
}

static void dump_comp_data(int fd)
{
	char buf[512];
	int size = 0;
	struct compass_raw_data *p_compass_raw_data;

	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;

		p_compass_raw_data = (struct compass_raw_data *)buf;
		while (size > 0) {
		printf("accuracy:%d  x, y, z is: %d, %d, %d, size is %d \n",
				p_compass_raw_data->accuracy,
				p_compass_raw_data->x, p_compass_raw_data->y,
				p_compass_raw_data->z, size);
			size = size - sizeof(struct compass_raw_data);
			p = p + sizeof(struct compass_raw_data);
			p_compass_raw_data = (struct compass_raw_data *)p;
		}
	}
}

static void dump_tc_data(int fd)
{
	char buf[512];
	int size = 0;
	struct tc_data *p_tc_data;

	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;

		p_tc_data = (struct tc_data *)buf;
		while (size > 0) {
		printf("orien_xy, orien_z is: %d, %d, size is %d \n",
				p_tc_data->orien_xy, p_tc_data->orien_z,
				size);
			size = size - sizeof(struct tc_data);
			p = p + sizeof(struct tc_data);
			p_tc_data = (struct tc_data *)p;
		}
	}
}

static void dump_baro_data(int fd)
{
	char buf[512];
	int size = 0;
	struct baro_raw_data *p_baro_raw_data;

	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;

		p_baro_raw_data = (struct baro_raw_data *)buf;
		while (size > 0) {
			printf("baro raw data is %d, size is %d \n",
				p_baro_raw_data->p, size);
			size = size - sizeof(struct baro_raw_data);
			p = p + sizeof(struct baro_raw_data);
			p_baro_raw_data = (struct baro_raw_data *)p;
		}
	}
}

static void dump_als_data(int fd)
{
	char buf[512];
	int size = 0;
	struct als_raw_data *p_als_raw_data;

	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;

		p_als_raw_data = (struct als_raw_data *)buf;
		while (size > 0) {
			printf("ALS data is %d, size is %d \n",
				p_als_raw_data->lux, size);
			size = size - sizeof(struct als_raw_data);
			p = p + sizeof(struct als_raw_data);
			p_als_raw_data = (struct als_raw_data *)p;
		}
	}

}

static void dump_proximity_data(int fd)
{
	char buf[512];
	int size = 0;
	struct ps_phy_data *p_ps_phy_data;

	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;

		p_ps_phy_data = (struct ps_phy_data *)buf;
		while (size > 0) {
			printf("proximity data is %d, size is %d \n",
				p_ps_phy_data->near, size);
			size = size - sizeof(struct ps_phy_data);
			p = p + sizeof(struct ps_phy_data);
			p_ps_phy_data = (struct ps_phy_data *)p;
		}
	}
}

static void dump_gs_data(int fd)
{
	char buf[4096];
	int size = 0;
	struct gs_data *p_gs_data;

	while ((size = read(fd, buf, 4096)) > 0) {
		char *p = buf;
		p_gs_data = (struct gs_data *)buf;

		while (size > 0) {
			printf("gs data size is %d\n", p_gs_data->size);
			size = size - (sizeof(struct gs_data) + p_gs_data->size);
			p = p + sizeof(struct gs_data) + p_gs_data->size;
			p_gs_data = (struct gs_data *)p;
		}
	}

}

static void dump_gesture_hmm_data(int fd)
{
	char buf[4096];
	int size = 0;
	struct gesture_hmm_data *p_gesture_hmm_data;

	while ((size = read(fd, buf, 4096)) > 0) {
		char *p = buf;
		p_gesture_hmm_data = (struct gesture_hmm_data *)buf;
		while (size > 0) {
			printf("prox/gesture data is %d, size is %d \n",
				p_gesture_hmm_data->prox_gesture, p_gesture_hmm_data->size);
			size = size - (sizeof(struct gesture_hmm_data) + p_gesture_hmm_data->size);
			p = p + sizeof(struct gesture_hmm_data) + p_gesture_hmm_data->size;
			p_gesture_hmm_data = (struct gesture_hmm_data *)p;
		}
	}
}

static void dump_activity_data(int fd)
{
	char buf[512];
	int size = 0;
	struct phy_activity_data *p_phy_activity_data;
	int i;

	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;

		p_phy_activity_data = (struct phy_activity_data *)buf;
		int unit_size = sizeof(p_phy_activity_data->len) +
			sizeof(p_phy_activity_data->values[0]) *
			p_phy_activity_data->len;
		while (size > 0) {
			printf("physical activity data is ");
			for (i = 0; i < p_phy_activity_data->len; ++i) {
				printf("%hd, ", p_phy_activity_data->values[i]);
			}
			printf("size is %d\n", size);
			size = size - unit_size;
			p = p + unit_size;
			p_phy_activity_data = (struct phy_activity_data *)p;
		}
	}
}

static void dump_gesture_flick_data(int fd)
{
	char buf[512];
	int size = 0;
	struct gesture_flick_data *p_gesture_flick_data;

	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;
		p_gesture_flick_data = (struct gesture_flick_data *)buf;
		while (size > 0) {
			printf("gesture flick data is %d, size is %d \n",
				p_gesture_flick_data->flick, size);
			size = size - sizeof(struct gesture_flick_data);
			p = p + sizeof(struct gesture_flick_data);
			p_gesture_flick_data = (struct gesture_flick_data *)p;
		}
	}
}

static void dump_shaking_data(int fd)
{
	char buf[512];
	int size = 0;
	struct shaking_data *p_shaking_data;

	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;
		p_shaking_data = (struct shaking_data *)buf;
		while (size > 0) {
			printf("shaking data is %d, size is %d \n",
				p_shaking_data->shaking, size);
			size = size - sizeof(struct shaking_data);
			p = p + sizeof(struct shaking_data);
			p_shaking_data = (struct shaking_data *)p;
		}
	}
}

static void dump_stap_data(int fd)
{
	char buf[512];
	int size = 0;
	struct stap_data *p_stap_data;

	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;
		p_stap_data = (struct stap_data *)buf;
		while (size > 0) {
			printf("stap data is %d, size is %d \n",
				p_stap_data->stap, size);
			size = size - sizeof(struct stap_data);
			p = p + sizeof(struct stap_data);
			p_stap_data = (struct stap_data *)p;
		}
	}
}

static void dump_rotation_vector_data(int fd)
{
	char buf[512];
	int size = 0;
	struct rotation_vector_data *p_rotation_vector_data;

	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;
		p_rotation_vector_data = (struct rotation_vector_data *)buf;
		while (size > 0) {
			printf("rotation vector data is (%d, %d, %d, %d), size"
				" is %d \n", p_rotation_vector_data->x,
				p_rotation_vector_data->y,
				p_rotation_vector_data->z,
				p_rotation_vector_data->w, size);
			size = size - sizeof(struct rotation_vector_data);
			p = p + sizeof(struct rotation_vector_data);
			p_rotation_vector_data = (struct rotation_vector_data *)p;
		}
	}
}


static void dump_game_rotation_vector_data(int fd)
{
	char buf[512];
	int size = 0;
	struct game_rotation_vector_data *p_game_rotation_vector_data;

	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;
		p_game_rotation_vector_data = (struct game_rotation_vector_data *)buf;
		while (size > 0) {
			printf("game rotation vector data is (%d, %d, %d, %d), size"
				" is %d \n", p_game_rotation_vector_data->x,
				p_game_rotation_vector_data->y,
				p_game_rotation_vector_data->z,
				p_game_rotation_vector_data->w, size);
			size = size - sizeof(struct game_rotation_vector_data);
			p = p + sizeof(struct game_rotation_vector_data);
			p_game_rotation_vector_data = (struct game_rotation_vector_data *)p;
		}
	}
}


static void dump_geomagnetic_rotation_vector_data(int fd)
{
	char buf[512];
	int size = 0;
	struct geomagnetic_rotation_vector_data *p_geomagnetic_rotation_vector_data;

	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;
		p_geomagnetic_rotation_vector_data = (struct geomagnetic_rotation_vector_data *)buf;
		while (size > 0) {
			printf("geomagnetic rotation vector data is (%d, %d, %d, %d), size"
				" is %d \n", p_geomagnetic_rotation_vector_data->x,
				p_geomagnetic_rotation_vector_data->y,
				p_geomagnetic_rotation_vector_data->z,
				p_geomagnetic_rotation_vector_data->w, size);
			size = size - sizeof(struct geomagnetic_rotation_vector_data);
			p = p + sizeof(struct geomagnetic_rotation_vector_data);
			p_geomagnetic_rotation_vector_data = (struct geomagnetic_rotation_vector_data *)p;
		}
	}
}

static void dump_gravity_data(int fd)
{
	char buf[512];
	int size = 0;
	struct gravity_data *p_gravity_data;

	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;
		p_gravity_data = (struct gravity_data *)buf;
		while (size > 0) {
			printf("gravity data is (%d, %d, %d), size is %d\n",
				p_gravity_data->x,
				p_gravity_data->y,
				p_gravity_data->z, size);
			size = size - sizeof(struct gravity_data);
			p = p + sizeof(struct gravity_data);
			p_gravity_data = (struct gravity_data *)p;
		}
	}
}

static void dump_linear_accel_data(int fd)
{
	char buf[512];
	int size = 0;
	struct linear_accel_data *p_linear_accel_data;

	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;
		p_linear_accel_data = (struct linear_accel_data *)buf;
		while (size > 0) {
			printf("linear acceleration data is (%d, %d, %d), size is %d\n",
				p_linear_accel_data->x,
				p_linear_accel_data->y,
				p_linear_accel_data->z, size);
			size = size - sizeof(struct linear_accel_data);
			p = p + sizeof(struct linear_accel_data);
			p_linear_accel_data = (struct linear_accel_data *)p;
		}
	}
}

static void dump_orientation_data(int fd)
{
	char buf[512];
	int size = 0;
	struct orientation_data *p_orientation_data;

	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;
		p_orientation_data = (struct orientation_data *)buf;
		while (size > 0) {
			printf("orientation data is (%d, %d, %d), size is %d\n",
				p_orientation_data->azimuth,
				p_orientation_data->pitch,
				p_orientation_data->roll, size);
			size = size - sizeof(struct orientation_data);
			p = p + sizeof(struct orientation_data);
			p_orientation_data = (struct orientation_data *)p;
		}
	}
}

static void dump_9dof_data(int fd)
{
	char buf[512];
	int size = 0;
	struct ndof_data *p_ndof_data;

	printf("dump_9dof_data 1 \n");
	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;
		p_ndof_data = (struct ndof_data *)buf;
		while (size > 0) {
			printf("9dof data is (%d, %d, %d, %d, %d, %d, %d, %d, %d), size is %d \n",
				p_ndof_data->m[0], p_ndof_data->m[1], p_ndof_data->m[2], p_ndof_data->m[3],
				p_ndof_data->m[4], p_ndof_data->m[5], p_ndof_data->m[6], p_ndof_data->m[7],
				p_ndof_data->m[8], size);
			size = size - sizeof(struct ndof_data);
			p = p + sizeof(struct ndof_data);
			p_ndof_data = (struct ndof_data *)p;
		}
	}
	printf("dump_9dof_data 2 \n");
}

static void dump_6dofag_data(int fd)
{
	char buf[512];
	int size = 0;
	struct ndofag_data *p_ndofag_data;

	printf("dump_6dofag_data 1 \n");
	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;
		p_ndofag_data = (struct ndofag_data *)buf;
		while (size > 0) {
			printf("6dofag data is (%d, %d, %d, %d, %d, %d, %d, %d, %d), size is %d \n",
				p_ndofag_data->m[0], p_ndofag_data->m[1], p_ndofag_data->m[2], p_ndofag_data->m[3],
				p_ndofag_data->m[4], p_ndofag_data->m[5], p_ndofag_data->m[6], p_ndofag_data->m[7],
				p_ndofag_data->m[8], size);
			size = size - sizeof(struct ndofag_data);
			p = p + sizeof(struct ndofag_data);
			p_ndofag_data = (struct ndofag_data *)p;
		}
	}
	printf("dump_6dofag_data 2 \n");
}

static void dump_6dofam_data(int fd)
{
	char buf[512];
	int size = 0;
	struct ndofam_data *p_ndofam_data;

	printf("dump_6dofam_data 1 \n");
	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;
		p_ndofam_data = (struct ndofam_data *)buf;
		while (size > 0) {
			printf("6dofam data is (%d, %d, %d, %d, %d, %d, %d, %d, %d), size is %d \n",
				p_ndofam_data->m[0], p_ndofam_data->m[1], p_ndofam_data->m[2], p_ndofam_data->m[3],
				p_ndofam_data->m[4], p_ndofam_data->m[5], p_ndofam_data->m[6], p_ndofam_data->m[7],
				p_ndofam_data->m[8], size);
			size = size - sizeof(struct ndofam_data);
			p = p + sizeof(struct ndofam_data);
			p_ndofam_data = (struct ndofam_data *)p;
		}
	}
	printf("dump_6dofam_data 2 \n");
}

static void dump_pedometer_data(int fd)
{
	char buf[512];
	int size = 0;
	struct pedometer_data *p_pedometer_data;

	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;
		p_pedometer_data = (struct pedometer_data *)buf;
		while (size > 0) {
			printf("pedometer data is %d\n", p_pedometer_data->num);
			size = size - sizeof(struct pedometer_data);
			p = p + sizeof(struct pedometer_data);
			p_pedometer_data = (struct pedometer_data *)p;
		}
	}

}

static void dump_mag_heading_data(int fd)
{
	char buf[512];
	int size = 0;
	struct mag_heading_data *p_mag_heading_data;

	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;
		p_mag_heading_data = (struct mag_heading_data *)buf;
		while (size > 0) {
			printf("magnetic north heading is %d, size is %d\n",
				p_mag_heading_data->heading, size);
			size = size - sizeof(struct mag_heading_data);
			p = p + sizeof(struct mag_heading_data);
			p_mag_heading_data = (struct mag_heading_data *)p;
		}
	}
}

static void dump_lpe_data(int fd)
{
	char buf[512];
	int size = 0;
	struct lpe_phy_data *p_lpe_data;

	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;
		p_lpe_data = (struct lpe_phy_data *)buf;
		while (size > 0) {
			printf("lpe_msg is %u, size is %d\n",
				p_lpe_data->lpe_msg, size);
			size = size - sizeof(struct lpe_phy_data);
			p = p + sizeof(struct lpe_phy_data);
			p_lpe_data = (struct lpe_phy_data *)p;
		}
	}

}

static void dump_md_data(int fd)
{
	char buf[512];
	int size = 0;
	struct md_data *p_md_data;

	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;
		p_md_data = (struct md_data *)buf;
		while (size > 0) {
			printf("move detect data is %d\n", p_md_data->state);
			size = size - sizeof(struct md_data);
			p = p + sizeof(struct md_data);
			p_md_data = (struct md_data *)p;
		}
	}
}

static void dump_ptz_data(int fd)
{
	char buf[512];
	int size = 0;
	struct ptz_data *p_ptz_data;

	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;
		p_ptz_data = (struct ptz_data *)buf;
		while (size > 0) {
			printf("pantiltzoom data is %d, angle is %d, size is %d \n",
				p_ptz_data->cls_name, p_ptz_data->angle, size);
			size = size - sizeof(struct ptz_data);
			p = p + sizeof(struct ptz_data);
			p_ptz_data = (struct ptz_data *)p;
		}
	}
}

static void dump_lv_data(int fd)
{
	char buf[512];
	int size = 0;
	struct lv_data *p_lv_data;

	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;
		p_lv_data = (struct lv_data *)buf;
		while (size > 0) {
			printf("lift vertical data is %d\n", p_lv_data->state);
			size = size - sizeof(struct lv_data);
			p = p + sizeof(struct lv_data);
			p_lv_data = (struct lv_data *)p;
		}
	}
}

static void dump_device_position_data(int fd)
{
	char buf[512];
	int size = 0;
	struct device_position_data *p_device_position_data;

	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;
		p_device_position_data = (struct device_position_data *)buf;
		while (size > 0) {
			printf("device position data is %d, size is %d \n",
				p_device_position_data->pos, size);
			size = size - sizeof(struct device_position_data);
			p = p + sizeof(struct device_position_data);
			p_device_position_data = (struct device_position_data *)p;
		}
	}
}

static void dump_stepcounter_data(int fd)
{
        char buf[512];
        int size = 0;
        struct stepcounter_data *p_stepcounter_data;

        while ((size = read(fd, buf, 512)) > 0) {
                char *p = buf;
                p_stepcounter_data = (struct stepcounter_data *)buf;
                while (size > 0) {
                        printf("step counter data is %d \n",
                                p_stepcounter_data->num);
                        size = size - sizeof(struct stepcounter_data);
                        p = p + sizeof(struct stepcounter_data);
                        p_stepcounter_data = (struct stepcounter_data *)p;
                }
        }
}

static void dump_stepdetector_data(int fd)
{
        char buf[512];
        int size = 0;
        struct stepdetector_data *p_stepdetector_data;

        while ((size = read(fd, buf, 512)) > 0) {
                char *p = buf;
                p_stepdetector_data = (struct stepdetector_data *)buf;
                while (size > 0) {
                        printf("step detector data is %d \n",
                                p_stepdetector_data->state);
                        size = size - sizeof(struct stepdetector_data);
                        p = p + sizeof(struct stepdetector_data);
                        p_stepdetector_data = (struct stepdetector_data *)p;
                }
        }
}

static void dump_significantmotion_data(int fd)
{
        char buf[512];
        int size = 0;
        struct sm_data *p_sm_data;

        while ((size = read(fd, buf, 512)) > 0) {
                char *p = buf;
                p_sm_data = (struct sm_data *)buf;
                while (size > 0) {
                        printf("significant motion data is %d \n",
                                p_sm_data->state);
                        size = size - sizeof(struct sm_data);
                        p = p + sizeof(struct sm_data);
                        p_sm_data = (struct sm_data *)p;
                }
        }
}

static void dump_lift_look_data(int fd)
{
	char buf[512];
	int size = 0;
	struct lift_look_data *p_lift_look_data;

        while ((size = read(fd, buf, 512)) > 0) {
                char *p = buf;
		p_lift_look_data = (struct lift_look_data *)buf;
		while (size > 0) {
			printf("lift&look data is %d, size is %d \n",
				p_lift_look_data->liftlook, size);
			size = size - sizeof(struct lift_look_data);
			p = p + sizeof(struct lift_look_data);
			p_lift_look_data = (struct lift_look_data *)p;
		}
	}
}

static void dump_dtwgs_data(int fd)
{
	char buf[512];
	int size = 0;
	struct dtwgs_data *p_dtwgs_data;

	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;
		p_dtwgs_data = (struct dtwgs_data *)buf;
		while (size > 0) {
			printf("dtwgs data is %d, score is %d, size is %d \n",
				p_dtwgs_data->gsnum, p_dtwgs_data->score, size);
			size = size - sizeof(struct dtwgs_data);
			p_dtwgs_data = (struct dtwgs_data *)p;
		}
	}
}

static void usage()
{
	printf("\n Usage: sensorhub_client [OPTION...] \n");
	printf("  -c, --cmd-type		0, get_single; 1 get_streaming \n");
	printf("  -t, --sensor-type		0, accel; 1, gyro; 2, compass;"
					" 3, barometer; 4, ALS; 5, Proximity;"
					" 6, terminal context;"
					" 7, LPE;"
					" 14, physical activity;"
					" 15, gesture spotting;"
					" 16, gesture flick;"
					" 17, rotation vector;"
					" 18, gravity;"
					" 19, linear acceleration;"
					" 20, orientation;"
					" 23, 9dof;"
					" 24, pedometer;"
					" 25, magnetic heading;"
					" 26, shaking;"
					" 27, move detect;"
					" 28, stap;"
					" 29, pan tilt zoom;"
					" 30, lift vertical;"
					" 31, device position;"
					" 32, step counter;"
					" 33, step detector;"
					" 34, significant motion;"
					" 35, game_rotation vector;"
					" 36, geomagnetic_rotation vector;"
					" 37, 6dofag;"
					" 38, 6dofam;"
					" 39, lift look;"
					" 40, dtwgs;"
					" 41, gesture hmm\n");
	printf("  -r, --date-rate		unit is Hz\n");
	printf("  -d, --buffer-delay		unit is ms, i.e. 1/1000 second\n");
	printf("  -p, --property-set		format: <property id>,<property value>\n");
	printf("  -h, --help			show this help message \n");

	exit(EXIT_SUCCESS);
}

int parse_prop_set(char *opt, int *prop, int *val)
{
	if (sscanf(opt, "%d,%d", prop, val) == 2)
		return 0;
	return -1;
}

int main(int argc, char **argv)
{
	handle_t handle;
	error_t ret;
	int fd, size = 0, cmd_type = -1, sensor_type = -1, data_rate = -1,
							buffer_delay = -1;
	int prop_ids[10];
	int prop_vals[10];
	int prop_count = 0;
	int i;

	char buf[512];
	struct accel_data *p_accel_data;

	while (1) {
		static struct option opts[] = {
			{"cmd", 1, NULL, 'c'},
			{"sensor-type", 1, NULL, 't'},
			{"data-rate", 1, NULL, 'r'},
			{"buffer-delay", 1, NULL, 'd'},
			{"property-set", 2, NULL, 'p'},
			{0, 0, NULL, 0}
		};
		int index, o;

		o = getopt_long(argc, argv, "c:t:r::d::p:", opts, &index);
		if (o == -1)
			break;
		switch (o) {
		case 'c':
			cmd_type = strtod(optarg, NULL);
			break;
		case 't':
			sensor_type = strtod(optarg, NULL);
			break;
		case 'r':
			data_rate = strtod(optarg, NULL);
			break;
		case 'd':
			buffer_delay = strtod(optarg, NULL);
			break;
		case 'p':
			if (prop_count == sizeof(prop_ids) / sizeof(prop_ids[0]))
				break;
			if (parse_prop_set(optarg,
				prop_ids + prop_count,
				prop_vals + prop_count)) {
				usage();
			}
			++prop_count;
			break;
		case 'h':
			usage();
			break;
		default:
			;
		}
	}

	if (cmd_type == -1 || (sensor_type == -1)
			|| (cmd_type != 0 && cmd_type != 1)) {
		usage();
		return 0;
	}

	if ((cmd_type == 1) && ((data_rate == -1) || (buffer_delay == -1))) {
		usage();
		return 0;
	}

	printf("cmd_type is %d, sensor_type is %d, data_rate is %d Hz, "
			"buffer_delay is %d ms\n", cmd_type, sensor_type,
						data_rate, buffer_delay);
#undef LOG_TAG
#define LOG_TAG "sensorhub_test"
	LOGD("sensor_type is %d, data_rate is %d Hz, buffer_delay is %d ms\n",
										sensor_type, data_rate, buffer_delay);

	handle = psh_open_session(sensor_type);

	if (handle == NULL) {
		printf("psh_open_session() returned NULL handle. \n");
		return -1;
	}

	if (cmd_type == 0) {
		p_accel_data = (struct accel_data *)buf;
		size = psh_get_single(handle, buf, 128);

		if (sensor_type == SENSOR_ACCELEROMETER) {
			struct accel_data *p_accel_data =
					(struct accel_data *)buf;
			printf("get_single returns, x, y, z is %d, %d, %d, "
				"size is %d \n", p_accel_data->x,
				p_accel_data->y, p_accel_data->z, size);
		} else if (sensor_type == SENSOR_GYRO) {
			struct gyro_raw_data *p_gyro_raw_data =
					(struct gyro_raw_data *)buf;
			printf("get_single returns, x, y, z is %d, %d, %d, "
				"size is %d \n", p_gyro_raw_data->x,
				p_gyro_raw_data->y, p_gyro_raw_data->z, size);
		} else if (sensor_type == SENSOR_COMP) {
			struct compass_raw_data *p_compass_raw_data =
					(struct compass_raw_data *)buf;
			printf("get_single returns, calibrated--%d\n x, y, z is %d, %d, %d, "
					"size is %d \n",
					p_compass_raw_data->accuracy,
					p_compass_raw_data->x,
					p_compass_raw_data->y,
					p_compass_raw_data->z, size);
		} else if (sensor_type == SENSOR_BARO) {
			struct baro_raw_data *p_baro_raw_data =
					(struct baro_raw_data *)buf;
			printf("get_single returns, baro raw data is %d \n",
					p_baro_raw_data->p);
		} else if (sensor_type == SENSOR_ALS) {
			struct als_raw_data *p_als_raw_data =
					(struct als_raw_data *)buf;
			printf("get_single returns, ALS raw data is %d\n",
					p_als_raw_data->lux);
		} else if (sensor_type == SENSOR_PROXIMITY) {
			struct ps_phy_data  *p_ps_phy_data =
					(struct ps_phy_data  *)buf;
			printf("get_single returns, near is %d\n",
					p_ps_phy_data->near);
		} else if (sensor_type == SENSOR_TC) {
			struct tc_data *p_tc_data = (struct tc_data *)buf;
			printf("get_single returns, orien_xy, "
					"orien_z is %d, %d size is %d \n",
					p_tc_data->orien_xy,
					p_tc_data->orien_z, size);
		} else if (sensor_type == SENSOR_LPE) {
						struct lpe_phy_data *p_lpe_phy_data =
										(struct lpe_phy_data *)buf;
						printf("get_single returns, lpe_msg is "
								"%u\n", p_lpe_phy_data->lpe_msg);
				} else if (sensor_type == SENSOR_ACTIVITY) {
			printf("activity doesn't support get_single\n");
		} else if (sensor_type == SENSOR_GS) {
			struct gs_data *p_gs_data =
					(struct gs_data *)buf;
			printf("get_single returns, size is %d\n",
					p_gs_data->size);
		} else if (sensor_type == SENSOR_GESTURE_HMM) {
			struct gesture_hmm_data *p_gesture_hmm_data =
					(struct gesture_hmm_data *)buf;
			printf("get_single returns, size is %d, gesture is %d\n",
					p_gesture_hmm_data->size, p_gesture_hmm_data->prox_gesture);
		} else if (sensor_type == SENSOR_GESTURE_FLICK) {
			struct gesture_flick_data *p_gesture_flick_data =
					(struct gesture_flick_data *)buf;
			printf("get_single returns, flick is %d\n",
					p_gesture_flick_data->flick);
		} else if (sensor_type == SENSOR_SHAKING) {
			struct shaking_data *p_shaking_data =
					(struct shaking_data *)buf;
			printf("get_single returns, shaking is %d\n",
					p_shaking_data->shaking);
		} else if (sensor_type == SENSOR_LIFT_VERTICAL) {
			struct lv_data *p_lv_data =
					(struct lv_data *)buf;
			printf("get_single returns, lift vertical is %d\n",
					p_lv_data->state);
		} else if (sensor_type == SENSOR_STAP) {
			struct stap_data *p_stap_data =
					(struct stap_data *)buf;
			printf("get_single returns, stap is %d\n",
					p_stap_data->stap);
		} else if (sensor_type == SENSOR_SIGNIFICANT_MOTION) {
			struct sm_data *p_sm_data =
					(struct sm_data *)buf;
			printf("get_single returns, significant motion is %d\n",
					p_sm_data->state);
		} else if (sensor_type == SENSOR_PAN_TILT_ZOOM) {
			struct ptz_data *p_ptz_data =
					(struct ptz_data *)buf;
			printf("get_single returns, ptz is %d %d\n",
					p_ptz_data->cls_name, p_ptz_data->angle);
		} else if (sensor_type == SENSOR_DEVICE_POSITION) {
			struct device_position_data *p_device_position_data =
					(struct device_position_data *)buf;
			printf("get_single returns, device position is %d\n",
					p_device_position_data->pos);
		} else if (sensor_type == SENSOR_LIFT_LOOK) {
			struct lift_look_data *p_lift_look_data =
					(struct lift_look_data *)buf;
			printf("get_single returns, lift look is %d\n",
					p_lift_look_data->liftlook);
		} else if (sensor_type == SENSOR_DTWGS) {
			struct dtwgs_data *p_dtwgs_data =
					(struct dtwgs_data *)buf;
			printf("get_single returns, dtwgs is (%d %d)\n",
					p_dtwgs_data->gsnum,
					p_dtwgs_data->score);
		} else if (sensor_type == SENSOR_ROTATION_VECTOR) {
			struct rotation_vector_data *p_rotation_vector_data =
					(struct rotation_vector_data *)buf;
			printf("get_single returns, rotation vector is (%d, %d,"
				" %d, %d)\n", p_rotation_vector_data->x,
				p_rotation_vector_data->y,
				p_rotation_vector_data->z,
				p_rotation_vector_data->w);
		} else if (sensor_type == SENSOR_GAME_ROTATION_VECTOR) {
			struct game_rotation_vector_data *p_game_rotation_vector_data =
					(struct game_rotation_vector_data *)buf;
			printf("get_single returns, game rotation vector is (%d, %d,"
				" %d, %d)\n", p_game_rotation_vector_data->x,
				p_game_rotation_vector_data->y,
				p_game_rotation_vector_data->z,
				p_game_rotation_vector_data->w);
		} else if (sensor_type == SENSOR_GEOMAGNETIC_ROTATION_VECTOR) {
			struct geomagnetic_rotation_vector_data *p_geomagnetic_rotation_vector_data =
					(struct geomagnetic_rotation_vector_data *)buf;
			printf("get_single returns, geomagnetic rotation vector is (%d, %d,"
				" %d, %d)\n", p_geomagnetic_rotation_vector_data->x,
				p_geomagnetic_rotation_vector_data->y,
				p_geomagnetic_rotation_vector_data->z,
				p_geomagnetic_rotation_vector_data->w);
		} else if (sensor_type == SENSOR_GRAVITY) {
			struct gravity_data *p_gravity_data =
					(struct gravity_data *)buf;
			printf("get_single returns, gravity is (%d, %d, %d)\n",
				p_gravity_data->x, p_gravity_data->y,
				p_gravity_data->z);
		} else if (sensor_type == SENSOR_LINEAR_ACCEL) {
			struct linear_accel_data *p_linear_accel_data =
					(struct linear_accel_data *)buf;
			printf("get_single returns, linear acceleration is "
				"(%d, %d, %d)\n", p_linear_accel_data->x,
				p_linear_accel_data->y,
				p_linear_accel_data->z);
		} else if (sensor_type == SENSOR_ORIENTATION) {
			struct orientation_data *p_orientation_data =
					(struct orientation_data *)buf;
			printf("get_single returns, orientation is "
				"(%d, %d, %d)\n", p_orientation_data->azimuth,
				p_orientation_data->pitch,
				p_orientation_data->roll);
		} else if (sensor_type == SENSOR_MAG_HEADING) {
			struct mag_heading_data *p_mag_heading_data =
					(struct mag_heading_data *)buf;
			printf("get_single returns, magnetic north heading is "
				"%d\n", p_mag_heading_data->heading);
		} else if (sensor_type == SENSOR_BIST) {
			int i;
			struct bist_data *p_bist_data =
					(struct bist_data *)buf;
			for (i = 0; i < PHY_SENSOR_MAX_NUM; i ++) {
				printf("get_single returns, bist_result[%d] is %d \n", i, p_bist_data->result[i]);
			}
		}
	} else if (cmd_type == 1) {

		if (sensor_type != SENSOR_LPE) {
			for (i = 0; i < prop_count; ++i) {
				printf("%d, %d\n", prop_ids[i], prop_vals[i]);
				ret = psh_set_property(handle, prop_ids[i], &prop_vals[i]);
				if (ret != ERROR_NONE) {
					printf("psh_set_property fail for %dth property with code %d\n", i, ret);
					return -1;
				}
			}
		}

		if (sensor_type == SENSOR_PEDOMETER || sensor_type == SENSOR_STEPCOUNTER || sensor_type == SENSOR_STEPDETECTOR)
			ret = psh_start_streaming_with_flag(handle, data_rate, buffer_delay, 2);
		else if (sensor_type == SENSOR_PROXIMITY)
			ret = psh_start_streaming_with_flag(handle, data_rate, buffer_delay, 1);
		else
			ret = psh_start_streaming(handle, data_rate, buffer_delay);

		if (ret != ERROR_NONE) {
			printf("psh_start_streaming() failed with code %d \n",
									ret);
			return -1;
		}

		if (sensor_type == SENSOR_LPE) {
			for (i = 0; i < prop_count; ++i) {
				printf("%d, %d\n", prop_ids[i], prop_vals[i]);
				ret = psh_set_property(handle, prop_ids[i], &prop_vals[i]);
				if (ret != ERROR_NONE) {
					printf("psh_set_property fail for %dth property with code %d\n", i, ret);
					return -1;
				}
			}
		}

		fd = psh_get_fd(handle);

		if (sensor_type == SENSOR_ACCELEROMETER)
			dump_accel_data(fd);
		else if (sensor_type == SENSOR_GYRO)
			dump_gyro_data(fd);
		else if (sensor_type == SENSOR_COMP)
			dump_comp_data(fd);
		else if (sensor_type == SENSOR_BARO)
			dump_baro_data(fd);
		else if (sensor_type == SENSOR_ALS)
			dump_als_data(fd);
		else if (sensor_type == SENSOR_PROXIMITY)
			dump_proximity_data(fd);
		else if (sensor_type == SENSOR_TC)
			dump_tc_data(fd);
		else if (sensor_type == SENSOR_LPE)
			dump_lpe_data(fd);
		else if (sensor_type == SENSOR_ACTIVITY)
			dump_activity_data(fd);
		else if (sensor_type == SENSOR_GS)
			dump_gs_data(fd);
		else if (sensor_type == SENSOR_GESTURE_HMM)
			dump_gesture_hmm_data(fd);
		else if (sensor_type == SENSOR_GESTURE_FLICK)
			dump_gesture_flick_data(fd);
		else if (sensor_type == SENSOR_ROTATION_VECTOR)
			dump_rotation_vector_data(fd);
		else if (sensor_type == SENSOR_GAME_ROTATION_VECTOR)
			dump_game_rotation_vector_data(fd);
		else if (sensor_type == SENSOR_GEOMAGNETIC_ROTATION_VECTOR)
			dump_geomagnetic_rotation_vector_data(fd);
		else if (sensor_type == SENSOR_GRAVITY)
			dump_gravity_data(fd);
		else if (sensor_type == SENSOR_LINEAR_ACCEL)
			dump_linear_accel_data(fd);
		else if (sensor_type == SENSOR_ORIENTATION)
			dump_orientation_data(fd);
		else if (sensor_type == SENSOR_9DOF)
			dump_9dof_data(fd);
		else if (sensor_type == SENSOR_6DOFAG)
			dump_6dofag_data(fd);
		else if (sensor_type == SENSOR_6DOFAM)
			dump_6dofam_data(fd);
		else if (sensor_type == SENSOR_PEDOMETER)
			dump_pedometer_data(fd);
		else if (sensor_type == SENSOR_MAG_HEADING)
			dump_mag_heading_data(fd);
		else if (sensor_type == SENSOR_SHAKING)
			dump_shaking_data(fd);
		else if (sensor_type == SENSOR_MOVE_DETECT)
			dump_md_data(fd);
		else if (sensor_type == SENSOR_STAP)
			dump_stap_data(fd);
		else if (sensor_type == SENSOR_PAN_TILT_ZOOM)
			dump_ptz_data(fd);
		else if (sensor_type == SENSOR_LIFT_VERTICAL)
			dump_lv_data(fd);
		else if (sensor_type == SENSOR_DEVICE_POSITION)
			dump_device_position_data(fd);
		else if (sensor_type == SENSOR_STEPCOUNTER)
			dump_stepcounter_data(fd);
		else if (sensor_type == SENSOR_STEPDETECTOR)
			dump_stepdetector_data(fd);
		else if (sensor_type == SENSOR_SIGNIFICANT_MOTION)
			dump_significantmotion_data(fd);
		else if (sensor_type == SENSOR_LIFT_LOOK)
			dump_lift_look_data(fd);
		else if (sensor_type == SENSOR_DTWGS)
			dump_dtwgs_data(fd);
	}
//	sleep(200);

	psh_stop_streaming(handle);

	psh_close_session(handle);

	return 0;
}
