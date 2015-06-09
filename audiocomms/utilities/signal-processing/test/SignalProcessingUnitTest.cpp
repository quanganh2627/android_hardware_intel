/**
 * @section License
 *
 * Copyright 2014 Intel Corporation
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

#include "signal-processing/SignalProcessing.hpp"
#include "TypedTest.hpp"

#include <utilities/TypeList.hpp>
#include <gtest/gtest.h>


namespace audio_comms
{
namespace utilities
{
namespace signal_processing
{

// Available Type definitions
typedef TYPELIST3 (int8_t, int16_t, int32_t) SignalProcessingTestTypes;


/** Mean Tests */

template <class T>
struct ConstMeanTest
{
    void operator()()
    {
        T val = 14;
        const T vals[] = {
            val, val, val, val
        };
        size_t size = sizeof(vals) / sizeof(T);
        double testMean = SignalProcessing<T>::mean(vals, size);
        EXPECT_EQ(val, testMean);
    }
};

AUDIOCOMMS_TYPED_TEST(ConstMeanTest, SignalProcessingTestTypes);

template <class T>
struct NullMeanTest
{
    void operator()()
    {
        T val = 0;
        const T vals[] = {
            val, val, val, val
        };
        size_t size = sizeof(vals) / sizeof(T);
        double testMean = SignalProcessing<T>::mean(vals, size);
        EXPECT_EQ(val, testMean);
    }
};

AUDIOCOMMS_TYPED_TEST(NullMeanTest, SignalProcessingTestTypes);

template <class T>
struct MaxMeanTest
{
    void operator()()
    {
        T val = std::numeric_limits<T>::max();
        const T vals[] = {
            val, val, val, val
        };
        size_t size = sizeof(vals) / sizeof(T);
        double testMean = SignalProcessing<T>::mean(vals, size);
        EXPECT_EQ(val, testMean);
    }
};

AUDIOCOMMS_TYPED_TEST(MaxMeanTest, SignalProcessingTestTypes);

template <class T>
struct MaxNullMeanTest
{
    void operator()()
    {
        T valMax = std::numeric_limits<T>::max();
        T valNull = 0;
        const T vals[] = {
            valMax, valNull, valMax, valNull
        };
        size_t size = sizeof(vals) / sizeof(T);
        double testMean = SignalProcessing<T>::mean(vals, size);
        EXPECT_EQ(static_cast<double>(valMax) / 2, testMean);
    }
};

AUDIOCOMMS_TYPED_TEST(MaxNullMeanTest, SignalProcessingTestTypes);

template <class T>
struct ScaleMeanTest
{
    void operator()()
    {
        T valA = 13;
        T valB = 15;
        const T vals[] = {
            valA, valB, valA, valB
        };
        size_t size = sizeof(vals) / sizeof(T);
        double testMean = SignalProcessing<T>::mean(vals, size);
        EXPECT_EQ(valA + 1, testMean);
    }
};

AUDIOCOMMS_TYPED_TEST(ScaleMeanTest, SignalProcessingTestTypes);

/** Variance Tests */

template <class T>
struct VarianceTest
{
    void operator()()
    {
        T valA = 13;
        T valB = 15;
        double mean = 14;
        const T vals[] = {
            valA, valB, valA, valB
        };
        size_t size = sizeof(vals) / sizeof(T);
        double testVariance = SignalProcessing<T>::variance(mean, vals, size);
        EXPECT_EQ(4, testVariance);
    }
};

AUDIOCOMMS_TYPED_TEST(VarianceTest, SignalProcessingTestTypes);

template <class T>
struct NullVarianceTest
{
    void operator()()
    {
        double mean = 10;
        const T vals[] = {
            10, 10, 10, 10
        };
        size_t size = sizeof(vals) / sizeof(T);
        double testVariance = SignalProcessing<T>::variance(mean, vals, size);
        EXPECT_EQ(0, testVariance);
    }
};

AUDIOCOMMS_TYPED_TEST(NullVarianceTest, SignalProcessingTestTypes);

/** Normalized Offset Product Tests */

template <class T>
struct SameNormalizedOffsetProductTest
{
    void operator()()
    {
        const T vals[] = {
            10, 20, 30, 40
        };
        size_t size = sizeof(vals) / sizeof(T);
        double mean = 50;
        double variance = 3000;

        double normalizedOffsetProduct =
            SignalProcessing<T>::normalizedOffsetProduct(
                mean,
                mean,
                vals,
                vals,
                size,
                0);

        // signal are identical
        EXPECT_EQ(variance, normalizedOffsetProduct);
    }
};

AUDIOCOMMS_TYPED_TEST(SameNormalizedOffsetProductTest, SignalProcessingTestTypes);

template <class T>
struct NullNormalizedOffsetProductTest
{
    void operator()()
    {
        const T valsA[] = {
            10, 20, 30, 40
        };
        const T valsB[] = {
            30, 40, 10, 20
        };

        size_t sizeA = sizeof(valsA) / sizeof(T);
        size_t sizeB = sizeof(valsB) / sizeof(T);

        ASSERT_EQ(sizeA, sizeB);

        double meanAB = 50;

        // Normalized Offset product with offset of 2
        double normalizedOffsetProductReal = 2500;

        double normalizedOffsetProduct =
            SignalProcessing<T>::normalizedOffsetProduct(
                meanAB,
                meanAB,
                valsB,
                valsA,
                sizeA,
                2);

        EXPECT_EQ(normalizedOffsetProductReal, normalizedOffsetProduct);
    }
};

AUDIOCOMMS_TYPED_TEST(NullNormalizedOffsetProductTest, SignalProcessingTestTypes);

template <class T>
struct MaxNormalizedOffsetProductTest
{
    void operator()()
    {
        T valMax = std::numeric_limits<T>::max();
        const T valsA[] = {
            valMax, valMax, valMax, valMax
        };
        const T valsB[] = {
            valMax, valMax, valMax, valMax
        };

        size_t sizeA = sizeof(valsA) / sizeof(T);
        size_t sizeB = sizeof(valsB) / sizeof(T);

        ASSERT_EQ(sizeA, sizeB);

        double meanAB = valMax;

        double varianceAB = 0;

        double normalizedOffsetProduct =
            SignalProcessing<T>::normalizedOffsetProduct(
                meanAB,
                meanAB,
                valsB,
                valsA,
                sizeA,
                0);

        EXPECT_EQ(varianceAB, normalizedOffsetProduct);
    }
};

AUDIOCOMMS_TYPED_TEST(MaxNormalizedOffsetProductTest, SignalProcessingTestTypes);

/* Cross Correlation tests */

template <class T>
struct HalfEQCrossCorrelationTest
{
    void operator()()
    {
        const T valsA[] = {
            10, 20, 30, 40
        };
        const T valsB[] = {
            30, 40, 10, 20
        };

        size_t sizeA = sizeof(valsA) / sizeof(T);
        size_t sizeB = sizeof(valsB) / sizeof(T);

        ASSERT_EQ(sizeA, sizeB);

        typename SignalProcessing<T>::CrossCorrelationResult resultCC = {
            0, 0
        };
        typename SignalProcessing<T>::Result result =
            SignalProcessing<T>::cross_correlate(
                valsA,
                valsB,
                sizeA,
                resultCC,
                0,
                4);

        // With an offset of two, signals are identical.
        ASSERT_TRUE(result.isSuccess()) << result.format();
        EXPECT_EQ(0.5, resultCC.coefficient);
        EXPECT_EQ(2, resultCC.delay);
    }
};

AUDIOCOMMS_TYPED_TEST(HalfEQCrossCorrelationTest, SignalProcessingTestTypes);

template <class T>
struct EQCrossCorrelationTest
{
    void operator()()
    {
        const T valsA[] = {
            10, 20, 30, 40
        };
        const T valsB[] = {
            10, 20, 30, 40
        };

        size_t sizeA = sizeof(valsA) / sizeof(T);
        size_t sizeB = sizeof(valsB) / sizeof(T);

        ASSERT_EQ(sizeA, sizeB);

        typename SignalProcessing<T>::CrossCorrelationResult resultCC = {
            0, 0
        };
        typename SignalProcessing<T>::Result result =
            SignalProcessing<T>::cross_correlate(
                valsA,
                valsB,
                sizeA,
                resultCC,
                0,
                4);

        ASSERT_TRUE(result.isSuccess()) << result.format();
        EXPECT_EQ(1, resultCC.coefficient);
        EXPECT_EQ(0, resultCC.delay);
    }
};

AUDIOCOMMS_TYPED_TEST(EQCrossCorrelationTest, SignalProcessingTestTypes);

template <class T>
struct DiffCrossCorrelationTest
{
    void operator()()
    {
        const T valsA[] = {
            1, 2, 3, 4
        };
        const T valsB[] = {
            5, 6, 7, 8
        };

        size_t sizeA = sizeof(valsA) / sizeof(T);
        size_t sizeB = sizeof(valsB) / sizeof(T);

        ASSERT_EQ(sizeA, sizeB);

        typename SignalProcessing<T>::CrossCorrelationResult resultCC = {
            0, 0
        };
        typename SignalProcessing<T>::Result result =
            SignalProcessing<T>::cross_correlate(
                valsA,
                valsB,
                sizeA,
                resultCC,
                0,
                4);

        // signals are identical if we look the wave form
        ASSERT_TRUE(result.isSuccess()) << result.format();
        EXPECT_EQ(1, resultCC.coefficient);
        EXPECT_EQ(0, resultCC.delay);
    }
};

AUDIOCOMMS_TYPED_TEST(DiffCrossCorrelationTest, SignalProcessingTestTypes);

template <class T>
struct ConstSignalCrossCorrelationTest
{
    void operator()()
    {
        T val = 1;
        const T valsA[] = {
            val, val, val, val
        };
        const T valsB[] = {
            val, val, val, val
        };

        size_t sizeA = sizeof(valsA) / sizeof(T);
        size_t sizeB = sizeof(valsB) / sizeof(T);

        ASSERT_EQ(sizeA, sizeB);

        typename SignalProcessing<T>::CrossCorrelationResult
        resultCC = {
            0, 0
        };
        typename SignalProcessing<T>::Result result =
            SignalProcessing<T>::cross_correlate(
                valsA,
                valsB,
                sizeA,
                resultCC,
                0,
                4);

        typename SignalProcessing<T>::Result errorConstResult(
            SignalProcessing<T>::ConstSignal);

        // signals are identical but constant
        ASSERT_EQ(errorConstResult, result) << result.format();
    }
};

AUDIOCOMMS_TYPED_TEST(ConstSignalCrossCorrelationTest, SignalProcessingTestTypes);

} /* namespace signal_processing */
} /* namespace utilities */
} /* namespace audio_comms */
