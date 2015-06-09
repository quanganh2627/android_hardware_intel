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

#include "AudioConverter.hpp"

namespace intel_audio
{

class Resampler : public AudioConverter
{

public:
    Resampler(SampleSpecItem sampleSpecItem);

    virtual ~Resampler();

    /**
     * Resamples buffer from source to destination sample rate.
     * Resamples input frames of the provided input buffer into the destination buffer already
     * allocated by the converter or given by the client.
     * Before using this function, configure must have been called.
     *
     * @param[in] src the source buffer.
     * @param[out] dst the destination buffer, caller to ensure the destination
     *             is large enough.
     * @param[in] inFrames number of input frames.
     * @param[out] outFrames output frames processed.
     *
     * @return error code.
     */
    android::status_t resampleFrames(const void *src,
                                     void *dst,
                                     const uint32_t inFrames,
                                     uint32_t *outFrames);

    /**
     * Configures the resampler.
     * It configures the resampler that may be used to convert samples from the source
     * to destination sample rate.
     *
     * @param[in] ssSrc source sample specifications.
     * @param[in] ssDst destination sample specification.
     *
     * @return status OK, error code otherwise.
     */
    virtual android::status_t configure(const SampleSpec &ssSrc, const SampleSpec &ssDst);

private:
    android::status_t allocateBuffer();

    /**
     * converts a buffer of S16 to float.
     *
     * @param[in] inp input S16 buffer to convert.
     * @param[out] out output float buffer.
     * @param[in] sz size of input buffer.
     */
    void convertShort2Float(int16_t *inp, float *out, size_t sz) const;

    /**
     * converts a buffer of float to S16.
     *
     * @param[in] inp input float buffer to convert.
     * @param[out] out output S16 buffer.
     * @param[in] sz size of input buffer.
     */
    void convertFloat2Short(float *inp, int16_t *out, size_t sz) const;

    static const int mBufSize = 4608; /**< default buffer is 24 ms @ 48kHz on S16LE samples. */
    size_t mMaxFrameCnt;  /* max frame count the buffer can store */
    void *mContext;      /* handle used to do resample */
    float *mFloatInp;     /* here sample size is 4 bytes */
    float *mFloatOut;     /* here sample size is 4 bytes */
};
}  // namespace intel_audio
