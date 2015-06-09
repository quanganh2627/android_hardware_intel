/*
 * Copyright (C) 2013 Intel Corporation
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

class SensorThread :
    public Thread {

public:
    static SensorThread * getInstance(int cameraId) {
        if(cameraId == 1) {
            if(sInstance_1 == NULL)
                sInstance_1 = new SensorThread(cameraId);
            return sInstance_1;
        } else {
            if (sInstance == NULL)
                sInstance = new SensorThread(cameraId);
            return sInstance;
        }
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

// inherited from Thread
private:
    virtual bool threadLoop();

// private methods
private:
    SensorThread(int cameraId);
    void orientationChanged(int orientation);

// private data
private:
    int mOrientation;
    sp<Looper> mLooper;
    sp<SensorEventQueue> mSensorEventQueue;
    SortedVector<IOrientationListener*> mListeners;
    Mutex mLock;
    int mCameraId;

// private static data
private:
    static SensorThread* sInstance;
    static SensorThread* sInstance_1;

friend int sensorEventsListener(int, int, void*);

}; // class SensorThread

}; // namespace android

#endif // ANDROID_LIBCAMERA_SENSOR_THREAD_H
