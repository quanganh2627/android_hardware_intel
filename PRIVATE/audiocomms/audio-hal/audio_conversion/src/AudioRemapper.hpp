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

class AudioRemapper : public AudioConverter
{
private:
    enum Channel
    {

        Left = 0,
        Right
    };

public:
    /**
     * Constructor of the remapper.
     *
     * @param[in] sampleSpecItem Sample specification item on which this audio
     *            converter is working on.
     */
    AudioRemapper(SampleSpecItem sampleSpecItem);

private:
    /**
     * Configures the remapper.
     *
     * Selects the appropriate remap operation to use according to the source
     * and destination sample specifications.
     *
     * @param[in] ssSrc the source sample specifications.
     * @param[in] ssDst the destination sample specifications.
     *
     * @return error code.
     */
    virtual android::status_t configure(const SampleSpec &ssSrc, const SampleSpec &ssDst);

    /**
     * Configure the remapper.
     *
     * Selects the appropriate remap operation to use according to the source
     * and destination sample specifications.
     *
     * @tparam type Audio data format from S16 to S32.
     *
     * @return error code.
     */
    template <typename type>
    android::status_t configure();

    /**
     * Remap from stereo to mono in typed format.
     *
     * Convert a stereo source into a mono destination in typed format.
     *
     * @tparam type Audio data format from S16 to S32, no other type allowed.
     * @param[in] src the source buffer.
     * @param[out] dst the destination buffer, the caller must ensure the destination
     *             is large enough.
     * @param[in] inFrames number of input frames.
     * @param[out] outFrames output frames processed.
     *
     * @return error code.
     */
    template <typename type>
    android::status_t convertStereoToMono(const void *src,
                                          void *dst,
                                          const uint32_t inFrames,
                                          uint32_t *outFrames);

    /**
     * Remap from mono to stereo in typed format.
     *
     * Convert a mono source into a stereo destination in typed format.
     *
     * @tparam type Audio data format from S16 to S32, no other type allowed.
     * @param[in] src the source buffer.
     * @param[out] dst the destination buffer, the caller must ensure the destination
     *             is large enough.
     * @param[in] inFrames number of input frames.
     * @param[out] outFrames output frames processed.
     *
     * @return error code.
     */
    template <typename type>
    android::status_t convertMonoToStereo(const void *src,
                                          void *dst,
                                          const uint32_t inFrames,
                                          uint32_t *outFrames);

    /**
     * Remap channels policy in typed format.
     *
     * Convert a stereo source into a stereo destination in typed format
     * with different channels policy.
     *
     * @tparam type Audio data format from S16 to S32, no other type allowed.
     * @param[in] src the source buffer.
     * @param[out] dst the destination buffer, the caller must ensure the destination
     *             is large enough.
     * @param[in] inFrames number of input frames.
     * @param[out] outFrames output frames processed.
     *
     * @return error code.
     */
    template <typename type>
    android::status_t convertChannelsPolicyInStereo(const void *src,
                                                    void *dst,
                                                    const uint32_t inFrames,
                                                    uint32_t *outFrames);

    /**
     * Convert a source sample in typed format.
     *
     * Gets destination channel from the source sample according to the destination
     * channel policy.
     *
     * @tparam type Audio data format from S16 to S32, no other type allowed.
     * @param[in] src16 the source frame.
     * @param[in] channel the channel of the destination.
     *
     * @return destination channel sample.
     */
    template <typename type>
    type convertSample(const type *src, Channel channel) const;

    /**
     * Average source frame in typed format.
     *
     * Gets an averaged value of the source audio frame taking into
     * account the policy of the source channels.
     *
     * @tparam type Audio data format from S16 to S32, no other type allowed.
     * @param[in] src16 the source frame.
     *
     * @return destination channel sample.
     */
    template <typename type>
    type getAveragedSrcFrame(const type *src16) const;

    /**
     * provide a compile time error if no specialization is provided for a given type.
     *
     * @tparam T: type of the audio data.
     */
    template <typename T>
    struct formatSupported;
};
}  // namespace intel_audio
