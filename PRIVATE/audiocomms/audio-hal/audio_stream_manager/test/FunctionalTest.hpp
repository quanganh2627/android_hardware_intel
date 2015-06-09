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

#include <gtest/gtest.h>
#include <system/audio.h>
#include <hardware/audio.h>
#include <hardware/hardware.h>
#include <utils/Errors.h>
#include <string>

struct audio_module;

class AudioHalTest : public ::testing::Test
{
public:
    static void SetUpTestCase();

    static void TearDownTestCase();

    virtual void SetUp();

    virtual void TearDown();

    static audio_hw_device_t *getDevice()
    {
        return mDevice;
    }

    static audio_module *getAudioModule()
    {
        return mAudioModule;
    }

    void setConfig(uint32_t rate, audio_channel_mask_t mask, audio_format_t format,
                   audio_config_t &config);

    virtual ~AudioHalTest() {}

    void printHardwareModuleInformation(hw_module_t *module);

private:
    static audio_hw_device_t *openAudioDevice();

    static android::status_t closeAudioDevice(audio_hw_device_t *audio_dev);

    static audio_module *mAudioModule; /**< HW Module handle. */
    static audio_hw_device_t *mDevice; /**< HW Device handle. */
};

class AudioHalAndroidModeTest
    : public AudioHalTest,
      public ::testing::WithParamInterface<audio_mode_t>
{
};

class AudioHalMicMuteTest
    : public AudioHalTest,
      public ::testing::WithParamInterface<bool>
{
};

class AudioHalMasterMuteTest
    : public AudioHalTest,
      public ::testing::WithParamInterface<bool>
{
};

class AudioHalMasterVolumeTest
    : public AudioHalTest,
      public ::testing::WithParamInterface<float>
{
};

class AudioHalValidInputDeviceTest
    : public AudioHalTest,
      public ::testing::WithParamInterface<audio_devices_t>
{
};

class AudioHalInvalidInputDeviceTest
    : public AudioHalTest,
      public ::testing::WithParamInterface<audio_devices_t>
{
};


class AudioHalValidOutputDevicesTest
    : public AudioHalTest,
      public ::testing::WithParamInterface<audio_devices_t>
{
};

class AudioHalInvalidOutputDevicesTest
    : public AudioHalTest,
      public ::testing::WithParamInterface<audio_devices_t>
{
};


class AudioHalOutputStreamSupportedRateTest
    : public AudioHalTest,
      public ::testing::WithParamInterface<std::pair<uint32_t, bool> >
{
};

class AudioHalOutputStreamSupportedFormatTest
    : public AudioHalTest,
      public ::testing::WithParamInterface<std::pair<audio_format_t, bool> >
{
};

class AudioHalOutputStreamSupportedChannelsTest
    : public AudioHalTest,
      public ::testing::WithParamInterface<std::pair<audio_channel_mask_t, bool> >
{
};

class AudioHalValidGlobalParameterTest
    : public ::testing::WithParamInterface<std::pair<std::string, std::string> >,
      public AudioHalTest
{
};

class AudioHalInvalidGlobalParameterTest
    : public ::testing::WithParamInterface<std::pair<std::string, std::string> >,
      public AudioHalTest
{
};
