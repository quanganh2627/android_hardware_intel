/*
 * INTEL CONFIDENTIAL
 * Copyright (c) 2012-2014 Intel
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

#include "SampleSpecTest.hpp"
#include <SampleSpec.hpp>

#include <hardware/audio.h>
#include <limits.h>
#include <limits>
#include <signal.h>
#include <errno.h>
#include <gtest/gtest.h>

using ::testing::Test;

namespace intel_audio
{

TEST(SampleSpec, setGet)
{
    // Test constructor
    SampleSpec sampleSpec;

    // Test setChannelCount
    sampleSpec.setChannelCount(1);
    EXPECT_EQ(1u, sampleSpec.getChannelCount());
    sampleSpec.setChannelCount(2);
    EXPECT_EQ(2u, sampleSpec.getChannelCount());


    // Test setSampleRate
    sampleSpec.setSampleRate(48000);
    EXPECT_EQ(48000u, sampleSpec.getSampleRate());
    sampleSpec.setSampleRate(0);
    EXPECT_EQ(0u, sampleSpec.getSampleRate());
    sampleSpec.setSampleRate(std::numeric_limits<uint32_t>::max());
    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), sampleSpec.getSampleRate());

    // Test setFormat
    sampleSpec.setFormat(AUDIO_FORMAT_PCM_16_BIT);
    EXPECT_EQ(AUDIO_FORMAT_PCM_16_BIT, sampleSpec.getFormat());
    sampleSpec.setFormat(AUDIO_FORMAT_PCM_8_24_BIT);
    EXPECT_EQ(AUDIO_FORMAT_PCM_8_24_BIT, sampleSpec.getFormat());

    // Channel mask
    sampleSpec.setChannelMask(AUDIO_CHANNEL_OUT_ALL);
    EXPECT_EQ(AUDIO_CHANNEL_OUT_ALL, sampleSpec.getChannelMask());
    sampleSpec.setChannelMask(AUDIO_CHANNEL_OUT_5POINT1);
    EXPECT_EQ(AUDIO_CHANNEL_OUT_5POINT1, sampleSpec.getChannelMask());

    // Test setSampleSpecItem
    sampleSpec.setSampleSpecItem(ChannelCountSampleSpecItem, 16);
    EXPECT_EQ(16u, sampleSpec.getSampleSpecItem(ChannelCountSampleSpecItem));
    sampleSpec.setSampleSpecItem(RateSampleSpecItem, 12000);
    EXPECT_EQ(12000u, sampleSpec.getSampleSpecItem(RateSampleSpecItem));
    sampleSpec.setSampleSpecItem(FormatSampleSpecItem, AUDIO_FORMAT_PCM_8_24_BIT);
    EXPECT_EQ(AUDIO_FORMAT_PCM_8_24_BIT,
              static_cast<audio_format_t>(sampleSpec.getSampleSpecItem(FormatSampleSpecItem)));
}

TEST(SampleSpec, setGetChannelPolicy)
{
    SampleSpec sampleSpec;
    sampleSpec.setSampleSpecItem(ChannelCountSampleSpecItem, 4);
    std::vector<SampleSpec::ChannelsPolicy> channelsPolicy;
    channelsPolicy.push_back(SampleSpec::Average);
    channelsPolicy.push_back(SampleSpec::Copy);
    channelsPolicy.push_back(SampleSpec::Ignore);
    channelsPolicy.push_back(SampleSpec::Average);
    sampleSpec.setChannelsPolicy(channelsPolicy);
    EXPECT_EQ(SampleSpec::Average, sampleSpec.getChannelsPolicy(0));
    EXPECT_EQ(SampleSpec::Copy, sampleSpec.getChannelsPolicy(1));
    EXPECT_EQ(SampleSpec::Ignore, sampleSpec.getChannelsPolicy(2));
    EXPECT_EQ(SampleSpec::Average, sampleSpec.getChannelsPolicy(3));

    EXPECT_EQ(channelsPolicy, sampleSpec.getChannelsPolicy());

}

TEST(SampleSpec, monoStereoHelpers)
{
    SampleSpec sampleSpec;
    sampleSpec.setSampleSpecItem(ChannelCountSampleSpecItem, 1);
    EXPECT_TRUE(sampleSpec.isMono());
    sampleSpec.setSampleSpecItem(ChannelCountSampleSpecItem, 2);
    EXPECT_FALSE(sampleSpec.isMono());
    EXPECT_TRUE(sampleSpec.isStereo());
    sampleSpec.setSampleSpecItem(ChannelCountSampleSpecItem, 3);
    EXPECT_FALSE(sampleSpec.isMono());
    EXPECT_FALSE(sampleSpec.isStereo());
}

TEST(SampleSpec, comparatorHelpers)
{
    SampleSpec sampleSpecSrc(2, AUDIO_FORMAT_PCM_16_BIT, 44100);
    SampleSpec sampleSpecDst(2, AUDIO_FORMAT_PCM_16_BIT, 44100);

    EXPECT_TRUE(sampleSpecSrc == sampleSpecDst);
    EXPECT_FALSE(sampleSpecSrc != sampleSpecDst);

    sampleSpecDst.setFormat(AUDIO_FORMAT_PCM_8_24_BIT);
    EXPECT_TRUE(SampleSpec::isSampleSpecItemEqual(ChannelCountSampleSpecItem,
                                                  sampleSpecSrc,
                                                  sampleSpecDst));
    EXPECT_FALSE(SampleSpec::isSampleSpecItemEqual(FormatSampleSpecItem,
                                                   sampleSpecSrc,
                                                   sampleSpecDst));
    EXPECT_TRUE(SampleSpec::isSampleSpecItemEqual(RateSampleSpecItem,
                                                  sampleSpecSrc,
                                                  sampleSpecDst));
    EXPECT_TRUE(sampleSpecSrc != sampleSpecDst);
    EXPECT_FALSE(sampleSpecSrc == sampleSpecDst);

    sampleSpecDst.setFormat(AUDIO_FORMAT_PCM_16_BIT);
    EXPECT_TRUE(sampleSpecSrc == sampleSpecDst);

    std::vector<SampleSpec::ChannelsPolicy> channelsPolicy;
    channelsPolicy.push_back(SampleSpec::Average);
    channelsPolicy.push_back(SampleSpec::Copy);
    sampleSpecDst.setChannelsPolicy(channelsPolicy);

    EXPECT_FALSE(sampleSpecSrc == sampleSpecDst);
    EXPECT_TRUE(sampleSpecSrc != sampleSpecDst);
    EXPECT_FALSE(SampleSpec::isSampleSpecItemEqual(ChannelCountSampleSpecItem,
                                                   sampleSpecSrc,
                                                   sampleSpecDst));
}



TEST(SampleSpec, equalOperator)
{
    SampleSpec sampleSpec1;
    sampleSpec1.setChannelCount(1);
    sampleSpec1.setFormat(AUDIO_FORMAT_PCM_16_BIT);
    sampleSpec1.setSampleRate(48000);
    sampleSpec1.setChannelMask(1234);

    SampleSpec sampleSpec2;
    sampleSpec2.setChannelCount(1);
    sampleSpec2.setFormat(AUDIO_FORMAT_PCM_16_BIT);
    sampleSpec2.setSampleRate(48000);
    sampleSpec2.setChannelMask(9876);

    SampleSpec sampleSpec3;
    sampleSpec3.setChannelCount(2);
    sampleSpec3.setFormat(AUDIO_FORMAT_PCM_16_BIT);
    sampleSpec3.setSampleRate(48000);
    sampleSpec3.setChannelMask(1234);

    SampleSpec sampleSpec4;
    sampleSpec4.setChannelCount(1);
    sampleSpec4.setFormat(AUDIO_FORMAT_PCM_8_24_BIT);
    sampleSpec4.setSampleRate(48000);
    sampleSpec4.setChannelMask(1234);

    SampleSpec sampleSpec5;
    sampleSpec5.setChannelCount(1);
    sampleSpec5.setFormat(AUDIO_FORMAT_PCM_8_24_BIT);
    sampleSpec5.setSampleRate(22000);
    sampleSpec5.setChannelMask(1234);

    EXPECT_TRUE(sampleSpec1 == sampleSpec2);
    EXPECT_FALSE(sampleSpec1 == sampleSpec3);
    EXPECT_FALSE(sampleSpec1 == sampleSpec4);
    EXPECT_FALSE(sampleSpec1 == sampleSpec5);
}


TEST_P(SampleSpecTestChannelCountParam, getFrameSize)
{
    uint32_t channelCount = GetParam();

    SampleSpec sampleSpec;
    sampleSpec.setChannelCount(channelCount);

    sampleSpec.setFormat(AUDIO_FORMAT_PCM_16_BIT);
    EXPECT_EQ(2 * channelCount, sampleSpec.getFrameSize());

    sampleSpec.setFormat(AUDIO_FORMAT_PCM_8_24_BIT);
    EXPECT_EQ(4 * channelCount, sampleSpec.getFrameSize());
}

INSTANTIATE_TEST_CASE_P(
    SampleSpecTestChannelCountAllValues,
    SampleSpecTestChannelCountParam,
    ::testing::Values(1u, 2u)
    );


TEST(SampleSpec, convertBytesToFramesMono)
{
    SampleSpec sampleSpec;
    sampleSpec.setChannelCount(1);

    sampleSpec.setFormat(AUDIO_FORMAT_PCM_16_BIT);
    EXPECT_EQ(0u, sampleSpec.convertBytesToFrames(0));
    EXPECT_EQ(0u, sampleSpec.convertBytesToFrames(1));
    EXPECT_EQ(1u, sampleSpec.convertBytesToFrames(2));
    EXPECT_EQ(1u, sampleSpec.convertBytesToFrames(3));
    EXPECT_EQ(2u, sampleSpec.convertBytesToFrames(4));
    EXPECT_EQ(21u, sampleSpec.convertBytesToFrames(42));
    EXPECT_EQ(21u, sampleSpec.convertBytesToFrames(43));

    sampleSpec.setFormat(AUDIO_FORMAT_PCM_8_24_BIT);
    EXPECT_EQ(0u, sampleSpec.convertBytesToFrames(0));
    EXPECT_EQ(0u, sampleSpec.convertBytesToFrames(1));
    EXPECT_EQ(0u, sampleSpec.convertBytesToFrames(2));
    EXPECT_EQ(0u, sampleSpec.convertBytesToFrames(3));
    EXPECT_EQ(1u, sampleSpec.convertBytesToFrames(4));
    EXPECT_EQ(1u, sampleSpec.convertBytesToFrames(5));
    EXPECT_EQ(1u, sampleSpec.convertBytesToFrames(6));
    EXPECT_EQ(1u, sampleSpec.convertBytesToFrames(7));
    EXPECT_EQ(2u, sampleSpec.convertBytesToFrames(8));
    EXPECT_EQ(10u, sampleSpec.convertBytesToFrames(42));
    EXPECT_EQ(10u, sampleSpec.convertBytesToFrames(43));
    EXPECT_EQ(11u, sampleSpec.convertBytesToFrames(44));
    EXPECT_EQ(11u, sampleSpec.convertBytesToFrames(45));
}

TEST(SampleSpec, convertBytesToFramesStereo)
{
    SampleSpec sampleSpec;
    sampleSpec.setChannelCount(2);

    sampleSpec.setFormat(AUDIO_FORMAT_PCM_SUB_16_BIT);
    EXPECT_EQ(0u, sampleSpec.convertBytesToFrames(0));
    EXPECT_EQ(0u, sampleSpec.convertBytesToFrames(1));
    EXPECT_EQ(0u, sampleSpec.convertBytesToFrames(2));
    EXPECT_EQ(0u, sampleSpec.convertBytesToFrames(3));
    EXPECT_EQ(1u, sampleSpec.convertBytesToFrames(4));
    EXPECT_EQ(1u, sampleSpec.convertBytesToFrames(5));
    EXPECT_EQ(1u, sampleSpec.convertBytesToFrames(6));
    EXPECT_EQ(1u, sampleSpec.convertBytesToFrames(7));
    EXPECT_EQ(2u, sampleSpec.convertBytesToFrames(8));
    EXPECT_EQ(10u, sampleSpec.convertBytesToFrames(42));
    EXPECT_EQ(10u, sampleSpec.convertBytesToFrames(43));
    EXPECT_EQ(11u, sampleSpec.convertBytesToFrames(44));
    EXPECT_EQ(11u, sampleSpec.convertBytesToFrames(45));

    sampleSpec.setFormat(AUDIO_FORMAT_PCM_SUB_8_24_BIT);
    EXPECT_EQ(0u, sampleSpec.convertBytesToFrames(0));
    EXPECT_EQ(0u, sampleSpec.convertBytesToFrames(1));
    EXPECT_EQ(0u, sampleSpec.convertBytesToFrames(2));
    EXPECT_EQ(0u, sampleSpec.convertBytesToFrames(3));
    EXPECT_EQ(0u, sampleSpec.convertBytesToFrames(4));
    EXPECT_EQ(0u, sampleSpec.convertBytesToFrames(5));
    EXPECT_EQ(0u, sampleSpec.convertBytesToFrames(6));
    EXPECT_EQ(0u, sampleSpec.convertBytesToFrames(7));
    EXPECT_EQ(1u, sampleSpec.convertBytesToFrames(8));
    EXPECT_EQ(10u, sampleSpec.convertBytesToFrames(86));
    EXPECT_EQ(10u, sampleSpec.convertBytesToFrames(87));
    EXPECT_EQ(11u, sampleSpec.convertBytesToFrames(88));
    EXPECT_EQ(11u, sampleSpec.convertBytesToFrames(89));
}


TEST(SampleSpec, convertFramesToBytesMono)
{
    SampleSpec sampleSpec;
    sampleSpec.setChannelCount(1);

    sampleSpec.setFormat(AUDIO_FORMAT_PCM_SUB_16_BIT);
    EXPECT_EQ(0u, sampleSpec.convertFramesToBytes(0));
    EXPECT_EQ(2u, sampleSpec.convertFramesToBytes(1));
    EXPECT_EQ(4u, sampleSpec.convertFramesToBytes(2));
    EXPECT_EQ(84u, sampleSpec.convertFramesToBytes(42));

    sampleSpec.setFormat(AUDIO_FORMAT_PCM_SUB_8_24_BIT);
    EXPECT_EQ(0u, sampleSpec.convertFramesToBytes(0));
    EXPECT_EQ(4u, sampleSpec.convertFramesToBytes(1));
    EXPECT_EQ(8u, sampleSpec.convertFramesToBytes(2));
    EXPECT_EQ(168u, sampleSpec.convertFramesToBytes(42));
}

TEST(SampleSpec, convertFramesToBytesStereo)
{
    SampleSpec sampleSpec;
    sampleSpec.setChannelCount(2);

    sampleSpec.setFormat(AUDIO_FORMAT_PCM_SUB_16_BIT);
    EXPECT_EQ(0u, sampleSpec.convertFramesToBytes(0));
    EXPECT_EQ(4u, sampleSpec.convertFramesToBytes(1));
    EXPECT_EQ(8u, sampleSpec.convertFramesToBytes(2));
    EXPECT_EQ(168u, sampleSpec.convertFramesToBytes(42));

    sampleSpec.setFormat(AUDIO_FORMAT_PCM_SUB_8_24_BIT);
    EXPECT_EQ(0u, sampleSpec.convertFramesToBytes(0));
    EXPECT_EQ(8u, sampleSpec.convertFramesToBytes(1));
    EXPECT_EQ(16u, sampleSpec.convertFramesToBytes(2));
    EXPECT_EQ(336u, sampleSpec.convertFramesToBytes(42));
}


TEST(SampleSpec, convertFramesToUsec)
{
    SampleSpec sampleSpec;

    // F(Hz) = NbrFrame / T(s)
    // T(s)  = NbrFrame / F(Hz)
    // T(ms) = NbrFrame / F(Hz) * 1000

    sampleSpec.setSampleRate(1);
    EXPECT_EQ(0u, sampleSpec.convertFramesToUsec(0));
    EXPECT_EQ(1000000u, sampleSpec.convertFramesToUsec(1));
    EXPECT_EQ(2000000u, sampleSpec.convertFramesToUsec(2));

    sampleSpec.setSampleRate(22000);
    EXPECT_EQ(0u, sampleSpec.convertFramesToUsec(0));
    // Rate is in Frames per seconds, so expected time is (frames / Rate) * microseconds per second
    // 45 = 1 / 22000 * 1000000
    EXPECT_EQ(45u, sampleSpec.convertFramesToUsec(1));
    EXPECT_EQ(1000u, sampleSpec.convertFramesToUsec(22));
    EXPECT_EQ(2000u, sampleSpec.convertFramesToUsec(44));

    sampleSpec.setSampleRate(44100);
    EXPECT_EQ(0u, sampleSpec.convertFramesToUsec(0));
    EXPECT_EQ(22u, sampleSpec.convertFramesToUsec(1));
    EXPECT_EQ(997u, sampleSpec.convertFramesToUsec(44));
    EXPECT_EQ(10000u, sampleSpec.convertFramesToUsec(441));
    EXPECT_EQ(1000000u, sampleSpec.convertFramesToUsec(44100));
}


TEST(SampleSpec, convertUsecToframes)
{
    SampleSpec sampleSpec;

    // F(Hz)    = NbrFrame / T(s)
    // NbrFrame = F(Hz) * T(s)
    // NbrFrame = F(Hz) * T(us) / 1000000

    sampleSpec.setSampleRate(0);
    EXPECT_EQ(0u, sampleSpec.convertUsecToframes(0));
    EXPECT_EQ(0u, sampleSpec.convertUsecToframes(1));
    EXPECT_EQ(0u, sampleSpec.convertUsecToframes(2));

    sampleSpec.setSampleRate(1);
    EXPECT_EQ(0u, sampleSpec.convertUsecToframes(0));
    EXPECT_EQ(1u, sampleSpec.convertUsecToframes(1000000));
    EXPECT_EQ(2u, sampleSpec.convertUsecToframes(2000000));

    sampleSpec.setSampleRate(22000);
    EXPECT_EQ(0u, sampleSpec.convertUsecToframes(0));
    EXPECT_EQ(22000 * 1 / 1000000u, sampleSpec.convertUsecToframes(1));
    EXPECT_EQ(22000 * 2 / 1000000u, sampleSpec.convertUsecToframes(2));

    sampleSpec.setSampleRate(44000);
    EXPECT_EQ(0u, sampleSpec.convertUsecToframes(0));
    EXPECT_EQ(44000 * 1 / 1000000u, sampleSpec.convertUsecToframes(1));
}

} // namespace intel_audio
