#ifndef SOCKETCLIENT_INTERFACE_H
#define SOCKETCLIENT_INTERFACE_H

#include "../include/socketinterface.h"


typedef struct struct_command
{
	char cmd[200];
	int serialno;
	char responsedata[200];
}Command;

int init_clientsocket();
int close_clientsocket();
int SendCommandAndReceive(const char* command, char* response ,int responsebufsize);
int ProcessResponse(char* cmdrequest, char* response);

//Get the connection status
int IsClientConnected();

//Reconnect the Server 
int reConnect();



#endif
