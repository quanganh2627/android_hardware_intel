/*
 * Copyright (c) 2014 Intel Corporation.
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
#define LOG_TAG "Camera_ThermalThrottleThread"

#include "ThermalThrottleThread.h"
#include "LogHelper.h"

namespace android {

const char ThermalThrottleThread::SYSFS_THERMAL_THROTTLE_NOTIFY[] = "/sys/fps_throttle/notify";
const char ThermalThrottleThread::SYSFS_THERMAL_THROTTLE_HANDSHAKE[] = "/sys/fps_throttle/handshake";

ThermalThrottleThread::ThermalThrottleThread(IHWSensorControl *SensorControl) :
    Thread(true)
    ,mMessageQueue("ThermalThrottleThread", MESSAGE_ID_MAX)
    ,mThreadRunning(false)
    ,mSensorCI(SensorControl)
    ,mMonitoring(false)
    ,mNotifyFd(-1)
    ,mHandshakeFd(-1)
    ,mFps(0)
{
    LOG1("@%s", __FUNCTION__);
}

ThermalThrottleThread::~ThermalThrottleThread()
{
    LOG1("@%s", __FUNCTION__);
    if (mMonitoring)
        closeThermalThrottle();
}

status_t ThermalThrottleThread::openThermalThrottle()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    if ((mNotifyFd = ::open(SYSFS_THERMAL_THROTTLE_NOTIFY, O_RDONLY)) < 0)
    {
        ALOGW("Unable to open notify: %s", strerror(errno));
        status = UNKNOWN_ERROR;
    }

    if ((mHandshakeFd = ::open(SYSFS_THERMAL_THROTTLE_HANDSHAKE, O_WRONLY)) < 0)
    {
        ALOGW("Unable to open handshake %s", strerror(errno));
        ::close(mNotifyFd);
        status = UNKNOWN_ERROR;
    }

    return status;
}

status_t ThermalThrottleThread::closeThermalThrottle()
{
    LOG1("@%s", __FUNCTION__);

    if (mNotifyFd < 0) {
        ALOGW("notify not opened!");
        return INVALID_OPERATION;
    }

    if (::close(mNotifyFd) < 0) {
        ALOGE("Close notify failed: %s", strerror(errno));
        return UNKNOWN_ERROR;
    }

    mNotifyFd = -1;

    if (mHandshakeFd < 0) {
        ALOGW("handshake not opened!");
        return INVALID_OPERATION;
    }

    if (::close(mHandshakeFd) < 0) {
        ALOGE("Close handshake failed: %s", strerror(errno));
        return UNKNOWN_ERROR;
    }

    mHandshakeFd = -1;
    return NO_ERROR;
}

int ThermalThrottleThread::poll(int timeout)
{
    LOG2("@%s", __FUNCTION__);
    struct pollfd pfd = {0,0,0};
    int ret = -1;

    if (mNotifyFd == -1) {
        ALOGW(" thermal throttling module already closed. Do nothing.");
        return ret;
    }

    pfd.fd = mNotifyFd;
    pfd.events = POLLPRI;

    ::poll(&pfd, 1, timeout);
    if (pfd.revents & POLLPRI) {
        LOG2("%s received POLLPRI", __FUNCTION__);
        return 1;
    }

    return ret;
}


bool ThermalThrottleThread::notifyArrived()
{
    LOG2("@%s", __FUNCTION__);
    bool notify_arrived = false;
    if (!mMonitoring)
        return notify_arrived;

    notify_arrived = poll(THERMAL_THROTTLE_POLL_TIMEOUT) > 0 ? true : false;
    monitorNotify();
    return notify_arrived;
}

/**
 * handle notify
 * This function shall be called when thermal throttling alert arrives.
 * Current solution is to modify the frame rate to control temperature.
 * TODO: the frame rate should not be changed directly by sensor driver,
 *       it should be by AIQ, so better solution is to set FLmin,FLmax,ETmax
 *       to AIQ.
 */
status_t ThermalThrottleThread::handleNotify()
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    char attrData[ATTR_LEN];

    memset(attrData, 0, ATTR_LEN);
    ::lseek(mNotifyFd, 0, SEEK_SET);
    int count = ::read(mNotifyFd, attrData, ATTR_LEN - 1);
    if (count > 0) {
        //attr_Data is percentage of frame rate.
        int fps_percent;
        sscanf(attrData, "%d", &fps_percent);
        LOG2("fps_percent: %d", fps_percent);
        if(fps_percent > 0 && fps_percent <= 100 && mSensorCI != NULL) {
            int fps = mFps * fps_percent / 100;
            status = mSensorCI->setFramerate(fps);
            //if setting FPS success, notice the thermal Throttling module
            if(status == NO_ERROR) {
                memset(attrData, 0, ATTR_LEN);
                sprintf(attrData, "%d", FPS_THROTTLE_SUCCESS);
                count = ::write(mHandshakeFd, attrData, 1);
            }
        }
    }

    return status;
}

status_t ThermalThrottleThread::startMonitoring()
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_START_MONITORING;
    return mMessageQueue.send(&msg);
}

status_t ThermalThrottleThread::stopMonitoring()
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_STOP_MONITORING;
    return mMessageQueue.send(&msg);
}

status_t ThermalThrottleThread::monitorNotify()
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_MMONITOR_NOTIFY;
    return mMessageQueue.send(&msg);
}

status_t ThermalThrottleThread::handleMessageExit()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    char attrData[ATTR_LEN];

    mThreadRunning = false;
    if (mMonitoring) {
        //Disable fps throttle.
        memset(attrData, 0, ATTR_LEN);
        sprintf(attrData, "%d", FPS_THROTTLE_DISABLED);
        ::write(mHandshakeFd, attrData, 2);

        closeThermalThrottle();
        mMonitoring = false;
    }

    return status;
}

status_t ThermalThrottleThread::handleMessageStartMonitoring()
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    char attrData[ATTR_LEN];

    if (mMonitoring)
        return INVALID_OPERATION;

    status = openThermalThrottle();
    if (status == NO_ERROR) {
        mFps = mSensorCI->getFramerate();
        mMonitoring = true;
    }

    //set the default throttling fps first.
    memset(attrData, 0, ATTR_LEN);
    ::lseek(mNotifyFd, 0, SEEK_SET);
    int count = ::read(mNotifyFd, attrData, ATTR_LEN - 1);
    if (count > 0) {
        //attr_Data is percentage of frame rate.
        int fps_percent;
        sscanf(attrData, "%d", &fps_percent);
        LOG2("mFps: %d, fps_percent: %d", mFps, fps_percent);
        if(fps_percent > 0 && fps_percent < 100 && mSensorCI != NULL) {
            LOG2("fps changed as per thermal request.");
            int fps = mFps * fps_percent / 100;
            status = mSensorCI->setFramerate(fps);
            //if setting FPS failed, reset the notify to default.
            if(status != NO_ERROR) {
                memset(attrData, 0, ATTR_LEN);
                sprintf(attrData, "%d", DEFAULT_FPS_PERCENT);
                count = ::write(mNotifyFd, attrData, 1);
            }
        }
    }

    memset(attrData, 0, ATTR_LEN);
    sprintf(attrData, "%d", FPS_THROTTLE_ENABLED);
    count = ::write(mHandshakeFd, attrData, 1);

    monitorNotify();
    return status;
}

status_t ThermalThrottleThread::handleMessageStopMonitoring()
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    char attrData[ATTR_LEN];

    if (!mMonitoring)
        return INVALID_OPERATION;

    memset(attrData, 0, ATTR_LEN);
    sprintf(attrData, "%d", FPS_THROTTLE_DISABLED);
    ::write(mHandshakeFd, attrData, 1);

    status = closeThermalThrottle();
    mMonitoring = false;
    return status;
}

status_t ThermalThrottleThread::handleMessageMonitorNotify()
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    if (mMonitoring && notifyArrived()) {
            // make sure thermal thorttle notify arrive, then handle
            status = handleNotify();
    }

    return status;
}

status_t ThermalThrottleThread::waitForAndExecuteMessage()
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    Message msg;
    mMessageQueue.receive(&msg);

    switch (msg.id) {
        case MESSAGE_ID_EXIT:
            status = handleMessageExit();
            break;
        case MESSAGE_ID_START_MONITORING:
            status = handleMessageStartMonitoring();
            break;
        case MESSAGE_ID_STOP_MONITORING:
            status = handleMessageStopMonitoring();
            break;
        case MESSAGE_ID_MMONITOR_NOTIFY:
            status = handleMessageMonitorNotify();
        default:
            ALOGE("Invalid message");
            status = BAD_VALUE;
            break;
    };
    return status;
}

bool ThermalThrottleThread::threadLoop()
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    mThreadRunning = true;
    while (mThreadRunning) {
        status = waitForAndExecuteMessage();
    }
    return false;
}

status_t ThermalThrottleThread::requestExitAndWait()
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_EXIT;

    // tell thread to exit
    // send message asynchronously
    mMessageQueue.send(&msg);

    // propagate call to base class
    return Thread::requestExitAndWait();
}

} // namespace android

