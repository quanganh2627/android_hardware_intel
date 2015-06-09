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

#define LOG_TAG "RemoteParameterConnector"

#include "RemoteParameterConnector.hpp"
#include <AudioCommsAssert.hpp>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utils/Log.h>
#include <cutils/sockets.h>

const char *const RemoteParameterConnector::mBaseName = "parameter.";

RemoteParameterConnector::RemoteParameterConnector(int socketFd)
    : mSocketFd(socketFd)
{
}

RemoteParameterConnector::~RemoteParameterConnector()
{
    if (isConnected()) {

        close(mSocketFd);
        mSocketFd = -1;
    }
}

bool RemoteParameterConnector::isConnected() const
{
    return mSocketFd != -1;
}

std::string RemoteParameterConnector::getServerName(const std::string &parameterName)
{
    return mBaseName + parameterName;
}

int RemoteParameterConnector::acceptConnection()
{
    return accept(mSocketFd, NULL, NULL);
}

int RemoteParameterConnector::getFd() const
{
    return mSocketFd;
}

int RemoteParameterConnector::createClientSocket(const std::string &paramName, std::string &error)
{
    /**
     * It is necessary to initialize errno to ENAMETOOLONG to have a chance to log
     * a consistent error in all cases.
     * The implementation of socket_local_client is not a POSIX function and may return
     * an error (i.e. a Fd = -1) in case of a name too long WITHOUT setting the errno.
     * In all other case of error, it will be consequent to a call of a POSIX function,
     * so errno would have been overwritten with the right error code.
     * So, setting manually errno to ENAMETOOLONG will garantee to have a consistent errno
     * in all cases with the current implementation of socket_local_client.
     */
    errno = ENAMETOOLONG;
    int socketFd = socket_local_client(getServerName(paramName).c_str(),
                                       ANDROID_SOCKET_NAMESPACE_ABSTRACT,
                                       SOCK_STREAM);
    if (socketFd == -1) {
        error = "socket_local_client connection to " + paramName + " failed : " + strerror(errno);
    }
    return socketFd;
}

int RemoteParameterConnector::createServerSocket(const std::string &paramName, std::string &error)
{
    /**
     * It is necessary to initialize errno to ENAMETOOLONG to have a chance to log
     * a consistent error in all cases.
     * The implementation of socket_local_client is not a POSIX function and may return
     * an error (i.e. a Fd = -1) in case of a name too long WITHOUT setting the errno.
     * In all other case of error, it will be consequent to a call of a POSIX function,
     * so errno would have been overwritten with the right error code.
     * So, setting manually errno to ENAMETOOLONG will garantee to have a consistent errno
     * in all cases with the current implementation of socket_local_client.
     */
    errno = ENAMETOOLONG;
    int socketFd = socket_local_server(getServerName(paramName).c_str(),
                                       ANDROID_SOCKET_NAMESPACE_ABSTRACT,
                                       SOCK_STREAM);
    if (socketFd == -1) {
        error = "socket_local_server connection to " + paramName + " failed : " + strerror(errno);
    }
    return socketFd;
}

void RemoteParameterConnector::setTimeoutMs(uint32_t timeoutMs)
{
    struct timeval tv;
    tv.tv_sec = timeoutMs / mMsecPerSec;
    tv.tv_usec = (timeoutMs % mUsecPerMsec) * mUsecPerMsec;

    setsockopt(mSocketFd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    setsockopt(mSocketFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

bool RemoteParameterConnector::send(const void *data, uint32_t size)
{
    AUDIOCOMMS_ASSERT(data != NULL, "NULL data pointer");

    uint32_t offset = 0;
    const uint8_t *pData = static_cast<const uint8_t *>(data);

    while (size) {

        int32_t accessedSize = ::send(mSocketFd, &pData[offset], size, MSG_NOSIGNAL);

        if (!accessedSize || accessedSize == -1) {

            return false;
        }
        size -= accessedSize;
        offset += accessedSize;
    }
    return true;
}

bool RemoteParameterConnector::receive(void *data, uint32_t size)
{
    AUDIOCOMMS_ASSERT(data != NULL, "NULL data pointer");

    uint8_t *pData = static_cast<uint8_t *>(data);

    while (size) {

        int32_t accessedSize = ::recv(mSocketFd, pData, size, MSG_NOSIGNAL);

        if (!accessedSize || accessedSize == -1) {

            return false;
        }
        size -= accessedSize;
        pData += accessedSize;
    }
    return true;
}

uid_t RemoteParameterConnector::getUid() const
{
    struct ucred cr;
    socklen_t len = sizeof(cr);

    if (getsockopt(mSocketFd, SOL_SOCKET, SO_PEERCRED, &cr, &len)) {

        ALOGE("could not get socket credentials: %s\n", strerror(errno));
        return -1;
    }

    return cr.uid;
}
