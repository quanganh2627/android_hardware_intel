/*
 *
 * Copyright 2013 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "AudioCommsAssert.hpp"
#include "Observer.hpp"
#include <utils/Log.h>
#include <vector>

namespace audio_comms
{

namespace utilities
{

class Observer;

class Observable
{
public:
    virtual ~Observable() {}

    /**
     * Add an observer to the subject.
     *
     * @param[in] o observer to add.
     */
    void addObserver(Observer *o)
    {
        AUDIOCOMMS_ASSERT(o != NULL, "Trying to add NULL observer");
        observers.push_back(o);
    }

    /**
     * Remove an observer from the subject.
     *
     * @param[in] o observer to remove.
     */
    void removeObserver(Observer *o)
    {
        ObsVector::iterator it = std::find(observers.begin(), observers.end(), o);

        if (it != observers.end()) {
            observers.erase(it);
        }
    }

    /**
     * Notify all the observers.
     */
    void notify()
    {
        ObsVector::iterator it = observers.begin();
        for (; it != observers.end(); ++it) {
            Observer *o = *it;
            o->notify();
        }
    }
private:
    typedef std::vector<Observer *> ObsVector;
    ObsVector observers; /**< vector of observer. */
};

} // namespace utilities

} // namespace audio_comms
