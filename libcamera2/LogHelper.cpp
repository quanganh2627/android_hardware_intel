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

#define LOG_TAG "Camera_LogHelper"

#include <stdint.h> // INT_MAX, INT_MIN
#include <stdlib.h> // atoi.h
#include <utils/Log.h>
#include <cutils/properties.h>

#include "LogHelper.h"
#include "PerformanceTraces.h"

int32_t gLogLevel = 0;
int32_t gPerfLevel = 0;
int32_t gPowerLevel = 0;
int32_t gControlLevel = 0;

using namespace android;

const char CameraParamsLogger::ParamsDelimiter[] = ";";
const char CameraParamsLogger::ValueDelimiter[]  = "=";

void android::LogHelper::setDebugLevel(void)
{
    char gLogLevelProp[PROPERTY_VALUE_MAX];
    char gPerfLevelProp[PROPERTY_VALUE_MAX];
    char gPowerLevelProp[PROPERTY_VALUE_MAX];
    char gControlLevelProp[PROPERTY_VALUE_MAX];
    PerformanceTraces::reset();

    if (property_get("camera.hal.debug", gLogLevelProp, NULL)) {
        gLogLevel = atoi(gLogLevelProp);
        ALOGD("Debug level is %d", gLogLevel);

        // Check that the property value is a valid integer
        if (gLogLevel >= INT_MAX || gLogLevel <= INT_MIN) {
            ALOGE("Invalid camera.hal.debug property integer value: %s",gLogLevelProp);
            gLogLevel = 0;
        }

        // legacy support: "setprop camera.hal.debug 2" is expected
        // to enable both LOG1 and LOG2 traces
        if (gLogLevel & CAMERA_DEBUG_LOG_LEVEL2)
            gLogLevel |= CAMERA_DEBUG_LOG_LEVEL1;
    }

    //Performance property
    if (property_get("camera.hal.perf", gPerfLevelProp, NULL)) {
        gPerfLevel = atoi(gPerfLevelProp);
        ALOGD("Performance level is %d", gPerfLevel);

        // Check that the property value is a valid integer
        if (gPerfLevel >= INT_MAX || gPerfLevel <= INT_MIN) {
            ALOGE("Invalid camera.hal.perf property integer value: %s",gPerfLevelProp);
            gPerfLevel = 0;
        }

        // bitmask of tracing categories
        if (gPerfLevel & CAMERA_DEBUG_LOG_PERF_TRACES) {
            PerformanceTraces::Launch2Preview::enable(true);
            PerformanceTraces::Launch2FocusLock::enable(true);
            PerformanceTraces::FaceLock::enable(true);
            PerformanceTraces::Shot2Shot::enable(true);
            PerformanceTraces::ShutterLag::enable(true);
            PerformanceTraces::SwitchCameras::enable(true);
            PerformanceTraces::HDRShot2Preview::enable(true);
        }

        if (gPerfLevel & CAMERA_DEBUG_LOG_PERF_TRACES_BREAKDOWN) {
            PerformanceTraces::PnPBreakdown::enable(true);
        }

        if (gPerfLevel & CAMERA_DEBUG_LOG_PERF_IO_BREAKDOWN) {
            PerformanceTraces::IOBreakdown::enableBD(true);
        }

        if (gPerfLevel & CAMERA_DEBUG_LOG_PERF_IO_MEMORY) {
            PerformanceTraces::IOBreakdown::enableMemInfo(true);
        }
    }

    //Power property
    if (property_get("camera.hal.power", gPowerLevelProp, NULL)) {
        gPowerLevel = atoi(gPowerLevelProp);
        ALOGD("Power level is %d", gPowerLevel);

        // Check that the property value is a valid integer
        if (gPowerLevel >= INT_MAX || gPowerLevel <= INT_MIN) {
            ALOGE("Invalid camera.hal.power property integer value: %s",gPowerLevelProp);
            gPowerLevel = 0;
        }
    }

    if (property_get("camera.hal.control", gControlLevelProp, NULL)) {
        gControlLevel = atoi(gControlLevelProp);
        ALOGD("Control level is %d", gControlLevel);

        // Check that the property value is a valid integer
        if (gControlLevel >= INT_MAX || gControlLevel <= INT_MIN) {
            ALOGE("Invalid camera.hal.control property integer value: %s",gControlLevelProp);
            gControlLevel = 0;
        }
    }
}

CameraParamsLogger::CameraParamsLogger(const char * params):mString(params) {

    fillMap(mPropMap, mString);
}

CameraParamsLogger::~CameraParamsLogger() {
    mPropMap.clear();
    mString.clear();
}

void
CameraParamsLogger::dump() const {
    LOG2("Dumping Camera Params");
    for (unsigned int i = 0; i < mPropMap.size(); i++) {
        LOG2("%s=%s",mPropMap.keyAt(i).string(), mPropMap.valueAt(i).string());
    }
}
void CameraParamsLogger::dumpDifference(const CameraParamsLogger &oldParams) const {
    String8 key_old, key_new, value_old, value_new;

    size_t i(0);
    size_t j(0);

    LOG1("Dumping camera params difference: size = %d -> %d", oldParams.mPropMap.size(), mPropMap.size());

    while (i < oldParams.mPropMap.size() && j < mPropMap.size() ) {
        key_old = oldParams.mPropMap.keyAt(i);
	key_new = mPropMap.keyAt(j);
	value_old = oldParams.mPropMap.valueAt(i);
	value_new = mPropMap.valueAt(j);
        if (key_old == key_new) {
            if (value_old != value_new) {
                LOG1("Value changed: %s: %s -> %s", key_old.string(), value_old.string(), value_new.string());
            }
            ++i;
            ++j;
        } else if (key_old < key_new) {
            LOG1("Deleted parameter: %s (%s)", key_old.string(), value_old.string());
            ++i;
        } else {
            LOG1("New parameter: %s: %s", key_new.string(), value_new.string());
            ++j;
        }
    }

    while (i < oldParams.mPropMap.size()) {
        key_old = oldParams.mPropMap.keyAt(i);
        value_old = oldParams.mPropMap.keyAt(i);
        LOG1("Deleted parameter: %s (%s)", key_old.string(), value_old.string());
        ++i;
    }

    while (j < mPropMap.size() ) {
        key_new = mPropMap.keyAt(j);
        value_new = mPropMap.keyAt(j);
        LOG1("New parameter: %s: %s", key_new.string(), value_new.string());
        ++j;
    }
}

int
CameraParamsLogger::splitParam(String8 &inParam , String8  &aKey, String8 &aValue) {

    ssize_t  start = 0, end = 0;

    end = inParam.find( ValueDelimiter, start);
    if (end == -1)
        return -1;

    aKey.setTo( &(inParam.string()[start]), end - start);

    start = end + 1;

    aValue.setTo(&(inParam.string()[start]), inParam.size() - start);

    return 0;
}

void
CameraParamsLogger::fillMap(KeyedVector<String8,String8> &aMap, String8 &aString) {

    ssize_t  start = 0, end = 0;
    String8 theParam;
    String8 aKey, aValue;

    while ( end != -1)
    {
       end = aString.find( ParamsDelimiter, start);

       theParam.setTo( &(aString.string()[start]),(end == -1) ? aString.size() - start : end - start);

       if(splitParam(theParam, aKey, aValue)) {
            ALOGE("Invalid Param: %s", theParam.string());
            start = end + 1;
            continue;
       }
       aMap.add(aKey, aValue);
       start = end + 1;
    }

}
