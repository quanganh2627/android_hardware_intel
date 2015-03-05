/*
 * hardware/Intel/cp_daemon/cpdModem.h
 *
 * Header file for cpdModem.c
 *
 * Martin Junkar 09/18/2011
 *
 *
 */

int modemClose(int *fd);
int modemOpen(char * name, int openOptions);
int modemWrite(int fd, void *pBuffer, int len);
int modemRead(int fd, void *pBuffer, int len);

