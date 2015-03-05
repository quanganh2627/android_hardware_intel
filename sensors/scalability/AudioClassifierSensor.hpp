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

#ifndef ANDROID_AUDIOCLASSIFIER_SENSOR_H
#define ANDROID_AUDIOCLASSIFIER_SENSOR_H

#include <stdint.h>
#include <errno.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <deque>

#include <utils/Mutex.h>

#include <hardware/audio.h>
#include <media/AudioSystem.h>
#include <aware.h>
#include "PSHSensor.hpp"
/*****************************************************************************/
/*
 * Author:Zheng Huan <huan.zheng@intel.com>,  Duan Qin <qin.duan@intel.com>
 * Group: PSI, MCG
 *
 * Virtual sensor that interacts with PSH to report audio classifier events to sensor manager
 * It supports the following four kinds of modes
 *   Instant Mode
 *   Quick Mode
 *   Normal Mode
 *   Statistic Mode
 *
 * When enabled, one thread will be running to interact with PSH to retrieve audio classifier data
 * and report to sensor manager.
 *
 * There's a pipe setup between the thread and sensor manager to report audio classifier event
 */

using android::Mutex;
enum _ClassifierMask
{
    FOR_CLASSIFIER_MASK = 0x0000ffff,
    FOR_DB_MASK         = 0xffff0000
};

class AudioClassifierSensor : public PSHSensor
{
    class AudioHAL;
    int mEnabled;

public:
    AudioClassifierSensor(SensorDevice &device);
    virtual ~AudioClassifierSensor();
    virtual int getData(std::queue<sensors_event_t> &eventQue);
    virtual int activate(int handle, int enabled);
    virtual int setDelay(int32_t handle, int64_t ns);
    virtual int getPollfd();
    virtual bool selftest();
private:
    AudioClassifierSensor(){};

    inline bool setupResultPipe();

    inline bool isResultPipeSetup();

    inline void tearDownResultPipe();

    inline bool setUpThreadWakeupPipe();

    inline bool isThreadWakeupPipeSetup();

    inline void tearDownThreadWakeupPipe();

    inline void connectToPSH();

    inline int startStreamForPSH();

    inline int stopStreamForPSH();

    inline int propertySetForPSH(int delay);

    inline bool isConnectToPSH();

    inline void disconnectFromPSH();

    static int  workerThread(void* data);

    void condEventWait(int timeout);

    void condEventStopWait();

    int getAudioDelay(int64_t);

    bool restartWorker(int64_t newDelay);

    bool startWorker();

    void stopWorker();

    AudioHAL * mAudioHal;
    int64_t     mCurrentDelay;
    Mutex       mDelayMutex;
    pthread_mutex_t mMutexTimeOut;
    pthread_cond_t mCondTimeOut;
    int         mResultPipe[2];
    handle_t    mAudioHandle;
    int         mWakeFDs[2];
    pthread_t   mWorkerThread;  // only one thread running at one time
    Mutex m_cond;
    class AudioHAL
    {
        public:
            AudioHAL();
            ~AudioHAL();
            void audioHalActivate();
            void audioHalDeActivate();
        private:
            bool audioHalInit();
            bool isInited();
            aware_hw_device_t        *device;
    };
};

/*****************************************************************************/
#endif  // ANDROID_AUDIOCLASSIFIER_SENSOR_H
