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

#include "serializer/framework/ConvertToString.hpp"
#include <convert.hpp>
#include <string>

namespace audio_comms
{
namespace utilities
{
namespace serializer
{

/** Policy class that handle T from/to string conversion. */
template <class T>
struct Convertor
{
    static bool toString(const T &raw, std::string &str)
    {
        return serializer::toString(raw, str);
    }
    static bool fromString(const std::string &str, T &raw)
    {
        return convertTo(str, raw);
    }
};

/** Specialisation for string, conversion is forwarding. */
template <>
struct Convertor<std::string>
{
    static bool toString(const std::string &raw, std::string &str)
    {
        str = raw;
        return true;
    }
    static bool fromString(const std::string &str, std::string &raw)
    {
        raw = str;
        return true;
    }
};


/** Policy class conversion with a cast from RealType to ConversionType.
 *
 * When using Convertor convert first RealType to ConversionType.
 */
template <class RealType, class ConversionType>
struct CastConvertor
{
    static bool toString(const RealType &raw, std::string &str)
    {
        return Convertor<ConversionType>::toString(
            reinterpret_cast<const ConversionType &>(raw),
            str);
    }
    static bool fromString(const std::string &str, RealType &raw)
    {
        return Convertor<ConversionType>::fromString(
            str,
            reinterpret_cast<ConversionType &>(raw));
    }
};

} // namespace serializer
} // namespace utilities
} // namespace audio_comms
