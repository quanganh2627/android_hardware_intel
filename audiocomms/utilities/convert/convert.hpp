/*
 ** Copyright 2013 Intel Corporation
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **     http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */

#pragma once

#include <limits>
#include <sstream>
#include <string>
#include <stdint.h>

namespace audio_comms
{

namespace utilities
{

/* details namespace is here to hide implementation details to header end user. It
 * is NOT intended to be used outside. */
namespace details
{

/** Helper class to limit instantiation of templates */
template<typename T>
struct ConvertionAllowed;

/* List of allowed types for conversion */
template<> struct ConvertionAllowed<bool> {};
template<> struct ConvertionAllowed<uint64_t> {};
template<> struct ConvertionAllowed<int64_t> {};
template<> struct ConvertionAllowed<uint32_t> {};
template<> struct ConvertionAllowed<int32_t> {};
template<> struct ConvertionAllowed<uint16_t> {};
template<> struct ConvertionAllowed<int16_t> {};
template<> struct ConvertionAllowed<float> {};

} // namespace details

/**
 * Convert a string to a given type.
 *
 * This template function read the value of the type T in the given string.
 * The function does not allow to have white spaces around the value to parse
 * and tries to parse the whole string, which means that if some bytes were not
 * read in the string, the function fails.
 * Hexadecimal representation (ie numbers starting with 0x) is supported only
 * for integral types conversions.
 * Result may be modified, even in case of failure.
 *
 * @param[in]  str    the string to parse.
 * @param[out] result reference to object where to store the result.
 *
 * @return true if conversion was successful, false otherwise.
 */
template<typename T>
static inline bool convertTo(const std::string &str, T &result)
{
    /* Check that conversion to that type is allowed.
     * If this fails, this means that this template was not intended to be used
     * with this type, thus that the result is undefined. */
    details::ConvertionAllowed<T>();

    if (str.find_first_of(std::string("\r\n\t\v ")) != std::string::npos) {
        return false;
    }

    /* Check for a '-' in string. If type is unsigned and a - is found, the
     * parsing fails. This is made necessary because "-1" is read as 65535 for
     * uint16_t, for example */
    if (str.find("-") != std::string::npos
        && !std::numeric_limits<T>::is_signed) {
        return false;
    }

    std::stringstream ss(str);

    /* Sadly, the stream conversion does not handle hexadecimal format, thus
     * check is done manually */
    if (str.substr(0, 2) == "0x") {
        if (std::numeric_limits<T>::is_integer) {
            ss >> std::hex >> result;
        }
        else {
            /* Conversion undefined for non integers */
            return false;
        }
    } else {
        ss >> result;
    }

    return ss.eof() && !ss.fail() && !ss.bad();
}

/**
 * Specialization for int16_t of convertTo template function.
 *
 * This function follows the same paradigm than it's generic version.
 *
 * The specific implementation is made necessary because the stlport version of
 * string streams is bugged and does not fail when giving overflowed values.
 * This specialisation can be safely removed when stlport behaviour is fixed.
 *
 * @param[in]  str    the string to parse.
 * @param[out] result reference to object where to store the result.
 *
 * @return true if conversion was successful, false otherwise.
 */
template<>
inline bool convertTo<int16_t>(const std::string &str, int16_t &result)
{
    int64_t res;

    if (!convertTo<int64_t>(str, res)) {
        return false;
    }

    if (res > std::numeric_limits<int16_t>::max() || res < std::numeric_limits<int16_t>::min()) {
        return false;
    }

    result = static_cast<int16_t>(res);
    return true;
}

/**
 * Specialization for boolean of convertTo template function.
 *
 * This function follows the same paradigm than it's generic version.
 * This function accepts to parse boolean as "0/1" or "false/true" or
 * "FALSE/TRUE".
 * The specific implementation is made necessary because the behaviour of
 * string streams when parsing boolean values is not sufficient to fit our
 * requirements. Indeed, parsing "true" will correctly parse the value, but the
 * end of stream is not reached which makes the ss.eof() fails in the generic
 * implementation.
 *
 * @param[in]  str    the string to parse.
 * @param[out] result reference to object where to store the result.
 *
 * @return true if conversion was successful, false otherwise.
 */
template<>
inline bool convertTo<bool>(const std::string &str, bool &result)
{
    if (str == "0" || str == "FALSE" || str == "false") {
        result = false;
        return true;
    }

    if (str == "1" || str == "TRUE" || str == "true") {
        result = true;
        return true;
    }

    return false;
}

} // namespace utilities

} // namespace audio_comms
