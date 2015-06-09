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

#include "serializer/framework/ConvertorPolicy.hpp"

namespace audio_comms
{
namespace utilities
{
namespace serializer
{

namespace detail
{
/** Define the default convertor for text nodes. */
template <typename T>
struct TextNodeDefaultConvertor
{
    typedef Convertor<T> Type;
};
} // namespace detail

/** Use this template to create the trait of stringable types.
 *
 * Used mainly to serialize basic types.
 */
template <typename Type,
          class Convertor = typename detail::TextNodeDefaultConvertor<Type>::Type>
struct TextTrait
{
    typedef Type Element;
};

} // namespace serializer
} // namespace utilities
} // namespace audio_comms
