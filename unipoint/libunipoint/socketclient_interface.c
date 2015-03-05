#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <../include/socketinterface.h>
#include <utils/Log.h>
#include <pthread.h>
#include "jnicallback_interface.h"
#include "socketclient_interface.h"
#include <errno.h>


static int loop = 1;
#define RECEIVE_MAX_BUFFER 1024
#define SM_MAX_CMDS 30
static int gSerialno = 0;
static Command gCmdqueue[SM_MAX_CMDS] = {0};
#define INITIAL_RET_VALUE -99
#define MAX_SERIAL_NO 200000
static int connectedsocket;
static   pthread_t clientsocketthread;


static int gCmdQuqueIndex;
//Callback from JNI layer
static time_t   timep_client;
static char* currentcmd = NULL;

pthread_mutex_t ret_mutex;
pthread_cond_t ret_found_cond;



static int retfromDaemon = INITIAL_RET_VALUE; //indicate the response

int connected = 0;


void* socketReceivethread(void* cnt);

//Get the connection status
int IsClientConnected()
{
    return connected;

}

//Reconnect the Server
int reConnect()
{

    LOGW("Start to close the clientsocket");

    close_clientsocket();


    LOGW("Start to open new clientsocket");

    if(!connected) {
        return init_clientsocket();

    } else {
        return 0;
    }

}


int makeAddr(const char* name, struct sockaddr_un* pAddr, socklen_t* pSockLen)
{
    int nameLen = strlen(name);
    if (nameLen >= (int) sizeof(pAddr->sun_path) -1)  /* too long? */
        return -1;
    pAddr->sun_path[0] = '\0';  /* abstract namespace */
    strcpy(pAddr->sun_path+1, name);
    pAddr->sun_family = AF_LOCAL;
    *pSockLen = 1 + nameLen + offsetof(struct sockaddr_un, sun_path);
    return 0;
}



// insert raw input to history for current touch engagement
void cmdqueuePush(const Command cmd)
{

    if(gCmdQuqueIndex == SM_MAX_CMDS) {
        gCmdQuqueIndex = 0;

    }
    gCmdqueue[gCmdQuqueIndex] = cmd; // does this copy
    gCmdQuqueIndex++;

}




//This function is used to initialization the socket
int init_clientsocket()
{
    int t, len;
    struct sockaddr_un remote;

    int ret = -1;
    int i = 0;
    pthread_attr_t attr;
    int retrycounts = 2000;



    ret = pthread_mutex_init(&ret_mutex, NULL);
    if(ret!=0)
    {
        perror("libUnipoint:pthread_mutex_init() error");
        LOGW("libUnipoint:pthread_mutex_init() error");

    } else
    {
        LOGW("libUnipoint:pthread_mutex_init() SUCCESS");

    }
    ret = pthread_cond_init (&ret_found_cond, NULL);
    if(ret!=0) {

        perror("libUnipoint: pthread_cond_init() error");
        LOGW("libUnipoint: pthread_cond_init() error");
    } else {
        LOGW("libUnipoint:pthread_cond_init() SUCCESS");
    }
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);


    //Create Receive Thread
    if(pthread_create(&clientsocketthread, &attr, (void *)socketReceivethread, NULL) == 0) {
        LOGV("libUnipoint: socketReceivethread thread create OK!\n");
    }


    return 0;

}





void* socketReceivethread(void* cnt)
{
    int recvlen ;
    char buf[RECEIVE_MAX_BUFFER];
    int newmode;
    int retrycounts = 2000;
    int t, len;
    struct sockaddr_un remote;
    if ((connectedsocket = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return (void*)-1;
    }

    LOGW("libUnipoint: Trying to connect...\n");
    if (makeAddr("unipointserver", &remote, &len) < 0)
    {
        LOGW("libUnipoint: MakeAddr for unitpoint server failed\n");
        return (void*)-1;

    }

    while(!connected && retrycounts >=0) {

        LOGW("libUnipoint: Try to Conntect To Server , Current retrycount = %d", retrycounts);
        if (connect(connectedsocket, (struct sockaddr *)&remote, len) == -1) {
            time(&timep_client);
            LOGW("libUnipoint: connect to server failed ,go to sleep for 3 seconds,curent time %s \n",ctime(&timep_client));

            sleep(3);
            retrycounts --;
            continue;
        }
        LOGW("libUnipoint: Unipointdaemon Connected.\n");

        connected = 1;
    }


    if(connected !=1) {
        LOGW("LibUnipoint: Timeout , Daemon Not Connected, fail and return \n");
        return (void*)-1;
    }


    while(loop) {

        //Add Mechanism to wack up from recv system call ?
        recvlen = recv(connectedsocket, buf, RECEIVE_MAX_BUFFER, 0);

        if(recvlen>0)
        {

            LOGW("Successfully received response \n");
            buf[recvlen] = '\0';
            LOGW("Got returned response : %s \n", buf);


            if(strncmp(buf,RET_CMD_NOTIFYCHANGE,strlen(RET_CMD_NOTIFYCHANGE)) ==0 )
            {

                //The command is NOTIFY CHANGE
                newmode = atoi(buf+strlen(RET_CMD_NOTIFYCHANGE));

                LOGW("Got Response, current mode is %d \n",newmode);


                callbackModeSwitchRequest(newmode);


            } else {

                pthread_mutex_lock(&ret_mutex);
                ProcessResponse( currentcmd, buf);
                pthread_cond_signal(&ret_found_cond);
                pthread_mutex_unlock(&ret_mutex);
            }

        } else {

            if (recvlen < 0)
            {
                LOGW("recv error ");

            } else {
                LOGW("Server closed connection\n");

                close_clientsocket();
                connected = 0;
                return (void*)-1;
            }

            pthread_mutex_lock(&ret_mutex);


            //	ProcessResponse( currentcmd, buf);

            LOGW("recvlen <= 0 socketReceivethread: pthread_cond_signal");

            pthread_cond_signal(&ret_found_cond);


            LOGW("recvlen <= 0 socketReceivethread: pthread_mutex_unlock");
            pthread_mutex_unlock(&ret_mutex);

            LOGW("recvlen <= 0 socketReceivethread: DONE");


        }
    }

    return (void*)0;

}


//This function is used close client socket
int close_clientsocket()
{
    void* result;
    int ret = 0;

    //Question , how to exit recv system call ?
    loop  = 0;

    //pthread_cancel(clientsocketthread);

    LOGW(" shutdown the clientsocket ");
    if(connectedsocket!=-1) {
        //ret = close(connectedsocket);
        ret = shutdown(connectedsocket,2);
        if(ret == 0)
        {
            LOGW(" shutdown the clientsocket SUCCESS ");

        } else
        {
            LOGW(" shutdown the clientsocket FAIL ");

        }
        connectedsocket = -1;

    }

    LOGW("clientsocket closed , waiting for client thread to join ");



    connected = 0;

    //destroy mutex and conditional variable
    pthread_mutex_destroy(&ret_mutex);
    pthread_cond_destroy(&ret_found_cond);


    return 0;

}



//This function is used to sendcommand to server and pass back returned response .
// Params:
// command:  command data to send
// response :   preallcoated memory buffer which will be filled with cmd response.
// return 0 success, return -1 fail
int SendCommandAndReceive(const char* command, char* response ,int responsebufsize)
{
    int sendlen = 0;
    int recvlen = 0;
    char buf[RECEIVE_MAX_BUFFER];
    int temp = 0;
    time_t T;
    struct timespec t;

    Command thiscmd = {0};



    if(-1==connectedsocket)
    {
        LOGW("The clientsocket is not valid \n");

        return -1;
    }

    //strncpy(thiscmd.cmd,command,strlen(command));
    //gSerialno++;
    //thiscmd.serialno = gSerialno;

    //cmdqueuePush(thiscmd);

    currentcmd = command;

    LOGW("SendCommandAndReceive, Set currentcmd = %s",command);


    //!!!!!NOTE: HAVE TO PUT THE MUTEX_LOCK BEFORE SEND ,OR ELSE ERROR HAPPEN (wait will never got signal)
    retfromDaemon = INITIAL_RET_VALUE;
    //wait for result return
    pthread_mutex_lock(&ret_mutex);


    //Start to send command
    sendlen = send(connectedsocket, command, strlen(command), 0);

    if ( sendlen == -1) {
        LOGW("send failed ");
        return -1;
    }

    LOGW("send string %s success,total bytes send %d\n",command,sendlen);



    if (retfromDaemon == INITIAL_RET_VALUE) {

        LOGW("Start to waiting for results from daemon, pthread_cond_wait");

        pthread_cond_wait(&ret_found_cond, &ret_mutex);


        //When we are here , there should be return value for this command .


        LOGW("Got response value %d",retfromDaemon);




    } else
    {
        LOGW("retfromDaemon already retruned , value is %d",retfromDaemon);
    }


    pthread_mutex_unlock(&ret_mutex);
//	pthread_exit(NULL);



    return retfromDaemon;
}




//if in GETMODE command, return -1 for failure, return mode when success
//if in SETMODE command, return -1 for failure, return 0 for mode set success
int ProcessResponse(char* cmdrequest, char* response)
{
    int ret = -1;

    //int returnedmode = atoi(response);
    if(NULL == cmdrequest)
    {
        LOGW("Got Response,currentcmd is NULL");
        return ret;
    }

    LOGW("--> ProcessResponse");

    if(strncmp(response,RET_SUCCESS,strlen(RET_SUCCESS)) ==0 )
    {
        if(strncmp(cmdrequest,CMD_GETMODE,strlen(CMD_GETMODE)) ==0 )
        {

            //This is response for GETMODE

            ret = atoi(response+strlen(RET_SUCCESS));

            LOGW("ProcessResponse: Got Response, current mode is %d \n",ret);

            goto end;


        }
        if(strncmp(cmdrequest,CMD_SETMODE,strlen(CMD_SETMODE)) ==0 )
        {


            LOGW("ProcessResponse: Got Response, Set Mode Successfully\n");
            ret = 0;
            goto end;

        }

    } else if(strncmp(response,RET_FAILURE,strlen(RET_FAILURE)) ==0)
    {

        LOGW("ProcessResponse: Got RET FAILURE FROM SERVER\n");

        ret = -1;
        goto end;


    } else {

        LOGW("ProcessResponse: Unrecognized response  %s , return\n",response );

        ret = -1;
        goto end;

    }

end:

    LOGW("<-- ProcessResponse, return value is %d", ret);
    currentcmd = NULL;

    retfromDaemon = ret;

    return ret;

}



int
test(int argc, char *argv[])
{
    int i = 0;
    int ret = 0;
    int loopcount = 2;
    char cmdbuf[1024] = {0};
    int mode = 1; //mode to switch
    if(init_clientsocket()!=0)
    {
        exit(1);
    }

    for(i=0; i<loopcount; i++) {

        char response[1024] = {0};
        //Test GET MODE
        ret = SendCommandAndReceive(CMD_GETMODE,response,sizeof(response));
        if(-1!=ret)
        {
            ProcessResponse(CMD_GETMODE,response);

        } else
        {
            LOGW("SendCommandAndReceive for command %s failed \n",CMD_GETMODE);
        }


        //TEST SET MODE
        memset(cmdbuf,0,sizeof(cmdbuf));
        sprintf(cmdbuf,"%s%d",CMD_SETMODE,mode);

        LOGW("SendCommandAndReceive command is %s  \n",cmdbuf);


        ret = SendCommandAndReceive(cmdbuf,response,sizeof(response));
        if(-1!=ret)
        {

            ProcessResponse(CMD_SETMODE,response);

        } else
        {
            LOGW("SendCommandAndReceive for command %s failed \n",CMD_SETMODE);
        }


        //SendCommandAndReceive(CMD_POLL,response,sizeof(response));
    }

    close_clientsocket();

    return 0;
}



