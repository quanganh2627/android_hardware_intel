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

#include <StreamInterface.hpp>
#include <AudioCommsAssert.hpp>


namespace intel_audio
{
//
// Helper macros
//
#define CAST_TO_EXT_STREAM(constness, stream) \
    reinterpret_cast<constness struct StreamInterface::ext *>(stream)

#define FORWARD_CALL_TO_STREAM_OUT_INSTANCE(constness, stream, function)                      \
    ({                                                                                        \
         constness struct StreamInterface::ext *_ext = CAST_TO_EXT_STREAM(constness, stream); \
         AUDIOCOMMS_ASSERT(_ext != NULL && _ext->obj.out != NULL, "Invalid device");          \
         _ext->obj.out->function;                                                             \
     })

#define FORWARD_CALL_TO_STREAM_IN_INSTANCE(constness, stream, function)                       \
    ({                                                                                        \
         constness struct StreamInterface::ext *_ext = CAST_TO_EXT_STREAM(constness, stream); \
         AUDIOCOMMS_ASSERT(_ext != NULL && _ext->obj.in != NULL, "Invalid device");           \
         _ext->obj.in->function;                                                              \
     })

#define FORWARD_CALL_TO_STREAM_INSTANCE(constness, stream, function)                          \
    ({                                                                                        \
         constness struct StreamInterface::ext *_ext = CAST_TO_EXT_STREAM(constness, stream); \
         AUDIOCOMMS_ASSERT(_ext != NULL &&                                                    \
                           _ext->dir == input ? _ext->obj.in != NULL : _ext->obj.out != NULL, \
                           "Invalid device");                                                 \
         _ext->dir == input ? _ext->obj.in->function : _ext->obj.out->function;               \
     })


//
// StreamInterface class C/C++ wrapping
//
uint32_t StreamInterface::wrapGetSampleRate(const audio_stream_t *stream)
{
    return FORWARD_CALL_TO_STREAM_INSTANCE(const, stream, getSampleRate());
}

int StreamInterface::wrapSetSampleRate(audio_stream_t *stream, uint32_t rate)
{
    return static_cast<int>(FORWARD_CALL_TO_STREAM_INSTANCE(, stream, setSampleRate(rate)));
}

size_t StreamInterface::wrapGetBufferSize(const audio_stream_t *stream)
{
    return FORWARD_CALL_TO_STREAM_INSTANCE(const, stream, getBufferSize());
}

audio_channel_mask_t StreamInterface::wrapGetChannels(const audio_stream_t *stream)
{
    return FORWARD_CALL_TO_STREAM_INSTANCE(const, stream, getChannels());
}

audio_format_t StreamInterface::wrapGetFormat(const audio_stream_t *stream)
{
    return FORWARD_CALL_TO_STREAM_INSTANCE(const, stream, getFormat());
}

int StreamInterface::wrapSetFormat(audio_stream_t *stream, audio_format_t format)
{
    return static_cast<int>(FORWARD_CALL_TO_STREAM_INSTANCE(, stream, setFormat(format)));
}

int StreamInterface::wrapStandby(audio_stream_t *stream)
{
    return static_cast<int>(FORWARD_CALL_TO_STREAM_INSTANCE(, stream, standby()));
}

int StreamInterface::wrapDump(const audio_stream_t *stream, int fd)
{
    return static_cast<int>(FORWARD_CALL_TO_STREAM_INSTANCE(const, stream, dump(fd)));
}

audio_devices_t StreamInterface::wrapGetDevice(const audio_stream_t *stream)
{
    return FORWARD_CALL_TO_STREAM_INSTANCE(const, stream, getDevice());
}

int StreamInterface::wrapSetDevice(audio_stream_t *stream, audio_devices_t device)
{
    return static_cast<int>(FORWARD_CALL_TO_STREAM_INSTANCE(, stream, setDevice(device)));
}

char *StreamInterface::wrapGetParameters(const audio_stream_t *stream, const char *keys)
{
    std::string keyList(keys);
    return strdup(FORWARD_CALL_TO_STREAM_INSTANCE(const, stream, getParameters(keyList)).c_str());
}

int StreamInterface::wrapSetParameters(audio_stream_t *stream, const char *keyValuePairs)
{
    std::string keyValuePairList(keyValuePairs);
    return static_cast<int>(FORWARD_CALL_TO_STREAM_INSTANCE(, stream,
                                                            setParameters(keyValuePairList)));
}

int StreamInterface::wrapAddAudioEffect(const audio_stream_t *stream, effect_handle_t effect)
{
    return static_cast<int>(FORWARD_CALL_TO_STREAM_INSTANCE(const, stream, addAudioEffect(effect)));
}

int StreamInterface::wrapRemoveAudioEffect(const audio_stream_t *stream, effect_handle_t effect)
{
    return static_cast<int>(FORWARD_CALL_TO_STREAM_INSTANCE(const, stream,
                                                            removeAudioEffect(effect)));
}

//
// StreamOutInterface class C/C++ wrapping
//
uint32_t StreamOutInterface::wrapGetLatency(const audio_stream_out_t *stream)
{
    return FORWARD_CALL_TO_STREAM_OUT_INSTANCE(const, stream, getLatency());
}

int StreamOutInterface::wrapSetVolume(audio_stream_out_t *stream, float left, float right)
{
    return static_cast<int>(FORWARD_CALL_TO_STREAM_OUT_INSTANCE(, stream, setVolume(left, right)));
}

ssize_t StreamOutInterface::wrapWrite(audio_stream_out_t *stream, const void *buffer, size_t bytes)
{
    android::status_t status = FORWARD_CALL_TO_STREAM_OUT_INSTANCE(, stream, write(buffer, bytes));
    return (status == android::OK) ? static_cast<ssize_t>(bytes) : static_cast<ssize_t>(status);
}

int StreamOutInterface::wrapGetRenderPosition(const audio_stream_out_t *stream,
                                              uint32_t *dspFrames)
{
    if (dspFrames == NULL) {
        return static_cast<int>(android::BAD_VALUE);
    }
    return static_cast<int>(FORWARD_CALL_TO_STREAM_OUT_INSTANCE(const, stream,
                                                                getRenderPosition(*dspFrames)));
}

int StreamOutInterface::wrapGetNextWriteTimestamp(const audio_stream_out_t *stream,
                                                  int64_t *timestamp)
{
    if (timestamp == NULL) {
        return static_cast<int>(android::BAD_VALUE);
    }
    return static_cast<int>(FORWARD_CALL_TO_STREAM_OUT_INSTANCE(const, stream,
                                                                getNextWriteTimestamp(*timestamp)));
}

int StreamOutInterface::wrapFlush(audio_stream_out_t *stream)
{
    return static_cast<int>(FORWARD_CALL_TO_STREAM_OUT_INSTANCE(, stream, flush()));
}

int StreamOutInterface::wrapSetCallback(audio_stream_out_t *stream,
                                        stream_callback_t callback, void *cookie)
{
    return static_cast<int>(FORWARD_CALL_TO_STREAM_OUT_INSTANCE(, stream,
                                                                setCallback(callback, cookie)));
}

int StreamOutInterface::wrapPause(audio_stream_out_t *stream)
{
    return static_cast<int>(FORWARD_CALL_TO_STREAM_OUT_INSTANCE(, stream, pause()));
}

int StreamOutInterface::wrapResume(audio_stream_out_t *stream)
{
    return static_cast<int>(FORWARD_CALL_TO_STREAM_OUT_INSTANCE(, stream, resume()));
}

int StreamOutInterface::wrapDrain(audio_stream_out_t *stream, audio_drain_type_t type)
{
    return static_cast<int>(FORWARD_CALL_TO_STREAM_OUT_INSTANCE(, stream, drain(type)));
}

int StreamOutInterface::wrapGetPresentationPosition(const audio_stream_out_t *stream,
                                                    uint64_t *frames, struct timespec *timestamp)
{
    if (frames == NULL || timestamp == NULL) {
        return static_cast<int>(android::BAD_VALUE);
    }
    return static_cast<int>(
        FORWARD_CALL_TO_STREAM_OUT_INSTANCE(const, stream,
                                            getPresentationPosition(*frames, *timestamp)));
}


//
// StreamInInterface class C/C++ wrapping
//
int StreamInInterface::wrapSetGain(audio_stream_in_t *stream, float gain)
{
    return FORWARD_CALL_TO_STREAM_IN_INSTANCE(, stream, setGain(gain));
}

ssize_t StreamInInterface::wrapRead(audio_stream_in_t *stream, void *buffer, size_t bytes)
{
    android::status_t status = FORWARD_CALL_TO_STREAM_IN_INSTANCE(, stream, read(buffer, bytes));
    return (status == android::OK) ? static_cast<ssize_t>(bytes) : static_cast<ssize_t>(status);
}

uint32_t StreamInInterface::wrapGetInputFramesLost(audio_stream_in_t *stream)
{
    return FORWARD_CALL_TO_STREAM_IN_INSTANCE(, stream, getInputFramesLost());
}


} // namespace intel_audio
