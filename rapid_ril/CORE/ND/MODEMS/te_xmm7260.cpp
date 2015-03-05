////////////////////////////////////////////////////////////////////////////
// te_xmm7260.cpp
//
// Copyright (C) Intel 2013.
//
//
// Description:
//    Overlay for the IMC 7260 modem
//
/////////////////////////////////////////////////////////////////////////////

#include "types.h"
#include "nd_structs.h"
#include "util.h"
#include "extract.h"
#include "rillog.h"
#include "te.h"
#include "te_xmm7260.h"
#include "init7260.h"

CTE_XMM7260::CTE_XMM7260(CTE& cte)
: CTE_XMM7160(cte)
{
    m_cte.SetDefaultPDNCid(DEFAULT_PDN_CID);
}

CTE_XMM7260::~CTE_XMM7260()
{
}

CInitializer* CTE_XMM7260::GetInitializer()
{
    RIL_LOG_VERBOSE("CTE_XMM7260::GetInitializer() - Enter\r\n");
    CInitializer* pRet = NULL;

    RIL_LOG_INFO("CTE_XMM7260::GetInitializer() - Creating CInit7260 initializer\r\n");
    m_pInitializer = new CInit7260();
    if (NULL == m_pInitializer)
    {
        RIL_LOG_CRITICAL("CTE_XMM7260::GetInitializer() - Failed to create a CInit7260 "
                "initializer!\r\n");
        goto Error;
    }

    pRet = m_pInitializer;

Error:
    RIL_LOG_VERBOSE("CTE_XMM7260::GetInitializer() - Exit\r\n");
    return pRet;
}

//
// RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING 103
//
RIL_RESULT_CODE CTE_XMM7260::CoreReportStkServiceRunning(REQUEST_DATA& rReqData,
                                                           void* pData,
                                                           UINT32 uiDataSize)
{
    RIL_LOG_INFO("CTE_XMM7260::CoreReportStkServiceRunning() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    /* Activate USAT profile +CUSATA */
    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1),
            "AT+CUSATA=1\r"))
    {
        RIL_LOG_CRITICAL("CTE_XMM7260::CoreReportStkServiceRunning() - "
                " Could not form string.\r\n");
        goto Error;
    }
    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_INFO("CTE_XMM7260::CoreReportStkServiceRunning() - Exit\r\n");
    return res;
}

BOOL CTE_XMM7260::ParseActivateUsatProfile(const char* pszResponse)
{

    RIL_LOG_VERBOSE("CTE_XMM7260::ParseActivateUsatProfile() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char* pszRsp = pszResponse;
    UINT32 uiUiccState = 0;
    UINT32 uiCmeError = 0;
    UINT32 uiAdditionalProfileSupport = 0;

    // Parse "+CUSATA: "
    if (!FindAndSkipString(pszRsp, "+CUSATA: ", pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_XMM7260::ParseActivateUsatProfile() -"
                " Could not skip over \"+CUSATA: \".\r\n");
        goto Error;
    }

    if (!ExtractUInt32(pszRsp, uiUiccState, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_XMM7260::ParseActivateUsatProfile() -"
                " Could not find UICC state argument.\r\n");
        goto Error;
    }
    else
    {
        RIL_LOG_INFO("CTE_XMM7260::ParseActivateUsatProfile() -"
                " uiUiccState= %u.\r\n", uiUiccState);
    }

    // Parse "," if additional Profile Support exists
    if (SkipString(pszRsp, ",", pszRsp))
    {
        if (ExtractUInt32(pszRsp, uiAdditionalProfileSupport, pszRsp))
        {
            RIL_LOG_INFO("CTE_XMM7260::ParseActivateUsatProfile() -"
                    " Found Additional Profile Support argument= %u.\r\n",
                    uiAdditionalProfileSupport);
        }
    }

    // Parse "<CR><LF>", to check if "CME error" or "OK" answer is present
    if (FindAndSkipRspEnd(pszRsp, m_szNewLine, pszRsp))
    {
        if (!SkipString(pszRsp, m_szNewLine, pszRsp))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseActivateUsatProfile() -"
                    " Could not extract response prefix.\r\n");
            goto Error;
        }
    }
    else
    {
        RIL_LOG_CRITICAL("CTE_XMM7260::ParseActivateUsatProfile - "
                "Could not skip response postfix.\r\n");
        goto Error;
    }

    // Parse "+CME ERROR: " if present
    if (SkipString(pszRsp, "+CME ERROR: ", pszRsp))
    {
        if (!ExtractUInt32(pszRsp, uiCmeError, pszRsp))
        {
            RIL_LOG_CRITICAL("CTE_XMM7260::ParseActivateUsatProfile() -"
                   " Could not find CME argument.\r\n");
            goto Error;
        }
        else
        {
            RIL_LOG_CRITICAL("CTE_XMM7260::ParseActivateUsatProfile() -"
                    " CME ERROR: %u \r\n", uiCmeError);
        }
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_INFO("CTE_XMM7260::ParseActivateUsatProfile() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_XMM7260::ParseReportStkServiceRunning(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_XMM7260::ParseReportStkServiceRunning() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char* pszRsp = rRspData.szResponse;
    UINT32 uiCmeError = 0;
    const char* pszDummy;

    if (pszRsp == NULL)
    {
        RIL_LOG_CRITICAL("CTE_XMM7260::ParseReportStkServiceRunning() -"
                " no Response string...\r\n");
        goto Error;
    }

    // As method SetAlwaysParse() was set, all answers from Modem needs to be handled
    // For AT+CUSATA (with/out parameters) we can received from Modem the following answers :
    // Basic responses: "OK" or "CME ERROR: "x" "
    // +CUSATA:<UICC_state>, [<additional_profile_support>=""]
    // +CUSATA:<UICC_state>, [<additional_profile_support>=""], [<CME ERROR>: "x"]

    // Parse <prefix>
    if (SkipString(pszRsp, m_szNewLine, pszDummy))
    {
        // Search for "+CME ERROR: " after prefix
        if (SkipString(pszDummy, "+CME ERROR: ", pszDummy))
        {
            if (!ExtractUInt32(pszDummy, uiCmeError, pszDummy))
            {
                RIL_LOG_CRITICAL("CTE_XMM7260::ParseReportStkServiceRunning() -"
                        " Could not find CME  argument.\r\n");
                goto Error;
            }
            else
            {
                RIL_LOG_CRITICAL("CTE_XMM7260::ParseReportStkServiceRunning() -"
                        " CME ERROR: %u \r\n", uiCmeError);
                goto Error;
            }
        }
        else
        {
            // "OK" or extended answer received
            res = RRIL_RESULT_OK;

            // Search for "+CUSATA: " after prefix
            if (SkipString(pszDummy, "+CUSATA: ", pszDummy))
            {
                // Parse +CUSATA:<UICC_state>, [<additional_profile_support>=""], [<CME ERROR>=""]
                res = ParseActivateUsatProfile(pszRsp);
            }
        }
    }

    if (!FindAndSkipRspEnd(pszRsp, m_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_XMM7260::ParseReportStkServiceRunning - "
                "Could not skip response postfix.\r\n");
        goto Error;
    }

Error:
    RIL_LOG_INFO("CTE_XMM7260::ParseReportStkServiceRunning() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_STK_GET_PROFILE
//
RIL_RESULT_CODE CTE_XMM7260::CoreStkGetProfile(REQUEST_DATA& rReqData,
                                                           void* pData,
                                                           UINT32 uiDataSize)
{
    RIL_LOG_INFO("CTE_XMM7260::CoreStkGetProfile() - Enter/Exit\r\n");
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_STK_SET_PROFILE
//
RIL_RESULT_CODE CTE_XMM7260::CoreStkSetProfile(REQUEST_DATA& rReqData,
                                                           void* pData,
                                                           UINT32 uiDataSize)
{
    RIL_LOG_INFO("CTE_XMM7260::CoreStkSetProfile() - Enter/Exit\r\n");
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE 70
//
RIL_RESULT_CODE CTE_XMM7260::CoreStkSendTerminalResponse(REQUEST_DATA& rReqData,
                                                                     void* pData,
                                                                     UINT32 uiDataSize)
{
    RIL_LOG_INFO("CTE_XMM7260::CoreStkSendTerminalResponse() - Enter\r\n");
    char* pszTermResponse = NULL;
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    // AT+CUSATT=<terminal_response>
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_XMM7260::CoreStkSendTerminalResponse() -"
                " Data pointer is NULL.\r\n");
        goto Error;
    }

    if (uiDataSize != sizeof(char*))
    {
        RIL_LOG_CRITICAL("CTE_XMM7260::CoreStkSendTerminalResponse() - Invalid data size.\r\n");
        goto Error;
    }
    RIL_LOG_INFO("CTE_XMM7260::CoreStkSendTerminalResponse() - uiDataSize= %d\r\n", uiDataSize);

    pszTermResponse = (char*)pData;

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1),
            "AT+CUSATT=\"%s\"\r", pszTermResponse))
    {
        RIL_LOG_CRITICAL("CTE_XMM7260::CoreStkSendTerminalResponse() -"
                " Could not form string.\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_INFO("CTE_XMM7260::CoreStkSendTerminalResponse() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_XMM7260::ParseStkSendTerminalResponse(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_XMM7260::ParseStkSendTerminalResponse() - Enter/Exit\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    return res;
}

//
// RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND 69
//
RIL_RESULT_CODE CTE_XMM7260::CoreStkSendEnvelopeCommand(REQUEST_DATA& rReqData,
                                                                    void* pData,
                                                                    UINT32 uiDataSize)
{
    RIL_LOG_INFO("CTE_XMM7260::CoreStkSendEnvelopeCommand() - Enter\r\n");
    char* pszEnvCommand = NULL;
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    /* AT+CUSATE=<envelope_command> */

    if (sizeof(char*) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTE_XMM7260::CoreStkSendEnvelopeCommand() -"
                " Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_XMM7260::CoreStkSendEnvelopeCommand() -"
                " Passed data pointer was NULL\r\n");
        goto Error;
    }

    // extract data
    pszEnvCommand = (char*)pData;

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1),
            "AT+CUSATE=\"%s\"\r", pszEnvCommand))
    {
        RIL_LOG_CRITICAL("CTE_XMM7260::CoreStkSendEnvelopeCommand() -"
                " Could not form string.\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_INFO("CTE_XMM7260::CoreStkSendEnvelopeCommand() - Exit\r\n");
    return res;
}

BOOL CTE_XMM7260::ParseEnvelopCommandResponse(const char* pszResponse, char* pszEnvelopResp,
        UINT32* puiBusy, UINT32* puiSw1, UINT32* puiSw2)
{
    RIL_LOG_INFO("CTE_XMM7260::ParseEnvelopCommandResponse() - Enter\r\n");
    const char* pszRsp = pszResponse;
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    // +CUSATE:<envelope_response>[,<busy>][<CR><LF>+CUSATE2:<sw1>,<sw2>]

    if (NULL == pszRsp)
    {
        RIL_LOG_CRITICAL("CTE_XMM7260::ParseEnvelopCommandResponse() -"
                " Response string is NULL!\r\n");
        goto Error;
    }

    // Parse "+CUSATE: "
    if (!FindAndSkipString(pszRsp, "+CUSATE: ", pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_XMM7260::ParseEnvelopCommandResponse() -"
                " Could not skip over \"+CUSATE: \".\r\n");
        goto Error;
    }

    // Parse "<envelope_response>"
    if (!ExtractQuotedString(pszRsp, pszEnvelopResp, MAX_BUFFER_SIZE, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_XMM7260::ParseEnvelopCommandResponse() -"
                " Could not parse envelope response.\r\n");
        goto Error;
    }

    RIL_LOG_INFO("CTE_XMM7260::ParseEnvelopCommandResponse() - response data: \"%s\".\r\n",
            pszEnvelopResp);

    // Parse "," if optional <Busy> exists
    if (SkipString(pszRsp, ",", pszRsp))
    {
        if (!ExtractUInt32(pszRsp, *puiBusy, pszRsp))
        {
            RIL_LOG_INFO("CTE_XMM7260::ParseEnvelopCommandResponse() -"
                    " Could not extract Busy.\r\n");
            goto Error;
        }
    }

    // Parse "<CR><LF>", to check if +CUSATE2 error is present
    if (FindAndSkipRspEnd(pszRsp, m_szNewLine, pszRsp))
    {
        if (!SkipString(pszRsp, m_szNewLine, pszRsp))
        {
            RIL_LOG_INFO("CTE_XMM7260::ParseEnvelopCommandResponse() -"
                    " Could not extract response prefix.\r\n");
            goto Error;
        }
    }
    else
    {
        RIL_LOG_CRITICAL("CTE_XMM7260::ParseEnvelopCommandResponse - "
                "Could not skip response postfix.\r\n");
        goto Error;
    }

    // Parse "+CUSATE2", added in 27.007 V11.8.0 (Rel.11)
    // Note: It might not be yet supported by the modem, but is mandatory
    // as fix for a defect of 27.007 spec Rel.10
    if (SkipString(pszRsp, "+CUSATE2: ", pszRsp))
    {
        // Parse "<sw1>"
        if (!ExtractUInt32(pszRsp, *puiSw1, pszRsp))
        {
            RIL_LOG_CRITICAL("CTE_XMM7260::ParseEnvelopCommandResponse() -"
                    " Could not extract sw1.\r\n");
            goto Error;
        }

        // Parse ",<sw2>"
        if (!SkipString(pszRsp, ",", pszRsp)
                || !ExtractUInt32(pszRsp, *puiSw2, pszRsp))
        {
            RIL_LOG_CRITICAL("CTE_XMM7260::ParseEnvelopCommandResponse() -"
                    " Could not extract sw2.\r\n");
            goto Error;
        }

        if (!FindAndSkipRspEnd(pszRsp, m_szNewLine, pszRsp))
        {
            RIL_LOG_CRITICAL("CTE_XMM7260::ParseEnvelopCommandResponse - "
                    "Could not skip response postfix.\r\n");
            goto Error;
        }

        RIL_LOG_INFO("CTE_XMM7260::ParseEnvelopCommandResponse() - "
                " +CUSATE2: sw1=%u, sw2=%u\r\n", *puiSw1, *puiSw2);
    }
    else
    {
        RIL_LOG_INFO("CTE_XMM7260::ParseEnvelopCommandResponse() -"
                " Could not find \"+CUSATE2: \".\r\n");
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_INFO("CTE_XMM7260::ParseEnvelopCommandResponse() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_XMM7260::ParseStkSendEnvelopeCommand(RESPONSE_DATA& rRspData)
{
    RIL_LOG_INFO("CTE_XMM7260::ParseStkSendEnvelopeCommand() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    UINT32 uiSw1 = 0;
    UINT32 uiSw2 = 0;
    UINT32 uiBusy = 0;
    const char* pszRsp = rRspData.szResponse;
    char* pszEnvelopResp = NULL;

    pszEnvelopResp = (char*)malloc(MAX_BUFFER_SIZE);
    if (NULL == pszEnvelopResp)
    {
        RIL_LOG_CRITICAL("CTE_XMM7260::ParseStkSendEnvelopeCommand() -"
                " Could not alloc mem for command.\r\n");
        goto Error;
    }

    memset(pszEnvelopResp, 0x00, MAX_BUFFER_SIZE);
    res = ParseEnvelopCommandResponse(pszRsp, pszEnvelopResp, &uiBusy, &uiSw1, &uiSw2);

    if (0 == strlen(pszEnvelopResp))
    {
        free(pszEnvelopResp);
        pszEnvelopResp = NULL;
    }

    rRspData.pData = (void*)pszEnvelopResp;
    rRspData.uiDataSize = sizeof(char*);

Error:
    free(pszEnvelopResp);
    pszEnvelopResp = NULL;

    RIL_LOG_INFO("CTE_XMM7260::ParseStkSendEnvelopeCommand() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_STK_SEND_ENVELOPE_WITH_STATUS 107
//
RIL_RESULT_CODE CTE_XMM7260::ParseStkSendEnvelopeWithStatus(RESPONSE_DATA& rRspData)
{
    RIL_LOG_INFO("CTE_XMM7260::ParseStkSendEnvelopeWithStatus() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    UINT32 uiSw1 = 0;
    UINT32 uiSw2 = 0;
    UINT32 uiBusy = 0;
    const char* pszRsp = rRspData.szResponse;
    char* pszEnvelopResp = NULL;
    UINT32 cbEnvelopResp = 0;
    RIL_SIM_IO_Response* pResponse = NULL;

    pszEnvelopResp = (char*)malloc(MAX_BUFFER_SIZE);
    if (NULL == pszEnvelopResp)
    {
        RIL_LOG_CRITICAL("CTE_XMM7260::ParseStkSendEnvelopeWithStatus() -"
                " Could not alloc mem for command.\r\n");
        goto Error;
    }

    memset(pszEnvelopResp, 0x00, MAX_BUFFER_SIZE);

    res = ParseEnvelopCommandResponse(pszRsp, pszEnvelopResp, &uiBusy, &uiSw1, &uiSw2);
    cbEnvelopResp = strlen(pszEnvelopResp);

    if (0 != cbEnvelopResp)
    {
        // Allocate memory for the RIL_SIM_IO_Response struct + sim response string.
        // The char* in the RIL_SIM_IO_Response will point to the buffer allocated
        // directly after the RIL_SIM_IO_Response.  When the RIL_SIM_IO_Response
        // is deleted, the corresponding response string will be freed as well.
        pResponse = (RIL_SIM_IO_Response*)malloc(sizeof(RIL_SIM_IO_Response)
                + cbEnvelopResp + 1);

        if (NULL == pResponse)
        {
            RIL_LOG_CRITICAL("CTE_XMM7260::ParseStkSendEnvelopeWithStatus() -"
                    " Could not allocate memory for a RIL_SIM_IO_Response struct.\r\n");
            goto Error;
        }
        memset(pResponse, 0, sizeof(RIL_SIM_IO_Response) + cbEnvelopResp + 1);

        // set location to copy sim response string just after RIL_SIM_IO_Response
        pResponse->simResponse = (char*)(((char*)pResponse) + sizeof(RIL_SIM_IO_Response));

        // Ensure NULL termination
        pResponse->simResponse[cbEnvelopResp] = '\0';

        if (!CopyStringNullTerminate(pResponse->simResponse, pszEnvelopResp, cbEnvelopResp+1))
        {
            RIL_LOG_CRITICAL("CTE_XMM7260::ParseStkSendEnvelopeWithStatus() -"
                    " Cannot CopyStringNullTerminate szResponseString\r\n");
            goto Error;
        }
    }
    else
    {
        pResponse = (RIL_SIM_IO_Response*)malloc(sizeof(RIL_SIM_IO_Response));
        if (NULL == pResponse)
        {
            RIL_LOG_CRITICAL("CTE_XMM7260::ParseStkSendEnvelopeWithStatus() -"
                    " Could not allocate memory for a RIL_SIM_IO_Response struct.\r\n");
            goto Error;
        }
        pResponse->simResponse = NULL;
    }

    pResponse->sw1 = uiSw1;
    pResponse->sw2 = uiSw2;

    rRspData.pData = (void*)pResponse;
    rRspData.uiDataSize = sizeof(RIL_SIM_IO_Response);

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pResponse);
        pResponse = NULL;
    }

    free(pszEnvelopResp);
    pszEnvelopResp = NULL;

    RIL_LOG_INFO("CTE_XMM7260::ParseStkSendEnvelopeWithStatus() - Exit\r\n");
    return res;
}
