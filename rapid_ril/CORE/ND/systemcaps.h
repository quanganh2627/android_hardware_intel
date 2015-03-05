////////////////////////////////////////////////////////////////////////////
// systemcaps.h
//
// Copyright 2013 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//      Defines the system capabilities used to configure the channels during
//      initialization.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef RRIL_SYSTEMCAPS_H
#define RRIL_SYSTEMCAPS_H

#include "types.h"
#include "constants.h"

class CSystemCapabilities
{
public:
    CSystemCapabilities() : m_bVoiceCapable(TRUE),
                            m_bSmsCapable(TRUE),
                            m_bStkCapable(TRUE),
                            m_bXDATASTATEnabled(FALSE),
                            m_bIMSCapable(FALSE),
                            m_uiModeOfOperation(MODE_CS_PS_VOICE_CENTRIC) {}

    BOOL IsVoiceCapable() { return m_bVoiceCapable; }
    BOOL IsSmsCapable() { return m_bSmsCapable; }
    BOOL IsStkCapable() { return m_bStkCapable; }
    BOOL IsXDATASTATReportingEnabled() { return m_bXDATASTATEnabled; }
    BOOL IsIMSCapable() { return m_bIMSCapable; }
    BOOL IsIMSApCentric() { return m_bIMSApCentric; }
    UINT32 GetModeOfOperation() { return m_uiModeOfOperation; }

    void SetVoiceCapable(BOOL bIsVoiceCapable) { m_bVoiceCapable = bIsVoiceCapable; }
    void SetSmsCapable(BOOL bIsSmsCapable) { m_bSmsCapable = bIsSmsCapable; }
    void SetIsStkCapable(BOOL bIsStkCapable) { m_bStkCapable = bIsStkCapable; }
    void SetXDATASTATReporting(BOOL bIsXDATASTATReporting)
                        { m_bXDATASTATEnabled = bIsXDATASTATReporting; }
    void SetIMSCapable(BOOL bIsIMSCapable) { m_bIMSCapable = bIsIMSCapable; }
    void SetIMSApCentric(BOOL bIsIMSApCentric) { m_bIMSApCentric = bIsIMSApCentric; }
    void SetModeOfOperation(UINT32 uiModeOfOperation) { m_uiModeOfOperation = uiModeOfOperation; }

private:
    BOOL m_bVoiceCapable;
    BOOL m_bSmsCapable;
    BOOL m_bStkCapable;
    BOOL m_bXDATASTATEnabled;
    BOOL m_bIMSCapable;
    BOOL m_bIMSApCentric;
    UINT32 m_uiModeOfOperation;
};

#endif // RRIL_SYSTEMCAPS_H
