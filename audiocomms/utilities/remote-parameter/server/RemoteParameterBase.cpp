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
#include "RemoteParameterImpl.hpp"
#include <string>

RemoteParameterBase::RemoteParameterBase(const std::string &parameterName,
                                         size_t size,
                                         const std::string &trustedPeerUserName)
    : mName(parameterName),
      mSize(size),
      mTrustedPeerUserName(trustedPeerUserName)
{
}

RemoteParameterBase::RemoteParameterBase(const std::string &parameterName,
                                         size_t size,
                                         uid_t trustedPeerUid)
    : mName(parameterName),
      mSize(size),
      mTrustedPeerUserName(RemoteParameterImpl::getUserName(trustedPeerUid))
{
}
