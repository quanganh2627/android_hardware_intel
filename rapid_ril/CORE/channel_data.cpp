////////////////////////////////////////////////////////////////////////////
// channel_Data.cpp
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides implementations for helper functions used
//    to facilitate the use of multiple data channels.
//    GPRS/UMTS data (1st primary context)
//
/////////////////////////////////////////////////////////////////////////////

#include <stdio.h>

// This is for socket-related calls.
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <linux/gsmmux.h>

// This is for close().
#include <unistd.h>
#include <wchar.h>

#include "types.h"
#include "rillog.h"
#include "util.h"
#include "channelbase.h"
#include "channel_data.h"
#include "repository.h"
#include "rril.h"
#include "te.h"
#include "util.h"
#include "data_util.h"

extern char* g_szDataPort1;
extern char* g_szDataPort2;
extern char* g_szDataPort3;
extern char* g_szDataPort4;
extern char* g_szDataPort5;
extern BOOL  g_bIsSocket;

// Init strings for this channel.
//  GPRS/UMTS data (1st primary context)

// Add any init cmd strings for this channel during PowerOn or Ready boot phase
INITSTRING_DATA DataPowerOnInitString = { "" };
INITSTRING_DATA DataReadyInitString = { "" };

// used for 6360 and 7160 modems.
extern int m_hsiChannelsReservedForClass1;
extern int m_hsiDataDirect;
extern int m_dataProfilePathAssignation[NUMBER_OF_APN_PROFILE];

// used by 6360 and 7160 modems.
UINT32 g_uiHSIChannel[RIL_MAX_NUM_IPC_CHANNEL] = {0};

CChannel_Data::CChannel_Data(UINT32 uiChannel)
:   CChannel(uiChannel),
    m_dataFailCause(PDP_FAIL_NONE),
    m_uiContextID(0),
    m_dataState(E_DATA_STATE_IDLE),
    m_ipcDataChannelMin(RIL_DEFAULT_IPC_CHANNEL_MIN),
    m_isRoutingEnabled(FALSE),
    m_refCount(0)
{
    RIL_LOG_VERBOSE("CChannel_Data::CChannel_Data() - Enter\r\n");

    ResetDataCallInfo();

    CopyStringNullTerminate(m_szModemResourceName, RIL_DEFAULT_IPC_RESOURCE_NAME,
            sizeof(m_szModemResourceName));

    RIL_LOG_VERBOSE("CChannel_Data::CChannel_Data() - Exit\r\n");
}

CChannel_Data::~CChannel_Data()
{
    RIL_LOG_VERBOSE("CChannel_Data::~CChannel_Data() - Enter\r\n");

    delete[] m_paInitCmdStrings;
    m_paInitCmdStrings = NULL;

    RIL_LOG_VERBOSE("CChannel_Data::~CChannel_Data() - Exit\r\n");
}

//  Override from base class
BOOL CChannel_Data::OpenPort()
{
    BOOL bRetVal = FALSE;

    switch(m_uiRilChannel)
    {
        case RIL_CHANNEL_DATA1:
            RIL_LOG_INFO("CChannel_Data::OpenPort() - Opening COM Port: %s...\r\n", g_szDataPort1);
            RIL_LOG_INFO("CChannel_Data::OpenPort() - g_bIsSocket=[%d]...\r\n", g_bIsSocket);
            bRetVal = m_Port.Open(g_szDataPort1, g_bIsSocket);
            break;

        case RIL_CHANNEL_DATA2:
            RIL_LOG_INFO("CChannel_Data::OpenPort() - Opening COM Port: %s...\r\n", g_szDataPort2);
            RIL_LOG_INFO("CChannel_Data::OpenPort() - g_bIsSocket=[%d]...\r\n", g_bIsSocket);
            bRetVal = m_Port.Open(g_szDataPort2, g_bIsSocket);
            break;

        case RIL_CHANNEL_DATA3:
            RIL_LOG_INFO("CChannel_Data::OpenPort() - Opening COM Port: %s...\r\n", g_szDataPort3);
            RIL_LOG_INFO("CChannel_Data::OpenPort() - g_bIsSocket=[%d]...\r\n", g_bIsSocket);
            bRetVal = m_Port.Open(g_szDataPort3, g_bIsSocket);
            break;

        case RIL_CHANNEL_DATA4:
            RIL_LOG_INFO("CChannel_Data::OpenPort() - Opening COM Port: %s...\r\n", g_szDataPort4);
            RIL_LOG_INFO("CChannel_Data::OpenPort() - g_bIsSocket=[%d]...\r\n", g_bIsSocket);
            bRetVal = m_Port.Open(g_szDataPort4, g_bIsSocket);
            break;

        case RIL_CHANNEL_DATA5:
            RIL_LOG_INFO("CChannel_Data::OpenPort() - Opening COM Port: %s...\r\n", g_szDataPort5);
            RIL_LOG_INFO("CChannel_Data::OpenPort() - g_bIsSocket=[%d]...\r\n", g_bIsSocket);
            bRetVal = m_Port.Open(g_szDataPort5, g_bIsSocket);
            break;

        default:
            RIL_LOG_CRITICAL("CChannel_Data::OpenPort() -"
                    "channel does not exist m_uiRilChannel=%d\r\n", m_uiRilChannel);
            bRetVal = FALSE;
            break;
    }



    RIL_LOG_INFO("CChannel_Data::OpenPort() - Opening COM Port: %s\r\n",
            bRetVal ? "SUCCESS" : "FAILED!");

    return bRetVal;
}

BOOL CChannel_Data::FinishInit()
{
    RIL_LOG_VERBOSE("CChannel_Data::FinishInit() - Enter\r\n");

    BOOL bRet = FALSE;
    CRepository repository;

    //  Init our channel AT init commands.
    m_paInitCmdStrings = new INITSTRING_DATA[COM_MAX_INDEX];
    if (!m_paInitCmdStrings)
    {
        RIL_LOG_CRITICAL("CChannel_Data::FinishInit() : chnl=[%d] Could not create new "
                "INITSTRING_DATA\r\n", m_uiRilChannel);
        goto Error;
    }

    // Set the init command strings for this channel
    m_paInitCmdStrings[COM_BASIC_INIT_INDEX].szCmd = m_szChannelBasicInitCmd;
    m_paInitCmdStrings[COM_UNLOCK_INIT_INDEX].szCmd = m_szChannelUnlockInitCmd;

    m_paInitCmdStrings[COM_POWER_ON_INIT_INDEX] = DataPowerOnInitString;
    m_paInitCmdStrings[COM_READY_INIT_INDEX] = DataReadyInitString;

    // read the minimum data channel index
    if (!repository.Read(g_szGroupModem, g_szIpcDataChannelMin, m_ipcDataChannelMin))
    {
        RIL_LOG_WARNING("CChannel_Data::FinishInit() : Could not read min"
                " IPC Data channel from repository\r\n");
        m_ipcDataChannelMin = RIL_DEFAULT_IPC_CHANNEL_MIN;
    }

    //  Grab the Modem data channel resource name
    if (!repository.Read(g_szGroupModem, g_szModemResourceName, m_szModemResourceName,
                MAX_MDM_RESOURCE_NAME_SIZE))
    {
        RIL_LOG_WARNING("CChannel_Data::FinishInit() - Could not read modem resource"
                " name from repository, using default\r\n");
        // Set default value.
        CopyStringNullTerminate(m_szModemResourceName, RIL_DEFAULT_IPC_RESOURCE_NAME,
                sizeof(m_szModemResourceName));
    }
    RIL_LOG_INFO("CChannel_Data::FinishInit() - m_szModemResourceName=[%s]\r\n",
                m_szModemResourceName);

    bRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CChannel_Data::FinishInit() - Exit\r\n");
    return bRet;
}

//
//  Returns a pointer to the channel linked to the given interface name
//
CChannel_Data* CChannel_Data::GetChnlFromIfName(const char * ifName)
{
    RIL_LOG_VERBOSE("CChannel_Data::GetChnlFromIfName() - Enter\r\n");

    CMutex::Lock(CSystemManager::GetInstance().GetDataChannelAccessorMutex());

    extern CChannel* g_pRilChannel[RIL_CHANNEL_MAX];
    CChannel_Data* pChannelData = NULL;

    for (UINT32 i = RIL_CHANNEL_DATA1; i < g_uiRilChannelCurMax && i < RIL_CHANNEL_MAX; i++)
    {
        if (NULL == g_pRilChannel[i]) // could be NULL if reserved channel
            continue;

        CChannel_Data* pTemp = static_cast<CChannel_Data*>(g_pRilChannel[i]);
        if (pTemp)
        {
            char szTempName[MAX_INTERFACE_NAME_SIZE];
            pTemp->GetInterfaceName(szTempName, MAX_INTERFACE_NAME_SIZE);
            if (!strncmp(szTempName, ifName, MAX_INTERFACE_NAME_SIZE)) {
                pChannelData = pTemp;
                break;
            }
        }
    }

    CMutex::Unlock(CSystemManager::GetInstance().GetDataChannelAccessorMutex());

    RIL_LOG_VERBOSE("CChannel_Data::GetChnlFromIfName() - Exit\r\n");
    return pChannelData;
}

//
//  Returns a pointer to the channel linked to the given context ID
//
CChannel_Data* CChannel_Data::GetChnlFromContextID(UINT32 uiContextID)
{
    RIL_LOG_VERBOSE("CChannel_Data::GetChnlFromContextID() - Enter\r\n");

    CMutex::Lock(CSystemManager::GetInstance().GetDataChannelAccessorMutex());

    extern CChannel* g_pRilChannel[RIL_CHANNEL_MAX];
    CChannel_Data* pChannelData = NULL;

    for (UINT32 i = RIL_CHANNEL_DATA1; i < g_uiRilChannelCurMax && i < RIL_CHANNEL_MAX; i++)
    {
        if (NULL == g_pRilChannel[i]) // could be NULL if reserved channel
            continue;

        CChannel_Data* pTemp = static_cast<CChannel_Data*>(g_pRilChannel[i]);
        if (pTemp && pTemp->GetContextID() == uiContextID)
        {
            pChannelData = pTemp;
            break;
        }
    }

Error:
    CMutex::Unlock(CSystemManager::GetInstance().GetDataChannelAccessorMutex());

    RIL_LOG_VERBOSE("CChannel_Data::GetChnlFromContextID() - Exit\r\n");
    return pChannelData;
}

//  Used for 6360 and 7160 modems.
//  Returns a free RIL channel and a free hsi channel to use for data.
//  [out] UINT32 outCID - If successful, the new context ID, else 0.
//  return value - CChannel_Data* - If successful, the free data channel, else NULL.
//  Note: If successful the returned CChannel_Data* will have the new context ID set.
//        m_hsiDirect will be set to TRUE if the data channel use directly a hsi channel.
//        m_hsiChannel will contains the free hsi channel to use.
//  The scheme is that the context ID will be set to the data channel number.
//  i.e. RIL_CHANNEL_DATA1 = CID of 1
//       RIL_CHANNEL_DATA2 = CID of 2
//       ...
//       RIL_CHANNEL_DATA5 = CID of 5
//  No other context ID
CChannel_Data* CChannel_Data::GetFreeChnlsRilHsi(UINT32& outCID, int dataProfile)
{
    RIL_LOG_VERBOSE("CChannel_Data::GetFreeChnlsRilHsi() - Enter\r\n");

    CMutex::Lock(CSystemManager::GetInstance().GetDataChannelAccessorMutex());

    extern CChannel* g_pRilChannel[RIL_CHANNEL_MAX];

    CChannel_Data* pChannelData = NULL;
    int hsiChannel = -1;
    int hsiDirect = FALSE;

    int ipcDataChannelMin = 0;

    // First try to get a free RIL channel, then for class 1
    // or class 2 apn, try to get a hsi channel.
    pChannelData = CChannel_Data::GetFreeChnl(outCID);
    if (pChannelData != NULL)
    {
        ipcDataChannelMin = pChannelData->GetIpcDataChannelMin();

        if (dataProfile >= 0 && dataProfile < NUMBER_OF_APN_PROFILE)
        {
            // According to the data profile class try to associate
            // a hsi channel to the RIL channel.
            switch (m_dataProfilePathAssignation[dataProfile])
            {
                case 1:
                    // For APN of the class 1 a hsi channel is available. Find the first available.
                    RIL_LOG_INFO("CChannel_Data::GetFreeChnlsRilHsi() -"
                            " data profile class: %d.\r\n",
                            m_dataProfilePathAssignation[dataProfile]);
                    hsiChannel = GetFreeHSIChannel(outCID,
                            ipcDataChannelMin, ipcDataChannelMin + m_hsiChannelsReservedForClass1);
                    if (hsiChannel == -1)
                    {
                        RIL_LOG_INFO("CChannel_Data::GetFreeChnlsRilHsi() - No hsi channel for"
                                " a data profile class 1.\r\n");
                        pChannelData->SetContextID(0);
                        pChannelData = NULL;
                        goto Error;
                    }
                    hsiDirect = TRUE;
                    break;

                case 2:
                    // For APN of the class 2, check if there is a free hsi channel
                    // that can be used.
                    RIL_LOG_INFO("CChannel_Data::GetFreeChnlsRilHsi() - data profile "
                            "class: %d.\r\n", m_dataProfilePathAssignation[dataProfile]);
                    hsiChannel = GetFreeHSIChannel(outCID,
                            ipcDataChannelMin + m_hsiChannelsReservedForClass1,
                            ipcDataChannelMin + m_hsiDataDirect);
                    if (hsiChannel != -1)
                    {
                        hsiDirect = TRUE;
                    }
                    break;

                default:
                    break;

            }
        }
        pChannelData->m_hsiChannel = hsiChannel;
        pChannelData->m_hsiDirect = hsiDirect;
        pChannelData->m_dataProfile = dataProfile;
    }

Error:
    if (NULL == pChannelData)
    {
        // Error, all channels full!
        RIL_LOG_CRITICAL("CChannel_Data::GetFreeChnlsRilHsi() - All channels full!!\r\n");
        outCID = 0;
    }

    CMutex::Unlock(CSystemManager::GetInstance().GetDataChannelAccessorMutex());

    RIL_LOG_VERBOSE("CChannel_Data::GetFreeChnlsRilHsi() - Exit\r\n");
    return pChannelData;
}

int CChannel_Data::GetFreeHSIChannel(UINT32 uiCID, int sIndex, int eIndex)
{
    RIL_LOG_VERBOSE("CChannel_Data::GetFreeHSIChannel() - Enter\r\n");

    CMutex::Lock(CSystemManager::GetInstance().GetDataChannelAccessorMutex());

    if (sIndex < 0 || eIndex > RIL_MAX_NUM_IPC_CHANNEL)
    {
        RIL_LOG_VERBOSE("CChannel_Data::GetFreeHSIChannel() - Index error\r\n");
        return -1;
    }

    for (int i = sIndex; i < eIndex; i++)
    {
        if (g_uiHSIChannel[i] == 0)
        {
            g_uiHSIChannel[i] = uiCID ;
            RIL_LOG_VERBOSE("CChannel_Data::GetFreeHSIChannel() - Success - Exit\r\n");
            CMutex::Unlock(CSystemManager::GetInstance().GetDataChannelAccessorMutex());
            return i;
        }
    }

    CMutex::Unlock(CSystemManager::GetInstance().GetDataChannelAccessorMutex());

    RIL_LOG_VERBOSE("CChannel_Data::GetFreeHSIChannel() - Not Success - Exit\r\n");
    return -1;
}

bool CChannel_Data::FreeHSIChannel(UINT32 uiCID)
{
    RIL_LOG_VERBOSE("CChannel_Data::FreeHSIChannel() - Enter\r\n");

    CMutex::Lock(CSystemManager::GetInstance().GetDataChannelAccessorMutex());

    for (int i = 0; i < RIL_MAX_NUM_IPC_CHANNEL; i++)
    {
        if (g_uiHSIChannel[i] == uiCID)
        {
            g_uiHSIChannel[i] = 0 ;
            RIL_LOG_VERBOSE("CChannel_Data::FreeHSIChannel() - Exit\r\n");
            CMutex::Unlock(CSystemManager::GetInstance().GetDataChannelAccessorMutex());
            return true;
        }
    }

    CMutex::Unlock(CSystemManager::GetInstance().GetDataChannelAccessorMutex());

    RIL_LOG_VERBOSE("CChannel_Data::FreeHSIChannel() - Exit\r\n");
    return false;
}

//
//  Returns a free channel to use for data
//  [out] UINT32 outCID - If successful, the new context ID, else 0.
//  return value - CChannel_Data* - If successful, the free data channel, else NULL.
//  Note: If successful the returned CChannel_Data* will have the new context ID set.
//
//  The scheme is that the context ID will be set to the data channel number.
//  i.e. RIL_CHANNEL_DATA1 = CID of 1
//       RIL_CHANNEL_DATA2 = CID of 2
//       ...
//       RIL_CHANNEL_DATA5 = CID of 5
//  No other context ID
CChannel_Data* CChannel_Data::GetFreeChnl(UINT32& outCID)
{
    RIL_LOG_VERBOSE("CChannel_Data::GetFreeChnl() - Enter\r\n");

    CMutex::Lock(CSystemManager::GetInstance().GetDataChannelAccessorMutex());

    extern CChannel* g_pRilChannel[RIL_CHANNEL_MAX];
    CChannel_Data* pChannelData = NULL;

    for (UINT32 i = RIL_CHANNEL_DATA1; i < g_uiRilChannelCurMax && i < RIL_CHANNEL_MAX; i++)
    {
        if (NULL == g_pRilChannel[i]) // could be NULL if reserved channel
            continue;

        CChannel_Data* pTemp = static_cast<CChannel_Data*>(g_pRilChannel[i]);
        if (pTemp && pTemp->GetContextID() == 0)
        {
            //  We found a free data channel.
            pChannelData = pTemp;
            outCID = (i - RIL_CHANNEL_DATA1) + 1;

            RIL_LOG_INFO("CChannel_Data::GetFreeChnl() - ****** Setting chnl=[%d] to CID=[%d]"
                    " ******\r\n", i, outCID);
            pChannelData->SetContextID(outCID);
            break;
        }
    }

Error:
    if (NULL == pChannelData)
    {
        // Error, all channels full!
        RIL_LOG_CRITICAL("CChannel_Data::GetFreeChnl() - All channels full!!\r\n");
        outCID = 0;
    }

    CMutex::Unlock(CSystemManager::GetInstance().GetDataChannelAccessorMutex());

    RIL_LOG_VERBOSE("CChannel_Data::GetFreeChnl() - Exit\r\n");
    return pChannelData;
}

//
//  Returns the Data Channel for the specified RIL channel number
//
CChannel_Data* CChannel_Data::GetChnlFromRilChannelNumber(UINT32 index)
{
    RIL_LOG_VERBOSE("CChannel_Data::GetChnlFromRilChannelNumber() - Enter\r\n");

    CMutex::Lock(CSystemManager::GetInstance().GetDataChannelAccessorMutex());

    extern CChannel* g_pRilChannel[RIL_CHANNEL_MAX];
    CChannel_Data* pChannelData = NULL;

    for (UINT32 i = RIL_CHANNEL_DATA1; i < g_uiRilChannelCurMax && i < RIL_CHANNEL_MAX; i++)
    {
        if (NULL == g_pRilChannel[i]) // could be NULL if reserved channel
            continue;

        CChannel_Data* pTemp = static_cast<CChannel_Data*>(g_pRilChannel[i]);
        if (pTemp && pTemp->GetRilChannel() == index)
        {
            pChannelData = pTemp;
            break;
        }
    }

Error:
    CMutex::Unlock(CSystemManager::GetInstance().GetDataChannelAccessorMutex());

    RIL_LOG_VERBOSE("CChannel_Data::GetChnlFromRilChannelNumber() - Exit\r\n");
    return pChannelData;
}



UINT32 CChannel_Data::GetContextID() const
{
    RIL_LOG_VERBOSE("CChannel_Data::GetContextID() - Enter\r\n");

    CMutex::Lock(CSystemManager::GetInstance().GetDataChannelAccessorMutex());

    UINT32 nCID = m_uiContextID;

    CMutex::Unlock(CSystemManager::GetInstance().GetDataChannelAccessorMutex());

    RIL_LOG_VERBOSE("CChannel_Data::GetContextID() - Exit\r\n");
    return nCID;
}


//
//  Sets this channel's context ID value
//
BOOL CChannel_Data::SetContextID(UINT32 dwContextID)
{
    RIL_LOG_VERBOSE("CChannel_Data::SetContextID() - Enter\r\n");

    CMutex::Lock(CSystemManager::GetInstance().GetDataChannelAccessorMutex());

    m_uiContextID = dwContextID;

    CMutex::Unlock(CSystemManager::GetInstance().GetDataChannelAccessorMutex());

    RIL_LOG_VERBOSE("CChannel_Data::SetContextID() - Exit\r\n");
    return TRUE;
}

void CChannel_Data::ResetDataCallInfo()
{
    RIL_LOG_VERBOSE("CChannel_Data::ResetDataCallInfo() - Enter\r\n");

    SetDataFailCause(PDP_FAIL_NONE);
    SetDataState(E_DATA_STATE_IDLE);

    UINT32 uiModemType = CTE::GetTE().GetModemType();
    if (MODEM_TYPE_XMM6360 == uiModemType
            || MODEM_TYPE_XMM7160 == uiModemType
            || MODEM_TYPE_XMM7260 == uiModemType)
    {
        FreeHSIChannel(m_uiContextID);
    }

    SetContextID(0);

    m_refCount = 0;
    m_isRoutingEnabled = FALSE;
    m_szApn[0] = '\0';
    m_szPdpType[0] = '\0';
    m_szInterfaceName[0] = '\0';
    m_szIpAddr[0] = '\0';
    m_szIpAddr2[0] = '\0';
    m_szDNS1[0] = '\0';
    m_szDNS2[0] = '\0';
    m_szIpV6DNS1[0] = '\0';
    m_szIpV6DNS2[0] = '\0';
    m_szIpV4Gateway[0] = '\0';
    m_szIpV6Gateway[0] = '\0';

    RIL_LOG_VERBOSE("CChannel_Data::ResetDataCallInfo() - Exit\r\n");
}

void CChannel_Data::SetDataFailCause(int cause)
{
    RIL_LOG_VERBOSE("CChannel_Data::SetDataFailCause() - Enter\r\n");

    m_dataFailCause = cause;

    RIL_LOG_VERBOSE("CChannel_Data::SetDataFailCause() - Exit\r\n");
}

int CChannel_Data::GetDataFailCause()
{
    RIL_LOG_VERBOSE("CChannel_Data::GetDataFailCause() - Enter/Exit\r\n");
    return m_dataFailCause;
}

void CChannel_Data::SetApn(const char* pApn)
{
    RIL_LOG_VERBOSE("CChannel_Data::SetApn() - Enter/Exit\r\n");

    CopyStringNullTerminate(m_szApn, pApn, MAX_BUFFER_SIZE);
}

void CChannel_Data::GetApn(char* pApn, const int maxSize)
{
    RIL_LOG_VERBOSE("CChannel_Data::GetApn() - Enter\r\n");

    if (NULL != pApn && 0 < maxSize)
    {
        CopyStringNullTerminate(pApn, m_szApn, maxSize);
    }

    RIL_LOG_VERBOSE("CChannel_Data::GetApn() - Exit\r\n");
}

BOOL CChannel_Data::IsApnEqual(const char* pApn)
{
    RIL_LOG_VERBOSE("CChannel_Data::IsApnEqual() - Enter\r\n");

    BOOL bRet = FALSE;

    if (m_szApn[0] == '\0' || NULL == pApn)
    {
        bRet = FALSE;
    }
    else
    {
        if (NULL != strcasestr(m_szApn, pApn))
        {
            bRet = TRUE;
        }
    }

    return bRet;
}

void CChannel_Data::SetPdpType(const char* pPdpType)
{
    RIL_LOG_VERBOSE("CChannel_Data::SetPdpType() - Enter\r\n");

    strncpy(m_szPdpType, pPdpType, MAX_PDP_TYPE_SIZE-1);
    m_szPdpType[MAX_PDP_TYPE_SIZE-1] = '\0';

    RIL_LOG_VERBOSE("CChannel_Data::SetPdpType() - Exit\r\n");
}

void CChannel_Data::GetPdpType(char* pPdpType, const int maxSize)
{
    RIL_LOG_VERBOSE("CChannel_Data::GetPdpType() - Enter\r\n");

    if (NULL != pPdpType && 0 < maxSize)
    {
        strncpy(pPdpType, m_szPdpType, maxSize-1);
        pPdpType[maxSize-1] = '\0';
    }

    RIL_LOG_VERBOSE("CChannel_Data::GetPdpType() - Exit\r\n");
}

void CChannel_Data::SetInterfaceName(const char* pInterfaceName)
{
    RIL_LOG_VERBOSE("CChannel_Data::SetInterfaceName() - Enter\r\n");

    strncpy(m_szInterfaceName, pInterfaceName, MAX_INTERFACE_NAME_SIZE-1);
    m_szInterfaceName[MAX_INTERFACE_NAME_SIZE-1] = '\0';

    RIL_LOG_VERBOSE("CChannel_Data::SetInterfaceName() - Exit\r\n");
}

void CChannel_Data::GetInterfaceName(char* pInterfaceName, const int maxSize)
{
    RIL_LOG_VERBOSE("CChannel_Data::GetInterfaceName() - Enter\r\n");

    if (NULL != pInterfaceName && 0 < maxSize)
    {
        strncpy(pInterfaceName, m_szInterfaceName, maxSize-1);
        pInterfaceName[maxSize-1] = '\0';
    }

    RIL_LOG_VERBOSE("CChannel_Data::GetInterfaceName() - Exit\r\n");
}

void CChannel_Data::SetIpAddress(const char* pIpAddr1,
                                        const char* pIpAddr2)
{
    RIL_LOG_VERBOSE("CChannel_Data::SetIpAddress() - Enter\r\n");

    strncpy(m_szIpAddr, pIpAddr1, MAX_IPADDR_SIZE-1);
    m_szIpAddr[MAX_IPADDR_SIZE-1] = '\0';

    strncpy(m_szIpAddr2, pIpAddr2, MAX_IPADDR_SIZE-1);
    m_szIpAddr2[MAX_IPADDR_SIZE-1] = '\0';

    RIL_LOG_VERBOSE("CChannel_Data::SetIpAddress() - Exit\r\n");
}

void CChannel_Data::GetIpAddress(char* pIpAddr, const int maxIpAddrSize,
                                char* pIpAddr2, const int maxIpAddr2Size)
{
    RIL_LOG_VERBOSE("CChannel_Data::GetIpAddress() - Enter\r\n");

    if (NULL != pIpAddr && 0 < maxIpAddrSize)
    {
        strncpy(pIpAddr, m_szIpAddr, maxIpAddrSize-1);
        pIpAddr[maxIpAddrSize-1] = '\0';
    }

    if (NULL != pIpAddr2 && 0 < maxIpAddr2Size)
    {
        strncpy(pIpAddr2, m_szIpAddr2, maxIpAddr2Size-1);
        pIpAddr2[maxIpAddr2Size-1] = '\0';
    }

    RIL_LOG_VERBOSE("CChannel_Data::GetIpAddress() - Exit\r\n");
}

void CChannel_Data::SetDNS(const char* pDNS1, const char* pDNS2,
                                const char* pIpV6DNS1, const char* pIpV6DNS2)
{
    RIL_LOG_VERBOSE("CChannel_Data::SetDNS() - Enter\r\n");

    strncpy(m_szDNS1, pDNS1, MAX_IPADDR_SIZE-1);
    m_szDNS1[MAX_IPADDR_SIZE-1] = '\0';

    strncpy(m_szDNS2, pDNS2, MAX_IPADDR_SIZE-1);
    m_szDNS2[MAX_IPADDR_SIZE-1] = '\0';

    strncpy(m_szIpV6DNS1, pIpV6DNS1, MAX_IPADDR_SIZE-1);
    m_szIpV6DNS1[MAX_IPADDR_SIZE-1] = '\0';

    strncpy(m_szIpV6DNS2, pIpV6DNS2, MAX_IPADDR_SIZE-1);
    m_szIpV6DNS2[MAX_IPADDR_SIZE-1] = '\0';

    RIL_LOG_VERBOSE("CChannel_Data::SetDNS() - Exit\r\n");
}

void CChannel_Data::GetDNS(char* pDNS1, const int maxDNS1Size,
                                char* pDNS2, const int maxDNS2Size,
                                char* pIpV6DNS1, const int maxIpV6DNS1Size,
                                char* pIpV6DNS2, const int maxIpV6DNS2Size)
{
    RIL_LOG_VERBOSE("CChannel_Data::GetDNS() - Enter\r\n");

    if (NULL != pDNS1 && 0 < maxDNS1Size)
    {
        strncpy(pDNS1, m_szDNS1, maxDNS1Size-1);
        pDNS1[maxDNS1Size-1] = '\0';
    }

    if (NULL != pDNS2 && 0 < maxDNS2Size)
    {
        strncpy(pDNS2, m_szDNS2, maxDNS2Size-1);
        pDNS2[maxDNS2Size-1] = '\0';
    }

    if (NULL != pIpV6DNS1 && 0 < maxIpV6DNS1Size)
    {
        strncpy(pIpV6DNS1, m_szIpV6DNS1, maxIpV6DNS1Size-1);
        pIpV6DNS1[maxIpV6DNS1Size-1] = '\0';
    }

    if (NULL != pIpV6DNS2 && 0 < maxIpV6DNS2Size)
    {
        strncpy(pIpV6DNS2, m_szIpV6DNS2, maxIpV6DNS2Size-1);
        pIpV6DNS2[maxIpV6DNS2Size-1] = '\0';
    }

    RIL_LOG_VERBOSE("CChannel_Data::GetDNS() - Exit\r\n");
}

void CChannel_Data::SetPcscf(const char* pPCSCF1, const char* pPCSCF2,
                               const char* pIpV6PCSCF1, const char* pIpV6PCSCF2)
{
    RIL_LOG_VERBOSE("CChannel_Data::SetPcscf() - Enter\r\n");

    if (NULL != pPCSCF1)
    {
        CopyStringNullTerminate(m_szPCSCF1, pPCSCF1, MAX_IPADDR_SIZE);
    }

    if (NULL != pPCSCF2)
    {
        CopyStringNullTerminate(m_szPCSCF2, pPCSCF2, MAX_IPADDR_SIZE);
    }

    if (NULL != pIpV6PCSCF1)
    {
        CopyStringNullTerminate(m_szIpV6PCSCF1, pIpV6PCSCF1, MAX_IPADDR_SIZE);
    }

    if (NULL != pIpV6PCSCF2)
    {
        CopyStringNullTerminate(m_szIpV6PCSCF2, pIpV6PCSCF2, MAX_IPADDR_SIZE);
    }

    RIL_LOG_VERBOSE("CChannel_Data::SetPcscf() - Exit\r\n");
}

void CChannel_Data::GetPcscf(char* pPCSCF1, const int maxPcscf1Size,
                               char* pPCSCF2, const int maxPcscf2Size,
                               char* pIpV6PCSCF1, const int maxIpV6Pcscf1Size,
                               char* pIpV6PCSCF2, const int maxIpV6Pcscf2Size)
{
    RIL_LOG_VERBOSE("CChannel_Data::GetPcscf() - Enter\r\n");

    if (NULL != pPCSCF1 && 0 < maxPcscf1Size)
    {
        CopyStringNullTerminate(pPCSCF1, m_szPCSCF1, maxPcscf1Size);
    }

    if (NULL != pPCSCF2 && 0 < maxPcscf2Size)
    {
        CopyStringNullTerminate(pPCSCF2, m_szPCSCF2, maxPcscf2Size);
    }

    if (NULL != pIpV6PCSCF1 && 0 < maxIpV6Pcscf1Size)
    {
        CopyStringNullTerminate(pIpV6PCSCF1, m_szIpV6PCSCF1, maxIpV6Pcscf1Size);
    }

    if (NULL != pIpV6PCSCF2 && 0 < maxIpV6Pcscf2Size)
    {
        CopyStringNullTerminate(pIpV6PCSCF2, m_szIpV6PCSCF2, maxIpV6Pcscf2Size);
    }

    RIL_LOG_VERBOSE("CChannel_Data::GetPcscf() - Exit\r\n");
}

void CChannel_Data::SetGateway(const char* pIpV4Gateway, const char* pIpV6Gateway)
{
    RIL_LOG_VERBOSE("CChannel_Data::SetGateway() - Enter\r\n");

    if (NULL != pIpV4Gateway)
    {
        CopyStringNullTerminate(m_szIpV4Gateway, pIpV4Gateway, MAX_IPADDR_SIZE);
    }

    if (NULL != pIpV6Gateway)
    {
        CopyStringNullTerminate(m_szIpV6Gateway, pIpV6Gateway, MAX_IPADDR_SIZE);
    }

    RIL_LOG_VERBOSE("CChannel_Data::SetGateway() - Exit\r\n");
}

void CChannel_Data::GetGateway(char* pIpV4Gateway, const int maxIpV4GatewaySize,
        char* pIpV6Gateway, const int maxIpV6GatewaySize)
{
    RIL_LOG_VERBOSE("CChannel_Data::GetGateway() - Enter\r\n");

    if (NULL != pIpV4Gateway && 0 < maxIpV4GatewaySize)
    {
        CopyStringNullTerminate(pIpV4Gateway, m_szIpV4Gateway, maxIpV4GatewaySize);
    }

    if (NULL != pIpV4Gateway && 0 < maxIpV6GatewaySize)
    {
        CopyStringNullTerminate(pIpV6Gateway, m_szIpV6Gateway, maxIpV6GatewaySize);
    }

    RIL_LOG_VERBOSE("CChannel_Data::GetGateway() - Exit\r\n");
}

void CChannel_Data::SetDataState(int state)
{
    RIL_LOG_VERBOSE("CChannel_Data::SetDataState() - Enter\r\n");

    CMutex::Lock(CSystemManager::GetInstance().GetDataChannelAccessorMutex());

    m_dataState = state;

    CMutex::Unlock(CSystemManager::GetInstance().GetDataChannelAccessorMutex());

    RIL_LOG_VERBOSE("CChannel_Data::SetDataState() - Exit\r\n");
}

int CChannel_Data::GetDataState()
{
    RIL_LOG_VERBOSE("CChannel_Data::GetDataState() - Enter\r\n");

    CMutex::Lock(CSystemManager::GetInstance().GetDataChannelAccessorMutex());

    int state = m_dataState;

    CMutex::Unlock(CSystemManager::GetInstance().GetDataChannelAccessorMutex());

    RIL_LOG_VERBOSE("CChannel_Data::GetDataState() - Exit\r\n");
    return state;
}

void CChannel_Data::GetDataCallInfo(S_DATA_CALL_INFO& rDataCallInfo)
{
    RIL_LOG_VERBOSE("CChannel_Data::GetDataCallInfo() - Enter\r\n");

    CMutex::Lock(CSystemManager::GetInstance().GetDataChannelAccessorMutex());

    rDataCallInfo.failCause = m_dataFailCause;
    rDataCallInfo.uiCID = m_uiContextID;
    rDataCallInfo.state = m_dataState;

    strncpy(rDataCallInfo.szPdpType, m_szPdpType, MAX_PDP_TYPE_SIZE-1);
    rDataCallInfo.szPdpType[MAX_PDP_TYPE_SIZE-1] = '\0';

    strncpy(rDataCallInfo.szInterfaceName, m_szInterfaceName,
                                        MAX_INTERFACE_NAME_SIZE-1);
    rDataCallInfo.szInterfaceName[MAX_INTERFACE_NAME_SIZE-1] = '\0';

    strncpy(rDataCallInfo.szIpAddr1, m_szIpAddr, MAX_IPADDR_SIZE-1);
    rDataCallInfo.szIpAddr1[MAX_IPADDR_SIZE-1] = '\0';

    strncpy(rDataCallInfo.szIpAddr2, m_szIpAddr2, MAX_IPADDR_SIZE-1);
    rDataCallInfo.szIpAddr2[MAX_IPADDR_SIZE-1] = '\0';

    strncpy(rDataCallInfo.szDNS1, m_szDNS1, MAX_IPADDR_SIZE-1);
    rDataCallInfo.szDNS1[MAX_IPADDR_SIZE-1] = '\0';

    strncpy(rDataCallInfo.szDNS2, m_szDNS2, MAX_IPADDR_SIZE-1);
    rDataCallInfo.szDNS2[MAX_IPADDR_SIZE-1] = '\0';

    strncpy(rDataCallInfo.szIpV6DNS1, m_szIpV6DNS1, MAX_IPADDR_SIZE-1);
    rDataCallInfo.szIpV6DNS1[MAX_IPADDR_SIZE-1] = '\0';

    strncpy(rDataCallInfo.szIpV6DNS2, m_szIpV6DNS2, MAX_IPADDR_SIZE-1);
    rDataCallInfo.szIpV6DNS2[MAX_IPADDR_SIZE-1] = '\0';

    PrintStringNullTerminate(rDataCallInfo.szGateways, MAX_IPADDR_SIZE, "%s %s",
            m_szIpV4Gateway, m_szIpV6Gateway);

    CMutex::Unlock(CSystemManager::GetInstance().GetDataChannelAccessorMutex());

    RIL_LOG_VERBOSE("CChannel_Data::GetDataCallInfo() - Exit\r\n");
}

int CChannel_Data::GetMuxControlChannel()
{
    RIL_LOG_VERBOSE("CChannel_Data::GetMuxControlChannel() - Enter\r\n");

    int muxControlChannel = -1;
    UINT32 uiRilChannel = GetRilChannel();

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
            RIL_LOG_CRITICAL("CChannel_Data::GetMuxControlChannel() - Unknown mux channel"
                    "for RIL Channel [%u] \r\n", uiRilChannel);
    }

    RIL_LOG_VERBOSE("CChannel_Data::GetMuxControlChannel() - Exit\r\n");

    return muxControlChannel;
}

void CChannel_Data::RemoveInterface(BOOL bKeepInterfaceUp)
{
    RIL_LOG_VERBOSE("CChannel_Data::RemoveInterface() - Enter\r\n");

    char szNetworkInterfaceName[MAX_INTERFACE_NAME_SIZE] = {'\0'};
    CChannel_Data* pChannelData = NULL;
    struct gsm_netconfig netconfig;
    int fd = GetFD();
    int ret = -1;
    int s = -1;
    UINT32 uiRilChannel = GetRilChannel();
    BOOL bIsHSIDirect = IsHSIDirect();
    int state = GetDataState();

    GetInterfaceName(szNetworkInterfaceName, sizeof(szNetworkInterfaceName));

    RIL_LOG_INFO("CChannel_Data::RemoveInterface() - szNetworkInterfaceName=[%s]\r\n",
            szNetworkInterfaceName);

    if (!bIsHSIDirect && bKeepInterfaceUp)
    {
        RIL_LOG_CRITICAL("CChannel_Data::RemoveInterface() : cannot keep TTY interface up.\r\n");
        bKeepInterfaceUp = FALSE;
    }

    if (!bIsHSIDirect)
    {
        if (E_DATA_STATE_IDLE != state
                && E_DATA_STATE_INITING != state
                && E_DATA_STATE_ACTIVATING != state)
        {
            // Blocking TTY flow.
            // Security in order to avoid IP data in response buffer.
            // Not mandatory.
            BlockAndFlushChannel(BLOCK_CHANNEL_BLOCK_TTY,
                    FLUSH_CHANNEL_NO_FLUSH);
        }

        // Put the channel back into AT command mode
        netconfig.adaption = 3;
        netconfig.protocol = htons(ETH_P_IP);

        if (fd >= 0)
        {
            RIL_LOG_INFO("CChannel_Data::RemoveInterface() -"
                    " ***** PUTTING channel=[%u] in AT COMMAND MODE *****\r\n", uiRilChannel);
            ret = ioctl(fd, GSMIOC_DISABLE_NET, &netconfig);
        }
    }
    else
    {
        /*
         * HSI inteface ENABLE/DISABLE can be done only by the HSI driver.
         * Rapid RIL can only bring up or down the interface.
         */
        RIL_LOG_INFO("CChannel_Data::RemoveInterface() : Bring down hsi network interface\r\n");

        // Open socket for ifconfig and setFlags commands
        s = socket(AF_INET, SOCK_DGRAM, 0);
        if (s < 0)
        {
            RIL_LOG_CRITICAL("CChannel_Data::RemoveInterface() : cannot open control socket "
                    "(error %d / '%s')\n", errno, strerror(errno));
            goto Error;
        }

        struct ifreq ifr;
        memset(&ifr, 0, sizeof(struct ifreq));
        strncpy(ifr.ifr_name, szNetworkInterfaceName, IFNAMSIZ-1);
        ifr.ifr_name[IFNAMSIZ-1] = '\0';

        // set ipv4 address to 0.0.0.0 to unset ipv4 address,
        // ipv6 addresses are automatically cleared
        if (!setaddr(s, &ifr, "0.0.0.0"))
        {
            RIL_LOG_CRITICAL("CChannel_Data::RemoveInterface() : Error removing addr\r\n");
        }

        if (!setflags(s, &ifr, 0, IFF_UP))
        {
            RIL_LOG_CRITICAL("CChannel_Data::RemoveInterface() : Error setting flags\r\n");
        }

        if (bKeepInterfaceUp)
        {
            /* Need to put the interface back up to flush out packets that might still be received
             * at modem side.
             *
             * Previous 'down' is needed to remove all 'manual' routes that might still use this
             * interface.
             */
            if (!setflags(s, &ifr, IFF_UP, 0))
            {
                RIL_LOG_CRITICAL("CChannel_Data::RemoveInterface() : Error setting flags\r\n");
            }
        }

        if (close(s) < 0)
        {
            RIL_LOG_CRITICAL("CChannel_Data::RemoveInterface() : could not close control socket "
                    "(error %d / '%s')\n", errno, strerror(errno));
        }
    }

    RIL_LOG_INFO("[RIL STATE] PDP CONTEXT DEACTIVATION chnl=%u\r\n", uiRilChannel);

Error:
    if (!bIsHSIDirect)
    {
        if (E_DATA_STATE_IDLE != state
                && E_DATA_STATE_INITING != state
                && E_DATA_STATE_ACTIVATING != state)
        {
            // Flush buffers and Unblock read thread.
            // Security in order to avoid IP data in response buffer.
            // Will unblock Channel read thread and TTY.
            // Unblock read thread whatever the result is to avoid forever block
            FlushAndUnblockChannel(UNBLOCK_CHANNEL_UNBLOCK_ALL,
                    FLUSH_CHANNEL_FLUSH_ALL);
        }
    }
}

