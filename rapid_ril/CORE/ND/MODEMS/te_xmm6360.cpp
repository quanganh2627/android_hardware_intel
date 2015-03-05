////////////////////////////////////////////////////////////////////////////
// te_xmm6360.cpp
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Overlay for the IMC 6360 modem
//
/////////////////////////////////////////////////////////////////////////////

#include <wchar.h>
#include <cutils/properties.h>

//  This is for socket-related calls.
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <linux/gsmmux.h>

#include "types.h"
#include "nd_structs.h"
#include "util.h"
#include "extract.h"
#include "rillog.h"
#include "te.h"
#include "sync_ops.h"
#include "command.h"
#include "te_xmm6360.h"
#include "rildmain.h"
#include "callbacks.h"
#include "oemhookids.h"
#include "repository.h"
#include "reset.h"
#include "data_util.h"
#include "init6360.h"


CTE_XMM6360::CTE_XMM6360(CTE& cte)
: CTE_XMM6260(cte)
{
}

CTE_XMM6360::~CTE_XMM6360()
{
}

CInitializer* CTE_XMM6360::GetInitializer()
{
    RIL_LOG_VERBOSE("CTE_XMM6360::GetInitializer() - Enter\r\n");
    CInitializer* pRet = NULL;

    RIL_LOG_INFO("CTE_XMM6360::GetInitializer() - Creating CInit6360 initializer\r\n");
    m_pInitializer = new CInit6360();
    if (NULL == m_pInitializer)
    {
        RIL_LOG_CRITICAL("CTE_XMM6360::GetInitializer() - Failed to create a CInit6360 "
                "initializer!\r\n");
        goto Error;
    }

    pRet = m_pInitializer;

Error:
    RIL_LOG_VERBOSE("CTE_XMM6360::GetInitializer() - Exit\r\n");
    return pRet;
}

char* CTE_XMM6360::GetBasicInitCommands(UINT32 uiChannelType)
{
    RIL_LOG_VERBOSE("CTE_XMM6360::GetBasicInitCommands() - Enter\r\n");

    char szInitCmd[MAX_BUFFER_SIZE] = {'\0'};
    char* pInitCmd = NULL;

    pInitCmd = CTE_XMM6260::GetBasicInitCommands(uiChannelType);
    if (NULL == pInitCmd)
    {
        RIL_LOG_CRITICAL("CTE_XMM6360::GetBasicInitCommands() - Failed to get the "
                "basic init cmd string!\r\n");
        goto Done;
    }

    // Add any XMM6360-specific init strings
    //
    if (RIL_CHANNEL_ATCMD == uiChannelType)
    {
        if (m_cte.IsSupportCGPIAF())
        {
            // Set IPV6 address format: Use IPv6 colon-notation, subnet prefix CIDR notation,
            //  leading zeros omitted, zero compression
            CopyStringNullTerminate(szInitCmd, "|+CGPIAF=1,1,0,1", sizeof(szInitCmd));
        }
    }
    else if (RIL_CHANNEL_URC == uiChannelType)
    {
        CopyStringNullTerminate(szInitCmd, pInitCmd, sizeof(szInitCmd));
    }

Done:
    free(pInitCmd);
    RIL_LOG_VERBOSE("CTE_XMM6360::GetBasicInitCommands() - Exit\r\n");
    return strndup(szInitCmd, strlen(szInitCmd));
}

char* CTE_XMM6360::GetUnlockInitCommands(UINT32 uiChannelType)
{
    RIL_LOG_VERBOSE("CTE_XMM6360::GetUnlockInitCommands() - Enter\r\n");

    char szInitCmd[MAX_BUFFER_SIZE] = {'\0'};

    if (RIL_CHANNEL_URC == uiChannelType)
    {
        ConcatenateStringNullTerminate(szInitCmd, sizeof(szInitCmd), "|+CGAUTO=0|+CRC=1");
    }

    RIL_LOG_VERBOSE("CTE_XMM6360::GetUnlockInitCommands() - Exit\r\n");
    return strndup(szInitCmd, strlen(szInitCmd));
}

const char* CTE_XMM6360::GetRegistrationInitString()
{
    return "+CREG=3|+XREG=3";
}

const char* CTE_XMM6360::GetCsRegistrationReadString()
{
    return "AT+CREG=3;+CREG?;+CREG=0\r";
}

const char* CTE_XMM6360::GetPsRegistrationReadString()
{
    return "AT+XREG=3;+XREG?;+XREG=0\r";
}

const char* CTE_XMM6360::GetLocationUpdateString(BOOL bIsLocationUpdateEnabled)
{
    return bIsLocationUpdateEnabled ? "AT+CREG=3\r" : "AT+CREG=1\r";
}

const char* CTE_XMM6360::GetScreenOnString()
{
    return m_cte.IsSignalStrengthReportEnabled()
            ? "AT+CREG=3;+CGREG=0;+XREG=3;+XCSQ=1\r" : "AT+CREG=3;+CGREG=0;+XREG=3\r";
}

// RIL_REQUEST_SETUP_DATA_CALL 27
RIL_RESULT_CODE CTE_XMM6360::CoreSetupDataCall(REQUEST_DATA& rReqData,
       void* pData, UINT32 uiDataSize, UINT32& uiCID)
{
    RIL_LOG_VERBOSE("CTE_XMM6360::CoreSetupDataCall() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int nPapChap = 0; // no auth
    PdpData stPdpData;
    S_SETUP_DATA_CALL_CONTEXT_DATA* pDataCallContextData = NULL;
    CChannel_Data* pChannelData = NULL;
    int dataProfile = -1;
    int nEmergencyFlag = 0 ; // 1: emergency pdn
    UINT32 uiDnsMode = 0;

    RIL_LOG_INFO("CTE_XMM6360::CoreSetupDataCall() - uiDataSize=[%u]\r\n", uiDataSize);

    memset(&stPdpData, 0, sizeof(PdpData));

    // extract data
    stPdpData.szRadioTechnology = ((char**)pData)[0];  // not used
    stPdpData.szRILDataProfile  = ((char**)pData)[1];
    stPdpData.szApn             = ((char**)pData)[2];
    stPdpData.szUserName        = ((char**)pData)[3];
    stPdpData.szPassword        = ((char**)pData)[4];
    stPdpData.szPAPCHAP         = ((char**)pData)[5];

    pDataCallContextData =
            (S_SETUP_DATA_CALL_CONTEXT_DATA*)malloc(sizeof(S_SETUP_DATA_CALL_CONTEXT_DATA));
    if (NULL == pDataCallContextData)
    {
        goto Error;
    }

    dataProfile = atoi(stPdpData.szRILDataProfile);
    pChannelData = CChannel_Data::GetFreeChnlsRilHsi(uiCID, dataProfile);
    if (NULL == pChannelData)
    {
        RIL_LOG_CRITICAL("CTE_XMM6360::CoreSetupDataCall() - "
                "****** No free data channels available ******\r\n");
        goto Error;
    }

    pDataCallContextData->uiCID = uiCID;

    RIL_LOG_INFO("CTE_XMM6360::CoreSetupDataCall() - stPdpData.szRadioTechnology=[%s]\r\n",
            stPdpData.szRadioTechnology);
    RIL_LOG_INFO("CTE_XMM6360::CoreSetupDataCall() - stPdpData.szRILDataProfile=[%s]\r\n",
            stPdpData.szRILDataProfile);
    RIL_LOG_INFO("CTE_XMM6360::CoreSetupDataCall() - stPdpData.szApn=[%s]\r\n", stPdpData.szApn);
    RIL_LOG_INFO("CTE_XMM6360::CoreSetupDataCall() - stPdpData.szUserName=[%s]\r\n",
            stPdpData.szUserName);
    RIL_LOG_INFO("CTE_XMM6360::CoreSetupDataCall() - stPdpData.szPassword=[%s]\r\n",
            stPdpData.szPassword);
    RIL_LOG_INFO("CTE_XMM6360::CoreSetupDataCall() - stPdpData.szPAPCHAP=[%s]\r\n",
            stPdpData.szPAPCHAP);

    // if user name is empty, always use no authentication
    if (stPdpData.szUserName == NULL || strlen(stPdpData.szUserName) == 0)
    {
        nPapChap = 0;    // No authentication
    }
    else
    {
        // PAP/CHAP auth type 3 (PAP or CHAP) is not supported. In this case if a
        // a username is defined we will use PAP for authentication.
        // Note: due to an issue in the Android/Fw (missing check of the username
        // length), if the authentication is not defined, it's the value 3 (PAP or
        // CHAP) which is sent to RRIL by default.
        nPapChap = atoi(stPdpData.szPAPCHAP);
        if (nPapChap == 3)
        {
            nPapChap = 1;    // PAP authentication

            RIL_LOG_INFO("CTE_XMM6360::CoreSetupDataCall() - New PAP/CHAP=[%d]\r\n", nPapChap);
        }
    }

    if (RIL_VERSION >= 4 && (uiDataSize >= (7 * sizeof(char*))))
    {
        stPdpData.szPDPType = ((char**)pData)[6]; // new in Android 2.3.4.
        RIL_LOG_INFO("CTE_XMM6360::CoreSetupDataCall() - stPdpData.szPDPType=[%s]\r\n",
                stPdpData.szPDPType);
    }

    if (dataProfile == RIL_DATA_PROFILE_EMERGENCY)
    {
        nEmergencyFlag = 1;
    }

    //
    //  IP type is passed in dynamically.
    if (NULL == stPdpData.szPDPType)
    {
        //  hard-code "IPV4V6" (this is the default)
        CopyStringNullTerminate(stPdpData.szPDPType, PDPTYPE_IPV4V6, sizeof(stPdpData.szPDPType));
    }

    //  dynamic PDP type, need to set XDNS parameter depending on szPDPType.
    //  If not recognized, just use IPV4V6 as default.
    uiDnsMode = GetXDNSMode(stPdpData.szPDPType);
    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1),
            "AT+CGDCONT=%d,\"%s\",\"%s\",,0,0,,%d;+XGAUTH=%d,%u,\"%s\",\"%s\";+XDNS=%d,%u\r",
            uiCID, stPdpData.szPDPType, stPdpData.szApn, nEmergencyFlag, uiCID, nPapChap,
            stPdpData.szUserName, stPdpData.szPassword, uiCID, uiDnsMode))
    {
        RIL_LOG_CRITICAL("CTE_XMM6360::CoreSetupDataCall() -"
                " cannot create CGDCONT command, stPdpData.szPDPType\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pDataCallContextData);
    }
    else
    {
        pChannelData->SetDataState(E_DATA_STATE_INITING);

        rReqData.pContextData = (void*)pDataCallContextData;
        rReqData.cbContextData = sizeof(S_SETUP_DATA_CALL_CONTEXT_DATA);
    }

    RIL_LOG_VERBOSE("CTE_XMM6360::CoreSetupDataCall() - Exit\r\n");
    return res;
}

BOOL CTE_XMM6360::PdpContextActivate(REQUEST_DATA& rReqData, void* pData,
                                                            UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_XMM6360::PdpContextActivate() - Enter\r\n");

    BOOL bRet = FALSE;
    UINT32 uiCID = 0;
    int muxControlChannel = -1;
    int muxDataChannel = -1;
    S_SETUP_DATA_CALL_CONTEXT_DATA* pDataCallContextData = NULL;
    CChannel_Data* pChannelData = NULL;
    BOOL bIsHSIDirect = FALSE;
    int hsiChannel = -1;
    UINT32 uiRilChannel = 0;
    int ipcDataChannelMin = 0;
    char* szModemResourceName = {'\0'};

    if (NULL == pData ||
                    sizeof(S_SETUP_DATA_CALL_CONTEXT_DATA) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTE_XMM6360::PdpContextActivate() - Invalid input data\r\n");
        goto Error;
    }

    pDataCallContextData = (S_SETUP_DATA_CALL_CONTEXT_DATA*)pData;
    uiCID = pDataCallContextData->uiCID;

    // Get Channel Data according to CID
    pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
    if (NULL == pChannelData)
    {
        RIL_LOG_CRITICAL("CTE_XMM6360::PdpContextActivate() -"
                " No Data Channel found for CID %u.\r\n",
                uiCID);
        goto Error;
    }

    bIsHSIDirect = pChannelData->IsHSIDirect();
    hsiChannel =  pChannelData->GetHSIChannel();
    uiRilChannel = pChannelData->GetRilChannel();

    muxControlChannel = pChannelData->GetMuxControlChannel();
    if (-1 == muxControlChannel)
    {
        RIL_LOG_CRITICAL("CTE_XMM6360::PdpContextActivate() - Unknown mux channel\r\n");
        goto Error;
    }

    if (!bIsHSIDirect)
    {
        muxDataChannel = muxControlChannel;

        if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1),
                "AT+CGACT=1,%d;+XDATACHANNEL=1,1,\"/mux/%d\",\"/mux/%d\",0\r",
                uiCID, muxControlChannel, muxDataChannel))
        {
            RIL_LOG_CRITICAL("CTE_XMM6360::PdpContextActivate() -"
                    "  cannot create CGACT command\r\n");
            goto Error;
        }
    }
    else
    {
        if (hsiChannel < 0)
        {
            RIL_LOG_CRITICAL("CTE_XMM6360::PdpContextActivate() - No free HSI Channel \r\n");
            goto Error;
        }

       // Get the hsi channel id
        int hsiNetworkPath = -1;

        ipcDataChannelMin = pChannelData->GetIpcDataChannelMin();

        if (ipcDataChannelMin <= hsiChannel && RIL_MAX_NUM_IPC_CHANNEL > hsiChannel)
        {
           hsiNetworkPath = hsiChannel;
        }
        else
        {
           RIL_LOG_CRITICAL("CTE_XMM6360::PdpContextActivate() - Unknown HSI Channel [%d] \r\n",
                            hsiChannel);
           goto Error;
        }

        szModemResourceName = pChannelData->GetModemResourceName();

        if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1),
                "AT+CGACT=1,%d;+XDATACHANNEL=1,1,\"/mux/%d\",\"/%s/%d\",0\r",
                uiCID, muxControlChannel,
                szModemResourceName, hsiNetworkPath))
        {
            RIL_LOG_CRITICAL("CTE_XMM6360::PdpContextActivate() -"
                    "  cannot create CGACT command\r\n");
            goto Error;
        }
    }

    if (!CopyStringNullTerminate(rReqData.szCmd2, "AT+CEER\r",
                                                    sizeof(rReqData.szCmd2)))
    {
        RIL_LOG_CRITICAL("CTE_XMM6360::PdpContextActivate() - Cannot create CEER command\r\n");
    }

    bRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CTE_XMM6360::PdpContextActivate() - Exit\r\n");
    return bRet;
}

RIL_RESULT_CODE CTE_XMM6360::ParseEnterDataState(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_XMM6360::ParseEnterDataState() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char* pszRsp = rRspData.szResponse;
    UINT32 uiCause;
    BOOL bIsHSIDirect = FALSE;
    CChannel_Data* pChannelData = NULL;
    S_SETUP_DATA_CALL_CONTEXT_DATA* pDataCallContextData = NULL;
    UINT32 uiCID = 0;

    if (NULL == rRspData.pContextData ||
            sizeof(S_SETUP_DATA_CALL_CONTEXT_DATA) != rRspData.cbContextData)
    {
        RIL_LOG_INFO("CTE_XMM6360::ParseEnterDataState() - Invalid context data\r\n");
        goto Error;
    }

    pDataCallContextData =
                    (S_SETUP_DATA_CALL_CONTEXT_DATA*)rRspData.pContextData;
    uiCID = pDataCallContextData->uiCID;

    pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
    if (NULL == pChannelData)
    {
        RIL_LOG_INFO("CTE_XMM6360::ParseEnterDataState() - No Data Channel for CID %u.\r\n",
                                                                    uiCID);
        goto Error;
    }

    if (ParseCEER(rRspData, uiCause))
    {
        RIL_LOG_INFO("CTE_XMM6360::ParseEnterDataState() - uiCause: %u\r\n",
                                                                    uiCause);
        int failCause = PDP_FAIL_ERROR_UNSPECIFIED;

        failCause = MapErrorCodeToRilDataFailCause(uiCause);
        pChannelData->SetDataFailCause(failCause);
        goto Error;
    }

    bIsHSIDirect = pChannelData->IsHSIDirect();

    if (!bIsHSIDirect)
    {
        // Block the read thread and then flush the tty and the channel
        // From now, any failure will lead to DataConfigDown
        pChannelData->BlockAndFlushChannel(BLOCK_CHANNEL_BLOCK_ALL, FLUSH_CHANNEL_NO_FLUSH);
        pChannelData->FlushAndUnblockChannel(UNBLOCK_CHANNEL_UNBLOCK_TTY, FLUSH_CHANNEL_FLUSH_ALL);
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTE_XMM6360::ParseEnterDataState() - Exit\r\n");
    return res;
}

BOOL CTE_XMM6360::SetupInterface(UINT32 uiCID)
{
    RIL_LOG_VERBOSE("CTE_XMM6360::SetupInterface() - Enter\r\n");

    BOOL bRet = FALSE;

    char szNetworkInterfaceName[IFNAMSIZ];
    struct gsm_netconfig netconfig;
    int fd = -1;
    int ret = 0;
    UINT32 uiChannel = 0;
    int state = 0;
    int networkInterfaceID = -1;
    CChannel_Data* pChannelData = NULL;
    PDP_TYPE eDataConnectionType = PDP_TYPE_IPV4;  //  dummy for now, set to IPv4.
    char szPdpType[MAX_BUFFER_SIZE] = {'\0'};
    char szIpAddr[MAX_IPADDR_SIZE] = {'\0'};
    char szIpAddr2[MAX_IPADDR_SIZE] = {'\0'};
    int dataProfile = -1;
    BOOL bIsHSIDirect = FALSE;
    int hsiChannel = -1;
    int ipcDataChannelMin = 0;
    UINT32 nw_if_pdp_mux_offset = 0;

    pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
    if (NULL == pChannelData)
    {
        RIL_LOG_CRITICAL("CTE_XMM6360::SetupInterface() - No Data Channel for CID %u.\r\n",
                                                                    uiCID);
        goto Error;
    }

    ipcDataChannelMin = pChannelData->GetIpcDataChannelMin();

    if (ipcDataChannelMin > RIL_MAX_NUM_IPC_CHANNEL)
    {
        RIL_LOG_CRITICAL("CTE_XMM6360::SetupInterface() - Invalid data channel range (%u).\r\n",
                                                                    ipcDataChannelMin);
        goto Error;
    }

    nw_if_pdp_mux_offset = (RIL_MAX_NUM_IPC_CHANNEL - ipcDataChannelMin);

    state = pChannelData->GetDataState();
    if (E_DATA_STATE_ACTIVE != state)
    {
        RIL_LOG_CRITICAL("CTE_XMM6360::SetupInterface() - Invalid data state %d.\r\n",
                                                                    state);
        goto Error;
    }

    bIsHSIDirect = pChannelData->IsHSIDirect();
    dataProfile =  pChannelData->GetDataProfile();
    hsiChannel =  pChannelData->GetHSIChannel();

    if (!bIsHSIDirect){
        // First network interface is rmnet3 for pdp over mux
        switch (dataProfile)
        {
        case RIL_DATA_PROFILE_DEFAULT:
            networkInterfaceID = nw_if_pdp_mux_offset + RIL_DATA_PROFILE_DEFAULT;
            break;
        case RIL_DATA_PROFILE_TETHERED:
            networkInterfaceID = nw_if_pdp_mux_offset + RIL_DATA_PROFILE_TETHERED;
            break;
        case RIL_DATA_PROFILE_IMS:
            networkInterfaceID = nw_if_pdp_mux_offset + RIL_DATA_PROFILE_IMS;
            break;
        case RIL_DATA_PROFILE_MMS:
            networkInterfaceID = nw_if_pdp_mux_offset + RIL_DATA_PROFILE_MMS;
            break;
        case RIL_DATA_PROFILE_CBS:
            networkInterfaceID = nw_if_pdp_mux_offset + RIL_DATA_PROFILE_CBS;
            break;
        case RIL_DATA_PROFILE_FOTA:
            networkInterfaceID = nw_if_pdp_mux_offset + RIL_DATA_PROFILE_FOTA;
            break;
        case RIL_DATA_PROFILE_SUPL:
            networkInterfaceID = nw_if_pdp_mux_offset + RIL_DATA_PROFILE_SUPL;
            break;
        case RIL_DATA_PROFILE_HIPRI:
            networkInterfaceID = nw_if_pdp_mux_offset + RIL_DATA_PROFILE_HIPRI;
            break;
        case RIL_DATA_PROFILE_EMERGENCY:
            networkInterfaceID = nw_if_pdp_mux_offset + RIL_DATA_PROFILE_EMERGENCY;
            break;
        default:
            RIL_LOG_CRITICAL("CTE_XMM6360::SetupInterface() - Unknown Data Profile [%d] \r\n",
                                                                dataProfile);
            goto Error;
        }
    }
    else
    {
        if (ipcDataChannelMin <= hsiChannel && RIL_MAX_NUM_IPC_CHANNEL > hsiChannel)
        {
            networkInterfaceID = (hsiChannel - ipcDataChannelMin);
            if (networkInterfaceID < 0)
            {
                RIL_LOG_CRITICAL("CTE_XMM6360::SetupInterface() - Invalid network"
                        " interface ID (%d) \r\n", networkInterfaceID);
                goto Error;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE_XMM6360::SetupInterface() - Unknown his channel [%d] \r\n",
                                                    hsiChannel);
            goto Error;
        }
    }

    if (!PrintStringNullTerminate(szNetworkInterfaceName, sizeof(szNetworkInterfaceName),
                                        "%s%d", m_szNetworkInterfaceNamePrefix,
                                                            networkInterfaceID))
    {
        RIL_LOG_CRITICAL("CTE_XMM6360::SetupInterface() - Cannot set network interface name\r\n");
        goto Error;
    }
    else
    {
        RIL_LOG_INFO("CTE_XMM6360::SetupInterface() - szNetworkInterfaceName=[%s], CID=[%u]\r\n",
                                                szNetworkInterfaceName, uiCID);
    }

    pChannelData->SetInterfaceName(szNetworkInterfaceName);

    if (!bIsHSIDirect)
    {
        UINT32 uiChannel = pChannelData->GetRilChannel();

        // N_GSM related code
        netconfig.adaption = 3;
        netconfig.protocol = htons(ETH_P_IP);
        strncpy(netconfig.if_name, szNetworkInterfaceName, IFNAMSIZ - 1);
        netconfig.if_name[IFNAMSIZ - 1] = '\0';

        // Add IF NAME
        fd = pChannelData->GetFD();
        if (fd >= 0)
        {
            RIL_LOG_INFO("CTE_XMM6360::SetupInterface() -"
                    " ***** PUTTING channel=[%u] in DATA MODE *****\r\n", uiChannel);
            ret = ioctl( fd, GSMIOC_ENABLE_NET, &netconfig );       // Enable data channel
            if (ret < 0)
            {
                RIL_LOG_CRITICAL("CTE_XMM6360::SetupInterface() -"
                        " Unable to create interface %s : %s \r\n",
                        netconfig.if_name, strerror(errno));
                goto Error;
            }
        }
        else
        {
            //  No FD.
            RIL_LOG_CRITICAL("CTE_XMM6360::SetupInterface() -"
                    " Could not get Data Channel chnl=[%u] fd=[%d].\r\n", uiChannel, fd);
            goto Error;
        }
    }

    pChannelData->GetIpAddress(szIpAddr, sizeof(szIpAddr),
                                    szIpAddr2, sizeof(szIpAddr2));

    if (szIpAddr2[0] == '\0')
    {
        eDataConnectionType = PDP_TYPE_IPV4;
        strcpy(szPdpType, PDPTYPE_IP);
    }
    else if (szIpAddr[0] == '\0')
    {
        eDataConnectionType = PDP_TYPE_IPV6;
        strcpy(szPdpType, PDPTYPE_IPV6);
    }
    else
    {
        eDataConnectionType = PDP_TYPE_IPV4V6;
        strcpy(szPdpType, PDPTYPE_IPV4V6);
    }

    pChannelData->SetPdpType(szPdpType);

    // set interface address(es) and bring up interface
    if (!DataConfigUp(szNetworkInterfaceName, pChannelData, eDataConnectionType))
    {
        RIL_LOG_CRITICAL("CTE_XMM6360::SetupInterface() -"
                " Unable to bringup interface ifconfig\r\n");
        goto Error;
    }

    bRet = TRUE;

Error:
    RIL_LOG_VERBOSE("CTE_XMM6360::SetupInterface() Exit\r\n");
    return bRet;
}

//
// RIL_REQUEST_BASEBAND_VERSION 51
//
RIL_RESULT_CODE CTE_XMM6360::CoreBasebandVersion(REQUEST_DATA& rReqData,
        void* pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_XMM6360::CoreBasebandVersion() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "at+xgendata\r",
            sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CTE_XMM6360::CoreBasebandVersion() - Exit\r\n");
    return res;
}

#define STRINGIFY(x) #x
#define TO_STRING(x) STRINGIFY(x)

RIL_RESULT_CODE CTE_XMM6360::ParseBasebandVersion(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_XMM6360::ParseBasebandVersion() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char* pszRsp = rRspData.szResponse;
    /* As baseband version is stored in a system property, it cannot be longer than
     * PROPERTY_VALUE_MAX-1 characters long (PROPERTY_VALUE_MAX counts the zero termination).
     *
     * + 1 is needed here as 'sscanf' will read PROPERTY_VALUE_MAX characters AND will add the
     * zero termination (cf comment above sscanf code).
     */
    char* pszBasebandVersion = (char*) malloc(PROPERTY_VALUE_MAX + 1);
    if (NULL == pszBasebandVersion)
    {
        RIL_LOG_CRITICAL("CTE_XMM6360::ParseBasebandVersion() - Could not allocate memory"
                "for pszBasebandVersion\r\n");
        goto Error;
    }

    memset(pszBasebandVersion, 0, PROPERTY_VALUE_MAX + 1);

    /* Modem version is what is reported between '*' in the +XGENDATA reply:
     *    +XGENDATA: "    XMM6360_REV_2.0 2013-Jul-31 13:42:17
     *    *CLV_6360_MODEM_01.1332.A*
     *    OK
     *
     * This is retrieved using 'sscanf':
     *   = %*[^*]  : consumes (without storing) everything that is not '*'
     *   = %*c     : consumes (without storing) the '*' character
     *   = %XXX[^*]: stores up to XXX chars or up to next '*' character in pszBasebandVersion
     *               XXX's numerical value is constructed by converting the PROPERTY_VALUE_MAX
     *               define to a string using the TO_STRING macro
     */
    if (!sscanf(pszRsp, "%*[^*]%*c%" TO_STRING(PROPERTY_VALUE_MAX) "[^*]", pszBasebandVersion))
    {
        RIL_LOG_CRITICAL("CTE_XMM6360::ParseBasebandVersion() - Could not "
                "extract the baseband version string.\r\n");
        goto Error;
    }
    if (pszBasebandVersion[PROPERTY_VALUE_MAX - 1] != '\0')
    {
        RIL_LOG_WARNING("CTE_XMM6360::ParseBasebandVersion() - "
                "Modem version too long, reporting truncated version.\r\n");
        pszBasebandVersion[PROPERTY_VALUE_MAX - 1] = '\0';
    }

    if (pszBasebandVersion[0] == '\0')
    {
        RIL_LOG_CRITICAL("CTE_XMM6360::ParseBasebandVersion() - "
                "Invalid baseband version string.\r\n");
        goto Error;
    }

    RIL_LOG_INFO("CTE_XMM6360::ParseBasebandVersion() - "
            "pszBasebandVersion=[%s]\r\n", pszBasebandVersion);

    rRspData.pData   = (void*)pszBasebandVersion;
    rRspData.uiDataSize  = sizeof(char*);

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pszBasebandVersion);
        pszBasebandVersion = NULL;
    }

    RIL_LOG_VERBOSE("CTE_XMM6360::ParseBasebandVersion() - Exit\r\n");
    return res;
}

RIL_RadioTechnology CTE_XMM6360::MapAccessTechnology(UINT32 uiStdAct)
{
    RIL_LOG_VERBOSE("CTE_XMM6360::MapAccessTechnology() ENTER  uiStdAct=[%u]\r\n", uiStdAct);

    /*
     * 20111103: There is no 3GPP standard value defined for GPRS and HSPA+
     * access technology. So, values 1 and 8 are used in relation with the
     * IMC proprietary +XREG: <Act> parameter.
     *
     * Note: GSM Compact is not supported by IMC modem.
     */
    RIL_RadioTechnology rtAct = RADIO_TECH_UNKNOWN;

    //  Check state and set global variable for network technology
    switch (uiStdAct)
    {
        /* 20130202:
         * case 9 is added for HSPA dual carrier
         */
        case 9: // Proprietary value introduced for HSPA+ DC-FSPA+
        rtAct = RADIO_TECH_HSPAP; // 15
        break;

        default:
        rtAct = CTEBase::MapAccessTechnology(uiStdAct);
        break;
    }
    RIL_LOG_VERBOSE("CTE_XMM6360::MapAccessTechnology() EXIT  rtAct=[%u]\r\n", (UINT32)rtAct);
    return rtAct;
}

//
// Response to AT+CGPADDR=<CID>
//
RIL_RESULT_CODE CTE_XMM6360::ParseIpAddress(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_XMM6360::ParseIpAddress() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char* szRsp = rRspData.szResponse;
    UINT32 uiCID;

    // Parse prefix
    if (!FindAndSkipString(szRsp, "+CGPADDR: ", szRsp))
    {
        RIL_LOG_CRITICAL("CTE_XMM6360::ParseIpAddress() -"
                " Unable to parse \"+CGPADDR\" prefix.!\r\n");
        goto Error;
    }

    // Parse <cid>
    if (!ExtractUInt32(szRsp, uiCID, szRsp))
    {
        RIL_LOG_CRITICAL("CTE_XMM6360::ParseIpAddress() - Unable to parse <cid>!\r\n");
        goto Error;
    }

    if (uiCID > 0)
    {
        //  The response could come back as:
        //      +CGPADDR: <cid>,<PDP_Addr1>,<PDP_Addr2>
        //  PDP_Addr1 could be in IPv4, or IPv6.  PDP_Addr2 is present only for IPv4v6
        //  in which case PDP_Addr1 is IPv4 and PDP_Addr2 is IPv6.
        //
        //  When +CGPIAF is set (CGPIAF=1,1,0,1), the returned address format
        //  is the expected format by Android.
        //    IPV4: a1.a2.a3.a4
        //    IPV6: a1a2:a3a4:a5a6:a7a8:a9a10:a11a12:a13a14:a15a16

        char szPdpAddr[MAX_IPADDR_SIZE] = {'\0'};
        char szIpAddr1[MAX_IPADDR_SIZE] = {'\0'}; // IPV4
        char szIpAddr2[MAX_IPADDR_SIZE] = {'\0'}; // IPV6
        int state = E_DATA_STATE_IDLE;

        CChannel_Data* pChannelData =
                CChannel_Data::GetChnlFromContextID(uiCID);
        if (NULL == pChannelData)
        {
            RIL_LOG_CRITICAL("CTE_XMM6360::ParseIpAddress() - No Data Channel for CID %u.\r\n",
                    uiCID);
            goto Error;
        }

        state = pChannelData->GetDataState();
        if (E_DATA_STATE_ACTIVE != state)
        {
            RIL_LOG_CRITICAL("CTE_XMM6360::ParseIpAddress() - Wrong data state: %d\r\n",
                    state);
            goto Error;
        }

        // Allow reverting to old implementation for IMC IPV6 AT commands when using CGPIAF
        if (m_cte.IsSupportCGPIAF())
        {
            //  Extract ,<Pdp_Addr1>
            if (!SkipString(szRsp, ",", szRsp) ||
                    !ExtractQuotedString(szRsp, szIpAddr1, MAX_IPADDR_SIZE, szRsp))
            {
                RIL_LOG_CRITICAL("CTE_XMM6360::ParseIpAddress() -"
                        " Unable to parse <PDP_addr1>!\r\n");
                goto Error;
            }

            //  Extract ,<PDP_Addr2>
            if (SkipString(szRsp, ",", szRsp))
            {
                if (!ExtractQuotedString(szRsp, szIpAddr2, MAX_IPADDR_SIZE, szRsp))
                {
                    RIL_LOG_CRITICAL("CTE_XMM6360::ParseIpAddress() -"
                            " Unable to parse <PDP_addr2>!\r\n");
                    goto Error;
                }

                RIL_LOG_INFO("CTE_XMM6360::ParseIpAddress() - IPV4 address: %s\r\n", szIpAddr1);
                RIL_LOG_INFO("CTE_XMM6360::ParseIpAddress() - IPV6 address: %s\r\n", szIpAddr2);
            }
        }
        else
        {
            //  Extract ,<Pdp_Addr1>
            if (!SkipString(szRsp, ",", szRsp) ||
                !ExtractQuotedString(szRsp, szPdpAddr, MAX_IPADDR_SIZE, szRsp))
            {
                RIL_LOG_CRITICAL("CTE_XMM6360::ParseIpAddress() - "
                        "Unable to parse <PDP_addr1>!\r\n");
                goto Error;
            }

            //  The AT+CGPADDR command doesn't return IPV4V6 format
            if (!ConvertIPAddressToAndroidReadable(szPdpAddr, szIpAddr1, MAX_IPADDR_SIZE,
                    szIpAddr2, MAX_IPADDR_SIZE))
            {
                RIL_LOG_CRITICAL("CTE_XMM6360::ParseIpAddress() -"
                        " ConvertIPAddressToAndroidReadable failed!\r\n");
                goto Error;
            }

            //  Extract ,<PDP_Addr2>
            //  Converted address is in szIpAddr2.
            if (SkipString(szRsp, ",", szRsp))
            {
                if (!ExtractQuotedString(szRsp, szPdpAddr, MAX_IPADDR_SIZE, szRsp))
                {
                    RIL_LOG_CRITICAL("CTE_XMM6360::ParseIpAddress() -"
                            " Unable to parse <PDP_addr2>!\r\n");
                    goto Error;
                }

                //  The AT+CGPADDR command doesn't return IPV4V6 format.
                if (!ConvertIPAddressToAndroidReadable(szPdpAddr, szIpAddr1, MAX_IPADDR_SIZE,
                        szIpAddr2, MAX_IPADDR_SIZE))
                {
                    RIL_LOG_CRITICAL("CTE_XMM6360::ParseIpAddress() -"
                            " ConvertIPAddressToAndroidReadable failed! m_szIpAddr2\r\n");
                    goto Error;
                }

                //  Extract ,<PDP_Addr2>
                //  Converted address is in pChannelData->m_szIpAddr2.
                if (SkipString(szRsp, ",", szRsp))
                {
                    if (!ExtractQuotedString(szRsp, szPdpAddr, MAX_IPADDR_SIZE, szRsp))
                    {
                        RIL_LOG_CRITICAL("CTE_XMM6360::ParseIpAddress() -"
                                " Unable to parse <PDP_addr2>!\r\n");
                        goto Error;
                    }

                    //  The AT+CGPADDR command doesn't return IPV4V6 format
                    //  Use a dummy IPV4 address as only an IPV6 address is expected here
                    char szDummyIpAddr[MAX_IPADDR_SIZE];
                    szDummyIpAddr[0] = '\0';

                    if (!ConvertIPAddressToAndroidReadable(szPdpAddr,
                            szDummyIpAddr, MAX_IPADDR_SIZE, szIpAddr2, MAX_IPADDR_SIZE))
                    {
                        RIL_LOG_CRITICAL("CTE_XMM6360::ParseIpAddress() -"
                                " ConvertIPAddressToAndroidReadable failed! m_szIpAddr2\r\n");
                        goto Error;
                    }

                RIL_LOG_INFO("CTE_XMM6360::ParseIpAddress() - IPV4 address: %s\r\n", szIpAddr1);
                RIL_LOG_INFO("CTE_XMM6360::ParseIpAddress() - IPV6 address: %s\r\n", szIpAddr2);
                }
            }
        }

        pChannelData->SetIpAddress(szIpAddr1, szIpAddr2);
        res = RRIL_RESULT_OK;
    }
    else
    {
        RIL_LOG_CRITICAL("CTE_XMM6360::ParseIpAddress() - uiCID=[%u] not valid!\r\n", uiCID);
    }

Error:
    RIL_LOG_VERBOSE("CTE_XMM6360::ParseIpAddress() - Exit\r\n");
    return res;
}

//
// Response to AT+XDNS?
//
RIL_RESULT_CODE CTE_XMM6360::ParseDns(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE_XMM6360::ParseDns() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char* szRsp = rRspData.szResponse;

    // Parse "+XDNS: "
    while (FindAndSkipString(szRsp, "+XDNS: ", szRsp))
    {
        UINT32 uiCID = 0;
        char szDNS[MAX_IPADDR_SIZE] = {'\0'};
        char szIpDNS1[MAX_IPADDR_SIZE] = {'\0'};
        char szIpDNS2[MAX_IPADDR_SIZE] = {'\0'};
        char szIpV6DNS1[MAX_IPADDR_SIZE] = {'\0'};
        char szIpV6DNS2[MAX_IPADDR_SIZE] = {'\0'};
        CChannel_Data* pChannelData = NULL;
        int state = E_DATA_STATE_IDLE;

        // Parse <cid>
        if (!ExtractUInt32(szRsp, uiCID, szRsp))
        {
            RIL_LOG_CRITICAL("CTE_XMM6360::ParseDns() - Unable to parse <cid>!\r\n");
            continue;
        }

        pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
        if (NULL == pChannelData)
        {
            RIL_LOG_CRITICAL("CTE_XMM6360::ParseDns() - No Data Channel for CID %u.\r\n",
                    uiCID);
            continue;
        }

        state = pChannelData->GetDataState();
        if (E_DATA_STATE_ACTIVE != state)
        {
            RIL_LOG_CRITICAL("CTE_XMM6360::ParseDns() - Wrong data state: %d\r\n",
                    state);
            continue;
        }

        //  The response could come back as:
        //    +XDNS: <cid>,<Primary_DNS>,<Secondary_DNS>
        //  Primary_DNS and Secondary_DNS could be in IPV4, IPV6, or IPV4V6 format.
        //
        //  When +CGPIAF is set (CGPIAF=1,1,0,1), the returned address format
        //  is the expected format by Android.
        //    IPV4: a1.a2.a3.a4
        //    IPV6: a1a2:a3a4:a5a6:a7a8:a9a10:a11a12:a13a14:a15a16
        //    IPV4V6: a1a2:a3a4:a5a6:a7a8:a9a10:a11a12:a13a14:a15a16:a17a18:a19a20

        // Parse <primary DNS>
        if (SkipString(szRsp, ",", szRsp))
        {
            // Allow reverting to old implementation for IMC IPV6 AT commands when using CGPIAF
            if (m_cte.IsSupportCGPIAF())
            {
                if (!ExtractQuotedString(szRsp, szIpDNS1, MAX_IPADDR_SIZE, szRsp))
                {
                    RIL_LOG_CRITICAL("CTE_XMM6360::ParseDns() - Unable to extact szDNS 1!\r\n");
                    continue;
                }

                RIL_LOG_INFO("CTE_XMM6360::ParseDns() - Primary DNS server: %s\r\n", szIpDNS1);
            }
            else
            {
                if (!ExtractQuotedString(szRsp, szDNS, MAX_IPADDR_SIZE, szRsp))
                {
                    RIL_LOG_CRITICAL("CTE_XMM6360::ParseDns() - Unable to extact szDNS 1!\r\n");
                    continue;
                }

                //  Now convert to Android-readable format (and split IPv4v6 parts (if applicable)
                if (!ConvertIPAddressToAndroidReadable(szDNS,
                                                        szIpDNS1,
                                                        MAX_IPADDR_SIZE,
                                                        szIpV6DNS1,
                                                        MAX_IPADDR_SIZE))
                {
                    RIL_LOG_CRITICAL("CTE_XMM6360::ParseDns() -"
                            " ConvertIPAddressToAndroidReadable failed! m_szDNS1\r\n");
                    continue;
                }

                RIL_LOG_INFO("CTE_XMM6360::ParseDns() - szIpDNS1: %s\r\n", szIpDNS1);

                if (strlen(szIpV6DNS1) > 0)
                {
                    RIL_LOG_INFO("CTE_XMM6360::ParseDns() - szIpV6DNS1: %s\r\n", szIpV6DNS1);
                }
                else
                {
                    RIL_LOG_INFO("CTE_XMM6360::ParseDns() - szIpV6DNS1: <NONE>\r\n");
                }
            }
        }

        // Parse <secondary DNS>
        if (SkipString(szRsp, ",", szRsp))
        {
            if (m_cte.IsSupportCGPIAF())
            {
                if (!ExtractQuotedString(szRsp, szIpDNS2, MAX_IPADDR_SIZE, szRsp))
                {
                    RIL_LOG_CRITICAL("CTE_XMM6360::ParseDns() - Unable to extact szDNS 2!\r\n");
                    continue;
                }

                RIL_LOG_INFO("CTE_XMM6360::ParseDns() - Secondary DNS server: %s\r\n", szIpDNS2);
            }
            else
            {
                if (!ExtractQuotedString(szRsp, szDNS, MAX_IPADDR_SIZE, szRsp))
                {
                    RIL_LOG_CRITICAL("CTE_XMM6360::ParseDns() - Unable to extact szDNS 2!\r\n");
                    continue;
                }

                //  Now convert to Android-readable format (and split IPv4v6 parts (if applicable)
                if (!ConvertIPAddressToAndroidReadable(szDNS,
                                                        szIpDNS2,
                                                        MAX_IPADDR_SIZE,
                                                        szIpV6DNS2,
                                                        MAX_IPADDR_SIZE))
                {
                    RIL_LOG_CRITICAL("CTE_XMM6360::ParseDns() -"
                            " ConvertIPAddressToAndroidReadable failed! szIpDNS2\r\n");
                    continue;
                }

                RIL_LOG_INFO("CTE_XMM6360::ParseDns() - szIpDNS2: %s\r\n", szIpDNS2);

                if (strlen(szIpV6DNS2) > 0)
                {
                    RIL_LOG_INFO("CTE_XMM6360::ParseDns() - szIpV6DNS2: %s\r\n", szIpV6DNS2);
                }
                else
                {
                    RIL_LOG_INFO("CTE_XMM6360::ParseDns() - szIpV6DNS2: <NONE>\r\n");
                }
            }
        }

        if (m_cte.IsSupportCGPIAF())
        {
            // For IPV6 addresses, copy to szIpV6DNS variables and set szIpDNS ones to empty
            if (strstr(szIpDNS1, ":"))
            {
                RIL_LOG_INFO("CTE_XMM6360::ParseDns() - IPV6 DNS addresses\r\n");

                CopyStringNullTerminate(szIpV6DNS1, szIpDNS1, MAX_IPADDR_SIZE);
                szIpDNS1[0] = '\0';

                CopyStringNullTerminate(szIpV6DNS2, szIpDNS2, MAX_IPADDR_SIZE);
                szIpDNS2[0] = '\0';
            }
        }

        pChannelData->SetDNS(szIpDNS1, szIpDNS2, szIpV6DNS1, szIpV6DNS2);

        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CTE_XMM6360::ParseDns() - Exit\r\n");
    return res;
}

// RIL_REQUEST_SET_INITIAL_ATTACH_APN 111
RIL_RESULT_CODE CTE_XMM6360::CoreSetInitialAttachApn(REQUEST_DATA& rReqData,
       void* pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_XMM6360::CoreSetInitialAttachApn() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    RIL_InitialAttachApn* pTemp = NULL;
    UINT32 uiMode = 0;

    if (pData == NULL)
    {
        RIL_LOG_CRITICAL("CTE_XMM6360::CoreSetInitialAttachApn() - "
                "pData is NULL \r\n");
        goto Error;
    }

    if (sizeof(RIL_InitialAttachApn) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTE_XMM6360::CoreSetInitialAttachApn() - "
                "pData size if wrong\r\n");
        goto Error;
    }

    pTemp = (RIL_InitialAttachApn*) pData;

    CopyStringNullTerminate(m_InitialAttachApnParams.szApn,
            pTemp->apn, sizeof(m_InitialAttachApnParams.szApn));

    if (NULL == pTemp->protocol || pTemp->protocol[0] == '\0')
    {
        CopyStringNullTerminate(m_InitialAttachApnParams.szPdpType,
                PDPTYPE_IPV4V6, sizeof(m_InitialAttachApnParams.szPdpType));
    }
    else
    {
        CopyStringNullTerminate(m_InitialAttachApnParams.szPdpType,
                pTemp->protocol, sizeof(m_InitialAttachApnParams.szPdpType));
    }

    if (RIL_APPSTATE_READY != GetSimAppState())
    {
        RIL_LOG_CRITICAL("CTE_XMM6360::CoreSetInitialAttachApn() - SIM not ready\r\n");
        res = RRIL_RESULT_OK_IMMEDIATE;
        goto Error;
    }

    if (!GetSetInitialAttachApnReqData(rReqData))
    {
        RIL_LOG_CRITICAL("CTE_XMM6360::CoreSetInitialAttachApn() - "
                "GetSetInitialAttachApnReqData failed\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTE_XMM6360::CoreSetInitialAttachApn() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_XMM6360::ParseSetInitialAttachApn(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_XMM6360::ParseSetInitialAttachApn() - Enter / Exit\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_OK;
    return res;
}

BOOL CTE_XMM6360::GetSetInitialAttachApnReqData(REQUEST_DATA& rReqData)
{
    UINT32 uiMode = GetXDNSMode(m_InitialAttachApnParams.szPdpType);

    if ('\0' == m_InitialAttachApnParams.szApn[0])
    {
        if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1),
                "AT+CGDCONT=1,\"%s\";+XDNS=1,%u\r", m_InitialAttachApnParams.szPdpType, uiMode))
        {
            RIL_LOG_CRITICAL("CTE_XMM6360::GetSetInitialAttachApnReqData() - "
                    "Can't construct szCmd1.\r\n");
        }
    }
    else
    {
        if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1),
                "AT+CGDCONT=1,\"%s\",\"%s\";+XDNS=1,%u\r", m_InitialAttachApnParams.szPdpType,
                m_InitialAttachApnParams.szApn, uiMode))
        {
            RIL_LOG_CRITICAL("CTE_XMM6360::GetSetInitialAttachApnReqData() - "
                    "Can't construct szCmd1.\r\n");
        }
    }

    return TRUE;
}
