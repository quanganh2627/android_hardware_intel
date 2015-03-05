/*
 * hardware/Intel/cp_daemon/cpdStart.h
 *
 * Startup manager for CPDD - header file
 *
 * Martin Junkar 09/18/2011
 *
 */


#ifndef _CPDSTART_H_
#define _CPDSTART_H_

/*
 * initialize & setup CPD environment, then open conenctions and start threads.
 * At the end start system monitor process.
 * If return value is CPD_OK, everything is ready and running.
 */
int cpdStart(pCPD_CONTEXT );
/*
 * Start system monitor process.
 * Close connections and de-initialize.
 * After this function returns it is safe to terminate.
 */
int cpdStop(pCPD_CONTEXT );


#endif

