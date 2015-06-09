/*
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

#include "AudioCommsAssert.hpp"

#include <semaphore.h>
#include <cerrno>
#include <string.h>
#include <inttypes.h>

namespace audio_comms
{
namespace utilities
{

class Semaphore
{
public:
    /**
     * Construct a semaphore
     *
     * @param initialValue initial semaphore value
     */
    Semaphore(unsigned int initialValue)
    {
        int err = sem_init(&_sem, 0, initialValue);
        AUDIOCOMMS_ASSERT(err == 0, "Unable to create semaphore: "
                          << strerror(errno) << "(" << errno << ")");
    }

    ~Semaphore()
    {
        int err = sem_destroy(&_sem);
        AUDIOCOMMS_ASSERT(err == 0, "Unable to destroy semaphore @"
                          << static_cast<const void *>(&_sem)
                          << ": " << strerror(errno) << "(" << errno << ")");
    }

    /**
     * Increment the semaphore value, which may result to unlock another thread
     *
     * If the semaphore's value consequently becomes greater than zero, then another process or
     * thread blocked in a wait() call will be woken up and proceed to lock the semaphore.
     */
    void post()
    {
        int err = sem_post(&_sem);
        AUDIOCOMMS_ASSERT(err == 0, "Unable to post semaphore @"
                          << static_cast<const void *>(&_sem)
                          << ": " << strerror(errno) << "(" << errno << ")");
    }

    /**
     * Decrement the semaphore value, which may result to lock the thread
     *
     * If the semaphore's value is greater than zero, then the decrement proceeds, and the function
     * returns, immediately.  If the semaphore currently has the value zero, then the call blocks
     * until either it becomes possible to perform the decrement.
     */
    void wait()
    {
        int err;
        do {
            err = sem_wait(&_sem);
        } while ((err == -1) && (errno == EINTR));

        AUDIOCOMMS_ASSERT(err == 0, "Unable to wait for semaphore @"
                          << static_cast<const void *>(&_sem)
                          << ": " << strerror(errno) << "(" << errno << ")");
    }

    /**
     * Decrement the semaphore value, which may result to lock the thread, limited by a timeout
     *
     * @param[in] timeoutMs the timeout in milliseconds
     * @return false if timeout has occured
     */
    bool wait(time_t timeoutMs)
    {
        timespec timeoutSpec;

        // get current timespec
        int err = clock_gettime(CLOCK_REALTIME, &timeoutSpec);
        AUDIOCOMMS_ASSERT(err == 0, "Unable to get clock time: "
                          << strerror(errno) << "(" << errno << ")");


        // increment timespec by timeoutMs parameter
        timeoutSpec.tv_sec = timeoutSpec.tv_sec + (uint32_t)timeoutMs / 1000;
        timeoutSpec.tv_nsec = timeoutSpec.tv_nsec + (((uint32_t)timeoutMs % 1000) * 1000000);
        if (timeoutSpec.tv_nsec >= 1000000000) {
            timeoutSpec.tv_nsec -= 1000000000;
            timeoutSpec.tv_sec++;
        }

        // wait on semaphore with supplied timeout
        do {
            err = sem_timedwait(&_sem, &timeoutSpec);
        } while ((err == -1) && (errno == EINTR));

        AUDIOCOMMS_ASSERT(err == 0 || errno == ETIMEDOUT, "Unable to wait for semaphore @"
                          << static_cast<const void *>(&_sem)
                          << ": " << strerror(errno) << "(" << errno << ")");

        return err == 0;
    }

private:
    sem_t _sem; /**< internal semaphore*/
};

}
}
