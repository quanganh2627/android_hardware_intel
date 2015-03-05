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

#ifndef ANDROID_PEDOMETER_SENSOR_H
#define ANDROID_PEDOMETER_SENSOR_H

#include <stdint.h>
#include <errno.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <deque>

#include <utils/Mutex.h>

#include "SensorBase.h"

/*****************************************************************************/
/*
 * Author: Zheng Huan <huan.zheng@intel.com> Sun Taiyi <taiyi.sun@intel.com>
 * Group: PSI, MCG
 * 
 * Virtual sensor that interacts with PSH to report pedometer events to sensor manager
 * It supports the following four kinds of modes
 *   Instant Mode
 *   Quick Mode
 *   Normal Mode
 *   Statistic Mode
 * 
 * When enabled, one thread will be running to interact with PSH to retrieve pedometer data
 * and report to sensor manager.
 * 
 * There's a pipe setup between the thread and sensor manager to report pedometer event
 */

using android::Mutex;

class PedometerSensor : public SensorBase
{
    int mEnabled;
    sensors_event_t mPendingEvent;
    bool mHasPendingEvent;

public:
    PedometerSensor();
    virtual ~PedometerSensor();
    virtual int readEvents(sensors_event_t* data, int count);
    virtual bool hasPendingEvents() const;
    virtual int enable(int32_t handle, int enabled);
    virtual int setDelay(int32_t handle, int64_t ns);
    virtual int getFd() const;

private:
    // Stop current worker thread
    void stopWorker();

    // Start worker thread
    bool startWorker();

    // Restart worker thread
    bool restartWorker(int64_t newDelay);

    inline void connectToPSH();

    inline bool isConnectToPSH();

    inline void disconnectFromPSH();

    inline bool setupResultPipe();

    inline bool isResultPipeSetup();

    inline void tearDownResultPipe();

    inline bool setUpThreadWakeupPipe();

    inline bool isThreadWakeupPipeSetup();

    inline void tearDownThreadWakeupPipe();

    static int getPedoDelay(int64_t ns);
    static void*  workerThread(void* data);
    static void threadID();

    Mutex       mDelayMutex;
    int64_t     mCurrentDelay;

    handle_t    mPedoHandle;

    int         mWakeFDs[2];
    pthread_t   mWorkerThread;  // only one thread running at one time

    int         mResultPipe[2];
};

/*****************************************************************************/
#endif  // ANDROID_PEDOMETER_SENSOR_H
