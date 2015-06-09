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

#include "Stream.hpp"
#include "Device.hpp"
#include <StreamInterface.hpp>

struct echo_reference_itfe;

namespace intel_audio
{

class StreamOut : public StreamOutInterface, public Stream
{
public:
    StreamOut(Device *parent, uint32_t streamFlagsMask);
    virtual ~StreamOut();


    // From AudioStreamOut
    virtual uint32_t getLatency();
    /** @note API not implemented in our Audio HAL */
    virtual android::status_t setVolume(float, float) { return android::OK; }
    virtual android::status_t write(const void *buffer, size_t &bytes);
    virtual android::status_t getRenderPosition(uint32_t &dspFrames) const;
    /** @note API not implemented in our Audio HAL */
    virtual android::status_t getNextWriteTimestamp(int64_t & /*ts*/) const { return android::OK; }
    virtual android::status_t flush();
    /** @note API not implemented in our Audio HAL */
    virtual android::status_t setCallback(stream_callback_t, void *) { return android::OK; }
    /** @note API not implemented in our Audio HAL */
    virtual android::status_t pause() { return android::OK; }
    /** @note API not implemented in our Audio HAL */
    virtual android::status_t resume() { return android::OK; }
    /** @note API not implemented in our Audio HAL */
    virtual android::status_t drain(audio_drain_type_t) { return android::OK; }
    /** @note API not implemented in our Audio HAL */
    virtual android::status_t getPresentationPosition(uint64_t &, struct timespec &) const
    {
        return android::OK;
    }
    virtual android::status_t setDevice(audio_devices_t device);

    /**
     * Request to provide Echo Reference.
     *
     * @param[in] echo reference structure pointer.
     */
    void addEchoReference(struct echo_reference_itfe *reference);

    /**
     * Cancel the request to provide Echo Reference.
     *
     * @param[in] echo reference structure pointer.
     *
     */
    void removeEchoReference(struct echo_reference_itfe *reference);

    // From IoStream
    /**
     * Get stream direction. From Stream class.
     *
     * @return always true for ouput, false for input.
     */
    virtual bool isOut() const { return true; }

protected:
    /**
     * Callback of route attachement called by the stream lib. (and so route manager)
     * Inherited from Stream class
     *
     * @return OK if streams attached successfully to the route, error code otherwise.
     */
    virtual android::status_t attachRouteL();

    /**
     * Callback of route detachement called by the stream lib. (and so route manager)
     * Inherited from Stream class
     *
     * @return OK if streams detached successfully from the route, error code otherwise.
     */
    virtual android::status_t detachRouteL();

private:
    /**
     * Push samples to echo reference.
     *
     * @param[in] buffer: output stream audio buffer to be appended to echo reference.
     * @param[in] frames: number of frames to be appended in echo reference.
     */
    void pushEchoReference(const void *buffer, ssize_t frames);

    /**
     * Get the playback delay.
     * Used when SW AEC effect is activated to informs at best the AEC engine of the rendering
     * delay.
     *
     * @param[in] frames: frames pushed in the echo reference.
     * @param[out] buffer: echo reference buffer given to set the timestamp when echo reference
     *                     is provided.
     *
     * @return 0 if success, error code otherwise.
     */
    int getPlaybackDelay(ssize_t frames, struct echo_reference_buffer *buffer);

    uint32_t mFrameCount; /**< number of audio frames written by AudioFlinger. */

    struct echo_reference_itfe *mEchoReference; /**< echo reference pointer, for SW AEC effect. */

    static const uint32_t mMaxAgainRetry; /**< Max retry for write operations before recovering. */
    static const uint32_t mWaitBeforeRetryUs; /**< Time to wait before retrial. */
    static const uint32_t mUsecPerMsec; /**< time conversion constant. */
};
} // namespace intel_audio
