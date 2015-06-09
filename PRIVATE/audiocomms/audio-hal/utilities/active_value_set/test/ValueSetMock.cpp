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

#include "ValueSetMock.hpp"
#include "ValueMock.hpp"
#include <stdint.h>
#include <gtest/gtest.h>


ValueSetMock::ValueSetMock(const std::vector<std::string> &keyValues,
                           ValueObserver *observer,
                           ValueRegister *reg)
    : ValueSet(observer, reg)
{
    std::vector<std::string>::const_iterator iter;
    for (iter = keyValues.begin(); iter != keyValues.end(); ++iter) {

        // Creates its own values and add them
        ValueMock *valueMock = new ValueMock(this, *iter);
        addValue(valueMock->getValue());
    }
}

void ValueSetMock::trigValueEventChange(const std::string &valueKey)
{
    notifyValueChange(valueKey);
}
