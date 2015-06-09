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
#define LOG_TAG "Camera_PostCaptureThread"
//#define LOG_NDEBUG 0

#include "LogHelper.h"
#include "PostCaptureThread.h"

namespace android {


PostCaptureThread::PostCaptureThread(IPostCaptureProcessObserver *anObserver):
    Thread(false) // callbacks will not call into java
    ,mMessageQueue("PostCaptureThread", (int) MESSAGE_ID_MAX)
    ,mThreadRunning(false)
    ,mObserver(anObserver)
    ,mCurrentTask(NULL)
    ,mBusy(false)
{

}

PostCaptureThread::~PostCaptureThread()
{

}

bool PostCaptureThread::isBusy()
{
    LOG1("@%s", __FUNCTION__);
    Mutex::Autolock _l(mBusyMutex);
    return mBusy;
}

status_t PostCaptureThread::handleExit()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    mThreadRunning = false;
    return status;
}

status_t PostCaptureThread::sendProcessItem(IPostCaptureProcessItem* item)
{
    LOG1("@%s", __FUNCTION__);
    Message msg;

    msg.id = MESSAGE_ID_PROCESS_ITEM;
    msg.data.procItem.item = item;

    return mMessageQueue.send(&msg);

}

status_t PostCaptureThread::handleProcessItem(MessageProcessItem &msg)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    { // define scope for the auto lock
        Mutex::Autolock _l(mBusyMutex);
        mBusy = true;
        mCurrentTask = msg.item;
    }

    status = mCurrentTask->process();

    mObserver->postCaptureProcesssingDone(mCurrentTask, status);

    { // define scope for the auto lock
        Mutex::Autolock _l(mBusyMutex);
        mBusy = false;
        mCurrentTask = NULL;
    }
    return status;
}

/**
 * Cancels the ongoing processing item
 *
 * This method runs in the context of the caller. It uses  the thread-safe method
 * cancelProcess  and then it sends a synchronous message to the PostCaptureThread
 * Q to make sure the current processing is completely cancel. It is the
 * ControlThread responsibility to cleanup the messages this may trigger
 * (i.e. MESSAGE_ID_POST_CAPTURE_PROCESSING_DONE)
 *
 * TODO: If in the future we allow more than 1 task in the Q we should remove any
 * new requests for processing here.
 *
 * \param item: Pointer to object of class IPostCaptureProcessItem to cancel
 *              if NULL is provided the current task is canceled.
 */
status_t PostCaptureThread::cancelProcessingItem(IPostCaptureProcessItem* item)
{
    LOG1("@%s", __FUNCTION__);
    Message msg;

    msg.id = MESSAGE_ID_CANCEL_PROCESS_ITEM;

    /**
     * Before sending the message to the thread we already warn the processing
     * item class that it has to cancel. This call has to be thread safe.
     *
     */
    if (item != NULL)
        item->cancelProcess();
    else if (mCurrentTask != NULL)
        mCurrentTask->cancelProcess();

    /**
     * Now we can send the message to make sure processing completes. This
     * message does nothing on the context of the thread, it just ensures that
     * the previous processing completed.
     */
    return mMessageQueue.send(&msg,MESSAGE_ID_CANCEL_PROCESS_ITEM);
}

status_t PostCaptureThread::handleCancelProcessItem()
{
    LOG1("@%s", __FUNCTION__);

    mMessageQueue.reply(MESSAGE_ID_CANCEL_PROCESS_ITEM, NO_ERROR);
    return NO_ERROR;
}
status_t PostCaptureThread::requestExitAndWait()
{
    LOG2("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_EXIT;
    // tell thread to exit
    // send message asynchronously
    mMessageQueue.send(&msg);

    // propagate call to base class
    return Thread::requestExitAndWait();
}

status_t PostCaptureThread::waitForAndExecuteMessage()
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    Message msg;
    mMessageQueue.receive(&msg);

    switch (msg.id)
    {
        case MESSAGE_ID_PROCESS_ITEM:
            status = handleProcessItem(msg.data.procItem);
            break;
        case MESSAGE_ID_CANCEL_PROCESS_ITEM:
            status = handleCancelProcessItem();
            break;
        case MESSAGE_ID_EXIT:
            status = handleExit();
            break;
        default:
            status = INVALID_OPERATION;
            break;
    }
    if (status != NO_ERROR) {
        ALOGE("operation failed, ID = %d, status = %d", msg.id, status);
    }
    return status;
}

bool PostCaptureThread::threadLoop()
{
    LOG2("@%s", __FUNCTION__);
    mThreadRunning = true;
    while(mThreadRunning)
        waitForAndExecuteMessage();

    return false;
}
} // namespace android
