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

#include "KeyValuePairsTest.hpp"
#include <KeyValuePairs.hpp>
#include <string>
#include <gtest/gtest.h>


using namespace intel_audio;
using namespace std;

TEST(KeyValuePairsTest, ConstructWithKey)
{
    string keyToTest = "dummykey";

    KeyValuePairs pairs(keyToTest);
    EXPECT_EQ(1u, pairs.size());
    string returnValue;
    ASSERT_EQ(android::OK, pairs.get(keyToTest, returnValue));

    EXPECT_TRUE(returnValue.empty());

    EXPECT_EQ(pairs.toString(), keyToTest + "=");
}


TEST(KeyValuePairsTest, AddOnlyKey)
{
    string keyToTest = "dummykey";

    KeyValuePairs pairs;
    EXPECT_EQ(0u, pairs.size());
    ASSERT_EQ(android::OK, pairs.add(keyToTest));
    EXPECT_EQ(1u, pairs.size());
    string returnValue;
    ASSERT_EQ(android::OK, pairs.get(keyToTest, returnValue));

    EXPECT_TRUE(returnValue.empty());

    EXPECT_EQ(pairs.toString(), keyToTest + "=");
}

TEST(KeyValuePairsTest, ConstructWithKeyAndValue)
{
    string keyToTest = "dummykey";
    string valueToTest = "dummy";

    KeyValuePairs pairs(keyToTest + "=" + valueToTest);
    EXPECT_EQ(1u, pairs.size());
    string returnValue;
    ASSERT_EQ(android::OK, pairs.get(keyToTest, returnValue));

    EXPECT_EQ(valueToTest, returnValue);

    EXPECT_EQ(pairs.toString(), keyToTest + "=" + valueToTest);
}

TEST(KeyValuePairsTest, EmptyConstructThenAddRemoveParamKey)
{
    string keyToTest = "dummykey";
    string valueToTest = "dummy";

    KeyValuePairs pairs;
    EXPECT_EQ(0u, pairs.size());
    ASSERT_EQ(android::OK, pairs.add(keyToTest, valueToTest));
    EXPECT_EQ(1u, pairs.size());
    string returnValue;
    ASSERT_EQ(android::OK, pairs.get(keyToTest, returnValue));

    EXPECT_EQ(valueToTest, returnValue);

    ASSERT_EQ(android::OK, pairs.remove(keyToTest));
    EXPECT_EQ(0u, pairs.size());
    EXPECT_EQ(android::BAD_VALUE, pairs.get(keyToTest, returnValue));
}

TEST(KeyValuePairsTest, AddKeyTwice)
{
    string keyToTest = "dummykey";
    string valueToTest = "dummy";

    KeyValuePairs pairs;
    EXPECT_EQ(0u, pairs.size());
    ASSERT_EQ(android::OK, pairs.add(keyToTest, valueToTest));
    EXPECT_EQ(1u, pairs.size());

    string returnValue;
    ASSERT_EQ(android::OK, pairs.get(keyToTest, returnValue));

    EXPECT_EQ(valueToTest, returnValue);

    EXPECT_EQ(android::ALREADY_EXISTS, pairs.add(keyToTest, valueToTest));
    EXPECT_EQ(1u, pairs.size())
        << "Size of the collection shall remains the same if same key added twice";
}

TEST(KeyValuePairsTest, ConstructWithKeyAndAddParamKeyTwice)
{
    string keyToTest = "dummykey";
    string valueToTest = "dummy";

    KeyValuePairs pairs(keyToTest);
    EXPECT_EQ(1u, pairs.size());
    EXPECT_EQ(android::ALREADY_EXISTS, pairs.add(keyToTest, valueToTest));
    EXPECT_EQ(1u, pairs.size())
        << "Size of the collection shall remains the same if same key added twice";
}


TEST(KeyValuePairsTest, RemoveTwiceParamKey)
{
    string keyToTest = "dummykey";
    string valueToTest = "dummy";

    KeyValuePairs pairs;

    ASSERT_EQ(android::OK, pairs.add(keyToTest, valueToTest));
    EXPECT_EQ(1u, pairs.size());
    string returnValue;
    ASSERT_EQ(android::OK, pairs.get(keyToTest, returnValue));

    EXPECT_EQ(valueToTest, returnValue);

    ASSERT_EQ(android::OK, pairs.remove(keyToTest));
    EXPECT_EQ(0u, pairs.size());

    EXPECT_EQ(android::BAD_VALUE, pairs.remove(keyToTest));
    EXPECT_EQ(0u, pairs.size());
}

TEST(KeyValuePairsTest, MultipleKey)
{
    string keyToTest1 = "dummykey1";
    string keyToTest2 = "dummykey2";
    string keyToTest3 = "dummykey3";
    string valueToTest1 = "dummy";
    string valueToTest2 = "second dummy";
    string valueToTest3 = "third_dummy";

    KeyValuePairs pairs;

    ASSERT_EQ(android::OK, pairs.add(keyToTest1, valueToTest1));
    ASSERT_EQ(android::OK, pairs.add(keyToTest2, valueToTest2));
    ASSERT_EQ(android::OK, pairs.add(keyToTest3, valueToTest3));
    EXPECT_EQ(3u, pairs.size());

    string returnValue;
    ASSERT_EQ(android::OK, pairs.get(keyToTest1, returnValue));
    EXPECT_EQ(valueToTest1, returnValue);

    ASSERT_EQ(android::OK, pairs.get(keyToTest2, returnValue));
    EXPECT_EQ(valueToTest2, returnValue);

    ASSERT_EQ(android::OK, pairs.get(keyToTest3, returnValue));
    EXPECT_EQ(valueToTest3, returnValue);

    returnValue = pairs.toString();
    EXPECT_NE(string::npos, returnValue.find(keyToTest1 + "=" + valueToTest1))
        << "Could not find key-value pair for key" << keyToTest1;
    EXPECT_NE(string::npos, returnValue.find(keyToTest2 + "=" + valueToTest2))
        << "Could not find key-value pair for key" << keyToTest2;
    EXPECT_NE(string::npos, returnValue.find(keyToTest3 + "=" + valueToTest3))
        << "Could not find key-value pair for key" << keyToTest3;
}


TEST(KeyValuePairsTest, MultiplePairs)
{
    const string keyToTest1 = "A";
    const string keyToTest2 = "B";
    const string keyToTest3 = "C";
    const string valueToTest1 = "dummy";
    const string valueToTest2 = "second dummy";
    const string valueToTest3 = "thrid_dummy";

    KeyValuePairs pairs;

    string keyValuePairs = keyToTest1 + "=" + valueToTest1 + ";" +
                           keyToTest2 + "=" + valueToTest2;
    EXPECT_EQ(android::OK, pairs.add(keyValuePairs));
    EXPECT_EQ(2u, pairs.size());

    string returnValue;
    ASSERT_EQ(android::OK, pairs.get(keyToTest1, returnValue));
    EXPECT_EQ(valueToTest1, returnValue);
    ASSERT_EQ(android::OK, pairs.get(keyToTest2, returnValue));
    EXPECT_EQ(valueToTest2, returnValue);
    EXPECT_EQ(android::BAD_VALUE, pairs.get(keyToTest3, returnValue));

    const string newValueToTest1 = "updatedValueForKey1";
    const string newValueToTest2 = "updatedValueForKey2";

    keyValuePairs = keyToTest1 + "=" + newValueToTest1 + ";" +
                    keyToTest2 + "=" + newValueToTest2 + ";" +
                    keyToTest3 + "=" + valueToTest3 + ";";

    EXPECT_EQ(android::ALREADY_EXISTS, pairs.add(keyValuePairs));
    EXPECT_EQ(3u, pairs.size());

    ASSERT_EQ(android::OK, pairs.get(keyToTest1, returnValue));
    EXPECT_EQ(newValueToTest1, returnValue);
    ASSERT_EQ(android::OK, pairs.get(keyToTest2, returnValue));
    EXPECT_EQ(newValueToTest2, returnValue);
    ASSERT_EQ(android::OK, pairs.get(keyToTest3, returnValue));
    EXPECT_EQ(valueToTest3, returnValue);
}

TEST(KeyValuePairsTest, EmptyKey)
{
    KeyValuePairs pairs;

    // key is intentionnaly kept empty
    const string keyToTest1;
    const string valueToTest1 = "dummy";
    string keyValuePairs = keyToTest1 + "=" + valueToTest1;
    EXPECT_EQ(android::BAD_VALUE, pairs.add(keyValuePairs));
    EXPECT_EQ(0u, pairs.size());
}

TEST(KeyValuePairsTest, EmptyKeyValue)
{
    KeyValuePairs pairs;

    // key is intentionnaly kept empty
    EXPECT_EQ(android::BAD_VALUE, pairs.add("="));
    EXPECT_EQ(0u, pairs.size());
}

TEST(KeyValuePairsTest, WrongConversion)
{
    string keyToTest = "dummykey";
    string valueToTest = "123u";

    KeyValuePairs parameter;

    ASSERT_EQ(android::OK, parameter.add(keyToTest, valueToTest));

    string returnValue;
    ASSERT_EQ(android::OK, parameter.get(keyToTest, returnValue));

    EXPECT_EQ(valueToTest, returnValue);

    uint32_t numericalValue;
    EXPECT_EQ(android::BAD_VALUE, parameter.get(keyToTest, numericalValue));
}

TEST_P(KeyValuePairsTestInt, uint32_t)
{
    const string key = "dummykey";
    uint32_t valueToTest = GetParam();

    KeyValuePairs parameter;

    ASSERT_EQ(android::OK, parameter.add(key, valueToTest));

    uint32_t returnValue = 0;
    ASSERT_EQ(android::OK, parameter.get(key, returnValue));

    EXPECT_EQ(valueToTest, returnValue);
}

INSTANTIATE_TEST_CASE_P(
    KeyValuePairsTestIntAll,
    KeyValuePairsTestInt,
    ::testing::Values(0,
                      999u,
                      std::numeric_limits<uint32_t>::min(),
                      666u,
                      std::numeric_limits<uint32_t>::max())
    );


TEST_P(KeyValuePairsTestString, string)
{
    const string key = "dummykey";
    string valueToTest = GetParam();

    KeyValuePairs parameter;

    ASSERT_EQ(android::OK, parameter.add(key, valueToTest));

    string returnValue;
    ASSERT_EQ(android::OK, parameter.get(key, returnValue));

    EXPECT_EQ(valueToTest, returnValue);
}

INSTANTIATE_TEST_CASE_P(
    KeyValuePairsTestStringAll,
    KeyValuePairsTestString,
    ::testing::Values("",
                      "dummy",
                      std::string(1024, 'c')
                      )
    );

TEST_P(KeyValuePairsTestFloat, float)
{
    const string key = "dummykey";
    float valueToTest = GetParam();

    KeyValuePairs parameter;

    ASSERT_EQ(android::OK, parameter.add(key, valueToTest));

    float returnValue = 0.0f;
    ASSERT_EQ(android::OK, parameter.get(key, returnValue));

    EXPECT_FLOAT_EQ(valueToTest, returnValue);
}

INSTANTIATE_TEST_CASE_P(
    KeyValuePairsTestFloatAll,
    KeyValuePairsTestFloat,
    ::testing::Values(0.0,
                      1234.5678,
                      3.141592,
                      std::numeric_limits<float>::max(),
                      -std::numeric_limits<float>::max(),
                      std::numeric_limits<float>::min()
                      )
    );
