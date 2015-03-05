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

#ifndef ANDROID_LIBCAMERA_POST_CAPTURE_THREAD_H_
#define ANDROID_LIBCAMERA_POST_CAPTURE_THREAD_H_

#include <utils/threads.h>
#include <time.h>
#include "MessageQueue.h"
#include "AtomCommon.h"

namespace android {
/**
 * \class  IPostCaptureProcessItem
 *
 * Virtual Interface implemented by the different algorithms run after capture
 * It allows them to run the in the PotCaptureThread
 *
 * The cancel call needs to be thread safe, since it is normally called from a
 * different thread than the processing thread. This method should signal the
 * algorithm so that the processing stops soon or when it completes, the correct
 * actions are taken.
 *
 */
class IPostCaptureProcessItem {
public:
    IPostCaptureProcessItem() {}
    virtual ~IPostCaptureProcessItem() {}
    virtual status_t process() = 0;
    virtual status_t cancelProcess() = 0;
};

/**
 * \class IPostCaptureProcessObserver
 *
 * Virtual Interface implemented by the ControlThread to receive notification
 * that a post capture processing item has completed.
 */
class IPostCaptureProcessObserver {
public:
    IPostCaptureProcessObserver() {}
    virtual ~IPostCaptureProcessObserver() {}
    virtual void postCaptureProcesssingDone(IPostCaptureProcessItem* item, status_t status) = 0;
};


/**
 * \class PostCaptureThread
 *
 * The PostCaptureThread is in charge of performing post-processing operations
 * on the captured images prior to the JPEG encoding process.
 *
 * It has a similar function to the PostProcThread, but this one operates only
 * on frames from the preview, where the PostCaptureThread operates on actual
 * full-res snapshots buffers.
 *
 * The operations on snapshots have to be encapsulated on classes derived
 * from IPostCaptureProcessItem
 *
 * The normal operation is:
 * 1.- ControlThread captures images
 * 2.- A class derived from IPostCaptureProcessItem stores the snapshots
 *     This class is specific for each post-processing algorithm
 * 3.- ControlThread sends the algo class to PostCaptureThread
 * 4.- Processing happens in the context of this thread.
 * 5.- Processing completes and thread calls back to
 *     IPostCaptureProcessObserver::postCaptureProcessingDone()
 * 6.- JPEG encoding for the output of the algorithm is triggered.
 *
 */
class PostCaptureThread : public Thread {
public:
    PostCaptureThread(IPostCaptureProcessObserver* obs);
    virtual ~PostCaptureThread();

    status_t sendProcessItem(IPostCaptureProcessItem* item);
    status_t cancelProcessingItem(IPostCaptureProcessItem* item = NULL);
    bool isBusy();
    // Thread class overrides
    status_t requestExitAndWait();

// prevent copy constructor and assignment operator
private:
    PostCaptureThread(const  PostCaptureThread& other);
    PostCaptureThread& operator=(const PostCaptureThread& other);

private:
    // thread message id's
    enum MessageId {

        MESSAGE_ID_EXIT = 0,            // call requestExitAndWait
        MESSAGE_ID_PROCESS_ITEM,
        MESSAGE_ID_CANCEL_PROCESS_ITEM,

        // max number of messages
        MESSAGE_ID_MAX
    };

    //
    // message data structures
    //
    struct MessageProcessItem {
       IPostCaptureProcessItem *item;
    };

    // union of all message data
    union MessageData {
        // MESSAGE_ID_FRAME
        MessageProcessItem procItem;
    };

    // message id and message data
    struct Message {
        MessageId id;
        MessageData data;
    };

private:
    // inherited from Thread
    virtual bool threadLoop();
    // main message function
    status_t waitForAndExecuteMessage();
    // Message processing methods
    status_t handleProcessItem(MessageProcessItem &msg);
    status_t handleCancelProcessItem();
    status_t handleExit();

private:
    MessageQueue<Message, MessageId> mMessageQueue;
    bool mThreadRunning;
    IPostCaptureProcessObserver *mObserver;
    IPostCaptureProcessItem     *mCurrentTask;

    bool mBusy;     /*!< Flag to signal and ongoing process is currently running
                         queries to this boolean must be protected with mutex */
    Mutex mBusyMutex;

};
}  // namespace android
#endif /* ANDROID_LIBCAMERA_POST_CAPTURE_THREAD_H_ */
