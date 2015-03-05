////////////////////////////////////////////////////////////////////////////
// nd_structs.h
//
// Copyright 2005-2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//      Defines blobs for use in Android to support expected return structures
//      of pointer arrays to valid data
//
/////////////////////////////////////////////////////////////////////////////

#ifndef ND_STRUCTS_H
#define ND_STRUCTS_H

#include "rril.h"

//
// Struct for reporting Current Call List to Android
//
typedef struct
{
    RIL_Call*           pCallPointers      [RRIL_MAX_CALL_ID_COUNT];
    RIL_Call            pCallData          [RRIL_MAX_CALL_ID_COUNT];
    char                pCallNumberBuffers [RRIL_MAX_CALL_ID_COUNT][MAX_BUFFER_SIZE];
    char                pCallNameBuffers   [RRIL_MAX_CALL_ID_COUNT][MAX_BUFFER_SIZE];
} S_ND_CALL_LIST_DATA, *P_ND_CALL_LIST_DATA;


//
// Struct for reporting Current Call Forwarding to Android
//
typedef struct
{
    RIL_CallForwardInfo* pCallFwdPointers[RIL_MAX_CALLFWD_ENTRIES];
    RIL_CallForwardInfo  pCallFwdData[RIL_MAX_CALLFWD_ENTRIES];
    char                 pCallFwdBuffers[RIL_MAX_CALLFWD_ENTRIES][MAX_BUFFER_SIZE];
} S_ND_CALLFWD_DATA, *P_ND_CALLFWD_DATA;

//
// Structs for getting the available operator list to Android
//
typedef struct
{
    char* pszOpInfoLong;
    char* pszOpInfoShort;
    char* pszOpInfoNumeric;
    char* pszOpInfoStatus;
} S_ND_OPINFO_PTRS, *P_ND_OPINFO_PTRS;

typedef struct
{
    char szOpInfoLong[MAX_BUFFER_SIZE];
    char szOpInfoShort[MAX_BUFFER_SIZE];
    char szOpInfoNumeric[MAX_BUFFER_SIZE];
    char szOpInfoStatus[MAX_BUFFER_SIZE];
} S_ND_OPINFO_DATA, *P_ND_OPINFO_DATA;


#define RIL_MAX_BROADCASTSMSCONFIGINFO_ENTRIES     10
//
// Struct for reporting Broadcast SMS config to Android
//
typedef struct
{
    RIL_GSM_BroadcastSmsConfigInfo* pBroadcastSmsConfigInfoPointers[RIL_MAX_BROADCASTSMSCONFIGINFO_ENTRIES];
    RIL_GSM_BroadcastSmsConfigInfo BroadcastSmsConfigInfoData[RIL_MAX_BROADCASTSMSCONFIGINFO_ENTRIES];
} S_ND_BROADCASTSMSCONFIGINFO_DATA, *P_ND_BROADCASTSMSCONFIGINFO_DATA;

typedef struct
{
    RIL_SMS_Response smsRsp;
    char szAckPDU[160];
} S_ND_SEND_MSG, *P_ND_SEND_MSG;

struct PdpData
{
    char* szRadioTechnology;
    char* szRILDataProfile;
    char* szApn;
    char* szUserName;
    char* szPassword;
    char* szPAPCHAP;
    char* szPDPType;
};

//
// Struct for reporting Setup Data Call to Android
//
typedef struct
{
    RIL_Data_Call_Response_v6   sPDPData;
    char                        szPdpType[MAX_BUFFER_SIZE];
    char                        szNetworkInterfaceName[MAX_BUFFER_SIZE];
    char                        szIPAddress[MAX_BUFFER_SIZE];
    char                        szDNS[MAX_BUFFER_SIZE];
    char                        szGateway[MAX_BUFFER_SIZE];
} S_ND_SETUP_DATA_CALL, *P_ND_SETUP_DATA_CALL;

//
// Struct for reporting PDP Context List to Android
//
typedef struct
{
    RIL_Data_Call_Response_v6   pPDPData[MAX_PDP_CONTEXTS];
    char                        pTypeBuffers[MAX_PDP_CONTEXTS][MAX_BUFFER_SIZE];
    char                        pIfnameBuffers[MAX_PDP_CONTEXTS][MAX_BUFFER_SIZE];
    char                        pAddressBuffers[MAX_PDP_CONTEXTS][MAX_BUFFER_SIZE];
    char                        pDnsesBuffers[MAX_PDP_CONTEXTS][MAX_BUFFER_SIZE];
    char                        pGatewaysBuffers[MAX_PDP_CONTEXTS][MAX_BUFFER_SIZE];
} S_ND_PDP_CONTEXT_DATA, *P_ND_PDP_CONTEXT_DATA;

//
// Struct for reporting Neighboring Cell List to Android
//
#define RRIL_MAX_CELL_ID_COUNT                      (60)
#define CELL_ID_ARRAY_LENGTH                        (9)

typedef struct
{
    RIL_NeighboringCell*     pnCellPointers      [RRIL_MAX_CELL_ID_COUNT];
    RIL_NeighboringCell      pnCellData          [RRIL_MAX_CELL_ID_COUNT];
    char                     pnCellCIDBuffers    [RRIL_MAX_CELL_ID_COUNT][CELL_ID_ARRAY_LENGTH];
} S_ND_N_CELL_DATA, *P_ND_N_CELL_DATA;

typedef struct
{
    RIL_CellInfo      pnCellData[RRIL_MAX_CELL_ID_COUNT];
} S_ND_N_CELL_INFO_DATA, *P_ND_N_CELL_INFO_DATA;

#define REG_STATUS_LENGTH 8

typedef struct
{
    char* pszStat;
    char* pszLAC;
    char* pszCID;
    char* pszNetworkType;
    char* pszBaseStationID;
    char* pszBaseStationLat;
    char* pszBaseStationLon;
    char* pszConcurrentServices;
    char* pszSystemID;
    char* pszNetworkID;
    char* pszTSB58;
    char* pszPRL;
    char* pszDefaultRoaming;
    char* pszReasonDenied;
    char* pszPrimaryScramblingCode;
} S_ND_REG_STATUS_POINTERS, *P_ND_REG_STATUS_POINTERS;

typedef struct
{
    S_ND_REG_STATUS_POINTERS sStatusPointers;
    char szStat[REG_STATUS_LENGTH];
    char szLAC[REG_STATUS_LENGTH];
    char szCID[REG_STATUS_LENGTH];
    char szNetworkType[REG_STATUS_LENGTH];
    char szBaseStationID[REG_STATUS_LENGTH];
    char szBaseStationLat[REG_STATUS_LENGTH];
    char szBaseStationLon[REG_STATUS_LENGTH];
    char szConcurrentServices[REG_STATUS_LENGTH];
    char szSystemID[REG_STATUS_LENGTH];
    char szNetworkID[REG_STATUS_LENGTH];
    char szTSB58[REG_STATUS_LENGTH];
    char szPRL[REG_STATUS_LENGTH];
    char szDefaultRoaming[REG_STATUS_LENGTH];
    char szReasonDenied[REG_STATUS_LENGTH];
    char szPrimaryScramblingCode[REG_STATUS_LENGTH];
} S_ND_REG_STATUS, *P_ND_REG_STATUS;

typedef struct
{
    char* pszStat;
    char* pszLAC;
    char* pszCID;
    char* pszNetworkType;
    char* pszReasonDenied;
    char* pszNumDataCalls;
} S_ND_GPRS_REG_STATUS_POINTERS, *P_ND_GPRS_REG_STATUS_POINTERS;

typedef struct
{
    S_ND_GPRS_REG_STATUS_POINTERS sStatusPointers;
    char szStat[REG_STATUS_LENGTH];
    char szLAC[REG_STATUS_LENGTH];
    char szCID[REG_STATUS_LENGTH];
    char szNetworkType[REG_STATUS_LENGTH];
    char szReasonDenied[REG_STATUS_LENGTH];
    char szNumDataCalls[REG_STATUS_LENGTH];
} S_ND_GPRS_REG_STATUS, *P_ND_GPRS_REG_STATUS;


typedef struct
{
    char* pszOpNameLong;
    char* pszOpNameShort;
    char* pszOpNameNumeric;
} S_ND_OP_NAME_POINTERS, *P_ND_OP_NAME_POINTERS;

typedef struct
{
    S_ND_OP_NAME_POINTERS sOpNamePtrs;
    char szOpNameLong[MAX_BUFFER_SIZE];
    char szOpNameShort[MAX_BUFFER_SIZE];
    char szOpNameNumeric[MAX_BUFFER_SIZE];
} S_ND_OP_NAMES, *P_ND_OP_NAMES;

typedef struct
{
    char* pszType;
    char* pszMessage;
} S_ND_USSD_POINTERS, *P_ND_USSD_POINTERS;

typedef struct
{
    S_ND_USSD_POINTERS sStatusPointers;
    char szType[MAX_BUFFER_SIZE];
    char szMessage[MAX_BUFFER_SIZE];
} S_ND_USSD_STATUS, *P_ND_USSD_STATUS;

const UINT32 MAX_ATR_SIZE = 80;

typedef struct
{
    char* pszATR;
} S_ND_GET_ATR_POINTER, *P_ND_GET_ATR_POINTER;

typedef struct
{
    S_ND_GET_ATR_POINTER sResponsePointer;
    char szATR[MAX_ATR_SIZE];
} S_ND_GET_ATR, *P_ND_GET_ATR;

enum PDP_TYPE
{
    PDP_TYPE_IPV4,
    PDP_TYPE_IPV6,
    PDP_TYPE_IPV4V6,
};

typedef struct
{
    int nUsed;
    int nTotal;
} S_ND_SIM_SMS_STORAGE, *P_ND_SIM_SMS_STORAGE;

const UINT32 MAX_TEMP_SIZE = 50;

//
// Structs for reporting thermal sensor temperatures to Android
//
typedef struct
{
    char* pszTemperature;
}  S_ND_THERMAL_SENSOR_VALUE_PTR, *P_ND_THERMAL_SENSOR_VALUE_PTR;

typedef struct
{
    S_ND_THERMAL_SENSOR_VALUE_PTR sResponsePointer;
    char pszTemperature[MAX_TEMP_SIZE];
} S_ND_THERMAL_SENSOR_VALUE, *P_ND_THERMAL_SENSOR_VALUE;

//
// Structs for OEM diagnostic commands
//
typedef struct
{
    char* pszGprsCellEnv;
}  S_ND_GPRS_CELL_ENV_PTR, *P_ND_GPRS_CELL_ENV_PTR;

typedef struct
{
    S_ND_GPRS_CELL_ENV_PTR sResponsePointer;
    char pszGprsCellEnv[MAX_BUFFER_SIZE*2];
} S_ND_GPRS_CELL_ENV, *P_ND_GPRS_CELL_ENV;

typedef struct
{
    char* pszDebugScreen;
}  S_ND_DEBUG_SCREEN_PTR, *P_ND_DEBUG_SCREEN_PTR;

typedef struct
{
    S_ND_DEBUG_SCREEN_PTR sResponsePointer;
    char pszDebugScreen[MAX_BUFFER_SIZE*2];
} S_ND_DEBUG_SCREEN, *P_ND_DEBUG_SCREEN;

const UINT32 MAX_SMS_TRANSPORT_MODE_SIZE = 5;
const UINT32 MAX_RF_POWER_CUTBACK_TABLE_SIZE = 15;

typedef struct
{
    char* pszService;
}  S_ND_SMS_TRANSPORT_MODE_PTR, *P_ND_SMS_TRANSPORT_MODE_PTR;

typedef struct
{
    S_ND_SMS_TRANSPORT_MODE_PTR sResponsePointer;
    char szService[MAX_SMS_TRANSPORT_MODE_SIZE];
} S_ND_SMS_TRANSPORT_MODE, *P_ND_SMS_TRANSPORT_MODE;

typedef struct
{
    char* pszResponse;
}  S_ND_SEND_AT_RESPONSE_PTR, *P_ND_SEND_AT_RESPONSE_PTR;

typedef struct
{
    S_ND_SEND_AT_RESPONSE_PTR sResponsePointer;
    char szResponse[1024];
} S_ND_SEND_AT_RESPONSE, *P_ND_SEND_AT_RESPONSE;

typedef struct
{
    char* pszCid;
    char* pszPcscf1;
    char* pszPcscf2;
    char* pszIpV6Pcscf1;
    char* pszIpV6Pcscf2;
}  S_ND_GET_PCSCF_RESPONSE_PTR, *P_ND_GET_PCSCF_RESPONSE_PTR;

typedef struct
{
    S_ND_GET_PCSCF_RESPONSE_PTR sResponsePointer;
    char szCid[REG_STATUS_LENGTH];
    char szPcscf1[MAX_IPADDR_SIZE];
    char szPcscf2[MAX_IPADDR_SIZE];
    char szIpV6Pcscf1[MAX_IPADDR_SIZE];
    char szIpV6Pcscf2[MAX_IPADDR_SIZE];
} S_ND_GET_PCSCF_RESPONSE, *P_ND_GET_PCSCF_RESPONSE;

//
// Structs for retrieving the RF Power Cutback Table
//
typedef struct
{
    char* pszRFPowerCutbackTable;
}  S_ND_RF_POWER_CUTBACK_TABLE_PTR, *P_ND_RF_POWER_CUTBACK_TABLE_PTR;

typedef struct
{
    S_ND_RF_POWER_CUTBACK_TABLE_PTR sResponsePointer;
    char szRFPowerCutbackTable[MAX_RF_POWER_CUTBACK_TABLE_SIZE];
} S_ND_RF_POWER_CUTBACK_TABLE, *P_ND_RF_POWER_CUTBACK_TABLE;

typedef struct
{
    /* it will contain all the pairs (CallId, TransferResult) concatenated */
    char* pszSrvccPairs;
} S_ND_SRVCC_RESPONSE_PTR, *P_ND_SRVCC_RESPONSE_PTR;

typedef struct
{
    S_ND_SRVCC_RESPONSE_PTR sResponsePointer;
    char szSrvccPairs[MAX_BUFFER_SIZE];
} S_ND_SRVCC_RESPONSE_VALUE, *P_ND_SRVCC_RESPONSE_VALUE;

#endif
