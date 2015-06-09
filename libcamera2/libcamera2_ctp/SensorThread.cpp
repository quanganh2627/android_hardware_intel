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

#define LOG_TAG "Camera_SensorThread"

#include "SensorThread.h"
#include "LogHelper.h"
#include <math.h>

namespace android {

SensorThread* SensorThread::sInstance = NULL;
SensorThread* SensorThread::sInstance_1 = NULL;

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

SensorThread::SensorThread(int cameraId) :
    Thread(false)
    , mOrientation(0)
    ,mCameraId(cameraId)
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

    if (mSensorEventQueue != NULL) {
        mLooper->removeFd(mSensorEventQueue->getFd());
    }

    mLooper.clear();
    mSensorEventQueue.clear();

    if (mCameraId == 0)
        sInstance = NULL;
    else
        sInstance_1 = NULL;
}

int SensorThread::registerOrientationListener(IOrientationListener* listener) {
    LOG1("@%s", __FUNCTION__);

    Mutex::Autolock lock(&mLock);

    if (mListeners.isEmpty()) {
        SensorManager& sensorManager(SensorManager::getInstance());
        Sensor const* sensor = sensorManager.getDefaultSensor(Sensor::TYPE_ACCELEROMETER);
        if (sensor == NULL) {
            ALOGE("@%s: fail to get accelerometer sensor", __FUNCTION__);
            return 0;
        }

        mSensorEventQueue->enableSensor(sensor);
        mSensorEventQueue->setEventRate(sensor, ms2ns(200));

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

bool SensorThread::threadLoop() {
    mLooper->pollOnce(-1);
    return true;
}

status_t SensorThread::requestExitAndWait() {
     LOG1("@%s", __FUNCTION__);
     Thread::requestExit();
     mLooper->wake();
     return Thread::requestExitAndWait();
}

} // namespace android
