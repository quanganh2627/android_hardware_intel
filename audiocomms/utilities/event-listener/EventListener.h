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

#include <stdint.h>

class IEventListener
{
public:
    /**
     * Callback upon read event on a given file descriptor.
     *
     * @param[in] fd on which the event was detected.
     *
     * @return true if the list of file descripter polled has changed, false otherwise.
     */
    virtual bool onEvent(int fd) = 0;

    /**
     * Callback upon an error event on a given file descriptor.
     *
     * @param[in] fd on which the event was detected.
     *
     * @return true if the list of file descripter polled has changed, false otherwise.
     */
    virtual bool onError(int fd) = 0;

    /**
     * Callback upon hang up event on a given file descriptor.
     *
     * @param[in] fd on which the event was detected.
     *
     * @return true if the list of file descripter polled has changed, false otherwise.
     */
    virtual bool onHangup(int fd) = 0;

    /**
     * Callback upon alarm. It supposed that the client has previously set an alarm on the event
     * thread.
     */
    virtual void onAlarm() = 0;

    /**
     * Callback upon read event on a given file descriptor.
     */
    virtual void onPollError() = 0;

    /**
     * Callback upon a trig request to process an event from the event thread context
     * (to avoid borrow the context of a caller for example).
     *
     * @param[in] context pointer that was given by the client of the event thread through trig.
     *                    Note that is may be NULL.
     * @param[in] eventId that was given by the client of the event thread through trig.
     *                    NOTE: this parameter is DEPRECATED, for maintained for compatibility.
     *
     * @return true if the list of file descripter polled has changed, false otherwise.
     * @deprecated Using this function with 2 parameters is deprecated, only context shall remain
     *             as a parameter of this function.
     */
    virtual bool onProcess(void *context, uint32_t eventId) = 0;

protected:
    virtual ~IEventListener() {}
};
