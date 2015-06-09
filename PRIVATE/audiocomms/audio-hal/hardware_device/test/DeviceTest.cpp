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
 */

#include "DeviceTest.hpp"
#include <StreamMock.hpp>


using namespace intel_audio;
using ::testing::Test;
using ::testing::_;
using ::testing::Return;
using ::testing::DoAll;
using ::testing::SetArgReferee;
using std::string;


// Hw module related
extern "C"
{
DeviceInterface *createAudioHardware(void)
{
    return new DeviceMock();
}
}

void DeviceTest::SetUp()
{
    hw_device_t *dev;
    const char *name = AUDIO_HARDWARE_INTERFACE;

    ASSERT_EQ(HAL_MODULE_INFO_SYM.common.methods->open(NULL, name, &dev), 0);
    ASSERT_TRUE(dev != NULL);
    mDevice = reinterpret_cast<audio_hw_device_t *>(dev);
    mDeviceMock = reinterpret_cast<intel_audio::DeviceMock *>(
        reinterpret_cast<intel_audio::DeviceInterface::ext *>(dev)->obj);
}

void DeviceTest::TearDown()
{
    EXPECT_EQ(mDevice->common.close(&mDevice->common), 0);
}


/** Check audio hardware device API wrapping. */
TEST_F(DeviceTest, Device)
{
    EXPECT_CALL(*mDeviceMock, initCheck())
    .WillOnce(Return(1));
    EXPECT_EQ(mDevice->init_check(mDevice), 1);

    EXPECT_CALL(*mDeviceMock, setVoiceVolume(1.1))
    .WillOnce(Return(2));
    EXPECT_EQ(mDevice->set_voice_volume(mDevice, 1.1), 2);

    EXPECT_CALL(*mDeviceMock, setMasterVolume(1.2))
    .WillOnce(Return(3));
    EXPECT_EQ(mDevice->set_master_volume(mDevice, 1.2), 3);

    float volume;
    EXPECT_CALL(*mDeviceMock, getMasterVolume(volume))
    .WillOnce(DoAll(SetArgReferee<0>(1.5f),
                    Return(4)));
    EXPECT_EQ(mDevice->get_master_volume(mDevice, &volume), 4);
    EXPECT_FLOAT_EQ(volume, 1.5f);

    EXPECT_CALL(*mDeviceMock, setMasterMute(false))
    .WillOnce(Return(43));
    EXPECT_EQ(mDevice->set_master_mute(mDevice, false), 43);

    bool muted;
    EXPECT_CALL(*mDeviceMock, getMasterMute(muted))
    .WillOnce(DoAll(SetArgReferee<0>(true),
                    Return(8)));
    EXPECT_EQ(mDevice->get_master_mute(mDevice, &muted), 8);
    EXPECT_EQ(muted, true);

    EXPECT_CALL(*mDeviceMock, setMode(AUDIO_MODE_INVALID))
    .WillOnce(Return(5));
    EXPECT_EQ(mDevice->set_mode(mDevice, AUDIO_MODE_INVALID), 5);

    EXPECT_CALL(*mDeviceMock, setMicMute(true))
    .WillOnce(Return(7));
    EXPECT_EQ(mDevice->set_mic_mute(mDevice, true), 7);

    EXPECT_CALL(*mDeviceMock, getMicMute(muted))
    .WillOnce(DoAll(SetArgReferee<0>(false),
                    Return(8)));
    EXPECT_EQ(mDevice->get_mic_mute(mDevice, &muted), 8);
    EXPECT_EQ(muted, false);

    string kvpairs("ohhh yeahhhh");
    EXPECT_CALL(*mDeviceMock, setParameters(kvpairs))
    .WillOnce(Return(9));
    EXPECT_EQ(mDevice->set_parameters(mDevice, kvpairs.c_str()), 9);

    string keys("ohhh");
    string values("yeahhhh");
    EXPECT_CALL(*mDeviceMock, getParameters(keys))
    .WillOnce(Return(values));
    char *read_values = mDevice->get_parameters(mDevice, keys.c_str());
    EXPECT_STREQ(values.c_str(), read_values);
    free(read_values);

    audio_config_t config;
    EXPECT_CALL(*mDeviceMock, getInputBufferSize(_))
    .WillOnce(Return(10));
    EXPECT_EQ(mDevice->get_input_buffer_size(mDevice, &config), static_cast<size_t>(10));

    EXPECT_CALL(*mDeviceMock, dump(246))
    .WillOnce(Return(11));
    EXPECT_EQ(mDevice->dump(mDevice, 246), 11);

    // Routing Control APIs test
    uint32_t sourcesCount = 8;
    uint32_t sinksCount = 3;
    struct audio_port_config sources[sourcesCount];
    struct audio_port_config sinks[sinksCount];
    audio_patch_handle_t handle;

    EXPECT_CALL(*mDeviceMock, createAudioPatch(sourcesCount, sources, sinksCount, sinks, &handle))
    .WillOnce(Return(3));
    EXPECT_EQ(mDevice->create_audio_patch(mDevice, sourcesCount, sources,
                                          sinksCount, sinks, &handle),
              3);

    EXPECT_CALL(*mDeviceMock, releaseAudioPatch(handle))
    .WillOnce(Return(8));
    EXPECT_EQ(mDevice->release_audio_patch(mDevice, handle), 8);

    struct audio_port port;
    struct audio_port_config portConfig;
    EXPECT_CALL(*mDeviceMock, getAudioPort(_))
    .WillOnce(Return(16));
    EXPECT_EQ(mDevice->get_audio_port(mDevice, &port), 16);

    EXPECT_CALL(*mDeviceMock, setAudioPortConfig(_))
    .WillOnce(Return(2));
    EXPECT_EQ(mDevice->set_audio_port_config(mDevice, &portConfig), 2);
}

/** Helper to handle DeviceMock::openOutputStream mock call.
 * Check that arguments are the expected ones and return a new StreamOutMock.
 */
ACTION_P5(ActionOpenStreamOut, expHandle, expDevices, expFlags, expConfig, expAddress)
{
    audio_io_handle_t checkHandle = static_cast<audio_io_handle_t>(expHandle);
    audio_devices_t checkDevices = static_cast<audio_devices_t>(expDevices);
    audio_output_flags_t checkFlags = static_cast<audio_output_flags_t>(expFlags);
    audio_config_t checkConfig = static_cast<audio_config_t>(expConfig);
    string checkAddress = static_cast<string>(expAddress);

    audio_io_handle_t handle = static_cast<audio_io_handle_t>(arg0);
    audio_devices_t devices = static_cast<audio_devices_t>(arg1);
    audio_output_flags_t flags = static_cast<audio_output_flags_t>(arg2);
    audio_config_t config = static_cast<audio_config_t>(arg3);
    string address = static_cast<string>(arg5);

    if (checkHandle != handle
        || checkDevices != devices
        || checkFlags != flags
        || checkConfig.sample_rate != config.sample_rate
        || checkConfig.format != config.format
        || checkConfig.channel_mask != config.channel_mask
        || checkAddress != address
        ) {
        return -1;
    }
    return 0;
}

/** Helper to handle DeviceMock::closeOutputStream mock call.
 * Free the associated StreamOutMock object.
 */
ACTION(ActionCloseStreamOut)
{
    StreamOutMock *stream = static_cast<intel_audio::StreamOutMock *>(arg0);
    delete stream;
}

int empty_callback(stream_callback_event_t, void *, void *)
{
    return 0;
}

/** Check audio output stream API wrapping. */
TEST_F(DeviceTest, StreamOut)
{
    audio_io_handle_t handle = static_cast<audio_io_handle_t>(0);
    audio_devices_t devices = static_cast<audio_devices_t>(0);
    audio_output_flags_t flags = static_cast<audio_output_flags_t>(0);
    audio_config_t config;
    audio_stream_out *stream_out;
    string deviceAdress("myCard:0,myDevice:1");

    EXPECT_CALL(*mDeviceMock, openOutputStream(_, _, _, _, _, _))
    .WillOnce(DoAll(SetArgReferee<4>(new StreamOutMock()),
                    ActionOpenStreamOut(handle, devices, flags, config, deviceAdress)));
    ASSERT_EQ(mDevice->open_output_stream(mDevice, handle, devices, flags, &config,
                                          &stream_out, deviceAdress.c_str()), 0);

    StreamOutMock *out = reinterpret_cast<intel_audio::StreamOutMock *>(
        reinterpret_cast<intel_audio::StreamInterface::ext *>(stream_out)->obj.out);

    // Common API check
    audio_stream *stream = reinterpret_cast<audio_stream *>(stream_out);

    EXPECT_CALL(*out, getSampleRate())
    .WillOnce(Return(123));
    EXPECT_EQ(stream_out->common.get_sample_rate(stream), static_cast<uint32_t>(123));

    EXPECT_CALL(*out, setSampleRate(48000))
    .WillOnce(Return(1));
    EXPECT_EQ(stream_out->common.set_sample_rate(stream, 48000), 1);

    EXPECT_CALL(*out, getBufferSize())
    .WillOnce(Return(1024));
    EXPECT_EQ(stream_out->common.get_buffer_size(stream), static_cast<size_t>(1024));

    EXPECT_CALL(*out, getChannels())
    .WillOnce(Return(5));
    EXPECT_EQ(stream_out->common.get_channels(stream), static_cast<audio_channel_mask_t>(5));

    EXPECT_CALL(*out, getFormat())
    .WillOnce(Return(AUDIO_FORMAT_PCM_32_BIT));
    EXPECT_EQ(stream_out->common.get_format(stream), AUDIO_FORMAT_PCM_32_BIT);

    EXPECT_CALL(*out, setFormat(AUDIO_FORMAT_AAC))
    .WillOnce(Return(2));
    EXPECT_EQ(stream_out->common.set_format(stream, AUDIO_FORMAT_AAC), 2);

    EXPECT_CALL(*out, standby())
    .WillOnce(Return(3));
    EXPECT_EQ(stream_out->common.standby(stream), 3);

    EXPECT_CALL(*out, dump(453))
    .WillOnce(Return(4));
    EXPECT_EQ(stream_out->common.dump(stream, 453), 4);

    EXPECT_CALL(*out, getDevice())
    .WillOnce(Return(static_cast<audio_devices_t>(AUDIO_DEVICE_OUT_WIDI)));
    EXPECT_EQ(stream_out->common.get_device(stream), AUDIO_DEVICE_OUT_WIDI);

    EXPECT_CALL(*out, setDevice(AUDIO_DEVICE_IN_AMBIENT))
    .WillOnce(Return(235));
    EXPECT_EQ(stream_out->common.set_device(stream, AUDIO_DEVICE_IN_AMBIENT), 235);

    string kvpairs("woannnagain bistoufly");
    EXPECT_CALL(*out, setParameters(kvpairs))
    .WillOnce(Return(5));
    EXPECT_EQ(stream_out->common.set_parameters(stream, kvpairs.c_str()), 5);

    string keys("woannnagain");
    string values("bistoufly");
    EXPECT_CALL(*out, getParameters(keys))
    .WillOnce(Return(values));
    char *read_values = stream_out->common.get_parameters(stream, keys.c_str());
    EXPECT_STREQ(values.c_str(), read_values);
    free(read_values);

    effect_handle_t effect = reinterpret_cast<effect_handle_t>(alloca(sizeof(effect)));
    EXPECT_CALL(*out, addAudioEffect(effect))
    .WillOnce(Return(6));
    EXPECT_EQ(stream_out->common.add_audio_effect(stream, effect), 6);

    EXPECT_CALL(*out, removeAudioEffect(effect))
    .WillOnce(Return(6));
    EXPECT_EQ(stream_out->common.remove_audio_effect(stream, effect), 6);

    // Specific API check
    EXPECT_CALL(*out, getLatency())
    .WillOnce(Return(7856));
    EXPECT_EQ(stream_out->get_latency(stream_out), static_cast<uint32_t>(7856));

    EXPECT_CALL(*out, setVolume(4.3, -6.7))
    .WillOnce(Return(7));
    EXPECT_EQ(stream_out->set_volume(stream_out, 4.3, -6.7), 7);

    size_t length = 1024;
    char buffer[1024];
    // Write success
    EXPECT_CALL(*out, write(buffer, length))
    .WillOnce(DoAll(SetArgReferee<1>(static_cast<size_t>(128)),
                    Return(static_cast<android::status_t>(android::OK))));
    EXPECT_EQ(stream_out->write(stream_out, buffer, 1024), 128);
    // Write fails
    EXPECT_CALL(*out, write(buffer, length))
    .WillOnce(DoAll(SetArgReferee<1>(static_cast<size_t>(128)),
                    Return(static_cast<android::status_t>(android::BAD_VALUE))));
    EXPECT_EQ(stream_out->write(stream_out, buffer, 1024), android::BAD_VALUE);

    uint32_t frames;
    EXPECT_CALL(*out, getRenderPosition(frames))
    .WillOnce(DoAll(SetArgReferee<0>(666u),
                    Return(8)));
    EXPECT_EQ(stream_out->get_render_position(stream_out, &frames), 8);
    EXPECT_EQ(frames, 666u);

    int64_t stamp;
    EXPECT_CALL(*out, getNextWriteTimestamp(stamp))
    .WillOnce(DoAll(SetArgReferee<0>(123456789),
                    Return(9)));
    EXPECT_EQ(stream_out->get_next_write_timestamp(stream_out, &stamp), 9);
    EXPECT_EQ(stamp, 123456789);

    EXPECT_CALL(*out, flush())
    .WillOnce(Return(10));
    EXPECT_EQ(stream_out->flush(stream_out), 10);

    int cookie;
    EXPECT_CALL(*out, setCallback(empty_callback, &cookie))
    .WillOnce(Return(11));
    EXPECT_EQ(stream_out->set_callback(stream_out, empty_callback, &cookie), 11);

    EXPECT_CALL(*out, pause())
    .WillOnce(Return(12));
    EXPECT_EQ(stream_out->pause(stream_out), 12);

    EXPECT_CALL(*out, resume())
    .WillOnce(Return(13));
    EXPECT_EQ(stream_out->resume(stream_out), 13);

    EXPECT_CALL(*out, drain(AUDIO_DRAIN_EARLY_NOTIFY))
    .WillOnce(Return(14));
    EXPECT_EQ(stream_out->drain(stream_out, AUDIO_DRAIN_EARLY_NOTIFY), 14);

    uint64_t frame;
    struct timespec time = {
        0, 0
    };
    struct timespec timeReturned = {
        1234, 5647
    };
    EXPECT_CALL(*out, getPresentationPosition(frame, _))
    .WillOnce(DoAll(SetArgReferee<1>(timeReturned),
                    Return(14)));
    EXPECT_EQ(stream_out->get_presentation_position(stream_out, &frame, &time), 14);
    EXPECT_EQ(time.tv_nsec, timeReturned.tv_nsec);
    EXPECT_EQ(time.tv_sec, timeReturned.tv_sec);

    EXPECT_CALL(*mDeviceMock, closeOutputStream(out))
    .WillOnce(ActionCloseStreamOut());
    mDevice->close_output_stream(mDevice, stream_out);
}


/** Helper to handle DeviceMock::openInputStream mock call.
 * Check that arguments are the expected ones and return a new StreamInMock.
 */
ACTION_P6(ActionOpenStreamIn, expHandle, expDevices, expConfig, expFlags, expAddress, expSource)
{
    audio_io_handle_t checkHandle = static_cast<audio_io_handle_t>(expHandle);
    audio_devices_t checkDevices = static_cast<audio_devices_t>(expDevices);
    audio_config_t checkConfig = static_cast<audio_config_t>(expConfig);
    audio_input_flags_t checkFlags = static_cast<audio_input_flags_t>(expFlags);
    string checkAddress = static_cast<string>(expAddress);
    audio_source_t checkSource = static_cast<audio_source_t>(expSource);

    audio_io_handle_t handle = static_cast<audio_io_handle_t>(arg0);
    audio_devices_t devices = static_cast<audio_devices_t>(arg1);
    audio_config_t config = static_cast<audio_config_t>(arg2);
    audio_input_flags_t flags = static_cast<audio_input_flags_t>(arg4);
    string address = static_cast<string>(arg5);
    audio_source_t source = static_cast<audio_source_t>(arg6);

    if (checkHandle != handle
        || checkDevices != devices
        || checkConfig.sample_rate != config.sample_rate
        || checkConfig.format != config.format
        || checkConfig.channel_mask != config.channel_mask
        || checkFlags != flags
        || checkAddress != address
        || checkSource != source
        ) {
        return -1;
    }
    return 0;
}

/** Helper to handle DeviceMock::closeOutputStream mock call.
 * Free the associated StreamInMock object.
 */
ACTION(ActionCloseStreamIn)
{
    StreamInMock *stream = static_cast<intel_audio::StreamInMock *>(arg0);
    delete stream;
}

/** Check audio input stream API wrapping. */
TEST_F(DeviceTest, StreamIn)
{
    audio_io_handle_t handle = static_cast<audio_io_handle_t>(0);
    audio_devices_t devices = static_cast<audio_devices_t>(0);
    audio_config_t config;
    audio_stream_in *stream_in;
    audio_input_flags_t flags = AUDIO_INPUT_FLAG_NONE;
    string deviceAddress("MyCard,myDeviceId");
    audio_source_t source = AUDIO_SOURCE_MIC;

    EXPECT_CALL(*mDeviceMock, openInputStream(_, _, _, _, _, _, _))
    .WillOnce(DoAll(SetArgReferee<3>(new StreamInMock()),
                    ActionOpenStreamIn(handle, devices, config, flags, deviceAddress, source)));
    ASSERT_EQ(mDevice->open_input_stream(mDevice, handle, devices, &config, &stream_in,
                                         flags, deviceAddress.c_str(), source), 0);

    StreamInMock *in = reinterpret_cast<intel_audio::StreamInMock *>(
        reinterpret_cast<intel_audio::StreamInterface::ext *>(stream_in)->obj.in);

    // Common API check
    audio_stream *stream = reinterpret_cast<audio_stream *>(stream_in);

    EXPECT_CALL(*in, getSampleRate())
    .WillOnce(Return(123));
    EXPECT_EQ(stream_in->common.get_sample_rate(stream), static_cast<uint32_t>(123));

    EXPECT_CALL(*in, setSampleRate(48000))
    .WillOnce(Return(1));
    EXPECT_EQ(stream_in->common.set_sample_rate(stream, 48000), 1);

    EXPECT_CALL(*in, getBufferSize())
    .WillOnce(Return(1024));
    EXPECT_EQ(stream_in->common.get_buffer_size(stream), static_cast<size_t>(1024));

    EXPECT_CALL(*in, getChannels())
    .WillOnce(Return(5));
    EXPECT_EQ(stream_in->common.get_channels(stream), static_cast<audio_channel_mask_t>(5));

    EXPECT_CALL(*in, getFormat())
    .WillOnce(Return(AUDIO_FORMAT_PCM_32_BIT));
    EXPECT_EQ(stream_in->common.get_format(stream), AUDIO_FORMAT_PCM_32_BIT);

    EXPECT_CALL(*in, setFormat(AUDIO_FORMAT_AAC))
    .WillOnce(Return(2));
    EXPECT_EQ(stream_in->common.set_format(stream, AUDIO_FORMAT_AAC), 2);

    EXPECT_CALL(*in, standby())
    .WillOnce(Return(3));
    EXPECT_EQ(stream_in->common.standby(stream), 3);

    EXPECT_CALL(*in, dump(453))
    .WillOnce(Return(4));
    EXPECT_EQ(stream_in->common.dump(stream, 453), 4);

    EXPECT_CALL(*in, getDevice())
    .WillOnce(Return(static_cast<audio_devices_t>(AUDIO_DEVICE_OUT_WIDI)));
    EXPECT_EQ(stream_in->common.get_device(stream), AUDIO_DEVICE_OUT_WIDI);

    EXPECT_CALL(*in, setDevice(AUDIO_DEVICE_IN_AMBIENT))
    .WillOnce(Return(235));
    EXPECT_EQ(stream_in->common.set_device(stream, AUDIO_DEVICE_IN_AMBIENT), 235);

    string kvpairs("woannnagain bistoufly");
    EXPECT_CALL(*in, setParameters(kvpairs))
    .WillOnce(Return(5));
    EXPECT_EQ(stream_in->common.set_parameters(stream, kvpairs.c_str()), 5);

    string keys("woannnagain");
    string values("bistoufly");
    EXPECT_CALL(*in, getParameters(keys))
    .WillOnce(Return(values));
    char *read_values = stream_in->common.get_parameters(stream, keys.c_str());
    EXPECT_STREQ(values.c_str(), read_values);
    free(read_values);

    effect_handle_t effect = reinterpret_cast<effect_handle_t>(alloca(sizeof(effect)));
    EXPECT_CALL(*in, addAudioEffect(effect))
    .WillOnce(Return(6));
    EXPECT_EQ(stream_in->common.add_audio_effect(stream, effect), 6);

    EXPECT_CALL(*in, removeAudioEffect(effect))
    .WillOnce(Return(6));
    EXPECT_EQ(stream_in->common.remove_audio_effect(stream, effect), 6);

    // Specific API check
    EXPECT_CALL(*in, setGain(-3.1))
    .WillOnce(Return(7));
    EXPECT_EQ(stream_in->set_gain(stream_in, -3.1), 7);

    size_t length = 1024;
    char buffer[1024];
    // Read successes
    EXPECT_CALL(*in, read(buffer, length))
    .WillOnce(DoAll(SetArgReferee<1>(static_cast<size_t>(12)),
                    Return(static_cast<android::status_t>(android::OK))));
    EXPECT_EQ(stream_in->read(stream_in, buffer, 1024), 12);
    // Read fails
    EXPECT_CALL(*in, read(buffer, length))
    .WillOnce(DoAll(SetArgReferee<1>(static_cast<size_t>(124)),
                    Return(static_cast<android::status_t>(android::BAD_VALUE))));
    EXPECT_EQ(stream_in->read(stream_in, buffer, 1024), android::BAD_VALUE);

    EXPECT_CALL(*in, getInputFramesLost())
    .WillOnce(Return(15));
    EXPECT_EQ(stream_in->get_input_frames_lost(stream_in), static_cast<uint32_t>(15));


    EXPECT_CALL(*mDeviceMock, closeInputStream(in))
    .WillOnce(ActionCloseStreamIn());
    mDevice->close_input_stream(mDevice, stream_in);
}

TEST_F(DeviceTest, DeviceErrorHandling)
{
    /** Check audio input stream error handling. */
    audio_io_handle_t handle = static_cast<audio_io_handle_t>(0);
    audio_devices_t devices = static_cast<audio_devices_t>(0);
    audio_config_t *nullConfig = NULL;
    audio_stream_in *stream_in;
    audio_input_flags_t inputFlags = AUDIO_INPUT_FLAG_NONE;
    string deviceAddress("MyCard,myDeviceId");
    audio_source_t source = AUDIO_SOURCE_MIC;

    // Input Stream creation with a NULL configuration structure pointer
    ASSERT_EQ(mDevice->open_input_stream(mDevice, handle, devices, nullConfig, &stream_in,
                                         inputFlags, deviceAddress.c_str(), source),
              android::BAD_VALUE);

    /** Check audio output stream error handling. */
    audio_output_flags_t flags = static_cast<audio_output_flags_t>(0);
    audio_stream_out *stream_out;

    // Output Stream creation with a NULL configuration structure pointer
    ASSERT_EQ(mDevice->open_output_stream(mDevice, handle, devices, flags, nullConfig, &stream_out,
                                          deviceAddress.c_str()),
              android::BAD_VALUE);

    /** Check Get Master volume with NULL pointer error handling. */
    float *volume = NULL;
    EXPECT_EQ(mDevice->get_master_volume(mDevice, volume), android::BAD_VALUE);

    /** Check Get Master mute with NULL pointer error handling. */
    bool *muted = NULL;
    EXPECT_EQ(mDevice->get_master_mute(mDevice, muted), android::BAD_VALUE);

    /** Check Get Mic mute with NULL pointer error handling. */
    EXPECT_EQ(mDevice->get_mic_mute(mDevice, muted), android::BAD_VALUE);

    /** Check Get Input Buffer Size with NULL configuration structure pointer error handling. */
    EXPECT_EQ(mDevice->get_input_buffer_size(mDevice, nullConfig),
              static_cast<size_t>(android::BAD_VALUE));

    /** Routing control APIs error handling. */
    uint32_t numSources = 8;
    uint32_t numSinks = 3;
    struct audio_port_config *nullSources = NULL;
    struct audio_port_config *nullSinks = NULL;
    struct audio_port_config sources[numSources];
    struct audio_port_config sinks[numSinks];

    EXPECT_EQ(mDevice->create_audio_patch(mDevice, numSources, nullSources,
                                          numSinks, sinks, &handle),
              android::BAD_VALUE);
    EXPECT_EQ(mDevice->create_audio_patch(mDevice, numSources, sources,
                                          numSinks, nullSinks, &handle),
              android::BAD_VALUE);

    struct audio_port *nullPort = NULL;
    EXPECT_EQ(mDevice->get_audio_port(mDevice, nullPort), android::BAD_VALUE);

    struct audio_port_config *nullPortConfig = NULL;
    EXPECT_EQ(mDevice->set_audio_port_config(mDevice, nullPortConfig), android::BAD_VALUE);
}

TEST_F(DeviceTest, OutputStreamErrorHandling)
{
    audio_io_handle_t handle = static_cast<audio_io_handle_t>(0);
    audio_devices_t devices = static_cast<audio_devices_t>(0);
    audio_output_flags_t flags = static_cast<audio_output_flags_t>(0);
    audio_config_t config;
    audio_stream_out *stream_out;
    string deviceAddress("myCard:0,myDevice:1");

    EXPECT_CALL(*mDeviceMock, openOutputStream(_, _, _, _, _, _))
    .WillOnce(DoAll(SetArgReferee<4>(new StreamOutMock()),
                    ActionOpenStreamOut(handle, devices, flags, config, deviceAddress)));
    ASSERT_EQ(mDevice->open_output_stream(mDevice, handle, devices, flags, &config,
                                          &stream_out, deviceAddress.c_str()), 0);

    StreamOutMock *out = reinterpret_cast<intel_audio::StreamOutMock *>(
        reinterpret_cast<intel_audio::StreamInterface::ext *>(stream_out)->obj.out);

    // Common API check
    /** Check get render position with NULL pointer error handling. */
    uint32_t *nullFrames = NULL;
    EXPECT_EQ(stream_out->get_render_position(stream_out, nullFrames), android::BAD_VALUE);

    /** Check get next write timestamp with NULL pointer error handling. */
    int64_t *nullStamp = NULL;
    EXPECT_EQ(stream_out->get_next_write_timestamp(stream_out, nullStamp), android::BAD_VALUE);

    /** Check get next presentation position with NULL pointer error handling. */
    struct timespec *nullTime = NULL;
    uint64_t validFrame;
    EXPECT_EQ(stream_out->get_presentation_position(stream_out, &validFrame, nullTime),
              android::BAD_VALUE);

    struct timespec validTime;
    uint64_t *nullFrame = NULL;
    EXPECT_EQ(stream_out->get_presentation_position(stream_out, nullFrame, &validTime),
              android::BAD_VALUE);

    EXPECT_EQ(stream_out->get_presentation_position(stream_out, nullFrame, nullTime),
              android::BAD_VALUE);


    EXPECT_CALL(*mDeviceMock, closeOutputStream(out))
    .WillOnce(ActionCloseStreamOut());
    mDevice->close_output_stream(mDevice, stream_out);
}
