/*
 * hardware/Intel/cp_daemon/cpdSystemMonitor.h
 *
 * System Monitor for CPDD - header file for cpdSystemMonitor.c
 *
 * Martin Junkar 09/18/2011
 *
 */

#ifndef _CPDSYSTEMMONITOR_H_
#define _CPDSYSTEMMONITOR_H_
int cpdSystemMonitorStart(void);
int cpdSystemMonitorStop(pCPD_CONTEXT);
int cpdStaremActiveMonitorStart( void );

#endif


