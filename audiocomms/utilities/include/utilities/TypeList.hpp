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

/** Typelist to be used in metaprograming.
 *
 * Sometime it is usefull to get multiple types as template param. As this is
 * not part of the syntax (at least before c++11), type list solves this problem.
 *
 * To iterate on a Typelist, use head tail recursion and pattern matching.
 */
template <class H, class T>
struct TypeList
{
    typedef H head;
    typedef T tail;
};

namespace detail
{
/** The last element of a TypeList is always a TypeNull.
 *
 * This type is implementation specific, and should never be referenced directly.
 */
struct TypeNull;
} // detail

#define TYPELIST0 \
    audio_comms::utilities::detail::TypeNull
#define TYPELIST1(A) \
    audio_comms::utilities::TypeList < A, TYPELIST0 >
#define TYPELIST2(A, B) \
    audio_comms::utilities::TypeList < A, TYPELIST1(B) >
#define TYPELIST3(A, B, C) \
    audio_comms::utilities::TypeList < A, TYPELIST2(B, C) >
#define TYPELIST4(A, B, C, D) \
    audio_comms::utilities::TypeList < A, TYPELIST3(B, C, D) >
#define TYPELIST5(A, B, C, D, E) \
    audio_comms::utilities::TypeList < A, TYPELIST4(B, C, D, E) >
#define TYPELIST6(A, B, C, D, E, F) \
    audio_comms::utilities::TypeList < A, TYPELIST5(B, C, D, E, F) >
#define TYPELIST7(A, B, C, D, E, F, G) \
    audio_comms::utilities::TypeList < A, TYPELIST6(B, C, D, E, F, G) >
#define TYPELIST8(A, B, C, D, E, F, G, H) \
    audio_comms::utilities::TypeList < A, TYPELIST7(B, C, D, E, F, G, H) >
#define TYPELIST9(A, B, C, D, E, F, G, H, I) \
    audio_comms::utilities::TypeList < A, TYPELIST8(B, C, D, E, F, G, H, I) >
#define TYPELIST10(A, B, C, D, E, F, G, H, I, J) \
    audio_comms::utilities::TypeList < A, TYPELIST9(B, C, D, E, F, G, H, I, J) >
#define TYPELIST11(A, B, C, D, E, F, G, H, I, J, K) \
    audio_comms::utilities::TypeList < A, TYPELIST10(B, C, D, E, F, G, H, I, J, K) >
#define TYPELIST12(A, B, C, D, E, F, G, H, I, J, K, L) \
    audio_comms::utilities::TypeList < A, TYPELIST11(B, C, D, E, F, G, H, I, J, K, L) >
#define TYPELIST13(A, B, C, D, E, F, G, H, I, J, K, L, M) \
    audio_comms::utilities::TypeList < A, TYPELIST12(B, C, D, E, F, G, H, I, J, K, L, M) >
#define TYPELIST14(A, B, C, D, E, F, G, H, I, J, K, L, M, N) \
    audio_comms::utilities::TypeList < A, TYPELIST13(B, C, D, E, F, G, H, I, J, K, L, M, N) >
#define TYPELIST15(A, B, C, D, E, F, G, H, I, J, K, L, M, N, O) \
    audio_comms::utilities::TypeList < A, TYPELIST14(B, C, D, E, F, G, H, I, J, K, L, M, N, O) >
#define TYPELIST16(A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P) \
    audio_comms::utilities::TypeList < A, TYPELIST15(B, C, D, E, F, G, H, I, J, K, L, M, N, O, P) >
#define TYPELIST17(A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q) \
    audio_comms::utilities::TypeList < A, TYPELIST16(                 \
        B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q) >
#define TYPELIST18(A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R) \
    audio_comms::utilities::TypeList < A, TYPELIST17(                    \
        B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R) >
#define TYPELIST19(A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S) \
    audio_comms::utilities::TypeList < A, TYPELIST18(                       \
        B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S) >
#define TYPELIST20(A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T) \
    audio_comms::utilities::TypeList < A, TYPELIST19(                          \
        B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T) >
#define TYPELIST21(A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U) \
    audio_comms::utilities::TypeList < A, TYPELIST20(                             \
        B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U) >
#define TYPELIST22(A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V) \
    audio_comms::utilities::TypeList < A, TYPELIST21(                                \
        B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V) >
#define TYPELIST23(A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W) \
    audio_comms::utilities::TypeList < A, TYPELIST22(                                   \
        B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W) >
#define TYPELIST24(A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X) \
    audio_comms::utilities::TypeList < A, TYPELIST23(                                      \
        B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X) >
#define TYPELIST25(A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y) \
    audio_comms::utilities::TypeList < A, TYPELIST24(                                         \
        B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y) >
#define TYPELIST26(A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z) \
    audio_comms::utilities::TypeList < A, TYPELIST25(                                            \
        B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z) >

/**
 * Append Helper for TypeLists
 * This class allow to Append a typelist to another
 * @tparam List1 first list
 * @tparam List2 second list
 * The result appended list is: Append<List1, List2>::list
 */
template <class List1, class List2>
struct Append;

template <class Head, class Tail, class List2>
struct Append<TypeList<Head, Tail>, List2>
{
    typedef TypeList<Head, typename Append<Tail, List2>::list> list;
};

template <class List>
struct Append<TYPELIST0, List>
{
    typedef List list;
};

} // namespace utilities
} // namespace audio_comms
