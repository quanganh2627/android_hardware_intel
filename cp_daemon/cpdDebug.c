/*
 *  hardware/Intel/cp_daemon/cpdDebug.c
 *
 * Proprietary (extra) debug & logging for CP DAEMON.
 * This is NOT production code!For testing and development only.
 * In production code this code won't be built and included in the binary.
 *
 * Creates extra log files in CPD_LOG_DIR     "/data/logs/gps"; where it stores data and executioon traces.
 *
 * Martin Junkar 09/18/2011
 *
 *
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <utils/Log.h>

#ifndef LOG_TAG
#define LOG_TAG "CPDD"
#endif

#include "cpd.h"
#include "cpdUtil.h"
#include "cpdDebug.h"

#ifdef MARTIN_LOGGING

#ifdef CLEAN_TRACE
pthread_mutex_t debugLock;
#endif

#define MAX_FILE_NAME_LEN   (254)
#define FILE_NAME_BUFF_LEN   (MAX_FILE_NAME_LEN+2)
#define LOG_FILES_HISTORY_SIZE  (255)
#define TIMETOSLEEP (5)
#define OPENLOOP (10)

FILE *pLog = NULL;
FILE *pModemRxLog = NULL;
FILE *pModemTxLog = NULL;
FILE *pModemRxTxLog = NULL;
FILE *pXmlRxLog = NULL;
FILE *pXmlTxLog = NULL;
int cpdDebugLogIndex = 0;
int cpdDebugLogTime = 0;
char cpdDebugFileName[FILE_NAME_BUFF_LEN];

static int cpdDebugFindLastIndex(char *pName)
{
    int result = 0;
    FILE *pLast = NULL;
    char fileName[FILE_NAME_BUFF_LEN];
    while (result < LOG_FILES_HISTORY_SIZE) {
        snprintf(fileName, MAX_FILE_NAME_LEN, "%s_%03d.txt", pName, result);
        pLast = fopen(fileName, "r");
        if (pLast == NULL) {
            break;
        }
        result++;
        fclose(pLast);
    }
    if (result >= LOG_FILES_HISTORY_SIZE) {
        result = 0;
    }
    return result;
}


void cpdDebugInit(char *pPrefix)
{
    int result, i;
    char fileName[FILE_NAME_BUFF_LEN];
    int isGps = 0;
    int loggingEnabled = 0; /* reserve values other than 0,1 for future use, expansion, logging granularity */

    fileName[0] = 0;

    /* check if logging is enabled */
    loggingEnabled = cpdParseConfigFile(GPS_CFG_FILENAME, fileName);
    if (loggingEnabled > 0) {
        /* GPS library also uses this directory to store log files */
        result = mkdir(fileName, 0777);
    }
    pLog = NULL;
    pModemRxLog = NULL;
    pModemTxLog = NULL;
    pModemRxTxLog = NULL;
    pXmlRxLog = NULL;
    pXmlTxLog = NULL;

    if (loggingEnabled <= 0) {
        return;
    }

    pthread_mutex_init(&debugLock, NULL);
    if (pPrefix != NULL) {
        snprintf(fileName, MAX_FILE_NAME_LEN, "%s/log_%s", fileName, pPrefix);
        if (strcmp(pPrefix, "GPS") != 0) {
                isGps = 1;
        }
    }
    else {
        snprintf(fileName, MAX_FILE_NAME_LEN, "%s/log_CPDD", fileName);
    }
    snprintf(cpdDebugFileName, MAX_FILE_NAME_LEN, "%s", fileName);
    cpdDebugLogIndex = cpdDebugFindLastIndex(cpdDebugFileName);
    snprintf(fileName, sizeof(fileName), "%s_%03d.txt", cpdDebugFileName, cpdDebugLogIndex);

    /* check if the filesystem is ready */
    for(i = 0; i < OPENLOOP; i++) {
        pLog = fopen(fileName, "w");
        if(pLog == NULL) {
            sleep(TIMETOSLEEP);
        }
        else {
            break;
        }
    }
    if(i == OPENLOOP) {
        LOGD("Failed to create CPD logfiles!\n");
    }

    if (isGps) {
        snprintf(fileName, sizeof(fileName), "%s_%03d_modem_rx.txt", cpdDebugFileName, cpdDebugLogIndex);
        pModemRxLog = fopen(fileName, "w");
        snprintf(fileName, sizeof(fileName), "%s_%03d_modem_tx.txt", cpdDebugFileName, cpdDebugLogIndex);
        pModemTxLog = fopen(fileName, "w");
        snprintf(fileName, sizeof(fileName), "%s_%03d_modem_rxtx.txt", cpdDebugFileName, cpdDebugLogIndex);
        pModemRxTxLog = fopen(fileName, "w");
        snprintf(fileName, sizeof(fileName), "%s_%03d_xml_rx.txt", cpdDebugFileName, cpdDebugLogIndex);
        pXmlRxLog = fopen(fileName, "w");
        snprintf(fileName, sizeof(fileName), "%s_%03d_xml_tx.txt", cpdDebugFileName, cpdDebugLogIndex);
        pXmlTxLog = fopen(fileName, "w");
    }
    cpdDebugLogIndex++;
}


FILE *cpdGetLogFp(int logID)
{
    FILE *fp = NULL;
    switch (logID) {
        case CPD_LOG_ID_TXT:
            fp = pLog;
            break;
        case CPD_LOG_ID_MODEM_RX:
            fp = pModemRxLog;
            break;
        case CPD_LOG_ID_MODEM_TX:
            fp = pModemTxLog;
            break;
        case CPD_LOG_ID_MODEM_RXTX:
            fp = pModemRxTxLog;
            break;
        case CPD_LOG_ID_XML_RX:
            fp = pXmlRxLog;
            break;
    }
    return fp;
}

void cpdDebugLog(int logID, const char *pFormat, ...)
{
    FILE *fp = cpdGetLogFp(logID);
    int result;
    int console = 0;
    int i;
    unsigned int mask;
    va_list args;
#ifdef CPD_DEBUG_ADD_TIMESTAMP
    char timeStampStr[128];
    char formatString[512];
    formatString[0] = 0;
    getTimeString(timeStampStr, 120);
#endif
    if ((logID == 0) || ((logID & CPD_LOG_ID_CONSOLE) != 0)) {
        console = 1;
    }
    va_start(args, pFormat); //Requires the last fixed parameter (to get the address)
#ifdef CPD_DEBUG_ADD_TIMESTAMP
    if (pFormat != NULL) {
        if (strlen(pFormat) < 500) {
            i = 0;
            if ((pFormat[i] == '\r') || (pFormat[i] == '\n')) {
                i++;
                if (pFormat[i] == '\n') {
                    i++;
                }
                snprintf(formatString, 510, "\n%s%s", timeStampStr, &(pFormat[i]));
            }
        }
    }
#endif
    i = 1;
    while (logID != 0) {
        mask = (1 << i);
        fp = cpdGetLogFp((logID & mask));
        if (fp != NULL) {
#ifdef CPD_DEBUG_ADD_TIMESTAMP
        if (formatString[0] != 0) {
            result = vfprintf(fp, formatString, args);
        }
        else {
            result = vfprintf(fp, pFormat, args);
        }
#else
            result = vfprintf(fp, pFormat, args);
#endif
            if (result < 0) {
                cpdDebugClose();
                cpdDebugInit(NULL);
            }
            else {
                fflush(fp);
            }
        }
        logID = logID & (~mask);
        i++;
    }
    if (console) {
        vprintf(pFormat, args);
    }
    va_end(args);
}


void cpdDebugLogData(int logID, const char *pB, int len)
{
    int result;
    FILE *fp;
    int i = 1;
    unsigned int mask;
    int console = 0;

    if ((logID == 0) || ((logID & CPD_LOG_ID_CONSOLE) != 0)) {
        console = 1;
    }
    while (logID != 0) {
        mask = (1 << i);
        fp = cpdGetLogFp((logID & mask));
        if (fp != NULL) {
            result = fwrite(pB, 1, len, fp);
            if (result < 0) {
                cpdDebugClose();
                cpdDebugInit(NULL);
            }
            else {
                fflush(fp);
            }

        }
        logID = logID & (~mask);
        i++;
    }
    if (console) {
        printf("\r\nLogData(%d)=[%s]", len, pB);
    }
}

void cpdDebugClose( void )
{
    if (pLog != NULL) {
        fclose(pLog);
    }
    if (pModemRxLog != NULL) {
        fclose(pModemRxLog);
    }
    if (pModemTxLog != NULL) {
        fclose(pModemTxLog);
    }
    if (pModemRxTxLog != NULL) {
        fclose(pModemRxTxLog);
    }
    if (pXmlRxLog != NULL) {
        fclose(pXmlRxLog);
    }
    if (pXmlTxLog != NULL) {
        fclose(pXmlTxLog);
    }
}

#endif
