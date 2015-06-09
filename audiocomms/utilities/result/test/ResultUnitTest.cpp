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

#include "result/Result.hpp"

#include <gtest/gtest.h>

struct TestResult
{
    enum Code
    {
        A = 666,
        B,
        C,
        D
    };

    static const Code success;
    static const Code defaultError;

    static std::string codeToString(const Code &code)
    {
        switch (code) {
        case A:
            return "A";
        case B:
            return "B";
        case C:
            return "C";
        case D:
            return "D";
        }
        /** @fixme: ASSERT? */
        return "";
    }
};

const TestResult::Code TestResult::success = A;
const TestResult::Code TestResult::defaultError = B;

typedef audio_comms::utilities::result::Result<TestResult> Result;

TEST(Result, defaultValueConstructor)
{
    Result aResult;

    /* Note, this also tests operator== */
    EXPECT_EQ(Result(TestResult::defaultError), aResult);
    EXPECT_EQ("", aResult.getMessage());
}

TEST(Result, someValueConstructor)
{
    Result aResult(TestResult::A);

    EXPECT_EQ(Result(TestResult::A), aResult);
    EXPECT_EQ("", aResult.getMessage());
}

TEST(Result, convertToCode)
{
    {
        Result aResult(TestResult::success);
        EXPECT_TRUE(aResult == TestResult::success);
    }

    {
        Result aResult(TestResult::C);
        EXPECT_FALSE(aResult.isSuccess());
    }
}

TEST(Result, isFailure)
{
    {
        Result aResult(TestResult::success);
        EXPECT_FALSE(aResult.isFailure());
        aResult << "that should not change success state";
        EXPECT_FALSE(aResult.isFailure());
    }

    {
        Result aResult(TestResult::D);
        EXPECT_TRUE(aResult.isFailure());
        aResult << "that should not change failure state";
        EXPECT_TRUE(aResult.isFailure());
    }
}

TEST(Result, isSuccess)
{
    {
        Result aResult(TestResult::D);
        EXPECT_FALSE(aResult.isSuccess());
        aResult << "that should not change failure state";
        EXPECT_FALSE(aResult.isSuccess());
    }

    {
        Result aResult(TestResult::success);
        EXPECT_TRUE(aResult.isSuccess());
        aResult << "that should not change success state";
        EXPECT_TRUE(aResult.isSuccess());
    }
}

TEST(Result, equalityOperator)
{
    Result aResult(TestResult::success);
    aResult << " Now error message differs.";
    EXPECT_EQ(Result(TestResult::success),  aResult);
}

TEST(Result, getErrorCode)
{
    Result aResult(TestResult::success);
    EXPECT_EQ(TestResult::success, aResult.getErrorCode());
}

TEST(Result, formatSuccess)
{
    Result aResult(TestResult::success);
    EXPECT_EQ("Success", aResult.format());
}

TEST(Result, formatError)
{
    Result aResult(TestResult::C);
    EXPECT_EQ("C (Code 668)", aResult.format());
}

TEST(Result, useWithStream)
{
    Result aResult(TestResult::B);
    aResult << "gnii";
    EXPECT_EQ("gnii",  aResult.getMessage());
    EXPECT_EQ("B (Code 667): gnii",  aResult.format());
}

TEST(Result, successSingleton)
{
    EXPECT_EQ(Result::success(), Result(TestResult::success));
}

TEST(Result, constructFromfailureRes)
{
    Result bResult(TestResult::B);
    bResult << "<B>";
    Result trans(bResult, TestResult::D);
    trans << "<D>";
    EXPECT_EQ("D (Code 669): <D> {B (Code 667): <B>}",  trans.format());
    EXPECT_EQ(trans.format(),
              (Result(TestResult::D) << "<D>" << (Result(TestResult::B) << "<B>")
              ).format());
}

TEST(Result, constructFromSuccessRes)
{
    Result trans(Result::success(), TestResult::D, TestResult::C);
    EXPECT_EQ(TestResult::C, trans);
}

TEST(Result, resultStream)
{
    EXPECT_EQ("B (Code 667): <B><end> {D (Code 669) && B (Code 667): <B>}",
              (Result(TestResult::B) << "<B>"
                                     << Result(TestResult::D)
                                     << (Result(TestResult::B) << "<B>")
                                     << "<end>"
              ).format());
}

TEST(ResultConcat, twoFailureEmptyMsg)
{
    const Result resultARef(TestResult::A);
    const Result resultBRef(TestResult::B);
    const Result resultCRef(TestResult::C);

    Result resultBC = resultARef;
    resultBC &= resultBRef;
    resultBC &= resultCRef;

    EXPECT_EQ(resultBC.format(), (resultARef & resultBRef & resultCRef).format());
    EXPECT_EQ("B (Code 667) && C (Code 668)", resultBC.format());
}

TEST(ResultConcat, twoFailureWithMsg)
{
    const Result result = (Result(TestResult::A) &
                           (Result(TestResult::B) << "<B>") &
                           Result(TestResult::C)
                           ) << "<end>";
    EXPECT_EQ("B (Code 667): <B><end> && C (Code 668)", result.format());

    Result resultD = Result(TestResult::D) << result;
    EXPECT_EQ("D (Code 669): {B (Code 667): <B><end> && C (Code 668)}",
              resultD.format());
    EXPECT_EQ("D (Code 669): <D> {B (Code 667): <B><end> && C (Code 668)}",
              (resultD << "<D>").format());
}

TEST(ResultConcat, twoSuccess)
{
    EXPECT_EQ(Result::success().format(),
              (Result::success() & Result::success()).format());
}

TEST(ResultConcat, leftSuccess_rightFailure)
{
    EXPECT_EQ((Result(TestResult::C) << "msg").format(),
              (Result::success() & (Result(TestResult::C) << "msg")).format());
}

TEST(ResultConcat, leftFailure_rightSuccess)
{
    EXPECT_EQ((Result(TestResult::C) << "msg").format(),
              ((Result(TestResult::C) << "msg") & Result::success()).format());
}
