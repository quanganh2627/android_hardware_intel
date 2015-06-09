/*
 * Copyright 2014 Intel Corporation
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

#include "RemoteParameter.hpp"
#include <AudioCommsAssert.hpp>
#include <string>
#include <stdint.h>
#include <sys/types.h>
#include <string.h>

static const uint32_t MAX_LENGTH = 256; /**< Max remote parameter string length. */
/**
 * Max remote parameter size including termination char.
 */
static const size_t MAX_SIZE = MAX_LENGTH + 1;

template <>
RemoteParameter<std::string>::RemoteParameter(const std::string &parameterName,
                                              const std::string &trustedPeerUserName)
    : RemoteParameterBase(parameterName, MAX_SIZE, trustedPeerUserName)
{}

template <>
RemoteParameter<std::string>::RemoteParameter(const std::string &parameterName,
                                              uid_t trustedPeerUid)
    : RemoteParameterBase(parameterName, MAX_SIZE, trustedPeerUid)
{}



template <>
bool RemoteParameter<std::string>::set(const uint8_t *data, size_t size)
{
    char typedData[size];
    AUDIOCOMMS_ASSERT(size <= MAX_SIZE, "Size to set must not exceed string max length");
    memcpy(static_cast<void *>(&typedData), static_cast<const void *>(data), sizeof(typedData));
    return set(typedData);
}

template <>
void RemoteParameter<std::string>::get(uint8_t *data, size_t &size) const
{
    std::string typedData;
    get(typedData);
    AUDIOCOMMS_ASSERT(size >= typedData.length() + 1,
                      "received size: " << typedData.length() + 1
                                        << " does not match requested size: " <<
                      size);
    size = typedData.length() + 1;
    memcpy(static_cast<void *>(data),
           typedData.c_str(),
           typedData.length() + 1);
}
