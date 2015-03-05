/*
 *  hardware/Intel/cp_daemon/cpdd.c
 *
 * Entry point and main thread for CP DAEMON (e911) location handler.
 * Secondary service is "pass-through" for other apps which need to comunicate with modem directly.
 * At this time Manufacturing test App uses this service. Pass-through interface is avaialbe via
 * TCP/IP sockets interface at localhost:4122. CPDD does not monitor, nor modify any data in the pass-through channel.
 *
 * CPDD runs as daemon in Android. It registers itself with modem for handling +CPOSR
 * location requests from wireless network. It uses gsmtty7 for modem comm.
 * CPDD creates severla threads: SystemMonitor, modem Rx, SocketServer and 1 Rx thread per open socket.
 * Most of memory is allocated statically or at startup and released on exit.
 * CPDD is supposed to be started from init.rc file at boot time.
 * CPDD can be starte/stopped like other services, with commands: start cpdd or stop cpdd
 *
 * At this time (September 2011) code contains extra debugging features which help resolving current issues with modem and telephony.
 * By default only Android logging is enabled at DEBUG level, proprietary logging is disabled by default. It is conterolled with a #define in cpdDebug.h.
 *
 * Martin Junkar 09/18/2011
 *
 *
 */

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>

#define LOG_NDEBUG 1    /* control debug logging */
#define LOG_TAG "CPDD"


#include "cpd.h"
#include "cpdInit.h"
#include "cpdStart.h"
#include "cpdModem.h"
#include "cpdMMgr.h"
#include "cpdUtil.h"
#include "cpdModemReadWrite.h"
#include "cpdSocketServer.h"
#include "cpdGpsComm.h"
#include "cpdXmlFormatter.h"
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


void daemonize(void) {
    int fd;
    pid_t pid, sid, parent;

    CPD_LOG(CPD_LOG_ID_TXT, "\nDeamonizing PID=%d, PPID=%d",  getpid(), getppid() );
    if ( getppid() == 1 ){
        return; /* already a deamon */
    }

    signal(SIGCHLD,cpdDeamonSignalHandler);
    signal(SIGUSR1,cpdDeamonSignalHandler);
    signal(SIGALRM,cpdDeamonSignalHandler);

    switch (fork()) {
    case 0:
        break;
    case -1:
        // Error
        exit(0);
        break;
    default:
        _exit(0);
    }

    if (setsid() < 0) {
        exit(0);
    }
    switch (fork()) {
    case 0:
        break;
    case -1:
        // Error
        exit(0);
        break;
    default:
        _exit(0);
    }

    /* enable access to filesystem */
    /* unmask(0); ? it is n ot in <sys/stat.h> */

    /* Cancel certain signals */
    signal(SIGCHLD,SIG_DFL); /* A child process dies */
    signal(SIGTSTP,SIG_IGN); /* Various TTY signals */
    signal(SIGTTOU,SIG_IGN);
    signal(SIGTTIN,SIG_IGN);
    signal(SIGHUP, SIG_IGN); /* Ignore hangup signal */
    signal(SIGTERM,SIG_DFL); /* Die on SIGTERM */

    if ((chdir("/")) < 0) {
        LOGW("PID=%d, PPID=%d, !ERROR, Can't cd / ",  getpid(), getppid() );
        CPD_LOG(CPD_LOG_ID_TXT, "\n PID=%d, PPID=%d, !ERROR, Can't cd / ",  getpid(), getppid() );
//        exit(0);
    }

    fd = open("/dev/null", O_RDONLY);
    if (fd != 0) {
        dup2(fd, 0);
        close(fd);
    }
    fd = open("/dev/null", O_WRONLY);
    if (fd != 1) {
        dup2(fd, 1);
        close(fd);
    }
    fd = open("/dev/null", O_WRONLY);
    if (fd != 2) {
        dup2(fd, 2);
        close(fd);
    }
}




/*
Main entry point for CPDD - CP Daemon
*/
int main(int argc, char *argv[])
{
    int result;
    int ret;
    pid_t pid, parent;
    pCPD_CONTEXT pCpd;
    unsigned int t0;
    sigset_t waitset;
    int sig;

    LOGD("Starting %s", argv[0]);
    t0 = getMsecTime();
    CPD_LOG_INT("CPDD");

    daemonize();
    pid = getpid();
    parent = getppid();


    pCpd = cpdInit();
    CPD_LOG(CPD_LOG_ID_TXT, "\n%s v.%08X, deamon PID=%d, PPID=%d",  argv[0], CPD_MSG_VERSION, pid, parent);
    LOGV("%s v.%08X, deamon PID=%d, PPID=%d",  argv[0], CPD_MSG_VERSION, pid, parent);
    if (pCpd == NULL) {
        CPD_LOG(CPD_LOG_ID_TXT, "CPD context is NULL!!!");
        LOGE("CPD context is NULL!!!");
        CPD_LOG_CLOSE();
        return 0;
    }
    result = cpdStart(pCpd);
    if (result == CPD_OK) {
        /* Creating MMgr threads if MMgr is available or
         * Starting SystemMonitor without Mmgr only.
         */
        result = cpdStartMMgrMonitor();
        if (result == CPD_OK) {
            sigemptyset(&waitset);
            sigaddset(&waitset, SIGHUP);
            sigaddset(&waitset, SIGTERM);
            sigaddset(&waitset, SIGSTOP);
            sigprocmask(SIG_BLOCK, &waitset, NULL);
            CPD_LOG(CPD_LOG_ID_TXT, "\n%u: Deamon is ready and WAITing on signals (%d, %d, %d)\n", getMsecTime(),  SIGHUP, SIGTERM, SIGSTOP);
            LOGV("%u: Deamon is ready and WAITing on signals (%d, %d, %d)", getMsecTime(),  SIGHUP, SIGTERM, SIGSTOP);
            sigwait(&waitset, &sig);
            CPD_LOG(CPD_LOG_ID_TXT, "\n%u:Deamon exited WAIT with signal %d \n", getMsecTime(), sig);
            LOGD("%u:Deamon exited WAIT with signal %d \n", getMsecTime(), sig);
        }
    }
    CPD_LOG(CPD_LOG_ID_TXT, "\n%u:STOP\n", getMsecTime());
    LOGV("%u:STOP", getMsecTime());

    cpdStop(pCpd);
    CPD_LOG(CPD_LOG_ID_TXT, "\n%u:END\n", getMsecTime());
    LOGD("%u:END", getMsecTime());
    CPD_LOG_CLOSE();

    return 0;

}

