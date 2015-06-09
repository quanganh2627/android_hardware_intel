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

class RemoteParameterProxyImpl
{
public:
    /**
     * Remote Parameter (Proxy Side) implementation based on android socket.
     * It makes use of a custom protocol to set / get parameters.
     *
     * First, the command identifier is sent as a size.
     *          0 -> Get command
     *          non null -> Set command with the size of the parameter to set
     *
     * Then, for a Get command:
     *              Proxy Side Remote Parameter implementor receives the size of the parameter.
     *              Proxy Side Remote Parameter implementor receives the value of the parameter.
     *
     *       for a set command:
     *              Proxy Side Remote Parameter implementor sends the value of the parameter to set.
     *              Proxy Side Remote Parameter receives the status from the server.
     */
    RemoteParameterProxyImpl(const std::string &parameterName);
    virtual ~RemoteParameterProxyImpl();

    /**
     * Write to socket accessor
     *
     * @param[in] data parameter value to write
     * @param[in] size of the parameter to write
     * @param[out] error human readable error, set if return code is false.
     *
     * @return true if success, false otherwise and error code is set.
     */
    bool write(const uint8_t *data, size_t size, std::string &error);

    /**
     * Read from socket accessor
     *
     * @param[out] data to read
     * @param[in|out] size of the data to read, updated with the real size retrieved from server.
     * @param[out] error human readable error, set if return code is false.
     *
     * @return true if success, false otherwise and error code is set.
     */
    bool read(uint8_t *data, size_t &size, std::string &error);

private:
    std::string mName; /**< Parameter Name. */

    static const uint32_t mCommunicationTimeoutMs = 5000; /**< Timeout. */

    static const char *const mConnectionError; /**< human readable connection error. */

    static const char *const mSendSizeProtocolError; /**< human readable send size error. */

    static const char *const mSendDataProtocolError; /**< human readable send data error. */

    static const char *const mReceiveProtocolError; /**< human readable receive error. */

    static const char *const mTransactionRefusedError; /**< human readable transaction error. */

    static const char *const mGetCommandError; /**< human readable connection error. */

    static const char *const mReadSizeProtocolError; /**< human readable send size error. */

    static const char *const mReadDataProtocolError; /**< human readable send data error. */

    static const char *const mReadProtocolError; /**< human readable receive error. */
};
