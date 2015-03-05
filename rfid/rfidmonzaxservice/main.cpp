/*
 *  Copyright (C) 2013 Intel Corporation
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
#define LOG_TAG "rfid_monzaxd"

#include <stdio.h>
#include <sys/prctl.h>
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include "MonzaxService.h"

using namespace android;

int main(int argc, char** argv)
{
    ALOGI("rfid monzax daemon launched (pid=%d)", getpid());
    MonzaxService::instantiate();
    ProcessState::self()->startThreadPool();
    IPCThreadState::self()->joinThreadPool();
    return EXIT_SUCCESS;
}

