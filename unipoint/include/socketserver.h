#ifndef SOCKETSERVER_H
#define SOCKETSERVER_H

void* socketthread(void* cnt);
int CallBack_NotifyClientOnStateChange();

void closeclientsocket();


#endif
