/*
 * INTEL CONFIDENTIAL
 * Copyright (c) 2014 Intel
 * Corporation All Rights Reserved.
 *
 * The source code contained or described herein and all documents related to
 * the source code ("Material") are owned by Intel Corporation or its suppliers
 * or licensors. Title to the Material remains with Intel Corporation or its
 * suppliers and licensors. The Material contains trade secrets and proprietary
 * and confidential information of Intel or its suppliers and licensors. The
 * Material is protected by worldwide copyright and trade secret laws and
 * treaty provisions. No part of the Material may be used, copied, reproduced,
 * modified, published, uploaded, posted, transmitted, distributed, or
 * disclosed in any way without Intel's prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 */

#pragma once

/* Always Listening/VTSV */
#define AUDIO_PARAMETER_KEY_ALWAYS_LISTENING_STATUS "vtsv_active"
#define AUDIO_PARAMETER_VALUE_ALWAYS_LISTENING_ON "true"
#define AUDIO_PARAMETER_VALUE_ALWAYS_LISTENING_OFF "false"

/* Always Listening Route/VTSV */
#define AUDIO_PARAMETER_KEY_ALWAYS_LISTENING_ROUTE "vtsv_route"
#define AUDIO_PARAMETER_KEY_LPAL_DEVICE "lpal_device"
#define AUDIO_PARAMETER_VALUE_ALWAYS_LISTENING_ROUTE_ON "on"
#define AUDIO_PARAMETER_VALUE_ALWAYS_LISTENING_ROUTE_OFF "off"

/* Context awareness */
#define AUDIO_PARAMETER_KEY_CONTEXT_AWARENESS_STATUS "context_awareness_status"
#define AUDIO_PARAMETER_VALUE_CONTEXT_AWARENESS_ON "on"
#define AUDIO_PARAMETER_VALUE_CONTEXT_AWARENESS_OFF "off"

/**
 * Stream flags key (e.g. for offload routing use case)
 * The values that the parameter  with this keys can use are the stream flags
 * enumeration found in @see audio_output_flags_t enum (system/audio.h)
 */
#define AUDIO_PARAMETER_KEY_STREAM_FLAGS "stream_flags"

/* No Non-linear Post processing */
#define AUDIO_PARAMETER_KEY_BYPASS_NON_LINEAR_POSTPROCESSING_SETTING "BypassNonLinearPp"
#define AUDIO_PARAMETER_VALUE_BYPASS_NON_LINEAR_PP_ON "on"
#define AUDIO_PARAMETER_VALUE_BYPASS_NON_LINEAR_PP_OFF "off"

/* No Linear Post processing */
#define AUDIO_PARAMETER_KEY_BYPASS_LINEAR_POSTPROCESSING_SETTING "BypassLinearPp"
#define AUDIO_PARAMETER_VALUE_BYPASS_LINEAR_PP_ON "on"
#define AUDIO_PARAMETER_VALUE_BYPASS_LINEAR_PP_OFF "off"

