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

#ifndef LOGHELPER_H_
#define LOGHELPER_H_


#include <utils/Log.h>
#include <cutils/atomic.h>

static int32_t gLogLevel = 1;

static void setLogLevel(int level) {
    android_atomic_write(level, &gLogLevel);
}

#define LOG1(...) ALOGD_IF(gLogLevel >= 1, __VA_ARGS__);
#define LOG2(...) ALOGD_IF(gLogLevel >= 2, __VA_ARGS__);

#ifndef ALOGE
    #define ALOGE LOGE
#endif
#ifndef ALOGI
    #define ALOGI LOGI
#endif
#ifndef ALOGV
    #define ALOGV LOGV
#endif

#endif /* LOGHELPER_H_ */
