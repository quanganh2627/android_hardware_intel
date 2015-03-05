/*
 * cp_deamon.h
 */

#ifndef _CP_DEAMON_H_
#define _CP_DEAMON_H_


#include <pthread.h>
#include <semaphore.h>
#include <utils/Log.h>
#include "cpdSocketServer.h"

#define CPD_OK              1
#define CPD_NOK             0
#define CPD_ERROR           -1

#define AT_CMD_CR_CHR       '\r'
#define AT_CMD_LF_CHR       '\n'
#define AT_CMD_QUERY_CHR    '?'
#define AT_CMD_EQ_CHR       '='
#define AT_CMD_COL_CHR      ':'
#define AT_CMD_SPACE_CHR    '_'
#define AT_CMD_ESC_CHR      (27)
#define AT_CMD_CTRL_Z_CHR   (26)



#define AT_CMD_CRLF         "\r\n"
#define AT_CMD_OK           "OK"
#define AT_CMD_ERROR        "ERROR"
#define AR_CMD_QUERY        "?"
#define AT_CMD_AT           "AT"

#define AT_CMD_CPOS         "+CPOS"
#define AT_CMD_CPOSR        "+CPOSR"
#define AT_CMD_XGENDATA     "+XGENDATA"
#define AT_CMD_RING         "RING"


#define AT_RESPONSE_NONE    0
#define AT_RESPONSE_ERROR   1
#define AT_RESPONSE_OK      2
#define AT_RESPONSE_CRLF    4
#define AT_RESPONSE_ANY     8


#define MODEM_MUX_AT_CMD_MAX_LENGTH     (1000)

#define XML_START_CHAR                  '<'
#define XML_END_CHAR                    '>'
#define XML_MAX_DATA_AGE_CPOS           (10000)

#define AT_UNSOL_RESPONSE_START     AT_CMD_CRLF
#define AT_UNSOL_RESPONSE_END1       (AT_CMD_CRLF AT_CMD_CRLF)
#define AT_UNSOL_RESPONSE_END2       ">\n\r\n"

/*****************************************************************************
 *
 *                           3GPP XML structures and definitions
 *
 *****************************************************************************/

#define GPS_MAX_N_SVS    (12)
/* conversion from floating-point representation of latitude & longitude into 3GPP integer fromat */
#define LATITUDE_FLOAT_TO_GPP  (93206.75556)
#define LONGITUDE_FLOAT_TO_GPP  (46603.37778)
/* conversion from 3GPP XML encoded latitude and longitude into floating-point representation */
#define LATITUDE_GPP_TO_FLOAT  (1.0 / LATITUDE_FLOAT_TO_GPP)
#define LONGITUDE_GPP_TO_FLOAT  (1.0 / LONGITUDE_FLOAT_TO_GPP)

/* Lat, Lon uncertainty calculation */
#define GPP_LL_UNCERT_C  (10.0)
#define GPP_LL_UNCERT_1X  (1.0 + 0.1)
#define GPP_LL_UNCERT_MAX_K (127)
#define GPP_LL_UNCERT_MAX_M (990)

/* Altitude uncertainty calculation */
#define GPP_ALT_UNCERT_C  (45.0)
#define GPP_ALT_UNCERT_1X  (1.0 + 0.025)
#define GPP_ALT_UNCERT_MAX_K    (127)
#define GPP_ALT_UNCERT_MAX_M    (990)

/*
 * 3GPP fix method type
 */
typedef enum {
    GPP_METHOD_TYPE_NONE = 0,
    GPP_METHOD_TYPE_MS_BASED = 3,
    GPP_METHOD_TYPE_MS_ASSISTED = 0x0C,
    GPP_METHOD_TYPE_MS_ASSISTED_NO_ACCURACY = 4,
} GPP_METHOD_TYPE_E;

/*
 * location_parameters_t
 */
typedef struct {
    int         north;
    double      degrees;

} LATITUDE, *pLATITUDE;

typedef struct {
    int         height_above_surface;
    int         height;

} ALTITUDE_T;

typedef struct {
    LATITUDE    latitude;
    double      longitude;

} COORDINATE, *pCOORDINATE;

/*
 *  ellipsoid_point
 */
typedef struct {
    COORDINATE      coordinate;

} POINT, *pPOINT;

/*
 *  ellipsoid_point_uncert_circle_t
 */
typedef struct {
    COORDINATE  coordinate;
    long        uncert_circle;

} POINT_UNCERT_CIRCLE, *pPOINT_UNCERT_CIRCLE;


/*
 *  ellipsoid_point_uncert_ellipse_t
 */
typedef struct {
    COORDINATE coordinate;
    long uncert_semi_major;
    long uncert_semi_minor;
    long orient_major;
    long confidence;

} POINT_UNCERT_ELLIPSE, *pPOINT_UNCERT_ELLIPSE;


/*
 *  ellipsoid_point_alt_t
 */
typedef struct {
    COORDINATE coordinate;
    ALTITUDE_T altitude;

} POINT_ALT, *pPOINT_ALT;

/*
 *  ellipsoid_point_alt_uncertellipse_t
 */
typedef struct {
    COORDINATE  coordinate;
    ALTITUDE_T  altitude;
    int         uncert_semi_major;
    int         uncert_semi_minor;
    int         orient_major;
    int         confidence;
    int         uncert_alt;

} POINT_ALT_UNCERTELLIPSE, *pPOINT_ALT_UNCERTELLIPSE;

/*
 *  polygon_t
 */
typedef struct {
    pCOORDINATE     pCoordinates;
    unsigned int    nItems;
} POLYGON, *pPOLYGON;

/*
 *  ellipsoid_arc_t
 */
typedef struct {
    COORDINATE  coordinate;
    int         inner_rad;
    int         uncert_rad;
    int         offset_angle;
    int         included_angle;
    int         confidence;

} ARC, *pARC;

typedef enum {
    SHAPE_TYPE_NONE,
    SHAPE_TYPE_POINT,
    SHAPE_TYPE_POINT_UNCERT_CIRCLE,
    SHAPE_TYPE_POINT_UNCERT_ELLIPSE,
    SHAPE_TYPE_POLYGON,
    SHAPE_TYPE_POINT_ALT,
    SHAPE_TYPE_POINT_ALT_UNCERT_ELLIPSE,
    SHAPE_TYPE_ARC

} SHAPE_TYPE_E;

typedef union {
    POINT                   point;
    POINT_UNCERT_CIRCLE     point_uncert_circle;
    POINT_UNCERT_ELLIPSE    point_uncert_ellipse;
    POINT_ALT               point_alt;
    POINT_ALT_UNCERTELLIPSE point_alt_uncertellipse;
    POLYGON                 polygon;
    ARC                     ellips_arc;
} SHAPE_DATA_U, *pSHAPE_DATA_U;

typedef struct {
    int     hor_velocity;
    int     vert_velocity;
    int     vert_velocity_direction;
    int     hor_uncert;
    int     vert_uncert;

} VELOCITY_DATA, *pVELOCITY_DATA;

typedef struct {
    int                 time;
    int                 direction;
    SHAPE_TYPE_E        shape_type;
    SHAPE_DATA_U        shape_data;
    VELOCITY_DATA       velocity_data;

} LOCATION_PARAMETERS, *pLOCATION_PARAMETERS;


typedef struct {
    LOCATION_PARAMETERS location_parameters;
    int                 time_of_fix;

} LOCATION, *pLOCATION;

/*
 *  2. assist_data
 *
 */
enum {
    ASSIST_DATA_GPS_ASSIST              = 0x00001,
    ASSIST_DATA_MSR_ASSIST_DATA         = 0x00002,
    ASSIST_DATA_SYSTEM_INFO_ASSIST_DATA = 0x00004,
    ASSIST_DATA_MORE_ASSIST_DATA        = 0x00008,
    ASSIST_DATA_EXT_CONTAINER           = 0x00010,
    ASSIST_DATA_REL98_ASSIST_DATA_EXT   = 0x00020,
    ASSIST_DATA_REL5_ASSIST_DATA_EXT    = 0x00040,
    ASSIST_DATA_REL7_ASSIST_DATA_EXT    = 0x00080
} ASSIST_DATA_E;

/*
 * ref_time_t
 */
typedef struct {
    int frame_number;
    int time_slot_number;
    int bit_number;
    int BCCH_carrier;
    int BSIC;

} GSM_TIME, *pGSM_TIME;

typedef struct {

    long    GPS_TOW_msec;
    int     GPS_week;
    unsigned long gpsTimeReceivedAt;
} GPS_TIME, *pGPS_TIME;

typedef struct {
    int sat_id;
    int tlm_word;
    int anti_sp;
    int alert;
    int tlm_res;

} GPS_TOW_ASSIST, *pGPS_TOW_ASSIST;

typedef struct {
    int             isSet;
    GPS_TIME        GPS_time;
    GSM_TIME        GSM_time;
    GPS_TOW_ASSIST  GPS_TOW_assist_arr[GPS_MAX_N_SVS];
    int             GPS_TOW_assist_arr_items;

} REF_TIME, *pREF_TIME;

/*
 * DGPS_corrections
 */
typedef struct {
    unsigned char   sat_id;
    int             IODE;
    int             UDRE;
    int             PRC;
    int             RRC;
    int             delta_PRC2;
    int             delta_RRC2;

} DGPS_CORRECTIONS, *pDGPS_CORRECTIONS;

/*
 * nav_model_elem
 */
typedef enum {
    NAV_ELEM_SAT_STATUS_NONE,
    NAV_ELEM_SAT_STATUS_NS_NN_U,
    NAV_ELEM_SAT_STATUS_ES_NN_U,
    NAV_ELEM_SAT_STATUS_NS_NN,
    NAV_ELEM_SAT_STATUS_ES_SN,
    NAV_ELEM_SAT_STATUS_REVD

} NAV_MODEL_SAT_STATUS_E;

typedef struct {
    int l2_code;
    int ura;
    int sv_health;
    int iodc;
    int l2p_flag;
    int esr1;
    int esr2;
    int esr3;
    int esr4;
    int tgd;
    int toc;
    int af2;
    int af1;
    int af0;
    int crs;
    int delta_n;
    int m0;
    int cuc;
    int ecc;
    int cus;
    long power_half;
    int toe;
    int fit_flag;
    int aoda;
    int cic;
    int omega0;
    int cis;
    int i0;
    int crc;
    int omega;
    long omega_dot;
    int idot;
} EPHEM_AND_CLOCK, *pEPHEM_AND_CLOCK;

typedef struct {
    int                     sat_id;
    NAV_MODEL_SAT_STATUS_E  sat_status;
    EPHEM_AND_CLOCK         ephem_and_clock;

} NAV_MODEL_ELEM, *pNAV_MODEL_ELEM;

/*
 * ionospheric_model
 */
typedef struct {
    int alfa0;
    int alfa1;
    int alfa2;
    int alfa3;
    int beta0;
    int beta1;
    int beta2;
    int beta3;

} IONOSPHERIC_MODEL, *pIONOSPHERIC_MODEL;

/*
 * UTC_model
 */
typedef struct {
    int a1;
    int a0;
    int tot;
    int wnt;
    int dtls;
    int wnlsf;
    int dn;
    int dtlsf;

} UTC_MODEL;

/*
 * almanac
 */
typedef struct {
    int data_id;
    unsigned char sat_id;
    int alm_ecc;
    int alm_toa;
    int alm_ksii;
    int alm_omega_dot;
    int alm_sv_health;
    int alm_power_half;
    int alm_omega0;
    int alm_omega;
    int alm_m0;
    int alm_af0;
    int alm_af1;

} ALM_ELEM, *pALM_ELEM;

typedef struct {
    int             wna;
    unsigned int    alm_elem_nb_items;
    pALM_ELEM       pAlm_elem;

} ALMANAC, *pALMANAC;

/*
 * acqu_assist_t
 */
typedef enum {
    SAT_INFO_DOPL1_UNCERT_12_5HZ,
    SAT_INFO_DOPL1_UNCERT_25HZ,
    SAT_INFO_DOPL1_UNCERT_50HZ,
    SAT_INFO_DOPL1_UNCERT_100HZ,
    SAT_INFO_DOPL1_UNCERT_200HZ

}dopl1_uncert_t;

typedef struct {
    int dopl1;
    dopl1_uncert_t dopl1_uncert;

} dopl_extra_t;

typedef struct {
    int az;
    int elev;

} az_el_t;

typedef struct {
    unsigned char sat_id;
    int dopl0;
    dopl_extra_t dopl_extra;
    int code_ph;
    int code_ph_int;
    int GPS_bitno;
    int srch_w;
    az_el_t az_el;

} sat_info_t;

typedef struct {
    long tow_msec;
    sat_info_t sat_info;

} ACQU_ASSIST, *pACQU_ASSIST;

enum {
    GPS_ASSIST_STATUS_HEALTH           = 0x00001,
    GPS_ASSIST_BTS_CLOCK_DRIFT         = 0x00002,
    GPS_ASSIST_REF_TIME_GPS            = 0x00004,
    GPS_ASSIST_REF_TIME_GSM            = 0x00008,
    GPS_ASSIST_REF_TIME_GPS_TOW_ASSIST = 0x00010,
    GPS_ASSIST_LOCATION_PARAMETERS     = 0x00020,
    GPS_ASSIST_DGPS_CORRECTIONS        = 0x00040,
    GPS_ASSIST_NAV_MODEL_ELEM          = 0x00080,
    GPS_ASSIST_IONOSPHERIC_MODEL       = 0x00100,
    GPS_ASSIST_UTC_MODEL               = 0x00200,
    GPS_ASSIST_ALMANAC                 = 0x00400,
    GPS_ASSIST_ACQU_ASSIST             = 0x00800,
    GPS_ASSIST_GPS_RT_INTEGRITY        = 0x01000,
    GPS_ASSIST_MSR_ASSIST_DATA         = 0x02000,
    GPS_ASSIST_SYS_INFO_ASSIST_DATA    = 0x04000,
    GPS_ASSIST_MORE_ASSIST_DATA        = 0x08000,
    GPS_ASSIST_EXT_CONTAINER           = 0x10000,
    GPS_ASSIST_REL98_ASSIST_DATA_EXT   = 0x20000,
    GPS_ASSIST_REL5_ASSIST_DATA_EXT    = 0x40000,
    GPS_ASSIST_REL7_ASSIST_DATA_EXT    = 0x80000
};

/*
 * GPS_assist
 */
typedef struct {
    int                     flag;

    int                     status_health;
    int                     BTS_clock_drift;
    REF_TIME                ref_time;
    LOCATION_PARAMETERS     location_parameters;
    DGPS_CORRECTIONS        DGPS_corrections;

    NAV_MODEL_ELEM          nav_model_elem_arr[GPS_MAX_N_SVS];
    int                     nav_model_elem_arr_items;

    IONOSPHERIC_MODEL ionospheric_model;

    UTC_MODEL UTC_model;

    ALMANAC almanac;

    ACQU_ASSIST acqu_assist;
    unsigned int acqu_assist_nb_items;

    int GPS_rt_integrity;

} GPS_ASSIST, *pGPS_ASSIST;

typedef struct {
    int             flag;
    GPS_ASSIST      GPS_assist;
    int             msr_assist_data;
    int             system_info_assist_data;
    int             more_assist_data;
    int             ext_container;
    int             rel98_assist_data_ext;
    int             rel5_assist_data_ext;
    int             rel7_assist_data_ext;

} ASSIST_DATA, *pASSIST_DATA;

/*
 *  3. pos_meas
 *
 */
typedef enum {
    POS_MEAS_NONE,
    POS_MEAS_RRLP,
    POS_MEAS_RRC,
    POS_MEAS_ABORT,
    POS_MEAS_STOP_GPS
} POS_MEAS_E;

typedef enum {
    MULT_SETS_NONE = -1,
    MULT_SETS_MULTIPLE = 0,
    MULT_SETS_ONE = 1
} MULT_SETS_E;

typedef enum {
    RRLP_METHOD_NONE,
    RRLP_METHOD_GPS

} RRLP_METHOD_E;

/*
 * RRLP_meas params
 */
typedef struct {
    GPP_METHOD_TYPE_E   method_type;
    int                 accurancy;
    RRLP_METHOD_E       RRLP_method;
    int                 resp_time_seconds;
    MULT_SETS_E         mult_sets;
} RRLP_MEAS, *pRRLP_MEAS;


/*
 * RRC_meas params
 */
typedef enum {
    REP_QUANT_RRC_METHOD_NONE,
    REP_QUANT_RRC_METHOD_OTDOA,
    REP_QUANT_RRC_METHOD_GPS,
    REP_QUANT_RRC_METHOD_OTDOAORGPS,
    REP_QUANT_RRC_METHOD_CELLID
} RRC_METHOD_E;

typedef struct {
    int             gps_timing_of_cell_wanted;
    int             addl_assist_data_req;
    int             RRC_method_type;
    RRC_METHOD_E    RRC_method;
    int             hor_acc;
    int             vert_accuracy;
} REP_QUANT, *pREP_QUANT;


typedef enum {
    REP_EVENT_NONE,
    REP_EVENT_TR_POS_CHG,
    REP_EVENT_TR_SFN_SFN_CHG,
    REP_EVENT_TR_SFN_GPS_CHG

} REP_EVENT_INFO_E;

typedef struct {
    REP_EVENT_INFO_E    type;

    int                 tr_pos_chg;
    int                 tr_SFN_SFN_chg;
    int                 tr_SFN_GPS_TOW;

} REP_EVENT_INFO, *pREP_EVENT_INFO;

typedef struct {

    int report_first_fix;
    int rep_amount;
    int meas_interval;
    REP_EVENT_INFO_E event_specific_info;

} REP_EVENT_PAR, *pREP_EVENT_PAR;

typedef struct {
    int rep_amount;
    int rep_interval_long;

} PERIOD_REP_CRIT, *pPERIOD_REP_CRIT;


typedef enum {
    REP_CRIT_NONE,
    REP_CRIT_EVENT,
    REP_CRIT_PERIOD

} REP_CRIT_E;

typedef struct {
    REP_CRIT_E          type;
    REP_EVENT_PAR       event_par;
    PERIOD_REP_CRIT     period_rep_crit;
} REP_CRIT, *pREP_CRIT;

typedef struct {
    GPP_METHOD_TYPE_E   method_type;
    REP_QUANT           rep_quant;
    REP_CRIT            rep_crit;
} RRC_MEAS, *pRRC_MEAS;

typedef union {
    RRLP_MEAS   rrlp_meas;
    RRC_MEAS    rrc_meas;
} POS_MEAS_U, *pPOS_MEAS_U;

typedef struct {
    POS_MEAS_E  flag;
    POS_MEAS_U  posMeas_u;
} POS_MEAS, *pPOS_MEAS;

/*
 *  4. GPS_meas
 *
 */
typedef enum {
    MULTIPATH_NOT_MEASURED,
    MULTIPATH_LOW,
    MULTIPATH_MEDIUM,
    MULTIPATH_HIGH
} MULTIPATH_E;

typedef struct {
    long tow_msec;
} ref_time_only_t;

typedef struct {
    unsigned char   sat_id;
    int             carr2_noise;
    int             dopl;
    int             whole_chips;
    int             fract_chips;
    MULTIPATH_E     multi_path;
    int             psr_rms_err;
} MEAS_PARAMS, *pMEAS_PARAMS;

typedef struct {
    unsigned long   tow_msec;
    unsigned int    meas_params_arr_items;
    MEAS_PARAMS     meas_params_arr[GPS_MAX_N_SVS];
} GPS_MEAS, *pGPS_MEAS;

/*
 *  5. GPS_assist_req
 *
 */
typedef struct {
    unsigned char   sat_id;
    int             iode;
} ADDL_REQ_SAT, *pADDL_REQ_SAT;

typedef struct {
    int         GPS_week;
    int         GPS_toe;
    int         ttoe_limit;
    ADDL_REQ_SAT addl_req_sat;
} NAV_ADDL_DATA, *pNAV_ADDL_DATA;

typedef struct {
    int alm_req;
    int UTC_model_req;
    int ion_req;
    int nav_model_req;
    int DGPS_corr_req;
    int ref_loc_req;
    int ref_time_req;
    int aqu_assist_req;
    int rt_integr_req;
    NAV_ADDL_DATA nav_addl_data;
} GPS_ASSIST_REQ, *pGPS_ASSIST_REQ;


/*
 *  6. msg
 *
 */
typedef enum {
    MSG_STATUS_ASSIST_DATA_NONE,
    MSG_STATUS_ASSIST_DATA_DELIVERED
} MSG_STATUS_E;

typedef struct {
    MSG_STATUS_E    status;
} MSG_STATUS, *pMSG_STATUS;


/*
 *  7. pos_er
 *
 */
typedef enum {
    POS_ERROR_UNDEFINED,
    POS_ERROR_NOT_ENOUGH_GPS_SATELLITES,
    POS_ERROR_NOT_GPS_ASSISTANCE_DATA_MISSING
} POS_ERR_REASON_E;

typedef struct {
    POS_ERR_REASON_E    err_reason;
    GPS_ASSIST_REQ      GPS_assist_req;
} POS_ERR, *pPOS_ERR;

typedef struct {
        unsigned int    posRequestedByNetwork;
        unsigned int    posRequestedFromGps;
        unsigned int    posReceivedFromGps1;
        unsigned int    posReceivedFromGps;
        unsigned int    posRequestId;
        unsigned int    posAbortId;
} POS_RESP_MEASUREMENTS, pPOS_RESP_MEASUREMENTS;

/* update version number, when structure or it's size changes */
#define CPD_MSG_VERSION (0x121218)


/*
 * 3GPP commands :
 *      1. location
 *      2. assist_data
 *      3. pos_meas
 *      4. GPS_meas
 *      5. GPS_assist_req.
 *      6. msg
 *      7. pos_err
 *
 */
typedef enum {
    REQUEST_FLAG_NONE = 0,
    REQUEST_FLAG_LOCATION = 1,
    REQUEST_FLAG_ASSIST_DATA = 2,
    REQUEST_FLAG_POS_MEAS = 4,
    REQUEST_FLAG_GPS_MEAS = 8,
    REQUEST_FLAG_GPS_ASSIST_REQ = 16,
    REQUEST_FLAG_CMD_MSG = 32,
    REQUEST_FLAG_CMD_POS_ERR = 64,
    REQUEST_FLAG_CTRL_MSG = 128
} REQUEST_FLAG_E;

typedef struct {
    GPP_METHOD_TYPE_E   method_type;  /* MS-BASD, MS-ASSISTED */
    int                 rep_resp_time_seconds; /* required/expected response time in seconds */
    int                 rep_amount;             /* number of position responses */
    int                 rep_interval_seconds;   /* position reporting interval in seconds - for multiple positions */
    int                 rep_hor_acc;            /* required horizonta accuracy in m */
    int                 rep_vert_accuracy;      /* required vertical accuracy in m */
    int                 assist_available;       /* flag for position assitance data below */
    double              assist_latitude;        /* assistance latitude */
    double              assist_longitude;       /* assistance longitude */
    int                 assist_hor_acc;         /* assistance horizontal acuracy in m */
    double              assist_altitude;        /* assistance altitude in m */
    int                 assist_vert_accuracy;   /* assistance altitude accuracy in m */
} REQUEST_SUMM, *pREQUEST_SUMM;

typedef struct {
    unsigned int requestReceivedAt;
    unsigned int responseFromGpsReceivedAt;
    unsigned int responseSentToModemAt;
    unsigned int nResponsesSent;
    unsigned int stopSentToGpsAt;
} REQUEST_STATUS, *pREQUEST_STATUS;

typedef struct {
    int             version;
    REQUEST_FLAG_E  flag;
    ASSIST_DATA     assist_data;
    POS_MEAS        posMeas;
    REQUEST_SUMM    rs;
//  GPS_ASSIST_REQ  GPS_assist_req;
    POS_RESP_MEASUREMENTS   dbgStats;
    REQUEST_STATUS      status;
} REQUEST_PARAMS, *pREQUEST_PARAMS;

typedef enum {
    RESPONSE_FLAG_NONE = 0,
    RESPONSE_FLAG_LOCATION = 1,
    RESPONSE_FLAG_ASSIST_DATA = 2,
    RESPONSE_FLAG_POS_MEAS = 4,
    RESPONSE_FLAG_GPS_MEAS = 8,
    RESPONSE_FLAG_GPS_ASSIST_REQ = 16,
    RESPONSE_FLAG_CMD_MSG = 32,
    RESPONSE_FLAG_CMD_POS_ERR = 64,
    RESPONSE_FLAG_CTRL_MSG = 128
} RESPONSE_FLAG_E;


typedef struct {
    int             version;
    RESPONSE_FLAG_E flag;
    LOCATION        location;
    GPS_MEAS        GPS_meas;
    GPS_ASSIST_REQ  GPS_assist_req;
    POS_ERR         pos_err;
    POS_RESP_MEASUREMENTS   dbgStats;
} RESPONSE_PARAMS, *pRESPONSE_PARAMS;


/*****************************************************************************
 *
 *   END of                  3GPP XML structures and definitions
 *
 *****************************************************************************/
#define GPS_CFG_FILENAME            "/system/etc/gps.conf"


#define RUN_STOPPED     0
#define RUN_TERMINATE   2
#define RUN_ACTIVE      8

#define MODEM_NAME_MAX_LEN 128
#define MODEM_NAME  "/dev/gsmtty7"
#define MODEM_POOL_INTERVAL     (1000UL)

#define MODEM_RX_BUFFER_SIZE    4096
#define MODEM_TX_BUFFER_SIZE    4096
#define XML_RX_BUFFER_SIZE      8192
#define GPS_COMM_RX_BUFFER_SIZE 4096

#define SOCKET_GPS_USE_LOCAL    (1) /* use local UNIX type sockets to connect to GPS */
#ifdef SOCKET_GPS_USE_LOCAL
#define SOCKET_HOST_GPS         "/data/data/gpsSocket"
#define SOCKET_PORT_GPS         0
#else
#define SOCKET_HOST_GPS         "localhost"
#define SOCKET_PORT_GPS         4121
#endif
#define SOCKET_SERVER_GPS_COMM_MAX_CONNECTIONS   (1)

#define SOCKET_PORT_MODEM_COMM_ENABLE_FILENAME  GPS_CFG_FILENAME
#define SOCKET_PORT_MODEM_COMM  4122
#define SOCKET_SERVER_MODEM_COMM_MAX_CONNECTIONS   (1)

typedef enum {
    THREAD_STATE_OFF,
    THREAD_STATE_STARTING,
    THREAD_STATE_RUNNING,
    THREAD_STATE_TERMINATE,
    THREAD_STATE_CANT_RUN,
    THREAD_STATE_TERMINATING,
    THREAD_STATE_TERMINATED
} THREAD_STATE_E;


typedef  int (fCPD_SEND_MSG_TO)(void * );
typedef  int (fCPD_SYSTEM_MONITOR)();

typedef struct {
    int             keepOpen;          /* y/N to keep open */
    int             keepOpenRetryCount;
    unsigned int    keepOpenRetryInterval;
    unsigned int    lastOpenAt;
} COMM_KEEP_OPEN_CTRL, *pCOMM_KEEP_OPEN_CTRL;

#define CPD_MODEM_MONITOR_RX_INTERVAL   300000
#define CPD_MODEM_MONITOR_TX_INTERVAL   300000
#define CPD_MODEM_MONITOR_CPOSR_EVENT   300000
#define CPD_MODEM_KEEPOPENRETRYINTERVAL (3000)
#define CPD_GPS_SOCKET_KEEPOPENRETRYINTERVAL (3000UL)
#define CPD_SYSTEMMONITOR_INTERVAL      (5000UL)    /* interval on which CPD will check services status */
#define CPD_SYSTEMMONITOR_INTERVAL_ACTIVE_SESSION  (1000UL)    /* interval on which CPD will check services status */

typedef struct {
    char                modemName[MODEM_NAME_MAX_LEN];
    int                 modemFd;
    COMM_KEEP_OPEN_CTRL keepOpenCtrl;

    pthread_t           modemReadThread;
    THREAD_STATE_E      modemReadThreadState;
    char                *pModemRxBuffer;
    int                 modemRxBufferSize;
    int                 modemRxBufferIndex;

    pthread_mutex_t     modemFdLock;

    int                 waitingForResponse;
    int                 waitForThisResponse;
    int                 haveResponse;
    int                 responseValue;

    int                 receivingXml;
    unsigned int        lastDataSent;
    unsigned int        lastDataReceived;

    int                 registeredForCPOSR;
    unsigned int                 registeredForCPOSRat;
    unsigned int                 receivedCPOSRat;
    unsigned int                 processingCPOSRat;
    unsigned int                 sendingCPOSat;
    int                 sentCPOSok;

    char                *pModemTxBuffer;
    int                 modemTxBufferSize;
    int                 modemTxBufferIndex;

} MODEM_INFO, pMODEM_INFO;

typedef struct {
    char    *pXmlBuffer;
    int     xmlBufferSize;
    int     xmlBufferIndex;
    unsigned int    lastUpdate;
    unsigned int    maxAge;
} XML_BUFFER, *pXML_BUFFER;


typedef struct {
    int     scGpsIndex;
    int     rxBufferSize;
    char    *pRxBuffer;
    int     rxBufferIndex;
    int     rxBufferMessageType;
    int     rxBufferCmdStart;
    int     rxBufferCmdEnd;
    int     rxBufferCmdDataStart;
    int     rxBufferCmdDataSize;

} GPS_COMM_BUFFER, *pGPS_COMM_BUFFER;

typedef struct {
    pthread_t           monitorThread;
    THREAD_STATE_E      monitorThreadState;
    unsigned int        loopInterval;
    unsigned int        lastCheck;
    int                 processingRequest;
    int                 pmfd;
} SYSTEM_MONITOR, *pSYSTEM_MONITOR;

typedef struct {
    int                     initialized;
    MODEM_INFO              modemInfo;
    XML_BUFFER              xmlRxBuffer;
    XML_BUFFER              xmlTxBuffer;

    REQUEST_PARAMS          request;
    RESPONSE_PARAMS         response;

    SOCKET_SERVER           ssModemComm;    /* socket server used for debug & modem comm pass-through */
    SOCKET_SERVER           scGps;          /* socket client used for comm with GPS */
    int                     scIndexToGps;   /* socket toward GPS - index in the array */
    COMM_KEEP_OPEN_CTRL     scGpsKeepOpenCtrl;

    GPS_COMM_BUFFER         gpsCommBuffer;

    /* Handle mesages received from modem, like CPOSR requests, etc... */
    fCPD_SEND_MSG_TO        *pfCposrMessageHandlerInCpd;  /* abstracted message handler for CPOSR messages in CPD, enables easy testing, otherwise not really needd, could be hard-coded */
    /* Interface between CPD and GPS - using sockets */
    fCPD_SEND_MSG_TO        *pfMessageHandlerInCpd;    /* called in CPDD when message from GPS is received */
    fCPD_SEND_MSG_TO        *pfMessageHandlerInGps;     /* called in GPS interface, when POS_MEAS request is received */
    /* Interface to GPS imlementation - used in CPD interface on GPS side, used to pass messages/requests from CPD to GPS code */
    fCPD_SEND_MSG_TO        *pfRequestHandlerInGps;     /* called in GPS interface, when POS_MEAS request is received */
    /* Interface for running cpdSystemMonitorStart() from GPS library */
    fCPD_SYSTEM_MONITOR     *pfSystemMonitorStart;


    SYSTEM_MONITOR          systemMonitor;
    SYSTEM_MONITOR          activeMonitor;

} CPD_CONTEXT, *pCPD_CONTEXT;


#endif

