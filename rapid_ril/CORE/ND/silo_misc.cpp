////////////////////////////////////////////////////////////////////////////
// silo_misc.cpp
//
// Copyright 2012 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides response handlers and parsing functions for the misc-related
//    RIL components.
//
/////////////////////////////////////////////////////////////////////////////

#include "types.h"
#include "rillog.h"
#include "response.h"
#include "rildmain.h"
#include "callbacks.h"
#include "silo_misc.h"
#include "extract.h"
#include "oemhookids.h"
#include "systemcaps.h"

#include <arpa/inet.h>

//
//
CSilo_MISC::CSilo_MISC(CChannel* pChannel, CSystemCapabilities* pSysCaps)
: CSilo(pChannel, pSysCaps)
{
    RIL_LOG_VERBOSE("CSilo_MISC::CSilo_MISC() - Enter\r\n");

    // AT Response Table
    static ATRSPTABLE pATRspTable[] =
    {
        { "+XDRVI: "   , (PFN_ATRSP_PARSE)&CSilo_MISC::ParseXDRVI  },
        { "+XMETRIC: "  , (PFN_ATRSP_PARSE)&CSilo_MISC::ParseXMETRIC },
        { "+XNRTCWSI: " , (PFN_ATRSP_PARSE)&CSilo_MISC::ParseXNRTCWSI },
        { ""           , (PFN_ATRSP_PARSE)&CSilo_MISC::ParseNULL }
    };

    m_pATRspTable = pATRspTable;
    RIL_LOG_VERBOSE("CSilo_MISC::CSilo_MISC() - Exit\r\n");
}


//
//
CSilo_MISC::~CSilo_MISC()
{
    RIL_LOG_VERBOSE("CSilo_MISC::~CSilo_MISC() - Enter\r\n");
    RIL_LOG_VERBOSE("CSilo_MISC::~CSilo_MISC() - Exit\r\n");
}

char* CSilo_MISC::GetBasicInitString()
{
    // misc silo-related channel basic init string
    // +XMETRIC/+XNRTCWS URCs are not enabled by default. This is done by the CWS Manager, using
    // the sendAtCommand OEM Hook
    const char szMiscBasicInitString[] = "+XGENDATA|+XPOW=0,0,0";

    if (!ConcatenateStringNullTerminate(m_szBasicInitString,
            sizeof(m_szBasicInitString), szMiscBasicInitString))
    {
        RIL_LOG_CRITICAL("CSilo_MISC::GetBasicInitString() : Failed to copy basic init "
                "string!\r\n");
        return NULL;
    }

    return m_szBasicInitString;
}

//
// Thermal sensor notification
//
BOOL CSilo_MISC::ParseXDRVI(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_MISC::ParseXDRVI() - Enter\r\n");
    BOOL   fRet = FALSE;
    sOEM_HOOK_RAW_UNSOL_THERMAL_ALARM_IND* pData = NULL;
    UINT32 nIpcChrGrp;
    UINT32 nIpcChrTempThresholdInd;
    UINT32 nXdrvResult;
    UINT32 nSensorId;
    UINT32 nTemp;

    if (NULL == pResponse)
    {
        RIL_LOG_CRITICAL("CSilo_MISC::ParseXDRVI() - pResponse is NULL.\r\n");
        goto Error;
    }

    pResponse->SetUnsolicitedFlag(TRUE);

    // Extract "<IPC_CHR_GRP>"
    if (!ExtractUInt32(rszPointer, nIpcChrGrp, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_MISC::ParseXDRVI() - Could not parse nIpcChrGrp.\r\n");
        goto Error;
    }

    // Parse <IPC_CHR_TEMP_THRESHOLD_IND>
    if (!SkipString(rszPointer, ",", rszPointer) ||
        !ExtractUInt32(rszPointer, nIpcChrTempThresholdInd, rszPointer))
    {
         RIL_LOG_CRITICAL("CSilo_MISC::ParseXDRVI() -"
                 " Unable to parse <IPC_CHR_TEMP_THRESHOLD_IND>!\r\n");
         goto Error;
    }

    // Parse <xdrv_result>
    if (!SkipString(rszPointer, ",", rszPointer) ||
        !ExtractUInt32(rszPointer, nXdrvResult, rszPointer))
    {
         RIL_LOG_CRITICAL("CSilo_MISC::ParseXDRVI() - Unable to parse <xdrv_result>!\r\n");
         goto Error;
    }

    // Parse <temp_sensor_id>
    if (!SkipString(rszPointer, ",", rszPointer) ||
        !ExtractUInt32(rszPointer, nSensorId, rszPointer))
    {
         RIL_LOG_CRITICAL("CSilo_MISC::ParseXDRVI() - Unable to parse <temp_sensor_id>!\r\n");
         goto Error;
    }

    // Parse <temp>
    if (!SkipString(rszPointer, ",", rszPointer) ||
        !ExtractUInt32(rszPointer, nTemp, rszPointer))
    {
         RIL_LOG_CRITICAL("CSilo_MISC::ParseXDRVI() - Unable to parse <temp>!\r\n");
         goto Error;
    }

    RIL_LOG_INFO("CSilo_MISC::ParseXDRVI - IPC_CHR_GRP: %u, IPC_CHR_TEMP_THRESHOLD_IND: %u,"
            " xdrv_result: %u\r\n", nIpcChrGrp, nIpcChrTempThresholdInd, nXdrvResult);
    RIL_LOG_INFO("CSilo_MISC::ParseXDRVI - temp_sensor_id: %u, temp: %u\r\n", nSensorId, nTemp);

    pData = (sOEM_HOOK_RAW_UNSOL_THERMAL_ALARM_IND*)malloc(
            sizeof(sOEM_HOOK_RAW_UNSOL_THERMAL_ALARM_IND));
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CSilo_MISC::ParseXDRVI() - Could not allocate memory for pData.\r\n");
        goto Error;
    }
    memset(pData, 0, sizeof(sOEM_HOOK_RAW_UNSOL_THERMAL_ALARM_IND));

    pData->nCommand = RIL_OEM_HOOK_RAW_UNSOL_THERMAL_ALARM_IND;
    pData->nSensorId = nSensorId;
    pData->nTemp = nTemp;

    pResponse->SetResultCode(RIL_UNSOL_OEM_HOOK_RAW);

    if (!pResponse->SetData((void*)pData, sizeof(sOEM_HOOK_RAW_UNSOL_THERMAL_ALARM_IND), FALSE))
    {
        goto Error;
    }

    fRet = TRUE;
Error:
    if (!fRet)
    {
        free(pData);
        pData = NULL;
    }

    RIL_LOG_VERBOSE("CSilo_MISC::ParseXDRVI() - Exit\r\n");
    return fRet;
}

BOOL CSilo_MISC::ParseXMETRIC(CResponse* const pResponse, const char*& rszPointer)
{
    // We have to prepend the URC name to its value
    return ParseCoexURC(pResponse, rszPointer, "+XMETRIC: ");
}


BOOL CSilo_MISC::ParseXNRTCWSI(CResponse* const pResponse, const char*& rszPointer)
{
    // We have to prepend the URC name to its value
    return ParseCoexURC(pResponse, rszPointer, "+XNRTCWSI: ");
}


//
// No complexity is needed to parse the URC received for Coexistence purpose (+XMETRIC/+XNRTCWS).
// The goal is just to extract the URC name/value and notify the up-layer (CWS Manager) about it.
//
BOOL CSilo_MISC::ParseCoexURC(CResponse* const pResponse, const char*& rszPointer,
                              const char* pUrcPrefix)
{
    RIL_LOG_VERBOSE("CSilo_MISC::ParseCoexURC() - Enter\r\n");

    BOOL fRet = FALSE;
    char szExtInfo[MAX_BUFFER_SIZE] = {0};
    sOEM_HOOK_RAW_UNSOL_COEX_INFO* pData = NULL;

    if (NULL == pResponse)
    {
        RIL_LOG_CRITICAL("CSilo_MISC::ParseCoexURC() - pResponse is NULL.\r\n");
        goto Error;
    }

    pResponse->SetUnsolicitedFlag(TRUE);

    // Performing a backup of the URC string (rszPointer) into szExtInfo, to not modify rszPointer
    ExtractUnquotedString(rszPointer, '\r', szExtInfo, MAX_BUFFER_SIZE, rszPointer);

    RIL_LOG_VERBOSE("CSilo_MISC::ParseCoexURC() - URC prefix=[%s] URC value=[%s]\r\n",
            pUrcPrefix, szExtInfo);

    // Creating the response
    pData = (sOEM_HOOK_RAW_UNSOL_COEX_INFO*)malloc(
            sizeof(sOEM_HOOK_RAW_UNSOL_COEX_INFO));
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CSilo_MISC::ParseCoexURC() -"
                " Could not allocate memory for pData.\r\n");
        goto Error;
    }

    memset(pData, 0, sizeof(sOEM_HOOK_RAW_UNSOL_COEX_INFO));

    // pData.response will contain the final result (URC name + URC value)
    // Adding the prefix of the URC (+XMETRIC or +XNRTCWS) to pData->response
    if (!CopyStringNullTerminate(pData->response, pUrcPrefix, COEX_INFO_BUFFER_SIZE))
    {
        RIL_LOG_CRITICAL("CSilo_MISC:ParseCoexURC - Copy of URC prefix failed\r\n");
        goto Error;
    }

    // Adding the value of the URC to pData->response
    if (!ConcatenateStringNullTerminate(pData->response, sizeof(pData->response), szExtInfo))
    {
        RIL_LOG_CRITICAL("CSilo_MISC::ParseCoexURC() : Failed to concat the URC "
                "prefix to its value!\r\n");
        goto Error;
    }

    RIL_LOG_INFO("CSilo_MISC::ParseCoexURC() - Final Response=[%s]\r\n", pData->response);

    pData->command = RIL_OEM_HOOK_RAW_UNSOL_COEX_INFO;
    pData->responseSize = strnlen(pData->response, COEX_INFO_BUFFER_SIZE);

    pResponse->SetResultCode(RIL_UNSOL_OEM_HOOK_RAW);

    if (!pResponse->SetData((void*)pData,
            sizeof(sOEM_HOOK_RAW_UNSOL_COEX_INFO), FALSE))
    {
        goto Error;
    }

    fRet = TRUE;
Error:
    if (!fRet)
    {
        free(pData);
        pData = NULL;
    }

    RIL_LOG_VERBOSE("CSilo_MISC::ParseCoexURC() - Exit\r\n");
    return fRet;
}
