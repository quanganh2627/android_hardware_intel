/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ANDROID_LIBCAMERA_VIDEO_THREAD_H
#define ANDROID_LIBCAMERA_VIDEO_THREAD_H

#include <utils/Timers.h>
#include <utils/threads.h>
#include "MessageQueue.h"
#include "CameraCommon.h"
#include "VAConvertor.h"


namespace android {

class Callbacks;

class VideoThread : public Thread {

// constructor destructor
public:
    VideoThread();
    virtual ~VideoThread();

// Thread overrides
public:
    status_t requestExitAndWait();

// public methods
public:

    status_t setConfig(int inputFormat, int outputFormat, int width, int height);

    // Input and output buffer supplied only if color conversion is required.
    // If no color conversion is required simply supply the input buffer
    status_t video(CameraBuffer *buff, CameraBuffer *interbuff, nsecs_t timestamp);
    status_t flushBuffers();
    void setCallbacks(sp<Callbacks> &callbacks) { mCallbacks = callbacks; }
// private types
private:

    // thread message id's
    enum MessageId {

        MESSAGE_ID_EXIT = 0,            // call requestExitAndWait
        MESSAGE_ID_VIDEO,
        MESSAGE_ID_FLUSH,

        // max number of messages
        MESSAGE_ID_MAX
    };

    //
    // message data structures
    //

    struct MessageVideo {
        CameraBuffer* yuv422hbuff;
        CameraBuffer* nv12buff;
        nsecs_t timestamp;
    };

    // union of all message data
    union MessageData {

        // MESSAGE_ID_VIDEO
        MessageVideo video;
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
    status_t handleMessageVideo(MessageVideo *msg);
    status_t handleMessageFlush();

    // main message function
    status_t waitForAndExecuteMessage();

// inherited from Thread
private:
    virtual bool threadLoop();

// private data
private:

    MessageQueue<Message, MessageId> mMessageQueue;
    bool mThreadRunning;
    sp<Callbacks> mCallbacks;

    int mInputFormat;
    int mOutputFormat;
    int mWidth;
    int mHeight;

    VAConvertor *mVaConvertor;

}; // class VideoThread

}; // namespace android

#endif // ANDROID_LIBCAMERA_VIDEO_THREAD_H
