#include "libsensorhub.h"

typedef enum {
	EVENT_HELLO_WITH_SENSOR_TYPE = 0,
	EVENT_HELLO_WITH_SENSOR_TYPE_ACK,
	EVENT_HELLO_WITH_SESSION_ID,
	EVENT_HELLO_WITH_SESSION_ID_ACK,
	EVENT_CMD,
	EVENT_CMD_ACK,
	EVENT_DATA
} event_t;

typedef enum {
	CMD_GET_SINGLE = 0,
	CMD_START_STREAMING,
	CMD_STOP_STREAMING,
	CMD_ADD_EVENT,
	CMD_CLEAR_EVENT,
	CMD_SET_CALIBRATION,
	CMD_GET_CALIBRATION,
	CMD_SET_PROPERTY,
	CMD_MAX
} cmd_t;

typedef enum {
	SUCCESS = 0,
	ERR_SENSOR_NOT_SUPPORT = -1,
	ERR_SESSION_NOT_EXIST = -2,
	ERR_SENSOR_NO_RESPONSE = -3,
	ERR_CMD_NOT_SUPPORT = -4,
	ERR_DATA_RATE_NOT_SUPPORT = -5
} ret_t;

#define SNR_NAME_MAX_LEN	5

struct sensor_name {
        char name[SNR_NAME_MAX_LEN + 1];
};

struct sensor_name sensor_type_to_name_str[SENSOR_MAX] = {
	{"ACCEL"}, {"GYRO"}, {"COMPS"}, {"BARO"}, {"ALS_P"}, {"PS_P"}, {"TERMC"}, {"LPE_P"},
	{"ACC1"}, {"GYRO1"}, {"COMP1"}, {"ALS1"}, {"PS1"}, {"BARO1"}, {"PHYAC"}, {"GSSPT"},
	{"GSFLK"}, {"RVECT"}, {"GRAVI"}, {"LACCL"}, {"ORIEN"}, {"COMPC"}, {"GYROC"}, {"9DOF"},
	{"PEDOM"}, {"MAGHD"}, {"SHAKI"}, {"MOVDT"}, {"STAP"}, {"PTZ"}, {"LTVTL"}, {"DVPOS"},
	{"SCOUN"}, {"SDET"}, {"SIGMT"}, {"6AGRV"}, {"6AMRV"}, {"6DOFG"}, {"6DOFM"}, {"LIFLK"},
	{"DTWGS"}, {"GSPX"}, {"BIST"}, {"EVENT"}
};

typedef unsigned int session_id_t;

typedef struct {
	event_t event_type;
	char name[SNR_NAME_MAX_LEN + 1];
} hello_with_sensor_type_event;

typedef struct {
	event_t event_type;
	session_id_t session_id;
} hello_with_sensor_type_ack_event;

typedef struct {
	event_t event_type;
	session_id_t session_id;
} hello_with_session_id_event;

typedef struct {
	event_t event_type;
	ret_t ret;
} hello_with_session_id_ack_event;

typedef struct {
	event_t event_type;
	cmd_t cmd;
	int parameter;
	int parameter1;
	int parameter2;
	unsigned char buf[];
} cmd_event;

typedef struct {
	event_t event_type;
	ret_t ret;
	int buf_len;
	unsigned char buf[];
} cmd_ack_event;

typedef struct {
	event_t event_type;
	int payload_size;
	char payload[];
} data_event;

struct cmd_event_param {
	unsigned char num;		/* sub event numbers */
	unsigned char relation;		/* AND/OR */
	struct sub_event evts[5];	/* currently only support 5 events */
}__attribute__ ((packed));
