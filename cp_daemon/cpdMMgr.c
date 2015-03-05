/*
 * cp_daemon/cpdMMgr.c
 *
 * Utilities functions that manages MMgr events.
 * These functions are all called in separeted threads. Events are triggered
 * by validating conditions.
 *
 * The main function is cpdHandleMMgrEvents which almost masterize triggering
 * of these conditions.
 *
 *
 */

/*
 * If we are not using MMGR, these functions have no effects
 */
#ifdef MODEM_MANAGER
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>

#define LOG_TAG "CPDD_MM"

#include "cpd.h"
#include "cpdInit.h"
#include "cpdSystemMonitor.h"
#include "cpdMMgr.h"
#include "cpdDebug.h"
#include "cpdModem.h"
#include "cpdUtil.h"

#include "mmgr_cli.h"
mmgr_cli_handle_t *mmgr_hdl = NULL;
char name[] = "CP Daemon";

/* Exit ongoing flag */
int bCpdMMgrStop = 1;

int cpdProcessModemUp(mmgr_cli_event_t *ev)
{
    pCPD_CONTEXT pCpd = (CPD_CONTEXT *)ev->context;

    if (pCpd == NULL) {
        LOGV("%%s pCpd is NULL\n", __FUNCTION__);
        return 0;
    }

    CPD_LOG(CPD_LOG_ID_TXT, "\n%u: %s()\n", getMsecTime(), __FUNCTION__);
    LOGV("%u: %s()", getMsecTime(), __FUNCTION__);

    LOGI("\tModem status received: MODEM_UP\n");
    LOGI("\tModem THREAD_STATE_OFF\n");
    pCpd->modemInfo.modemReadThreadState = THREAD_STATE_OFF;
    if(pCpd->pfSystemMonitorStart != NULL)
    {
        CPD_LOG(CPD_LOG_ID_TXT, "\tStarting SystemMonitor!\n");
        pCpd->pfSystemMonitorStart();
    }

    return 0;
}

int cpdProcessModemDown(mmgr_cli_event_t *ev)
{
    pCPD_CONTEXT pCpd = (CPD_CONTEXT *)ev->context;

    if (pCpd == NULL) {
        LOGV("%%s pCpd is NULL\n", __FUNCTION__);
        return 0;
    }

    CPD_LOG(CPD_LOG_ID_TXT, "\n%u: %s()\n", getMsecTime(), __FUNCTION__);
    LOGV("%u: %s()", getMsecTime(), __FUNCTION__);

    LOGI("\tModem status received: MODEM_DOWN\n");
    LOGI("\tModem THREAD_STATE_CANT_RUN\n");
    pCpd->systemMonitor.monitorThreadState = THREAD_STATE_TERMINATED;
    pCpd->modemInfo.modemReadThreadState = THREAD_STATE_CANT_RUN;

    /* Ensure close is done properly before trying to re-open */
    pthread_mutex_lock(&(pCpd->modemInfo.modemFdLock));
    LOGI("\tModem Close gsmtty\n");
    modemClose(&(pCpd->modemInfo.modemFd));
    pthread_mutex_unlock(&(pCpd->modemInfo.modemFdLock));

    return 0;
}

mmgr_cli_handle_t *cpdProcessMMgrInitConnection(void *pArg)
{
    pCPD_CONTEXT pCpd = NULL;
    mmgr_cli_handle_t *hdl = NULL;

    if (pArg == NULL) {
        return NULL;
    }
    pCpd = (pCPD_CONTEXT) pArg;

    CPD_LOG(CPD_LOG_ID_TXT, "\n%u: %s()\n", getMsecTime(), __FUNCTION__);
    LOGV("%u: %s()", getMsecTime(), __FUNCTION__);

    int ret = E_ERR_CLI_FAILED;
    int connect_tries =
        WAIT_FOR_MMGR_CONNECTION_SEC * 1000
        / MMGR_CONNECTION_RETRY_TIME_MS;

    do {
        /* Check if we should not exit */
        if (bCpdMMgrStop) {
            break;
        }

        /* Now we can do real job */
        if (hdl) {
            /* Destroying the handler for retry init */
            mmgr_cli_delete_handle(hdl);
            hdl = NULL;
        }

        LOGV("mmgr_cli_create_handle\n");
        ret = mmgr_cli_create_handle(&hdl, name, pCpd);

        if (ret != E_ERR_CLI_SUCCEED) {
            usleep(MMGR_CONNECTION_RETRY_TIME_MS * 1000);
            continue;
        }
        LOGV("mmgr_cli_subscribe_event - E_MMGR_EVENT_MODEM_UP\n");
        ret = mmgr_cli_subscribe_event(hdl, cpdProcessModemUp,
                E_MMGR_EVENT_MODEM_UP);

        if (ret != E_ERR_CLI_SUCCEED) {
            usleep(MMGR_CONNECTION_RETRY_TIME_MS * 1000);
            continue;
        }
        LOGV("mmgr_cli_subscribe_event - E_MMGR_EVENT_MODEM_DOWN\n");
        ret = mmgr_cli_subscribe_event(hdl, cpdProcessModemDown,
                E_MMGR_EVENT_MODEM_DOWN);

        if (ret != E_ERR_CLI_SUCCEED) {
            usleep(MMGR_CONNECTION_RETRY_TIME_MS * 1000);
            continue;
        }
        LOGV("mmgr_cli_subscribe_event - E_MMGR_EVENT_MODEM_OUT_OF_SERVICE\n");
        ret = mmgr_cli_subscribe_event(hdl, cpdProcessModemDown,
                E_MMGR_EVENT_MODEM_OUT_OF_SERVICE);

        if (ret != E_ERR_CLI_SUCCEED) {
            usleep(MMGR_CONNECTION_RETRY_TIME_MS * 1000);
            continue;
        }

        /* We succeeded until now, try to connect to mmgr.
         * In case of failure, we'll perform a new attempt from
         * beginning.
         */

        ret = mmgr_cli_connect(hdl);
        if (ret == E_ERR_CLI_SUCCEED) {
            break;
        }
        usleep(MMGR_CONNECTION_RETRY_TIME_MS * 1000);
    } while (connect_tries--);

    return hdl;
}

/*
 * cpdStartMMgrMonitor - Initialize specific handlers for modem events watching
 *
 */
int cpdStartMMgrMonitor(void)
{
    pCPD_CONTEXT pCpd = NULL;
    int result = CPD_ERROR;

    CPD_LOG(CPD_LOG_ID_TXT, "\n%u: %s()\n", getMsecTime(), __FUNCTION__);
    LOGV("%u: %s()\n", getMsecTime(), __FUNCTION__);

    pCpd = cpdGetContext();
    if (pCpd == NULL) {
        LOGV("%%s pCpd is NULL\n", __FUNCTION__);
        return result;
    }

    /* Reinitializing variables */
    bCpdMMgrStop = 0;
    mmgr_hdl = cpdProcessMMgrInitConnection(pCpd);

    if(mmgr_hdl == NULL) {
        cpdStopMMgrMonitor();
        return result;
    } else {
        result = CPD_OK;
    }

    /* Init done */
    CPD_LOG(CPD_LOG_ID_TXT, "\n%s MMgr communication is initialised\n",
            __FUNCTION__);
    LOGD("%s MMgr communication is initialised\n", __FUNCTION__);

    return result;
}

/*
 * cpdStopMMgrMonitor - Stop and destroy MMgr connection
 *
 * No error expected.
 */
void cpdStopMMgrMonitor(void)
{
    /* Set flag to 1 and wake up each thread */
    bCpdMMgrStop = 1;

    /* Destroying lib stuff */
    if(mmgr_hdl) {
        mmgr_cli_disconnect(mmgr_hdl);
        mmgr_cli_delete_handle(mmgr_hdl);
    }
}
#endif /* MODEM_MANAGER */
