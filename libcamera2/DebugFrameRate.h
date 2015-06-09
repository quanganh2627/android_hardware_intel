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

#ifndef ANDROID_LIBCAMERA_DEBUG_FPS
#define ANDROID_LIBCAMERA_DEBUG_FPS

#include <utils/threads.h>
#include <utils/Timers.h>
#include "LogHelper.h"

namespace android {

class DebugFrameRate : public Thread {

public:
    DebugFrameRate();
    ~DebugFrameRate();

    void update();
    status_t requestExitAndWait();  // override
    status_t run();                 // override

private:
    virtual bool threadLoop();

private:

    static const int WAIT_TIME_NSECS = 2000000000; // 2 seconds

    int mCount;
    nsecs_t mStartTime;
    Condition mCondition;
    Mutex mMutex;
    bool mActive;
};

};
#endif // ANDROID_LIBCAMERA_DEBUG_FPS
