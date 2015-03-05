#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <linux/un.h>
#include <string.h>
#include <sys/select.h>
#include <poll.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <pthread.h>
#include <hardware_legacy/power.h>
#include <cutils/sockets.h>

#include "../include/socket.h"
#include "../include/utils.h"
#include "../include/message.h"
#include "../include/bist.h"
#ifdef ENABLE_CONTEXT_ARBITOR
#include <libcontextarbitor.h>
#endif

#undef LOG_TAG
#define LOG_TAG "Libsensorhub"

#define MAX_STRING_SIZE 256
#define MAX_PROP_VALUE_SIZE 58

#define MAX_FW_VERSION_STR_LEN 256
#define BYT_FW "/system/etc/firmware/psh_bk.bin"
#define FWUPDATE_SCRIPT "/system/bin/fwupdate_script.sh "

#define IA_BIT_CFG_AS_WAKEUP_SRC ((unsigned short)(0x1 << 0))

/* The Unix socket file descriptor */
static int sockfd = -1, ctlfd = -1, datafd = -1, datasizefd = -1, fwversionfd = -1;

static int enable_debug_data_rate = 0;

#define COMPASS_CALIBRATION_FILE "/data/compass.conf"
#define GYRO_CALIBRATION_FILE "/data/gyro.conf"

/* definition and struct for calibration used by psh_fw */
#define CMD_CALIBRATION_SET	(0x1)
#define CMD_CALIBRATION_GET	(0x2)
#define CMD_CALIBRATION_START	(0x3)
#define CMD_CALIBRATION_STOP	(0x4)
struct cmd_compasscal_param {
	unsigned char sub_cmd;
	struct compasscal_info info;
} __attribute__ ((packed));

struct cmd_gyrocal_param {
	unsigned char sub_cmd;
	struct gyrocal_info info;
} __attribute__ ((packed));

#define CALIB_RESULT_NONE	(0)
#define CALIB_RESULT_USER 	(1)
#define CALIB_RESULT_AUTO 	(2)
struct resp_compasscal {
	unsigned char calib_result;
	struct compasscal_info info;
} __attribute__ ((packed));

struct resp_gyrocal {
	unsigned char calib_result;
	struct gyrocal_info info;
} __attribute__ ((packed));

/* Calibration status */
#define	CALIBRATION_INIT	(1 << 1)	// Init status
#define	CALIBRATION_RESET	(1 << 2)	// Reset status
#define	CALIBRATION_IN_PROGRESS (1 << 3)	// Calibration request has been sent but get no reply
#define	CALIBRATION_DONE	(1 << 4)	// Get CALIBRATION_GET response
#define	CALIBRATION_NEED_STORE	(1 << 31)	// Additional sepcial status for parameters stored to file

typedef enum {
	INACTIVE = 0,
	ACTIVE,
	ALWAYS_ON,
	NEED_RESUME,
} state_t;

/* data structure to keep per-session state */
typedef struct session_state_t {
	state_t state;
	char flag;		// To differentiate between no_stop (0) and no_stop_no_report (1)
	int get_single;		// 1: pending; 0: not pending
	int get_calibration;
	int datafd;
	char datafd_invalid;
	int ctlfd;
	session_id_t session_id;
	union {
	int data_rate;
	unsigned char trans_id;
	};
	union {
	int buffer_delay;
	unsigned char event_id;
	};
	void *handle;
	struct session_state_t *next;
} session_state_t;

/* data structure to keep state for a particular type of sensor */
typedef struct {
	int sensor_id;
	char name[SNR_NAME_MAX_LEN + 1];
	short freq_max;
	int data_rate;
	int buffer_delay;
	int calibration_status; // only for compass_cal and gyro_cal
	session_state_t *list;
} sensor_state_t;

#define MAX_SENSOR_INDEX 25

static sensor_state_t *sensor_list = NULL;

static int current_sensor_index = 0;	// means the current empty slot of sensor_list[]

#define MERRIFIELD 0
#define BAYTRAIL 1

char platform;

/* Daemonize the sensorhubd */
static void daemonize()
{
	pid_t sid, pid = fork();

	if (pid < 0) {
		LOGE("error in fork daemon. \n");
		exit(EXIT_FAILURE);
	} else if (pid > 0)
		exit(EXIT_SUCCESS);

	/* new SID for daemon process */
	sid = setsid();
	if (sid < 0) {
		LOGE("error in setsid(). \n");
		exit(EXIT_FAILURE);
	}
#ifdef GCOV_DAEMON
	/* just for gcov test, others can ignore it */
	gcov_thread_start();
#endif

	/* close STD fds */
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	struct passwd *pw;

	pw = getpwnam("system");
	if (pw == NULL) {
		LOGE("daemonize(): getpwnam return NULL \n");
		return;
	}
	setuid(pw->pw_uid);

	log_message(DEBUG, "sensorhubd is started in daemon mode. \n");
}

static sensor_state_t* get_sensor_state_with_name(char *name)
{
        int i;

        for (i = 0; i < current_sensor_index; i ++) {
                if (strncmp(sensor_list[i].name, name, SNR_NAME_MAX_LEN) == 0) {
                        return &sensor_list[i];
                }
        }

        return NULL;
}

static sensor_state_t* get_sensor_state_with_id(unsigned char sensor_id)
{
	int i;

	for (i = 0; i < current_sensor_index; i ++) {
		if (sensor_list[i].sensor_id == sensor_id) {
			return &sensor_list[i];
		}
	}

	return NULL;
}

static session_state_t* get_session_state_with_session_id(
					session_id_t session_id)
{
	int i;

	log_message(DEBUG, "get_session_state_with_session_id(): "
				"session_id %d\n", session_id);

	for (i = 0; i < current_sensor_index; i ++) {
		session_state_t *p_session_state = sensor_list[i].list;

		while (p_session_state != NULL) {
			if (p_session_state->session_id == session_id)
				return p_session_state;

			p_session_state = p_session_state->next;
		}
        }

	log_message(WARNING, "get_session_state_with_session_id(): "
				"NULL is returned\n");

	return NULL;
}

#define MAX_UNSIGNED_INT 0xffffffff

static session_id_t allocate_session_id()
{
        /* rewind session ID */
	static session_id_t session_id = 0;
	static int rewind = 0;

	session_id++;

	if (rewind) {
		while (get_session_state_with_session_id(session_id) != NULL)
			session_id++;
	}

	if (session_id == MAX_UNSIGNED_INT)
		rewind = 1;

	return session_id;
}

/* return 0 on success; -1 on fail */
static int get_sensor_state_session_state_with_fd(int ctlfd,
					sensor_state_t **pp_sensor_state,
					session_state_t **pp_session_state)
{
	int i;

	 for (i = 0; i < current_sensor_index; i ++) {
		session_state_t *p_session_state = sensor_list[i].list;

		while (p_session_state != NULL) {
			if (p_session_state->ctlfd == ctlfd) {
				*pp_sensor_state = &sensor_list[i];
				*pp_session_state = p_session_state;

				return 0;
			}

			p_session_state = p_session_state->next;
		}

	}

	return -1;
}

/* flag: 1 means start_streaming, 0 means stop_streaming */
static int data_rate_arbiter(sensor_state_t *p_sensor_state,
				int data_rate,
				session_state_t *p_session_state,
				char flag)
{
	session_state_t *p_session_state_tmp;
	int max_data_rate = 0;

	if (p_sensor_state == NULL) {
		log_message(CRITICAL, "%s: p_sensor_state is NULL \n", __func__);
		return 0;
	}

	if (flag == 1) {
		if (p_sensor_state->data_rate == 0)
			return data_rate;
		else if (data_rate > p_sensor_state->data_rate)
			return data_rate;
		else
			max_data_rate = data_rate;

	}

	/* flag == 0 */
	p_session_state_tmp = p_sensor_state->list;

	while (p_session_state_tmp != NULL) {
		if ((p_session_state_tmp != p_session_state)
			&& ((p_session_state_tmp->state == ACTIVE)
			|| (p_session_state_tmp->state == ALWAYS_ON))) {
			if (max_data_rate < p_session_state_tmp->data_rate) {
				max_data_rate = p_session_state_tmp->data_rate;
			}
		}

		p_session_state_tmp = p_session_state_tmp->next;
	}

	return max_data_rate;
}

/* flag: 1 means add a new buffer_delay, 0 means remove a buffer_delay */
static int buffer_delay_arbiter(sensor_state_t *p_sensor_state,
				int buffer_delay,
				session_state_t *p_session_state,
				char flag)
{
	if (p_sensor_state == NULL) {
		log_message(CRITICAL, "%s: p_sensor_state is NULL \n", __func__);
		return 0;
	}

	if (flag == 1) {
		int r;
		if (buffer_delay == 0)
			return 0;

		/* a new buffer_delay and not the first one */
		if (p_sensor_state->buffer_delay == 0) {
			session_state_t *tmp = p_sensor_state->list;
			while (tmp != NULL) {
				if ((tmp != p_session_state)
					&& ((tmp->state == ACTIVE)
					|| (tmp->state == ALWAYS_ON)))
					return 0;
				tmp = tmp->next;
			}
		}
		r = max_common_factor(p_sensor_state->buffer_delay,
							buffer_delay);
		return r;
	}
	/* flag == 0 */
	int data1 = 0, data2;
	session_state_t *p_session_state_tmp = p_sensor_state->list;

	if ((p_sensor_state->buffer_delay == 0) && (buffer_delay != 0))
		return 0;

	while (p_session_state_tmp != NULL) {
		if ((p_session_state_tmp != p_session_state)
			&& ((p_session_state_tmp->state == ACTIVE)
			|| (p_session_state_tmp->state == ALWAYS_ON))) {
			if (p_session_state_tmp->buffer_delay == 0)
				return 0;

			data2 = p_session_state_tmp->buffer_delay;
			data1 = max_common_factor(data1, data2);
		}

		p_session_state_tmp = p_session_state_tmp->next;
	}

	return data1;
}

/* return as long as anyone is ALWAYS_ON */
static unsigned short bit_cfg_arbiter(sensor_state_t *p_sensor_state,
                                unsigned short bit_cfg,
                                session_state_t *p_session_state)
{
	session_state_t *p_session_state_tmp;

	if (bit_cfg != 0)
		return bit_cfg;

	p_session_state_tmp = p_sensor_state->list;
	while (p_session_state_tmp != NULL) {
		if (p_session_state_tmp != p_session_state) {
			if (p_session_state_tmp->state == ALWAYS_ON) {
				return IA_BIT_CFG_AS_WAKEUP_SRC;
			}
		}

		p_session_state_tmp = p_session_state_tmp->next;
	}

	return 0;
}

enum resp_type {
	RESP_CMD_ACK,
	RESP_GET_TIME,
	RESP_GET_SINGLE,
	RESP_STREAMING,
	RESP_DEBUG_MSG,
	RESP_DEBUG_GET_MASK = 5,
	RESP_GYRO_CAL_RESULT,
	RESP_BIST_RESULT,
	RESP_ADD_EVENT,
	RESP_CLEAR_EVENT,
	RESP_EVENT = 10,
	RESP_GET_STATUS,
	RESP_COMP_CAL_RESULT,
};

static int cmd_type_to_cmd_id[CMD_MAX] = {2, 3, 4, 5, 6, 9, 9, 12};

static int send_control_cmd(int tran_id, int cmd_id, int sensor_id, unsigned short data_rate, unsigned short buffer_delay, unsigned short bit_cfg);

#if 0
static void error_handling(int error_no)
{

	/* 0 issue RESET and wait for psh fw to be ready */

	/* 1 re-issue start_streaming */
	int i, ret;

restart:
	for (i = SENSOR_ACCELEROMETER; i < SENSOR_MAX; i++) {
		/* no start_streaming with below ones */
		if ((i == SENSOR_CALIBRATION_COMP)
			|| (i == SENSOR_CALIBRATION_GYRO)
			|| (i == SENSOR_EVENT))
			continue;

		/* no pending active request */
		if (sensor_list[i].data_rate == 0)
			continue;

		ret = send_control_cmd(0, cmd_type_to_cmd_id[CMD_START_STREAMING], sensor_type_to_sensor_id[i],
				sensor_list[i].data_rate, sensor_list[i].buffer_delay, 0);
		if (ret < 0)
			goto restart;
	}

	/* 2 re-issue get_single */

	/* 3 re-issue set_property */

	/* 4 re-issue calibration */
}
#endif

/* 0 on success; -1 on fail */
static int send_control_cmd(int tran_id, int cmd_id, int sensor_id,
				unsigned short data_rate, unsigned short buffer_delay, unsigned short bit_cfg)
{
	char cmd_string[MAX_STRING_SIZE];
	int size;
	ssize_t ret;
	unsigned char *data_rate_byte = (unsigned char *)&data_rate;
	unsigned char *buffer_delay_byte = (unsigned char *)&buffer_delay;
	unsigned char *bit_cfg_byte = (unsigned char *)&bit_cfg;

	size = snprintf(cmd_string, MAX_STRING_SIZE, "%d %d %d %d %d %d %d %d %d",
				tran_id, cmd_id, sensor_id, data_rate_byte[0],
				data_rate_byte[1], buffer_delay_byte[0],
				buffer_delay_byte[1], bit_cfg_byte[0], bit_cfg_byte[1]);
	if (size <= 0)
		return -1;

	log_message(DEBUG, "cmd to sysfs is: %s\n", cmd_string);
	/* TODO: error handling if (size <= 0 || size > MAX_STRING_SIZE) */
	ret = write(ctlfd, cmd_string, size);
	log_message(DEBUG, "cmd return value is %d\n", ret);
	if (ret < 0)
		return -1;

	return 0;
}


/*
 * 60 bytes can be transfered once maximumly, including property value length and property type.
 * So actually 58 bytes of property value can be sent once.
 *
 * send_set_property() will split property value buffer larger than 58 bytes into segments and
 * send them one by one.
 *
 * User should handle start/end flags, by themselves in their *value buffer.
 * User should handle segments concatenation by themselves in virtual sensor.
 */
static int send_set_property(sensor_state_t *p_sensor_state, property_type prop_type, int len, unsigned char *value)
{
	char cmd_string[MAX_STRING_SIZE];
	int size, ret, i, j, set_len, left_len;
	unsigned char *p = value;
	unsigned char prop_type_byte = (unsigned char )prop_type;

	if (p_sensor_state == NULL) {
		log_message(CRITICAL, "%s: p_sensor_state is NULL \n", __func__);
		return -1;
	}

	log_message(CRITICAL, "prop type: %d len: %d value: %d %d %d %d\n", prop_type_byte, len,
			value[0], value[1], value[2], value[3]);

	left_len = len;
	for (j = 0; j < (len + MAX_PROP_VALUE_SIZE - 1) / MAX_PROP_VALUE_SIZE; j++) {
		set_len = left_len > MAX_PROP_VALUE_SIZE ? MAX_PROP_VALUE_SIZE : left_len;
		size = snprintf(cmd_string, MAX_STRING_SIZE, "%d %d %d %d %d", 0,
				cmd_type_to_cmd_id[CMD_SET_PROPERTY], p_sensor_state->sensor_id, set_len + 1,
				prop_type_byte);
		for (i = 0; i < set_len; i++) {
			size += snprintf(cmd_string + size, MAX_STRING_SIZE - size, " %d", p[i]);
		}

		log_message(DEBUG, "cmd to sysfs is: %s\n", cmd_string);
		ret = write(ctlfd, cmd_string, size);
		log_message(DEBUG, "cmd return value is %d\n", ret);

                while (ret < 0)
			return -1;

		p += set_len;
		left_len -= set_len;
	}

	return 0;
}

/* flag: 2 means no_stop_no_report; 1 means no_stop when screen off; 0 means stop when screen off */
static void start_streaming(sensor_state_t *p_sensor_state,
				session_state_t *p_session_state,
				int data_rate, int buffer_delay, int flag)
{
	int data_rate_arbitered, buffer_delay_arbitered;
	unsigned short bit_cfg = 0;

	if (p_sensor_state == NULL) {
		log_message(CRITICAL, "%s: p_sensor_state is NULL \n", __func__);
		return;
	}

//	log_message(CRITICAL, "requested data rate is %d \n", data_rate);

	if (data_rate != -1)
		data_rate_arbitered = data_rate_arbiter(p_sensor_state, data_rate,
						p_session_state, 1);
	else
		data_rate_arbitered = -1;

	buffer_delay_arbitered = buffer_delay_arbiter(p_sensor_state, buffer_delay,
						p_session_state, 1);

	log_message(CRITICAL, "requested data rate is %d, arbitered is %d \n", data_rate, data_rate_arbitered);

	log_message(DEBUG, "CMD_START_STREAMING, data_rate_arbitered "
				"is %d, buffer_delay_arbitered is %d \n",
				data_rate_arbitered, buffer_delay_arbitered);

	p_sensor_state->data_rate = data_rate_arbitered;
	p_sensor_state->buffer_delay = buffer_delay_arbitered;

	/* send CMD_START_STREAMING to psh_fw no matter data rate changes or not,
	   this is requested by Dong for terminal context sensor on 2012/07/17 */
//	if ((sensor_list[sensor_type].data_rate != data_rate_arbitered)
//	||(sensor_list[sensor_type].buffer_delay != buffer_delay_arbitered)) {
		/* send cmd to sysfs control node to change data_rate.
		   NO NEED to send CMD_STOP_STREAMING
		   before CMD_START_STREAMING according to Dong's enhancement
		   on 2011/12/29 */
//		send_control_cmd(0, cmd_type_to_cmd_id[CMD_STOP_STREAMING],
//				sensor_type_to_sensor_id[sensor_type], 0, 0);

	if ((flag == 1) || (flag == 2))
		bit_cfg = IA_BIT_CFG_AS_WAKEUP_SRC;
	else
		bit_cfg = bit_cfg_arbiter(p_sensor_state, 0, p_session_state);

	send_control_cmd(0, cmd_type_to_cmd_id[CMD_START_STREAMING],
			p_sensor_state->sensor_id,
			data_rate_arbitered, buffer_delay_arbitered, bit_cfg);
//	}

	if (data_rate != 0) {
		p_session_state->data_rate = data_rate;
		p_session_state->buffer_delay = buffer_delay;
		if (flag == 0)
			p_session_state->state = ACTIVE;
		else if (flag == 1)
			p_session_state->state = ALWAYS_ON;
		else if (flag == 2) {
			p_session_state->state = ALWAYS_ON;
			p_session_state->flag = 1;
		}
	}
}

static void stop_streaming(sensor_state_t *p_sensor_state,
				session_state_t *p_session_state)
{
	int data_rate_arbitered, buffer_delay_arbitered;
	unsigned short bit_cfg_arbitered;

	if (p_sensor_state == NULL) {
		log_message(CRITICAL, "%s: p_sensor_state is NULL \n", __func__);
		return;
	}

	if (p_session_state->state == INACTIVE)
		return;

	if (p_session_state->state == NEED_RESUME) {
		p_session_state->state = INACTIVE;
		return;
	}

	data_rate_arbitered = data_rate_arbiter(p_sensor_state,
						p_session_state->data_rate,
						p_session_state, 0);
	buffer_delay_arbitered = buffer_delay_arbiter(p_sensor_state,
						p_session_state->buffer_delay,
						p_session_state, 0);

	p_session_state->state = INACTIVE;

	if ((p_sensor_state->data_rate == data_rate_arbitered)
	&& (p_sensor_state->buffer_delay == buffer_delay_arbitered))
		return;

	p_sensor_state->data_rate = data_rate_arbitered;
	p_sensor_state->buffer_delay = buffer_delay_arbitered;

	if (data_rate_arbitered != 0) {
		/* send CMD_START_STREAMING to sysfs control node to
		   re-config the data rate */
		bit_cfg_arbitered = bit_cfg_arbiter(p_sensor_state, 0, p_session_state);
		send_control_cmd(0, cmd_type_to_cmd_id[CMD_START_STREAMING],
				p_sensor_state->sensor_id,
				data_rate_arbitered, buffer_delay_arbitered, bit_cfg_arbitered);
	} else {
		/* send CMD_STOP_STREAMING cmd to sysfs control node */
		send_control_cmd(0, cmd_type_to_cmd_id[CMD_STOP_STREAMING],
			p_sensor_state->sensor_id, 0, 0, 0);
	}

//	sensor_list[sensor_type].count = 0;

	log_message(DEBUG, "CMD_STOP_STREAMING, data_rate_arbitered is %d, "
			"buffer_delay_arbitered is %d \n", data_rate_arbitered,
			buffer_delay_arbitered);
}

static int get_fw_version(char *fw_version_str)
{
	FILE *fp;
	int filelen;
	char *ver_p = NULL;
	int n = 0;
	int ret = 0;

	fp = fopen(BYT_FW, "r");
	if (fp == NULL) {
		log_message(CRITICAL,"Failed to open firmware file !!!\r\n");
		return -1;
	}

	fseek(fp, 0, SEEK_END);
	filelen = ftell(fp);

	char *buf = malloc(filelen + 1);
	if (buf == NULL) {
		fclose(fp);
		return -1;
	}

	fseek(fp, 0, SEEK_SET);
	fread(buf, 1, filelen, fp);
	buf[filelen]='\0';

	while(ver_p == NULL) {
		ver_p = strstr(buf+n, "VERSION:");
		if (ver_p == NULL) {
			while ((n<(filelen-8))&&(buf[n]!='\0'))
				n++;

			while ((n<(filelen-8))&&(buf[n]=='\0'))
				n++;
		}
		if (n>=(filelen-8)) break;
	}

	if (ver_p!=NULL) {
		/* filter string"VERSION:" */
		strncpy(fw_version_str, ver_p+8, MAX_FW_VERSION_STR_LEN-1);
		ret = 0;
	} else {
		ret = -1;
	}

	fclose(fp);
	free(buf);

	return ret;
}

static int fw_verion_compare()
{
	char version_str_running[MAX_FW_VERSION_STR_LEN];
	char version_str[MAX_FW_VERSION_STR_LEN];
	int length = 0;

	if (get_fw_version(version_str)) {
		log_message(CRITICAL, "can not get the bin fw version!!!\n");
		return -2;
	}

	version_str[MAX_FW_VERSION_STR_LEN-1] = '\0';

	length = strlen(version_str);
	if (length<8) {
		log_message(CRITICAL, "fw version string is error!!!\n");
		return -1;
	}

	lseek(fwversionfd, 0, SEEK_SET);
	if (read(fwversionfd, version_str_running, MAX_FW_VERSION_STR_LEN) <= 0) {
		log_message(CRITICAL, "can not get the running fw version!!!\n");
		return -1;
	}

	version_str_running[length] = '\0';

	if (strcmp(version_str, version_str_running)) {
		log_message(CRITICAL, "psh firmware versions are not same!!!\n");
		return -1;
	}

	return 0;
}

static void get_single(sensor_state_t *p_sensor_state,
				session_state_t *p_session_state)
{
	log_message(DEBUG, "get_single is called with sensor_type %s \n",
							p_sensor_state->name);

	if (p_sensor_state == NULL) {
		log_message(CRITICAL, "%s: p_sensor_state is NULL \n", __func__);
		return;
	}

	log_message(DEBUG, "get_single is called with %s \n", p_sensor_state->name);

	/* send CMD_GET_SINGLE to sysfs control node */
	p_session_state->get_single = 1;

	if (strncmp(p_sensor_state->name, "BIST", SNR_NAME_MAX_LEN) == 0) {
		send_control_cmd(0, 7, 0, 0, 0, 0);
	} else {
		send_control_cmd(0, cmd_type_to_cmd_id[CMD_GET_SINGLE],
			p_sensor_state->sensor_id, 0, 0, 0);
	}
}

static void get_calibration(sensor_state_t *p_sensor_state,
				session_state_t *p_session_state)
{
	char cmdstring[MAX_STRING_SIZE];
	int ret, len;

	if (p_sensor_state == NULL) {
		log_message(DEBUG, "%s: p_sensor_state is NULL \n", __func__);
		return;
	}

	if (strncmp(p_sensor_state->name, "COMPC", SNR_NAME_MAX_LEN) != 0 &&
		strncmp(p_sensor_state->name, "GYROC", SNR_NAME_MAX_LEN) != 0) {
		log_message(DEBUG, "Get calibration with confused parameter,"
					"sensor type is not for calibration, is %s\n",
					p_sensor_state->name);
		return;
	}

	len = snprintf (cmdstring, MAX_STRING_SIZE, "%d %d %d %d",
			0,			// tran_id
			cmd_type_to_cmd_id[CMD_GET_CALIBRATION],	// cmd_id
			p_sensor_state->sensor_id,	// sensor_id
			SUBCMD_CALIBRATION_GET);		// sub_cmd

	if (len <= 0) {
		log_message(CRITICAL, "[%s] failed to compose cmd_string \n", __func__);
		return;
	}

	log_message(DEBUG, "Send to control node :%s\n", cmdstring);
	ret = write(ctlfd, cmdstring, len);
	if (ret != len)
		log_message(DEBUG, "[%s]Error in sending cmd:%d\n", __func__, ret);

	if (p_session_state)
		p_session_state->get_calibration = 1;
}

static void set_calibration(sensor_state_t *p_sensor_state,
				struct cmd_calibration_param *param)
{
	char cmdstring[MAX_STRING_SIZE];
	int ret, len = 0;

	if (p_sensor_state == NULL) {
		log_message(DEBUG, "%s: p_sensor_state is NULL \n", __func__);
		return;
	}

	/* Too bad we cannot use send_control_cmd, send cmd by hand. */
	// echo trans cmd sensor [sensor subcmd calibration [struct parameter info]]

	log_message(DEBUG, "Begin to form command string.\n");
	len = snprintf (cmdstring, MAX_STRING_SIZE, "%d %d %d %d ",
			0,			// tran_id
			cmd_type_to_cmd_id[CMD_SET_CALIBRATION],	// cmd_id
			p_sensor_state->sensor_id,	// sensor_id
			param->sub_cmd);		// sub_cmd

	if (len <= 0) {
		log_message(CRITICAL, "[%s] failed to compose cmd_string \n", __func__);
		return;
	}

	if (param->sub_cmd == SUBCMD_CALIBRATION_SET) {
		// 30 parameters, horrible Orz...
		unsigned char* p;
		log_message(DEBUG, "Set calibration, sensor_type %s\n", p_sensor_state->name);

		if (param->sub_cmd != SUBCMD_CALIBRATION_SET) {
			log_message(DEBUG, "Set calibration with confused parameter,"
						"sub_cmd is %d\n",param->sub_cmd);

			return;
		}

		if (param->calibrated != SUBCMD_CALIBRATION_TRUE) {
			log_message(DEBUG, "Set calibration with confused parameter,"
						"calibrated is %u\n",param->calibrated);

			return;
		}

		if (strncmp(p_sensor_state->name, "COMPC", SNR_NAME_MAX_LEN) == 0) {
			/* For compass_cal, the parameter is:
			 * offset[3], w[3][3], bfield
			 */
			int i, j;

			for (i = 0; i < 3; i++) {
				p = (unsigned char*)&param->cal_param.compass.offset[i];
				len += snprintf (cmdstring + len, MAX_STRING_SIZE - len, "%d %d %d %d ",
					p[0], p[1], p[2], p[3]); // offset[i]
			}

			if (len <= 0) {
				log_message(CRITICAL, "[%s] failed to compose cmd_string \n", __func__);
				return;
			}

			for (i = 0; i < 3; i++)
				for (j = 0; j < 3; j++) {
				p = (unsigned char*)&param->cal_param.compass.w[i][j];
				len += snprintf (cmdstring + len, MAX_STRING_SIZE - len, "%d %d %d %d ",
					p[0], p[1], p[2], p[3]); // w[i][j]
			}

			if (len <= 0) {
				log_message(CRITICAL, "[%s] failed to compose cmd_string \n", __func__);
				return;
			}

			p = (unsigned char*)&param->cal_param.compass.bfield;
			len += snprintf (cmdstring + len, MAX_STRING_SIZE - len, "%d %d %d %d",
				p[0], p[1], p[2], p[3]); // bfield

			if (len <= 0) {
				log_message(CRITICAL, "[%s] failed to compose cmd_string \n", __func__);
				return;
			}
		} else if (strncmp(p_sensor_state->name, "GYROC", SNR_NAME_MAX_LEN) == 0) {
			/* For gyro_cal, the parameter is:
			 * x, y, z
			 */
			p = (unsigned char*)&param->cal_param.gyro.x;
			len += snprintf (cmdstring + len, MAX_STRING_SIZE - len, "%d %d ",
				p[0], p[1]); // x

			if (len <= 0) {
				log_message(CRITICAL, "[%s] failed to compose cmd_string \n", __func__);
				return;
			}

			p = (unsigned char*)&param->cal_param.gyro.y;
			len += snprintf (cmdstring + len, MAX_STRING_SIZE - len, "%d %d ",
				p[0], p[1]); // y

			if (len <= 0) {
				log_message(CRITICAL, "[%s] failed to compose cmd_string \n", __func__);
				return;
			}

			p = (unsigned char*)&param->cal_param.gyro.z;
			len += snprintf (cmdstring + len, MAX_STRING_SIZE - len, "%d %d",
				p[0], p[1]); // z

			if (len <= 0) {
				log_message(CRITICAL, "[%s] failed to compose cmd_string \n", __func__);
				return;
			}

		}
	} else if (param->sub_cmd == SUBCMD_CALIBRATION_START) {
		log_message(DEBUG, "Calibration START, sensor_type %s\n", p_sensor_state->name);

	} else if (param->sub_cmd == SUBCMD_CALIBRATION_STOP) {
		log_message(DEBUG, "Calibration STOP, sensor_type %s\n", p_sensor_state->name);

	}
	if (len <= 0) {
		log_message(CRITICAL, "[%s] failed to compose cmd_string \n", __func__);
		return;
	}

	log_message(DEBUG, "Send to control node :%s\n", cmdstring);
	ret = write(ctlfd, cmdstring, len);
	if (ret != len)
		log_message(DEBUG, "[%s]Error in sending cmd:%d\n", __func__, ret);
}

static void load_calibration_from_file(sensor_state_t *p_sensor_state,
					struct cmd_calibration_param *param)
{
	FILE *conf;
	const char *file_name;

	if (p_sensor_state == NULL) {
		log_message(DEBUG, "%s: p_sensor_state is NULL \n", __func__);
		return;
	}

	memset(param, 0, sizeof(struct cmd_calibration_param));

	if (strncmp(p_sensor_state->name, "COMPC", SNR_NAME_MAX_LEN) == 0)
		file_name = COMPASS_CALIBRATION_FILE;
	else if (strncmp(p_sensor_state->name, "GYROC", SNR_NAME_MAX_LEN) == 0)
		file_name = GYRO_CALIBRATION_FILE;
	else
		return;

	conf = fopen(file_name, "r");

	if (!conf) {
		/* calibration file doesn't exist, creat a new file */
		log_message(DEBUG, "Open file %s failed, create a new one!\n", file_name);
		conf = fopen(file_name, "w");
		if (conf == NULL) {
			LOGE("load_calibration_from_file(): failed to open file \n");
			return;
		}
		fwrite(param, sizeof(struct cmd_calibration_param), 1, conf);
		fclose(conf);

		return;
	}

	fread(param, sizeof(struct cmd_calibration_param), 1, conf);
	fclose(conf);
}

static void store_calibration_to_file(sensor_state_t *p_sensor_state,
					struct cmd_calibration_param *param)
{
	FILE *conf;
	const char *file_name;

	if (p_sensor_state == NULL) {
		log_message(DEBUG, "%s: p_sensor_state is NULL \n", __func__);
		return;
	}

	if (strncmp(p_sensor_state->name, "COMPC", SNR_NAME_MAX_LEN) == 0)
		file_name = COMPASS_CALIBRATION_FILE;
	else if (strncmp(p_sensor_state->name, "GYROC", SNR_NAME_MAX_LEN) == 0)
		file_name = GYRO_CALIBRATION_FILE;
	else
		return;

	conf = fopen(file_name, "w");

	if (!conf) {
		/* calibration file doesn't exist, creat a new file */
		log_message(DEBUG, "Can not Open file %s for writing\n", file_name);
		return;
	}

	fwrite(param, sizeof(struct cmd_calibration_param), 1, conf);
	fclose(conf);
}

static inline int check_calibration_status(sensor_state_t *p_sensor_state, int status)
{
	if (p_sensor_state == NULL)
		return 0;

	if (p_sensor_state->calibration_status & status)
		return 1;
	else
		return 0;
}

static int set_calibration_status(sensor_state_t *p_sensor_state, int status,
					struct cmd_calibration_param *param)
{
	if (p_sensor_state == NULL) {
		log_message(DEBUG, "%s: p_sensor_state is NULL \n", __func__);
		return -1;
	}

	if (strncmp(p_sensor_state->name, "COMPC", SNR_NAME_MAX_LEN) != 0 &&
		strncmp(p_sensor_state->name, "GYROC", SNR_NAME_MAX_LEN) != 0)
		return -1;

	if (status & CALIBRATION_NEED_STORE)
		p_sensor_state->calibration_status |=
						CALIBRATION_NEED_STORE;

	if (status & CALIBRATION_INIT) {
		struct cmd_calibration_param param_temp;

		/* If current status is calibrated, calibration init isn't needed*/
		if (p_sensor_state->calibration_status &
				CALIBRATION_DONE)
			return p_sensor_state->calibration_status;

		load_calibration_from_file(p_sensor_state, &param_temp);

		if (param_temp.calibrated == SUBCMD_CALIBRATION_TRUE) {
			/* Set calibration parameter to psh_fw */
			param_temp.sub_cmd = SUBCMD_CALIBRATION_SET;
			set_calibration(p_sensor_state, &param_temp);

			/* Clear 0~30bit, 31bit is for CALIBRATION_NEED_STORE,
			 * 31bit can coexist with other bits */
			p_sensor_state->calibration_status &=
							CALIBRATION_NEED_STORE;
			/* Then set the status bit */
			p_sensor_state->calibration_status |=
							CALIBRATION_DONE;
		} else {
			memset(&param_temp, 0, sizeof(struct cmd_calibration_param));
			/* clear the parameter file */
			store_calibration_to_file(p_sensor_state, &param_temp);
			/* If no calibration parameter, just set status */
			p_sensor_state->calibration_status &=
							CALIBRATION_NEED_STORE;
			p_sensor_state->calibration_status |=
							CALIBRATION_IN_PROGRESS;
		}
	} else if (status & CALIBRATION_RESET) {
		/* Start the calibration */
		if (param && param->sub_cmd == SUBCMD_CALIBRATION_START) {
			set_calibration(p_sensor_state, param);

			memset(param, 0, sizeof(struct cmd_calibration_param));
			/* clear the parameter file */
			store_calibration_to_file(p_sensor_state, param);

			p_sensor_state->calibration_status &=
							CALIBRATION_NEED_STORE;
			p_sensor_state->calibration_status |=
							CALIBRATION_IN_PROGRESS;
		}
	} else if (status & CALIBRATION_IN_PROGRESS) {
		/* CALIBRATION_IN_PROGRESS just a self-changed middle status,
		 * This status can not be set
		 * In this status, can only send SUBCMD_CALIBRATION_STOP
		 */
		if (param && param->sub_cmd == SUBCMD_CALIBRATION_STOP)
			set_calibration(p_sensor_state, param);
	} else if (status & CALIBRATION_DONE) {
		if (param && param->calibrated == SUBCMD_CALIBRATION_TRUE && param->sub_cmd == SUBCMD_CALIBRATION_SET) {
			/* Set calibration parameter to psh_fw */
			set_calibration(p_sensor_state, param);

			p_sensor_state->calibration_status &=
							CALIBRATION_NEED_STORE;
			p_sensor_state->calibration_status |=
							CALIBRATION_DONE;

			/* There is a chance to check if needs store param to file
			 * If so, then do it, and clear the CALIBRATION_NEED_STORE bit
			 */
			if (p_sensor_state->calibration_status &
				CALIBRATION_NEED_STORE) {
				store_calibration_to_file(p_sensor_state, param);

				p_sensor_state->calibration_status &=
							(~CALIBRATION_NEED_STORE);
			}
		}
	}

	return p_sensor_state->calibration_status;
}

static session_state_t* get_session_state_with_trans_id(
					unsigned char trans_id)
{
	sensor_state_t *p_sensor_state = get_sensor_state_with_name("EVENT");
	session_state_t *p_session_state;

	if (p_sensor_state == NULL) {
		log_message(DEBUG, "%s: p_sensor_state is NULL \n", __func__);
		return NULL;
	}

	p_session_state = p_sensor_state->list;

	while (p_session_state != NULL) {
		if (p_session_state->trans_id == trans_id)
			return p_session_state;

		p_session_state = p_session_state->next;
	}

	return NULL;
}

#define MAX_UNSIGNED_CHAR 0xff
/* trans_id starts from 1 */
static unsigned char allocate_trans_id()
{
	/* rewind transaction ID */
	static unsigned char trans_id = 0;
	static int rewind = 0;

	trans_id++;

	if (rewind) {
		while ((get_session_state_with_trans_id(trans_id) != NULL) || (trans_id == 0))
		trans_id++;
	}

	if (trans_id == MAX_UNSIGNED_CHAR)
		rewind = 1;

	return trans_id;
}

static void handle_add_event(session_state_t *p_session_state, cmd_event* p_cmd)
{
	unsigned char trans_id;
	char cmd_string[MAX_STRING_SIZE];
	int len, ret;

	log_message(DEBUG, "[%s] Ready to handle command %d\n", __func__, p_cmd->cmd);

	/* allocate a tran_id, send cmd to PSH */
	struct cmd_event_param *evt_param = (struct cmd_event_param *)p_cmd->buf;
	int i, num = evt_param->num;

	trans_id = allocate_trans_id();

	p_session_state->trans_id = trans_id;

	/* struct ia_cmd { u8 tran_id; u8 cmd_id; u8 sensor_id; char param[] }; */
	len = snprintf(cmd_string, MAX_STRING_SIZE, "%d %d %d ", trans_id, cmd_type_to_cmd_id[CMD_ADD_EVENT], 0);
	if (len <= 0) {
		log_message(CRITICAL, "[%s] failed to compose cmd_string \n", __func__);
		return;
	}
	/* struct cmd_event_param {u8 num; u8 op; struct sub_event evts[] }; */
	len += snprintf(cmd_string + len, MAX_STRING_SIZE - len, "%d %d ", num, evt_param->relation);
	if (len <= 0) {
		log_message(CRITICAL, "[%s] failed to compose cmd_string \n", __func__);
		return;
	}
	/* struct sub_event {u8 sensor_id; u8 chan_id; opt_id; u32 param1; u32 param2; }; */
	for (i = 0; i < num; i++) {
		struct sub_event *sub_event = &(evt_param->evts[i]);
		unsigned char* p;
		sensor_state_t *p_sensor_state;

		p_sensor_state = get_sensor_state_with_name(sensor_type_to_name_str[sub_event->sensor_id].name);
		if (p_sensor_state == NULL) {
			log_message(CRITICAL, "[%s] failed to compose cmd_string \n", __func__);
			return;
		}

		len += snprintf(cmd_string + len, MAX_STRING_SIZE - len, "%d %d %d ", p_sensor_state->sensor_id, sub_event->chan_id, sub_event->opt_id);
		if (len <= 0) {
			log_message(CRITICAL, "[%s] failed to compose cmd_string \n", __func__);
			return;
		}
		p = (unsigned char*)&sub_event->param1;
		len += snprintf(cmd_string + len, MAX_STRING_SIZE - len, "%d %d %d %d ", p[0], p[1], p[2], p[3]);
		if (len <= 0) {
			log_message(CRITICAL, "[%s] failed to compose cmd_string \n", __func__);
			return;
		}
		p = (unsigned char*)&sub_event->param2;
		len += snprintf(cmd_string + len, MAX_STRING_SIZE - len, "%d %d %d %d ", p[0], p[1], p[2], p[3]);
		if (len <= 0) {
			log_message(CRITICAL, "[%s] failed to compose cmd_string \n", __func__);
			return;
		}
	}

	if (len <= 0) {
		log_message(CRITICAL, "[%s] failed to compose cmd_string \n", __func__);
		return;
	}

	ret = write(ctlfd, cmd_string, len);
	if (ret != len)
		log_message(DEBUG, "[%s]Error in sending cmd:%d\n", __func__, ret);

	return;
}

static void handle_clear_event(session_state_t *p_session_state)
{
	char cmd_string[MAX_STRING_SIZE];
	int len, ret;

	if (p_session_state->event_id == 0)
		return;

	/* struct ia_cmd { u8 tran_id; u8 cmd_id; u8 sensor_id; char param[] }; */
	len = snprintf(cmd_string, MAX_STRING_SIZE, "%d %d %d ", 0, cmd_type_to_cmd_id[CMD_CLEAR_EVENT], 0);
	/* struct clear_evt_param {u8 evt_id}; */
	len += snprintf(cmd_string + len, MAX_STRING_SIZE - len, "%d ", p_session_state->event_id);

	p_session_state->event_id = 0;
	p_session_state->trans_id = 0;

	if (len <= 0) {
		log_message(CRITICAL, "[%s] failed to compose cmd_string \n", __func__);
		return;
	}

	ret = write(ctlfd, cmd_string, len);
	if (ret != len)
		log_message(DEBUG, "[%s]Error in sending cmd:%d\n", __func__, ret);

	return;
}

static int recalculate_data_rate(sensor_state_t *p_sensor_state, int data_rate)
{
	short freq_max;

	if (p_sensor_state == NULL) {
		log_message(DEBUG, "%s: p_sensor_state is NULL \n", __func__);
		return -1;
	}

	freq_max = p_sensor_state->freq_max;

//	if (freq_max == -1)
//		return -1;

	return data_rate;
}

/* 1 arbiter
   2 send cmd to control node under sysfs.
     cmd format is <TRANID><CMDID><SENSORID><RATE><BUFFER DELAY>
     CMDID refer to psh_fw/include/cmd_engine.h;
     SENSORID refer to psh_fw/sensors/include/sensor_def.h */
static ret_t handle_cmd(int fd, cmd_event* p_cmd, int parameter, int parameter1,
						int parameter2,	int *reply_now)
{
	session_state_t *p_session_state;
	sensor_state_t *p_sensor_state;
	int data_rate_calculated, size;
	int ret = get_sensor_state_session_state_with_fd(fd, &p_sensor_state,
							&p_session_state);
	cmd_t cmd = p_cmd->cmd;

	*reply_now = 1;

	if (ret != 0)
		return ERR_SESSION_NOT_EXIST;

	log_message(DEBUG, "[%s] Ready to handle command %d\n", __func__, cmd);

	if (cmd == CMD_START_STREAMING) {
		data_rate_calculated = recalculate_data_rate(p_sensor_state, parameter);
		start_streaming(p_sensor_state, p_session_state,
					data_rate_calculated, parameter1, parameter2);
	} else if (cmd == CMD_STOP_STREAMING) {
		stop_streaming(p_sensor_state, p_session_state);
	} else if (cmd == CMD_GET_SINGLE) {
		get_single(p_sensor_state, p_session_state);
		*reply_now = 0;
	} else if (cmd == CMD_GET_CALIBRATION) {
		get_calibration(p_sensor_state, p_session_state);
		*reply_now = 0;
	} else if (cmd == CMD_SET_CALIBRATION) {
		char *p = (char*)p_cmd;
		struct cmd_calibration_param *cal_param;

		if (!parameter) {
			log_message(DEBUG, "ERR: This CMD need take parameter\n");
			return ERR_CMD_NOT_SUPPORT;
		}

		cal_param = (struct cmd_calibration_param*)(p + sizeof(cmd_event));

		if (cal_param->sub_cmd == SUBCMD_CALIBRATION_SET)
			set_calibration_status(p_sensor_state,
						CALIBRATION_DONE | CALIBRATION_NEED_STORE,
						cal_param);
		else if (cal_param->sub_cmd == SUBCMD_CALIBRATION_START)
			set_calibration_status(p_sensor_state,
						CALIBRATION_RESET | CALIBRATION_NEED_STORE,
						cal_param);
		else if (cal_param->sub_cmd == SUBCMD_CALIBRATION_STOP)
			set_calibration_status(p_sensor_state,
						CALIBRATION_IN_PROGRESS,
						cal_param);
	} else if (cmd == CMD_ADD_EVENT) {
		handle_add_event(p_session_state, p_cmd);
		*reply_now = 0;
	} else if (cmd == CMD_CLEAR_EVENT) {
		if (p_session_state->event_id == p_cmd->parameter)
			handle_clear_event(p_session_state);
		else
			return ERR_SESSION_NOT_EXIST;
	} else if (cmd == CMD_SET_PROPERTY) {
#ifdef ENABLE_CONTEXT_ARBITOR
		if (strncmp(p_sensor_state->name, "PHYAC", SNR_NAME_MAX_LEN) == 0
			|| strncmp(p_sensor_state->name, "PEDOM", SNR_NAME_MAX_LEN) == 0
			|| strncmp(p_sensor_state->name, "STAP", SNR_NAME_MAX_LEN) == 0
			|| strncmp(p_sensor_state->name, "GSFLK", SNR_NAME_MAX_LEN) == 0
			|| strncmp(p_sensor_state->name, "SHAKI", SNR_NAME_MAX_LEN) == 0
			|| strncmp(p_sensor_state->name, "GSPX", SNR_NAME_MAX_LEN) == 0
			|| strncmp(p_sensor_state->name, "SCOUN", SNR_NAME_MAX_LEN) == 0
			|| strncmp(p_sensor_state->name, "SDET", SNR_NAME_MAX_LEN) == 0) {
			if (strncmp(p_sensor_state->name, "PHYAC", SNR_NAME_MAX_LEN) == 0) {
				ctx_activity_option_t activity_option;
				if (ctx_set_option(p_session_state->handle, p_cmd->parameter, (char *)p_cmd->buf, &activity_option) == -1) {
					return ERR_CMD_NOT_SUPPORT;
				}
LOGI("STY prop_mode %d cycle %d duty %d", activity_option.prop_mode, activity_option.prop_cycle_n & 0xFF, activity_option.prop_duty_m);
				send_set_property(p_sensor_state, PROP_ACT_MODE, 4, (unsigned char *)&activity_option.prop_mode);
				send_set_property(p_sensor_state, PROP_ACT_N, 4, (unsigned char *)&activity_option.prop_cycle_n);
				send_set_property(p_sensor_state, PROP_ACT_DUTY_M, 4, (unsigned char *)&activity_option.prop_duty_m);
				send_set_property(p_sensor_state, PROP_ACT_CLSMASK, 4, (unsigned char *)&activity_option.prop_clsmask);
			} else if (strncmp(p_sensor_state->name, "PEDOM", SNR_NAME_MAX_LEN) == 0) {
				ctx_pedometer_option_t pedometer_option;
				if (ctx_set_option(p_session_state->handle, p_cmd->parameter, (char *)p_cmd->buf, &pedometer_option) == -1)
					return ERR_CMD_NOT_SUPPORT;
				send_set_property(p_sensor_state, PROP_PEDOMETER_MODE, 4, (unsigned char *)&pedometer_option.prop_mode);
				send_set_property(p_sensor_state, PROP_PEDOMETER_N, 4, (unsigned char *)&pedometer_option.prop_cycle_n);
			} else if (strncmp(p_sensor_state->name, "STAP", SNR_NAME_MAX_LEN) == 0) {
				ctx_tapping_option_t tapping_option;
				if (ctx_set_option(p_session_state->handle, p_cmd->parameter, (char *)p_cmd->buf, &tapping_option) == -1)
					return ERR_CMD_NOT_SUPPORT;
				send_set_property(p_sensor_state, PROP_STAP_CLSMASK, 4, (unsigned char *)&tapping_option.prop_clsmask);
				send_set_property(p_sensor_state, PROP_STAP_LEVEL, 4, (unsigned char *)&tapping_option.prop_level);
			} else if (strncmp(p_sensor_state->name, "GSFLK", SNR_NAME_MAX_LEN) == 0) {
				ctx_gestureflick_option_t gestureflick_option;
				if (ctx_set_option(p_session_state->handle, p_cmd->parameter, (char *)p_cmd->buf, &gestureflick_option) == -1)
					return ERR_CMD_NOT_SUPPORT;
				send_set_property(p_sensor_state, PROP_GFLICK_CLSMASK, 4, (unsigned char *)&gestureflick_option.prop_clsmask);
				send_set_property(p_sensor_state, PROP_GFLICK_LEVEL, 4, (unsigned char *)&gestureflick_option.prop_level);
			} else if (strncmp(p_sensor_state->name, "SHAKI", SNR_NAME_MAX_LEN) == 0) {
				ctx_shaking_option_t shaking_option;
				if (ctx_set_option(p_session_state->handle, p_cmd->parameter, (char *)p_cmd->buf, &shaking_option) == -1)
					return ERR_CMD_NOT_SUPPORT;
				send_set_property(p_sensor_state, PROP_SHAKING_SENSITIVITY, 4, (unsigned char *)&shaking_option.prop_sensitivity);
			} else if (strncmp(p_sensor_state->name, "GSPX", SNR_NAME_MAX_LEN) == 0) {
				ctx_gesturehmm_option_t gesturehmm_option;
				if (ctx_set_option(p_session_state->handle, p_cmd->parameter, (char *)p_cmd->buf, &gesturehmm_option) == -1)
					return ERR_CMD_NOT_SUPPORT;
				// no option set to psh_fw for gesture hmm
			} else if (strncmp(p_sensor_state->name, "SCOUN", SNR_NAME_MAX_LEN) == 0) {
				ctx_stepcount_option_t stepcount_option;
				if (ctx_set_option(p_session_state->handle, p_cmd->parameter, (char *)p_cmd->buf, &stepcount_option) == -1)
					return ERR_CMD_NOT_SUPPORT;
				send_set_property(p_sensor_state, PROP_PEDOPLUS_SCOUNTERMODE, 4, (unsigned char *)&stepcount_option.prop_mode);
				send_set_property(p_sensor_state, PROP_PEDOPLUS_NSC, 4, (unsigned char *)&stepcount_option.prop_cycle_n);
			} else if (strncmp(p_sensor_state->name, "SDET", SNR_NAME_MAX_LEN) == 0) {
				ctx_stepdetect_option_t stepdetect_option;
				if (ctx_set_option(p_session_state->handle, p_cmd->parameter, (char *)p_cmd->buf, &stepdetect_option) == -1)
					return ERR_CMD_NOT_SUPPORT;
				send_set_property(p_sensor_state, PROP_PEDOPLUS_ADMISSION, 4, (unsigned char *)&stepdetect_option.prop_admission);
			}

		} else {
#endif
			send_set_property(p_sensor_state, p_cmd->parameter, p_cmd->parameter1, p_cmd->buf);	// property type, property size, property value
#ifdef ENABLE_CONTEXT_ARBITOR
		}
#endif
	}

	return SUCCESS;
}

static void handle_message(int fd, char *message)
{
	int event_type = *((int *) message);

	log_message(DEBUG, "handle_message(): fd is %d, event_type is %d\n",
							fd, event_type);

	if (event_type == EVENT_HELLO_WITH_SENSOR_TYPE) {
		hello_with_sensor_type_ack_event hello_with_sensor_type_ack;
		session_state_t *p_session_state, **next;
		hello_with_sensor_type_event *p_hello_with_sensor_type =
					(hello_with_sensor_type_event *)message;
		sensor_state_t *p_sensor_state = NULL;

		if ((p_sensor_state = get_sensor_state_with_name(p_hello_with_sensor_type->name)) == NULL) {
			hello_with_sensor_type_ack.event_type = EVENT_HELLO_WITH_SENSOR_TYPE;
			hello_with_sensor_type_ack.session_id = 0;
			send(fd, &hello_with_sensor_type_ack, sizeof(hello_with_sensor_type_ack), 0);
			LOGE("handle_message(): sensor type %s not supported \n", p_hello_with_sensor_type->name);
			return;
		}
		session_id_t session_id = allocate_session_id();

		/* allocate session ID and return it to client;
		   allocate session_state and add it to sensor_list */
		hello_with_sensor_type_ack.event_type =
					EVENT_HELLO_WITH_SENSOR_TYPE_ACK;
		hello_with_sensor_type_ack.session_id = session_id;
		send(fd, &hello_with_sensor_type_ack,
			sizeof(hello_with_sensor_type_ack), 0);

		p_session_state = malloc(sizeof(session_state_t));
		if (p_session_state == NULL) {
			LOGE("handle_message(): malloc failed \n");
			return;
		}
		memset(p_session_state, 0, sizeof(session_state_t));
		p_session_state->datafd = fd;
		p_session_state->session_id = session_id;

#ifdef ENABLE_CONTEXT_ARBITOR

		if (strncmp(p_sensor_state->name, "PHYAC", SNR_NAME_MAX_LEN) == 0
			|| strncmp(p_sensor_state->name, "PEDOM", SNR_NAME_MAX_LEN) == 0
			|| strncmp(p_sensor_state->name, "STAP", SNR_NAME_MAX_LEN) == 0
			|| strncmp(p_sensor_state->name, "GSFLK", SNR_NAME_MAX_LEN) == 0
			|| strncmp(p_sensor_state->name, "SHAKI", SNR_NAME_MAX_LEN) == 0
			|| strncmp(p_sensor_state->name, "GSPX", SNR_NAME_MAX_LEN) == 0
			|| strncmp(p_sensor_state->name, "SCOUN", SNR_NAME_MAX_LEN) == 0
			|| strncmp(p_sensor_state->name, "SDET", SNR_NAME_MAX_LEN) == 0) {
			void *handle;
			handle = ctx_open_session(p_sensor_state->name);
			if (handle == NULL) {
				free(p_session_state);
				return;
			}
			p_session_state->handle = handle;
		}
#endif

		if ((strncmp(p_sensor_state->name, "COMPS", SNR_NAME_MAX_LEN) == 0 || strncmp(p_sensor_state->name, "GYRO", SNR_NAME_MAX_LEN) == 0) &&
			p_sensor_state->list == NULL) {
			sensor_state_t *p_cal_sensor_state;
			/* In this case, this sensor is opened firstly,
			 * So calibration init is needed
			 */
			if (strncmp(p_sensor_state->name, "COMPS", SNR_NAME_MAX_LEN) == 0)
				p_cal_sensor_state = get_sensor_state_with_name("COMPC");
			else
				p_cal_sensor_state = get_sensor_state_with_name("GYROC");

			set_calibration_status(p_cal_sensor_state, CALIBRATION_INIT, NULL);
		}

		p_session_state->next = p_sensor_state->list;
		p_sensor_state->list = p_session_state;

	} else if (event_type == EVENT_HELLO_WITH_SESSION_ID) {
		hello_with_session_id_ack_event hello_with_session_id_ack;
		hello_with_session_id_event *p_hello_with_session_id =
					(hello_with_session_id_event *)message;
		session_id_t session_id = p_hello_with_session_id->session_id;
		session_state_t *p_session_state =
				get_session_state_with_session_id(session_id);

		if (p_session_state == NULL) {
			LOGE("handle_message(): EVENT_HELLO_WITH_SESSION_ID, not find matching session_id \n");
			return;
		}

		p_session_state->ctlfd = fd;

		hello_with_session_id_ack.event_type =
					EVENT_HELLO_WITH_SESSION_ID_ACK;
		hello_with_session_id_ack.ret = SUCCESS;
		send(fd, &hello_with_session_id_ack,
				sizeof(hello_with_session_id_ack), 0);

	} else if (event_type == EVENT_CMD) {
		ret_t ret;
		int reply_now;
		cmd_event *p_cmd = (cmd_event *)message;
		cmd_ack_event cmd_ack;

		ret = handle_cmd(fd, p_cmd, p_cmd->parameter,
					p_cmd->parameter1, p_cmd->parameter2, &reply_now);

		if (reply_now == 0)
			return;

		cmd_ack.event_type = EVENT_CMD_ACK;
		cmd_ack.ret = ret;
		send(fd, &cmd_ack, sizeof(cmd_ack), 0);
	} else {
		/* TODO: unknown message and drop it */
	}
}

/* 1 (re)open control node and send 'reset' cmd
   2 (re)open data node
   3 (re)open Unix socket */
static void reset_sensorhub()
{
	struct sockaddr_un serv_addr;
	char node_path[MAX_STRING_SIZE];
	DIR * dirp;
	struct dirent * entry;
	int found = 0;

	if (ctlfd != -1) {
		close(ctlfd);
		ctlfd = -1;
	}

	if (datafd != -1) {
		close(datafd);
		datafd = -1;
	}

	if (sockfd != -1) {
		close(sockfd);
		sockfd = -1;
	}

	if (datasizefd != -1) {
		close(datasizefd);
		datasizefd = -1;
	}

	if (fwversionfd != -1) {
		close(fwversionfd);
		fwversionfd = -1;
	}

	/* detect the device node */
	dirp = opendir("/sys/class/hwmon");
	if (dirp == NULL) {
		LOGE("can't find device node \n");
		exit(EXIT_FAILURE);
	}

	while ((entry = readdir(dirp)) != NULL) {
		int fd;
		char magic_string[32];

		if ((strcmp((const char *)entry->d_name, ".") == 0) ||(strcmp((const char *)entry->d_name, "..") == 0))
			continue;

		snprintf(node_path, MAX_STRING_SIZE, "/sys/class/hwmon/%s/device/modalias", entry->d_name);

		fd = open(node_path, O_RDONLY);
		if (fd == -1)
			continue;

		read(fd, magic_string, 32);
		magic_string[31] = '\0';
		close(fd);

		log_message(DEBUG, "magic_string is %s \n", magic_string);

		if (strstr(magic_string, "11A4") != NULL) {
			platform = MERRIFIELD;
			found = 1;
			break;
		}

		if ((strstr(magic_string, "psh") != NULL)
			|| (strstr(magic_string, "SMO91D0") != NULL)) {
			platform = BAYTRAIL;
			found = 1;
			break;
		}
	}

	if (found == 0) {
		LOGE("can't find device node \n");
		exit(EXIT_FAILURE);
	}

	log_message(DEBUG, "detected node dir is %s \n", entry->d_name);

	/* open control node */
	snprintf(node_path, MAX_STRING_SIZE, "/sys/class/hwmon/%s/device/control", entry->d_name);
	ctlfd = open(node_path, O_WRONLY);
	if (ctlfd == -1) {
		LOGE("open %s failed, errno is %d\n",
				node_path, errno);
		exit(EXIT_FAILURE);
	}

	/* TODO: send 'reset' cmd */

	/* open data node */
	snprintf(node_path, MAX_STRING_SIZE, "/sys/class/hwmon/%s/device/data", entry->d_name);
	datafd = open(node_path, O_RDONLY);
	if (datafd == -1) {
		LOGE("open %s failed, errno is %d\n",
				node_path, errno);
		exit(EXIT_FAILURE);
	}

	/* open data_size node */
	snprintf(node_path, MAX_STRING_SIZE, "/sys/class/hwmon/%s/device/data_size", entry->d_name);
	datasizefd = open(node_path, O_RDONLY);
	if (datasizefd == -1) {
		LOGE("open %s failed, errno is %d\n",
				node_path, errno);
		exit(EXIT_FAILURE);
	}

	/* create sensorhubd Unix socket */
	sockfd = android_get_control_socket(UNIX_SOCKET_PATH);
	listen(sockfd, MAX_Q_LENGTH);

	/* open fwversion node */
	snprintf(node_path, MAX_STRING_SIZE, "/sys/class/hwmon/%s/device/fw_version", entry->d_name);
	fwversionfd = open(node_path, O_RDONLY);
	if (fwversionfd == -1) {
		LOGE("open %s failed, errno is %d\n",
			node_path, errno);
		exit(EXIT_FAILURE);
	}

	closedir(dirp);
}

/* 1 release resources in sensor_list
   2 clear sensor_list to 0 */
static void reset_client_sessions()
{
	/* TODO: release resources in sensor_list */

	memset(sensor_list, 0, sizeof(sensor_list));
}

/* 1 read data from data node
   2 analyze the message format and extract the sensor data
   3 send to clients according to arbiter result */

struct cmd_resp {
	unsigned char tran_id;
	unsigned char cmd_type;
	unsigned char sensor_id;
	unsigned short data_len;
	char buf[0];
} __attribute__ ((packed));

static void write_data(session_state_t *p_session_state, void *data, int size)
{
	char buf[1];
	int ret;

	ret = recv(p_session_state->datafd, buf, 1, MSG_DONTWAIT);
	if (ret == 0)
		p_session_state->datafd_invalid = 1;

	if (p_session_state->datafd_invalid == 1)
		return;

	ret = write(p_session_state->datafd, data, size);
	if (ret < 0)
		p_session_state->datafd_invalid = 1;
}

static void send_data_to_clients(sensor_state_t *p_sensor_state, void *data,
						int size)
{
	session_state_t *p_session_state = p_sensor_state->list;
	char buf[MAX_MESSAGE_LENGTH];

	/* Use slide window mechanism to send data to target client,
	   buffer_delay is gauranteed, data count is not gauranteed.
	   calculate count of incoming data, send to clients by count  */
	for (; p_session_state != NULL;
		p_session_state = p_session_state->next) {
		int step, index = 0, i = 0;

		if ((p_session_state->state != ACTIVE) && (p_session_state->state != ALWAYS_ON))
			continue;

#ifdef ENABLE_CONTEXT_ARBITOR
		if (strncmp(p_sensor_state->name, "PHYAC", SNR_NAME_MAX_LEN) == 0
			|| strncmp(p_sensor_state->name, "PEDOM", SNR_NAME_MAX_LEN) == 0
			|| strncmp(p_sensor_state->name, "STAP", SNR_NAME_MAX_LEN) == 0
			|| strncmp(p_sensor_state->name, "GSFLK", SNR_NAME_MAX_LEN) == 0
			|| strncmp(p_sensor_state->name, "SHAKI", SNR_NAME_MAX_LEN) == 0
			|| strncmp(p_sensor_state->name, "GSPX", SNR_NAME_MAX_LEN) == 0
			|| strncmp(p_sensor_state->name, "SCOUN", SNR_NAME_MAX_LEN) == 0
			|| strncmp(p_sensor_state->name, "SDET", SNR_NAME_MAX_LEN) == 0) {
			void *out_data;
			int out_size;
			if (ctx_dispatch_data(p_session_state->handle, data, size, &out_data, &out_size) == 1)
				send(p_session_state->datafd, out_data, out_size, MSG_NOSIGNAL|MSG_DONTWAIT);
		} else
#endif
			send(p_session_state->datafd, data, size, MSG_NOSIGNAL|MSG_DONTWAIT);
	}
}

static void dispatch_get_single(struct cmd_resp *p_cmd_resp)
{
	int i;
	sensor_state_t *p_sensor_state = NULL;
	session_state_t *p_session_state;
	cmd_ack_event *p_cmd_ack;

	for (i = 0; i < current_sensor_index; i ++) {
		if (sensor_list[i].sensor_id == p_cmd_resp->sensor_id) {
			p_sensor_state = &sensor_list[i];
			break;
		}
	}

	if (p_sensor_state == NULL) {
		LOGE("dispatch_get_single(): unkown sensor id from psh fw %d \n", p_cmd_resp->sensor_id);
		return;
	}

	p_session_state = p_sensor_state->list;
	for (; p_session_state != NULL;
		p_session_state = p_session_state->next) {
		if (p_session_state->get_single == 0)
			continue;
		// TODO Compass data breaks the uniform handle of get_single ...
		// FIXME hard copy , too bad ...
		if (strncmp(p_sensor_state->name, "COMPS", SNR_NAME_MAX_LEN) == 0 || strncmp(p_sensor_state->name, "GYRO", SNR_NAME_MAX_LEN) == 0) {
			sensor_state_t *p_cal_sensor_state;

			if (strncmp(p_sensor_state->name, "COMPS", SNR_NAME_MAX_LEN) == 0) {
				p_cal_sensor_state = get_sensor_state_with_name("COMPC");
			} else {
				p_cal_sensor_state = get_sensor_state_with_name("GYROC");
			}

			p_cmd_ack = malloc(sizeof(cmd_ack_event) + p_cmd_resp->data_len + 2);
			if (p_cmd_ack == NULL) {
				LOGE("dispatch_get_single(): malloc failed \n");
				goto fail;
			}
			p_cmd_ack->event_type = EVENT_CMD_ACK;
			p_cmd_ack->ret = SUCCESS;
			p_cmd_ack->buf_len = p_cmd_resp->data_len + 2;
			{
				short *p = (short*)(p_cmd_ack->buf + p_cmd_resp->data_len);
				*p = check_calibration_status(p_cal_sensor_state, CALIBRATION_DONE);
			}
			memcpy(p_cmd_ack->buf, p_cmd_resp->buf, p_cmd_resp->data_len + 2);

			send(p_session_state->ctlfd, p_cmd_ack, sizeof(cmd_ack_event)
						+ p_cmd_resp->data_len + 2, 0);
		} else if (strncmp(p_sensor_state->name, "BIST", SNR_NAME_MAX_LEN) == 0) {
			int i;
			p_cmd_ack = malloc(sizeof(cmd_ack_event) + sizeof(struct bist_data));
			if (p_cmd_ack == NULL) {
				LOGE("dispatch_get_single(): malloc failed \n");
				goto fail;
			}
			p_cmd_ack->event_type = EVENT_CMD_ACK;
			p_cmd_ack->ret = SUCCESS;
			p_cmd_ack->buf_len = sizeof(struct bist_data);

//			p_cmd_resp->buf[0] = BIST_RET_NOSUPPORT;

			for (i = 0; i < PHY_SENSOR_MAX_NUM; i++) {
				sensor_state_t *p_sensor_state = get_sensor_state_with_name(sensor_type_to_name_str[i].name);

				if (p_sensor_state == NULL)
					p_cmd_ack->buf[i] = BIST_RET_NOSUPPORT;
				else if (p_sensor_state->sensor_id <= PHY_SENSOR_MAX_NUM)
					p_cmd_ack->buf[i] = p_cmd_resp->buf[p_sensor_state->sensor_id];
				else
					p_cmd_ack->buf[i] = BIST_RET_NOSUPPORT;
			}

			send(p_session_state->ctlfd, p_cmd_ack, sizeof(cmd_ack_event)
						+ sizeof(struct bist_data), 0);
		} else {
			p_cmd_ack = malloc(sizeof(cmd_ack_event)
						+ p_cmd_resp->data_len);
			if (p_cmd_ack == NULL) {
				LOGE("dispatch_get_single(): malloc failed \n");
				goto fail;
			}
			p_cmd_ack->event_type = EVENT_CMD_ACK;
			p_cmd_ack->ret = SUCCESS;
			p_cmd_ack->buf_len = p_cmd_resp->data_len;
			memcpy(p_cmd_ack->buf, p_cmd_resp->buf, p_cmd_resp->data_len);

			send(p_session_state->ctlfd, p_cmd_ack, sizeof(cmd_ack_event)
						+ p_cmd_resp->data_len, 0);

		}
			free(p_cmd_ack);
fail:
			p_session_state->get_single = 0;
	}
}

static void dispatch_streaming(struct cmd_resp *p_cmd_resp)
{
	psh_sensor_t sensor_type;
	unsigned char sensor_id = p_cmd_resp->sensor_id;
	sensor_state_t *p_sensor_state = get_sensor_state_with_id(sensor_id);

	if (p_sensor_state == NULL) {
		LOGE("%s: p_sensor_state is NULL \n", __func__);
		return;
	}

	if ((strncmp(p_sensor_state->name, "GYRO", SNR_NAME_MAX_LEN) == 0)
		|| (strncmp(p_sensor_state->name, "GYRO1", SNR_NAME_MAX_LEN) == 0)) {
		sensor_state_t *p_cal_sensor_state;
		struct gyro_raw_data *p_gyro_raw_data;
		struct gyro_raw_data *p_fake
			= (struct gyro_raw_data *)p_cmd_resp->buf;
		int i, len;

		len = p_cmd_resp->data_len / (sizeof(struct gyro_raw_data) - 2);
		p_gyro_raw_data = (struct gyro_raw_data *)malloc(len * sizeof(struct gyro_raw_data));
		if (p_gyro_raw_data == NULL)
			goto fail;

		p_cal_sensor_state = get_sensor_state_with_name("GYROC");
		for (i = 0; i < len; ++i) {
			p_gyro_raw_data[i].x = p_fake[i].x;
			p_gyro_raw_data[i].y = p_fake[i].y;
			p_gyro_raw_data[i].z = p_fake[i].z;
			p_gyro_raw_data[i].accuracy =
				check_calibration_status(p_cal_sensor_state,
							CALIBRATION_DONE);
		}
		send_data_to_clients(p_sensor_state, p_gyro_raw_data,
					len*sizeof(struct gyro_raw_data));
		free(p_gyro_raw_data);
	} else if ((strncmp(p_sensor_state->name, "COMPS", SNR_NAME_MAX_LEN) == 0)
		|| (strncmp(p_sensor_state->name, "COMP1", SNR_NAME_MAX_LEN) == 0)) {
		sensor_state_t *p_cal_sensor_state;
		struct compass_raw_data *p_compass_raw_data;
		struct compass_raw_data *p_fake = (struct compass_raw_data *)p_cmd_resp->buf;
		int i, len;

		len = p_cmd_resp->data_len / (sizeof(struct compass_raw_data) - 2);
		p_compass_raw_data = malloc(len * sizeof(struct compass_raw_data));
		if(p_compass_raw_data == NULL)
			goto fail;

		p_cal_sensor_state = get_sensor_state_with_name("COMPC");
		for (i = 0; i < len; ++i){
			p_compass_raw_data[i].x = p_fake[i].x;
			p_compass_raw_data[i].y = p_fake[i].y;
			p_compass_raw_data[i].z = p_fake[i].z;
			p_compass_raw_data[i].accuracy =
				check_calibration_status(p_cal_sensor_state,
							CALIBRATION_DONE);
		}
		//TODO What to do?
		send_data_to_clients(p_sensor_state, p_compass_raw_data,
					len*sizeof(struct compass_raw_data)); // Unggg ...
		free(p_compass_raw_data);
        } else {
		send_data_to_clients(p_sensor_state, p_cmd_resp->buf, p_cmd_resp->data_len);
	}

	return;
fail:
	LOGE("failed to allocate memory \n");
}

static void handle_calibration(struct cmd_calibration_param * param){
	sensor_state_t *p_sensor_state;
	session_state_t *p_session_state;
	cmd_ack_event *p_cmd_ack;
	psh_sensor_t sensor_type = param->sensor_type;

	p_sensor_state = get_sensor_state_with_name(sensor_type_to_name_str[sensor_type].name);
	if (p_sensor_state == NULL) {
		log_message(CRITICAL, "%s: p_sensor_state is NULL \n", __func__);
		return;
	}
	p_session_state = p_sensor_state->list;

	log_message(DEBUG, "Subcmd:%d.  Calibrated? %u\n", param->sub_cmd, param->calibrated);

	for (; p_session_state != NULL;
			p_session_state = p_session_state->next){
		struct cmd_calibration_param * p_cal_info;

		if (p_session_state->get_calibration == 0)
			continue;

		p_cmd_ack = malloc(sizeof(cmd_ack_event) + sizeof(struct cmd_calibration_param));
		if (p_cmd_ack == NULL) {
			LOGE("failed to allocate memory \n");
			goto fail;
		}
		p_cmd_ack->event_type = EVENT_CMD_ACK;
		p_cmd_ack->ret = SUCCESS;
		p_cmd_ack->buf_len = sizeof(struct cmd_calibration_param);

		p_cal_info = (struct cmd_calibration_param *)p_cmd_ack->buf;
		*p_cal_info = *param;

		send(p_session_state->ctlfd, p_cmd_ack,
				sizeof(cmd_ack_event) + sizeof(struct cmd_calibration_param), 0);

		free(p_cmd_ack);
fail:
		p_session_state->get_calibration = 0;
	}

	if (param->calibrated == SUBCMD_CALIBRATION_TRUE) {
		param->sub_cmd = SUBCMD_CALIBRATION_SET;
		set_calibration_status(p_sensor_state, CALIBRATION_DONE | CALIBRATION_NEED_STORE, param);
	} else {
		// SUBCMD_CALIBRATION_FALSE
		struct cmd_calibration_param param_temp;

		log_message(DEBUG, "Get calibration message, but calibrated not done.\n");
		memset(&param_temp, 0, sizeof(struct cmd_calibration_param));
                /* clear the parameter file */
		store_calibration_to_file(p_sensor_state, &param_temp);
		/* clear CALIBRATION_DONE */
		p_sensor_state->calibration_status &= ~CALIBRATION_DONE;
	}
}

/* use trans_id to match client */
static void handle_add_event_resp(struct cmd_resp *p_cmd_resp)
{
	unsigned char trans_id = p_cmd_resp->tran_id;
	sensor_state_t *p_sensor_state = get_sensor_state_with_name("EVENT");
	session_state_t *p_session_state;
	cmd_ack_event *p_cmd_ack;

	if (p_sensor_state == NULL) {
		LOGE("%s: p_sensor_state is NULL \n", __func__);
		return;
	}

	p_session_state = p_sensor_state->list;

	/* trans_id starts from 1, 0 means no trans_id */
	for (; p_session_state != NULL;
		p_session_state = p_session_state->next) {
		if (trans_id == p_session_state->trans_id) {
			p_cmd_ack = malloc(sizeof(cmd_ack_event)
					+ p_cmd_resp->data_len);
			if (p_cmd_ack == NULL) {
				LOGE("handle_add_event_resp(): malloc failed \n");
				return;
			}

			p_cmd_ack->event_type = EVENT_CMD_ACK;
			p_cmd_ack->ret = SUCCESS;
			p_cmd_ack->buf_len = p_cmd_resp->data_len;
			memcpy(p_cmd_ack->buf, p_cmd_resp->buf, p_cmd_resp->data_len);
			send(p_session_state->ctlfd, p_cmd_ack, sizeof(cmd_ack_event)
				+ p_cmd_resp->data_len, 0);

			log_message(DEBUG, "event id is %d \n", p_cmd_resp->buf[0]);
			free(p_cmd_ack);

			p_session_state->event_id = p_cmd_resp->buf[0];
//			p_session_state->trans_id = 0;
			break;
		}
	}
}

/* use event_id to match client */
static void handle_clear_event_resp(struct cmd_resp *p_cmd_resp)
{

}

/* use event_id to match client */
static void dispatch_event(struct cmd_resp *p_cmd_resp)
{
	unsigned char event_id;
	sensor_state_t *p_sensor_state = get_sensor_state_with_name("EVENT");
	session_state_t *p_session_state;

	if (p_sensor_state == NULL) {
		LOGE("%s: p_sensor_state is NULL \n", __func__);
		return;
	}

	p_session_state = p_sensor_state->list;

	if (p_cmd_resp->data_len != 0) {
		event_id = p_cmd_resp->buf[0];
	} else
		return;

	for (; p_session_state != NULL;
		p_session_state = p_session_state->next) {
		if (event_id == p_session_state->event_id) {
//			write(p_session_state->datafd, p_cmd_resp->buf, p_cmd_resp->data_len);
			send(p_session_state->datafd, p_cmd_resp->buf, p_cmd_resp->data_len, MSG_NOSIGNAL);
			break;
		}
	}
}

static void dispatch_data()
{
	static char *buf = NULL;
	char *p;
	int ret, data_size, left = 0;
	struct cmd_resp *p_cmd_resp;
	struct timeval tv, tv1;
	char datasize_buf[8];

	if (buf == NULL)
		buf = (char *)malloc(128 * 1024);

	if (buf == NULL) {
		LOGE("dispatch_data(): malloc failed \n");
		return;
	}

	gettimeofday(&tv, NULL);

	log_message(DEBUG, "Got a packet, timestamp is %d, %d \n",
					tv.tv_sec, tv.tv_usec);

	/* read data_size node */
	lseek(datasizefd, 0, SEEK_SET);
	ret = read(datasizefd, datasize_buf, 8);
	if (ret <= 0)
		return;

	datasize_buf[7] = '\0';
	sscanf(datasize_buf, "%d", &data_size);

	if ((data_size <= 0) || (data_size > 128 * 1024))
		return;

	left = data_size;

	/* sysfs has limitation that max 4K data can be returned at once */
	while (left > 0) {
		ret = read(datafd, buf + data_size - left, left);
		if (ret <= 0)
			return;

		left = left - ret;
	}

	ret = data_size;

//	log_message(DEBUG, "data_size is: %d, read() return value is %d \n",
//							data_size, ret);

	p_cmd_resp = (struct cmd_resp *) buf;
	p = buf;
	while (ret > 0) {
		log_message(DEBUG, "tran_id: %d, cmd_type: %d, sensor_id: %d, "
				"data_len: %d \n", p_cmd_resp->tran_id,
				p_cmd_resp->cmd_type, p_cmd_resp->sensor_id,
				p_cmd_resp->data_len);

		if ((p_cmd_resp->cmd_type == RESP_GET_SINGLE) || (p_cmd_resp->cmd_type == RESP_BIST_RESULT))
			dispatch_get_single(p_cmd_resp);
		else if (p_cmd_resp->cmd_type == RESP_STREAMING) {
			dispatch_streaming(p_cmd_resp);
		} else if (p_cmd_resp->cmd_type == RESP_COMP_CAL_RESULT){
			struct cmd_calibration_param param;
			struct resp_compasscal *p = (struct resp_compasscal*)p_cmd_resp->buf;

			if (p_cmd_resp->data_len != sizeof(struct resp_compasscal))
				log_message(DEBUG, "Get a calibration_get response with wrong data_len:%d\n",
											p_cmd_resp->data_len);

			param.sensor_type = SENSOR_CALIBRATION_COMP;
			param.sub_cmd = SUBCMD_CALIBRATION_GET;
			param.calibrated = p->calib_result;
			param.cal_param.compass = p->info;
			handle_calibration(&param);
		} else if (p_cmd_resp->cmd_type == RESP_GYRO_CAL_RESULT){
			struct cmd_calibration_param param;
			struct resp_gyrocal *p = (struct resp_gyrocal*)p_cmd_resp->buf;

			if (p_cmd_resp->data_len != sizeof(struct resp_gyrocal))
				log_message(DEBUG, "Get a calibration_get response with wrong data_len:%d\n",
											p_cmd_resp->data_len);

			param.sensor_type = SENSOR_CALIBRATION_GYRO;
			param.sub_cmd = SUBCMD_CALIBRATION_GET;
			param.calibrated = p->calib_result;
			param.cal_param.gyro = p->info;
			handle_calibration(&param);
		} else if (p_cmd_resp->cmd_type == RESP_ADD_EVENT) {
			/* TODO: record event id, match client with trans id, send return value to client */
			handle_add_event_resp(p_cmd_resp);
		} else if (p_cmd_resp->cmd_type == RESP_CLEAR_EVENT) {
			handle_clear_event_resp(p_cmd_resp);
		} else if (p_cmd_resp->cmd_type == RESP_EVENT) {
			/* dispatch event */
			dispatch_event(p_cmd_resp);
		} else
			log_message(DEBUG,
			    "%d: not support this response currently\n", p_cmd_resp->cmd_type);

		ret = ret - (sizeof(struct cmd_resp) + p_cmd_resp->data_len);
//		log_message(DEBUG, "remain data len is %d\n", ret);

		p = p + sizeof(struct cmd_resp) + p_cmd_resp->data_len;
		p_cmd_resp = (struct cmd_resp *)p;
	}

	gettimeofday(&tv1, NULL);
	log_message(DEBUG, "Done with dispatching a packet, latency is "
					"%d \n", tv1.tv_usec - tv.tv_usec);
}

static void remove_session_by_fd(int fd)
{
	int i = 0;

	for (i = 0; i < current_sensor_index; i ++) {
		session_state_t *p, *p_session_state = sensor_list[i].list;

		p = p_session_state;

		for (; p_session_state != NULL;
			p_session_state = p_session_state->next) {
			if ((p_session_state->ctlfd != fd)
				&& (p_session_state->datafd != fd)) {
				p = p_session_state;
				continue;
			}
//			close(p_session_state->ctlfd);
//			close(p_session_state->datafd);

			if (strncmp(sensor_list[i].name, "EVENT", SNR_NAME_MAX_LEN) == 0) {
			/* special treatment for SENSOR_EVENT */
				handle_clear_event(p_session_state);
			} else if (i == SENSOR_CALIBRATION_COMP ||
					i == SENSOR_CALIBRATION_GYRO) {

			} else {

				stop_streaming(&sensor_list[i], p_session_state);
			}
			if (p_session_state == sensor_list[i].list)
				sensor_list[i].list = p_session_state->next;
			else
				p->next = p_session_state->next;

			if (strncmp(sensor_list[i].name, "COMPS", SNR_NAME_MAX_LEN) == 0 || strncmp(sensor_list[i].name, "GYRO", SNR_NAME_MAX_LEN) == 0) {
				sensor_state_t *p_sensor_state;
				/* In this case, this session is closed,
				 * So calibration parameter is needed to store to file
				 */
				if (strncmp(sensor_list[i].name, "COMPS", SNR_NAME_MAX_LEN) == 0) {
					p_sensor_state = get_sensor_state_with_name("COMPC");
				} else {
					p_sensor_state = get_sensor_state_with_name("GYROC");
				}

				/* Set the CALIBRATION_NEED_STORE bit,
				 * parameter will be stored when get_calibration() response arrives
				 */
				set_calibration_status(p_sensor_state, CALIBRATION_NEED_STORE, NULL);
				get_calibration(p_sensor_state, NULL);
			}
#ifdef ENABLE_CONTEXT_ARBITOR
			if (strncmp(sensor_list[i].name, "PHYAC", SNR_NAME_MAX_LEN) == 0) {
				ctx_activity_option_t activity_option;
				if (ctx_close_session(p_session_state->handle, &activity_option) == 1) {
					send_set_property(&sensor_list[i], PROP_ACT_MODE, 4, (unsigned char *)&activity_option.prop_mode);
					send_set_property(&sensor_list[i], PROP_ACT_N, 4, (unsigned char *)&activity_option.prop_cycle_n);
					send_set_property(&sensor_list[i], PROP_ACT_DUTY_M, 4, (unsigned char *)&activity_option.prop_duty_m);
					send_set_property(&sensor_list[i], PROP_ACT_CLSMASK, 4, (unsigned char *)&activity_option.prop_clsmask);
				}
			} else if (strncmp(sensor_list[i].name, "PEDOM", SNR_NAME_MAX_LEN) == 0) {
				ctx_pedometer_option_t pedometer_option;
				if (ctx_close_session(p_session_state->handle, &pedometer_option) == 1) {
					send_set_property(&sensor_list[i], PROP_PEDOMETER_MODE, 4, (unsigned char *)&pedometer_option.prop_mode);
					send_set_property(&sensor_list[i], PROP_PEDOMETER_N, 4, (unsigned char *)&pedometer_option.prop_cycle_n);
				}
			} else if (strncmp(sensor_list[i].name, "STAP", SNR_NAME_MAX_LEN) == 0) {
				ctx_tapping_option_t tapping_option;
				if(ctx_close_session(p_session_state->handle, &tapping_option) == 1) {
					send_set_property(&sensor_list[i], PROP_STAP_CLSMASK, 4, (unsigned char *)&tapping_option.prop_clsmask);
					send_set_property(&sensor_list[i], PROP_STAP_LEVEL, 4, (unsigned char *)&tapping_option.prop_level);
				}
			}  else if (strncmp(sensor_list[i].name, "GSFLK", SNR_NAME_MAX_LEN) == 0) {
				ctx_gestureflick_option_t gestureflick_option;
				if (ctx_close_session(p_session_state->handle, &gestureflick_option) == 1) {
					send_set_property(&sensor_list[i], PROP_GFLICK_CLSMASK, 4, (unsigned char *)&gestureflick_option.prop_clsmask);
					send_set_property(&sensor_list[i], PROP_GFLICK_LEVEL, 4, (unsigned char *)&gestureflick_option.prop_level);
				}
			} else if (strncmp(sensor_list[i].name, "SHAKI", SNR_NAME_MAX_LEN) == 0) {
				ctx_shaking_option_t shaking_option;
				if (ctx_close_session(p_session_state->handle, &shaking_option) == 1) {
					send_set_property(&sensor_list[i], PROP_SHAKING_SENSITIVITY, 4, (unsigned char *)&shaking_option.prop_sensitivity);
				}
			} else if (strncmp(sensor_list[i].name, "GSPX", SNR_NAME_MAX_LEN) == 0) {
				ctx_gesturehmm_option_t gesturehmm_option;
				if (ctx_close_session(p_session_state->handle, &gesturehmm_option) == 1) {
					// no option need to be set to psh_fw for gesture hmm
				}
			} else if (strncmp(sensor_list[i].name, "SCOUN", SNR_NAME_MAX_LEN) == 0) {
				ctx_stepcount_option_t stepcount_option;
				if (ctx_close_session(p_session_state->handle, &stepcount_option) == 1) {
					send_set_property(&sensor_list[i], PROP_PEDOPLUS_SCOUNTERMODE, 4, (unsigned char *)&stepcount_option.prop_mode);
					send_set_property(&sensor_list[i], PROP_PEDOPLUS_NSC, 4, (unsigned char *)&stepcount_option.prop_cycle_n);
				}
			} else if (strncmp(sensor_list[i].name, "SDET", SNR_NAME_MAX_LEN) == 0) {
				ctx_stepdetect_option_t stepdetect_option;
				if (ctx_close_session(p_session_state->handle, &stepdetect_option) == 1) {
					send_set_property(&sensor_list[i], PROP_PEDOPLUS_ADMISSION, 4, (unsigned char *)&stepdetect_option.prop_admission);
				}
			}
#endif
			log_message(DEBUG, "session with datafd %d, ctlfd %d "
				"is removed \n", p_session_state->datafd,
				p_session_state->ctlfd);
			free(p_session_state);
			return;
		}
        }
}

/* 1 create data thread
   2 wait and handle the request from client in main thread */
static void start_sensorhubd()
{
	fd_set listen_fds, read_fds;
	int maxfd;
	int ret;

	/* two fd_set for I/O multiplexing */
	FD_ZERO(&listen_fds);
	FD_ZERO(&read_fds);

	/* add sockfd to listen_fds */
//	listen(sockfd, MAX_Q_LENGTH);
	FD_SET(sockfd, &listen_fds);
	maxfd = sockfd;

	/* get data from data node and dispatch it to clients, begin */
	fd_set datasize_fds;
	FD_ZERO(&datasize_fds);

	if (datasizefd > maxfd)
		maxfd = datasizefd;
	/* get data from data node and dispatch it to clients, end */

	while (1) {
		read_fds = listen_fds;
		FD_SET(datasizefd, &datasize_fds);

		/* error handling of select() */
		if (select(maxfd + 1, &read_fds, NULL, &datasize_fds, NULL)
								== -1) {
			if (errno == EINTR)
				continue;
			else {
				LOGE("sensorhubd socket "
					"select() failed. errno "
					"is %d\n", errno);
				exit(EXIT_FAILURE);
			}
		}

		/* handle new connection request */
		if (FD_ISSET(sockfd, &read_fds)) {
			struct sockaddr_un client_addr;
			socklen_t addrlen = sizeof(client_addr);
			int clientfd = accept(sockfd,
					(struct sockaddr *) &client_addr,
					&addrlen);
			if (clientfd == -1) {
				LOGE("sensorhubd socket "
					"accept() failed.\n");
				exit(EXIT_FAILURE);
			} else {
				LOGI("new connection from client\n");
				FD_SET(clientfd, &listen_fds);
				if (clientfd > maxfd)
					maxfd = clientfd;
			}

			FD_CLR(sockfd, &read_fds);
		}

		/* get data from data node and dispatch it to clients, begin */
		if (FD_ISSET(datasizefd, &datasize_fds)) {
			dispatch_data();
		}
		/* get data from data node and dispatch it to clients, end */

		/* handle request from clients */
		int i;
		for (i = maxfd; i >= 0; i--) {
			char message[MAX_MESSAGE_LENGTH];

			if (!FD_ISSET(i, &read_fds))
				continue;

			int length = recv(i, message, MAX_MESSAGE_LENGTH, 0);
			if (length <= 0) {
				/* release session resource if necessary */
				remove_session_by_fd(i);
				close(i);
				log_message(DEBUG, "fd %d:error reading message \n", i);
				FD_CLR(i, &listen_fds);
			} else {
				/* process message */
				handle_message(i, message);
			}
		}
	}
}

static void setup_psh()
{
	// setup DDR
	send_control_cmd(0, 1, 0, 0, 0, 0);
	// reset
	send_control_cmd(0, 0, 0, 0, 0, 0);

	return;
}

#define CMD_GET_STATUS		11

static void get_status()
{
#define LINK_AS_CLIENT		(0)
#define LINK_AS_MONITOR		(1)
#define LINK_AS_REPORTER	(2)
	struct link_info {
		unsigned char id;
		unsigned char ltype;
		unsigned short slide;
	} __attribute__ ((packed));

	struct sensor_info {
		unsigned char id;
		unsigned char status;
		unsigned short freq;
		unsigned short data_cnt;
		unsigned short slide;
		unsigned short priv;
		unsigned short attri;

		short freq_max;	// -1, means no fixed data rate
		char name[SNR_NAME_MAX_LEN + 1];

		unsigned char health;
		unsigned char link_num;
		struct link_info linfo[0];
	} __attribute__ ((packed));

	char cmd_string[MAX_STRING_SIZE];
	int size, ret;
	struct cmd_resp resp;
	struct sensor_info *snr_info;
	char buf[512];

	struct timeval tv, tv1;
	gettimeofday(&tv, NULL);

	size = snprintf(cmd_string, MAX_STRING_SIZE, "%d %d %d %d %d %d %d",
			0, CMD_GET_STATUS, 0, 0xff, 0xff, 0xff, 0xff);
	if (size <= 0) {
		log_message(CRITICAL, "[%s] failed to compose cmd_string \n", __func__);
		exit(EXIT_FAILURE);
	}

	LOGI("cmd to sysfs is: %s\n", cmd_string);

	ret = write(ctlfd, cmd_string, size);
	LOGI("cmd return value is %d\n", ret);
	if (ret < 0)
		exit(EXIT_FAILURE);

	while ((ret = read(datafd, &resp, sizeof(resp))) >= 0) {
		if (ret == 0) {
			usleep(100000);
			continue;
		}

		// it's safe to cast ret to unsigned int since ret > 0 at this point
		if ((unsigned int)ret < sizeof(resp))
			exit(EXIT_FAILURE);

		if (resp.cmd_type != CMD_GET_STATUS) {
			ret = read(datafd, buf, resp.data_len);
			if (ret != resp.data_len)
				exit(EXIT_FAILURE);
			continue;
		}

		if (resp.data_len == 0)
			break;

		ret = read(datafd, buf, resp.data_len);
		if (ret != resp.data_len)
			exit(EXIT_FAILURE);

		if (current_sensor_index > MAX_SENSOR_INDEX) {
			sensor_list = realloc(sensor_list, sizeof(sensor_state_t) * (current_sensor_index + 1));
			if (sensor_list == NULL)
				exit(EXIT_FAILURE);
			memset(&sensor_list[current_sensor_index], 0, sizeof(sensor_state_t));
		}

		snr_info = (struct sensor_info *)buf;

		LOGI("sensor id is %d, name is %s, freq_max is %d, current_sensor_index is %d \n", snr_info->id, snr_info->name, snr_info->freq_max, current_sensor_index);

		sensor_list[current_sensor_index].sensor_id = snr_info->id;
		sensor_list[current_sensor_index].freq_max = snr_info->freq_max;
		memcpy(sensor_list[current_sensor_index].name, snr_info->name, SNR_NAME_MAX_LEN);

		current_sensor_index++;
	}

	if (current_sensor_index > MAX_SENSOR_INDEX) {
		sensor_list = realloc(sensor_list, sizeof(sensor_state_t) * (current_sensor_index + 1));
		if (sensor_list == NULL)
			exit(EXIT_FAILURE);
		memset(&sensor_list[current_sensor_index], 0, sizeof(sensor_state_t));
	}

	memcpy(sensor_list[current_sensor_index].name, "EVENT", SNR_NAME_MAX_LEN);
	current_sensor_index++;

	gettimeofday(&tv1, NULL);
	LOGI("latency of is get_status() is "
		"%d \n", tv1.tv_usec - tv.tv_usec);

	if (ret < 0)
		 exit(EXIT_FAILURE);
}

static void usage()
{
	printf("Usage: sensorhubd [OPTION...] \n");
	printf("  -d, --enable-data-rate-debug      1: enable; 0: disable (default) \n");
	printf("  -l, --log-level        0-2, 2 is most verbose level \n");
	printf("  -h, --help             show this help message \n");

	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	int log_level = CRITICAL;

	if (is_first_instance() != 1)
		exit(0);

	while (1) {
		static struct option opts[] = {
			{"log-level", 2, NULL, 'l'},
			{"help", 0, NULL, 'h'},
			{0, 0, NULL, 0}
		};

		int index, o;

		o = getopt_long(argc, argv, "d:l::h", opts, &index);
		if (o == -1)
			break;

		switch (o) {
		case 'l':
			if (optarg != NULL)
				log_level = strtod(optarg, NULL);
			log_message(DEBUG, "log_level is %d \n", log_level);
			set_log_level(log_level);
			break;
		case 'h':
			usage();
			break;
		default:
			usage();
		}
	}

	sensor_list = malloc((MAX_SENSOR_INDEX + 1) * sizeof(sensor_state_t));
	if (sensor_list == NULL)
		exit(EXIT_FAILURE);
	memset(sensor_list, 0, (MAX_SENSOR_INDEX + 1) * sizeof(sensor_state_t));

	while (1) {
		reset_sensorhub();
		if (platform == BAYTRAIL)
			/* if fwupdat.flag is not exist or update failed, update fw */
			system(FWUPDATE_SCRIPT BYT_FW);
//		sleep(60);
		setup_psh();

		if (platform == BAYTRAIL) {
			/* fw version is not same or can't get version, update firmware */
			if (fw_verion_compare() == -1) {
				send_control_cmd(0, 255, 0, 0, 0, 0);
				system(FWUPDATE_SCRIPT BYT_FW " force");
				setup_psh();
			}
		}

		get_status();
//		reset_client_sessions();
		start_sensorhubd();
	}
	return 0;
}
