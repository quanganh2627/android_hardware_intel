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


#include <hardware/audio.h>
#include <tinyalsa/asoundlib.h>
#include <string.h>
#include <vector>

namespace intel_audio
{

//
// Do not change the order: convertion steps will be ordered to make
// the computing as light as possible, it will works successively
// on the channels, then the format and the rate
//
enum SampleSpecItem
{
    ChannelCountSampleSpecItem = 0,
    FormatSampleSpecItem,
    RateSampleSpecItem,

    NbSampleSpecItems
};


class SampleSpec
{

public:
    /**
     * Checks equality of SampleSpec instances.
     *
     * @param[in] right member to compare with left (i.e. *this).
     *
     * @return true if not only channels, rate and format are equal, but also channel policy.
     */
    bool operator==(const SampleSpec &right) const
    {

        return !memcmp(mSampleSpec, right.mSampleSpec, sizeof(mSampleSpec)) &&
               (mChannelsPolicy == right.mChannelsPolicy);
    }

    /**
     * Checks not equality of SampleSpec instances.
     *
     * @param[in] right member to compare with left (i.e. *this).
     *
     * @return true if either at least channels, rate, format or channel policy differs.
     */
    bool operator!=(const SampleSpec &right) const
    {

        return !operator==(right);
    }

    /**
     * Channel policy definition.
     * The channel policy will be usefull in case of remap operation.
     * From this definition, the remapper must be able to infer conversion table.
     *
     * For example: on some stereo devices, a channel might be empty/invalid.
     * So, the other channel will be tagged as "average".
     *
     *      SOURCE              DESTINATION
     *  channel 1 (copy) ---> channel 1 (average) = (source channel 1 + source channel 2) / 2
     *  channel 2 (copy) ---> channel 2 (ignore)  = empty
     *
     */
    enum ChannelsPolicy
    {
        Copy = 0,      /**< This channel is valid. */
        Average,       /**< This channel contains all valid audio data. */
        Ignore,        /**< This channel does not contains any valid audio data. */

        NbChannelsPolicy
    };

    SampleSpec(uint32_t channel = mDefaultChannels,
               uint32_t format = mDefaultFormat,
               uint32_t rate = mDefaultRate);

    SampleSpec(uint32_t channel,
               uint32_t format,
               uint32_t rate,
               const std::vector<ChannelsPolicy> &channelsPolicy);

    // Specific Accessors
    void setChannelCount(uint32_t channelCount)
    {

        setSampleSpecItem(ChannelCountSampleSpecItem, channelCount);
    }
    uint32_t getChannelCount() const
    {
        return getSampleSpecItem(ChannelCountSampleSpecItem);
    }
    void setSampleRate(uint32_t sampleRate)
    {

        setSampleSpecItem(RateSampleSpecItem, sampleRate);
    }
    uint32_t getSampleRate() const
    {
        return getSampleSpecItem(RateSampleSpecItem);
    }
    void setFormat(uint32_t format)
    {
        setSampleSpecItem(FormatSampleSpecItem, format);
    }
    audio_format_t getFormat() const
    {
        return static_cast<audio_format_t>(getSampleSpecItem(FormatSampleSpecItem));
    }
    void setChannelMask(audio_channel_mask_t channelMask)
    {
        mChannelMask = channelMask;
    }
    audio_channel_mask_t getChannelMask() const
    {
        return mChannelMask;
    }

    void setChannelsPolicy(const std::vector<ChannelsPolicy> &channelsPolicy);
    const std::vector<ChannelsPolicy> &getChannelsPolicy() const
    {
        return mChannelsPolicy;
    }
    ChannelsPolicy getChannelsPolicy(uint32_t channelIndex) const;

    // Generic Accessor
    void setSampleSpecItem(SampleSpecItem sampleSpecItem, uint32_t value);

    uint32_t getSampleSpecItem(SampleSpecItem sampleSpecItem) const;

    size_t getFrameSize() const;

    /**
     * Converts the bytes number to frames number.
     * It converts a bytes number into a frame number it represents in this sample spec instance.
     * It asserts if the frame size is null.
     *
     * @param[in] number of bytes to be translated in frames.
     *
     * @return frames number in the sample spec given in param for this instance.
     */
    size_t convertBytesToFrames(size_t bytes) const;

    /**
     * Converts the frames number to bytes number.
     * It converts the frame number (independant of the channel number and format number) into
     * a byte number (sample spec dependant). In case of overflow, this function will assert.
     *
     * @param[in] number of frames to be translated in bytes.
     *
     * @return bytes number in the sample spec given in param for this instance
     */
    size_t convertFramesToBytes(size_t frames) const;

    /**
     * Translates the frame number into time.
     * It converts the frame number into the time it represents in this sample spec instance.
     * It may assert if overflow is detected.
     *
     * @param[in] number of frames to be translated in time.
     *
     * @return time in microseconds.
     */
    size_t convertFramesToUsec(uint32_t frames) const;

    /**
     * Translates a time period into a frame number.
     * It converts a period of time into a frame number it represents in this sample spec instance.
     *
     * @param[in] time interval in micro second to be translated.
     *
     * @return number of frames corresponding to the period of time.
     */
    size_t convertUsecToframes(uint32_t intervalUsec) const;

    bool isMono() const
    {
        return mSampleSpec[ChannelCountSampleSpecItem] == 1;
    }

    bool isStereo() const
    {
        return mSampleSpec[ChannelCountSampleSpecItem] == 2;
    }

    /**
     * Checks upon equality of a sample spec item.
     *  For channels, it checks:
     *          -not only that channels count is equal
     *          -but also the channels policy of source and destination is the same.
     * @param[in] sampleSpecItem item to checks.
     * @param[in] ssSrc source sample specifications.
     * @param[in] ssDst destination sample specifications.
     *
     * @return true upon equality, false otherwise.
     */
    static bool isSampleSpecItemEqual(SampleSpecItem sampleSpecItem,
                                      const SampleSpec &ssSrc,
                                      const SampleSpec &ssDst);

private:
    /**
     * Initialise the sample specifications.
     * Parts of the private constructor. It sets the basic fields, reset the channel mask to 0,
     * and sets the channel policy to "Copy" for each of the channels used.
     *
     * @param[in] channel number of channels.
     * @param[in] format sample format, eg 16 or 24 bits(coded on 32 bits).
     * @param[in] rate sample rate.
     */
    void init(uint32_t channel, uint32_t format, uint32_t rate);

    uint32_t mSampleSpec[NbSampleSpecItems]; /**< Array of sample spec items:
                                              *         -channel number
                                              *         -format
                                              *         -sample rate. */

    audio_channel_mask_t mChannelMask; /**< Bit field that defines the channels used. */

    std::vector<ChannelsPolicy> mChannelsPolicy; /**< channels policy array. */

    static const uint32_t mUsecPerSec = 1000000; /**<  to convert sec to-from microseconds. */
    static const uint32_t mDefaultChannels = 2; /**< default channel used is stereo. */
    static const uint32_t mDefaultFormat = AUDIO_FORMAT_PCM_16_BIT; /**< default format is 16bits.*/
    static const uint32_t mDefaultRate = 48000; /**< default rate is 48 kHz. */
    static const uint32_t mMaxChannels = 32; /**< supports until 32 channels. */
};

} // namespace intel_audio
