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

#include <stdint.h>
#include <math.h>
#include <sys/types.h>
#include <utils/Timers.h>
#include <utils/Errors.h>
#include <utils/KeyedVector.h>
#include <hardware_legacy/AudioPolicyManagerBase.h>


namespace android_audio_legacy
{

// ----------------------------------------------------------------------------


class AudioPolicyManagerALSA : public AudioPolicyManagerBase
{

public:
    AudioPolicyManagerALSA(AudioPolicyClientInterface *clientInterface);
    virtual ~AudioPolicyManagerALSA();

    /**
     * Set the device availability depending on its connection state
     */
    virtual status_t setDeviceConnectionState(AudioSystem::audio_devices device,
                                              AudioSystem::device_connection_state state,
                                              const char *device_address);

    virtual status_t startOutput(audio_io_handle_t output,
                                 AudioSystem::stream_type stream,
                                 int session);

    /**
     * Selects the input device corresponding to requested audio source
     *
     * @param[in] inputSource input source from the io handle that has to be processed
     *
     * @return device chosen according to the input source
     *                default value is AUDIO_DEVICE_NONE
     */
    virtual audio_devices_t getDeviceForInputSource(int inputSource);

    /**
     * Gets audio input handle from current input source and parameters
     */
    virtual audio_io_handle_t getInput(int inputSource,
                                       uint32_t samplingRate,
                                       audio_format_t format,
                                       audio_channel_mask_t channels,
                                       AudioSystem::audio_in_acoustics acoustics);

    status_t startInput(audio_io_handle_t input);

    virtual status_t setStreamVolumeIndex(AudioSystem::stream_type stream,
                                          int index,
                                          audio_devices_t device);

    virtual float computeVolume(int stream,
                                int index,
                                audio_devices_t device);

    virtual audio_devices_t getDeviceForStrategy(routing_strategy strategy, bool fromCache = true);

    /**
     * Stops the input stream
     *
     * @param[in] input io handle that has to be stopped
     *
     * @return status
     */
    virtual status_t stopInput(audio_io_handle_t input);

    /**
     * This function delegates to parent implementation the release of input stream and then
     * it checks if it is necessary to restart a previously stopped input stream.
     *
     * @param [in] input handle of input stream to release
     */
    virtual void releaseInput(audio_io_handle_t input);

protected:
    /**
     * Parses the parameters received from the upper layers in case some specific handling
     * must be done
     *
     * @param[in] param parameters received from above
     *
     * @return status
     */
    virtual status_t doParseParameters(AudioParameter &param);

private:
    /**
     * Is voice volume applied after mixing while in mode IN_COMM ?
     */
    bool mVoiceVolumeAppliedAfterMixInComm;
    /**
     * Is voice volume applied after mixing while in mode IN_CALL ?
     */
    bool mVoiceVolumeAppliedAfterMixInCall;
    /**
     * Music attenuation in dB while in call (csv or voip)
     */
    float mInCallMusicAttenuation_dB;

    /// audio_policy.conf file custom properies
    /**
     * Is voice volume applied after mixing while in mode IN_COMM ?
     */
    static const String8 mVoiceVolumeAppliedAfterMixInCommPropName;
    /**
     * Is voice volume applied after mixing while in mode IN_CALL ?
     */
    static const String8 mVoiceVolumeAppliedAfterMixInCallPropName;
    /**
     * Music attenuation in dB while in call (csv or voip)
     */
    static const String8 mInCallMusicAttenuation_dBPropName;

    /**
     * true if current platform implements a back microphone
     */
    inline bool hasBackMicrophone() const
    {
        return mAvailableInputDevices &
               AudioSystem::DEVICE_IN_BACK_MIC;
    }
    /**
     * true if current platform implements an earpiece
     */
    inline bool hasEarpiece() const
    {
        return mAttachedOutputDevices &
               AudioSystem::DEVICE_OUT_EARPIECE;
    }

    /**
     * Get raw stream volume level for selected device.
     * Computes the ratio between current volume index and whole volume index range.
     *
     * @param[in] stream the audio stream for which raw volume level is computed.
     * @param[in] device audio device on which computed raw volume level is applied.
     *
     * @return Raw volume ratio between 0.0 and 1.0
     */
    float getRawVolumeLevel(AudioSystem::stream_type stream, audio_devices_t device) const;

    /**
     * Check if voice volume is applied after mixing media streams.
     * Returns based on audio Policy configuration if voice volume is applied
     * after music/tones are mixed with voice call stream; only significant for
     * current established call (CSV or VoIP). Asserts if called out-of-call.
     *
     * @return true if voice volume is set after mixing for current call, false otherwise.
     */
    bool isVoiceVolumeSetAfterMix() const;

    /**
     * Set volume on given stream.
     * Compute and apply stream volume on all outputs according to connected device.
     *
     * @param[in] stream the audio stream for which volume is computed and applied
     * @param[in] device audio device on which volume is applied for stream;
     *            AUDIO_DEVICE_OUT_DEFAULT implies setting volume on all devices.
     *
     * @return OK on success, INVALID_OPERATION if operation not allowed
     */
    status_t applyVolumeOnStream(AudioSystem::stream_type stream, audio_devices_t device);

    /**
     * Return the handle corresponding to the source
     *
     * @param[in] input_source
     *
     * @return handle for source and 0 otherwise
     */
    audio_io_handle_t getHandleFromInputSource(int input_source);

    static const float _maxVolume = 1.0; /**< Max supported volume (range is [0..1]). */
    static const float _base10 = 10.0; /**< base 10 used for volume computation. */
};

}
