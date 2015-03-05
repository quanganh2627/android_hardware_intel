/*
 *  hardware/Intel/cp_daemon/cpd.c
 *
 * Entry point and main thread for CP DAEMON TestApplication (e911) location handler.
 * This is NOT production code!For testing and development only.
 * This App simulates CPDD.
 *
 * Martin Junkar 09/18/2011
 *
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <termios.h>
#include <sys/poll.h>

#define LOG_NDEBUG 0    /* control debug logging */
#define LOG_TAG "CPD"


#include "cpd.h"
#include "cpdInit.h"
#include "cpdModem.h"
#include "cpdUtil.h"
#include "cpdModemReadWrite.h"
#include "cpdGpsComm.h"
#include "cpdXmlParser.h"
#include "cpdXmlFormatter.h"
#include "cpdStart.h"
#include "cpdSystemMonitor.h"

#include "cpdDebug.h"

static int cpdDeamonRun;
static void cpdDeamonSignalHandler(int sig)
{
    pid_t pid;
    CPD_LOG(CPD_LOG_ID_TXT, "Signal(%s)=%d\n", __FUNCTION__, sig);
    LOGV("Signal(%s)=%d", __FUNCTION__, sig);
    switch (sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGQUIT:
    case SIGINT:
    case SIGUSR1:
    case SIGSTOP:
        pid = getpid();
        kill(pid, SIGSTOP);
    }
}

int cpdSendStopRequestToGps_t(pCPD_CONTEXT pCpd)
{
    int result = CPD_OK;
    memset(&(pCpd->request), 0, sizeof(REQUEST_PARAMS));
    memset(&(pCpd->response), 0, sizeof(RESPONSE_PARAMS));
    pCpd->request.flag = REQUEST_FLAG_POS_MEAS;
    pCpd->request.posMeas.flag = POS_MEAS_ABORT;
    pCpd->modemInfo.sentCPOSok = CPD_NOK;
    pCpd->modemInfo.processingCPOSRat = getMsecTime();
    if (pCpd->pfCposrMessageHandlerInCpd != NULL) {
        memset(&(pCpd->request.dbgStats), 0, sizeof(POS_RESP_MEASUREMENTS));
        pCpd->request.dbgStats.posRequestedByNetwork = getMsecTime();
        pCpd->pfCposrMessageHandlerInCpd(pCpd);
    }

    return result;
}

int cpdSendRequestMSBToGps_t(pCPD_CONTEXT pCpd)
{
    int result = CPD_OK;
    memset(&(pCpd->request), 0, sizeof(REQUEST_PARAMS));
    memset(&(pCpd->response), 0, sizeof(RESPONSE_PARAMS));
    pCpd->request.flag = REQUEST_FLAG_POS_MEAS;
    pCpd->request.posMeas.flag = POS_MEAS_RRLP;
    pCpd->request.posMeas.posMeas_u.rrlp_meas.method_type = GPP_METHOD_TYPE_MS_BASED;
    pCpd->modemInfo.sentCPOSok = CPD_NOK;
    pCpd->modemInfo.processingCPOSRat = getMsecTime();
    if (pCpd->pfCposrMessageHandlerInCpd != NULL) {
        memset(&(pCpd->request.dbgStats), 0, sizeof(POS_RESP_MEASUREMENTS));
        pCpd->request.dbgStats.posRequestedByNetwork = getMsecTime();
        pCpd->pfCposrMessageHandlerInCpd(pCpd);
    }

    return result;
}

int cpdSendRequestMSAToGps_t(pCPD_CONTEXT pCpd)
{
    int result = CPD_OK;
    memset(&(pCpd->request), 0, sizeof(REQUEST_PARAMS));
    memset(&(pCpd->response), 0, sizeof(RESPONSE_PARAMS));
    pCpd->request.flag = REQUEST_FLAG_POS_MEAS;
    pCpd->request.posMeas.flag = POS_MEAS_RRLP;
    pCpd->request.posMeas.posMeas_u.rrlp_meas.method_type = GPP_METHOD_TYPE_MS_ASSISTED;
    pCpd->request.posMeas.posMeas_u.rrlp_meas.resp_time_seconds = 60;
    pCpd->modemInfo.sentCPOSok = CPD_NOK;
    pCpd->modemInfo.processingCPOSRat = getMsecTime();
    if (pCpd->pfCposrMessageHandlerInCpd != NULL) {
        memset(&(pCpd->request.dbgStats), 0, sizeof(POS_RESP_MEASUREMENTS));
        pCpd->request.dbgStats.posRequestedByNetwork = getMsecTime();
        pCpd->pfCposrMessageHandlerInCpd(pCpd);
    }

    return result;
}

#define MSA  1
#define MSB  2
int cpdParseCmdLine(pCPD_CONTEXT pCpd, int argc, char *argv[])
{
    int result = MSB;
    int i;
    for (i = 1; i < argc; i++) {
        if (strncasecmp (argv[i], "-m", 2) == 0) {
            i++;
            if (i < argc) {
                if (strncasecmp (argv[i], "A", 1) == 0) {
                    result = MSA;
                }
            }
        }
        if (strncasecmp (argv[i], "-f", 2) == 0) {
            i++;
            if (i < argc) {
                strcpy(argv[i], pCpd->modemInfo.modemName);
            }
        }
    }
    return result;
}


int main(int argc, char *argv[])
{
    int ret, i;
    char *cmd_data;
    pCPD_CONTEXT pCpd;
    unsigned int t0;
    char file_name[128];
    int result;
    pid_t pid, parent;
    sigset_t waitset;
    int sig = 0;
    unsigned int tRequest;
    unsigned int tResponse;
    int mode = 0;


    LOGD("Starting %s", argv[0]);

    t0 = getMsecTime();
    CPD_LOG_INT("CPD");

    pid = getpid();
    parent = getppid();


    pCpd = cpdInit();
    if (pCpd == NULL) {
        return 1;
    }
    mode = cpdParseCmdLine(pCpd, argc, argv);

    CPD_LOG(CPD_LOG_ID_TXT | CPD_LOG_ID_CONSOLE, "\n%s Running as a deamon PID=%d, PPID=%d",  argv[0], pid, parent );
    LOGV("%s Running as a deamon PID=%d, PPID=%d",  argv[0], pid, parent);
    result = cpdStart(pCpd);
    cpdSystemMonitorStart();


    sigemptyset(&waitset);
    sigaddset(&waitset, SIGHUP);
    sigaddset(&waitset, SIGTERM);
    sigaddset(&waitset, SIGSTOP);
    sigprocmask(SIG_BLOCK, &waitset, NULL);
    CPD_LOG(CPD_LOG_ID_TXT | CPD_LOG_ID_CONSOLE, "\n%u: Deamon is ready and WAITing on signals (%d, %d, %d)\n", getMsecTime(),  SIGHUP, SIGTERM, SIGSTOP);
    LOGV("%u: Deamon is ready and WAITing on signals (%d, %d, %d)", getMsecTime(),  SIGHUP, SIGTERM, SIGSTOP);
//  sigwait(&waitset, &sig);
#ifdef CPDD_DEBUG_ENABLED
    CPD_LOG(CPD_LOG_ID_TXT | CPD_LOG_ID_CONSOLE, "\nCPD_ENG_BUILD");
#else
    CPD_LOG(CPD_LOG_ID_TXT | CPD_LOG_ID_CONSOLE, "\n!!! CPD_ENG_BUILD is not defined");
#endif
{
//    cpdSendStopRequestToGps_t(pCpd);
    sleep(1);
    int i = 0;
    for (i = 0; i < 100; i++) {
        tRequest =  getMsecTime();
        if ((i % 2) == 0) {
            cpdSendRequestMSBToGps_t(pCpd);
            mode = 1;
            printf("\nMSB: ");
            while (pCpd->response.flag == CPD_NOK) {
                sleep(1);
                if (getMsecDt(tRequest) > 72000) {
                    cpdSendStopRequestToGps_t(pCpd);
                    break;
                }
            }
        }
        else {
            cpdSendRequestMSAToGps_t(pCpd);
            printf("\nMSA: ");
            mode = 2;
            while (1) {
                sleep(1);
                if (getMsecDt(tRequest) > 72000) {
                    cpdSendStopRequestToGps_t(pCpd);
                    break;
                }
            }
        }
        if (pCpd->response.flag == CPD_NOK) {
            CPD_LOG(CPD_LOG_ID_TXT | CPD_LOG_ID_CONSOLE, "\n%u:%d NO FIX, %u", getMsecTime(), i, getMsecDt(tRequest));
        }
        else {
            CPD_LOG(CPD_LOG_ID_TXT | CPD_LOG_ID_CONSOLE, "\n%u:%d TTFF, %u", getMsecTime(), i, getMsecDt(tRequest));
        }

        pCpd->request.flag = REQUEST_FLAG_NONE;
        sleep(3);
        pCpd->response.flag = CPD_NOK;
    }
}
    CPD_LOG(CPD_LOG_ID_TXT | CPD_LOG_ID_CONSOLE, "\n%u:Deamon exited WAIT with signal %d \n", getMsecTime(), sig);
    LOGD("%u:Deamon exited WAIT with signal %d \n", getMsecTime(), sig);
    CPD_LOG(CPD_LOG_ID_TXT | CPD_LOG_ID_CONSOLE, "\n%u:STOP\n", getMsecTime());
    LOGV("%u:STOP", getMsecTime());
    cpdStop(pCpd);
    CPD_LOG(CPD_LOG_ID_TXT | CPD_LOG_ID_CONSOLE, "\n%u:END\n", getMsecTime());
    LOGD("%u:END", getMsecTime());
    CPD_LOG_CLOSE();



    printf("\n");
    return 0;
}



