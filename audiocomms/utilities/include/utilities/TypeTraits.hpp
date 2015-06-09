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

/** @file
 * This file aims to implement the c++11 type_traits new header.
 */

namespace audio_comms
{
namespace utilities
{

/** Check that 2 types are the same.
 * @see std::is_same (c++11)*/
template <class A, class B>
struct is_same
{
    static const bool value = false;
};

template <class A>
struct is_same<A, A>
{
    static const bool value = true;
};

/** Remove the reference of a type.
 * @see std::remove_reference (c++11)
 *
 * If the type T is a reference type,
 * provides the member typedef type which is the type referred to by T.
 * Otherwise type is T.
 */
template <class T>
struct remove_reference
{
    /** The type referred by T or T if it is not a reference */
    typedef T type;
};
template <class T>
struct remove_reference<T &>
{
    typedef T type;
};

/** Remove the pointer of a type.
 * @see std::remove_pointer (c++11)
 *
 * If the type T is a pointer type,
 * provides the member typedef type which is the type pointed to by T,
 * otherwise type is the same as T.
 */
template <class T>
struct remove_pointer
{
    typedef T type;
};
template <class T>
struct remove_pointer<T *>
{
    typedef T type;
};
template <class T>
struct remove_pointer<T *const>
{
    typedef T type;
};
template <class T>
struct remove_pointer<T *volatile>
{
    typedef T type;
};
template <class T>
struct remove_pointer<T *const volatile>
{
    typedef T type;
};

} // namespace utilities
} // namespace audio_comms
