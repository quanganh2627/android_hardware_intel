/*
 * INTEL CONFIDENTIAL
 *
 * Copyright (c) 2014 Intel Corporation All Rights Reserved.
 *
 * The source code contained or described herein and all documents related to
 * the source code ("Material") are owned by Intel Corporation or its suppliers
 * or licensors.
 *
 * Title to the Material remains with Intel Corporation or its suppliers and
 * licensors. The Material contains trade secrets and proprietary and
 * confidential information of Intel or its suppliers and licensors. The
 * Material is protected by worldwide copyright and trade secret laws and treaty
 * provisions. No part of the Material may be used, copied, reproduced,
 * modified, published, uploaded, posted, transmitted, distributed, or disclosed
 * in any way without Intel's prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 */

#pragma once

#include <gmock/gmock.h>
#include <DeviceInterface.hpp>

namespace intel_audio
{

class DeviceMock : public DeviceInterface
{
public:
    MOCK_METHOD6(openOutputStream,
                 android::status_t(audio_io_handle_t handle,
                                   audio_devices_t devices,
                                   audio_output_flags_t flags,
                                   audio_config_t &config,
                                   StreamOutInterface *&stream,
                                   const std::string &address));
    MOCK_METHOD1(closeOutputStream,
                 void(StreamOutInterface *stream));
    MOCK_METHOD7(openInputStream,
                 android::status_t(audio_io_handle_t handle,
                                   audio_devices_t devices,
                                   audio_config_t &config,
                                   StreamInInterface *&stream,
                                   audio_input_flags_t flags,
                                   const std::string &address,
                                   audio_source_t source));
    MOCK_METHOD1(closeInputStream,
                 void(StreamInInterface *stream));
    MOCK_CONST_METHOD0(initCheck,
                       android::status_t());
    MOCK_METHOD1(setVoiceVolume,
                 android::status_t(float volume));
    MOCK_METHOD1(setMasterVolume,
                 android::status_t(float volume));
    MOCK_CONST_METHOD1(getMasterVolume,
                       android::status_t(float &volume));
    MOCK_METHOD1(setMasterMute,
                 android::status_t(bool mute));
    MOCK_CONST_METHOD1(getMasterMute,
                       android::status_t(bool &muted));
    MOCK_METHOD1(setMode,
                 android::status_t(audio_mode_t mode));
    MOCK_METHOD1(setMicMute,
                 android::status_t(bool state));
    MOCK_CONST_METHOD1(getMicMute,
                       android::status_t(bool &state));
    MOCK_METHOD1(setParameters,
                 android::status_t(const std::string &keyValuePairs));
    MOCK_CONST_METHOD1(getParameters,
                       std::string(const std::string &keys));
    MOCK_CONST_METHOD1(getInputBufferSize,
                       size_t(const audio_config_t &config));
    MOCK_CONST_METHOD1(dump,
                       android::status_t(const int fd));
    MOCK_METHOD5(createAudioPatch,
                 android::status_t(uint32_t sourcesCount, const struct audio_port_config *sources,
                                   uint32_t sinksCount, const struct audio_port_config *sinks,
                                   audio_patch_handle_t *handle));
    MOCK_METHOD1(releaseAudioPatch,
                 android::status_t(audio_patch_handle_t handle));
    MOCK_CONST_METHOD1(getAudioPort,
                       android::status_t(struct audio_port &port));
    MOCK_METHOD1(setAudioPortConfig,
                 android::status_t(const struct audio_port_config &config));
};

} // namespace intel_audio
