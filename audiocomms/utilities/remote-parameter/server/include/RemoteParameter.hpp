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

#include <AudioCommsAssert.hpp>
#include <string>
#include <stdint.h>
#include <sys/types.h>
#include <string.h>
#include <private/android_filesystem_config.h>
#include <pwd.h>
/**
 * Server side Remote Parameter object.
 * This class may be used if parameter handled as an array of char
 */
class RemoteParameterBase
{
public:
    explicit RemoteParameterBase(const std::string &parameterName,
                                 size_t size,
                                 const std::string &trustedPeerUserName);

    explicit RemoteParameterBase(const std::string &parameterName,
                                 size_t size,
                                 uid_t trustedPeerUid);

    virtual ~RemoteParameterBase() {}

    /**
     * Get the name of the parameter.
     *
     * @return name of the parameter.
     */
    const std::string getName() const { return mName; }

    /**
     * Get the user name of the peer allowed to control this parameter.
     *
     * @return user name of the peer allowed to control this parameter.
     */
    const std::string &getTrustedPeerUserName() const { return mTrustedPeerUserName; }

    /**
     * Get the size of the parameter.
     *
     * @return size of the parameter.
     */
    size_t getSize() const { return mSize; }

    /**
     * Set parameter.
     *
     * @param[in] data value to set (as a buffer).
     * @param[in] size in bytes of the buffer to set.
     *
     * @return true if set is successful, false otherwise.
     */
    virtual bool set(const uint8_t *data, size_t size) = 0;

    /**
     * Get parameter.
     *
     * @param[out] data value to be written to (as a buffer).
     * @param[in] size in bytes of the buffer to get.
     *
     * @return true if get is successful, false otherwise.
     */
    virtual void get(uint8_t *data, size_t &size) const = 0;

private:
    std::string mName; /**< Parameter Name. */
    size_t mSize; /**< Parameter Size. */
    std::string mTrustedPeerUserName; /**< Name of the peer allowed to control this parameter. */
};

/**
 * Server side Remote Parameter template.
 * This class may be used if parameter handled as an typed parameter.
 */
template <typename TypeParam>
class RemoteParameter : public RemoteParameterBase
{
public:
    RemoteParameter(const std::string &parameterName, const std::string &trustedPeerUserName = "");
    RemoteParameter(const std::string &parameterName, uid_t trustedPeerUid);

    /**
     * Set typed parameter.
     *
     * @tparam type of the parameter
     * @param[in] data value to set
     *
     * @return true if set is successful, false otherwise.
     */
    virtual bool set(const TypeParam &data) = 0;

    /**
     * Get typed parameter.
     *
     * @tparam type of the parameter
     * @param[out] data value to get
     *
     * @return true if set is successful, false otherwise.
     */
    virtual void get(TypeParam &data) const = 0;

private:
    virtual bool set(const uint8_t *data, size_t size);

    virtual void get(uint8_t *data, size_t &size) const;
};
