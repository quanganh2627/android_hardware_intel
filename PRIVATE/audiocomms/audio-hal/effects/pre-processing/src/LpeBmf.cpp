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

#include "LpeBmf.hpp"

BmfAudioEffect::BmfAudioEffect(const effect_interface_s *itfe)
    : AudioEffect(itfe, &mBmfDescriptor)
{
}

static const effect_uuid_t FX_IID_BMF_ = {
    timeLow: 0x30927220,
    timeMid: 0xbfb0,
    timeHiAndVersion: 0x11e3,
    clockSeq: 0xb03a,
    node: { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b }
};

const effect_uuid_t *const FX_IID_BMF = &FX_IID_BMF_;

const effect_descriptor_t BmfAudioEffect::mBmfDescriptor = {
    type:         FX_IID_BMF_,
    uuid:         {
        timeLow: 0xff619c00,
        timeMid: 0xc458,
        timeHiAndVersion: 0x11e3,
        clockSeq: 0x9af1,
        node: { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b }
    },
    apiVersion:   EFFECT_CONTROL_API_VERSION,
    flags:        (EFFECT_FLAG_TYPE_PRE_PROC | EFFECT_FLAG_DEVICE_IND),
    cpuLoad:      0,
    memoryUsage:  0,
    "Beam Forming",                 /**< name. */
    "IntelLPE"                 /**< implementor. */
};
