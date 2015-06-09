/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ANDROID_LIBCAMERA_PIPE_THREAD_H
#define ANDROID_LIBCAMERA_PIPE_THREAD_H

#include <utils/Timers.h>
#include <utils/threads.h>
#include "MessageQueue.h"
#include "CameraCommon.h"
#include "VAConvertor.h"
#include "Callbacks.h"


namespace android {

class PreviewThread;
class VideoThread;
class VAConvertor;


//#define COLOR_CONVERT_HW

class PipeThread : public Thread {

// public types
public:

    struct Config {
        int width;
        int height;
        int inputFormat;
        int outputFormat;
    };

// constructor destructor
public:
    PipeThread();
    virtual ~PipeThread();

// Thread overrides
public:
    status_t requestExitAndWait();

// public methods
public:

    void setThreads(sp<PreviewThread> &previewThread, sp<VideoThread> &videoThread);
    void setConfig(int inputFormat, int outputFormat, int width, int height);
    status_t preview(CameraBuffer *input, CameraBuffer *output,CameraBuffer *midConvert);
    status_t previewVideo(CameraBuffer *input, CameraBuffer *output,CameraBuffer *toAndroid,CameraBuffer *midConvert,nsecs_t timestamp);
    status_t flushBuffers();

// private types
private:

    // thread message id's
    enum MessageId {

        MESSAGE_ID_EXIT = 0,            // call requestExitAndWait
        MESSAGE_ID_PREVIEW,
        MESSAGE_ID_PREVIEW_VIDEO,
        MESSAGE_ID_FLUSH,

        // max number of messages
        MESSAGE_ID_MAX
    };

    //
    // message data structures
    //

    struct MessagePreview {
        CameraBuffer *input;
        CameraBuffer *output;
        CameraBuffer *midConvert;
    };

    struct MessagePreviewVideo {
        CameraBuffer *input;
        CameraBuffer *output;
        CameraBuffer *toAndroid;
        CameraBuffer *midConvert;
        nsecs_t timestamp;
    };
    // union of all message data
    union MessageData {

        // MESSAGE_ID_PREVIEW
        MessagePreview preview;

        // MESSAGE_ID_PREVIEW_VIDEO
        MessagePreviewVideo previewVideo;
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
    status_t handleMessagePreview(MessagePreview *msg);
    status_t handleMessagePreviewVideo(MessagePreviewVideo *msg);
    status_t handleMessageFlush();


    // main message function
    status_t waitForAndExecuteMessage();

// inherited from Thread
private:
    virtual bool threadLoop();

// private data
private:

    int mInputFormat;
    int mOutputFormat;
    int mWidth;
    int mHeight;

    sp<PreviewThread> mPreviewThread;
    sp<VideoThread> mVideoThread;
    MessageQueue<Message, MessageId> mMessageQueue;
    bool mThreadRunning;
    VAConvertor *mVaConvertor;

}; // class PipeThread

}; // namespace android

#endif // ANDROID_LIBCAMERA_PIPE_THREAD_H
