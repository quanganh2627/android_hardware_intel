/*
 *  hardware/Intel/cp_daemon/cpdModemReadWrite.c
 *
 * Modem communication handler.
 * Opens modem comm channel (gsmtty7), parses modem responses, sends AT comands, handles communication errors.
 * Implements "pass-through" for modem via sockets interface.
 * "Pass-through" interface is used for debugging & diagnostics also, modem messages are broadcast to all open socket connections.
 *
 * Martin Junkar 09/18/2011
 *
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <termios.h>
#include <sys/poll.h>

#define LOG_TAG "CPDD_MRW"
#define LOG_NDEBUG 1    /* control debug logging */

#include "cpd.h"
#include "cpdModem.h"
#include "cpdUtil.h"
#include "cpdXmlParser.h"
#include "cpdInit.h"

/* for debug logging */
#include "cpdDebug.h"
#include "cpdSocketServer.h"


#define TEMP_RX_BUFF_SIZE   256

#define AT_RESPONSE_TIMEOUT 300UL

const char *pCmdAt = (AT_CMD_AT AT_CMD_CRLF);
const char *pCmdAtA = (AT_CMD_AT "A" AT_CMD_CRLF);
const char *pCmdAtH = (AT_CMD_AT "H" AT_CMD_CRLF);

const char *pCmdXgendata = (AT_CMD_AT AT_CMD_XGENDATA AT_CMD_CRLF);

const char *pCmdCposr1 = (AT_CMD_AT AT_CMD_CPOSR "=1" AT_CMD_CRLF);
const char *pCmdCposrQ = (AT_CMD_AT AT_CMD_CPOSR "?" AT_CMD_CRLF);

const char *pCmdCreg2 = (AT_CMD_AT "+CREG=2" AT_CMD_CRLF);
const char *pCmdCregQ = (AT_CMD_AT "+CREG?" AT_CMD_CRLF);

const char *pCmdCopsQ = (AT_CMD_AT "+COPS?" AT_CMD_CRLF);
const char *pCmdCops0 = (AT_CMD_AT "+COPS=0" AT_CMD_CRLF);
const char *pCmdCops2 = (AT_CMD_AT "+COPS=2" AT_CMD_CRLF);

const char *pCmdXratQ = (AT_CMD_AT "+XRAT?" AT_CMD_CRLF);
const char *pCmdXrat2G = (AT_CMD_AT "+XRAT=0" AT_CMD_CRLF);
const char *pCmdXrat3G = (AT_CMD_AT "+XRAT=1,2" AT_CMD_CRLF);

const char *pCmdCsqQ = (AT_CMD_AT "+CSQ" AT_CMD_CRLF);

const char *pCmdMuxDebugOn = (AT_CMD_AT "+xmux=1,3,4095" AT_CMD_CRLF);

static void cpdModemReadThreadSignalHandler(int );
static int cpdCheckIfReceivedWaitForString(pCPD_CONTEXT  );


static void cpdModemReadThreadSignalHandler(int sig)
{
    CPD_LOG(CPD_LOG_ID_TXT, "Signal(%s)=%d\n", __FUNCTION__, sig);
    LOGW("Signal(%s())=%d\n", __FUNCTION__, sig);
    switch (sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGQUIT:
    case SIGUSR1:
        pthread_exit(0);
    }
}



/*
 * Send AT Commands to modem.
 * Return value is number of bytes sent.
 */
int cpdModemSendCommand(pCPD_CONTEXT pCpd, const char *pB, int len, unsigned int waitForResponse)
{
    int result = 0;
    unsigned int t0;

    if ((pB == NULL) || (len <= 0)) {
        return result;
    }
    pCpd->modemInfo.haveResponse = 0;
    pCpd->modemInfo.responseValue = AT_RESPONSE_NONE;
    if (waitForResponse > 0) {
        pCpd->modemInfo.waitingForResponse = 1;
    }

    result = modemWrite(pCpd->modemInfo.modemFd, (void *) pB, len);
    if(result < 0) {
        if(pCpd->pfSystemMonitorStart != NULL) {
            CPD_LOG(CPD_LOG_ID_TXT, "\nStarting SystemMonitor!");
            pCpd->pfSystemMonitorStart();
        }
    }
    t0 = getMsecTime();
    if (result == len) {
        pCpd->modemInfo.lastDataSent = t0;
    }
    else {
        LOGE("Tx, %09u,%d,%d", t0, len, result);
    }
    LOGV("Tx, %09u,%d,%d", t0, len, result);
    CPD_LOG(CPD_LOG_ID_TXT, "\r\nTx, %09u,[", t0);
    CPD_LOG_DATA(CPD_LOG_ID_MODEM_RXTX | CPD_LOG_ID_MODEM_TX | CPD_LOG_ID_TXT, pB,  len);
    CPD_LOG(CPD_LOG_ID_TXT, "]\r\n");
    {
        int r;
        /* pass-through message, this is quick-fix implementation, change later */
        r = cpdSocketWriteToAll(&(pCpd->ssModemComm), (char *) pB, result);
    }
    if (waitForResponse > 0) {
        if (result != len) {
            result = 0;
            pCpd->modemInfo.waitingForResponse = 0;
        }
        else {
            while ((pCpd->modemInfo.haveResponse == 0) &&
                    (pCpd->modemInfo.responseValue == AT_RESPONSE_NONE)) {
                if (getMsecDt(t0) > waitForResponse) {
                    break;
                }
                usleep(10000);
            }
            if (pCpd->modemInfo.haveResponse == 0) {
                CPD_LOG(CPD_LOG_ID_TXT, "\r\n%u: !!! ModemResponseTimeout, %u, %u\n", getMsecTime(), getMsecDt(t0), waitForResponse);
                LOGE("%u: !!! ModemResponseTimeout, %u, %u", getMsecTime(), getMsecDt(t0), waitForResponse);
            }
            else {
                CPD_LOG(CPD_LOG_ID_TXT, "\r\n%u: ModemResponse, %u, %u\n", getMsecTime(), pCpd->modemInfo.haveResponse, pCpd->modemInfo.responseValue);
                LOGV("%u: ModemResponse, %u, %u", getMsecTime(), pCpd->modemInfo.haveResponse, pCpd->modemInfo.responseValue);
            }
        }
    }
    return result;
}

int cpdModemSocketWriteToAllExcpet(pSOCKET_SERVER pSS, char *pB, int len, int noWrite)
{
    int rr;
    int i;
    pCPD_CONTEXT pCpd = cpdGetContext();
    int result = 0;
    unsigned int t0;

    if ((pB == NULL) || (len <= 0) || (pCpd == NULL)) {
        return result;
    }
    pCpd->modemInfo.haveResponse = 0;
    pCpd->modemInfo.responseValue = AT_RESPONSE_NONE;

    result = modemWrite(pCpd->modemInfo.modemFd, pB, len);
    t0 = getMsecTime();
    CPD_LOG(CPD_LOG_ID_TXT, "\r\nTx, %09u,[", t0);
    CPD_LOG_DATA(CPD_LOG_ID_MODEM_RXTX | CPD_LOG_ID_MODEM_TX | CPD_LOG_ID_TXT , pB,  len);
    CPD_LOG(CPD_LOG_ID_TXT, "]\r\n");
    {
        int r;
        /* pass-through message to other sockets */
        r = cpdSocketWriteToAllExcpet(pSS, pB, result, noWrite);
    }
    result = CPD_OK;
    return result;
}


int cpdModemFindOk(char *pB, int len)
{
    int result = -1;
    char pStr[8];
    char *pS = NULL;

    if (pB == NULL) {
        return result;
    }
    /* is there OK anywhere? */
    snprintf(pStr, 7, "%s%s", AT_CMD_OK, AT_CMD_CRLF);
    pS = strstr(pB, pStr);
    if (pS != NULL){
        result = (int) (pS - pB);
        if (result > len) {
            result = -1;
        }
    }
    return result;
}

int cpdModemFindError(char *pB, int len)
{
    int result = -1;
    char pStr[16];
    char *pS = NULL;

    if (pB == NULL) {
        return result;
    }

    /* is there ERROR anywhere? */
    snprintf(pStr, 15, "%s%s", AT_CMD_ERROR, AT_CMD_CRLF);
    pS = strstr(pB, pStr);
    if (pS != NULL){
        result = (int) (pS - pB);
        if (result > len) {
            result = -1;
        }
    }
    return result;
}


int cpdModemFindCharX(char *pB, int len, char findMe)
{
    int result = -1;
    char *pS = NULL;

    if (len <= 0) {
        return result;
    }
    if (pB == NULL) {
        return result;
    }

    pS = strchr(pB, findMe);
    if (pS != NULL){
        result = (int) (pS - pB);
        if (result > len) {
            result = -1;
        }
    }
    return result;
}


int modem_find_str(char *pB, int len, char *findMe, int lenStr)
{
    int result = -1;
    char *pS = NULL;

    if (len <= 0) {
        return result;
    }
    if (pB == NULL) {
        return result;
    }

    pS = strstr(pB, findMe);
    if (pS != NULL){
        result = (int) (pS - pB);
        if (result > len) {
            result = -1;
        }
    }
    return result;
}



int modem_find_Esc(char *pB, int len)
{
    int result = -1;
    int i;

    if (pB == NULL) {
        return result;
    }

    for (i = 0; i < len; i++)
    {
        if (pB[i] == AT_CMD_ESC_CHR) {
            if (result < 0) {
                result = i;
                CPD_LOG(CPD_LOG_ID_TXT, "\nESC at %d\n", result);
                break;
            }
        }
    }
    return result;
}


int modem_find_CtrlZ(char *pB, int len)
{
    int result = -1;
    int i;

    if (pB == NULL) {
        return result;
    }

    for (i = 0; i < len; i++)
    {
        if (pB[i] == AT_CMD_CTRL_Z_CHR) {
            if (result < 0) {
                result = i;
                CPD_LOG(CPD_LOG_ID_TXT, "\nCtrl-Z at %d\n", result);
                break;
            }
        }
    }
    return result;
}

int modem_find_any_unsol_response(char *pB, int len)
{
    int result = CPD_ERROR;
    char unsolResponse[64];
    snprintf(unsolResponse, 63, "%s%c", AT_CMD_CPOSR,AT_CMD_COL_CHR);
    result = modem_find_str(pB, len, unsolResponse, strlen(unsolResponse));
    if (result > 1) {
        if (!((pB[result - 2] == '\r') && (pB[result - 1] == '\n'))) {
            result = CPD_ERROR;
        }
    }
    if (result >= 0) {
        if (result > len) {
            result = CPD_ERROR;
        }
    }
    //TODO: add other responses here and find the first one..
    return result;
}

int cpdModemMoveRxBufferLeft(pCPD_CONTEXT pCpd, int left)
{
    if (left < 0) {
        return pCpd->modemInfo.modemRxBufferIndex;
    }

    if (left > pCpd->modemInfo.modemRxBufferIndex) {
        left = pCpd->modemInfo.modemRxBufferIndex;
    }
    memmove((pCpd->modemInfo.pModemRxBuffer), (pCpd->modemInfo.pModemRxBuffer + left), pCpd->modemInfo.modemRxBufferIndex);
    pCpd->modemInfo.modemRxBufferIndex = pCpd->modemInfo.modemRxBufferIndex - left;
    return pCpd->modemInfo.modemRxBufferIndex;
}

/*
 * remove all CR, LF and spaces from left and right of the string.
 * pB must be null-terminated string.
 */
int utilStripResponse(char *pB)
{
    int len;
    int more = 1;
    if (pB == NULL){
        return 0;
    }
    while (((len = strlen(pB)) > 0) && (more)){
        more = 0;
        if ((pB[len-1] == ' ') || (pB[len-1] == AT_CMD_CR_CHR) || (pB[len-1] == AT_CMD_LF_CHR)) {
            pB[len-1] = 0;
            more++;
        }
        if ((pB[0] == ' ') || (pB[0] == AT_CMD_CR_CHR) || (pB[0] == AT_CMD_LF_CHR)) {
            memmove(pB, &(pB[1]),len);
            more++;
        }
    }
    return strlen(pB);

}

int cpdModemProcessOkResponse(pCPD_CONTEXT pCpd, int iOk, int iOkLen)
{
    int i, j;
    int iCol = -1;
    char *pName, *pValue;
    int result = CPD_NOK;
    /* null-terminate response string */
    i = iOk;
    for (j = 0; j < iOkLen; j++) {
        pCpd->modemInfo.pModemRxBuffer[i++] = 0;
    }

    utilStripResponse(pCpd->modemInfo.pModemRxBuffer);
    iCol = cpdModemFindCharX(pCpd->modemInfo.pModemRxBuffer, iOk, AT_CMD_COL_CHR);

    pName = pCpd->modemInfo.pModemRxBuffer;
    if (iCol > 0) {
        pCpd->modemInfo.pModemRxBuffer[iCol] = 0;
        pValue = &(pCpd->modemInfo.pModemRxBuffer[iCol + 1]);
        utilStripResponse(pValue);
    }
    else {
        pValue = NULL;
    }
//    CPD_LOG(CPD_LOG_ID_TXT , "\n%u, Name=[%s]\n Value=[%s]\n", getMsecTime(), pName, pValue);
//    LOGI("%u, Name=[%s] Value=[%s]", getMsecTime(), pName, pValue);
    result = CPD_OK;

    if (strncmp(pName, AT_CMD_RING, strlen(AT_CMD_RING)) == 0) {
        CPD_LOG(CPD_LOG_ID_TXT , "\n%u,RING %u\n", getMsecTime());
        iOkLen = 1;
    }

    if (strncmp(pName, AT_CMD_CPOSR, strlen(AT_CMD_CPOSR)) == 0) {
        if (pValue) {
            pCpd->modemInfo.registeredForCPOSR = atoi(pValue);
            pCpd->modemInfo.registeredForCPOSRat = getMsecTime();
        }
        CPD_LOG(CPD_LOG_ID_TXT , "\n%u, pCpd->modemInfo.registeredForCPOSR=%d @ %u\n",
            getMsecTime(), pCpd->modemInfo.registeredForCPOSR, pCpd->modemInfo.registeredForCPOSRat);
    }

    if (pCpd->modemInfo.waitingForResponse != 0){
        pCpd->modemInfo.haveResponse    = 1;
        pCpd->modemInfo.responseValue = AT_RESPONSE_OK;
        pCpd->modemInfo.waitingForResponse = 0;
    }


    cpdModemMoveRxBufferLeft(pCpd, iOk + iOkLen);
    return result;
}

int cpdModemProcessErrorResponse(pCPD_CONTEXT pCpd, int iError, int iErrorLen)
{
    int i, j;
    i = iError;
    for (j = 0; j < iErrorLen; j++) {
        pCpd->modemInfo.pModemRxBuffer[i++] = 0;
    }
    CPD_LOG(CPD_LOG_ID_TXT, "\n%u,ERROR @ %d, message:%s\n", getMsecTime(), iError, pCpd->modemInfo.pModemRxBuffer);
    LOGE("%u,MODEM ERROR @ %d, message:%s\n", getMsecTime(), iError, pCpd->modemInfo.pModemRxBuffer);

    if (pCpd->modemInfo.waitingForResponse != 0){
        pCpd->modemInfo.haveResponse    = 1;
        pCpd->modemInfo.responseValue = AT_RESPONSE_ERROR;
        pCpd->modemInfo.waitingForResponse = 0;
    }

    cpdModemMoveRxBufferLeft(pCpd, iError + iErrorLen);
    return 0;
}

/*
 * Process "unsolicited" modem respnse CPOSR: <xml>
  * Check if response is just plain CPOSR status reponse, if so, process it accordingly.
 */
int cpdModemProcessUnsolResponse(pCPD_CONTEXT pCpd, int iUsolResp, int iCrLf2x)
{
    int result = CPD_NOK;
    int i, j;
    int iCol = -1;
    int iCrLf2xLen;
    int iOk = -1;
    int iXml = -1;
    char *pName, *pValue;


    /* check if this is just a response to +CPOSR? comand */
    iOk = cpdModemFindOk(pCpd->modemInfo.pModemRxBuffer, pCpd->modemInfo.modemRxBufferIndex);
    iXml = cpdModemFindCharX(pCpd->modemInfo.pModemRxBuffer, iCrLf2x, XML_START_CHAR);

    if (iOk > 0) {
        if ((iXml < 0) || (iXml > iOk)) {
            if (pCpd->modemInfo.receivingXml != CPD_OK) {
                result = cpdModemProcessOkResponse(pCpd, iOk, 4);
                return result;
            }
        }
    }
    iCrLf2xLen = strlen(AT_UNSOL_RESPONSE_END1);
    if ((pCpd->modemInfo.pModemRxBuffer[iCrLf2x] == AT_CMD_ESC_CHR) ||
        (pCpd->modemInfo.pModemRxBuffer[iCrLf2x] == AT_CMD_CTRL_Z_CHR)) {
        iCrLf2xLen = 1;
    }
    i = iCrLf2x;
    for (j = 0; j < iCrLf2xLen; j++) {
        if (pCpd->modemInfo.pModemRxBuffer[i] <= ' ') {
            pCpd->modemInfo.pModemRxBuffer[i] = 0;
        }
        i++;
    }

    iCol = cpdModemFindCharX(pCpd->modemInfo.pModemRxBuffer, iCrLf2x, AT_CMD_COL_CHR);
    pName = pCpd->modemInfo.pModemRxBuffer;
    if (iCol > 0) {
        pCpd->modemInfo.pModemRxBuffer[iCol] = 0;
        utilStripResponse(pName);
        pValue = &(pCpd->modemInfo.pModemRxBuffer[iCol + 2]);
    }
    else {
        pValue = NULL;
    }
    if (pValue != NULL) {
        result = cpdXmlParse(pCpd, pValue, strlen(pValue));
    }

    if (result == CPD_OK) {
        cpdModemMoveRxBufferLeft(pCpd, iCrLf2x + iCrLf2xLen);
    }

    return result;
}




int cpdModemProcessRxData(pCPD_CONTEXT pCpd)
{
    int iOk = -1;
    int iError = -1;
    int iCtrlZ = -1;
    int iEsc = -1;
    int iRing = -1;
    int iColumn = -1;
    int iTermLen = 1;

    while (pCpd->modemInfo.modemRxBufferIndex > 0) {

        iOk = cpdModemFindOk(pCpd->modemInfo.pModemRxBuffer, pCpd->modemInfo.modemRxBufferIndex);
        iError = cpdModemFindError(pCpd->modemInfo.pModemRxBuffer, pCpd->modemInfo.modemRxBufferIndex);
        iCtrlZ = modem_find_CtrlZ(pCpd->modemInfo.pModemRxBuffer, pCpd->modemInfo.modemRxBufferIndex);
        iEsc = modem_find_Esc(pCpd->modemInfo.pModemRxBuffer, pCpd->modemInfo.modemRxBufferIndex);
        iRing = modem_find_str(pCpd->modemInfo.pModemRxBuffer, pCpd->modemInfo.modemRxBufferIndex, AT_CMD_RING AT_CMD_CRLF, strlen(AT_CMD_RING AT_CMD_CRLF));

        if (((iOk < 0) && (iRing >= 0)) ||
            ((iOk > iRing) && (iRing >= 0))) {
            iOk = iRing;
        }

        if ((iOk < 0) && (iError < 0) && (iCtrlZ < 0) && (iEsc < 0)) {
            /* no more complete responses in the buffer */
            break;
        }

        if ((iOk >= 0) || (iCtrlZ >= 0)) {
            if (iOk < 0) {
                iOk = iCtrlZ;
            }
            else {
                iTermLen = 4;
            }
            if ((iError < 0) || (iError > iOk)) {
                cpdModemProcessOkResponse(pCpd, iOk, iTermLen);
                iOk = -1;
            }
        }

        if ((iError >= 0) || ((iEsc >= 0))) {
            if (iError < 0) {
                iError = iEsc;
            }
            else {
                iTermLen = 7;
            }

            if ((iOk < 0) || (iOk > iError)) {
                cpdModemProcessErrorResponse(pCpd, iError, iTermLen);
                iError = -1;
            }
        }
    }

    return CPD_OK; /* TBD */
}

static int cpdCheckIfReceivedWaitForString(pCPD_CONTEXT pCpd)
{
    int result = CPD_NOK;
    int index;
    CPD_LOG(CPD_LOG_ID_TXT, "\n%u: %s()\n", getMsecTime(), __FUNCTION__);
    LOGV("%u: %s()", getMsecTime(), __FUNCTION__);
    switch (pCpd->modemInfo.waitForThisResponse) {
        case AT_RESPONSE_CRLF:
            index = modem_find_str(pCpd->modemInfo.pModemRxBuffer, pCpd->modemInfo.modemRxBufferIndex, AT_CMD_CRLF, strlen(AT_CMD_CRLF));
            if (index >= 0) {
                result = CPD_OK;
            }
            break;
        case AT_RESPONSE_ANY:
            result = CPD_OK;
            break;
        default:
            break;
    }
    if (result == CPD_OK) {
        pCpd->modemInfo.haveResponse = result;
        pCpd->modemInfo.waitForThisResponse = AT_RESPONSE_NONE;
    }
    CPD_LOG(CPD_LOG_ID_TXT, "\n%u: %s()=%d", getMsecTime(), __FUNCTION__, result);
    LOGD("%u: %s()=%d", getMsecTime(), __FUNCTION__, result);
    return result;
}

/*
 * Copy received data into buffer and do first-pass of checking buffer for any useful data.
 */
int cpdModemReadAndCopyData(pCPD_CONTEXT pCpd, char *pRxBuffer, int len)
{
    int i;
    int available = 0;
    int iOk = -1;
    int iError = -1;
    int iCtrlZ = -1;
    int iEsc = -1;
    int iCrLf2x = -1;
    int iUsolResp = -1;
    int iUsolResp2 = -1;
    int iRing = -1;
    int tryAgain = 0;
    char pStr[64];

    if (pCpd->modemInfo.pModemRxBuffer == NULL) {
        return CPD_NOK;
    }

    /* copy data into the main buffer */
    available = pCpd->modemInfo.modemRxBufferSize - pCpd->modemInfo.modemRxBufferIndex - 1;

    if (len > available) {
        cpdModemMoveRxBufferLeft(pCpd, (len - available));
    }
    memcpy((pCpd->modemInfo.pModemRxBuffer + pCpd->modemInfo.modemRxBufferIndex), pRxBuffer, len);
    pCpd->modemInfo.modemRxBufferIndex =  pCpd->modemInfo.modemRxBufferIndex + len;
    pCpd->modemInfo.pModemRxBuffer[pCpd->modemInfo.modemRxBufferIndex] = 0;

    if (pCpd->modemInfo.waitForThisResponse > AT_RESPONSE_OK) {
        cpdCheckIfReceivedWaitForString(pCpd);
    }

    while (pCpd->modemInfo.modemRxBufferIndex > 0) {
        tryAgain = 0;
        iOk = cpdModemFindOk(pCpd->modemInfo.pModemRxBuffer, pCpd->modemInfo.modemRxBufferIndex);

        iError = cpdModemFindError(pCpd->modemInfo.pModemRxBuffer, pCpd->modemInfo.modemRxBufferIndex);

        iCtrlZ = modem_find_CtrlZ(pCpd->modemInfo.pModemRxBuffer, pCpd->modemInfo.modemRxBufferIndex);

        iEsc = modem_find_Esc(pCpd->modemInfo.pModemRxBuffer, pCpd->modemInfo.modemRxBufferIndex);

        iRing = modem_find_str(pCpd->modemInfo.pModemRxBuffer, pCpd->modemInfo.modemRxBufferIndex, AT_CMD_RING AT_CMD_CRLF, strlen(AT_CMD_RING AT_CMD_CRLF));
        if (iRing >= 0) {
            CPD_LOG(CPD_LOG_ID_TXT, "\n !!! RING !!! @ %d", iRing) ;
        }

        iUsolResp = modem_find_any_unsol_response(pCpd->modemInfo.pModemRxBuffer, pCpd->modemInfo.modemRxBufferIndex);
        /*
        CPD_LOG(CPD_LOG_ID_TXT , "\niOk=%d, iError=%d, iCtrlZ=%d, iEsc=%d, iRing=%d, iUsolResp=%d, tryAgain=%d",
            iOk, iError, iCtrlZ, iEsc, iRing, iUsolResp, tryAgain);
        */
        if (iUsolResp >= 0) {
            if ((iOk >= 0) && (iOk < iUsolResp)) {
                iUsolResp = -1;
/*
                CPD_LOG(CPD_LOG_ID_TXT , "\niOk=%d, iError=%d, iCtrlZ=%d, iEsc=%d, iRing=%d, iUsolResp=%d, tryAgain=%d",
                    iOk, iError, iCtrlZ, iEsc, iRing, iUsolResp, tryAgain);
*/
            }
            if ((iError >= 0) && (iError < iUsolResp)) {
                iUsolResp = -1;
                /*
                CPD_LOG(CPD_LOG_ID_TXT , "\niOk=%d, iError=%d, iCtrlZ=%d, iEsc=%d, iRing=%d, iUsolResp=%d, tryAgain=%d",
                    iOk, iError, iCtrlZ, iEsc, iRing, iUsolResp, tryAgain);
*/
            }
            if ((iCtrlZ >= 0) && (iCtrlZ < iUsolResp)) {
                iUsolResp = -1;
                /*
                CPD_LOG(CPD_LOG_ID_TXT , "\niOk=%d, iError=%d, iCtrlZ=%d, iEsc=%d, iRing=%d, iUsolResp=%d, tryAgain=%d",
                    iOk, iError, iCtrlZ, iEsc, iRing, iUsolResp, tryAgain);
                */
            }
            if ((iRing >= 0) && (iRing < iUsolResp)) {
                iUsolResp = -1;
                /*
                CPD_LOG(CPD_LOG_ID_TXT , "\niOk=%d, iError=%d, iCtrlZ=%d, iEsc=%d, iRing=%d, iUsolResp=%d, tryAgain=%d",
                    iOk, iError, iCtrlZ, iEsc, iRing, iUsolResp, tryAgain);
                    */
            }
        }

        if (iUsolResp >= 0) {
            /*
                    CPD_LOG(CPD_LOG_ID_TXT, "\nProcessing iUsolResp @ %d", iUsolResp) ;
                    */
            iCrLf2x = modem_find_str(&(pCpd->modemInfo.pModemRxBuffer[iUsolResp]), (pCpd->modemInfo.modemRxBufferIndex - iUsolResp), AT_UNSOL_RESPONSE_END1, strlen(AT_UNSOL_RESPONSE_END1));
            if (iCrLf2x < 0) {
                iCrLf2x = modem_find_str(&(pCpd->modemInfo.pModemRxBuffer[iUsolResp]), (pCpd->modemInfo.modemRxBufferIndex - iUsolResp), AT_UNSOL_RESPONSE_END2, strlen(AT_UNSOL_RESPONSE_END2));
            }
            if (iCrLf2x > 0) {
                iCrLf2x = iCrLf2x + iUsolResp;
/*                CPD_LOG(CPD_LOG_ID_TXT, "\n1.Found iCrLf2x @ %d = %02X",iCrLf2x, pCpd->modemInfo.pModemRxBuffer[iCrLf2x]) ; */
                iUsolResp2 = modem_find_any_unsol_response(&(pCpd->modemInfo.pModemRxBuffer[iUsolResp+1]), pCpd->modemInfo.modemRxBufferIndex - (iUsolResp+1));
                if (iUsolResp2 >= 0) {
                    iUsolResp2 = iUsolResp2 + iUsolResp + 1;
                    if (iCrLf2x > iUsolResp2) {
                        /*
                                    CPD_LOG(CPD_LOG_ID_TXT, "\n!.Found iCrLf2x @ %d = %02X, iUsolResp2 = %d",iCrLf2x, pCpd->modemInfo.pModemRxBuffer[iCrLf2x], iUsolResp2) ;
                                    */
                    }
               }
            }
            if ((iCrLf2x < 0) && ((iCtrlZ > iUsolResp) || (iEsc > iUsolResp))) {
                if ((iCtrlZ >= 0) && ((iCtrlZ < iEsc) || (iEsc < 0))){
                    iCrLf2x = iCtrlZ;
                    iCtrlZ = -1;
                    iEsc = -1;
                }
                if ((iEsc >= 0) && ((iEsc < iCtrlZ)  || (iCtrlZ < 0))){
                    iCrLf2x = iEsc;
                    iCtrlZ = -1;
                    iEsc = -1;
                }
            }

            if (iCrLf2x >= 0) {
                if ((iUsolResp < iOk) && (iOk >= 0)) {
                    iOk = -1;
                }
                if ((iUsolResp < iError) && (iError >= 0)) {
                    iError = -1;
                }
                if ((iUsolResp < iCtrlZ) && (iCtrlZ >= 0)) {
                    iCtrlZ = -1;
                }
                if ((iUsolResp < iEsc) && (iEsc >= 0)) {
                    iEsc = -1;
                }
                if ((iUsolResp < iRing) && (iRing >= 0)) {
                    iRing = -1;
                }
                if ((iOk < 0) && (iError < 0) && (iCtrlZ < 0) && (iEsc < 0)) {
                    cpdModemProcessUnsolResponse(pCpd, iUsolResp, iCrLf2x);
                    tryAgain = 1;
                }
            }
        }


        if ((iOk >= 0) || (iError >= 0) || (iCtrlZ >= 0) || (iEsc >= 0) || (iRing >= 0)) {
            cpdModemProcessRxData(pCpd);
            tryAgain = 1;
        }
        if (tryAgain == 0) {
            break;
        }
    }
    return CPD_OK;
}



void *cpdModemReadThreadLoop(void *arg)
{
    int fd = 0;
    char pRxBuffer[TEMP_RX_BUFF_SIZE];
    int result, i, r;
    struct sigaction sigact;
    pCPD_CONTEXT pCpd = (pCPD_CONTEXT) arg;

    CPD_LOG(CPD_LOG_ID_TXT, "\n%u: cpdModemReadThreadLoop()\n", getMsecTime());
    LOGV("%u: %s()", getMsecTime(), __FUNCTION__);
    if (arg == NULL) {
        return NULL;
    }

    /* Register the SIGUSR1 signal handler */
    memset(&sigact, 0, sizeof(sigact));
    sigact.sa_handler = cpdModemReadThreadSignalHandler;
    sigaction(SIGUSR1, &sigact, NULL);

    pCpd->modemInfo.modemReadThreadState = THREAD_STATE_RUNNING;
    CPD_LOG(CPD_LOG_ID_TXT , "cpdModemReadThreadLoop() fd=%d, %d",
            pCpd->modemInfo.modemFd,
            pCpd->modemInfo.modemReadThreadState);
    LOGV("%s(), fd=%d, %d", __FUNCTION__,
            pCpd->modemInfo.modemFd,
            pCpd->modemInfo.modemReadThreadState);

    while (pCpd->modemInfo.modemReadThreadState == THREAD_STATE_RUNNING) {
        pRxBuffer[0] = 0;
        result = modemRead(pCpd->modemInfo.modemFd, pRxBuffer, TEMP_RX_BUFF_SIZE - 1);
        /* return value will be negative only on real error, not on empty buffer */
        if (result < 0) {
            CPD_LOG(CPD_LOG_ID_TXT , "\n%u: !!!Error %d reading from fd=%d Closing fd!\n", getMsecTime(), result, pCpd->modemInfo.modemFd);
            LOGE("%u: !!!Error %d reading from fd=%d Closing fd!\n", getMsecTime(), result, pCpd->modemInfo.modemFd);
            /* Avoid cross open/close */
            pthread_mutex_lock(&(pCpd->modemInfo.modemFdLock));
            modemClose(&(pCpd->modemInfo.modemFd));
            pthread_mutex_unlock(&(pCpd->modemInfo.modemFdLock));
            if(pCpd->pfSystemMonitorStart != NULL) {
                CPD_LOG(CPD_LOG_ID_TXT, "\nStarting SystemMonitor!");
                pCpd->pfSystemMonitorStart();
            }
            break;
        }
        /* don't waste time with null responses */
        if (result > 0)
        {
            pRxBuffer[result] = 0;
            pCpd->modemInfo.lastDataReceived = getMsecTime();
            /* pass-through message */
            r = cpdSocketWriteToAll(&(pCpd->ssModemComm), pRxBuffer, result);
            LOGV("Rx, %09u,%d", getMsecTime(), result);
            CPD_LOG(CPD_LOG_ID_TXT, "\r\nRx, %09u,[", getMsecTime());
            CPD_LOG_DATA(CPD_LOG_ID_MODEM_RXTX | CPD_LOG_ID_MODEM_RX | CPD_LOG_ID_TXT, pRxBuffer, result);
            CPD_LOG(CPD_LOG_ID_TXT, "]\r\n");
            /* process received data */
            r = cpdModemReadAndCopyData(pCpd, pRxBuffer, result);
        }

        /* no data, don't try reading again, but wait.. */
        if (result == 0) {
//            CPD_LOG(CPD_LOG_ID_TXT, "\r\n%09u,!!! 0 read from modem", getMsecTime());
            usleep(MODEM_POOL_INTERVAL * 1000UL);
        }
    }
    CPD_LOG(CPD_LOG_ID_TXT , "\n %u, !!! EXIT %s()\n", getMsecTime(), __FUNCTION__);
    LOGD("%u, !!! EXIT %s()\n", getMsecTime(), __FUNCTION__);
    pCpd->modemInfo.modemReadThreadState = THREAD_STATE_TERMINATED;
    return NULL;
}


/*
 * Register CPD with modem to receive CPOSR <XML> responses from modem.
 * returns: CPD_OK if modem accepts registration,
 *             CPD_NOK otherwise.
 */
int cpdModemInitForCP(pCPD_CONTEXT pCpd)
{
    int result = CPD_ERROR;
    int r;
    CPD_LOG(CPD_LOG_ID_TXT, "\n%u:%s()", getMsecTime(), __FUNCTION__);
    LOGV("%u:%s()", getMsecTime(), __FUNCTION__);

    r = cpdModemSendCommand(pCpd, pCmdAt, strlen(pCmdAt), AT_RESPONSE_TIMEOUT);
    /* these comands are sent as part of debug procedure, remove later, not needed for real CPD operation */
    r = cpdModemSendCommand(pCpd, pCmdXgendata, strlen(pCmdXgendata), AT_RESPONSE_TIMEOUT);
    r = cpdModemSendCommand(pCpd, pCmdCsqQ, strlen(pCmdCsqQ), AT_RESPONSE_TIMEOUT);
    r = cpdModemSendCommand(pCpd, pCmdCregQ, strlen(pCmdCregQ), AT_RESPONSE_TIMEOUT);
    r = cpdModemSendCommand(pCpd, pCmdXratQ, strlen(pCmdXratQ), AT_RESPONSE_TIMEOUT);
    r = cpdModemSendCommand(pCpd, pCmdCopsQ, strlen(pCmdCopsQ), AT_RESPONSE_TIMEOUT);
    /* end debug commands */
    pCpd->modemInfo.registeredForCPOSR = 0;
    pCpd->modemInfo.registeredForCPOSRat = 0;
    pCpd->modemInfo.receivedCPOSRat = 0;
    pCpd->modemInfo.processingCPOSRat = 0;
    pCpd->modemInfo.sendingCPOSat = 0;

    r = cpdModemSendCommand(pCpd, pCmdCposr1, strlen(pCmdCposr1), AT_RESPONSE_TIMEOUT);
    r = cpdModemSendCommand(pCpd, pCmdCposrQ, strlen(pCmdCposrQ), AT_RESPONSE_TIMEOUT);

    // MUX debug code, turn MUX logging ON:
//    r = cpdModemSendCommand(pCpd, pCmdMuxDebugOn, strlen(pCmdMuxDebugOn), AT_RESPONSE_TIMEOUT);

    /* TODO: define error and handling here.. */
    if (pCpd->modemInfo.registeredForCPOSR == CPD_OK) {
        result = CPD_OK;
    }
    else {
        result = CPD_NOK;
    }
    CPD_LOG(CPD_LOG_ID_TXT, "\n %u:%s() = %d", getMsecTime(), __FUNCTION__, result);
    LOGD("%u:%s() = %d", getMsecTime(), __FUNCTION__, result);
    return result;

}

/*
 * Open gsmtty port to modem. Default is gsmtty7.
 * Allocate and initialize Rx, Tx buffers.
 * Create and start Rx read thread.
 * Register wityh modem for +CPOSR.
 * Return: CPD_OK if all steps are successful.
 *             CPD_NOK if any of the step fails.
 * Function may be called again if it returns CPD_NOK. This is done in System Monitor thrad automatically.
 */
int cpdModemOpen(pCPD_CONTEXT pCpd)
{
    int result = CPD_ERROR;
    CPD_LOG(CPD_LOG_ID_TXT, "\n%u: %s()", getMsecTime(), __FUNCTION__);
    LOGV("%u: %s()", getMsecTime(), __FUNCTION__);
    if (pCpd == NULL) {
        LOGE("%u: %s()=%d, NULL", getMsecTime(), __FUNCTION__,  result);
        return result;
    }

    if (pCpd->modemInfo.pModemRxBuffer == NULL) {
        pCpd->modemInfo.pModemRxBuffer = malloc(MODEM_RX_BUFFER_SIZE);
        if (pCpd->modemInfo.pModemRxBuffer != NULL) {
            pCpd->modemInfo.modemRxBufferSize = MODEM_RX_BUFFER_SIZE;
        }
        else {
            LOGE("%u: %s()=%d, RxBuff==NULL", getMsecTime(), __FUNCTION__,  result);
            return result;
        }
    }
    if (pCpd->modemInfo.pModemTxBuffer == NULL) {
        pCpd->modemInfo.pModemTxBuffer = malloc(MODEM_TX_BUFFER_SIZE);
        if (pCpd->modemInfo.pModemTxBuffer != NULL) {
            pCpd->modemInfo.modemTxBufferSize = MODEM_TX_BUFFER_SIZE;
        }
        else {
            LOGE("%u: %s()=%d, TxBuff==NULL", getMsecTime(), __FUNCTION__,  result);
            return result;
        }
    }


    if ((pCpd->modemInfo.modemReadThreadState == THREAD_STATE_OFF) ||
        (pCpd->modemInfo.modemReadThreadState == THREAD_STATE_TERMINATED)) {

        memset(pCpd->modemInfo.pModemRxBuffer, 0, pCpd->modemInfo.modemRxBufferSize);
        pCpd->modemInfo.modemRxBufferIndex = 0;
        memset(pCpd->modemInfo.pModemTxBuffer, 0, pCpd->modemInfo.modemTxBufferSize);
        pCpd->modemInfo.modemTxBufferIndex = 0;

        if (pCpd->modemInfo.keepOpenCtrl.lastOpenAt > 0) {
            if (getMsecDt(pCpd->modemInfo.keepOpenCtrl.lastOpenAt) < 1000) {
                return result;
            }
        }

        pthread_mutex_lock(&(pCpd->modemInfo.modemFdLock));
        if (pCpd->modemInfo.modemFd <= 0) {
            pCpd->modemInfo.modemFd = modemOpen(pCpd->modemInfo.modemName, 0);
        }
        pthread_mutex_unlock(&(pCpd->modemInfo.modemFdLock));

        CPD_LOG(CPD_LOG_ID_TXT,"\n%s(%s) = %d", __FUNCTION__, pCpd->modemInfo.modemName, pCpd->modemInfo.modemFd);
        pCpd->modemInfo.keepOpenCtrl.keepOpenRetryCount++;
        pCpd->modemInfo.keepOpenCtrl.lastOpenAt = getMsecTime();
        pCpd->modemInfo.registeredForCPOSR = 0;
        pCpd->modemInfo.receivedCPOSRat = 0;
        pCpd->modemInfo.registeredForCPOSRat = 0;
        pCpd->modemInfo.waitForThisResponse = 0;

        if (pCpd->modemInfo.modemFd > CPD_ERROR) {
            pCpd->modemInfo.modemReadThreadState = THREAD_STATE_STARTING;
            result = pthread_create(&(pCpd->modemInfo.modemReadThread), NULL, cpdModemReadThreadLoop, (void *) pCpd);
            usleep(100000);
            if ((result == 0) && (pCpd->modemInfo.modemReadThreadState == THREAD_STATE_RUNNING)) {
                result = CPD_OK;
            }
        }
    }
    if (result == CPD_OK) {
        cpdModemInitForCP(pCpd);
        /* do not need to handle error case here, System Monitor will check it and take action if needed. */
    }
    CPD_LOG(CPD_LOG_ID_TXT , "\n  %u: %s()=%d", getMsecTime(), __FUNCTION__, result);
    LOGD("%u: %s(%s)=%d, %d", getMsecTime(), __FUNCTION__, pCpd->modemInfo.modemName, result, pCpd->modemInfo.modemFd);
    return result;
}

/*
 * Close modem connection.
 * Terminate Rx thread, close gsmttyX.
 * Does not release allocateb Rx, TX memory buffers. New Open() wil reuse already allocated space.
 */
int cpdModemClose(pCPD_CONTEXT pCpd)
{
    int result = CPD_ERROR;
    if (pCpd == NULL) {
        return result;
    }
    CPD_LOG(CPD_LOG_ID_TXT, "\n%u: %s()\n", getMsecTime(), __FUNCTION__);
    LOGV("%u: %s()", getMsecTime(), __FUNCTION__);
    pCpd->modemInfo.keepOpenCtrl.keepOpen = 0;
    pCpd->modemInfo.modemReadThreadState = THREAD_STATE_TERMINATE;
    usleep(100);
    /* Avoid cross open/close */
    pthread_mutex_lock(&(pCpd->modemInfo.modemFdLock));
    modemClose(&(pCpd->modemInfo.modemFd));
    pthread_mutex_unlock(&(pCpd->modemInfo.modemFdLock));
    usleep(1000);
    if ((pCpd->modemInfo.modemReadThreadState == THREAD_STATE_TERMINATED) ||
        (pCpd->modemInfo.modemReadThreadState == THREAD_STATE_OFF)) {
        result = CPD_OK;
    }
    else {
        if (pCpd->modemInfo.modemReadThread != 0) {
            pthread_kill(pCpd->modemInfo.modemReadThread, SIGUSR1);
            usleep(100);
        }
        /* Wait for thread termination */
//        pthread_join(pCpd->modemInfo.modemReadThread, NULL);
        pCpd->modemInfo.modemReadThreadState = THREAD_STATE_TERMINATED;
        result = CPD_NOK;
    }
    CPD_LOG(CPD_LOG_ID_TXT, "\n %u:%s() = %d", getMsecTime(), __FUNCTION__, result);
    LOGD("%u:%s() = %d", getMsecTime(), __FUNCTION__, result);
    return result;
}


