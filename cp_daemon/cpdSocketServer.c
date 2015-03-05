/*
 *  hardware/Intel/cp_daemon/cpdSocketServer.c
 *
 * TCP/IP sockets code.
 * Implements Server and Client type sockets on IPv4.
 * Methods for sockets reading & writing , error handling.
 * Each open socket is assigned read thread. User provides read-callback function pointer.
 * SocketServer groups together sockets for the same context - for example application can bundle together
 * all Client-type sockets into one SocketServer context.
 * All Server-type sockets (result of accept) are automatically buindled together into server-context.
 * This is utility-type code, can be reused elsewhere.
 *
 * TODO: impement UDP sockets.
 *
 * Martin Junkar 09/18/2011
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>  /* used with cleint sockets */
#include <sys/un.h> /* local sockets */
#include <sys/poll.h>
#include <pthread.h>

#define LOG_TAG "CPDD_SS"
#define LOG_NDEBUG 1    /* control debug logging */


#include "cpd.h"
#include "cpdUtil.h"
#include "cpdDebug.h"
#include "cpdSocketServer.h"


static void cpdSocketAcceptThreadSignalHandler(int sig)
{
    LOGW("%u: %s()=%d", getMsecTime(), __FUNCTION__, sig);
    CPD_LOG(CPD_LOG_ID_TXT | CPD_LOG_ID_CONSOLE, "Signal(%s)=%d\n", __FUNCTION__, sig);
    switch (sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGQUIT:
    case SIGUSR1:
        pthread_exit(0);
    }
}


int cpdSocketServerInit(pSOCKET_SERVER pSS)
{
    int result = CPD_NOK;
    int i;
    int maxConnections;
    int optval;
    struct sockaddr_un local;
    int len;

    CPD_LOG(CPD_LOG_ID_TXT, "%u: %s()", getMsecTime(), __FUNCTION__);
    LOGV("%u: %s()", getMsecTime(), __FUNCTION__);

    maxConnections = pSS->maxConnections;
    if (maxConnections < 0) {
        maxConnections = 1;
    }
    if (maxConnections > SOCKET_SERVER_MAX_FD) {
        maxConnections = SOCKET_SERVER_MAX_FD;
    }
    pSS->initialized = CPD_NOK;
    pSS->maxConnections = 0;
    pSS->sockfd = CPD_ERROR;

    CPD_LOG(CPD_LOG_ID_TXT, "\r\nsocket type= %d\n", pSS->type);
    if (pSS->type == SOCKET_SERVER_TYPE_SERVER) {
        pSS->localSocketname[0] = 0;
        pSS->sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (pSS->sockfd < 0) {
            return result-1;
        }
        CPD_LOG(CPD_LOG_ID_TXT, "\r\n cpdSocketServerInit(), Created socket %d\n", pSS->sockfd);
        optval = 1;
        bzero((char *) &(pSS->serverAddr), sizeof(struct sockaddr_in));
        setsockopt(pSS->sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
        pSS->serverAddr.sin_family = AF_INET;
        pSS->serverAddr.sin_addr.s_addr = INADDR_ANY;
        pSS->serverAddr.sin_port = htons(pSS->portNo);
        if (bind(pSS->sockfd, (struct sockaddr *) &(pSS->serverAddr), sizeof(struct sockaddr_in)) < 0) {
            LOGE("%u: %s(), can't bind socket, %08X:%04X", getMsecTime(), __FUNCTION__, pSS->serverAddr.sin_addr.s_addr, pSS->portNo);
            close(pSS->sockfd);
            CPD_LOG(CPD_LOG_ID_TXT, "\r\n cpdSocketServerInit(), !!! ERROR on binding");
            return (result - 2);
        }
        CPD_LOG(CPD_LOG_ID_TXT, "\r\n Socket %d bind OK\n", pSS->sockfd);
        pSS->type = SOCKET_SERVER_TYPE_SERVER;
    }
    else if (pSS->type == SOCKET_SERVER_TYPE_SERVER_LOCAL) {
        pSS->sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (pSS->sockfd < 0) {
            return result-1;
        }
        CPD_LOG(CPD_LOG_ID_TXT, "\r\n cpdSocketServerInit(), Created Local Socket fd=%d, name=%s", pSS->sockfd, pSS->localSocketname);
        bzero((char *) &(pSS->serverAddr), sizeof(struct sockaddr_in));
        local.sun_family = AF_LOCAL;
        snprintf((char*) &(local.sun_path), sizeof(local.sun_path), "%s", pSS->localSocketname);
        len = strlen(local.sun_path) + sizeof(local.sun_family);
        unlink(pSS->localSocketname);
        if (bind(pSS->sockfd, (struct sockaddr *)&local, len) < 0) {
            LOGE("%u: %s(), can't bind socket, %s", getMsecTime(), __FUNCTION__, pSS->localSocketname);
            close(pSS->sockfd);
            CPD_LOG(CPD_LOG_ID_TXT, "\r\n!!! ERROR on binding %s", pSS->localSocketname);
            return (result - 2);
        }
        CPD_LOG(CPD_LOG_ID_TXT, "\r\n Socket(%s) %d bind OK\n", pSS->localSocketname, pSS->sockfd);
        pSS->type = SOCKET_SERVER_TYPE_SERVER_LOCAL;
    }
    result = CPD_OK;
    pSS->initialized = result;
    pSS->maxConnections = maxConnections;
    for (i = 0; i < pSS->maxConnections; i++) {
        pSS->clients[i].state = SOCKET_STATE_OFF;
        pSS->clients[i].fd = CPD_ERROR;
        pSS->clients[i].pRxBuff = NULL;
        pSS->clients[i].pfReadCallback = NULL;
    }
    CPD_LOG(CPD_LOG_ID_TXT, "\r\n %s()= %d", __FUNCTION__, result);
    LOGD("%u: %s()=%d", getMsecTime(), __FUNCTION__, result);
    return result;

}

int cpdSocketServerFindFreeFD(pSOCKET_SERVER pSS)
{
    int result = CPD_ERROR;
    int i;
    for (i = 0; i < pSS->maxConnections; i++) {
        if ((pSS->clients[i].state == SOCKET_STATE_OFF) ||
            (pSS->clients[i].state == SOCKET_STATE_TERMINATED)) {
            if (pSS->clients[i].fd == CPD_ERROR) {
                result = i;
                break;
            }
        }
    }
    LOGV("%u: %s()=%d", getMsecTime(), __FUNCTION__, result);
    return result;
}

void  *cpdSocketReadThreadLoop(void *pArg)
{
    int indexSocketFD;
    int nRead;
    int result;
    struct sigaction sigact;
    pSOCKET_SERVER_READ_THREAD_LINK pSSRL;
    pSOCKET_SERVER pSS = NULL;
    pSOCKET_CLIENT pSc = NULL;


    LOGV("%u: %s()", getMsecTime(), __FUNCTION__);
    CPD_LOG(CPD_LOG_ID_TXT | CPD_LOG_ID_CONSOLE, "\n  %u: %s()\n", getMsecTime(), __FUNCTION__);
    /* Register the SIGUSR1 signal handler */
    memset(&sigact, 0, sizeof(sigact));
    sigact.sa_handler = cpdSocketAcceptThreadSignalHandler;
    sigaction(SIGUSR1, &sigact, NULL);

    pSSRL = (pSOCKET_SERVER_READ_THREAD_LINK) pArg;
    indexSocketFD = pSSRL->socketIndex;
    pSS = pSSRL->pSS;

    pSc = &(pSS->clients[indexSocketFD]);

    CPD_LOG(CPD_LOG_ID_TXT , "\r\n %u: %s(), pSc->pRxBuff=%p, pSc->pfReadCallback=%p",
            getMsecTime(), __FUNCTION__,
            pSc->pRxBuff, pSc->pfReadCallback);
    if (pSc->pRxBuff == NULL) {
        pSc->state = SOCKET_STATE_CANT_RUN;
        usleep(1000);
        return NULL;
    }
    pSc->state = SOCKET_STATE_RUNNING;
    pSSRL->socketIndex = CPD_ERROR; /* set flag that socket thread started OK */
    CPD_LOG(CPD_LOG_ID_TXT , "\r\n %u: %s(), pSc->fd=%d, pSc->state=%d, pSc->pRxBuff=%p, pSc->pfReadCallback=%p",
            getMsecTime(), __FUNCTION__,
            pSc->fd, pSc->state, pSc->pRxBuff, pSc->pfReadCallback);

    while ((pSc->fd > CPD_ERROR) &&
            (pSc->state == SOCKET_STATE_RUNNING)) {
        nRead = read(pSc->fd, pSc->pRxBuff, SOCKET_RX_BUFFER_SIZE-4);
        if (nRead <= 0) {
            LOGV("%u: %s(), read error, %d", getMsecTime(), __FUNCTION__, nRead);
            break;
        }
        /*  TODO: add lock for socket here */
        /* maybe not, changed logic, no lock needed now */
        if (pSc->pfReadCallback != NULL) {
            result = pSc->pfReadCallback(pSS,
                                         pSc->pRxBuff,
                                         nRead,
                                         indexSocketFD);
            if (result == CPD_ERROR) {
                LOGE("%u: %s(), read callback error, %d", getMsecTime(), __FUNCTION__, result);
                break;
            }
        }

    }

    pSc->state = SOCKET_STATE_TERMINATING;

    CPD_LOG(CPD_LOG_ID_TXT , "\r\n %u: %s(), Exiting read thread %d", getMsecTime(), __FUNCTION__, indexSocketFD);
    if (pSc->fd != CPD_ERROR) {
        close(pSc->fd);
        pSc->fd = CPD_ERROR;
    }
    if (pSc->pRxBuff != NULL) {
        CPD_LOG(CPD_LOG_ID_TXT , "\r\n Socket Rx buffer free(%p)", pSc->pRxBuff);
        free(pSc->pRxBuff);
        CPD_LOG(CPD_LOG_ID_TXT, " Socket Rx buffer freed OK\n");
        pSc->pRxBuff = NULL;
    }
    pSc->state = SOCKET_STATE_TERMINATED;
    LOGV("%u: %s()=%d", getMsecTime(), __FUNCTION__, pSc->state);
    return NULL;
}



void *cpdSocketAcceptThreadLoop(void *pArg)
{
    int result;
    struct sigaction sigact;
    int newsockfd;
    struct sockaddr_in cli_addr;
    socklen_t clilen;
    int clilen_local;
    struct sockaddr_un cli_addr_local;
    struct pollfd p;
    int poolResult;
    int indexFreeFD;
    int nRetry;
    pSOCKET_SERVER pSS = (pSOCKET_SERVER) pArg;
    SOCKET_SERVER_READ_THREAD_LINK ssrl;
    pSOCKET_CLIENT pSc = NULL;

    LOGV("%u: %s()", getMsecTime(), __FUNCTION__);
    /* Register the SIGUSR1 signal handler */
    memset(&sigact, 0, sizeof(sigact));
    sigact.sa_handler = cpdSocketAcceptThreadSignalHandler;
    sigaction(SIGUSR1, &sigact, NULL);

    CPD_LOG(CPD_LOG_ID_TXT | CPD_LOG_ID_CONSOLE, "\n  %u: %s()\n", getMsecTime(), __FUNCTION__);

    ssrl.socketIndex = CPD_ERROR;
    ssrl.pSS = pSS;
    clilen = sizeof(struct sockaddr_in);
    memset(&cli_addr, 0, clilen);
    listen(pSS->sockfd, 1);

    CPD_LOG(CPD_LOG_ID_TXT , "\r\n Listening on %d", pSS->sockfd);

    pSS->state = SOCKET_STATE_RUNNING;
    while ((pSS->sockfd != CPD_ERROR) && (pSS->state == SOCKET_STATE_RUNNING)) {
        newsockfd = CPD_ERROR;
        p.fd = pSS->sockfd;
        p.events = POLLERR | POLLHUP | POLLNVAL | POLLIN | POLLPRI;
        p.revents = 0;
        poolResult = poll( &p, 1, -1 );
        if (poolResult < 0) {
            LOGE("%u: %s(), poll error, %d", getMsecTime(), __FUNCTION__, poolResult);
            CPD_LOG(CPD_LOG_ID_TXT , "\r\n %u: %s(), poolResult= %d\n", getMsecTime(), __FUNCTION__, poolResult);
            pSS->state = SOCKET_STATE_TERMINATING;
            break;
        }
        if ((p.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            LOGE("%u: %s(), poll event error, %d, %04X", getMsecTime(), __FUNCTION__, poolResult, p.revents);
            CPD_LOG(CPD_LOG_ID_TXT , "\r\n pool.revents= %u\n", p.revents);
            pSS->state = SOCKET_STATE_TERMINATING;
            break;
        }
        if ((p.revents | POLLIN) == POLLIN) {
            CPD_LOG(CPD_LOG_ID_TXT , "\r\n %u:%s() poolResult=%d, pool.revents= %u\n", getMsecTime(), __FUNCTION__, poolResult, p.revents);
            if (pSS->type == SOCKET_SERVER_TYPE_SERVER) {
                newsockfd = accept(pSS->sockfd, (struct sockaddr *) &cli_addr, &clilen);
            }
            else { /* local sockets type, SOCKET_SERVER_TYPE_SERVER_LOCAL */
                clilen_local = sizeof(cli_addr_local);
                newsockfd = accept(pSS->sockfd, (struct sockaddr *)&cli_addr_local, &clilen_local);
            }
            CPD_LOG(CPD_LOG_ID_TXT , "\r\n Accepted %d\n", newsockfd);
            indexFreeFD = CPD_ERROR;
            if (newsockfd < 0) {
                LOGE("%u: %s(), accept error, %d", getMsecTime(), __FUNCTION__, newsockfd);
                CPD_LOG(CPD_LOG_ID_TXT , "\r\n %u: %s(), !!! ERROR on accept", getMsecTime(), __FUNCTION__);
            }
            else {
                indexFreeFD = cpdSocketServerFindFreeFD(pSS);
            }
            if (indexFreeFD != CPD_ERROR) {
                pSc = &(pSS->clients[indexFreeFD]);
                pSc->fd = newsockfd;
                pSc->pfReadCallback = pSS->pfReadCallback;
                memcpy(&(pSc->destAddr), &cli_addr, sizeof(struct sockaddr_in));
                strncpy(pSc->localSocketname, pSS->localSocketname, sizeof(pSc->localSocketname));
                pSc->state = SOCKET_STATE_STARTING;
                pSc->pRxBuff = malloc(SOCKET_RX_BUFFER_SIZE);

                ssrl.socketIndex = indexFreeFD;
                if (pSS->type == SOCKET_SERVER_TYPE_SERVER) {
                    CPD_LOG(CPD_LOG_ID_TXT , "\r\n %u: Accepted socket[%d] %d, %08X:%d\n", getMsecTime(), ssrl.socketIndex, newsockfd, ntohl(cli_addr.sin_addr.s_addr), ntohs(cli_addr.sin_port));
                }
                else {
                    CPD_LOG(CPD_LOG_ID_TXT , "\r\n %u: Accepted Local socket[%d] %d, %s\n", getMsecTime(), ssrl.socketIndex, newsockfd, pSc->localSocketname);
                }
                result = pthread_create(&(pSc->clientReadThread), NULL, cpdSocketReadThreadLoop, (void *) &(ssrl));
                usleep(100);
                nRetry = 7;
                if (result == 0) {
                    /* wait for read thread to start */
                    while (ssrl.socketIndex >= 0) {
                        usleep(1000);
                        nRetry--;
                        if (nRetry == 0) {
                            CPD_LOG(CPD_LOG_ID_TXT, "\r\n %u: !!! Warning, nRetry == 0, %d\n", getMsecTime(),  ssrl.socketIndex);
                            LOGE("%u: !!! Socket Warning, nRetry == 0, %d, %08X:%d \n", getMsecTime(),  ssrl.socketIndex, ntohl(cli_addr.sin_addr.s_addr), ntohs(cli_addr.sin_port));
                            break;
                        }
                    }
                }
                CPD_LOG(CPD_LOG_ID_TXT, "\r\n %u: Socket %d, %08X:%d is ready\n", getMsecTime(), newsockfd, ntohl(cli_addr.sin_addr.s_addr), ntohs(cli_addr.sin_port));
                LOGD("%u: Socket %d, %08X:%d is ready", getMsecTime(), newsockfd, ntohl(cli_addr.sin_addr.s_addr), ntohs(cli_addr.sin_port));

            }
            else {
                LOGE("%u: %s(), no more sockets available, %d, %d", getMsecTime(), __FUNCTION__, newsockfd, indexFreeFD);
                CPD_LOG(CPD_LOG_ID_TXT , "\r\n %u: cpdSocketAcceptThreadLoop(), !!! No more free socket handles, closing accepted socket", getMsecTime());
                if (newsockfd >= 0) {
                    close(newsockfd);
                }
            }
        }
    }

    if (pSS->sockfd != CPD_ERROR) {
        close(pSS->sockfd);
        pSS->sockfd = CPD_ERROR;
    }
    CPD_LOG(CPD_LOG_ID_TXT , "\r\n EXIT %s", __FUNCTION__);
    pSS->state = SOCKET_STATE_TERMINATED;
    return NULL;
}


int cpdSocketServerOpen(pSOCKET_SERVER pSS)
{
    int result = CPD_ERROR;
    CPD_LOG(CPD_LOG_ID_TXT | CPD_LOG_ID_CONSOLE, "\n  %u: %s()\n", getMsecTime(), __FUNCTION__);
    LOGV("%u: %s()", getMsecTime(), __FUNCTION__);
    if (pSS->initialized != CPD_OK) {
        LOGE("%u: %s()=%d", getMsecTime(), __FUNCTION__, result);
        return result;
    }

    if ((pSS->type == SOCKET_SERVER_TYPE_SERVER) || (pSS->type == SOCKET_SERVER_TYPE_SERVER_LOCAL)) {
        pSS->state = SOCKET_STATE_STARTING;
        result = pthread_create(&(pSS->socketAcceptThread), NULL,
                                cpdSocketAcceptThreadLoop, (void *) pSS);
    }
    if (result == 0) {
        result = CPD_OK;
        CPD_LOG(CPD_LOG_ID_TXT, "\r\n cpdSocketServerOpen(), accept thread started");
    }
    LOGD("%u: %s()=%d", getMsecTime(), __FUNCTION__, result);
    return result;
}



int cpdSocketClientOpen(pSOCKET_SERVER pSs, char *serverName, int serverPort)
{
    int result = CPD_ERROR;
    int indexFreeFD;
    int nTimeout;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    struct sockaddr_un serv_addr_local;
    pSOCKET_CLIENT pSc;
    SOCKET_SERVER_READ_THREAD_LINK ssrl;

    LOGV("%u: %s(%s:%d)", getMsecTime(), __FUNCTION__, serverName, serverPort);
    CPD_LOG(CPD_LOG_ID_TXT, "\n  %u: %s(%s:%d)\n", getMsecTime(), __FUNCTION__, serverName, serverPort);

    if (pSs->initialized != CPD_OK) {
        CPD_LOG(CPD_LOG_ID_TXT, "\n  %u: %s(%s:%d) = %d", getMsecTime(), __FUNCTION__, serverName, serverPort, result);
        LOGE("%u: %s(SS not initialized)=%d", getMsecTime(), __FUNCTION__, result);
        return result;
    }
    result--;
    ssrl.socketIndex = CPD_ERROR;
    ssrl.pSS = pSs;
    indexFreeFD = cpdSocketServerFindFreeFD(pSs);
    if (indexFreeFD == CPD_ERROR) {
        CPD_LOG(CPD_LOG_ID_TXT, "\n  %u: %s(%s:%d) = %d", getMsecTime(), __FUNCTION__, serverName, serverPort, result);
        LOGE("%u: %s(no more sockets avauilable)=%d", getMsecTime(), __FUNCTION__, result);
        return result;
    }
    result--;
    pSc = &(pSs->clients[indexFreeFD]);
    if (pSs->type == SOCKET_SERVER_TYPE_CLIENT) {
        pSc->fd = socket(AF_INET, SOCK_STREAM, 0);
    }
    else if (pSs->type == SOCKET_SERVER_TYPE_CLIENT_LOCAL) {
        pSc->fd = socket(AF_UNIX, SOCK_STREAM, 0);
    }
    if (pSc->fd < 0) {
        CPD_LOG(CPD_LOG_ID_TXT, "\n  %u: %s(%s:%d) = %d", getMsecTime(), __FUNCTION__, serverName, serverPort, result);
        LOGE("%u: %s(socket open failed)=%d", getMsecTime(), __FUNCTION__, result);
        return result;
    }
    result--;
    pSc->state = SOCKET_STATE_STARTING;
    CPD_LOG(CPD_LOG_ID_TXT, "\npSs->typ=%d", pSs->type);
    if (pSs->type == SOCKET_SERVER_TYPE_CLIENT) {
        server = gethostbyname(serverName);
        if (server == NULL) {
            LOGE("%u: %s(hostbyname failed)=%d", getMsecTime(), __FUNCTION__, result);
            CPD_LOG(CPD_LOG_ID_TXT, "\n  %u: %s(%s:%d) = %d", getMsecTime(), __FUNCTION__, serverName, serverPort, result);
            cpdSocketClose(pSc);
            return result;
        }
        result--;
        bzero((char *) &(pSc->destAddr), sizeof(struct sockaddr_in));
        pSc->destAddr.sin_family = AF_INET;
        bcopy((char *)server->h_addr,
             (char *)&(pSc->destAddr.sin_addr.s_addr),
             server->h_length);
        pSc->destAddr.sin_port = htons(serverPort);
        result = connect(pSc->fd,(struct sockaddr *) &(pSc->destAddr), sizeof(struct sockaddr_in));
        if (result < 0) {
            LOGE("%u: %s(connect failed)=%d", getMsecTime(), __FUNCTION__, result);
            cpdSocketClose(pSc);
            CPD_LOG(CPD_LOG_ID_TXT, "\n  %u: %s(%s:%d) = %d", getMsecTime(), __FUNCTION__, serverName, serverPort, result);
            usleep(1000);
            return result;
        }
    }
    else { /*SOCKET_SERVER_TYPE_CLIENT_LOCAL */
        int len;
        snprintf(pSc->localSocketname, sizeof(pSc->localSocketname), "%s", serverName);
        serv_addr_local.sun_family = AF_UNIX;
        snprintf((char *) &(serv_addr_local.sun_path), sizeof(serv_addr_local.sun_path), "%s", pSc->localSocketname);
        len = strlen(serv_addr_local.sun_path) + sizeof(serv_addr_local.sun_family);
        result = connect(pSc->fd, (struct sockaddr *)&serv_addr_local, len);
        if (result < 0) {
            LOGE("%u: %s(local connect failed:%s)=%d", getMsecTime(), __FUNCTION__, pSc->localSocketname, result);
            cpdSocketClose(pSc);
            CPD_LOG(CPD_LOG_ID_TXT, "\n%u:%s(%s) = %d, connect failed!", getMsecTime(), __FUNCTION__, pSc->localSocketname, result);
            usleep(1000);
            return result;
        }
    }

    result--;
    pSc->pfReadCallback = pSs->pfReadCallback;
    pSc->pRxBuff = malloc(SOCKET_RX_BUFFER_SIZE);
    ssrl.socketIndex = indexFreeFD;
    CPD_LOG(CPD_LOG_ID_TXT , "\r\n Starting NEW Scoket Read Thread for socket fd:%d", pSc->fd);
    result = pthread_create(&(pSc->clientReadThread), NULL, cpdSocketReadThreadLoop, (void *) &(ssrl));
    if (result == 0) {
        CPD_LOG(CPD_LOG_ID_TXT , "\r\n Read Thread STARTED for socket fd:%d", pSc->fd);
        nTimeout = 10000;
        while (ssrl.socketIndex >= 0) {
            usleep(100);
            nTimeout--;
            if (nTimeout == 0) {
                LOGE("%u: %s(thread startup time expired)=%d", getMsecTime(), __FUNCTION__, result);
                CPD_LOG(CPD_LOG_ID_TXT , "\r\n Thread failed to initialize, closing socket fd:%d, socket state=%d, ssrl.socketIndex=%d, nTimeout=%d",
                        pSc->fd, pSc->state, ssrl.socketIndex, nTimeout);
                result = CPD_ERROR;
                break;
            }
        }
        CPD_LOG(CPD_LOG_ID_TXT , "\r\n Socket read Thread initialization result ssrl.socketIndex = %d, socket state=%d, nTimeout=%d", ssrl.socketIndex, pSc->state, nTimeout);
    }
    else {
        LOGE("%u: %s(thread did not start)=%d", getMsecTime(), __FUNCTION__, result);
        CPD_LOG(CPD_LOG_ID_TXT , "\r\n Thread did not start, closing socket %d", pSc->fd);
        cpdSocketClose(pSc);
    }
    if (result == 0) {
        result = indexFreeFD;
    }
    CPD_LOG(CPD_LOG_ID_TXT, "\n  %u: %s(%s:%d) = %d", getMsecTime(), __FUNCTION__, serverName, serverPort, result);
    LOGD("%u: %s(%s:%d)=%d", getMsecTime(), __FUNCTION__, serverName, serverPort, result);
    return result;
}



/*
 * Close client socket refrenced as parameter.
 */
int cpdSocketClose(pSOCKET_CLIENT pSc)
{
    int nTimeout = 7;
    CPD_LOG(CPD_LOG_ID_TXT, "\n  %u: %s()\n", getMsecTime(), __FUNCTION__);
    LOGV("%u: %s()", getMsecTime(), __FUNCTION__);
    if (pSc == NULL) {
        return CPD_ERROR;
    }
    pSc->state = SOCKET_STATE_TERMINATE;
    usleep(100);
    pSc->pfReadCallback = NULL;
    if (pSc->fd != CPD_ERROR) {
        close(pSc->fd);
        pSc->fd = CPD_ERROR;
    }
    while (pSc->state == SOCKET_STATE_TERMINATING) {
        nTimeout--;
        if (nTimeout == 0) {
            break;
        }
    }
    if (pSc->pRxBuff != NULL) {
        free(pSc->pRxBuff);
        pSc->pRxBuff = NULL;
    }
    pSc->state = SOCKET_STATE_TERMINATED;
    CPD_LOG(CPD_LOG_ID_TXT, "\n  %u: %s()=%d", getMsecTime(), __FUNCTION__, pSc->state);
    return CPD_OK;
}

/*
 * Close one of the sockets clients in Socket Server.
 */
int cpdSocketClientClose(pSOCKET_SERVER pSS, int index)
{
    int result = CPD_ERROR;
    LOGV("%u: %s(%d)", getMsecTime(), __FUNCTION__, index);
    CPD_LOG(CPD_LOG_ID_TXT , "\n%u: %s()\n", getMsecTime(), __FUNCTION__);
    if (pSS->initialized != CPD_OK) {
        return result;
    }
    if ((index < 0) || (index > pSS->maxConnections)) {
        return result;
    }
    return cpdSocketClose(&(pSS->clients[index]));
}


/*
 * Close Socket Server and all it's Client sockets
  */
int cpdSocketServerClose(pSOCKET_SERVER pSS)
{
    int result = CPD_NOK;
    int i;
    LOGV("%u: %s()", getMsecTime(), __FUNCTION__);
    CPD_LOG(CPD_LOG_ID_TXT, "\n%u: %s()\n", getMsecTime(), __FUNCTION__);
    if (pSS->initialized != CPD_OK) {
        return result;
    }

    pSS->state = SOCKET_STATE_TERMINATE;
    if ((pSS->type == SOCKET_SERVER_TYPE_SERVER) || (pSS->type == SOCKET_SERVER_TYPE_SERVER_LOCAL)) {
        if (pSS->sockfd != CPD_ERROR) {
            if (pSS->type == SOCKET_SERVER_TYPE_SERVER_LOCAL) {
                unlink(pSS->localSocketname);
            }
            close(pSS->sockfd);
            pSS->sockfd = CPD_ERROR;
        }
    }
    for (i = 0; i < pSS->maxConnections; i++) {
        cpdSocketClose(&(pSS->clients[i]));
    }
    usleep(100000);
    if ((pSS->state != SOCKET_STATE_TERMINATED) && (pSS->state != SOCKET_STATE_OFF)) {
        if (pSS->socketAcceptThread) {
            pthread_kill(pSS->socketAcceptThread, SIGUSR1);
            usleep(100);
        }
    }
    pSS->initialized = CPD_ERROR;
    pSS->state = SOCKET_STATE_TERMINATED;
    result = CPD_OK;
    CPD_LOG(CPD_LOG_ID_TXT, "\n  %u: %s()=%d", getMsecTime(), __FUNCTION__, result);
    LOGD("%u: %s()=%d", getMsecTime(), __FUNCTION__, result);
    return result;

}

int cpdSocketWriteToAllExcpet(pSOCKET_SERVER pSS, char *pB, int len, int noWrite)
{
    int result = CPD_OK;
    int rr;
    int i;
    if (pSS == NULL) {
        return CPD_ERROR;
    }
    if (pSS->initialized != CPD_OK) {
        return CPD_ERROR;
    }
    if ((len > 0) && (pB != NULL)) {
        for (i = 0; i < pSS->maxConnections; i++) {
            if (i != noWrite) {
                if (pSS->clients[i].fd != CPD_ERROR) {
                    rr = write(pSS->clients[i].fd, pB, len);
                    if (rr != len) {
                        result = CPD_ERROR;
                    }
                }
            }
        }
    }
    return result;
}

int cpdSocketWriteToAll(pSOCKET_SERVER pSS, char *pB, int len)
{
    int result = CPD_OK;
    int rr;
    int i;
    if ((len > 0) && (pB != NULL)) {
        result = cpdSocketWriteToAllExcpet(pSS, pB, len, -1);
    }
    return result;
}


int cpdSocketWrite(pSOCKET_CLIENT pSc , char *pB, int len)
{
    int result = CPD_ERROR;
    if (pSc == NULL) {
        return result;
    }
    if ((pB == NULL) || (len <= 0)) {
        return result;
    }
    if (pSc->fd < 0) {
        return result;
    }
    result = write(pSc->fd, pB, len); /* could also use send() */
/*
    LOGD("%u: %s(%d) = %d", getMsecTime(), __FUNCTION__, len, result);
*/
    return result;
}

int cpdSocketWriteToIndex(pSOCKET_SERVER pSS, char *pB, int len, int index)
{
    int result = CPD_ERROR;
    if (pSS->initialized != CPD_OK) {
        return result;
    }
    if ((index < 0) || (index > pSS->maxConnections)) {
        return result;
    }
    return cpdSocketWrite(&(pSS->clients[index]), pB, len);
}




