/*
 * @file
 * audio comms assert facilities for C++.
 *
 * @section License
 *
 * Copyright 2013 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

/**
 * Macro to wrap gcc builtin function to specify that a condition is unlikely
 * to happen. */
#define audio_comms_unlikely(cond) __builtin_expect((cond), 0)

/**
 * This AUDIOCOMMS_COMPILE_TIME_ASSERT MACRO will fail to compile if the condition is false.
 * Currently it only works in functions
 * Example:
 *     Put "AUDIOCOMMS_COMPILE_TIME_ASSERT(1==0)" your C++ source.
 * On compilation you will get error that looks like:
 * error: 'compileTimeAssertFailure<false>::compileTimeAssertFailure()' is private
 *      YOUR_SOURCE_FILE.cpp:YOUR_ASSERT_LINE: error: within this context
 */
#define AUDIOCOMMS_COMPILE_TIME_ASSERT(c) audio_comms::utilities::compileTimeAssertFailure<c>()

namespace audio_comms
{

namespace utilities
{

template <bool condition>
class compileTimeAssertFailure;

// If true do nothing
template <>
class compileTimeAssertFailure<true> {};

// If false instanciate with private constructor
template <>
class compileTimeAssertFailure<false>
{
    compileTimeAssertFailure();
};

} // namespace utilities

} // namespace audio_comms

#include <cstdlib> /* assert */
#include <iostream>

/**
 * Checks a condition and abort if it is not met.
 * This assert is ALWAYS activated (even in release mode), thus it must be used
 * wisely.
 *
 * @param[in] cond   the condition to check
 * @param[in] iostr  an iostream giving some details about the error
 */
#define AUDIOCOMMS_ASSERT(cond, iostr) \
 do { \
   if (audio_comms_unlikely(!(cond))) { \
       std::cerr << __BASE_FILE__ ":" << __LINE__ << ": Assertion " #cond " failed: " \
                 << iostr << std::endl; \
       abort(); \
   } \
 } while (false)

