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

#include <ActiveValueSet.hpp>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <semaphore.h>

class ValueSetMock;

class ActiveValueSetTest : public ActiveValueSet
{
public:
    ActiveValueSetTest();
    virtual ~ActiveValueSetTest();

    /**
     * Notification callback from the Subject observed
     */
    void notify();

    /**
     * Synchronously wait a notification from the Subject observed
     */
    void waitNotification();

    MOCK_METHOD2(onValueChanged, void(const std::string &key, const std::string &value));

private:
    sem_t mSyncSem; /**< Synchronization semaphore. */
};

typedef std::pair<std::string, std::string> TestPair;
typedef std::vector<TestPair> TestVector;
typedef std::vector<TestPair>::iterator TestVectorIterator;
typedef std::vector<TestPair>::const_iterator TestVectoConstIterator;

/**
 * Helper class for parameterized test on a vector of pair key to test, value to inject.
 */
class ActiveValueSetTestVector
    : public ActiveValueSetTest,
      public ::testing::Test,
      public ::testing::WithParamInterface<std::vector<TestPair> >
{
};
