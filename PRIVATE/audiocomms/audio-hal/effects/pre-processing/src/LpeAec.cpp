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

#include "LpeAec.hpp"
#include <audio_effects/effect_aec.h>

AecAudioEffect::AecAudioEffect(const effect_interface_s *itfe)
    : AudioEffect(itfe, &mAecDescriptor)
{
}

const effect_descriptor_t AecAudioEffect::mAecDescriptor = {
    type:         FX_IID_AEC_,
    uuid:         {
        timeLow: 0x1bf4de00,
        timeMid: 0x3c8b,
        timeHiAndVersion: 0x11e3,
        clockSeq: 0xbca7,
        node: { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b }
    },
    apiVersion:   EFFECT_CONTROL_API_VERSION,
    flags:        (EFFECT_FLAG_TYPE_PRE_PROC | EFFECT_FLAG_DEVICE_IND),
    cpuLoad:      0,
    memoryUsage:  0,
    "Acoustic Echo Canceller", /**< name. */
    "IntelLPE"                 /**< implementor. */
};
