/* Modem Manager - logs header file
**
** Copyright (C) Intel 2012
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
*/

#ifndef __MMGR_LOGS_HEADER__
#define __MMGR_LOGS_HEADER__

#ifndef STDIO_LOGS

extern char pattern[];

void logs_init(int id);

static inline const char *logs_get_pattern()
{
    return pattern;
}

#define LOG_NDEBUG 0
#define LOG_TAG MODULE_NAME
#include <utils/Log.h>

/* define debug LOG functions */
#define LOG_ERROR(format, args ...) \
    do { ALOGE("%s %s - " format, logs_get_pattern(), \
               __FUNCTION__, ## args); } while (0)
#define LOG_DEBUG(format, args ...) \
    do { ALOGD("%s %s - " format, logs_get_pattern(), \
               __FUNCTION__, ## args); } while (0)
#define LOG_VERBOSE(format, args ...) \
    do { ALOGV("%s %s - " format, logs_get_pattern(), \
               __FUNCTION__, ## args); } while (0)
#define LOG_INFO(format, args ...) \
    do { ALOGI("%s %s - " format, logs_get_pattern(), \
               __FUNCTION__, ## args); } while (0)

#else                           /* STDIO_LOGS */

#include <stdio.h>
#define LOG_ERROR(format, args ...) do { fprintf(stderr, "ERROR: %s - " \
                                                 format "\n", __FUNCTION__, \
                                                 ## args); } while (0)
#define LOG_DEBUG(format, args ...) do { fprintf(stdout, "DEBUG: %s - " \
                                                 format "\n", __FUNCTION__, \
                                                 ## args); } while (0)
#define LOG_VERBOSE(format, args ...) do { fprintf(stdout, "VERBOSE: %s - " \
                                                   format "\n", __FUNCTION__, \
                                                   ## args); } while (0)

#endif                          /* STDIO_LOGS */

/* display macros */
#define PRINT_KEY     "%-25s: "
#define PRINT_STRING  PRINT_KEY "%s\n"
#define PRINT_INTEGER PRINT_KEY "%d\n"
#define PRINT_BOOLEAN PRINT_STRING

#endif                          /* __MMGR_LOGS_HEADER__ */
