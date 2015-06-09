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

#include "RemoteParameterProxy.hpp"
#include "RemoteParameterProxyImpl.hpp"
#include <string>
#include <stdint.h>
#include <sys/types.h>
#include <utils/Log.h>
#include <string.h>

static const uint32_t MAX_LENGTH = 256; /**< Max remote parameter string length. */
/**
 * Max remote parameter size including termination char.
 */
static const size_t MAX_SIZE = MAX_LENGTH + 1;

template <typename TypeParam>
bool RemoteParameterProxy<TypeParam>::set(const TypeParam &data, std::string &error)
{
    RemoteParameterProxyImpl proxy(mName);
    if (data.length() > MAX_LENGTH) {

        error = "String Parameter exceeds max allowed length";
        return false;
    }
    return proxy.write(reinterpret_cast<const uint8_t *>(data.c_str()), data.length() + 1, error);
}


template <typename TypeParam>
bool RemoteParameterProxy<TypeParam>::get(TypeParam &data, std::string &error)
{
    RemoteParameterProxyImpl proxy(mName);
    char dataToRead[MAX_SIZE];
    size_t size = MAX_SIZE;
    if (!proxy.read(reinterpret_cast<uint8_t *>(dataToRead), size, error)) {

        // error is set and return empty string.
        return false;
    }
    data = TypeParam(dataToRead, strlen(dataToRead));
    return true;
}

template class RemoteParameterProxy<std::string>;
