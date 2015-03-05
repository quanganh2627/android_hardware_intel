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

#ifndef ANDROID_PHYSICAL_ACTIVITY_SENSOR_H
#define ANDROID_PHYSICAL_ACTIVITY_SENSOR_H

#include <stdint.h>
#include <errno.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <dlfcn.h>
#include <deque>

#include <utils/Mutex.h>
#include "activity.h"
#include "PSHSensor.hpp"

/*****************************************************************************/
/*
 * Author: Zheng Huan <huan.zheng@intel.com> Sun Taiyi <taiyi.sun@intel.com>
 * Group: PSI, MCG
 * Virtual sensor that interacts with PSH to report physical activity events to sensor manager
 *
 * The following physical activity events will be reported
 *   Walking
 *   Running
 *   Sedentary
 *   Random
 *   Biking
 *   Driving
 *
 * It supports the following four kinds of modes
 *   Instant Mode
 *   Quick Mode
 *   Normal Mode
 *   Statistic Mode
 *
 * When enabled, one thread will be running to interact with PSH.
 * If it is instant mode, the thread will poll accelerameter data and do algorithm calculation
 * If it is one of other three modes, the thread will just poll physical-activity info from PSH directly.
 *
 * There's a pipe setup between the thread and sensor manager to report physical activity event
 */

using android::Mutex;

#define SYMBOL_ACTIVITY_INSTANT_INIT "activity_instant_init"
#define SYMBOL_ACTIVITY_INSTANT_COLLECT_DATA "activity_instant_collect_data"
#define SYMBOL_ACTIVITY_INSTANT_PROCESS "activity_instant_process"
#define LIB_ACTIVITY_INSTANT "libActivityInstant.so"

typedef SH_STATUS (*FUNC_ACTIVITY_INSTANT_INIT) (ACTIVITY_CB cb, void *ctx);
typedef int (*FUNC_ACTIVITY_INSTANT_COLLECT_DATA) (short ax, short ay, short az);
typedef SH_STATUS (*FUNC_ACTIVITY_INSTANT_PROCESS) ();

class PhysicalActivitySensor : public PSHSensor
{
        int mEnabled;
public:
        PhysicalActivitySensor() {};
        PhysicalActivitySensor(SensorDevice &device);
        virtual ~PhysicalActivitySensor();
        virtual int getData(std::queue<sensors_event_t> &eventQue);
        virtual int activate(int32_t handle, int enabled);
        virtual int setDelay(int32_t handle, int64_t ns);
        virtual int getPollfd();
        virtual bool selftest();

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

        void loadAlgorithm();

        bool isAlgorithmLoaded();

        void unLoadAlgorithm();

        static int getPADelay(int64_t ns);
        static int getPA(short result);
        static void*  workerThread(void* data);
        static int ActCB(void *ctx, short *results, int len);

        Mutex       mDelayMutex;
        int64_t     mCurrentDelay;

        handle_t    mPAHandle;
        handle_t    mAccHandle;

        int         mWakeFDs[2];
        pthread_t   mWorkerThread;  // only one thread running at one time

        int         mResultPipe[2];
        int64_t     last_timestamp;

        // for instant mode calculation
        short mPSHCn;  // result from lab algorithm

        // Algorithm function pointers
        void *mLibActivityInstant;
        FUNC_ACTIVITY_INSTANT_INIT mActivityInstantInit;
        FUNC_ACTIVITY_INSTANT_COLLECT_DATA mActivityInstantCollectData;
        FUNC_ACTIVITY_INSTANT_PROCESS mActivityInstantProcess;

        // decorator stuff to process output coming from PSH
        class Client
        {
        public:
                virtual ~Client();
                virtual bool accept(short *data, int len) = 0;
                virtual void reduce(void);
                virtual void reset(void);
                virtual void publish(int &result, int &score);
                int convertResult(int cn);

        public:
                std::deque<short> mStream;
                std::deque<short> mScore;

                // number of results inside buffer
                int N;
        };

        class NCycleClient : public Client
        {
        public:
                explicit NCycleClient(int n);
                virtual ~NCycleClient();
                virtual bool accept(short *item, int len);
        };

        class ClientSummarizer : public Client
        {
        public:
                explicit ClientSummarizer(const Client * c);
                virtual ~ClientSummarizer();
                virtual bool accept(short *item, int len);
                virtual void reduce(void);
                virtual void reset(void);
        protected:
                Client * mClient;
                int * mWeights;
        };
};

/*****************************************************************************/
#endif  // ANDROID_PHYSICAL_ACTIVITY_SENSOR_H
