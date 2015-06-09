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

#pragma once

#include <string>
#include <stdint.h>
#include <sys/types.h>
#include <utils/Log.h>

/**
 * Client side Remote Parameter template.
 * This class may be used if parameter handled as an typed parameter.
 */
template <typename TypeParam>
class RemoteParameterProxy
{
public:
    RemoteParameterProxy(const std::string &parameterName)
        : mName(parameterName)
    {}

    /**
     * Set Parameter accessor
     *
     * @tparam TypeParam type of the parameter
     * @param[in] data parameter value to set
     * @param[out] error human readable error, set if return code is false.
     *
     * @return true if success, false otherwise and error code is set.
     */
    bool set(const TypeParam &data, std::string &error);

    /**
     * Get Parameter accessor
     *
     * @tparam TypeParam type of the parameter
     * @param[out] data parameter value to get
     * @param[out] error human readable error, set if return code is false.
     *
     * @return true if success, false otherwise and error code is set.
     */
    bool get(TypeParam &data, std::string &error);

private:
    std::string mName; /**< Parameter Name. */
};
