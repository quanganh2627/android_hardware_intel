/*
 *  hardware/Intel/cp_daemon/cpdInit.c
 *
 * Initialize variables for CPDD.
 * CPDD uses one global variable - CPD_CONTEXT structure.
 * Memory for buffers is allocated at the time of initialization and released when CPDD exits.
 * The exception are messages sent over TCP/IP socket connection to GPS, where output buffer is allocated dynamically and message is formated.
 * Where needed, code shall call cpdGetContext() to retreive pointer to CPDD context structure.
 * In most cases pointer to CPD_CONTEXT is passed as the first argument in function calls, it is used like "this" in OO programming environments.
 *
 * Martin Junkar 09/18/2011
 *
 *
 */

#include <stdlib.h>
#include <stdio.h>
#define LOG_TAG "CPDD_IN"

#include "cpd.h"
#include "cpdUtil.h"

void cpdDeInit(void);

CPD_CONTEXT cpdContext;

pCPD_CONTEXT cpdInit(void)
{
    int result = CPD_OK;
/*
    LOGD("%u: %s()", getMsecTime(), __FUNCTION__);
*/
    cpdContext.initialized = CPD_ERROR;
    memset(&cpdContext, 0, sizeof(CPD_CONTEXT));

    cpdContext.modemInfo.modemReadThreadState = THREAD_STATE_OFF;
    snprintf((char*) (cpdContext.modemInfo.modemName), MODEM_NAME_MAX_LEN, "%s" , MODEM_NAME);
    cpdContext.modemInfo.pModemRxBuffer = NULL;
    cpdContext.modemInfo.pModemTxBuffer = NULL;
    cpdContext.modemInfo.modemFd = 0;
    pthread_mutex_init(&(cpdContext.modemInfo.modemFdLock), NULL);


    cpdContext.xmlRxBuffer.pXmlBuffer = NULL;

    cpdContext.xmlRxBuffer.pXmlBuffer = malloc(XML_RX_BUFFER_SIZE);
    if (cpdContext.xmlRxBuffer.pXmlBuffer != NULL) {
        cpdContext.xmlRxBuffer.xmlBufferSize = XML_RX_BUFFER_SIZE;
        memset(cpdContext.xmlRxBuffer.pXmlBuffer, 0, cpdContext.xmlRxBuffer.xmlBufferSize);
        cpdContext.xmlRxBuffer.maxAge = XML_MAX_DATA_AGE_CPOS;
    }
    else {
        cpdDeInit();
        result = CPD_NOK;
    }


    cpdContext.gpsCommBuffer.pRxBuffer = NULL;
    cpdContext.gpsCommBuffer.rxBufferIndex = CPD_ERROR;
    cpdContext.gpsCommBuffer.rxBufferMessageType = CPD_ERROR;
    cpdContext.gpsCommBuffer.rxBufferSize = CPD_ERROR;
    cpdContext.gpsCommBuffer.scGpsIndex = CPD_ERROR;
    cpdContext.gpsCommBuffer.pRxBuffer = malloc(GPS_COMM_RX_BUFFER_SIZE);
    if (cpdContext.gpsCommBuffer.pRxBuffer != NULL) {
        cpdContext.gpsCommBuffer.rxBufferIndex = 0;
        cpdContext.gpsCommBuffer.rxBufferSize = GPS_COMM_RX_BUFFER_SIZE;
        memset(cpdContext.gpsCommBuffer.pRxBuffer, 0, cpdContext.gpsCommBuffer.rxBufferSize);
    }
    cpdContext.gpsCommBuffer.rxBufferCmdStart = CPD_ERROR;
    cpdContext.gpsCommBuffer.rxBufferCmdEnd = CPD_ERROR;

    cpdContext.scIndexToGps = CPD_ERROR;

    cpdContext.systemMonitor.pmfd = -1;

    if (result == CPD_OK) {
        cpdContext.initialized = result;
        return &cpdContext;
    }
    LOGE("%u: %s() = NULL", getMsecTime(), __FUNCTION__);
    return NULL;
}


void cpdDeInit(void)
{
    LOGD("%u: %s()", getMsecTime(), __FUNCTION__);
    if (cpdContext.initialized != CPD_OK) {
        return;
    }
    if (cpdContext.modemInfo.pModemRxBuffer != NULL) {
        free(cpdContext.modemInfo.pModemRxBuffer);
    }
    if (cpdContext.modemInfo.pModemTxBuffer != NULL) {
        free(cpdContext.modemInfo.pModemTxBuffer);
    }
    if (cpdContext.xmlRxBuffer.pXmlBuffer != NULL) {
        free(cpdContext.xmlRxBuffer.pXmlBuffer);
    }
    return;
}

pCPD_CONTEXT cpdGetContext(void)
{
    if (cpdContext.initialized == CPD_OK) {
        return &cpdContext;
    }
    return NULL;
}


