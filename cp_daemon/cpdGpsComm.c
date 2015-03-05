/*
 * hardware/Intel/cp_daemon/cpdGpsComm.c
 *
 * Connector between CPDD and GPS library.
 * Uses TCP/IP socket connection for bi-directional communication between GPS and CPDD.
 * GPS library creates Server-type socket on localhost:4121
 * CPDD opens Client-type socket connection to GPS.
 * Messages are in binary format - data structures, enclosed in header/tail, checksum.
 *
 * Functions that end with "_t(" are test functions and they won't be included in production code. Will be removed later.
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

#include <termios.h>
#include <sys/poll.h>

#define LOG_TAG "CPDDCOM"

#include "cpd.h"
#include "cpdInit.h"
#include "cpdUtil.h"

#include "cpdDebug.h"
#include "cpdSocketServer.h"

#include "cpdGpsComm.h"


int cpdFormatAndSendMsgToCpd(pCPD_CONTEXT pCpd);
int cpdSendStopToGPS(pCPD_CONTEXT pCpd);


/* =========== DEBUG & TEST functions ================= */

void cpdLogRequestParameters_t(pCPD_CONTEXT pCpd)
{
    int i;
    pNAV_MODEL_ELEM pNavModelElem;
    pGPS_ASSIST pGPSassist = &(pCpd->request.assist_data.GPS_assist);
    pREF_TIME pRefTime = &(pGPSassist->ref_time);
    pRRLP_MEAS pRrlpMeas = &(pCpd->request.posMeas.posMeas_u.rrlp_meas);
    pLOCATION_PARAMETERS pLocationParameters = &(pGPSassist->location_parameters);
    pPOINT_ALT_UNCERTELLIPSE pEllipse = &(pLocationParameters->shape_data.point_alt_uncertellipse);

    LOGD("%u: %s()", getMsecTime(), __FUNCTION__);
    CPD_LOG(CPD_LOG_ID_TXT, "\r\n %u, REQUEST, %d, %d\n", getMsecTime(), pCpd->request.flag, sizeof(REQUEST_PARAMS));

    CPD_LOG(CPD_LOG_ID_TXT , "\r\n RRLP_MEAS,%d, %d, %d, %d, %d\n",
        pRrlpMeas->method_type,
        pRrlpMeas->accurancy,
        pRrlpMeas->RRLP_method,
        pRrlpMeas->resp_time_seconds,
        pRrlpMeas->mult_sets
        );
    CPD_LOG(CPD_LOG_ID_TXT , "\r\n LOCATION_PARAMETERS,%d,%d\n",
        pLocationParameters->shape_type,
        pLocationParameters->time
        );
    if (pLocationParameters->shape_type == SHAPE_TYPE_POINT_ALT_UNCERT_ELLIPSE) {
        CPD_LOG(CPD_LOG_ID_TXT , "\r\n POINT_ALT_UNCERTELLIPSE,%d,%f,%f,%d,%d,%d,%d,%d,%d,%d\n",
            pEllipse->coordinate.latitude.north,
            pEllipse->coordinate.latitude.degrees,
            pEllipse->coordinate.longitude,
            pEllipse->altitude.height_above_surface,
            pEllipse->altitude.height,
            pEllipse->uncert_semi_major,
            pEllipse->uncert_semi_minor,
            pEllipse->orient_major,
            pEllipse->confidence,
            pEllipse->uncert_alt);
    }

    CPD_LOG(CPD_LOG_ID_TXT , "\r\n GPS_ASSIST,%d,%d\n",
        pGPSassist->flag,
        pGPSassist->nav_model_elem_arr_items
        );
    for (i = 0; i < pGPSassist->nav_model_elem_arr_items; i++) {
        pNavModelElem = &(pGPSassist->nav_model_elem_arr[i]);
        CPD_LOG(CPD_LOG_ID_TXT , "\r\n    NAV_MODEL_ELEM,[%d],%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
            i,
            pNavModelElem->sat_id,
            pNavModelElem->sat_status,
            pNavModelElem->ephem_and_clock.l2_code,
            pNavModelElem->ephem_and_clock.ura,
            pNavModelElem->ephem_and_clock.sv_health,
            pNavModelElem->ephem_and_clock.iodc,
            pNavModelElem->ephem_and_clock.l2p_flag,
            pNavModelElem->ephem_and_clock.esr1,
            pNavModelElem->ephem_and_clock.esr2,
            pNavModelElem->ephem_and_clock.esr3,
            pNavModelElem->ephem_and_clock.esr4,
            pNavModelElem->ephem_and_clock.tgd,
            pNavModelElem->ephem_and_clock.toc,
            pNavModelElem->ephem_and_clock.af2,
            pNavModelElem->ephem_and_clock.af1,
            pNavModelElem->ephem_and_clock.af0,
            pNavModelElem->ephem_and_clock.crs,
            pNavModelElem->ephem_and_clock.delta_n,
            pNavModelElem->ephem_and_clock.m0,
            pNavModelElem->ephem_and_clock.cuc,
            pNavModelElem->ephem_and_clock.ecc,
            pNavModelElem->ephem_and_clock.cus,
            pNavModelElem->ephem_and_clock.power_half,
            pNavModelElem->ephem_and_clock.toe,
            pNavModelElem->ephem_and_clock.fit_flag,
            pNavModelElem->ephem_and_clock.aoda,
            pNavModelElem->ephem_and_clock.cic,
            pNavModelElem->ephem_and_clock.omega0,
            pNavModelElem->ephem_and_clock.cis,
            pNavModelElem->ephem_and_clock.i0,
            pNavModelElem->ephem_and_clock.crc,
            pNavModelElem->ephem_and_clock.omega,
            pNavModelElem->ephem_and_clock.omega_dot,
            pNavModelElem->ephem_and_clock.idot
            );
    }
    CPD_LOG(CPD_LOG_ID_TXT , "\r\n REF_TIME,%d,%d,%d\n",
        pRefTime->GPS_time.GPS_TOW_msec,
        pRefTime->GPS_time.GPS_week,
        pRefTime->GPS_TOW_assist_arr_items);
    {
        int i;
        for (i = 0; i < pRefTime->GPS_TOW_assist_arr_items; i++) {
            CPD_LOG(CPD_LOG_ID_TXT , "\r\n    [%d],%d,%d,%d,%d,%d\n",
                i,
                pRefTime->GPS_TOW_assist_arr[i].sat_id,
                pRefTime->GPS_TOW_assist_arr[i].tlm_word,
                pRefTime->GPS_TOW_assist_arr[i].anti_sp,
                pRefTime->GPS_TOW_assist_arr[i].alert,
                pRefTime->GPS_TOW_assist_arr[i].tlm_res
                );
        }
    }

}

int cpdCreateLocResponseFromAssistData_t(pCPD_CONTEXT pCpd)
{
    pLOCATION pLoc;
    LOGD("%u: %s()", getMsecTime(), __FUNCTION__);

    pLoc = &(pCpd->response.location);

    memset(&(pCpd->response), 0, sizeof(RESPONSE_PARAMS));
    pLoc->time_of_fix = 0;
    pLoc->location_parameters.shape_type = SHAPE_TYPE_POINT_ALT_UNCERT_ELLIPSE;

    pLoc->location_parameters.shape_data.point_alt_uncertellipse.coordinate.latitude.north = 1;
    pLoc->location_parameters.shape_data.point_alt_uncertellipse.coordinate.latitude.degrees = 3712345;
    pLoc->location_parameters.shape_data.point_alt_uncertellipse.coordinate.longitude = -12212345;

    pLoc->location_parameters.shape_data.point_alt_uncertellipse.altitude.height_above_surface = 90;
    pLoc->location_parameters.shape_data.point_alt_uncertellipse.altitude.height = 0;

    pLoc->location_parameters.shape_data.point_alt_uncertellipse.uncert_semi_major = 7;
    pLoc->location_parameters.shape_data.point_alt_uncertellipse.uncert_semi_minor = 7;
    pLoc->location_parameters.shape_data.point_alt_uncertellipse.orient_major = 0;
    pLoc->location_parameters.shape_data.point_alt_uncertellipse.confidence = 100;
    pLoc->location_parameters.shape_data.point_alt_uncertellipse.uncert_alt = 10;

    if (pCpd->request.assist_data.GPS_assist.location_parameters.shape_type == SHAPE_TYPE_POINT_ALT_UNCERT_ELLIPSE) {
        memcpy(&(pLoc->location_parameters.shape_data.point_alt_uncertellipse.coordinate),
               &(pCpd->request.assist_data.GPS_assist.location_parameters.shape_data.point_alt_uncertellipse),
               sizeof(POINT_ALT_UNCERTELLIPSE));
        pLoc->location_parameters.shape_data.point_alt_uncertellipse.coordinate.longitude =
            pLoc->location_parameters.shape_data.point_alt_uncertellipse.coordinate.longitude + (rand() % 100) - (rand() % 100);
        pLoc->location_parameters.shape_data.point_alt_uncertellipse.coordinate.latitude.degrees =
            pLoc->location_parameters.shape_data.point_alt_uncertellipse.coordinate.latitude.degrees + (rand() % 100) - (rand() % 100);
    }
    pLoc->time_of_fix = pCpd->request.assist_data.GPS_assist.ref_time.GPS_time.GPS_TOW_msec + 1000;
    pCpd->response.flag = CPD_OK;
    CPD_LOG(CPD_LOG_ID_TXT | CPD_LOG_ID_CONSOLE, "\r\n Created location response, sending");
    if (pCpd->pfMessageHandlerInCpd != NULL) {
        pCpd->pfMessageHandlerInCpd(pCpd);
    }
    return CPD_OK;
}





/* =========== END  DEBUG & TEST functions ================= */




int cpdGpsMsgFindHeadTail(pGPS_COMM_BUFFER pGpsComm)
{
    int result = CPD_NOK;
    int *pI;

    if ((pGpsComm->rxBufferCmdStart >= 0) &&
        (pGpsComm->rxBufferCmdEnd > 0) &&
        (pGpsComm->rxBufferMessageType != CPD_MSG_TYPE_NONE)) {
        return CPD_OK;
    }

    if (pGpsComm->rxBufferCmdStart < 0) {
        pGpsComm->rxBufferCmdStart = cpdFindString(pGpsComm->pRxBuffer, pGpsComm->rxBufferIndex, CPD_MSG_HEADER_TO_GPS, 0);
        if (pGpsComm->rxBufferCmdStart < 0) {
            pGpsComm->rxBufferCmdStart = cpdFindString(pGpsComm->pRxBuffer, pGpsComm->rxBufferIndex, CPD_MSG_HEADER_FROM_GPS, 0);
        }
        if (pGpsComm->rxBufferCmdStart < 0) {
            pGpsComm->rxBufferCmdStart = cpdFindString(pGpsComm->pRxBuffer, pGpsComm->rxBufferIndex, CPD_MSG_HEADER_QUERRY, 0);
            if (pGpsComm->rxBufferCmdStart >= 0) {
                pGpsComm->rxBufferCmdEnd = pGpsComm->rxBufferCmdStart + strlen(CPD_MSG_HEADER_QUERRY) - strlen(CPD_MSG_TAIL);
                pGpsComm->rxBufferMessageType = CPD_MSG_TYPE_QUERRY;
                pGpsComm->rxBufferCmdDataSize = CPD_ERROR;
                result = CPD_OK;
                LOGD("%u: %s()=%d", getMsecTime(), __FUNCTION__, result);
                return result;
            }
        }
        if (pGpsComm->rxBufferCmdStart < 0) {
            return result;
        }
    }
    if (pGpsComm->rxBufferCmdStart > 0) {
        memmove(pGpsComm->pRxBuffer, &(pGpsComm->pRxBuffer[pGpsComm->rxBufferCmdStart]), pGpsComm->rxBufferIndex - pGpsComm->rxBufferCmdStart);
        pGpsComm->rxBufferIndex = pGpsComm->rxBufferIndex - pGpsComm->rxBufferCmdStart;
        pGpsComm->rxBufferCmdStart = 0;
    }
    if (pGpsComm->rxBufferIndex < (int) (strlen(CPD_MSG_HEADER_TO_GPS) + sizeof(int))) {
        return result;
    }
    pI = (int *) &(pGpsComm->pRxBuffer[strlen(CPD_MSG_HEADER_TO_GPS)]);
    pGpsComm->rxBufferMessageType = *pI;

    pGpsComm->rxBufferCmdEnd = cpdFindString(pGpsComm->pRxBuffer, pGpsComm->rxBufferIndex, CPD_MSG_TAIL, 0);
    if (pGpsComm->rxBufferCmdEnd <= pGpsComm->rxBufferCmdStart) {
        return result;
    }

    pI++;
    pGpsComm->rxBufferCmdDataSize = *pI;
    pI++;
    pGpsComm->rxBufferCmdDataStart = (int) (((char *) pI) - pGpsComm->pRxBuffer);

    if ((pGpsComm->rxBufferCmdDataStart + pGpsComm->rxBufferCmdDataSize) == pGpsComm->rxBufferCmdEnd) {
        result = CPD_OK;
    } else {
        memmove(pGpsComm->pRxBuffer, &(pGpsComm->pRxBuffer[pGpsComm->rxBufferCmdEnd]), pGpsComm->rxBufferIndex - pGpsComm->rxBufferCmdEnd);
        pGpsComm->rxBufferIndex = pGpsComm->rxBufferIndex - pGpsComm->rxBufferCmdEnd;
        pGpsComm->rxBufferCmdEnd = CPD_ERROR;
        pGpsComm->rxBufferCmdStart = CPD_ERROR;
        pGpsComm->rxBufferCmdDataSize = CPD_ERROR;
        pGpsComm->rxBufferCmdDataStart = CPD_ERROR;
        pGpsComm->rxBufferMessageType = CPD_ERROR;
    }

    LOGD("%u: %s()=%d", getMsecTime(), __FUNCTION__, result);
    return result;
}


/*
 *
 */
int cpdGpsCommHandlePacket(pCPD_CONTEXT pCpd)
{
    int result = CPD_OK;
    pGPS_COMM_BUFFER pGpsComm;

    pGpsComm = &(pCpd->gpsCommBuffer);
    LOGD("%u: %s(%d,%d)", getMsecTime(), __FUNCTION__,
        pGpsComm->rxBufferMessageType,
        pGpsComm->rxBufferCmdDataSize);
    CPD_LOG(CPD_LOG_ID_TXT, "\n%u: %s(%d,%d) \n", getMsecTime(), __FUNCTION__,
        pGpsComm->rxBufferMessageType,
        pGpsComm->rxBufferCmdDataSize);

    switch (pGpsComm->rxBufferMessageType) {
        case CPD_MSG_TYPE_MEAS_ABORT_REQ:
            CPD_LOG(CPD_LOG_ID_TXT, "\r\n  CPD_MSG_TYPE_MEAS_ABORT_REQ, %d, %d, %d\n", pGpsComm->rxBufferMessageType, pGpsComm->rxBufferCmdDataStart, pGpsComm->rxBufferCmdDataSize);
            LOGD("%u: %s(CPD_MSG_TYPE_MEAS_ABORT_REQ)", getMsecTime(), __FUNCTION__);
            memset(&(pCpd->request), 0, sizeof(REQUEST_PARAMS));
            pCpd->request.flag = REQUEST_FLAG_POS_MEAS;
            pCpd->request.posMeas.flag = POS_MEAS_ABORT;
            if (pCpd->pfMessageHandlerInGps != NULL) {
                pCpd->pfMessageHandlerInGps(pCpd);
            }
            break;
        case CPD_MSG_TYPE_POS_MEAS_REQ:
            CPD_LOG(CPD_LOG_ID_TXT,"%u: %s(CPD_MSG_TYPE_POS_MEAS_REQ, %d, %d), ID=%d", getMsecTime(), __FUNCTION__, pGpsComm->rxBufferCmdDataStart, pGpsComm->rxBufferCmdDataSize, pCpd->request.flag);
            LOGD("%u: %s(CPD_MSG_TYPE_POS_MEAS_REQ, %d, %d), ID=%d", getMsecTime(), __FUNCTION__, pGpsComm->rxBufferCmdDataStart, pGpsComm->rxBufferCmdDataSize, pCpd->request.flag);
            memset(&(pCpd->request), 0, sizeof(REQUEST_PARAMS));
            pCpd->request.flag = CPD_ERROR;
            if ((int) sizeof(REQUEST_PARAMS) >= pGpsComm->rxBufferCmdDataSize) {
                memcpy(&(pCpd->request),
                        &(pGpsComm->pRxBuffer[pGpsComm->rxBufferCmdDataStart]),
                        pGpsComm->rxBufferCmdDataSize);
                if (pCpd->request.version != CPD_MSG_VERSION) {
                    pCpd->request.flag = CPD_ERROR;
                }
            }
            if (pCpd->pfMessageHandlerInGps != NULL) {
                pCpd->pfMessageHandlerInGps(pCpd);
            }
            break;
        case CPD_MSG_TYPE_POS_MEAS_RESP:
            LOGD("%u: %s(CPD_MSG_TYPE_POS_MEAS_RESP)=%d", getMsecTime(), __FUNCTION__, pCpd->response.flag);
            CPD_LOG(CPD_LOG_ID_TXT , "\r\n  CPD_MSG_POS_MEAS_RESP , %d, %d, %d, ID=%d", pGpsComm->rxBufferMessageType, pGpsComm->rxBufferCmdDataStart, pGpsComm->rxBufferCmdDataSize, pCpd->response.flag);
            memset(&(pCpd->response), 0, sizeof(RESPONSE_PARAMS));
            pCpd->response.flag = CPD_ERROR;
            if ((int) sizeof(RESPONSE_PARAMS) >= pGpsComm->rxBufferCmdDataSize) {
                memcpy(&(pCpd->response),
                        &(pGpsComm->pRxBuffer[pGpsComm->rxBufferCmdDataStart]),
                        pGpsComm->rxBufferCmdDataSize);
                if (pCpd->response.version != CPD_MSG_VERSION) {
                    pCpd->response.flag = CPD_ERROR;
                }
            }
            pCpd->request.status.responseFromGpsReceivedAt = getMsecTime();
            if (pCpd->pfMessageHandlerInCpd != NULL) {
                result = pCpd->pfMessageHandlerInCpd(pCpd);
                if (result == CPD_OK) {
                    if (cpdIsNumberOfResponsesSufficientForRequest(pCpd)) {
                        pCpd->request.posMeas.flag = POS_MEAS_NONE;
                        pCpd->request.assist_data.flag = CPD_NOK;
                        pCpd->request.flag = CPD_NOK;
                    }
                }
            }
            break;
        case CPD_MSG_TYPE_QUERRY:
            LOGD("%u: %s(CPD_MSG_TYPE_QUERRY)", getMsecTime(), __FUNCTION__);
            CPD_LOG(CPD_LOG_ID_TXT , "\r\n  CPD_MSG_TYPE_QUERRY , %d, %d, %d\n", pGpsComm->rxBufferMessageType, pGpsComm->rxBufferCmdDataStart, pGpsComm->rxBufferCmdDataSize);
            break;
        default:
            LOGD("%u: %s(DEFAULT)", getMsecTime(), __FUNCTION__);
            CPD_LOG(CPD_LOG_ID_TXT , "\r\n  CPD_MSG_TYPE_QUERRY, %d, %d, %d\n", pGpsComm->rxBufferMessageType, pGpsComm->rxBufferCmdDataStart, pGpsComm->rxBufferCmdDataSize);
            break;
    }

    pGpsComm->rxBufferCmdEnd =  pGpsComm->rxBufferCmdEnd + strlen(CPD_MSG_TAIL);
    memmove(pGpsComm->pRxBuffer, &(pGpsComm->pRxBuffer[pGpsComm->rxBufferCmdEnd]), pGpsComm->rxBufferIndex - pGpsComm->rxBufferCmdEnd);
    pGpsComm->rxBufferIndex = pGpsComm->rxBufferIndex - pGpsComm->rxBufferCmdEnd;
    pGpsComm->rxBufferCmdEnd = CPD_ERROR;
    pGpsComm->rxBufferCmdStart = CPD_ERROR;
    pGpsComm->rxBufferCmdDataSize = CPD_ERROR;
    pGpsComm->rxBufferCmdDataStart = CPD_ERROR;
    pGpsComm->rxBufferMessageType = CPD_ERROR;
    LOGD("%u: %s()=%d", getMsecTime(), __FUNCTION__, result);
    return result;
}


/*
 * Message parsing.
 * Data is received by socket thread, which calls this function with new data blocks.
 * Function assembled received blocks (parts of message), strips-out all non-related data and
 * generates real CP/GPS message structures.
 */
int cpdGpsCommMsgReader(void * pArg, char *pB, int len, int index)
{
    int result = CPD_NOK;
    pCPD_CONTEXT pCpd;
    pGPS_COMM_BUFFER pGpsComm;
    int copySize;
    int remaining;

    LOGD("%u: %s()", getMsecTime(), __FUNCTION__);
    CPD_LOG(CPD_LOG_ID_TXT, "\n%u: %s(%d)\n", getMsecTime(), __FUNCTION__, len);
    pCpd = cpdGetContext();
    if (pCpd == NULL) {
        return result;
    }
    pGpsComm = &(pCpd->gpsCommBuffer);
    if (pGpsComm->rxBufferSize <= 0) {
        return result;
    }
    if (pGpsComm->rxBufferIndex >= pGpsComm->rxBufferSize) {
        pGpsComm->rxBufferIndex = 0;
        memset(pGpsComm->pRxBuffer, 0, pGpsComm->rxBufferSize);
    }
    remaining = len;
    while (remaining > 0) {
        copySize = pGpsComm->rxBufferSize - pGpsComm->rxBufferIndex - 1;
        if (copySize <= 0) {
            break;
        }
        if (copySize >= len) {
            copySize = len;
        }
        memcpy(&(pGpsComm->pRxBuffer[pGpsComm->rxBufferIndex]), pB, copySize);
        pGpsComm->rxBufferIndex =  pGpsComm->rxBufferIndex + copySize;
        remaining = remaining - copySize;
        if (cpdGpsMsgFindHeadTail(pGpsComm) == CPD_OK) {
            result = result + cpdGpsCommHandlePacket(pCpd);
        }
    }
    CPD_LOG(CPD_LOG_ID_TXT, "\n%u: %s(%d) = %d\n", getMsecTime(), __FUNCTION__, len, result);
    LOGD("%u: %s(%d)=%d", getMsecTime(), __FUNCTION__, len, result);
    return result;
}


/*
 * Message format:
 * <CPD_MSG_HEAGER_xx><CPD_MSG_TYPE_E><int length><data><CPD_MSG_TAIL>
 * length is length of data part of the message, all other pars have fixed length.
 */


/*
 * Create data packet with GPS abort request and send it to GPS.
 */
int cpdFormatAndSendMsg_MeasAbort(pCPD_CONTEXT pCpd)
{
    int result = CPD_ERROR;
    char pB[80];
    char *pC;
    int len;
    int *pI;
    pSOCKET_CLIENT pSc;

    LOGD("%u: %s()", getMsecTime(), __FUNCTION__);
    CPD_LOG(CPD_LOG_ID_TXT, "\n%u: %s()\n", getMsecTime(), __FUNCTION__);

    memset(pB, 0, 64);
    snprintf(pB, 64, "%s", CPD_MSG_HEADER_TO_GPS);
    len = strlen(pB);
    pI = (int *) &(pB[len]);
    *pI = (int) CPD_MSG_TYPE_MEAS_ABORT_REQ;
    pI++;
    *pI = 0;  /* no data for this message */
    pI++;
    pC = (char*) pI;
    len = strlen(CPD_MSG_TAIL);
    strncpy(pC, CPD_MSG_TAIL, len);
    pC = pC + len;
    len  = (int) (pC - pB);

    pSc = &(pCpd->scGps.clients[0]);
    result = cpdSocketWrite(pSc, pB, len);
    CPD_LOG(CPD_LOG_ID_TXT | CPD_LOG_ID_CONSOLE, "\r\n %u, %s([%s]) = %d = %d", getMsecTime(), __FUNCTION__, CPD_MSG_HEADER_TO_GPS, len, result);
    LOGD("%u: %s()=%d", getMsecTime(), __FUNCTION__, result);
    if(result == CPD_ERROR)
    {
        if(pCpd->pfSystemMonitorStart != NULL)
        {
            CPD_LOG(CPD_LOG_ID_TXT, "\nStarting SystemMonitor!");
            pCpd->pfSystemMonitorStart();
        }
    }
    return result;
}


int isCpdSessionActive(pCPD_CONTEXT pCpd)
{
    int result = CPD_NOK;
    if ((pCpd->request.posMeas.flag == POS_MEAS_RRC) || (pCpd->request.posMeas.flag == POS_MEAS_RRLP)) {
        if ((pCpd->request.status.requestReceivedAt > 0) && (pCpd->request.flag == REQUEST_FLAG_POS_MEAS)) {
            if (pCpd->request.status.requestReceivedAt > pCpd->request.status.stopSentToGpsAt)
            {
                result = CPD_OK;
            }
        }
    }
    return result;
}


/*
Check if number of responses sent so far is sufficient to fulfill request.
In the case where periodic updates are requested, return COD_NOK - requests are fulfiled only when requesting side stops the request
*/
int cpdIsNumberOfResponsesSufficientForRequest(pCPD_CONTEXT pCpd)
{
    int result = CPD_OK;
    if (pCpd->request.posMeas.flag == POS_MEAS_RRC) {
        if (pCpd->request.posMeas.posMeas_u.rrc_meas.rep_crit.period_rep_crit.rep_amount == 0){
            result = CPD_NOK;
        }
        else if ((unsigned int) (pCpd->request.posMeas.posMeas_u.rrc_meas.rep_crit.period_rep_crit.rep_amount) > pCpd->request.status.nResponsesSent) {
            result = CPD_NOK;
        }
    }
    else if (pCpd->request.posMeas.flag == POS_MEAS_RRLP) {
        if (pCpd->request.posMeas.posMeas_u.rrlp_meas.mult_sets == MULT_SETS_MULTIPLE) {
            result = CPD_NOK;
        }
        else if (pCpd->request.status.nResponsesSent < 1) {
            result = CPD_NOK;
        }
    }
    return result;
}

/*
 Return number of seconds allowed to fulfil request = initial time + number of positions (at 1/s)
 */
int cpdCalcRequredTimneToServiceRequest(pCPD_CONTEXT pCpd)
{
    int result = -1; /* infinite time */
    if (pCpd->request.posMeas.flag == POS_MEAS_RRC) {
        if (pCpd->request.posMeas.posMeas_u.rrc_meas.rep_crit.period_rep_crit.rep_amount == 0){
            result = -1; /* infinite time - run until canceled */
        }
        else {
            result = pCpd->request.posMeas.posMeas_u.rrc_meas.rep_crit.period_rep_crit.rep_amount *
                    pCpd->request.posMeas.posMeas_u.rrc_meas.rep_crit.period_rep_crit.rep_interval_long +
                    pCpd->request.posMeas.posMeas_u.rrc_meas.rep_crit.period_rep_crit.rep_interval_long;
        }
    }
    else if (pCpd->request.posMeas.flag == POS_MEAS_RRLP) {
        if (pCpd->request.posMeas.posMeas_u.rrlp_meas.mult_sets == MULT_SETS_MULTIPLE) {
            result = -1; /* infinite time - run until canceled */
        }
        else if (pCpd->request.status.nResponsesSent < 1) {
            result = pCpd->request.posMeas.posMeas_u.rrlp_meas.resp_time_seconds + 1;
        }
    }
    return result;
}


/*
  Function is aclled when response is sent to modem and modem acqs the message with OK.
  It is time to stop GPS now.
  Do not wait for the network to stop GPS if this is single-position request case.
  If it is contignous position reporting, do not send STOP to GPS.
 */
int cpdSendAbortToGps(pCPD_CONTEXT pCpd)
{
    int result = CPD_NOK;

    LOGD("%u: %s(), %u", getMsecTime(), __FUNCTION__, pCpd->request.status.stopSentToGpsAt);
    CPD_LOG(CPD_LOG_ID_TXT, "\n%u: %s(), %u\n", getMsecTime(), __FUNCTION__, pCpd->request.status.stopSentToGpsAt);
    result = cpdFormatAndSendMsg_MeasAbort(pCpd);
    usleep(1000);
    if (result > CPD_NOK) {
        result = CPD_OK;
        pCpd->request.dbgStats.posAbortId++;
        pCpd->systemMonitor.processingRequest = CPD_NOK;
        pCpd->activeMonitor.processingRequest = CPD_NOK;
        pCpd->request.status.stopSentToGpsAt = getMsecTime();
    }
    LOGD("%u: %s()=%d, %u", getMsecTime(), __FUNCTION__, result, pCpd->request.status.stopSentToGpsAt);
    CPD_LOG(CPD_LOG_ID_TXT, "\n%u: %s()=%d, %u\n", getMsecTime(), __FUNCTION__, result, pCpd->request.status.stopSentToGpsAt);
    return result;
}


/*
 * Create data packet with the request & aiding info and send it to GPS.
 */
int cpdFormatAndSendMsgToGps(pCPD_CONTEXT pCpd)
{
    int result = CPD_NOK;
    char *pB = NULL;
    char *pC;
    int pBSize;
    int len;
    int *pI;
    pSOCKET_CLIENT pSc;

    pBSize = strlen(CPD_MSG_HEADER_TO_GPS) + strlen(CPD_MSG_TAIL) + sizeof(REQUEST_PARAMS) + 128;
    pB = malloc(pBSize);
    if (pB == NULL) {
        return result;
    }
    pCpd->request.version = CPD_MSG_VERSION;
    memset(pB, 0, pBSize);

    snprintf(pB, pBSize, "%s", CPD_MSG_HEADER_TO_GPS);
    len = strlen(pB);
    pI = (int *) &(pB[len]);
    *pI = (int) CPD_MSG_TYPE_POS_MEAS_REQ;
    pI++;
    *pI = sizeof(REQUEST_PARAMS);  /* size of the structure */
    pI++;
    pC = (char*) pI;
    memcpy(pC, &(pCpd->request),sizeof(REQUEST_PARAMS));
    pC = pC + sizeof(REQUEST_PARAMS);
    len = strlen(CPD_MSG_TAIL);
    strncpy(pC, CPD_MSG_TAIL, len);
    pC = pC + len;
    len  = (int) (pC - pB);


    pSc = &(pCpd->scGps.clients[0]);
    result = cpdSocketWrite(pSc, pB, len);
    CPD_LOG(CPD_LOG_ID_TXT | CPD_LOG_ID_CONSOLE, "\r\n %u, %s([%s]) = %d = %d", getMsecTime(), __FUNCTION__, CPD_MSG_HEADER_TO_GPS, len, result);
    LOGD("%u: %s()=%d", getMsecTime(), __FUNCTION__, result);
    free((void *) pB);
    return result;
}

/*
 * Create data packet from GPS measurements and send it to CPD.
 */
int cpdFormatAndSendMsgToCpd(pCPD_CONTEXT pCpd)
{
    int result = CPD_NOK;
    char *pB = NULL;
    char *pC;
    int pBSize;
    int len;
    int *pI;
    pSOCKET_SERVER pSs;

    pBSize = strlen(CPD_MSG_HEADER_FROM_GPS) + strlen(CPD_MSG_TAIL) + sizeof(RESPONSE_PARAMS) + 128;
    pB = malloc(pBSize);
    if (pB == NULL) {
        return result;
    }
    pCpd->response.version = CPD_MSG_VERSION;

    memset(pB, 0, pBSize);
    snprintf(pB, pBSize, "%s", CPD_MSG_HEADER_FROM_GPS);
    len = strlen(pB);
    pI = (int *) &(pB[len]);
    *pI = (int) CPD_MSG_TYPE_POS_MEAS_RESP;
    pI++;
    *pI = sizeof(RESPONSE_PARAMS);  /* size of the structure */
    pI++;
    pC = (char*) pI;
    memcpy(pC, &(pCpd->response), sizeof(RESPONSE_PARAMS));
    pC = pC + sizeof(RESPONSE_PARAMS);
    len = strlen(CPD_MSG_TAIL);
    strncpy(pC, CPD_MSG_TAIL, len);
    pC = pC + len;
    len  = (int) (pC - pB);

    pSs = &(pCpd->scGps);
    result = cpdSocketWriteToAll(pSs, pB, len);
    CPD_LOG(CPD_LOG_ID_TXT, "\r\n %u, %s([%s]) = %d = %d", getMsecTime(), __FUNCTION__, CPD_MSG_HEADER_FROM_GPS, len, result);
    LOGD("%u: %s()=%d", getMsecTime(), __FUNCTION__, result);
    free((void *) pB);
    if(result != CPD_OK)
    {
        if(pCpd->pfSystemMonitorStart != NULL)
        {
            CPD_LOG(CPD_LOG_ID_TXT, "\nStarting SystemMonitor!");
            pCpd->pfSystemMonitorStart();
        }
    }
    return result;
}

/*
 * request GPS to stop.
 */
int cpdSendStopToGPS(pCPD_CONTEXT pCpd)
{
    int result = CPD_NOK;
    memset(&(pCpd->request), 0, sizeof(REQUEST_PARAMS));
    pCpd->request.flag = REQUEST_FLAG_CTRL_MSG;
    pCpd->request.posMeas.flag = POS_MEAS_STOP_GPS;
    result = cpdFormatAndSendMsgToGps(pCpd);
    CPD_LOG(CPD_LOG_ID_TXT,"\n%u: %s()=%d", getMsecTime(), __FUNCTION__, result);
    LOGD("%u: %s()=%d", getMsecTime(), __FUNCTION__, result);
    return result;
}



