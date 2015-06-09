/*
 * @file
 * Audiocomms log facility
 *
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

#pragma once

/** the default LOG_TAG to be used when no tag is specified */
#define DEFAULT_LOG_TAG "AUDIOCOMMS"

#include <utilities/TypeList.hpp>
#include <iostream>
#include <sstream>
#include <cstdio>
#include <cstdarg>

#ifdef __ANDROID__
#include <cutils/log.h>
#endif

namespace audio_comms
{

namespace utilities
{

namespace details
{

/* Log level mostly inspired from android's one */
struct Level
{
    enum Enum
    {
        Fatal = 33,
        Error,
        Warn,
        Info,
        Debug,
        Verbose,
    };
};

} // namespace details

/**
 * GenericLog is design to ease logging in your code.
 * tparam TraitList  The traitList that defines the function which is used to perform
 * the IO operation (print the log)
 */
template <class TraitList>
class GenericLog;

template <>
class GenericLog<TYPELIST0>
{
private:
    template <details::Level::Enum>
    static void valog(const char *, const char *, va_list)
    {}

    template <class>
    friend class GenericLog;
};

template <class FirstTrait, class RemainingTraits>
class GenericLog<TypeList<FirstTrait, RemainingTraits> >
{
private:
    /* All other template instances need to be friend in order to be called in valog */
    template <class T>
    friend class GenericLog;

    template <details::Level::Enum l>
    static void valog(const char *logTag, const char *format, va_list args)
    {
        va_list argsCopy;

        va_copy(argsCopy, args);

        GenericLog<RemainingTraits>().valog<l>(logTag, format, argsCopy);

        va_end(argsCopy);

        FirstTrait().valog<l>(logTag, format, args);
    }

    /**
     * Stream log class.
     * tparam the log level for this stream.
     */
    template <typename details::Level::Enum l>
    class Stream
    {
    public:
        Stream() : message(), mLogTag(
#ifdef LOG_TAG
                       /* Careful, LOG_TAG may be defined to NULL by android's headers, thus having it
                        * defined here does not mean it is 'valid' */
                       LOG_TAG
#else
                       NULL
#endif
                       )
        {
            if (mLogTag == NULL) {
                mLogTag = DEFAULT_LOG_TAG;
            }
        }
        Stream(const char *logTag) : message(), mLogTag(logTag) {}

        /* Helper function needed to pass from ... to va_list
         * This function is left public for backward compatibility with old log APIs that use
         * printf like functions */
        void log(const char *format, ...) __attribute__((format(__printf__, 2, 3)))
        {
            va_list ap;
            va_start(ap, format);

            valog<l>(mLogTag, format, ap);

            va_end(ap);
        }

        /**
         * operator<< is used to mock a usual ostream.
         */
        template <class T>
        std::ostringstream &operator<<(const T &s)
        {
            message << s;
            return message;
        }

        ~Stream()
        {
            /* Prevents printing useless empty log */
            if (!message.str().empty()) {
                log("%s", message.str().c_str());
            }
        }

    private:
        std::ostringstream message;
        const char *mLogTag;
    };

public:
    typedef Stream<details::Level::Fatal> Fatal;
    typedef Stream<details::Level::Error> Error;
    typedef Stream<details::Level::Warn>  Warning;
    typedef Stream<details::Level::Info>  Info;
    typedef Stream<details::Level::Debug> Debug;
    typedef Stream<details::Level::Verbose> Verbose;
};

/**
 * Trait for logging on standard output/error
 */
class StdIoLogTrait
{
public:
    template <details::Level::Enum l>
    void valog(const char *logTag, const char *format, va_list args)
    {
        FILE *outputFile;
        const char *prefix;
        switch (l) {
        case details::Level::Fatal:
            outputFile = stderr;
            prefix = "F";
            break;
        case details::Level::Error:
            outputFile = stderr;
            prefix = "E";
            break;
        case details::Level::Warn:
            outputFile = stderr;
            prefix = "W";
            break;
        case details::Level::Info:
            outputFile = stderr;
            prefix = "I";
            break;
        case details::Level::Debug:
            outputFile = stderr;
            prefix = "D";
            break;
        case details::Level::Verbose:
            outputFile = stdout;
            prefix = "V";
            break;
        }

        /* prevent to bother with printing when output file was closed which usually happen when
         * started as a daemon */
        if (outputFile == NULL) {
            return;
        }
        fprintf(outputFile, "%s/%s ", prefix, logTag);
        vfprintf(outputFile, format, args);
        fprintf(outputFile, "\n");
    }
};

#ifdef __ANDROID__
/**
 * AndroidLogTrait defines the android specific log type.
 * tparam debugEnabled  if set, all log messages (VERBOSE included) are active.
 *                      if not set, VERBOSE messages are filtered.
 */
template <bool debugEnabled>
class AndroidLogTrait
{
public:
    template <details::Level::Enum l>
    void valog(const char *logTag, const char *format, va_list args)
    {
        android_LogPriority aprio;
        switch (l) {
        case details::Level::Fatal:
            aprio = ANDROID_LOG_FATAL;
            break;
        case details::Level::Error:
            aprio = ANDROID_LOG_ERROR;
            break;
        case details::Level::Warn:
            aprio = ANDROID_LOG_WARN;
            break;
        case details::Level::Info:
            aprio = ANDROID_LOG_INFO;
            break;
        case details::Level::Debug:
            aprio = ANDROID_LOG_DEBUG;
            break;
        case details::Level::Verbose:
            aprio = ANDROID_LOG_VERBOSE;
            break;
        }
        if (debugEnabled || aprio != ANDROID_LOG_VERBOSE) {
            __android_log_vprint(aprio, logTag, format, args);
        }
    }
};
/* Android strips VERBOSE message from release builds. This behavior can be modified
 * by #define LOG_NDEBUG 0 at the top of each source file.*/
typedef TYPELIST2 (StdIoLogTrait, AndroidLogTrait<!LOG_NDEBUG> ) DefaultLogTraitList;

#else
typedef TYPELIST1 (StdIoLogTrait) DefaultLogTraitList;
#endif

/* Defines a 'default' Log type to be used when caller just needs logging on
 * default log output (depends on the platform) */
typedef GenericLog<DefaultLogTraitList> Log;

} // namespace utilities

} // namespace audio_comms
