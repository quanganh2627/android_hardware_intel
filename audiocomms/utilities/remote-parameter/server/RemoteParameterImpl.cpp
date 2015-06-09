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

#define LOG_TAG "RemoteParameter"

#include "RemoteParameterImpl.hpp"
#include "RemoteParameter.hpp"
#include <RemoteParameterConnector.hpp>
#include <AudioCommsAssert.hpp>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utils/Log.h>
#include <cutils/sockets.h>
#include <pwd.h>

using std::string;

RemoteParameterImpl *RemoteParameterImpl::create(RemoteParameterBase *parameter,
                                                 const string &parameterName,
                                                 size_t size,
                                                 string &error)
{
    int socketFd = RemoteParameterConnector::createServerSocket(parameterName, error);
    if (socketFd == -1) {
        return NULL;
    }

    RemoteParameterConnector *connector = new RemoteParameterConnector(socketFd);
    if (connector == NULL) {
        error = "Could not create Remote Parameter %s " + parameterName + ": connector error.";
        close(socketFd);
        return NULL;
    }
    return new RemoteParameterImpl(parameter, parameterName, size, connector);
}

/**
 * Remote Parameter (Server Side) implementation based on socket
 *
 */
RemoteParameterImpl::RemoteParameterImpl(RemoteParameterBase *parameter,
                                         const string &parameterName,
                                         size_t size,
                                         RemoteParameterConnector *connector)
    : mName(parameterName),
      mSize(size),
      mParameter(parameter),
      mServerConnector(connector)
{
}

int RemoteParameterImpl::getPollFd() const
{
    AUDIOCOMMS_ASSERT(mServerConnector != NULL, "server connector invalid");

    return mServerConnector->getFd();
}

std::string RemoteParameterImpl::getUserName(uid_t uid)
{
    string name;
    struct passwd pwd, *result;
    ssize_t bufSize = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (bufSize == -1) {

        // Value was undeterminate, 1024 should be enough.
        bufSize = 1024;
    }
    char buf[bufSize];

    getpwuid_r(uid, &pwd, buf, sizeof(buf), &result);
    if (result != NULL) {

        name = pwd.pw_name;
    }
    return name;
}

bool RemoteParameterImpl::checkCredential(uid_t uid) const
{
    if (mParameter->getTrustedPeerUserName().empty()) {

        /**
         * No allowed peer user name were defined.
         * Check at least the caller is launched by the same user.
         */
        return getuid() == uid;

    }
    return getUserName(uid) == mParameter->getTrustedPeerUserName();
}

void RemoteParameterImpl::handleNewConnection()
{
    int clientSocketFd = mServerConnector->acceptConnection();
    if (clientSocketFd < 0) {

        ALOGE("%s: accept: %s", __FUNCTION__, strerror(errno));
        return;
    }

    RemoteParameterConnector clientConnector(clientSocketFd);

    if (!checkCredential(clientConnector.getUid())) {

        ALOGE("%s: security error: Requester (%s) is not the allowed peer (%s)", __FUNCTION__,
              getUserName(clientConnector.getUid()).c_str(),
              mParameter->getTrustedPeerUserName().c_str());
        return;
    }

    // Set timeout
    clientConnector.setTimeoutMs(mCommunicationTimeoutMs);

    /// Read command first
    // By protocol convention, get is 0, set is non null (size of the parameter to set in fact)
    uint32_t size;
    if (!clientConnector.receive((void *)&size, sizeof(size))) {

        ALOGE("%s: recv size: %s", __FUNCTION__, strerror(errno));
        return;
    }

    if (size == RemoteParameterConnector::mSizeCommandGet) {

        /// Get parameter
        size = mSize;

        // Read data
        uint8_t data[size];

        mParameter->get(data, size);

        // Send Size
        if (!clientConnector.send((const void *)&size, sizeof(size))) {

            ALOGE("%s: send status: %s", __FUNCTION__, strerror(errno));
            return;
        }

        // Send data
        if (!clientConnector.send((const void *)data, size)) {

            ALOGE("%s: send data: %s", __FUNCTION__, strerror(errno));
        }
    } else {

        // Read data
        uint8_t data[size];

        if (!clientConnector.receive((void *)data, size)) {

            ALOGE("%s: recv data: %s", __FUNCTION__, strerror(errno));
            return;
        }

        // Set data
        uint32_t status = RemoteParameterConnector::mTransactionSucessfull;
        if (!mParameter->set(data, size)) {

            status = RemoteParameterConnector::mTransactionFailed;
        }

        // Send status back
        if (!clientConnector.send((const void *)&status, sizeof(status))) {

            ALOGE("%s: send status: %s", __FUNCTION__, strerror(errno));
        }
    }

}

RemoteParameterImpl::~RemoteParameterImpl()
{
    delete mServerConnector;
}
