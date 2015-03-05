/* EventThread.cpp
 **
 ** Copyright 2011 Intel Corporation
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **     http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */
#include <unistd.h>
#include <assert.h>
#include <strings.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#define LOG_TAG "EVENT_THREAD"
#include <utils/Log.h>
#include "EventListener.h"

#include "EventThread.h"

const int64_t MILLISECONDS_IN_SECONDS      = 1000;
const int64_t NANOSECONDS_IN_MILLISECONDS  = 1000 * 1000;

#define SECONDS_TO_MILLISECONDS(seconds)            (int32_t(seconds) * MILLISECONDS_IN_SECONDS)
#define NANOSECONDS_TO_MILLISECONDS(nanoseconds)    ((nanoseconds) / NANOSECONDS_IN_MILLISECONDS)

CEventThread::CEventThread(IEventListener* pEventListener, bool bLogsOn) :
    _pEventListener(pEventListener),
    _bIsStarted(false),
    _ulThreadId(0),
    _uiNbPollFds(0),
    _iAlarmMs(-1),
    _bThreadContext(false),
    _bLogsOn(bLogsOn)
{
    assert(pEventListener);

    // Create inband pipe
    pipe(_aiInbandPipe);

    // Add to poll fds
    addOpenedFd(-1, _aiInbandPipe[0], true);
}

CEventThread::~CEventThread()
{
    // Make sure we're stopped
    stop();

    // Close inband pipe
    close(_aiInbandPipe[0]);
    close(_aiInbandPipe[1]);
}

// Add open FDs
void CEventThread::addOpenedFd(uint32_t uiFdClientId, int iFd, bool bToListenTo)
{
    assert(!_bIsStarted || inThreadContext());

    _sFdList.push_back(SFd(uiFdClientId, iFd, bToListenTo));

    if (bToListenTo) {

        // Keep track of number of polled Fd
        _uiNbPollFds++;
    }
}

// Remove and close FD
void CEventThread::closeAndRemoveFd(uint32_t uiClientFdId)
{
    assert(!_bIsStarted || inThreadContext());

    SFdListIterator it;

    for (it = _sFdList.begin(); it != _sFdList.end(); ++it) {

        const SFd* pFd = &(*it);

        if (pFd->_uiClientFdId == uiClientFdId) {

            // Close
            close(pFd->_iFd);

            if (pFd->_bToListenTo) {

                // Keep track of number of polled Fd
                _uiNbPollFds--;
            }

            // Remove element
            _sFdList.erase(it);

            // Done
            return;
        }
    }
}

// Get FD
int CEventThread::getFd(uint32_t uiClientFdId) const
{
    SFdListConstIterator it;

    for (it = _sFdList.begin(); it != _sFdList.end(); ++it) {

        const SFd* pFd = &(*it);

        if (pFd->_uiClientFdId == uiClientFdId) {

            return pFd->_iFd;
        }
    }

    if (_bLogsOn) {

        LOGD("%s: Could not find File descriptor from List", __func__);
    }

    return -1;
}

// Start an alarm which will trig onAlarm() in 'last' ms from now (must be called from the EventThread thread context)
void CEventThread::startAlarm(uint32_t uiDurationMs)
{

    LOGD("%s %dms", __func__, uiDurationMs);

    // Add the alarm duration to the current date to compute the alarm date
    _iAlarmMs = getCurrentDateMs() + uiDurationMs;

}

// Clear the alarm (must be called from the EventThread thread context)
void CEventThread::cancelAlarm()
{
    LOGD("%s", __func__);

    _iAlarmMs = -1;
}

// Start
bool CEventThread::start()
{
    assert(!_bIsStarted);

    // Create thread
    pthread_create(&_ulThreadId, NULL, thread_func, this);

    // State
    _bIsStarted = true;

    return true;
}

// Stop
void CEventThread::stop()
{
    // Check state
    if (!_bIsStarted) {

        return;
    }

    // Cause exiting of the thread
    uint8_t ucData = EExit;
    ::write(_aiInbandPipe[1], &ucData, sizeof(ucData));

    // Join thread
    pthread_join(_ulThreadId, NULL);

    // State
    _bIsStarted = false;
}

// Trigger
void CEventThread::trig(uint16_t uiEventId)
{
    if (_bLogsOn) {

        LOGD("%s: in", __func__);
    }

    assert(_bIsStarted);

    uint32_t ulData = (((uint32_t)uiEventId) << 16) | EProcess;
    ::write(_aiInbandPipe[1], &ulData, sizeof(ulData));

    if (_bLogsOn) {

        LOGD("%s: out", __func__);
    }
}

// Context check
bool CEventThread::inThreadContext() const
{
    return pthread_self() == _ulThreadId;
}

// Thread
void* CEventThread::thread_func(void* pData)
{
    reinterpret_cast<CEventThread*>(pData)->run();

    return NULL;
}

void CEventThread::run()
{
    while (true) {

        // Rebuild polled FDs
        struct pollfd astPollFds[_uiNbPollFds];

        buildPollFds(astPollFds);

        /// Poll
        int iTimeoutMs = -1;
        // Compute the poll timeout regarding the alarm
        if (_iAlarmMs >= 0) {

            // Get current time in milliseconds
            int64_t now = getCurrentDateMs();

            // Future ?
            if (_iAlarmMs > now) {

                iTimeoutMs = (int) (_iAlarmMs - now);
            } else {

                iTimeoutMs = 0;
            }

        }
        // Do poll
        LOGD("%s Do poll with timeout: %d", __func__, iTimeoutMs);
        int iPollRes = poll(astPollFds, _uiNbPollFds, iTimeoutMs);

        if (!iPollRes) {

            // Timeout case
            _pEventListener->onAlarm();

            continue;
        }
        if (iPollRes < 0) {

            // I/O error?
            _pEventListener->onPollError();

            continue;
        }

        // Exit request?
        if (astPollFds[0].revents & POLLIN) {

            // Consume request
            uint32_t ucData;
            ::read(_aiInbandPipe[0], &ucData, sizeof(ucData));

            if ((uint16_t)ucData == EProcess) {

                if (_pEventListener->onProcess((uint16_t)(ucData >> 16))) {

                    continue;
                }
            } else {
                assert(ucData == EExit);

                if (_bLogsOn) {

                    LOGD("%s exit", __func__);
                }
                // Exit
                return ;
            }
        }

        {
            //bool bContinue = false;
            uint32_t uiIndex;

            // Check for read events
            for (uiIndex = 1; uiIndex < _uiNbPollFds; uiIndex++) {

                // Check for errors first
                if (astPollFds[uiIndex].revents & POLLERR) {
                    if (_bLogsOn) {

                        LOGD("%s POLLERR event on Fd (%d)", __func__, uiIndex);
                    }

                    // Process
                    if (_pEventListener->onError(astPollFds[uiIndex].fd)) {

                        // FD list has changed, bail out
                        break;
                    }
                }
                // Check for hang ups
                if (astPollFds[uiIndex].revents & POLLHUP) {
                    if (_bLogsOn) {

                        LOGD("%s POLLHUP event on Fd (%d)", __func__, uiIndex);
                    }

                    if (_pEventListener->onHangup(astPollFds[uiIndex].fd)) {

                        // FD list has changed, bail out
                        break;
                    }
                }
                // Check for read events
                if (astPollFds[uiIndex].revents & POLLIN) {
                    if (_bLogsOn) {

                        LOGD("%s POLLIN event on Fd (%d)", __func__, uiIndex);
                    }
                    // Process
                    if (_pEventListener->onEvent(astPollFds[uiIndex].fd)) {

                        // FD list has changed, bail out
                        break;
                    }
                }
            }
        }
    }
}

// Poll FD computation
void CEventThread::buildPollFds(struct pollfd* paPollFds) const
{
    // Reset memory
    bzero(paPollFds, sizeof(struct pollfd) * _uiNbPollFds);

    // Fill
    uint32_t uiFdIndex = 0;
    SFdListConstIterator it;

    for (it = _sFdList.begin(); it != _sFdList.end(); ++it) {

        const SFd* pFd = &(*it);

        if (pFd->_bToListenTo) {

            paPollFds[uiFdIndex].fd = pFd->_iFd;
            paPollFds[uiFdIndex++].events = POLLIN;
        }
    }
    // Consistency
    assert(uiFdIndex == _uiNbPollFds);
}

// Get current date in milliseconds
int64_t CEventThread::getCurrentDateMs()
{
    timespec now;

    // Get current time
    clock_gettime(CLOCK_MONOTONIC, &now);

    return SECONDS_TO_MILLISECONDS(now.tv_sec) + NANOSECONDS_TO_MILLISECONDS(now.tv_nsec);
}

// Logs Activation
void CEventThread::setLogsState(bool bLogsOn)
{
    _bLogsOn = bLogsOn;
}

bool CEventThread::isLogsOn() const
{
    return _bLogsOn;
}
