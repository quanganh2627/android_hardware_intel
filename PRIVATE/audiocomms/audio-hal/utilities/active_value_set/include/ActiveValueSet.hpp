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
#pragma once

#include "ValueObserver.hpp"
#include "ValueRegister.hpp"
#include <EventListener.h>
#include <NonCopyable.hpp>
#include <string>
#include <map>

class Value;
class CEventThread;

class ActiveValueSet
    : public ValueObserver,
      public ValueRegister,
      private IEventListener,
      private audio_comms::utilities::NonCopyable
{
private:
    typedef std::map<std::string, Value *> ValueMap;
    typedef ValueMap::iterator ValueMapIterator;
    typedef ValueMap::const_iterator ValueMapConstIterator;

public:
    ActiveValueSet();
    ~ActiveValueSet();

    /**
     * Callback to report a change on a value.
     *
     * @param[in] key of the @see Value object that reported a change.
     * @param[in] value that @see Value object has reported.
     */
    virtual void onValueChanged(const std::string &key, const std::string &value) = 0;

    /**
     * Starts the Active Value Set. On startup, it will get the initial value on all @see Value
     * added to the ActiveValueSet. It will synchronously report them to the client.
     *
     * @return true if started successfuly, false otherwise
     */
    bool start();

    /**
     * Stop the service.
     */
    void stop();

private:
    /**
     * Add a @see Value object to the active value set.
     *
     * @param[in] value object to be added to the map of @see Value to observer.
     */
    virtual void addValue(Value *value);

    virtual void onValueChanged(const std::string &key);
    virtual bool onEvent(int) { return false; }
    virtual bool onError(int) { return false; }
    virtual bool onHangup(int) { return false; }
    virtual void onAlarm() {}
    virtual void onPollError() {}
    virtual bool onProcess(void *context, uint32_t);

private:
    CEventThread *mEventThread; /**< Worker Thread. */
    std::map<std::string, Value *> mValueMap; /**< Values Map indexed by the ValueSet Id. */
};
