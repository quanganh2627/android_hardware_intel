/**
 * @section License
 *
 * Copyright 2013-2014 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include "result/Result.hpp"
#include <cerrno>
#include <cstring>
#include <string>

namespace audio_comms
{
namespace utilities
{
namespace result
{

/** Code list can by return by stl functions */
class ErrnoTrait
{
public:
    typedef int Code;
    static const Code success = 0;
    static const Code defaultError = -666;

    /** Return a string describing the errno number. */
    static std::string codeToString(Code errnum)
    {
        return strerror(errnum);
    }
};

typedef Result<ErrnoTrait> ErrnoResult;

} /* namespace result */

} /* namespace utilities */

} /* namespace audio_comms */
