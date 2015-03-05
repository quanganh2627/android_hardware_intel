/**
 * Copyright 2009 - 2010 (c) Intel Corporation. All rights reserved.

 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at

 * http://www.apache.org/licenses/LICENSE-2.0

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __WATCHDOGD_H__
#define __WATCHDOGD_H__

/* Logs macros */
#define LOG_TAG "ia_watchdog"
#ifdef __ANDROID__
#include <android/log.h>
#define LOG_INIT(a) do {  } while(0)
/* #define LOGE(args...) do { __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, args); } while(0) */
/* #define LOGW(args...) do { __android_log_print(ANDROID_LOG_WARN, LOG_TAG, args); } while(0) */
/* #define LOGI(args...) do { __android_log_print(ANDROID_LOG_INFO, LOG_TAG, args); } while(0) */
/* #define LOGD(args...) do { __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, args); } while(0) */
#define LOGE(args...) do { __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, args); } while(0)
#define LOGW(args...) do { __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, args); } while(0)
#define LOGI(args...) do { __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, args); } while(0)
#define LOGD(args...) do { __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, args); } while(0)
#else /* __ANDROID__ */
#ifdef USE_SYSLOG
/* Not to be used in POS, otherwise the daemon will be killed when formatting the log partition */
#include <syslog.h>
#define LOG_INIT(a) do { openlog(a, LOG_CONS|LOG_NDELAY|LOG_PID, LOG_INFO); } while(0)
#define LOGE(args...) do { syslog(LOG_ERR, args); } while(0)
#define LOGW(args...) do { syslog(LOG_WARNING, args); } while(0)
#define LOGI(args...) do { syslog(LOG_INFO, args); } while(0)
#define LOGD(args...) do { syslog(LOG_DEBUG, args); } while(0)
#else /* USE_SYSLOG */
#define LOG_INIT(a) do {  } while(0)
#define LOGE(args...) do { printf(LOG_TAG " Error: "); printf(args); } while(0)
#define LOGW(args...) do { printf(LOG_TAG " Warning: "); printf(args); } while(0)
#define LOGI(args...) do { printf(LOG_TAG " Info: "); printf(args); } while(0)
#define LOGD(args...) do { printf(LOG_TAG " Debug: "); printf(args); } while(0)
#endif /* USE_SYSLOG */
#endif /* __ANDROID__ */

/* dummy macro */
#define max(a, b) ((a >= b) ? a : b)

#endif /* __WATCHDOGD_H__ */
