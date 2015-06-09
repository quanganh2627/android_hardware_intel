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

#include <DeviceInterface.hpp>
#include <AudioCommsAssert.hpp>


namespace intel_audio
{

extern "C"
{
DeviceInterface *createAudioHardware(void);

static struct hw_module_methods_t audio_module_methods = {
open: DeviceInterface::wrapOpen
};

struct audio_module HAL_MODULE_INFO_SYM = {
common:
{
    tag: HARDWARE_MODULE_TAG,
    module_api_version: AUDIO_MODULE_API_VERSION_0_1,
    hal_api_version: HARDWARE_HAL_API_VERSION,
    id: AUDIO_HARDWARE_MODULE_ID,
    name: "Intel Audio HW HAL",
    author: "The Android Open Source Project",
    methods: &audio_module_methods,
    dso: NULL,
    reserved:
    {
        0
    },
}
};
}; // extern "C"


//
// Helper macros
//
#define FORWARD_CALL_TO_DEV_INSTANCE(constness, device, function)                       \
    ({                                                                                  \
         constness struct ext *_ext = reinterpret_cast<constness struct ext *>(device); \
         AUDIOCOMMS_ASSERT(_ext != NULL && _ext->obj != NULL, "Invalid device");        \
         _ext->obj->function;                                                           \
     })


//
// DeviceInterface class C/C++ wrapping
//
int DeviceInterface::wrapOpen(const hw_module_t *module, const char *name, hw_device_t **device)
{
    struct ext *ext_dev;

    if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0) {
        return static_cast<int>(android::BAD_VALUE);
    }

    ext_dev = (struct ext *)calloc(1, sizeof(*ext_dev));
    if (ext_dev == NULL) {
        return -ENOMEM;
    }

    ext_dev->device.common.tag = HARDWARE_DEVICE_TAG;
    ext_dev->device.common.version = AUDIO_DEVICE_API_VERSION_2_0;
    ext_dev->device.common.module = const_cast<hw_module_t *>(module);
    ext_dev->device.common.close = DeviceInterface::wrapClose;

    ext_dev->device.init_check = DeviceInterface::wrapInitCheck;
    ext_dev->device.set_voice_volume = DeviceInterface::wrapSetVoiceVolume;
    ext_dev->device.set_master_volume = DeviceInterface::wrapSetMasterVolume;
    ext_dev->device.get_master_volume = DeviceInterface::wrapGetMasterVolume;
    ext_dev->device.set_master_mute = DeviceInterface::wrapSetMasterMute;
    ext_dev->device.get_master_mute = DeviceInterface::wrapGetMasterMute;
    ext_dev->device.set_mode = DeviceInterface::wrapSetMode;
    ext_dev->device.set_mic_mute = DeviceInterface::wrapSetMicMute;
    ext_dev->device.get_mic_mute = DeviceInterface::wrapGetMicMute;
    ext_dev->device.set_parameters = DeviceInterface::wrapSetParameters;
    ext_dev->device.get_parameters = DeviceInterface::wrapGetParameters;
    ext_dev->device.get_input_buffer_size = DeviceInterface::wrapGetInputBufferSize;
    ext_dev->device.open_output_stream = DeviceInterface::wrapOpenOutputStream;
    ext_dev->device.close_output_stream = DeviceInterface::wrapCloseOutputStream;
    ext_dev->device.open_input_stream = DeviceInterface::wrapOpenInputStream;
    ext_dev->device.close_input_stream = DeviceInterface::wrapCloseInputStream;
    ext_dev->device.dump = DeviceInterface::wrapDump;
    ext_dev->device.create_audio_patch = DeviceInterface::wrapCreateAudioPatch;
    ext_dev->device.release_audio_patch = DeviceInterface::wrapReleaseAudioPatch;
    ext_dev->device.get_audio_port = DeviceInterface::wrapGetAudioPort;
    ext_dev->device.set_audio_port_config = DeviceInterface::wrapSetAudioPortConfig;

    ext_dev->obj = createAudioHardware();
    if (ext_dev->obj == NULL) {
        free(ext_dev);
        return -EIO;
    }

    AUDIOCOMMS_ASSERT(device != NULL, "Invalid device argument");
    *device = reinterpret_cast<hw_device_t *>(ext_dev);
    return static_cast<int>(android::OK);
}

int DeviceInterface::wrapClose(hw_device_t *device)
{
    struct ext *ext_dev = reinterpret_cast<struct ext *>(device);
    AUDIOCOMMS_ASSERT(ext_dev != NULL, "Invalid device argument");

    delete ext_dev->obj;
    ext_dev->obj = NULL;

    free(ext_dev);
    return static_cast<int>(android::OK);
}

int DeviceInterface::wrapOpenOutputStream(struct audio_hw_device *dev,
                                          audio_io_handle_t handle,
                                          audio_devices_t devices,
                                          audio_output_flags_t flags,
                                          audio_config_t *config,
                                          audio_stream_out_t **stream_out,
                                          const char *address)
{
    if (config == NULL) {
        return static_cast<int>(android::BAD_VALUE);
    }

    struct StreamInterface::ext *ext_stream =
        (struct StreamInterface::ext *)calloc(1, sizeof(*ext_stream));
    if (!ext_stream) {
        return static_cast<int>(android::NO_MEMORY);
    }

    struct ext *ext_dev = reinterpret_cast<struct ext *>(dev);
    AUDIOCOMMS_ASSERT(ext_dev != NULL, "Invalid device");

    std::string deviceAddress(address);
    int error = static_cast<int>(
        ext_dev->obj->openOutputStream(handle, devices, flags, *config, ext_stream->obj.out,
                                       deviceAddress));
    if (error || ext_stream->obj.out == NULL) {
        free(ext_stream);
        *stream_out = NULL;
        return error;
    }

    ext_stream->dir = StreamInterface::output;

    ext_stream->stream.out.common.get_sample_rate = StreamInterface::wrapGetSampleRate;
    ext_stream->stream.out.common.set_sample_rate = StreamInterface::wrapSetSampleRate;
    ext_stream->stream.out.common.get_buffer_size = StreamInterface::wrapGetBufferSize;
    ext_stream->stream.out.common.get_channels = StreamInterface::wrapGetChannels;
    ext_stream->stream.out.common.get_format = StreamInterface::wrapGetFormat;
    ext_stream->stream.out.common.set_format = StreamInterface::wrapSetFormat;
    ext_stream->stream.out.common.standby = StreamInterface::wrapStandby;
    ext_stream->stream.out.common.dump = StreamInterface::wrapDump;
    ext_stream->stream.out.common.get_device = StreamInterface::wrapGetDevice;
    ext_stream->stream.out.common.set_device = StreamInterface::wrapSetDevice;
    ext_stream->stream.out.common.set_parameters = StreamInterface::wrapSetParameters;
    ext_stream->stream.out.common.get_parameters = StreamInterface::wrapGetParameters;
    ext_stream->stream.out.common.add_audio_effect = StreamInterface::wrapAddAudioEffect;
    ext_stream->stream.out.common.remove_audio_effect = StreamInterface::wrapRemoveAudioEffect;

    ext_stream->stream.out.get_latency = StreamOutInterface::wrapGetLatency;
    ext_stream->stream.out.set_volume = StreamOutInterface::wrapSetVolume;
    ext_stream->stream.out.write = StreamOutInterface::wrapWrite;
    ext_stream->stream.out.get_render_position = StreamOutInterface::wrapGetRenderPosition;
    ext_stream->stream.out.get_next_write_timestamp = StreamOutInterface::wrapGetNextWriteTimestamp;
    ext_stream->stream.out.flush = StreamOutInterface::wrapFlush;
    ext_stream->stream.out.set_callback = StreamOutInterface::wrapSetCallback;
    ext_stream->stream.out.pause = StreamOutInterface::wrapPause;
    ext_stream->stream.out.resume = StreamOutInterface::wrapResume;
    ext_stream->stream.out.drain = StreamOutInterface::wrapDrain;
    ext_stream->stream.out.get_presentation_position =
        StreamOutInterface::wrapGetPresentationPosition;

    *stream_out = &ext_stream->stream.out;
    return static_cast<int>(android::OK);
}

void DeviceInterface::wrapCloseOutputStream(struct audio_hw_device *dev,
                                            audio_stream_out_t *stream)
{
    struct ext *ext_dev = reinterpret_cast<struct ext *>(dev);
    AUDIOCOMMS_ASSERT(ext_dev != NULL && ext_dev->obj != NULL, "Invalid device");
    struct StreamInterface::ext *ext_stream =
        reinterpret_cast<struct StreamInterface::ext *>(stream);
    AUDIOCOMMS_ASSERT(ext_stream != NULL && ext_stream->obj.out != NULL, "Invalid stream");

    ext_dev->obj->closeOutputStream(ext_stream->obj.out);
    free(ext_stream);
}

int DeviceInterface::wrapOpenInputStream(struct audio_hw_device *dev,
                                         audio_io_handle_t handle,
                                         audio_devices_t devices,
                                         audio_config_t *config,
                                         audio_stream_in_t **stream_in,
                                         audio_input_flags_t flags,
                                         const char *address,
                                         audio_source_t source)
{
    if (config == NULL) {
        return static_cast<int>(android::BAD_VALUE);
    }

    struct StreamInterface::ext *ext_stream =
        (struct StreamInterface::ext *)calloc(1, sizeof(*ext_stream));
    if (ext_stream == NULL) {
        return static_cast<int>(android::NO_MEMORY);
    }

    struct ext *ext_dev = reinterpret_cast<struct ext *>(dev);
    AUDIOCOMMS_ASSERT(ext_dev != NULL, "Invalid device");
    std::string deviceAddress(address);
    int error = static_cast<int>(
        ext_dev->obj->openInputStream(handle, devices, *config,
                                      ext_stream->obj.in, flags, deviceAddress, source));
    if (ext_stream->obj.in == NULL) {
        free(ext_stream);
        *stream_in = NULL;
        return error;
    }

    ext_stream->dir = StreamInterface::input;

    ext_stream->stream.in.common.get_sample_rate = StreamInterface::wrapGetSampleRate;
    ext_stream->stream.in.common.set_sample_rate = StreamInterface::wrapSetSampleRate;
    ext_stream->stream.in.common.get_buffer_size = StreamInterface::wrapGetBufferSize;
    ext_stream->stream.in.common.get_channels = StreamInterface::wrapGetChannels;
    ext_stream->stream.in.common.get_format = StreamInterface::wrapGetFormat;
    ext_stream->stream.in.common.set_format = StreamInterface::wrapSetFormat;
    ext_stream->stream.in.common.standby = StreamInterface::wrapStandby;
    ext_stream->stream.in.common.dump = StreamInterface::wrapDump;
    ext_stream->stream.in.common.get_device = StreamInterface::wrapGetDevice;
    ext_stream->stream.in.common.set_device = StreamInterface::wrapSetDevice;
    ext_stream->stream.in.common.set_parameters = StreamInterface::wrapSetParameters;
    ext_stream->stream.in.common.get_parameters = StreamInterface::wrapGetParameters;
    ext_stream->stream.in.common.add_audio_effect = StreamInterface::wrapAddAudioEffect;
    ext_stream->stream.in.common.remove_audio_effect = StreamInterface::wrapRemoveAudioEffect;

    ext_stream->stream.in.set_gain = StreamInInterface::wrapSetGain;
    ext_stream->stream.in.read = StreamInInterface::wrapRead;
    ext_stream->stream.in.get_input_frames_lost = StreamInInterface::wrapGetInputFramesLost;

    *stream_in = &ext_stream->stream.in;
    return static_cast<int>(android::OK);
}

void DeviceInterface::wrapCloseInputStream(struct audio_hw_device *dev,
                                           audio_stream_in_t *stream)
{
    struct ext *ext_dev = reinterpret_cast<struct ext *>(dev);
    AUDIOCOMMS_ASSERT(ext_dev != NULL && ext_dev->obj != NULL, "Invalid device");
    struct StreamInterface::ext *ext_stream =
        reinterpret_cast<struct StreamInterface::ext *>(stream);
    AUDIOCOMMS_ASSERT(ext_stream != NULL && ext_stream->obj.in != NULL, "Invalid stream");

    ext_dev->obj->closeInputStream(ext_stream->obj.in);
    free(ext_stream);
}

int DeviceInterface::wrapInitCheck(const struct audio_hw_device *dev)
{
    return static_cast<int>(FORWARD_CALL_TO_DEV_INSTANCE(const, dev, initCheck()));
}

int DeviceInterface::wrapSetVoiceVolume(struct audio_hw_device *dev, float volume)
{
    return static_cast<int>(FORWARD_CALL_TO_DEV_INSTANCE(, dev, setVoiceVolume(volume)));
}

int DeviceInterface::wrapSetMasterVolume(struct audio_hw_device *dev, float volume)
{
    return static_cast<int>(FORWARD_CALL_TO_DEV_INSTANCE(, dev, setMasterVolume(volume)));
}

int DeviceInterface::wrapGetMasterVolume(struct audio_hw_device *dev, float *volume)
{
    if (volume == NULL) {
        return static_cast<int>(android::BAD_VALUE);
    }
    return static_cast<int>(FORWARD_CALL_TO_DEV_INSTANCE(const, dev, getMasterVolume(*volume)));
}

int DeviceInterface::wrapSetMasterMute(struct audio_hw_device *dev, bool mute)
{
    return static_cast<int>(FORWARD_CALL_TO_DEV_INSTANCE(, dev, setMasterMute(mute)));
}

int DeviceInterface::wrapGetMasterMute(struct audio_hw_device *dev, bool *muted)
{
    if (muted == NULL) {
        return static_cast<int>(android::BAD_VALUE);
    }
    return static_cast<int>(FORWARD_CALL_TO_DEV_INSTANCE(const, dev, getMasterMute(*muted)));
}

int DeviceInterface::wrapSetMode(struct audio_hw_device *dev, audio_mode_t mode)
{
    return static_cast<int>(FORWARD_CALL_TO_DEV_INSTANCE(, dev, setMode(mode)));
}

int DeviceInterface::wrapSetMicMute(struct audio_hw_device *dev, bool state)
{
    return static_cast<int>(FORWARD_CALL_TO_DEV_INSTANCE(, dev, setMicMute(state)));
}

int DeviceInterface::wrapGetMicMute(const struct audio_hw_device *dev, bool *state)
{
    if (state == NULL) {
        return static_cast<int>(android::BAD_VALUE);
    }
    return static_cast<int>(FORWARD_CALL_TO_DEV_INSTANCE(const, dev, getMicMute(*state)));
}

int DeviceInterface::wrapSetParameters(struct audio_hw_device *dev, const char *keyValuePairs)
{
    std::string keyValuePairList(keyValuePairs);
    return static_cast<int>(FORWARD_CALL_TO_DEV_INSTANCE(, dev, setParameters(keyValuePairList)));
}

char *DeviceInterface::wrapGetParameters(const struct audio_hw_device *dev, const char *keys)
{
    std::string keyList(keys);
    return strdup(FORWARD_CALL_TO_DEV_INSTANCE(const, dev, getParameters(keyList)).c_str());
}

size_t DeviceInterface::wrapGetInputBufferSize(const struct audio_hw_device *dev,
                                               const audio_config_t *config)
{
    if (config == NULL) {
        return static_cast<int>(android::BAD_VALUE);
    }
    return FORWARD_CALL_TO_DEV_INSTANCE(const, dev, getInputBufferSize(*config));
}

int DeviceInterface::wrapDump(const struct audio_hw_device *dev, int fd)
{
    return static_cast<int>(FORWARD_CALL_TO_DEV_INSTANCE(const, dev, dump(fd)));
}

int DeviceInterface::wrapCreateAudioPatch(struct audio_hw_device *dev,
                                          unsigned int num_sources,
                                          const struct audio_port_config *sources,
                                          unsigned int num_sinks,
                                          const struct audio_port_config *sinks,
                                          audio_patch_handle_t *handle)
{
    if (sources == NULL || sinks == NULL) {
        return static_cast<int>(android::BAD_VALUE);
    }
    return static_cast<int>(FORWARD_CALL_TO_DEV_INSTANCE(, dev, createAudioPatch(num_sources,
                                                                                 sources,
                                                                                 num_sinks,
                                                                                 sinks,
                                                                                 handle)));
}

int DeviceInterface::wrapReleaseAudioPatch(struct audio_hw_device *dev, audio_patch_handle_t handle)
{
    return static_cast<int>(FORWARD_CALL_TO_DEV_INSTANCE(, dev, releaseAudioPatch(handle)));
}

int DeviceInterface::wrapGetAudioPort(struct audio_hw_device *dev, struct audio_port *port)
{
    if (port == NULL) {
        return static_cast<int>(android::BAD_VALUE);
    }
    return static_cast<int>(FORWARD_CALL_TO_DEV_INSTANCE(const, dev, getAudioPort(*port)));
}

int DeviceInterface::wrapSetAudioPortConfig(struct audio_hw_device *dev,
                                            const struct audio_port_config *config)
{
    if (config == NULL) {
        return static_cast<int>(android::BAD_VALUE);
    }
    return static_cast<int>(FORWARD_CALL_TO_DEV_INSTANCE(, dev, setAudioPortConfig(*config)));
}

} // namespace intel_audio
