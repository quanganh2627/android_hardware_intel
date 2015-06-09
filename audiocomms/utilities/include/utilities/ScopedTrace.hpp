/*
 * @file
 * Scope traceing class
 *
 * @section License
 *
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

#include "utilities/Log.hpp"
#include <pthread.h>
#include <string>

namespace audio_comms
{
namespace utilities
{

/**
 * A ScopedTrace clas is used to track when execution enters a specified scope.
 * You may create a ScopedTrace object at any place. At object creation a log is written via the
 * logger interface; when the object is destroyed a log is displayed giving the time spent in the
 * scope.
 * tparam ScopeTrait  the trait that allows to tune up the logging scopes, ie, giving a suitable
 *                    event name
 * tparam activateAtStart  boolean that gives the default logging behaviour of the class. If set to
 *                         false, the logging may be activated by calling the activate() function.
 * tparam logger      the class that gives the functions used to perform the IO (printing logs)
 *                    It may write them in some system log file but also standart outputs or
 *                    specific files
 * example of use:
 * void someFunction() {
 * ScopedTrace scope(ScopeTrait::somePlace);
 * doSomething();
 * }
 * this will log:
 * Enters some place @1150283893
 * Leaves some place @1150284218 time spent: 325us
 */
template <class ScopeTrait, bool activateAtStart = false, class logger = Log::Verbose>
class ScopedTrace
{
public:
    ScopedTrace(const typename ScopeTrait::Event &event)
        : mEventName(ScopeTrait::getEventName(event)),
          mActive(active)
    {
        if (mActive) {
            mStartTime = gettime();
            logger log;
            for (size_t i = 0; i < indentDepth * 2; i++) {
                log << " ";
            }
            log << "Enters " << mEventName << " @" << mStartTime;
            indentDepth++;
        }
    }
    ~ScopedTrace()
    {
        if (mActive) {
            indentDepth--;
            logger log;
            for (size_t i = 0; i < indentDepth * 2; i++) {
                log << " ";
            }
            uint64_t end = gettime();
            log << "Leaves " << mEventName
                << " @" << end << " time spent: " << end - mStartTime << "us";
        }
    }
    template <class T>
    void trace(const T &val)
    {
        if (mActive) {
            logger log;
            for (size_t i = 0; i < indentDepth * 2; i++) {
                log << " ";
            }
            log << "trace " << mEventName << " @" << gettime()
                << " value=" << val;
        }
    }

    /**
     * Activate the logging on the platform for ALL scope objects
     */
    static void activate()
    {
        active = true;
    }

    /**
     * Deactivate the logging for ALL scope objects
     */
    static void deactivate()
    {
        active = false;
    }

    static bool isActive()
    {
        return active;
    }

private:
    uint64_t gettime()
    {
        struct timespec tp;
        clock_gettime(CLOCK_MONOTONIC, &tp);
        uint64_t microSecondes = tp.tv_sec * 1000000 + tp.tv_nsec / 1000;
        return microSecondes;
    }

    static volatile bool active;
    static size_t __thread indentDepth;

    const std::string mEventName;
    uint64_t mStartTime;
    const bool mActive;
};

template <class ScopeTrait, bool activeAtStart, class logger>
size_t __thread ScopedTrace<ScopeTrait, activeAtStart, logger>::indentDepth = 0;

template <class ScopeTrait, bool activeAtStart, class logger>
volatile bool ScopedTrace<ScopeTrait, activeAtStart, logger>::active = activeAtStart;

} /* namespace utilities */
} /* namespace audio_comms */
