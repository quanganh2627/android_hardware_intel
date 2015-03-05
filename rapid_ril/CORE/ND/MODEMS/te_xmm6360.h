////////////////////////////////////////////////////////////////////////////
// te_xmm6360.h
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Overlay for the IMC 6360 modem
//
/////////////////////////////////////////////////////////////////////////////

#ifndef RRIL_TE_XMM6360_H
#define RRIL_TE_XMM6360_H

#include "te_xmm6260.h"
#include "rril.h"
#include "channel_data.h"

class CEvent;
class CInitializer;

class CTE_XMM6360 : public CTE_XMM6260
{
public:

    CTE_XMM6360(CTE& cte);
    virtual ~CTE_XMM6360();

private:

    CTE_XMM6360();

    //  Prevent assignment: Declared but not implemented.
    CTE_XMM6360(const CTE_XMM6360& rhs);  // Copy Constructor
    CTE_XMM6360& operator=(const CTE_XMM6360& rhs);  //  Assignment operator

public:
    // modem overrides
    virtual char* GetBasicInitCommands(UINT32 uiChannelType);
    virtual char* GetUnlockInitCommands(UINT32 uiChannelType);

    virtual CInitializer* GetInitializer();

    // RIL_REQUEST_SETUP_DATA_CALL 27
    virtual RIL_RESULT_CODE CoreSetupDataCall(REQUEST_DATA& rReqData,
            void* pData, UINT32 uiDataSize, UINT32& uiCID);
    virtual BOOL PdpContextActivate(REQUEST_DATA& rReqData, void* pData,
            UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseEnterDataState(RESPONSE_DATA& rRspData);
    virtual BOOL SetupInterface(UINT32 uiCID);

    // RIL_REQUEST_BASEBAND_VERSION 51
    virtual RIL_RESULT_CODE CoreBasebandVersion(REQUEST_DATA& rReqData,
            void* pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseBasebandVersion(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_SET_INITIAL_ATTACH_APN 111
    virtual RIL_RESULT_CODE CoreSetInitialAttachApn(REQUEST_DATA& rReqData, void* pData,
            UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseSetInitialAttachApn(RESPONSE_DATA& rRspData);

    virtual RIL_RadioTechnology MapAccessTechnology(UINT32 uiStdAct);

protected:
    virtual RIL_RESULT_CODE ParseIpAddress(RESPONSE_DATA& rRspData);
    virtual RIL_RESULT_CODE ParseDns(RESPONSE_DATA& rRspData);

    virtual const char* GetRegistrationInitString();
    virtual const char* GetCsRegistrationReadString();
    virtual const char* GetPsRegistrationReadString();
    virtual const char* GetLocationUpdateString(BOOL bIsLocationUpdateEnabled);
    virtual const char* GetScreenOnString();

    virtual BOOL GetSetInitialAttachApnReqData(REQUEST_DATA& rReqData);
};

#endif
