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
#include <media/AudioBufferProvider.h>
#include <NonCopyable.hpp>
#include <list>

namespace intel_audio
{

class AudioConverter;

class AudioConversion : public audio_comms::utilities::NonCopyable
{
public:
    typedef std::list<AudioConverter *>::iterator AudioConverterListIterator;
    typedef std::list<AudioConverter *>::const_iterator AudioConverterListConstIterator;

    AudioConversion();
    virtual ~AudioConversion();

    /**
     * Configures the conversion chain.
     *
     * It configures the conversion chain that may be used to convert samples from the source
     * to destination sample specification. This configuration tries to order the list
     * of converters so that it minimizes the number of samples on which the resampling is done.
     * To optimize the convertion and make the processing as light as possible, the
     * order of converter is important.
     *
     * This function will call the recursive function configureAndAddConverter starting
     * from the remapper operation (i.e. the converter working on the number of channels),
     * then the reformatter operation (i.e. converter changing the format of the samples),
     * and finally the resampler (i.e. converter changing the sample rate).
     *
     * @param[in] ssSrc source sample specifications.
     * @param[in] ssDst destination sample specifications.
     *
     * @return status OK, error code otherwise.
     */
    android::status_t configure(const SampleSpec &ssSrc, const SampleSpec &ssDst);

    /**
     * Converts audio samples.
     *
     * It converts audio samples using the conversion chain that must be configured before.
     * Destination buffer may be given or not to minimize the number of copy. If not given,
     * allocation is done by the resampler. In this case, the ouput buffer will contain valid data
     * until next convert call or configure.
     *
     * @param[in] src buffer of samples to conversion.
     * @param[out] dst destination sample buffer. If the value pointed to by dst
     *                 is null, the converter will allocated memory and give it back to the caller.
     *                 This memory will be freed on next configure call or on destruction of the
     *                 instance of the converter.
     *                 If no error is returned, the ouput buffer will contain valid data until next
     *                 convert call or configure.
     * @param[in] inFrames number of frames in the source sample specification to convert.
     * @param[out] outFrames number of frames in the destination sample specification converted.
     *
     * @return status OK, error code otherwise.
     */
    android::status_t convert(const void *src,
                              void **dst,
                              const uint32_t inFrames,
                              uint32_t *outFrames);

    /**
     * Converts audio samples and output an exact number of output frames.
     *
     * The caller must give an AudioBufferProvider object that may implement getNextBuffer API
     * to feed the conversion chain.
     * The caller must allocate itself the destination buffer and guarantee overflow
     * will not happen.
     *
     * @param[out] dst pointer on the caller destination buffer.
     * @param[in] outFrames frames in the destination sample specification requested
     *            to be outputted.
     * @param[in:out] bufferProvider object that will provide source buffer.
     *
     * @return status OK, error code otherwise.
     */
    android::status_t getConvertedBuffer(void *dst,
                                         const uint32_t outFrames,
                                         android::AudioBufferProvider *bufferProvider);

private:
    /**
     * This function pushes the converter to the list.
     * and alters the source sample spec according to the sample spec reached
     * after this convertion.
     *
     * Lets take an example:
     * ssSrc = { a, b, c } and ssDst = { a', b', c' } where:
     *              -a is the channel numbers,
     *              -b is the number of bytes used in the audio format.
     *              -c is the rate,
     *
     * Let s' take the assumption that our converter (SampleSpecItem input parameter) is a
     * resampler i.e. works on sample spec item b.
     * After the converter, temporary destination sample spec will be: { a, b', c }
     *
     * Update the source Sample Spec to this temporary sample spec for the
     * next convertion that might have to be added.
     * ssSrc = temp dest = { a, b', c }.
     *
     * @param[in] sampleSpecItem sample spec item on which the converter is working.
     * @param[in:out] ssSrc source sample specifications.
     * @param[in] ssDst destination sample specifications.
     *
     * @return status OK, error code otherwise.
     */
    android::status_t doConfigureAndAddConverter(SampleSpecItem sampleSpecItem,
                                                 SampleSpec *ssSrc,
                                                 const SampleSpec *ssDst);

    /**
     * Recursive function to add converters to the chain of convertion required.
     *
     * When a converter is added, the source sample specification is modified to represent the
     * audio data sample specification AFTER applying this converter.
     * This sample spec will be used as the source for next convertion.
     * In order to minimize power consumption, resampling operation should be applied
     * on the minimum frame size. So, down-remapping or down-formatting will be done in
     * prior of resampling.
     *
     * Let's take an example:
     * ssSrc = { a, b, c } and ssDst = { a', b', c' } where:
     *              -a / a' are the channel numbers,
     *              -b / b' are the number of bytes used in the audio format.
     *              -c / c' are the rates,
     * and with (a' > a) and (b' < b)
     * As all sample spec items are different, we need to use 3 converter to reach the destination
     * audio data sample specifications.
     *
     * First take into account a (number of channels):
     *      As a' is higher than a, first performs the remapping:
     *      ssSrc = { a, b, c } dst = { a', b', c' }
     *      The temporary output becomes the new source for next converter
     *      ssSrc = temporary Output = { a', b, c }
     *
     * Then, take into account b (format size).
     *      As b' is lower than b, do not perform the reformating now...
     *
     * Finally, take into account the sample rate:
     *      as they are different, use a resampler:
     *      ssSrc = { a', b, c } dst = { a', b', c' }
     *      The temporary output becomes the new source for next converter
     *      ssSrc = temporary Output = { a', b, c' }
     *
     * No more converter: exit from last recursive call
     * Taking into account b again...(format size)
     *      as b' < b, use a reformatter
     *      ssSrc = { a', b, c' } dst = { a', b', c' }
     *      The temporary output becomes the new source for next converter
     *      ssSrc = temporary Output = { a', b', c' }
     *
     * Exit from recursive call.
     *
     * @param[in] sampleSpecItem sample spec item on which the converter is working.
     * @param[in:out] ssSrc source sample specifications.
     * @param[in] ssDst destination sample specifications.
     *
     * @return status OK, error code otherwise.
     */
    android::status_t configureAndAddConverter(SampleSpecItem sampleSpecItem,
                                               SampleSpec *ssSrc,
                                               const SampleSpec *ssDst);

    /**
     * Reset the list of active converter.
     * This function must be called before reconfiguring the conversion chain.
     */
    void emptyConversionChain();

    /**
     * List of audio converter enabled.
     */
    std::list<AudioConverter *> mActiveAudioConvList;

    /**
     * List of Audio Converter objects available.
     * Each converter works on a dedicated sample spec item.
     */
    AudioConverter *mAudioConverter[NbSampleSpecItems];

    /**
     * Source audio data sample specifications.
     */
    SampleSpec mSsSrc;

    /**
     * Destination audio data sample specifications.
     */
    SampleSpec mSsDst;

    // Conversion is done into ConvOutBuffer
    size_t mConvOutBufferIndex; /**< Current position into the Converted buffer. */
    size_t mConvOutFrames; /**< Number of converted Frames. */
    size_t mConvOutBufferSizeInFrames; /**< Converted buffer size in Frames. */
    int16_t *mConvOutBuffer; /**< Converted buffer. */

    /**
     * Buffer is acquired from the provider into ConvInBuffer.
     */
    android::AudioBufferProvider::Buffer mConvInBuffer;

    static const uint32_t mMaxRate; /**< Max rate supported by resampler converter. */

    static const uint32_t mMinRate; /**< Min rate supported by resampler converter. */

    /**
     * Multiplication factor used to allocate a big enough conversion buffer.
     */
    static const uint32_t mAllocBufferMultFactor;
};
}  // namespace intel_audio
