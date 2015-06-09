/*
 * Copyright © 2012 Intel Corporation
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Jackie Li <yaodong.li@intel.com>
 *
 */
#include <cutils/log.h>
#include <stdarg.h>

#include <Log.h>
#include <HwcConfig.h>

namespace android {

using namespace intel;
ANDROID_SINGLETON_STATIC_INSTANCE(Log);

namespace intel {

Log::Log() : Singleton<Log>()
{

}

void Log::e(int comp, const char *fmt, ...)
{
    int logLevel;

    HwcConfig::getInstance().debugLogLevel(logLevel);

    if (logLevel <= HwcConfig::DEBUG_LOG_ERROR) {
        va_list ap;

        va_start(ap, fmt);
        LOG_PRI_VA(ANDROID_LOG_ERROR, LOG_TAG, fmt, ap);
        va_end(ap);
    }
}

void Log::e(const char *fmt, ...)
{
    int logLevel;

    HwcConfig::getInstance().debugLogLevel(logLevel);

    if (logLevel <= HwcConfig::DEBUG_LOG_ERROR) {
        va_list ap;

        va_start(ap, fmt);
        LOG_PRI_VA(ANDROID_LOG_ERROR, LOG_TAG, fmt, ap);
        va_end(ap);
    }
}

void Log::w(int comp, const char *fmt, ...)
{
    int logLevel;

    HwcConfig::getInstance().debugLogLevel(logLevel);

    if (logLevel <= HwcConfig::DEBUG_LOG_WARNING) {
        va_list ap;

        va_start(ap, fmt);
        LOG_PRI_VA(ANDROID_LOG_WARN, LOG_TAG, fmt, ap);
        va_end(ap);
    }
}


void Log::w(const char *fmt, ...)
{
    int logLevel;

    HwcConfig::getInstance().debugLogLevel(logLevel);

    if (logLevel <= HwcConfig::DEBUG_LOG_WARNING) {
        va_list ap;

        va_start(ap, fmt);
        LOG_PRI_VA(ANDROID_LOG_WARN, LOG_TAG, fmt, ap);
        va_end(ap);
    }
}

void Log::d(int comp, const char *fmt, ...)
{
    int logLevel;

    HwcConfig::getInstance().debugLogLevel(logLevel);

    if (logLevel <= HwcConfig::DEBUG_LOG_DEBUG) {
        va_list ap;

        va_start(ap, fmt);
        LOG_PRI_VA(ANDROID_LOG_DEBUG, LOG_TAG, fmt, ap);
        va_end(ap);
    }
}


void Log::d(const char *fmt, ...)
{
    int logLevel;

    HwcConfig::getInstance().debugLogLevel(logLevel);

    if (logLevel <= HwcConfig::DEBUG_LOG_DEBUG) {
        va_list ap;

        va_start(ap, fmt);
        LOG_PRI_VA(ANDROID_LOG_DEBUG, LOG_TAG, fmt, ap);
        va_end(ap);
    }
}

void Log::i(int comp, const char *fmt, ...)
{
    int logLevel;

    HwcConfig::getInstance().debugLogLevel(logLevel);

    if (logLevel <= HwcConfig::DEBUG_LOG_INFO) {
        va_list ap;

        va_start(ap, fmt);
        LOG_PRI_VA(ANDROID_LOG_INFO, LOG_TAG, fmt, ap);
        va_end(ap);
    }
}

void Log::i(const char *fmt, ...)
{
    int logLevel;

    HwcConfig::getInstance().debugLogLevel(logLevel);

    if (logLevel <= HwcConfig::DEBUG_LOG_INFO) {
        va_list ap;

        va_start(ap, fmt);
        LOG_PRI_VA(ANDROID_LOG_INFO, LOG_TAG, fmt, ap);
        va_end(ap);
    }
}

void Log::v(int comp, const char *fmt, ...)
{
    int logLevel;

    HwcConfig::getInstance().debugLogLevel(logLevel);

    if (logLevel <= HwcConfig::DEBUG_LOG_VERBOSE) {
        va_list ap;

        va_start(ap, fmt);
        LOG_PRI_VA(ANDROID_LOG_VERBOSE, LOG_TAG, fmt, ap);
        va_end(ap);
    }
}

void Log::v(const char *fmt, ...)
{
    int logLevel;

    HwcConfig::getInstance().debugLogLevel(logLevel);

    if (logLevel <= HwcConfig::DEBUG_LOG_VERBOSE) {
        va_list ap;

        va_start(ap, fmt);
        LOG_PRI_VA(ANDROID_LOG_VERBOSE, LOG_TAG, fmt, ap);
        va_end(ap);
    }
}

};
};

