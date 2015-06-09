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
#define LOG_TAG "Resampler"

#include "Resampler.hpp"
#include <AudioCommsAssert.hpp>
#include <utilities/Log.hpp>
#include <iasrc_resampler.h>
#include <limits.h>

using audio_comms::utilities::Log;
using namespace android;

namespace intel_audio
{

Resampler::Resampler(SampleSpecItem sampleSpecItem)
    : AudioConverter(sampleSpecItem),
      mMaxFrameCnt(0),
      mContext(NULL), mFloatInp(NULL), mFloatOut(NULL)
{
}

Resampler::~Resampler()
{
    if (mContext) {
        iaresamplib_reset(mContext);
        iaresamplib_delete(&mContext);
    }

    delete[] mFloatInp;
    delete[] mFloatOut;
}

status_t Resampler::allocateBuffer()
{
    if (mMaxFrameCnt == 0) {
        mMaxFrameCnt = mBufSize;
    } else {
        mMaxFrameCnt *= 2; // simply double the buf size
    }

    delete[] mFloatInp;
    delete[] mFloatOut;

    mFloatInp = new float[(mMaxFrameCnt + 1) * mSsSrc.getChannelCount()];
    mFloatOut = new float[(mMaxFrameCnt + 1) * mSsSrc.getChannelCount()];

    if (!mFloatInp || !mFloatOut) {
        Log::Error() << "cannot allocate resampler tmp buffers";
        delete[] mFloatInp;
        delete[] mFloatOut;

        return NO_MEMORY;
    }
    return NO_ERROR;
}

status_t Resampler::configure(const SampleSpec &ssSrc, const SampleSpec &ssDst)
{
    Log::Debug() << __FUNCTION__ << ": SOURCE rate=" << ssSrc.getSampleRate()
                 << " format=" << static_cast<int32_t>(ssSrc.getFormat())
                 << " channels=" << ssSrc.getChannelCount();
    Log::Debug() << __FUNCTION__ << ": DST rate=" << ssDst.getSampleRate()
                 << " format=" << static_cast<int32_t>(ssDst.getFormat())
                 << " channels=" << ssDst.getChannelCount();

    if ((ssSrc.getSampleRate() == mSsSrc.getSampleRate()) &&
        (ssDst.getSampleRate() == mSsDst.getSampleRate()) && mContext) {

        return NO_ERROR;
    }

    status_t status = AudioConverter::configure(ssSrc, ssDst);
    if (status != NO_ERROR) {

        return status;
    }

    if (mContext) {
        iaresamplib_reset(mContext);
        iaresamplib_delete(&mContext);
        mContext = NULL;
    }

    if (!iaresamplib_supported_conversion(ssSrc.getSampleRate(), ssDst.getSampleRate())) {
        Log::Error() << __FUNCTION__ << ": SRC lib doesn't support this conversion";
        return INVALID_OPERATION;
    }

    iaresamplib_new(&mContext, ssSrc.getChannelCount(),
                    ssSrc.getSampleRate(), ssDst.getSampleRate());
    if (!mContext) {
        Log::Error() << "cannot create resampler handle for lacking of memory";
        return BAD_VALUE;
    }

    mConvertSamplesFct = static_cast<SampleConverter>(&Resampler::resampleFrames);
    return NO_ERROR;
}

void Resampler::convertShort2Float(int16_t *inp, float *out, size_t sz) const
{
    AUDIOCOMMS_ASSERT(inp != NULL && out != NULL, "Invalid input and/or output buffer(s)");
    size_t i;
    for (i = 0; i < sz; i++) {
        *out++ = (float)*inp++;
    }
}

void Resampler::convertFloat2Short(float *inp, int16_t *out, size_t sz) const
{
    AUDIOCOMMS_ASSERT(inp != NULL && out != NULL, "Invalid input and/or output buffer(s)");
    size_t i;
    for (i = 0; i < sz; i++) {
        if (*inp > SHRT_MAX) {
            *inp = SHRT_MAX;
        } else if (*inp < SHRT_MIN) {
            *inp = SHRT_MIN;
        }
        *out++ = (short)*inp++;
    }
}

status_t Resampler::resampleFrames(const void *src,
                                   void *dst,
                                   const uint32_t inFrames,
                                   uint32_t *outFrames)
{
    size_t outFrameCount = convertSrcToDstInFrames(inFrames);

    while (outFrameCount > mMaxFrameCnt) {

        status_t ret = allocateBuffer();
        if (ret != NO_ERROR) {
            Log::Error() << __FUNCTION__ << ": could not allocate memory for resampling operation";
            return ret;
        }
    }
    unsigned int outNbFrames;
    convertShort2Float((short *)src, mFloatInp, inFrames * mSsSrc.getChannelCount());
    iaresamplib_process_float(mContext, mFloatInp, inFrames, mFloatOut, &outNbFrames);
    convertFloat2Short(mFloatOut, (short *)dst, outNbFrames * mSsSrc.getChannelCount());

    *outFrames = outNbFrames;

    return NO_ERROR;
}
}  // namespace intel_audio
