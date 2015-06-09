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

#include <SampleSpec.hpp>
#include <NonCopyable.hpp>
#include <utils/Errors.h>

using audio_comms::utilities::NonCopyable;

namespace intel_audio
{

class AudioConverter : public NonCopyable
{

public:

    /**
     * Conversion function pointer definition.
     *
     * @param[in] src the source buffer.
     * @param[out] dst the destination buffer.
     *                  Note that if the memory is allocated by the converted,
     *                  it is freed upon next call of configure or upon deletion
     *                  of the converter.
     * @param[in] inFrames number of input frames.
     * @param[out] outFrames output frames processed.
     *
     * @return status OK if conversion is successful, error code otherwise.
     */
    typedef android::status_t (AudioConverter::*SampleConverter)(const void *src,
                                                                 void *dst,
                                                                 uint32_t inFrames,
                                                                 uint32_t *outFrames);

    AudioConverter(SampleSpecItem sampleSpecItem);

    virtual ~AudioConverter();

    /**
     * Configures the converters.
     *
     * It configures the converter that may be used to convert one sample spec item from the source
     * to destination sample spec item.
     *
     * @param[in] ssSrc source sample specifications.
     * @param[in] ssDst destination sample specification.
     *
     * @return status OK if converter is configured properly, error code otherwise.
     */
    virtual android::status_t configure(const SampleSpec &ssSrc, const SampleSpec &ssDst);

    /**
     * Converts buffer from source to destination sample spec item.
     *
     * Converts input frames of the provided input buffer into the destination buffer that may be
     * allocated by the client. If not, the converter will allocate the output and give it back
     * to the client.
     * Before using this function, configure must have been called.
     *
     * @param[in] src the source buffer.
     * @param[out] dst the destination buffer.
     *                  Note that if the memory is allocated by the converted,
     *                  it is freed upon next call of configure or upon deletion of the converter.
     * @param[in] inFrames number of input frames.
     * @param[out] outFrames output frames processed.
     *
     * @return status OK if convertion is successful, error code otherwise.
     */
    virtual android::status_t convert(const void *src,
                                      void **dst,
                                      uint32_t inFrames,
                                      uint32_t *outFrames);

protected:
    /**
     * Converts the number of frames in the destination sample spec in a number of frames in the
     * source sample spec.
     *
     * @param[in] frames in the destination sample spec.
     *
     * @return frames in the source sample spec.
     */
    size_t convertSrcFromDstInFrames(ssize_t frames) const;

    /**
     * Converts the number of frames in the source sample spec in a number of frames in the
     * destination sample spec.
     *
     * @param[in] frames in the source sample spec.
     *
     * @return frames in the destination sample spec.
     */
    size_t convertSrcToDstInFrames(ssize_t frames) const;

    SampleConverter mConvertSamplesFct;

    /**
     * Source audio data sample specifications.
     */
    SampleSpec mSsSrc;

    /**
     * Destination audio data sample specifications.
     */
    SampleSpec mSsDst;

private:
    /**
     * Returns a suitable output buffer.
     *
     * The buffer is used to safely execute conversion operations.
     */
    void *getOutputBuffer(ssize_t inFrames);

    /**
     * Allocate internal memory for the conversion operation.
     *
     * @param[in] bytes memory required to output the converted samples.
     *
     * @return OK if allocation is successful, error code otherwise.
     */
    android::status_t allocateConvertBuffer(ssize_t bytes);

    char *mConvertBuf; /**< Internal memory for destination samples. */
    size_t mConvertBufSize; /**< Size of the internal memory allocated. */

    SampleSpecItem mSampleSpecItem; /**< Sample spec item on which the converter is working. */
};
}  // namespace intel_audio
