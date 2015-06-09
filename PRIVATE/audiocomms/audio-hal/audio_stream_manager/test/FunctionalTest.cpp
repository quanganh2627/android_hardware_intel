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
#include "FunctionalTest.hpp"
#include <media/AudioParameter.h>
#include <KeyValuePairs.hpp>
#include <AudioCommsAssert.hpp>
#include <utilities/Log.hpp>
#include <utils/Vector.h>

#include <iostream>
#include <algorithm>

using namespace android;
using namespace std;
using audio_comms::utilities::Log;

audio_module *AudioHalTest::mAudioModule = NULL;
audio_hw_device_t *AudioHalTest::mDevice = NULL;

void printHardwareModuleInformation(hw_module_t *module)
{
    ASSERT_TRUE(module != NULL);
    Log::Debug() << " Audio HW module name: "
                 << module->name;
    Log::Debug() << " Audio HW module author: "
                 << module->author;
    Log::Debug() << " Audio HW module API version: 0x" << std::hex
                 << module->module_api_version;
    Log::Debug() << " Audio HW module HAL API version: 0x" << std::hex
                 << module->hal_api_version;
}

void AudioHalTest::setConfig(uint32_t rate, audio_channel_mask_t mask, audio_format_t format,
                             audio_config_t &config)
{
    config.sample_rate = rate;
    config.channel_mask = mask;
    config.format = format;
}

void AudioHalTest::SetUpTestCase()
{
    Log::Debug() << "*** SetUpTestCase";
    hw_module_t *module = NULL;
    int errorCode = hw_get_module_by_class(AUDIO_HARDWARE_MODULE_ID,
                                           "primary",
                                           (const hw_module_t **)&module);
    ASSERT_EQ(0, errorCode)
        << "Failure opening audio hardware module: " << errorCode;
    ASSERT_TRUE(NULL != module)
        << "No audio hw module was set by hw_get_module";


    mAudioModule = reinterpret_cast<audio_module *>(module);
    mDevice = openAudioDevice();
    AUDIOCOMMS_ASSERT(mDevice != NULL, "Invalid Audio Device");
    ASSERT_EQ(NO_ERROR, mDevice->init_check(mDevice)) << "init_check returned !(NO_ERROR)";
}

void AudioHalTest::TearDownTestCase()
{
    Log::Debug() << "*** TearDownTestCase";
    closeAudioDevice(getDevice());
}

void AudioHalTest::SetUp()
{
    const ::testing::TestInfo *const testInfo =
        ::testing::UnitTest::GetInstance()->current_test_info();

    Log::Debug() << "*** Starting test " << testInfo->name()
                 << " in test case " << testInfo->test_case_name();
}

void AudioHalTest::TearDown()
{
    const ::testing::TestInfo *const testInfo =
        ::testing::UnitTest::GetInstance()->current_test_info();

    Log::Debug() << "*** Stopping test " << testInfo->name()
                 << " in test case " << testInfo->test_case_name();
}

audio_hw_device_t *AudioHalTest::openAudioDevice()
{
    Log::Debug() << "openAudioDevice()";

    const audio_module *audio_module = getAudioModule();
    if (audio_module == NULL) {
        Log::Debug() << "audio_module is NULL";
        return NULL;
    }

    audio_hw_device_t *device = NULL;
    int res = audio_hw_device_open
                  ((const struct hw_module_t *)audio_module,
                  &device);
    if (res != NO_ERROR || device == NULL) {
        Log::Error() << "open failed. res=" << res << ", device=0x" << std::hex << device;
        return NULL;
    }
    Log::Debug() << "Audio device 0x" << std::hex << device << " opened.";

    return device;
}

status_t AudioHalTest::closeAudioDevice(audio_hw_device_t *audio_dev)
{
    Log::Debug() << "Closing audio device " << audio_dev;

    hw_device_t *dev = reinterpret_cast<hw_device_t *>(audio_dev);
    return dev->close(dev);
}

TEST_F(AudioHalTest, testVersions)
{
    ASSERT_TRUE(getAudioModule() != NULL);

    // Module version check
    int16_t version0_1 = AUDIO_MODULE_API_VERSION_0_1;
    ASSERT_EQ(version0_1, getAudioModule()->common.module_api_version)
        << "Audio HW module version is 0x"
        << std::hex << getAudioModule()->common.module_api_version
        << ", not 2.0. (0x"
        << std::hex << AUDIO_MODULE_API_VERSION_0_1 << ")";

    // Device version check
    audio_hw_device *audioDevice = getDevice();
    Log::Debug() << "Audio device common version: "
                 << audioDevice->common.version;
    ASSERT_EQ(static_cast<uint32_t>(AUDIO_DEVICE_API_VERSION_2_0), audioDevice->common.version);
}

TEST_P(AudioHalAndroidModeTest, audio_mode_t)
{
    audio_hw_device *audioDevice = getDevice();
    audio_mode_t mode = GetParam();

    ASSERT_EQ(android::OK, audioDevice->set_mode(audioDevice, mode));
}

INSTANTIATE_TEST_CASE_P(
    AudioHalAndroidModeTestAll,
    AudioHalAndroidModeTest,
    ::testing::Values(
        AUDIO_MODE_NORMAL,
        AUDIO_MODE_RINGTONE,
        AUDIO_MODE_IN_CALL,
        AUDIO_MODE_IN_COMMUNICATION
        )
    );


TEST_P(AudioHalMicMuteTest, bool)
{
    bool micMute = GetParam();
    audio_hw_device *audioDevice = getDevice();
    bool readMicMute;
    ASSERT_EQ(android::OK, audioDevice->get_mic_mute(audioDevice, &readMicMute));

    ASSERT_EQ(android::OK, audioDevice->set_mic_mute(audioDevice, micMute));

    ASSERT_EQ(android::OK, audioDevice->get_mic_mute(audioDevice, &readMicMute));
    EXPECT_EQ(micMute, readMicMute);
}

INSTANTIATE_TEST_CASE_P(
    AudioHalMicMuteTestAll,
    AudioHalMicMuteTest,
    ::testing::Values(
        true,
        true,
        false,
        true
        )
    );

TEST_P(AudioHalMasterMuteTest, bool)
{
    bool masterMute = GetParam();
    audio_hw_device *audioDevice = getDevice();
    bool readMasterMute;
    ASSERT_EQ(android::OK, audioDevice->get_master_mute(audioDevice, &readMasterMute));

    ASSERT_EQ(android::OK, audioDevice->set_master_mute(audioDevice, masterMute));
}

// Note that master mute is not implemented within Audio HAL, just test the APIs return code
INSTANTIATE_TEST_CASE_P(
    AudioHalMasterMuteTestAll,
    AudioHalMasterMuteTest,
    ::testing::Values(
        true,
        false
        )
    );

TEST_P(AudioHalMasterVolumeTest, float)
{
    float masterVolume = GetParam();
    audio_hw_device *audioDevice = getDevice();
    float readMasterVolume;
    ASSERT_EQ(android::OK, audioDevice->get_master_volume(audioDevice, &readMasterVolume));

    ASSERT_EQ(android::OK, audioDevice->set_master_volume(audioDevice, masterVolume));
}

// Note that master volume is not implemented within Audio HAL, just test the APIs return code
INSTANTIATE_TEST_CASE_P(
    AudioHalMasterVolumeTestAll,
    AudioHalMasterVolumeTest,
    ::testing::Values(
        0.0f,
        0.5f,
        1.0f,
        1.1f
        )
    );


TEST_F(AudioHalTest, audioRecordingBufferSize)
{

    audio_hw_device *audioDevice = getDevice();
    audio_config_t config;
    setConfig(48000, AUDIO_CHANNEL_IN_STEREO, AUDIO_FORMAT_PCM_16_BIT, config);
    // Expected size if 20 ms * 48000 * frame size
    ASSERT_EQ(48u * 20 * 2 * 2, audioDevice->get_input_buffer_size(audioDevice, &config));
}

TEST_F(AudioHalTest, inputStreamSpec)
{

    audio_hw_device *audioDevice = getDevice();
    audio_config_t config;
    setConfig(48000, AUDIO_CHANNEL_IN_STEREO, AUDIO_FORMAT_PCM_16_BIT, config);
    audio_stream_in_t *inStream = NULL;
    audio_devices_t devices = static_cast<audio_devices_t>(AUDIO_DEVICE_IN_COMMUNICATION);
    audio_input_flags_t flags = AUDIO_INPUT_FLAG_NONE;
    const char *address = "dont_care";
    audio_source_t source = AUDIO_SOURCE_MIC;

    status_t status = audioDevice->open_input_stream(audioDevice, 0, devices, &config, &inStream,
                                                     flags, address, source);
    ASSERT_EQ(status, android::OK);
    ASSERT_FALSE(inStream == NULL);

    EXPECT_EQ(config.sample_rate, inStream->common.get_sample_rate(&inStream->common));
    EXPECT_EQ(config.format, inStream->common.get_format(&inStream->common));
    EXPECT_EQ(config.channel_mask, inStream->common.get_channels(&inStream->common));

    audioDevice->close_input_stream(audioDevice, inStream);
}

TEST_F(AudioHalTest, outputStreamSpec)
{

    audio_hw_device *audioDevice = getDevice();
    audio_config_t config;
    setConfig(48000, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_16_BIT, config);
    audio_output_flags_t flags = AUDIO_OUTPUT_FLAG_PRIMARY;
    audio_stream_out_t *outStream = NULL;
    audio_devices_t devices = static_cast<uint32_t>(AUDIO_DEVICE_OUT_EARPIECE);
    const char *address = "dont_care";

    status_t status = audioDevice->open_output_stream(audioDevice,
                                                      0,
                                                      devices,
                                                      flags,
                                                      &config,
                                                      &outStream,
                                                      address);
    ASSERT_EQ(status, android::OK);
    ASSERT_FALSE(outStream == NULL);

    EXPECT_EQ(config.sample_rate, outStream->common.get_sample_rate(&outStream->common));
    EXPECT_EQ(config.format, outStream->common.get_format(&outStream->common));
    EXPECT_EQ(config.channel_mask, outStream->common.get_channels(&outStream->common));

    audioDevice->close_output_stream(audioDevice, outStream);
}

TEST_P(AudioHalValidInputDeviceTest, audio_devices_t)
{
    audio_devices_t devices = GetParam();
    audio_hw_device *audioDevice = getDevice();
    audio_config_t config;
    setConfig(48000, AUDIO_CHANNEL_IN_STEREO, AUDIO_FORMAT_PCM_16_BIT, config);
    audio_stream_in_t *inStream = NULL;
    audio_input_flags_t flags = AUDIO_INPUT_FLAG_NONE;
    const char *address = "dont_care";
    audio_source_t source = AUDIO_SOURCE_MIC;

    ASSERT_EQ(android::OK,
              audioDevice->open_input_stream(audioDevice, 0, devices, &config, &inStream,
                                             flags, address, source));
    ASSERT_FALSE(inStream == NULL);

    intel_audio::KeyValuePairs valuePair;
    string returnedParam = inStream->common.get_parameters(&inStream->common,
                                                           android::AudioParameter::keyRouting);
    valuePair.add<int32_t>(android::AudioParameter::keyRouting, devices);

    Log::Debug() << "Test input device " << valuePair.toString();
    ASSERT_EQ(inStream->common.set_parameters(&inStream->common,
                                              valuePair.toString().c_str()), android::OK);

    returnedParam.clear();
    returnedParam = inStream->common.get_parameters(&inStream->common,
                                                    android::AudioParameter::keyRouting);
    EXPECT_EQ(returnedParam, valuePair.toString());

    audioDevice->close_input_stream(audioDevice, inStream);
}

INSTANTIATE_TEST_CASE_P(
    AudioHalValidInputDeviceTestAll,
    AudioHalValidInputDeviceTest,
    ::testing::Values(
        static_cast<audio_devices_t>(AUDIO_DEVICE_IN_COMMUNICATION),
        static_cast<audio_devices_t>(AUDIO_DEVICE_IN_AMBIENT),
        static_cast<audio_devices_t>(AUDIO_DEVICE_IN_BUILTIN_MIC),
        static_cast<audio_devices_t>(AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET),
        static_cast<audio_devices_t>(AUDIO_DEVICE_IN_WIRED_HEADSET),
        static_cast<audio_devices_t>(AUDIO_DEVICE_IN_VOICE_CALL),
        static_cast<audio_devices_t>(AUDIO_DEVICE_IN_BACK_MIC)
        )
    );

TEST_P(AudioHalInvalidInputDeviceTest, audio_devices_t)
{
    audio_devices_t devices = GetParam();
    audio_hw_device *audioDevice = getDevice();
    audio_config_t config;
    setConfig(48000, AUDIO_CHANNEL_IN_STEREO, AUDIO_FORMAT_PCM_16_BIT, config);
    audio_stream_in_t *inStream = NULL;
    audio_input_flags_t flags = AUDIO_INPUT_FLAG_NONE;
    const char *address = "dont_care";
    audio_source_t source = AUDIO_SOURCE_MIC;

    // Try to create the input stream with the wrong device
    ASSERT_EQ(android::BAD_VALUE,
              audioDevice->open_input_stream(audioDevice, 0, devices, &config, &inStream,
                                             flags, address, source));
    ASSERT_TRUE(inStream == NULL);

    // Open the input stream with a valid device then try to set the invalid device
    ASSERT_EQ(android::OK,
              audioDevice->open_input_stream(audioDevice, 0, AUDIO_DEVICE_IN_COMMUNICATION, &config,
                                             &inStream, flags, address, source));
    ASSERT_FALSE(inStream == NULL);

    intel_audio::KeyValuePairs valuePair;
    valuePair.add<int32_t>(android::AudioParameter::keyRouting, devices);
    ASSERT_EQ(inStream->common.set_parameters(&inStream->common,
                                              valuePair.toString().c_str()), android::BAD_VALUE);

    audioDevice->close_input_stream(audioDevice, inStream);
}

INSTANTIATE_TEST_CASE_P(
    AudioHalInvalidInputDeviceTestAll,
    AudioHalInvalidInputDeviceTest,
    ::testing::Values(
        // Only one input device supported
        static_cast<audio_devices_t>(AUDIO_DEVICE_IN_ALL),

        // Remove sign bit
        static_cast<audio_devices_t>(AUDIO_DEVICE_IN_COMMUNICATION &
                                     ~AUDIO_DEVICE_BIT_IN),

        // Output device
        static_cast<audio_devices_t>(AUDIO_DEVICE_OUT_SPEAKER)
        )
    );


TEST_P(AudioHalValidOutputDevicesTest, audio_devices_t)
{
    audio_devices_t devices = GetParam();
    audio_hw_device *audioDevice = getDevice();
    audio_output_flags_t flags = AUDIO_OUTPUT_FLAG_PRIMARY;
    audio_config_t config;
    setConfig(48000, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_16_BIT, config);
    audio_stream_out_t *outStream = NULL;
    const char *address = "dont_care";

    status_t status = audioDevice->open_output_stream(audioDevice,
                                                      0,
                                                      devices,
                                                      flags,
                                                      &config,
                                                      &outStream,
                                                      address);
    ASSERT_EQ(status, android::OK);

    ASSERT_TRUE(outStream != NULL);
    EXPECT_EQ(48000u, outStream->common.get_sample_rate(&outStream->common));
    EXPECT_EQ(AUDIO_FORMAT_PCM_16_BIT, outStream->common.get_format(&outStream->common));
    EXPECT_EQ(AUDIO_CHANNEL_OUT_STEREO, outStream->common.get_channels(&outStream->common));

    intel_audio::KeyValuePairs valuePair;
    string returnedParam = outStream->common.get_parameters(&outStream->common,
                                                            android::AudioParameter::keyRouting);
    valuePair.add<int32_t>(android::AudioParameter::keyRouting, devices);
    ASSERT_EQ(outStream->common.set_parameters(&outStream->common,
                                               valuePair.toString().c_str()), android::OK);

    returnedParam.clear();
    returnedParam = outStream->common.get_parameters(&outStream->common,
                                                     android::AudioParameter::keyRouting);
    EXPECT_EQ(returnedParam, valuePair.toString());

    audioDevice->close_output_stream(audioDevice, outStream);
}

INSTANTIATE_TEST_CASE_P(
    AudioHalValidOutputDeviceTestAll,
    AudioHalValidOutputDevicesTest,
    ::testing::Values(
        static_cast<audio_devices_t>(AUDIO_DEVICE_OUT_EARPIECE),
        static_cast<audio_devices_t>(AUDIO_DEVICE_OUT_SPEAKER),
        static_cast<audio_devices_t>(AUDIO_DEVICE_OUT_WIRED_HEADSET),
        static_cast<audio_devices_t>(AUDIO_DEVICE_OUT_WIRED_HEADPHONE),
        static_cast<audio_devices_t>(AUDIO_DEVICE_OUT_BLUETOOTH_SCO),
        static_cast<audio_devices_t>(AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET),
        static_cast<audio_devices_t>(AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT)
        )
    );

TEST_P(AudioHalInvalidOutputDevicesTest, audio_devices_t)
{
    audio_devices_t devices = GetParam();
    audio_hw_device *audioDevice = getDevice();
    audio_output_flags_t flags = AUDIO_OUTPUT_FLAG_PRIMARY;
    audio_config_t config;
    setConfig(48000, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_16_BIT, config);
    audio_stream_out_t *outStream = NULL;
    const char *address = "dont_care";

    // Try to create the output stream with the wrong device
    status_t status = audioDevice->open_output_stream(audioDevice,
                                                      0,
                                                      devices,
                                                      flags,
                                                      &config,
                                                      &outStream,
                                                      address);
    ASSERT_EQ(android::BAD_VALUE, status);
    ASSERT_TRUE(outStream == NULL);

    // Open the input stream with a valid device then try to set the invalid device
    status = audioDevice->open_output_stream(audioDevice,
                                             0,
                                             AUDIO_DEVICE_OUT_EARPIECE,
                                             flags,
                                             &config,
                                             &outStream,
                                             address);
    ASSERT_EQ(status, android::OK);
    ASSERT_TRUE(outStream != NULL);

    intel_audio::KeyValuePairs valuePair;
    valuePair.add<int32_t>(android::AudioParameter::keyRouting, devices);
    ASSERT_EQ(outStream->common.set_parameters(&outStream->common,
                                               valuePair.toString().c_str()), android::BAD_VALUE);
    audioDevice->close_output_stream(audioDevice, outStream);
}

INSTANTIATE_TEST_CASE_P(
    AudioHalInvalidOutputDevicesTestAll,
    AudioHalInvalidOutputDevicesTest,
    ::testing::Values(
        // Add sign bit
        static_cast<audio_devices_t>(AUDIO_DEVICE_OUT_WIRED_HEADPHONE | AUDIO_DEVICE_BIT_IN),

        // Input device
        static_cast<audio_devices_t>(AUDIO_DEVICE_IN_COMMUNICATION)
        )
    );

// This test ensure that {keys,value} pair sent with restarting key are taken into account
TEST_F(AudioHalTest, restartingKey)
{
    audio_hw_device *audioDevice = getDevice();

    ASSERT_EQ(android::OK, audioDevice->set_parameters(audioDevice,
                                                       "screen_state=on"));

    string returnedParam = audioDevice->get_parameters(audioDevice, "screen_state");
    EXPECT_EQ("screen_state=on", returnedParam);

    ASSERT_EQ(android::OK, audioDevice->set_parameters(audioDevice,
                                                       "restarting=true;screen_state=off"));
    returnedParam.clear();
    returnedParam = audioDevice->get_parameters(audioDevice, "screen_state");
    EXPECT_EQ("screen_state=off", returnedParam);
}

TEST_P(AudioHalValidGlobalParameterTest, key)
{
    string key = GetParam().first;
    string value = GetParam().second;

    audio_hw_device *audioDevice = getDevice();

    intel_audio::KeyValuePairs valuePair;
    string returnedParam = audioDevice->get_parameters(audioDevice, key.c_str());

    valuePair.add(key, value);
    ASSERT_EQ(android::OK, audioDevice->set_parameters(audioDevice, valuePair.toString().c_str()));
}

INSTANTIATE_TEST_CASE_P(
    AudioHalValidGlobalParameterTestAll,
    AudioHalValidGlobalParameterTest,
    ::testing::Values(
        std::make_pair("stream_flags", "16"),
        std::make_pair("stream_flags", "0"),
        std::make_pair("lpal_device", "-2147483644"),
        std::make_pair("lpal_device", "-2147483616")
        )
    );


TEST_P(AudioHalInvalidGlobalParameterTest, key)
{
    string key = GetParam().first;
    string value = GetParam().second;
    audio_hw_device *audioDevice = getDevice();

    intel_audio::KeyValuePairs valuePair;
    string returnedParam = audioDevice->get_parameters(audioDevice, key.c_str());

    valuePair.add(key, value);
    ASSERT_NE(android::OK, audioDevice->set_parameters(audioDevice, valuePair.toString().c_str()));
}

INSTANTIATE_TEST_CASE_P(
    AudioHalInvalidGlobalParameterTestAll,
    AudioHalInvalidGlobalParameterTest,
    ::testing::Values(
        std::make_pair("stream_flags", "5"),
        std::make_pair("screen_state", "neither_on_nor_off")
        )
    );
