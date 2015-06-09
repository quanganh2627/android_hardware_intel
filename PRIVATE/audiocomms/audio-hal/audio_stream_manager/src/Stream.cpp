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
#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "AudioStream"

#include "Device.hpp"
#include "Stream.hpp"
#include <AudioPlatformState.hpp>
#include <AudioCommsAssert.hpp>
#include <utilities/Log.hpp>
#include "Property.h"
#include <AudioConversion.hpp>
#include <HalAudioDump.hpp>
#include <string>

using android::status_t;
using audio_comms::utilities::Direction;
using audio_comms::utilities::Log;
using namespace std;

namespace intel_audio
{

/**
 * Audio dump properties management (set with setprop)
 */
const std::string Stream::dumpBeforeConvProps[Direction::_nbDirections] = {
    "media.dump_input.befconv", "media.dump_output.befconv"
};

const std::string Stream::dumpAfterConvProps[Direction::_nbDirections] = {
    "media.dump_input.aftconv", "media.dump_output.aftconv"
};

Stream::Stream(Device *parent)
    : mParent(parent),
      mStandby(true),
      mDevices(0),
      mAudioConversion(new AudioConversion),
      mLatencyMs(0),
      mApplicabilityMask(0),
      mDumpBeforeConv(NULL),
      mDumpAfterConv(NULL)
{
}

Stream::~Stream()
{
    setStandby(true);

    delete mAudioConversion;
    delete mDumpAfterConv;
    delete mDumpBeforeConv;
}

status_t Stream::set(audio_config_t &config)
{
    bool badChannelCount = false;
    bool badFormat = false;
    audio_channel_mask_t channelMask = config.channel_mask;
    uint32_t channelCount = popcount(config.channel_mask);

    if (channelMask != 0) {
        Log::Debug() << __FUNCTION__ << ": requested channel mask: " << channelMask
                     << " (Channels count: " << channelCount << ")";
        // Always accept the channels requested by the client
        // as far as the channel count is supported
        IoStream::setChannels(channelMask);
        if (channelCount > 2) {
            Log::Debug() << __FUNCTION__ << ": channels= " << channelCount << " not supported";
            badChannelCount = true;
        }
    }
    if ((badChannelCount) || (channelMask == 0)) {
        // No channels information was provided by the client
        // or not supported channels
        // Use default: stereo
        if (isOut()) {
            config.channel_mask = AUDIO_CHANNEL_OUT_FRONT_LEFT |
                                  AUDIO_CHANNEL_OUT_FRONT_RIGHT;
        } else {
            config.channel_mask = AUDIO_CHANNEL_IN_LEFT | AUDIO_CHANNEL_IN_RIGHT;
        }
        IoStream::setChannels(config.channel_mask);
    }
    Log::Debug() << __FUNCTION__ << ": set channels to " << config.channel_mask;

    // Resampler is always working @ the channel count of the HAL
    IoStream::setChannelCount(popcount(IoStream::getChannels()));

    if (config.sample_rate != 0) {
        Log::Debug() << __FUNCTION__ << ": requested rate: " << config.sample_rate;
        // Always accept the rate provided by the client
        setSampleRate(config.sample_rate);
    } else {
        // No rate information was provided by the client
        // or set rate error
        // Use default HAL rate
        config.sample_rate = mDefaultSampleRate;
        setSampleRate(config.sample_rate);
    }
    Log::Debug() << __FUNCTION__ << ": set rate to " << config.sample_rate;

    if (config.format != 0) {
        Log::Debug() << __FUNCTION__
                     << ": requested format: " << static_cast<int32_t>(config.format);
        // Always accept the rate provided by the client
        // as far as this rate is supported
        if ((config.format != AUDIO_FORMAT_PCM_16_BIT) &&
            (config.format != AUDIO_FORMAT_PCM_8_24_BIT)) {
            Log::Debug() << __FUNCTION__ << ": format=( " << static_cast<int32_t>(config.format)
                         << ") not supported";
            badFormat = true;
        }

        setFormat(config.format);
    }
    if ((badFormat) || (config.format == 0)) {
        // No format provided or set format error
        // Use default HAL format
        config.format = Stream::mDefaultFormat;
        setFormat(config.format);
    }
    Log::Debug() << __FUNCTION__ << " : set format to " << static_cast<int32_t>(getFormat());

    if (badChannelCount || badFormat) {
        return android::BAD_VALUE;
    }
    return android::OK;
}

uint32_t Stream::getSampleRate() const
{
    return IoStream::getSampleRate();
}

status_t Stream::setSampleRate(uint32_t rate)
{
    IoStream::setSampleRate(rate);
    return android::OK;
}

audio_channel_mask_t Stream::getChannels() const
{
    return IoStream::getChannels();
}

audio_format_t Stream::getFormat() const
{
    return IoStream::getFormat();
}

status_t Stream::setFormat(audio_format_t format)
{
    IoStream::setFormat(format);
    return android::OK;
}

status_t Stream::standby()
{
    return setStandby(true);
}

audio_devices_t Stream::getDevice() const
{
    return mDevices;
}

bool Stream::isRoutedByPolicy() const
{
    return mDevices != 0;
}

uint32_t Stream::getApplicabilityMask() const
{
    AutoR lock(mStreamLock);
    return mApplicabilityMask;
}

status_t Stream::setDevice(audio_devices_t device)
{
    AutoW lock(mStreamLock);
    mDevices = device;
    return android::OK;
}

status_t Stream::setParameters(const string &keyValuePairs)
{
    KeyValuePairs pairs(keyValuePairs);
    // Routing device is added as an integer by the policy, so it has to be retrieved in the same
    // type to handle the sign "-" literal...
    int32_t routingDevice;
    string key(AUDIO_PARAMETER_STREAM_ROUTING);

    // Replace the routing key by the input / output device key
    if (pairs.get(key, routingDevice) == android::OK) {
        pairs.remove(key);
        // Remove the sign bit for the input device only.
        status_t status = setDevice(routingDevice);
        if (status != android::OK) {
            return status;
        }
        pairs.add(isOut() ? AudioPlatformState::mKeyDeviceOut : AudioPlatformState::mKeyDeviceIn,
                  getDevice());
    }
    // Give a chance to parent to handle the change
    return mParent->setStreamParameters(this, pairs.toString());
}

string Stream::getParameters(const string &keys) const
{
    KeyValuePairs pairs(keys);
    string value;
    string key(AUDIO_PARAMETER_STREAM_ROUTING);

    if (pairs.get(key, value) == android::OK) {
        audio_devices_t device = isOut() ? getDevice() : (getDevice() | AUDIO_DEVICE_BIT_IN);
        pairs.add<int32_t>(key, device);
    }
    Log::Verbose() << __FUNCTION__ << ": " << pairs.toString();
    return pairs.toString();
}

size_t Stream::getBufferSize() const
{
    size_t size = mSampleSpec.convertUsecToframes(
        mParent->getStreamInterface()->getPeriodInUs(isOut(), getApplicabilityMask()));

    size = AudioUtils::alignOn16(size);

    size_t bytes = mSampleSpec.convertFramesToBytes(size);
    Log::Debug() << __FUNCTION__ << ": " << bytes << " (in bytes) for "
                 << (isOut() ? "output" : "input") << " stream.";

    return bytes;
}


status_t Stream::generateSilence(size_t &bytes, void *buffer)
{
    if (buffer != NULL) {
        // Send zeroed buffer
        memset(buffer, 0, bytes);
    }
    usleep(streamSampleSpec().convertFramesToUsec(streamSampleSpec().convertBytesToFrames(bytes)));
    return android::OK;
}

uint32_t Stream::getLatencyMs() const
{
    return mLatencyMs;
}

void Stream::setApplicabilityMask(uint32_t applicabilityMask)
{
    if (getApplicabilityMask() == applicabilityMask) {

        return;
    }
    mStreamLock.writeLock();
    mApplicabilityMask = applicabilityMask;
    mStreamLock.unlock();
    updateLatency();
}

void Stream::updateLatency()
{
    mLatencyMs = AudioUtils::convertUsecToMsec(
        mParent->getStreamInterface()->getLatencyInUs(isOut(), getApplicabilityMask()));
}

status_t Stream::setStandby(bool isSet)
{
    if (isStarted() == !isSet) {

        return android::OK;
    }
    setStarted(!isSet);

    return isSet ? mParent->stopStream(this) : mParent->startStream(this);
}

status_t Stream::attachRouteL()
{
    Log::Verbose() << __FUNCTION__ << ": " << (isOut() ? "output" : "input") << " stream";
    TinyAlsaIoStream::attachRouteL();

    SampleSpec ssSrc;
    SampleSpec ssDst;

    ssSrc = isOut() ? streamSampleSpec() : routeSampleSpec();
    ssDst = isOut() ? routeSampleSpec() : streamSampleSpec();

    status_t err = configureAudioConversion(ssSrc, ssDst);
    if (err != android::OK) {
        Log::Error() << __FUNCTION__
                     << ": could not initialize audio conversion chain (err=" << err << ")";
        return err;
    }

    return android::OK;
}

status_t Stream::detachRouteL()
{
    Log::Verbose() << __FUNCTION__ << ": " << (isOut() ? "output" : "input") << " stream";
    TinyAlsaIoStream::detachRouteL();

    return android::OK;
}

status_t Stream::configureAudioConversion(const SampleSpec &ssSrc, const SampleSpec &ssDst)
{
    return mAudioConversion->configure(ssSrc, ssDst);
}

status_t Stream::getConvertedBuffer(void *dst, const uint32_t outFrames,
                                    android::AudioBufferProvider *bufferProvider)
{
    return mAudioConversion->getConvertedBuffer(dst, outFrames, bufferProvider);
}

status_t Stream::applyAudioConversion(const void *src, void **dst, uint32_t inFrames,
                                      uint32_t *outFrames)
{
    return mAudioConversion->convert(src, dst, inFrames, outFrames);
}

bool Stream::isStarted() const
{
    AutoR lock(mStreamLock);
    return !mStandby;
}

void Stream::setStarted(bool isStarted)
{
    AutoW lock(mStreamLock);
    mStandby = !isStarted;

    if (isStarted) {

        initAudioDump();
    }
}

void Stream::initAudioDump()
{
    /**
     * Read the dump properties when a new output/input stream is started.
     * False in second argument is the default value. If the property is true
     * then the dump object is created if it doesn't exist. Otherwise if it
     * is set to false, the dump object will be deleted to stop the dump.
     */
    if (TProperty<bool>(dumpBeforeConvProps[isOut()], false)) {
        if (!mDumpBeforeConv) {
            Log::Info() << __FUNCTION__ << ": create dump object for audio before conversion";
            mDumpBeforeConv = new HalAudioDump();
        }
    } else if (mDumpBeforeConv) {
        delete mDumpBeforeConv;
        mDumpBeforeConv = NULL;
    }
    if (TProperty<bool>(dumpAfterConvProps[isOut()], false)) {
        if (!mDumpAfterConv) {
            Log::Info() << __FUNCTION__ << ": create dump object for audio after conversion";
            mDumpAfterConv = new HalAudioDump();
        }
    } else if (mDumpAfterConv) {
        delete mDumpAfterConv;
        mDumpAfterConv = NULL;
    }
}

bool Stream::safeSleep(uint32_t sleepTimeUs)
{
    struct timespec tim;

    if (sleepTimeUs > mMaxSleepTime) {
        sleepTimeUs = mMaxSleepTime;
    }

    tim.tv_sec = 0;
    tim.tv_nsec = sleepTimeUs * mNsecPerUsec;

    return nanosleep(&tim, NULL) > 0;
}

} // namespace intel_audio
