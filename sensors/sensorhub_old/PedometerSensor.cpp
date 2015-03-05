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
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/select.h>
#include <assert.h>

#include <cutils/log.h>
#include <utils/AndroidThreads.h>
#include <libsensorhub.h>
#include "PedometerSensor.h"

/*****************************************************************************/
#undef LOG_TAG
#define LOG_TAG "PedometerSensor"

#define SLEEP_ON_FAIL_USEC 500000
#define PEDO_RATE 1
#define PEDO_BUFD 0
/*This flag makes pedometer context
  continue computing but not report
  when the screen is off
 */
#define PEDO_FLAG NO_STOP_NO_REPORT_WHEN_SCREEN_OFF

#define PEDO_QUICK 4
#define PEDO_NORMAL 32
#define PEDO_STATISTIC 256

#define THREAD_NOT_STARTED 0
#define PIPE_NOT_OPENED -1
#define PSH_SESSION_NOT_OPENED NULL

PedometerSensor::PedometerSensor()
    : SensorBase("pedometer"),
      mEnabled(0),
      mHasPendingEvent(false)
{
    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = SENSORS_HANDLE_PEDOMETER;
    mPendingEvent.type = SENSOR_TYPE_PEDOMETER;
    memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));

    mResultPipe[0] = PIPE_NOT_OPENED;
    mResultPipe[1] = PIPE_NOT_OPENED;

    mWakeFDs[0] = PIPE_NOT_OPENED;
    mWakeFDs[1] = PIPE_NOT_OPENED;

    mCurrentDelay = 0;
    mWorkerThread = THREAD_NOT_STARTED;

    mPedoHandle = PSH_SESSION_NOT_OPENED;

    // set up psh connection
    connectToPSH();
    // Establish result pipe to sensor HAL
    setupResultPipe();
}

PedometerSensor::~PedometerSensor()
{
    LOGI("~PedomterSensor %d\n", mEnabled);

    stopWorker();
    // close connection
    disconnectFromPSH();
    // close pipes
    tearDownResultPipe();
}

inline void PedometerSensor::connectToPSH()
{
    assert(!isConnectToPSH());
    // Establish physical activity connection to PSH
    mPedoHandle = psh_open_session(SENSOR_PEDOMETER);
    if (mPedoHandle == PSH_SESSION_NOT_OPENED) {
        LOGE("psh_open_session failed. retry once");
        usleep(SLEEP_ON_FAIL_USEC);
        mPedoHandle = psh_open_session(SENSOR_PEDOMETER);
        if (mPedoHandle == PSH_SESSION_NOT_OPENED) {
            LOGE("psh_open_session failed.");
            return;
        }
    }
}

inline bool PedometerSensor::isConnectToPSH()
{
    if (mPedoHandle != PSH_SESSION_NOT_OPENED)
        return true;
    else
        return false;
}

inline void PedometerSensor::disconnectFromPSH()
{
    // close connections
    if (mPedoHandle != PSH_SESSION_NOT_OPENED)
        psh_close_session(mPedoHandle);
    mPedoHandle = PSH_SESSION_NOT_OPENED;
}

inline bool PedometerSensor::setupResultPipe()
{
    assert(!isResultPipeSetup());
    pipe(mResultPipe);
    return isResultPipeSetup();
}

inline bool PedometerSensor::isResultPipeSetup()
{
    if (mResultPipe[0] != PIPE_NOT_OPENED
        && mResultPipe[1] != PIPE_NOT_OPENED)
        return true;
    else
        return false;
}

inline void PedometerSensor::tearDownResultPipe()
{
    if (mResultPipe[0] != PIPE_NOT_OPENED)
        close(mResultPipe[0]);

    if (mResultPipe[1] != PIPE_NOT_OPENED)
        close(mResultPipe[1]);

    mResultPipe[0] = mResultPipe[1] = PIPE_NOT_OPENED;
}

inline bool PedometerSensor::setUpThreadWakeupPipe()
{
    assert(!isThreadWakeupPipeSetup());
    pipe(mWakeFDs);
    return isThreadWakeupPipeSetup();
}

inline bool PedometerSensor::isThreadWakeupPipeSetup()
{
    if (mWakeFDs[0] != PIPE_NOT_OPENED
        && mWakeFDs[1] != PIPE_NOT_OPENED)
        return true;
    else
        return false;
}

inline void PedometerSensor::tearDownThreadWakeupPipe()
{
    if (mWakeFDs[0] != PIPE_NOT_OPENED)
        close(mWakeFDs[0]);

    if (mWakeFDs[1] != PIPE_NOT_OPENED)
        close(mWakeFDs[1]);

    mWakeFDs[0] = mWakeFDs[1] = PIPE_NOT_OPENED;
}


// stop worker thread, and wait for it to exit
void PedometerSensor::stopWorker()
{
    if (mWorkerThread == THREAD_NOT_STARTED) {
        LOGI("Stop worker thread while thread is not running");
        return;
    }

    // when thread is active, the wake up pipes should be valid
    assert(isThreadWakeupPipeSetup());

    // notify thread to exit
    write(mWakeFDs[1], "a", 1);
    pthread_join(mWorkerThread, NULL);
    LOGI("Worker Thread Ended");

    // reset status
    tearDownThreadWakeupPipe();
    mWorkerThread = THREAD_NOT_STARTED;
}

// start worker thread
bool PedometerSensor::startWorker()
{
    if (!isConnectToPSH() || !isResultPipeSetup()) {
        LOGE("Invalid connections to PSH or bad result pipe");
        return false;
    }

    // set up wake up pipes
    if (!setUpThreadWakeupPipe())
        goto error_handle;

    if (0 == pthread_create(&mWorkerThread, NULL,
                                    workerThread, this)) {
        LOGI("create thread succeed");
        return true;
    } else {
        LOGE("create thread failed");
    }
error_handle:
    tearDownThreadWakeupPipe();
    mWorkerThread = THREAD_NOT_STARTED;
    return false;
}

bool PedometerSensor::restartWorker(int64_t newDelay)
{
    {
        Mutex::Autolock _l(mDelayMutex);
        mCurrentDelay = newDelay;
    }
    stopWorker();
    return startWorker();
}

int PedometerSensor::enable(int32_t handle, int en)
{
    int flags = en ? 1 : 0;
    int ret;

    LOGI("PedomterSensor - %s - enable=%d", __FUNCTION__, en);
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
    }  // we start worker when setDelay

    return 0;
}

int PedometerSensor::setDelay(int32_t handle, int64_t ns)
{
    LOGI("setDelay - %s - %lld", __FUNCTION__, ns);
    threadID();

    if (ns != SENSOR_DELAY_TYPE_PEDOMETER_INSTANT * 1000 &&
        ns != SENSOR_DELAY_TYPE_PEDOMETER_QUICK * 1000 &&
        ns != SENSOR_DELAY_TYPE_PEDOMETER_NORMAL * 1000 &&
        ns != SENSOR_DELAY_TYPE_PEDOMETER_STATISTIC * 1000) {
        LOGW("Delay not supported");
        return -1;
    }

    if (!isConnectToPSH() || !isResultPipeSetup()) {
        LOGE("Invalid status while setDelay");
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

bool PedometerSensor::hasPendingEvents() const
{
    return mHasPendingEvent;
}

void PedometerSensor::threadID()
{
    LOGD("Thread Id is %x", (unsigned int)android::getThreadId());
}

int PedometerSensor::getPedoDelay(int64_t ns)
{
    int pedoDelay = 0;

    if (SENSOR_DELAY_TYPE_PEDOMETER_QUICK * 1000 == ns) {
        pedoDelay = PEDO_QUICK;
    } else if (SENSOR_DELAY_TYPE_PEDOMETER_NORMAL * 1000 == ns) {
        pedoDelay = PEDO_NORMAL;
    } else if (SENSOR_DELAY_TYPE_PEDOMETER_STATISTIC * 1000 == ns) {
        pedoDelay = PEDO_STATISTIC;
    } else {
        // shouldn't reach here
        pedoDelay = 1;
    }

    return pedoDelay;
}

void* PedometerSensor::workerThread(void *data)
{
    PedometerSensor *src = (PedometerSensor*) data;

    int psh_fd = -1;
    bool instantMode = false;

    struct pedometer_data pedoData;
    // Get current delay
    int64_t delay;
    {
        Mutex::Autolock _l(src->mDelayMutex);
        delay = src->mCurrentDelay;
    }

    if (SENSOR_DELAY_TYPE_PEDOMETER_INSTANT * 1000 == delay)
        instantMode = true;

    // start streaming and get read fd
    if (instantMode) {
        int property = PROP_PEDO_MODE_ONCHANGE;
        if (psh_set_property(src->mPedoHandle,
                             PROP_PEDOMETER_MODE, &property) != ERROR_NONE) {
            LOGE("psh_set_property mode failed.");
            return NULL;
        }
    } else {
        int property = PROP_PEDO_MODE_NCYCLE;
        if (psh_set_property(src->mPedoHandle,
                             PROP_PEDOMETER_MODE, &property) != ERROR_NONE) {
            LOGE("psh_set_property mode failed.");
            return NULL;
        }

        int pedoDelay = getPedoDelay(delay);
        if (psh_set_property(src->mPedoHandle,
                             PROP_PEDOMETER_N, &pedoDelay) != ERROR_NONE) {
            LOGE("psh_set_property n failed.");
            return NULL;
        }
    }

    // parameters 1 and 0 are just place holders
    if (psh_start_streaming_with_flag(src->mPedoHandle,
        1, 0, PEDO_FLAG) != ERROR_NONE) {
        LOGE("psh_start_streaming_with_flag failed.");
        usleep(SLEEP_ON_FAIL_USEC);
        if (psh_start_streaming_with_flag(src->mPedoHandle,
            1, 0, PEDO_FLAG) != ERROR_NONE) {
            LOGE("psh_start_streaming_with_flag failed.");
            return NULL;
        }
    }

    psh_fd = psh_get_fd(src->mPedoHandle);

    // set up polling fds
    struct pollfd polls[2];
    polls[0].fd = src->mWakeFDs[0];
    polls[0].events = POLLIN;
    polls[0].revents = 0;
    polls[1].fd = psh_fd;
    polls[1].events = POLLIN;
    polls[1].revents = 0;

    // let's poll
    while (poll(polls, 2, -1) > 0) {
        if ((polls[0].revents & POLLIN) != 0) {
            LOGI("Wake FD Polled.");
            break;
        }

        struct pedometer_data pedoData;
        int size = read(psh_fd, &pedoData, sizeof(pedoData));

        if (size != sizeof(pedoData)) {
            LOGE("Read End Unexpectedly, Size: %d", size);
            break;
        }
        int finalResult = pedoData.num;
        LOGI("Update %d", finalResult);

        // write to result pipe
        write(src->mResultPipe[1], &finalResult, sizeof(finalResult));
    }

    // close streaming
    if (src->mPedoHandle != NULL) {
        if (psh_stop_streaming(src->mPedoHandle) != ERROR_NONE) {
            LOGE("psh_stop_streaming failed.");
            usleep(SLEEP_ON_FAIL_USEC);
            if (psh_stop_streaming(src->mPedoHandle) != ERROR_NONE) {
                LOGE("psh_stop_streaming failed.");
            }
        }
    }
    pthread_exit(NULL);
    return NULL;
}

int PedometerSensor::getFd() const
{
    LOGI("Fd is %d, %d", mResultPipe[0], mResultPipe[1]);
    threadID();
    return mResultPipe[0];
}

int PedometerSensor::readEvents(sensors_event_t* data, int count)
{
    int size, numEventReceived = 0;
    char buf[512];
    int *p_pedometer_data;
    int unit_size = sizeof(*p_pedometer_data);

    LOGI("PedomterSensor - readEvents");
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
        p_pedometer_data = (int *)p;
        mPendingEvent.data[0] = (static_cast<float>(*p_pedometer_data));
        LOGI("Event value is %d", *p_pedometer_data);

        mPendingEvent.timestamp = getTimestamp();

        if (mEnabled == 1) {
           *data++ = mPendingEvent;
            numEventReceived++;
        }

        size = size - unit_size;
        p = p + unit_size;
    }

    D("PedomterSensor - read %d events", numEventReceived);
    return numEventReceived;
}
