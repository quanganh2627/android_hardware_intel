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

/**
 * Server side Remote Parameter template.
 * This class may be used if parameter handled as an typed parameter.
 */
template <typename TypeParam>
RemoteParameter<TypeParam>::RemoteParameter(const std::string &parameterName,
                                            const std::string &allowedPeerUserName)
    : RemoteParameterBase(parameterName, sizeof(TypeParam), allowedPeerUserName)
{}

template <typename TypeParam>
RemoteParameter<TypeParam>::RemoteParameter(const std::string &parameterName,
                                            uid_t trustedPeerUid)
    : RemoteParameterBase(parameterName, sizeof(TypeParam), trustedPeerUid)
{}

template <typename TypeParam>
bool RemoteParameter<TypeParam>::set(const uint8_t *data, size_t size)
{
    TypeParam typedData;
    AUDIOCOMMS_ASSERT(size == sizeof(typedData), "Size to set must match parameter size");
    memcpy(static_cast<void *>(&typedData), static_cast<const void *>(data), sizeof(typedData));
    return set(typedData);
}

template <typename TypeParam>
void RemoteParameter<TypeParam>::get(uint8_t *data, size_t &size) const
{
    TypeParam typedData;
    AUDIOCOMMS_ASSERT(size == sizeof(typedData), "received size does not match");
    get(typedData);
    memcpy(static_cast<void *>(data), static_cast<void *>(&typedData), sizeof(typedData));
}


template class RemoteParameter<uint32_t>;
template class RemoteParameter<bool>;
