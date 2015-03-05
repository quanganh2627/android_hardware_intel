////////////////////////////////////////////////////////////////////////////
// te_xmm7260.h
//
// Copyright (C) Intel 2013.
//
//
// Description:
//    Overlay for the IMC 7260 modem
//
/////////////////////////////////////////////////////////////////////////////

#ifndef RRIL_TE_XMM7260_H
#define RRIL_TE_XMM7260_H

#include "te_xmm7160.h"

class CInitializer;

class CTE_XMM7260 : public CTE_XMM7160
{
public:

    enum { DEFAULT_PDN_CID = 1 };

    CTE_XMM7260(CTE& cte);
    virtual ~CTE_XMM7260();

private:

    CTE_XMM7260();

    //  Prevent assignment: Declared but not implemented.
    CTE_XMM7260(const CTE_XMM7260& rhs);  // Copy Constructor
    CTE_XMM7260& operator=(const CTE_XMM7260& rhs);  //  Assignment operator

public:
    // modem overrides

    virtual CInitializer* GetInitializer();

    // RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND 69
    virtual RIL_RESULT_CODE CoreStkSendEnvelopeCommand(REQUEST_DATA& rReqData,
                                                                  void* pData,
                                                                  UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseStkSendEnvelopeCommand(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE 70
    virtual RIL_RESULT_CODE CoreStkSendTerminalResponse(REQUEST_DATA& rReqData,
                                                                   void* pData,
                                                                   UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseStkSendTerminalResponse(RESPONSE_DATA& rRspData);

        // RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING 103
    virtual RIL_RESULT_CODE CoreReportStkServiceRunning(REQUEST_DATA& rReqData,
                                                                   void* pData,
                                                                   UINT32 uiDataSize);

    virtual BOOL ParseActivateUsatProfile(const char* szResponse);
    virtual RIL_RESULT_CODE ParseReportStkServiceRunning(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_STK_SEND_ENVELOPE_WITH_STATUS 107
    virtual RIL_RESULT_CODE ParseStkSendEnvelopeWithStatus(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_STK_GET_PROFILE
    virtual RIL_RESULT_CODE CoreStkGetProfile(REQUEST_DATA& rReqData,
                                                void* pData,
                                                UINT32 uiDataSize);
    // RIL_REQUEST_STK_SET_PROFILE
    virtual RIL_RESULT_CODE CoreStkSetProfile(REQUEST_DATA& rReqData,
                                                void* pData,
                                                UINT32 uiDataSize);

private:

    BOOL ParseEnvelopCommandResponse(const char* pszResponse, char* pszEnvelopResp,
            UINT32* puiBusy, UINT32* puiSw1, UINT32* puiSw2);

};

#endif
