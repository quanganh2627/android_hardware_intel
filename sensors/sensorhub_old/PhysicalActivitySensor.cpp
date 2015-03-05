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

#include "PhysicalActivitySensor.h"

#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/select.h>
#include <assert.h>
#include <cutils/log.h>
#include <utils/AndroidThreads.h>

#include <sstream>

/*****************************************************************************/

#undef LOG_TAG
#define LOG_TAG "PhysicalActivitySensor"

#define SLEEP_ON_FAIL_USEC 500000

#define PA_INTERVAL (static_cast<float>(5.12))
#define PA_QUICK (static_cast<float>(5.12))
#define PA_NORMAL (static_cast<float>(30))
#define PA_STATISTIC (static_cast<float>(300))

#define THREAD_NOT_STARTED 0
#define PIPE_NOT_OPENED -1
#define PSH_SESSION_NOT_OPENED NULL

static const char * classNames[] = {
    "none", "biking", "walking", "running",
    "incar", "intrain", "random", "sedentary",
};

PhysicalActivitySensor::PhysicalActivitySensor()
    : SensorBase("physical_activity"),
      mEnabled(0),
      mHasPendingEvent(false)
{
    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = SENSORS_HANDLE_PHYSICAL_ACTIVITY;
    mPendingEvent.type = SENSOR_TYPE_PHYSICAL_ACTIVITY;
    memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));

    mResultPipe[0] = PIPE_NOT_OPENED;
    mResultPipe[1] = PIPE_NOT_OPENED;

    mWakeFDs[0] = PIPE_NOT_OPENED;
    mWakeFDs[1] = PIPE_NOT_OPENED;

    mCurrentDelay = 0;
    mWorkerThread = THREAD_NOT_STARTED;

    mPSHCn = 0;

    mPAHandle = PSH_SESSION_NOT_OPENED;
    mAccHandle = PSH_SESSION_NOT_OPENED;

    mLibActivityInstant = NULL;
    mActivityInstantInit = NULL;
    mActivityInstantCollectData = NULL;
    mActivityInstantProcess = NULL;

    connectToPSH();

    setupResultPipe();

    loadAlgorithm();
}

PhysicalActivitySensor::~PhysicalActivitySensor() {
    LOGI("~PhysicalActivitySensor %d\n", mEnabled);

    stopWorker();

    disconnectFromPSH();

    tearDownResultPipe();

    unLoadAlgorithm();
}

void PhysicalActivitySensor::loadAlgorithm()
{
    // load libActivityInstant.so
    mLibActivityInstant = dlopen(LIB_ACTIVITY_INSTANT, RTLD_NOW);
    if (mLibActivityInstant != NULL) {
        mActivityInstantInit = (FUNC_ACTIVITY_INSTANT_INIT) dlsym(mLibActivityInstant,
                                                 SYMBOL_ACTIVITY_INSTANT_INIT);
        mActivityInstantCollectData = (FUNC_ACTIVITY_INSTANT_COLLECT_DATA) dlsym(
                                                 mLibActivityInstant,
                                                 SYMBOL_ACTIVITY_INSTANT_COLLECT_DATA);
        mActivityInstantProcess = (FUNC_ACTIVITY_INSTANT_PROCESS) dlsym(mLibActivityInstant,
                                                 SYMBOL_ACTIVITY_INSTANT_PROCESS);

        if (mActivityInstantInit != NULL && mActivityInstantCollectData != NULL
            && mActivityInstantProcess != NULL) {
            LOGI("Got all required functions from libActivityInstant.so");
        } else {
            LOGE("Can't get all required functions from libActivityInstant.so!!");
        }
    } else {
        LOGE("Can't find libActivityInstant.so!!");
    }
}

void PhysicalActivitySensor::unLoadAlgorithm()
{
    if (mLibActivityInstant != NULL) {
        dlclose(mLibActivityInstant);
    }
}

bool PhysicalActivitySensor::isAlgorithmLoaded()
{
    if (mActivityInstantInit != NULL && mActivityInstantCollectData != NULL &&
        mActivityInstantProcess != NULL) {
        return true;
    } else {
        return false;
    }
}

inline void PhysicalActivitySensor::connectToPSH()
{
    assert(!isConnectToPSH());
    // Establish physical activity connection to PSH
    mPAHandle = psh_open_session(SENSOR_ACTIVITY);
    if (mPAHandle == PSH_SESSION_NOT_OPENED) {
        LOGE("psh_open_session failed. retry once");
        usleep(SLEEP_ON_FAIL_USEC);
        mPAHandle = psh_open_session(SENSOR_ACTIVITY);
        if (mPAHandle == PSH_SESSION_NOT_OPENED) {
            LOGE("psh_open_session failed.");
            return;
        }
    }

    // Establish accelemeter connection to PSH
    mAccHandle = psh_open_session(SENSOR_ACCELEROMETER);
    if (mAccHandle == PSH_SESSION_NOT_OPENED) {
        LOGE("psh_open_session failed. retry once");
        usleep(SLEEP_ON_FAIL_USEC);
        mAccHandle = psh_open_session(SENSOR_ACCELEROMETER);
        if (mAccHandle == PSH_SESSION_NOT_OPENED) {
            LOGE("psh_open_session failed.");
            psh_close_session(mPAHandle);
            mPAHandle = PSH_SESSION_NOT_OPENED;
            return;
        }
    }
}

inline bool PhysicalActivitySensor::isConnectToPSH()
{
    if (mPAHandle != PSH_SESSION_NOT_OPENED
        && mAccHandle != PSH_SESSION_NOT_OPENED)
        return true;
    else
        return false;
}

inline void PhysicalActivitySensor::disconnectFromPSH()
{
    // close connections
    if (mPAHandle != PSH_SESSION_NOT_OPENED)
        psh_close_session(mPAHandle);
    if (mAccHandle != PSH_SESSION_NOT_OPENED)
        psh_close_session(mAccHandle);
    mPAHandle = PSH_SESSION_NOT_OPENED;
    mAccHandle = PSH_SESSION_NOT_OPENED;
}

inline bool PhysicalActivitySensor::setupResultPipe()
{
    assert(!isResultPipeSetup());
    pipe(mResultPipe);
    return isResultPipeSetup();
}

inline bool PhysicalActivitySensor::isResultPipeSetup()
{
    if (mResultPipe[0] != PIPE_NOT_OPENED
        && mResultPipe[1] != PIPE_NOT_OPENED)
        return true;
    else
        return false;
}

inline void PhysicalActivitySensor::tearDownResultPipe()
{
    if (mResultPipe[0] != PIPE_NOT_OPENED)
        close(mResultPipe[0]);

    if (mResultPipe[1] != PIPE_NOT_OPENED)
        close(mResultPipe[1]);

    mResultPipe[0] = mResultPipe[1] = PIPE_NOT_OPENED;
}

inline bool PhysicalActivitySensor::setUpThreadWakeupPipe()
{
    assert(!isThreadWakeupPipeSetup());
    pipe(mWakeFDs);
    return isThreadWakeupPipeSetup();
}

inline bool PhysicalActivitySensor::isThreadWakeupPipeSetup()
{
    if (mWakeFDs[0] != PIPE_NOT_OPENED
        && mWakeFDs[1] != PIPE_NOT_OPENED)
        return true;
    else
        return false;
}

inline void PhysicalActivitySensor::tearDownThreadWakeupPipe()
{
    if (mWakeFDs[0] != PIPE_NOT_OPENED)
        close(mWakeFDs[0]);

    if (mWakeFDs[1] != PIPE_NOT_OPENED)
        close(mWakeFDs[1]);

    mWakeFDs[0] = mWakeFDs[1] = PIPE_NOT_OPENED;
}

// start worker thread
bool PhysicalActivitySensor::startWorker()
{
    if (!isConnectToPSH() || !isResultPipeSetup()) {
        LOGE("Invalid connections to PSH");
        return false;
    }

    // set up wake up pipes
    if (!setUpThreadWakeupPipe()) {
        goto err_handle;
    }

    if (0 == pthread_create(&mWorkerThread, NULL,
                            workerThread, this)) {
        LOGI("create thread succeed");
        return true;
    } else {
        LOGE("create worker thread failed");
    }

err_handle:
    tearDownThreadWakeupPipe();
    mWorkerThread = THREAD_NOT_STARTED;
    return false;
}

// stop worker thread, and wait for it to exit
void PhysicalActivitySensor::stopWorker()
{
    if (mWorkerThread == THREAD_NOT_STARTED) {
        LOGI("Stop worker thread while thread is not running");
        return;
    }

    if (!isThreadWakeupPipeSetup()) {
        LOGI("Wake up fd invalid");
        return;
    }

    // notify thread to exit
    write(mWakeFDs[1], "a", 1);
    pthread_join(mWorkerThread, NULL);
    LOGI("Worker Thread Ended");

    // reset status
    tearDownThreadWakeupPipe();
    mWorkerThread = THREAD_NOT_STARTED;
}

bool PhysicalActivitySensor::restartWorker(int64_t newDelay)
{
    {
        Mutex::Autolock _l(mDelayMutex);
        mCurrentDelay = newDelay;
    }
    stopWorker();
    return startWorker();
}

int PhysicalActivitySensor::enable(int32_t handle, int en) {
    int flags = en ? 1 : 0;
    int ret;

    LOGI("PhysicalActivitySensor - %s - enable=%d", __FUNCTION__, en);
    threadID();

    if (!isConnectToPSH() || !isResultPipeSetup()) {
        LOGE("Invalid status while enable");
        return -1;
    }

    if (mEnabled == en) {
        LOGI("Duplicate request");
        return -1;
    }

    mEnabled = en;

    if (0 == mEnabled) {
        stopWorker();
        mCurrentDelay = 0;
    }  //  we start worker when setDelay

    return 0;
}

int PhysicalActivitySensor::setDelay(int32_t handle, int64_t ns)
{
    LOGI("setDelay - %s - %lld", __FUNCTION__, ns);
    threadID();

    if (ns != SENSOR_DELAY_TYPE_PHYSICAL_ACTIVITY_INSTANT * 1000 &&
        ns != SENSOR_DELAY_TYPE_PHYSICAL_ACTIVITY_QUICK * 1000 &&
        ns != SENSOR_DELAY_TYPE_PHYSICAL_ACTIVITY_NORMAL * 1000 &&
        ns != SENSOR_DELAY_TYPE_PHYSICAL_ACTIVITY_STATISTIC * 1000) {
        LOGW("Delay not supported");
        return -1;
    }

    if (ns == SENSOR_DELAY_TYPE_PHYSICAL_ACTIVITY_INSTANT * 1000 &&
        !isAlgorithmLoaded()) {
        LOGE("Instant mode without algorithm, fail!!!!!");
        return -1;
    }

    if (!isConnectToPSH() || !isResultPipeSetup()) {
        LOGE("Invalid status while enable");
        return -1;
    }

    if (mEnabled == 0) {
        LOGW("setDelay while not enabled");
        return -1;
    }

    if (mCurrentDelay == ns) {
        LOGI("Setting the same delay, do nothing");
        return 0;
    }

    // Restart worker thread
    if (restartWorker(ns)) {
        return 0;
    } else {
        mCurrentDelay = 0;
        return -1;
    }
}

bool PhysicalActivitySensor::hasPendingEvents() const
{
    return mHasPendingEvent;
}

void PhysicalActivitySensor::threadID()
{
    LOGD("Thread Id is %x", (unsigned int)(android::getThreadId()));
}

int PhysicalActivitySensor::ActCB(void *ctx, short *results, int len)
{
    PhysicalActivitySensor *src = (PhysicalActivitySensor*)ctx;
    src->mPSHCn = results[0] & 0xF;

    // report out final value
    int report = getPA(src->mPSHCn);
    write(src->mResultPipe[1], &report, sizeof(report));

    return 0;
}

int PhysicalActivitySensor::getPA(short result)
{
    switch (result) {
        case WALKING:
            return SENSOR_EVENT_TYPE_PHYSICAL_ACTIVITY_WALKING;
        case RUNNING:
            return SENSOR_EVENT_TYPE_PHYSICAL_ACTIVITY_RUNNING;
        case RAND:
            return SENSOR_EVENT_TYPE_PHYSICAL_ACTIVITY_RANDOM;
        case SED:
            return SENSOR_EVENT_TYPE_PHYSICAL_ACTIVITY_SEDENTARY;
        default:
            return SENSOR_EVENT_TYPE_PHYSICAL_ACTIVITY_RANDOM;
    }
}

int PhysicalActivitySensor::getPADelay(int64_t ns)
{
    int paDelay = 0;

    if (SENSOR_DELAY_TYPE_PHYSICAL_ACTIVITY_QUICK * 1000 == ns) {
        paDelay = static_cast<int>(PA_QUICK/PA_INTERVAL);
    } else if (SENSOR_DELAY_TYPE_PHYSICAL_ACTIVITY_NORMAL * 1000 == ns) {
        paDelay = static_cast<int>(PA_NORMAL/PA_INTERVAL);
    } else if (SENSOR_DELAY_TYPE_PHYSICAL_ACTIVITY_STATISTIC * 1000 == ns) {
        paDelay = static_cast<int>(PA_STATISTIC/PA_INTERVAL);
    } else {
        // shouldn't reach here
        paDelay = 1;
    }

    return paDelay;
}

void* PhysicalActivitySensor::workerThread(void *data)
{
    PhysicalActivitySensor *src = (PhysicalActivitySensor*) data;

    int psh_fd = -1;
    bool instantMode = false;
    short accel[3];
    struct phy_activity_data actData;
    Client *client = NULL;
    Client *tmpClient = NULL;
    // Get current delay
    int64_t delay;
    {
        Mutex::Autolock _l(src->mDelayMutex);
        delay = src->mCurrentDelay;
    }

    if (SENSOR_DELAY_TYPE_PHYSICAL_ACTIVITY_INSTANT * 1000 == delay)
        instantMode = true;

    // start streaming and get read fd
    if (instantMode) {
        // instant mode, open accelerometer stream
        if (psh_start_streaming(src->mAccHandle, 100, 10) != ERROR_NONE) {
            LOGE("psh_start_streaming failed.");
            usleep(SLEEP_ON_FAIL_USEC);
            if (psh_start_streaming(src->mAccHandle, 100, 10) != ERROR_NONE) {
                LOGE("psh_start_streaming failed.");
                return NULL;
            }
        }
        psh_fd = psh_get_fd(src->mAccHandle);
    } else {
        // none-instant mode, open physical activity stream
        // parameters 1 and 0 are just place holders
        int paDelay = getPADelay(delay);
        int param = (paDelay << 16) | paDelay;
        if (psh_set_property(src->mPAHandle,
                             PROP_ACT_N, &param) != ERROR_NONE) {
            LOGE("psh_set_property n failed.");
            return NULL;
        }
        if (psh_start_streaming(src->mPAHandle, 1, 0) != ERROR_NONE) {
            LOGE("psh_start_streaming failed.");
            usleep(SLEEP_ON_FAIL_USEC);
            if (psh_start_streaming(src->mPAHandle, 1, 0) != ERROR_NONE) {
                LOGE("psh_start_streaming failed.");
                return NULL;
            }
        }

        // create decorators
        tmpClient = new NCycleClient(paDelay);
        client = new ClientSummarizer(tmpClient);

        psh_fd = psh_get_fd(src->mPAHandle);
    }

    // set up polling fds
    struct pollfd polls[2];
    polls[0].fd = src->mWakeFDs[0];
    polls[0].events = POLLIN;
    polls[0].revents = 0;
    polls[1].fd = psh_fd;
    polls[1].events = POLLIN;
    polls[1].revents = 0;

    // if instant mode, initialize algorithm
    if (instantMode) {
        (*src->mActivityInstantInit)(ActCB, data);
    }

    // let's poll
    while (poll(polls, 2, -1) > 0) {
        if ((polls[0].revents & POLLIN) != 0) {
            LOGI("Wake FD Polled.");
            break;
        }

        if (instantMode) {
            if (read(psh_fd, accel, sizeof(accel)) != sizeof(accel)) {
                LOGE("Unexpected Read End");
                break;
            }
            if ((*src->mActivityInstantCollectData)(accel[0], accel[1], accel[2])) {
                LOGI("Process");
                (*src->mActivityInstantProcess)();
            }
        } else {
            if (read(psh_fd, &actData.len,
                     sizeof(actData.len)) != sizeof(actData.len)) {
                LOGE("Unexpected Read End");
                break;
            }
            if (actData.len < 1
                || actData.len > (int)(sizeof(actData.values)/sizeof(actData.values[0]))) {
                LOGE("Invalid Len %hd", actData.len);
                continue;
            }
            if (read(psh_fd, actData.values, sizeof(actData.values[0]) * actData.len) !=
                (int)sizeof(actData.values[0]) * actData.len) {
                LOGE("Physical Activity read end");
                break;
            }

            // publish result
            if (client->accept(actData.values, actData.len)) {
                int finalResult;
                client->publish(finalResult);
                // write to result pipe
                write(src->mResultPipe[1], &finalResult, sizeof(finalResult));
            }
        }
    }


    // close streaming
    handle_t pshSession;
    if (instantMode) {
        pshSession = src->mAccHandle;
    } else {
        pshSession = src->mPAHandle;
    }
    if (pshSession != PSH_SESSION_NOT_OPENED) {
        if (psh_stop_streaming(pshSession) != ERROR_NONE) {
            LOGE("psh_stop_streaming failed.");
            usleep(SLEEP_ON_FAIL_USEC);
            if (psh_stop_streaming(pshSession) != ERROR_NONE) {
                LOGE("psh_stop_streaming failed.");
            }
        }
    }

    if (client != NULL) {
       delete client;
       client = NULL;
    }

    pthread_exit(NULL);
    return NULL;
}

int PhysicalActivitySensor::getFd() const
{
    LOGI("Fd is %d, %d", mResultPipe[0], mResultPipe[1]);
    threadID();
    return mResultPipe[0];
}

int PhysicalActivitySensor::readEvents(sensors_event_t* data, int count)
{
    int size, numEventReceived = 0;
    char buf[512];
    int *p_activity_data;
    int unit_size = sizeof(*p_activity_data);

    LOGI("PhysicalActivitySensor - readEvents");
    threadID();
    if (count < 1)
        return -EINVAL;

    if (!isResultPipeSetup()) {
        LOGI("invalid status ");
        return 0;
    }

    if ((unit_size * count) <= 512)
        size = unit_size * count;
    else
        size = (512 / unit_size) * unit_size;

    LOGI("Try to read %d", size);
    size = read(mResultPipe[0], buf, size);
    LOGI("Actually read %d", size);

    char *p = buf;

    while (size > 0) {
        p_activity_data = (int *)p;
        mPendingEvent.data[0] = (static_cast<float>(*p_activity_data));
        LOGI("Event value is %d", *p_activity_data);

        mPendingEvent.timestamp = getTimestamp();

        if (mEnabled == 1) {
           *data++ = mPendingEvent;
            numEventReceived++;
        }

        size = size - unit_size;
        p = p + unit_size;
    }

    D("PhysicalActivitySensor - read %d events", numEventReceived);
    return numEventReceived;
}

// decorator related below

#define RAND 6
#define INIT_WEIGHT 1024

// the result coming from psh contains two portions
// xxxx xxxx xxxx xxxx
// xxxx xxxx xxxx represents sequence number
//                         xxxx represents activity type

// get sequence number
#define SN(a) ((a)>>4)

// get activity type
#define CN(a) ((a)&0xF)

// put sequence number and activity type together
#define SNCN(a,b) (((a)<<4)|(b))

PhysicalActivitySensor::Client::~Client()
{
}

void PhysicalActivitySensor::Client::publish(int &result)
{
    std::stringstream ss;
    if ((int)mStream.size() < N)
        return;
    int cn = 0;
    for (int i = 0; i < N; ++i) {
        cn = CN(mStream[i]);
        LOGI("publish %d", cn);
    }

    switch (cn) {
        case BIKING:
            result = SENSOR_EVENT_TYPE_PHYSICAL_ACTIVITY_BIKING;
            break;
        case WALKING:
            result = SENSOR_EVENT_TYPE_PHYSICAL_ACTIVITY_WALKING;
            break;
        case RUNNING:
            result = SENSOR_EVENT_TYPE_PHYSICAL_ACTIVITY_RUNNING;
            break;
        case INCAR:
            result = SENSOR_EVENT_TYPE_PHYSICAL_ACTIVITY_DRIVING;
            break;
        case RAND:
            result = SENSOR_EVENT_TYPE_PHYSICAL_ACTIVITY_RANDOM;
            break;
        case SED:
            result = SENSOR_EVENT_TYPE_PHYSICAL_ACTIVITY_SEDENTARY;
            break;
        default:
            result = SENSOR_EVENT_TYPE_PHYSICAL_ACTIVITY_RANDOM;
    }
    reduce();
}

void PhysicalActivitySensor::Client::reduce(void)
{
    for (int i = 0; i < N; ++i) {
        mStream.pop_front();
    }
}

void PhysicalActivitySensor::Client::reset(void)
{
    mStream.clear();
}

////////////////////////////////////////////////////////////////
#define SWIP -1
PhysicalActivitySensor::NCycleClient::NCycleClient(int n)
{
    N = n;
}

PhysicalActivitySensor::NCycleClient::~NCycleClient()
{
    mStream.clear();
}

bool PhysicalActivitySensor::NCycleClient::accept(short *item, int len)
{
    int s = mStream.size();
    if (s > 0) {
        int lastsn = SN(mStream[s - 1]);
        int nextsn = SN(*item);
        if (nextsn != lastsn + 1) {
            mStream.clear();
            s = 0;
        }
    }
    while (len > 0 && (SN(*item) - s) % N != 0) {
        ++item;
        --len;
    }
    for (int i = 0; i < len; ++i) {
        mStream.push_back(item[i]);
    }
    s = mStream.size();
    return s >= N;
}


////////////////////////////////////////////////////////////////
PhysicalActivitySensor::ClientSummarizer::ClientSummarizer(const Client * c)
{
    N = 1;
    mWeights = new int[c->N];
    int w = INIT_WEIGHT;
    // mWeight[i] = mWeight[i-1] * 2^-swip
    for (int i = 0; i < c->N; ++i) {
        mWeights[i] = w;
        w += SWIP == -1 ? 0 : (w >> SWIP);
    }
    mClient = const_cast<Client *>(c);
}

PhysicalActivitySensor::ClientSummarizer::~ClientSummarizer()
{
    mStream.clear();
    delete [] mWeights;
    mWeights = NULL;
    delete mClient;
    mClient = NULL;
}

#define ACTNUM 8

bool PhysicalActivitySensor::ClientSummarizer::accept(short *item, int len)
{
    int scores[ACTNUM];
    int indice[ACTNUM];
    int orders[ACTNUM];
    int sum = 0;
    int sn = 0;
    memset(scores, 0, sizeof(scores));

    if (!mClient->accept(item, len)) {
        return false;
    }

    if (mClient->mStream.size() == 0)
        return false;

    sn = SN(mClient->mStream[0]);

    for (int i = 0; i < mClient->N; ++i) {
        int cn = CN(mClient->mStream[i]);
        if (cn < 0 || cn >= ACTNUM)
            return false;
        scores[cn] += mWeights[i];
        sum += mWeights[i];
    }
    // sort by indice of each cn
    for (int i = 0; i < ACTNUM; ++i) {
        indice[i] = i;
    }
    for (int i = 0; i < ACTNUM; ++i) {
        int max = indice[i];
        int maxinx = i;
        for (int j = i + 1; j < ACTNUM; ++j) {
            if ( scores[max] < scores[ indice[j] ] ) {
                max = indice[j];
                maxinx = j;
            }
        }
        indice[maxinx] = indice[i];
        indice[i] = max;
    }
    // assign the sorting ranks to each cn
    for (int i = 0; i < ACTNUM; ++i) {
        orders[ indice[i] ] = i;
    }
    // threshold: 50%
    int sumper = sum >> 1;
    if (scores[ indice[0] ] > sumper) {
        // upon threshold, then final cn is the one with the greatest score
        mStream.push_back(SNCN(sn, indice[0]));
    } else {
        // below threshold, then final cn is random
        mStream.push_back(SNCN(sn, RAND));
    }
    return true;
}

void PhysicalActivitySensor::ClientSummarizer::reduce(void)
{
    mStream.clear();
    mClient->reduce();
}

void PhysicalActivitySensor::ClientSummarizer::reset(void)
{
    mStream.clear();
    mClient->reset();
}
