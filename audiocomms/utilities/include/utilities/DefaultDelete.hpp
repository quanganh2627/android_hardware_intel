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

/** Function object class, takes an object of type T* and deletes it.
 *
 * The non-specialized version simply uses delete for the deletion operation.
 *
 * Likewise, the specialization for arrays with runtime length uses delete[].
 *
 * This it the implementation of std::default_delete (c++11)
 */
template <class T>
class DefaultDelete
{
public:
    /** Calls delete on ptr. */
    void operator()(T *ptr) const
    {
        delete ptr;
    }
};

/** Partial specialization for array types that uses delete[] is also provided.
 *
 * @see DefaultDelete and std::default_delete (c++11)
 * */
template <class T>
class DefaultDelete<T[]>
{
public:
    /** Calls delete[] on ptr. */
    void operator()(T array[]) const
    {
        delete[] array;
    }
};

} // namespace utilities
} // namespace audio_comms
