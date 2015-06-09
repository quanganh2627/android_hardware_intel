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

#include <NonCopyable.hpp>
#include <map>
#include <string>


class Value;
class ValueObserver;
class ValueRegister;

class ValueSet : private audio_comms::utilities::NonCopyable
{
public:
    ValueSet(ValueObserver *observer, ValueRegister *reg);
    virtual ~ValueSet();

protected:
    /**
     * Add a @see Value object to the value set. The value set will use its register handler
     * to add this @see Value to the @see ValueRegister.
     *
     * @param[in] value object to be added to the map of value to observer.
     */
    void addValue(Value *value);

    /**
     * Notify of a change on a @see Value object identified by its key.
     *
     * @param[in] key of the @see Value reporting the change event.
     */
    void notifyValueChange(const std::string &key);

    ValueObserver *mObserver; /**< handler on observer to notify of a value change. */
    ValueRegister *mRegister; /**< handler on register object to add new @see Value. */
};
