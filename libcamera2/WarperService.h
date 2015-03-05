/*
 * Copyright (C) 2011 The Android Open Source Project
 * Copyright (C) 2011,2012,2013,2014 Intel Corporation
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

#ifndef WARPERSERVICE_H_
#define WARPERSERVICE_H_

#include "MessageQueue.h"
#include "AtomCommon.h"
#include "GPUWarper.h"

namespace android {

/**
 * \class WarperService
 *
 * Given input frame (in NV12 format) and motion matrix (3x3 projective matrix)
 * it performs warping operation using OpenGL ES/EGL on the GPU.
 *
 * Warping is implemented in GPUWarper class. Given that each thread needs
 * its own EGL context, WarperService is implemented based on Thread
 * class and using message queue, with GPUWarper as a private member.
 *
 * Currently, it can be used on IMG graphics (SGX and RGX)
 *
 */
class WarperService : public Thread
{

public:
    WarperService();
    virtual ~WarperService();

    // Thread overrides
    status_t requestExitAndWait();

    status_t warpBackFrame(AtomBuffer *frame, double projective[PROJ_MTRX_DIM][PROJ_MTRX_DIM]);
    status_t updateFrameDimensions(GLuint width, GLuint height);
    status_t updateStatus(bool active);

// prevent copy constructor and assignment operator
private:
    WarperService(const WarperService& other);
    WarperService& operator=(const WarperService& other);

// private types
private:

    // thread message id's
      enum MessageId {
          MESSAGE_ID_EXIT = 0,            // call requestExitAndWait
          MESSAGE_ID_WARP_BACK_FRAME,
          MESSAGE_ID_UPDATE_FRAME_DIMENSIONS,
          MESSAGE_ID_UPDATE_STATUS,
          // max number of messages
          MESSAGE_ID_MAX
      };

      //
      // message data structures
      //

    struct MessageWarpBackFrame {
        AtomBuffer *frame;
        double projective[PROJ_MTRX_DIM][PROJ_MTRX_DIM];
    };

    struct MessageUpdateFrameDimensions {
        GLuint width;
        GLuint height;
    };

    struct MessageUpdateStatus {
        bool active;
    };

    // union of all message data
    union MessageData {
        MessageWarpBackFrame messageWarpBackFrame;
        MessageUpdateFrameDimensions messageUpdateFrameDimensions;
        MessageUpdateStatus messageUpdateStatus;
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
    status_t handleMessageWarpBackFrame(MessageWarpBackFrame &msg);
    status_t handleMessageUpdateFrameDimensions(MessageUpdateFrameDimensions &msg);
    status_t handleMessageUpdateStatus(MessageUpdateStatus &msg);

    // main message function
    status_t waitForAndExecuteMessage();

// inherited from Thread
private:
    virtual bool threadLoop();

// private data
private:

    MessageQueue<Message, MessageId> mMessageQueue;
    bool mThreadRunning;
    GPUWarper *mGPUWarper;

    GLuint mWidth;
    GLuint mHeight;

    bool mFrameDimChanged;
    bool mGPUWarperActive;
};

} /* namespace android */
#endif /* WARPERSERVICE_H_ */
