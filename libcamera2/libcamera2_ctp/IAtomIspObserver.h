/*
 * Copyright (C) 2011 The Android Open Source Project
 * Copyright (c) 2013 Intel Corporation. All Rights Reserved.
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

#ifndef ANDROID_LIBCAMERA_ISP_OBSERVER_INTERFACE_H
#define ANDROID_LIBCAMERA_ISP_OBSERVER_INTERFACE_H
#include "MessageQueue.h"
#include "AtomCommon.h"

namespace android {

enum ObserverState {
    OBSERVER_STATE_STOPPED = 0,
    OBSERVER_STATE_PAUSED,
    OBSERVER_STATE_RUNNING
};

class IAtomIspObserver {
public:
    enum MessageId {
        MESSAGE_ID_EVENT = 0,
        MESSAGE_ID_FRAME,
        MESSAGE_ID_END_OF_STREAM,
        MESSAGE_ID_ERROR
    };

    enum EventType {
        EVENT_TYPE_SOF,
        EVENT_TYPE_STATISTICS_READY,
        EVENT_TYPE_STATISTICS_SKIPPED
    };

    struct MessageFrameBuffer {
        AtomBuffer buff;
    };

    struct MessageEvent {
        EventType       type;
        struct timeval  timestamp;
        unsigned int    sequence;   // for debugging, not reliable as frame indentifier
    };

    union MessageData {
        MessageFrameBuffer  frameBuffer;
        MessageEvent        event;
    };

    struct Message {
        MessageId id;
        MessageData data;
    };

    virtual bool atomIspNotify(Message *msg, const ObserverState state) = 0;
    virtual ~IAtomIspObserver() {};
};

};

#endif
