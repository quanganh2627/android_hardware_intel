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

#include "serializer/framework/GetterHelper.hpp"
#include "serializer/framework/SetterHelper.hpp"

namespace audio_comms
{
namespace utilities
{
namespace serializer
{

/** Template used to describe a class child (member). */
template <typename _ChildTrait,
          typename _Getter, _Getter _getter,
          typename _Setter, _Setter _setter, bool _takeOwnership,
          bool _optional = false>
struct Child
{
    typedef _ChildTrait ChildTrait;
    typedef GetterHelper<_Getter, _getter> Getter;
    typedef SetterHelper<_Setter, _setter> Setter;
    static const bool takeOwnership = _takeOwnership;
    /** @fixme: It would be far better to be optional if the _ChildTrait had a
     * specific base class (called Optional).
     * Instead of writing : Child<myChildTrait, ... , true>
     * you could write :    Child<Optional<myChildTrait, ...> >
     * with Optional defined as such :
     * template <class Base> struct Optional : Base {};
     */
    static const bool optional = _optional;
};

} // namespace serializer
} // namespace utilities
} // namespace audio_comms
