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

#include "serializer/framework/TextNode.hpp"
#include "serializer/framework/Child.hpp"
#include "utilities/TypeList.hpp"

namespace audio_comms
{
namespace utilities
{
namespace serializer
{

/** Wrap a text node around xml element.
 *
 * Sometime it is useful to surround a text node with an xml element.
 * Example:
 *  - using a TextTrait<int> will be serialized to "20".
 *  - using a NamedTextTrait<int, "num"> will be serialized in "<num>20</num>"
 */
template <typename T, const char *_tag, bool optional = false,
          class Convertor = typename detail::TextNodeDefaultConvertor<T>::Type>
struct NamedTextTrait
{
    static const char *tag;
    typedef T Element;

    static const T &get(const Element &e) { return e; }

    static void set(Element &e, const T &newValue) { e = newValue; }

    typedef const T & (*Get)(const Element &);
    typedef void (*Set)(Element &, const T &);

    typedef Child<TextTrait<T, Convertor>,
                  Get, &NamedTextTrait<T, _tag>::get,
                  Set, &NamedTextTrait<T, _tag>::set, false, optional> Child1;
    typedef TYPELIST1 (Child1) Children;
};
template <typename T, const char *_tag, bool o, class C>
const char *NamedTextTrait<T, _tag, o, C>::tag = _tag;

} // namespace serializer
} // namespace utilities
} // namespace audio_comms
