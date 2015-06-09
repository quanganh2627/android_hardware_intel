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

#define LOG_TAG "IntelPreProcessingFx"

#include "LpeWnr.hpp"

WnrAudioEffect::WnrAudioEffect(const effect_interface_s *itfe)
    : AudioEffect(itfe, &mWnrDescriptor)
{
}

static const effect_uuid_t FX_IID_WNR_ = {
    timeLow: 0xdab015e0,
    timeMid: 0xbfac,
    timeHiAndVersion: 0x11e3,
    clockSeq: 0xbcd9,
    node: { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b }
};

const effect_uuid_t *const FX_IID_WNR = &FX_IID_WNR_;

const effect_descriptor_t WnrAudioEffect::mWnrDescriptor = {
    type:         FX_IID_WNR_,
    uuid:         {
        timeLow: 0xe8c32c80,
        timeMid: 0xeb21,
        timeHiAndVersion: 0x11e3,
        clockSeq: 0x8132,
        node: { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b }
    },
    apiVersion:   EFFECT_CONTROL_API_VERSION,
    flags:        (EFFECT_FLAG_TYPE_PRE_PROC | EFFECT_FLAG_DEVICE_IND),
    cpuLoad:      0,
    memoryUsage:  0,
    "Wind Noise Reduction",                 /**< name. */
    "IntelLPE"                 /**< implementor. */
};
