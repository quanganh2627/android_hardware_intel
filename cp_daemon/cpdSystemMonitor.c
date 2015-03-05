/*
 * hardware/Intel/cp_daemon/cpdSystemMonitor.c
 *
 * System Monitor for CPDD.
 * SystemMonitor runs as a thread. It monotors connection with modem and socket connections.
 * In the case of problem(s) it wil restart connections, reinitialize everything, so that other parts of CPDD can proced with work.
 *
 * Martin Junkar 09/18/2011
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <cutils/sockets.h>
#include <sys/stat.h>
#include <poll.h>

#define LOG_TAG "CPDD_SM"
#define LOG_NDEBUG   1    /* control debug logging */


#include "cpd.h"
#include "cpdInit.h"
#include "cpdUtil.h"
#include "cpdModemReadWrite.h"
#include "cpdGpsComm.h"
#include "cpdDebug.h"

/* this is from kernel-mode PM driver */
#define OS_STATE_NONE           0
#define OS_STATE_ON         1
#define OS_STATE_BEFORE_EARLYSUSPEND    2

#define OS_PM_CURRENT_STATE_NAME "/sys/power/current_state"

#define OS_PMU_CURRENT_STATE_NAME "/sys/module/mid_pmu/parameters/s0ix"
static int pmufd = -1;


#define PM_STATE_BUFFER_SIZE    64


static void cpdSystemMonitorThreadSignalHandler(int );
static int cpdInitSystemPowerState(pCPD_CONTEXT pCpd);
static int cpdGetSystemPowerState(pCPD_CONTEXT);
void cpdCloseSystemPowerState(pCPD_CONTEXT );
static int cpdReadSystemPowerState(pCPD_CONTEXT );


static int cpdReadSystemPowerState(pCPD_CONTEXT pCpd)
{
    int state = CPD_ERROR;
    int nRd;
    char pmStateBuffer[PM_STATE_BUFFER_SIZE];
    lseek(pCpd->systemMonitor.pmfd, 0, SEEK_SET);
    nRd = read(pCpd->systemMonitor.pmfd, &pmStateBuffer, sizeof(pmStateBuffer)-1);
    if (nRd <= 0) {
        LOGE("%u: Unexpected PM value, nRd=%d", getMsecTime(),nRd);
        cpdCloseSystemPowerState(pCpd);
        return state;
    }
    pmStateBuffer[nRd] = 0;
    state = atoi(pmStateBuffer);
    return state;
}

/*
 * Initialize PM monitoring.
 * return curent System power mode: CPD_OK = Active, CPD_NOK = Inactive
 */
static int cpdInitSystemPowerState(pCPD_CONTEXT pCpd)
{
    int result = CPD_ERROR;
    int state;

    CPD_LOG(CPD_LOG_ID_TXT, "\n%u:%s()\n", getMsecTime(), __FUNCTION__);
    LOGV("%u:%s()", getMsecTime(), __FUNCTION__);

    if (pCpd->systemMonitor.pmfd < 0) {
        pCpd->systemMonitor.pmfd = open(OS_PM_CURRENT_STATE_NAME, O_RDONLY);
        if(pCpd->systemMonitor.pmfd < 0) {
            LOGI("Power management is not enabled/supported.");
            CPD_LOG(CPD_LOG_ID_TXT, "Power management is not enabled/supported.");
        }
        else {
            state = cpdReadSystemPowerState(pCpd);
            if (state == OS_STATE_NONE){
                result = CPD_OK;
            }
            else if (state == OS_STATE_ON){
                result = CPD_OK;
            }
            else {
                result = CPD_NOK;
            }
        }
    }

    CPD_LOG(CPD_LOG_ID_TXT, "\n%u:%s()=%d\n", getMsecTime(), __FUNCTION__, result);
    LOGV("%u:%s()=%d", getMsecTime(), __FUNCTION__, result);
    return result;
}

/*
 * Monitor system power state.
 * Exit woth CPD_OK, if systemm is in active state.
 * Do not return if system is in inactive state.
 * Exit with an error if there is error in PM state.
 */
static int cpdGetSystemPowerState(pCPD_CONTEXT pCpd)
{
    int result = CPD_ERROR;
    int ret, state;
    struct pollfd fds;

    if (pCpd->systemMonitor.pmfd < 0)
    {
        return result;
    }
    fds.fd = pCpd->systemMonitor.pmfd;
    fds.events = POLLERR | POLLPRI;
    state = cpdReadSystemPowerState(pCpd);
    if ((state == OS_STATE_ON) || (state == OS_STATE_NONE)) {
        result = CPD_OK;
    }
    if (pCpd->systemMonitor.processingRequest == CPD_OK) {
        result = pCpd->systemMonitor.processingRequest;
    }
    while (result != CPD_OK) {
        ret = poll(&fds, 1, -1);
        if (ret > 0) {
            if (fds.revents & (POLLERR | POLLPRI)) {
                state = cpdReadSystemPowerState(pCpd);
                if ((state == OS_STATE_ON) || (state == OS_STATE_NONE)){
                    result = CPD_OK;
                    /* System is alive, exit, handle events.. */
                }
                else if (state < 0) {
                    result = CPD_ERROR;
                    /* PM error, exit! */
                    break;
                }
                else {
                    result = CPD_NOK;
                    /* sysytem is in inactive state, wait here */
                }
            }
        }
        else {
            result = CPD_ERROR;
            CPD_LOG(CPD_LOG_ID_TXT, "\n%u:%s(), poll resul=%d\n", getMsecTime(), __FUNCTION__, ret);
            LOGE("%u:%s(), poll result=%d",getMsecTime(), __FUNCTION__, ret);
            break;
        }
    }
    CPD_LOG(CPD_LOG_ID_TXT, "\n%u:%s()=%d\n", getMsecTime(), __FUNCTION__, result);
    LOGV("%u:%s()=%d",getMsecTime(), __FUNCTION__, result);
    return result;
}

/*
 * Close and disable Power management.
 */
void cpdCloseSystemPowerState(pCPD_CONTEXT pCpd)
{
    CPD_LOG(CPD_LOG_ID_TXT, "\n%u:%s()\n", getMsecTime(), __FUNCTION__);
    LOGV("%u:%s()", getMsecTime(), __FUNCTION__);
    if (pCpd->systemMonitor.pmfd >= 0)
    {
        close(pCpd->systemMonitor.pmfd);
        pCpd->systemMonitor.pmfd = CPD_ERROR;
    }
}



static void cpdSystemMonitorThreadSignalHandler(int sig)
{
    CPD_LOG(CPD_LOG_ID_TXT, "Signal(%s)=%d\n", __FUNCTION__, sig);
    LOGW("Signal(%s())=%d", __FUNCTION__, sig);
    switch (sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGQUIT:
    case SIGUSR1:
        pthread_exit(0);
    }
}

// return CPD_OK when OK, CPD_NOK otherwise
static int cpdSystemMonitorModem(pCPD_CONTEXT pCpd)
{
    int result = CPD_NOK;
    if (pCpd == NULL) {
        return result;
    }
    if (pCpd->modemInfo.keepOpenCtrl.keepOpen == 0) {
        return CPD_OK;
    }
    if ((pCpd->modemInfo.modemReadThreadState != THREAD_STATE_RUNNING) ||
        (pCpd->modemInfo.modemFd <= 0)) {
        if (getMsecDt(pCpd->modemInfo.keepOpenCtrl.lastOpenAt) >= pCpd->modemInfo.keepOpenCtrl.keepOpenRetryInterval) {
            result = cpdModemOpen(pCpd);
            return result;
        }
    }
    return CPD_OK;
}

// return CPD_OK when OK, CPD_NOK otherwise
static int cpdSystemMonitorRegisterForCP(pCPD_CONTEXT pCpd)
{
    int result = CPD_NOK;
    int needRegistering = CPD_OK;
    int doNotRegisterNow = CPD_NOK;
    if (pCpd == NULL) {
        return result;
    }
    if (pCpd->modemInfo.keepOpenCtrl.keepOpen == 0) {
        return CPD_OK;
    }

    if ((pCpd->modemInfo.registeredForCPOSR == 0) ||
         (pCpd->modemInfo.registeredForCPOSRat == 0)) {
        needRegistering = CPD_OK;
    }
    if (getMsecDt(pCpd->modemInfo.receivedCPOSRat) < CPD_MODEM_MONITOR_CPOSR_EVENT) {
        doNotRegisterNow = CPD_OK;
    }
    if (getMsecDt(pCpd->modemInfo.processingCPOSRat) < CPD_MODEM_MONITOR_CPOSR_EVENT) {
        doNotRegisterNow = CPD_OK;
    }
    result = CPD_OK;
    if (pCpd->modemInfo.modemReadThreadState == THREAD_STATE_RUNNING) {
        if ((getMsecDt(pCpd->modemInfo.lastDataReceived) > CPD_MODEM_MONITOR_RX_INTERVAL) &&
            (getMsecDt(pCpd->modemInfo.lastDataSent) > CPD_MODEM_MONITOR_TX_INTERVAL)) {
            if ((needRegistering == CPD_OK) && (doNotRegisterNow == CPD_NOK)) {
                result = cpdModemInitForCP(pCpd);
            }
            else {
                CPD_LOG(CPD_LOG_ID_TXT, "\n %u, Skip CPOSR registration, %d, %d", getMsecTime(), needRegistering, doNotRegisterNow);
            }
        }
    }
    return result;
}

// return CPD_OK when OK, CPD_NOK otherwise
static int cpdSystemMonitorGpsSocket(pCPD_CONTEXT pCpd)
{
    int result = CPD_NOK;
    if (pCpd == NULL) {
        return result;
    }

    if (pCpd->scIndexToGps >= 0) {
        if ((pCpd->scGps.clients[pCpd->scIndexToGps].state == SOCKET_STATE_OFF) ||
            (pCpd->scGps.clients[pCpd->scIndexToGps].state == SOCKET_STATE_TERMINATED)) {
            pCpd->scGps.clients[pCpd->scIndexToGps].state = SOCKET_STATE_OFF;
            pCpd->scIndexToGps = CPD_ERROR;
        }
    }
    if (pCpd->scIndexToGps == CPD_ERROR) {
        if (getMsecDt(pCpd->scGpsKeepOpenCtrl.lastOpenAt) >= pCpd->scGpsKeepOpenCtrl.keepOpenRetryInterval) {
            pCpd->scIndexToGps = cpdSocketClientOpen(&(pCpd->scGps), SOCKET_HOST_GPS, SOCKET_PORT_GPS);
            pCpd->scGpsKeepOpenCtrl.lastOpenAt = getMsecTime();
            pCpd->scGpsKeepOpenCtrl.keepOpenRetryCount++;
            if (pCpd->scIndexToGps >= 0)
            {
                result = CPD_OK;
            }
        }
    }
    else {
        if (pCpd->scGps.clients[pCpd->scIndexToGps].state != SOCKET_STATE_RUNNING) {
            cpdSocketClose(&(pCpd->scGps.clients[pCpd->scIndexToGps]));
        }
        else {
            result = CPD_OK;
        }
    }
    return result;
}

void cpdSystemMonitorGPSOnOff(pCPD_CONTEXT pCpd)
{
    int sendStop = CPD_NOK;
    int tRequired = -1;
    CPD_LOG(CPD_LOG_ID_TXT, "\n%u: %s(%u)", getMsecTime(), __FUNCTION__, pCpd->request.status.requestReceivedAt);
    LOGV("%u: %s()", getMsecTime(), __FUNCTION__);
    if (pCpd->request.status.requestReceivedAt > 0)
    {
        CPD_LOG(CPD_LOG_ID_TXT, "\n%u: reqRec, %u, %u, %u, %u, NisOK=%d, tReq=%d", getMsecTime(),
                pCpd->request.status.requestReceivedAt,
                pCpd->request.status.responseFromGpsReceivedAt,
                pCpd->request.status.responseSentToModemAt,
                pCpd->request.status.stopSentToGpsAt,
                cpdIsNumberOfResponsesSufficientForRequest(pCpd),
                cpdCalcRequredTimneToServiceRequest(pCpd)
        );
        if ((pCpd->request.status.requestReceivedAt <= pCpd->request.status.responseFromGpsReceivedAt) &&
            (pCpd->request.status.requestReceivedAt <= pCpd->request.status.responseSentToModemAt))
        {
            if (pCpd->request.status.requestReceivedAt > pCpd->request.status.stopSentToGpsAt)
            {
                if (cpdIsNumberOfResponsesSufficientForRequest(pCpd) == CPD_OK)
                {
                    LOGD("%u: Stopping GPS, requests fulfilled", getMsecTime());
                    CPD_LOG(CPD_LOG_ID_TXT, "\n%u: %u: Stopping GPS, requests fulfilled", getMsecTime());
                    sendStop = CPD_OK;
                }
            }
        }
        /* check total timeout */
        if (pCpd->request.status.requestReceivedAt > pCpd->request.status.stopSentToGpsAt)
        {
            if (pCpd->request.status.requestReceivedAt > pCpd->request.status.stopSentToGpsAt)
            {
                tRequired = cpdCalcRequredTimneToServiceRequest(pCpd);
                CPD_LOG(CPD_LOG_ID_TXT, "\n%u: tRequired= %d", getMsecTime(), tRequired);
                if (tRequired > 0) {
                    tRequired = tRequired * 1800; /* use 1.8 margin */
                    if (getMsecDt(pCpd->request.status.requestReceivedAt) > (unsigned int) tRequired) {
                        LOGD("%u: Stopping GPS, total timeout!!!", getMsecTime());
                        CPD_LOG(CPD_LOG_ID_TXT, "\n%u: %u: Stopping GPS, total timeout!!!", getMsecTime());
                        sendStop = CPD_OK;
                    }
                }
                if (pCpd->request.status.nResponsesSent == 0) {
                    if (getMsecDt(pCpd->request.status.requestReceivedAt) > 120000) {
                        LOGD("%u: Stopping GPS, total timeout, no fix!!!", getMsecTime());
                        CPD_LOG(CPD_LOG_ID_TXT, "\n%u: %u: Stopping GPS, total timeout, no fix!!!", getMsecTime());
                        sendStop = CPD_OK;
                    }
                }
            }
        }
    }
    if (sendStop == CPD_OK){
        cpdSendAbortToGps(pCpd);
    }
}

void *cpdSystemMonitorThread( void *pArg)
{
    int result = CPD_ERROR;
    int n = 0;
    pCPD_CONTEXT pCpd;
    struct sigaction sigact;

    if (pArg == NULL) {
        return NULL;
    }

    CPD_LOG(CPD_LOG_ID_TXT, "\n%u: %s()\n", getMsecTime(), __FUNCTION__);
    LOGV("%u: %s()", getMsecTime(), __FUNCTION__);
    /* Register the SIGUSR1 signal handler */
    memset(&sigact, 0, sizeof(sigact));
    sigact.sa_handler = cpdSystemMonitorThreadSignalHandler;
    sigaction(SIGUSR1, &sigact, NULL);

    pCpd = (pCPD_CONTEXT) pArg;
    pCpd->systemMonitor.lastCheck = 0;
    if (pCpd->systemMonitor.loopInterval < 100) {
        pCpd->systemMonitor.loopInterval = 100;
    }
    pCpd->systemMonitor.monitorThreadState = THREAD_STATE_RUNNING;
    CPD_LOG(CPD_LOG_ID_TXT , "\n  %u: last:%u, loopInterval:%u, TS=%d\n", getMsecTime(),pCpd->systemMonitor.lastCheck, pCpd->systemMonitor.loopInterval, pCpd->systemMonitor.monitorThreadState);
    LOGV("%u:%s(), last:%u, loopInterval:%u, TS=%d", getMsecTime(), __FUNCTION__, pCpd->systemMonitor.lastCheck, pCpd->systemMonitor.loopInterval, pCpd->systemMonitor.monitorThreadState);
    usleep(20000UL); /* wait before processing events, so that callers can exit and shut-down their threads */
    while (pCpd->systemMonitor.monitorThreadState == THREAD_STATE_RUNNING) {
        if (getMsecDt(pCpd->systemMonitor.lastCheck) >= pCpd->systemMonitor.loopInterval) {
            result = 0;
            result += cpdSystemMonitorModem(pCpd);
            result += (cpdSystemMonitorGpsSocket(pCpd) << 1);
            result += (cpdSystemMonitorRegisterForCP(pCpd) << 2);
            pCpd->systemMonitor.lastCheck = getMsecTime();
            if(result == 7) {
                /* If 3 OKs in a row, the monitor exits */
                n++;
                if(n == 3) break;
            }
            else {
                n = 0;
                CPD_LOG(CPD_LOG_ID_TXT, "\n%u: loop flags = %d\n", getMsecTime(), result);
            }
        }
        result = cpdGetSystemPowerState(pCpd);
        if (result == CPD_OK) {
            sleep((pCpd->systemMonitor.loopInterval / 4) * 1000UL);
        }
        else {
            if(result == CPD_NOK) {
                /* Read error: try to re-initialize PM, use longer delay than in active mode */
                usleep((pCpd->systemMonitor.loopInterval) * 1000UL);
                cpdInitSystemPowerState(pCpd);
            }
            else {
                /* the power management does not exist, use even longer delay */
                usleep((pCpd->systemMonitor.loopInterval * 2 ) * 1000UL);
            }
        }
    }
    pCpd->systemMonitor.monitorThreadState = THREAD_STATE_TERMINATED;
    CPD_LOG(CPD_LOG_ID_TXT, "\n %u: EXIT %s()", getMsecTime(), __FUNCTION__);
    LOGV("%u: EXIT %s()", getMsecTime(), __FUNCTION__);
    return NULL;
}

int cpdSystemMonitorStart( void )
{
    int result = CPD_ERROR;
    int retryCount = 3;
    pCPD_CONTEXT pCpd;
    pCpd = cpdGetContext();
    CPD_LOG(CPD_LOG_ID_TXT, "\n%u: %s()\n", getMsecTime(), __FUNCTION__);
    LOGV("%u: %s()\n", getMsecTime(), __FUNCTION__);
    if (pCpd == NULL) {
        return result;
    }
    pCpd->systemMonitor.pmfd = -1;
    cpdInitSystemPowerState(pCpd);

    while (result != CPD_OK)
    {
        if ((pCpd->systemMonitor.monitorThreadState == THREAD_STATE_OFF) ||
            (pCpd->systemMonitor.monitorThreadState == THREAD_STATE_TERMINATED))
        {
            pCpd->systemMonitor.monitorThreadState = THREAD_STATE_STARTING;
            result = pthread_create(&(pCpd->systemMonitor.monitorThread), NULL, cpdSystemMonitorThread, (void *) pCpd);
            usleep(10000UL);
            CPD_LOG(CPD_LOG_ID_TXT, "\n %u: %s():%d, %d\n", getMsecTime(), __FUNCTION__, result, pCpd->systemMonitor.monitorThreadState);
            LOGV("%u: %s():%d, %d\n", getMsecTime(), __FUNCTION__, result, pCpd->systemMonitor.monitorThreadState);
            if ((result == 0) && (pCpd->systemMonitor.monitorThreadState == THREAD_STATE_RUNNING))
            {
                result = CPD_OK;
            }
        }
        else
        {
            usleep(10000UL);
        }
        retryCount--;
        if (retryCount <= 0) break;
    }
    if (pCpd->systemMonitor.monitorThreadState == THREAD_STATE_STARTING)
    {
        pCpd->systemMonitor.monitorThreadState = THREAD_STATE_OFF;
    }
    CPD_LOG(CPD_LOG_ID_TXT, "\n %u: %s()=%d\n", getMsecTime(), __FUNCTION__, result);
    LOGV("%u: %s()=%d", getMsecTime(), __FUNCTION__, result);
    return result;
}

int cpdSystemMonitorStop(pCPD_CONTEXT pCpd)
{
    int count = 1000;
    CPD_LOG(CPD_LOG_ID_TXT, "\n%u: %s()\n", getMsecTime(), __FUNCTION__);
    LOGV("%u: %s()", getMsecTime(), __FUNCTION__);
    if (pCpd == NULL) {
        return CPD_ERROR;
    }
    cpdCloseSystemPowerState(pCpd);
    usleep(1000);
    pCpd->systemMonitor.monitorThreadState = THREAD_STATE_TERMINATE;
    usleep(10000);
    if (pCpd->systemMonitor.monitorThreadState != THREAD_STATE_TERMINATED) {
        pthread_kill(pCpd->systemMonitor.monitorThread, SIGUSR1);
        usleep(100);
        pCpd->modemInfo.modemReadThreadState = THREAD_STATE_TERMINATED;
    }
    CPD_LOG(CPD_LOG_ID_TXT, "\n%u: EXIT %s()", getMsecTime(), __FUNCTION__);
    LOGV("%u: EXIT %s()", getMsecTime(), __FUNCTION__);
    return CPD_OK;
}



static void cpdSystemActiveMonitorThreadSignalHandler(int sig)
{
    CPD_LOG(CPD_LOG_ID_TXT, "Signal(%s)=%d\n", __FUNCTION__, sig);
    LOGW("Signal(%s())=%d", __FUNCTION__, sig);
    switch (sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGQUIT:
    case SIGUSR1:
        pthread_exit(0);
    }
}


void *cpdSystemActiveMonitorThread( void *pArg)
{
    int result = CPD_ERROR;
    pCPD_CONTEXT pCpd;
    struct sigaction sigact;

    if (pArg == NULL) {
        return NULL;
    }

    CPD_LOG(CPD_LOG_ID_TXT, "\n%u: %s()\n", getMsecTime(), __FUNCTION__);
    LOGV("%u: %s()", getMsecTime(), __FUNCTION__);
    /* Register the SIGUSR1 signal handler */
    memset(&sigact, 0, sizeof(sigact));
    sigact.sa_handler = cpdSystemActiveMonitorThreadSignalHandler;
    sigaction(SIGUSR1, &sigact, NULL);

    pCpd = (pCPD_CONTEXT) pArg;
    pCpd->activeMonitor.lastCheck = 0;
    if (pCpd->activeMonitor.loopInterval < 1000) {
        pCpd->activeMonitor.loopInterval = 1000;
    }
    pCpd->activeMonitor.monitorThreadState = THREAD_STATE_RUNNING;
    pCpd->activeMonitor.lastCheck = 0;
    CPD_LOG(CPD_LOG_ID_TXT , "\n  %u: last:%u, loopInterval:%u, TS=%d\n", getMsecTime(),pCpd->activeMonitor.lastCheck, pCpd->activeMonitor.loopInterval, pCpd->activeMonitor.monitorThreadState);
    LOGV("%u:%s(), last:%u, loopInterval:%u, TS=%d", getMsecTime(), __FUNCTION__, pCpd->activeMonitor.lastCheck, pCpd->activeMonitor.loopInterval, pCpd->activeMonitor.monitorThreadState);
    while (pCpd->activeMonitor.monitorThreadState == THREAD_STATE_RUNNING) {
        cpdSystemMonitorGPSOnOff(pCpd);
        pCpd->activeMonitor.lastCheck = getMsecTime();
        usleep(pCpd->activeMonitor.loopInterval);
        if (isCpdSessionActive(pCpd) != CPD_OK) {
            break;
        }
        if (pCpd->activeMonitor.processingRequest == CPD_NOK) {
            break;
        }
    }
    pCpd->activeMonitor.monitorThreadState = THREAD_STATE_TERMINATED;
    CPD_LOG(CPD_LOG_ID_TXT, "\n %u: EXIT %s()", getMsecTime(), __FUNCTION__);
    LOGV("%u: EXIT %s()", getMsecTime(), __FUNCTION__);
    return NULL;
}


int cpdSystemActiveMonitorStart( void )
{
    int result = CPD_ERROR;
    pCPD_CONTEXT pCpd;
    pCpd = cpdGetContext();
    CPD_LOG(CPD_LOG_ID_TXT, "\n%u: %s()\n", getMsecTime(), __FUNCTION__);
    LOGV("%u: %s()\n", getMsecTime(), __FUNCTION__);
    if (pCpd == NULL) {
        return result;
    }
    pCpd->activeMonitor.loopInterval = (CPD_SYSTEMMONITOR_INTERVAL_ACTIVE_SESSION * 1000UL);
    pCpd->activeMonitor.processingRequest = CPD_OK;
    if ((pCpd->activeMonitor.monitorThreadState == THREAD_STATE_OFF) ||
        (pCpd->activeMonitor.monitorThreadState == THREAD_STATE_TERMINATED)) {
        pCpd->activeMonitor.monitorThreadState = THREAD_STATE_STARTING;
        result = pthread_create(&(pCpd->activeMonitor.monitorThread), NULL, cpdSystemActiveMonitorThread, (void *) pCpd);
        usleep(10000UL);
        CPD_LOG(CPD_LOG_ID_TXT, "\n %u: %s():%d, %d\n", getMsecTime(), __FUNCTION__, result, pCpd->activeMonitor.monitorThreadState);
        LOGV("%u: %s():%d, %d\n", getMsecTime(), __FUNCTION__, result, pCpd->activeMonitor.monitorThreadState);
        if ((result == 0) && (pCpd->activeMonitor.monitorThreadState == THREAD_STATE_RUNNING)) {
            result = CPD_OK;
        }
    }
    CPD_LOG(CPD_LOG_ID_TXT, "\n %u: %s()=%d\n", getMsecTime(), __FUNCTION__, result);
    LOGV("%u: %s()=%d", getMsecTime(), __FUNCTION__, result);
    return result;
}


