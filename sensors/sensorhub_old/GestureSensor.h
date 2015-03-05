/*
 * Copyright (C) 2008 The Android Open Source Project
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

#ifndef ANDROID_GESTURE_SENSOR_H
#define ANDROID_GESTURE_SENSOR_H

#include <stdint.h>
#include <errno.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <dlfcn.h>

#include <utils/Mutex.h>

#include "SensorBase.h"

/*****************************************************************************/
/*
 * Author: Zheng Huan <huan.zheng@intel.com>; Han Ke <ke.a.han@intel.com>
 * Group: PSI, MCG
 *
 * Virtual sensor that interacts with PSH to report gesture glyph events to sensor manager
 * The following gesture flick events will be reported
 *   Ear Touch
 *   Ear Touch Back
 *   Number One
 *   Number Two
 *   Number Three
 *   Number Four
 *   Number Five
 *   Number Six
 *   Number Seven
 *   Number Eight
 *   Number Nine
 *   Number Zero
 *
 * When this virtual sensor is enabled, two threads will be started
 * One thread is running glyph-gesture detection algorithm
 * The other thread is monitering proximity status to improve precision of glyph detection
 * 
 * There's a pipe setup between the thread which runs algorithm and sensor manager to 
 * report gesture event
 */

using android::Mutex;

#define SYMBOL_GESTURE_PROCESS_SINGLE_DATA "gesture_process_single_data"
#define SYMBOL_GESTURE_INIT "gesture_initial"
#define SYMBOL_GESTURE_CLOSE "gesture_close"
#define LIBGESTURE "libgesture.so"
typedef bool (*FUNC_GESTURE_INIT) (int argc, char* argv[]);
typedef char* (*FUNC_GESTURE_PROCESS_SINGLE_DATA) (short *data, bool segmented, bool last);
typedef void (*FUNC_GESTURE_CLOSE) ();

class GestureSensor : public SensorBase
{
    int mEnabled;
    sensors_event_t mPendingEvent;
    bool mHasPendingEvent;

public:
    GestureSensor();
    virtual ~GestureSensor();
    virtual int readEvents(sensors_event_t* data, int count);
    virtual bool hasPendingEvents() const;
    virtual int enable(int32_t handle, int enabled);
    virtual int setDelay(int32_t handle, int64_t ns);
    virtual int getFd() const;

private:
    /**
     * Thread starts in ActivateContext(), stops in DeactivateContext()
     * In thread, read PSH accel output, and call libgesture libgesturespotting API
     * to process data;
     */
    static void*            PollingThread_accel(void* data);
    bool                    Start_accel();
    void                    Stop_accel();
    pthread_t               mPollingThreadIDAccel;
    handle_t                mHandleAccel;
    int                     mWakeFdsAccel[2];
    Mutex                   mMutexAccel;

    /**
     * Thread starts in ActivateContext(), stops in DeactivateContext()
     * In thread, read PSH gyro output
     */
    static void*            PollingThread_gyro(void* data);
    bool                    Start_gyro();
    void                    Stop_gyro();
    pthread_t               mPollingThreadIDGyro;
    handle_t                mHandleGyro;
    int                     mWakeFdsGyro[2];
    short                   mDataGyro[3];
    Mutex                   mMutexGyro;


    /**
     * Thread starts when gesture sensor is enabled, stops when gesture sensor is disabled
     * In thread, read PSH proximity sensor output, and update mFlagProximity
     */
    static void*            PollingThread_proximity(void* data);
    bool                    Start_proximity();
    void                    Stop_proximity();
    pthread_t               mPollingThreadIDProximity;
    handle_t                mHandleProximity;
    int                     mWakeFdsProximity[2];
    bool                    mFlagProximity;
    Mutex                   mMutexProximity;

    int                     mResultPipe[2];

    /**
       * symbols relate to libgesture library
       */
    void*                   mLibraryHandle;
    FUNC_GESTURE_INIT       mGestureInit;
    FUNC_GESTURE_PROCESS_SINGLE_DATA mGestureProcessSingleData;
    FUNC_GESTURE_CLOSE      mGestureClose;
    bool                    mHasGestureLibrary;

    static int getGestureFromString(char* gesture);
    static void threadID();

    bool mInitGesture;
};

/*****************************************************************************/
#endif  // ANDROID_GESTURE_SENSOR_H
