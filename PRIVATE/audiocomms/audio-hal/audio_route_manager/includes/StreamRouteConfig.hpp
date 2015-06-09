/*
 * INTEL CONFIDENTIAL
 * Copyright (c) 2013-2014 Intel
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
 *
 */
#pragma once

#include <hardware/audio.h>
#include <SampleSpec.hpp>

namespace intel_audio
{

struct StreamRouteConfig
{
    /**
     * flags to indicate whether the route must be enabled before or after opening the device.
     */
    bool requirePreEnable;

    /**
     * flags to indicate whether the route must be closed before or after opening the device.
     */
    bool requirePostDisable;

    const char *cardName;   /**< Alsa card used by the stream route. */
    uint8_t deviceId;       /**< Alsa card used by the stream route. */

    /**
     * pcm configuration supported by the stream route.
     */
    uint32_t channels;
    uint32_t rate;
    uint32_t periodSize;
    uint32_t periodCount;
    audio_format_t format;
    uint32_t startThreshold;
    uint32_t stopThreshold;
    uint32_t silenceThreshold;
    uint32_t availMin;

    uint8_t silencePrologInMs; /**< if needed, silence to be appended before valid samples. */
    uint32_t applicabilityMask; /**< extra mask to check for applicability of stream. */

    /**
     * Channel policy vector followed by this route.
     * Each channel must specify its channel policy among these values:
     *   - copy policy: channel has no specific weight
     *   - ignore policy: channel has a null weight, and must/will be ignored by the HW
     *   - average policy: channel has a highest weight among the other, it must contains all valid
     */
    std::vector<SampleSpec::ChannelsPolicy> channelsPolicy;
};

} // namespace intel_audio
