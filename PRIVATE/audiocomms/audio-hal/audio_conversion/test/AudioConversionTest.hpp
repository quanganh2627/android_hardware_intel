/*
 * INTEL CONFIDENTIAL
 * Copyright (c) 2014 Intel
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

#include <stddef.h>
#include <media/AudioBufferProvider.h>
#include <NonCopyable.hpp>
#include <gtest/gtest.h>
#include <stdint.h>

namespace intel_audio
{

class MyAudioBufferProvider
    : public android::AudioBufferProvider,
      private audio_comms::utilities::NonCopyable
{

public:
    MyAudioBufferProvider(const uint16_t *src, uint32_t samples)
        : readPos(0)
    {

        sourceBuffer = new uint16_t[samples];
        memcpy(sourceBuffer, src, samples * sizeof(uint16_t));
    }

    ~MyAudioBufferProvider()
    {

        delete[] sourceBuffer;
    }

    /**
     * Provides new buffer to the client.
     * From AudioBufferProvider interface.
     *
     * @param[out] buffer audio data to be provided to the client.
     *                    Buffer shall not be taken into account by the client if
     *                    error code is returned.
     * @param[in] pts local time when the next sample yielded by getNextBuffer will be rendered.
     *
     * @return status_t OK if buffer provided, error code otherwise.
     */
    virtual android::status_t getNextBuffer(android::AudioBufferProvider::Buffer *buffer,
                                            int64_t /*pts*/)
    {

        buffer->raw = &sourceBuffer[readPos];
        readPos += buffer->frameCount;
        return android::NO_ERROR;
    }

    /**
     * Release buffer provided to the client on previous getNextBuffer call.
     * From AudioBufferProvider interface.
     *
     * @param[out] buffer client buffer to be released. Audio data have been consumed by the client
     *                    and the provider memory can now be released.
     */
    virtual void releaseBuffer(Buffer */*buffer*/) {}

private:
    uint32_t readPos; /**< Position within the source buffer. */
    uint16_t *sourceBuffer; /**< Source buffer to convert. */

};

}  // namespace intel_audio
