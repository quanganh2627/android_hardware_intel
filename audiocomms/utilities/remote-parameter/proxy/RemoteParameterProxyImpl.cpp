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

#define LOG_TAG "RemoteParameterProxyImpl"

#include "RemoteParameterProxyImpl.hpp"

#include <RemoteParameterConnector.hpp>
#include <AudioCommsAssert.hpp>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utils/Log.h>
#include <cutils/sockets.h>

using std::string;

const char *const RemoteParameterProxyImpl::mConnectionError = "Proxy: Not connected";

const char *const RemoteParameterProxyImpl::mSendSizeProtocolError =
    "Proxy: Send Size:Protocol error";

const char *const RemoteParameterProxyImpl::mSendDataProtocolError =
    "Proxy: Send Data:Protocol error";

const char *const RemoteParameterProxyImpl::mReceiveProtocolError = "Proxy: receive protocol error";

const char *const RemoteParameterProxyImpl::mTransactionRefusedError = "Proxy: Transaction refused";

const char *const RemoteParameterProxyImpl::mGetCommandError =
    "Proxy: protocol error: get should start with empty size";

const char *const RemoteParameterProxyImpl::mReadSizeProtocolError =
    "Proxy: protocol error while trying to read the size";

const char *const RemoteParameterProxyImpl::mReadDataProtocolError =
    "Proxy: protocol error: answered size does not meet the expected data size";

const char *const RemoteParameterProxyImpl::mReadProtocolError =
    "Proxy: protocol error in receiving the data";

/**
 * Remote Parameter (Proxy Side) implementation based on socket
 *
 */
RemoteParameterProxyImpl::RemoteParameterProxyImpl(const string &parameterName)
    : mName(parameterName)
{
}

RemoteParameterProxyImpl::~RemoteParameterProxyImpl()
{
}

bool RemoteParameterProxyImpl::write(const uint8_t *data, size_t size, string &error)
{
    AUDIOCOMMS_ASSERT(data != NULL, "NULL data pointer");

    int socketFd = RemoteParameterConnector::createClientSocket(mName, error);
    if (socketFd == -1) {

        return false;
    }

    RemoteParameterConnector connector(socketFd);
    if (!connector.isConnected()) {
        error = mConnectionError;
        return false;
    }

    // Set timeout
    connector.setTimeoutMs(mCommunicationTimeoutMs);

    /// For a set command first
    // By protocol convention, set command is identified with non-zero size sent first.
    // The size sent is the size of the parameter to set.
    if (!connector.send(static_cast<const void *>(&size), sizeof(size))) {

        error = mSendSizeProtocolError;
        return false;
    }

    /// Send data
    if (!connector.send((const void *)data, size)) {

        error = mSendDataProtocolError;
        return false;
    }

    /// Read answer (status)
    uint32_t status;

    // Status
    if (!connector.receive(static_cast<void *>(&status), sizeof(status))) {

        error = mReceiveProtocolError;
        return false;
    }

    if (status != RemoteParameterConnector::mTransactionSucessfull) {

        // Got an error
        error = mTransactionRefusedError;
        return false;
    }

    return true;
}

bool RemoteParameterProxyImpl::read(uint8_t *data, size_t &size, string &error)
{
    AUDIOCOMMS_ASSERT(data != NULL, "NULL data pointer");

    int socketFd = RemoteParameterConnector::createClientSocket(mName, error);
    if (socketFd == -1) {

        return false;
    }

    RemoteParameterConnector connector(socketFd);

    // Set timeout
    connector.setTimeoutMs(mCommunicationTimeoutMs);

    // Check connection
    if (!connector.isConnected()) {

        error = mConnectionError;
        return false;
    }

    /// Send Get Command
    // By protocol convention, get command is identified by a NULL size
    uint32_t cmdSize = RemoteParameterConnector::mSizeCommandGet;
    if (!connector.send(static_cast<const void *>(&cmdSize), sizeof(cmdSize))) {

        error = mGetCommandError;
        return false;
    }

    /// Read answer (status + data)
    size_t answerSize;

    // Size
    if (!connector.receive(static_cast<void *>(&answerSize), sizeof(answerSize))) {

        error = mReadSizeProtocolError;
        return false;
    }

    if (answerSize > size) {

        error = mReadDataProtocolError;
        return false;
    }
    size = answerSize;

    // Read data
    if (!connector.receive(static_cast<void *>(data), answerSize)) {

        error = mReadProtocolError;
        return false;
    }

    return true;
}
