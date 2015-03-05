/*
 * hardware/Intel/cp_daemon/cpdSocketServer.h
 *
 * TCP/IP sockets code - header file for cpdSocketServer.c
 *
 * Martin Junkar 09/18/2011
 *
 */

#ifndef _SOCKET_SERVER_H_
#define _SOCKET_SERVER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>  /* used with cleint sockets */
#include <pthread.h>

#define SOCKET_SERVER_MAX_FD        4
#define SOCKET_RX_BUFFER_SIZE       (4096)



//typedef void (modem_mngr_exit_cb)(void *arg);
//typedef void * (*pfSOCKET_READ_CB)(int, char *, int);

typedef  int (fSOCKET_READ_CB)(void *, char *, int, int);

typedef enum {
    SOCKET_STATE_OFF,
    SOCKET_STATE_STARTING,
    SOCKET_STATE_RUNNING,
    SOCKET_STATE_TERMINATE,
    SOCKET_STATE_CANT_RUN,
    SOCKET_STATE_TERMINATING,
    SOCKET_STATE_TERMINATED
} SOCKET_STATE_E;

#define SOCKET_NAME_MAX_LEN (128)
typedef enum {
    SOCKET_SERVER_TYPE_NONE,
    SOCKET_SERVER_TYPE_SERVER,
    SOCKET_SERVER_TYPE_CLIENT,
    SOCKET_SERVER_TYPE_SERVER_LOCAL,
    SOCKET_SERVER_TYPE_CLIENT_LOCAL
} SOCKET_SERVER_TYPE_E;

typedef struct {
    int                 state;
    int                 fd;
    struct sockaddr_in  destAddr;
    char                localSocketname[SOCKET_NAME_MAX_LEN];
    char                *pRxBuff;
    pthread_t           clientReadThread;
    fSOCKET_READ_CB     *pfReadCallback;
} SOCKET_CLIENT, *pSOCKET_CLIENT;

typedef struct {
    int                 initialized;
    SOCKET_SERVER_TYPE_E    type;
    int                 portNo;
    struct sockaddr_in  serverAddr;
    char                localSocketname[SOCKET_NAME_MAX_LEN];

    int                 sockfd;
    pthread_t           socketAcceptThread;
    int                 state;
    int                 maxConnections;
    SOCKET_CLIENT       clients[SOCKET_SERVER_MAX_FD];
    fSOCKET_READ_CB     *pfReadCallback; /* all sockets share the same read calback function */
} SOCKET_SERVER, *pSOCKET_SERVER;


typedef struct {
    int             socketIndex;
    pSOCKET_SERVER  pSS;
} SOCKET_SERVER_READ_THREAD_LINK, *pSOCKET_SERVER_READ_THREAD_LINK;

int cpdSocketServerInit(pSOCKET_SERVER );
int cpdSocketServerOpen(pSOCKET_SERVER );
int cpdSocketServerClose(pSOCKET_SERVER );

int cpdSocketClientOpen(pSOCKET_SERVER, char *, int);
int cpdSocketClientClose(pSOCKET_SERVER, int);
int cpdSocketClose(pSOCKET_CLIENT);

int cpdSocketWriteToAll(pSOCKET_SERVER , char *, int );
int cpdSocketWriteToAllExcpet(pSOCKET_SERVER , char *, int , int );
int cpdSocketWrite(pSOCKET_CLIENT , char *, int);
int cpdSocketWriteToIndex(pSOCKET_SERVER , char *, int , int );



#endif

