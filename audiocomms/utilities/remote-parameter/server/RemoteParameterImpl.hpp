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

#include <NonCopyable.hpp>
#include <string>
#include <stdint.h>
#include <sys/types.h>

class RemoteParameterConnector;
class RemoteParameterBase;

/**
 * Remote Parameter (Server Side) implementation based on android socket.
 * It makes use of a custom protocol to set / get parameters.
 *
 * First, the command identifier is received as a size.
 *          0 -> Get command
 *          non null -> Set command with the size of the parameter to set
 *
 * Then, for a Get command:
 *              Server Side Remote Parameter implementor sends the size of the parameter.
 *              Server Side Remote Parameter implementor sends the value of the parameter.
 *
 *       for a set command:
 *              Server Side Remote Parameter implementor receives the value of the parameter to set.
 *              Server Side Remote Parameter callback the set function of the interface.
 *              Server Side Remote Parameter sends the status to the client.
 */
class RemoteParameterImpl : private audio_comms::utilities::NonCopyable
{
public:
    /**
     * Create a RemoteParameterImpl instance.
     *
     * @param[in] parameter interface to the parameter.
     * @param[in] parameterName label to be used to access the parameter.
     * @param[in] size parameter size in bytes.
     * @param[out] error humain readable error, set if returned pointer is NULL
     *
     * @return a pointer to new instance if succeed, NULL otherwise and error is set.
     */
    static RemoteParameterImpl *create(RemoteParameterBase *parameter,
                                       const std::string &parameterName,
                                       size_t size,
                                       std::string &error);

    ~RemoteParameterImpl();

    /**
     * Client connection handler
     * When the Event Thread is woken up on an event of the remote parameter.
     */
    void handleNewConnection();

    /**
     * Return a file descriptor to poll.
     * Requested by EventListener based implementation of the server.
     *
     * @return file descriptor of the remote parameter (server side) to poll.
     */
    int getPollFd() const;

    /**
     * Get the name of the parameter.
     *
     * @return name of the parameter.
     */
    const std::string &getName() const { return mName; }

    /**
     * Returns the user name.
     *
     * @param[in] uid User Identifier.
     *
     * @return valid string if user name found, empty string otherwise.
     */
    static std::string getUserName(uid_t uid);

private:
    RemoteParameterImpl(RemoteParameterBase *parameter,
                        const std::string &parameterName,
                        size_t size,
                        RemoteParameterConnector *connector);

    /**
     * Check the client's credential and only accept socket connection from the allowed peer
     * user name defined for this remote parameter.
     *
     * @param[in] uid User Identifier of the peer connected to the socket.
     *
     * @return true if the peer name matches the allowed user name, false otherwise.
     */
    bool checkCredential(uid_t uid) const;


    std::string mName; /**< Parameter Name. */
    size_t mSize; /**< Parameter Size. */

    RemoteParameterBase *mParameter; /**< Interface for set/get operation. */

    RemoteParameterConnector *mServerConnector; /**< Remote Parameter Server Side connector. */

    static const uint32_t mCommunicationTimeoutMs = 5000; /**< Timeout. */
};
