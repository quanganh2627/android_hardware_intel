/*
 * Copyright (C) 2011 The Android Open Source Project
 * Copyright (C) 2011,2012,2013 Intel Corporation
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

#ifndef SCALERSERVICE_H_
#define SCALERSERVICE_H_

#include "MessageQueue.h"
#include "AtomCommon.h"
#include "IHWScaler.h"

namespace android {

class ScalerService : public Thread
{
    // public types
public:
    enum BufferDirection {
        SCALER_INPUT,
        SCALER_OUTPUT
    };

public:
    ScalerService(int cameraId);
    virtual ~ScalerService();

    // Thread overrides
    status_t requestExitAndWait();

    status_t registerBuffer(AtomBuffer &buffer, BufferDirection dir);
    status_t unRegisterBuffer(AtomBuffer &buffer, BufferDirection dir);
    status_t scaleAndZoom(const AtomBuffer *input, AtomBuffer *output, float zoomFactor);

// private types
private:

    // thread message id's
    enum MessageId {
        MESSAGE_ID_EXIT = 0,            // call requestExitAndWait
        MESSAGE_ID_SCALE_AND_ZOOM,
        MESSAGE_ID_REGISTER_BUFFER,
        MESSAGE_ID_UNREGISTER_BUFFER,
        // max number of messages
        MESSAGE_ID_MAX
    };

    //
    // message data structures
    //

    struct MessageScaleAndZoom {
        const AtomBuffer *input;
        AtomBuffer *output;
        float zoomFactor;
    };

    struct MessageRegister {
        AtomBuffer *buffer;
        BufferDirection dir;
    };

    // union of all message data
    union MessageData {
        MessageScaleAndZoom messageScaleAndZoom;
        MessageRegister messageRegister;
        MessageRegister messageUnregister;
    };

    // message id and message data
    struct Message {
        MessageId id;
        MessageData data;
    };

// private methods
private:
    // thread message execution functions
    status_t handleMessageExit();
    status_t handleMessageScaleAndZoom(MessageScaleAndZoom &msg);
    status_t handleMessageRegisterBuffer(MessageRegister &msg);
    status_t handleMessageUnregisterBuffer(MessageRegister &msg);

    // main message function
    status_t waitForAndExecuteMessage();

// inherited from Thread
private:
    virtual bool threadLoop();

// private data
private:

    MessageQueue<Message, MessageId> mMessageQueue;
    bool mThreadRunning;
    IHWScaler *mHWScaler;
    unsigned int mFrameCounter;
    int mCameraId;
};

} /* namespace android */
#endif /* SCALERSERVICE_H_ */
