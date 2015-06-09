/*
 * Copyright 2013-2014 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include "GcovFlushWithProp.hpp"
#include <Property.h>
#define LOG_TAG "GcovFlushWithProp"
#include <utils/Log.h>

extern "C" void __gcov_flush();

GcovFlushWithProp::GcovFlushWithProp()
    : stopThread(false)
{
    // On constructor start the thread
    pthread_create(&thread, NULL, staticThread, this);
}

GcovFlushWithProp::~GcovFlushWithProp()
{
    // Ask to stop the thread
    stopThread = true;
    pthread_join(thread, NULL);

    // Force gcov flush at the end of the main or shared lib
    // But this code is never call in a system shared lib
    ALOGD("GcovFlushWithProp __gcov_flush begin");
    __gcov_flush();
    ALOGD("GcovFlushWithProp __gcov_flush end");
}

GcovFlushWithProp &GcovFlushWithProp::getInstance()
{
    static GcovFlushWithProp instance;
    return instance;
}

void *GcovFlushWithProp::staticThread(void *that)
{
    static_cast<GcovFlushWithProp *>(that)->run();
    return NULL;
}

void GcovFlushWithProp::run()
{
    // Use the property "gcov.flush.force" to communicate between the Android
    // shell and this thread.
    TProperty<bool> gcovFlushForceProp = TProperty<bool>("gcov.flush.force", false);
    bool hasFlush = true;
    while (!stopThread) {
        // Read the property value to take into account Android shell modification
        bool gcovFlushForcePropValue = gcovFlushForceProp.getValue();
        // If the property become true
        if (!hasFlush && gcovFlushForcePropValue) {
            // gcov flush
            ALOGD("GcovFlushWithProp __gcov_flush begin !!!");
            __gcov_flush();
            hasFlush = true;
            ALOGD("GcovFlushWithProp __gcov_flush end !!!");
        }
        // If the property become false
        else if (hasFlush && !gcovFlushForcePropValue) {
            hasFlush = false;
            ALOGD("GcovFlushWithProp wait property \"gcov.flush.force\" is \"true\"");
        }
        // Wait a little between property read
        // to let the processor to others threads.
        // 1 second is a good compromised between the consumed time by the
        // processor during all test and the reacted time of the manual
        // modification of this property.
        sleep(1);
    }
}
