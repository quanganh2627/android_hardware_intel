////////////////////////////////////////////////////////////////////////////
// silo_SMS.cpp
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides response handlers and parsing functions for the SMS-related
//    RIL components.
//
/////////////////////////////////////////////////////////////////////////////

#include "types.h"
#include "rillog.h"
#include "channel_nd.h"
#include "response.h"
#include "extract.h"
#include "rildmain.h"
#include "callbacks.h"
#include "silo_sms.h"
#include "te.h"

//
//
CSilo_SMS::CSilo_SMS(CChannel* pChannel, CSystemCapabilities* pSysCaps)
: CSilo(pChannel, pSysCaps)
{
    RIL_LOG_VERBOSE("CSilo_SMS::CSilo_SMS() - Enter\r\n");

    // AT Response Table
    static ATRSPTABLE pATRspTable[] =
    {
        { "+CMT: "   , (PFN_ATRSP_PARSE)&CSilo_SMS::ParseCMT  },
        { "+CBM: "   , (PFN_ATRSP_PARSE)&CSilo_SMS::ParseCBM  },
        { "+CDS: "   , (PFN_ATRSP_PARSE)&CSilo_SMS::ParseCDS  },
        { "+CMTI: "  , (PFN_ATRSP_PARSE)&CSilo_SMS::ParseCMTI },
        { "+CBMI: "  , (PFN_ATRSP_PARSE)&CSilo_SMS::ParseCBMI },
        { "+CDSI: "  , (PFN_ATRSP_PARSE)&CSilo_SMS::ParseCDSI },
        { ""         , (PFN_ATRSP_PARSE)&CSilo_SMS::ParseNULL }
    };

    m_pATRspTable = pATRspTable;
    RIL_LOG_VERBOSE("CSilo_SMS::CSilo_SMS() - Exit\r\n");
}

//
//
CSilo_SMS::~CSilo_SMS()
{
    RIL_LOG_VERBOSE("CSilo_SMS::~CSilo_SMS() - Enter\r\n");
    RIL_LOG_VERBOSE("CSilo_SMS::~CSilo_SMS() - Exit\r\n");
}


char* CSilo_SMS::GetBasicInitString()
{
    // sms silo-related channel basic init string
    const char szSmsBasicInitString[] = "+CMGF=0";

    if (m_pSystemCapabilities->IsSmsCapable())
    {
        if (!ConcatenateStringNullTerminate(m_szBasicInitString,
                sizeof(m_szBasicInitString), szSmsBasicInitString))
        {
            RIL_LOG_CRITICAL("CSilo_SMS::GetBasicInitString() : Failed to copy basic init "
                    "string!\r\n");
            return NULL;
        }
    }
    return m_szBasicInitString;
}

char* CSilo_SMS::GetUnlockInitString()
{
    // sms silo-related channel unlock init string
    const char szSmsUnlockInitString[] = "+CSMS=1|+CGSMS=3";

    if (m_pSystemCapabilities->IsSmsCapable())
    {
        if (!ConcatenateStringNullTerminate(m_szUnlockInitString,
                sizeof(m_szUnlockInitString), szSmsUnlockInitString))
        {
            RIL_LOG_CRITICAL("CSilo_SMS::GetUnlockInitString() : Failed to copy unlock init "
                    "string!\r\n");
            return NULL;
        }
    }
    return m_szUnlockInitString;
}

char* CSilo_SMS::GetURCInitString()
{
    // sms silo-related URC channel basic init string
    const char szSmsURCInitString[] = "+CMGF=0|+CSCS=\"UCS2\"";
    if (m_pSystemCapabilities->IsSmsCapable())
    {
        if (!ConcatenateStringNullTerminate(m_szURCInitString,
                sizeof(m_szURCInitString), szSmsURCInitString))
        {
            RIL_LOG_CRITICAL("CSilo_SMS::GetURCInitString() : Failed to copy URC init "
                    "string!\r\n");
            return NULL;
        }
    }
    return m_szURCInitString;
}

char* CSilo_SMS::GetURCUnlockInitString()
{
    // sms silo-related URC channel unlock init string
    const char szSmsURCUnlockInitString[] = "+CNMI=2,2,2,1";
    if (m_pSystemCapabilities->IsSmsCapable())
    {
        if (!ConcatenateStringNullTerminate(m_szURCUnlockInitString,
                sizeof(m_szURCUnlockInitString), szSmsURCUnlockInitString))
        {
            RIL_LOG_CRITICAL("CSilo_SMS::GetURCUnlockInitString() : Failed to copy URC unlock "
                    "init string!\r\n");
            return NULL;
        }
    }
    return m_szURCUnlockInitString;
}

///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
//  Parse functions here
///////////////////////////////////////////////////////////////////////////////////////////////

//
//
BOOL CSilo_SMS::ParseMessage(CResponse* const pResponse, const char*& rszPointer,
                                                      SILO_SMS_MSG_TYPES msgType)
{
    RIL_LOG_VERBOSE("CSilo_SMS::ParseMessage() - Enter\r\n");
    RIL_LOG_VERBOSE("CSilo_SMS::ParseMessage() - Exit\r\n");
    return FALSE;
}

//
//
//
BOOL CSilo_SMS::ParseMessageInSim(CResponse* const pResponse, const char*& rszPointer,
                                                           SILO_SMS_MSG_TYPES msgType)
{
    RIL_LOG_VERBOSE("CSilo_SMS::ParseMessageInSim() - Enter\r\n");

    BOOL   fRet = FALSE;
    const char* szDummy;
    UINT32 Location;
    UINT32 Index;
    int* pIndex = NULL;

    if (SILO_SMS_MSG_IN_SIM == msgType)
    {
        CTE::GetTE().TriggerQuerySimSmsStoreStatus();
    }

    if (pResponse == NULL)
    {
        RIL_LOG_CRITICAL("CSilo_SMS::ParseMessageInSim() : pResponse was NULL\r\n");
        goto Error;
    }

    pResponse->SetUnsolicitedFlag(TRUE);

    pIndex = (int*)malloc(sizeof(int));
    if (NULL == pIndex)
    {
        RIL_LOG_CRITICAL("CSilo_SMS::ParseMessageInSim() - Could not alloc mem for int.\r\n");
        goto Error;
    }

    // Look for a "<postfix>" to be sure we got a whole message
    if (!FindAndSkipRspEnd(rszPointer, m_szNewLine, szDummy))
    {
        goto Error;
    }

    // Look for a "<anything>,<Index>"
    if ( (!FindAndSkipString(rszPointer, ",", rszPointer))     ||
         (!ExtractUInt32(rszPointer, Index, rszPointer)))
    {
        goto Error;
    }

    pResponse->SetResultCode(RIL_UNSOL_RESPONSE_NEW_SMS_ON_SIM);

    *pIndex = Index;

    if (!pResponse->SetData((void*)pIndex, sizeof(int), FALSE))
    {
        goto Error;
    }

    fRet = TRUE;

Error:
    if (!fRet)
    {
        free(pIndex);
        pIndex = NULL;
    }

    RIL_LOG_VERBOSE("CSilo_SMS::ParseMessageInSim() - Exit\r\n");

    return fRet;
}

//
// SMS-DELIVER notification
//
BOOL CSilo_SMS::ParseCMT(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_SMS::ParseCMT() - Enter\r\n");

    BOOL   fRet     = FALSE;
    UINT32 uiLength = 0;
    char*  szPDU    = NULL;
    char   szAlpha[MAX_BUFFER_SIZE];
    const char* szDummy;

    if (NULL == pResponse)
    {
        RIL_LOG_CRITICAL("CSilo_SMS::ParseCMT() - pResponse is NULL.\r\n");
        goto Error;
    }

    pResponse->SetUnsolicitedFlag(TRUE);

    // Throw out the alpha chars if there are any
    (void)ExtractQuotedString(rszPointer, szAlpha, MAX_BUFFER_SIZE, rszPointer);

    // Parse ",<length><CR><LF>
    if (!SkipString(rszPointer, ",", rszPointer)       ||
        !ExtractUInt32(rszPointer, uiLength, rszPointer) ||
        !SkipString(rszPointer, m_szNewLine, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_SMS::ParseCMT() - Could not parse PDU Length.\r\n");
        goto Error;
    }

    //RIL_LOG_INFO("CMT=[%s]\r\n", CRLFExpandedString(rszPointer,strlen(rszPointer)).GetString() );

    // The PDU will be followed by m_szNewLine, so look for m_szNewLine
    // and use its position to determine the length of the PDU string.

    if (!FindAndSkipString(rszPointer, m_szNewLine, szDummy))
    {
        // This isn't a complete message notification -- no need to parse it
        RIL_LOG_CRITICAL("CSilo_SMS::ParseCMT() - Could not find postfix; assuming this is an"
                " incomplete response.\r\n");
        goto Error;
    }
    else
    {
        // Override the given length with the actual length. Don't forget the '\0'.
        uiLength = ((UINT32)(szDummy - rszPointer)) - strlen(m_szNewLine) + 1;
        RIL_LOG_INFO("CSilo_SMS::ParseCMT() - Calculated PDU String length: %u chars.\r\n",
                uiLength);
    }

    szPDU = (char*)malloc(sizeof(char) * uiLength);
    if (NULL == szPDU)
    {
        RIL_LOG_CRITICAL("CSilo_SMS::ParseCMT() - Could not allocate memory for szPDU.\r\n");
        goto Error;
    }
    memset(szPDU, 0, sizeof(char) * uiLength);

    if (!ExtractUnquotedString(rszPointer, m_cTerminator, szPDU, uiLength, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_SMS::ParseCMT() - Could not parse PDU String.\r\n");
        goto Error;
    }

    // Ensure NULL Termination.
    szPDU[uiLength-1] = '\0';

    RIL_LOG_INFO("CSilo_SMS::ParseCMT() - PDU String: \"%s\".\r\n", szPDU);

    pResponse->SetResultCode(RIL_UNSOL_RESPONSE_NEW_SMS);

    if (!pResponse->SetData((void*)szPDU, sizeof(char) * uiLength, FALSE))
    {
        goto Error;
    }

    fRet = TRUE;

Error:
    if (!fRet)
    {
        free(szPDU);
        szPDU = NULL;
    }

    RIL_LOG_VERBOSE("CSilo_SMS::ParseCMT() - Exit\r\n");
    return fRet;
}

//
//  Incoming cell broadcast.
//
BOOL CSilo_SMS::ParseCBM(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_SMS::ParseCBM() - Enter\r\n");

    BOOL   fRet     = FALSE;
    UINT32   uiLength = 0;
    char*  szPDU    = NULL;
    BYTE*  pByteBuffer = NULL;
    char   szAlpha[MAX_BUFFER_SIZE];
    const char* szDummy;
    UINT32 bytesUsed = 0;

    if (NULL == pResponse)
    {
        RIL_LOG_CRITICAL("CSilo_SMS::ParseCBM() - pResponse is NULL.\r\n");
        goto Error;
    }

    pResponse->SetUnsolicitedFlag(TRUE);

    // Throw out the alpha chars if there are any
    (void)ExtractQuotedString(rszPointer, szAlpha, MAX_BUFFER_SIZE, rszPointer);

    // Parse "<length><CR><LF>
    if (!ExtractUInt32(rszPointer, uiLength, rszPointer) ||
        !SkipString(rszPointer, m_szNewLine, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_SMS::ParseCBM() - Could not parse PDU Length.\r\n");
        goto Error;
    }

    // The PDU will be followed by m_szNewLine, so look for m_szNewLine
    // and use its position to determine the length of the PDU string.

    if (!FindAndSkipString(rszPointer, m_szNewLine, szDummy))
    {
        // This isn't a complete message notification -- no need to parse it
        RIL_LOG_CRITICAL("CSilo_SMS::ParseCBM() - Could not find postfix; assuming this is an"
                " incomplete response.\r\n");
        goto Error;
    }
    else
    {
        uiLength = (UINT32)(szDummy - rszPointer) - strlen(m_szNewLine);
        RIL_LOG_INFO("CSilo_SMS::ParseCBM() - Calculated PDU String length: %u chars.\r\n",
                uiLength);
    }

    // Don't forget the '\0'.
    szPDU = (char*) malloc(sizeof(char) * (uiLength + 1));
    pByteBuffer = (BYTE*) malloc(sizeof(BYTE) * (uiLength / 2) + 1);

    if ((NULL == szPDU) || (NULL == pByteBuffer))
    {
        RIL_LOG_CRITICAL("CSilo_SMS::ParseCBM() - Could not allocate memory for szPDU or"
                " pByteBuffer\r\n");
        goto Error;
    }

    memset(szPDU, 0, sizeof(char) * (uiLength + 1));
    memset(pByteBuffer, 0, sizeof(BYTE) * (uiLength / 2) + 1);

    if (!ExtractUnquotedString(rszPointer, m_cTerminator, szPDU, (uiLength + 1), rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_SMS::ParseCBM() - Could not parse PDU String.\r\n");
        goto Error;
    }

    if (!GSMHexToGSM(szPDU, uiLength, pByteBuffer, uiLength / 2, bytesUsed))
    {
        RIL_LOG_CRITICAL("CSilo_SMS::ParseCBM() - GSMHexToGSM conversion failed\r\n");
        goto Error;
    }

    pByteBuffer[bytesUsed] = '\0';

    pResponse->SetResultCode(RIL_UNSOL_RESPONSE_NEW_BROADCAST_SMS);

    if (!pResponse->SetData(pByteBuffer, bytesUsed, FALSE))
    {
        goto Error;
    }

    fRet = TRUE;

Error:
    free(szPDU);
    szPDU = NULL;

    if (!fRet)
    {
        free(pByteBuffer);
        pByteBuffer = NULL;
    }

    RIL_LOG_VERBOSE("CSilo_SMS::ParseCBM() - Exit\r\n");
    return fRet;
}

//
//
//
BOOL CSilo_SMS::ParseCDS(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_SMS::ParseCDS() - Enter\r\n");

    UINT32  uiLength = 0;
    const char* szDummy;
    char* szPDU = NULL;
    char   szAlpha[MAX_BUFFER_SIZE];
    BOOL  fRet = FALSE;

    if (NULL == pResponse)
    {
        RIL_LOG_CRITICAL("CSilo_SMS::ParseCDS() - pResponse is NULL.\r\n");
        goto Error;
    }

    pResponse->SetUnsolicitedFlag(TRUE);

    // Throw out the alpha chars if there are any
    (void)ExtractQuotedString(rszPointer, szAlpha, MAX_BUFFER_SIZE, rszPointer);

    // Parse ",<length><CR><LF>
    if (!ExtractUInt32(rszPointer, uiLength, rszPointer) ||
        !SkipString(rszPointer, m_szNewLine, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_SMS::ParseCDS() - Could not parse PDU Length.\r\n");
        goto Error;
    }

    //RIL_LOG_INFO("CDS=[%s]\r\n", CRLFExpandedString(rszPointer,strlen(rszPointer)).GetString() );

    // The PDU will be followed by m_szNewLine, so look for m_szNewLine
    // and use its position to determine the length of the PDU string
    if (!FindAndSkipString(rszPointer, m_szNewLine, szDummy))
    {
        // This isn't a complete message notification -- no need to parse it
        RIL_LOG_CRITICAL("CSilo_SMS::ParseCDS() - Could not find postfix; assuming this is an"
                " incomplete response.\r\n");
        goto Error;
    }
    else
    {
        // Override the given length with the actual length. Don't forget the '\0'.
        uiLength = ((UINT32)(szDummy - rszPointer)) - strlen(m_szNewLine) + 1;
        RIL_LOG_INFO("CSilo_SMS::ParseCDS() - Calculated PDU String length: %u chars.\r\n",
                uiLength);
    }

    szPDU = (char*) malloc(sizeof(char) * uiLength);
    if (NULL == szPDU)
    {
        RIL_LOG_CRITICAL("CSilo_SMS::ParseCDS() - Could not allocate memory for szPDU.\r\n");
        goto Error;
    }
    memset(szPDU, 0, sizeof(char) * uiLength);

    if (!ExtractUnquotedString(rszPointer, m_cTerminator, szPDU, uiLength, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_SMS::ParseCDS() - Could not parse PDU String.\r\n");
        goto Error;
    }

    // Ensure NULL Termination.
    szPDU[uiLength-1] = '\0';

    RIL_LOG_INFO("CSilo_SMS::ParseCDS() - PDU String: \"%s\".\r\n", szPDU);

    pResponse->SetResultCode(RIL_UNSOL_RESPONSE_NEW_SMS_STATUS_REPORT);

    if (!pResponse->SetData((void*) szPDU, sizeof(char) * uiLength, FALSE))
    {
        goto Error;
    }

    fRet = TRUE;

Error:
    if (!fRet)
    {
        free(szPDU);
        szPDU = NULL;
    }

    RIL_LOG_VERBOSE("CSilo_SMS::ParseCDS() - Exit\r\n");
    return fRet;

}

//
//
//
BOOL CSilo_SMS::ParseCMTI(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_SMS::ParseCMTI() - Enter\r\n");
    RIL_LOG_VERBOSE("CSilo_SMS::ParseCMTI() - Exit\r\n");
    return (ParseMessageInSim(pResponse, rszPointer, SILO_SMS_MSG_IN_SIM));
}

//
//
//
BOOL CSilo_SMS::ParseCBMI(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_SMS::ParseCBMI() - Enter\r\n");
    RIL_LOG_VERBOSE("CSilo_SMS::ParseCBMI() - Exit\r\n");
    return (ParseMessageInSim(pResponse, rszPointer, SILO_SMS_CELLBC_MSG_IN_SIM));
}

//
//
//
BOOL CSilo_SMS::ParseCDSI(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_SMS::ParseCDSI() - Enter\r\n");
    RIL_LOG_VERBOSE("CSilo_SMS::ParseCDSI() - Exit\r\n");
    return (ParseMessageInSim(pResponse, rszPointer, SILO_SMS_STATUS_MSG_IN_SIM));
}

