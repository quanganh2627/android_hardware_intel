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
#define LOG_TAG "AudioIntelHal"

#include "Device.hpp"
#include "AudioConversion.hpp"
#include "AudioParameterHandler.hpp"
#include "StreamIn.hpp"
#include "StreamOut.hpp"
#include <AudioPlatformState.hpp>
#include <AudioCommsAssert.hpp>
#include <hardware/audio.h>
#include "Property.h"
#include <InterfaceProviderLib.h>
#include <hardware/audio_effect.h>
#include <media/AudioRecord.h>
#include <utilities/Log.hpp>
#include <string>
#include <cutils/properties.h>

using namespace std;
using android::status_t;
using audio_comms::utilities::Log;

namespace intel_audio
{
extern "C"
{
/**
 * Function for dlsym() to look up for creating a new DeviceInterface.
 */
DeviceInterface *createAudioHardware(void)
{
    Log::Debug() << "Using Audio XML HAL";
    return new Device();
}
}         // extern "C"

const char *const Device::mRouteLibPropName = "audiocomms.routeLib";
const char *const Device::mRouteLibPropDefaultValue = "audio.routemanager.so";
const char *const Device::mRestartingKey = "restarting";
const char *const Device::mRestartingRequested = "true";

Device::Device()
    : mEchoReference(NULL),
      mPlatformState(NULL),
      mAudioParameterHandler(new AudioParameterHandler()),
      mStreamInterface(NULL)
{
    /// Get the Stream Interface of the Route manager
    NInterfaceProvider::IInterfaceProvider *interfaceProvider =
        getInterfaceProvider(TProperty<string>(mRouteLibPropName,
                                               mRouteLibPropDefaultValue).getValue().c_str());
    if (!interfaceProvider) {
        Log::Error() << __FUNCTION__ << ": Could not connect to interface provider";
        return;
    }
    // Retrieve the Stream Interface
    mStreamInterface = interfaceProvider->queryInterface<IStreamInterface>();
    if (mStreamInterface == NULL) {
        Log::Error() << "Failed to get Stream Interface on RouteMgr";
        return;
    }

    /// Construct the platform state component and start it
    mPlatformState = new AudioPlatformState(mStreamInterface);
    if (mPlatformState->start() != android::OK) {
        Log::Error() << __FUNCTION__ << ": could not start Platform State";
        mStreamInterface = NULL;
        delete mPlatformState;
        mPlatformState = NULL;
        return;
    }

    /// Start Routing service
    if (mStreamInterface->startService() != android::OK) {
        Log::Error() << __FUNCTION__ << ": Could not start Route Manager stream service";
        // Reset interface pointer to give a chance for initCheck to catch any issue
        // with the RouteMgr.
        mStreamInterface = NULL;
        delete mPlatformState;
        mPlatformState = NULL;
        return;
    }
    mPlatformState->sync();

    mStreamInterface->reconsiderRouting();

    Log::Debug() << __FUNCTION__ << ": Route Manager Service successfully started";
}

Device::~Device()
{
    mStreamInterface->stopService();
    // Remove parameter handler
    delete mAudioParameterHandler;
    // Remove Platform State component
    delete mPlatformState;
}

status_t Device::initCheck() const
{
    return (mStreamInterface && mPlatformState && mPlatformState->isStarted()) ?
           android::OK : android::NO_INIT;
}

status_t Device::setVoiceVolume(float volume)
{
    return mStreamInterface->setVoiceVolume(volume);
}

android::status_t Device::openOutputStream(audio_io_handle_t /*handle*/,
                                           audio_devices_t devices,
                                           audio_output_flags_t flags,
                                           audio_config_t &config,
                                           StreamOutInterface *&stream,
                                           const std::string &/*address*/)
{
    Log::Debug() << __FUNCTION__ << ": called for devices: " << devices;

    if (!audio_is_output_devices(devices)) {
        Log::Error() << __FUNCTION__ << ": called with bad devices";
        return android::BAD_VALUE;
    }
    StreamOut *out = new StreamOut(this, flags);
    status_t err = out->set(config);
    if (err != android::OK) {
        Log::Error() << __FUNCTION__ << ": set error.";
        delete out;
        return err;
    }
    // Informs the route manager of stream creation
    mStreamInterface->addStream(out);
    stream = out;

    Log::Debug() << __FUNCTION__ << ": output created with status=" << err;
    return android::OK;
}

void Device::closeOutputStream(StreamOutInterface *out)
{
    // Informs the route manager of stream destruction
    AUDIOCOMMS_ASSERT(out != NULL, "Invalid output stream to remove");
    mStreamInterface->removeStream(static_cast<StreamOut *>(out));
    delete out;
}

android::status_t Device::openInputStream(audio_io_handle_t /*handle*/,
                                          audio_devices_t devices,
                                          audio_config_t &config,
                                          StreamInInterface *&stream,
                                          audio_input_flags_t /*flags*/,
                                          const std::string &/*address*/,
                                          audio_source_t source)
{
    Log::Debug() << __FUNCTION__ << ": called for devices: " << devices
                 << " and input source: 0x%08x" << static_cast<uint32_t>(source);
    if (!audio_is_input_device(devices)) {
        Log::Error() << __FUNCTION__ << ": called with bad device " << devices;
        return android::BAD_VALUE;
    }

    StreamIn *in = new StreamIn(this, source);
    status_t err = in->set(config);
    if (err != android::OK) {
        Log::Error() << __FUNCTION__ << ": Set err";
        delete in;
        return err;
    }
    // Informs the route manager of stream creation
    mStreamInterface->addStream(in);
    stream = in;

    Log::Debug() << __FUNCTION__ << ": input created with status=" << err;
    return android::OK;
}

void Device::closeInputStream(StreamInInterface *in)
{
    // Informs the route manager of stream destruction
    AUDIOCOMMS_ASSERT(in != NULL, "Invalid input stream to remove");
    mStreamInterface->removeStream(static_cast<StreamIn *>(in));
    delete in;
}

status_t Device::setMicMute(bool mute)
{
    Log::Verbose() << __FUNCTION__ << ": " << (mute ? "true" : "false");
    KeyValuePairs pair;
    status_t status = pair.add(AudioPlatformState::mKeyMicMute, mute);
    if (status != android::OK) {
        return status;
    }
    return mPlatformState->setParameters(pair.toString());
}

status_t Device::getMicMute(bool &muted) const
{
    KeyValuePairs pair(mPlatformState->getParameters(AudioPlatformState::mKeyMicMute));
    return pair.get(AudioPlatformState::mKeyMicMute, muted);
}

size_t Device::getInputBufferSize(const struct audio_config &config) const
{
    switch (config.sample_rate) {
    case 8000:
    case 11025:
    case 12000:
    case 16000:
    case 22050:
    case 24000:
    case 32000:
    case 44100:
    case 48000:
        break;
    default:
        Log::Warning() << __FUNCTION__ << ": bad sampling rate: " << config.sample_rate;
        return 0;
    }
    if (config.format != AUDIO_FORMAT_PCM_16_BIT) {
        Log::Warning() << __FUNCTION__ << ": bad format: " << static_cast<int32_t>(config.format);
        return 0;
    }
    uint32_t channelCount = popcount(config.channel_mask);

    // multichannel capture is currently supported only for submix
    if ((channelCount < 1) || (channelCount > 8)) {
        Log::Warning() << __FUNCTION__ << ": bad channel count: " << channelCount;
        return 0;
    }
    SampleSpec spec(channelCount, config.format, config.sample_rate);
    return spec.convertFramesToBytes(spec.convertUsecToframes(mRecordingBufferTimeUsec));
}

status_t Device::setParameters(const string &keyValuePairs)
{
    Log::Verbose() << __FUNCTION__ << ": key value pair " << keyValuePairs;
    KeyValuePairs pairs(keyValuePairs);
    string restart;
    string key(mRestartingKey);
    status_t status = pairs.get(key, restart);
    if (status == android::OK) {
        pairs.remove(key);
        if (restart == mRestartingRequested) {
            // Replace the restarting key with all previously backuped {keys,value} pairs in order
            // to restore the previous HAL state
            Log::Info() << __FUNCTION__
                        << ": Restore audio parameters as mediaserver is restarted param="
                        << mAudioParameterHandler->getParameters();
            KeyValuePairs backupedPairs(mAudioParameterHandler->getParameters());
            backupedPairs.add(pairs.toString());
            pairs = backupedPairs;
        }
    }
    status = mPlatformState->setParameters(pairs.toString());
    if (status == android::OK) {
        Log::Verbose() << __FUNCTION__
                       << ": saving the parameters to recover in case of media server crash";
        mAudioParameterHandler->saveParameters(pairs.toString());
    }
    return status;
}

string Device::getParameters(const string &keys) const
{
    Log::Verbose() << __FUNCTION__ << ": requested keys " << keys;
    return mPlatformState->getParameters(keys);
}

android::status_t Device::setMode(audio_mode_t mode)
{
    mMode = mode;
    return android::OK;
}


status_t Device::startStream(Stream *stream)
{
    AUDIOCOMMS_ASSERT(stream != NULL, "Null stream");
    Log::Debug() << __FUNCTION__ << ": " << (stream->isOut() ? "output" : "input") << " stream.";
    mPlatformState->startStream(stream);
    getStreamInterface()->startStream();
    return android::OK;
}

status_t Device::stopStream(Stream *stream)
{
    AUDIOCOMMS_ASSERT(stream != NULL, "Null stream");
    Log::Debug() << __FUNCTION__ << ": " << (stream->isOut() ? "output" : "input") << " stream.";
    mPlatformState->stopStream(stream);
    getStreamInterface()->stopStream();
    return android::OK;
}

void Device::updateRequestedEffect()
{
    mPlatformState->updateRequestedEffect();
    getStreamInterface()->reconsiderRouting();
}

status_t Device::setStreamParameters(Stream *stream, const string &keyValuePairs)
{
    AUDIOCOMMS_ASSERT(stream != NULL, "Null stream");
    KeyValuePairs pairs(keyValuePairs);
    if (stream->isOut()) {
        // Appends the mode key only for output stream, as the policy may only reconsider
        // the mode and selects a new parameter for output
        pairs.add(AudioPlatformState::mKeyAndroidMode, mode());
    }
    Log::Verbose() << __FUNCTION__ << ": key value pair " << pairs.toString();
    return mPlatformState->setParameters(pairs.toString());
}

void Device::resetEchoReference(struct echo_reference_itfe *reference)
{
    Log::Debug() << __FUNCTION__ << ": (reference=" << reference << ")";
    // Check that the reset is possible:
    //  - reference and _echoReference shall both point to the same struct (consistency check)
    //  - they should not be NULL because the reset process will remove the reference from
    //    the output voice stream and then free the local echo reference.
    if ((reference == NULL) || (mEchoReference == NULL) || (mEchoReference != reference)) {

        /* Nothing to do */
        return;
    }

    // Get active voice output stream
    IoStream *stream = getStreamInterface()->getVoiceOutputStream();
    if (stream == NULL) {
        Log::Error() << __FUNCTION__
                     << ": no voice output found"
                     << " so problem to provide data reference for AEC effect!";
        return;
    }
    StreamOut *out = static_cast<StreamOut *>(stream);
    out->removeEchoReference(mEchoReference);
    release_echo_reference(mEchoReference);
    mEchoReference = NULL;
}

struct echo_reference_itfe *Device::getEchoReference(const SampleSpec &inputSampleSpec)
{
    Log::Debug() << __FUNCTION__;
    resetEchoReference(mEchoReference);

    // Get active voice output stream
    IoStream *stream = getStreamInterface()->getVoiceOutputStream();
    if (stream == NULL) {
        Log::Error() << __FUNCTION__ << ": no voice output found,"
                     << " so problem to provide data reference for AEC effect!";
        return NULL;
    }
    StreamOut *out = static_cast<StreamOut *>(stream);
    SampleSpec outputSampleSpec = out->streamSampleSpec();

    if (create_echo_reference(inputSampleSpec.getFormat(),
                              inputSampleSpec.getChannelCount(),
                              inputSampleSpec.getSampleRate(),
                              outputSampleSpec.getFormat(),
                              outputSampleSpec.getChannelCount(),
                              outputSampleSpec.getSampleRate(),
                              &mEchoReference) < 0) {
        Log::Error() << __FUNCTION__ << ": Could not create echo reference";
        return NULL;
    }
    out->addEchoReference(mEchoReference);
    Log::Debug() << __FUNCTION__ << ": return that mEchoReference=" << mEchoReference << ")";
    return mEchoReference;
}

void Device::printPlatformFwErrorInfo()
{
    mPlatformState->printPlatformFwErrorInfo();
}

} // namespace intel_audio
