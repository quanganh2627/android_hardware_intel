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
#include "ICameraHwControls.h"
#include "ICallbackPreview.h"

#ifdef GRAPHIC_IS_GEN
#include <VideoVPPBase.h>
#include <intel_bufmgr.h>
#endif

namespace android {

class CallbacksThread;
class AtomISP;

class VideoThread :
    public Thread,
    public IAtomIspObserver,
    public ICallbackPreview {

// constructor destructor
public:
    VideoThread(AtomISP *atomIsp, sp<CallbacksThread> callbacksThread);
    virtual ~VideoThread();

// prevent copy constructor and assignment operator
private:
    VideoThread(const VideoThread& other);
    VideoThread& operator=(const VideoThread& other);

// ICallbackPreview overrides
public:
    virtual void previewBufferCallback(AtomBuffer *memory, ICallbackPreview::CallbackType t);
    virtual int getCameraID();

// IAtomIspObserver overrides
public:
    virtual bool atomIspNotify(IAtomIspObserver::Message *msg, const ObserverState state);

// Thread overrides
public:
    status_t requestExitAndWait();

// public methods
public:
    status_t startRecording(); // sync call
    status_t stopRecording(); // sync call
    status_t flushBuffers();
    status_t setSlowMotionRate(int rate);
    void getDefaultParameters(CameraParameters *intel_params, int cameraId);
    // return recording frame to driver (asynchronous)
    status_t releaseRecordingFrame(void *buff);
    status_t getVideoSnapshot(AtomBuffer &buff);
    status_t putVideoSnapshot(AtomBuffer *buff);
    status_t setRecordingMirror(bool mirror, int recOrientation, int camOrientation);

// private types
private:

    // thread message id's
    enum MessageId {

        MESSAGE_ID_EXIT = 0,            // call requestExitAndWait
        MESSAGE_ID_START_RECORDING,
        MESSAGE_ID_STOP_RECORDING,
        MESSAGE_ID_FLUSH,
        MESSAGE_ID_SET_SLOWMOTION_RATE,
        MESSAGE_ID_PUSH_FRAME,
        MESSAGE_ID_HAL_VIDEO_STABILIZATION,
        MESSAGE_ID_RELEASE_RECORDING_FRAME,
        MESSAGE_ID_DEQUEUE_RECORDING,

        // max number of messages
        MESSAGE_ID_MAX
    };

    //
    // message data structures
    //
    struct MessageReleaseRecordingFrame {
        void *buff;
    };

    struct MessagePushFrame {
        AtomBuffer *buf;
    };

    struct MessageDequeueRecording {
        bool skipFrame;
    };

    struct MessageSetSlowMotionRate {
        int rate;
    };

    // union of all message data
    union MessageData {
        // MESSAGE_ID_SET_SLOWMOTION_RATE,
        MessageSetSlowMotionRate setSlowMotionRate;
        // MESSAGE_ID_RELEASE_RECORDING_FRAME
        MessageReleaseRecordingFrame releaseRecordingFrame;
        // MESSAGE_ID_PUSH_FRAME
        MessagePushFrame pushFrame;
        // MESSAGE_ID_DEQUEUE_RECORDING
        MessageDequeueRecording   dequeueRecording;
    };

    // message id and message data
    struct Message {
        MessageId id;
        MessageData data;
    };

    enum VideoState {
        STATE_IDLE,
        STATE_PREVIEW,
        STATE_RECORDING
    };

// private methods
private:

    // thread message execution functions
    status_t handleMessageStartRecording();
    status_t handleMessageStopRecording();
    status_t handleMessageExit();
    status_t handleMessageFlush();
    status_t handleMessageSetSlowMotionRate(MessageSetSlowMotionRate* msg);
    status_t handleMessageReleaseRecordingFrame(MessageReleaseRecordingFrame *msg);
    status_t handleMessageDequeueRecording(MessageDequeueRecording *msg);
    status_t handleMessagePushFrame(MessagePushFrame *msg);
    AtomBuffer* findVideoSnapshotBuffer(int index);
    AtomBuffer* findRecordingBuffer(void *findMe);
    AtomBuffer* findRecordingBuffer(int index);
    status_t processVideoBuffer(AtomBuffer &buff);
    void reset();

    // main message function
    status_t waitForAndExecuteMessage();

    // BYT need this conversion for video encoding. do nothing for others
    status_t convertNV12Linear2Tiled(const AtomBuffer &buff);
// inherited from Thread
private:
    virtual bool threadLoop();

// private data
private:

    AtomISP *mIsp;
    MessageQueue<Message, MessageId> mMessageQueue;
    bool mThreadRunning;
    Mutex mLock;
    Condition mFrameCondition;
    sp<CallbacksThread> mCallbacksThread;
    int mSlowMotionRate;
    nsecs_t mFirstFrameTimestamp;
#if GRAPHIC_IS_GEN //only availble with Gen GPU
    VideoVPPBase *mVpp;
#endif
    Vector<AtomBuffer> mSnapshotBuffers; /*!< buffers reserved from stream for videosnapshot */
    Vector<AtomBuffer> mRecordingBuffers; /*!< buffers reserverd from stream for video encoding */
    VideoState mState;
    bool mMirror;
    bool mHALVideoStabilization;
    int mRecOrientation;
    int mCamOrientation;
    int mCameraId;

}; // class VideoThread

}; // namespace android

#endif // ANDROID_LIBCAMERA_VIDEO_THREAD_H
