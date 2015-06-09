/* EventThread.cpp
**
** Copyright 2011-2014 Intel Corporation
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

#define LOG_TAG "EVENT_THREAD"
#include <utils/Log.h>
#include "EventListener.h"

#include "EventThread.h"
#include <AudioCommsAssert.hpp>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <time.h>

const int64_t MILLISECONDS_IN_SECONDS = 1000;
const int64_t NANOSECONDS_IN_MILLISECONDS = 1000 * 1000;

#define SECONDS_TO_MILLISECONDS(seconds)            (int32_t(seconds) * MILLISECONDS_IN_SECONDS)
#define NANOSECONDS_TO_MILLISECONDS(nanoseconds)    ((nanoseconds) / NANOSECONDS_IN_MILLISECONDS)

CEventThread::CEventThread(IEventListener *eventListener, bool logsOn)
    : mEventListener(eventListener),
      mIsStarted(false),
      mThreadId(0),
      mNbPollFds(0),
      mAlarmMs(-1),
      mLogsOn(logsOn)
{
    AUDIOCOMMS_ASSERT(eventListener, "Invalid event listener");

    // Create inband pipe
    pipe(mInbandPipe);

    // Add to poll fds
    addOpenedFd(-1, mInbandPipe[0], true);
}

CEventThread::~CEventThread()
{
    stop();

    close(mInbandPipe[0]);
    close(mInbandPipe[1]);
}

void CEventThread::addOpenedFd(uint32_t fdClientId, int fd, bool toListenTo)
{
    AUDIOCOMMS_ASSERT(!mIsStarted || inThreadContext(), "Operation invalid within this context");

    mFdList.push_back(SFd(fdClientId, fd, toListenTo));

    if (toListenTo) {
        // Keep track of number of polled Fd
        mNbPollFds++;
    }
}

void CEventThread::closeAndRemoveFd(uint32_t ClientFdId)
{
    AUDIOCOMMS_ASSERT(!mIsStarted || inThreadContext(), "Operation invalid within this context");

    FdListIterator it;
    for (it = mFdList.begin(); it != mFdList.end(); ++it) {
        const SFd *fd = &(*it);

        if (fd->mClientFdId == ClientFdId) {
            close(fd->mFd);

            if (fd->mToListenTo) {
                // Keep track of number of polled Fd
                mNbPollFds--;
            }
            mFdList.erase(it);
            return;
        }
    }
}

int CEventThread::getFd(uint32_t clientFdId) const
{
    FdListConstIterator it;

    for (it = mFdList.begin(); it != mFdList.end(); ++it) {
        const SFd *pFd = &(*it);

        if (pFd->mClientFdId == clientFdId) {
            return pFd->mFd;
        }
    }
    ALOGD_IF(mLogsOn, "%s: Could not find File descriptor from List", __func__);
    return -1;
}

void CEventThread::startAlarm(uint32_t durationMs)
{
    ALOGD("%s %dms", __func__, durationMs);
    // Add the alarm duration to the current date to compute the alarm date
    mAlarmMs = getCurrentDateMs() + durationMs;
}

void CEventThread::cancelAlarm()
{
    ALOGD("%s", __func__);
    mAlarmMs = -1;
}

bool CEventThread::start()
{
    AUDIOCOMMS_ASSERT(!mIsStarted, "Event thread already started");

    pthread_create(&mThreadId, NULL, thread_func, this);
    mIsStarted = true;
    return true;
}

void CEventThread::stop()
{
    if (!mIsStarted) {
        return;
    }

    // Cause exiting of the thread
    Message toWrite;
    toWrite.eventId = 0;
    toWrite.context = NULL;
    toWrite.msg = EExit;

    ssize_t ret;
    do {
        ret = ::write(mInbandPipe[1], &toWrite, sizeof(toWrite));
    } while (ret == -1 && errno == EINTR);
    AUDIOCOMMS_ASSERT(ret == sizeof(toWrite),
                      "Unable to write message in pipe: ret: "
                      << ret << " status: " << strerror(errno));

    pthread_join(mThreadId, NULL);
    mIsStarted = false;
}

void CEventThread::trig(void *context, uint32_t eventId /* = -1 */)
{
    ALOGD_IF(mLogsOn, "%s: in", __func__);

    AUDIOCOMMS_ASSERT(mIsStarted, "Event thread not started");

    Message toWrite;
    toWrite.eventId = eventId;
    toWrite.context = context;
    toWrite.msg = EProcess;

    ssize_t ret;
    do {
        ret = ::write(mInbandPipe[1], &toWrite, sizeof(toWrite));
    } while (ret == -1 && errno == EINTR);
    AUDIOCOMMS_ASSERT(ret == sizeof(toWrite),
                      "Unable to write message in pipe: ret: "
                      << ret << " status: " << strerror(errno));

    ALOGD_IF(mLogsOn, "%s: out", __func__);
}

bool CEventThread::inThreadContext() const
{
    return pthread_self() == mThreadId;
}

void *CEventThread::thread_func(void *data)
{
    reinterpret_cast<CEventThread *>(data)->run();
    return NULL;
}

void CEventThread::run()
{
    while (true) {
        // Rebuild polled FDs
        struct pollfd pollFds[mNbPollFds];

        buildPollFds(pollFds);

        /// Poll
        int timeoutMs = -1;
        // Compute the poll timeout regarding the alarm
        if (mAlarmMs >= 0) {
            // Get current time in milliseconds
            int64_t now = getCurrentDateMs();

            // Future ?
            if (mAlarmMs > now) {
                timeoutMs = (int)(mAlarmMs - now);
            } else {
                timeoutMs = 0;
            }

        }
        // Do poll
        ALOGD("%s Do poll with timeout: %d", __func__, timeoutMs);
        int pollResult = poll(pollFds, mNbPollFds, timeoutMs);

        if (!pollResult) {
            // Timeout case
            mEventListener->onAlarm();
            continue;
        }
        if (pollResult < 0) {
            // I/O error?
            mEventListener->onPollError();
            continue;
        }
        if (pollFds[0].revents & POLLIN) {
            // Consume request
            Message dataRead;
            ssize_t ret;
            do {
                ret = ::read(mInbandPipe[0], &dataRead, sizeof(dataRead));
            } while (ret == -1 && errno == EINTR);
            AUDIOCOMMS_ASSERT(ret == sizeof(dataRead),
                              "Unable to write message in pipe: ret: "
                              << ret << " status: " << strerror(errno));
            AUDIOCOMMS_ASSERT(dataRead.msg < ENbPipeMsg, "Invalid message in pipe");

            if (dataRead.msg == EProcess) {
                if (mEventListener->onProcess(dataRead.context, dataRead.eventId)) {
                    continue;
                }
            } else {
                ALOGD_IF(mLogsOn, "%s exit", __func__);
                return;
            }
        }
        {
            uint32_t index;
            for (index = 1; index < mNbPollFds; index++) {
                // Check for errors first and reports to the listener
                if (pollFds[index].revents & POLLERR) {
                    ALOGD_IF(mLogsOn, "%s POLLERR event on Fd (%d)", __func__, index);

                    if (mEventListener->onError(pollFds[index].fd)) {
                        // FD list has changed, bail out
                        break;
                    }
                }
                // Check for hang ups and reports to the listener
                if (pollFds[index].revents & POLLHUP) {
                    ALOGD_IF(mLogsOn, "%s POLLHUP event on Fd (%d)", __func__, index);

                    if (mEventListener->onHangup(pollFds[index].fd)) {
                        // FD list has changed, bail out
                        break;
                    }
                }
                // Check for read events and reports to the listener
                if (pollFds[index].revents & POLLIN) {
                    ALOGD_IF(mLogsOn, "%s POLLIN event on Fd (%d)", __func__, index);

                    if (mEventListener->onEvent(pollFds[index].fd)) {
                        // FD list has changed, bail out
                        break;
                    }
                }
            }
        }
    }
}

void CEventThread::buildPollFds(struct pollfd *pollFds) const
{
    // Reset memory
    bzero(pollFds, sizeof(struct pollfd) * mNbPollFds);

    // Fill
    uint32_t fdIndex = 0;
    FdListConstIterator it;

    for (it = mFdList.begin(); it != mFdList.end(); ++it) {
        const SFd *fd = &(*it);

        if (fd->mToListenTo) {
            pollFds[fdIndex].fd = fd->mFd;
            pollFds[fdIndex++].events = POLLIN;
        }
    }
    AUDIOCOMMS_ASSERT(fdIndex == mNbPollFds, "Inconsistent list of file descriptor to poll");
}

int64_t CEventThread::getCurrentDateMs()
{
    timespec now;

    // Get current time
    clock_gettime(CLOCK_MONOTONIC, &now);

    return SECONDS_TO_MILLISECONDS(now.tv_sec) + NANOSECONDS_TO_MILLISECONDS(now.tv_nsec);
}

void CEventThread::setLogsState(bool logsOn)
{
    mLogsOn = logsOn;
}
