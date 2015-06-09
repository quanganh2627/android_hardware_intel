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

#pragma once

#include <StreamInterface.hpp>
#include <gmock/gmock.h>


namespace intel_audio
{

class StreamOutMock : public StreamOutInterface
{
public:
    MOCK_CONST_METHOD0(getSampleRate,
                       uint32_t());
    MOCK_METHOD1(setSampleRate,
                 android::status_t(uint32_t rate));
    MOCK_CONST_METHOD0(getBufferSize,
                       size_t());
    MOCK_CONST_METHOD0(getChannels,
                       audio_channel_mask_t());
    MOCK_CONST_METHOD0(getFormat,
                       audio_format_t());
    MOCK_METHOD1(setFormat,
                 android::status_t(audio_format_t format));
    MOCK_METHOD0(standby,
                 android::status_t());
    MOCK_CONST_METHOD1(dump,
                       android::status_t(int fd));
    MOCK_CONST_METHOD0(getDevice,
                       audio_devices_t());
    MOCK_METHOD1(setDevice,
                 android::status_t(audio_devices_t device));
    MOCK_CONST_METHOD1(getParameters,
                       std::string(const std::string &keys));
    MOCK_METHOD1(setParameters,
                 android::status_t(const std::string &keyValuePairs));
    MOCK_METHOD1(addAudioEffect,
                 android::status_t(effect_handle_t effect));
    MOCK_METHOD1(removeAudioEffect,
                 android::status_t(effect_handle_t effect));
    MOCK_METHOD0(getLatency,
                 uint32_t());
    MOCK_METHOD2(setVolume,
                 android::status_t(float left, float right));
    MOCK_METHOD2(write,
                 android::status_t(const void *buffer, size_t &bytes));
    MOCK_CONST_METHOD1(getRenderPosition,
                       android::status_t(uint32_t &dspFrames));
    MOCK_CONST_METHOD1(getNextWriteTimestamp,
                       android::status_t(int64_t &timestamp));
    MOCK_METHOD0(flush,
                 android::status_t());
    MOCK_METHOD2(setCallback,
                 android::status_t(stream_callback_t callback, void *cookie));
    MOCK_METHOD0(pause,
                 android::status_t());
    MOCK_METHOD0(resume,
                 android::status_t());
    MOCK_METHOD1(drain,
                 android::status_t(audio_drain_type_t type));
    MOCK_CONST_METHOD2(getPresentationPosition,
                       android::status_t(uint64_t &frames, struct timespec &timestamp));
};

class StreamInMock : public StreamInInterface
{
public:
    StreamInMock() {}
    virtual ~StreamInMock() {}

    MOCK_CONST_METHOD0(getSampleRate,
                       uint32_t());
    MOCK_METHOD1(setSampleRate,
                 android::status_t(uint32_t rate));
    MOCK_CONST_METHOD0(getBufferSize,
                       size_t());
    MOCK_CONST_METHOD0(getChannels,
                       audio_channel_mask_t());
    MOCK_CONST_METHOD0(getFormat,
                       audio_format_t());
    MOCK_METHOD1(setFormat,
                 android::status_t(audio_format_t format));
    MOCK_METHOD0(standby,
                 android::status_t());
    MOCK_CONST_METHOD1(dump,
                       android::status_t(int fd));
    MOCK_CONST_METHOD0(getDevice,
                       audio_devices_t());
    MOCK_METHOD1(setDevice,
                 android::status_t(audio_devices_t device));
    MOCK_CONST_METHOD1(getParameters,
                       std::string(const std::string &keys));
    MOCK_METHOD1(setParameters,
                 android::status_t(const std::string &keyValuePairs));
    MOCK_METHOD1(addAudioEffect,
                 android::status_t(effect_handle_t effect));
    MOCK_METHOD1(removeAudioEffect,
                 android::status_t(effect_handle_t effect));

    MOCK_METHOD1(setGain,
                 android::status_t(float gain));
    MOCK_METHOD2(read,
                 android::status_t(void *buffer, size_t &bytes));
    MOCK_CONST_METHOD0(getInputFramesLost,
                       uint32_t());
};

} // namespace intel_audio
