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
#pragma once
#include <string>

#include <sstream>

namespace audio_comms
{
namespace utilities
{
namespace result
{

/** A result that carries a error code describing the error.
 *
 * @tparam ErrorTrait a class describing a specific error field
 *  ErrorTrait MUST have a Code type which hold its error values and MUST have
 *  codeToString() static function that converts this code type into std::string.
 */
template <class ErrorTrait>
class Result
{
public:
    typedef ErrorTrait Trait;
    typedef typename ErrorTrait::Code Code;

    /** Result constructor
     *
     * this creates a Result object which holds the given error code.
     *
     * @param[in] code the code to be put in Result; Default value is
     *                       ErrorTrait::defaultError.
     */
    explicit Result(Code code = ErrorTrait::defaultError)
        : _errorCode(code)
    {}

    /** Result constructor from an other result
     *
     * Construct a result where it's
     *     error_code = inputResult.isFailure() ? failureCode : successCode
     * If input result is a failure, append it (formated) to the message.
     *
     * @tparam InputTrait is the trait of the input result
     * @param inputResult is the input result
     * @param failureCode is the error code to use if the input result is a failure
     * @param successCode is the error code to use if the input result is a success
     */
    template <class InputTrait>
    explicit Result(const Result<InputTrait> &inputResult,
                    Code failureCode,
                    Code successCode = ErrorTrait::success)
    {
        if (inputResult.isFailure()) {
            _errorCode = failureCode;
            *this << inputResult;
        } else {
            _errorCode = successCode;
        }
    }

    /** Get the error code.
     *
     * @return the error code, undefined behaviour if the result is a success.
     */
    Code getErrorCode() const { return _errorCode; }

    /** @return The formated error message. */
    const std::string getMessage() const
    {
        return getMainMessage() + _peerMessage;
    }

    /** Comparison operator.
     * Careful, comparison is performed on error code only, messages MAY differ
     * even if this function returns true.
     *
     * @param[in] toCompare  a Coderesult to compare with us
     *
     * @return true on equality, false otherwise
     */
    bool operator==(const Result<ErrorTrait> &toCompare) const
    {
        return _errorCode == toCompare._errorCode;
    }
    /** not == */
    bool operator!=(const Result<ErrorTrait> &toCompare) const
    {
        return not operator==(toCompare);
    }

    /** Is code the same as getErrorCode() */
    bool operator==(Code code) const { return _errorCode == code; }

    /** Is code different than getErrorCode() */
    bool operator!=(Code code) const { return _errorCode != code; }

    /**
     * Success
     * This function alway return a successful Result
     *
     * @return a successful coderesult
     */
    static const Result<ErrorTrait> &success()
    {
        static Result<ErrorTrait> success(ErrorTrait::success);
        return success;
    }

    /**
     * is object holding a Success value
     *
     * @return true is the error code is success, false otherwise
     */
    bool isSuccess() const { return _errorCode == ErrorTrait::success; }

    /**
     * is object holding a failure
     *
     * @return true if error code is NOT a success
     */
    bool isFailure() const { return !isSuccess(); }

    /** Format a Result in oder to have it under a synthetic human readable view.
     *
     * The output format is implementation defined and should not be consider
     * presistant across versions.
     * @return a formated Result
     */
    std::string format() const
    {
        if (isSuccess()) {
            return "Success";
        }

        std::ostringstream ss;

        // Error code and error description
        ss << ErrorTrait::codeToString(_errorCode)
           << " (Code " << static_cast<int>(_errorCode) << ")";

        // Construct main message
        std::string message = getMainMessage();

        // Append main message
        if (not message.empty()) {
            ss << ": " << message;
        }

        // Append peer message
        ss << _peerMessage;

        return ss.str();
    }

    /**
     * Appends any data to message
     * Careful: this function expects that T has a known conversion to ostream
     *          or a Result
     *
     * @tparam T   type of the thing to append
     * @param[in]  aT  a T thing to append
     *
     * @return a copy of *this
     */
    template <class T>
    Result<ErrorTrait> operator<<(const T &aT)
    {
        Append<T>::run(*this, aT);
        return *this;
    }

    /** Join with an other result from the same level.
     *
     * @see operator&
     */
    Result<ErrorTrait> &operator&=(const Result<ErrorTrait> res)
    {
        return *this = *this & res;
    }

    /** Join two result from the same level.
     *
     * @param[in] res Is the result to concatenate with.
     * @return If both are successful, returns success.
     *         If one is successful returns the other.
     *         If both are failure, returns *this with the formated second
     *            appended to its message.
     */
    Result<ErrorTrait> operator&(const Result<ErrorTrait> res) const
    {
        // If one is successfull return the other
        if (isSuccess()) {
            return res;
        }
        if (res.isSuccess()) {
            return *this;
        }

        // As both result are failure, concatenate their messages.
        Result concatRes = *this;
        concatRes._peerMessage += " && " + res.format();

        return concatRes;
    }

private:
    /** Artefact class to partial specialize operator<< */
    template <class Data>
    struct Append
    {
        /** Append a data to _message. @see Result::operator<< */
        static void run(Result<ErrorTrait> &res, const Data &data)
        {
            std::ostringstream ss;
            ss << data;
            res._message += ss.str();
        }
    };
    template <class Err>
    struct Append<Result<Err> >
    {
        /** Register a result as a sub message. @see Result::operator<< */
        static void run(Result<ErrorTrait> &res, const Result<Err> &data)
        {
            if (not res._subMessage.empty()) {
                res._subMessage += " && ";
            }
            res._subMessage += data.format();
        }
    };
    template <class>
    template <class Data>
    friend void Append<Data>::run();

    /** @return the main and sub message in a formated way. */
    const std::string getMainMessage() const
    {
        std::string message = _message;

        // Append sub messages
        if (not _subMessage.empty()) {
            if (not message.empty()) {
                message += ' ';
            }
            message += '{' + _subMessage + '}';
        }
        return message;
    }

    Code _errorCode; /*< result code held by class */
    std::string _message; /*< an error message explaining the error */
    std::string _peerMessage;
    std::string _subMessage;
};

template <class ErrorTrait>
bool operator==(typename ErrorTrait::Code code, Result<ErrorTrait> result)
{
    return result == code;
}

template <class ErrorTrait>
bool operator!=(typename ErrorTrait::Code code, Result<ErrorTrait> result)
{
    return result != code;
}

} /* namespace result */
} /* namespace utilities */
} /* namespace audio_comms */
