/*
 * INTEL CONFIDENTIAL
 *
 * Copyright 2014 Intel Corporation
 * All Rights Reserved.
 *
 * The source code contained or described herein and all documents related
 * to the source code ("Material") are owned by Intel Corporation or its
 * suppliers or licensors.
 * Title to the Material remains with Intel Corporation or its suppliers and
 * licensors. The Material contains trade secrets and proprietary and
 * confidential information of Intel or its suppliers and licensors.
 * The Material is protected by worldwide copyright and trade secret laws and
 * treaty provisions. No part of the Material may be used, copied, reproduced,
 * modified, published, uploaded, posted, transmitted, distributed, or
 * disclosed in any way without Intel's prior express written permission.
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 */

#include "AudioEffectsFcct.hpp"

#include <media/AudioRecord.h>
#include <audio_effects/effect_aec.h>
#include <audio_effects/effect_agc.h>
#include <audio_effects/effect_ns.h>
#include <audio_effects/effect_visualizer.h>
#include <audio_effects/effect_equalizer.h>
#include <utils/String16.h>

using ::testing::Test;
using std::string;
using std::numeric_limits;
using namespace android;
using audio_comms::utilities::Log;

const string AudioEffectsFunctionalTest::mLpeEffectLibPath =
    "/system/lib/soundfx/liblpepreprocessing.so";

const std::string AudioEffectsFunctionalTest::mAudioServiceName =
    "media.audio_flinger";

void AudioEffectsFunctionalTest::SetUp()
{
    mServiceManager = defaultServiceManager();
    EXPECT_TRUE(mServiceManager != 0);

    mBinder =
        mServiceManager->getService(String16(AudioEffectsFunctionalTest::mAudioServiceName.c_str()));
    EXPECT_TRUE(mBinder != 0);

    mAudioFlinger = interface_cast<IAudioFlinger>(mBinder);
}

bool AudioEffectsFunctionalTest::checkPlatformHasEffectType(const effect_uuid_t *effectType)
{
    effect_descriptor_t fxDesc;
    uint32_t numFx;

    if (AudioEffect::queryNumberEffects(&numFx) != NO_ERROR) {
        return false;
    }
    for (uint32_t i = 0; i < numFx; i++) {
        if ((AudioEffect::queryEffect(i, &fxDesc) == NO_ERROR) &&
            (memcmp(&fxDesc.type, effectType, sizeof(effect_uuid_t)) == 0)) {
            return true;
        }
    }
    return false;
}

void AudioEffectsFunctionalTest::compareEffectParams(const effect_param_t *param1,
                                                     const effect_param_t *param2)
{
    AUDIOCOMMS_ASSERT(param1->psize != 0, "effect param size 1 is 0");
    AUDIOCOMMS_ASSERT(param2->psize != 0, "effect param size 2 is 0");

    EXPECT_EQ(0, memcmp(param1->data, param2->data, param1->psize))
        << "Effects parameters differ";

    uint32_t paramValueOffsetInBytes =
        ((sizeof(param2->psize) - 1) / sizeof(uint32_t) + 1) * sizeof(uint32_t);
    const char *vOrigData = param1->data + paramValueOffsetInBytes;
    const char *vData = param2->data + paramValueOffsetInBytes;

    EXPECT_EQ(0, memcmp(vOrigData, vData, param1->vsize)) << "Effects values differ";
}

void AudioEffectsFunctionalTest::getParameterForEffect(AudioEffect *effect,
                                                       const TestEffectParameterBase *effectTest,
                                                       bool compareWithOrigValue)
{
    ASSERT_TRUE(effectTest != NULL);
    /**
     * Get a copy of effect parameter structure
     */
    effect_param_t *effectParamCopy = (effect_param_t *)malloc(effectTest->getSize());
    AUDIOCOMMS_ASSERT(effectParamCopy != NULL, "NULL effect param");
    memcpy(effectParamCopy, effectTest->getEffectParam(), effectTest->getSize());

    status_t status;
    status_t expectedGetResult = effectTest->readable() ? NO_ERROR : BAD_VALUE;
    status = effect->getParameter(effectParamCopy);
    ASSERT_EQ(status, NO_ERROR)
        << "add failed with error=" << status;
    EXPECT_EQ(expectedGetResult, effectParamCopy->status) << "add failed with error=" <<
        effectParamCopy->status;

    if (effectParamCopy->status == NO_ERROR) {

        if (effectTest->readable()) {
            // Did we request the right size?
            EXPECT_EQ(effectParamCopy->vsize, effectTest->getEffectParam()->vsize)
                << "Requested value to be size = " << effectTest->getEffectParam()->vsize
                << " but effect returned a value size =  " << effectParamCopy->vsize;
        }
        if (compareWithOrigValue && effectTest->rwAble()) {
            compareEffectParams(effectTest->getEffectParam(), effectParamCopy);
        }
    }
    free(effectParamCopy);
}

void AudioEffectsFunctionalTest::setParameterForEffect(AudioEffect *effect,
                                                       const TestEffectParameterBase *effectTest)
{
    ASSERT_TRUE(effectTest != NULL);
    /**
     * Get a copy of effect parameter structure
     */
    effect_param_t *effectParamCopy = (effect_param_t *)malloc(effectTest->getSize());
    AUDIOCOMMS_ASSERT(effectParamCopy != NULL, "NULL effect param");
    memcpy(effectParamCopy, effectTest->getEffectParam(), effectTest->getSize());

    status_t status;

    status_t expectedSetResult = effectTest->writable() ? NO_ERROR : BAD_VALUE;
    status = effect->setParameter(effectParamCopy);

    EXPECT_EQ(status, NO_ERROR) << "add failed with error=" << status;
    if (status == NO_ERROR) {

        EXPECT_EQ(effectParamCopy->status, expectedSetResult) << "add failed with error=" <<
            effectParamCopy->status;
    }
    free(effectParamCopy);
}

bool AudioEffectsFunctionalTest::loadEffectLibrary()
{
    Parcel data;
    data.writeInterfaceToken(mAudioFlinger->getInterfaceDescriptor());
    data.writeCString(AudioEffectsFunctionalTest::mLpeEffectLibPath.c_str());

    // test 100 IAudioFlinger binder transaction values and check that none corresponds
    // to LOAD_EFFECT_LIBRARY and successfully loads our test library
    for (uint32_t i = IBinder::FIRST_CALL_TRANSACTION;
         i < IBinder::FIRST_CALL_TRANSACTION + 100;
         i++) {

        Parcel reply;
        status_t status = AudioEffectsFunctionalTest::mBinder->transact(i, data, &reply);
        if (status != NO_ERROR) {
            continue;
        }
        status = reply.readInt32();
        if (status != NO_ERROR) {
            continue;
        }
        return true;
    }
    return false;
}

TEST_P(TAudioEffectsSetParamTests, TestEffectParameterBase)
{
    const TestEffectParameterBase effectTest = GetParam().first;
    effect_uuid_t effectUuidToTest = effectTest.getEffectType();
    bool expectedPass = GetParam().second;

    ASSERT_TRUE(loadEffectLibrary());

    EXPECT_TRUE(checkPlatformHasEffectType(&effectUuidToTest));

    if (expectedPass) {
        // Initialise AudioRecord
        AudioEffect *effect;

        if (effectTest.getDirection() == TestEffectParameterBase::input) {

            sp<AudioRecord> record = new AudioRecord();
            // Configure the record
            EXPECT_TRUE(record->set(AUDIO_SOURCE_VOICE_COMMUNICATION, /* Input source to use */
                                    16000, /* sample rate. */
                                    AUDIO_FORMAT_PCM_16_BIT, /* sample format. */
                                    AUDIO_CHANNEL_IN_MONO, /* channels. */
                                    24000, /* frame count. */
                                    NULL, /* callback_t */
                                    NULL, /* user */
                                    0, /* notificationFrames */
                                    false, /* threadCanCallJava */
                                    0, /* sessionId */
                                    AudioRecord::TRANSFER_OBTAIN) == NO_ERROR);

            // Create the effect
            effect = new AudioEffect(&effectUuidToTest, // type
                                     NULL, // UUID
                                     -1, // priority
                                     NULL, // callback
                                     0, // user
                                     record->getSessionId(), // session Id
                                     record->getInput() // io handle
                                     );


        } else {
            // Create the effect
            effect = new AudioEffect(&effectUuidToTest, // type
                                     NULL, // UUID
                                     -1, // priority
                                     NULL, // callback
                                     0, // user
                                     0, // session Id
                                     0 // io handle, 0 so using default primary output
                                     );
        }
        EXPECT_TRUE(effect != NULL);
        status_t status = effect->initCheck();
        EXPECT_FALSE(status != NO_ERROR && status != ALREADY_EXISTS);

        getParameterForEffect(effect, &effectTest);

        setParameterForEffect(effect, &effectTest);

        getParameterForEffect(effect, &effectTest, true);

        delete effect;
    }
}

template <typename T>
class ValuesVector
{
public:
    typedef ValuesVector<T> my_type;
    my_type &operator<<(const T &val)
    {
        mData.push_back(val);
        return *this;
    }
    operator std::vector<T>() const {
        return mData;
    }

private:
    std::vector<T> mData;
};

#define EXP_OK(list) std::make_pair(list, true)


INSTANTIATE_TEST_CASE_P(
    AudioEffectsSetParamsTestAll,
    TAudioEffectsSetParamTests,
    ::testing::Values(
        /**
         * AEC
         */
        EXP_OK((TTestEffectParameter<uint32_t, uint32_t>(
                    FX_IID_AEC_,
                    ValuesVector<uint32_t>() << 0,
                    ValuesVector<uint32_t>() << 88,
                    TestEffectParameterBase::readWrite,
                    TestEffectParameterBase::input))),
        /**
         * Visualizer effect test section
         */
        EXP_OK((TTestEffectParameter<uint32_t, uint32_t>(
                    SL_IID_VISUALIZATION_,
                    ValuesVector<uint32_t>() << VISUALIZER_PARAM_CAPTURE_SIZE,
                    ValuesVector<uint32_t>() << 8,
                    TestEffectParameterBase::readWrite,
                    TestEffectParameterBase::output))),
        EXP_OK((TTestEffectParameter<uint32_t, uint32_t>(
                    SL_IID_VISUALIZATION_,
                    ValuesVector<uint32_t>() << VISUALIZER_PARAM_SCALING_MODE,
                    ValuesVector<uint32_t>() << 8,
                    TestEffectParameterBase::readWrite,
                    TestEffectParameterBase::output))),
        EXP_OK((TTestEffectParameter<uint32_t, uint32_t>(
                    SL_IID_VISUALIZATION_,
                    ValuesVector<uint32_t>() << VISUALIZER_PARAM_LATENCY,
                    ValuesVector<uint32_t>() << 28,
                    TestEffectParameterBase::write,
                    TestEffectParameterBase::output))),
        EXP_OK((TTestEffectParameter<uint32_t, uint32_t>(
                    SL_IID_VISUALIZATION_,
                    ValuesVector<uint32_t>() << VISUALIZER_PARAM_MEASUREMENT_MODE,
                    ValuesVector<uint32_t>() << 33,
                    TestEffectParameterBase::readWrite,
                    TestEffectParameterBase::output))),
        /**
         * Equalizer effect test section
         */
        EXP_OK((TTestEffectParameter<uint32_t, uint16_t>(
                    SL_IID_EQUALIZER_,
                    ValuesVector<uint32_t>() <<
                    EQ_PARAM_CUR_PRESET,
                    ValuesVector<uint16_t>() << 2,
                    TestEffectParameterBase::readWrite,
                    TestEffectParameterBase::output))),
        EXP_OK((TTestEffectParameter<uint32_t, uint32_t>(
                    SL_IID_EQUALIZER_,
                    ValuesVector<uint32_t>() << EQ_PARAM_CENTER_FREQ << 2,
                    ValuesVector<uint32_t>() << 0,
                    TestEffectParameterBase::read,
                    TestEffectParameterBase::output)))
        )
    );
