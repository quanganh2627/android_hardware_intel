/*
 * INTEL CONFIDENTIAL
 *
 * Copyright (c) 2014 Intel Corporation All Rights Reserved.
 *
 * The source code contained or described herein and all documents related to
 * the source code ("Material") are owned by Intel Corporation or its suppliers
 * or licensors.
 *
 * Title to the Material remains with Intel Corporation or its suppliers and
 * licensors. The Material contains trade secrets and proprietary and
 * confidential information of Intel or its suppliers and licensors. The
 * Material is protected by worldwide copyright and trade secret laws and treaty
 * provisions. No part of the Material may be used, copied, reproduced,
 * modified, published, uploaded, posted, transmitted, distributed, or disclosed
 * in any way without Intel's prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 */
#define LOG_TAG "ActiveValueSet"


#include "ActiveValueSet.hpp"
#include "Value.hpp"
#include <EventThread.h>
#include <AudioCommsAssert.hpp>
#include <utilities/Log.hpp>

using audio_comms::utilities::Log;

ActiveValueSet::ActiveValueSet()
    : mEventThread(new CEventThread(this))
{
}

ActiveValueSet::~ActiveValueSet()
{
    stop();
    // Delete all ValueSet
    ValueMapIterator it;
    for (it = mValueMap.begin(); it != mValueMap.end(); ++it) {

        delete it->second;
    }
    delete mEventThread;
}

bool ActiveValueSet::start()
{
    if (mEventThread->isStarted()) {
        Log::Warning() << __FUNCTION__ << ": already started";
        return false;
    }
    if (!mEventThread->start()) {
        Log::Error() << __FUNCTION__ << ": could not start event thread";
        return false;
    }
    // Loops on Values to get and report the initial values
    ValueMapIterator it;
    for (it = mValueMap.begin(); it != mValueMap.end(); ++it) {
        Value *value = it->second;
        string key(it->first);
        AUDIOCOMMS_ASSERT(value != NULL, "Invalid value (key=" << key << ")");
        onValueChanged(key, value->get());
    }
    return true;
}

void ActiveValueSet::stop()
{
    if (!mEventThread->isStarted()) {
        Log::Warning() << __FUNCTION__ << ":already stopped";
        return;
    }
    mEventThread->stop();
}

bool ActiveValueSet::onProcess(void *context, uint32_t)
{
    Value *value = static_cast<Value *>(context);
    AUDIOCOMMS_ASSERT(value != NULL, "NULL context");
    onValueChanged(value->getKey(), value->get());
    return false;
}

void ActiveValueSet::onValueChanged(const std::string &key)
{
    AUDIOCOMMS_ASSERT(mValueMap.find(key) != mValueMap.end(),
                      "Value (key=) " << key << " not found");
    const Value *value = mValueMap[key];

    AUDIOCOMMS_ASSERT(value != NULL, "Value (Key=) " << key << " not found");

    // Serve the value change notification within our own thread context, so auto trig
    // the event thread
    if (!mEventThread->isStarted()) {
        Log::Info() << __FUNCTION__
                    << ": could not report value changes as event thread is not started";
        return;
    }
    mEventThread->trig((void *)value);
}

void ActiveValueSet::addValue(Value *value)
{
    AUDIOCOMMS_ASSERT(value != NULL, "Trying to add invalid Value");
    std::string key = value->getKey();
    AUDIOCOMMS_ASSERT(mValueMap.find(key) == mValueMap.end(),
                      "Value (key=) " << key << " already added");
    mValueMap[key] = value;
}
