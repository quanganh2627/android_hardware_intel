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

#include <SampleSpec.hpp>
#include <utils/RWLock.h>
#include <string>

typedef android::RWLock::AutoRLock AutoR;
typedef android::RWLock::AutoWLock AutoW;

namespace intel_audio
{

class IStreamRoute;
class IAudioDevice;

class IoStream
{
public:
    IoStream()
        : mCurrentStreamRoute(NULL),
          mNewStreamRoute(NULL),
          mEffectsRequestedMask(0),
          mIsRouted(false)
    {}

    /**
     * indicates if the stream has been routed (ie audio device available and the routing is done)
     *
     * @return true if stream is routed, false otherwise
     */
    bool isRouted() const;

    /**
     * indicates if the stream has been routed (ie audio device available and the routing is done).
     * Must be called from locked context.
     *
     * @return true if stream is routed, false otherwise
     */
    bool isRoutedL() const;

    /**
     * indicates if the stream has already been attached to a new route.
     * Note that the routing is still not done.
     *
     * @return true if stream has a new route assigned, false otherwise
     */
    bool isNewRouteAvailable() const;

    /**
     * get stream direction.
     * Note that true=playback, false=capture.
     * @return true if playback, false otherwise.
     */
    virtual bool isOut() const = 0;

    /**
     * get stream state.
     * Note that true=playing, false=standby|stopped.
     * @return true if started, false otherwise.
     */
    virtual bool isStarted() const = 0;

    /**
     * Checks if a stream has been routed from a policy point of view..
     *
     * @return true if the stream is routed by policy, false otherwise.
     */
    virtual bool isRoutedByPolicy() const = 0;

    /**
     * Applicability mask.
     * For an input stream, applicability mask is the ID of the input source
     * For an output stream, applicability mask is the output flags
     *
     * @return applicability mask.
     */
    virtual uint32_t getApplicabilityMask() const = 0;

    /**
     * Get output silence to be appended before playing.
     * Some route may require to append silence in the ring buffer as powering on of components
     * involved in this route may take a while, and audio sample might be lost. It will result in
     * loosing the beginning of the audio samples.
     *
     * @return silence to append in milliseconds.
     */
    uint32_t getOutputSilencePrologMs() const;

    /**
     * Adds an effect to the mask of requested effect.
     *
     * @param[in] effectId Id of the requested effect.
     */
    void addRequestedEffect(uint32_t effectId);

    /**
     * Removes an effect from the mask of requested effect.
     *
     * @param[in] effectId Id of the requested effect.
     */
    void removeRequestedEffect(uint32_t effectId);

    /**
     * Get effects requested for this stream.
     * The route manager will select the route that supports all requested effects.
     *
     * @return mask with Id of requested effects
     */
    uint32_t getEffectRequested() const { return mEffectsRequestedMask; }

    /**
     * Get the sample specifications of the stream route.
     *
     * @return sample specifications.
     */
    SampleSpec routeSampleSpec() const { return mRouteSampleSpec; }

    /**
     * Get the stream sample specification.
     * Stream Sample specification is the sample spec in which the client gives/receives samples
     *
     * @return sample specifications.
     */
    SampleSpec streamSampleSpec() const
    {
        return mSampleSpec;
    }

    /**
     * Get the sample rate of the stream.
     *
     * @return sample rate of the stream.
     */
    inline uint32_t getSampleRate() const
    {
        return mSampleSpec.getSampleRate();
    }

    /**
     * Set the sample rate of the stream.
     *
     * @param[in] rate of the stream.
     */
    inline void setSampleRate(uint32_t rate)
    {
        mSampleSpec.setSampleRate(rate);
    }

    /**
     * Get the format of the stream.
     *
     * @return format of the stream.
     */
    inline audio_format_t getFormat() const
    {
        return mSampleSpec.getFormat();
    }

    /**
     * Set the format of the stream.
     *
     * @param[in] format of the stream.
     */
    inline void setFormat(audio_format_t format)
    {
        mSampleSpec.setFormat(format);
    }

    /**
     * Get the channel count of the stream.
     *
     * @return channel count of the stream.
     */
    inline uint32_t getChannelCount() const
    {
        return mSampleSpec.getChannelCount();
    }

    /**
     * Get the channel count of the stream.
     *
     * @return channel count of the stream.
     */
    inline void setChannelCount(uint32_t channels)
    {
        return mSampleSpec.setChannelCount(channels);
    }


    /**
     * Get the channels of the stream.
     * Channels is a mask, each bit represents a specific channel.
     *
     * @return channel mask of the stream.
     */
    inline audio_channel_mask_t getChannels() const
    {
        return mSampleSpec.getChannelMask();
    }

    /**
     * Set the channels of the stream.
     * Channels is a mask, each bit represents a specific channel.
     *
     * @param[in] channel mask of the stream.
     */
    inline void setChannels(audio_channel_mask_t mask)
    {
        mSampleSpec.setChannelMask(mask);
    }

    /**
     * Reset the new stream route.
     */
    void resetNewStreamRoute();

    /**
     * Set a new route for this stream.
     * No need to lock as the newStreamRoute is for unique usage of the route manager,
     * so accessed from atomic context.
     *
     * @param[in] newStreamRoute: stream route to be attached to this stream.
     */
    void setNewStreamRoute(IStreamRoute *newStreamRoute);

    virtual uint32_t getBufferSizeInBytes() const = 0;

    virtual size_t getBufferSizeInFrames() const = 0;

    /**
     * Read frames from audio device.
     *
     * @param[in] buffer: audio samples buffer to fill from audio device.
     * @param[out] frames: number of frames to read.
     * @param[out] error: string containing readable error, if any is set
     *
     * @return status_t error code of the pcm read operation.
     */
    virtual android::status_t pcmReadFrames(void *buffer, size_t frames,
                                            std::string &error) const = 0;

    /**
     * Write frames to audio device.
     *
     * @param[in] buffer: audio samples buffer to render on audio device.
     * @param[out] frames: number of frames to render.
     * @param[out] error: string containing readable error, if any is set
     *
     * @return status_t error code of the pcm write operation.
     */
    virtual android::status_t pcmWriteFrames(void *buffer, ssize_t frames,
                                             std::string &error) const = 0;

    virtual android::status_t pcmStop() const = 0;

    /**
     * Returns available frames in pcm buffer and corresponding time stamp.
     * For an input stream, frames available are frames ready for the
     * application to read.
     * For an output stream, frames available are the number of empty frames available
     * for the application to write.
     */
    virtual android::status_t getFramesAvailable(uint32_t &avail,
                                                 struct timespec &tStamp) const = 0;

    IStreamRoute *getCurrentStreamRoute() { return mCurrentStreamRoute; }

    IStreamRoute *getNewStreamRoute() { return mNewStreamRoute; }

    /**
     * Attach the stream to its route.
     * Called by the StreamRoute to allow accessing the pcm device.
     * Set the new pcm device and sample spec given by the stream route.
     *
     * @return true if attach successful, false otherwise.
     */
    android::status_t attachRoute();

    /**
     * Detach the stream from its route.
     * Either the stream has been preempted by another stream or the stream has stopped.
     * Called by the StreamRoute to prevent from accessing the device any more.
     *
     * @return true if detach successful, false otherwise.
     */
    android::status_t detachRoute();

protected:
    /**
     * Attach the stream to its route.
     * Set the new pcm device and sample spec given by the stream route.
     * Called from locked context.
     *
     * @return true if attach successful, false otherwise.
     */
    virtual android::status_t attachRouteL();

    /**
     * Detach the stream from its route.
     * Either the stream has been preempted by another stream or the stream has stopped.
     * Called from locked context.
     *
     * @return true if detach successful, false otherwise.
     */
    virtual android::status_t detachRouteL();

    /**
     * Lock to protect not only the access to pcm device but also any access to device dependant
     * parameters as sample specification.
     */
    mutable android::RWLock mStreamLock;

    virtual ~IoStream() {}

    SampleSpec mSampleSpec; /**< stream sample specifications. */

private:
    void setCurrentStreamRouteL(IStreamRoute *currentStreamRoute);

    /**
     * Sets the route sample specification.
     * Must be called with stream lock held.
     *
     * @param[in] sampleSpec specifications of the route attached to the stream.
     */
    void setRouteSampleSpecL(SampleSpec sampleSpec);

    IStreamRoute *mCurrentStreamRoute; /**< route assigned to the stream (routed yet). */
    IStreamRoute *mNewStreamRoute; /**< New route assigned to the stream (not routed yet). */

    /**
     * Sample specifications of the route assigned to the stream.
     */
    SampleSpec mRouteSampleSpec;

    uint32_t mEffectsRequestedMask; /**< Mask of requested effects. */

    bool mIsRouted; /**< flag indicating the stream is routed and device is ready to use. */
};

} // namespace intel_audio
