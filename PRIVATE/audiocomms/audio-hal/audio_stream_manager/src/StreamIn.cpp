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
#define LOG_TAG "AudioStreamIn"

#include "StreamIn.hpp"
#include <AudioCommsAssert.hpp>
#include <HalAudioDump.hpp>
#include <KeyValuePairs.hpp>
#include <BitField.hpp>
#include <EffectHelper.hpp>
#include <utilities/Log.hpp>

using namespace std;
using audio_comms::utilities::BitField;
using android::status_t;
using audio_comms::utilities::Log;

namespace intel_audio
{

const std::string StreamIn::mHwEffectImplementor = "IntelLPE";

StreamIn::StreamIn(Device *parent, audio_source_t source)
    : Stream(parent),
      mFramesLost(0),
      mFramesIn(0),
      mProcessingFramesIn(0),
      mProcessingBuffer(NULL),
      mProcessingBufferSizeInFrames(0),
      mReferenceFramesIn(0),
      mReferenceBuffer(NULL),
      mReferenceBufferSizeInFrames(0),
      mPreprocessorsHandlerList(),
      mHwBuffer(NULL)
{
    setInputSource(source);
}

StreamIn::~StreamIn()
{
    freeAllocatedBuffers();
}

status_t StreamIn::getNextBuffer(AudioBufferProvider::Buffer *buffer,
                                 int64_t /* presentationTimeStamp */)
{
    size_t maxFrames = getBufferSizeInFrames();


    ssize_t hwFramesToRead = min(maxFrames, buffer->frameCount);

    status_t status = readHwFrames(mHwBuffer, hwFramesToRead);
    if (status < 0) {

        return status;
    }
    buffer->raw = mHwBuffer;
    buffer->frameCount = hwFramesToRead;

    return android::OK;
}

status_t StreamIn::readHwFrames(void *buffer, size_t frames)
{
    uint32_t retryCount = 0;
    status_t ret;

    do {
        std::string error;

        ret = pcmReadFrames(buffer, frames, error);

        if (ret < 0) {
            Log::Error() << __FUNCTION__ << ": read error: " << error << " - requested " << frames
                         << " (bytes=" << streamSampleSpec().convertFramesToBytes(frames)
                         << ") frames";

            if (error.find(strerror(EBADFD)) != std::string::npos) {
                return android::DEAD_OBJECT;
            }

            AUDIOCOMMS_ASSERT(++retryCount < mMaxReadWriteRetried,
                              "Hardware not responding, restarting media server");

            // Get the number of microseconds to sleep, inferred from the number of
            // frames to write.
            size_t sleepUSecs = routeSampleSpec().convertFramesToUsec(frames);

            // Go sleeping before trying I/O operation again.
            if (safeSleep(sleepUSecs)) {
                // If some error arises when trying to sleep, try I/O operation anyway.
                // Error counter will provoke the restart of mediaserver.
                Log::Error() << __FUNCTION__ << ":  Error while calling nanosleep interface";
            }
        }

    } while (ret < 0);

    // Dump audio input before eventual conversions
    // FOR DEBUG PURPOSE ONLY
    if (getDumpObjectBeforeConv() != NULL) {
        getDumpObjectBeforeConv()->dumpAudioSamples(buffer,
                                                    routeSampleSpec().convertFramesToBytes(frames),
                                                    isOut(),
                                                    routeSampleSpec().getSampleRate(),
                                                    routeSampleSpec().getChannelCount(),
                                                    "before_conversion");
    }

    return ret;
}

status_t StreamIn::readFrames(void *buffer, size_t frames, ssize_t *processedFrames)
{
    //
    // No conversion required, read HW frames directly
    //
    if (streamSampleSpec() == routeSampleSpec()) {

        status_t status = readHwFrames(buffer, frames);
        if (status == android::OK) {
            *processedFrames = frames;
        }
        return status;
    }

    //
    // Otherwise, request for a converted buffer
    //
    status_t status = getConvertedBuffer(buffer, frames, this);
    if (status != android::OK) {

        return status;
    }

    if (getDumpObjectAfterConv() != NULL) {
        getDumpObjectAfterConv()->dumpAudioSamples(buffer,
                                                   streamSampleSpec().convertFramesToBytes(frames),
                                                   isOut(),
                                                   streamSampleSpec().getSampleRate(),
                                                   streamSampleSpec().getChannelCount(),
                                                   "after_conversion");
    }

    *processedFrames = frames;
    return status;
}

int StreamIn::doProcessFrames(const void *buffer, ssize_t frames,
                              ssize_t *processedFrames,
                              ssize_t *processingFramesIn)
{
    int ret = 0;

    audio_buffer_t inBuf;
    audio_buffer_t outBuf;

    while ((*processedFrames < frames) && (*processingFramesIn > 0) && (ret == 0)) {

        vector<AudioEffectHandle>::const_iterator it;
        for (it = mPreprocessorsHandlerList.begin(); it != mPreprocessorsHandlerList.end(); ++it) {

            if (it->mEchoReference != NULL) {

                pushEchoReference(*processingFramesIn, it->mPreprocessor, it->mEchoReference);
            }
            // in_buf.frameCount and out_buf.frameCount indicate respectively
            // the maximum number of frames to be consumed and produced by process()
            inBuf.frameCount = *processingFramesIn;
            inBuf.s16 = (int16_t *)((char *)mProcessingBuffer +
                                    streamSampleSpec().convertFramesToBytes(*processedFrames));
            outBuf.frameCount = frames - *processedFrames;
            outBuf.s16 = (int16_t *)((char *)buffer +
                                     streamSampleSpec().convertFramesToBytes(*processedFrames));

            ret = (*(it->mPreprocessor))->process(it->mPreprocessor, &inBuf, &outBuf);
            if (ret == 0) {
                // Note: it is useless to recopy the output of effect processing as input
                // for the next effect processing because it is done in webrtc::audio_processing

                // process() has updated the number of frames consumed and produced in
                // in_buf.frameCount and out_buf.frameCount respectively
                *processingFramesIn -= inBuf.frameCount;
                *processedFrames += outBuf.frameCount;
            }
        }
    }
    return ret;
}

status_t StreamIn::processFrames(void *buffer, ssize_t frames, ssize_t *processedFrames)
{
    // first reload enough frames at the end of process input buffer
    if (mProcessingFramesIn < frames) {

        if (mProcessingBufferSizeInFrames < frames) {

            status_t ret = allocateProcessingMemory(frames);
            if (ret != android::OK) {

                return ret;
            }
        }

        ssize_t read_frames = frames - mProcessingFramesIn;

        status_t status = readFrames((char *)mProcessingBuffer +
                                     streamSampleSpec().convertFramesToBytes(
                                         mProcessingFramesIn),
                                     read_frames, processedFrames);
        if (status < 0) {

            return status;
        }
        /* OK, we have to process all read frames */
        mProcessingFramesIn += read_frames;
        AUDIOCOMMS_ASSERT(mProcessingFramesIn >= frames, "Not enough frames");

    }

    *processedFrames = 0;
    ssize_t processingFramesIn = mProcessingFramesIn;
    int processingReturn = 0;

    // Then process the frames
    processingReturn = doProcessFrames(buffer, frames, processedFrames, &processingFramesIn);
    if (processingReturn != 0) {

        // Effects processing failed
        // at least, it is necessary to return the read HW frames
        Log::Debug() << __FUNCTION__ << ": unable to apply any effect, ret=" << processingReturn;
        memcpy(buffer,
               mProcessingBuffer,
               streamSampleSpec().convertFramesToBytes(mProcessingFramesIn));
        *processedFrames = mProcessingFramesIn;
    } else {

        // move remaining frames to the beginning of mProccesingBuffer because currently,
        // the configuration imposes working with 160 frames and effects library works
        // with 80 frames per cycle (10 ms), i.e. the processing of 160 read HW frames
        // requests two calls to effects library (which are done by while loop. In future or
        // if configuration changed, effects library processing could be not more multiple of
        // HW read frames, so it is necessary to realign the buffer
        if (processingFramesIn != 0) {

            AUDIOCOMMS_ASSERT(processingFramesIn > 0, "Not enough frames");
            memmove(mProcessingBuffer,
                    (char *)mProcessingBuffer +
                    streamSampleSpec().convertFramesToBytes(mProcessingFramesIn -
                                                            processingFramesIn),
                    streamSampleSpec().convertFramesToBytes(processingFramesIn));
        }
    }
    // at the end, we keep remainder frames not cosumed by effect processor.
    mProcessingFramesIn = processingFramesIn;

    return android::OK;
}

status_t StreamIn::read(void *buffer, size_t &bytes)
{
    setStandby(false);

    mStreamLock.readLock();

    status_t status;
    // Check if the audio route is available for this stream
    if (!isRoutedL()) {
        Log::Warning() << __FUNCTION__ << ": (buffer=" << buffer
                       << ", bytes=" << bytes
                       << ") No route available. Generating silence for stream " << this;
        status = generateSilence(bytes, buffer);

        mStreamLock.unlock();
        return status;
    }

    ssize_t received_frames = -1;
    ssize_t frames = streamSampleSpec().convertBytesToFrames(bytes);

    // Take the effect lock while processing
    mPreProcEffectLock.readLock();

    if (!mPreprocessorsHandlerList.empty()) {

        status = processFrames(buffer, frames, &received_frames);
    } else {

        status = readFrames(buffer, frames, &received_frames);
    }
    mPreProcEffectLock.unlock();

    if (status < 0) {
        Log::Error() << __FUNCTION__ << ": (buffer=" << buffer << ", bytes=" << bytes
                     << ") returns " << received_frames
                     << ". Generating silence for stream " << this;
        generateSilence(bytes, buffer);

        mStreamLock.unlock();

        if (status == android::DEAD_OBJECT) {
            Log::Error() << __FUNCTION__ << ": execute device recovery";
            setStandby(true);
        }
        return -EBADFD;
    }
    bytes = streamSampleSpec().convertFramesToBytes(received_frames);

    mStreamLock.unlock();
    return android::OK;
}

void StreamIn::resetFramesLost()
{
    // setVoiceVolume and mixing during voice call cannot happen together
    // need a lock; but deadlock may appear during simultaneous R or W
    // so remove lock and the reset of mFramesLost which is never updated btw
}

unsigned int StreamIn::getInputFramesLost() const
{
    unsigned int count = mFramesLost;   // set to 0 during construction

    StreamIn *mutable_this = const_cast<StreamIn *>(this);
    // Requirement from AudioHardwareInterface.h:
    // Audio driver is expected to reset the value to 0 and restart counting upon
    // returning the current value by this function call.
    mutable_this->resetFramesLost();
    return count;
}


status_t StreamIn::allocateHwBuffer()
{
    freeAllocatedBuffers();

    mHwBufferSize = getBufferSizeInBytes();

    mHwBuffer = new char[mHwBufferSize];
    if (!mHwBuffer) {
        Log::Error() << __FUNCTION__ << ": cannot allocate resampler Hwbuffer";
        return android::NO_MEMORY;
    }
    return android::OK;
}

void StreamIn::freeAllocatedBuffers()
{
    delete[] mHwBuffer;
    mHwBuffer = NULL;
}

status_t StreamIn::attachRouteL()
{
    status_t status = Stream::attachRouteL();
    if (status != android::OK) {

        return status;
    }
    return allocateHwBuffer();
}

status_t StreamIn::detachRouteL()
{
    freeAllocatedBuffers();
    return Stream::detachRouteL();
}


bool StreamIn::isHwEffectL(effect_handle_t effect)
{
    AUDIOCOMMS_ASSERT(effect != NULL, "NULL effect context");
    AUDIOCOMMS_ASSERT(*effect != NULL, "NULL effect interface");

    string implementor;
    status_t err = getAudioEffectImplementorFromHandle(effect, implementor);
    if (err != android::OK) {

        return false;
    }
    return implementor == mHwEffectImplementor;
}

status_t StreamIn::setParameters(const string &keyValuePairs)
{
    KeyValuePairs pairs(keyValuePairs);
    int inputSource;
    string key(AUDIO_PARAMETER_STREAM_INPUT_SOURCE);
    if (pairs.get(key, inputSource) == android::OK) {
        setInputSource(static_cast<audio_source_t>(inputSource));
        // Removes this input stream specific key
        pairs.remove(key);
    }
    return Stream::setParameters(pairs.toString());
}

status_t StreamIn::setDevice(audio_devices_t device)
{
    if (!audio_is_input_device(device)) {
        Log::Error() << __FUNCTION__ << ": invalid input device " << device;
        return android::BAD_VALUE;
    }
    return Stream::setDevice(device & ~AUDIO_DEVICE_BIT_IN);
}

void StreamIn::setInputSource(audio_source_t inputSource)
{
    setApplicabilityMask(BitField::indexToMask(inputSource));
}

status_t StreamIn::addAudioEffect(effect_handle_t effect)
{
    Log::Debug() << __FUNCTION__ << ": effect=" << effect;
    AUDIOCOMMS_ASSERT(effect != NULL, "NULL effect context");
    AUDIOCOMMS_ASSERT(*effect != NULL, "NULL effect interface");

    // Called from different context than the stream,
    // so effect Lock must be held
    AutoW lock(mPreProcEffectLock);

    if (isHwEffectL(effect)) {
        Log::Debug() << __FUNCTION__ << ": HW Effect requested";
        /**
         * HW Effects management
         */
        string name;
        status_t err = getAudioEffectNameFromHandle(effect, name);
        if (err != android::OK) {

            return android::BAD_VALUE;
        }
        addRequestedEffect(EffectHelper::convertEffectNameToProcId(name));
        if (isStarted()) {
            Log::Debug() << __FUNCTION__ << ": stream running, reconsider routing";
            // If the stream is routed, force a reconsider routing to take effect into account
            mParent->updateRequestedEffect();
        }
    } else {
        Log::Debug() << __FUNCTION__ << ": SW Effect requested(effect=" << effect << ")";
        /**
         * SW Effects management
         */
        if (isAecEffect(effect)) {

            struct echo_reference_itfe *stReference = NULL;
            stReference = mParent->getEchoReference(streamSampleSpec());
            return addSwAudioEffectL(effect, stReference);
        }
        addSwAudioEffectL(effect);
    }

    return android::OK;
}

status_t StreamIn::removeAudioEffect(effect_handle_t effect)
{
    Log::Debug() << __FUNCTION__ << ": effect=" << effect;

    // Called from different context than the stream,
    // so effect Lock must be held.
    AutoW lock(mPreProcEffectLock);

    AUDIOCOMMS_ASSERT(effect != NULL, "NULL effect context");
    AUDIOCOMMS_ASSERT(*effect != NULL, "NULL effect interface");

    if (isHwEffectL(effect)) {
        Log::Debug() << __FUNCTION__ << ": HW Effect requested";
        /**
         * HW Effects management
         */
        string name;
        status_t err = getAudioEffectNameFromHandle(effect, name);
        if (err != android::OK) {

            return android::BAD_VALUE;
        }
        removeRequestedEffect(EffectHelper::convertEffectNameToProcId(name));
        if (isStarted()) {
            Log::Debug() << __FUNCTION__ << ": stream running, reconsider routing";
            // If the stream is routed,
            // force a reconsider routing to take effect removal into account
            mParent->updateRequestedEffect();
        }
    } else {
        Log::Debug() << __FUNCTION__ << ": SW Effect requested";
        /**
         * SW Effects management
         */
        removeSwAudioEffectL(effect);
    }
    return android::OK;
}

status_t StreamIn::addSwAudioEffectL(effect_handle_t effect,
                                     echo_reference_itfe *reference)
{
    AUDIOCOMMS_ASSERT(effect != NULL, "NULL effect context");
    AUDIOCOMMS_ASSERT(*effect != NULL, "NULL effect interface");

    // audio effects processing is very costy in term of CPU,
    // so useless to add the same effect more than one time
    vector<AudioEffectHandle>::const_iterator it;
    it = std::find_if(mPreprocessorsHandlerList.begin(), mPreprocessorsHandlerList.end(),
                      std::bind2nd(MatchEffect(), effect));
    if (it != mPreprocessorsHandlerList.end()) {
        Log::Warning() << __FUNCTION__ << ": (effect=" << effect
                       << "): it is useless to add again the same effect";
        return android::OK;
    }
    mPreprocessorsHandlerList.push_back(AudioEffectHandle(effect, reference));
    Log::Debug() << __FUNCTION__ << ": (effect=" << effect
                 << "): effect added. number of stored effects is"
                 << effect, mPreprocessorsHandlerList.size();
    return android::OK;
}

status_t StreamIn::removeSwAudioEffectL(effect_handle_t effect)
{
    AUDIOCOMMS_ASSERT(effect != NULL, "NULL effect context");
    AUDIOCOMMS_ASSERT(*effect != NULL, "NULL effect interface");

    vector<AudioEffectHandle>::iterator it;
    it = std::find_if(mPreprocessorsHandlerList.begin(), mPreprocessorsHandlerList.end(),
                      std::bind2nd(MatchEffect(), effect));
    if (it != mPreprocessorsHandlerList.end()) {
        Log::Debug() << __FUNCTION__ << ": (effect=" << effect
                     << "): effect has been found. number of effects before erase "
                     << mPreprocessorsHandlerList.size();
        if (it->mEchoReference != NULL) {

            /* stop reading from echo reference */
            it->mEchoReference->read(it->mEchoReference, NULL);
            mParent->resetEchoReference(it->mEchoReference);
            it->mEchoReference = NULL;
        }
        mPreprocessorsHandlerList.erase(it);
        Log::Debug() << __FUNCTION__ << " (effect=" << effect
                     << "): effect has been found. number of effects after erase "
                     << mPreprocessorsHandlerList.size();
        return android::OK;
    }
    return android::BAD_VALUE;
}

status_t StreamIn::getAudioEffectNameFromHandle(effect_handle_t effect,
                                                string &name) const
{
    AUDIOCOMMS_ASSERT(effect != NULL, "NULL effect context");
    AUDIOCOMMS_ASSERT(*effect != NULL, "NULL effect interface");
    effect_descriptor_t desc;
    if ((*effect)->get_descriptor(effect, &desc) != 0) {
        Log::Error() << __FUNCTION__ << ": could not get effect descriptor";
        return android::BAD_VALUE;
    }
    Log::Verbose() << __FUNCTION__ << ": Name=" << desc.name;
    name = string(desc.name);
    return android::OK;
}

status_t StreamIn::getAudioEffectImplementorFromHandle(effect_handle_t effect,
                                                       string &implementor) const
{
    AUDIOCOMMS_ASSERT(effect != NULL, "NULL effect context");
    AUDIOCOMMS_ASSERT(*effect != NULL, "NULL effect interface");
    effect_descriptor_t desc;
    if ((*effect)->get_descriptor(effect, &desc) != 0) {
        Log::Error() << __FUNCTION__ << ": could not get effect descriptor";
        return android::BAD_VALUE;
    }
    Log::Verbose() << __FUNCTION__ << ": Name=" << desc.implementor;
    implementor = string(desc.implementor);
    return android::OK;
}

bool StreamIn::isAecEffect(effect_handle_t effect)
{
    AUDIOCOMMS_ASSERT(effect != NULL, "NULL effect context");
    AUDIOCOMMS_ASSERT(*effect != NULL, "NULL effect interface");
    effect_descriptor_t desc;
    if ((*effect)->get_descriptor(effect, &desc) != 0) {
        Log::Error() << __FUNCTION__ << ": could not get effect descriptor";
        return false;
    }
    if (memcmp(&desc.type, FX_IID_AEC, sizeof(effect_uuid_t)) == 0) {
        Log::Debug() << __FUNCTION__ << ": effect is AEC";
        return true;
    }
    return false;
}

void StreamIn::getCaptureDelay(struct echo_reference_buffer *buffer)
{
    /* read frames available in kernel driver buffer */
    size_t kernel_frames;
    struct timespec tstamp;
    long buf_delay;
    long kernel_delay;
    long delay_ns;

    if (getFramesAvailable(kernel_frames, tstamp) != android::OK) {

        buffer->time_stamp.tv_sec = 0;
        buffer->time_stamp.tv_nsec = 0;
        buffer->delay_ns = 0;
        Log::Warning() << __FUNCTION__ << ": read get_capture_delay(): pcm_htimestamp error";
        return;
    }
    // read frames available in audio HAL input buffer
    // add number of frames being read as we want the capture time of first sample
    // in current buffer.
    buf_delay = streamSampleSpec().convertFramesToUsec(mFramesIn + mProcessingFramesIn);

    // add delay introduced by kernel
    kernel_delay = routeSampleSpec().convertFramesToUsec(kernel_frames);

    delay_ns = kernel_delay + buf_delay;

    buffer->time_stamp = tstamp;
    buffer->delay_ns = delay_ns;
    Log::Verbose() << "get_capture_delay time_stamp = [" << buffer->time_stamp.tv_sec
                   << "].[" << buffer->time_stamp.tv_nsec << "], delay_ns: [" << buffer->delay_ns
                   << "], kernel_delay:[" << kernel_delay << "], buf_delay:[" << buf_delay
                   << "], kernel_frames:[" << kernel_frames << "]";
}

int32_t StreamIn::updateEchoReference(ssize_t frames,
                                      struct echo_reference_itfe *reference)
{
    struct echo_reference_buffer b;

    AUDIOCOMMS_ASSERT(reference != NULL, "Null reference handle");

    b.delay_ns = 0;

    if (mReferenceFramesIn < frames) {

        if (mReferenceBufferSizeInFrames < frames) {

            mReferenceBufferSizeInFrames = frames;
            int16_t *referenceBuffer = (int16_t *)realloc(mReferenceBuffer,
                                                          streamSampleSpec().convertFramesToBytes(
                                                              mReferenceBufferSizeInFrames));
            if (referenceBuffer == NULL) {
                Log::Error() << __FUNCTION__ << ": (frames=" << frames << "): realloc failed";
                return android::NO_MEMORY;
            }
            mReferenceBuffer = referenceBuffer;
        }

        b.frame_count = frames - mReferenceFramesIn;
        b.raw = (void *)((char *)mReferenceBuffer +
                         streamSampleSpec().convertFramesToBytes(mReferenceFramesIn));

        getCaptureDelay(&b);

        if (reference->read(reference, &b) == 0) {

            mReferenceFramesIn += b.frame_count;
        } else {
            Log::Warning() << __FUNCTION__ << ": NOT enough frames to read ref buffer";
        }
    }
    return b.delay_ns;
}

status_t StreamIn::pushEchoReference(ssize_t frames, effect_handle_t preprocessor,
                                     struct echo_reference_itfe *reference)
{
    /* read frames from echo reference buffer and update echo delay
     * mReferenceFramesIn is updated with frames available in mReferenceBuffer */
    int32_t delay_us = updateEchoReference(frames, reference) / 1000;

    AUDIOCOMMS_ASSERT(preprocessor != NULL, "Null preproc pointer");
    AUDIOCOMMS_ASSERT(*preprocessor != NULL, "Null preproc");
    AUDIOCOMMS_ASSERT(reference != NULL, "Null eference");

    if (mReferenceFramesIn < frames) {

        frames = mReferenceFramesIn;
    }

    if ((*preprocessor)->process_reverse == NULL) {
        Log::Warning() << __FUNCTION__ << ": (frames " << frames << ": process_reverse is NULL";
        return android::BAD_VALUE;
    }

    audio_buffer_t buf;

    buf.frameCount = mReferenceFramesIn;
    buf.s16 = mReferenceBuffer;

    status_t processingReturn = (*preprocessor)->process_reverse(preprocessor,
                                                                 &buf,
                                                                 NULL);
    setPreprocessorEchoDelay(preprocessor, delay_us);
    mReferenceFramesIn -= buf.frameCount;

    if (mReferenceFramesIn > 0) {

        memcpy(mReferenceBuffer,
               (char *)mReferenceBuffer + streamSampleSpec().convertFramesToBytes(buf.frameCount),
               streamSampleSpec().convertFramesToBytes(mReferenceFramesIn));
    }

    return processingReturn;
}

status_t StreamIn::setPreprocessorParam(effect_handle_t effect, effect_param_t *param)
{
    AUDIOCOMMS_ASSERT(effect != NULL, "NULL effect context");
    AUDIOCOMMS_ASSERT(*effect != NULL, "NULL effect interface");
    AUDIOCOMMS_ASSERT(param != NULL, "Null param");

    status_t ret;
    uint32_t size = sizeof(int);
    AUDIOCOMMS_ASSERT(param->psize >= 1, "Invalid parameter size");
    uint32_t psize = ((param->psize - 1) / sizeof(int) + 1) * sizeof(int) + param->vsize;

    ret = (*effect)->command(effect,
                             EFFECT_CMD_SET_PARAM,
                             sizeof(effect_param_t) + psize,
                             param,
                             &size,
                             &param->status);

    return ret == 0 ? param->status : ret;
}

status_t StreamIn::setPreprocessorEchoDelay(effect_handle_t effect, int32_t delayInUs)
{
    AUDIOCOMMS_ASSERT(effect != NULL, "NULL effect context");
    AUDIOCOMMS_ASSERT(*effect != NULL, "NULL effect interface");
    /** effect_param_t contains extensible field "data"
     * in our case, it is necessary to "allocate" memory to store
     * AEC_PARAM_ECHO_DELAY and delay_us as uint32_t
     * so, computation of "allocated" memory is size of
     * effect_param_t in uint32_t + 2
     */
    uint32_t buf[sizeof(effect_param_t) / sizeof(uint32_t) + 2];
    effect_param_t *param = reinterpret_cast<effect_param_t *>(buf);

    param->psize = sizeof(uint32_t);
    param->vsize = sizeof(uint32_t);

    struct delay
    {
        uint32_t aecEchoDelay;
        uint32_t delayUs;
    };
    delay *data = reinterpret_cast<delay *>(param->data);

    data->aecEchoDelay = AEC_PARAM_ECHO_DELAY;
    data->delayUs = delayInUs;

    return setPreprocessorParam(effect, param);
}

status_t StreamIn::allocateProcessingMemory(ssize_t frames)
{
    mProcessingBufferSizeInFrames = frames;

    int16_t *processingBuffer = (int16_t *)realloc(mProcessingBuffer,
                                                   streamSampleSpec().convertFramesToBytes(
                                                       mProcessingBufferSizeInFrames));
    if (processingBuffer == NULL) {
        Log::Error() << __FUNCTION__ << ": (frames=" << frames
                     << "): realloc failed errno = " << strerror(errno) << "!";
        return android::NO_MEMORY;
    }
    mProcessingBuffer = processingBuffer;
    Log::Debug() << __FUNCTION__ << ": (frames=" << frames
                 << "): mProcessingBuffer=" << mProcessingBuffer
                 << " size extended to " << mProcessingBufferSizeInFrames
                 << " frames (i.e. "
                 << streamSampleSpec().convertFramesToBytes(mProcessingBufferSizeInFrames)
                 << " bytes)";
    return android::OK;
}

} // namespace intel_audio
