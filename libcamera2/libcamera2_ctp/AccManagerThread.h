/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef ANDROID_LIBCAMERA_ACCMANAGER_THREAD_H
#define ANDROID_LIBCAMERA_ACCMANAGER_THREAD_H

#include <utils/threads.h>
#include "MessageQueue.h"
#include "PreviewThread.h" // ICallbackPreview
#include "IntelParameters.h"
#include "ICameraHwControls.h"

namespace android {

//class Callbacks;

class AccManagerThread : public Thread,
                         public ICallbackPreview
{

// constructor/destructor
public:
    AccManagerThread(HWControlGroup &ispCI, sp<CallbacksThread> callbacksThread, Callbacks *callbacks, int cameraId);
    virtual ~AccManagerThread();

// Thread overrides
public:
    status_t requestExitAndWait();

// ICallbackPreview overrides
public:
    virtual void previewBufferCallback(AtomBuffer* buff, ICallbackPreview::CallbackType t);
    int getCameraID();

// Common methods
public:
    status_t returnBuffer(int id);
    status_t alloc(int size);
    status_t free(unsigned int idx);
    status_t map(unsigned int idx);
    status_t unmap(unsigned int idx);
    status_t setArgToBeSend(unsigned int idx);
    status_t configureIspStandalone(int mode);
    status_t load(unsigned int idx);
    status_t loadIspExtensions();
    status_t unloadIspExtensions();

// private types
private:

    // thread message id's
    enum MessageId {

        MESSAGE_ID_EXIT = 0,                 // call requestExitAndWait
        MESSAGE_ID_LOAD,                     // send from 'outside' via ControlThread's sendCommand(CAMERA_CMD_ACC_LOAD)
        MESSAGE_ID_ALLOC,                    // send from 'outside' via ControlThread's sendCommand(CAMERA_CMD_ACC_ALLOC)
        MESSAGE_ID_FREE,                     // send from 'outside' via ControlThread's sendCommand(CAMERA_CMD_ACC_FREE)
        MESSAGE_ID_MAP,                      // send from 'outside' via ControlThread's sendCommand(CAMERA_CMD_ACC_MAP)
        MESSAGE_ID_UNMAP,                    // send from 'outside' via ControlThread's sendCommand(CAMERA_CMD_ACC_UNMAP)
        MESSAGE_ID_SEND_ARG,                 // send from 'outside' via ControlThread's sendCommand(CAMERA_CMD_ACC_SEND_ARG)
        MESSAGE_ID_CONFIGURE_ISP_STANDALONE, // send from 'outside' via ControlThread's sendCommand(CAMERA_CMD_ACC_CONFIGURE_ISP_STANDALONE)
        MESSAGE_ID_RETURN_BUFFER,            // send from 'outside' via ControlThread's sendCommand(CAMERA_CMD_ACC_RETURN_BUFFER)

        MESSAGE_ID_LOAD_ISP_EXTENSIONS,      // only send from ControlThread
        MESSAGE_ID_UNLOAD_ISP_EXTENSIONS,    // only send from ControlThread

        MESSAGE_ID_FRAME,                    // used by previewBufferCallback

        // max number of messages
        MESSAGE_ID_MAX
    };

    struct MessageFirmware {
        unsigned int idx;
    };

    struct MessageAlloc {
        int size;
    };

    struct MessageMap {
        unsigned int idx;
    };

    struct MessageConfigureIspStandalone {
        int mode;
    };

    struct MessageBuffer {
        unsigned int idx;
    };

    struct MessageReturn {
        int idx;
    };

    struct MessageFrame {
        AtomBuffer img;
    };

    // union of all message data
    union MessageData {
        // MESSAGE_ID_LOAD
        MessageFirmware fw;
        // MESSAGE_ID_ALLOC
        MessageAlloc alloc;
        // MESSAGE_ID_MAP, MESSAGE_ID_UNMAP, MESSAGE_ID_SEND_ARG
        MessageMap map;
        // MESSAGE_ID_CONFIGURE_ISP_STANDALONE
        MessageConfigureIspStandalone configureIspStandalone;
        // MESSAGE_ID_FREE
        MessageBuffer buffer;
        // MESSAGE_ID_RETURN
        MessageReturn ret;
        // MESSAGE_ID_FRAME
        MessageFrame frame;
    };

    // message id and message data
    struct Message {
        MessageId id;
        MessageData data;
    };

    // A preview frame
    struct Frame {
        void* img_data;         // Raw image data
        int id;                 // Id for debugging data flow path
        int frameCounter;       // Frame counter. Reset upon preview_start
        int width;              // Pixel width of image
        int height;             // Pixel height of image
        int fourcc;             // XXX TODO not valid at preset
        int bpl;                // Bpl of the buffer
        int size;               // Size of img_data in bytes. For NV12 size==1.5*width*height
    };

    // struct holding arguments for ISP
    struct ArgumentBuffer {
        camera_memory_t mem;    // the allocated memory
        void* ptr;              // mapped pointer, valid on ISP
        bool toBeSend;          // if this buffer will be send to ISP
    };

    struct MyAtomBuffer {
        camera_memory_t mem;
        int frameCounter;
        bool inUse;
    };

// inherited from Thread
private:
    virtual bool threadLoop();

// private methods
private:
    status_t handleMessageFrame(MessageFrame msg);
    status_t handleMessageReturnBuffer(const MessageReturn& msg);
    status_t handleMessageAlloc(const MessageAlloc& msg);
    status_t handleMessageFree(const MessageBuffer& msg);
    status_t handleMessageMap(const MessageMap& msg);
    status_t handleMessageUnmap(const MessageMap& msg);
    status_t handleMessageSetArgToBeSend(const MessageMap& msg);
    status_t handleMessageConfigureIspStandalone(const MessageConfigureIspStandalone& msg);
    status_t handleMessageLoad(const MessageFirmware& msg);
    status_t handleMessageLoadIspExtensions();
    status_t handleMessageUnloadIspExtensions();
    status_t handleExit();

    status_t sendFirmwareArguments();

    // main message function
    status_t waitForAndExecuteMessage();

// private data
private:
    MessageQueue<Message, MessageId> mMessageQueue;
    Callbacks* mCallbacks;
    sp<CallbacksThread> mCallbacksThread;

    bool mThreadRunning;
    bool mFirmwareLoaded;
    IHWIspControl* mIspHandle;
    unsigned int mFirmwareHandle;
    void* mFirmwareBuffer;
    size_t mFirmwareBufferSize;

    Vector<ArgumentBuffer> mArgumentBuffers;

    MyAtomBuffer mFrameOne;
    MyAtomBuffer mFrameTwo;
    int mFramesGrabbed;

    Frame* mFrameMetadata;
    camera_memory_t mFrameMetadataBuffer;
    int mCameraId;
}; // class AccManagerThread

}; // namespace android

#endif // ANDROID_LIBCAMERA_ACCMANAGER_THREAD_H
