////////////////////////////////////////////////////////////////////////////
// rilchannels.h
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides function prototypes for
//    channel-related helper functions.
//    Also enumerates all of the channels that
//    can be used when sending AT Commands.
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

//  List channels here (one per COM port)
#define RIL_CHANNEL_MAX 11

// Upper limit on number of RIL channels to create
extern UINT32 g_uiRilChannelUpperLimit;

// Current RIL channel index maximum (depends on number of data channels created)
extern UINT32 g_uiRilChannelCurMax;

// Number of apn profile
#define NUMBER_OF_APN_PROFILE 9

// Number of channels able to carry data.
#define RIL_MAX_NUM_IPC_CHANNEL     5

// HSI: channel 0 and 1 are not used for data.
#define RIL_DEFAULT_IPC_CHANNEL_MIN    2

// HSI: default configuration for 7x60.
#define RIL_DEFAULT_IPC_RESOURCE_NAME   "mipi_ipc"

//  Call control commands, misc commands
#define RIL_CHANNEL_ATCMD 0

//  GPRS/UMTS management (GPRS attach/detach), network commands
#define RIL_CHANNEL_DLC2 1

//  Call settings, SMS, supplementary services
#define RIL_CHANNEL_DLC6 2

//  SIM related functions, SIM toolkit
#define RIL_CHANNEL_DLC8 3

//  URC channel
#define RIL_CHANNEL_URC 4

//  OEM channel (diagnostic commands)
#define RIL_CHANNEL_OEM 5

//  GPRS/UMTS data (1st primary context)
#define RIL_CHANNEL_DATA1 6

//  GPRS/UMTS data (2nd primary context)
#define RIL_CHANNEL_DATA2 7

//  GPRS/UMTS data (3rd primary context)
#define RIL_CHANNEL_DATA3 8

//  GPRS/UMTS data (4th primary context)
#define RIL_CHANNEL_DATA4 9

//  GPRS/UMTS data (5th primary context)
#define RIL_CHANNEL_DATA5 10

// TODO - currently allow up to one reserved channel (e.g., for direct audio use).
// Set this to -1 or a number greater than RIL_CHANNEL_MAX if there is no reserved channel
#define RIL_CHANNEL_RESERVED 555

