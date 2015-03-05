#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/un.h>
#include <assert.h>
#include <cutils/sockets.h>

#include "../include/message.h"
#include "../include/socket.h"
#include "../include/utils.h"
#include "../include/bist.h"

#undef LOG_TAG
#define LOG_TAG "LibsensorhubClient"

typedef struct {
	int datafd;
	int ctlfd;
	session_id_t session_id;
	char name[SNR_NAME_MAX_LEN + 1];
	unsigned char evt_id;
	struct cmd_event_param *evt_param;
} session_context_t;

handle_t psh_open_session_with_name(char *name)
{
	int datafd, len, ret, event_type, ctlfd;
	char message[MAX_MESSAGE_LENGTH];
	hello_with_sensor_type_event hello_with_sensor_type;
	hello_with_sensor_type_ack_event *p_hello_with_sensor_type_ack;
	hello_with_session_id_event hello_with_session_id;
	hello_with_session_id_ack_event *p_hello_with_session_id_ack;
	session_id_t session_id;
	struct cmd_event_param *evt_param = NULL;

	if (name == NULL)
		return NULL;

	len = strlen(name);
	if (len > SNR_NAME_MAX_LEN)
		return NULL;

	/* set up data connection */
	datafd = socket_local_client(UNIX_SOCKET_PATH, ANDROID_SOCKET_NAMESPACE_RESERVED, SOCK_STREAM);
	if (datafd < 0) {
		LOGE("socket_local_client() failed\n");
		return NULL;
	}

	/* send  EVENT_HELLO_WITH_SENSOR_TYPE and get session_id in ACK */
	hello_with_sensor_type.event_type = EVENT_HELLO_WITH_SENSOR_TYPE;
	memcpy(hello_with_sensor_type.name, name, len);
	hello_with_sensor_type.name[len] = '\0';
	ret = send(datafd, &hello_with_sensor_type, sizeof(hello_with_sensor_type), 0);
	if (ret < 0) {
		close(datafd);
		LOGE("write EVENT_HELLO_WITH_SENSOR_TYPE "
						"failed \n");
		return NULL;
	}

	ret = recv(datafd, message, MAX_MESSAGE_LENGTH, 0);
	if (ret < 0) {
		close(datafd);
		LOGE("read EVENT_HELLO_WITH_SENSOR_TYPE_ACK failed \n");
		return NULL;
	}

	event_type = *((int *) message);
	if (event_type != EVENT_HELLO_WITH_SENSOR_TYPE_ACK) {
		close(datafd);
		LOGE("not get expected EVENT_HELLO_WITH_SENSOR_TYPE_ACK \n");
		return NULL;
	}

	p_hello_with_sensor_type_ack = (hello_with_sensor_type_ack_event *) message;
	session_id = p_hello_with_sensor_type_ack->session_id;

	/* set up control connection */
	ctlfd = socket_local_client("sensorhubd", ANDROID_SOCKET_NAMESPACE_RESERVED, SOCK_STREAM);
	if (ctlfd < 0) {
		close(datafd);
		LOGE("socket_local_client() failed\n");
		return NULL;
	}

	/* send EVENT_HELLO_WITH_SESSION_ID and get ACK */
	hello_with_session_id.event_type = EVENT_HELLO_WITH_SESSION_ID;
	hello_with_session_id.session_id = session_id;
	ret = send(ctlfd, &hello_with_session_id, sizeof(hello_with_session_id), 0);
	if (ret < 0) {
		close(datafd);
		close(ctlfd);
		LOGE("write EVENT_HELLO_WITH_SESSION_ID failed \n");
		return NULL;
	}

	ret = recv(ctlfd, message, MAX_MESSAGE_LENGTH, 0);
	if (ret < 0) {
		close(datafd);
		close(ctlfd);
		LOGE("read EVENT_HELLO_WITH_SESSION_ID_ACK failed \n");
		return NULL;
	}

	event_type = *((int *) message);
	if (event_type != EVENT_HELLO_WITH_SESSION_ID_ACK) {
		close(datafd);
		close(ctlfd);
		LOGE("not get expected EVENT_HELLO_WITH_SESSION_ID_ACK \n");
		return NULL;
	}

	p_hello_with_session_id_ack = (hello_with_session_id_ack_event *) message;
	ret = p_hello_with_session_id_ack->ret;
	if (ret != SUCCESS) {
		close(datafd);
		close(ctlfd);
		LOGE("failed: EVENT_HELLO_WITH_SESSION_ID_ACK returned %d \n", ret);
		return NULL;
	}

	if (strncmp(name, "EVENT", SNR_NAME_MAX_LEN) == 0) {
		evt_param = malloc(sizeof(struct cmd_event_param));
		if (evt_param == NULL) {
			close(datafd);
			close(ctlfd);
			LOGE("failed to allocate memory for cmd_event_param");
			return NULL;
		}

		evt_param->num = 0;
		evt_param->relation = 0;
	}

	session_context_t *session_context = malloc(sizeof(session_context_t));
	if (session_context == NULL) {
		close(datafd);
		close(ctlfd);
		free(evt_param);
		LOGE("failed to allocate memory for session_context \n");
		return NULL;
	}

	session_context->datafd = datafd;
	session_context->ctlfd = ctlfd;
	session_context->session_id = session_id;
	memcpy(session_context->name, name, len);
	hello_with_sensor_type.name[len] = '\0';
	session_context->evt_id = 0;
	session_context->evt_param = evt_param;

	return session_context;
}

/* 1 set up data connection
   2 set up control connection */
handle_t psh_open_session(psh_sensor_t sensor_type)
{
	if (sensor_type >= SENSOR_MAX)
		return NULL;

	return psh_open_session_with_name(sensor_type_to_name_str[sensor_type].name);
}

void psh_close_session(handle_t handle)
{
	session_context_t *session_context = (session_context_t *)handle;

	if (session_context == NULL)
		return;

	close(session_context->datafd);
	close(session_context->ctlfd);
	if (session_context->evt_param != NULL)
		free(session_context->evt_param);
	free(session_context);
}

error_t psh_start_streaming(handle_t handle, int data_rate, int buffer_delay)
{
	session_context_t *session_context = (session_context_t *)handle;
	cmd_event cmd;
	int ret, event_type;
	char message[MAX_MESSAGE_LENGTH];
	cmd_ack_event *p_cmd_ack;

	if (session_context == NULL)
		return ERROR_NOT_AVAILABLE;

	if (data_rate == 0)
		return ERROR_NOT_AVAILABLE;

	cmd.event_type = EVENT_CMD;
	cmd.cmd = CMD_START_STREAMING;
	cmd.parameter = data_rate;
	cmd.parameter1 = buffer_delay;
	cmd.parameter2 = 0;

	ret = send(session_context->ctlfd, &cmd, sizeof(cmd), 0);
	if (ret <= 0)
		return ERROR_MESSAGE_NOT_SENT;

	ret = recv(session_context->ctlfd, message, MAX_MESSAGE_LENGTH, 0);
	if (ret <= 0)
		return ERROR_CAN_NOT_GET_REPLY;

	event_type = *((int *) message);
	if (event_type != EVENT_CMD_ACK)
		return ERROR_CAN_NOT_GET_REPLY;

	p_cmd_ack = (cmd_ack_event *)message;

	if (p_cmd_ack->ret != SUCCESS)
		return ERROR_DATA_RATE_NOT_SUPPORTED;

	return ERROR_NONE;
}

/* flag: 2 means no_stop_no_report when screen off; 1 means no_stop when screen off; 0 means stop when screen off */
error_t psh_start_streaming_with_flag(handle_t handle, int data_rate, int buffer_delay, streaming_flag flag)
{
	session_context_t *session_context = (session_context_t *)handle;
	cmd_event cmd;
	int ret, event_type;
	char message[MAX_MESSAGE_LENGTH];
	cmd_ack_event *p_cmd_ack;

	if (session_context == NULL)
		return ERROR_NOT_AVAILABLE;

	if (data_rate == 0)
		return ERROR_NOT_AVAILABLE;

	if ((flag != 0) && (flag != 1) && (flag != 2))
		return ERROR_WRONG_PARAMETER;

	cmd.event_type = EVENT_CMD;
	cmd.cmd = CMD_START_STREAMING;
	cmd.parameter = data_rate;
	cmd.parameter1 = buffer_delay;
	cmd.parameter2 = flag;

	ret = send(session_context->ctlfd, &cmd, sizeof(cmd), 0);
	if (ret <= 0)
		return ERROR_MESSAGE_NOT_SENT;

	ret = recv(session_context->ctlfd, message, MAX_MESSAGE_LENGTH, 0);
	if (ret <= 0)
		return ERROR_CAN_NOT_GET_REPLY;

	event_type = *((int *) message);
	if (event_type != EVENT_CMD_ACK)
		return ERROR_CAN_NOT_GET_REPLY;

	p_cmd_ack = (cmd_ack_event *)message;

	if (p_cmd_ack->ret != SUCCESS)
		return ERROR_DATA_RATE_NOT_SUPPORTED;
	return ERROR_NONE;
}

error_t psh_stop_streaming(handle_t handle)
{
	session_context_t *session_context = (session_context_t *)handle;
	cmd_event cmd;
	int ret, event_type;
	char message[MAX_MESSAGE_LENGTH];
	cmd_ack_event *p_cmd_ack;

	if (session_context == NULL)
		return ERROR_NOT_AVAILABLE;

	cmd.event_type = EVENT_CMD;
	cmd.cmd = CMD_STOP_STREAMING;

	ret = send(session_context->ctlfd, &cmd, sizeof(cmd), 0);
	if (ret <= 0)
		return ERROR_MESSAGE_NOT_SENT;

	ret = recv(session_context->ctlfd, message, MAX_MESSAGE_LENGTH, 0);
	if (ret <= 0)
		return ERROR_CAN_NOT_GET_REPLY;

	event_type = *((int *) message);
	if (event_type != EVENT_CMD_ACK)
		return ERROR_CAN_NOT_GET_REPLY;

	p_cmd_ack = (cmd_ack_event *)message;

	if (p_cmd_ack->ret != SUCCESS)
		return ERROR_NOT_AVAILABLE;

	return ERROR_NONE;
}

error_t psh_get_single(handle_t handle, void *buf, int buf_size)
{
	session_context_t *session_context = (session_context_t *)handle;
	cmd_event cmd;
	int ret, event_type, len;
	char message[MAX_MESSAGE_LENGTH];
	cmd_ack_event *p_cmd_ack;

	if (session_context == NULL)
		return ERROR_NOT_AVAILABLE;

	cmd.event_type = EVENT_CMD;
	cmd.cmd = CMD_GET_SINGLE;

	ret = send(session_context->ctlfd, &cmd, sizeof(cmd), 0);
	if (ret <= 0)
		return ERROR_MESSAGE_NOT_SENT;

	ret = recv(session_context->ctlfd, message, MAX_MESSAGE_LENGTH, 0);
	if (ret <= 0)
		return ERROR_CAN_NOT_GET_REPLY;

	event_type = *((int *) message);
	if (event_type != EVENT_CMD_ACK)
		return ERROR_CAN_NOT_GET_REPLY;

	p_cmd_ack = (cmd_ack_event *)message;

	if (p_cmd_ack->ret != SUCCESS)
		return ERROR_NOT_AVAILABLE;

	if (p_cmd_ack->buf_len < buf_size)
		len = p_cmd_ack->buf_len;
	else
		len = buf_size;
	memcpy(buf, p_cmd_ack->buf, len);

	return len;
}

int psh_get_fd(handle_t handle)
{
	session_context_t *session_context = (session_context_t *)handle;

	if (session_context == NULL)
		return ERROR_NOT_AVAILABLE;

	return session_context->datafd;
}

error_t psh_set_calibration(handle_t handle,
			        struct cmd_calibration_param * param)
{
	session_context_t *session_context = (session_context_t *)handle;
	int ret, event_type,  cmd_event_len;
	char message[MAX_MESSAGE_LENGTH];
	cmd_ack_event *p_cmd_ack;
	struct cmd_cal_info {
		cmd_event cmd;
		struct cmd_calibration_param param;
	} __attribute__ ((packed)) cmd_cal;

	if (session_context == NULL)
		return ERROR_NOT_AVAILABLE;

	if (strncmp(session_context->name, "COMPC", SNR_NAME_MAX_LEN) != 0 &&
		strncmp(session_context->name, "GYROC", SNR_NAME_MAX_LEN) != 0)
		return ERROR_WRONG_ACTION_ON_SENSOR_TYPE;

	if (param == NULL)
		return ERROR_WRONG_PARAMETER;

	cmd_event_len = sizeof(cmd_event) + sizeof(*param);
	param->sensor_type = 0;

	cmd_cal.cmd.event_type = EVENT_CMD;
	cmd_cal.cmd.cmd = CMD_SET_CALIBRATION;
	cmd_cal.param = *param;
	/* Why need this? */
	/* Because handle_cmd only sees cmd->parameter{0,1} ...  */
	cmd_cal.cmd.parameter = 1;

	ret = send(session_context->ctlfd, &cmd_cal, cmd_event_len, 0);
	if (ret <= 0)
		return ERROR_MESSAGE_NOT_SENT;

	ret = recv(session_context->ctlfd, message, MAX_MESSAGE_LENGTH, 0);
	if (ret <= 0)
		return ERROR_CAN_NOT_GET_REPLY;

	event_type = *((int *) message);
	if (event_type != EVENT_CMD_ACK)
		return ERROR_CAN_NOT_GET_REPLY;

	p_cmd_ack = (cmd_ack_event *)message;

	if (p_cmd_ack->ret != SUCCESS)
		return ERROR_NOT_AVAILABLE;

	/*	No non-status return message ...
	 *
	 *	if (p_cmd_ack->buf_len < buf_size)
	 *		len = p_cmd_ack->buf_len;
	 *	else
	 *		len = buf_size;
	 *	memcpy(buf, p_cmd_ack->buf, len);
	 */

	return ERROR_NONE;
}

error_t psh_get_calibration(handle_t handle,
				struct cmd_calibration_param * param){

	session_context_t *session_context = (session_context_t *)handle;
	int ret, event_type,  cmd_event_len;
	char message[MAX_MESSAGE_LENGTH];
	cmd_ack_event *p_cmd_ack;
	cmd_event req;

	log_message(DEBUG, "***********DEL-%s:%d", __func__, __LINE__);
	if (session_context == NULL)
		return ERROR_NOT_AVAILABLE;

	if (strncmp(session_context->name, "COMPC", SNR_NAME_MAX_LEN) != 0 &&
		strncmp(session_context->name, "GYROC", SNR_NAME_MAX_LEN) != 0)
		return ERROR_WRONG_ACTION_ON_SENSOR_TYPE;

	req.event_type = EVENT_CMD;
	req.cmd = CMD_GET_CALIBRATION;

	ret = send(session_context->ctlfd, &req, sizeof req, 0);
	if (ret <= 0)
		return ERROR_MESSAGE_NOT_SENT;

	ret = recv(session_context->ctlfd, message, MAX_MESSAGE_LENGTH, 0);
	if (ret <= 0)
		return ERROR_CAN_NOT_GET_REPLY;

	event_type = *((int *) message);
	if (event_type != EVENT_CMD_ACK)
		return ERROR_CAN_NOT_GET_REPLY;

	p_cmd_ack = (cmd_ack_event *)message;

	if (p_cmd_ack->ret != SUCCESS)
		return ERROR_NOT_AVAILABLE;

	assert(p_cmd_ack->buf_len == sizeof(*param));

	*param = *(struct cmd_calibration_param*)p_cmd_ack->buf;
	return ERROR_NONE;
}

/* relation: 0, AND; 1, OR */
error_t psh_event_set_relation(handle_t handle, relation relation)
{
	session_context_t *session_context = (session_context_t *)handle;

	if ((relation != 0) && (relation != 1))
		return ERROR_WRONG_PARAMETER;

	if (session_context == NULL)
		return ERROR_NOT_AVAILABLE;

	if (strncmp(session_context->name, "EVENT", SNR_NAME_MAX_LEN) != 0)
		return ERROR_WRONG_ACTION_ON_SENSOR_TYPE;

	session_context->evt_param->relation = relation;

	return ERROR_NONE;
}

error_t psh_event_append(handle_t handle, struct sub_event *sub_evt)
{
	session_context_t *session_context = (session_context_t *)handle;
	struct cmd_event_param *evt_param;

	if (session_context == NULL)
		return ERROR_NOT_AVAILABLE;

	if (strncmp(session_context->name, "EVENT", SNR_NAME_MAX_LEN) != 0)
		return ERROR_WRONG_ACTION_ON_SENSOR_TYPE;

	/* currently only support 5 sub events */
	evt_param = session_context->evt_param;
	if (evt_param->num >= 5) {
		LOGE("psh_event_append() "
			"currently only support 5 sub events \n");
		return ERROR_NOT_AVAILABLE;
	}

	if (sub_evt->sensor_id >= SENSOR_EVENT)
		return ERROR_WRONG_PARAMETER;

	if ((sub_evt->chan_id == 0) || (sub_evt->chan_id > 7))
		return ERROR_WRONG_PARAMETER;

	if (sub_evt->opt_id >= OP_MAX)
		return ERROR_WRONG_PARAMETER;

	evt_param->evts[evt_param->num] = *sub_evt;
	evt_param->num++;

	return ERROR_NONE;
}

error_t psh_add_event(handle_t handle)
{
	session_context_t *session_context = (session_context_t *)handle;
	struct cmd_event_param *evt_param;
	cmd_event *cmd;
	int ret, event_type;
	char message[MAX_MESSAGE_LENGTH];
	cmd_ack_event *p_cmd_ack;

	if (session_context == NULL)
		return ERROR_NOT_AVAILABLE;

	if (strncmp(session_context->name, "EVENT", SNR_NAME_MAX_LEN) != 0)
		return ERROR_WRONG_ACTION_ON_SENSOR_TYPE;

	/* psh_add_event() has been called before */
	if (session_context->evt_id != 0)
		return ERROR_NOT_AVAILABLE;

	evt_param = session_context->evt_param;
	if (evt_param->num == 0)
		return ERROR_NOT_AVAILABLE;

	cmd = malloc(sizeof(cmd_event) + sizeof(struct cmd_event_param));
	if (cmd == NULL)
		return ERROR_NOT_AVAILABLE;

	cmd->event_type = EVENT_CMD;
	cmd->cmd = CMD_ADD_EVENT;
	*(struct cmd_event_param *)(cmd->buf) = *evt_param;

	ret = send(session_context->ctlfd, cmd, sizeof(cmd_event) + sizeof(struct cmd_event_param), 0);
	free(cmd);
	if (ret <= 0)
		return ERROR_MESSAGE_NOT_SENT;

	ret = recv(session_context->ctlfd, message, MAX_MESSAGE_LENGTH, 0);
	if (ret <= 0)
		return ERROR_CAN_NOT_GET_REPLY;

	event_type = *((int *) message);
	if (event_type != EVENT_CMD_ACK)
		return ERROR_CAN_NOT_GET_REPLY;

	p_cmd_ack = (cmd_ack_event *)message;
	if (p_cmd_ack->ret != SUCCESS)
		return ERROR_NOT_AVAILABLE;

	if (p_cmd_ack->buf_len > 0)
		session_context->evt_id = p_cmd_ack->buf[0];

	return ERROR_NONE;
}

error_t psh_clear_event(handle_t handle)
{
	session_context_t *session_context = (session_context_t *)handle;
	cmd_event cmd;
	int ret, event_type;
	char message[MAX_MESSAGE_LENGTH];
	cmd_ack_event *p_cmd_ack;

	if (session_context == NULL)
		return ERROR_NOT_AVAILABLE;

	if (strncmp(session_context->name, "EVENT", SNR_NAME_MAX_LEN) != 0)
		return ERROR_WRONG_ACTION_ON_SENSOR_TYPE;

	if (session_context->evt_id == 0)
		return ERROR_NOT_AVAILABLE;

	cmd.event_type = EVENT_CMD;
	cmd.cmd = CMD_CLEAR_EVENT;
	cmd.parameter = session_context->evt_id;

	ret = send(session_context->ctlfd, &cmd, sizeof(cmd_event), 0);
	if (ret <= 0)
		return ERROR_MESSAGE_NOT_SENT;

	ret = recv(session_context->ctlfd, message, MAX_MESSAGE_LENGTH, 0);
	if (ret <= 0)
		return ERROR_CAN_NOT_GET_REPLY;

	event_type = *((int *) message);
	if (event_type != EVENT_CMD_ACK)
		return ERROR_CAN_NOT_GET_REPLY;

	p_cmd_ack = (cmd_ack_event *)message;
	if (p_cmd_ack->ret != SUCCESS)
		return ERROR_NOT_AVAILABLE;

	return ERROR_NONE;
}

/* 0 means psh_add_event() has not been called */
short psh_get_event_id(handle_t handle)
{
	session_context_t *session_context = (session_context_t *)handle;

	if (session_context == NULL)
		return ERROR_NOT_AVAILABLE;

	if (strncmp(session_context->name, "EVENT", SNR_NAME_MAX_LEN) != 0)
		return ERROR_WRONG_ACTION_ON_SENSOR_TYPE;

	return session_context->evt_id;
}

error_t psh_set_property(handle_t handle, property_type prop_type, void *value)
{
	int ret;
	ret = psh_set_property_with_size(handle, prop_type, 4, value);
	return ret;
}

error_t psh_set_property_with_size(handle_t handle, property_type prop_type, int size, void *value)
{
	session_context_t *session_context = (session_context_t *)handle;
	cmd_event *cmd;
	int ret, event_type;
	char message[MAX_MESSAGE_LENGTH];
	cmd_ack_event *p_cmd_ack;

	if (session_context == NULL)
		return ERROR_NOT_AVAILABLE;

	cmd = malloc(sizeof(cmd_event) + size);
	if (cmd == NULL)
		return ERROR_NOT_AVAILABLE;

	cmd->event_type = EVENT_CMD;
	cmd->cmd = CMD_SET_PROPERTY;
	cmd->parameter = prop_type;
	cmd->parameter1 = size;
	memcpy(cmd->buf, value, size);

	ret = send(session_context->ctlfd, cmd, sizeof(cmd_event) + size, 0);
	free(cmd);
	if (ret <= 0)
		return ERROR_MESSAGE_NOT_SENT;

	ret = recv(session_context->ctlfd, message, MAX_MESSAGE_LENGTH, 0);
	if (ret <= 0)
		return ERROR_CAN_NOT_GET_REPLY;

	event_type = *((int *) message);
	if (event_type != EVENT_CMD_ACK)
		return ERROR_CAN_NOT_GET_REPLY;

	p_cmd_ack = (cmd_ack_event *)message;
	if (p_cmd_ack->ret != SUCCESS)
		return ERROR_DATA_RATE_NOT_SUPPORTED;

	return ERROR_NONE;
}

error_t run_bist(struct bist_data *bist)
{
	handle_t handle;
	int size;

	handle = psh_open_session(SENSOR_BIST);
	if (handle == NULL)
		return ERROR_NOT_AVAILABLE;

	size = psh_get_single(handle, bist, sizeof(struct bist_data));
	if (size <= 0)
		return ERROR_NOT_AVAILABLE;


	psh_close_session(handle);
	return ERROR_NONE;
}
