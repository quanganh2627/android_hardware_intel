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

#include <sstream>
#include <string>

namespace audio_comms
{
namespace utilities
{
namespace serializer
{

/** Convert an element of type Source to string. */
template <typename Source>
static bool toString(const Source &source, std::string &dest)
{
    std::ostringstream stream;
    stream << source;
    dest = stream.str();
    return not stream.fail();

}

/** For boolean, sepecialize to return "true" or "false". */
template <>
inline bool toString<bool>(const bool &source, std::string &dest)
{
    if (source) {
        dest = "true";
    } else {
        dest = "false";
    }
    return true;
}

} // namespace serializer
} // namespace utilities
} // namespace audio_comms
