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
#define LOG_TAG "Camera_CallbacksThread"

#include "CallbacksThread.h"
#include "LogHelper.h"
#include "Callbacks.h"

namespace android {

CallbacksThread::CallbacksThread() :
    Thread(true) // callbacks may call into java
    ,mMessageQueue("CallbacksThread", MESSAGE_ID_MAX)
    ,mThreadRunning(false)
    ,mCallbacks(NULL)
{
    LOG1("@%s", __FUNCTION__);
}

CallbacksThread::~CallbacksThread()
{
    LOG1("@%s", __FUNCTION__);
    if (mCallbacks.get())
        mCallbacks.clear();
}

status_t CallbacksThread::shutterSound()
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_CALLBACK_SHUTTER;
    return mMessageQueue.send(&msg);
}

status_t CallbacksThread::handleMessageExit()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    mThreadRunning = false;
    return status;
}

status_t CallbacksThread::handleMessageCallbackShutter()
{
    LOG1("@%s", __FUNCTION__);
    mCallbacks->shutterSound();
    return NO_ERROR;
}

status_t CallbacksThread::waitForAndExecuteMessage()
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    Message msg;
    mMessageQueue.receive(&msg);

    switch (msg.id) {
        case MESSAGE_ID_EXIT:
            status = handleMessageExit();
            break;
        case MESSAGE_ID_CALLBACK_SHUTTER:
            status = handleMessageCallbackShutter();
            break;
        default:
            status = BAD_VALUE;
            break;
    };
    return status;
}

bool CallbacksThread::threadLoop()
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    mThreadRunning = true;
    while (mThreadRunning)
        status = waitForAndExecuteMessage();

    return false;
}

status_t CallbacksThread::requestExitAndWait()
{
    LOG1("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_EXIT;

    // tell thread to exit
    // send message asynchronously
    mMessageQueue.send(&msg);

    // propagate call to base class
    return Thread::requestExitAndWait();
}

} // namespace android
