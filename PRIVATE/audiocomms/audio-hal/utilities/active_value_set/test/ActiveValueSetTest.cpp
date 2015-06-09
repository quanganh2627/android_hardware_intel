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

#include "ActiveValueSetTest.hpp"
#include "ValueSetMock.hpp"
#include <gtest/gtest.h>

using ::testing::Return;
using ::testing::_;
using ::testing::InvokeWithoutArgs;

class ValuesVector
{
public:
    ValuesVector &operator<<(const TestPair &val)
    {
        mData.push_back(val);
        return *this;
    }
    operator std::vector<TestPair>() const {
        return mData;
    }

private:
    TestVector mData;
};

ActiveValueSetTest::ActiveValueSetTest()
{
    sem_init(&mSyncSem, 0, 0);
}

ActiveValueSetTest::~ActiveValueSetTest()
{
    sem_destroy(&mSyncSem);
}

void ActiveValueSetTest::notify()
{
    ASSERT_EQ(sem_post(&mSyncSem), 0);
}

void ActiveValueSetTest::waitNotification()
{
    ASSERT_EQ(sem_wait(&mSyncSem), 0);
}

TEST_P(ActiveValueSetTestVector, KeyValue1)
{
    TestVector testVector = GetParam();

    std::vector<std::string> valueKeysVector;
    TestVectorIterator iter;
    for (iter = testVector.begin(); iter != testVector.end(); ++iter) {
        TestPair myTestPair = *iter;
        std::string valueKeyToTest = myTestPair.first;

        std::vector<std::string>::iterator it =
            std::find(valueKeysVector.begin(), valueKeysVector.end(), valueKeyToTest);
        if (it == valueKeysVector.end()) {
            valueKeysVector.push_back(valueKeyToTest);
            std::cout << "Appending " << valueKeyToTest << std::endl;
        }
    }

    ActiveValueSetTest *activeValueSetTest = new ActiveValueSetTest();

    ValueSetMock *mValueSetMock = new ValueSetMock(valueKeysVector,
                                                   activeValueSetTest, activeValueSetTest);
    const std::string initValue("InitialValue");

    // Set up the mock for initial values retrieval and starts the active value set
    EXPECT_CALL(*mValueSetMock, getValueCallback(_))
    .WillRepeatedly(Return(initValue));
    EXPECT_CALL(*activeValueSetTest, onValueChanged(_, initValue))
    .Times(valueKeysVector.size());
    ASSERT_TRUE(activeValueSetTest->start());

    for (iter = testVector.begin(); iter != testVector.end(); ++iter) {
        TestPair myTestPair = *iter;
        std::string valueKeyToTest = myTestPair.first;
        std::string valueToTest = myTestPair.second;
        // Set Up the mock for all values getter within test vector.
        EXPECT_CALL(*mValueSetMock, getValueCallback(valueKeyToTest))
        .WillOnce(Return(valueToTest));

        // Set up the mock for ParameterAdapter to receive the key and the
        // value set in the getter mock
        EXPECT_CALL(*activeValueSetTest, onValueChanged(valueKeyToTest, valueToTest))
        .WillOnce(InvokeWithoutArgs(activeValueSetTest, &ActiveValueSetTest::notify));

        mValueSetMock->trigValueEventChange(valueKeyToTest);

        activeValueSetTest->waitNotification();
    }
    activeValueSetTest->stop();

    delete mValueSetMock;
    delete activeValueSetTest;
}

INSTANTIATE_TEST_CASE_P(
    ActiveValueSetTestVectorAll,
    ActiveValueSetTestVector,
    ::testing::Values(
        ValuesVector() << std::make_pair("KeyVal1", "my string")
                       << std::make_pair("KeyVal1", std::string(1024, 'p'))
                       << std::make_pair("KeyVal1", ""),
        ValuesVector() << std::make_pair("KeyVal2", "my string")
                       << std::make_pair("KeyVal2", std::string(1024, 'p'))
                       << std::make_pair("KeyVal2", ""),
        ValuesVector() << std::make_pair("KeyVal3", "my string")
                       << std::make_pair("KeyVal3", std::string(1024, 'p'))
                       << std::make_pair("KeyVal3", ""),
        ValuesVector() << std::make_pair("KeyVal1", "my string")
                       << std::make_pair("KeyVal2", std::string(1024, 'p'))
                       << std::make_pair("KeyVal1", "new value")
                       << std::make_pair("KeyVal3", ""),
        ValuesVector() << std::make_pair(std::string(1024, 'p'), "my string")
                       << std::make_pair("KeyValN", std::string(1024, 'p'))
                       << std::make_pair("Key Value 3", "")
        )
    );



TEST(ActiveValueSetTest, StartActiveValueSetBeforeAddValue)
{
    ActiveValueSetTest activeValueSetTest;
    ASSERT_TRUE(activeValueSetTest.start());

    const std::string valueKey("KeyVal1");
    const std::string initValue("InitialValue");

    std::vector<std::string> myTestVector;
    myTestVector.push_back(valueKey);
    ValueSetMock *mValueSetMock = new ValueSetMock(myTestVector,
                                                   &activeValueSetTest,
                                                   &activeValueSetTest);

    EXPECT_CALL(*mValueSetMock, getValueCallback(valueKey))
    .WillOnce(Return(initValue));

    // Set up the mock for ParameterAdapter to receive the key and the
    // value set in the getter mock
    EXPECT_CALL(activeValueSetTest, onValueChanged(valueKey, initValue))
    .WillOnce(InvokeWithoutArgs(&activeValueSetTest, &ActiveValueSetTest::notify));

    // trig the event change for the value key
    mValueSetMock->trigValueEventChange(valueKey);

    activeValueSetTest.waitNotification();

    activeValueSetTest.stop();

    delete mValueSetMock;
}

TEST(ActiveValueSetTest, TrigAValueChangedWhileActiveValueSetNotStarted)
{
    ActiveValueSetTest *activeValueSetTest = new ActiveValueSetTest();
    const std::string valueKey("KeyVal1");

    std::vector<std::string> myTestVector;
    myTestVector.push_back(valueKey);
    ValueSetMock *mValueSetMock = new ValueSetMock(myTestVector,
                                                   activeValueSetTest,
                                                   activeValueSetTest);

    EXPECT_CALL(*activeValueSetTest, onValueChanged(_, _))
    .Times(0);

    // trig the event change for Value key
    mValueSetMock->trigValueEventChange(valueKey);

    delete mValueSetMock;
    delete activeValueSetTest;
}

TEST(ActiveValueSetTest, StartTwiceStopTwiceActiveValueSet)
{
    ActiveValueSetTest *activeValueSetTest = new ActiveValueSetTest();

    EXPECT_TRUE(activeValueSetTest->start());

    EXPECT_FALSE(activeValueSetTest->start());

    activeValueSetTest->stop();

    activeValueSetTest->stop();

    delete activeValueSetTest;
}

TEST(ActiveValueSetTest, StartAndStopTwiceActiveValueSet)
{
    ActiveValueSetTest *activeValueSetTest = new ActiveValueSetTest();

    EXPECT_TRUE(activeValueSetTest->start());

    activeValueSetTest->stop();

    EXPECT_TRUE(activeValueSetTest->start());

    activeValueSetTest->stop();

    delete activeValueSetTest;
}
