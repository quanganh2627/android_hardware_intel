/*
 *  hardware/Intel/cp_daemon/cpdDebug.h
 *
 * Proprietary (extra) debug & logging for CP DAEMON.
 * This is NOT production code!For testing and development only.
 * In production code this code won't be built and included in the binary.
 *
 * Creates extra log files in CPD_LOG_DIR     "/data/logs/gps"; where it stores data and executioon traces.
 * Note: GPS library saves proprietary CSR logs into the same directory as CPDD.
 *
 * Martin Junkar 09/18/2011
 *
 */

#ifndef __CPDDEBUG_H__
#define __CPDDEBUG_H__

#include <stdarg.h>
#include <stdio.h>
#include <pthread.h>

#define CPD_LOG_ID_CONSOLE          1
#define CPD_LOG_ID_MODEM_RX         2
#define CPD_LOG_ID_MODEM_TX         4
#define CPD_LOG_ID_MODEM_RXTX       8
#define CPD_LOG_ID_XML_RX           16
#define CPD_LOG_ID_XML_TX           32
#define CPD_LOG_ID_TXT              0x4000000

#define CLEAN_TRACE                 1
#define MARTIN_LOGGING              3
#define CPD_DEBUG_ADD_TIMESTAMP     1

#ifdef MARTIN_LOGGING
void cpdDebugInit(char *prefix);
void cpdDebugLog(int logID, const char *pFormat, ...);
void cpdDebugLogData(int logID, const char *pB, int len);
void cpdDebugClose( void );

/* Log macros */
extern pthread_mutex_t debugLock;
#define CPD_LOG_INT(prefix)                         cpdDebugInit(prefix)
#define CPD_LOG(log, format, args...)               do { pthread_mutex_lock(&debugLock); cpdDebugLog(log, format, ## args); pthread_mutex_unlock(&debugLock); } while(0)
#define CPD_LOG_DATA(log, buffer, len)              cpdDebugLogData(log, buffer, len)
#define CPD_LOG_CLOSE()                             cpdDebugClose( )

#else


#define CPD_LOG_INT(prefix)
#define CPD_LOG(log, format, args...)
#define CPD_LOG_DATA(log, buffer, len)
#define CPD_LOG_CLOSE()

#endif




#endif

