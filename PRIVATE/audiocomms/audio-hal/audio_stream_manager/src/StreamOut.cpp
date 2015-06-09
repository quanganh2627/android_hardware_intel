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
#include <utilities/Log.hpp>
#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "AudioStreamOut"

#include "StreamOut.hpp"
#include <AudioCommsAssert.hpp>
#include <HalAudioDump.hpp>

using namespace std;
using android::status_t;
using audio_comms::utilities::Log;

namespace intel_audio
{

const uint32_t StreamOut::mMaxAgainRetry = 2;
const uint32_t StreamOut::mWaitBeforeRetryUs = 10000; // 10ms
const uint32_t StreamOut::mUsecPerMsec = 1000;

StreamOut::StreamOut(Device *parent, uint32_t streamFlagsMask)
    : Stream(parent),
      mFrameCount(0),
      mEchoReference(NULL)
{
    setApplicabilityMask(streamFlagsMask);
}

StreamOut::~StreamOut()
{
}

status_t StreamOut::write(const void *buffer, size_t &bytes)
{
    AUDIOCOMMS_ASSERT(buffer != NULL, "NULL client buffer");
    setStandby(false);

    mStreamLock.readLock();
    status_t status;
    // Check if the audio route is available for this stream
    if (!isRoutedL()) {
        Log::Warning() << __FUNCTION__ << ": (buffer=" << buffer << ", bytes=" << bytes
                       << ") No route available. Trashing samples for stream " << this;

        status = generateSilence(bytes);
        mStreamLock.unlock();
        return status;
    }

    ssize_t srcFrames = streamSampleSpec().convertBytesToFrames(bytes);
    size_t dstFrames = 0;
    char *dstBuf = NULL;
    uint32_t retryCount = 0;

    pushEchoReference(buffer, srcFrames);

    // Dump audio output before any conversion.
    // FOR DEBUG PURPOSE ONLY
    if (getDumpObjectBeforeConv() != NULL) {
        getDumpObjectBeforeConv()->dumpAudioSamples(buffer,
                                                    bytes,
                                                    isOut(),
                                                    streamSampleSpec().getSampleRate(),
                                                    streamSampleSpec().getChannelCount(),
                                                    "before_conversion");
    }

    status = applyAudioConversion(buffer, (void **)&dstBuf, srcFrames, &dstFrames);

    if (status != android::OK) {
        mStreamLock.unlock();
        return status;
    }
    Log::Verbose() << __FUNCTION__ << ": srcFrames=" << srcFrames << ", bytes=" << bytes
                   << " dstFrames=" << dstFrames;

    do {
        std::string error;

        status = pcmWriteFrames(dstBuf, dstFrames, error);

        if (status < 0) {
            Log::Error() << __FUNCTION__ << ": write error: " << error
                         << " - requested " << srcFrames
                         << " (bytes=" << streamSampleSpec().convertFramesToBytes(srcFrames)
                         << ") frames";

            if (error.find(strerror(EIO)) != std::string::npos) {
                // Dump hw registers debug file info in console
                mParent->printPlatformFwErrorInfo();

            } else if (error.find(strerror(EBADFD)) != std::string::npos) {
                mStreamLock.unlock();
                Log::Error() << __FUNCTION__ << ": execute device recovery";
                setStandby(true);
                return -EBADFD;
            }

            AUDIOCOMMS_ASSERT(++retryCount < mMaxReadWriteRetried,
                              "Hardware not responding, restarting media server");

            // Get the number of microseconds to sleep, inferred from the number of
            // frames to write.
            size_t sleepUsecs = routeSampleSpec().convertFramesToUsec(dstFrames);

            // Go sleeping before trying I/O operation again.
            if (safeSleep(sleepUsecs)) {
                // If some error arises when trying to sleep, try I/O operation anyway.
                // Error counter will provoke the restart of mediaserver.
                Log::Error() << __FUNCTION__ << ":  Error while calling nanosleep interface";
            }
        }
    } while (status < 0);

    Log::Verbose() << __FUNCTION__ << ": returns " << streamSampleSpec().convertFramesToBytes(
        AudioUtils::convertSrcToDstInFrames(status, routeSampleSpec(), streamSampleSpec()));

    // Dump audio output after eventual conversions
    // FOR DEBUG PURPOSE ONLY
    if (getDumpObjectAfterConv() != NULL) {
        getDumpObjectAfterConv()->dumpAudioSamples((const void *)dstBuf,
                                                   routeSampleSpec().convertFramesToBytes(
                                                       dstFrames),
                                                   isOut(),
                                                   routeSampleSpec().getSampleRate(),
                                                   routeSampleSpec().getChannelCount(),
                                                   "after_conversion");
    }
    bytes = streamSampleSpec().convertFramesToBytes(
        AudioUtils::convertSrcToDstInFrames(dstFrames, routeSampleSpec(), streamSampleSpec()));

    mStreamLock.unlock();
    return status;
}

uint32_t StreamOut::getLatency()
{
    return getLatencyMs();
}

status_t StreamOut::attachRouteL()
{
    status_t status = Stream::attachRouteL();
    if (status != android::OK) {

        return status;
    }
    // Need to generate silence?
    uint32_t silenceMs = getOutputSilencePrologMs();
    if (silenceMs) {

        // Allocate a 1Ms buffer in stack
        uint32_t bufferSizeInFrames = routeSampleSpec().convertUsecToframes(1 * mUsecPerMsec);
        void *silenceBuffer = alloca(routeSampleSpec().convertFramesToBytes(bufferSizeInFrames));
        memset(silenceBuffer, 0, routeSampleSpec().convertFramesToBytes(bufferSizeInFrames));

        uint32_t msCount;
        for (msCount = 0; msCount < silenceMs; msCount++) {
            std::string writeError;
            status = pcmWriteFrames(silenceBuffer, bufferSizeInFrames, writeError);
            if (status < 0) {
                Log::Error() << "Write error when writing silence : " << writeError;
            }
        }
    }

    return android::OK;
}

status_t StreamOut::detachRouteL()
{
    removeEchoReference(mEchoReference);
    return Stream::detachRouteL();
}

status_t StreamOut::getRenderPosition(uint32_t &dspFrames) const
{
    dspFrames = mFrameCount;
    return android::OK;
}

status_t StreamOut::flush()
{
    AutoR lock(mStreamLock);
    if (!isRoutedL()) {

        return android::OK;
    }

    return pcmStop();
}

void StreamOut::addEchoReference(struct echo_reference_itfe *reference)
{
    AUDIOCOMMS_ASSERT(reference != NULL, "Null echo reference pointer");
    AutoW lock(mPreProcEffectLock);
    Log::Debug() << __FUNCTION__ << ": (reference = " << reference
                 << "): note mEchoReference = " << mEchoReference;
    // Called from a WLocked context
    mEchoReference = reference;
}

void StreamOut::removeEchoReference(struct echo_reference_itfe *reference)
{
    AutoW lock(mPreProcEffectLock);
    if (reference == NULL || mEchoReference == NULL) {

        return;
    }
    Log::Debug() << __FUNCTION__ << ": (reference = " << reference
                 << "): note mEchoReference = " << mEchoReference;
    if (mEchoReference == reference) {

        mEchoReference->write(mEchoReference, NULL);
        mEchoReference = NULL;
    } else {
        Log::Error() << __FUNCTION__ << ": reference requested was not attached to this stream...";
    }
}

int StreamOut::getPlaybackDelay(ssize_t frames, struct echo_reference_buffer *buffer)
{
    size_t kernelFrames;
    int status;
    status = getFramesAvailable(kernelFrames, buffer->time_stamp);
    if (status != android::OK) {

        buffer->time_stamp.tv_sec = 0;
        buffer->time_stamp.tv_nsec = 0;
        buffer->delay_ns = 0;
        Log::Error() << "get_playback_delay(): pcm_get_htimestamp error,"
                     << "setting playbackTimestamp to 0";
        return status;
    }
    kernelFrames = getBufferSizeInFrames() - kernelFrames;

    /* adjust render time stamp with delay added by current driver buffer.
     * Add the duration of current frame as we want the render time of the last
     * sample being written.
     */
    buffer->delay_ns = streamSampleSpec().convertFramesToUsec(kernelFrames + frames);

    Log::Verbose() << __FUNCTION__
                   << ": kernel_frames=" << kernelFrames
                   << " buffer->time_stamp.tv_sec=" << buffer->time_stamp.tv_sec << ","
                   << "buffer->time_stamp.tv_nsec =" << buffer->time_stamp.tv_nsec
                   << " buffer->delay_ns=" << buffer->delay_ns;

    return 0;
}

void StreamOut::pushEchoReference(const void *buffer, ssize_t frames)
{
    AutoR lock(mPreProcEffectLock);
    if (mEchoReference != NULL) {
        struct echo_reference_buffer b;
        b.raw = (void *)buffer;
        b.frame_count = frames;
        getPlaybackDelay(b.frame_count, &b);
        mEchoReference->write(mEchoReference, &b);
    }
}

status_t StreamOut::setDevice(audio_devices_t device)
{
    /*
     * Why do we need to filter AUDIO_DEVICE_NONE?
     * When for example A2DP Headset is disconnected, the policy manager sends routing=0.
     * When the stream is started again, the policy does not force the routing as it
     * considers that the new device elected is the same than the previous one.
     * In fact, selecting device=AUDIO_DEVICE_NONE does not update the device member of output
     * descriptor, leading the policy to think that no changes happened on the routing.
     *
     * Ignoring AUDIO_DEVICE_NONE fixes the issue in the audio HAL as it avoids reseting the routing
     * information at HAL layer.
     */
    if (!audio_is_output_devices(device) || device == AUDIO_DEVICE_NONE) {
        Log::Error() << __FUNCTION__ << ": invalid output device " << device;
        return android::BAD_VALUE;
    }
    return Stream::setDevice(device);
}

} // namespace intel_audio
