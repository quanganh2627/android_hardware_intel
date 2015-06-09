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

#include "IoStream.hpp"
#include <SampleSpec.hpp>
#include <utils/RWLock.h>

namespace intel_audio
{

class TinyAlsaAudioDevice;

class TinyAlsaIoStream : public IoStream
{
public:
    TinyAlsaIoStream()
        : IoStream::IoStream(), mDevice(NULL)
    {}

    virtual uint32_t getBufferSizeInBytes() const;

    virtual size_t getBufferSizeInFrames() const;

    virtual android::status_t pcmReadFrames(void *buffer, size_t frames, std::string &error) const;

    virtual android::status_t pcmWriteFrames(void *buffer, ssize_t frames,
                                             std::string &error) const;

    virtual android::status_t pcmStop() const;

    /**
     * Returns available frames in pcm buffer and corresponding time stamp.
     * For an input stream, frames available are frames ready for the
     * application to read.
     * For an output stream, frames available are the number of empty frames available
     * for the application to write.
     */
    virtual android::status_t getFramesAvailable(uint32_t &avail, struct timespec &tStamp) const;

protected:
    /**
     * Attach the stream to its route.
     * Called by the StreamRoute to allow accessing the pcm device.
     *
     * @return true if attach successful, false otherwise.
     */
    virtual android::status_t attachRouteL();

    /**
     * Detach the stream from its route.
     * Either the stream has been preempted by another stream or the stream has stopped.
     * Called by the StreamRoute to prevent from accessing the device any more.
     *
     * @return true if detach successful, false otherwise.
     */
    virtual android::status_t detachRouteL();

private:
    /**
     * Get the pcm device handle.
     * Must only be called if isRouteAvailable returns true.
     * and any access to the device must be called with Lock held.
     *
     * @return pcm handle on alsa tiny device.
     */
    pcm *getPcmDevice() const;

    TinyAlsaAudioDevice *mDevice;

    /** Ratio between microseconds and milliseconds */
    static const uint32_t mUsecPerMsec = 1000;
};

} // namespace intel_audio
