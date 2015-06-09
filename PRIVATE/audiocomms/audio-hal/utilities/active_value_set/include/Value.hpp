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
#include <string>

class Value : private audio_comms::utilities::NonCopyable
{
public:
    /**
     * Callback prototype for get value.
     *
     * @return value as a string.
     */
    typedef const std::string (Value::*GetValueCallback)(void *context) const;

public:
    Value(const std::string &key, GetValueCallback callback, void *context);

    /**
     * Returns the value of the Value Object
     *
     * @return literal value retrieved from the getter callback.
     */
    const std::string get() const;

    /**
     * Returns the key of the Value Object.
     *
     * @return key associated to this Value
     */
    const std::string &getKey() const { return mKey; }

private:
    GetValueCallback mGetValueCallback; /**< Getter callback function pointer. */
    std::string mKey; /**< Key associated to this value. */
    void *mContext; /**< Execution context to be given back by getter function pointer. */
};
