////////////////////////////////////////////////////////////////////////////
// silo_network.cpp
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides response handlers and parsing functions for the network-related
//    RIL components.
//
/////////////////////////////////////////////////////////////////////////////

#include <stdio.h>  // for sscanf
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>
#include <math.h>

#include "types.h"
#include "rillog.h"
#include "channel_nd.h"
#include "response.h"
#include "extract.h"
#include "callbacks.h"
#include "rildmain.h"
#include "silo_network.h"
#include "channel_data.h"
#include "data_util.h"
#include "te.h"
#include "te_base.h"
#include "oemhookids.h"
#if defined(M2_DUALSIM_FEATURE_ENABLED)
#include "repository.h"
#endif

//
//
CSilo_Network::CSilo_Network(CChannel* pChannel, CSystemCapabilities* pSysCaps)
: CSilo(pChannel, pSysCaps)
{
    RIL_LOG_VERBOSE("CSilo_Network::CSilo_Network() - Enter\r\n");

    // AT Response Table
    static ATRSPTABLE pATRspTable[] =
    {
        { "+XCSQ:"      , (PFN_ATRSP_PARSE)&CSilo_Network::ParseXCSQ         },
        { "+XCESQI:"    , (PFN_ATRSP_PARSE)&CSilo_Network::ParseXCESQI       },
        { "+CREG: "     , (PFN_ATRSP_PARSE)&CSilo_Network::ParseCREG         },
        { "+CGREG: "    , (PFN_ATRSP_PARSE)&CSilo_Network::ParseCGREG        },
        { "+XREG: "     , (PFN_ATRSP_PARSE)&CSilo_Network::ParseXREG         },
        { "+CEREG: "    , (PFN_ATRSP_PARSE)&CSilo_Network::ParseCEREG        },
        { "+CGEV: "     , (PFN_ATRSP_PARSE)&CSilo_Network::ParseCGEV         },
        { "+XDATASTAT: ", (PFN_ATRSP_PARSE)&CSilo_Network::ParseXDATASTAT    },
        { "+CTZV: "     , (PFN_ATRSP_PARSE)&CSilo_Network::ParseUnrecognized },
        { "+CTZDST: "   , (PFN_ATRSP_PARSE)&CSilo_Network::ParseUnrecognized },
        { "+XNITZINFO"  , (PFN_ATRSP_PARSE)&CSilo_Network::ParseXNITZINFO    },
        { "+PACSP1"     , (PFN_ATRSP_PARSE)&CSilo_Network::ParseUnrecognized },
        { ""            , (PFN_ATRSP_PARSE)&CSilo_Network::ParseNULL         }
    };

    m_pATRspTable = pATRspTable;

    RIL_LOG_VERBOSE("CSilo_Network::CSilo_Network() - Exit\r\n");
}

//
//
CSilo_Network::~CSilo_Network()
{
    RIL_LOG_VERBOSE("CSilo_Network::~CSilo_Network() - Enter\r\n");
    RIL_LOG_VERBOSE("CSilo_Network::~CSilo_Network() - Exit\r\n");
}


char* CSilo_Network::GetURCInitString()
{
    // network silo-related URC channel basic init string
    const char szNetworkURCInitString[] = "+CTZU=1|+XNITZINFO=1|+CGEREP=1,0";

    if (!ConcatenateStringNullTerminate(m_szURCInitString,
            sizeof(m_szURCInitString), szNetworkURCInitString))
    {
        RIL_LOG_CRITICAL("CSilo_Network::GetURCInitString() : Failed to copy URC init "
                "string!\r\n");
        return NULL;
    }

    if (CTE::GetTE().IsSignalStrengthReportEnabled())
    {
        const char* pszSignalStrengthURC = CTE::GetTE().GetSignalStrengthReportingString();
        if (NULL != pszSignalStrengthURC)
        {
            if (!ConcatenateStringNullTerminate(m_szURCInitString, sizeof(m_szURCInitString), "|"))
            {
                RIL_LOG_CRITICAL("CSilo_Network::GetURCInitString() - concat of | failed\r\n");
                return NULL;
            }

            if (!ConcatenateStringNullTerminate(m_szURCInitString, sizeof(m_szURCInitString),
                    pszSignalStrengthURC))
            {
                RIL_LOG_CRITICAL("CSilo_Network::GetURCInitString() : Failed to copy signal"
                        " strength URC string!\r\n");
                return NULL;
            }
        }
    }

    if (m_pSystemCapabilities->IsXDATASTATReportingEnabled())
    {
        if (!ConcatenateStringNullTerminate(m_szURCInitString,
                sizeof(m_szURCInitString), "|+XDATASTAT=1"))
        {
            RIL_LOG_CRITICAL("CSilo_Network::GetURCInitString() : Failed to concat "
                    "XDATASTAT to URC init string!\r\n");
            return NULL;
        }
    }
    return m_szURCInitString;
}

///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
//  Parse functions here
///////////////////////////////////////////////////////////////////////////////////////////////

//
//
BOOL CSilo_Network::ParseXNITZINFO(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Network::ParseXNITZINFO() - Enter\r\n");
    BOOL fRet = FALSE;
    const int TIME_ZONE_SIZE = 5;
    const int DATE_TIME_SIZE = 20;
    char szTimeZone[TIME_ZONE_SIZE] = {0};
    char szDateTime[DATE_TIME_SIZE] = {0};
    char* pFullName = NULL;
    UINT32 uiFullNameLength = 0;
    char* pShortName = NULL;
    UINT32 uiShortNameLength = 0;
    char* pUtf8FullName = NULL;
    char* pUtf8ShortName = NULL;

    UINT32 uiDst = 0;
    UINT32 uiHour, uiMins, uiSecs;
    UINT32 uiMonth, uiDay, uiYear;
    char* pszTimeData = NULL;
    BOOL isTimeZoneAvailable = FALSE;

    //  The notification could come in as:
    //  "<fullname>","<shortname>","<tz>","<year>/<month>/<day>,<hr>:<min>:<sec>",<dst><postfix>
    RIL_LOG_INFO("CSilo_Network::ParseXNITZINFO() - string rszPointer: %s", rszPointer);

    if (NULL == pResponse)
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseXNITZINFO() - pResponse is NULL\r\n");
        goto Error;
    }

    pResponse->SetUnsolicitedFlag(TRUE);

    //  Skip  ":",
    if (!SkipString(rszPointer,":", rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseXNITZINFO() - Skip :\r\n");
        goto Error;
    }

    //  Parse the "<fullname>",
    if (!ExtractQuotedStringWithAllocatedMemory(rszPointer, pFullName, uiFullNameLength,
            rszPointer) || !SkipString(rszPointer, ",", rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseXNITZINFO() - Could not extract the Full Format"
                " Operator Name.\r\n");
        goto Error;
    }

    pUtf8FullName = ConvertUCS2ToUTF8(pFullName, uiFullNameLength - 1);
    RIL_LOG_INFO("CSilo_Network::ParseXNITZINFO() - Long oper: \"%s\"\r\n",
            pUtf8FullName ? pUtf8FullName : "NULL");

    //  Parse the "<Shortname>"
    if (!ExtractQuotedStringWithAllocatedMemory(rszPointer, pShortName, uiShortNameLength,
            rszPointer) || !SkipString(rszPointer, ",", rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseXNITZINFO() - Could not extract the Short Format"
                " Operator Name.\r\n");
        goto Error;
    }

    pUtf8ShortName = ConvertUCS2ToUTF8(pShortName, uiShortNameLength - 1);
    RIL_LOG_INFO("CSilo_Network::ParseXNITZINFO() - Short oper: \"%s\"\r\n",
            pUtf8ShortName ? pUtf8ShortName: "NULL");

    //  Parse the <tz>,
    if (!ExtractQuotedString(rszPointer, szTimeZone, TIME_ZONE_SIZE, rszPointer) ||
            !SkipString(rszPointer, ",", rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseXNITZINFO() - Unable to find time zone!\r\n");
        goto Error;
    }

    if (MODEM_TYPE_XMM6360 == CTE::GetTE().GetModemType()
            || MODEM_TYPE_XMM7160 == CTE::GetTE().GetModemType())
    {
        // WORAROUND : BZ28102.
        // TZ = -1h : szTimeZone = "0-4" -> "-04"
        // TZ = -4h : szTimeZone = "-16" -> "-16"
        // TZ = +1h : szTimeZone = "004" -> "+04"
        // TZ = +4h : szTimeZone = "016" -> "+16"
        if (strlen(szTimeZone) > 0)
        {
            if ((szTimeZone[0] == '0') && ((szTimeZone[1] == '-')))
            {
                szTimeZone[0] = '-';
                szTimeZone[1] = '0';
            }
            else if ((szTimeZone[0] == '0') && ((szTimeZone[1] != '-')))
            {
                szTimeZone[0] = '+';
            }
        }
    }

    RIL_LOG_INFO("CSilo_Network::ParseXNITZINFO() - szTimeZone: \"%s\"\r\n", szTimeZone);

    // Extract "<date,time>"
    if (!ExtractQuotedString(rszPointer, szDateTime, DATE_TIME_SIZE, rszPointer) ||
            !SkipString(rszPointer, ",", rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseXNITZINFO() - Unable to find date/time string!\r\n");
        goto Error;
    }
    RIL_LOG_INFO("CSilo_Network::ParseXNITZINFO() - szDateTime: \"%s\"\r\n", szDateTime);

    //  Extract <dst>
    if (!ExtractUInt32(rszPointer, uiDst, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseXNITZINFO() - Cannot parse <dst>\r\n");
        goto Error;
    }
    RIL_LOG_INFO("CSilo_Network::ParseXNITZINFO() - uiDst: \"%u\"\r\n", uiDst);

    if (strlen(szTimeZone) > 0)
    {
        isTimeZoneAvailable = TRUE;
    }

    // If the "<date,time>" (szDateTime) is not empty.
    if (strlen(szDateTime) != 0)
    {
        // Scan the Date and the Time in the szDateTime buffer to check the date and time.
        int num = sscanf(szDateTime, "%2d/%2d/%2d,%2d:%2d:%2d", &uiYear, &uiMonth, &uiDay, &uiHour,
                &uiMins, &uiSecs);

        // Check the elements number of the date and time.
        if (num != 6)
        {
            RIL_LOG_CRITICAL("CSilo_Network::ParseXNITZINFO() - bad element number in the scan"
                    " szDateTime\r\n");
            goto Error;
        }

        // Check the coherence of the date and time.
        if ((uiMonth > 12) || (uiDay > 31) || (uiHour > 24) || (uiMins > 59) || (uiSecs > 59))
        {
            RIL_LOG_CRITICAL("CSilo_Network::ParseXNITZINFO() - bad date time\r\n");
            goto Error;
        }
    }
    // If the "<date,time>" (szDateTime) is empty.
    else
    {
        /*
         * If timezone is available, then framework expects the RIL to provide
         * the time expressed in UTC(or GMT timzone).
         */
        if (isTimeZoneAvailable)
        {
            time_t ctime_secs;
            struct tm* pGMT;

            // Read the current time.
            ctime_secs = time(NULL);

            // Ckeck ctime_secs.
            if (-1 == ctime_secs)
            {
                RIL_LOG_CRITICAL("CSilo_Network::ParseXNITZINFO() - Unable to convert local to"
                        " calendar time!\r\n");
                //  Just skip over notification
                goto Error;
            }

            pGMT = gmtime(&ctime_secs);
            // Ckeck pGMT.
            if (NULL == pGMT)
            {
                RIL_LOG_CRITICAL("CSilo_Network::ParseXNITZINFO() - pGMT is NULL!\r\n");
                //  Just skip over notification
                goto Error;
            }

            // Insert the date/time as "yy/mm/dd,hh:mm:ss"
            strftime(szDateTime, DATE_TIME_SIZE, "%y/%m/%d,%T", pGMT);
        }
    }

    // Check the dst.
    if (uiDst > 2)
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseXNITZINFO() - bad dst\r\n");
        goto Error;
    }

    /*
     * As per the 3GPP 24.008 specification, if the local time zone has been
     * adjusted for Daylight Saving Time, the network shall indicate this by
     * including the IE Network Daylight Saving Time. This means the DST is
     * always received along with the TimeZone. So, for the case where only
     * time is received but not time zone, no need to update the NITZ to framework.
     */
    if  (isTimeZoneAvailable)
    {
        pszTimeData = (char*)malloc(sizeof(char) * MAX_BUFFER_SIZE);
        if (NULL == pszTimeData)
        {
            RIL_LOG_CRITICAL("CSilo_Network::ParseXNITZINFO() - Could not allocate memory for"
                    " pszTimeData.\r\n");
            goto Error;
        }
        memset(pszTimeData, 0, sizeof(char) * MAX_BUFFER_SIZE);

        // Insert the date/time as "yy/mm/dd,hh:mm:ss".
        // Add tz: "+-xx", dst: ",x"
        if (snprintf(pszTimeData, MAX_BUFFER_SIZE-1, "%s%s,%u", szDateTime, szTimeZone,
                uiDst) == 0)
        {
            RIL_LOG_CRITICAL("CSilo_Network::ParseXNITZINFO() - snprintf pszTimeData buffer\r\n");
            goto Error;
        }
        pszTimeData[MAX_BUFFER_SIZE-1] = '\0';  //  KW fix
        RIL_LOG_INFO("CSilo_Network::ParseXNITZINFO() - INFO: pszTimeData: %s\r\n", pszTimeData);

        pResponse->SetResultCode(RIL_UNSOL_NITZ_TIME_RECEIVED);
        if (!pResponse->SetData((void*)pszTimeData, sizeof(char) * MAX_BUFFER_SIZE, FALSE))
        {
            goto Error;
        }
    }
    else
    {
        pResponse->SetUnrecognizedFlag(TRUE);
    }

    /*
     * Completing RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED will result in
     * framework triggering the RIL_REQUEST_OPERATOR request.
     */
    if ((NULL != pUtf8FullName && 0 < strlen(pUtf8FullName)) ||
                                (NULL != pUtf8ShortName && 0 < strlen(pUtf8ShortName)))
    {
        RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED, NULL, 0);
    }

    fRet = TRUE;
Error:
    if (!fRet)
    {
        free(pszTimeData);
        pszTimeData = NULL;
    }

    delete[] pUtf8FullName;
    pUtf8FullName = NULL;

    delete[] pUtf8ShortName;
    pUtf8ShortName = NULL;

    delete[] pFullName;
    pFullName = NULL;

    delete[] pShortName;
    pShortName = NULL;

    RIL_LOG_VERBOSE("CSilo_Network::ParseXNITZINFO() - Exit\r\n");
    return fRet;
}

// Note that this function will get called for Solicited and Unsolicited
// responses that start with "+CGREG: ". The function will determine whether
// or not it should parse the response (using the number of arguments in the
//  response) and will proceed appropriately.
//
BOOL CSilo_Network::ParseRegistrationStatus(CResponse* const pResponse, const char*& rszPointer,
                                            int regType)
{
    RIL_LOG_VERBOSE("CSilo_Network::ParseRegistrationStatus() - Enter\r\n");

    const char* szDummy;
    BOOL   fRet = FALSE, fUnSolicited = TRUE;
    int nNumParams = 1;
    char* pszCommaBuffer = NULL;  //  Store notification in here.
                                  //  Note that cannot loop on rszPointer as it may contain
                                  //  other notifications as well.

    S_ND_GPRS_REG_STATUS psRegStatus;
    S_ND_REG_STATUS csRegStatus;
    void* pRegStatusInfo = NULL;

    if (NULL == pResponse)
    {
        goto Error;
    }

    // Look for a "<postfix>"
    if (!FindAndSkipRspEnd(rszPointer, m_szNewLine, szDummy))
    {
        // This isn't a complete registration notification -- no need to parse it
        goto Error;
    }

    pszCommaBuffer = new char[szDummy-rszPointer+1];
    if (!pszCommaBuffer)
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseRegistrationStatus() -"
                " cannot allocate pszCommaBuffer\r\n");
        goto Error;
    }

    memset(pszCommaBuffer, 0, szDummy-rszPointer+1);
    strncpy(pszCommaBuffer, rszPointer, szDummy-rszPointer);
    pszCommaBuffer[szDummy-rszPointer] = '\0'; // Klokworks fix

    //RIL_LOG_INFO("pszCommaBuffer=[%s]\r\n", CRLFExpandedString(pszCommaBuffer,szDummy-rszPointer).GetString() );


    // Valid XREG notifications can have from one to five parameters, as follows:
    //       <status>                               for an unsolicited notification without location data
    //       <status>, <AcT>, <Band>                for an unsolicited notification without location data
    //  <n>, <status>, <AcT>, <Band>                for a command response without location data
    //       <status>, <AcT>, <Band>, <lac>, <ci>   for an unsolicited notification with location data
    //  <n>, <status>, <AcT>, <Band>, <lac>, <ci>   for a command response with location data

    // Valid CREG notifications can have from one to five parameters, as follows:
    //       <status>                      for an unsolicited notification without location data
    //  <n>, <status>                      for a command response without location data
    //       <status>, <lac>, <ci>, <AcT>  for an unsolicited notification with location data
    //  <n>, <status>, <lac>, <ci>, <AcT>  for a command response with location data

    // Valid CGREG notifications can have from one to six parameters, as follows:
    //       <status>                             for an unsolicited notification without location data
    //  <n>, <status>                             for a command response without location data
    //       <status>, <lac>, <ci>, <AcT>, <rac>  for an unsolicited notification with location data
    //  <n>, <status>, <lac>, <ci>, <AcT>, <rac>  for a command response with location data

    //  3GPP TS 27.007 version 5.4.0 release 5 section 7.2
    //  <n>      valid values: 0, 1, 2
    //  <status> valid values: 0, 1, 2, 3, 4, 5
    //  <lac>    string type, in hex
    //  <ci>     string type, in hex
    //  <AcT>    valid values: 0=GSM, 1=GSM Compact, 2=UTRAN
    //  <rac>    (routing area code) string type, in hex (one byte)


    //  Loop and count parameters (separated by comma)
    szDummy = pszCommaBuffer;
    while(FindAndSkipString(szDummy, ",", szDummy))
    {
        nNumParams++;
    }
    //RIL_LOG_INFO("CSilo_Network::ParseRegistrationStatus() - nNumParams=[%d]\r\n", nNumParams);


    if (E_REGISTRATION_TYPE_CGREG == regType)
    {
        //  The +CGREG case
        //  Unsol is 1,5 and 7
        if ((1 == nNumParams) || (5 == nNumParams) || (7 == nNumParams))
        {
            fUnSolicited = TRUE;
        }
        else if ((2 == nNumParams) || (6 == nNumParams) || (8 == nNumParams))
        {
            //  Sol is 2,6 and 8
            fUnSolicited = FALSE;
        }
        else
        {
            RIL_LOG_CRITICAL("CSilo_Network::ParseRegistrationStatus() -"
                    " GPRS Unknown param count=%d\r\n", nNumParams);
            fUnSolicited = TRUE;
        }
        RIL_LOG_INFO("CSilo_Network::ParseRegistrationStatus() - CGREG paramcount=%d"
                "  fUnsolicited=%d\r\n", nNumParams, fUnSolicited);
    }
    else if (E_REGISTRATION_TYPE_CREG == regType)
    {
        //  The +CREG case
        //  Unsol is 1, 4 and 6
        if ((1 == nNumParams) || (4 == nNumParams) || (6 == nNumParams))
        {
            fUnSolicited = TRUE;
        }
        else if ((2 == nNumParams) || (5 == nNumParams) || (7 == nNumParams))
        {
            // sol is 2,5 and 7
            fUnSolicited = FALSE;
        }
        else
        {
            RIL_LOG_CRITICAL("CSilo_Network::ParseRegistrationStatus() -"
                    " Unknown param count=%d\r\n", nNumParams);
            fUnSolicited = TRUE;
        }
        RIL_LOG_INFO("CSilo_Network::ParseRegistrationStatus() - CREG paramcount=%d"
                "  fUnsolicited=%d\r\n", nNumParams, fUnSolicited);
    }
    else if (E_REGISTRATION_TYPE_CEREG == regType)
    {
        //  The +CEREG case
        //  Unsol is 1, 4 and 6
        if ((1 == nNumParams) || (4 == nNumParams) || (6 == nNumParams))
        {
            fUnSolicited = TRUE;
        }
        else if ((2 == nNumParams) || (5 == nNumParams) || (7 == nNumParams))
        {
            // sol is 2,5 and 7
            fUnSolicited = FALSE;
        }
        else
        {
            RIL_LOG_CRITICAL("CSilo_Network::ParseRegistrationStatus() -"
                    " Unknown param count=%d\r\n", nNumParams);
            fUnSolicited = TRUE;
        }
        RIL_LOG_INFO("CSilo_Network::ParseRegistrationStatus() - CEREG paramcount=%d"
                "  fUnsolicited=%d\r\n", nNumParams, fUnSolicited);

    }
    else if (E_REGISTRATION_TYPE_XREG == regType)
    {
        //  The +XREG case
        //  Unsol is 1,3,5 and 8
        if ((1 == nNumParams) || (3 == nNumParams) || (5 == nNumParams) || (8 == nNumParams))
        {
            fUnSolicited = TRUE;
        }
#if defined(M2_DUALSIM_FEATURE_ENABLED)
        else if (4 == nNumParams)
        {
            CRepository repository;

            // Temporary Out of Service Notifications are only supported on
            //  DSDS modems and therefore must not be active by default
            const int TEMP_OUT_OF_SERVICE_EN_DEFAULT = 0;
            int nEnableTempOoSNotif = TEMP_OUT_OF_SERVICE_EN_DEFAULT;

            if (!repository.Read(g_szGroupModem, g_szTempOoSNotificationEnable,
                    nEnableTempOoSNotif))
            {
                nEnableTempOoSNotif = TEMP_OUT_OF_SERVICE_EN_DEFAULT;
            }

            if (nEnableTempOoSNotif)
            {
                // handle special case for Temporary Out of Service Notification
                fRet = ParseXREGFastOoS(pResponse, rszPointer);
            }
            else
            {
                // Temporary Out of Service Notifications not supported
                fRet = ParseUnrecognized(pResponse, rszPointer);
            }

            delete[] pszCommaBuffer;
            pszCommaBuffer = NULL;

            RIL_LOG_VERBOSE("CSilo_Network::ParseRegistrationStatus() - Exit\r\n");
            return fRet;
        }
        else if ((6 == nNumParams)) //  Sol is 6
#else
        //  Sol is 2,4,6 and 9
        else if ((2 == nNumParams) || (4 == nNumParams) || (6 == nNumParams) || (9 == nNumParams))
#endif // M2_DUALSIM_FEATURE_ENABLED
        {
            //  Sol is 2,4,6 and 9
            fUnSolicited = FALSE;
        }
        else
        {
            RIL_LOG_CRITICAL("CSilo_Network::ParseRegistrationStatus() -"
                    " Unknown param count=%d\r\n", nNumParams);
            fUnSolicited = TRUE;
        }

        RIL_LOG_INFO("CSilo_Network::ParseRegistrationStatus() - XREG paramcount=%d"
                "  fUnsolicited=%d\r\n", nNumParams, fUnSolicited);
    }

    if (fUnSolicited)
    {
        pResponse->SetUnsolicitedFlag(TRUE);

        if (E_REGISTRATION_TYPE_CGREG == regType)
        {
            fRet = CTE::GetTE().ParseCGREG(rszPointer, fUnSolicited, psRegStatus);
            pRegStatusInfo = (void*) &psRegStatus;
        }
        else if (E_REGISTRATION_TYPE_CREG == regType)
        {
            fRet = CTE::GetTE().ParseCREG(rszPointer, fUnSolicited, csRegStatus);
            pRegStatusInfo = (void*) &csRegStatus;
        }
        else if (E_REGISTRATION_TYPE_XREG == regType)
        {
            S_REG_INFO previousRegInfo;

            CTE::GetTE().GetPreviousGprsRegInfo(previousRegInfo);

            fRet = CTE::GetTE().ParseXREG(rszPointer, fUnSolicited, psRegStatus);
            pRegStatusInfo = (void*) &psRegStatus;

            if (fRet)
            {
                // if nothing changed or only band has changed
                if ((0 == strncmp(previousRegInfo.szState,
                        psRegStatus.szStat, MAX_REG_STATUS_LENGTH))
                        && (0 == strncmp(previousRegInfo.szAcT,
                        psRegStatus.szNetworkType, MAX_REG_STATUS_LENGTH))
                        && (0 == strncmp(previousRegInfo.szLAC,
                        psRegStatus.szLAC, MAX_REG_STATUS_LENGTH))
                        && (0 == strncmp(previousRegInfo.szCID,
                        psRegStatus.szCID, MAX_REG_STATUS_LENGTH)) )
                {
                    pResponse->SetIgnoreFlag(TRUE);
                    goto Error;
                }
            }
        }
        else if (E_REGISTRATION_TYPE_CEREG == regType)
        {
            fRet = CTE::GetTE().ParseCEREG(rszPointer, fUnSolicited, psRegStatus);
            pRegStatusInfo = (void*) &psRegStatus;
        }

        if (fRet)
        {
            CTE::GetTE().StoreRegistrationInfo(pRegStatusInfo, regType);
            pResponse->SetResultCode(RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED);
        }

        fRet = TRUE;
    }
    else
    {
        fRet = FALSE;
        goto Error;
    }


    fRet = TRUE;
Error:
    delete[] pszCommaBuffer;
    pszCommaBuffer = NULL;
    RIL_LOG_VERBOSE("CSilo_Network::ParseRegistrationStatus() - Exit\r\n");
    return fRet;
}

//
//
BOOL CSilo_Network::ParseCREG(CResponse* const pResponse, const char*& rszPointer)
{
    char szBackup[MAX_NETWORK_DATA_SIZE] = {0};
    const char* pszStart = NULL;
    const char* pszEnd = NULL;

    RIL_LOG_VERBOSE("CSilo_Network::ParseCREG() - Enter / Exit\r\n");

    // Backup the CREG response string to report data on crashtool
    pszStart = rszPointer;
    ExtractUnquotedString(pszStart, m_szNewLine, szBackup, MAX_NETWORK_DATA_SIZE, pszEnd);
    CTE::GetTE().SaveNetworkData(LAST_NETWORK_CREG, szBackup);

    return ParseRegistrationStatus(pResponse, rszPointer, E_REGISTRATION_TYPE_CREG);
}

//
//
BOOL CSilo_Network::ParseCGREG(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Network::ParseCGREG() - Enter / Exit\r\n");
    return ParseRegistrationStatus(pResponse, rszPointer, E_REGISTRATION_TYPE_CGREG);
}

//
//
BOOL CSilo_Network::ParseXREG(CResponse* const pResponse, const char*& rszPointer)
{
    char szBackup[MAX_NETWORK_DATA_SIZE] = {0};
    const char* pszStart = NULL;
    const char* pszEnd = NULL;

    RIL_LOG_VERBOSE("CSilo_Network::ParseXREG() - Enter / Exit\r\n");

    // Backup the XREG response string to report data on crashtool
    pszStart = rszPointer;
    ExtractUnquotedString(pszStart, m_szNewLine, szBackup, MAX_NETWORK_DATA_SIZE, pszEnd);
    CTE::GetTE().SaveNetworkData(LAST_NETWORK_XREG, szBackup);

    return ParseRegistrationStatus(pResponse, rszPointer, E_REGISTRATION_TYPE_XREG);
}

//
//
BOOL CSilo_Network::ParseCEREG(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Network::ParseCEREG() - Enter / Exit\r\n");
    return ParseRegistrationStatus(pResponse, rszPointer, E_REGISTRATION_TYPE_CEREG);
}

//
//
BOOL CSilo_Network::ParseCGEV(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_INFO("CSilo_Network::ParseCGEV() - Enter\r\n");

    BOOL bRet = FALSE;
    const char* szStrExtract = NULL;
    const char* szResponse = NULL;
    char* pszCommaBuffer = NULL;
    UINT32 uiPCID = 0;
    UINT32 uiCID = 0;
    UINT32 uiReason = 0;
    UINT32 uiEvent = 0;
    UINT32 uiLength = 0;
    CChannel_Data* pChannelData = NULL;
    sOEM_HOOK_RAW_UNSOL_MT_CLASS_IND* pData = NULL;
    REQUEST_DATA rReqData;
    UINT32 uiTemp = 0;

    szResponse = rszPointer;

    if (NULL == pResponse)
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseCGEV() - pResponse is NULL\r\n");
        goto Error;
    }

    pResponse->SetUnsolicitedFlag(TRUE);

    if (!FindAndSkipRspEnd(rszPointer, m_szNewLine, szResponse))
    {
        // This isn't a complete registration notification -- no need to parse it
        RIL_LOG_CRITICAL("CSilo_Network::ParseCGEV() - This isn't a complete registration"
                " notification -- no need to parse it\r\n");
        goto Error;
    }

    //  Back up over the "\r\n".
    szResponse -= strlen(m_szNewLine);

    // Allocate +CGEV string buffer
    uiLength = szResponse - rszPointer;
    pszCommaBuffer = new char[uiLength + 1];
    if (!pszCommaBuffer)
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseCGEV() - cannot allocate pszCommaBuffer\r\n");
        goto Error;
    }

    // Extract +CGEV string buffer
    memset(pszCommaBuffer, 0, uiLength + 1);
    strncpy(pszCommaBuffer, rszPointer, uiLength);
    pszCommaBuffer[uiLength] = '\0'; // Klockworks fix

    szStrExtract = pszCommaBuffer;
    //  Format is "ME PDN ACT, <cid>[, <reason>]"
    if (FindAndSkipString(szStrExtract, "ME PDN ACT", szStrExtract))
    {
        if (!ExtractUInt32(szStrExtract, uiPCID, szStrExtract))
        {
            goto Error;
        }
        else
        {
            RIL_LOG_INFO("CSilo_Network::ParseCGEV() - ME PDN ACT , extracted pcid=[%u]\r\n",
                    uiPCID);
        }

        pChannelData = CChannel_Data::GetChnlFromContextID(uiPCID);
        if (NULL == pChannelData)
        {
            const int DEFAULT_DATA_PROFILE = 0;

            // This is possible for Default PDN
            pChannelData = CChannel_Data::GetFreeChnlsRilHsi(uiPCID, DEFAULT_DATA_PROFILE);
            if (NULL != pChannelData)
            {
                pChannelData->SetDataState(E_DATA_STATE_INITING);
                RIL_requestTimedCallback(triggerQueryDefaultPDNContextParams,
                        (void*)pChannelData, 0, 0);
            }
        }

        if (NULL != pChannelData)
        {
            // Reset fail cause
            pChannelData->SetDataFailCause(PDP_FAIL_NONE);
        }

        if (FindAndSkipString(szStrExtract, ",", szStrExtract))
        {
            if (!ExtractUInt32(szStrExtract, uiReason, szStrExtract))
            {
                RIL_LOG_CRITICAL("CSilo_Network::ParseCGEV() - Couldn't extract reason\r\n");
                goto Error;
            }
            else
            {
                RIL_LOG_INFO("CSilo_Network::ParseCGEV() - ME PDN ACT , extracted reason=[%u]\r\n",
                        uiReason);

                int failCause;
                pResponse->SetUnsolicitedFlag(TRUE);

                // IPV4 only allowed
                if (uiReason == 0)
                {
                    pResponse->SetResultCode(RRIL_RESULT_OK);
                    failCause = PDP_FAIL_ONLY_IPV4_ALLOWED;
                }
                // IPV6 only allowed
                else if (uiReason == 1)
                {
                    pResponse->SetResultCode(RRIL_RESULT_OK);
                    failCause = PDP_FAIL_ONLY_IPV6_ALLOWED;
                }
                // Single bearrer allowed
                else if (uiReason == 2)
                {
                    pResponse->SetResultCode(RRIL_RESULT_OK);
                    failCause = PDP_FAIL_ONLY_SINGLE_BEARER_ALLOWED;
                }
                else
                {
                    RIL_LOG_CRITICAL("CSilo_Network::ParseCGEV() - reason unknown\r\n");
                    failCause = PDP_FAIL_NONE;
                }

                if (NULL != pChannelData)
                {
                    pChannelData->SetDataFailCause(failCause);
                }

                if (PDP_FAIL_ERROR_UNSPECIFIED == failCause)
                {
                    goto Error;
                }
            }
        }
    }
    // Format: "NW ACT <p_cid>, <cid>, <event_type>.
    else if (FindAndSkipString(szStrExtract, "NW ACT", szStrExtract))
    {
        if (!ExtractUInt32(szStrExtract, uiPCID, szStrExtract))
        {
            goto Error;
        }
        else
        {
            RIL_LOG_INFO("CSilo_Network::ParseCGEV() - NW ACT , extracted pcid=[%u]\r\n",
                    uiPCID);

            if (FindAndSkipString(szStrExtract, ",", szStrExtract))
            {
                if (!ExtractUInt32(szStrExtract, uiCID, szStrExtract))
                {
                    RIL_LOG_CRITICAL("CSilo_Network::ParseCGEV() - couldn't extract cid\r\n");
                    goto Error;
                }
            }
            else
            {
                RIL_LOG_CRITICAL("CSilo_Network::ParseCGEV() - couldn't extract cid\r\n");
                goto Error;
            }
            RIL_LOG_INFO("CSilo_Network::ParseCGEV() - NW ACT, extracted cid=[%u]\r\n",
                    uiCID);
            if (FindAndSkipString(szStrExtract, ",", szStrExtract))
            {
                if (!ExtractUInt32(szStrExtract, uiEvent, szStrExtract))
                {
                    RIL_LOG_CRITICAL("CSilo_Network::ParseCGEV() - Couldn't extract event\r\n");
                    goto Error;
                }
            }

            RIL_LOG_INFO("CSilo_Network::ParseCGEV() - NW ACT, extracted event=[%u]\r\n",
                            uiEvent);

            if (1 == uiEvent)
            {
                /*
                 * Note: This is a temporary solution to get the conformance test case
                 * passed. If the NW initiated connection needs to be used across applications
                 * then framework should be informed once the NW intiated context is activated.
                 */
                CTE::GetTE().AcceptOrRejectNwInitiatedContext();
            }

            pChannelData = CChannel_Data::GetChnlFromContextID(uiPCID);
            if (NULL != pChannelData)
            {
                pChannelData->GetHSIChannel();
                void** callbackParams = new void*[3];
                if (callbackParams != NULL) {
                    callbackParams[0] = (void*)uiPCID;
                    callbackParams[1] = (void*)uiCID;
                    callbackParams[2] = (void*)pChannelData;
                    RIL_requestTimedCallback(triggerQueryBearerParams,
                            (void*)callbackParams, 0, 0);
                }
                else
                {
                    RIL_LOG_CRITICAL("CSilo_Network::ParseCGEV() - "
                            "cannot allocate callbackParams\r\n");
                    goto Error;
                }
            }
            else
            {
                RIL_LOG_CRITICAL("CSilo_Network::ParseCGEV() - Invalid PCID=[%u],"
                        " no data channel found!\r\n", uiPCID);
                goto Error;
            }
        }
    }
    // Format: "ME ACT <p_cid>, <cid>, <event_type>. Unsupported.
    else if (FindAndSkipString(szStrExtract, "ME ACT", szStrExtract))
    {
        // nothing to do, since secondary PDP contexts are not supported
        RIL_LOG_INFO("CSilo_Network::ParseCGEV(): ME ACT event for secondary PDP "
                "context ignored (unsupported)!\r\n");
    }
    else if (FindAndSkipString(szStrExtract, "NW CLASS", szStrExtract) ||
            FindAndSkipString(szStrExtract, "ME CLASS", szStrExtract))
    {
        int mt_class = 0;

        RIL_LOG_INFO("CSilo_Network::ParseCGEV() - NW CLASS/ME CLASS\r\n");

        if (FindAndSkipString(rszPointer, "NW CLASS A", rszPointer) ||
            FindAndSkipString(rszPointer, "ME CLASS A", rszPointer))
        {
            mt_class = E_MT_CLASS_A;
        }
        else if (FindAndSkipString(rszPointer, "NW CLASS B", rszPointer) ||
                FindAndSkipString(rszPointer, "ME CLASS B", rszPointer))
        {
            mt_class = E_MT_CLASS_B;
        }
        else if (FindAndSkipString(rszPointer, "NW CLASS CG", rszPointer) ||
                FindAndSkipString(rszPointer, "ME CLASS CG", rszPointer))
        {
            mt_class = E_MT_CLASS_CG;
        }
        else if (FindAndSkipString(rszPointer, "NW CLASS CC", rszPointer) ||
                FindAndSkipString(rszPointer, "ME CLASS CC", rszPointer))
        {
            mt_class = E_MT_CLASS_CC;
        }

        pData = (sOEM_HOOK_RAW_UNSOL_MT_CLASS_IND*)malloc(
                sizeof(sOEM_HOOK_RAW_UNSOL_MT_CLASS_IND));
        if (NULL == pData)
        {
            RIL_LOG_CRITICAL("CSilo_Network::ParseCGEV() -"
                    " Could not allocate memory for pData.\r\n");
            goto Error;
        }
        memset(pData, 0, sizeof(sOEM_HOOK_RAW_UNSOL_MT_CLASS_IND));

        pData->command = RIL_OEM_HOOK_RAW_UNSOL_MT_CLASS_IND;
        pData->mt_class = mt_class;

        pResponse->SetResultCode(RIL_UNSOL_OEM_HOOK_RAW);

        if (!pResponse->SetData((void*)pData, sizeof(sOEM_HOOK_RAW_UNSOL_MT_CLASS_IND),
                FALSE))
        {
            goto Error;
        }
    }
    else if (FindAndSkipString(szStrExtract, "ME DETACH", szStrExtract) ||
            FindAndSkipString(szStrExtract, "NW DETACH", szStrExtract))
    {
        RIL_LOG_INFO("CSilo_Network::ParseCGEV(): ME or NW DETACH");

        CTE::GetTE().CleanupAllDataConnections();
        RIL_onUnsolicitedResponse(RIL_UNSOL_DATA_CALL_LIST_CHANGED, NULL, 0);
    }
    // see new format: "ME PDN DEACT" below
    else if (FindAndSkipString(szStrExtract, "ME DEACT", szStrExtract))
    {
        RIL_LOG_INFO("CSilo_Network::ParseCGEV(): ME DEACT");

        // We need to differentiate between former format
        // "ME DEACT <PDP_type>, <PDP_addr>, [<cid>]" and new format for
        // secondary PDP contexts "ME DEACT <p_cid>, <cid>, <event_type>

        // If first parameter is a string, former format
        if (!ExtractUInt32(szStrExtract, uiTemp, szStrExtract))
        {
            if (GetContextIdFromDeact(szStrExtract, uiPCID))
            {
                RIL_LOG_INFO("CSilo_Network::ParseCGEV(): ME DEACT CID- %u", uiPCID);

                HandleMEDeactivation(uiPCID);
            }
        }
        else // Otherwise, must be format for secondary PDP context
        {
            // nothing to do, since secondary PDP contexts are not supported
            RIL_LOG_INFO("CSilo_Network::ParseCGEV(): ME DEACT event for secondary PDP "
                    "context ignored (unsupported)!\r\n");
        }
    }
    // see new format: "NW DEACT" below
    else if (FindAndSkipString(szStrExtract, "NW DEACT", szStrExtract))
    {
        RIL_LOG_INFO("CSilo_Network::ParseCGEV(): NW DEACT");

        HandleNwDeact(szStrExtract);
    }
    // Format: "ME PDN DEACT <cid>"
    else if (FindAndSkipString(szStrExtract, "ME PDN DEACT", szStrExtract))
    {
        RIL_LOG_INFO("CSilo_Network::ParseCGEV(): ME PDN DEACT");

        if (!ExtractUInt32(szStrExtract, uiPCID, szStrExtract))
        {
            RIL_LOG_CRITICAL("CSilo_Network::ParseCGEV() - ME PDN DEACT, couldn't "
                    "extract cid\r\n");
            goto Error;
        }
        else
        {
            RIL_LOG_INFO("CSilo_Network::ParseCGEV() - ME PDN DEACT, extracted "
                    "cid=[%u]\r\n", uiPCID);

            HandleMEDeactivation(uiPCID);
        }
    }
    // Format: "NW PDN DEACT, <cid>"
    else if (FindAndSkipString(szStrExtract, "NW PDN DEACT", szStrExtract))
    {
        RIL_LOG_INFO("CSilo_Network::ParseCGEV(): NW PDN DEACT");

        if (!ExtractUInt32(szStrExtract, uiPCID, szStrExtract))
        {
            RIL_LOG_CRITICAL("CSilo_Network::ParseCGEV() - NW PDN DEACT, couldn't "
                    "extract cid\r\n");
            goto Error;
        }
        else
        {
            RIL_LOG_INFO("CSilo_Network::ParseCGEV() - NW PDN DEACT, extracted "
                    "cid=[%u]\r\n", uiPCID);

            pChannelData = CChannel_Data::GetChnlFromContextID(uiPCID);
            if (NULL == pChannelData)
            {
                //  This may occur using AT proxy during 3GPP conformance testing.
                //  Let normal processing occur.
                RIL_LOG_CRITICAL("CSilo_Network::ParseCGEV() - Invalid CID=[%u],"
                        " no data channel found!\r\n", uiPCID);
            }
            else
            {
                pChannelData->SetDataState(E_DATA_STATE_DEACTIVATED);
                /*
                 * @TODO: If fail cause is provided as part of NW PDN DEACT,
                 * map the fail cause to ril cause values and set it.
                 */
                pChannelData->SetDataFailCause(PDP_FAIL_ERROR_UNSPECIFIED);

                CTE::GetTE().DataConfigDown(uiPCID, TRUE);

                CTE::GetTE().CompleteDataCallListChanged();
            }
        }
    }

    bRet = TRUE;
Error:
    if (!bRet)
    {
        free(pData);
        pData = NULL;
    }

    rszPointer = szResponse;
    delete[] pszCommaBuffer;
    pszCommaBuffer = NULL;
    RIL_LOG_INFO("CSilo_Network::ParseCGEV() - Exit\r\n");
    return bRet;
}

BOOL CSilo_Network::GetContextIdFromDeact(const char* pData, UINT32& uiCID)
{
    RIL_LOG_INFO("CSilo_Network::GetContextIdFromDeact() - Enter\r\n");

    char szPDPAddr[MAX_IPADDR_SIZE] = {'\0'};
    BOOL bRet = FALSE;
    if (!FindAndSkipString(pData, ",", pData) ||
        !ExtractQuotedString(pData, szPDPAddr, MAX_IPADDR_SIZE, pData))
    {
        RIL_LOG_CRITICAL("CSilo_Network::GetContextIdFromDeact() -"
                " Could not extract ipaddress.\r\n");
        goto Error;
    }

    if (!FindAndSkipString(pData, ",", pData))
    {
        RIL_LOG_CRITICAL("CSilo_Network::GetContextIdFromDeact() - context id not provided.\r\n");
        goto Error;
    }

    if (!ExtractUInt32(pData, uiCID, pData))
    {
        RIL_LOG_CRITICAL("CSilo_Network::GetContextIdFromDeact() - Could not extract cid.\r\n");
        goto Error;
    }
    else
    {
        RIL_LOG_INFO("CSilo_Network::GetContextIdFromDeact() - extracted cid=[%u]\r\n",
                                                uiCID);
    }

    bRet = TRUE;

Error:
    RIL_LOG_INFO("CSilo_Network::GetContextIdFromDeact() - Exit\r\n");
    return bRet;
}

void CSilo_Network::HandleNwDeact(const char*& szStrExtract)
{
    UINT32 uiPCID = 0;
    UINT32 uiCID = 0;
    UINT32 uiEvent = 0;

    // We need to differentiate between former format
    // "NW DEACT <PDP_type>, <PDP_addr>, [<cid>]" and new format for
    // secondary PDP contexts "NW DEACT <p_cid>, <cid>, <event_type>

    // If first parameter is a string, former format
    if (!ExtractUInt32(szStrExtract, uiPCID, szStrExtract))
    {
        GetContextIdFromDeact(szStrExtract, uiCID);
    }
    else
    {
        RIL_LOG_INFO("CSilo_Network::HandleNwDeact() - NW DEACT , extracted pcid=[%u]\r\n",
                uiPCID);

        if (FindAndSkipString(szStrExtract, ",", szStrExtract))
        {
            if (!ExtractUInt32(szStrExtract, uiCID, szStrExtract))
            {
                RIL_LOG_CRITICAL("CSilo_Network::ParseCGEV() - couldn't extract cid\r\n");
                return;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CSilo_Network::HandleNwDeact() - couldn't extract cid\r\n");
            return;
        }
        RIL_LOG_INFO("CSilo_Network::HandleNwDeact() - NW DEACT, extracted cid=[%u]\r\n",
                uiCID);
        if (FindAndSkipString(szStrExtract, ",", szStrExtract))
        {
            if (!ExtractUInt32(szStrExtract, uiEvent, szStrExtract))
            {
                RIL_LOG_CRITICAL("CSilo_Network::HandleNwDeact() - Couldn't extract event\r\n");
                return;
            }
        }

        RIL_LOG_INFO("CSilo_Network::HandleNwDeact() - NW DEACT, extracted event=[%u]\r\n",
                        uiEvent);

        sOEM_HOOK_RAW_UNSOL_BEARER_DEACT* pNwDeact =
                (sOEM_HOOK_RAW_UNSOL_BEARER_DEACT*)malloc(
                sizeof(sOEM_HOOK_RAW_UNSOL_BEARER_DEACT));
        if (NULL != pNwDeact)
        {
            pNwDeact->command = RIL_OEM_HOOK_RAW_UNSOL_BEARER_DEACT;
            pNwDeact->uiCid = uiCID;
            pNwDeact->uiPcid = uiPCID;

            RIL_onUnsolicitedResponse(RIL_UNSOL_OEM_HOOK_RAW, (void*)pNwDeact,
                    sizeof(sOEM_HOOK_RAW_UNSOL_BEARER_DEACT));
        }
    }

    CChannel_Data* pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
    if (NULL == pChannelData)
    {
        //  This may occur using AT proxy during 3GPP conformance testing.
        //  Let normal processing occur.
        RIL_LOG_CRITICAL("CSilo_Network::HandleNwDeact() - Invalid CID=[%u],"
                " no data channel found!\r\n", uiCID);
    }
    else
    {
        pChannelData->SetDataState(E_DATA_STATE_DEACTIVATED);
        /*
         * @TODO: If fail cause is provided as part of NW DEACT,
         * map the fail cause to ril cause values and set it.
         */
        pChannelData->SetDataFailCause(PDP_FAIL_ERROR_UNSPECIFIED);

        CTE::GetTE().DataConfigDown(uiCID, TRUE);

        CTE::GetTE().CompleteDataCallListChanged();
    }
}

void CSilo_Network::HandleMEDeactivation(const UINT32 uiCID)
{
    CChannel_Data* pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
    if (NULL == pChannelData)
    {
        RIL_LOG_CRITICAL("CSilo_Network::HandleMEDeactivation() - Invalid cid=[%u],"
                " no data channel found!\r\n", uiCID);
    }
    else if (E_DATA_STATE_DEACTIVATING != pChannelData->GetDataState())
    {
        pChannelData->SetDataState(E_DATA_STATE_DEACTIVATED);
        pChannelData->SetDataFailCause(PDP_FAIL_ERROR_UNSPECIFIED);

        CTE::GetTE().DataConfigDown(uiCID, TRUE);

        CTE::GetTE().CompleteDataCallListChanged();
    }
}

//
//
BOOL CSilo_Network::ParseXDATASTAT(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Network::ParseXDATASTAT() - Enter\r\n");
    BOOL bRet = FALSE;
    UINT32 uiDataStatus = 0;
    sOEM_HOOK_RAW_UNSOL_DATA_STATUS_IND* pData = NULL;
    BOOL bIsDataSuspended  = 0;

    if (NULL == pResponse)
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseXDATASTAT() - pResponse is NULL\r\n");
        goto Error;
    }

    pResponse->SetUnsolicitedFlag(TRUE);

    if (!ExtractUInt32(rszPointer, uiDataStatus, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseXDATASTAT() - Could not extract data status.\r\n");
        goto Error;
    }

    RIL_LOG_INFO("CSilo_Network::ParseXDATASTAT() - Data State: %s\r\n",
                                    (0 == uiDataStatus) ? "SUSPENDED" : "RESUMED");

    bIsDataSuspended = (0 == uiDataStatus) ? true : false;
    if (bIsDataSuspended != CTE::GetTE().IsDataSuspended())
    {
        CTE::GetTE().SetDataSuspended(bIsDataSuspended);

        if (!bIsDataSuspended)
        {
            pData = (sOEM_HOOK_RAW_UNSOL_DATA_STATUS_IND*) malloc(
                    sizeof(sOEM_HOOK_RAW_UNSOL_DATA_STATUS_IND));
            if (NULL == pData)
            {
                RIL_LOG_CRITICAL("CSilo_Network::ParseXDATASTAT() -"
                        " Could not allocate memory for pszData.\r\n");
                goto Error;
            }
            memset(pData, 0, sizeof(sOEM_HOOK_RAW_UNSOL_DATA_STATUS_IND));

            pData->command = RIL_OEM_HOOK_RAW_UNSOL_DATA_STATUS_IND;
            pData->status = uiDataStatus;

            pResponse->SetResultCode(RIL_UNSOL_OEM_HOOK_RAW);

            if (!pResponse->SetData((void*)pData,
                    sizeof(sOEM_HOOK_RAW_UNSOL_DATA_STATUS_IND), FALSE))
            {
                goto Error;
            }
        }
        else
        {
            // Delay notifying the SUSPENDED status by 3seconds to avoid icon toggling in send/receive SMS
            RIL_requestTimedCallback(triggerDataSuspendInd, NULL, 3, 0);
        }
    }

    bRet = TRUE;
Error:
    if (!bRet)
    {
        free(pData);
        pData = NULL;
    }

    RIL_LOG_VERBOSE("CSilo_Network::ParseXDATASTAT() - Exit\r\n");
    return bRet;
}

//
//
BOOL CSilo_Network::ParseXCSQ(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Network::ParseXCSQ() - Enter\r\n");

    BOOL bRet = FALSE;
    const char* pszDummy = NULL;
    const char* pszStart = NULL;
    char szBackup[MAX_NETWORK_DATA_SIZE] = {0};
    UINT32 uiRSSI = 0, uiBER = 0;
    RIL_SignalStrength_v6* pSigStrData = NULL;

    if (NULL == pResponse)
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseXCSQ() - pResponse is NULL\r\n");
        goto Error;
    }

    pResponse->SetUnsolicitedFlag(TRUE);

    pSigStrData = (RIL_SignalStrength_v6*)malloc(sizeof(RIL_SignalStrength_v6));
    if (NULL == pSigStrData)
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseXCSQ() -"
                " Could not allocate memory for RIL_SignalStrength_v6.\r\n");
        goto Error;
    }
    memset(pSigStrData, 0x00, sizeof(RIL_SignalStrength_v6));

    if (!FindAndSkipRspEnd(rszPointer, m_szNewLine, pszDummy))
    {
        // This isn't a complete notification -- no need to parse it
        RIL_LOG_CRITICAL("CSilo_Network::ParseXCSQ: Failed to find rsp end!\r\n");
        goto Error;
    }

    // Backup the XCSQ response string to report data on crashtool
    pszStart = rszPointer;
    PrintStringNullTerminate(szBackup, MAX_NETWORK_DATA_SIZE, "+XCSQ: ");
    ExtractUnquotedString(pszStart, m_szNewLine, szBackup + strlen(szBackup),
            MAX_NETWORK_DATA_SIZE - strlen(szBackup), pszDummy);
    CTE::GetTE().SaveNetworkData(LAST_NETWORK_XCSQ, szBackup);

    if (!ExtractUInt32(rszPointer, uiRSSI, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseXCSQ() - Could not extract uiRSSI.\r\n");
        goto Error;
    }

    if (!SkipString(rszPointer, ",", rszPointer) ||
        !ExtractUInt32(rszPointer, uiBER, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseXCSQ() - Could not extract uiBER.\r\n");
        goto Error;
    }

    pSigStrData->GW_SignalStrength.signalStrength = (int) uiRSSI;
    pSigStrData->GW_SignalStrength.bitErrorRate   = (int) uiBER;

    pSigStrData->CDMA_SignalStrength.dbm=-1;
    pSigStrData->CDMA_SignalStrength.ecio=-1;
    pSigStrData->EVDO_SignalStrength.dbm=-1;
    pSigStrData->EVDO_SignalStrength.ecio=-1;
    pSigStrData->EVDO_SignalStrength.signalNoiseRatio=-1;
    pSigStrData->LTE_SignalStrength.signalStrength=-1;
    pSigStrData->LTE_SignalStrength.rsrp=INT_MAX;
    pSigStrData->LTE_SignalStrength.rsrq=INT_MAX;
    pSigStrData->LTE_SignalStrength.rssnr=INT_MAX;
    pSigStrData->LTE_SignalStrength.cqi=INT_MAX;

    pResponse->SetResultCode(RIL_UNSOL_SIGNAL_STRENGTH);

    if (!pResponse->SetData((void*)pSigStrData, sizeof(RIL_SignalStrength_v6), FALSE))
    {
        goto Error;
    }

    bRet = TRUE;

Error:
    if (!bRet)
    {
        free(pSigStrData);
        pSigStrData = NULL;
    }

    RIL_LOG_VERBOSE("CSilo_Network::ParseXCSQ() - Exit\r\n");
    return bRet;
}

//
//
BOOL CSilo_Network::ParseXCESQI(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Network::ParseXCESQI() - Enter\r\n");

    BOOL bRet = FALSE;
    const char* pszDummy = NULL;
    const char* pszStart = NULL;
    char szBackup[MAX_NETWORK_DATA_SIZE] = {0};
    RIL_SignalStrength_v6* pSigStrData = NULL;

    if (NULL == pResponse)
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseXCESQI() - pResponse is NULL\r\n");
        goto Error;
    }

    pResponse->SetUnsolicitedFlag(TRUE);

    if (!FindAndSkipRspEnd(rszPointer, m_szNewLine, pszDummy))
    {
        // This isn't a complete notification -- no need to parse it
        RIL_LOG_CRITICAL("CSilo_Network::ParseXCESQI() - Failed to find rsp end!\r\n");
        goto Error;
    }

    // Backup the XCESQI response string to report data on crashtool
    pszStart = rszPointer;
    PrintStringNullTerminate(szBackup, MAX_NETWORK_DATA_SIZE, "+XCESQI: ");
    ExtractUnquotedString(pszStart, m_szNewLine, szBackup + strlen(szBackup),
            MAX_NETWORK_DATA_SIZE - strlen(szBackup), pszDummy);
    CTE::GetTE().SaveNetworkData(LAST_NETWORK_XCSQ, szBackup);

    pSigStrData = CTE::GetTE().ParseXCESQ(rszPointer, TRUE);
    if (NULL == pSigStrData)
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseXCESQI() -"
                " Could not allocate memory for RIL_SignalStrength_v6.\r\n");
        goto Error;
    }

    if (!pResponse->SetData((void*)pSigStrData, sizeof(RIL_SignalStrength_v6), FALSE))
    {
        goto Error;
    }

    pResponse->SetResultCode(RIL_UNSOL_SIGNAL_STRENGTH);
    bRet = TRUE;
Error:
    if (!bRet)
    {
        free(pSigStrData);
        pSigStrData = NULL;
    }

    RIL_LOG_VERBOSE("CSilo_Network::ParseXCESQI() - Exit\r\n");
    return bRet;
}

#if defined(M2_DUALSIM_FEATURE_ENABLED)
// Special parse function to handle Fast Out of Service Notifications
BOOL CSilo_Network::ParseXREGFastOoS(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Network::ParseXREGFastOoS() - Enter\r\n");

    BOOL bRet = FALSE;
    UINT32 uiState = 0;
    UINT32 commandId = 0;
    BYTE i = 0;

    const UINT32 NETWORK_REG_STATUS_FAST_OOS = 20;
    const UINT32 NETWORK_REG_STATUS_IN_SERVICE = 21;
    const BYTE MAX_NUM_EMPTY_VALUES = 3;

    if (NULL == pResponse)
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseXREGFastOoS() - pResponse is NULL\r\n");
        goto Error;
    }

    pResponse->SetUnsolicitedFlag(TRUE);

    // Extract "<state>"
    if (!ExtractUInt32(rszPointer, uiState, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseXREGFastOoS() - Could not extract <state>.\r\n");
        goto Error;
    }

    do
    {
        /* skip empty response values */
        FindAndSkipString(rszPointer, ",", rszPointer);
        i++;
    } while (i < MAX_NUM_EMPTY_VALUES);

    if (NETWORK_REG_STATUS_FAST_OOS == uiState)
    {
        commandId = RIL_OEM_HOOK_RAW_UNSOL_FAST_OOS_IND;
    }
    else if (NETWORK_REG_STATUS_IN_SERVICE == uiState)
    {
        commandId = RIL_OEM_HOOK_RAW_UNSOL_IN_SERVICE_IND;
    }
    else // unsupported state
    {
         RIL_LOG_CRITICAL("CSilo_Network::ParseXREGFastOoS() -"
                 " Unrecognized network reg state\r\n");
         goto Error;
    }

    pResponse->SetResultCode(RIL_UNSOL_OEM_HOOK_RAW);

    if (!pResponse->SetData((void*)&commandId, sizeof(UINT32), TRUE))
    {
        goto Error;
    }

    bRet = TRUE;

Error:

    RIL_LOG_VERBOSE("CSilo_Network::ParseXREGFastOoS() - Exit[%d]\r\n", bRet);
    return bRet;
}
#endif // M2_DUALSIM_FEATURE_ENABLED
