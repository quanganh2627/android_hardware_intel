/* EventThread.h
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
#pragma once

#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <stddef.h>
#include <list>

using namespace std;

class IEventListener;

class CEventThread
{
private:
    enum PipeMsg
    {
        EProcess,
        EExit,

        ENbPipeMsg
    };

    struct Message
    {
        void *context;
        uint32_t eventId;
        uint32_t msg;
    };

    struct SFd
    {
        SFd(uint32_t clientFdId, int fd,
            bool toListenTo) : mClientFdId(clientFdId), mFd(fd), mToListenTo(toListenTo) {}
        uint32_t mClientFdId;
        int mFd;
        bool mToListenTo;
    };

    typedef list<SFd>::iterator FdListIterator;
    typedef list<SFd>::const_iterator FdListConstIterator;

public:
    CEventThread(IEventListener *eventListener, bool bLogsOn = true);
    ~CEventThread();

    /**
     * Add a File Descriptor that the client has previously opened to the list of file descriptors
     * to be polled by the event thread.
     *
     * @param[in] fdClientId identifier given by the client associated to this fd to poll.
     * @param[in] fd file descriptor to add to the list.
     * @param[in] toListenTo indicates if the client expects to poll on this file descriptor.
     */
    void addOpenedFd(uint32_t fdClientId, int fd, bool toListenTo = false);

    /**
     * Get File descriptor associated to a client File Descriptor Id.
     *
     * @param[in] clientFdId client file descriptor Id.
     *
     * @return file descriptor associated to this id.
     */
    int getFd(uint32_t clientFdId) const;

    /**
     * Remove and close all file descriptors polled by the event thread.
     */
    void closeAndRemoveFds();

    /**
     * Remove and close file descriptor identified by its id.
     *
     * @param[in] clientFdId client file descriptor Id.
     */
    void closeAndRemoveFd(uint32_t clientFdId);

    /**
     * Start an alarm which will trig onAlarm() in 'last' ms from now.
     * (must be called from the EventThread thread context).
     *
     * @param[in] durationMs duration in milliseconds to wait before onAlarm callback is called.
     */
    void startAlarm(uint32_t durationMs);

    /**
     * Clear the alarm (must be called from the EventThread thread context).
     */
    void cancelAlarm();

    /**
     * Start the event thread service.
     *
     * @return true if successfully started, false otherwise.
     */
    bool start();

    /**
     * Stop the even thread service. This function is synchronous.
     */
    void stop();

    /**
     * Returns the state of the event thread. When started, i.e. the event thread is listening
     * to event from its list of file descriptor and from its pipe, no file descriptor can be
     * added outside the context of the event thread. The only way to change this list is to append
     * remove file descriptor upon call back of the event thread and returning true to notify
     * of the change.
     *
     * @return true if started false otherwise.
     */
    bool isStarted() const { return mIsStarted; }

    /**
     * Trig an execution context change. The caller of this function has to process an event within
     * theevent thread context in order to avoid borrowing the execution context of another thread
     * for example.
     *
     * @param[in] context pointer given by the client of the event thread.
     *                    Note that is may be NULL.
     * @param[in] eventId given by the client of the event thread.
     *                    NOTE: this parameter is DEPRECATED, for maintained for compatibility.
     * @deprecated Using this function with 2 parameters is deprecated, only context shall remain
     *             as a parameter of this function.
     */
    void trig(void *context, uint32_t eventId = -1);

    /**
     * Helper function to check the context. The list of file descriptor polled can be added only
     * within the context of the event thread when the event thread is started.
     *
     * @return true if execution of the caller is within the thread context.
     * @return false otherwise.
     */
    bool inThreadContext() const;

    /**
     * Logs activation.
     */
    void setLogsState(bool logsOn);
    bool isLogsOn() const { return mLogsOn; }

private:
    /**
     * Event Thread function.
     *
     * @param[in] data given back (used as context).
     */
    static void *thread_func(void *data);

    /**
     * Run loop of the event thread.
     */
    void run();

    /**
     * Add a file descriptor to the list of polled file descriptors.
     *
     * @param[in] fd to add.
     */
    void addListenedFd(int fd);

    /**
     * Remove a file descriptor from the list of polled file descriptors.
     *
     * @param[in] fd to remove.
     */
    void removeListenedFd(int fd);

    /**
     * Rebuild the list of file descriptors to be polled.
     *
     * @param[in]
     */
    void buildPollFds(struct pollfd *pollFds) const;

    /**
     * Helper function for alarm management.
     *
     * @return current date in milliseconds
     */
    static int64_t getCurrentDateMs();

private:
    IEventListener *mEventListener; /**< Listener handler to report event. */
    bool mIsStarted; /**< State of the event thread. */
    pthread_t mThreadId; /**< thread id, used for context check. */
    int mInbandPipe[2]; /**< inband pipe used to trig internal events. */
    list<SFd> mFdList; /**< List of File descriptors polled by the event thread. */
    uint32_t mNbPollFds; /**< Number of file descriptor polled. */
    int64_t mAlarmMs; /**< Alarm date in milliseconds. */
    bool mLogsOn; /**< Event Thread enabled log flag. */
};
