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

#include <result/Result.hpp>

namespace audio_comms
{
namespace utilities
{
namespace xmlserializer
{

enum ResultCode
{
    successCode = 66,
    unknown,
    wrongXmlNode,
    childNotFound,
    unexpectedNodeType,
    conversionFailed,
    invalidArgument,
    invalidInputData
};

namespace detail
{
struct ResultTrait
{
    typedef ResultCode Code;
    /** Enum coding the failures that can occur in the class methods. */
    static const Code success = successCode;
    static const Code defaultError = unknown;

    static std::string codeToString(const Code &code)
    {
        switch (code) {
        case successCode:
            return "Success";
        case unknown:
            return "Unknown error";
        case wrongXmlNode:
            return "Wrong XML node";
        case childNotFound:
            return "Child not found";
        case unexpectedNodeType:
            return "Unexpected node type";
        case conversionFailed:
            return "Conversion failed";
        case invalidArgument:
            return "Invalid argument";
        case invalidInputData:
            return "Input stream is invalid";
        }
        /* Unreachable, prevents gcc to complain */
        return "Invalid error (Unreachable)";
    }
};
} // namespace detail

typedef result::Result<detail::ResultTrait> Result;

} // namespace serializer
} // namespace utilities
} // namespace audio_comms
