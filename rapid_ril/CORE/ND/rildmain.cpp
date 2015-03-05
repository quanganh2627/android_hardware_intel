////////////////////////////////////////////////////////////////////////////
// rildmain.cpp
//
// Copyright 2005-2008 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides implementations for top-level RIL functions.
//
/////////////////////////////////////////////////////////////////////////////

#include <telephony/ril.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include "types.h"
#include "rillog.h"
#include "te.h"
#include "thread_ops.h"
#include "systemmanager.h"
#include "repository.h"
#include "rildmain.h"
#include "reset.h"
#include <cutils/properties.h>
#include <utils/Log.h>

///////////////////////////////////////////////////////////
//  FUNCTION PROTOTYPES
//

static void onRequest(int request, void* data, size_t datalen, RIL_Token t);
static RIL_RadioState onGetCurrentRadioState();
static int onSupports(int requestCode);
static void onCancel(RIL_Token t);
static const char* getVersion();


///////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
//

char* g_szCmdPort  = NULL;
BOOL  g_bIsSocket = FALSE;
char* g_szDataPort1 = NULL;
char* g_szDataPort2 = NULL;
char* g_szDataPort3 = NULL;
char* g_szDataPort4 = NULL;
char* g_szDataPort5 = NULL;
char* g_szDLC2Port = NULL;
char* g_szDLC6Port = NULL;
char* g_szDLC8Port = NULL;
char* g_szURCPort = NULL;
char* g_szOEMPort = NULL;
char* g_szSIMID = NULL;

// Upper limit on number of RIL channels to create
UINT32 g_uiRilChannelUpperLimit = RIL_CHANNEL_MAX;

// Current RIL channel index maximum (depends on number of data channels created)
UINT32 g_uiRilChannelCurMax = 0;

static const RIL_RadioFunctions gs_callbacks =
{
    RIL_VERSION,
    onRequest,
    onGetCurrentRadioState,
    onSupports,
    onCancel,
    getVersion
};

static const struct RIL_Env* gs_pRilEnv;


///////////////////////////////////////////////////////////
// FUNCTION DEFINITIONS
//

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
void RIL_onRequestComplete(RIL_Token tRIL, RIL_Errno eErrNo, void* pResponse, size_t responseLen)
{
    gs_pRilEnv->OnRequestComplete(tRIL, eErrNo, pResponse, responseLen);
    RIL_LOG_INFO("After OnRequestComplete(): token=0x%08x, eErrNo=%d, pResponse=[0x%08x],"
            " len=[%d]\r\n", tRIL, eErrNo, pResponse, responseLen);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
void RIL_onUnsolicitedResponse(int unsolResponseID, const void* pData, size_t dataSize)
{
    bool bSendNotification = true;

    if ((CTE::GetTE().IsPlatformShutDownRequested() || CTE::GetTE().IsRadioRequestPending())
            && RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED != unsolResponseID
            && RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED != unsolResponseID
            && RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED != unsolResponseID
            && RIL_UNSOL_DATA_CALL_LIST_CHANGED != unsolResponseID
            && RIL_UNSOL_OEM_HOOK_RAW != unsolResponseID)
    {
        RIL_LOG_INFO("RIL_onUnsolicitedResponse() - ignoring id=%d due to "
                "radio on/off requested\r\n", unsolResponseID);
        return;
    }

    switch (unsolResponseID)
    {
        case RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED: // 1000
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() -"
                    " RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED\r\n");
            break;

        case RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED:  // 1001
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() -"
                    " RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED\r\n");
            break;

        case RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED:  // 1002
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() -"
                    " RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED\r\n");
            break;

        case RIL_UNSOL_RESPONSE_NEW_SMS:  // 1003
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() -"
                    " RIL_UNSOL_RESPONSE_NEW_SMS\r\n");
            if (pData && dataSize)
            {
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - PDU=\"%s\"\r\n", (char*)pData);
            }
            break;

        case RIL_UNSOL_RESPONSE_NEW_SMS_STATUS_REPORT:  // 1004
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() -"
                    " RIL_UNSOL_RESPONSE_NEW_SMS_STATUS_REPORT\r\n");
            if (pData && dataSize)
            {
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - PDU=\"%s\"\r\n", (char*)pData);
            }
            break;

        case RIL_UNSOL_RESPONSE_NEW_SMS_ON_SIM:  // 1005
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_RESPONSE_NEW_SMS_ON_SIM\r\n");
            if (pData && dataSize)
            {
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - index=%d\r\n", ((int*)pData)[0]);
            }
            break;

        case RIL_UNSOL_ON_USSD:  // 1006
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_ON_USSD\r\n");
            if (pData && dataSize)
            {
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - type code=\"%s\"\r\n",
                        ((char**)pData)[0]);
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - msg=\"%s\"\r\n", ((char**)pData)[1]);
            }
            break;

        case RIL_UNSOL_ON_USSD_REQUEST:  // 1007 - obsolete, use RIL_UNSOL_ON_USSD 1006
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_ON_USSD_REQUEST\r\n");
            break;

        case RIL_UNSOL_NITZ_TIME_RECEIVED:  // 1008
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_NITZ_TIME_RECEIVED\r\n");
            if (pData && dataSize)
            {
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - NITZ info=\"%s\"\r\n", (char*)pData);
            }
            break;

        case RIL_UNSOL_SIGNAL_STRENGTH:  // 1009
            RIL_LOG_VERBOSE("RIL_onUnsolicitedResponse() - RIL_UNSOL_SIGNAL_STRENGTH\r\n");
            if (pData && dataSize)
            {
                RIL_LOG_VERBOSE("RIL_onUnsolicitedResponse() - GW_signalStrength=%d\r\n",
                        ((RIL_SignalStrength_v6*)pData)->GW_SignalStrength.signalStrength);
                RIL_LOG_VERBOSE("RIL_onUnsolicitedResponse() - GW_bitErrorRate=%d\r\n",
                        ((RIL_SignalStrength_v6*)pData)->GW_SignalStrength.bitErrorRate);
            }
            break;

        case RIL_UNSOL_DATA_CALL_LIST_CHANGED:  // 1010
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_DATA_CALL_LIST_CHANGED\r\n");
            if (pData && dataSize)
            {
                int nDataCallResponseNum = dataSize / sizeof(RIL_Data_Call_Response_v6);
                RIL_Data_Call_Response_v6* pDCR = (RIL_Data_Call_Response_v6*)pData;
                for (int i=0; i<nDataCallResponseNum; i++)
                {
                    RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_Data_Call_Response_v6[%d]"
                            " status=%d suggRetryTime=%d cid=%d active=%d type=\"%s\" ifname=\"%s\""
                            " addresses=\"%s\" dnses=\"%s\" gateways=\"%s\"\r\n",
                            i,
                            pDCR[i].status,
                            pDCR[i].suggestedRetryTime,
                            pDCR[i].cid, pDCR[i].active,
                            pDCR[i].type, pDCR[i].ifname,
                            pDCR[i].addresses,
                            pDCR[i].dnses,
                            pDCR[i].gateways);
                }
            }
            break;

        case RIL_UNSOL_SUPP_SVC_NOTIFICATION:  // 1011
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_SUPP_SVC_NOTIFICATION\r\n");
            if (pData && dataSize)
            {
                RIL_SuppSvcNotification* pSSN = (RIL_SuppSvcNotification*)pData;
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - notification type=%d code=%d index=%d"
                        " type=%d number=\"%s\"\r\n",
                        pSSN->notificationType,
                        pSSN->code,
                        pSSN->index,
                        pSSN->type,
                        pSSN->number);
            }
            break;

        case RIL_UNSOL_STK_SESSION_END:  // 1012
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_STK_SESSION_END\r\n");
            break;

        case RIL_UNSOL_STK_PROACTIVE_COMMAND:  // 1013
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_STK_PROACTIVE_COMMAND\r\n");
            if (pData && dataSize)
            {
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - data=\"%s\"\r\n", (char*)pData);
            }
            else
            {
                // If there is no data, this unsolicited command will generate a JAVA CRASH
                // So ignore it if we are in this case
                bSendNotification = false;
            }
            break;

        case RIL_UNSOL_STK_EVENT_NOTIFY:  // 1014
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_STK_EVENT_NOTIFY\r\n");
            if (pData && dataSize)
            {
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - data=\"%s\"\r\n", (char*)pData);
            }
            break;

        case RIL_UNSOL_STK_CALL_SETUP:  // 1015
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_STK_CALL_SETUP\r\n");
            if (pData && dataSize)
            {
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - timeout=%d ms\r\n", ((int*)pData)[0]);
            }
            break;

        case RIL_UNSOL_SIM_SMS_STORAGE_FULL:  // 1016
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_SIM_SMS_STORAGE_FULL\r\n");
            break;

        case RIL_UNSOL_SIM_REFRESH:  // 1017
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_SIM_REFRESH\r\n");
            if (pData && dataSize)
            {
                RIL_SimRefreshResponse_v7* pSimRefreshRsp = (RIL_SimRefreshResponse_v7*)pData;
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_SimRefreshResult=%d efid=%d"
                        " aid=%s\r\n", pSimRefreshRsp->ef_id,
                        (NULL == pSimRefreshRsp->aid) ? "" : pSimRefreshRsp->aid);
            }
            break;

        case RIL_UNSOL_CALL_RING:  // 1018
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_CALL_RING\r\n");
            break;

        case RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED:  // 1019
            /*
             * If the device is encrypted but not yet decrypted, then modem have been powered
             * on for emergency call. Don't report sim status as this results in emergency call
             * getting disconnected due to airplane mode activated by CryptKeeper on configuration
             * changes.
             */
            if (CSystemManager::GetInstance().IsDeviceDecrypted())
            {
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - "
                        "RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED\r\n");
            }
            else
            {
                bSendNotification = FALSE;
            }
            break;

        case RIL_UNSOL_RESPONSE_CDMA_NEW_SMS:  // 1020 - CDMA, not supported
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_RESPONSE_CDMA_NEW_SMS\r\n");
            bSendNotification = false;
            break;

        case RIL_UNSOL_RESPONSE_NEW_BROADCAST_SMS:  // 1021
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_RESPONSE_NEW_BROADCAST_SMS\r\n");
            if (pData && dataSize)
            {
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - PDU=\"%s\"\r\n", (char*)pData);
            }
            break;

        case RIL_UNSOL_CDMA_RUIM_SMS_STORAGE_FULL:  // 1022 - CDMA, not supported
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_CDMA_RUIM_SMS_STORAGE_FULL\r\n");
            bSendNotification = false;
            break;

        case RIL_UNSOL_RESTRICTED_STATE_CHANGED:  // 1023
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_RESTRICTED_STATE_CHANGED\r\n");
            if (pData && dataSize)
            {
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() -"
                        " RIL_RESTRICTED_STATE_* bitmask=0x%08X\r\n", ((int*)pData)[0]);
            }
            break;

        case RIL_UNSOL_ENTER_EMERGENCY_CALLBACK_MODE:  // 1024
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() -"
                    " RIL_UNSOL_ENTER_EMERGENCY_CALLBACK_MODE\r\n");
            break;

        case RIL_UNSOL_CDMA_CALL_WAITING:  // 1025 - CDMA, not supported
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_CDMA_CALL_WAITING\r\n");
            bSendNotification = false;
            break;

        case RIL_UNSOL_CDMA_OTA_PROVISION_STATUS:  // 1026 - CDMA, not supported
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_CDMA_OTA_PROVISION_STATUS\r\n");
            bSendNotification = false;
            break;

        case RIL_UNSOL_CDMA_INFO_REC:  // 1027 - CDMA, not supported
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_CDMA_INFO_REC\r\n");
            bSendNotification = false;
            break;

        case RIL_UNSOL_OEM_HOOK_RAW:  // 1028
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_OEM_HOOK_RAW\r\n");
            if (pData && dataSize)
            {
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - dataSize=%d\r\n", dataSize);
            }
            else
            {
                // If there is no data, this unsolicited command will generate a JAVA CRASH
                // So ignore it if we are in this case
                bSendNotification = false;
            }
            break;

        case RIL_UNSOL_RINGBACK_TONE:  // 1029
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_RINGBACK_TONE\r\n");
            if (pData && dataSize)
            {
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_RINGBACK_TONE=[%d]\r\n",
                        ((int*)pData)[0]);
            }
            break;

        case RIL_UNSOL_RESEND_INCALL_MUTE:  // 1030
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_RESEND_INCALL_MUTE\r\n");
            break;

        case RIL_UNSOL_CDMA_SUBSCRIPTION_SOURCE_CHANGED:  // 1031 - not supported
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() -"
                    " RIL_UNSOL_CDMA_SUBSCRIPTION_SOURCE_CHANGED\r\n");
            bSendNotification = false;
            break;

        case RIL_UNSOL_CDMA_PRL_CHANGED:  // 1032 - not supported
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_CDMA_PRL_CHANGED\r\n");
            bSendNotification = false;
            break;

        case RIL_UNSOL_EXIT_EMERGENCY_CALLBACK_MODE:  // 1033
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() -"
                    " RIL_UNSOL_EXIT_EMERGENCY_CALLBACK_MODE\r\n");
            break;

        case RIL_UNSOL_RIL_CONNECTED:  // 1034
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_RIL_CONNECTED\r\n");
            if (pData && dataSize)
            {
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_RIL_CONNECTED=[%d]\r\n",
                        ((int*)pData)[0]);
            }
            break;

        case RIL_UNSOL_CELL_INFO_LIST: //1035
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_CELL_INFO_LIST\r\n");
            if (pData && dataSize)
            {
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - "
                        "RIL_UNSOL_CELL_INFO_LIST data size=[%d]\r\n", dataSize);
            }
            break;
        //  ************************* END OF REGULAR NOTIFICATIONS *******************************

#if defined(M2_CALL_FAILED_CAUSE_FEATURE_ENABLED)

        case RIL_UNSOL_CALL_FAILED_CAUSE:  // 1035
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_CALL_FAILED_CAUSE\r\n");
            if (pData && dataSize)
            {
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - call id=[%d]\r\n", ((int*)pData)[0]);
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - failed cause=[%d]\r\n",
                        ((int*)pData)[1]);
            }
            break;

#endif //  M2_CALL_FAILED_CAUSE_FEATURE_ENABLED


        default:
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - Ignoring Unknown Notification id=0x%08X,"
                    " %d\r\n", unsolResponseID, unsolResponseID);
            bSendNotification = false;
            break;
    }

    if ( (NULL == pData) && (0 == dataSize) )
    {
        RIL_LOG_INFO("RIL_onUnsolicitedResponse() - pData is NULL! id=%d\r\n", unsolResponseID);
    }

    if (bSendNotification)
    {
        RIL_LOG_VERBOSE("Calling gs_pRilEnv->OnUnsolicitedResponse()... id=%d\r\n",
                unsolResponseID);
        gs_pRilEnv->OnUnsolicitedResponse(unsolResponseID, pData, dataSize);
    }
    else
    {
        RIL_LOG_INFO("RIL_onUnsolicitedResponse() - ignoring id=%d\r\n", unsolResponseID);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
void RIL_requestTimedCallback(RIL_TimedCallback callback, void* pParam,
                                   const struct timeval* pRelativeTime)
{
    if (pRelativeTime)
    {
        RIL_LOG_INFO("Calling gs_pRilEnv->RequestTimedCallback() timeval sec=[%d]  usec=[%d]\r\n",
                pRelativeTime->tv_sec, pRelativeTime->tv_usec);
    }
    else
    {
        RIL_LOG_INFO("Calling gs_pRilEnv->RequestTimedCallback() timeval sec=[0]  usec=[0]\r\n");
    }
    gs_pRilEnv->RequestTimedCallback(callback, pParam, pRelativeTime);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
void RIL_requestTimedCallback(RIL_TimedCallback callback, void* pParam,
                                           const unsigned long seconds,
                                           const unsigned long microSeconds)
{
    RIL_LOG_INFO("Calling gs_pRilEnv->RequestTimedCallback() sec=[%d]  usec=[%d]\r\n",
            seconds, microSeconds);
    struct timeval myTimeval = {0,0};
    myTimeval.tv_sec = seconds;
    myTimeval.tv_usec = microSeconds;
    gs_pRilEnv->RequestTimedCallback(callback, pParam, &myTimeval);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Call from RIL to us to make a RIL_REQUEST
 *
 * Must be completed with a call to RIL_onRequestComplete()
 *
 * RIL_onRequestComplete() may be called from any thread, before or after
 * this function returns.
 *
 * Will always be called from the same thread, so returning here implies
 * that the radio is ready to process another command (whether or not
 * the previous command has completed).
 */
static void onRequest(int requestID, void* pData, size_t datalen, RIL_Token hRilToken)
{
    RIL_LOG_INFO("onRequest() - id=%d token: 0x%08x\r\n", requestID, (int) hRilToken);

    CTE::GetTE().HandleRequest(requestID, pData, datalen, hRilToken);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Synchronous call from the RIL to us to return current radio state.
 *
 * RADIO_STATE_OFF
 * RADIO_STATE_UNAVAILABLE
 * RADIO_STATE_SIM_NOT_READY
 * RADIO_STATE_SIM_LOCKED_OR_ABSENT
 * RADIO_STATE_SIM_READY
 */
static RIL_RadioState onGetCurrentRadioState()
{
    return CTE::GetTE().GetRadioState();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Call from RIL to us to find out whether a specific request code
 * is supported by this implementation.
 *
 * Return 1 for "supported" and 0 for "unsupported"
 */
static int onSupports(int requestCode)
{
    RIL_LOG_INFO("onSupports() - Request [%d]\r\n", requestCode);

    return CTE::GetTE().IsRequestSupported(requestCode);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
static void onCancel(RIL_Token t)
{
    RIL_LOG_INFO("onCancel() - *******************************************************\r\n");
    RIL_LOG_INFO("onCancel() - Enter - Exit  token=0x%08X\r\n", (int)t);
    RIL_LOG_INFO("onCancel() - *******************************************************\r\n");
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
static const char* getVersion(void)
{
    return "Intrinsyc Rapid-RIL M6.59 for Android 4.2 (Build September 17/2013)";
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
static void* mainLoop(void* param)
{
    RLOGI("mainLoop() - Enter\r\n");

    UINT32 dwRet = 1;

    // Make sure we can access Non-Volatile Memory
    if (!CRepository::Init())
    {
        //RIL_LOG_CRITICAL("mainLoop() - Could not initialize non-volatile memory access.\r\n");

        dwRet = 0;
        goto Error;
    }

    // Initialize logging class
    CRilLog::Init(g_szSIMID);

    // Initialize storage mechanism for error causes
    CModemRestart::Init();

    // Initialize helper thread that processes MMGR callbacks
    if (!CDeferThread::Init())
    {
        RIL_LOG_CRITICAL("mainLoop() - InitModemManagerEventHelpers() FAILED\r\n");

        dwRet = 0;
        goto Error;
    }

    // Create and start system manager
    if (!CSystemManager::GetInstance().InitializeSystem())
    {
        RIL_LOG_CRITICAL("mainLoop() - RIL InitializeSystem() FAILED\r\n");

        dwRet = 0;
        goto Error;
    }

    RIL_LOG_INFO("[RIL STATE] RIL INIT COMPLETED\r\n");

Error:
    if (!dwRet)
    {
        RIL_LOG_CRITICAL("mainLoop() - RIL Initialization FAILED\r\n");

        CModemRestart::Destroy();
        CDeferThread::Destroy();
        CSystemManager::Destroy();
    }

    RIL_LOG_INFO("mainLoop() - Exit\r\n");
    return (void*)dwRet;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
static void usage(char* szProgName)
{
    fprintf(stderr, "RapidRIL requires:\n");
    fprintf(stderr, "    -a <Call and Misc AT command port>\n");
    fprintf(stderr, "    -n <Network AT command port>\n");
    fprintf(stderr, "    -m <SMS, Supp Service, Call Setting AT command port>\n");
    fprintf(stderr, "    -c <SIM related, SIM Toolkit AT command port>\n");
    fprintf(stderr, "    -u <Notification channel>\n");
    fprintf(stderr, "    -o <OEM Hook channel>\n");
    fprintf(stderr, "    -d <PDP Primary Context - data channel 1>\n");
    fprintf(stderr, "    [-d <PDP Primary Context - data channel 2>]\n");
    fprintf(stderr, "    [-d <PDP Primary Context - data channel 3>]\n");
    fprintf(stderr, "    [-d <PDP Primary Context - data channel 4>]\n");
    fprintf(stderr, "    [-d <PDP Primary Context - data channel 5>]\n");
    fprintf(stderr, "    [-i <SIM ID for DSDS>]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Example in init.rc file:\n");
    fprintf(stderr, "    service ril-daemon /system/bin/rild -l %s -- -a /dev/gsmtty12"
            " -n /dev/gsmtty2 -m /dev/gsmtty6 -c /dev/gsmtty8 -u /dev/gsmtty1"
            " -o /dev/gsmtty9 -d /dev/gsmtty3 -d /dev/gsmtty4 -d /dev/gsmtty15"
            " -d /dev/gsmtty16 -d /dev/gsmtty17\n", szProgName);
    fprintf(stderr, "\n");
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
static bool RIL_SetGlobals(int argc, char** argv)
{
    int opt;
    UINT32 uiDataPortIndex = RIL_CHANNEL_DATA1;
    char* szDualSim = CSystemManager::GetInstance().m_szDualSim;

    property_get("persist.dual_sim", szDualSim , "none");
    g_uiRilChannelUpperLimit = RIL_CHANNEL_MAX;
    g_pReqInfo = (REQ_INFO*)g_ReqInfoDefault;

    while (-1 != (opt = getopt(argc, argv, "d:s:a:n:m:c:u:o:i:")))
    {
        switch (opt)
        {
            // This should be the emulator case.
            case 's':
                g_szCmdPort  = optarg;
                g_bIsSocket = TRUE;
                RLOGI("RIL_SetGlobals() - Using socket \"%s\"\r\n", g_szCmdPort);
            break;

            // This should be the non-emulator case.
            case 'a':
                g_szCmdPort = optarg;
                RLOGI("RIL_SetGlobals() - Using tty device \"%s\" for AT channel chnl=[%d] -a\r\n",
                        g_szCmdPort, RIL_CHANNEL_ATCMD);
            break;

            // This should be the non-emulator case.
            case 'n':
                g_szDLC2Port = optarg;
                RLOGI("RIL_SetGlobals() - Using tty device \"%s\" for Network channel chnl=[%d]"
                        " -n\r\n", g_szDLC2Port, RIL_CHANNEL_DLC2);
            break;

            // This should be the non-emulator case.
            case 'm':
                g_szDLC6Port = optarg;
                RLOGI("RIL_SetGlobals() - Using tty device \"%s\" for Messaging channel chnl=[%d]"
                        " -m\r\n", g_szDLC6Port, RIL_CHANNEL_DLC6);
            break;

            // This should be the non-emulator case.
            case 'c':
                g_szDLC8Port = optarg;
                RLOGI("RIL_SetGlobals() -"
                        " Using tty device \"%s\" for SIM/USIM Card channel chnl=[%d]"
                        " -c\r\n", g_szDLC8Port, RIL_CHANNEL_DLC8);
            break;

            // This should be the non-emulator case.
            case 'u':
                g_szURCPort = optarg;
                RLOGI("RIL_SetGlobals() - Using tty device \"%s\" for URC channel chnl=[%d] -u\r\n",
                        g_szURCPort, RIL_CHANNEL_URC);
            break;

            // This should be the non-emulator case.
            case 'o':
                g_szOEMPort = optarg;
                RLOGI("RIL_SetGlobals() - Using tty device \"%s\" for OEM channel chnl=[%d] -u\r\n",
                        g_szOEMPort, RIL_CHANNEL_OEM);
            break;

            // This should be the non-emulator case.
            case 'i':
                g_szSIMID = optarg;
                RLOGI("RIL_SetGlobals() - Using SIMID \"%s\" for all channels\r\n", g_szSIMID);
            break;

            // This should be the non-emulator case.
            // For multiple PDP contexts (which is default in ICS), must choose
            // more than 1 data port. Also note that 1 data port is the minimum.
            case 'd':
                if (uiDataPortIndex >= g_uiRilChannelUpperLimit)
                {
                    RLOGI("RIL_SetGlobals() - Too many RIL data channels!  uiDataPortIndex=%d,"
                            " Upper Limit=%d\r\n", uiDataPortIndex, g_uiRilChannelUpperLimit);
                    usage(argv[0]);
                    return false;
                }
                else
                {
                    switch(uiDataPortIndex)
                    {
                        case RIL_CHANNEL_DATA1:
                            g_szDataPort1 = optarg;
                            RLOGI("RIL_SetGlobals() - Using tty device \"%s\" for Data channel"
                                    " chnl=[%d] -d\r\n", g_szDataPort1, RIL_CHANNEL_DATA1);
                            break;

                        case RIL_CHANNEL_DATA2:
                            g_szDataPort2 = optarg;
                            RLOGI("RIL_SetGlobals() - Using tty device \"%s\" for Data channel"
                                    " chnl=[%d] -d\r\n", g_szDataPort2, RIL_CHANNEL_DATA2);
                            break;

                        case RIL_CHANNEL_DATA3:
                            g_szDataPort3 = optarg;
                            RLOGI("RIL_SetGlobals() - Using tty device \"%s\" for Data channel"
                                    " chnl=[%d] -d\r\n", g_szDataPort3, RIL_CHANNEL_DATA3);
                            break;

                        case RIL_CHANNEL_DATA4:
                            g_szDataPort4 = optarg;
                            RLOGI("RIL_SetGlobals() - Using tty device \"%s\" for Data channel"
                                    " chnl=[%d] -d\r\n", g_szDataPort4, RIL_CHANNEL_DATA4);
                            break;

                        case RIL_CHANNEL_DATA5:
                            g_szDataPort5 = optarg;
                            RLOGI("RIL_SetGlobals() - Using tty device \"%s\" for Data channel"
                                    " chnl=[%d] -d\r\n", g_szDataPort5, RIL_CHANNEL_DATA5);
                            break;

                        default:
                            usage(argv[0]);
                            return false;
                    }
                    uiDataPortIndex++;
                }
            break;

            default:
                usage(argv[0]);
                return false;
        }
    }

    g_uiRilChannelCurMax = uiDataPortIndex;
    if (g_uiRilChannelCurMax > g_uiRilChannelUpperLimit)
    {
        RLOGE("RIL_SetGlobals() - g_uiRilChannelCurMax = %d higher than g_uiRilChannelUpperLimit ="
                " %d\r\n", g_uiRilChannelCurMax, g_uiRilChannelUpperLimit);
        usage(argv[0]);
        return false;
    }
    else
    {
        RLOGI("RIL_SetGlobals() - g_uiRilChannelCurMax = %d  g_uiRilChannelUpperLimit = %d\r\n",
                g_uiRilChannelCurMax, g_uiRilChannelUpperLimit);
    }

    if (!g_szCmdPort || !g_szDLC2Port || !g_szDLC6Port || !g_szDLC8Port
            || !g_szURCPort || !g_szOEMPort || !g_szDataPort1 || !g_szDataPort2 || !g_szDataPort3
#if defined(M2_DUALSIM_FEATURE_ENABLED)
            || !g_szSIMID
#endif
            )
    {
        usage(argv[0]);
        return false;
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
const RIL_RadioFunctions* RIL_Init(const struct RIL_Env* pRilEnv, int argc, char** argv)
{
    RIL_LOG_INFO("RIL_Init() - Enter\r\n");

    gs_pRilEnv = pRilEnv;

    if  (RIL_SetGlobals(argc, argv))
    {
        //  Call mainLoop()
        //  This returns when init is complete.
        mainLoop(NULL);

        RIL_LOG_INFO("RIL_Init() - returning gs_callbacks\r\n");
        return &gs_callbacks;
    }
    else
    {
        RIL_LOG_CRITICAL("RIL_Init() - returning NULL\r\n");
        return NULL;
    }
}

