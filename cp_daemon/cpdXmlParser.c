/*
 * hardware/Intel/cp_daemon/cpdXmlParser.c
 *
 * Parses XML files received with +CPOSR from modem.
 * Extracts data, applies any conversions needed(from 3GPP into logical) and populates request structure in CPD_CONTEXT.
 * Request structure is C-type structure which closely mimics XML file structure.
 * Request structure is then sent to GPS - position request - handled in cpdGpsComm.c.
 * Uses xmllib2 for XML parsing.
 *
 * Martin Junkar 09/18/2011
 *
 */


#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <math.h>
#define LOG_TAG "CPDD_XP"

#include "cpd.h"
#include "cpdUtil.h"
#include "cpdXmlUtils.h"
#include "cpdGpsComm.h"
#include "cpdXmlFormatter.h"
#include "cpdDebug.h"

#define CPOSR_POS_ELEMENT             "pos"
#define CPOSR_LOCATION_ELEMENT        "location"
#define CPOSR_ASSIST_DATA_ELEMENT     "assist_data"
#define CPOSR_POS_MEAS_ELEMENT        "pos_meas"
#define CPOSR_GPS_MEAS_ELEMENT        "GPS_meas"
#define CPOSR_GPS_ASSIST_REQ_ELEMENT  "GPS_assist_req"
#define CPOSR_MSG_ELEMENT             "msg"
#define CPOSR_POS_ERR_ELEMENT         "pos_err"


extern void cpdCloseSystemPowerState(pCPD_CONTEXT );
extern int cpdSystemActiveMonitorStart( void );

static int cpdConvert3GPPHorizontalAccuracyToM(int accuracyK)
{
    int result;
    double ftemp;
    ftemp = (double) GPP_LL_UNCERT_C * (pow(GPP_LL_UNCERT_1X, accuracyK) - 1.0);
    result = (int) ftemp;
    return result;
}

static int cpdConvert3GPPVerticalAccuracyToM(int accuracyK)
{
    int result;
    double ftemp;
    ftemp = (double) GPP_ALT_UNCERT_C * (pow(GPP_ALT_UNCERT_1X, accuracyK) - 1.0);
    result = (int) ftemp;
    return result;
}

void cpdLogRequestParametersInXmlParser_t(pCPD_CONTEXT pCpd)
{
    int i;
    pNAV_MODEL_ELEM pNavModelElem;
    pGPS_ASSIST pGPSassist = &(pCpd->request.assist_data.GPS_assist);
    pREF_TIME pRefTime = &(pGPSassist->ref_time);
    pRRLP_MEAS pRrlpMeas = &(pCpd->request.posMeas.posMeas_u.rrlp_meas);
    pLOCATION_PARAMETERS pLocationParameters = &(pGPSassist->location_parameters);
    pPOINT_ALT_UNCERTELLIPSE pEllipse = &(pLocationParameters->shape_data.point_alt_uncertellipse);


    CPD_LOG(CPD_LOG_ID_TXT, "\r\n %u, REQUEST, %d, %d\n", getMsecTime(), pCpd->request.flag, sizeof(REQUEST_PARAMS));

    CPD_LOG(CPD_LOG_ID_TXT, "\r\n RRLP_MEAS,%d, %d, %d, %d, %d\n",
        pRrlpMeas->method_type,
        pRrlpMeas->accurancy,
        pRrlpMeas->RRLP_method,
        pRrlpMeas->resp_time_seconds,
        pRrlpMeas->mult_sets
        );
    CPD_LOG(CPD_LOG_ID_TXT, "\r\n LOCATION_PARAMETERS,%d,%d\n",
        pLocationParameters->shape_type,
        pLocationParameters->time
        );
    if (pLocationParameters->shape_type == SHAPE_TYPE_POINT_ALT_UNCERT_ELLIPSE) {
        CPD_LOG(CPD_LOG_ID_TXT, "\r\n POINT_ALT_UNCERTELLIPSE,%d,%f,%f,%d,%d,%d,%d,%d,%d,%d\n",
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

    CPD_LOG(CPD_LOG_ID_TXT, "\r\n GPS_ASSIST,%d,%d\n",
        pGPSassist->flag,
        pGPSassist->nav_model_elem_arr_items
        );
    for (i = 0; i < pGPSassist->nav_model_elem_arr_items; i++) {
        pNavModelElem = &(pGPSassist->nav_model_elem_arr[i]);
        CPD_LOG(CPD_LOG_ID_TXT, "\r\n    NAV_MODEL_ELEM,[%d],%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
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
    CPD_LOG(CPD_LOG_ID_TXT, "\r\n REF_TIME,%d,%d,%d\n",
        pRefTime->GPS_time.GPS_TOW_msec,
        pRefTime->GPS_time.GPS_week,
        pRefTime->GPS_TOW_assist_arr_items);
    {
        int i;
        for (i = 0; i < pRefTime->GPS_TOW_assist_arr_items; i++) {
            CPD_LOG(CPD_LOG_ID_TXT, "\r\n    [%d],%d,%d,%d,%d,%d\n",
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

void cpdSendResponseThreradHandler_t(int sig)
{
    switch (sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGQUIT:
    case SIGUSR1:
        pthread_exit(0);
    }
}


void *cpdSendResponseThrerad_t(void *pArg)
{
    int result = 0;
    int n = 10;
    pCPD_CONTEXT pCpd;
    struct sigaction sigact;

    /* Register the SIGUSR1 signal handler */
    memset(&sigact, 0, sizeof(sigact));
    sigact.sa_handler = cpdSendResponseThreradHandler_t;
    sigaction(SIGUSR1, &sigact, NULL);



    pCpd = (pCPD_CONTEXT) pArg;
    CPD_LOG(CPD_LOG_ID_TXT , "\n  %u: %s()\n", getMsecTime(), __FUNCTION__);
    if (pCpd != NULL) {
        while (n > 0) {
            usleep(10000);

            result = cpdSendCpPositionResponseToModem(pCpd);

            if (result != CPD_NOK) {
                break;
            }
            else {
                /* wait for next retry */
                sleep(3);
            }
            n--;
        }
    }
    CPD_LOG(CPD_LOG_ID_TXT , "\n  %u: %s()=%d\n", getMsecTime(), __FUNCTION__, result);
    return NULL;
}

void cpdCreatePositionResponse_t(pCPD_CONTEXT pCpd)
{
    pLOCATION pLoc;
    int n = 7;
    int result = CPD_NOK;
    static int nRun = 0;
    double lla;
    static pthread_t sendThread = 0;


    if (pCpd->request.flag == REQUEST_FLAG_POS_MEAS) {
        if (pCpd->request.posMeas.flag == POS_MEAS_ABORT) {
            if (sendThread != 0) {
                pthread_kill(pCpd->systemMonitor.monitorThread, SIGUSR1);
                usleep(1000);
            }
        }
    }
    if ((pCpd->request.posMeas.flag == POS_MEAS_RRLP) ||
        (pCpd->request.posMeas.flag == POS_MEAS_RRC)) {


        pLoc = &(pCpd->response.location);
        memset(pLoc, 0, sizeof(LOCATION));
        CPD_LOG(CPD_LOG_ID_TXT , "\n  %u: %s()\n", getMsecTime(), __FUNCTION__);

        pLoc->time_of_fix = getMsecTime();
        pLoc->location_parameters.shape_type = SHAPE_TYPE_POINT_ALT_UNCERT_ELLIPSE;

        pLoc->location_parameters.shape_data.point_alt_uncertellipse.coordinate.latitude.north = TRUE;
        pLoc->location_parameters.shape_data.point_alt_uncertellipse.coordinate.latitude.degrees = 3712345;
        pLoc->location_parameters.shape_data.point_alt_uncertellipse.coordinate.longitude = -12212345;

        pLoc->location_parameters.shape_data.point_alt_uncertellipse.altitude.height_above_surface = 90;
        pLoc->location_parameters.shape_data.point_alt_uncertellipse.altitude.height = 0;

        pLoc->location_parameters.shape_data.point_alt_uncertellipse.uncert_semi_major = 7;
        pLoc->location_parameters.shape_data.point_alt_uncertellipse.uncert_semi_minor = 7;
        pLoc->location_parameters.shape_data.point_alt_uncertellipse.orient_major = 0;
        pLoc->location_parameters.shape_data.point_alt_uncertellipse.confidence = 100;
        pLoc->location_parameters.shape_data.point_alt_uncertellipse.uncert_alt = 10;

        pLoc->location_parameters.shape_data.point_alt_uncertellipse.altitude.height = nRun;

        lla = ((double) nRun) * 1.0;
        if (nRun > 0) {
            lla = lla + 0.00001;
        }
        pLoc->location_parameters.shape_data.point_alt_uncertellipse.coordinate.latitude.degrees = lla;
        pLoc->location_parameters.shape_data.point_alt_uncertellipse.coordinate.longitude = lla;
        nRun++;
#if(0)
        if (pCpd->request.assist_data.GPS_assist.location_parameters.shape_type == SHAPE_TYPE_POINT_ALT_UNCERT_ELLIPSE) {
            memcpy(&(pLoc->location_parameters.shape_data.point_alt_uncertellipse.coordinate),
                   &(pCpd->request.assist_data.GPS_assist.location_parameters.shape_data.point_alt_uncertellipse),
                   sizeof(POINT_ALT_UNCERTELLIPSE));
            pLoc->location_parameters.shape_data.point_alt_uncertellipse.coordinate.longitude =
                pLoc->location_parameters.shape_data.point_alt_uncertellipse.coordinate.longitude + (rand() % 100);
            pLoc->location_parameters.shape_data.point_alt_uncertellipse.coordinate.latitude.degrees =
                pLoc->location_parameters.shape_data.point_alt_uncertellipse.coordinate.latitude.degrees + (rand() % 200);
        }
#endif
        if (sendThread != 0) {
            pthread_kill(pCpd->systemMonitor.monitorThread, SIGUSR1);
            usleep(1000);
        }
        result = pthread_create(&(sendThread), NULL, cpdSendResponseThrerad_t, (void *) pCpd);
    }
}




/*
 * Find child node
 * Return value:
 *  - the a child with the given child name; or NULL
 *  - the fisrt node if child name is NULL; or NULL
 */
static xmlNode *xmlNodeGetChild(xmlNode *pParent, const char *pChildName)
{
    xmlNode *pNode = NULL;
    if (pParent == NULL) {
        return pNode;
    }
    pNode = pParent->xmlChildrenNode;

    while (pNode != NULL) {
        if (pNode->type == XML_ELEMENT_NODE) {
            /* Find a specific node, or find the fist node ? */
            if (pChildName != NULL) {
                if (!xmlStrcmp(pNode->name, (xmlChar *)pChildName)) {
                    break;
                }
            }
            else {
                break;
            }
        }
        pNode = pNode->next;
    }
    return pNode;
}


/*
 * Find child node anywhere in the tree satrting from <pParent>
 * Return value:
 *  - the child with the given child name; or NULL
 *  - the fisrt node if child name is NULL; or NULL
 */
static xmlNode *xmlNodeGetNode(xmlNode *pParent, const char *pChildName)
{
    xmlNode *pNode = NULL;
    xmlNode *pIt;
    if (pParent == NULL) {
        return pNode;
    }
    pNode = xmlNodeGetChild(pParent, pChildName);
    if (pNode == NULL) {
        pIt = pParent->xmlChildrenNode;
        while (pIt != NULL) {
            pNode = xmlNodeGetNode(pIt, pChildName);
            if (pNode != NULL) {
                break;
            }
            pIt = pIt->next;
        }
    }
    return pNode;
}

/*
 * Calculates the number of nodes with the given name in the XML tree
 */
static int xmlNodeGetChildCount(xmlNode *parent)
{
    int count=0;
    xmlNode *cur = parent->xmlChildrenNode;

    while (cur != NULL) {
        if (cur->type == XML_ELEMENT_NODE)
        {
            count++;
        }

        cur = cur->next;
    }

    return count;
}

/*
 * Calculates the number of nodes with the given name in the XML tree
 */
static int xmlNodeGetChildNameCount(xmlNode *parent, const char *child_name)
{
    int count=0;
    xmlNode *cur = parent->xmlChildrenNode;

    while (cur != NULL) {
        if ((cur->type == XML_ELEMENT_NODE) &&
            (!xmlStrcmp(cur->name, (xmlChar *) child_name)))
        {
            count++;
        }

        cur = cur->next;
    }

    return count;
}



/*
 * Return value stored in <node>.
 * Must use xmlFree() to fee retuned string!!!!
 */
static xmlChar *xmlNodeGetChildString(xmlDoc *pDoc, xmlNode *parent, const char *child_name)
{
    xmlChar *value = NULL;
    xmlNode *node;

    node = xmlNodeGetChild(parent, child_name);
    if (node != NULL) {
        value = xmlNodeListGetString(pDoc, node->xmlChildrenNode, 1);
    }

    return value;
}

/*
 * Return property value for <property_name> in this <node>.
 *
 */
static xmlChar *xmlNodeGetProperty(xmlNode *node, const char *property_name)
{
    xmlChar *value = NULL;

    if (node != NULL) {
        value = xmlGetProp(node, (xmlChar *) property_name);
    }

    return value;
}

/*
 * Return property value for <property_name> in given child node.
 * result is string - value; or NULL if either child or property does not exist.
 */
static xmlChar *xmlNodeGetChildProperty(xmlNode *parent, const char *child_name, const char *property_name)
{
    xmlChar *value = NULL;
    xmlNode *node;

//  node = xmlNodeGetChild(parent, child_name);
    node = xmlNodeGetNode(parent, child_name);
    if (node != NULL) {
        value = xmlNodeGetProperty(node, property_name);
    }
    return value;
}

/*
 * 1. convert an xml string to XML_PARSER_METHOD_TYPE_*
 */
static int cpdXmlStrTo_gpp_method_type(const xmlChar *value)
{
    int result = GPP_METHOD_TYPE_NONE;
    if (value == NULL) {
       return result;
    }

    if ((!xmlStrcmp(value,  (xmlChar *) "ms_assisted")) ||
        (!xmlStrcmp(value, (xmlChar *) "ms_assisted_pref")) ||
        (!xmlStrcmp(value, (xmlChar *) "ue_assisted")) ||
        (!xmlStrcmp(value, (xmlChar *) "ue_assisted_pref")))
    {
        result = GPP_METHOD_TYPE_MS_ASSISTED;
    }
    else if ((!xmlStrcmp(value, (xmlChar *) "ms_based")) ||
             (!xmlStrcmp(value, (xmlChar *) "ms_based_pref")) ||
             (!xmlStrcmp(value, (xmlChar *) "ue_based")) ||
             (!xmlStrcmp(value, (xmlChar *) "ue_based_pref")))
    {
        result = GPP_METHOD_TYPE_MS_BASED;
    }
    else if (!xmlStrcmp(value, (xmlChar *) "ms_assisted_no_accuracy")) {
        result = GPP_METHOD_TYPE_MS_ASSISTED_NO_ACCURACY;
    }
    return result;
}

/*
 * 1. convert an xml  string (rrlp_method) to RRLP_METHOD_T
 * 2. Release the provided xml string
 */
int cpdXmlStringTo_rrlp_method(xmlChar *value)
{
    int result = RRLP_METHOD_NONE;
    if (value == NULL) {
       return result;
    }
    if (xmlStrcmp(value, (xmlChar *) "gps") == 0) {
        result = RRLP_METHOD_GPS;
    }
    return result;
}

static int cposXmlStringTo_mult_sets(xmlChar *value)
{
    int result = MULT_SETS_NONE;
    if (value == NULL) {
       return result;
    }

    if (!xmlStrcmp(value, (xmlChar *) "multiple")) {
        result = MULT_SETS_MULTIPLE;
    }
    else if (!xmlStrcmp(value, (xmlChar *) "one")) {
        result = MULT_SETS_ONE;
    }

    return result;
}

int cpdAddTextToXmlBuffer(pCPD_CONTEXT pCpd, char *pB, int len)
{
    int result = CPD_NOK;
    int available;
    /* copy data into the main buffer */
    available = pCpd->xmlRxBuffer.xmlBufferSize - pCpd->xmlRxBuffer.xmlBufferIndex - 1;

    if (len > available) {
        LOGE("%s(), Not enough space in XML buffer!!!, %d, %d", __FUNCTION__, available, len);
        len = available;
        CPD_LOG(CPD_LOG_ID_TXT, "Not enough space in XML buffer!!!\n");
    }
    else {
        available = len;
    }

    if (pCpd->xmlRxBuffer.xmlBufferIndex == 0) {
        if (pB[0] != XML_START_CHAR) {
            CPD_LOG(CPD_LOG_ID_TXT,"\n!!!Invalid Start for XML file\n");
            LOGE("!!!Invalid Start for XML file");
            return len;
        }
    }

    pCpd->modemInfo.receivingXml = CPD_OK;

    memcpy((pCpd->xmlRxBuffer.pXmlBuffer + pCpd->xmlRxBuffer.xmlBufferIndex), pB, available);
    pCpd->xmlRxBuffer.xmlBufferIndex =  pCpd->xmlRxBuffer.xmlBufferIndex + available;
    pCpd->xmlRxBuffer.pXmlBuffer[pCpd->xmlRxBuffer.xmlBufferIndex] = 0;
    CPD_LOG(CPD_LOG_ID_TXT, "\nAdded %d bytes to XML buffer", len);
    LOGD("Added %d bytes to XML buffer", len);
    return available;
}

/*
 * Check if XML buffer contains old data.
 * This is a sign that previous XML document was never completed.
 * Clear old data, if needed.
 */
int cpdClearOldXmlData(pXML_BUFFER pXmlBuff)
{
    int result = CPD_OK;
    unsigned int tNow = get_msec_time_now();
    unsigned int dT;
    if ((pXmlBuff->lastUpdate == 0) || (pXmlBuff->maxAge == 0)) {
        return result;
    }
    if (pXmlBuff->lastUpdate != 0) {
        if ( getMsecDt(pXmlBuff->lastUpdate) > pXmlBuff->maxAge) {
            pXmlBuff->pXmlBuffer[0] = 0;
            pXmlBuff->xmlBufferIndex = 0;
            pXmlBuff->lastUpdate = 0;
            return CPD_NOK;

        }
    }
    return result;
}

/*
 * Check if xml buffer looks like a complete XML document.
 * This is to avoid parsing XML buffer if it is obvious that this is not full/complete XML document.
 * return:
 *          CPD_OK if buffer looks like closed XML document
 *          CPD_NOK if buffer does not look like closed XML document.
 */
int cpdIsXmlClosed(char *pB, int len)
{
    int result = CPD_NOK;
    int i, j;
    if (len < 2) {
        CPD_LOG(CPD_LOG_ID_TXT,"\nXML is too short");
        return result;
    }
    i = 0;
    while ((pB[i] == ' ') || (pB[i] == AT_CMD_CR_CHR) || (pB[i] == AT_CMD_LF_CHR)) {
        i++;
    }
    j = len - 1;
    while ((pB[j] == ' ') || (pB[j] == AT_CMD_CR_CHR) || (pB[j] == AT_CMD_LF_CHR)) {
        j--;
    }
    if ((pB[i] == XML_START_CHAR) && (pB[j] == XML_END_CHAR)) {
        result = CPD_OK;
    }
    CPD_LOG(CPD_LOG_ID_TXT,"\nXML[%d]=%c, XML[%d]=%c", i, pB[i], j, pB[j]);
    return result;
}




/*
 * Parse "ref_time" element
 */
int cpdXmlParse_ref_time(xmlDoc *pDoc, xmlNode *pParent, pREF_TIME pRefTime)
{
    int result = CPD_ERROR;
    xmlNode *pNode;
    xmlNode *pNodeTow;
    double dtemp;
    CPD_LOG(CPD_LOG_ID_TXT, "\n  %u: %s()\n", getMsecTime(), __FUNCTION__);
    LOGD("%u: %s()", getMsecTime(), __FUNCTION__);

    memset(pRefTime, 0, sizeof(REF_TIME));
    pNode = xmlNodeGetNode(pParent, "GPS_TOW_msec");
    if (pNode != NULL) {
        xmlNodeGetLong(pNode, &(pRefTime->GPS_time.GPS_TOW_msec));
        pRefTime->GPS_time.gpsTimeReceivedAt = getMsecTime();
        pRefTime->isSet = CPD_OK;
        CPD_LOG(CPD_LOG_ID_TXT ,"\n GPS_TOW_msec= %d\n", pRefTime->GPS_time.GPS_TOW_msec);
    }
    pNode = xmlNodeGetNode(pParent, "GPS_week");
    if (pNode != NULL) {
        xmlNodeGetInt(pNode, &(pRefTime->GPS_time.GPS_week));
    }

    pNodeTow = xmlNodeGetNode(pParent, "GPS_TOW_assist");
    while (pNodeTow != NULL) {
        if (pNodeTow->type == XML_ELEMENT_NODE) {
            if (xmlStrcmp(pNodeTow->name, (xmlChar *) "GPS_TOW_assist") == 0) {
                pNode = xmlNodeGetNode(pNodeTow, "sat_id");
                if (pNode != NULL) {
                    xmlNodeGetInt(pNode, &(pRefTime->GPS_TOW_assist_arr[pRefTime->GPS_TOW_assist_arr_items].sat_id));
                    pRefTime->GPS_TOW_assist_arr[pRefTime->GPS_TOW_assist_arr_items].sat_id++;
                }
                pNode = xmlNodeGetNode(pNodeTow, "tlm_word");
                if (pNode != NULL) {
                    xmlNodeGetInt(pNode, &(pRefTime->GPS_TOW_assist_arr[pRefTime->GPS_TOW_assist_arr_items].tlm_word));
                }
                pNode = xmlNodeGetNode(pNodeTow, "anti_sp");
                if (pNode != NULL) {
                    xmlNodeGetInt(pNode, &(pRefTime->GPS_TOW_assist_arr[pRefTime->GPS_TOW_assist_arr_items].anti_sp));
                }
                pNode = xmlNodeGetNode(pNodeTow, "alert");
                if (pNode != NULL) {
                    xmlNodeGetInt(pNode, &(pRefTime->GPS_TOW_assist_arr[pRefTime->GPS_TOW_assist_arr_items].alert));
                }
                pNode = xmlNodeGetNode(pNodeTow, "tlm_res");
                if (pNode != NULL) {
                    xmlNodeGetInt(pNode, &(pRefTime->GPS_TOW_assist_arr[pRefTime->GPS_TOW_assist_arr_items].tlm_res));
                }
                pRefTime->GPS_TOW_assist_arr_items++;
                if (pRefTime->GPS_TOW_assist_arr_items >= GPS_MAX_N_SVS) {
                    /* no more space, too many SVs? */
                    break;
                }
            }
        }
        pNodeTow = pNodeTow->next;
    }
    CPD_LOG(CPD_LOG_ID_TXT , "\r\nREF_TIME,%d,%d,%d\n",
        pRefTime->GPS_time.GPS_TOW_msec,
        pRefTime->GPS_time.GPS_week,
        pRefTime->GPS_TOW_assist_arr_items);
    {
        int i;
        for (i = 0; i < pRefTime->GPS_TOW_assist_arr_items; i++) {
            CPD_LOG(CPD_LOG_ID_TXT, "\r\n    [%d],%d,%d,%d,%d,%d\n",
                i,
                pRefTime->GPS_TOW_assist_arr[i].sat_id,
                pRefTime->GPS_TOW_assist_arr[i].tlm_word,
                pRefTime->GPS_TOW_assist_arr[i].anti_sp,
                pRefTime->GPS_TOW_assist_arr[i].alert,
                pRefTime->GPS_TOW_assist_arr[i].tlm_res
                );
        }
    }
    LOGD("ASSIST, REF_TIME,%ld,%d,%d",
        pRefTime->GPS_time.GPS_TOW_msec,
        pRefTime->GPS_time.GPS_week,
        pRefTime->GPS_TOW_assist_arr_items);
    result = CPD_OK;
    return result;
}

/*
 * Parse "assist_data" element
 */
int cpdXmlParse_ellipsoid_point_alt_uncertellipse(xmlDoc *pDoc, xmlNode *pParent, pPOINT_ALT_UNCERTELLIPSE pEllipse)
{
    int result = CPD_ERROR;
    xmlNode *pNode;
    double dtemp;
    long ltemp;

    CPD_LOG(CPD_LOG_ID_TXT, "\n  %u: %s()\n", getMsecTime(), __FUNCTION__);
    LOGD("%u: %s()", getMsecTime(), __FUNCTION__);
    memset(pEllipse, 0, sizeof(POINT_ALT_UNCERTELLIPSE));
    pNode = xmlNodeGetNode(pParent, "north");
    if (pNode != NULL) {
        xmlNodeGetInt(pNode, &(pEllipse->coordinate.latitude.north));
    }
    pNode = xmlNodeGetNode(pParent, "degrees");
    if (pNode != NULL) {
        xmlNodeGetLong(pNode, &ltemp);
        /* Convert 3GGP value into degrees */
        CPD_LOG(CPD_LOG_ID_TXT ,"\n degrees=%d, %08X, N=%d\n", ltemp, ltemp, pEllipse->coordinate.latitude.degrees);
        pEllipse->coordinate.latitude.degrees = ((double) ltemp) * LATITUDE_GPP_TO_FLOAT;
        if (pEllipse->coordinate.latitude.north == 1) {
            pEllipse->coordinate.latitude.degrees =  -pEllipse->coordinate.latitude.degrees;
        }
        CPD_LOG(CPD_LOG_ID_TXT ,"\nLatitude=%f", pEllipse->coordinate.latitude.degrees);
    }
    pNode = xmlNodeGetNode(pParent, "longitude");
    if (pNode != NULL) {
        xmlNodeGetLong(pNode, &ltemp);
        /* Convert 3GGP value into degrees */
        pEllipse->coordinate.longitude = ((double) ltemp) * LONGITUDE_GPP_TO_FLOAT; //(90777.39816);
        CPD_LOG(CPD_LOG_ID_TXT ,"\nLongitude=%d, %08X, %f\n", ltemp, ltemp, pEllipse->coordinate.longitude);
        LOGD("Longitude=%ld %08X, %f", ltemp, (unsigned int)ltemp, pEllipse->coordinate.longitude);
    }
    pNode = xmlNodeGetNode(pParent, "height_above_surface");
    if (pNode != NULL) {
        xmlNodeGetInt(pNode, &(pEllipse->altitude.height_above_surface));
    }
    pNode = xmlNodeGetNode(pParent, "height");
    if (pNode != NULL) {
        xmlNodeGetInt(pNode, &(pEllipse->altitude.height));
    }
    pNode = xmlNodeGetNode(pParent, "uncert_semi_major");
    if (pNode != NULL) {
        xmlNodeGetInt(pNode, &(pEllipse->uncert_semi_major));
//        dtemp = GPP_LL_UNCERT_C * (pow(GPP_LL_UNCERT_1X, (double) pEllipse->uncert_semi_major) - 1.0);
//        pEllipse->uncert_semi_major = (int) dtemp;
        pEllipse->uncert_semi_major = cpdConvert3GPPHorizontalAccuracyToM(pEllipse->uncert_semi_major);
    }
    pNode = xmlNodeGetNode(pParent, "uncert_semi_minor");
    if (pNode != NULL) {
        xmlNodeGetInt(pNode, &(pEllipse->uncert_semi_minor));
//        dtemp = GPP_LL_UNCERT_C * (pow(GPP_LL_UNCERT_1X, (double) pEllipse->uncert_semi_minor) - 1.0);
//        pEllipse->uncert_semi_minor = (int) dtemp;
        pEllipse->uncert_semi_minor = cpdConvert3GPPHorizontalAccuracyToM(pEllipse->uncert_semi_minor);
    }
    pNode = xmlNodeGetNode(pParent, "orient_major");
    if (pNode != NULL) {
        xmlNodeGetInt(pNode, &(pEllipse->orient_major));
    }
    pNode = xmlNodeGetNode(pParent, "confidence");
    if (pNode != NULL) {
        xmlNodeGetInt(pNode, &(pEllipse->confidence));
    }
    pNode = xmlNodeGetNode(pParent, "uncert_alt");
    if (pNode != NULL) {
        xmlNodeGetInt(pNode, &(pEllipse->uncert_alt));
//        dtemp = GPP_ALT_UNCERT_C * (pow(GPP_ALT_UNCERT_1X, (double) pEllipse->uncert_alt) - 1.0);
//        pEllipse->uncert_alt = dtemp;
        pEllipse->uncert_alt = cpdConvert3GPPVerticalAccuracyToM(pEllipse->uncert_alt);
    }
    CPD_LOG(CPD_LOG_ID_TXT , "POINT_ALT_UNCERTELLIPSE,%d,%f,%f,%d,%d,%d,%d,%d,%d,%d\n",
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
    LOGD("POINT_ALT_UNCERTELLIPSE,%d,%f,%f,%d,%d,%d,%d,%d,%d,%d",
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
     result = CPD_OK;
    return result;
}

/*
 * Parse "assist_data" element
 */
int cpdXmlParse_location_parameters(xmlDoc *pDoc, xmlNode *pParent, pLOCATION_PARAMETERS pLocationParameters)
{
    int result = CPD_ERROR;
    xmlNode *pNode;

    CPD_LOG(CPD_LOG_ID_TXT , "\n  %u: %s()\n", getMsecTime(), __FUNCTION__);
    LOGD("%u: %s()", getMsecTime(), __FUNCTION__);
    memset(pLocationParameters, 0, sizeof(LOCATION_PARAMETERS));
    if (pParent == NULL) {
        return result;
    }
    pNode = xmlNodeGetNode(pParent, "shape_data");
    if (pNode == NULL) {
        CPD_LOG(CPD_LOG_ID_TXT, "\n !!! %s->shape_data not present !!!",pParent->name);
        return result;
    }
    pNode = xmlNodeGetChild(pNode, NULL);
    if (pNode == NULL) {
        CPD_LOG(CPD_LOG_ID_TXT, "\n !!! shape_data has no children !!!");
        return result;
    }
    CPD_LOG(CPD_LOG_ID_TXT, "->%s\n",pNode->name);
    pLocationParameters->shape_type = xmlStringTo3GPP_shape_type(pNode->name);
    CPD_LOG(CPD_LOG_ID_TXT, "\nshape_type=%d", pLocationParameters->shape_type);
    switch (pLocationParameters->shape_type) {
        case SHAPE_TYPE_POINT:
            break;
        case SHAPE_TYPE_POINT_UNCERT_CIRCLE:
            break;
        case SHAPE_TYPE_POINT_UNCERT_ELLIPSE:
            break;
        case SHAPE_TYPE_POLYGON:
            break;
        case SHAPE_TYPE_POINT_ALT:
            break;
        case SHAPE_TYPE_POINT_ALT_UNCERT_ELLIPSE:
            result = cpdXmlParse_ellipsoid_point_alt_uncertellipse(pDoc, pNode, &(pLocationParameters->shape_data.point_alt_uncertellipse));
            break;
        case SHAPE_TYPE_ARC:
            break;
        default:
            result = CPD_ERROR;
            break;
    }
    return result;
}



/*
 * Parse "nav_model_elem(" element
 */
int cpdXmlParse_nav_model_elem(xmlDoc *pDoc, xmlNode *pParent, pGPS_ASSIST pGPSassist)
{
    int result = CPD_ERROR;
    xmlNode *pNode;
    xmlNode *pNodeNvm;
    xmlChar *pS;
    pNAV_MODEL_ELEM pNavModelElem;

    if (pParent == NULL) {
        return result;
    }
    if (pGPSassist == NULL) {
        return result;
    }
    pNodeNvm = pParent;

    LOGD("%s(%d), %s", __FUNCTION__,
        pGPSassist->nav_model_elem_arr_items,
        pParent->name);
    CPD_LOG(CPD_LOG_ID_TXT ,"\n %s(%d), %s\n", __FUNCTION__,
        pGPSassist->nav_model_elem_arr_items,
        pParent->name);

    while ((pNodeNvm != NULL) && (pGPSassist->nav_model_elem_arr_items < GPS_MAX_N_SVS)) {
        if (pNodeNvm->type == XML_ELEMENT_NODE) {
            if (xmlStrcmp(pNodeNvm->name, (xmlChar *) "nav_model_elem") == 0) {
                pNavModelElem = &(pGPSassist->nav_model_elem_arr[pGPSassist->nav_model_elem_arr_items]);
                memset(pNavModelElem, 0, sizeof(NAV_MODEL_ELEM));

                pNode = xmlNodeGetNode(pNodeNvm, "l2_code");
                xmlNodeGetInt(pNode, &(pNavModelElem->ephem_and_clock.l2_code));
                pNode = xmlNodeGetNode(pNodeNvm, "ura");
                xmlNodeGetInt(pNode, &(pNavModelElem->ephem_and_clock.ura));
                pNode = xmlNodeGetNode(pNodeNvm, "sv_health");
                xmlNodeGetInt(pNode, &(pNavModelElem->ephem_and_clock.sv_health));
                pNode = xmlNodeGetNode(pNodeNvm, "iodc");
                xmlNodeGetInt(pNode, &(pNavModelElem->ephem_and_clock.iodc));
                pNode = xmlNodeGetNode(pNodeNvm, "l2p_flag");
                xmlNodeGetInt(pNode, &(pNavModelElem->ephem_and_clock.l2p_flag));
                pNode = xmlNodeGetNode(pNodeNvm, "esr1");
                xmlNodeGetInt(pNode, &(pNavModelElem->ephem_and_clock.esr1));
                pNode = xmlNodeGetNode(pNodeNvm, "esr2");
                xmlNodeGetInt(pNode, &(pNavModelElem->ephem_and_clock.esr2));
                pNode = xmlNodeGetNode(pNodeNvm, "esr3");
                xmlNodeGetInt(pNode, &(pNavModelElem->ephem_and_clock.esr3));
                pNode = xmlNodeGetNode(pNodeNvm, "esr4");
                xmlNodeGetInt(pNode, &(pNavModelElem->ephem_and_clock.esr4));
                pNode = xmlNodeGetNode(pNodeNvm, "tgd");
                xmlNodeGetInt(pNode, &(pNavModelElem->ephem_and_clock.tgd));
                pNode = xmlNodeGetNode(pNodeNvm, "toc");
                xmlNodeGetInt(pNode, &(pNavModelElem->ephem_and_clock.toc));
                pNode = xmlNodeGetNode(pNodeNvm, "af2");
                xmlNodeGetInt(pNode, &(pNavModelElem->ephem_and_clock.af2));
                pNode = xmlNodeGetNode(pNodeNvm, "af1");
                xmlNodeGetInt(pNode, &(pNavModelElem->ephem_and_clock.af1));
                pNode = xmlNodeGetNode(pNodeNvm, "af0");
                xmlNodeGetInt(pNode, &(pNavModelElem->ephem_and_clock.af0));
                pNode = xmlNodeGetNode(pNodeNvm, "crs");
                xmlNodeGetInt(pNode, &(pNavModelElem->ephem_and_clock.crs));
                pNode = xmlNodeGetNode(pNodeNvm, "delta_n");
                xmlNodeGetInt(pNode, &(pNavModelElem->ephem_and_clock.delta_n));
                pNode = xmlNodeGetNode(pNodeNvm, "m0");
                xmlNodeGetInt(pNode, &(pNavModelElem->ephem_and_clock.m0));
                pNode = xmlNodeGetNode(pNodeNvm, "cuc");
                xmlNodeGetInt(pNode, &(pNavModelElem->ephem_and_clock.cuc));
                pNode = xmlNodeGetNode(pNodeNvm, "ecc");
                xmlNodeGetInt(pNode, &(pNavModelElem->ephem_and_clock.ecc));
                pNode = xmlNodeGetNode(pNodeNvm, "cus");
                xmlNodeGetInt(pNode, &(pNavModelElem->ephem_and_clock.cus));
                pNode = xmlNodeGetNode(pNodeNvm, "power_half");
                xmlNodeGetLong(pNode, &(pNavModelElem->ephem_and_clock.power_half));
                pNode = xmlNodeGetNode(pNodeNvm, "toe");
                xmlNodeGetInt(pNode, &(pNavModelElem->ephem_and_clock.toe));
                pNode = xmlNodeGetNode(pNodeNvm, "fit_flag");
                xmlNodeGetInt(pNode, &(pNavModelElem->ephem_and_clock.fit_flag));
                pNode = xmlNodeGetNode(pNodeNvm, "aoda");
                xmlNodeGetInt(pNode, &(pNavModelElem->ephem_and_clock.aoda));
                pNode = xmlNodeGetNode(pNodeNvm, "cic");
                xmlNodeGetInt(pNode, &(pNavModelElem->ephem_and_clock.cic));
                pNode = xmlNodeGetNode(pNodeNvm, "omega0");
                xmlNodeGetInt(pNode, &(pNavModelElem->ephem_and_clock.omega0));
                pNode = xmlNodeGetNode(pNodeNvm, "cis");
                xmlNodeGetInt(pNode, &(pNavModelElem->ephem_and_clock.cis));
                pNode = xmlNodeGetNode(pNodeNvm, "i0");
                xmlNodeGetInt(pNode, &(pNavModelElem->ephem_and_clock.i0));
                pNode = xmlNodeGetNode(pNodeNvm, "crc");
                xmlNodeGetInt(pNode, &(pNavModelElem->ephem_and_clock.crc));
                pNode = xmlNodeGetNode(pNodeNvm, "omega");
                xmlNodeGetInt(pNode, &(pNavModelElem->ephem_and_clock.omega));
                pNode = xmlNodeGetNode(pNodeNvm, "omega_dot");
                xmlNodeGetLong(pNode, &(pNavModelElem->ephem_and_clock.omega_dot));
                pNode = xmlNodeGetNode(pNodeNvm, "idot");
                xmlNodeGetInt(pNode, &(pNavModelElem->ephem_and_clock.idot));

                pNode = xmlNodeGetNode(pNodeNvm, "sat_id");
                if (pNode != NULL) {
                    xmlNodeGetInt(pNode, &(pNavModelElem->sat_id));
                    pNavModelElem->sat_id++;

                    pS = xmlNodeGetChildProperty(pNodeNvm, "sat_status", "literal");
                    pNavModelElem->sat_status = xmlStringTo3GPP_nav_elem_sat_status(pS);
                }

                CPD_LOG(CPD_LOG_ID_TXT , "\r\n NAV_MODEL_ELEM,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
                    pGPSassist->nav_model_elem_arr_items,
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

                pGPSassist->nav_model_elem_arr_items++;
                result = CPD_OK;

            }
        }
        pNodeNvm = pNodeNvm->next;
    }
    result = CPD_OK;
    return result;
}



/*
 * Parse "assist_data" element
 */
int cpdXmlParse_assist_data(xmlDoc *pDoc, xmlNode *pParent, pASSIST_DATA pAssistData)
{
    int result = CPD_ERROR;
    xmlNode *pNode;

    if (pAssistData->flag == 0) {
        memset(pAssistData, 0, sizeof(ASSIST_DATA));
    }

    if (pParent == NULL) {
        return result;
    }

    pNode = xmlNodeGetNode(pParent, "location_parameters");
    if (pNode != NULL) {
        result = cpdXmlParse_location_parameters(pDoc, pNode, &(pAssistData->GPS_assist.location_parameters));
        if (result != CPD_ERROR) {
            pAssistData->flag++;
        }
    }

    pNode = xmlNodeGetNode(pParent, "nav_model_elem");
    if (pNode != NULL) {
        result = cpdXmlParse_nav_model_elem(pDoc, pNode, &(pAssistData->GPS_assist));
        if (result != CPD_ERROR) {
            pAssistData->flag++;
        }
    }

    pNode = xmlNodeGetNode(pParent, "ref_time");
    if (pNode != NULL) {
        result = cpdXmlParse_ref_time(pDoc, pNode, &(pAssistData->GPS_assist.ref_time));
        if (result != CPD_ERROR) {
            pAssistData->flag++;
        }
    }
    CPD_LOG(CPD_LOG_ID_TXT, "\r\n pAssistData->flag = %d ", pAssistData->flag);

    return result;
}



/*
 * Parse "RRLP_meas" message
 */
static int cpdXmlParse_RRLP_meas(xmlDoc *pDoc, xmlNode *pStartNode, pRRLP_MEAS pRrlpMeas)
{
    int result = CPD_ERROR;
    xmlNode *pNode;
    xmlNode *pN;
    xmlChar *pS;


    CPD_LOG(CPD_LOG_ID_TXT, "\n  %u: %s()\n", getMsecTime(), __FUNCTION__);
    LOGD("%u: %s()", getMsecTime(), __FUNCTION__);
    pNode = xmlNodeGetChild(pStartNode, "RRLP_pos_instruct");
    if (pNode == NULL) {
        CPD_LOG(CPD_LOG_ID_TXT,"Can't find RRLP_pos_instruct\n");
        return result;
    }
    pS = xmlNodeGetChildProperty(pNode, "RRLP_method", "literal");
    pRrlpMeas->RRLP_method = cpdXmlStringTo_rrlp_method(pS);
    xmlFree(pS);

    pS = xmlNodeGetChildString(pDoc, pNode, "resp_time_seconds");
    pRrlpMeas->resp_time_seconds = xmlStringToInt(pS);

    pS = xmlNodeGetChildProperty(pNode, "mult_sets", "literal");
    pRrlpMeas->mult_sets = cposXmlStringTo_mult_sets(pS);
    xmlFree(pS);

    pNode = xmlNodeGetChild(pNode, "RRLP_method_type");
    if (pNode == NULL) {
        CPD_LOG(CPD_LOG_ID_TXT, "Can't find RRLP_method_type\n");
        return result;
    }

    pN = xmlNodeGetChild(pNode, NULL);
    if (pN == NULL) {
        CPD_LOG(CPD_LOG_ID_TXT, "Can't find method_type\n");
        return result;
    }
    pRrlpMeas->method_type = cpdXmlStrTo_gpp_method_type(pN->name);

    pN = xmlNodeGetChild(pN, "method_accuracy");
    if (pN == NULL) {
        CPD_LOG(CPD_LOG_ID_TXT, "Can't find method_accuracy\n");
        return result;
    }
    pRrlpMeas->accurancy = xmlStringToInt(xmlNodeGetChildString(pDoc, pN, "uncertainty"));
    pRrlpMeas->accurancy = cpdConvert3GPPHorizontalAccuracyToM(pRrlpMeas->accurancy);

    CPD_LOG(CPD_LOG_ID_TXT , "\r\nRRLP_MEAS,%d, %d, %d, %d, %d\n",
        pRrlpMeas->method_type,
        pRrlpMeas->accurancy,
        pRrlpMeas->RRLP_method,
        pRrlpMeas->resp_time_seconds,
        pRrlpMeas->mult_sets
        );
    LOGD("RRLP_MEAS request,%d, %d, %d, %d, %d",
        pRrlpMeas->method_type,
        pRrlpMeas->accurancy,
        pRrlpMeas->RRLP_method,
        pRrlpMeas->resp_time_seconds,
        pRrlpMeas->mult_sets
        );
    result = CPD_OK;
    return result;
}

/*
 * Parse "RRC_meas" message
 */
static int cpdXmlParse_RRC_meas(xmlDoc *pDoc, xmlNode *pStartNode, pRRC_MEAS pRrcMeas)
{
    int result = CPD_ERROR;
    xmlNode *pNode;
    xmlNode *pN;
    xmlChar *pS;


    CPD_LOG(CPD_LOG_ID_TXT, "\n  %u: %s()\n", getMsecTime(), __FUNCTION__);
    LOGD("%u: %s()", getMsecTime(), __FUNCTION__);
    pNode = xmlNodeGetChild(pStartNode, "rep_quant");
    if (pNode == NULL) {
        CPD_LOG(CPD_LOG_ID_TXT,"Can't find RRC_meas/rep_quant\n");
    }
    pS = xmlNodeGetChildProperty(pStartNode, "rep_quant", "gps_timing_of_cell_wanted");
    pRrcMeas->rep_quant.gps_timing_of_cell_wanted = xmlStringToBool(pS);

    pS = xmlNodeGetChildProperty(pStartNode, "rep_quant", "addl_assist_data_req");
    pRrcMeas->rep_quant.addl_assist_data_req = xmlStringToBool(pS);

    pS = xmlNodeGetChildProperty(pStartNode, "RRC_method_type", "literal");
    pRrcMeas->rep_quant.RRC_method_type = cpdXmlStrTo_gpp_method_type(pS);
    xmlFree(pS);

    pS = xmlNodeGetChildProperty(pStartNode, "RRC_method", "literal");
    pRrcMeas->rep_quant.RRC_method = cpdXmlStringTo_rrlp_method(pS);
    xmlFree(pS);

    pNode = xmlNodeGetNode(pStartNode, "hor_acc");
    xmlNodeGetInt(pNode, &(pRrcMeas->rep_quant.hor_acc));
    pRrcMeas->rep_quant.hor_acc = cpdConvert3GPPHorizontalAccuracyToM(pRrcMeas->rep_quant.hor_acc);

    pS = xmlNodeGetChildProperty(pStartNode, "period_rep_crit", "rep_amount");
    pRrcMeas->rep_crit.period_rep_crit.rep_amount = xmlStringTo3GPP_rep_amount(pS);

    pS = xmlNodeGetChildProperty(pStartNode, "period_rep_crit", "rep_interval_long");
    pRrcMeas->rep_crit.period_rep_crit.rep_interval_long = xmlStringTo3GPP_rep_interval_long(pS);

    CPD_LOG(CPD_LOG_ID_TXT , "\r\nRRC_MEAS,%d, %d, %d, %d, %d, %d, %d\n",
        pRrcMeas->rep_quant.gps_timing_of_cell_wanted,
        pRrcMeas->rep_quant.addl_assist_data_req,
        pRrcMeas->rep_quant.RRC_method_type,
        pRrcMeas->rep_quant.RRC_method,
        pRrcMeas->rep_quant.hor_acc,
        pRrcMeas->rep_crit.period_rep_crit.rep_amount,
        pRrcMeas->rep_crit.period_rep_crit.rep_interval_long
        );
    LOGD("RRC_MEAS request,%d, %d, %d, %d, %d, %d, %d",
        pRrcMeas->rep_quant.gps_timing_of_cell_wanted,
        pRrcMeas->rep_quant.addl_assist_data_req,
        pRrcMeas->rep_quant.RRC_method_type,
        pRrcMeas->rep_quant.RRC_method,
        pRrcMeas->rep_quant.hor_acc,
        pRrcMeas->rep_crit.period_rep_crit.rep_amount,
        pRrcMeas->rep_crit.period_rep_crit.rep_interval_long
        );
    result = CPD_OK;
    return result;
}

/*
 * Parse "pos_meas" XML element
 */
static int cpdXmlParse_pos_meas(pCPD_CONTEXT pCpd, xmlDoc *pDoc, xmlNode *pParent)
{
    int result = CPD_ERROR;
    xmlNode *pNode;
    pPOS_MEAS pPosMeas;

    pPosMeas = &(pCpd->request.posMeas);

    CPD_LOG(CPD_LOG_ID_TXT, "\n  %u: %s()\n", getMsecTime(), __FUNCTION__);
    LOGD("%u: %s()", getMsecTime(), __FUNCTION__);
    pPosMeas->flag = POS_MEAS_NONE;

    if (pParent == NULL) {
        return result;
    }

    /* meas_abort */
    pNode = xmlNodeGetChild(pParent, "meas_abort");
    if (pNode != NULL) {
        pPosMeas->flag = POS_MEAS_ABORT;
        CPD_LOG(CPD_LOG_ID_TXT , "\r\nr\n !!! parsed meas_abort !!!\n");
        result = CPD_OK;
    }

    /* RRLP_meas */
    pNode = xmlNodeGetChild(pParent, "RRLP_meas");
    if (pNode != NULL) {
        memset(&(pPosMeas->posMeas_u.rrlp_meas), 0, sizeof(RRLP_MEAS));
        result = cpdXmlParse_RRLP_meas(pDoc, pNode, &(pPosMeas->posMeas_u.rrlp_meas));
        CPD_LOG(CPD_LOG_ID_TXT , "\r\n\n +++ parsed cpdXmlParse_RRLP_meas() = %d +++\n", result);
        if (result == CPD_OK) {
            if (pPosMeas->posMeas_u.rrlp_meas.RRLP_method == RRLP_METHOD_GPS) {
                pPosMeas->flag = POS_MEAS_RRLP;
                pCpd->request.rs.method_type = pPosMeas->posMeas_u.rrlp_meas.method_type;
                pCpd->request.rs.rep_resp_time_seconds = pPosMeas->posMeas_u.rrlp_meas.resp_time_seconds;
                pCpd->request.rs.rep_hor_acc = pPosMeas->posMeas_u.rrlp_meas.accurancy;
                pCpd->request.rs.rep_amount = pPosMeas->posMeas_u.rrlp_meas.mult_sets;
                pCpd->request.rs.rep_vert_accuracy = 0;
                pCpd->request.rs.rep_interval_seconds = 1;
            }
        }
    }

    /* RRC_meas */
    pNode = xmlNodeGetChild(pParent, "RRC_meas");
    if (pNode != NULL) {
        memset(&(pPosMeas->posMeas_u.rrc_meas), 0, sizeof(RRC_MEAS));
        result = cpdXmlParse_RRC_meas(pDoc, pNode, &(pPosMeas->posMeas_u.rrc_meas));
        CPD_LOG(CPD_LOG_ID_TXT , "\r\n\n +++ parsed cpdXmlParse_RRC_meas() = %d +++\n", result);
        if (result == CPD_OK) {
            pPosMeas->flag = POS_MEAS_RRC;
            pCpd->request.rs.method_type = pPosMeas->posMeas_u.rrc_meas.method_type;
            pCpd->request.rs.rep_resp_time_seconds = pPosMeas->posMeas_u.rrc_meas.rep_crit.period_rep_crit.rep_interval_long;
            pCpd->request.rs.rep_hor_acc = pPosMeas->posMeas_u.rrc_meas.rep_quant.hor_acc;
            pCpd->request.rs.rep_amount = pPosMeas->posMeas_u.rrc_meas.rep_crit.period_rep_crit.rep_amount;
            pCpd->request.rs.rep_vert_accuracy = pPosMeas->posMeas_u.rrc_meas.rep_quant.vert_accuracy;
            if (pCpd->request.rs.rep_amount == 0) {
                pCpd->request.rs.rep_interval_seconds = pPosMeas->posMeas_u.rrc_meas.rep_crit.period_rep_crit.rep_interval_long;
                pCpd->request.rs.rep_resp_time_seconds = 16;
            }
            else {
                pCpd->request.rs.rep_interval_seconds = 1;
            }
        }
    }
    CPD_LOG(CPD_LOG_ID_TXT , "pPosMeas->flag=%d, result=%d\n", pPosMeas->flag, result);
    return result;
}

/*
 * Converts string buffer into XML document and parses it.
 */
static int cpdXmlParseDoc(pCPD_CONTEXT pCpd, char *pB, int len)
{
    int result = CPD_ERROR;
    int ret;
    xmlDocPtr pDoc;
    xmlNode *pNode, *pRoot;
    POS_MEAS posMeas;

//    char msg[128];

    /* check if xml buffer contains stale data */
    cpdClearOldXmlData(&(pCpd->xmlRxBuffer));


    result = cpdAddTextToXmlBuffer(pCpd, pB, len);
    if (result == len) {
        result = CPD_OK;
    }

    if (cpdIsXmlClosed(pCpd->xmlRxBuffer.pXmlBuffer, pCpd->xmlRxBuffer.xmlBufferIndex) != CPD_OK) {
        CPD_LOG(CPD_LOG_ID_TXT,"\nXML not closed yet");
        return result;
    }

    /*
         * The document in memory - it has no base per RFC 2396,
         * "noname.xml" argument will serve as its base.
         */

    pDoc = xmlReadMemory(pCpd->xmlRxBuffer.pXmlBuffer, pCpd->xmlRxBuffer.xmlBufferIndex, "noname.xml", NULL, 0);
    if (pDoc == NULL) {
        CPD_LOG(CPD_LOG_ID_TXT,"\nFailed to read document:");
        CPD_LOG(CPD_LOG_ID_TXT,"[\r\n");
        CPD_LOG_DATA(CPD_LOG_ID_TXT, pCpd->xmlRxBuffer.pXmlBuffer, pCpd->xmlRxBuffer.xmlBufferIndex);
        CPD_LOG(CPD_LOG_ID_TXT,"]\r\n");
        return result;
    }

    /* Check if it is a valid XML document */
    pRoot = xmlDocGetRootElement(pDoc);
    if (pRoot == NULL) {
        CPD_LOG(CPD_LOG_ID_TXT,"\nXML Failed to get the document root");
        CPD_LOG(CPD_LOG_ID_TXT,"[\r\n");
        CPD_LOG_DATA(CPD_LOG_ID_TXT, pCpd->xmlRxBuffer.pXmlBuffer, pCpd->xmlRxBuffer.xmlBufferIndex);
        CPD_LOG(CPD_LOG_ID_TXT,"]\r\n");
        xmlFreeDoc(pDoc);
        return result;
    }

    /* Check if it is a valid document type => Should start with <pos> */
    if (xmlStrcmp(pRoot->name, (const xmlChar *) CPOSR_POS_ELEMENT)) {
        CPD_LOG(CPD_LOG_ID_TXT,"XML root node != pos (%s)\n", (char *) pRoot->name);
        CPD_LOG(CPD_LOG_ID_TXT,"[\r\n");
        CPD_LOG_DATA(CPD_LOG_ID_TXT, pCpd->xmlRxBuffer.pXmlBuffer, pCpd->xmlRxBuffer.xmlBufferIndex);
        CPD_LOG(CPD_LOG_ID_TXT,"]\r\n");
        xmlFreeDoc(pDoc);
        return result;
    }

    CPD_LOG(CPD_LOG_ID_TXT | CPD_LOG_ID_XML_RX,"\nXML OK:");
    CPD_LOG(CPD_LOG_ID_TXT | CPD_LOG_ID_XML_RX,"[\r\n");
    CPD_LOG_DATA(CPD_LOG_ID_TXT | CPD_LOG_ID_XML_RX, pCpd->xmlRxBuffer.pXmlBuffer, pCpd->xmlRxBuffer.xmlBufferIndex);
    CPD_LOG(CPD_LOG_ID_TXT | CPD_LOG_ID_XML_RX ,"]\r\n===== clearing XML Rx buffer =====\r\n\r\n");

    /* OK, got full XML, do not need data in the buffer any more */
    pCpd->xmlRxBuffer.pXmlBuffer[0] = 0;
    pCpd->xmlRxBuffer.xmlBufferIndex = 0;
    pCpd->xmlRxBuffer.lastUpdate = 0;

    pNode = xmlNodeGetChild(pRoot, NULL);

    if (pNode != NULL) {
        pCpd->modemInfo.receivedCPOSRat = getMsecTime();
        pCpd->modemInfo.processingCPOSRat = 0;
        pCpd->modemInfo.sendingCPOSat = 0;
        if (!xmlStrcmp(pNode->name, (const xmlChar *) CPOSR_LOCATION_ELEMENT)) {
            CPD_LOG(CPD_LOG_ID_TXT, "\r\n !!! Decode for %s not handled yet\n", (char *)pNode->name);
        }
        else if (!xmlStrcmp(pNode->name, (const xmlChar *) CPOSR_ASSIST_DATA_ELEMENT)) {
            pCpd->systemMonitor.processingRequest = CPD_OK; /* disable power management during request processing */
            ret = cpdXmlParse_assist_data(pDoc, pNode, &(pCpd->request.assist_data));
            if (ret == CPD_OK) {
                pCpd->request.flag = REQUEST_FLAG_ASSIST_DATA;
            }
        }
        else if (!xmlStrcmp(pNode->name, (const xmlChar *) CPOSR_POS_MEAS_ELEMENT)) {
            pCpd->systemMonitor.processingRequest = CPD_OK; /* disable power management during request processing */
            ret = cpdXmlParse_pos_meas(pCpd, pDoc, pNode);
            if (ret == CPD_OK) {
                pCpd->request.flag = REQUEST_FLAG_POS_MEAS;
            }
        }
        else if (!xmlStrcmp(pNode->name, (const xmlChar *) CPOSR_GPS_MEAS_ELEMENT)) {
            CPD_LOG(CPD_LOG_ID_TXT, "\r\n !!! Decode for %s not handled yet\n", (char *)pNode->name);
        }
        else if (!xmlStrcmp(pNode->name, (const xmlChar *) CPOSR_GPS_ASSIST_REQ_ELEMENT)) {
            CPD_LOG(CPD_LOG_ID_TXT, "\r\n !!! Decode for %s not handled yet\n", (char *)pNode->name);
        }
        else if (!xmlStrcmp(pNode->name, (const xmlChar *) CPOSR_MSG_ELEMENT)) {
            CPD_LOG(CPD_LOG_ID_TXT, "\r\n !!! Decode for %s not handled yet\n", (char *)pNode->name);
        }
        else if (!xmlStrcmp(pNode->name, (const xmlChar *) CPOSR_POS_ERR_ELEMENT)) {
            CPD_LOG(CPD_LOG_ID_TXT, "\r\n !!! Decode for %s not handled yet\n", (char *)pNode->name);
        }
        else {
            CPD_LOG(CPD_LOG_ID_TXT, "\r\n !!! Invalid XML format (Unknown command : %s)!\n", (char *)pNode->name);
        }
    }
    xmlFreeDoc(pDoc);
    pDoc = NULL;
    pCpd->modemInfo.receivingXml = CPD_NOK;
    if (pCpd->request.flag == REQUEST_FLAG_POS_MEAS) {
        if (pCpd->request.posMeas.flag != POS_MEAS_NONE) {
            if (pCpd->request.posMeas.flag == POS_MEAS_ABORT) {
                pCpd->request.dbgStats.posAbortId++;
                pCpd->request.status.stopSentToGpsAt = getMsecTime();
                pCpd->systemMonitor.processingRequest = CPD_NOK;
                pCpd->activeMonitor.processingRequest = CPD_NOK;
            }
            if ((pCpd->request.posMeas.flag == POS_MEAS_RRLP) || (pCpd->request.posMeas.flag == POS_MEAS_RRC)) {
                pCpd->request.dbgStats.posRequestId++;
                cpdSystemActiveMonitorStart();
//                cpdCloseSystemPowerState(pCpd);
            }
            pCpd->modemInfo.sentCPOSok = CPD_NOK;
/*            cpdLogRequestParametersInXmlParser_t(pCpd);    */ /* debug printout TODO: remove after it's not needed any more */
            pCpd->modemInfo.processingCPOSRat = getMsecTime();
            pCpd->request.status.requestReceivedAt = getMsecTime();
            pCpd->request.status.responseFromGpsReceivedAt = 0;
            pCpd->request.status.responseSentToModemAt = 0;
            pCpd->request.status.stopSentToGpsAt = 0;
            pCpd->request.status.nResponsesSent = 0;
            if (pCpd->pfCposrMessageHandlerInCpd != NULL) {
                pCpd->request.dbgStats.posRequestedByNetwork = 0;
                pCpd->request.dbgStats.posRequestedFromGps = 0;
                pCpd->request.dbgStats.posReceivedFromGps = 0;
                pCpd->request.dbgStats.posReceivedFromGps1 = 0;
                pCpd->request.dbgStats.posRequestedByNetwork = getMsecTime();
                pCpd->pfCposrMessageHandlerInCpd(pCpd);
            }
        }
    }
    return result;
}


int cpdXmlParse(pCPD_CONTEXT pCpd, char *pB, int len)
{
    int result = -1;
    /*
         * this initialize the library and check potential ABI mismatches
         * between the version it was compiled for and the actual shared
         * library used.
         */
    LIBXML_TEST_VERSION


    result = cpdXmlParseDoc(pCpd, pB, len);
    /*
         * Cleanup function for the XML library.
         */
    xmlCleanupParser();
    /*
         * this is to debug memory for regression tests
         xmlMemoryDump();
         */
    return result;
}

