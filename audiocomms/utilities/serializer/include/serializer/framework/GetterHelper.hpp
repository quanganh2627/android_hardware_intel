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

/** Getter helper
 * Transforms a method getter to a function getter.
 * If M is already a function getter forwards it.
 */
template <class M, M m>
struct GetterHelper;

// Method getter
template <class Ret, class Class, Ret(Class::*getter) ()>
struct GetterHelper<Ret(Class::*) (), getter>
{
    typedef Ret (*type) (Class &instance);
    static Ret function(Class &instance)
    {
        return (instance.*getter)();
    }
};

// Const method getter
template <class Ret, class Class, Ret(Class::*getter) () const>
struct GetterHelper<Ret(Class::*) () const, getter>
{
    typedef Ret (*type) (const Class &);
    static Ret function(const Class &instance)
    {
        return (instance.*getter)();
    }
};

// Function getter
template <class Ret, class Class, Ret(*getter) (Class)>
struct GetterHelper<Ret (*)(Class), getter>
{
    typedef Ret (*type) (Class &);
    static Ret function(Class &instance)
    {
        return (*getter)(instance);
    }
};

} // namespace serializer
} // namespace utilities
} // namespace audio_comms
