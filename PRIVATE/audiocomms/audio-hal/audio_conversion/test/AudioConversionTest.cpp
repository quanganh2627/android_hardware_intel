/*
 * INTEL CONFIDENTIAL
 * Copyright (c) 2014 Intel
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

#include "AudioConversionTest.hpp"
#include <AudioConversion.hpp>
#include <SampleSpec.hpp>
#include <AudioUtils.hpp>
#include <media/AudioBufferProvider.h>
#include <gtest/gtest.h>

namespace intel_audio
{

typedef std::tr1::tuple<const SampleSpec, const SampleSpec, const void *, size_t,
                        const void *, size_t, bool> AudioConversionParam;

class AudioConversionT : public ::testing::TestWithParam<AudioConversionParam>
{
};

/**
 * Parametrizable test case that takes 6 parameters. It converts a source buffer
 * to a destination buffer using or not internal memory of the AudioConversion library.
 *
 * @tparam[in] source sample specifications
 * @tparam[in] destination sample specifications
 * @tparam[in] pointer on the source buffer
 * @tparam[in] size in byte of the source buffer to convert
 * @tparam[in:out] pointer on the destination buffer
 * @tparam[in:out] size in byte of the destination buffer to output
 * @tparam[in] boolean to indicate if need allocate memory or use AudioConversion library buffer.
 */
TEST_P(AudioConversionT, audioConversion)
{
    const SampleSpec sampleSpecSrc = std::tr1::get<0>(GetParam());
    const SampleSpec sampleSpecDst = std::tr1::get<1>(GetParam());

    AudioConversion audioConversion;
    EXPECT_EQ(0, audioConversion.configure(sampleSpecSrc, sampleSpecDst));

    const void *sourceBuf = std::tr1::get<2>(GetParam());
    size_t sourceBufSize = std::tr1::get<3>(GetParam());
    const uint32_t inputFrames = sampleSpecSrc.convertBytesToFrames(sourceBufSize);

    const uint8_t *expectedDstBuf = (uint8_t *)std::tr1::get<4>(GetParam());
    size_t expectedDstBufSize = std::tr1::get<5>(GetParam());
    const uint32_t expextedDstFrames = AudioUtils::convertSrcToDstInFrames(inputFrames,
                                                                           sampleSpecSrc,
                                                                           sampleSpecDst);

    if (expectedDstBuf != NULL) {

        EXPECT_EQ(expextedDstFrames, sampleSpecDst.convertBytesToFrames(expectedDstBufSize));
    }

    bool allocateBuffer = std::tr1::get<6>(GetParam());
    uint8_t *dstBuf = NULL;
    uint32_t dstFrames = 0;

    if (allocateBuffer) {

        uint32_t dstSizeInBytes = AudioUtils::convertSrcToDstInBytes(sourceBufSize,
                                                                     sampleSpecSrc,
                                                                     sampleSpecDst);

        if (expectedDstBuf != NULL) {

            EXPECT_EQ(expectedDstBufSize, dstSizeInBytes);
        }
        dstBuf = new uint8_t[dstSizeInBytes];
    }

    EXPECT_EQ(0, audioConversion.convert(sourceBuf, reinterpret_cast<void **>(&dstBuf),
                                         inputFrames, &dstFrames));

    EXPECT_EQ(expextedDstFrames, dstFrames);

    // May not be able to do a simple comparison to check the output... (resampling)
    if (expectedDstBuf != NULL) {

        EXPECT_EQ(0, memcmp(expectedDstBuf, dstBuf, expectedDstBufSize));
    }

    if (allocateBuffer) {

        delete[] dstBuf;
    }
}


const uint16_t sourceBufEmptyConv[] = {
    10, 10,
    0xFF00, 0x00FF,
    2, 9,
    0x7F00, 0x00FF,
    0xFFFF, 0xFFFF,
    0x0000, 0x0000
};

/**
 * Test a void conversion without allocated destination buffer.
 */
INSTANTIATE_TEST_CASE_P(emptyConversionWithoutMemoryAllocation,
                        AudioConversionT,
                        ::testing::Values(
                            AudioConversionParam(
                                SampleSpec(2, AUDIO_FORMAT_PCM_16_BIT, 44100),
                                SampleSpec(2, AUDIO_FORMAT_PCM_16_BIT, 44100),
                                sourceBufEmptyConv,
                                sizeof(sourceBufEmptyConv),
                                sourceBufEmptyConv,
                                sizeof(sourceBufEmptyConv),
                                false
                                )
                            )
                        );

/**
 * Test a void conversion with allocated destination buffer.
 */
INSTANTIATE_TEST_CASE_P(emptyConversionWithMemoryAllocation,
                        AudioConversionT,
                        ::testing::Values(
                            AudioConversionParam(
                                SampleSpec(2, AUDIO_FORMAT_PCM_16_BIT, 44100),
                                SampleSpec(2, AUDIO_FORMAT_PCM_16_BIT, 44100),
                                sourceBufEmptyConv,
                                sizeof(sourceBufEmptyConv),
                                sourceBufEmptyConv,
                                sizeof(sourceBufEmptyConv),
                                true
                                )
                            )
                        );

const uint16_t sourceBuf1[] = {
    10, 10,
    0xFF00, 0x00FF,
    2, 9,
    0x7F00, 0x00FF,
    0xFFFF, 0xFFFF,
    0x0000, 0x0000
};

const uint16_t expectedDstBuf1[] = {
    10,
    0xFFFF,
    5,
    0x3FFF,
    0xFFFF,
    0x0000
};

/**
 * Test a remapping from stereo to mono in S16 format without allocated output buffer
 */
INSTANTIATE_TEST_CASE_P(remapStereoToMonoInS16le,
                        AudioConversionT,
                        ::testing::Values(
                            AudioConversionParam(
                                SampleSpec(2, AUDIO_FORMAT_PCM_16_BIT, 44100),
                                SampleSpec(1, AUDIO_FORMAT_PCM_16_BIT, 44100),
                                sourceBuf1,
                                sizeof(sourceBuf1),
                                expectedDstBuf1,
                                sizeof(expectedDstBuf1),
                                false
                                )
                            )
                        );

/**
 * Test a remapping from stereo to mono in S16 format with allocated output buffer
 */
INSTANTIATE_TEST_CASE_P(conversionStereoToMonoInS16leWithAllocatedOutputBuffer,
                        AudioConversionT,
                        ::testing::Values(
                            AudioConversionParam(
                                SampleSpec(2, AUDIO_FORMAT_PCM_16_BIT, 44100),
                                SampleSpec(1, AUDIO_FORMAT_PCM_16_BIT, 44100),
                                sourceBuf1,
                                sizeof(sourceBuf1),
                                expectedDstBuf1,
                                sizeof(expectedDstBuf1),
                                true
                                )
                            )
                        );

const uint16_t sourceBuf2[] = {
    0xDEAD,
    0xBEEF,
    0x1234,
    0xFFFF,
    0x0000
};

const uint16_t expectedDstBuf2[] = {
    0xDEAD, 0xDEAD,
    0xBEEF, 0xBEEF,
    0x1234, 0x1234,
    0xFFFF, 0xFFFF,
    0x0000, 0x0000
};

/**
 * Test a remapping from mono to stereo in S16 format without allocated output buffer
 */
INSTANTIATE_TEST_CASE_P(remapMonoToStereoInS16le,
                        AudioConversionT,
                        ::testing::Values(
                            AudioConversionParam(
                                SampleSpec(1, AUDIO_FORMAT_PCM_16_BIT, 44100),
                                SampleSpec(2, AUDIO_FORMAT_PCM_16_BIT, 44100),
                                sourceBuf2,
                                sizeof(sourceBuf2),
                                expectedDstBuf2,
                                sizeof(expectedDstBuf2),
                                false
                                )
                            )
                        );

const uint32_t sourceBuf3[] = {
    0xDEADBEEF, 0xDEADBEEF,
    0xBEEFDEAD, 0xDEADBEEF,
    0x12345678, 0x56781234,
    0x0000FFFF, 0xFFFF0000,
    0xFFFFFFFF, 0xFFFFFFFF,
    0x00000000, 0x00000000
};

const uint32_t expectedDstBuf3[] = {
    0xDEADBEEF,
    0xCECECECE,
    0x34563456,
    0x7FFFFFFF,
    0xFFFFFFFF,
    0x00000000
};

/**
 * Test a remapping from stereo to mono in S24 format without allocated output buffer
 */
INSTANTIATE_TEST_CASE_P(remapStereoToMonoInS24le,
                        AudioConversionT,
                        ::testing::Values(
                            AudioConversionParam(
                                SampleSpec(2, AUDIO_FORMAT_PCM_8_24_BIT, 44100),
                                SampleSpec(1, AUDIO_FORMAT_PCM_8_24_BIT, 44100),
                                sourceBuf3,
                                sizeof(sourceBuf3),
                                expectedDstBuf3,
                                sizeof(expectedDstBuf3),
                                false
                                )
                            )
                        );

const uint32_t sourceBuf4[] = {
    0xDEADBEEF,
    0xBEEFDEAD,
    0x12345678,
    0x0000FFFF,
    0xFFFFFFFF,
    0x00000000
};

const uint32_t expectedDstBuf4[] = {
    0xDEADBEEF, 0xDEADBEEF,
    0xBEEFDEAD, 0xBEEFDEAD,
    0x12345678, 0x12345678,
    0x0000FFFF, 0x0000FFFF,
    0xFFFFFFFF, 0xFFFFFFFF,
    0x00000000, 0x00000000
};

/**
 * Test a remapping from mono to stereo in S24 format without allocated output buffer
 */
INSTANTIATE_TEST_CASE_P(remapMonoToStereoInS24le,
                        AudioConversionT,
                        ::testing::Values(
                            AudioConversionParam(
                                SampleSpec(1, AUDIO_FORMAT_PCM_8_24_BIT, 44100),
                                SampleSpec(2, AUDIO_FORMAT_PCM_8_24_BIT, 44100),
                                sourceBuf4,
                                sizeof(sourceBuf4),
                                expectedDstBuf4,
                                sizeof(expectedDstBuf4),
                                false
                                )
                            )
                        );

const uint16_t sourceBuf5[] = {
    10, 20,
    5, 1,
    3, 8,
    12, 15,
    0, 0
};

const uint16_t expectedDstBuf5[] = {
    15, 0,
    3, 0,
    5, 0,
    13, 0,
    0, 0
};

static const SampleSpec::ChannelsPolicy stereoCC[] = {
    SampleSpec::Copy, SampleSpec::Copy
};
static const SampleSpec::ChannelsPolicy stereoCI[] = {
    SampleSpec::Copy, SampleSpec::Ignore
};
static const SampleSpec::ChannelsPolicy stereoAI[] = {
    SampleSpec::Average, SampleSpec::Ignore
};

/**
 * Test a remapping from stereo to stereo with channel policy adaptation, outputting stereo
 * samples that:
 *      -average right and left source sample on left destination sample.
 *      -empty right destination sample
 * It is performed in S16 format without allocated output buffer
 */
INSTANTIATE_TEST_CASE_P(remapPolicyCcToAiInS16le,
                        AudioConversionT,
                        ::testing::Values(
                            AudioConversionParam(
                                SampleSpec(2, AUDIO_FORMAT_PCM_16_BIT, 44100,
                                           std::vector<SampleSpec::ChannelsPolicy>(stereoCC,
                                                                                   stereoCC + 2)),
                                SampleSpec(2, AUDIO_FORMAT_PCM_16_BIT, 44100,
                                           std::vector<SampleSpec::ChannelsPolicy>(stereoAI,
                                                                                   stereoAI + 2)),
                                sourceBuf5,
                                sizeof(sourceBuf5),
                                expectedDstBuf5,
                                sizeof(expectedDstBuf5),
                                false
                                )
                            )
                        );

const uint16_t sourceBuf6[] = {
    0xDEAD, 0xBEEF,
    0xDEAD, 0xBEEF,
    0x1234, 0x5678,
    0x0000, 0xFFFF,
    0xFFFF, 0xFFFF,
    0x0000, 0x0000
};

const uint16_t expectedDstBuf6[] = {
    0xDEAD, 0xDEAD,
    0xDEAD, 0xDEAD,
    0x1234, 0x1234,
    0x0000, 0x0000,
    0xFFFF, 0xFFFF,
    0x0000, 0x0000
};

/**
 * Test a remapping from stereo to stereo with channel policy adaptation, outputting stereo
 * samples that ignores right samples of the source.
 * It is performed in S16 format without allocated output buffer
 */
INSTANTIATE_TEST_CASE_P(remapPolicyCiToCcInS16le,
                        AudioConversionT,
                        ::testing::Values(
                            AudioConversionParam(
                                SampleSpec(2, AUDIO_FORMAT_PCM_16_BIT, 44100,
                                           std::vector<SampleSpec::ChannelsPolicy>(stereoCI,
                                                                                   stereoCI + 2)),
                                SampleSpec(2, AUDIO_FORMAT_PCM_16_BIT, 44100,
                                           std::vector<SampleSpec::ChannelsPolicy>(stereoCC,
                                                                                   stereoCC + 2)),
                                sourceBuf6,
                                sizeof(sourceBuf6),
                                expectedDstBuf6,
                                sizeof(expectedDstBuf6),
                                false
                                )
                            )
                        );

const uint32_t sourceBuf7[] = {
    0xDEADBEEF, 0xDEADBEEF,
    0xBEEFDEAD, 0xDEADBEEF,
    0x12345678, 0x56781234,
    0x0000FFFF, 0xFFFF0000,
    0xFFFFFFFF, 0xFFFFFFFF,
    0x00000000, 0x00000000
};

const uint32_t expectedDstBuf7[] = {
    0xDEADBEEF, 0x00000000,
    0xCECECECE, 0x00000000,
    0x34563456, 0x00000000,
    0x7FFFFFFF, 0x00000000,
    0xFFFFFFFF, 0x00000000,
    0x00000000, 0x00000000
};

/**
 * Test a remapping from stereo to stereo with channel policy adaptation, outputting stereo
 * samples that:
 *      -average right and left source sample on left destination sample.
 *      -empty right destination sample
 * It is performed in S24 format without allocated output buffer
 */
INSTANTIATE_TEST_CASE_P(remapPolicyCcToAiInS24le,
                        AudioConversionT,
                        ::testing::Values(
                            AudioConversionParam(
                                SampleSpec(2, AUDIO_FORMAT_PCM_8_24_BIT, 44100,
                                           std::vector<SampleSpec::ChannelsPolicy>(stereoCC,
                                                                                   stereoCC + 2)),
                                SampleSpec(2, AUDIO_FORMAT_PCM_8_24_BIT, 44100,
                                           std::vector<SampleSpec::ChannelsPolicy>(stereoAI,
                                                                                   stereoAI + 2)),
                                sourceBuf7,
                                sizeof(sourceBuf7),
                                expectedDstBuf7,
                                sizeof(expectedDstBuf7),
                                false
                                )
                            )
                        );

const uint32_t sourceBuf8[] = {
    0xDEADDEAD, 0xBEEFBEEF,
    0xDEADBEEF, 0xDEADBEEF,
    0x12345678, 0x56781234,
    0x00000000, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF,
    0x00000000, 0x00000000
};

const uint32_t expectedDstBuf8[] = {
    0xDEADDEAD, 0xDEADDEAD,
    0xDEADBEEF, 0xDEADBEEF,
    0x12345678, 0x12345678,
    0x00000000, 0x00000000,
    0xFFFFFFFF, 0xFFFFFFFF,
    0x00000000, 0x00000000
};

/**
 * Test a remapping from stereo to stereo with channel policy adaptation, outputting stereo
 * samples that ignores right samples of the source.
 * It is performed in S24 format without allocated output buffer
 */
INSTANTIATE_TEST_CASE_P(remapPolicyCiToCcInS24le,
                        AudioConversionT,
                        ::testing::Values(
                            AudioConversionParam(
                                SampleSpec(2, AUDIO_FORMAT_PCM_8_24_BIT, 44100,
                                           std::vector<SampleSpec::ChannelsPolicy>(stereoCI,
                                                                                   stereoCI + 2)),
                                SampleSpec(2, AUDIO_FORMAT_PCM_8_24_BIT, 44100,
                                           std::vector<SampleSpec::ChannelsPolicy>(stereoCC,
                                                                                   stereoCC + 2)),
                                sourceBuf8,
                                sizeof(sourceBuf8),
                                expectedDstBuf8,
                                sizeof(expectedDstBuf8),
                                false
                                )
                            )
                        );

const uint16_t sourceBuf9[] = {
    0xDEAD, 0xBEEF,
    0xBEEF, 0xDEAD,
    0x1234, 0x5678,
    0x0000, 0xFFFF,
    0xFFFF, 0xFFFF,
    0x0000, 0x0000
};

const uint32_t expectedDstBuf9[] = {
    0x00DEAD00, 0x00BEEF00,
    0x00BEEF00, 0x00DEAD00,
    0x00123400, 0x00567800,
    0x00000000, 0x00FFFF00,
    0x00FFFF00, 0x00FFFF00,
    0x00000000, 0x00000000
};

/**
 * Test a reformating from S16 to S24 format in iso-channels and rate.
 */
INSTANTIATE_TEST_CASE_P(reformatS16leToS24,
                        AudioConversionT,
                        ::testing::Values(
                            AudioConversionParam(
                                SampleSpec(2, AUDIO_FORMAT_PCM_16_BIT, 44100),
                                SampleSpec(2, AUDIO_FORMAT_PCM_8_24_BIT, 44100),
                                sourceBuf9,
                                sizeof(sourceBuf9),
                                expectedDstBuf9,
                                sizeof(expectedDstBuf9),
                                false
                                )
                            )
                        );


const uint32_t sourceBuf10[] = {
    0x00ADBEEF, 0x00EFDEAD,
    0xBEEFDEAD, 0xDEADBEEF,
    0x12345678, 0x56781234,
    0x0000FFFF, 0xFFFF0000,
    0xFFFFFFFF, 0xFFFFFFFF,
    0x00000000, 0x00000000
};

const uint16_t expectedDstBuf10[] = {
    0xADBE, 0xEFDE,
    0xEFDE, 0xADBE,
    0x3456, 0x7812,
    0x00FF, 0xFF00,
    0xFFFF, 0xFFFF,
    0x0000, 0x0000
};

/**
 * Test a reformating from S24 to S16 format in iso-channels and rate.
 */
INSTANTIATE_TEST_CASE_P(reformatS24leToS16,
                        AudioConversionT,
                        ::testing::Values(
                            AudioConversionParam(
                                SampleSpec(2, AUDIO_FORMAT_PCM_8_24_BIT, 48000),
                                SampleSpec(2, AUDIO_FORMAT_PCM_16_BIT, 48000),
                                sourceBuf10,
                                sizeof(sourceBuf10),
                                expectedDstBuf10,
                                sizeof(expectedDstBuf10),
                                false
                                )
                            )
                        );

const uint16_t sourceBuf11[] = {
    10, 20,
    5, 1,
    3, 8,
    12, 15,
    0xFFFF, 0xFFFF,
    0xF000, 0x9880
};

/**
 * Test a resampling from 24kHz to 48kHz in S16 format in iso channels.
 */
INSTANTIATE_TEST_CASE_P(resampleFrom24kTo48kInS16le,
                        AudioConversionT,
                        ::testing::Values(
                            AudioConversionParam(
                                SampleSpec(2, AUDIO_FORMAT_PCM_8_24_BIT, 48000),
                                SampleSpec(2, AUDIO_FORMAT_PCM_16_BIT, 48000),
                                sourceBuf11,
                                sizeof(sourceBuf11),
                                NULL,
                                sizeof(expectedDstBuf10),
                                false
                                )
                            )
                        );

/**
 * Chains 2 conversions with the same instance of the conversion library
 */
TEST(AudioConversion, resampleDoubleConfigure)
{
    const SampleSpec sampleSpecSrc(2, AUDIO_FORMAT_PCM_16_BIT, 24000);
    SampleSpec sampleSpecDst(2, AUDIO_FORMAT_PCM_16_BIT, 48000);

    AudioConversion audioConversion;
    EXPECT_EQ(0, audioConversion.configure(sampleSpecSrc, sampleSpecDst));

    const uint16_t sourceBuf[] = {
        10, 20,
        5, 1,
        3, 8,
        12, 15,
        0xFFFF, 0xFFFF,
        0xF000, 0x9880
    };
    const uint32_t inputFrames =
        sizeof(sourceBuf) / (sizeof(uint16_t) * sampleSpecSrc.getChannelCount());

    uint16_t *dstBuf = NULL;
    uint32_t dstFrames = 0;
    EXPECT_EQ(0, audioConversion.convert(sourceBuf, reinterpret_cast<void **>(&dstBuf),
                                         inputFrames, &dstFrames));

    sampleSpecDst.setSampleRate(44100);
    EXPECT_EQ(0, audioConversion.configure(sampleSpecSrc, sampleSpecDst));
    dstBuf = NULL;
    dstFrames = 0;

    const uint16_t secondSourceBuf[] = {
        10, 20, 5, 1, 3, 8, 12, 15, 10, 20, 5, 1, 3, 8, 12, 15,
        10, 20, 5, 1, 3, 8, 12, 15, 10, 20, 5, 1, 3, 8, 12, 15,
        10, 20, 5, 1, 3, 8, 12, 15, 10, 20, 5, 1, 3, 8, 12, 15,
        10, 20, 5, 1, 3, 8, 12, 15, 10, 20, 5, 1, 3, 8, 12, 15,
        10, 20, 5, 1, 3, 8, 12, 15, 10, 20, 5, 1, 3, 8, 12, 15,
        10, 20, 5, 1, 3, 8, 12, 15, 10, 20, 5, 1, 3, 8, 12, 15
    };
    const uint32_t inputFrames2 =
        sizeof(secondSourceBuf) / (sizeof(uint16_t) * sampleSpecSrc.getChannelCount());

    EXPECT_EQ(0, audioConversion.convert(secondSourceBuf, reinterpret_cast<void **>(&dstBuf),
                                         inputFrames2, &dstFrames));
}

const uint16_t sourceBuf12[] = {
    10, 20,
    5, 1,
    3, 8,
    12, 15,
    10, 20,
    5, 1,
    3, 8,
    12, 15,
    0xFFFF, 0x1111,
    0x00FF, 0xFF00
};

const uint16_t expectedDstBuf12[] = {
    0, 0,
    0, 1,
    4, 7,
    8, 12,
    7, 25
};

/**
 * Test a resample from 48kHz to 24kHz in S16 format, stereo with allocated destination
 * buffer.
 */
INSTANTIATE_TEST_CASE_P(resampleWithAllocatedMemoryFrom48kTo24kInS16le,
                        AudioConversionT,
                        ::testing::Values(
                            AudioConversionParam(
                                SampleSpec(2, AUDIO_FORMAT_PCM_16_BIT, 48000),
                                SampleSpec(2, AUDIO_FORMAT_PCM_16_BIT, 24000),
                                sourceBuf12,
                                sizeof(sourceBuf12),
                                expectedDstBuf12,
                                sizeof(expectedDstBuf12),
                                true
                                )
                            )
                        );


const uint16_t sourceBuf13[] = {
    10, 20,
    5, 1,
    3, 8,
    12, 15
};

/**
 * Test a resampling from 8kHzto 16kHz in S16 format, stereo without allocated destination
 * buffer.
 */
INSTANTIATE_TEST_CASE_P(resampleFrom8kTo16kInS16le,
                        AudioConversionT,
                        ::testing::Values(
                            AudioConversionParam(
                                SampleSpec(2, AUDIO_FORMAT_PCM_16_BIT, 8000),
                                SampleSpec(2, AUDIO_FORMAT_PCM_16_BIT, 16000),
                                sourceBuf13,
                                sizeof(sourceBuf13),
                                NULL,
                                0,
                                true
                                )
                            )
                        );

/**
 * Test creation / deletion of the AudioConversion library.
 */
TEST(AudioConversion, memoryTest)
{
    const SampleSpec sampleSpecSrc(2, AUDIO_FORMAT_PCM_8_24_BIT, 8000);
    const SampleSpec sampleSpecDst(1, AUDIO_FORMAT_PCM_16_BIT, 48000);

    uint16_t *dstBuf = NULL;
    EXPECT_TRUE(dstBuf == static_cast<uint16_t *>(NULL));

    AudioConversion *audioConversion = new AudioConversion();
    EXPECT_TRUE(audioConversion != NULL);
    EXPECT_EQ(0, audioConversion->configure(sampleSpecSrc, sampleSpecDst));

    const uint16_t sourceBuf[] = {
        10, 20, 5, 1, 3, 8, 12, 15, 10, 20, 5, 1, 3, 8, 12, 15,
        10, 20, 5, 1, 3, 8, 12, 15, 10, 20, 5, 1, 3, 8, 12, 15,
        10, 20, 5, 1, 3, 8, 12, 15, 10, 20, 5, 1, 3, 8, 12, 15,
        10, 20, 5, 1, 3, 8, 12, 15, 10, 20, 5, 1, 3, 8, 12, 15,
        10, 20, 5, 1, 3, 8, 12, 15, 10, 20, 5, 1, 3, 8, 12, 15,
        10, 20, 5, 1, 3, 8, 12, 15, 10, 20, 5, 1, 3, 8, 12, 15
    };
    const uint32_t inputFrames =
        sizeof(sourceBuf) / (sizeof(uint16_t) * sampleSpecSrc.getChannelCount());

    uint32_t dstFrames = 0;
    EXPECT_EQ(0, audioConversion->convert(sourceBuf,
                                          reinterpret_cast<void **>(&dstBuf),
                                          inputFrames, &dstFrames));
    EXPECT_TRUE(dstBuf != static_cast<uint16_t *>(NULL));
    EXPECT_TRUE(dstFrames != 0);
    delete audioConversion;
}


/**
 * Test the ability of the conversion library to return exactly the number of frames requested.
 */
TEST(AudioConversion, frameExactApi)
{
    const SampleSpec sampleSpecSrc(2, AUDIO_FORMAT_PCM_16_BIT, 8000);
    const SampleSpec sampleSpecDst(2, AUDIO_FORMAT_PCM_16_BIT, 48000);

    AudioConversion audioConversion;
    EXPECT_EQ(0, audioConversion.configure(sampleSpecSrc, sampleSpecDst));

    const uint16_t sourceBuf[] = {
        10, 20, 5, 1, 3, 8, 12, 15, 10, 20, 5, 1, 3, 8, 12, 15, 10, 20,
        10, 20, 5, 1, 3, 8, 12, 15, 10, 20, 5, 1, 3, 8, 12, 15, 10, 20,
        10, 20, 5, 1, 3, 8, 12, 15, 10, 20, 5, 1, 3, 8, 12, 15, 10, 20,
        10, 20, 5, 1, 3, 8, 12, 15, 10, 20, 5, 1, 3, 8, 12, 15, 10, 20,
        10, 20, 5, 1, 3, 8, 12, 15, 10, 20, 5, 1, 3, 8, 12, 15, 10, 20
    };
    const uint32_t inputFrames = sizeof(sourceBuf) /
                                 (sizeof(uint16_t) * sampleSpecSrc.getChannelCount());
    const uint32_t expectedDstFrames = AudioUtils::convertSrcToDstInFrames(inputFrames,
                                                                           sampleSpecSrc,
                                                                           sampleSpecDst);

    uint16_t dstBuf[sampleSpecDst.convertFramesToBytes(expectedDstFrames)];
    MyAudioBufferProvider bufferProvider(&sourceBuf[0], sizeof(sourceBuf) / sizeof(uint16_t));
    EXPECT_EQ(0, audioConversion.getConvertedBuffer(static_cast<void *>(dstBuf),
                                                    expectedDstFrames / 2,
                                                    &bufferProvider));

    uint16_t *tmpDst = dstBuf + (sampleSpecDst.convertFramesToBytes(expectedDstFrames / 2) /
                                 sizeof(uint16_t));
    EXPECT_EQ(0, audioConversion.getConvertedBuffer(static_cast<void *>(tmpDst),
                                                    expectedDstFrames / 2,
                                                    &bufferProvider));

    // @todo: quality check of output
}

} // namespace intel_audio
