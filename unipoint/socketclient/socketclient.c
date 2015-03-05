#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <../include/socketinterface.h>


#define RECEIVE_MAX_BUFFER 1024


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


static int clientsocket;
static int connected = 0; 




//This function is used to initialization the socket 
static int init_clientsocket()
{
		int t, len;
	   struct sockaddr_un remote;
	 
	
	   int i = 0;
	   if ((clientsocket = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		   perror("socket");
		  return -1;
	   }
	
	   printf("Trying to connect...\n");
	
	   if (makeAddr("unipointserver", &remote, &len) < 0)
	   {
		printf("MakeAddr for unitpoint server failed\n");
	     return -1;

	   }
	 
	   if (connect(clientsocket, (struct sockaddr *)&remote, len) == -1) {
		   perror("connect to server failed ");
		   return -1;
	   }


	   //the connection is established 
	   connected = 1;

	   
	   printf("Connected.\n");

	   return 0;

}



//This function is used close client socket 
static int close_clientsocket()
{
	if(clientsocket!=-1){
		close(clientsocket);
		clientsocket = -1;

	}
	connected = 0;


	return 0;

}



//This function is used to sendcommand to server and pass back returned response .
// Params:
// command:  command data to send 
// response :   preallcoated memory buffer which will be filled with cmd response.
// return 0 success, return -1 fail 
static int SendCommandAndReceive(const char* command, char* response ,int responsebufsize)
{
	int sendlen = 0;
	int recvlen = 0;
	char buf[RECEIVE_MAX_BUFFER];
	int temp = 0;
	if(-1==clientsocket)
	{
		printf("The clientsocket is not valid \n");
		return -1;
	}

	
	
	sendlen = send(clientsocket, command, strlen(command), 0);
	
	if ( sendlen == -1) {
	      perror("send failed ");
	      return -1;
	}

	printf("send string %s success,total bytes send %d\n",command,sendlen);


	//flush();
		
	recvlen = recv(clientsocket, buf, RECEIVE_MAX_BUFFER, 0);

	if(recvlen>0)
	{

			printf("Successfully received response \n");
	        buf[recvlen] = '\0';
	        printf("Got returned response : %s \n", buf);


			
			temp = recvlen;

			if(responsebufsize<temp)
			{
				printf("ERROR :Too small response buffer size \n");
				return -1;
			}else
			{
				strncpy(response,buf,recvlen);

			}


	} else {

	  	if (recvlen < 0) 
		{
			perror("recv");

		}else{
			printf("Server closed connection\n");

			connected = 0;
	        return -1;
	    }
	}


	return 0;
}


static int ProcessResponse(char* cmdrequest, char* response)
{
	int mode = -1;

	//int returnedmode = atoi(response);

	if(strncmp(response,RET_SUCCESS,strlen(RET_SUCCESS)) ==0 )
	{
		if(strncmp(cmdrequest,CMD_GETMODE,strlen(CMD_GETMODE)) ==0 )
		{

			//This is response for GETMODE 

			mode = atoi(response+strlen(RET_SUCCESS));

			printf("Got Response, current mode is %d \n",mode);

			
		}if(strncmp(cmdrequest,CMD_SETMODE,strlen(CMD_SETMODE)) ==0 )
		{


			printf("Got Response, Set Mode Successfully\n");

			
		}
		
	}else if(strncmp(response,RET_FAILURE,strlen(RET_SUCCESS)) ==0)
	{

		printf("Got RET FAILURE FROM SERVER\n");
		

	}else {

		printf("Unrecognized response  %s , return\n",response );
		
	}

	return 0;

}



int 
main(int argc, char *argv[])
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

    for(i=0;i<loopcount;i++) {

		char response[1024] = {0};
		//Test GET MODE 
		ret = SendCommandAndReceive(CMD_GETMODE,response,sizeof(response));
		if(-1!=ret)
		{
			ProcessResponse(CMD_GETMODE,response);

		}else
		{
			printf("SendCommandAndReceive for command %s failed \n",CMD_GETMODE);
		}


		//TEST SET MODE 
		memset(cmdbuf,0,sizeof(cmdbuf));
		sprintf(cmdbuf,"%s%d",CMD_SETMODE,mode);
		ret = SendCommandAndReceive(cmdbuf,response,sizeof(response));
		if(-1!=ret)
		{

			ProcessResponse(CMD_SETMODE,response);
			
		}else
		{
			printf("SendCommandAndReceive for command %s failed \n",CMD_SETMODE);
		}

		
		//SendCommandAndReceive(CMD_POLL,response,sizeof(response));
	}

  	close_clientsocket();
	
    return 0;
}



