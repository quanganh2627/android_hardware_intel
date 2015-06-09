/*
* Copyright (c) 2009-2011 Intel Corporation.  All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/


#ifndef IMAGE_DECODER_TRACE_H_
#define IMAGE_DECODER_TRACE_H_


#define ENABLE_IMAGE_DECODER_TRACE
//#define ANDROID


#ifdef ENABLE_IMAGE_DECODER_TRACE

#ifndef ANDROID

#include <stdio.h>
#include <stdarg.h>

extern void TraceImageDecoder(const char* cat, const char* fun, int line, const char* format, ...);
#define IMAGE_DECODER_TRACE(cat, format, ...) \
TraceImageDecoder(cat, __FUNCTION__, __LINE__, format,  ##__VA_ARGS__)

#define ETRACE(format, ...) IMAGE_DECODER_TRACE("ERROR:   ",  format, ##__VA_ARGS__)
#define WTRACE(format, ...) IMAGE_DECODER_TRACE("WARNING: ",  format, ##__VA_ARGS__)
#define ITRACE(format, ...) IMAGE_DECODER_TRACE("INFO:    ",  format, ##__VA_ARGS__)
#define VTRACE(format, ...) IMAGE_DECODER_TRACE("VERBOSE: ",  format, ##__VA_ARGS__)

#else
// for Android OS

#ifdef LOG_NDEBUG
#undef LOG_NDEBUG
#endif
#define LOG_NDEBUG 0


#include <utils/Log.h>
#define ETRACE(...) ALOGE(__VA_ARGS__)
#define WTRACE(...) ALOGW(__VA_ARGS__)
#define ITRACE(...) ALOGI(__VA_ARGS__)
#define VTRACE(...) ALOGV(__VA_ARGS__)

#endif


#else

#define ETRACE(format, ...)
#define WTRACE(format, ...)
#define ITRACE(format, ...)
#define VTRACE(format, ...)


#endif /* ENABLE_VIDEO_DECODER_TRACE*/

#endif /*IMAGE_DECODER_TRACE_H_*/


