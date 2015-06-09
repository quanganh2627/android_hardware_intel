/*
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
#pragma once

#include <result/Result.hpp>
#include <utilities/FileMapper.hpp>
#include <AudioCommsAssert.hpp>
#include <cmath>
#include <limits>


namespace audio_comms
{
namespace utilities
{
namespace signal_processing
{
namespace details
{

/** Helper class to limit instantiation of templates */
template <typename T>
struct ProcessingAllowed;

/** List of allowed types for audio processing*/
template <>
struct ProcessingAllowed<int8_t> {};
template <>
struct ProcessingAllowed<int16_t> {};
template <>
struct ProcessingAllowed<int32_t> {};

}


/** This class allows to cross-correlate two signals
 *
 *  @tparam T the type of the array used to carry the
 *  signal.
 */
template <class T>
class SignalProcessing
{

public:

    enum FailureCode
    {
        Success = 999,
        Unknown,
        ConstSignal
    };

    struct CrossCorrelationResult
    {
        double  coefficient;
        ssize_t delay;
    };

    struct SignalProcStatus
    {
        typedef FailureCode Code;

        /** Enum coding the failures that can occur in the class methods. */
        const static Code success = Success;
        const static Code defaultError = Unknown;

        static std::string codeToString(const Code &code)
        {
            switch (code) {
            case Success:
                return "Success";
            case Unknown:
                return "Unknown error";
            case ConstSignal:
                return "Cross Correlation of a const signal (silence)";
            }
            /* Unreachable, prevents gcc to complain */
            return "Invalid error (Unreachable)";
        }
    };

    /** The type of the method returns. */
    typedef utilities::result::Result<SignalProcStatus> Result;

    /** Normalized cross corelation.
     *
     *  @return the normalized cross correlation between the 2 signals A and B.
     */
    static Result cross_correlate(const T *signalA,
                                  const T *signalB,
                                  size_t valueNb,
                                  CrossCorrelationResult &result,
                                  ssize_t minDelay = 0,
                                  ssize_t maxDelay = 500);

    /** Calculate the mean (average) of a signal. */
    static double mean(const T *signal, size_t valueNb);

    /** Calculate the variance of a signal. */
    static double variance(double mean, const T *signal, size_t valueNb);

    /** Calculate the sum((A(t) - meanA)*(B(t + offsetB) - meanB))
     *
     *  @param[in] meanA is the signal A geometric mean
     *  @param[in] meanB is the signal B geometric mean
     *  @param[in] signalA is the signal A(t) define in [0, valueNb[
     *  @param[in] signalB is the signal B(t) define in [0, valueNb[
     *  @param[in] valueNb is the number of values if the signals A and B
     *  @param[in] offsetB is the
     *
     *  @return the sum.
     */
    static double normalizedOffsetProduct(
        double meanA, double meanB,
        const T *signalA, const T *signalB,
        size_t valueNb, ssize_t offsetB);
};


template <class T>
double SignalProcessing<T>::mean(const T *signal, size_t valueNb)
{
    /* Check that processing with that type is allowed.
     * If this fails, this means that this template was not intended to be used
     * with this type, thus that the result is undefined. */
    details::ProcessingAllowed<T>();

    double sum = 0;
    for (size_t i = 0; i < valueNb; i++) {
        sum += signal[i];
    }
    return sum / valueNb;
}

template <class T>
double SignalProcessing<T>::variance(double mean, const T *signal, size_t valueNb)
{
    /* Check that processing with that type is allowed.
     * If this fails, this means that this template was not intended to be used
     * with this type, thus that the result is undefined. */
    details::ProcessingAllowed<T>();

    double variance = 0;
    for (size_t i = 0; i < valueNb; i++) {
        variance += pow(signal[i] - mean, 2);
    }
    return variance;
}

template <class T>
double SignalProcessing<T>::normalizedOffsetProduct(double meanA, double meanB,
                                                    const T *signalA, const T *signalB,
                                                    size_t valueNb, ssize_t offsetB)
{
    /* Check that processing with that type is allowed.
     * If this fails, this means that this template was not intended to be used
     * with this type, thus that the result is undefined. */
    details::ProcessingAllowed<T>();

    size_t startIndexA = std::max(0, offsetB);
    size_t stopIndexA = valueNb + std::min(0, offsetB);

    double normProd = 0;
    for (size_t indexA = startIndexA; indexA < stopIndexA; indexA++) {
        size_t indexB = indexA - offsetB;
        normProd += (signalA[indexA] - meanA) * (signalB[indexB] - meanB);
    }
    return normProd;
}

template <class T>
typename SignalProcessing<T>::Result SignalProcessing<T>::cross_correlate(
    const T *signalA,
    const T *signalB,
    size_t valueNb,
    CrossCorrelationResult &result,
    ssize_t minDelay,
    ssize_t maxDelay)
{
    /* Check that processing with that type is allowed.
     * If this fails, this means that this template was not intended to be used
     * with this type, thus that the result is undefined. */
    details::ProcessingAllowed<T>();

    double meanA = mean(signalA, valueNb);
    double meanB = mean(signalB, valueNb);

    double varianceA = variance(meanA, signalA, valueNb);
    double varianceB = variance(meanB, signalB, valueNb);

    double denom = sqrt(varianceA * varianceB);

    if (denom == 0) {
        /** if denom is 0, the cross correlation can't work because at least one
         *  signal is constant, so the variance is zero
         */
        return Result(ConstSignal);
    }

    result.coefficient = 0;
    // Will be overwriten as for any x, x > maxCorrelationCoef (result.coefficient)
    result.delay = std::numeric_limits<ssize_t>::max();

    for (ssize_t delay = minDelay; delay <= maxDelay; delay++) {
        double correlationCoef = normalizedOffsetProduct(meanA, meanB,
                                                         signalA, signalB,
                                                         valueNb, -delay);
        correlationCoef /= denom;

        if (correlationCoef > result.coefficient) {
            result.coefficient = correlationCoef;
            result.delay = delay;
        }
    }

    return Result::success();

}

}
}
}
