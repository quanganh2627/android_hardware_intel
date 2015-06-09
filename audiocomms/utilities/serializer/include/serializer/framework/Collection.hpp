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

namespace audio_comms
{
namespace utilities
{
namespace serializer
{

/** Trait that can be used to serialized any Collection instance.
 *
 * tparam _tag names the collection
 * tparam Collection is the collection type to serialize. It should implement
 *                   like some stl containors (as std::list):
 *                   - begin, end and push_back methods
 *                   - iterator, const_iterator and value_type types
 * tparam _ItemTrait is a trait to serialize Collection::value_type.
 * tparam takeOwnership if the Collection::push_back method takes the ownership
 *                      of the pushed item (ie. if it should not be deleted
 *                      after), @see Child.
 */
template <const char *_tag, class Collection, class _ItemTrait,
          bool takeOwnership = false>
struct CollectionTrait
{
    static const char *tag;
    typedef Collection Element;
};
template <const char *_tag, class C, class I, bool o>
const char *CollectionTrait<_tag, C, I, o>::tag = _tag;

} // namespace serializer
} // namespace utilities
} // namespace audio_comms
