/*
 * Copyright (c) 2014 Intel Corporation
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

#ifndef ANDROID_LIBCAMERA_THERMALTHROTTLE_H_
#define ANDROID_LIBCAMERA_THERMALTHROTTLE_H_

#include <fcntl.h>
#include <sys/stat.h>
#include <poll.h>
#include <utils/threads.h>
#include "ICameraHwControls.h"
#include "MessageQueue.h"
#include "AtomCommon.h"

namespace android {

/**
 * A class encapsulating camera module fps thermal throttling
 *
 * The class listens the notify from thermal throttling device.
 * when the notify arrives, that means the current temperature is high,
 * so at that time the fps should be decreased based on the thermal throttling demand.
 */
class ThermalThrottleThread : public Thread {

// constructor destructor
public:
    ThermalThrottleThread(IHWSensorControl *SensorControl);
    virtual ~ThermalThrottleThread();

// prevent copy constructor and assignment operator
private:
    ThermalThrottleThread(const ThermalThrottleThread& other);
    ThermalThrottleThread& operator=(const ThermalThrottleThread& other);

// Thread overrides
public:
    status_t requestExitAndWait();

// public methods
public:
    virtual status_t startMonitoring();
    virtual status_t stopMonitoring();
    virtual int poll(int timeout);
    bool isMonitoring() { return mMonitoring;};

// private types
private:

    // thread message id's
    enum MessageId {
        MESSAGE_ID_EXIT = 0,            // call requestExitAndWait
        MESSAGE_ID_START_MONITORING,
        MESSAGE_ID_STOP_MONITORING,
        MESSAGE_ID_MMONITOR_NOTIFY,
        // max number of messages
        MESSAGE_ID_MAX
    };

    enum mFpsThrottleState {
        FPS_THROTTLE_DISABLED = 0,
        FPS_THROTTLE_ENABLED,
        FPS_THROTTLE_SUCCESS
    };

    // message id and message data
    struct Message {
        MessageId id;
    };

// private methods
private:

    // thread message execution functions
    status_t handleMessageExit();
    status_t handleMessageStartMonitoring();
    status_t handleMessageStopMonitoring();
    status_t handleMessageMonitorNotify();
    // main message function
    status_t waitForAndExecuteMessage();

    status_t openThermalThrottle();
    status_t closeThermalThrottle();
    status_t monitorNotify();
    bool notifyArrived();
    status_t handleNotify();

// inherited from Thread
private:
    virtual bool threadLoop();

// const data
private:
    static const char SYSFS_THERMAL_THROTTLE_NOTIFY[];
    static const char SYSFS_THERMAL_THROTTLE_HANDSHAKE[];
    static const int ATTR_LEN = 16;
    static const int DEFAULT_FPS_PERCENT = 100;
    static const int THERMAL_THROTTLE_POLL_TIMEOUT = 500;

// private data
private:

    MessageQueue<Message, MessageId> mMessageQueue;
    bool mThreadRunning;
    IHWSensorControl *mSensorCI;
    bool mMonitoring;
    int mNotifyFd;
    int mHandshakeFd;
    int mFps;

};
} /* namespace android */
#endif /* ANDROID_LIBCAMERA_THERMALTHROTTLE_H_ */
