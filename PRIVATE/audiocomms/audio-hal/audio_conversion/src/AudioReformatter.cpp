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
#define LOG_TAG "AudioReformatter"

#include "AudioReformatter.hpp"
#include <utilities/Log.hpp>

using audio_comms::utilities::Log;
using namespace android;

namespace intel_audio
{

const uint32_t AudioReformatter::mReformatterShiftLeft16 = 16;
const uint32_t AudioReformatter::mReformatterShiftRight8 = 8;

AudioReformatter::AudioReformatter(SampleSpecItem sampleSpecItem)
    : AudioConverter(sampleSpecItem)
{
}

status_t AudioReformatter::configure(const SampleSpec &ssSrc, const SampleSpec &ssDst)
{
    status_t status = AudioConverter::configure(ssSrc, ssDst);
    if (status != NO_ERROR) {

        return status;
    }

    if ((ssSrc.getFormat() == AUDIO_FORMAT_PCM_16_BIT) &&
        (ssDst.getFormat() == AUDIO_FORMAT_PCM_8_24_BIT)) {

        mConvertSamplesFct =
            static_cast<SampleConverter>(&AudioReformatter::convertS16toS24over32);
    } else if ((ssSrc.getFormat() == AUDIO_FORMAT_PCM_8_24_BIT) &&
               (ssDst.getFormat() == AUDIO_FORMAT_PCM_16_BIT)) {

        mConvertSamplesFct =
            static_cast<SampleConverter>(&AudioReformatter::convertS24over32toS16);
    } else {
        Log::Error() << __FUNCTION__ << ": reformatter not available";
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

status_t AudioReformatter::convertS16toS24over32(const void *src,
                                                 void *dst,
                                                 const uint32_t inFrames,
                                                 uint32_t *outFrames)
{
    uint32_t frameIndex;
    const int16_t *src16 = (const int16_t *)src;
    uint32_t *dst32 = (uint32_t *)dst;
    size_t n = inFrames * mSsSrc.getChannelCount();

    for (frameIndex = 0; frameIndex < n; frameIndex++) {

        *(dst32 + frameIndex) = (uint32_t)((int32_t)*(src16 + frameIndex) <<
                                           mReformatterShiftLeft16) >> mReformatterShiftRight8;
    }

    // Transformation is "iso" frames
    *outFrames = inFrames;

    return NO_ERROR;
}

status_t AudioReformatter::convertS24over32toS16(const void *src,
                                                 void *dst,
                                                 const uint32_t inFrames,
                                                 uint32_t *outFrames)
{
    const uint32_t *src32 = (const uint32_t *)src;
    int16_t *dst16 = (int16_t *)dst;
    uint32_t frameIndex;

    size_t n = inFrames * mSsSrc.getChannelCount();

    for (frameIndex = 0; frameIndex < n; frameIndex++) {

        *(dst16 + frameIndex) = (int16_t)(((int32_t)(*(src32 + frameIndex)) <<
                                           mReformatterShiftRight8) >> mReformatterShiftLeft16);
    }

    // Transformation is "iso" frames
    *outFrames = inFrames;

    return NO_ERROR;
}
}  // namespace intel_audio
