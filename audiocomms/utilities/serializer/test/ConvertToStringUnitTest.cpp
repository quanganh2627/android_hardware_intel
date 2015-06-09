/**
 * @section License
 *
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
 */
#include "serializer/framework/ConvertToString.hpp"
#include <gtest/gtest.h>

using namespace audio_comms::utilities::serializer;


template <typename Source>
void TestConversion(const Source &src, std::string destExpected, bool success = true)
{
    std::string dest;
    EXPECT_TRUE(toString(src, dest));
    if (success) {
        EXPECT_EQ(destExpected, dest);
    } else {
        EXPECT_NE(destExpected, dest);
    }
}

TEST(Convert, Int)
{
    TestConversion<int>(20, "20");
    TestConversion<int>(100, "one hundred", false);

    TestConversion<int>(-200, "-200");
    TestConversion<int>(-1000, "1", false);
}

TEST(Convert, Bool)
{
    TestConversion<bool>(true, "true");
    TestConversion<bool>(false, "false");
    TestConversion<bool>(false, "none", false);
}

TEST(Convert, Chars)
{
    TestConversion<const char *>("", "");
    TestConversion<const char *>("toto", "toto");
    TestConversion<const char *>("toto", "tito", false);
}

TEST(Convert, String)
{
    TestConversion<std::string>("", "");
    TestConversion<std::string>("bou", "bou");
    TestConversion<std::string>("bar", "bone", false);
}
