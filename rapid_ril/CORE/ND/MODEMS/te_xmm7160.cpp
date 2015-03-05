////////////////////////////////////////////////////////////////////////////
// te_xmm7160.cpp
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Overlay for the IMC 7160 modem
//
/////////////////////////////////////////////////////////////////////////////

#include <wchar.h>
#include <math.h>

//  This is for socket-related calls.
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <linux/gsmmux.h>
#include <cutils/properties.h>

#include "types.h"
#include "nd_structs.h"
#include "util.h"
#include "extract.h"
#include "rillog.h"
#include "te.h"
#include "sync_ops.h"
#include "command.h"
#include "te_xmm7160.h"
#include "rildmain.h"
#include "callbacks.h"
#include "oemhookids.h"
#include "repository.h"
#include "reset.h"
#include "data_util.h"
#include "init7160.h"

CTE_XMM7160::CTE_XMM7160(CTE& cte)
: CTE_XMM6360(cte)
{
    m_cte.SetDefaultPDNCid(DEFAULT_PDN_CID);
}

CTE_XMM7160::~CTE_XMM7160()
{
}

CInitializer* CTE_XMM7160::GetInitializer()
{
    RIL_LOG_VERBOSE("CTE_XMM7160::GetInitializer() - Enter\r\n");
    CInitializer* pRet = NULL;

    RIL_LOG_INFO("CTE_XMM7160::GetInitializer() - Creating CInit7160 initializer\r\n");
    m_pInitializer = new CInit7160();
    if (NULL == m_pInitializer)
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::GetInitializer() - Failed to create a CInit7160 "
                "initializer!\r\n");
        goto Error;
    }

    pRet = m_pInitializer;

Error:
    RIL_LOG_VERBOSE("CTE_XMM7160::GetInitializer() - Exit\r\n");
    return pRet;
}

char* CTE_XMM7160::GetUnlockInitCommands(UINT32 uiChannelType)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::GetUnlockInitCommands() - Enter\r\n");

    char szInitCmd[MAX_BUFFER_SIZE] = {'\0'};
    char* pUnlockInitCmd = NULL;

    pUnlockInitCmd = CTE_XMM6360::GetUnlockInitCommands(uiChannelType);
    if (pUnlockInitCmd != NULL)
    {
        // copy base class init command
        CopyStringNullTerminate(szInitCmd, pUnlockInitCmd, sizeof(szInitCmd));
        free(pUnlockInitCmd);
    }

    if (RIL_CHANNEL_URC == uiChannelType)
    {
        char szConformanceProperty[PROPERTY_VALUE_MAX] = {'\0'};
        BOOL bConformance = FALSE;
        // read the conformance property
        property_get("persist.conformance", szConformanceProperty, NULL);
        bConformance =
                (0 == strncmp(szConformanceProperty, "true", PROPERTY_VALUE_MAX)) ? TRUE : FALSE;

        // read the property enabling ciphering
        CRepository repository;
        int uiEnableCipheringInd = 1;
        if (!repository.Read(g_szGroupModem, g_szEnableCipheringInd, uiEnableCipheringInd))
        {
            RIL_LOG_VERBOSE("CTE_XMM7160::GetUnlockInitCommands()- Repository read failed!\r\n");
        }

        ConcatenateStringNullTerminate(szInitCmd, sizeof(szInitCmd),
                (bConformance || (uiEnableCipheringInd != 0)) ? "|+XUCCI=1" : "|+XUCCI=0");
    }

    RIL_LOG_VERBOSE("CTE_XMM7160::GetUnlockInitCommands() - Exit\r\n");
    return strndup(szInitCmd, strlen(szInitCmd));
}

const char* CTE_XMM7160::GetRegistrationInitString()
{
    return "+CREG=3|+XREG=3|+CEREG=3";
}

const char* CTE_XMM7160::GetPsRegistrationReadString()
{
    if (m_cte.IsEPSRegistered())
    {
        return "AT+CEREG=3;+XREG=3;+CEREG?;+XREG?;+CEREG=0;+XREG=0;\r";
    }
    else
    {
        return "AT+XREG=3;+XREG?;+XREG=0\r";
    }
}

const char* CTE_XMM7160::GetScreenOnString()
{
    if (m_cte.IsSignalStrengthReportEnabled())
    {
        return "AT+CREG=3;+CGREG=0;+XREG=3;+CEREG=3;+XCESQ=1\r";
    }
    return "AT+CREG=3;+CGREG=0;+XREG=3;+CEREG=3\r";
}

const char* CTE_XMM7160::GetScreenOffString()
{
    if (m_cte.IsLocationUpdatesEnabled())
    {
        return m_cte.IsSignalStrengthReportEnabled() ?
                "AT+CGREG=1;+CEREG=1;+XREG=0;+XCESQ=0\r" : "AT+CGREG=1;+CEREG=1;+XREG=0\r";
    }
    else
    {
        return m_cte.IsSignalStrengthReportEnabled() ?
                "AT+CREG=1;+CGREG=1;+CEREG=1;+XREG=0;+XCESQ=0\r" :
                "AT+CREG=1;+CGREG=1;+CEREG=1;+XREG=0\r";
    }
}

const char* CTE_XMM7160::GetSignalStrengthReportingString()
{
    return "+XCESQ=1";
}

const char* CTE_XMM7160::GetReadCellInfoString()
{
    return "AT+XMCI=\r";
}

//
// RIL_REQUEST_SIGNAL_STRENGTH 19
//
RIL_RESULT_CODE CTE_XMM7160::CoreSignalStrength(REQUEST_DATA& rReqData,
        void* pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::CoreSignalStrength() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+XCESQ?\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CTE_XMM7160::CoreSignalStrength() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_XMM7160::ParseSignalStrength(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::ParseSignalStrength() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    RIL_SignalStrength_v6* pSigStrData = NULL;
    const char* pszRsp = rRspData.szResponse;

    pSigStrData = ParseXCESQ(pszRsp, FALSE);
    if (NULL == pSigStrData)
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::ParseSignalStrength() -"
                " Could not allocate memory for RIL_SignalStrength_v6.\r\n");
        goto Error;
    }

    rRspData.pData   = (void*)pSigStrData;
    rRspData.uiDataSize  = sizeof(RIL_SignalStrength_v6);

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTE_XMM7160::ParseSignalStrength() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_DATA_REGISTRATION_STATE 21
//
RIL_RESULT_CODE CTE_XMM7160::CoreGPRSRegistrationState(REQUEST_DATA& rReqData,
        void* pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::CoreGPRSRegistrationState() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, GetPsRegistrationReadString(),
            sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CTE_XMM7160::CoreGPRSRegistrationState() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_XMM7160::ParseGPRSRegistrationState(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::ParseGPRSRegistrationState() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char* pszRsp = rRspData.szResponse;
    const char* pszDummy;

    S_ND_GPRS_REG_STATUS psRegStatus;
    P_ND_GPRS_REG_STATUS pGPRSRegStatus = NULL;

    pGPRSRegStatus = (P_ND_GPRS_REG_STATUS)malloc(sizeof(S_ND_GPRS_REG_STATUS));
    if (NULL == pGPRSRegStatus)
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::ParseGPRSRegistrationState() -"
                " Could not allocate memory for a S_ND_GPRS_REG_STATUS struct.\r\n");
        goto Error;
    }
    memset(pGPRSRegStatus, 0, sizeof(S_ND_GPRS_REG_STATUS));

    if (FindAndSkipString(pszRsp, "+CEREG: ", pszDummy))
    {
        if (!m_cte.ParseCEREG(pszRsp, FALSE, psRegStatus))
        {
            RIL_LOG_CRITICAL("CTE_XMM7160::ParseGPRSRegistrationState() - "
                    "ERROR in parsing CEREG response.\r\n");
            goto Error;
        }

        m_cte.StoreRegistrationInfo(&psRegStatus, E_REGISTRATION_TYPE_CEREG);
    }

    if (FindAndSkipString(pszRsp, "+XREG: ", pszDummy))
    {
        if (!m_cte.ParseXREG(pszRsp, FALSE, psRegStatus))
        {
            RIL_LOG_CRITICAL("CTE_XMM7160::ParseGPRSRegistrationState() - "
                    "ERROR in parsing XREG response.\r\n");
            goto Error;
        }

        m_cte.StoreRegistrationInfo(&psRegStatus, E_REGISTRATION_TYPE_XREG);
    }

    m_cte.CopyCachedRegistrationInfo(pGPRSRegStatus, TRUE);

    rRspData.pData  = (void*)pGPRSRegStatus;
    rRspData.uiDataSize = sizeof(S_ND_GPRS_REG_STATUS_POINTERS);

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pGPRSRegStatus);
        pGPRSRegStatus = NULL;
    }

    RIL_LOG_VERBOSE("CTE_XMM7160::ParseGPRSRegistrationState() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_DEACTIVATE_DATA_CALL 41
//
RIL_RESULT_CODE CTE_XMM7160::CoreDeactivateDataCall(REQUEST_DATA& rReqData,
                                                                void* pData,
                                                                UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::CoreDeactivateDataCall() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    char* pszCid = NULL;
    UINT32 uiCID = 0;
    const LONG REASON_RADIO_OFF = 1;
    const LONG REASON_PDP_RESET = 2;
    LONG reason = 0;
    CChannel_Data* pChannelData = NULL;

    if (uiDataSize < (1 * sizeof(char *)))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CoreDeactivateDataCall() -"
                " Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CoreDeactivateDataCall() -"
                " Passed data pointer was NULL\r\n");
        goto Error;
    }

    RIL_LOG_INFO("CTE_XMM7160::CoreDeactivateDataCall() - uiDataSize=[%d]\r\n", uiDataSize);

    pszCid = ((char**)pData)[0];
    if (pszCid == NULL || '\0' == pszCid[0])
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CoreDeactivateDataCall() - pszCid was NULL\r\n");
        goto Error;
    }

    RIL_LOG_INFO("CTE_XMM7160::CoreDeactivateDataCall() - pszCid=[%s]\r\n", pszCid);

    //  Get CID as UINT32.
    if (sscanf(pszCid, "%u", &uiCID) == EOF)
    {
        // Error
        RIL_LOG_CRITICAL("CTE_XMM7160::CoreDeactivateDataCall() -  cannot convert %s to int\r\n",
                pszCid);
        goto Error;
    }

    pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
    if (NULL == pChannelData)
    {
        RIL_LOG_VERBOSE("CTE_XMM7160::CoreDeactivateDataCall() -  "
                "no channel found for CID:%u\r\n", uiCID);
        goto Error;
    }

    if ((RIL_VERSION >= 4) && (uiDataSize >= (2 * sizeof(char *))))
    {
        reason = GetDataDeactivateReason(((char**)pData)[1]);
        RIL_LOG_INFO("CTE_XMM7160::CoreDeactivateDataCall() - reason=[%ld]\r\n", reason);
    }

    if (reason == REASON_RADIO_OFF || RIL_APPSTATE_READY != GetSimAppState())
    {
        // complete the request without sending the AT command to modem.
        res = RRIL_RESULT_OK_IMMEDIATE;
        DataConfigDown(uiCID, TRUE);
    }
    else if (reason != REASON_PDP_RESET && m_cte.IsEPSRegistered() && uiCID == DEFAULT_PDN_CID)
    {
        // complete the request without sending the AT command to modem.
        res = RRIL_RESULT_OK_IMMEDIATE;
        DataConfigDown(uiCID, FALSE);
    }
    else
    {
        res = CTE_XMM6260::CoreDeactivateDataCall(rReqData, pData, uiDataSize);
    }

Error:
    RIL_LOG_VERBOSE("CTE_XMM7160::CoreDeactivateDataCall() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE 73
//
RIL_RESULT_CODE CTE_XMM7160::CoreSetPreferredNetworkType(REQUEST_DATA& rReqData,
        void* pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::CoreSetPreferredNetworkType() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    RIL_PreferredNetworkType networkType = PREF_NET_TYPE_LTE_GSM_WCDMA; //9
    UINT32 uiModeOfOperation = m_cte.GetModeOfOperation();

    RIL_LOG_INFO("CTE_XMM7160::CoreSetPreferredNetworkType() - "
            "Mode of Operation:%u.\r\n", uiModeOfOperation);

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CoreSetPreferredNetworkType() - Data "
                "pointer is NULL.\r\n");
        goto Error;
    }

    if (uiDataSize != sizeof(RIL_PreferredNetworkType*))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CoreSetPreferredNetworkType() - "
                "Invalid data size.\r\n");
        goto Error;
    }

    RIL_LOG_INFO("CTE_XMM7160::CoreSetPreferredNetworkType() - "
                 "Network type {%d} from framework.\r\n",
                 ((RIL_PreferredNetworkType*)pData)[0]);

    networkType = ((RIL_PreferredNetworkType*)pData)[0];

    // if network type already set, NO-OP this command
    if (m_currentNetworkType == networkType)
    {
        rReqData.szCmd1[0] = '\0';
        res = RRIL_RESULT_OK;
        RIL_LOG_INFO("CTE_XMM7160::CoreSetPreferredNetworkType() - "
                "Network type {%d} already set.\r\n", networkType);
        goto Error;
    }

    switch (networkType)
    {
        case PREF_NET_TYPE_GSM_WCDMA: // WCDMA Preferred
            RIL_LOG_VERBOSE("CTE_XMM7160::CoreSetPreferredNetworkType() - "
                            "WCDMA pref:XACT=3,1) - Enter\r\n");
            if (!CopyStringNullTerminate(rReqData.szCmd1, "AT+XACT=3,1\r",
                    sizeof(rReqData.szCmd1)))
            {
                RIL_LOG_CRITICAL("CTE_XMM7160::HandleNetworkType() - Can't "
                        "construct szCmd1 networkType=%d\r\n", networkType);
                break;
            }
            res = RRIL_RESULT_OK;
            break;

        case PREF_NET_TYPE_GSM_ONLY: // GSM Only
            RIL_LOG_VERBOSE("CTE_XMM7160::CoreSetPreferredNetworkType() -"
                    "GSM only:XACT=0) - Enter\r\n");
            if (!CopyStringNullTerminate(rReqData.szCmd1, "AT+XACT=0\r",
                    sizeof(rReqData.szCmd1)))
            {
                RIL_LOG_CRITICAL("CTE_XMM7160::HandleNetworkType() - Can't "
                        "construct szCmd1 networkType=%d\r\n", networkType);
                break;
            }
            res = RRIL_RESULT_OK;
            break;

        case PREF_NET_TYPE_WCDMA: // WCDMA Only
            RIL_LOG_VERBOSE("CTE_XMM7160::CoreSetPreferredNetworkType() - "
                    "WCDMA only:XACT=1) - Enter\r\n");
            if (!CopyStringNullTerminate(rReqData.szCmd1, "AT+XACT=1\r",
                    sizeof(rReqData.szCmd1)))
            {
                RIL_LOG_CRITICAL("CTE_XMM7160::HandleNetworkType() - Can't "
                        "construct szCmd1 networkType=%d\r\n", networkType);
                break;
            }
            res = RRIL_RESULT_OK;
            break;

        case PREF_NET_TYPE_LTE_ONLY: // LTE Only
            RIL_LOG_VERBOSE("CTE_XMM7160::CoreSetPreferredNetworkType() - "
                    "LTE Only:XACT=2) - Enter\r\n");
            if (!CopyStringNullTerminate(rReqData.szCmd1, "AT+XACT=2;+CEMODE=2\r",
                    sizeof(rReqData.szCmd1)))
            {
                RIL_LOG_CRITICAL("CTE_XMM7160::CoreSetPreferredNetworkType() - "
                        "Can't construct szCmd1 networkType=%d\r\n", networkType);
                goto Error;
            }
            break;

        /*
         * PREF_NET_TYPE_GSM_WCDMA_CDMA_EVDO_AUTO value is received as a result
         * of the recovery mechanism in the framework.
         */
        case PREF_NET_TYPE_LTE_GSM_WCDMA: // LTE Preferred
        case PREF_NET_TYPE_GSM_WCDMA_CDMA_EVDO_AUTO:
            RIL_LOG_VERBOSE("CTE_XMM7160::CoreSetPreferredNetworkType() - "
                    "LTE,GSM,WCDMA:XACT=6,2,1) - Enter\r\n");
            if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1),
                    "AT+XACT=6,2,1;+CEMODE=%u\r", uiModeOfOperation))
            {
                RIL_LOG_CRITICAL("CTE_XMM7160::CoreSetPreferredNetworkType() - "
                    "Can't construct szCmd1 networkType=%d\r\n", networkType);
                goto Error;
            }
            break;

        case PREF_NET_TYPE_LTE_WCDMA: // LTE Preferred
            RIL_LOG_VERBOSE("CTE_XMM7160::CoreSetPreferredNetworkType() - "
                    "LTE,WCDMA:XACT=4,2) - Enter\r\n");
            if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1),
                    "AT+XACT=4,2;+CEMODE=%u\r", uiModeOfOperation))
            {
                RIL_LOG_CRITICAL("CTE_XMM7160::HandleNetworkType() - Can't "
                        "construct szCmd1 networkType=%d\r\n", networkType);
                goto Error;
            }
            break;

        default:
            RIL_LOG_CRITICAL("CTE_XMM7160::CoreSetPreferredNetworkType() - "
                    "Undefined rat code: %d\r\n", networkType);
            res = RIL_E_MODE_NOT_SUPPORTED;
            goto Error;
    }

    //  Set the context of this command to the network type we're attempting to set
    rReqData.pContextData = (void*)networkType;  // Store this as an int.

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTE_XMM7160::CoreSetPreferredNetworkType() - "
                    "Exit:%d\r\n", res);
    return res;
}

// RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE 74
//
RIL_RESULT_CODE CTE_XMM7160::CoreGetPreferredNetworkType(REQUEST_DATA& rReqData,
        void* pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::CoreGetPreferredNetworkType() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+XACT?\r",
            sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CTE_XMM7160::CoreGetPreferredNetworkType() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_XMM7160::ParseGetPreferredNetworkType(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::ParseGetPreferredNetworkType() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char* pszRsp = rRspData.szResponse;

    UINT32 rat = 0;
    UINT32 pref = 0;

    int* pRat = (int*)malloc(sizeof(int));
    if (NULL == pRat)
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::ParseGetPreferredNetworkType() - Could "
                "not allocate memory for response.\r\n");
        goto Error;
    }

    // Skip "<prefix>"
    if (!SkipRspStart(pszRsp, m_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::ParseGetPreferredNetworkType() - Could "
                "not skip response prefix.\r\n");
        goto Error;
    }

    // Skip "+XACT: "
    if (!SkipString(pszRsp, "+XACT: ", pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::ParseGetPreferredNetworkType() - Could "
                "not skip \"+XACT: \".\r\n");
        goto Error;
    }

    if (!ExtractUInt32(pszRsp, rat, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::ParseGetPreferredNetworkType() - Could "
                "not extract rat value.\r\n");
        goto Error;
    }

    if (FindAndSkipString(pszRsp, ",", pszRsp))
    {
        if (!ExtractUInt32(pszRsp, pref, pszRsp))
        {
            RIL_LOG_CRITICAL("CTE_XMM7160::ParseGetPreferredNetworkType() - "
                    "Could not find and skip pref value even though it was expected.\r\n");
            goto Error;
        }
    }

    switch (rat)
    {
        case 0:     // GSM Only
        {
            pRat[0] = PREF_NET_TYPE_GSM_ONLY;
            m_currentNetworkType = PREF_NET_TYPE_GSM_ONLY;
            break;
        }

        case 1:     // WCDMA Only
        {
            pRat[0] = PREF_NET_TYPE_WCDMA;
            m_currentNetworkType = PREF_NET_TYPE_WCDMA;
            break;
        }

        case 2:     // LTE only
        {
            pRat[0] = PREF_NET_TYPE_LTE_ONLY;
            m_currentNetworkType = PREF_NET_TYPE_LTE_ONLY;
            break;
        }

        case 3:     // WCDMA preferred
        {
            pRat[0] = PREF_NET_TYPE_GSM_WCDMA;
            m_currentNetworkType = PREF_NET_TYPE_GSM_WCDMA;
            break;
        }

        case 4:     // LTE/WCDMA, LTE preferred
        {
            pRat[0] = PREF_NET_TYPE_LTE_WCDMA;
            m_currentNetworkType = PREF_NET_TYPE_LTE_WCDMA;
            break;
        }

        case 6:     // LTE/WCDMA/GSM, LTE preferred
        {
            pRat[0] = PREF_NET_TYPE_LTE_GSM_WCDMA;
            m_currentNetworkType = PREF_NET_TYPE_LTE_GSM_WCDMA;
            break;
        }

        default:
        {
            RIL_LOG_CRITICAL("CTE_XMM7160::ParseGetPreferredNetworkType() - "
                    "Unexpected rat found: %d. Failing out.\r\n", rat);
            goto Error;
        }
    }

    rRspData.pData  = (void*)pRat;
    rRspData.uiDataSize = sizeof(int*);

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pRat);
        pRat = NULL;
    }

    RIL_LOG_VERBOSE("CTE_XMM7160::ParseGetPreferredNetworkType() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GET_NEIGHBORING_CELL_IDS 75
//
RIL_RESULT_CODE CTE_XMM7160::CoreGetNeighboringCellIDs(REQUEST_DATA& rReqData,
        void* pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::CoreGetNeighboringCellIDs() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+XMCI=\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CTE_XMM7160::CoreGetNeighboringCellIDs() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_XMM7160::ParseGetNeighboringCellIDs(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::ParseGetNeighboringCellIDs() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    P_ND_N_CELL_DATA pNeighboringCellData = NULL;
    P_ND_N_CELL_INFO_DATA pCellInfoData = NULL;
    int nCellInfos = 0;
    int nNeighboringCellInfos = 0;

    pCellInfoData = ParseXMCI(rRspData, nCellInfos);

    if (NULL != pCellInfoData && nCellInfos > 0)
    {
        pNeighboringCellData = (P_ND_N_CELL_DATA)malloc(sizeof(S_ND_N_CELL_DATA));
        if (NULL == pNeighboringCellData)
        {
            RIL_LOG_CRITICAL("CTE_XMM7160::ParseGetNeighboringCellIDs() -"
                    " Could not allocate memory for a S_ND_N_CELL_DATA struct.\r\n");
            goto Error;
        }
        memset(pNeighboringCellData, 0, sizeof(S_ND_N_CELL_DATA));

        for (int i = 0; i < nCellInfos; i++)
        {
            RIL_CellInfo info = pCellInfoData->pnCellData[i];
            if (!info.registered)
            {
                switch (info.cellInfoType)
                {
                    case RIL_CELL_INFO_TYPE_GSM:
                        pNeighboringCellData->pnCellData[nNeighboringCellInfos].cid
                                = pNeighboringCellData->pnCellCIDBuffers[nNeighboringCellInfos];

                        // cid = upper 16 bits (LAC), lower 16 bits (CID)
                        snprintf(pNeighboringCellData->pnCellCIDBuffers[nNeighboringCellInfos],
                                CELL_ID_ARRAY_LENGTH, "%04X%04X",
                                info.CellInfo.gsm.cellIdentityGsm.lac,
                                info.CellInfo.gsm.cellIdentityGsm.cid);

                        pNeighboringCellData->pnCellData[nNeighboringCellInfos].rssi
                                = info.CellInfo.gsm.signalStrengthGsm.signalStrength;

                        pNeighboringCellData->pnCellPointers[nNeighboringCellInfos]
                                = &(pNeighboringCellData->pnCellData[nNeighboringCellInfos]);
                        nNeighboringCellInfos++;
                    break;

                    case RIL_CELL_INFO_TYPE_WCDMA:
                        pNeighboringCellData->pnCellData[nNeighboringCellInfos].cid
                                = pNeighboringCellData->pnCellCIDBuffers[nNeighboringCellInfos];

                        // cid = upper 16 bits (LAC), lower 16 bits (CID)
                        snprintf(pNeighboringCellData->pnCellCIDBuffers[nNeighboringCellInfos],
                                CELL_ID_ARRAY_LENGTH, "%04X%04X",
                                info.CellInfo.wcdma.cellIdentityWcdma.lac,
                                info.CellInfo.wcdma.cellIdentityWcdma.cid);

                        pNeighboringCellData->pnCellData[nNeighboringCellInfos].rssi
                                = info.CellInfo.wcdma.signalStrengthWcdma.signalStrength;

                        pNeighboringCellData->pnCellPointers[nNeighboringCellInfos]
                                = &(pNeighboringCellData->pnCellData[nNeighboringCellInfos]);
                        nNeighboringCellInfos++;
                    break;

                    case RIL_CELL_INFO_TYPE_LTE:
                        pNeighboringCellData->pnCellData[nNeighboringCellInfos].cid
                                = pNeighboringCellData->pnCellCIDBuffers[nNeighboringCellInfos];

                        // cid = upper 16 bits (TAC), lower 16 bits (CID)
                        snprintf(pNeighboringCellData->pnCellCIDBuffers[nNeighboringCellInfos],
                                CELL_ID_ARRAY_LENGTH, "%04X%04X",
                                info.CellInfo.lte.cellIdentityLte.tac,
                                info.CellInfo.lte.cellIdentityLte.ci);

                        pNeighboringCellData->pnCellData[nNeighboringCellInfos].rssi = 0;

                        pNeighboringCellData->pnCellPointers[nNeighboringCellInfos]
                                = &(pNeighboringCellData->pnCellData[nNeighboringCellInfos]);
                        nNeighboringCellInfos++;
                    break;

                    default:
                    break;
                }
            }
        }
    }

    res = RRIL_RESULT_OK;
Error:
    if (nNeighboringCellInfos > 0 && RRIL_RESULT_OK == res)
    {
        rRspData.pData  = (void*)pNeighboringCellData;
        rRspData.uiDataSize = nNeighboringCellInfos * sizeof(RIL_NeighboringCell*);
    }
    else
    {
        rRspData.pData  = NULL;
        rRspData.uiDataSize = 0;
        free(pNeighboringCellData);
        pNeighboringCellData = NULL;
    }

    free(pCellInfoData);
    pCellInfoData = NULL;

    RIL_LOG_VERBOSE("CTE_XMM7160::ParseGetNeighboringCellIDs() - Exit\r\n");
    return res;
}

BOOL CTE_XMM7160::IMSRegister(REQUEST_DATA& rReqData, void* pData,
        UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::IMSRegister() - Enter\r\n");

    BOOL bRet = FALSE;

    if ((NULL == pData) || (sizeof(int) != uiDataSize))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::IMSRegister() - Invalid input data\r\n");
        return bRet;
    }

    int* pService = (int*)pData;

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1),
                "AT+XIREG=%d\r", *pService))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::IMSRegister() - Can't construct szCmd1.\r\n");
        return bRet;
    }

    int temp = 0;
    const int DEFAULT_XIREG_TIMEOUT = 180000;
    CRepository repository;

    if (repository.Read(g_szGroupOtherTimeouts, g_szTimeoutWaitForXIREG, temp))
    {
        rReqData.uiTimeout = temp;
    }
    else
    {
        rReqData.uiTimeout = DEFAULT_XIREG_TIMEOUT;
    }

    bRet = TRUE;

    return bRet;
}

RIL_RESULT_CODE CTE_XMM7160::ParseIMSRegister(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::ParseIMSRegister() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_OK;
    RIL_LOG_VERBOSE("CTE_XMM7160::ParseIMSRegister() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_XMM7160::CreateIMSRegistrationReq(REQUEST_DATA& rReqData,
        const char** pszRequest,
        const UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::CreateIMSRegistrationReq() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (pszRequest == NULL || '\0' == pszRequest[1])
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CreateIMSRegistrationReq() - pszRequest was empty\r\n");
        goto Error;
    }

    if (uiDataSize < (2 * sizeof(char*)))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CreateIMSRegistrationReq() :"
                " received_size < required_size\r\n");
        goto Error;
    }

    int service;
    if (sscanf(pszRequest[1], "%d", &service) == EOF)
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CreateIMSRegistrationReq() -"
                " cannot convert %s to int\r\n", pszRequest);
        goto Error;
    }

    if ((service < 0) || (service > 1))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CreateIMSRegistrationReq() -"
                " service %s out of boundaries\r\n", service);
        goto Error;
    }

    RIL_LOG_INFO("CTE_XMM7160::CreateIMSRegistrationReq() - service=[%d]\r\n", service);

    if (!IMSRegister(rReqData, &service, sizeof(int)))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CreateIMSRegistrationReq() - Can't construct szCmd1.\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;
Error:
    RIL_LOG_VERBOSE("CTE_XMM7160::CreateIMSRegistrationReq() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_XMM7160::CreateIMSConfigReq(REQUEST_DATA& rReqData,
        const char** pszRequest,
        const int nNumStrings)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::CreateIMSConfigReq() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    char szXicfgCmd[MAX_BUFFER_SIZE] = {'\0'};
    int xicfgParams[XICFG_N_PARAMS] = {0};

    if (pszRequest == NULL)
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CreateIMSConfigReq() - pszRequest was empty\r\n");
        return res;
    }

    // There should be XICFG_N_PARAMS parameters and the request ID
    if (nNumStrings < (XICFG_N_PARAMS + 1))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CreateIMSConfigReq() :"
                " received_size < required_size\r\n");
        return res;
    }

    xicfgParams[0] = E_XICFG_IMS_APN;
    xicfgParams[1] = E_XICFG_PCSCF_ADDRESS;
    xicfgParams[2] = E_XICFG_PCSCF_PORT;
    xicfgParams[3] = E_XICFG_IMS_AUTH_MODE;
    xicfgParams[4] = E_XICFG_PHONE_CONTEXT;
    xicfgParams[5] = E_XICFG_LOCAL_BREAKOUT;
    xicfgParams[6] = E_XICFG_XCAP_APN;
    xicfgParams[7] = E_XICFG_XCAP_ROOT_URI;
    xicfgParams[8] = E_XICFG_XCAP_USER_NAME;
    xicfgParams[9] = E_XICFG_XCAP_USER_PASSWORD;

    char szTemp1Xicfg[MAX_BUFFER_SIZE] = {'\0'};
    char szTemp2Xicfg[MAX_BUFFER_SIZE] = {'\0'};
    int nParams = 0;

    for (int i = 1; i <= XICFG_N_PARAMS; i++)
    {
        if ((pszRequest[i] != NULL) && (0 != strncmp(pszRequest[i], "default", 7)))
        {   // The XICFG parameter is a numeric hence "" not required.
            if (xicfgParams[i - 1] == E_XICFG_PCSCF_PORT ||
                xicfgParams[i - 1] == E_XICFG_IMS_AUTH_MODE ||
                xicfgParams[i - 1] == E_XICFG_LOCAL_BREAKOUT)
            {
                if (!PrintStringNullTerminate(szTemp1Xicfg,
                                             MAX_BUFFER_SIZE,",%d,%s",
                                             xicfgParams[i - 1], pszRequest[i]))
                {
                    RIL_LOG_CRITICAL("CTE_XMM7160::CreateIMSConfigReq() - Can't add %s.\r\n",
                                     pszRequest[i]);
                    goto Error;
                }
            }
            else
            {   // The XICFG parameter is a string hence "" required.
                if (!PrintStringNullTerminate(szTemp1Xicfg,
                                              MAX_BUFFER_SIZE,",%d,\"%s\"",
                                              xicfgParams[i - 1], pszRequest[i]))
                {
                    RIL_LOG_CRITICAL("CTE_XMM7160::CreateIMSConfigReq() - Can't add %s.\r\n",
                                     pszRequest[i]);
                    goto Error;
                }
            }
            if (!ConcatenateStringNullTerminate(szTemp2Xicfg, sizeof(szTemp2Xicfg), szTemp1Xicfg))
            {
                RIL_LOG_CRITICAL("CTE_XMM7160::CreateIMSConfigReq() - Can't add %s.\r\n",
                                 szTemp1Xicfg);
                goto Error;
            }
            nParams++;
        }
    }

    if (nParams == 0)
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CreateIMSConfigReq() - nParams=0\r\n");
        goto Error;
    }

    if (!ConcatenateStringNullTerminate(szTemp2Xicfg, sizeof(szTemp2Xicfg), "\r"))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CreateIMSConfigReq() - Can't add %s.\r\n",
                         "\r");
        goto Error;
    }

    if (!PrintStringNullTerminate(szXicfgCmd,
            MAX_BUFFER_SIZE,"AT+XICFG=%d,%d", XICFG_SET, nParams))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CreateIMSConfigReq() - Can't construct szCmd1.\r\n");
        goto Error;
    }

    if (!ConcatenateStringNullTerminate(szXicfgCmd, sizeof(szXicfgCmd), szTemp2Xicfg))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CreateIMSConfigReq() - Can't construct szCmd1.\r\n");
        goto Error;
    }

    RIL_LOG_INFO("CTE_XMM7160::CreateIMSConfigReq() - IMS_APN=[%s]\r\n",
                 szXicfgCmd);

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), szXicfgCmd))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CreateIMSConfigReq() - Can't construct szCmd1.\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;
Error:
    RIL_LOG_VERBOSE("CTE_XMM7160::CreateIMSConfigReq() - Exit\r\n");
    return res;
}

BOOL CTE_XMM7160::QueryIpAndDns(REQUEST_DATA& rReqData, UINT32 uiCID)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::QueryIpAndDns() - Enter\r\n");
    BOOL bRet = FALSE;

    if (uiCID != 0)
    {
        if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1),
                "AT+CGCONTRDP=%u\r", uiCID))
        {
            bRet = TRUE;
        }
    }

    RIL_LOG_VERBOSE("CTE_XMM7160::QueryIpAndDns() - Exit\r\n");
    return bRet;
}

RIL_RESULT_CODE CTE_XMM7160::ParseQueryIpAndDns(RESPONSE_DATA& rRspData)
{
    return ParseReadContextParams(rRspData);
}

RIL_RESULT_CODE CTE_XMM7160::HandleSetupDefaultPDN(RIL_Token rilToken,
        CChannel_Data* pChannelData)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::HandleSetupDefaultPDN() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    char* szModemResourceName = {'\0'};
    int muxControlChannel = -1;
    int hsiChannel = pChannelData->GetHSIChannel();
    int ipcDataChannelMin = 0;
    UINT32 uiRilChannel = pChannelData->GetRilChannel();
    REQUEST_DATA reqData;
    S_SETUP_DATA_CALL_CONTEXT_DATA* pDataCallContextData = NULL;
    CCommand* pCmd = NULL;

    pDataCallContextData =
            (S_SETUP_DATA_CALL_CONTEXT_DATA*)malloc(sizeof(S_SETUP_DATA_CALL_CONTEXT_DATA));
    if (NULL == pDataCallContextData)
    {
        goto Error;
    }

    pDataCallContextData->uiCID = pChannelData->GetContextID();

    // Get the mux channel id corresponding to the control of the data channel
    switch (uiRilChannel)
    {
        case RIL_CHANNEL_DATA1:
            sscanf(g_szDataPort1, "/dev/gsmtty%d", &muxControlChannel);
            break;
        case RIL_CHANNEL_DATA2:
            sscanf(g_szDataPort2, "/dev/gsmtty%d", &muxControlChannel);
            break;
        case RIL_CHANNEL_DATA3:
            sscanf(g_szDataPort3, "/dev/gsmtty%d", &muxControlChannel);
            break;
        case RIL_CHANNEL_DATA4:
            sscanf(g_szDataPort4, "/dev/gsmtty%d", &muxControlChannel);
            break;
        case RIL_CHANNEL_DATA5:
            sscanf(g_szDataPort5, "/dev/gsmtty%d", &muxControlChannel);
            break;
        default:
            RIL_LOG_CRITICAL("CTE_XMM7160::HandleSetupDefaultPDN() - Unknown mux channel"
                    "for RIL Channel [%u] \r\n", uiRilChannel);
            goto Error;
    }

    szModemResourceName = pChannelData->GetModemResourceName();
    ipcDataChannelMin = pChannelData->GetIpcDataChannelMin();

    if (ipcDataChannelMin > hsiChannel || RIL_MAX_NUM_IPC_CHANNEL <= hsiChannel )
    {
       RIL_LOG_CRITICAL("CTE_XMM7160::HandleSetupDefaultPDN() - Unknown HSI Channel [%d] \r\n",
                hsiChannel);
       goto Error;
    }

    memset(&reqData, 0, sizeof(REQUEST_DATA));

    if (pChannelData->IsRoutingEnabled())
    {
        /*
         * Default PDN is already active and also routing is enabled. Don't send any command to
         * modem. Just bring up the interface.
         */
    }
    else
    {
        if (!PrintStringNullTerminate(reqData.szCmd1, sizeof(reqData.szCmd1),
                "AT+XDATACHANNEL=1,1,\"/mux/%d\",\"/%s/%d\",0,%d;+CGDATA=\"M-RAW_IP\",%d\r",
                muxControlChannel, szModemResourceName, hsiChannel,
                pDataCallContextData->uiCID, pDataCallContextData->uiCID))
        {
            RIL_LOG_CRITICAL("CTE_XMM7160::HandleSetupDefaultPDN() - cannot create XDATACHANNEL"
                    "command\r\n");
            goto Error;
        }
    }

    pCmd = new CCommand(uiRilChannel, rilToken, REQ_ID_NONE, reqData,
            &CTE::ParseSetupDefaultPDN, &CTE::PostSetupDefaultPDN);

    if (pCmd)
    {
        pCmd->SetContextData(pDataCallContextData);
        pCmd->SetContextDataSize(sizeof(S_SETUP_DATA_CALL_CONTEXT_DATA));

        if (!CCommand::AddCmdToQueue(pCmd))
        {
            RIL_LOG_CRITICAL("CTE_XMM7160::HandleSetupDefaultPDN() - "
                    "Unable to add command to queue\r\n");
            delete pCmd;
            pCmd = NULL;
            goto Error;
        }
    }
    else
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::HandleSetupDefaultPDN() -"
                " Unable to allocate memory for command\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pDataCallContextData);
        pDataCallContextData = NULL;
    }

    RIL_LOG_VERBOSE("CTE_XMM7160::HandleSetupDefaultPDN() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_XMM7160::ParseSetupDefaultPDN(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::ParseSetupDefaultPDN() - Enter\r\n");

    RIL_LOG_VERBOSE("CTE_XMM7160::ParseSetupDefaultPDN() - Exit\r\n");
    return RRIL_RESULT_OK;
}

void CTE_XMM7160::PostSetupDefaultPDN(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::PostSetupDefaultPDN() Enter\r\n");

    BOOL bSuccess = FALSE;
    S_SETUP_DATA_CALL_CONTEXT_DATA* pDataCallContextData = NULL;
    UINT32 uiCID = 0;
    CChannel_Data* pChannelData = NULL;

    if (NULL == rData.pContextData ||
            sizeof(S_SETUP_DATA_CALL_CONTEXT_DATA) != rData.uiContextDataSize)
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::PostSetupDefaultPDN() - Invalid context data\r\n");
        goto Error;
    }

    pDataCallContextData = (S_SETUP_DATA_CALL_CONTEXT_DATA*)rData.pContextData;
    uiCID = pDataCallContextData->uiCID;

    if (RIL_E_SUCCESS != rData.uiResultCode)
    {
        RIL_LOG_INFO("CTE_XMM7160::PostSetupDefaultPDN() - Failure\r\n");
        goto Error;
    }

    RIL_LOG_INFO("CTE_XMM7160::PostSetupDefaultPDN() - CID=%d\r\n", uiCID);

    pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
    if (NULL == pChannelData)
    {
        RIL_LOG_INFO("CTE_XMM7160::PostSetupDefaultPDN() -"
                " No Data Channel for CID %u.\r\n", uiCID);
        goto Error;
    }

    RIL_LOG_VERBOSE("CTE_XMM7160::PostSetupDefaultPDN() set channel data\r\n");

    pChannelData->SetDataState(E_DATA_STATE_ACTIVE);
    pChannelData->SetRoutingEnabled(TRUE);

    if (!SetupInterface(uiCID))
    {
        RIL_LOG_INFO("CTE_XMM7160::PostSetupDefaultPDN() - SetupInterface failed\r\n");
        goto Error;
    }

    bSuccess = TRUE;

Error:
    free(rData.pContextData);
    rData.pContextData = NULL;

    if (!bSuccess)
    {
        HandleSetupDataCallFailure(uiCID, rData.pRilToken, rData.uiResultCode);
    }
    else
    {
        pChannelData->IncrementRefCount();
        HandleSetupDataCallSuccess(uiCID, rData.pRilToken);
    }
}

//
//  Call this whenever data is disconnected
//
BOOL CTE_XMM7160::DataConfigDown(UINT32 uiCID, BOOL bForceCleanup)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::DataConfigDown() - Enter\r\n");

    //  First check to see if uiCID is valid
    if (uiCID > MAX_PDP_CONTEXTS || uiCID == 0)
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::DataConfigDown() - Invalid CID = [%u]\r\n", uiCID);
        return FALSE;
    }

    CChannel_Data* pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
    if (NULL == pChannelData)
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::DataConfigDown() -"
                " Invalid CID=[%u], no data channel found!\r\n", uiCID);
        return FALSE;
    }

    /*
     * Bring down the interface and bring up the interface again if the data down is for
     * default PDN(EPS registered) and not force cleanup.
     *
     * This is done to make sure packets are flushed out if the data is deactivated
     * in LTE.
     */
    BOOL bStopInterface = !m_cte.IsEPSRegistered() || uiCID != DEFAULT_PDN_CID || bForceCleanup;
    CMutex* pDataChannelRefCountMutex = m_cte.GetDataChannelRefCountMutex();
    CMutex::Lock(pDataChannelRefCountMutex);
    pChannelData->DecrementRefCount();
    if (pChannelData->GetRefCount() == 0)
    {
        pChannelData->RemoveInterface(!bStopInterface);
    }
    CMutex::Unlock(pDataChannelRefCountMutex);

    if (bStopInterface)
    {
        pChannelData->ResetDataCallInfo();
    }

    RIL_LOG_VERBOSE("CTE_XMM7160::DataConfigDown() EXIT\r\n");
    return TRUE;
}

//
// RIL_REQUEST_SET_BAND_MODE 65
//
RIL_RESULT_CODE CTE_XMM7160::CoreSetBandMode(REQUEST_DATA& rReqData,
                                                         void* pData,
                                                         UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::CoreSetBandMode() - Enter\r\n");
    // TODO: Change to +XACT usage when the modem is ready
    return CTE_XMM6260::CoreSetBandMode(rReqData, pData, uiDataSize);
}

RIL_RESULT_CODE CTE_XMM7160::ParseSetBandMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::ParseSetBandMode() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_OK;
    RIL_LOG_VERBOSE("CTE_XMM7160::ParseSetBandMode() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_XMM7160::CreateSetDefaultApnReq(REQUEST_DATA& rReqData,
        const char** pszRequest, const int numStrings)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::CreateSetDefaultApnReq() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (numStrings != 3)
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CreateSetDefaultApnReq() :"
                " received_size != required_size\r\n");
        return res;
    }

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1),
            "AT+CGDCONT=1,\"%s\",\"%s\"\r", pszRequest[2], pszRequest[1]))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CreateSetDefaultApnReq() - "
                "Can't construct szCmd1.\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;
Error:
    RIL_LOG_VERBOSE("CTE_XMM7160::CreateSetDefaultApnReq() - Exit\r\n");
    return res;
}

RIL_SignalStrength_v6* CTE_XMM7160::ParseXCESQ(const char*& rszPointer, const BOOL bUnsolicited)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::ParseXCESQ() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    int mode = 0;
    int rxlev = 0; // received signal strength level
    int ber = 0; // channel bit error rate
    int rscp = 0; // Received signal code power
    // ratio of the received energy per PN chip to the total received power spectral density
    int ec = 0;
    int rsrq = 0; // Reference signal received quality
    int rsrp = 0; // Reference signal received power
    int rssnr = -1; // Radio signal strength Noise Ratio value
    RIL_SignalStrength_v6* pSigStrData = NULL;

    if (!bUnsolicited)
    {
        // Parse "<prefix>+XCESQ: <n>,<rxlev>,<ber>,<rscp>,<ecno>,<rsrq>,<rsrp>,<rssnr><postfix>"
        if (!SkipRspStart(rszPointer, m_szNewLine, rszPointer)
                || !SkipString(rszPointer, "+XCESQ: ", rszPointer))
        {
            RIL_LOG_CRITICAL("CTE_XMM7160::ParseUnsolicitedSignalStrength() - "
                    "Could not find AT response.\r\n");
            goto Error;
        }

        if (!ExtractInt(rszPointer, mode, rszPointer))
        {
            RIL_LOG_CRITICAL("CTE_XMM7160::ParseUnsolicitedSignalStrength() - "
                    "Could not extract <mode>\r\n");
            goto Error;
        }

        if (!SkipString(rszPointer, ",", rszPointer))
        {
            RIL_LOG_CRITICAL("CTE_XMM7160::ParseXCESQ() - Could not extract , before <rxlev>\r\n");
            goto Error;
        }
    }

    if (!ExtractInt(rszPointer, rxlev, rszPointer))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::ParseXCESQ() - Could not extract <rxlev>\r\n");
        goto Error;
    }

    if (!SkipString(rszPointer, ",", rszPointer)
            || !ExtractInt(rszPointer, ber, rszPointer))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::ParseXCESQ() - Could not extract <ber>\r\n");
        goto Error;
    }

    if (!SkipString(rszPointer, ",", rszPointer)
            || !ExtractInt(rszPointer, rscp, rszPointer))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::ParseXCESQ() - Could not extract <rscp>\r\n");
        goto Error;
    }

    // Not used
    if (!SkipString(rszPointer, ",", rszPointer)
            || !ExtractInt(rszPointer, ec, rszPointer))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::ParseXCESQ() - Could not extract <ecno>\r\n");
        goto Error;
    }

    if (!SkipString(rszPointer, ",", rszPointer)
            || !ExtractInt(rszPointer, rsrq, rszPointer))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::ParseXCESQ() - Could not extract <rsrq>\r\n");
        goto Error;
    }

    if (!SkipString(rszPointer, ",", rszPointer)
            || !ExtractInt(rszPointer, rsrp, rszPointer))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::ParseXCESQ() - Could not extract <rsrp>.\r\n");
        goto Error;
    }

    if (!SkipString(rszPointer, ",", rszPointer)
            || !ExtractInt(rszPointer, rssnr, rszPointer))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::ParseXCESQ() - "
                "Could not extract <rssnr>.\r\n");
        goto Error;
    }

    if (!bUnsolicited)
    {
        if (!FindAndSkipRspEnd(rszPointer, m_szNewLine, rszPointer))
        {
            RIL_LOG_CRITICAL("CTE_XMM7160::ParseXCESQ() -"
                    " Could not extract the response end.\r\n");
            goto Error;
        }
    }

    pSigStrData = (RIL_SignalStrength_v6*)malloc(sizeof(RIL_SignalStrength_v6));
    if (NULL == pSigStrData)
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::ParseXCESQ() -"
                " Could not allocate memory for RIL_SignalStrength_v6.\r\n");
        goto Error;
    }

    // reset to default values
    pSigStrData->GW_SignalStrength.signalStrength = -1;
    pSigStrData->GW_SignalStrength.bitErrorRate   = -1;
    pSigStrData->CDMA_SignalStrength.dbm = -1;
    pSigStrData->CDMA_SignalStrength.ecio = -1;
    pSigStrData->EVDO_SignalStrength.dbm = -1;
    pSigStrData->EVDO_SignalStrength.ecio = -1;
    pSigStrData->EVDO_SignalStrength.signalNoiseRatio = -1;
    pSigStrData->LTE_SignalStrength.signalStrength = -1;
    pSigStrData->LTE_SignalStrength.rsrp = INT_MAX;
    pSigStrData->LTE_SignalStrength.rsrq = INT_MAX;
    pSigStrData->LTE_SignalStrength.rssnr = INT_MAX;
    pSigStrData->LTE_SignalStrength.cqi = INT_MAX;

    /*
     * If the current serving cell is GERAN cell, then <rxlev> and <ber> are set to
     * valid values.
     * For <rxlev>, valid values are 0 to 63.
     * For <ber>, valid values are 0 to 7.
     * If the current service cell is not GERAN cell, then <rxlev> and <ber> are set
     * to value 99.
     *
     * If the current serving cell is UTRA cell, then <rscp> is set to valid value.
     * For <rscp>, valid values are 0 to 96.
     * If the current service cell is not UTRA cell, then <rscp> is set to value 255.
     *
     * If the current serving cell is E-UTRA cell, then <rsrq> and <rsrp> are set to
     * valid values.
     * For <rsrq>, valid values are 0 to 34.
     * For <rsrp>, valid values are 0 to 97.
     * If the current service cell is not E-UTRA cell, then <rsrq> and <rsrp> are set
     * to value 255.
     */
    if (99 != rxlev)
    {
        /*
         * As <rxlev> reported as part of XCESQ is not in line with the <rssi> reported
         * as part of AT+CSQ and also what android expects, following conversion is done.
         */
        if (rxlev <= 57)
        {
            rxlev = floor(rxlev / 2) + 2;
        }
        else
        {
            rxlev = 31;
        }

        pSigStrData->GW_SignalStrength.signalStrength = rxlev;
        pSigStrData->GW_SignalStrength.bitErrorRate   = ber;
    }
    else if (255 != rscp)
    {
        /*
         * As <rscp> reported as part of XCESQ is not in line with the <rssi> reported
         * as part of AT+CSQ and also what android expects, following conversion is done.
         */
        if (rscp <= 7)
        {
            rscp = 0;
        }
        else if (rscp <= 67)
        {
            rscp = floor((rscp - 6) / 2);
        }
        else
        {
            rscp = 31;
        }

        pSigStrData->GW_SignalStrength.signalStrength = rscp;
    }
    else if (255 != rsrq && 255 != rsrp)
    {
        /*
         * for rsrp if modem returns 0 then rsrp = -140 dBm.
         * for rsrp if modem returns 1 then rsrp = -139 dBm.
         * As Android does the inversion, rapid ril needs to send (140 - rsrp) to framework.
         *
         * for rsrq if modem return 0 then rsrq = -19.5 dBm.
         * for rsrq if modem return 1 then rsrq = -19 dBm.
         * As Android does the inversion, rapid ril needs to send (20 - rsrq/2) to framework.
         *
         * for rssnr if modem returns 0 then rssnr = 0 dBm
         * for rssnr if modem returns 1 then rssnr = 0.5 dBm
         * As Android has granularity of 0.1 dB units, rapid ril needs to send
         * (rssnr/2)*10 => rssnr * 5 to framework.
         *
         * You can refer to the latest CAT specification on XCESQI AT command
         * to understand where these numbers come from
         */
        pSigStrData->LTE_SignalStrength.rsrp = 140 - rsrp;
        pSigStrData->LTE_SignalStrength.rsrq = 20 - rsrq / 2;
        pSigStrData->LTE_SignalStrength.rssnr = rssnr * 5;
    }
    else
    {
        RIL_LOG_INFO("CTE_XMM7160::ParseXCESQ - "
                "pSigStrData set to default values\r\n");
    }

    res = RRIL_RESULT_OK;
Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pSigStrData);
        pSigStrData = NULL;
    }

    RIL_LOG_VERBOSE("CTE_XMM7160::ParseXCESQ - Exit()\r\n");
    return pSigStrData;
}

void CTE_XMM7160::QuerySignalStrength()
{
    CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_SIGNAL_STRENGTH].uiChannel, NULL,
            RIL_REQUEST_SIGNAL_STRENGTH, "AT+XCESQ?\r", &CTE::ParseUnsolicitedSignalStrength);

    if (pCmd)
    {
        if (!CCommand::AddCmdToQueue(pCmd))
        {
            RIL_LOG_CRITICAL("CTE_XMM7160::QuerySignalStrength() - Unable to queue command!\r\n");
            delete pCmd;
            pCmd = NULL;
        }
    }
    else
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::QuerySignalStrength() - "
                "Unable to allocate memory for new command!\r\n");
    }
}

//
// RIL_UNSOL_SIGNAL_STRENGTH  1009
//
RIL_RESULT_CODE CTE_XMM7160::ParseUnsolicitedSignalStrength(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::ParseUnsolicitedSignalStrength() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    RIL_SignalStrength_v6* pSigStrData = NULL;
    const char* pszRsp = rRspData.szResponse;

    pSigStrData = ParseXCESQ(pszRsp, FALSE);
    if (NULL == pSigStrData)
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::ParseUnsolicitedSignalStrength() -"
                " parsing failed\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

    RIL_onUnsolicitedResponse(RIL_UNSOL_SIGNAL_STRENGTH, (void*)pSigStrData,
            sizeof(RIL_SignalStrength_v6));

Error:
    free(pSigStrData);
    pSigStrData = NULL;

    RIL_LOG_VERBOSE("CTE_XMM7160::ParseUnsolicitedSignalStrength() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_ISIM_AUTHENTICATE 106
//
RIL_RESULT_CODE CTE_XMM7160::CoreISimAuthenticate(REQUEST_DATA& rReqData,
                                                                void* pData,
                                                                UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::CoreISimAuthenticate() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    char szAutn[AUTN_LENGTH+1]; //32 bytes + null terminated
    char szRand[RAND_LENGTH+1]; //32 bytes + null terminated
    char* pszInput = (char*) pData;
    int nChannelId = 0; // Use default channel ( USIM ) for now. may need to use the one
                        // for ISIM.
    int nContext = 1;   // 1 is USIM security context. ( see CAT Spec )

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CoreISimAuthenticate() - Passed data pointer was NULL\r\n");
        goto Error;
    }

    CopyStringNullTerminate(szAutn, pszInput, sizeof(szAutn));
    CopyStringNullTerminate(szRand, pszInput+AUTN_LENGTH, sizeof(szRand));

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1),
            "AT+XAUTH=%u,%u,\"%s\",\"%s\"\r", nChannelId, nContext, szRand, szAutn))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CoreISimAuthenticate() - Cannot create XAUTH command -"
                " szRand=%s, szAutn=%s\r\n",szRand,szAutn);
        goto Error;
    }

    res = RRIL_RESULT_OK;
Error:
    RIL_LOG_VERBOSE("CTE_XMM7160::CoreISimAuthenticate() - Exit\r\n");

    return res;
}

RIL_RESULT_CODE CTE_XMM7160::ParseISimAuthenticate(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::ParseISimAuthenticate() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char* pszRsp = rRspData.szResponse;
    int reslen = 0;
    char * pszResult = NULL;
    UINT32 uiStatus;
    char szRes[MAX_BUFFER_SIZE];
    char szCk[MAX_BUFFER_SIZE];
    char szIk[MAX_BUFFER_SIZE];
    char szKc[MAX_BUFFER_SIZE];

    memset(szRes, '\0', sizeof(szRes));
    memset(szCk, '\0', sizeof(szCk));
    memset(szIk, '\0', sizeof(szIk));
    memset(szKc, '\0', sizeof(szKc));

    if (NULL == rRspData.szResponse)
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::ParseISimAuthenticate() -"
                " Response String pointer is NULL.\r\n");
        goto Error;
    }

    if (FindAndSkipString(pszRsp, "+XAUTH: ", pszRsp))
    {
        if (!ExtractUInt32(pszRsp, uiStatus, pszRsp)) {
            RIL_LOG_CRITICAL("CTE_XMM7160::ParseISimAuthenticate() -"
                    " Error parsing status.\r\n");
            goto Error;
        }

        if ((uiStatus == 0)||(uiStatus == 1))
        {
            // Success, need to parse the extra parameters...
            if (!SkipString(pszRsp, ",", pszRsp))
            {
                RIL_LOG_CRITICAL("CTE_XMM7160::ParseISimAuthenticate() -"
                                 " Error parsing status.\r\n");
                goto Error;
            }

            if (!ExtractQuotedString(pszRsp, szRes, sizeof(szRes), pszRsp)) {
                RIL_LOG_CRITICAL("CTE_XMM7160::ParseISimAuthenticate() -"
                                 " Error parsing Res.\r\n");
                goto Error;
            }

            if (uiStatus == 0)
            {
                // This is success, so we need to get CK, IK, KC
                if (!SkipString(pszRsp, ",", pszRsp))
                {
                    RIL_LOG_CRITICAL("CTE_XMM7160::ParseISimAuthenticate() -"
                                     " Error parsing Res.\r\n");
                    goto Error;
                }

                if (!ExtractQuotedString(pszRsp, szCk, sizeof(szCk), pszRsp)) {
                    RIL_LOG_CRITICAL("CTE_XMM7160::ParseISimAuthenticate() -"
                                     " Error parsing CK.\r\n");
                    goto Error;
                }
                if (!SkipString(pszRsp, ",", pszRsp))
                {
                    RIL_LOG_CRITICAL("CTE_XMM7160::ParseISimAuthenticate() -"
                                     " Error parsing CK.\r\n");
                    goto Error;
                }

                if (!ExtractQuotedString(pszRsp, szIk, sizeof(szIk), pszRsp)) {
                    RIL_LOG_CRITICAL("CTE_XMM7160::ParseISimAuthenticate() -"
                                     " Error parsing IK.\r\n");
                    goto Error;
                }
                if (!SkipString(pszRsp, ",", pszRsp))
                {
                    RIL_LOG_CRITICAL("CTE_XMM7160::ParseISimAuthenticate() -"
                                     " Error parsing IK.\r\n");
                    goto Error;
                }

                if (!ExtractQuotedString(pszRsp, szKc, sizeof(szKc), pszRsp)) {
                    RIL_LOG_CRITICAL("CTE_XMM7160::ParseISimAuthenticate() -"
                                     " Error parsing Kc.\r\n");
                    goto Error;
                }
            }

            // Log the result for debug
            RIL_LOG_VERBOSE("CTE_XMM7160::ParseISimAuthenticate - Res/Auts -"
                            " =%s\r\n", szRes);
        }
    }
    else
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::ParseISimAuthenticate() -"
                " Error searching +XAUTH:.\r\n");
        goto Error;
    }

    if (!FindAndSkipRspEnd(pszRsp, m_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::ParseISimAuthenticate() -"
                " Could not extract the response end.\r\n");
        goto Error;
    }

    // reslen = 3 bytes for the status (int), 4 bytes for the :, and the rest + 1 for
    // the carriage return...
    reslen = 3 + 4 + strlen(szRes) + strlen(szCk) + strlen(szIk) + strlen(szKc) + 1;
    pszResult = (char *) malloc(reslen);
    if (!pszResult)
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::ParseISimAuthenticate() -"
                " Could not allocate memory for result string.\r\n");
        goto Error;
    }
    if (!PrintStringNullTerminate(pszResult, reslen,
                                  "%d:%s:%s:%s:%s", uiStatus, szRes, szCk, szIk, szKc))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::ParseISimAuthenticate() -"
                " Error creating response string.\r\n");
        goto Error;
    }
    rRspData.pData = (void *) pszResult;
    rRspData.uiDataSize = reslen;

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTE_XMM7160::ParseISimAuthenticate() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GET_CELL_INFO_LIST 109
//
RIL_RESULT_CODE CTE_XMM7160::CoreGetCellInfoList(REQUEST_DATA& rReqData,
        void* pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::CoreGetCellInfoList() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+XMCI=\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CTE_XMM7160::CoreGetCellInfoList() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_XMM7160::ParseCellInfoList(RESPONSE_DATA& rRspData, BOOL isUnsol)
{
    RIL_RESULT_CODE res = RRIL_RESULT_OK;
    P_ND_N_CELL_INFO_DATA pCellData = NULL;
    int nCellInfos = 0;

    pCellData = ParseXMCI(rRspData, nCellInfos);

    if (!isUnsol)
    {
        if (nCellInfos > 0 && NULL != pCellData)
        {
            rRspData.pData = (void*)pCellData->pnCellData;
            rRspData.uiDataSize = nCellInfos * sizeof(RIL_CellInfo);
        }
        else
        {
            nCellInfos = 0;
            rRspData.pData  = NULL;
            rRspData.uiDataSize = 0;
            free(pCellData);
            pCellData = NULL;
        }
    }
    else
    {
        // Unsolicited CELL INFO LIST
        // update the list only if there is a change in the information
        // compare the read values with the cellinfo cache and if the values
        // are different, report RIL_UNSOL_CELL_INFO_LIST
        if (nCellInfos > 0)
        {
            int requestedRate = (int)rRspData.pContextData;
            if (m_cte.updateCellInfoCache(pCellData, nCellInfos)
                    && -1 != requestedRate && INT_MAX != requestedRate)
            {
                RIL_onUnsolicitedResponse(RIL_UNSOL_CELL_INFO_LIST,
                        (void*)pCellData->pnCellData, (sizeof(RIL_CellInfo) * nCellInfos));
            }
        }

        // restart the timer now with the latest rate setting.
        if (!m_cte.IsCellInfoTimerRunning())
        {
            int newRate = m_cte.GetCellInfoListRate();
            RestartUnsolCellInfoListTimer(newRate);
        }
    }

Error:

    if (RRIL_RESULT_OK != res || isUnsol)
    {
        free(pCellData);
        pCellData = NULL;
    }

    RIL_LOG_VERBOSE("CTE_XMM7160::ParseCellInfoList() - Exit\r\n");
    return res;
}

P_ND_N_CELL_INFO_DATA CTE_XMM7160::ParseXMCI(RESPONSE_DATA& rspData, int& nCellInfos)
{
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int index = 0;
    int mcc = -1;
    int mnc = -1;
    UINT32 uiLac = 0;
    UINT32 uiCi = -1;
    const char* pszRsp = rspData.szResponse;
    P_ND_N_CELL_INFO_DATA pCellData = NULL;

    pCellData = (P_ND_N_CELL_INFO_DATA)malloc(sizeof(S_ND_N_CELL_INFO_DATA));
    if (NULL == pCellData)
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::ParseXMCI() -"
                " Could not allocate memory for a S_ND_N_CELL_INFO_DATA struct.\r\n");
        goto Error;
    }
    memset(pCellData, 0, sizeof(S_ND_N_CELL_INFO_DATA));

    /*
     * GSM serving and neighboring cell:
     *
     * +XMCI: 0,<MCC>,<MNC>,<LAC>,<CI>,<BSIC>,<RXLEV>,<BER>
     * +XMCI: 1,<MCC>,<MNC>,<LAC>,<CI>,<BSIC>,<RXLEV>,<BER>
     *
     * UMTS serving and neighboring cell:
     *
     * +XMCI: 2,<MCC>,<MNC>,<LAC>,<CI>,<PSC>,<RSCP>,<ECNO>
     * +XMCI: 3,<MCC>,<MNC>,<LAC>,<CI>,<PSC>,<RSCP>,<ECNO>
     *
     * LTE serving and neighboring cell:
     *
     * +XMCI: 4,<MCC>,<MNC>,<TAC>,<CI>,<PCI>,<RSRP>,<RSRQ>,<RSSNR>,<TA>,<CQI>
     * +XMCI: 5,<MCC>,<MNC>,<TAC>,<CI>,<PCI>,<RSRP>,<RSRQ>,<RSSNR>,<TA>,<CQI>
     */

    // Loop on +XMCI until no more entries are found
    while (FindAndSkipString(pszRsp, "+XMCI: ", pszRsp))
    {
        int type = 0;
        int dummy;

        if (RRIL_MAX_CELL_ID_COUNT == index)
        {
            //  We're full.
            RIL_LOG_CRITICAL("CTE_XMM7160::ParseXMCI() -"
                    " Exceeded max count = %d\r\n", RRIL_MAX_CELL_ID_COUNT);
            break;
        }

        // Read <TYPE>
        if (!ExtractInt(pszRsp, type, pszRsp))
        {
            RIL_LOG_CRITICAL("CTE_XMM7160::ParseXMCI() -"
                    " could not extract <TYPE> value\r\n");
            goto Error;
        }

        // Read <MCC>
        if ((!SkipString(pszRsp, ",", pszRsp))
                || (!ExtractInt(pszRsp, mcc, pszRsp)))
        {
            RIL_LOG_CRITICAL("CTE_XMM7160::ParseXMCI() -"
                    " could not extract <MCC> value\r\n");
            goto Error;
        }

        // Read <MNC>
        if ((!SkipString(pszRsp, ",", pszRsp))
                || (!ExtractInt(pszRsp, mnc, pszRsp)))
        {
            RIL_LOG_CRITICAL("CTE_XMM7160::ParseXMCI() -"
                    " could not extract <MNC> value\r\n");
            goto Error;
        }

        // Ignore the cell information if modem reports invalid mcc and mnc but continue parsing
        // next response line.
        if (0xFFFF == mcc && 0xFFFF == mnc)
        {
            continue;
        }

        // Read <LAC> or <TAC>
        if (SkipString(pszRsp, ",", pszRsp))
        {
            if (SkipString(pszRsp, "\"", pszRsp))
            {
                if (!ExtractHexUInt32(pszRsp, uiLac, pszRsp))
                {
                    RIL_LOG_CRITICAL("CTE_XMM7160::ParseXMCI() -"
                            " could not extract <LAC>\r\n");
                }
                SkipString(pszRsp, "\"", pszRsp);
            }
        }

        // Read <CI>
        if (SkipString(pszRsp, ",", pszRsp))
        {
            if (SkipString(pszRsp, "\"", pszRsp))
            {
                if (!ExtractHexUInt32(pszRsp, uiCi, pszRsp))
                {
                    RIL_LOG_CRITICAL("CTE_XMM7160::ParseXMCI() -"
                            " could not extract <CI> value\r\n");
                }
                SkipString(pszRsp, "\"", pszRsp);
            }
        }

        switch (type)
        {
            case 0: // GSM serving cell
            case 1: // GSM neighboring cell
            {
                int rxlev;
                int ber;

                // Read <BSIC>
                if ((!SkipString(pszRsp, ",", pszRsp))
                        || (!ExtractInt(pszRsp, dummy, pszRsp)))
                {
                    RIL_LOG_CRITICAL("CTE_XMM7160::ParseXMCI() -"
                            " could not extract <BSIC> value\r\n");
                    goto Error;
                }

                // Read <RXLEV>
                if ((!SkipString(pszRsp, ",", pszRsp))
                        || (!ExtractInt(pszRsp, rxlev, pszRsp)))
                {
                    RIL_LOG_CRITICAL("CTE_XMM7160::ParseXMCI() -"
                            " could not extract <RXLEV> value\r\n");
                    goto Error;
                }

                // Read <BER>
                if ((!SkipString(pszRsp, ",", pszRsp))
                        || (!ExtractInt(pszRsp, ber, pszRsp)))
                {
                    RIL_LOG_CRITICAL("CTE_XMM7160::ParseXMCI() -"
                            " could not extract <BER> value\r\n");
                    goto Error;
                }

                RIL_CellInfo& info = pCellData->pnCellData[index];
                info.registered = (0 == type) ? SERVING_CELL : NEIGHBOURING_CELL;
                info.cellInfoType = RIL_CELL_INFO_TYPE_GSM;
                info.timeStampType = RIL_TIMESTAMP_TYPE_JAVA_RIL;
                info.timeStamp = ril_nano_time();
                info.CellInfo.gsm.signalStrengthGsm.signalStrength =
                        MapRxlevToSignalStrengh(rxlev);
                info.CellInfo.gsm.signalStrengthGsm.bitErrorRate = ber;
                info.CellInfo.gsm.cellIdentityGsm.lac = uiLac;
                info.CellInfo.gsm.cellIdentityGsm.cid = uiCi;
                info.CellInfo.gsm.cellIdentityGsm.mnc = mnc;
                info.CellInfo.gsm.cellIdentityGsm.mcc = mcc;
                RIL_LOG_INFO("CTE_XMM7160::ParseXMCI() - "
                        "GSM LAC,CID,MCC,MNC index=[%d] cid=[%d] lac[%d] mcc[%d] mnc[%d]\r\n",
                        index, info.CellInfo.gsm.cellIdentityGsm.cid,
                        info.CellInfo.gsm.cellIdentityGsm.lac,
                        info.CellInfo.gsm.cellIdentityGsm.mcc,
                        info.CellInfo.gsm.cellIdentityGsm.mnc);


                index++;
            }
            break;

            case 2: // UMTS serving cell
            case 3: // UMTS neighboring cell
            {
                int rscp;
                int psc;

                // Read <PSC>
                if (!SkipString(pszRsp, ",", pszRsp)
                        || !ExtractInt(pszRsp, psc, pszRsp))
                {
                    RIL_LOG_CRITICAL("CTE_XMM7160::ParseXMCI() -"
                            " could not extract <PSC>\r\n");
                    goto Error;
                }

                // Read <RSCP>
                if (!SkipString(pszRsp, ",", pszRsp)
                        || !ExtractInt(pszRsp, rscp, pszRsp))
                {
                    RIL_LOG_CRITICAL("CTE_XMM7160::ParseXMCI() -"
                            " could not extract <RSCP>\r\n");
                    goto Error;
                }

                // Read <ECNO>
                if (!SkipString(pszRsp, ",", pszRsp)
                        || !ExtractInt(pszRsp, dummy, pszRsp))
                {
                    RIL_LOG_CRITICAL("CTE_XMM7160::ParseXMCI() -"
                            " could not extract <ECNO>\r\n");
                    goto Error;
                }

                RIL_CellInfo& info = pCellData->pnCellData[index];
                info.registered = (2 == type) ? SERVING_CELL : NEIGHBOURING_CELL;
                info.cellInfoType = RIL_CELL_INFO_TYPE_WCDMA;
                info.timeStampType = RIL_TIMESTAMP_TYPE_JAVA_RIL;
                info.timeStamp = ril_nano_time();

                info.CellInfo.wcdma.signalStrengthWcdma.signalStrength = MapRscpToRssi(rscp);
                info.CellInfo.wcdma.signalStrengthWcdma.bitErrorRate = BER_UNKNOWN;
                info.CellInfo.wcdma.cellIdentityWcdma.lac = uiLac;
                info.CellInfo.wcdma.cellIdentityWcdma.cid = uiCi;
                info.CellInfo.wcdma.cellIdentityWcdma.psc = psc;
                info.CellInfo.wcdma.cellIdentityWcdma.mnc = mnc;
                info.CellInfo.wcdma.cellIdentityWcdma.mcc = mcc;
                RIL_LOG_INFO("CTE_XMM7160::ParseXMCI() - "
                        "UMTS LAC,CID,MCC,MNC,ScrCode "
                        "index=[%d]  cid=[%d] lac[%d] mcc[%d] mnc[%d] scrCode[%d]\r\n",
                        index, info.CellInfo.wcdma.cellIdentityWcdma.cid,
                        info.CellInfo.wcdma.cellIdentityWcdma.lac,
                        info.CellInfo.wcdma.cellIdentityWcdma.mcc,
                        info.CellInfo.wcdma.cellIdentityWcdma.mnc,
                        info.CellInfo.wcdma.cellIdentityWcdma.psc);

                index++;
            }
            break;

            case 4: // LTE serving cell
            case 5: // LTE neighboring cell
            {
                int pci;
                int rsrp;
                int rsrq;
                int rssnr;
                int ta;
                int cqi;

                // Read <PCI>
                if (!SkipString(pszRsp, ",", pszRsp)
                        || !ExtractInt(pszRsp, pci, pszRsp))
                {
                    RIL_LOG_CRITICAL("CTE_XMM7160::ParseXMCI() -"
                            " could not extract <PCI>\r\n");
                    goto Error;
                }

                // Read <RSRP>
                if (!SkipString(pszRsp, ",", pszRsp)
                        || !ExtractInt(pszRsp, rsrp, pszRsp))
                {
                    RIL_LOG_CRITICAL("CTE_XMM7160::ParseXMCI() -"
                            " could not extract <RSRP>\r\n");
                    goto Error;
                }

                // Read <RSRQ>
                if (!SkipString(pszRsp, ",", pszRsp)
                        || !ExtractInt(pszRsp, rsrq, pszRsp))
                {
                    RIL_LOG_CRITICAL("CTE_XMM7160::ParseXMCI() -"
                            " could not extract <RSRQ>\r\n");
                    goto Error;
                }

                // Read <RSSNR>
                if (!SkipString(pszRsp, ",", pszRsp)
                        || !ExtractInt(pszRsp, rssnr, pszRsp))
                {
                    RIL_LOG_CRITICAL("CTE_XMM7160::ParseXMCI() -"
                            " could not extract <RSSNR>\r\n");
                    goto Error;
                }

                // Read <TA>
                if (!SkipString(pszRsp, ",", pszRsp)
                        || !ExtractInt(pszRsp, ta, pszRsp))
                {
                    RIL_LOG_CRITICAL("CTE_XMM7160::ParseXMCI() -"
                            " could not extract <TA>\r\n");
                    goto Error;
                }

                // Read <CQI>
                if (!SkipString(pszRsp, ",", pszRsp)
                        || !ExtractInt(pszRsp, cqi, pszRsp))
                {
                    RIL_LOG_CRITICAL("CTE_XMM7160::ParseXMCI() -"
                            " could not extract <CQI>\r\n");
                    goto Error;
                }

                RIL_CellInfo& info = pCellData->pnCellData[index];
                info.registered = (4 == type) ? SERVING_CELL : NEIGHBOURING_CELL;
                info.cellInfoType = RIL_CELL_INFO_TYPE_LTE;
                info.timeStampType = RIL_TIMESTAMP_TYPE_JAVA_RIL;
                info.timeStamp = ril_nano_time();
                info.CellInfo.lte.signalStrengthLte.signalStrength = RSSI_UNKNOWN;
                info.CellInfo.lte.signalStrengthLte.rsrp = MapToAndroidRsrp(rsrp);
                info.CellInfo.lte.signalStrengthLte.rsrq = MapToAndroidRsrq(rsrq);
                info.CellInfo.lte.signalStrengthLte.rssnr = MapToAndroidRssnr(rssnr);
                info.CellInfo.lte.signalStrengthLte.cqi = cqi;
                info.CellInfo.lte.signalStrengthLte.timingAdvance = ta;
                info.CellInfo.lte.cellIdentityLte.tac = uiLac;
                info.CellInfo.lte.cellIdentityLte.ci = uiCi;
                info.CellInfo.lte.cellIdentityLte.pci = pci;
                info.CellInfo.lte.cellIdentityLte.mnc = mnc;
                info.CellInfo.lte.cellIdentityLte.mcc = mcc;
                RIL_LOG_INFO("CTE_XMM7160::ParseXMCI() -"
                        "LTE TAC,CID,MCC,MNC,RSRP,RSRQ,TA,RSSNR,PhyCellId "
                        "index=[%d] cid=[%d] tac[%d] mcc[%d] mnc[%d] rsrp[%d] rsrq[%d] "
                        "ta[%d] rssnr[%d] Phyci[%d] \r\n",
                        index, info.CellInfo.lte.cellIdentityLte.ci,
                        info.CellInfo.lte.cellIdentityLte.tac,
                        info.CellInfo.lte.cellIdentityLte.mcc,
                        info.CellInfo.lte.cellIdentityLte.mnc,
                        info.CellInfo.lte.signalStrengthLte.rsrp,
                        info.CellInfo.lte.signalStrengthLte.rsrq,
                        info.CellInfo.lte.signalStrengthLte.timingAdvance,
                        info.CellInfo.lte.signalStrengthLte.rssnr,
                        info.CellInfo.lte.cellIdentityLte.pci);

                index++;
            }
            break;

            default:
            {
                RIL_LOG_INFO("CTE_XMM7160::ParseXMCI() -"
                        " Invalid type=[%d]\r\n", type);
                goto Error;
            }
            break;
        }
    }

    nCellInfos = index;
    res = RRIL_RESULT_OK;
Error:
    while (FindAndSkipString(pszRsp, "+XMCI: ", pszRsp));

    // Skip "<postfix>"
    if (!FindAndSkipRspEnd(pszRsp, m_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::ParseXMCI - Could not skip response postfix.\r\n");
    }

    if (RRIL_RESULT_OK != res)
    {
        nCellInfos = 0;
        free(pCellData);
        pCellData = NULL;
    }

    return pCellData;
}

int CTE_XMM7160::MapRxlevToSignalStrengh(int rxlev)
{
    // In case of invalid rxlev values, return RSSI_UNKNOWN.
    if (rxlev > 57 || rxlev < 0)
        return RSSI_UNKNOWN;
    else if (rxlev <= 57)
        return (rxlev / 2) + 2;
    else
        return 31;
}

int CTE_XMM7160::MapToAndroidRsrp(int rsrp)
{
    /*
     * In case of invalid rsrp values, return INT_MAX.
     * If modem returns 0 then rsrp = -140 dBm.
     * If modem returns 1 then rsrp = -139 dBm.
     *
     * As Android expects a positive value, rapid ril needs to send (140 - rsrp) to
     * framework.
     */
    if (rsrp > 97 || rsrp < 0)
        return INT_MAX;
    else if (rsrp < 1)
        return 140;
    else
        return 140 - rsrp;
}

int CTE_XMM7160::MapToAndroidRsrq(int rsrq)
{
    /*
     * In case of invalid rsrq values, return INT_MAX.
     * If modem return 0 then rsrq = -20 dBm.
     * If modem return 1 then rsrq = -20 dBm.
     *
     * Note: -19.5dBm is rounded to -20dbm
     *
     * As Android expects a positive value, rapid ril needs to send (20 - rsrq / 2) to
     * framework.
     */
    if (rsrq > 34 || rsrq < 0)
        return INT_MAX;
    else
        return 20 - rsrq / 2;
}

int CTE_XMM7160::MapToAndroidRssnr(int rssnr)
{
    /*
     * If modem returns 0 then rssnr = 0 dBm
     * If modem returns 1 then rssnr = 0.5 dBm
     * As Android has granularity of 0.1 dB units, rapid ril needs to send
     * (rssnr/2)*10 => rssnr * 5 to framework.
     *
     * As per ril documentation, valid rssnr range is -200 to +300(-200 = -20.0dB, +300 = 30dB).
     * Return INT_MAX if the value is not within this range.
     */
    int androidRssnr = rssnr * 5;
    if (androidRssnr > 300 || androidRssnr < -200)
        return INT_MAX;
    else
        return androidRssnr;
}
