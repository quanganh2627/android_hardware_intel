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
#define LOG_TAG "Camera_DebugFrameRate"

#include <utils/threads.h>
#include <utils/Log.h>
#include <time.h>
#include "DebugFrameRate.h"

namespace android {

DebugFrameRate::DebugFrameRate() :
    Thread(false)
    ,mCount(0)
    ,mStartTime(0)
    ,mActive(false)
{

    if (gPerfLevel & CAMERA_DEBUG_LOG_PERF_TRACES) {
        mActive = true;
    }
}

DebugFrameRate::~DebugFrameRate()
{
}

status_t DebugFrameRate::run()
{
   if(mActive)
       return Thread::run("CamHAL_DEBFPS");

   return NO_ERROR;
}

void DebugFrameRate::update()
{
    mMutex.lock();
    ++mCount;
    mMutex.unlock();
}

status_t DebugFrameRate::requestExitAndWait()
{
    if(!mActive)
        return NO_ERROR;

    mMutex.lock();
    mCondition.signal();
    mMutex.unlock();

    return Thread::requestExitAndWait();
}

bool DebugFrameRate::threadLoop()
{
    status_t status;

    while (1) {
        mMutex.lock();
        mCount = 0;
        mStartTime = systemTime();
        status = mCondition.waitRelative(mMutex, WAIT_TIME_NSECS);

        if (status == 0) {
            ALOGD("Exiting...\n");
            mMutex.unlock();
            return false;
        }

        // compute stats
        double delta = (systemTime() - mStartTime) / 1000000000.0;
        float fps;

        delta = delta < 0.0 ? -delta : delta; // make sure is positive
        fps = mCount / delta;

        ALOGD("time: %f seconds, frames: %d, fps: %f\n", (float) delta, mCount, fps);
        mMutex.unlock();
    }

    return false;
}

} // namespace android
