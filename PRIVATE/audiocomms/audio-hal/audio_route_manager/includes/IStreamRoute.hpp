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
#include <string>

namespace intel_audio
{

class StreamRouteConfig;
class IAudioDevice;

class IStreamRoute
{
public:
    /**
     * Get the sample specifications of the stream route.
     *
     * @return sample specifications.
     */
    virtual const SampleSpec getSampleSpec() const = 0;

    /**
     * Get output silence to be appended before playing.
     * Some route may require to append silence in the ring buffer as powering on of components
     * involved in this route may take a while, and audio sample might be lost. It will result in
     * loosing the beginning of the audio samples.
     *
     * @return silence to append in milliseconds.
     */
    virtual uint32_t getOutputSilencePrologMs() const = 0;

    virtual IAudioDevice *getAudioDevice() = 0;

    virtual ~IStreamRoute() {}
};

} // namespace intel_audio
