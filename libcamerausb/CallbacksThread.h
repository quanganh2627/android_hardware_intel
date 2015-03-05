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

#ifndef ANDROID_LIBCAMERA_CALLBACKS_THREAD_H
#define ANDROID_LIBCAMERA_CALLBACKS_THREAD_H

#include <utils/threads.h>
#include <utils/Vector.h>
#include "MessageQueue.h"
#include "IFaceDetectionListener.h"

namespace android {

class Callbacks;

class CallbacksThread :
    public Thread {

public:
    CallbacksThread();
    virtual ~CallbacksThread();

// prevent copy constructor and assignment operator
private:
    CallbacksThread(const CallbacksThread& other);
    CallbacksThread operator=(const CallbacksThread& other);

// Thread overrides
public:
    status_t requestExitAndWait();

// public methods
public:

    status_t shutterSound();
    void setCallbacks(sp<Callbacks> &callbacks) { mCallbacks = callbacks; }
// private types
private:

    // thread message id's
    enum MessageId {

        MESSAGE_ID_EXIT = 0,            // call requestExitAndWait
        MESSAGE_ID_CALLBACK_SHUTTER,    // send the shutter callback
        MESSAGE_ID_JPEG_DATA_READY,     // we have a JPEG image ready
        MESSAGE_ID_JPEG_DATA_REQUEST,   // a JPEG image was requested
        MESSAGE_ID_AUTO_FOCUS_DONE,
        MESSAGE_ID_FOCUS_MOVE,
        MESSAGE_ID_FLUSH,
        MESSAGE_ID_FACES,
        MESSAGE_ID_SCENE_DETECTED,
        MESSAGE_ID_PREVIEW_DONE,
        MESSAGE_ID_VIDEO_DONE,
        MESSAGE_ID_POSTVIEW_RENDERED,

        // panorama callbacks
        MESSAGE_ID_PANORAMA_SNAPSHOT,
        MESSAGE_ID_PANORAMA_DISPL_UPDATE,

        // Ultra Low Light Callbacks
        MESSAGE_ID_ULL_JPEG_DATA_REQUEST,
        MESSAGE_ID_ULL_TRIGGERED,

        // Error callback
        MESSAGE_ID_ERROR_CALLBACK,

        // max number of messages
        MESSAGE_ID_MAX
    };

    //
    // message data structures
    //

    struct MessageFaces {
        camera_frame_metadata_t meta_data;
    };

    struct MessageAutoFocusDone {
        bool status;
    };

    struct MessageFocusMove {
        bool start;
    };

    struct MessageDataRequest {
        bool postviewCallback;
        bool rawCallback;
        bool waitRendering;
    };

    struct MessageSceneDetected {
        int sceneMode;
        bool sceneHdr;
    };

    struct MessageError {
        int id;
    };
    // union of all message data
    union MessageData {


        //MESSAGE_ID_JPEG_DATA_REQUEST
        MessageDataRequest dataRequest;

        //MESSAGE_ID_AUTO_FOCUS_DONE
        MessageAutoFocusDone autoFocusDone;

        // MESSAGE_ID_FOCUS_MOVE
        MessageFocusMove focusMove;

        // MESSAGE_ID_FACES
        MessageFaces faces;

        // MESSAGE_ID_SCENE_DETECTED
        MessageSceneDetected    sceneDetected;

        // MESSAGE_ID_ERROR_CALLBACK
        MessageError error;

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
    status_t handleMessageCallbackShutter();
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
// public data
public:

}; // class CallbacksThread

}; // namespace android

#endif // ANDROID_LIBCAMERA_CALLBACKS_THREAD_H
