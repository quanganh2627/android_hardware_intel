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

#ifndef __VIDEO_ENCODER_LOG_H__
#define __VIDEO_ENCODER_LOG_H__

#define LOG_TAG "VideoEncoder"

// Components
#include <cutils/log.h>

//gLogLevel: 0, none log; 1, statistic log; 2, all logs
#if 1
#define LOG_S(...) ALOGI_IF(gLogLevel, __VA_ARGS__)
#define LOG_V(...) ALOGV_IF((gLogLevel > 1), __VA_ARGS__)
#define LOG_D(...) ALOGD_IF((gLogLevel > 1), __VA_ARGS__)
#define LOG_I(...) ALOGI_IF((gLogLevel > 1), __VA_ARGS__)
#define LOG_W(...) ALOGW_IF((gLogLevel > 1), __VA_ARGS__)
#define LOG_E ALOGE
#else
#define LOG_S printf
#define LOG_V printf
#define LOG_I printf
#define LOG_W printf
#define LOG_E printf
#endif

extern int32_t gLogLevel;
#define CHECK_VA_STATUS_RETURN(FUNC)\
    if (vaStatus != VA_STATUS_SUCCESS) {\
        LOG_E(FUNC" failed. vaStatus = 0x%08x\n", vaStatus);\
        return ENCODE_DRIVER_FAIL;\
    }

#define CHECK_VA_STATUS_GOTO_CLEANUP(FUNC)\
    if (vaStatus != VA_STATUS_SUCCESS) {\
        LOG_E(FUNC" failed. vaStatus = 0x%08x\n", vaStatus);\
        ret = ENCODE_DRIVER_FAIL; \
        goto CLEAN_UP;\
    }

#define CHECK_ENCODE_STATUS_RETURN(FUNC)\
    if (ret != ENCODE_SUCCESS) { \
        LOG_E(FUNC" Failed. ret = %d\n", ret); \
        return ret; \
    }

#define CHECK_ENCODE_STATUS_CLEANUP(FUNC)\
    if (ret != ENCODE_SUCCESS) { \
        LOG_E(FUNC" Failed, ret = %d\n", ret); \
        goto CLEAN_UP;\
    }

#define CHECK_NULL_RETURN_IFFAIL(POINTER)\
    if (POINTER == NULL) { \
        LOG_E("Invalid pointer\n"); \
        return ENCODE_NULL_PTR;\
    }
#endif /*  __VIDEO_ENCODER_LOG_H__ */
