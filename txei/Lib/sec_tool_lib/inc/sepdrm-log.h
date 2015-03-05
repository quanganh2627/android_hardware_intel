/**********************************************************************
 * Copyright (C) 2011 Intel Corporation. All rights reserved.

 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at

 * http://www.apache.org/licenses/LICENSE-2.0

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 **********************************************************************/

#ifndef __SEPLOG_H__
#define __SEPLOG_H__


#include <stdio.h>

/*
 * define the following macros as appropriate:
 * LOGE()
 * LOGERR()
 * LOGDBG()
 * DBG()
 * PrintMem()
 * Enter()
 * Leave()
 *
 * They use the Android log-print mechanism by default but can be directed
 * to use printf instead if LOG_STYLE is set
 */

#define LOG_STYLE_LOGDAEMON	1
#define LOG_STYLE_PRINTF	2
#define LOG_STYLE_DATA_FILE	3
#ifndef LOG_STYLE
#define LOG_STYLE		LOG_STYLE_LOGDAEMON
#endif

#ifdef ANDROID
#include <android/log.h>
#ifndef LOG_TAG
#define LOG_TAG "DRM-LibSEP"
#endif

#define DBG(fmt, arg...) fprintf( stderr,fmt, ##arg)

#if LOG_STYLE == LOG_STYLE_LOGDAEMON
	#define LOGE(fmt, arg...)						\
		__android_log_print( ANDROID_LOG_ERROR, LOG_TAG, fmt, ##arg )
	#define LOGERR(fmt, arg...)						\
		__android_log_print( ANDROID_LOG_ERROR, LOG_TAG,		\
		     "%s():%d: ERROR - " fmt, __func__, __LINE__, ##arg )
	#define LOGDBG(fmt, arg...)						\
		__android_log_print( ANDROID_LOG_ERROR, LOG_TAG,		\
		     "%s():%d: DEBUG - " fmt, __func__, __LINE__, ##arg )
#elif LOG_STYLE == LOG_STYLE_PRINTF
	#define LOGE(fmt, arg...)						\
		fprintf( stderr, fmt, ##arg )
	#define LOGERR(fmt, arg...)						\
		fprintf( stderr, "%s():%d: ERROR - " fmt, __func__, __LINE__, ##arg )
	#define LOGDBG(fmt, arg...)						\
		fprintf( stderr, "%s():%d: DEBUG - " fmt, __func__, __LINE__, ##arg )
#elif LOG_STYLE == LOG_STYLE_DATA_FILE
	/* requires access to FILE * variable named 'lsdf_fp' */
	#define LOGE_DF_OPEN() do {					\
		if (!lsdf_fp)						\
			lsdf_fp = fopen("/data/sepdrmlog.txt", "a+");	\
	} while (0)
	#define LOGE_DF_CLOSE() do {					\
		if (lsdf_fp) {						\
			fclose(lsdf_fp);				\
			lsdf_fp = NULL;					\
		}							\
	} while (0)
	#define LOGE(fmt, arg...) do {					\
		LOGE_DF_OPEN();						\
		fprintf( lsdf_fp, fmt, ##arg );				\
		/* LOGE_DF_CLOSE(); */					\
	} while (0)
	#define LOGERR(fmt, arg...) do {				\
		LOGE_DF_OPEN();						\
		fprintf( lsdf_fp, "%s():%d: ERROR - " fmt, __func__,	\
			 __LINE__, ##arg );				\
		/* LOGE_DF_CLOSE(); */					\
	} while (0)
	#define LOGDBG(fmt, arg...) do {				\
		LOGE_DF_OPEN();						\
		fprintf( lsdf_fp, "%s():%d: DEBUG - " fmt, __func__,	\
			 __LINE__, ##arg );				\
		/* LOGE_DF_CLOSE(); */					\
	} while (0)
#else
	#define LOGE(fmt, arg...)
	#define LOGERR(fmt, arg...)
	#define LOGDBG(fmt, arg...)
#endif /* LOG_STYLE */

#else /* ANDROID */

#ifdef MSVS
	#define LOGE(fmt, ...)						\
		fprintf( stderr, fmt, __VA_ARGS__ )
	#define LOGERR(fmt, ...)						\
		fprintf( stderr, \
		     "%s():%d: ERROR - " fmt, __func__, __LINE__, __VA_ARGS__ )
	#define LOGDBG(fmt, ...)						\
		fprintf( stderr, \
		     "%s():%d: DEBUG - " fmt, __func__, __LINE__, __VA_ARGS__ )
#else   //  MSVS
	#define LOGE(fmt, arg...)						\
		fprintf( stderr, ANDROID_LOG_ERROR, LOG_TAG, fmt, ##arg )
	#define LOGERR(fmt, arg...)						\
		fprintf( stderr, ANDROID_LOG_ERROR, LOG_TAG,		\
		     "%s():%d: ERROR - " fmt, __func__, __LINE__, ##arg )
	#define LOGDBG(fmt, arg...)						\
		fprintf( stderr, ANDROID_LOG_ERROR, LOG_TAG,		\
		     "%s():%d: DEBUG - " fmt, __func__, __LINE__, ##arg )
#endif  //  MSVS
#endif /* ANDROID */

#define PrintMem(msg, buf, len)						\
	do {								\
		size_t i;							\
		LOGE("%s", (msg));					\
		for (i = 0; i < len; i++) {				\
			LOGE("%02X ", (unsigned char)buf[i] & 0xff);	\
			if ((i+1) % 16 == 0)				\
				LOGE("\n");				\
		}							\
		LOGE("\n");						\
	} while(0)
#ifdef MSVS
#define Enter(msg, ...) LOGE("%s(): Enter - " msg, __func__, __VA_ARGS__ )
#define Leave(msg, ...) LOGE("%s(): Leave - " msg, __func__, __VA_ARGS__ )
#else   //  MSVS
#define Enter(msg, arg...) LOGE("%s(): Enter - " msg, __func__, ##arg)
#define Leave(msg, arg...) LOGE("%s(): Leave - " msg, __func__, ##arg)
#endif  //  MSVS

#endif /* __SEPLOG_H__ */
