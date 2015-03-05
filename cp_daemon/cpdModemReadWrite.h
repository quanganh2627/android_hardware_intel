/*
 *  hardware/Intel/cp_daemon/cpdModemReadWrite.h
 *
 * Modem communication handler - header file.
 *
 * Martin Junkar 09/18/2011
 *
 *
 */

#ifndef _CPDMODEM_RW_H_
#define _CPDMODEM_RW_H_
int cpdModemSendCommand(pCPD_CONTEXT , const char *, int , unsigned int );
int cpdModemSocketWriteToAllExcpet(pSOCKET_SERVER , char *, int , int );
void *cpdModemReadThreadLoop(void *);
int cpdModemOpen(pCPD_CONTEXT );
int cpdModemInitForCP(pCPD_CONTEXT );
int cpdModemClose(pCPD_CONTEXT );
#endif  /* _CPDMODEM_RW_H_ */

