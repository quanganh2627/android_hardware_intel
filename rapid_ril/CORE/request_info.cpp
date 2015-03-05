////////////////////////////////////////////////////////////////////////////
// request_info.cpp
//
// Copyright 2013 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Maps Android AT command requests to DLC channels.
//
/////////////////////////////////////////////////////////////////////////////

#include "types.h"
#include "rilchannels.h"
#include "request_info.h"

// Internal request info
REQ_INFO_INTERNAL g_ReqInternal[] =
{
     { { "SilentPinEntry", RIL_CHANNEL_DLC8, 0 }, E_REQ_ID_INTERNAL_SILENT_PIN_ENTRY },
     { { "QuerySimSmsStoreStatus", RIL_CHANNEL_OEM, 0 },
            E_REQ_ID_INTERNAL_QUERY_SIM_SMS_STORE_STATUS }
};

const int INTERNAL_REQ_ID_TOTAL = (sizeof(g_ReqInternal) / sizeof(REQ_INFO_INTERNAL));

REQ_INFO* g_pReqInfo;

// Request info array - Maps a request id to request names and channels
// Access request info using request ids defined in ril.h
const REQ_INFO g_ReqInfoDefault[] =
{
    // reserved/not used 0
    { "", RIL_CHANNEL_RESERVED, 0 },
    // RIL_REQUEST_GET_SIM_STATUS 1
    { "GetSimStatus", RIL_CHANNEL_DLC8, 0 },
    // RIL_REQUEST_ENTER_SIM_PIN 2
    { "EnterSimPin", RIL_CHANNEL_DLC8, 0 },
    // RIL_REQUEST_ENTER_SIM_PUK 3
    { "EnterSimPuk", RIL_CHANNEL_DLC8, 0 },
    // RIL_REQUEST_ENTER_SIM_PIN2 4
    { "EnterSimPin2", RIL_CHANNEL_DLC8, 0 },
    // RIL_REQUEST_ENTER_SIM_PUK2 5
    { "EnterSimPuk2", RIL_CHANNEL_DLC8, 0 },
    // RIL_REQUEST_CHANGE_SIM_PIN 6
    { "ChangeSimPin", RIL_CHANNEL_DLC8, 0 },
    // RIL_REQUEST_CHANGE_SIM_PIN2 7
    { "ChangeSimPin2", RIL_CHANNEL_DLC8, 0 },
    // RIL_REQUEST_ENTER_NETWORK_DEPERSONALIZATION 8
    { "EnterNetworkDepersonalization", RIL_CHANNEL_DLC2, 0},
    // RIL_REQUEST_GET_CURRENT_CALLS 9
    { "GetCurrentCalls", RIL_CHANNEL_ATCMD, 0 },
    // RIL_REQUEST_DIAL 10
    { "Dial", RIL_CHANNEL_DLC6, 0 },
    // RIL_REQUEST_GET_IMSI 11
    { "GetIMSI", RIL_CHANNEL_DLC8, 0 },
    // RIL_REQUEST_HANGUP 12
    { "Hangup", RIL_CHANNEL_DLC6, 0 },
    // RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND 13
    { "HangupWaitingOrBackground", RIL_CHANNEL_DLC6, 0 },
    // RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND 14
    { "HangupForegroundResumeBackground", RIL_CHANNEL_DLC6, 0 },
    // RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE 15
    { "SwitchHoldingAndActive", RIL_CHANNEL_DLC6, 0 },
    // RIL_REQUEST_CONFERENCE 16
    { "Conference", RIL_CHANNEL_ATCMD, 0 },
    // RIL_REQUEST_UDUB 17
    { "UDUB", RIL_CHANNEL_DLC6, 0 },
    // RIL_REQUEST_LAST_CALL_FAIL_CAUSE 18
    { "LastCallFailCause", RIL_CHANNEL_ATCMD, 0 },
    // RIL_REQUEST_SIGNAL_STRENGTH 19
    { "SignalStrength", RIL_CHANNEL_DLC2, 0 },
    // RIL_REQUEST_VOICE_REGISTRATION_STATE 20
    { "RegistrationState", RIL_CHANNEL_DLC2, 0 },
    // RIL_REQUEST_DATA_REGISTRATION_STATE 21
    { "GprsRegistrationState", RIL_CHANNEL_DLC2, 0 },
    // RIL_REQUEST_OPERATOR 22
    { "Operator", RIL_CHANNEL_DLC8, 0 },
    // RIL_REQUEST_RADIO_POWER 23
    { "RadioPower", RIL_CHANNEL_ATCMD, 0 },
    // RIL_REQUEST_DTMF 24
    { "Dtmf", RIL_CHANNEL_DLC6, 0 },
    // RIL_REQUEST_SEND_SMS 25
    { "SendSms", RIL_CHANNEL_DLC6, 0 },
    // RIL_REQUEST_SEND_SMS_EXPECT_MORE 26
    { "SendSmsExpectMore", RIL_CHANNEL_DLC6, 0 },
    // RIL_REQUEST_SETUP_DATA_CALL 27
    { "SetupDefaultPDP", RIL_CHANNEL_DATA1, 0 },
    // RIL_REQUEST_SIM_IO 28
    { "SimIO", RIL_CHANNEL_DLC8, 0 },
    // RIL_REQUEST_SEND_USSD 29
    { "SendUSSD", RIL_CHANNEL_DLC6, 0 },
    // RIL_REQUEST_CANCEL_USSD 30
    { "CancelUSSD", RIL_CHANNEL_DLC6, 0 },
    // RIL_REQUEST_GET_CLIR 31
    { "GetCLIR", RIL_CHANNEL_DLC6, 0 },
    // RIL_REQUEST_SET_CLIR 32
    { "SetCLIR", RIL_CHANNEL_DLC6, 0 },
    // RIL_REQUEST_QUERY_CALL_FORWARD_STATUS 33
    { "QueryCallForwardStatus", RIL_CHANNEL_DLC6, 0 },
    // RIL_REQUEST_SET_CALL_FORWARD 34
    { "SetCallForward", RIL_CHANNEL_DLC6, 0 },
    // RIL_REQUEST_QUERY_CALL_WAITING 35
    { "QueryCallWaiting", RIL_CHANNEL_DLC6, 0 },
    // RIL_REQUEST_SET_CALL_WAITING 36
    { "SetCallWaiting", RIL_CHANNEL_DLC6, 0 },
    // RIL_REQUEST_SMS_ACKNOWLEDGE 37
    { "SmsAcknowledge", RIL_CHANNEL_ATCMD, 0 },
    // RIL_REQUEST_GET_IMEI 38
    { "GetIMEI", RIL_CHANNEL_DLC8, 0 },
    // RIL_REQUEST_GET_IMEISV 39
    { "GetIMEISV", RIL_CHANNEL_DLC8, 0 },
    // RIL_REQUEST_ANSWER 40
    { "Answer", RIL_CHANNEL_ATCMD, 0 },
    // RIL_REQUEST_DEACTIVATE_DATA_CALL 41
    { "DeactivateDataCall", RIL_CHANNEL_DLC2, 0 },
    // RIL_REQUEST_QUERY_FACILITY_LOCK 42
    { "QueryFacilityLock", RIL_CHANNEL_DLC6, 0 },
    // RIL_REQUEST_SET_FACILITY_LOCK 43
    { "SetFacilityLock", RIL_CHANNEL_DLC6, 0 },
    // RIL_REQUEST_CHANGE_BARRING_PASSWORD 44
    { "ChangeBarringPassword", RIL_CHANNEL_DLC8, 0 },
    // RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE 45
    { "QueryNetworkSelectionMode", RIL_CHANNEL_DLC2, 0 },
    // RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC 46
    { "SetNetworkSelectionAutomatic", RIL_CHANNEL_DLC2, 0 },
    // RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL 47
    { "SetNetworkSelectionManual", RIL_CHANNEL_DLC2, 0 },
    // RIL_REQUEST_QUERY_AVAILABLE_NETWORKS 48
    { "QueryAvailableNetworks", RIL_CHANNEL_DLC2, 0 },
    // RIL_REQUEST_DTMF_START 49
    { "RequestDtmfStart", RIL_CHANNEL_ATCMD, 0 },
    // RIL_REQUEST_DTMF_STOP 50
    { "RequestDtmfStop", RIL_CHANNEL_ATCMD, 0 },
    // RIL_REQUEST_BASEBAND_VERSION 51
    { "BasebandVersion", RIL_CHANNEL_DLC8, 0 },
    // RIL_REQUEST_SEPARATE_CONNECTION 52
    { "SeperateConnection", RIL_CHANNEL_ATCMD, 0 },
    // RIL_REQUEST_SET_MUTE 53
    { "SetMute", RIL_CHANNEL_ATCMD, 0 },
    // RIL_REQUEST_GET_MUTE 54
    { "GetMute", RIL_CHANNEL_ATCMD, 0 },
    // RIL_REQUEST_QUERY_CLIP 55
    { "QueryCLIP", RIL_CHANNEL_DLC6, 0 },
    // RIL_REQUEST_LAST_DATA_CALL_FAIL_CAUSE 56
    { "LastPdpFailCause", RIL_CHANNEL_DLC2, 0 },
    // RIL_REQUEST_DATA_CALL_LIST 57
    { "PdpContextList", RIL_CHANNEL_ATCMD, 0 },
    // RIL_REQUEST_RESET_RADIO 58
    { "ResetRadio", RIL_CHANNEL_ATCMD, 0 },
    // RIL_REQUEST_OEM_HOOK_RAW 59
    { "OemHookRaw", RIL_CHANNEL_ATCMD, 0 },
    // RIL_REQUEST_OEM_HOOK_STRINGS 60
    { "OemHookStrings", RIL_CHANNEL_ATCMD, 0 },
    // RIL_REQUEST_SCREEN_STATE 61
    { "ScreenState", RIL_CHANNEL_URC, 0 },
    // RIL_REQUEST_SET_SUPP_SVC_NOTIFICATION 62
    { "SetSuppSvcNotification", RIL_CHANNEL_DLC6, 0 },
    // RIL_REQUEST_WRITE_SMS_TO_SIM 63
    { "WriteSmsToSim", RIL_CHANNEL_DLC8, 0 },
    // RIL_REQUEST_DELETE_SMS_ON_SIM 64
    { "DeleteSmsOnSim", RIL_CHANNEL_DLC8, 0 },
    // RIL_REQUEST_SET_BAND_MODE 65
    { "SetBandMode", RIL_CHANNEL_DLC2, 0 },
    // RIL_REQUEST_QUERY_AVAILABLE_BAND_MODE 66
    { "QueryAvailableBandMode", RIL_CHANNEL_DLC2, 0 },
    // RIL_REQUEST_STK_GET_PROFILE 67
    { "StkGetProfile", RIL_CHANNEL_DLC8, 0 },
    // RIL_REQUEST_STK_SET_PROFILE 68
    { "StkSetProfile", RIL_CHANNEL_DLC8, 0 },
    // RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND 79
    { "StkSendEnvelopeCommand", RIL_CHANNEL_DLC8, 0 },
    // RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE 70
    { "StkSendTerminalResponse", RIL_CHANNEL_DLC8, 0 },
    // RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM 71
    { "StkHandleCallSetupRequestedFromSim", RIL_CHANNEL_DLC8, 0 },
    // RIL_REQUEST_EXPLICIT_CALL_TRANSFER 72
    { "ExplicitCallTransfer", RIL_CHANNEL_ATCMD, 0 },
    // RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE 73
    { "SetPreferredNetworkType", RIL_CHANNEL_DLC2, 0 },
    // RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE 74
    { "GetPreferredNetworkType", RIL_CHANNEL_DLC2, 0 },
    // RIL_REQUEST_GET_NEIGHBORING_CELL_IDS 75
    { "GetNeighboringCellIDs", RIL_CHANNEL_OEM, 0 },
    // RIL_REQUEST_SET_LOCATION_UPDATES 76
    { "SetLocationUpdates", RIL_CHANNEL_URC, 0 },
    // RIL_REQUEST_CDMA_SET_SUBSCRIPTION_SOURCE 77
    { "CdmaSetSubscription", RIL_CHANNEL_RESERVED, 0 },
    // RIL_REQUEST_CDMA_SET_ROAMING_PREFERENCE 78
    { "CdmaSetRoamingPreference", RIL_CHANNEL_RESERVED, 0 },
    // RIL_REQUEST_CDMA_QUERY_ROAMING_PREFERENCE 79
    { "CdmaQueryRoamingPreference", RIL_CHANNEL_RESERVED, 0 },
    // RIL_REQUEST_SET_TTY_MODE 80
    { "SetTtyMode", RIL_CHANNEL_ATCMD, 0 },
    // RIL_REQUEST_QUERY_TTY_MODE 81
    { "QueryTtyMode", RIL_CHANNEL_ATCMD, 0 },
    // RIL_REQUEST_CDMA_SET_PREFERRED_VOICE_PRIVACY_MODE 82
    { "CdmaSetPreferredVoicePrivacyMode", RIL_CHANNEL_RESERVED, 0 },
    // RIL_REQUEST_CDMA_QUERY_PREFERRED_VOICE_PRIVACY_MODE 83
    { "CdmaQueryPreferredVoicePrivacyMode", RIL_CHANNEL_RESERVED, 0 },
    // RIL_REQUEST_CDMA_FLASH 84
    { "CdmaFlash", RIL_CHANNEL_RESERVED, 0 },
    // RIL_REQUEST_CDMA_BURST_DTMF 85
    { "CdmaBurstDtmf", RIL_CHANNEL_RESERVED, 0 },
    // RIL_REQUEST_CDMA_VALIDATE_AND_WRITE_AKEY 86
    { "CdmaValidateKey", RIL_CHANNEL_RESERVED, 0 },
    // RIL_REQUEST_CDMA_SEND_SMS 87
    { "CdmaSendSms", RIL_CHANNEL_RESERVED, 0 },
    // RIL_REQUEST_CDMA_SMS_ACKNOWLEDGE 88
    { "CdmaSmsAcknowledge", RIL_CHANNEL_RESERVED, 0 },
    // RIL_REQUEST_GSM_GET_BROADCAST_SMS_CONFIG 89
    { "GetBroadcastSmsConfig", RIL_CHANNEL_DLC6, 0 },
    // RIL_REQUEST_GSM_SET_BROADCAST_SMS_CONFIG 90
    { "SetBroadcastSmsConfig", RIL_CHANNEL_DLC6, 0 },
    // RIL_REQUEST_GSM_SMS_BROADCAST_ACTIVATION 91
    { "SmsBroadcastActivation", RIL_CHANNEL_DLC6, 0 },
    // RIL_REQUEST_CDMA_GET_BROADCAST_SMS_CONFIG 92
    { "CdmaGetBroadcastSmsConfig", RIL_CHANNEL_RESERVED, 0 },
    // RIL_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG 93
    { "CdmaSetBroadcastSmsConfig", RIL_CHANNEL_RESERVED, 0 },
    // RIL_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION 94
    { "CdmaSmsBroadcastActivation", RIL_CHANNEL_RESERVED, 0 },
    // RIL_REQUEST_CDMA_SUBSCRIPTION 95
    { "CdmaSubscription", RIL_CHANNEL_RESERVED, 0 },
    // RIL_REQUEST_CDMA_WRITE_SMS_TO_RUIM 96
    { "CdmaWriteSmsToRuim", RIL_CHANNEL_RESERVED, 0 },
    // RIL_REQUEST_CDMA_DELETE_SMS_ON_RUIM 97
    { "CdmaDeleteSmsOnRuim", RIL_CHANNEL_RESERVED, 0 },
    // RIL_REQUEST_DEVICE_IDENTITY 98
    { "DeviceIdentity", RIL_CHANNEL_DLC8, 0 },
    // RIL_REQUEST_EXIT_EMERGENCY_CALLBACK_MODE 99
    { "ExitEmergencyCallBackMode", RIL_CHANNEL_RESERVED, 0 },
    // RIL_REQUEST_GET_SMSC_ADDRESS 100
    { "GetSmscAddress", RIL_CHANNEL_DLC8, 0 },
    // RIL_REQUEST_SET_SMSC_ADDRESS 101
    { "SetSmscAddress", RIL_CHANNEL_DLC8, 0 },
    // RIL_REQUEST_REPORT_SMS_MEMORY_STATUS 102
    { "ReportSmsMemoryStatus", RIL_CHANNEL_DLC6, 0 },
    // RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING 103
    { "ReportStkServiceIsRunning", RIL_CHANNEL_URC, 0 },
    // RIL_REQUEST_CDMA_GET_SUBSCRIPTION_SOURCE 104
    { "GetSubscriptionSource", RIL_CHANNEL_RESERVED, 0 },
    // RIL_REQUEST_ISIM_AUTHENTICATION 105
    { "IsimAuthentication", RIL_CHANNEL_DLC8, 0 },
    // RIL_REQUEST_ACKNOWLEDGE_INCOMING_GSM_SMS_WITH_PDU 106
    { "AckIncomingSmsWithPdu", RIL_CHANNEL_DLC6, 0 },
    // RIL_REQUEST_STK_SEND_ENVELOPE_WITH_STATUS 107
    { "StkSendEnvelopeWithStatus", RIL_CHANNEL_DLC8, 0 },
    // RIL_REQUEST_VOICE_RADIO_TECH 108
    { "VoiceRadioTech", RIL_CHANNEL_DLC2, 0 },
    // RIL_REQUEST_GET_CELL_INFO_LIST 109
    { "GetCellInfoList", RIL_CHANNEL_OEM, 0 },
    // RIL_REQUEST_SET_UNSOL_CELL_INFO_LIST_RATE 110
    { "SetCellInfoListRate", RIL_CHANNEL_OEM, 0 },
    // RIL_REQUEST_SET_INITIAL_ATTACH_APN 111
    { "SetInitialAttachApn", RIL_CHANNEL_DLC2, 0},
    // RIL_REQUEST_IMS_REGISTRATION_STATE 112
    { "GetImsRegistrationState", RIL_CHANNEL_RESERVED, 0},
    // RIL_REQUEST_IMS_SEND_SMS 113
    { "SendImsSms", RIL_CHANNEL_RESERVED, 0},
    // RIL_REQUEST_SIM_TRANSMIT_BASIC 114
    { "SimTransmitBasic", RIL_CHANNEL_DLC8, 0 },
    // RIL_REQUEST_SIM_OPEN_CHANNEL 115
    { "SimOpenChannel", RIL_CHANNEL_DLC8, 0 },
    // RIL_REQUEST_SIM_CLOSE_CHANNEL 116
    { "SimCloseChannel", RIL_CHANNEL_DLC8, 0 },
    // RIL_REQUEST_SIM_TRANSMIT_CHANNEL 117
    { "SimTransmitChannel", RIL_CHANNEL_DLC8, 0 },
#if defined(M2_VT_FEATURE_ENABLED)
    // RIL_REQUEST_HANGUP_VT 118
    { "HangupVT", RIL_CHANNEL_DLC6, 0 },
    // RIL_REQUEST_DIAL_VT 119
    { "DialVT", RIL_CHANNEL_ATCMD, 0 },
#endif  // M2_VT_FEATURE_ENABLED
#if defined(M2_GET_SIM_SMS_STORAGE_ENABLED)
    // RIL_REQUEST_GET_SIM_SMS_STORAGE 118 or 120
    { "GetSimSmsStorage", RIL_CHANNEL_OEM, 0 }
#endif
};

const int REQ_ID_TOTAL = (sizeof(g_ReqInfoDefault) / sizeof(REQ_INFO));
