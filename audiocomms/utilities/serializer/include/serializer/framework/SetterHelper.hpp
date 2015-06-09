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

/** Setter helper
 * Transforms a method setter to a function setter.
 * If M is already a function setter forwards it.
 */
template <class M, M m>
struct SetterHelper;

// Method setter
template <class Ret, class Class, class Arg, Ret(Class::*setter) (Arg)>
struct SetterHelper<Ret(Class::*) (Arg), setter>
{
    typedef Ret (*type) (Class &, Arg);
    static Ret function(Class &instance, Arg arg)
    {
        return (instance.*setter)(arg);
    }
};

// Const method setter
template <class Ret, class Class, class Arg, Ret(Class::*setter) (Arg)const>
struct SetterHelper<Ret(Class::*) (Arg)const, setter>
{
    typedef Ret (*type) (const Class &, Arg);
    static Ret function(const Class &instance, Arg arg)
    {
        return (instance.*setter)(arg);
    }
};

// Function setter
template <class Ret, class Class, class Arg, Ret(*setter) (Class, Arg)>
struct SetterHelper<Ret (*)(Class, Arg), setter>
{
    typedef Ret (*type) (Class &, Arg);
    static Ret function(Class &instance, Arg arg)
    {
        return (*setter)(instance, arg);
    }
};

} // namespace serializer
} // namespace utilities
} // namespace audio_comms
