////////////////////////////////////////////////////////////////////////////
// rril.h
//
// Copyright 2005-2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides RIL structures and constants.
//
/////////////////////////////////////////////////////////////////////////////
#ifndef RRIL_RRIL_H
#define RRIL_RRIL_H

#include <telephony/ril.h>
#include <telephony/librilutils.h>
#include <cutils/properties.h>
///////////////////////////////////////////////////////////////////////////////
// Typedefs
//
typedef long                RIL_CMD_ID;
typedef long                RIL_RESULT_CODE;


////////////////////////////////////////////////////////////////////////////////
// Android / WM structs conversion
//

#define WAIT_FOREVER            (0xFFFFFFFF)

///////////////////////////////////////////////////////////////////////////////
// Number of Calls to Track
//
#define RRIL_MAX_CALL_ID_COUNT       (10)

///////////////////////////////////////////////////////////////////////////////
// Number of Call Forwarding Entries to support
//
#define RIL_MAX_CALLFWD_ENTRIES     10

///////////////////////////////////////////////////////////////////////////////
// Value to indicate no retry for RIL_REQUEST_SETUP_DATA_CALL request.
// value comes from request description which can be found in ril.h
//
#define MAX_INT 0x7FFFFFFF;

///////////////////////////////////////////////////////////////////////////////
// Maximum length for various buffers and string parameters
//
#define MAX_BUFFER_SIZE            (1024)
#define MAX_PIN_SIZE               (9)
#define MAX_FACILITY_CODE          (5)
#define MAX_IPADDR_SIZE            (255)
#define MAX_PDP_CONTEXTS           (5)
#define MAX_INTERFACE_NAME_SIZE    (50)
#define MAX_PDP_TYPE_SIZE          (20)
#define MAX_MDM_RESOURCE_NAME_SIZE (20)
#define MAX_FDTIMER_SIZE           (5)
#define MODEM_STATE_UNKNOWN        (-1)
#define MAX_PARAM_LENGTH           (256)
#define MAX_APN_SIZE               (101)
#define AUTN_LENGTH                (32)
#define RAND_LENGTH                (32)

///////////////////////////////////////////////////////////////////////////////
// SIM related constants
//

const int EF_FDN = 0x6F3B;
const int EF_EXT2 = 0x6F4B;

const int SIM_COMMAND_READ_BINARY = 176;
const int SIM_COMMAND_READ_RECORD = 178;
const int SIM_COMMAND_GET_RESPONSE = 192;
const int SIM_COMMAND_UPDATE_BINARY = 214;
const int SIM_COMMAND_UPDATE_RECORD = 220;
const int SIM_COMMAND_STATUS = 242;
const int SIM_COMMAND_RETRIEVE_DATA = 203;
const int SIM_COMMAND_SET_DATA = 219;

const int SIM_USIM_APP_INDEX = 0;
const int ISIM_APP_INDEX = 1;

const UINT32 MAX_APP_LABEL_SIZE = 33; // including null termination
const UINT32 MAX_AID_SIZE = 33; // Hex string length including null termination

///////////////////////////////////////////////////////////////////////////////
// Radio off reasons
//
enum
{
    E_RADIO_OFF_REASON_NONE,
    E_RADIO_OFF_REASON_SHUTDOWN,
    E_RADIO_OFF_REASON_AIRPLANE_MODE
};

///////////////////////////////////////////////////////////////////////////////
// Registration type constants. This is used in determining the parsing
// function to be called, cache variable to be used etc.
//
enum
{
    E_REGISTRATION_TYPE_CREG = 0,
    E_REGISTRATION_TYPE_CGREG,
    E_REGISTRATION_TYPE_CEREG,
    E_REGISTRATION_TYPE_XREG
};

///////////////////////////////////////////////////////////////////////////////
// PIN cache mode constants
//
enum
{
    E_PIN_CACHE_MODE_FS = 1,
    E_PIN_CACHE_MODE_NVRAM = 2
};

///////////////////////////////////////////////////////////////////////////////
// Network selection mode constants
//
enum
{
    E_NETWORK_SELECTION_MODE_AUTOMATIC = 0,
    E_NETWORK_SELECTION_MODE_MANUAL = 1
};

///////////////////////////////////////////////////////////////////////////////
// radio power constants
//
const int RADIO_POWER_UNKNOWN = -1;
const int RADIO_POWER_OFF = 0;
const int RADIO_POWER_ON = 1;

///////////////////////////////////////////////////////////////////////////////
// screen state constants
//

const int SCREEN_STATE_UNKNOWN = -1;
const int SCREEN_STATE_OFF = 0;
const int SCREEN_STATE_ON = 1;

///////////////////////////////////////////////////////////////////////////////
// Selectable SIM application list information
//

typedef struct
{
    int appType;
    char szAid[MAX_AID_SIZE];
    char szAppLabel[MAX_APP_LABEL_SIZE];
    int sessionId;
} S_ND_SIM_APP_INFO;

typedef struct
{
    int nApplications;
    S_ND_SIM_APP_INFO aAppInfo[RIL_CARD_MAX_APPS];
} S_ND_SIM_APP_LIST_DATA, *P_ND_SIM_APP_LIST_DATA;


///////////////////////////////////////////////////////////////////////////////
// Retry count information
//
typedef struct
{
    int pin;
    int pin2;
    int puk;
    int puk2;
} S_PIN_RETRY_COUNT;

///////////////////////////////////////////////////////////////////////////////
// Network selection mode parameters
//
const int MAX_OPERATOR_NUMERIC_SIZE = 10;
typedef struct
{
    int mode;
    char szOperatorNumeric[MAX_OPERATOR_NUMERIC_SIZE];
} S_NETWORK_SELECTION_MODE_PARAMS;

///////////////////////////////////////////////////////////////////////////////
// Network selection mode parameters
//
typedef struct
{
    char szApn[MAX_APN_SIZE];
    char szPdpType[MAX_PDP_TYPE_SIZE];
} S_INITIAL_ATTACH_APN_PARAMS;

///////////////////////////////////////////////////////////////////////////////
// Facility Lock Context data
//
typedef struct
{
    UINT32 uiResultCode;
    char szFacilityLock[MAX_FACILITY_CODE];
} S_SET_FACILITY_LOCK_CONTEXT_DATA;

///////////////////////////////////////////////////////////////////////////////
// SIM IO Context data
//
typedef struct
{
    int command;
    int fileId;
} S_SIM_IO_CONTEXT_DATA;

///////////////////////////////////////////////////////////////////////////////
// Setup data call Context data
//
typedef struct
{
    UINT32 uiCID;
} S_SETUP_DATA_CALL_CONTEXT_DATA;

///////////////////////////////////////////////////////////////////////////////
// Data call Info
//
typedef struct
{
    int failCause;
    UINT32 uiCID;
    int state;
    char szPdpType[MAX_PDP_TYPE_SIZE];
    char szInterfaceName[MAX_INTERFACE_NAME_SIZE];
    char szIpAddr1[MAX_IPADDR_SIZE];
    char szIpAddr2[MAX_IPADDR_SIZE];
    char szDNS1[MAX_IPADDR_SIZE];
    char szDNS2[MAX_IPADDR_SIZE];
    char szIpV6DNS1[MAX_IPADDR_SIZE];
    char szIpV6DNS2[MAX_IPADDR_SIZE];
    char szGateways[MAX_IPADDR_SIZE];
} S_DATA_CALL_INFO;

///////////////////////////////////////////////////////////////////////////////
// Default PDN context parameters
//
typedef struct
{
    char szIpV4Addr[MAX_IPADDR_SIZE];
    char szIpV6Addr[MAX_IPADDR_SIZE];
    char szIpv4SubnetMask[MAX_IPADDR_SIZE];
    char szIpv6SubnetMask[MAX_IPADDR_SIZE];
    char szIpV4DNS1[MAX_IPADDR_SIZE];
    char szIpV4DNS2[MAX_IPADDR_SIZE];
    char szIpV6DNS1[MAX_IPADDR_SIZE];
    char szIpV6DNS2[MAX_IPADDR_SIZE];
    char szIpV4GatewayAddr[MAX_IPADDR_SIZE];
    char szIpV6GatewayAddr[MAX_IPADDR_SIZE];
    char szIpV4PCSCF1[MAX_IPADDR_SIZE];
    char szIpV4PCSCF2[MAX_IPADDR_SIZE];
    char szIpV6PCSCF1[MAX_IPADDR_SIZE];
    char szIpV6PCSCF2[MAX_IPADDR_SIZE];
} S_DEFAULT_PDN_CONTEXT_PARAMS, *P_DEFAULT_PDN_CONTEXT_PARAMS;

///////////////////////////////////////////////////////////////////////////////
// registration status information used internally
//

#define MAX_REG_STATUS_LENGTH 8

typedef struct
{
    char szState[MAX_REG_STATUS_LENGTH];
    char szAcT[MAX_REG_STATUS_LENGTH];
    char szLAC[MAX_REG_STATUS_LENGTH];
    char szCID[MAX_REG_STATUS_LENGTH];
} S_REG_INFO;

// Pref network type request information cache.
typedef struct
{
    RIL_Token token;
    RIL_PreferredNetworkType type;
    size_t datalen;
} PREF_NET_TYPE_REQ_INFO;

///////////////////////////////////////////////////////////////////////////////
// DTMF states
//
enum
{
    E_DTMF_STATE_START = 1,
    E_DTMF_STATE_STOP,
};

///////////////////////////////////////////////////////////////////////////////
// Voice call state information
//
typedef struct
{
    int id;
    int state;
    BOOL bDtmfAllowed;
} S_VOICECALL_STATE_INFO;

///////////////////////////////////////////////////////////////////////////////
// Data call states
//
enum
{
    E_DATA_STATE_IDLE = 0,
    E_DATA_STATE_INITING,
    E_DATA_STATE_ACTIVATING,
    E_DATA_STATE_ACTIVE,
    E_DATA_STATE_DEACTIVATING,
    E_DATA_STATE_DEACTIVATED
};

///////////////////////////////////////////////////////////////////////////////
// Registration States
//
// Check 3GPP 27.007 R11 section 7.2
enum
{
    E_REGISTRATION_NOT_REGISTERED_NOT_SEARCHING = 0,
    E_REGISTRATION_REGISTERED_HOME_NETWORK = 1,
    E_REGISTRATION_NOT_REGISTERED_SEARCHING = 2,
    E_REGISTRATION_DENIED = 3,
    E_REGISTRATION_UNKNOWN = 4,
    E_REGISTRATION_REGISTERED_ROAMING = 5,
    E_REGISTRATION_REGISTERED_FOR_SMS_ONLY_HOME_NETWORK = 6,
    E_REGISTRATION_REGISTERED_FOR_SMS_ONLY_ROAMING = 7,
    E_REGISTRATION_EMERGENCY_SERVICES_ONLY = 8,
    E_REGISTRATION_REGISTERED_FOR_CSFB_NP_HOME_NETWORK = 9,
    E_REGISTRATION_REGISTERED_FOR_CSFB_NP_ROAMING = 10
};

///////////////////////////////////////////////////////////////////////////////
// Call status
//
// Check 3GPP 27.007 section 7.18, 6 and 7 are IMC specific
enum
{
    E_CALL_STATUS_ACTIVE =          0,
    E_CALL_STATUS_HELD =            1,
    E_CALL_STATUS_DIALING =         2,
    E_CALL_STATUS_ALERTING =        3,
    E_CALL_STATUS_INCOMING =        4,
    E_CALL_STATUS_WAITING =         5,
    E_CALL_STATUS_DISCONNECTED =    6,
    E_CALL_STATUS_CONNECTED =       7
};

///////////////////////////////////////////////////////////////////////////////
// SIM States
//
enum
{
    E_SIM_READY = 1,
    E_SIM_PIN,
    E_SIM_PIN2,
    E_SIM_PUK,
    E_SIM_PUK2,
    E_SIM_PH_NETWORK,
    E_SIM_ABSENT
};

///////////////////////////////////////////////////////////////////////////////
// Fast Dormancy modes
//
enum
{
    E_FD_MODE_OEM_MANAGED = 1,
    E_FD_MODE_DISPLAY_DRIVEN,
    E_FD_MODE_ALWAYS_ON
};

///////////////////////////////////////////////////////////////////////////////
// MT CLASS
//
enum
{
    E_MT_CLASS_A = 1,
    E_MT_CLASS_B = 2,
    E_MT_CLASS_CG = 3,
    E_MT_CLASS_CC = 4,
};

///////////////////////////////////////////////////////////////////////////////
// mode used in +XCGEDPAGE
//
enum
{
    E_MODE_ONE_SHOT_DUMP = 0,
    E_MODE_PERIODIC_REFRESHED_DUMP = 1,
    E_MODE_STOP_PERIODIC_DUMP = 2,
    E_MODE_RESET_STATISTICS = 3,
    E_MODE_STOP_EM = 4
};

///////////////////////////////////////////////////////////////////////////////
// XICFG parmameters
//
enum
{
    E_XICFG_IMS_APN = 51,
    E_XICFG_PCSCF_ADDRESS = 102,
    E_XICFG_PCSCF_PORT = 101,
    E_XICFG_IMS_AUTH_MODE = 150,
    E_XICFG_PHONE_CONTEXT = 10,
    E_XICFG_LOCAL_BREAKOUT = 52,
    E_XICFG_XCAP_APN = 200,
    E_XICFG_XCAP_ROOT_URI = 201,
    E_XICFG_XCAP_USER_NAME = 202,
    E_XICFG_XCAP_USER_PASSWORD = 203
};
///////////////////////////////////////////////////////////////////////////////
// XICFG_SET : XICFG write command
// XICFG_GET : XICFG read  command
// XICFG number of parameters used in rril
const int   XICFG_SET = 0;
const int   XICFG_GET  = 1;
const int   XICFG_N_PARAMS  = 10;

///////////////////////////////////////////////////////////////////////////////
// Notify / Result Codes (m_dwCode)
//

#define RRIL_RESULT_OK                  RIL_E_SUCCESS                // 0x00000000
#define RRIL_RESULT_RADIOOFF            RIL_E_RADIO_NOT_AVAILABLE    // 0x00000001
#define RRIL_RESULT_ERROR               RIL_E_GENERIC_FAILURE        // 0x00000002
#define RRIL_RESULT_PASSWORDINCORRECT   RIL_E_PASSWORD_INCORRECT     // 0x00000003
#define RRIL_RESULT_SIMPIN2             RIL_E_SIM_PIN2               // 0x00000004
#define RRIL_RESULT_SIMPUK2             RIL_E_SIM_PUK2               // 0x00000005
#define RRIL_RESULT_NOTSUPPORTED        RIL_E_REQUEST_NOT_SUPPORTED  // 0x00000006
#define RRIL_RESULT_CALLABORTED         RIL_E_CANCELLED              // 0x00000007
#define RRIL_RESULT_FDN_FAILURE         RIL_E_FDN_CHECK_FAILURE      // 0x0000000E
#define RRIL_RESULT_OK_IMMEDIATE        (0x00000010)

// V25 Results
#define RRIL_RESULT_NOCARRIER      RIL_E_GENERIC_FAILURE
#define RRIL_RESULT_NODIALTONE     RIL_E_GENERIC_FAILURE
#define RRIL_RESULT_BUSY           RIL_E_GENERIC_FAILURE
#define RRIL_RESULT_NOANSWER       RIL_E_GENERIC_FAILURE
#define RRIL_NOTIFY_CONNECT        (0x00010000)


///////////////////////////////////////////////////////////////////////////////
// Error codes (in pBlobs) - used to trigger actions during response handling
//

#define RRIL_E_NO_ERROR                             (0x00000000)
#define RRIL_E_UNKNOWN_ERROR                        (0x00001000)
#define RRIL_E_ABORT                                (0x00001002)
#define RRIL_E_DIALSTRING_TOO_LONG                  (0x00001003)
#define RRIL_E_MODEM_NOT_READY                      (0x00001004)
#define RRIL_E_TIMED_OUT                            (0x00001005)
#define RRIL_E_CANCELLED                            (0x00001006)
#define RRIL_E_RADIOOFF                             (0x00001007)
#define RRIL_E_MODEM_RESET                          (0x00001008)


///////////////////////////////////////////////////////////////////////////////
// Information Classes
//
#define RRIL_INFO_CLASS_NONE                        (0x00000000)      // None
#define RRIL_INFO_CLASS_VOICE                       (0x00000001)      // Voice
#define RRIL_INFO_CLASS_DATA                        (0x00000002)      // Data
#define RRIL_INFO_CLASS_FAX                         (0x00000004)      // Fax
#define RRIL_INFO_CLASS_SMS                         (0x00000008)      // Short Message Service
#define RRIL_INFO_CLASS_DATA_CIRCUIT_SYNC           (0x00000010)      // Data Circuit Sync
#define RRIL_INFO_CLASS_DATA_CIRCUIT_ASYNC          (0x00000020)      // Data Circuit Async
#define RRIL_INFO_CLASS_DEDICATED_PACKET_ACCESS     (0x00000040)      // Dedicated packet access
#define RRIL_INFO_CLASS_DEDICATED_PAD_ACCESS        (0x00000080)      // Dedicated PAD access
#define RRIL_INFO_CLASS_ALL                         (0x000000FF)      // All Infoclasses

///////////////////////////////////////////////////////////////////////////////
// Call Forwarding defines
//
#define RRIL_CALL_FWD_UNCONDITIONAL                 (0x00000000)
#define RRIL_CALL_FWD_MOBILE_BUSY                   (0x00000001)
#define RRIL_CALL_FWD_NO_REPLY                      (0x00000002)
#define RRIL_CALL_FWD_NOT_REACHABLE                 (0x00000003)
#define RRIL_CALL_FWD_ALL_CALL_FWD                  (0x00000004)
#define RRIL_CALL_FWD_ALL_COND_CALL_FWD             (0x00000005)

///////////////////////////////////////////////////////////////////////////////
// Call Waiting Status defines
//
#define RRIL_CALL_WAIT_DISABLE                      (0x00000000)
#define RRIL_CALL_WAIT_ENABLE                       (0x00000001)
#define RRIL_CALL_WAIT_QUERY_STATUS                 (0x00000002)

///////////////////////////////////////////////////////////////////////////////
// Caller ID Restriction defines
//
#define RRIL_CLIR_NETWORK                           (0x00000000)
#define RRIL_CLIR_INVOCATION                        (0x00000001)
#define RRIL_CLIR_SUPPRESSION                       (0x00000002)

#endif // RRIL_RRIL_H

