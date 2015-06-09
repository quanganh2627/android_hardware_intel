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
#include <camera/CameraParameters.h>
#include "MessageQueue.h"
#include "AtomCommon.h"

#ifdef GRAPHIC_IS_GEN
#include <VideoVPPBase.h>
#include <intel_bufmgr.h>
#endif

namespace android {

class CallbacksThread;

class VideoThread : public Thread {

// constructor destructor
public:
    VideoThread(sp<CallbacksThread> callbacksThread);
    virtual ~VideoThread();

// prevent copy constructor and assignment operator
private:
    VideoThread(const VideoThread& other);
    VideoThread& operator=(const VideoThread& other);

// Thread overrides
public:
    status_t requestExitAndWait();

// public methods
public:

    status_t video(AtomBuffer *buff);
    status_t flushBuffers();
    status_t setSlowMotionRate(int rate);
    void getDefaultParameters(CameraParameters *intel_params, int cameraId);

// private types
private:

    // thread message id's
    enum MessageId {

        MESSAGE_ID_EXIT = 0,            // call requestExitAndWait
        MESSAGE_ID_VIDEO,
        MESSAGE_ID_FLUSH,
        MESSAGE_ID_SET_SLOWMOTION_RATE,

        // max number of messages
        MESSAGE_ID_MAX
    };

    //
    // message data structures
    //

    struct MessageVideo {
        AtomBuffer buff;
    };

    struct MessageSetSlowMotionRate {
        int rate;
    };

    // union of all message data
    union MessageData {

        // MESSAGE_ID_VIDEO
        MessageVideo video;
        MessageSetSlowMotionRate setSlowMotionRate;
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
    status_t handleMessageSetSlowMotionRate(MessageSetSlowMotionRate* msg);

    // main message function
    status_t waitForAndExecuteMessage();

    // BYT need this conversion for video encoding. do nothing for others
    status_t convertNV12Linear2Tiled(const AtomBuffer &buff);
// inherited from Thread
private:
    virtual bool threadLoop();

// private data
private:

    MessageQueue<Message, MessageId> mMessageQueue;
    bool mThreadRunning;
    sp<CallbacksThread> mCallbacksThread;
    int mSlowMotionRate;
    nsecs_t mFirstFrameTimestamp;
#if GRAPHIC_IS_GEN //only availble with Gen GPU
    VideoVPPBase *mVpp;
#endif

}; // class VideoThread

}; // namespace android

#endif // ANDROID_LIBCAMERA_VIDEO_THREAD_H
