/*
 * @file
 * Mutual exclusion class an utilities.
 *
 * @section License
 *
 * Copyright 2013 Intel Corporation
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

#include "AudioCommsAssert.hpp"

#include <pthread.h>
#include <cerrno>
#include <string.h>

namespace audio_comms
{

namespace utilities
{

/**
 * Mutex class wraps the system's mutex implementation to give a suitable C++
 * API */
class Mutex
{
public:
    Mutex()
    {
        pthread_mutex_init(&_mutex, NULL);
    }

    ~Mutex()
    {
        int err = pthread_mutex_destroy(&_mutex);
        AUDIOCOMMS_ASSERT(err == 0,
                          "Unable to destroy mutex @" << static_cast<const void *>(&_mutex)
                                                      << ": " << strerror(
                              err) << "(" << err << ")");
    }

    /**
     * Lock the mutex
     */
    void lock()
    {
        int err = pthread_mutex_lock(&_mutex);
        AUDIOCOMMS_ASSERT(err == 0, "Unable to lock mutex @" << static_cast<const void *>(&_mutex)
                                                             << ": " << strerror(
                              err) << "(" << err << ")");
    }

    /**
     * Unlock the mutex
     */
    void unlock()
    {
        int err = pthread_mutex_unlock(&_mutex);
        AUDIOCOMMS_ASSERT(err == 0, "Unable to unlock mutex @" << static_cast<const void *>(&_mutex)
                                                               << ": " << strerror(
                              err) << "(" << err << ")");
    }

    /**
     * Locker class automatically take and release a lock
     */
    class Locker
    {
    public:
        /**
         * @param[in] mutex  mutex to lock
         */
        Locker(Mutex &mutex) : _mutex(mutex)
        {
            _mutex.lock();
        }

        /**
         * Destructor releases the lock
         */
        ~Locker()
        {
            _mutex.unlock();
        }

    private:
        Mutex &_mutex; /**< mutex the locker holds */
    };

private:
    pthread_mutex_t _mutex; /**< internal mutex */
};

} // namespace utilities

} // namespace audio_comms
