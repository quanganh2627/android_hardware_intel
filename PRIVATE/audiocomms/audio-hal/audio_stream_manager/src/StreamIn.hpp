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

#include "Device.hpp"
#include "Stream.hpp"
#include <StreamInterface.hpp>
#include <media/AudioBufferProvider.h>
#include <vector>
#include <list>

namespace intel_audio
{

class StreamIn : public StreamInInterface, public Stream,
                 public android::AudioBufferProvider
{
private:
    typedef std::list<effect_handle_t>::iterator AudioEffectsListIterator;

public:
    StreamIn(Device *parent, audio_source_t source);
    virtual ~StreamIn();

    // From AudioStreamIn
    virtual android::status_t addAudioEffect(effect_handle_t effect);
    virtual android::status_t removeAudioEffect(effect_handle_t effect);

    virtual int setGain(float /* gain */) { return android::OK; }
    virtual android::status_t read(void *buffer, size_t &bytes);
    virtual uint32_t getInputFramesLost() const;
    virtual android::status_t setParameters(const std::string &keyValuePairs);
    virtual android::status_t setDevice(audio_devices_t device);

    // From AudioBufferProvider
    virtual android::status_t getNextBuffer(android::AudioBufferProvider::Buffer *buffer,
                                            int64_t presentationTimeStamp = kInvalidPTS);

    /** @note API not implemented in input stream*/
    virtual void releaseBuffer(android::AudioBufferProvider::Buffer */* buffer */) {}

    // From IoStream
    /**
     * Get stream direction.
     * From Stream class.
     * @return always false (for input).
     */
    virtual bool isOut() const
    {
        return false;
    }

protected:
    /**
     * Callback of route attachement called by the stream lib. (and so route manager).
     * Inherited from Stream class.
     *
     * @return OK if streams attached successfully to the route, error code otherwise.
     */
    virtual android::status_t attachRouteL();

    /**
     * Callback of route detachement called by the stream lib. (and so route manager).
     * Inherited from Stream class.
     *
     * @return OK if streams detached successfully from the route, error code otherwise.
     */
    virtual android::status_t detachRouteL();

private:
    /**
     * Set the input source.
     * This function is non-reetrant.
     *
     * @param[in] inputSource: input source to be used by this stream.
     */
    void setInputSource(audio_source_t inputSource);

    android::status_t readHwFrames(void *buffer, size_t frames);

    /**
     * Performs the removal of an effect.
     * It removes the effect from the stream list of requested effects
     * and add the effect only if the stream is still attached to the route.
     *
     * @param[in] effect structure of the effect to add.
     *
     * @return status_t OK upon succes, error code otherwise.
     */
    android::status_t removeSwAudioEffectL(effect_handle_t effect);

    /**
     * Add effect on the stream in routing locked context.
     * It adds an audio effect on the input stream.
     *
     * @param[in] structure of the effect to add.
     *
     * @return status_t OK upon succes, error code otherwise.
     */
    android::status_t addSwAudioEffectL(effect_handle_t effect,
                                        struct echo_reference_itfe *reference = NULL);

    /**
     * Retrieve audio effect name from effect handle.
     *
     * @param[in] effect: handle in the effect
     * @param[out] name: effect name.
     *
     * @return OK if name retrieved, error code otherwise.
     */
    android::status_t getAudioEffectNameFromHandle(effect_handle_t effect, std::string &name) const;

    /**
     * Retrieve audio effect implementor name from effect handle.
     *
     * @param[in] effect: handle in the effect.
     * @param[out] implementor: effect implementor.
     *
     * @return OK if implementor retrieved, error code otherwise.
     */
    android::status_t getAudioEffectImplementorFromHandle(effect_handle_t effect,
                                                          std::string &implementor) const;

    /**
     * Checks if the requested effect is a HW supported effect.
     * Note that this function uses the implementor field to detect LPE Hw effect.
     *
     * @param[in] effect: handle in the effect
     *
     * @return true if HW effect, false if SW.
     */
    bool isHwEffectL(effect_handle_t effect);

    /**
     * Checks if effect is AEC.
     * AEC is a specific effect as it involves not only input stream but also output stream
     * to provide an echo reference.
     *
     * @param[in] effect: handle in the effect.
     *
     * @return true if effect is identified as AEC, false otherwise.
     */
    bool isAecEffect(effect_handle_t effect);

    class AudioEffectHandle
    {
    public:
        effect_handle_t mPreprocessor;
        struct echo_reference_itfe *mEchoReference;
        AudioEffectHandle()
            : mPreprocessor(NULL), mEchoReference(NULL) {}
        AudioEffectHandle(effect_handle_t effect, struct echo_reference_itfe *reference)
            : mPreprocessor(effect), mEchoReference(reference) {}
        ~AudioEffectHandle() {}
    };

    /**
     * Function to be used as the predicate in find_if call.
     */
    struct MatchEffect : public std::binary_function<AudioEffectHandle, effect_handle_t, bool>
    {

        bool operator()(const AudioEffectHandle &effectHandle,
                        const effect_handle_t &effect) const
        {

            return effectHandle.mPreprocessor == effect;
        }
    };

    /**
     * Reset the amount of input frames lost in the audio driver since the last call of
     * getInputFramesLost.
     */
    void resetFramesLost();

    /**
     * Read audio frames into the buffer.
     *
     * @param[out] buffer memory in which it will copy the frames.
     * @param[in] frames requested frames to read.
     * @param[out] processedFrames number of frames processed if successful.
     *
     * @return 0 if success, negative error code otherwise.
     */
    android::status_t readFrames(void *buffer, size_t frames, ssize_t *processedFrames);

    /**
     * Free internal buffers allocated for read / processing operations.
     */
    void freeAllocatedBuffers();

    /**
     * Allocate memory to process a certain amount of frames.
     *
     * @param[in] frames number of frame that we may process.
     *
     * @return OK if successful allocation, error code otherwise.
     */
    android::status_t allocateProcessingMemory(ssize_t frames);

    /**
     * Allocate the buffer in which it reads the samples from the audio device.
     *
     * @return OK if successful allocation, error code otherwise.
     */
    inline android::status_t allocateHwBuffer();

    /**
     * Process audio frames into the buffer.
     *
     * @param[out] buffer memory in which it will copy the processed frames.
     * @param[in] frames requested frames to read.
     * @param[out] processedFrames number of frames processed if successful.
     *
     * @return 0 if success, negative error code otherwise.
     */
    android::status_t processFrames(void *buffer, ssize_t frames, ssize_t *processedFrames);

    /**
     * Process audio frames into the buffer.
     *
     * @param[out] buffer memory in which it will copy the processed frames.
     * @param[in] frames requested frames to read.
     * @param[out] processed_frames number of frames processed.
     * @param[out] processing_frames_in remaining frames to process.
     *
     * @return 0 if success, negative error code otherwise.
     */
    int doProcessFrames(const void *buffer, ssize_t frames,
                        ssize_t *processed_frames,
                        ssize_t *processing_frames_in);

    /**
     * Read frames from echo reference buffer and update echo delay.
     *
     * @param[in] frames to read from echo reference.
     * @param[in] preprocessor handle on AEC preprocessor.
     * @param[in] reference echo reference structure.
     *
     * @return OK if successful operation, error code otherwise.
     */
    android::status_t pushEchoReference(ssize_t frames, effect_handle_t preprocessor,
                                        struct echo_reference_itfe *reference);

    /**
     * Update the echo reference with the frames read from the audio device.
     *
     * @param[in] frames number of frames ready to process by AEC.
     * @param[in] reference echo reference handle.
     *
     * @return 0 if successful operation, error code otherwise.
     */
    int32_t updateEchoReference(ssize_t frames, struct echo_reference_itfe *reference);

    /**
     * Set preprocessor echo delay.
     *
     * @param[out] effect preprocessor handle.
     * @param[out] delay_us delay of the echo in micro seconds.
     *
     * @return OK if successful operation, error code otherwise.
     */
    android::status_t setPreprocessorEchoDelay(effect_handle_t effect, int32_t delay_us);

    /**
     * Set preprocessor parameters.
     *
     * @param[out] effect preprocessor handle
     * @param[out] param parameters to send to the preprocessor.
     *
     * @return OK if successful operation, error code otherwise.
     */
    android::status_t setPreprocessorParam(effect_handle_t effect, effect_param_t *param);

    /**
     * Get the capture delay.
     * It computes the time between the data were read and retrieved and sets the value in the
     * echo reference structure.
     *
     * @param[in,out] buffer echo reference structure.
     */
    void getCaptureDelay(struct echo_reference_buffer *buffer);

    /**
     * amount of input frames lost in the audio driver (i.e. not provided on time to client).
     */
    unsigned int mFramesLost;

    ssize_t mFramesIn; /**< frames available in stream input buffer. */

    /**
     * This variable represents the number of frames of in mProcessingBuffer.
     */
    ssize_t mProcessingFramesIn;

    /**
     * This variable is a dynamic buffer and contains raw data read from input device.
     * It is used as input buffer before application of SW accoustics effects.
     */
    int16_t *mProcessingBuffer;

    /**
     * This variable represents the size in frames of in mProcessingBuffer.
     */
    ssize_t mProcessingBufferSizeInFrames;

    /**
     * This variable represents the number of frames of in mReferenceBuffer.
     */
    ssize_t mReferenceFramesIn;

    /**
     * This variable is a dynamic buffer and contains the data used as reference for AEC and
     * which are read from AudioEffectHandle::mEchoReference.
     */
    int16_t *mReferenceBuffer;

    /**
     * This variable represents the size in frames of in mReferenceBuffer.
     */
    ssize_t mReferenceBufferSizeInFrames;

    /**
     * It is vector which contains the handlers to accoustics SW effects.
     */
    std::vector<AudioEffectHandle> mPreprocessorsHandlerList;

    char *mHwBuffer; /**< buffer in which samples are read from audio device. */
    ssize_t mHwBufferSize; /**< Size of the buffer in which samples are read from audio device. */

    static const std::string mHwEffectImplementor; /**< Implementor name for HW effects. */
};
} // namespace intel_audio
