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

#include "result/ErrnoResult.hpp"

#include <gtest/gtest.h>

typedef audio_comms::utilities::result::ErrnoResult ErrnoResult;

TEST(ErrnoResult, defaultValueConstructor)
{
    ErrnoResult errResult;

    /* Some strerror() implementations display the error as int, other one display the error
     * as unsigned int */

    std::stringstream expectedStringSigned;
    expectedStringSigned << "Unknown error " << (int)-666 << " (Code -666)";

    std::stringstream expectedStringUnsigned;
    expectedStringUnsigned << "Unknown error " << (unsigned int)-666 << " (Code -666): ";

    EXPECT_TRUE(errResult.format() == expectedStringSigned.str() ||
                errResult.format() == expectedStringUnsigned.str());
}

TEST(ErrnoResult, sucess)
{
    ErrnoResult errResult = ErrnoResult::success();
    EXPECT_EQ(0, errResult.getErrorCode());

}
TEST(ErrnoResult, error)
{
    ErrnoResult errResult(EPERM);
    EXPECT_EQ("Operation not permitted (Code 1)", errResult.format());
}
