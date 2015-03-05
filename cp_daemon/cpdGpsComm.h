/*
 *  hardware/Intel/cp_daemon/cpdGpsComm.h
 *
 * Header file forConnector between CPDD and GPS library.
 *
 * Martin Junkar 09/18/2011
 *
 *
 */

#ifndef _CPDGPSCOMM_H_
#define _CPDGPSCOMM_H_

#define CPD_MSG_HEADER_QUERRY       "\r\n?????\r\n"
#define CPD_MSG_HEADER_TO_GPS       "\r\nCPTG\r\n"
#define CPD_MSG_HEADER_FROM_GPS     "\r\nGTCP\r\n"
#define CPD_MSG_TAIL                "\r\nEOM\r\n"

typedef enum {
    CPD_MSG_TYPE_NONE,
    CPD_MSG_TYPE_QUERRY,
    CPD_MSG_TYPE_MEAS_ABORT_REQ,
    CPD_MSG_TYPE_POS_MEAS_REQ,
    CPD_MSG_TYPE_POS_MEAS_RESP
} CPD_MSG_TYPE_E;


int cpdGpsCommMsgReader(void * , char *, int , int );
int cpdFormatAndSendMsg_MeasAbort(pCPD_CONTEXT );
int cpdIsNumberOfResponsesSufficientForRequest(pCPD_CONTEXT );
int cpdCalcRequredTimneToServiceRequest(pCPD_CONTEXT );
int isCpdSessionActive(pCPD_CONTEXT );
int cpdSendAbortToGps(pCPD_CONTEXT );
int cpdFormatAndSendMsgToGps(pCPD_CONTEXT );
int cpdFormatAndSendMsgToCpd(pCPD_CONTEXT );
int cpdSendStopToGPS(pCPD_CONTEXT );


#endif


