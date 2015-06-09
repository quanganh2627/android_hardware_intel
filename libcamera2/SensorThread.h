/*
 * Copyright (C) 2013-2014 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_LIBCAMERA_SENSOR_THREAD_H
#define ANDROID_LIBCAMERA_SENSOR_THREAD_H

#include <android/sensor.h>
#include <gui/Sensor.h>
#include <gui/SensorManager.h>
#include <gui/SensorEventQueue.h>
#include <utils/Looper.h>
#include <utils/SortedVector.h>
#include "IOrientationListener.h"

namespace android {

class SensorLooperThread :
    public Thread {

public:
    SensorLooperThread(Looper* looper);
    ~SensorLooperThread();
    virtual bool threadLoop();
    virtual void requestExit();
    virtual status_t requestExitAndWait();

// prevent copy constructor and assignment operator
private:
    SensorLooperThread(const SensorLooperThread& other);
    SensorLooperThread& operator=(const SensorLooperThread& other);

    void preventFastLooping();

private:
    sp<Looper> mLooper;
    uint64_t mLastPollTimestamp;
    uint32_t mFastLoopCounter;
};


class SensorThread {

public:
    static SensorThread * getInstance() {
        if (sInstance == NULL)
            sInstance = new SensorThread();

        return sInstance;
    }

    virtual ~SensorThread();

    status_t requestExitAndWait();

    /**
     * register orientation listener
     *
     * After regiseration listener callback function is called when
     * orientation change.
     *
     * \param listener to register
     *
     * \return current orientation value
     */
    int registerOrientationListener(IOrientationListener* listener);

    /**
     * unregister orientation listener
     *
     * remove orientation listener so it not get callback anymore
     *
     * \param listener to register
     *
     * \return none
     */
    void unRegisterOrientationListener(IOrientationListener* listener);

// prevent copy constructor and assignment operator
private:
    SensorThread(const SensorThread& other);
    SensorThread& operator=(const SensorThread& other);

// private methods
private:
    SensorThread();
    void orientationChanged(int orientation);

// private data
private:
    int mOrientation;
    sp<Looper> mLooper;
    sp<SensorLooperThread> mThread;
    sp<SensorEventQueue> mSensorEventQueue;
    SortedVector<IOrientationListener*> mListeners;
    Mutex mLock;

// private static data
private:
    static SensorThread* sInstance;

friend int sensorEventsListener(int, int, void*);

}; // class SensorThread

}; // namespace android

#endif // ANDROID_LIBCAMERA_SENSOR_THREAD_H
