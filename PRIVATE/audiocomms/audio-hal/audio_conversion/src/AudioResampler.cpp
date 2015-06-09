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
#define LOG_TAG "AudioResampler"

#include "AudioResampler.hpp"
#include "Resampler.hpp"
#include <AudioCommsAssert.hpp>
#include <utilities/Log.hpp>

using audio_comms::utilities::Log;
using namespace android;

namespace intel_audio
{

AudioResampler::AudioResampler(SampleSpecItem sampleSpecItem)
    : AudioConverter(sampleSpecItem),
      mResampler(new Resampler(RateSampleSpecItem)),
      mPivotResampler(new Resampler(RateSampleSpecItem)),
      mActiveResamplerList()
{
}

AudioResampler::~AudioResampler()
{
    mActiveResamplerList.clear();
    delete mResampler;
    delete mPivotResampler;
}

status_t AudioResampler::configure(const SampleSpec &ssSrc, const SampleSpec &ssDst)
{
    mActiveResamplerList.clear();

    status_t status = AudioConverter::configure(ssSrc, ssDst);
    if (status != NO_ERROR) {

        return status;
    }

    status = mResampler->configure(ssSrc, ssDst);
    if (status != NO_ERROR) {

        //
        // Our resampling lib does not support all conversions
        // using 2 resamplers
        //
        Log::Debug() << __FUNCTION__ << ": trying to use working sample rate @ 48kHz";
        SampleSpec pivotSs = ssDst;
        pivotSs.setSampleRate(mPivotSampleRate);

        status = mPivotResampler->configure(ssSrc, pivotSs);
        if (status != NO_ERROR) {
            Log::Debug() << __FUNCTION__ << ": trying to use pivot sample rate @"
                         << mPivotSampleRate << "kHz: FAILED";
            return status;
        }

        mActiveResamplerList.push_back(mPivotResampler);

        status = mResampler->configure(pivotSs, ssDst);
        if (status != NO_ERROR) {
            Log::Debug() << __FUNCTION__ << ": trying to use pivot sample rate @ 48kHz: FAILED";
            return status;
        }
    }

    mActiveResamplerList.push_back(mResampler);

    return NO_ERROR;
}

status_t AudioResampler::convert(const void *src,
                                 void **dst,
                                 uint32_t inFrames,
                                 uint32_t *outFrames)
{
    AUDIOCOMMS_ASSERT(src != NULL, "NULL source buffer");
    const void *srcBuf = static_cast<const void *>(src);
    void *dstBuf = NULL;
    size_t srcFrames = inFrames;
    size_t dstFrames = 0;

    ResamplerListIterator it;
    for (it = mActiveResamplerList.begin(); it != mActiveResamplerList.end(); ++it) {

        Resampler *conv = *it;
        dstFrames = 0;

        if (*dst && (conv == mActiveResamplerList.back())) {

            // Last converter must output within the provided buffer (if provided!!!)
            dstBuf = *dst;
        }

        status_t status = conv->convert(srcBuf, &dstBuf, srcFrames, &dstFrames);
        if (status != NO_ERROR) {

            return status;
        }

        srcBuf = dstBuf;
        srcFrames = dstFrames;
    }

    *dst = dstBuf;
    *outFrames = dstFrames;

    return NO_ERROR;
}
}  // namespace intel_audio
