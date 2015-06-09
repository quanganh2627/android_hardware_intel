/*
 * INTEL CONFIDENTIAL
 * Copyright (c) 2013-2014 Intel
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
#pragma once

#include <InterfaceProviderImpl.h>
#include <IStreamInterface.hpp>
#include <audio_effects/effect_aec.h>
#include <audio_utils/echo_reference.h>
#include <hardware/audio_effect.h>
#include <hardware/hardware.h>
#include <DeviceInterface.hpp>
#include <AudioUtils.hpp>
#include <SampleSpec.hpp>
#include <NonCopyable.hpp>
#include <AudioCommsAssert.hpp>
#include <string>

struct echo_reference_itfe;

namespace intel_audio
{

class CAudioConversion;
class StreamOut;
class StreamIn;
class Stream;
class AudioPlatformState;
class AudioParameterHandler;

class Device : public DeviceInterface,
               private audio_comms::utilities::NonCopyable
{
public:
    Device();
    virtual ~Device();

    // From AudioHwDevice
    virtual android::status_t openOutputStream(audio_io_handle_t handle,
                                               audio_devices_t devices,
                                               audio_output_flags_t flags,
                                               audio_config_t &config,
                                               StreamOutInterface *&stream,
                                               const std::string &address);
    virtual android::status_t openInputStream(audio_io_handle_t handle,
                                              audio_devices_t devices,
                                              audio_config_t &config,
                                              StreamInInterface *&stream,
                                              audio_input_flags_t flags,
                                              const std::string &address,
                                              audio_source_t source);
    virtual void closeOutputStream(StreamOutInterface *stream);
    virtual void closeInputStream(StreamInInterface *stream);
    virtual android::status_t initCheck() const;
    virtual android::status_t setVoiceVolume(float volume);
    /** @note API not implemented in our Audio HAL */
    virtual android::status_t setMasterVolume(float /* volume */)
    {
        return android::INVALID_OPERATION;
    }
    /** @note API not implemented in our Audio HAL */
    virtual android::status_t getMasterVolume(float &) const
    {
        return android::INVALID_OPERATION;
    }
    /** @note API not implemented in our Audio HAL */
    virtual android::status_t setMasterMute(bool /*mute*/)
    {
        return android::INVALID_OPERATION;
    }
    /** @note API not implemented in our Audio HAL */
    virtual android::status_t getMasterMute(bool &/*muted*/) const
    {
        return android::INVALID_OPERATION;
    }
    virtual android::status_t setMode(audio_mode_t mode);
    virtual android::status_t setMicMute(bool mute);
    virtual android::status_t getMicMute(bool &muted) const;
    virtual android::status_t setParameters(const std::string &keyValuePairs);
    virtual std::string getParameters(const std::string &keys) const;
    virtual size_t getInputBufferSize(const audio_config_t &config) const;
    /** @note API not implemented in our Audio HAL */
    virtual android::status_t dump(const int /* fd */) const { return android::OK; }
    /** @note Routing control API not implemented in our Audio HAL */
    virtual android::status_t createAudioPatch(unsigned int /*sourcesCount*/,
                                               const struct audio_port_config */*sources*/,
                                               unsigned int /*sinksCount*/,
                                               const struct audio_port_config */*sinks*/,
                                               audio_patch_handle_t */*handle*/)
    {
        return android::INVALID_OPERATION;
    }
    /** @note Routing control API not implemented in our Audio HAL */
    virtual android::status_t releaseAudioPatch(audio_patch_handle_t /*handle*/)
    {
        return android::INVALID_OPERATION;
    }
    /** @note Routing control API not implemented in our Audio HAL */
    virtual android::status_t getAudioPort(struct audio_port &/*port*/) const
    {
        return android::INVALID_OPERATION;
    }
    /** @note Routing control API not implemented in our Audio HAL */
    virtual android::status_t setAudioPortConfig(const struct audio_port_config &/*config*/)
    {
        return android::INVALID_OPERATION;
    }

protected:
    /**
     * Returns the stream interface of the route manager.
     * As a AudioHAL creator must ensure HAL is started to performs any action on AudioHAL,
     * this function leaves if the stream interface was not created successfully upon start of HAL.
     *
     * @return stream interface of the route manager.
     */
    IStreamInterface *getStreamInterface()
    {
        AUDIOCOMMS_ASSERT(mStreamInterface != NULL, "Invalid stream interface");
        return mStreamInterface;
    }

    friend class StreamOut;
    friend class StreamIn;
    friend class Stream;

    /**
     * Print debug information from hw registers files.
     * Dump on console platform hw debug files.
     */
    void printPlatformFwErrorInfo();

private:
    /**
     * Get the android telephony mode.
     *
     * @return android telephony mode, members of the mother class AudioHardwareBase
     *                                 so coding style is the one from this class...
     */
    int mode() const
    {
        return mMode;
    }

    /**
     * Handle any setParameters called from the streams.
     * It may result in a routing reconsideration.
     *
     * @param[in] stream Stream from which the setParameters is originated.
     * @param[in] keyValuePairs: one or more value pair "name=value", semicolon-separated.
     *
     * @return OK if parameters successfully taken into account, error code otherwise.
     */
    android::status_t setStreamParameters(Stream *stream, const std::string &keyValuePairs);

    /**
     * Handle a stream start request.
     * It results in a routing reconsideration. It must be SYNCHRONOUS to avoid loosing samples.
     *
     * @param[in] stream requester Stream.
     *
     * @return OK if stream started successfully, error code otherwise.
     */
    android::status_t startStream(Stream *stream);

    /**
     * Handle a stream stop request.
     * It results in a routing reconsideration.
     *
     * @param[in] stream requester Stream.
     *
     * @return OK if stream stopped successfully, error code otherwise.
     */
    android::status_t stopStream(Stream *stream);

    /**
     * Handle a change of requested effect.
     * It results in a routing reconsideration.
     */
    void updateRequestedEffect();

    /**
     * Resets an echo reference.
     *
     * @param[in] reference: echo reference to reset.
     */
    void resetEchoReference(struct echo_reference_itfe *reference);

    /**
     * Get the echo reference for AEC effect.
     * Called by an input stream on which SW echo cancellation is performed.
     * Audio HAL needs to provide the echo reference (output stream) to the input stream.
     *
     * @param[in] inputSampleSpec: input stream sample specification.
     *
     * @return valid echo reference is found, NULL otherwise.
     */
    struct echo_reference_itfe *getEchoReference(const SampleSpec &inputSampleSpec);

    struct echo_reference_itfe *mEchoReference; /**< Echo reference to use for AEC effect. */

    AudioPlatformState *mPlatformState; /**< Platform state pointer. */

    AudioParameterHandler *mAudioParameterHandler; /**< backup and restore audio parameters. */

    IStreamInterface *mStreamInterface; /**< Route Manager Stream Interface pointer. */

    audio_mode_t mMode; /**< Android telephony mode. */

    static const char *const mDefaultGainPropName; /**< Gain property name. */
    static const float mDefaultGainValue; /**< Default gain value if empty property. */
    static const char *const mRouteLibPropName;  /**< Route Manager name property. */
    static const char *const mRouteLibPropDefaultValue;  /**< Route Manager lib default value. */
    static const char *const mRestartingKey; /**< Restart key parameter. */
    static const char *const mRestartingRequested; /**< Restart key parameter value. */

    static const uint32_t mRecordingBufferTimeUsec = 20000;
};

} // namespace intel_audio
