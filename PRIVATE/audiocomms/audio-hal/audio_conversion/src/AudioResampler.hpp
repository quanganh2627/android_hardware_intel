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
#include <list>

namespace intel_audio
{

class Resampler;

class AudioResampler : public AudioConverter
{

private:
    /**
     * Iterator on resamplers list.
     */
    typedef std::list<Resampler *>::iterator ResamplerListIterator;

public:
    /**
     * Class constructor.
     *
     * @param[in] Reference sample specification.
     */
    AudioResampler(SampleSpecItem sampleSpecItem);

    virtual ~AudioResampler();

private:
    /**
     * Configures the resampling operations.
     *
     * Sets the resampler(s) to use, based on destination sample spec, on
     * source sample spec as well as supported resampling operations.
     *
     * @param[in] ssSrc source sample specification.
     * @param[in] ssDst destination sample specification.
     *
     * @return status NO_ERROR, error code otherwise.
     */
    virtual android::status_t configure(const SampleSpec &ssSrc, const SampleSpec &ssDst);

    /**
     * Resamples audio samples.
     *
     * It resamples audio samples using the resamplers list set when configure was called.
     *
     * @param[in] src  buffer of samples to resample.
     * @param[out] dst destination samples buffer. If no error is returned, the ouput buffer
     *                 will contain valid data until next convert call or configure.
     *
     * @param[in] inFrames number of frames in the source sample specification to convert.
     * @param[out] outFrames number of frames in the destination sample specification converted.
     *
     * @return status NO_ERROR, error code otherwise.
     */
    virtual android::status_t convert(const void *src,
                                      void **dst,
                                      uint32_t inFrames,
                                      uint32_t *outFrames);

    /**
     * Resampler to use for all conversions.
     */
    Resampler *mResampler;

    /**
     * Resampler to use when desired conversion is not supported.
     * It defaults to 48Khz conversion.
     */
    Resampler *mPivotResampler;

    /**
     * List of enabled audio converters.
     */
    std::list<Resampler *> mActiveResamplerList;

    /**
     * Reference sample rate.
     */
    static const uint32_t mPivotSampleRate = 48000;
};
}  // namespace intel_audio
