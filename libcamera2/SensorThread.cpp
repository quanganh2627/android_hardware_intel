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

#define LOG_TAG "Camera_SensorThread"

#include "SensorThread.h"
#include "LogHelper.h"
#include <math.h>

namespace android {

static const uint64_t POLL_INTERVAL = 200000000; /* ns. equals 200ms. */

SensorThread* SensorThread::sInstance = NULL;

SensorLooperThread::SensorLooperThread(Looper* looper)
    : Thread(false)
    ,mLooper(looper)
    ,mLastPollTimestamp(0)
    ,mFastLoopCounter(0)
{
    LOG1("@%s", __FUNCTION__);
}

SensorLooperThread::~SensorLooperThread()
{
    LOG1("@%s", __FUNCTION__);
    mLooper.clear();
}


void SensorLooperThread::preventFastLooping()
{
    // Sometimes it can happen that the threadLoop starts to run in a busy-loop kind of fashion.
    // Therefore, check the timestamps and throttle down as the last resort, if
    // this is happening.
    uint64_t now = systemTime();
    if (now - mLastPollTimestamp < POLL_INTERVAL / 2) {
        if (++mFastLoopCounter > 2) {
            // poll returned more than twice faster than it should, and it isn't
            // just a couple of incidents like what happens during thread stopping,
            // so throttle down
            ALOGW("@%s, loop running too fast, throttling down!", __FUNCTION__);
            usleep(ns2us(POLL_INTERVAL / 2)); // sleep half of interval in microseconds
        }
    } else {
        // all ok. reset counter.
        mFastLoopCounter = 0;
    }
    mLastPollTimestamp = now;
}

bool SensorLooperThread::threadLoop()
{
    mLooper->pollOnce(-1);
    preventFastLooping();
    return true;
}

void SensorLooperThread::requestExit()
{
     LOG1("@%s", __FUNCTION__);
     Thread::requestExit();
     mLooper->wake();
}

status_t SensorLooperThread::requestExitAndWait()
{
     LOG1("@%s", __FUNCTION__);
     Thread::requestExit();
     mLooper->wake();
     return Thread::requestExitAndWait();
}

int sensorEventsListener(int fd, int events, void* data)
{
    LOG2("@%s", __FUNCTION__);
    SensorThread *sensorThread(static_cast<SensorThread*>(data));
    ssize_t num_sensors(0);
    int orientation(-1);
    ASensorEvent sen_events[8];

    while ((num_sensors = sensorThread->mSensorEventQueue->read(sen_events, 8)) > 0) {
        for (int i = 0; i < num_sensors; i++) {
            if (sen_events[i].type == Sensor::TYPE_ACCELEROMETER) {
                float x = sen_events[i].acceleration.x;
                float y = sen_events[i].acceleration.y;
                float z = sen_events[i].acceleration.z;

                orientation = (int) (atan2f(-x, y) * 180 / M_PI);

                if (orientation < 0) {
                         orientation += 360;
                }

                 LOG2("@%s: Accelerometer event: x = %f y = %f z = %f orientation = %d",
                      __FUNCTION__, x, y, z, orientation);
            }
        }
    }

    if (num_sensors < 0 && num_sensors != -EAGAIN) {
        ALOGE("reading sensors events failed: %s", strerror(-num_sensors));
    }

    if (orientation != -1) {
        // round orientation to 0, 90, 180 or 270
        orientation += 45;
        orientation = (orientation / 90) * 90;
        orientation = orientation % 360;

        if (orientation != sensorThread->mOrientation)
            sensorThread->orientationChanged(orientation);
    }

    return 1; // continue looper listening
}

SensorThread::SensorThread()
    : mOrientation(0)
{
    LOG1("@%s", __FUNCTION__);

    mLooper = new Looper(false);
    if (mLooper == NULL) {
        ALOGE("Looper alloc failed");
        return;
    }

    SensorManager& sensorManager(SensorManager::getInstance());
    mSensorEventQueue = sensorManager.createEventQueue();

    if (mSensorEventQueue != NULL) {
        mLooper->addFd(mSensorEventQueue->getFd(), 0, ALOOPER_EVENT_INPUT,
                       sensorEventsListener, this);
    } else {
        ALOGE("sensorManager createEventQueue failed");
    }
}

SensorThread::~SensorThread()
{
    LOG1("@%s", __FUNCTION__);

    if (mThread != NULL) {
        mThread->requestExitAndWait();
        mThread.clear();
    }

    if (mSensorEventQueue != NULL) {
        mLooper->removeFd(mSensorEventQueue->getFd());
    }

    mLooper.clear();
    mSensorEventQueue.clear();

    sInstance = NULL;
}

int SensorThread::registerOrientationListener(IOrientationListener* listener) {
    LOG1("@%s", __FUNCTION__);

    Mutex::Autolock lock(&mLock);

    if (mThread == NULL) {
        mThread = new SensorLooperThread(mLooper.get());
        if (mThread == NULL) {
            ALOGE("Sensor looper thread alloc failed");
            return NO_MEMORY;
        }

        if (mThread->run("CamHAL_SENSOR") != NO_ERROR) {
            ALOGE("Error starting sensor thread!");
            return UNKNOWN_ERROR;
        }
    }

    if (mListeners.isEmpty()) {
        SensorManager& sensorManager(SensorManager::getInstance());
        Sensor const* sensor = sensorManager.getDefaultSensor(Sensor::TYPE_ACCELEROMETER);
        if (sensor == NULL) {
            ALOGE("@%s: fail to get accelerometer sensor", __FUNCTION__);
            return 0;
        }

        mSensorEventQueue->enableSensor(sensor);
        mSensorEventQueue->setEventRate(sensor, POLL_INTERVAL);

        ALOGD("@%s: accelerometer sensor start %p (%s)", __FUNCTION__ , sensor, sensor->getName().string());
    }

    mListeners.add(listener);

    return mOrientation;
}

void SensorThread::unRegisterOrientationListener(IOrientationListener* listener){
    LOG1("@%s", __FUNCTION__);

    Mutex::Autolock lock(&mLock);

    mListeners.remove(listener);

    if (mListeners.isEmpty()) {
        // need to request exit here, since in some crash cases calls to SensorManager will get stuck
        if (mThread != NULL) {
            mThread->requestExit();
            mThread.clear();
        }

        SensorManager& sensorManager(SensorManager::getInstance());
        Sensor const* sensor = sensorManager.getDefaultSensor(Sensor::TYPE_ACCELEROMETER);
        if (sensor == NULL) {
            ALOGE("@%s: fail to get accelerometer sensor", __FUNCTION__);
            return;
        }

        mSensorEventQueue->disableSensor(sensor);

        ALOGD("@%s: accelerometer sensor stop %p (%s)", __FUNCTION__ , sensor, sensor->getName().string());
    }
}

void SensorThread::orientationChanged(int orientation){
    LOG1("@%s: orientation = %d", __FUNCTION__, orientation);

    Mutex::Autolock lock(&mLock);

    mOrientation = orientation;

    for (size_t i = 0; i < mListeners.size() ; ++i) {
        mListeners[i]->orientationChanged(orientation);
    }
}

} // namespace android
