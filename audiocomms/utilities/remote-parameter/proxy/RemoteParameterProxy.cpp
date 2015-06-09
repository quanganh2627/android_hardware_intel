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

template <typename TypeParam>
bool RemoteParameterProxy<TypeParam>::set(const TypeParam &data, std::string &error)
{
    RemoteParameterProxyImpl proxy(mName);
    return proxy.write(reinterpret_cast<const uint8_t *>(&data), sizeof(TypeParam), error);
}
template <typename TypeParam>
bool RemoteParameterProxy<TypeParam>::get(TypeParam &data, std::string &error)
{
    RemoteParameterProxyImpl proxy(mName);
    size_t size = sizeof(TypeParam);
    return proxy.read(reinterpret_cast<uint8_t *>(&data), size, error);
}

template class RemoteParameterProxy<uint32_t>;
template class RemoteParameterProxy<bool>;
