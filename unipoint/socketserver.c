#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <pthread.h>
#include <include/socketinterface.h>
#include <include/statemachine.h>
#include <unistd.h>

#ifndef UNIPOINT_DEBUG
#define UNIPOINT_DEBUG 1
#endif

#define DBUG_OUT(...) \
	if (UNIPOINT_DEBUG) \
		fprintf(stderr, __VA_ARGS__)



extern volatile sig_atomic_t do_event_loop ;

int statechanged = 0;
static int clientSock = -1;

//CHeck whether is is valid mode or not .
static int isValidMode(int mode )
{
    if(mode!=MODE_VOLUME && mode !=MODE_NORMAL && mode !=MODE_NONE)
    {

        return 0;
    } else
    {

        return 1;
    }

}

int makeAddr(const char* name, struct sockaddr_un* pAddr, socklen_t* pSockLen)
{
    unlink(name);
    int nameLen = strlen(name);
    if (nameLen >= (int) sizeof(pAddr->sun_path) -1)  /* too long? */
        return -1;
    pAddr->sun_path[0] = '\0';  /* abstract namespace */
    strcpy(pAddr->sun_path+1, name);
    pAddr->sun_family = AF_LOCAL;
    *pSockLen = 1 + nameLen + offsetof(struct sockaddr_un, sun_path);
    return 0;
}


void closeclientsocket()
{
    if( clientSock != -1) {

        DBUG_OUT("in function closeclientsocket ,clientSock is closed \n");
        close(clientSock);
        clientSock = -1;

    }

}


//Init function is used to set up SocketServer for internal IPC
void* socketthread(void* cnt)
{
    int  s2, t, len, val=1;
    struct sockaddr_un local, remote;
    char str[100] = {0};
    char sendstr[100] = {0};
    struct sockaddr_un sockAddr;
    socklen_t sockLen;
    int fd = 0;

    if (makeAddr("unipointserver", &sockAddr, &sockLen) < 0)
        return((void*)1);


    fd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (fd < 0) {
        DBUG_OUT("Daemon_Server:  socket() failure");
        return((void*)1);
    }

    DBUG_OUT("SERVER %s\n", sockAddr.sun_path+1);
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    if (bind(fd, (const struct sockaddr*) &sockAddr, sockLen) < 0) {
        DBUG_OUT("Daemon_Server: server bind() failure");
        goto bail;
    }


    if (listen(fd, 5) == -1) {
        DBUG_OUT("Daemon_Server: listen failed ~~~~~~~~");
        goto bail;
    }
    DBUG_OUT("Daemon_Server: Listening success...\n");



    while(do_event_loop) {


        int done, nreceived;
        int nsend;
        DBUG_OUT("Daemon_Server: Waiting for a connection...\n");
        //  t = sizeof(remote);

        clientSock = accept(fd, NULL, NULL);
        if (clientSock < 0) {
            DBUG_OUT("Daemon_Server: server accept failure");
            goto bail;
        }

        DBUG_OUT("Daemon_Server: Connected.\n");

        done = 0;


        do {


            nreceived = recv(clientSock, str, 100, 0);
            if (nreceived <= 0) {
                if (nreceived < 0)
                {
                    DBUG_OUT("Daemon_Server: recv");
                }
                DBUG_OUT("Daemon_Server:Detected that  Client is disconnected from Daemon Server \n");

                break; // skip this transaction
            }

            DBUG_OUT("Daemon_Server: Received str %s from clientSocket\n",str);

            if(strncmp(str,CMD_GETMODE,strlen(CMD_GETMODE)) ==0 )
            {
                memset(sendstr,0,sizeof(sendstr));
                strncpy(sendstr,RET_SUCCESS,strlen(RET_SUCCESS));

                char retmode[100]= {0};
                DBUG_OUT("Daemon_Server: Got CMD_GETMODE command ,processing \n");

                //getcurrentmode from statemachine
                MODE currentmode = GetCurrentMode();
                snprintf(retmode,sizeof(retmode),"%d",currentmode);

                strncat(sendstr,retmode,strlen(retmode));

                nsend = strlen(sendstr);
                if (send(clientSock, sendstr, nsend, 0) < 0) {
                    DBUG_OUT("Daemon_Server: send failed ");
                    break;

                }

            } else if(strncmp(str,CMD_SETMODE,strlen(CMD_SETMODE)) ==0)
            {

                int value = atoi(str+strlen(CMD_SETMODE));

                if(!isValidMode(value))
                {
                    DBUG_OUT("Daemon_Server: Invalid MODE to set ");
                    strncpy(sendstr,RET_FAILURE,strlen(RET_FAILURE));
                    nsend = strlen(sendstr);
                    if (send(clientSock, sendstr, nsend, 0) < 0) {
                        DBUG_OUT("Daemon_Server: send failed ");
                        break;

                    }
                } else {
                    SwtichToNewMode((MODE)value);

                    strncpy(sendstr,RET_SUCCESS,strlen(RET_SUCCESS));
                    nsend = strlen(sendstr);
                    if (send(clientSock, sendstr, nsend, 0) < 0) {
                        DBUG_OUT("Daemon_Server: send failed ");
                        break;

                    }

                }



            } else {



                DBUG_OUT("Daemon_Server: Unrecognized CMMAND %s , return\n",str );

            }



            DBUG_OUT("Daemon_Server: Finished process one command %s\n",str);

        } while (1);




        //Close client socket if we break out from the loop
        closeclientsocket();

    }


bail:
    close(fd);
    pthread_exit((void*)0);




    return((void*)0);
}



//We should protect the clientSock varible and preventing sending data at the same time, this may cause issue.  Add it later .

//This function is called to notify Client about Server Status Change .
//This function is called from Daemon's Event handler thread .
int CallBack_NotifyClientOnStateChange()
{

    int  s2, t, len;
    int nsend ;

    char str[100] = {0};
    char sendstr[100] = {0};

    if(clientSock == -1)
    {
        DBUG_OUT("Daemon_Server: clientSock Invalid , exit NotifyClientOnStateChange \n ");
        return -1;
    }


    if(statechanged)
    {

        memset(sendstr,0,sizeof(sendstr));
        strncpy(sendstr,RET_CMD_NOTIFYCHANGE,strlen(RET_CMD_NOTIFYCHANGE));

        char retmode[100]= {0};
        DBUG_OUT("Daemon_Server: SENDING RET_CMD_NOTIFYCHANGE command  \n");

        //getcurrentmode from statemachine
        MODE currentmode = GetCurrentMode();
        snprintf(retmode,sizeof(retmode),"%d",currentmode);

        strncat(sendstr,retmode,strlen(retmode));

        nsend = strlen(sendstr);
        if (send(clientSock, sendstr, nsend, 0) < 0) {
            DBUG_OUT("Daemon_Server: send failed ");
            return -1;

        }

        //RET statechanged back to 0
        statechanged = 0;

    }


    return 0;


}
