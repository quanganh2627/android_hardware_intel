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
#define LOG_TAG "RouteManager/StreamRoute"

#include "AudioStreamRoute.hpp"
#include <AudioDevice.hpp>
#include <AudioUtils.hpp>
#include <IoStream.hpp>
#include <StreamLib.hpp>
#include <EffectHelper.hpp>
#include <AudioCommsAssert.hpp>
#include <utilities/Log.hpp>

using std::string;
using audio_comms::utilities::Log;

namespace intel_audio
{

AudioStreamRoute::AudioStreamRoute(const string &name, uint32_t routeIndex)
    : AudioRoute(name, routeIndex),
      mCurrentStream(NULL),
      mNewStream(NULL),
      mEffectSupported(0),
      mAudioDevice(StreamLib::createAudioDevice())
{
}

AudioStreamRoute::~AudioStreamRoute()
{
    delete mAudioDevice;
}

void AudioStreamRoute::updateStreamRouteConfig(const StreamRouteConfig &config)
{
    Log::Verbose() << __FUNCTION__
                   << ": config for route " << getName() << ":"
                   << "\n\t requirePreEnable=" << config.requirePreEnable
                   << "\n\t  requirePostDisable=" << config.requirePostDisable
                   << "\n\t  cardName=" << config.cardName
                   << "\n\t  deviceId=" << config.deviceId
                   << "\n\t  rate=" << config.rate
                   << "\n\t  silencePrologInMs=" << config.silencePrologInMs
                   << "\n\t  applicabilityMask=" << config.applicabilityMask
                   << "\n\t  channels=" << config.channels
                   << "\n\t  rate=" << config.rate
                   << "\n\t  format=" << static_cast<int32_t>(config.format);
    mConfig = config;

    mSampleSpec = SampleSpec(mConfig.channels, mConfig.format,
                             mConfig.rate,  mConfig.channelsPolicy);
}

bool AudioStreamRoute::needReflow() const
{
    return mPreviouslyUsed && mIsUsed &&
           (mRoutingStageRequested.test(Flow) || mRoutingStageRequested.test(Path) ||
            mCurrentStream != mNewStream);
}

android::status_t AudioStreamRoute::route(bool isPreEnable)
{
    if (isPreEnable == isPreEnableRequired()) {

        android::status_t err = mAudioDevice->open(getCardName(), getPcmDeviceId(),
                                                   getRouteConfig(), isOut());
        if (err) {

            // Failed to open PCM device -> bailing out
            return err;
        }
    }

    if (!isPreEnable) {

        if (!mAudioDevice->isOpened()) {
            Log::Error() << __FUNCTION__ << ": error opening audio device, cannot route new stream";
            return android::NO_INIT;
        }

        /**
         * Attach the stream to its route only once routing stage is completed
         * to let the audio-parameter-manager performing the required configuration of the
         * audio path.
         */
        android::status_t err = attachNewStream();
        if (err) {

            // Failed to open PCM device -> bailing out
            return err;
        }
    }
    return android::OK;
}

void AudioStreamRoute::unroute(bool isPostDisable)
{
    if (!isPostDisable) {

        if (!mAudioDevice->isOpened()) {
            Log::Error() << __FUNCTION__
                         << ": error opening audio device, cannot unroute current stream";
            return;
        }

        /**
         * Detach the stream from its route at the beginning of unrouting stage
         * Action of audio-parameter-manager on the audio path may lead to blocking issue, so
         * need to garantee that the stream will not access to the device before unrouting.
         */
        detachCurrentStream();
    }

    if (isPostDisable == isPostDisableRequired()) {

        android::status_t err = mAudioDevice->close();
        if (err) {

            return;
        }
    }
}

void AudioStreamRoute::configure()
{
    if (mCurrentStream != mNewStream) {

        if (!mAudioDevice->isOpened()) {
            Log::Error() << __FUNCTION__
                         << ": error opening audio device, cannot configure any stream";
            return;
        }

        /**
         * Route is still in use, but the stream attached to this route has changed...
         * Unroute previous stream.
         */
        detachCurrentStream();

        // route new stream
        attachNewStream();
    }
}

void AudioStreamRoute::resetAvailability()
{
    if (mNewStream) {

        mNewStream->resetNewStreamRoute();
        mNewStream = NULL;
    }
    AudioRoute::resetAvailability();
}

void AudioStreamRoute::setStream(IoStream *stream)
{
    AUDIOCOMMS_ASSERT(stream != NULL, "Fatal: invalid stream parameter!");
    Log::Verbose() << __FUNCTION__ << ": to " << getName() << " route";
    AUDIOCOMMS_ASSERT(stream->isOut() == isOut(), "Fatal: unexpected stream direction!");

    AUDIOCOMMS_ASSERT(mNewStream == NULL, "Fatal: invalid stream value!");

    mNewStream = stream;
    mNewStream->setNewStreamRoute(this);
}

bool AudioStreamRoute::isApplicable(const IoStream *stream) const
{
    AUDIOCOMMS_ASSERT(stream != NULL, "NULL stream");
    uint32_t mask = stream->getApplicabilityMask();
    Log::Verbose() << __FUNCTION__ << ": is Route " << getName() << " applicable? "
                   << "\n\t\t\t isOut=" << (isOut() ? "output" : "input")
                   << " && uiMask=" << mask
                   << " & _uiApplicableMask[" << (isOut() ? "output" : "input")
                   << "]=" << mConfig.applicabilityMask;

    return AudioRoute::isApplicable() && !isUsed() && (mask & mConfig.applicabilityMask) &&
           implementsEffects(stream->getEffectRequested());
}

bool AudioStreamRoute::implementsEffects(uint32_t effectsMask) const
{
    return (mEffectSupportedMask & effectsMask) == effectsMask;
}

android::status_t AudioStreamRoute::attachNewStream()
{
    AUDIOCOMMS_ASSERT(mNewStream != NULL, "Fatal: invalid stream value!");

    android::status_t err = mNewStream->attachRoute();

    if (err != android::OK) {
        Log::Error() << "Failing to attach route for new stream : " << err;
        return err;
    }

    mCurrentStream = mNewStream;

    return android::OK;
}

void AudioStreamRoute::detachCurrentStream()
{
    AUDIOCOMMS_ASSERT(mCurrentStream != NULL, "Fatal: invalid stream value!");

    mCurrentStream->detachRoute();
    mCurrentStream = NULL;
}

void AudioStreamRoute::addEffectSupported(const std::string &effect)
{
    mEffectSupportedMask |= EffectHelper::convertEffectNameToProcId(effect);
}

uint32_t AudioStreamRoute::getLatencyInUs() const
{
    return getSampleSpec().convertFramesToUsec(mConfig.periodSize * mConfig.periodCount);
}

uint32_t AudioStreamRoute::getPeriodInUs() const
{
    return getSampleSpec().convertFramesToUsec(mConfig.periodSize);
}

} // namespace intel_audio
