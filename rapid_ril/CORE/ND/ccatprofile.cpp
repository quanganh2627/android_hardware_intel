////////////////////////////////////////////////////////////////////////////
// ccatprofile.cpp
//
// Copyright (C) Intel 2013.
//
//
// Description:
//     CCatProfile handles the TE and MT profiles to be sent to modem for USAT facilities
//
/////////////////////////////////////////////////////////////////////////////

#include "rillog.h"
#include "bertlv_util.h"
#include "ccatprofile.h"
#include "extract.h"
#include "util.h"
#include "repository.h"

CCatProfile::ProfileItem CCatProfile::s_proactiveUICCTable[] =
{
    // FIRST BYTE
    // SECOND BYTE
    // THIRD BYTE
    { 3, DISPLAY_TEXT , 1 },                // 0000 0001
    { 3, GET_INKEY , 2 },                   // 0000 0010
    { 3, GET_INPUT , 4 },                   // 0000 0100
    { 3, MORE_TIME , 8 },                   // 0000 1000
    { 3, PLAY_TONE , 16 },                   // 0001 0000
    { 3, POLL_INTERVAL , 32 },               // 0010 0000
    { 3, POLLING_OFF , 64 },                 // 0100 0000
    { 3, REFRESH , 128 },                    // 1000 0000
    // FOURTH BYTE
    { 4, SELECT_ITEM , 1 },
    { 4, SEND_SHORT_MESSAGE , 2 },
    { 4, SEND_SS , 4 },
    { 4, SEND_USSD , 8 },
    { 4, SET_UP_CALL , 16 },
    { 4, SET_UP_MENU , 32 },
    { 4, PROVIDE_LOCAL_INFORMATION , 64 },
    { 4, PROVIDE_LOCAL_INFORMATION_NMR , 128 },
    // FIFTH BYTE
    { 5, SET_UP_EVENT_LIST , 1 },
    { 5, EVENT_MT_CALL , 2 },
    { 5, EVENT_CALL_CONNECTED , 4 },
    { 5, EVENT_CALL_DISCONNECTED , 8 },
    { 5, EVENT_LOCATION_STATUS , 16 },
    { 5, EVENT_USER_ACTIVITY , 32 },
    { 5, EVENT_IDLE_SCREEN_AVAILABLE , 64 },
    { 5, EVENT_CARD_READER_STATUS , 128 },
    // SIXTH BYTE
    { 6, EVENT_LANGUAGE_SELECTION , 0 },
    { 6, EVENT_BROWSER_TERMINATION , 0 },
    { 6, EVENT_DATA_AVAILABLE , 0 },
    { 6, EVENT_CHANNEL_STATUS , 0 },
    { 6, EVENT_ACCESS_TECH_CHANGE , 0 },
    { 6, EVENT_DISPLAY_PARAMS_CHANGED , 0 },
    { 6, EVENT_LOCAL_CONNECTION , 0 },
    { 6, EVENT_NETWORK_SEARCH_MODE_CHANGE , 0 },
    // SEVENTH BYTE
    { 7, POWER_ON_CARD , 1 },
    { 7, POWER_OFF_CARD , 2 },
    { 7, PERFORM_CARD_APDU , 4 },
    { 7, GET_READER_STATUS_STATUS , 8 },
    { 7, GET_READER_STATUS_IDENTIFIER , 16 },
    // EIGHTH BYTE
    { 8, TIMER_MANAGEMENT_START_STOP , 1 },
    { 8, TIMER_MANAGEMENT_GET_CURRENT , 2 },
    { 8, PROVIDE_LOCAL_INFORMATION_DATE , 4 },
    { 8, GET_INKEY_VALID , 8 },
    { 8, SET_UP_IDLE_MODE_TEXT , 16 },
    { 8, RUN_AT_COMMAND , 32 },
    { 8, SET_UP_CALL_VALID , 64 },
    { 8, CALL_CONTROL_VALID , 128 },
    // NINTH BYTE
    { 9, SEND_DTMF , 2 },
    { 9, PROVIDE_LOCAL_INFORMATION_NMR_VALID , 4 },
    { 9, PROVIDE_LOCAL_INFORMATION_LANG , 128 },
    { 9, PROVIDE_LOCAL_INFORMATION_TIMING , 128 },
    { 9, LANGUAGE_NOTIFICATION , 32 },
    { 9, LAUNCH_BROWSER , 64 },
    { 9, PROVIDE_LOCAL_INFORMATION_ACCESS , 128 },
    // TENTH BYTE
    // ELEVENTH BYTE
    // TWELFTH BYTE
    { 12, OPEN_CHANNEL , 1 },
    { 12, CLOSE_CHANNEL , 2 },
    { 12, RECEIVE_DATA , 4 },
    { 12, SEND_DATA , 8 },
    { 12, GET_CHANNEL_STATUS , 16 },
    { 12, SERVICE_SEARCH , 32 },
    { 12, GET_SERVICE_INFORMATION , 64 },
    { 12, DECLARE_SERVICE , 128 },
    // THIRTEENTH BYTE
    // FOURTEENTH BYTE
    // FIFTEENTH BYTE
    // SIXTEENTH BYTE
    // SEVENTEENTH BYTE
    // EIGHTEENTH BYTE
    { 18, DISPLAY_TEXT_VAR_TIME_OUT , 1 },
    { 18, GET_INKEY_HELP_SUPPORTED , 2 },
    { 18, GET_INKEY_VAR_TIME_OUT , 8 },
    { 18, PROVIDE_LOCAL_INFORMATION_ESN , 16 },
    { 18, PROVIDE_LOCAL_INFORMATION_IMEISV , 64 },
    { 18, PROVIDE_LOCAL_INFORMATION_SEARCH_MODE_CHANGE , 128 },
    // NINETEENTH BYTE
    // TWENTIETH BYTE
    // TWENTY-FIRST BYTE
    // TWENTY-SECOND BYTE
    { 22, PROVIDE_LOCAL_INFORMATION_BATT_STATE , 2 },
    { 22, PLAY_TONE_MELODY , 4 },
    { 22, RETRIEVE_MULTIMEDIA_MESSAGE , 32},
    { 22, SUBMIT_MULTIMEDIA_MESSAGE , 64},
    { 22, DISPLAY_MULTIMEDIA_MESSAGE , 128 },
    // TWENTY-THIRD BYTE
    { 23, SET_FRAMES , 1 },
    { 23, GET_FRAMES_STATUS , 2 },
    { 23, PROVIDE_LOCAL_INFORMATION_MEID , 32 },
    { 23, PROVIDE_LOCAL_INFORMATION_NMR_UTRAN , 64 },
    // TWENTY-FOURTH BYTE
    // TWENTY-FIFTH BYTE
    // TWENTY-FOURTH BYTE
    // THIRTIETH BYTE
    { 30, PROVIDE_LOCAL_INFORMATION_WSID , 2 },
    { 30, ACTIVATE_CLASS_L , 16 },
    { 30, GEO_LOCATION_REQUEST , 32 },
    { 30, PROVIDE_LOCAL_INFORMATION_BROADCAST , 64 },
    // THIRTY-FIRST BYTE
    { 31, CONTACTLESS_STATE_CHANGED , 1 },
    { 31, COMMAND_CONTAINER , 128 },
    // THIRTY-SECOND BYTE
    { 32, ENCAPSULATED_SESSION_CONTROL , 64 },
    // END
    { 0, 0 , 0 }
};


CCatProfile::CCatProfile()
 : m_isTeProfileSet(FALSE)
{
    RIL_LOG_VERBOSE("CCatProfile::CCatProfile() - Enter\r\n");
    InitTeProfile();
    // Mapping of a profile string.
    // Focused on proactive commands.
}

/**
 * Specify the TE profile to use.
 * Once set it cannot be re-set, except if calling ResetTeProfile method.
 *
 * @param pszProfile : An allocated string of hexadecimal characters specifying the TE profile.
 * @param uiLength : The length of the profile string.
 * @return false it is not possible to set the TE profile
 */
BOOL CCatProfile::SetTeProfile(const char* pszProfile, const UINT32 uiLength)
{
    BOOL bRet = TRUE;

    if (!pszProfile)
    {
        RIL_LOG_CRITICAL("CCatProfile::SetTeProfile() - ERROR NULL PROFILE!\r\n");
        return FALSE;
    }

    if (m_isTeProfileSet)
    {
        RIL_LOG_CRITICAL("CCatProfile::SetTeProfile() - ERROR TE Profile is already set.\r\n");
        return FALSE;
    }

    RIL_LOG_INFO("CCatProfile::SetTeProfile() - TE Profile: %s\r\n", pszProfile);

    bRet = extractByteArrayFromString(pszProfile, uiLength, m_achTeProfile);
    if (bRet)
    {
        m_isTeProfileSet = TRUE;
    }

    RIL_LOG_CRITICAL("CCatProfile::SetTeProfile() - END, bRet:%d\r\n", bRet);
    return bRet;
}

/**
 * This method parses the given PDU and extract info into ProactiveCommandInfo given object.
 *
 * @param pszPdu : String of hexadecimal characters.
 * @param uiLength : Length of PDU.
 * @param pPduInfo : Allocated pointer to an ProactiveCommandInfo object.
 */
BOOL CCatProfile::ExtractPduInfo(const char* pszPdu, const UINT32 uiLength,
        ProactiveCommandInfo* pPduInfo)
{
    BOOL bRet = FALSE;
    BerTlv tlvPdu;
    BerTlv tlvCmdDetails;
    UINT8* pPduBytes = NULL;
    const UINT8* pbCmdData = NULL;
    UINT32 cbCmdDataSize = 0;
    BOOL bFound = FALSE;
    UINT8 uiCmd = 0;

    if (!pPduInfo)
    {
        RIL_LOG_CRITICAL("CCatProfile::ExtractPduInfo() - ERROR BAD PARAMETER\r\n");
        goto Error;
    }

    // Init returned struct
    pPduInfo->uiCommandId = 0;
    pPduInfo->isProactiveCmd = FALSE;

    // NEED TO CONVERT chars string onto bytes array
    pPduBytes = (UINT8*)malloc(uiLength);
    if (NULL == pPduBytes)
    {
        RIL_LOG_CRITICAL("CCatProfile::ExtractPduInfo() -"
                " Could not allocate memory for PDU.\r\n");
        goto Error;
    }
    bRet = extractByteArrayFromString(pszPdu, uiLength, pPduBytes);

    RIL_LOG_INFO("CCatProfile::ExtractPduInfo() - ENTER:Length:%d, PDU:[0x%X][0x%X]\r\n",
            uiLength, pPduBytes[0], pPduBytes[1]);

    if (tlvPdu.Parse(pPduBytes, uiLength))
    {
        RIL_LOG_INFO("CCatProfile::ExtractPduInfo() : First Tag:0x%X, Length:%d\r\n",
                tlvPdu.GetTag(), tlvPdu.GetLength());

        pbCmdData = tlvPdu.GetValue();
        cbCmdDataSize = tlvPdu.GetLength();

        if (tlvCmdDetails.Parse(pbCmdData, cbCmdDataSize))
        {
            uiCmd = tlvCmdDetails.GetTag();
            RIL_LOG_INFO("CCatProfile::ExtractPduInfo() : Command DETAILS Tag:0x%X\r\n", uiCmd);
            if (uiCmd == COMMAND_DETAILS_TAG)
            {
                pbCmdData = tlvCmdDetails.GetValue();
                // Format is (see ETSI 102223 - Annex B and C):
                // pbCmdData[0] = Command number
                // pbCmdData[1] and pbCmdData[2] = Command indentifier
                cbCmdDataSize = tlvCmdDetails.GetLength();
                if (cbCmdDataSize > 1)
                {
                    uiCmd = pbCmdData[1];
                }
                else
                {
                    RIL_LOG_CRITICAL("CCatProfile::ExtractPduInfo() -"
                            "ERROR COMMAND DETAILS WRONG SIZE:%d!\r\n", cbCmdDataSize);
                    goto Error;
                }
            }
            else
            {
                RIL_LOG_CRITICAL("CCatProfile::ExtractPduInfo() -"
                        "ERROR CmdId:0x%X NOT A COMMAND DETAILS\r\n", uiCmd);
                goto Error;
            }
            pPduInfo->uiCommandId = uiCmd;
            RIL_LOG_INFO("CCatProfile::ExtractPduInfo() -"
                    "Command Tag:0x%X, Length:%d\r\n", uiCmd, cbCmdDataSize);

            int indexFound = -1;
            for (int i = 0; s_proactiveUICCTable[i].uiByteId != 0; ++i)
            {

                if (s_proactiveUICCTable[i].uiCmdId == uiCmd)
                {
                    indexFound = i;
                    break;
                }
            }
            if (indexFound > -1)
            {
                UINT8 byteId = s_proactiveUICCTable[indexFound].uiByteId;
                bFound = (m_achTeProfile[byteId - 1]
                        & s_proactiveUICCTable[indexFound].uiBitMask) != 0;
                pPduInfo->isProactiveCmd = bFound;
                bRet = TRUE;
                RIL_LOG_INFO("CCatProfile::ExtractPduInfo() -"
                        " indexFound:%d, CmdId:0x%X, uiByteId:0x%X,"
                        " uiCmdId:0x%X, uiBitMask:0x%X, Found:%d\r\n", indexFound, uiCmd,
                        s_proactiveUICCTable[indexFound].uiByteId,
                        s_proactiveUICCTable[indexFound].uiCmdId,
                        s_proactiveUICCTable[indexFound].uiBitMask, bFound);
            }
        }
    }

    // Return TRUE if CMD was known and found in ProactiveUICC table.

Error:
    free(pPduBytes);
    pPduBytes = NULL;

    RIL_LOG_CRITICAL("CCatProfile::ExtractPduInfo() : Return:%d\r\n", bRet);
    return bRet;
}

void CCatProfile::InitTeProfile()
{
    RIL_LOG_INFO("CCatProfile::InitTeProfile() - Enter\r\n");
    CRepository repository;
    const char* DEFAULT_TE_PROFILE
            = "0000000000000000000000000000000000000000000000000000000000000000";
    UINT32 uiProfileLength = strlen(DEFAULT_TE_PROFILE);
    char szTeProfile[MAX_BUFFER_SIZE] = {'\0'};

    if (IsTeProfileSet() == FALSE)
    {
        // Read the Te profile from repository.txt
        if (!repository.Read(g_szGroupModem, g_szTeProfile, szTeProfile, MAX_BUFFER_SIZE))
        {
            SetTeProfile(DEFAULT_TE_PROFILE, uiProfileLength);
            RIL_LOG_CRITICAL("CCatProfile::InitTeProfile() - "
                    "No TE profile found in repository\r\n");
        }
        else
        {
            SetTeProfile(szTeProfile, strlen(szTeProfile));
        }
    }

    RIL_LOG_INFO("CCatProfile::InitTeProfile() - Exit\r\n");
}

