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
#include "GestureSensor.h"

#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/select.h>

#include <cutils/log.h>
#include <utils/AndroidThreads.h>

#include "gesture.h"

/*****************************************************************************/

#undef LOG_TAG
#define LOG_TAG "GestureSensor"

#define GS_BUF_SIZE     4096    /* gesture spotter buffer size */
#define PX_BUF_SIZE     512     /* proximity buffer size */
#define GS_SAMPLE_RATE  100     /* gesture spotter sampling rate, Hz */
#define GS_BUF_DELAY    0       /* gesture spotter buffer delay, ms */
#define PX_SAMPLE_RATE  5       /* proximity sampling rate, Hz */
#define PX_BUF_DELAY    0       /* proximity buffer delay, ms */
#define GS_DATA_LENGTH  6       /* length of g_spotter single data */

#define THREAD_NOT_STARTED 0
#define PIPE_NOT_OPENED -1
#define PSH_SESSION_NOT_OPENED NULL
#define INVALID_GESTURE_RESULT -1

GestureSensor::GestureSensor()
    : SensorBase("gesture"),
      mEnabled(0),
      mHasPendingEvent(false)
{
    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = SENSORS_HANDLE_GESTURE;
    mPendingEvent.type = SENSOR_TYPE_GESTURE;
    memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));

    mPollingThreadIDGyro= THREAD_NOT_STARTED;
    mPollingThreadIDProximity = THREAD_NOT_STARTED;
    mPollingThreadIDAccel = THREAD_NOT_STARTED;
    mFlagProximity = false;
    mHandleGyro= PSH_SESSION_NOT_OPENED;
    mHandleProximity = PSH_SESSION_NOT_OPENED;
    mHandleAccel = PSH_SESSION_NOT_OPENED;
    mWakeFdsGyro[0] = mWakeFdsGyro[1] = PIPE_NOT_OPENED;
    mWakeFdsProximity[0] = mWakeFdsProximity[1] = PIPE_NOT_OPENED;
    mWakeFdsAccel[0] = mWakeFdsAccel[1] = PIPE_NOT_OPENED;
    mResultPipe[0] = mResultPipe[1] = PIPE_NOT_OPENED;
    mInitGesture = false;

    // Establish result pipe to sensor HAL
    pipe(mResultPipe);

    // Start to load libgesture library
    mLibraryHandle = NULL;
    mGestureInit = NULL;
    mGestureProcessSingleData = NULL;
    mGestureClose = NULL;
    mHasGestureLibrary = false;

    mLibraryHandle = dlopen(LIBGESTURE, RTLD_NOW);
    if (mLibraryHandle != NULL) {
        mGestureInit = (FUNC_GESTURE_INIT) dlsym(mLibraryHandle,
                                                SYMBOL_GESTURE_INIT);
        mGestureProcessSingleData = (FUNC_GESTURE_PROCESS_SINGLE_DATA) dlsym(mLibraryHandle,
                                                SYMBOL_GESTURE_PROCESS_SINGLE_DATA);
        mGestureClose = (FUNC_GESTURE_CLOSE) dlsym(mLibraryHandle,
                                                SYMBOL_GESTURE_CLOSE);

        if (mGestureInit != NULL && mGestureProcessSingleData != NULL && mGestureClose != NULL) {
            mHasGestureLibrary = true;
            LOGI("Got all required functions");
        } else {
            LOGE("Can't get all required functions!!");
        }
    } else {
        LOGE("Can't find libgesture!!");
    }
}

GestureSensor::~GestureSensor()
{
    LOGI("~GestureSensor %d\n", mEnabled);
    Stop_accel();
    Stop_gyro();
    Stop_proximity();

    if (mResultPipe[0] != PIPE_NOT_OPENED)
        close(mResultPipe[0]);

    if (mResultPipe[1] != PIPE_NOT_OPENED)
        close(mResultPipe[1]);

    if (mLibraryHandle != NULL) {
        dlclose(mLibraryHandle);
    }
}

int GestureSensor::enable(int32_t handle, int en)
{
    int flags = en ? 1 : 0;

    LOGI("GestureSensor - %s - enable=%d", __FUNCTION__, en);
    threadID();
    if (!mHasGestureLibrary) {
        LOGE("Gesture library error!");
        return -1;
    }

    if (mResultPipe[0] == PIPE_NOT_OPENED || mResultPipe[1] == PIPE_NOT_OPENED)
        return -1;

    if (mEnabled == en)
        return 0;

    if (en == 1) {
        bool proximityThreadStarted = Start_proximity();
        bool gyroThreadStarted = Start_gyro();
        bool acclThreadStarted = Start_accel();

        if (!acclThreadStarted || !proximityThreadStarted || !gyroThreadStarted) {
            LOGE("Failed to start gesture or proximity thread");
            Stop_accel();
            Stop_gyro();
            Stop_proximity();
            return -1;
        }
        mEnabled = 1;
    } else {
        Stop_accel();
        Stop_gyro();
        Stop_proximity();
        mEnabled = 0;
    }

    return 0;
}

int GestureSensor::setDelay(int32_t handle, int64_t ns)
{
    LOGI("setDelay - %s - %lld", __FUNCTION__, ns);
    threadID();
    return 0;
}

bool GestureSensor::hasPendingEvents() const
{
    return mHasPendingEvent;
}

int GestureSensor::getFd() const
{
    LOGI("Fd is %d, %d", mResultPipe[0], mResultPipe[1]);
    threadID();
    return mResultPipe[0];
}

int GestureSensor::readEvents(sensors_event_t* data, int count)
{
    int size, numEventReceived = 0;
    char buf[512];
    int *p_gesture_data;
    int unit_size = sizeof(*p_gesture_data);

    LOGI("GestureSensor - readEvents");
    threadID();
    if (count < 1)
        return -EINVAL;

    if (mResultPipe[0] == PIPE_NOT_OPENED) {
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
        p_gesture_data = (int *)(p);
        mPendingEvent.data[0] = (static_cast<float>(*p_gesture_data));
        LOGI("Event value is %d", *p_gesture_data);

        mPendingEvent.timestamp = getTimestamp();

        if (mEnabled == 1) {
           *data++ = mPendingEvent;
            numEventReceived++;
        }

        size = size - unit_size;
        p = p + unit_size;
    }

    D("GestureSensor - read %d events", numEventReceived);
    return numEventReceived;
}

static const char * gestures[] = {
    "NumberOne ", "NumberTwo ", "NumberThree ", "NumberFour ",
    "NumberFive ", "NumberSix ", "NumberSeven ", "NumberEight ",
    "NumberNine ", "NumberZero ", "EarTouch ", "EarTouchBack "
};

int GestureSensor::getGestureFromString(char *gesture)
{
    int index = -1;
    for (int i = 0; i < (static_cast<int>(ARRAY_SIZE(gestures))); i++) {
        if (strcmp(gesture, gestures[i]) == 0) {
            index = i;
            break;
        }
    }

    if (index != -1)
        return SENSOR_EVENT_TYPE_GESTURE_NUMBER_ONE + index;
    else
        return INVALID_GESTURE_RESULT;
}

void GestureSensor::threadID()
{
    LOGD("Thread Id is %x", (unsigned int)android::getThreadId());
}

/* Additional Method */
/* thread to process raw accel and gyro data with libgesture */
void* GestureSensor::PollingThread_accel(void *source)
{
    LOGD("context thread start: wait for spotter data");
    GestureSensor* tSrc = static_cast<GestureSensor*>(source);

    int fd = psh_get_fd(tSrc->mHandleAccel);
    char* buf = new char[GS_BUF_SIZE];
    int size = 0;
    struct accel_data *p_accel_data;
    fd_set listen_fds, read_fds;
    int ret, max_fd;

    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);
    FD_SET(tSrc->mWakeFdsAccel[0], &read_fds);

    if (fd > tSrc->mWakeFdsAccel[0])
        max_fd = fd;
    else
        max_fd = tSrc->mWakeFdsAccel[0];

    while ((ret = (select(max_fd +1, &read_fds, NULL, NULL, NULL)) >= 0)) {
        /* if sensor is shut down, return */
        if (FD_ISSET(tSrc->mWakeFdsAccel[0], &read_fds)) {
            LOGD("context thread end - gesture");
            break;
        }
        /* get accel sensor data */
        size = read(fd, buf, GS_BUF_SIZE);
        char *p = buf;
        p_accel_data = (struct accel_data *)buf;
        while (size > 0) {
            short data[6];
            data[0] = p_accel_data->x;
            data[1] = p_accel_data->y;
            data[2] = p_accel_data->z;
            tSrc->mMutexGyro.lock();
                data[3] = tSrc->mDataGyro[0];
                data[4] = tSrc->mDataGyro[1];
                data[5] = tSrc->mDataGyro[2];
            tSrc->mMutexGyro.unlock();
            //LOGD("%hd %hd %hd %hd %hd %hd", data[0], data[1], data[2], data[3], data[4], data[5]);

            /* process with libgesture */
            /* CAUTION: this function is not multi-thread safe */
            tSrc->mMutexAccel.lock();
                char *gesture = (*tSrc->mGestureProcessSingleData)(data, false, false);
            tSrc->mMutexAccel.unlock();

            /* if gesture is detected */
            if (gesture != NULL) {
                LOGD("-- gesture: %s", gesture);
                /* change EarTouchL to EarTouch, same as EarTouchLBack */
                if (strcmp(gesture, "EarTouchL ") == 0)
                    strcpy(gesture, "EarTouch ");
                else if (strcmp(gesture, "EarTouchLBack ") == 0)
                    strcpy(gesture, "EarTouchBack ");
                tSrc->mMutexProximity.lock();
                    bool f1 = tSrc->mFlagProximity;
                tSrc->mMutexProximity.unlock();
                bool f2 = (strcmp(gesture, "EarTouch ") == 0);
                /* eartouch end with prox = 1, others end with prox = 0 */
                if ((f1 && f2) || ((!f1) && (!f2))) {
                    int gestureResult = getGestureFromString(gesture);
                    if (gestureResult != INVALID_GESTURE_RESULT) {
                        LOGI("write into result pipe start");
                        write(tSrc->mResultPipe[1], (char*)(&gestureResult),
                              sizeof(gestureResult));
                        LOGI("write into result pipe end");
                    }
                }
            }
            delete [] gesture;
            size = size - sizeof(struct accel_data);
            p = p + sizeof(struct accel_data);
            p_accel_data = (struct accel_data *)p;
        }
        FD_SET(fd, &read_fds);
        FD_SET(tSrc->mWakeFdsAccel[0], &read_fds);
    }
    delete [] buf;
    pthread_exit(NULL);
    return NULL;
}

/* thread to process proximity data */
void* GestureSensor::PollingThread_proximity(void *source)
{
    LOGD("context thread start: wait for proximity data");
    GestureSensor* termSrc = static_cast<GestureSensor*>(source);

    int fd = psh_get_fd(termSrc->mHandleProximity);
    char* buf = new char[PX_BUF_SIZE];
    int size = 0;
    struct ps_phy_data *p_ps_phy_data;
    fd_set listen_fds, read_fds;
    int ret, max_fd;

    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);
    FD_SET(termSrc->mWakeFdsProximity[0], &read_fds);

    if (fd > termSrc->mWakeFdsProximity[0])
        max_fd = fd;
    else
        max_fd = termSrc->mWakeFdsProximity[0];

    while ((ret = (select(max_fd +1, &read_fds, NULL, NULL, NULL)) >= 0)) {
        /* if sensor is shut down, return */
        if (FD_ISSET(termSrc->mWakeFdsProximity[0], &read_fds)) {
            LOGD("context thread end - proximity");
            break;
        }
        /* get proximity sensor data */
        size = read(fd, buf, PX_BUF_SIZE);
        char *p = buf;
        p_ps_phy_data = (struct ps_phy_data *) buf;
        while (size > 0) {
            LOGD("-- proximity: %d", p_ps_phy_data->near);
            termSrc->mMutexProximity.lock();
                if (p_ps_phy_data->near == 1)
                    termSrc->mFlagProximity = true;
                else
                    termSrc->mFlagProximity = false;
            termSrc->mMutexProximity.unlock();
            size = size - (sizeof(struct ps_phy_data));
            p = p + sizeof(struct ps_phy_data);
            p_ps_phy_data = (struct ps_phy_data *)p;
        }
        FD_SET(fd, &read_fds);
        FD_SET(termSrc->mWakeFdsProximity[0], &read_fds);
    }
    delete [] buf;
    pthread_exit(NULL);
    return NULL;
}

/* thread to process gyro data */
void* GestureSensor::PollingThread_gyro(void *source)
{
    LOGD("context thread start: wait for gyro data");
    GestureSensor* termSrc = static_cast<GestureSensor*>(source);

    int fd = psh_get_fd(termSrc->mHandleGyro);
    char* buf = new char[GS_BUF_SIZE];
    int size = 0;
    struct gyro_raw_data *p_gyro_raw_data;
    fd_set listen_fds, read_fds;
    int ret, max_fd;

    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);
    FD_SET(termSrc->mWakeFdsGyro[0], &read_fds);

    if (fd > termSrc->mWakeFdsGyro[0])
        max_fd = fd;
    else
        max_fd = termSrc->mWakeFdsGyro[0];

    while ((ret = (select(max_fd +1, &read_fds, NULL, NULL, NULL)) >= 0)) {
        /* if sensor is shut down, return */
        if (FD_ISSET(termSrc->mWakeFdsGyro[0], &read_fds)) {
            LOGD("context thread end - gyro");
            break;
        }
        /* get gyro sensor data */
        size = read(fd, buf, PX_BUF_SIZE);
        char *p = buf;
        p_gyro_raw_data = (struct gyro_raw_data *)buf;
        while (size > 0) {
            termSrc->mMutexGyro.lock();
                termSrc->mDataGyro[0] = p_gyro_raw_data->x;
                termSrc->mDataGyro[1] = p_gyro_raw_data->y;
                termSrc->mDataGyro[2] = p_gyro_raw_data->z;
            termSrc->mMutexGyro.unlock();
            size = size - sizeof(struct gyro_raw_data);
            p = p + sizeof(struct gyro_raw_data);
            p_gyro_raw_data = (struct gyro_raw_data *)p;
        }
        FD_SET(fd, &read_fds);
        FD_SET(termSrc->mWakeFdsGyro[0], &read_fds);
    }
    delete [] buf;
    pthread_exit(NULL);
    return NULL;
}

/* start gesture algorithm, and start accel in psh */
/* start accel polling thread */
bool GestureSensor::Start_accel()
{
    LOGD("init gesture algorithm");
    mMutexAccel.lock();
    bool r = (*mGestureInit)(0, NULL);  /* use default model */
    mMutexAccel.unlock();
    mInitGesture = true;
    if (r == true) {
        LOGD("init psh sensor hub - accel");
        mHandleAccel = psh_open_session(SENSOR_ACCELEROMETER);
        if (mHandleAccel != PSH_SESSION_NOT_OPENED) {
            error_t ret;
            ret = psh_start_streaming(mHandleAccel,
                                    GS_SAMPLE_RATE, GS_BUF_DELAY);
            if (ret == ERROR_NONE) {
                LOGD("init pthread - accel");
                pipe(mWakeFdsAccel);
                int err = pthread_create(&mPollingThreadIDAccel, NULL,
                                    PollingThread_accel, this);
                if (err == 0)
                    return true;
            }
        }
    }
    Stop_accel();
    LOGE("psh & algorithm start return false - gesture");
    return false;
}

/* stop gesture algorithm, and stop accel in psh */
/* stop accel polling thread */
void GestureSensor::Stop_accel()
{
    if (mInitGesture == true) {
        LOGD("stop algorithm - gesture");
        mMutexAccel.lock();
        (*mGestureClose)();
        mMutexAccel.unlock();
        mInitGesture = false;
    }
    if (mHandleAccel != PSH_SESSION_NOT_OPENED) {
        LOGD("stop sensor hub - accel");
        psh_stop_streaming(mHandleAccel);
        psh_close_session(mHandleAccel);
        mHandleAccel = PSH_SESSION_NOT_OPENED;
    }
    if (mPollingThreadIDAccel != THREAD_NOT_STARTED) {
        LOGD("stop pthread - accel");
        write(mWakeFdsAccel[1], "a", 1);
        if (mPollingThreadIDAccel != THREAD_NOT_STARTED) {
            pthread_join(mPollingThreadIDAccel, NULL);
            mPollingThreadIDAccel = THREAD_NOT_STARTED;
        }
    }
    if (mWakeFdsAccel[0] != PIPE_NOT_OPENED)
        close(mWakeFdsAccel[0]);
    if (mWakeFdsAccel[1] != PIPE_NOT_OPENED)
        close(mWakeFdsAccel[1]);
    mWakeFdsAccel[0] = mWakeFdsAccel[1] = PIPE_NOT_OPENED;
}

/* start proximity in psh */
/* start proximity polling thread */
bool GestureSensor::Start_proximity()
{
    LOGD("init psh sensor hub - proximity");
    mHandleProximity = psh_open_session(SENSOR_PROXIMITY);
    if (mHandleProximity != PSH_SESSION_NOT_OPENED) {
        error_t ret;
        ret = psh_start_streaming(mHandleProximity,
                                    PX_SAMPLE_RATE, PX_BUF_DELAY);
        if (ret == ERROR_NONE) {
            LOGD("init pthread - proximity");
            pipe(mWakeFdsProximity);
            int err = pthread_create(&mPollingThreadIDProximity, NULL,
                                    PollingThread_proximity, this);
            if (err == 0)
                return true;
        }
    }
    Stop_proximity();
    LOGE("psh start return false - proximity");
    return false;
}

/* stop proximity in psh */
/* stop proximity polling thread */
void GestureSensor::Stop_proximity()
{
    if (mHandleProximity != PSH_SESSION_NOT_OPENED) {
        LOGD("stop sensor hub - proximity");
        psh_stop_streaming(mHandleProximity);
        psh_close_session(mHandleProximity);
        mHandleProximity = PSH_SESSION_NOT_OPENED;
    }
    if (mPollingThreadIDProximity != THREAD_NOT_STARTED) {
        LOGD("stop pthread - proximity");
        write(mWakeFdsProximity[1], "a", 1);
        if (mPollingThreadIDProximity != THREAD_NOT_STARTED) {
            pthread_join(mPollingThreadIDProximity, NULL);
            mPollingThreadIDProximity = THREAD_NOT_STARTED;
        }
    }
    if (mWakeFdsProximity[0] != PIPE_NOT_OPENED)
        close(mWakeFdsProximity[0]);
    if (mWakeFdsProximity[1] != PIPE_NOT_OPENED)
        close(mWakeFdsProximity[1]);
    mWakeFdsProximity[0] = mWakeFdsProximity[1] = PIPE_NOT_OPENED;
}

/* start gyro in psh */
/* start gyro polling thread */
bool GestureSensor::Start_gyro()
{
    LOGD("init psh sensor hub - gyro");
    mHandleGyro = psh_open_session(SENSOR_GYRO);
    if (mHandleGyro != PSH_SESSION_NOT_OPENED) {
        error_t ret;
        ret = psh_start_streaming(mHandleGyro,
                                    GS_SAMPLE_RATE, GS_BUF_DELAY);
        if (ret == ERROR_NONE) {
            LOGD("init pthread - gyro");
            pipe(mWakeFdsGyro);
            int err = pthread_create(&mPollingThreadIDGyro, NULL,
                                    PollingThread_gyro, this);
            if (err == 0)
                return true;
        }
    }
    Stop_gyro();
    LOGE("psh start return false - gyro");
    return false;
}

/* stop gyro in psh */
/* stop gyro polling thread */
void GestureSensor::Stop_gyro()
{
    if (mHandleGyro != PSH_SESSION_NOT_OPENED) {
        LOGD("stop sensor hub - gyro");
        psh_stop_streaming(mHandleGyro);
        psh_close_session(mHandleGyro);
        mHandleGyro = NULL;
    }
    if (mPollingThreadIDGyro != THREAD_NOT_STARTED) {
        LOGD("stop pthread - gyro");
        write(mWakeFdsGyro[1], "a", 1);
        if (mPollingThreadIDGyro != THREAD_NOT_STARTED) {
            pthread_join(mPollingThreadIDGyro, NULL);
            mPollingThreadIDGyro = THREAD_NOT_STARTED;
        }
    }
    if (mWakeFdsGyro[0] != PIPE_NOT_OPENED)
        close(mWakeFdsGyro[0]);
    if (mWakeFdsGyro[1] != PIPE_NOT_OPENED)
        close(mWakeFdsGyro[1]);
    mWakeFdsGyro[0] = mWakeFdsGyro[1] = PIPE_NOT_OPENED;
}

