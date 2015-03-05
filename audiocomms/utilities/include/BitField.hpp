/*
 *
 * Copyright 2013 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "AudioCommsAssert.hpp"
#include <utils/Log.h>

namespace audio_comms
{

namespace utilities
{

class BitField
{
public:
    /**
     * Converts an index to a mask
     *
     * @param[in] index: index to be transposed in a mask, it may assert if index is out of range.
     *
     * @return mask
     */
    static inline uint32_t indexToMask(uint32_t index)
    {
        AUDIOCOMMS_ASSERT(index < _wordSize, "Index outside range");
        return 1 << index;
    }
private:
    static const uint32_t _wordSize = 32;
};
} // namespace utilities

} // namespace audio_comms
