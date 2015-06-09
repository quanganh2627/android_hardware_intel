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

#include "EventListener.h"
#include <NonCopyable.hpp>
#include <map>
#include <string>
#include <stdint.h>
#include <sys/types.h>

class CEventThread;
class RemoteParameterBase;
class RemoteParameterImpl;

class RemoteParameterServer : public IEventListener, private audio_comms::utilities::NonCopyable
{
public:
    explicit RemoteParameterServer();
    ~RemoteParameterServer();

    /**
     * Start the server.
     *
     * @return true if server started successfully, false otherwise.
     */
    bool start();

    /**
     * Stop the server.
     */
    void stop();

    /**
     * Parameter populate.
     * It is allowed to add parameter only when the server is not started.
     *
     * @param[in] remoteParameter parameter to add.
     * @param[out] error human readable error.
     *
     * @return true if parameter added, false otherwise and error is set appropriately.
     */
    bool addRemoteParameter(RemoteParameterBase *remoteParameter, std::string &error);

private:
    /**
     * Start/stop the server.
     *
     * @param[in] isStarted true if need to start the server, false otherwise.
     *
     * @return true if server started / stopped successfully, false if the server is already in the
     *              requested state.
     */
    bool setStarted(bool isStarted);

    /**
     * Event processing - From IEventListener
     */
    virtual bool onEvent(int fd);
    virtual bool onError(int fd);
    virtual bool onHangup(int fd);
    virtual void onAlarm();
    virtual void onPollError();
    virtual bool onProcess(void *context, uint32_t eventId);

    CEventThread *mEventThread; /**< Event thread. */

    uint32_t mFdClientId; /**< FDs identifiers for CEventThread. */

    bool mStarted; /**< started attribute of the server. */

protected:
    typedef std::map<int,
                     RemoteParameterImpl *>::const_iterator RemoteParameterImplMapConstIterator;

    typedef std::map<int, RemoteParameterImpl *>::iterator RemoteParameterImplMapIterator;

    std::map<int, RemoteParameterImpl *> mRemoteParameterImplMap; /**< Parameter Collection. */
};
