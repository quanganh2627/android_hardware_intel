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

#define LOG_TAG "RemoteParameterServer"

#include "RemoteParameterServer.hpp"
#include "RemoteParameter.hpp"
#include "RemoteParameterImpl.hpp"
#include <AudioCommsAssert.hpp>
#include "EventThread.h"
#include <utils/Log.h>

using std::string;

RemoteParameterServer::RemoteParameterServer()
    : mEventThread(new CEventThread(this)),
      mFdClientId(0),
      mStarted(false)
{
}

RemoteParameterServer::~RemoteParameterServer()
{
    stop();

    // Destroy remote parameters
    RemoteParameterImplMapIterator it;

    for (it = mRemoteParameterImplMap.begin(); it != mRemoteParameterImplMap.end(); ++it) {

        delete it->second;
        it->second = NULL;
    }

    // Event Thread
    delete mEventThread;
    mEventThread = NULL;
}

bool RemoteParameterServer::addRemoteParameter(RemoteParameterBase *remoteParameter, string &error)
{
    AUDIOCOMMS_ASSERT(remoteParameter != NULL, "invalid remote parameter");
    if (mStarted) {

        error = "Parameter added in not permitted context";
        return false;
    }

    // Check if the parameter name has already been added (opening twice the file descriptor would
    // block any client to connect to the file descriptor of the socket
    RemoteParameterImplMapConstIterator it;
    for (it = mRemoteParameterImplMap.begin(); it != mRemoteParameterImplMap.end(); ++it) {

        RemoteParameterImpl *implementor = it->second;
        if (remoteParameter->getName() == implementor->getName()) {

            error = "Parameter Name already added";
            return false;
        }
    }

    // Create new Remote Parameter Implementor
    RemoteParameterImpl *implementor = RemoteParameterImpl::create(remoteParameter,
                                                                   remoteParameter->getName(),
                                                                   remoteParameter->getSize(),
                                                                   error);
    if (implementor == NULL) {

        error += " new RemoteParameterImpl failed.";
        return false;
    }

    // Check not already inserted
    if (mRemoteParameterImplMap.find(implementor->getPollFd()) != mRemoteParameterImplMap.end()) {

        error = "Poll Fd added twice";
        delete implementor;
        return false;
    }

    // Record parameter
    mRemoteParameterImplMap[implementor->getPollFd()] = implementor;

    // Listen to new server requests
    mEventThread->addOpenedFd(mFdClientId++, implementor->getPollFd(), true);

    return true;
}

bool RemoteParameterServer::start()
{
    if (!setStarted(true)) {

        return false;
    }
    bool status = mEventThread->start();
    if (!status) {

        setStarted(false);
    }
    return status;
}

void RemoteParameterServer::stop()
{
    if (!setStarted(false)) {

        return;
    }
    mEventThread->stop();
}

bool RemoteParameterServer::setStarted(bool isStarted)
{
    if (isStarted == mStarted) {

        return false;
    }
    mStarted = isStarted;
    return true;
}

bool RemoteParameterServer::onEvent(int fd)
{
    ALOGD("%s", __FUNCTION__);

    // Find appropriate server
    RemoteParameterImplMapConstIterator it = mRemoteParameterImplMap.find(fd);

    if (it == mRemoteParameterImplMap.end()) {

        ALOGE("%s: remote parameter not found!", __FUNCTION__);
    } else {

        // Process request
        RemoteParameterImpl *remoteParameterImpl = it->second;

        remoteParameterImpl->handleNewConnection();
    }

    return false;
}

bool RemoteParameterServer::onError(int fd)
{
    // Nothing to do
    return false;
}

bool RemoteParameterServer::onHangup(int fd)
{
    // Nothing to do
    return false;
}

void RemoteParameterServer::onAlarm()
{
    ALOGE("%s: server timeout!", __FUNCTION__);
}

void RemoteParameterServer::onPollError()
{
    ALOGE("%s: server poll error!", __FUNCTION__);
}

bool RemoteParameterServer::onProcess(void *context, uint32_t eventId)
{
    // Nothing to do
    return false;
}
