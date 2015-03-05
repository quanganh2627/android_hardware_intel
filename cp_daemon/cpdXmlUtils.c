/*
 * hardware/Intel/cp_daemon/cpdXmlUtils.c
 *
 * Miscellaneous XML parsing utilities.
 * Uses xmllib2 for XML parsing.
 *
 * Martin Junkar 09/18/2011
 *
 */

#include <libxml/parser.h>
#include <libxml/tree.h>

#define LOG_TAG "CPDD_XU"

#include "cpd.h"

 /*
  * convert an xml int to bool string (CPD_ERROR if the input is NULL)
  */
 char *xmlBoolToString(int value)
 {
     char *res;
     res = (value) ? "true":"false";
     return res;
 }

 /*
  * 1. convert an xml string from "false"/"true" ---> to 0/1
  * 2. Release the provided xml string
  */
int xmlStringToBool(xmlChar *pValue)
{
    int result = CPD_NOK;

    if (pValue != NULL) {
        result = (xmlStrcmp(pValue, BAD_CAST"true") == 0);

        xmlFree(pValue);
    }
    return result;
}


 /*
  * 1. convert an xml string to integer (CPD_ERROR if the input is NULL)
  * 2. Release the provided xml string
  */
int xmlStringToInt(xmlChar *pValue)
 {
     int res = CPD_ERROR;
     if (pValue != NULL) {
         res = atoi((const char *)pValue);
         xmlFree(pValue);
     }
     return res;
 }

 /*
  * 1. convert an xml string to long (CPD_ERROR if the input is NULL)
  * 2. Release the provided xml string
  */
long xmlStringToLong(xmlChar *value)
{
     long res = CPD_ERROR;
     if (value) {
         res = atol((const char *)value);
         xmlFree(value);
     }

     return res;
}

int xmlNodeGetInt(xmlNode *pNode, int *pValue)
{
    int result = CPD_NOK;
    xmlChar *pNodeValue = NULL;
    if (pNode == NULL) {
        return result;
    }
    if (pValue == NULL) {
        return result;
    }
    pNodeValue = xmlNodeGetContent(pNode);
    if (pNodeValue != NULL) {
        *pValue = atoi((const char *)pNodeValue);
        xmlFree(pNodeValue);
        result = CPD_OK;
    }
    return result;
}

int xmlNodeGetStr(xmlNode *pNode, char *pValue, int maxLen)
{
    int result = CPD_NOK;
    xmlChar *pNodeValue = NULL;
    if (pNode == NULL) {
        return result;
    }
    if (pValue == NULL) {
        return result;
    }
    pNodeValue = xmlNodeGetContent(pNode);
    if (pNodeValue != NULL) {
        strncpy(pValue, (const char *)pNodeValue, maxLen);
        xmlFree(pNodeValue);
        result = CPD_OK;
    }
    return result;
}


 int xmlNodeGetLong(xmlNode *pNode, long *pValue)
 {
     int result = CPD_NOK;
     xmlChar *pNodeValue = NULL;
     if (pNode == NULL) {
         return result;
     }
     if (pValue == NULL) {
         return result;
     }
     pNodeValue = xmlNodeGetContent(pNode);
     if (pNodeValue != NULL) {
         *pValue = atol((const char *)pNodeValue);
         xmlFree(pNodeValue);
         result = CPD_OK;
     }
     return result;
 }

 /*
  * 1. convert an xml string (meas_inerval) to integer
  * 2. Release the provided xml string
  */
int xmlStringTo3GPP_meas_interval(xmlChar *value)
 {
     int result = CPD_ERROR;
     if (value == NULL) {
        return result;
     }

     if (!xmlStrcmp(value, BAD_CAST"e5")) {
         result = 5;
     }
     else if  (!xmlStrcmp(value, BAD_CAST"e15")) {
         result = 15;
     }
     else if  (!xmlStrcmp(value, BAD_CAST"e60")) {
         result = 60;
     }
     else if  (!xmlStrcmp(value, BAD_CAST"e300")) {
         result = 300;
     }
     else if  (!xmlStrcmp(value, BAD_CAST"e900")) {
         result = 900;
     }
     else if  (!xmlStrcmp(value, BAD_CAST"e1800")) {
         result = 1800;
     }
     else if  (!xmlStrcmp(value, BAD_CAST"e3600")) {
         result = 3600;
     }
     else if  (!xmlStrcmp(value, BAD_CAST"e7200")) {
         result = 7200;
     }

     xmlFree(value);
     return result;
 }


 /*
  * 1. convert an xml  string (rep_amount) to integer
  * 2. Release the provided xml string
  */
int xmlStringTo3GPP_rep_amount(xmlChar *value)
 {
     int result = CPD_ERROR;
     if (value == NULL) {
        return result;
     }

     if (!xmlStrcmp(value, BAD_CAST"ra1")) {
         result = 1;
     }
     else if  (!xmlStrcmp(value, BAD_CAST"ra2")) {
         result = 2;
     }
     else if  (!xmlStrcmp(value, BAD_CAST"ra4")) {
         result = 4;
     }
     else if  (!xmlStrcmp(value, BAD_CAST"ra8")) {
         result = 8;
     }
     else if  (!xmlStrcmp(value, BAD_CAST"ra16")) {
         result = 16;
     }
     else if  (!xmlStrcmp(value, BAD_CAST"ra32")) {
         result = 32;
     }
     else if  (!xmlStrcmp(value, BAD_CAST"ra64")) {
         result = 64;
     }
     else if  (!xmlStrcmp(value, BAD_CAST"ra-Infinity")) {
         result = 0;
     }
     xmlFree(value);
     return result;
 }


 /*
  * 1. convert an xml string (rep_interval_long) to integer
  * 2. Release the provided xml string
  */
int xmlStringTo3GPP_rep_interval_long(xmlChar *value)
 {
     int result = CPD_ERROR;
     if (value == NULL) {
        return result;
     }
     if (!xmlStrcmp(value, BAD_CAST"ril0")) {
         result = 0;
     }
     else if  (!xmlStrcmp(value, BAD_CAST"ril0-25")) {
         result = 1;
     }
     else if  (!xmlStrcmp(value, BAD_CAST"ril0-5")) {
         result = 1;
     }
     else if  (!xmlStrcmp(value, BAD_CAST"ril1")) {
         result = 1;
     }
     else if  (!xmlStrcmp(value, BAD_CAST"ril2")) {
         result = 2;
     }
     else if  (!xmlStrcmp(value, BAD_CAST"ril3")) {
         result = 3;
     }
     else if  (!xmlStrcmp(value, BAD_CAST"ril4")) {
         result = 4;
     }
     else if  (!xmlStrcmp(value, BAD_CAST"ril6")) {
         result = 6;
     }
     else if  (!xmlStrcmp(value, BAD_CAST"ril8")) {
         result = 8;
     }
     else if  (!xmlStrcmp(value, BAD_CAST"ril12")) {
         result = 12;
     }
     else if  (!xmlStrcmp(value, BAD_CAST"ril16")) {
         result = 16;
     }
     else if  (!xmlStrcmp(value, BAD_CAST"ril20")) {
         result = 20;
     }
     else if  (!xmlStrcmp(value, BAD_CAST"ril24")) {
         result = 24;
     }
     else if  (!xmlStrcmp(value, BAD_CAST"ril28")) {
         result = 28;
     }
     else if  (!xmlStrcmp(value, BAD_CAST"ril32")) {
         result = 32;
     }
     else if  (!xmlStrcmp(value, BAD_CAST"ril64")) {
         result = 64;
     }

     xmlFree(value);
     return result;
 }


 /*
  * 1. convert an xml  string (mutl_sets) to mutl_sets_t
  * 2. Release the provided xml string
  */
int xmlStringTo3GPP_mult_sets(xmlChar *value)
 {
     int result = CPD_ERROR;
     if (value == NULL) {
        return result;
     }

     if (!xmlStrcmp(value, BAD_CAST"multiple")) {
         result = MULT_SETS_MULTIPLE;
     }
     else if (!xmlStrcmp(value, BAD_CAST"one")) {
         result = MULT_SETS_ONE;
     }

     xmlFree(value);
     return result;
 }

 /*
  * 1. convert an xml  string to msg_status_t
  * 2. Release the provided xml string
  */
int xmlStringTo3GPP_msg_status(xmlChar *value)
 {
     int result = CPD_ERROR;
     if (value == NULL) {
        return result;
     }
     if (!xmlStrcmp(value, BAD_CAST"assist_data_delivered")) {
         result = MSG_STATUS_ASSIST_DATA_DELIVERED;
     }

     xmlFree(value);
     return result;
 }

 /*
  * 1. convert an xml  string (rrlp_method) to rrlp_method_t
  * 2. Release the provided xml string
  */
int xmlStringTo3GPP_rrlp_method(xmlChar *value)
 {
     int result = CPD_ERROR;
     if (value == NULL) {
        return result;
     }
     if (!xmlStrcmp(value, BAD_CAST"gps")) {
         result = RRLP_METHOD_GPS;
     }

     xmlFree(value);
     return result;
 }


 /*
  * 1. convert an xml  string (tr_pos_chg) to tr_pos_chg_t
  * 2. Release the provided xml string
  */
int xmlStringTo3GPP_tr_pos_chg(xmlChar *value)
 {
     int result = CPD_ERROR;
     if (value == NULL) {
        return result;
     }

     if (!xmlStrcmp(value, BAD_CAST"pc10")) {
         result = 10;
     }
     else if (!xmlStrcmp(value, BAD_CAST"pc20")) {
         result = 20;
     }
     else if (!xmlStrcmp(value, BAD_CAST"pc30")) {
         result = 30;
     }
     else if (!xmlStrcmp(value, BAD_CAST"pc40")) {
         result = 40;
     }
     else if (!xmlStrcmp(value, BAD_CAST"pc50")) {
         result = 50;
     }
     else if (!xmlStrcmp(value, BAD_CAST"pc100")) {
         result = 100;
     }
     else if (!xmlStrcmp(value, BAD_CAST"pc200")) {
         result = 200;
     }
     else if (!xmlStrcmp(value, BAD_CAST"pc300")) {
         result = 300;
     }
     else if (!xmlStrcmp(value, BAD_CAST"pc500")) {
         result = 500;
     }
     else if (!xmlStrcmp(value, BAD_CAST"pc1000")) {
         result = 1000;
     }
     else if (!xmlStrcmp(value, BAD_CAST"pc2000")) {
         result = 2000;
     }
     else if (!xmlStrcmp(value, BAD_CAST"pc5000")) {
         result = 5000;
     }
     else if (!xmlStrcmp(value, BAD_CAST"pc10000")) {
         result = 10000;
     }
     else if (!xmlStrcmp(value, BAD_CAST"pc20000")) {
         result = 20000;
     }
     else if (!xmlStrcmp(value, BAD_CAST"pc50000")) {
         result = 50000;
     }
     else if (!xmlStrcmp(value, BAD_CAST"pc100000")) {
         result = 100000;
     }

     xmlFree(value);
     return result;
 }

 /*
  * 1. convert an xml  string (tr_SFN_SFN_chg) to tr_SFN_SFN_chg_t
  * 2. Release the provided xml string
  */
int xmlStringTo3GPP_tr_SFN_SFN_chg(xmlChar *value)
 {
     int res = CPD_ERROR;

     if (!xmlStrcmp(value, BAD_CAST"c0-25")) {
         res = 25;
     }
     else if (!xmlStrcmp(value, BAD_CAST"c0-5")) {
         res = 50;
     }
     else if (!xmlStrcmp(value, BAD_CAST"c1")) {
         res = 1;
     }
     else if (!xmlStrcmp(value, BAD_CAST"c2")) {
         res = 2;
     }
     else if (!xmlStrcmp(value, BAD_CAST"c3")) {
         res = 3;
     }
     else if (!xmlStrcmp(value, BAD_CAST"c4")) {
         res = 4;
     }
     else if (!xmlStrcmp(value, BAD_CAST"c5")) {
         res = 5;
     }
     else if (!xmlStrcmp(value, BAD_CAST"c10")) {
         res = 10;
     }
     else if (!xmlStrcmp(value, BAD_CAST"c20")) {
         res = 20;
     }
     else if (!xmlStrcmp(value, BAD_CAST"c50")) {
         res = 50;
     }
     else if (!xmlStrcmp(value, BAD_CAST"c100")) {
         res = 100;
     }
     else if (!xmlStrcmp(value, BAD_CAST"c200")) {
         res = 200;
     }
     else if (!xmlStrcmp(value, BAD_CAST"c500")) {
         res = 500;
     }
     else if (!xmlStrcmp(value, BAD_CAST"c1000")) {
         res = 1000;
     }
     else if (!xmlStrcmp(value, BAD_CAST"c2000")) {
         res = 2000;
     }
     else if (!xmlStrcmp(value, BAD_CAST"c5000")) {
         res = 5000;
     }

     xmlFree(value);
     return res;
}

 /*
  * 1. convert an xml  string (tr_SFN_GPS_TOW) to tr_SFN_GPS_TOW_t
  * 2. Release the provided xml string
  */
int xmlStringTo3GPP_tr_SFN_GPS_TOW(xmlChar *value)
{
    int res = CPD_ERROR;

    if (!xmlStrcmp(value, BAD_CAST"ms1")) {
        res = 1;
    }
    else if (!xmlStrcmp(value, BAD_CAST"ms2")) {
        res = 2;
    }
    else if (!xmlStrcmp(value, BAD_CAST"ms3")) {
        res = 3;
    }
    else if (!xmlStrcmp(value, BAD_CAST"ms5")) {
        res = 5;
    }
    else if (!xmlStrcmp(value, BAD_CAST"ms10")) {
        res = 10;
    }
    else if (!xmlStrcmp(value, BAD_CAST"ms20")) {
        res = 20;
    }
    else if (!xmlStrcmp(value, BAD_CAST"ms50")) {
        res = 50;
    }
    else if (!xmlStrcmp(value, BAD_CAST"ms100")) {
        res = 100;
    }

    xmlFree(value);
    return res;
}


 /*
  * 1. convert an xml  string to shape_data_type
  */
SHAPE_TYPE_E xmlStringTo3GPP_shape_type(const xmlChar *pValue)
{
    SHAPE_TYPE_E result = SHAPE_TYPE_NONE;
    if (!xmlStrcmp(pValue, (xmlChar *) "ellipsoid_point")) {
        result = SHAPE_TYPE_POINT;
    }
    else if (!xmlStrcmp(pValue, (xmlChar *) "ellipsoid_point_uncert_circle")) {
        result = SHAPE_TYPE_POINT_UNCERT_CIRCLE;
    }
    else if (!xmlStrcmp(pValue, (xmlChar *) "ellipsoid_point_uncert_ellipse")) {
        result = SHAPE_TYPE_POINT_UNCERT_ELLIPSE;
    }
    else if (!xmlStrcmp(pValue, (xmlChar *) "polygon")) {
        result = SHAPE_TYPE_POLYGON;
    }
    else if (!xmlStrcmp(pValue, (xmlChar *) "ellipsoid_point_alt")) {
        result = SHAPE_TYPE_POINT_ALT;
    }
    else if (!xmlStrcmp(pValue, (xmlChar *) "ellipsoid_point_alt_uncertellipse")) {
        result = SHAPE_TYPE_POINT_ALT_UNCERT_ELLIPSE;
    }
    else if (!xmlStrcmp(pValue, (xmlChar *) "ellips_arc")) {
        result = SHAPE_TYPE_ARC;
    }

    return result;
}

 /*
  * 1. convert an xml string to NAV_ELEM_SAT_STATUS_*
  * 2. Release the provided xml string
  */
int xmlStringTo3GPP_nav_elem_sat_status(xmlChar *pValue)
 {
     int result = NAV_ELEM_SAT_STATUS_NONE;

     if (pValue != NULL) {
         if (!xmlStrcmp(pValue, BAD_CAST"NS_NN-U")) {
             result = NAV_ELEM_SAT_STATUS_NS_NN_U;
         }
         else if (!xmlStrcmp(pValue, BAD_CAST"ES_NN-U")) {
             result = NAV_ELEM_SAT_STATUS_ES_NN_U;
         }
         else if (!xmlStrcmp(pValue, BAD_CAST"NS_NN")) {
             result = NAV_ELEM_SAT_STATUS_NS_NN;
         }
         else if (!xmlStrcmp(pValue, BAD_CAST"ES_SN")) {
             result = NAV_ELEM_SAT_STATUS_ES_SN;
         }
         else if (!xmlStrcmp(pValue, BAD_CAST"REVD")) {
             result = NAV_ELEM_SAT_STATUS_REVD;
         }

         xmlFree(pValue);
     }
     return result;
 }

 /*
  * 1. convert an xml string to REP_QUANT_RRC_METHOD_*
  * 2. Release the provided xml string
  */
int xmlStringTo3GPP_rep_quant_rrc_method(xmlChar *value)
 {
     int res = CPD_ERROR;

     if (!xmlStrcmp(value, BAD_CAST"otdoa")) {
         res = REP_QUANT_RRC_METHOD_OTDOA;
     }
     else if (!xmlStrcmp(value, BAD_CAST"gps")) {
         res = REP_QUANT_RRC_METHOD_GPS;
     }
     else if (!xmlStrcmp(value, BAD_CAST"otdoaOrGPS")) {
         res = REP_QUANT_RRC_METHOD_OTDOAORGPS;
     }
     else if (!xmlStrcmp(value, BAD_CAST"cellID")) {
         res = REP_QUANT_RRC_METHOD_CELLID;
     }

     xmlFree(value);
     return res;
 }

 /*
  * 1. convert an xml string to SAT_INFO_DOPL1_UNCERT_*
  * 2. Release the provided xml string
  */
int xmlStringTo3GPP_dopl1_uncert(xmlChar *value)
 {
     int res = CPD_ERROR;

     if (!xmlStrcmp(value, BAD_CAST"hz12-5")) {
         res = SAT_INFO_DOPL1_UNCERT_12_5HZ;
     }
     else if (!xmlStrcmp(value, BAD_CAST"hz25")) {
         res = SAT_INFO_DOPL1_UNCERT_25HZ;
     }
     else if (!xmlStrcmp(value, BAD_CAST"hz50")) {
         res = SAT_INFO_DOPL1_UNCERT_50HZ;
     }
     else if (!xmlStrcmp(value, BAD_CAST"hz100")) {
         res = SAT_INFO_DOPL1_UNCERT_100HZ;
     }
     else if (!xmlStrcmp(value, BAD_CAST"hz200")) {
         res = SAT_INFO_DOPL1_UNCERT_200HZ;
     }

     xmlFree(value);
     return res;
 }

 /*
  * 1. convert an xml string to XML_PARSER_METHOD_TYPE_*
  */
int xmlStringTo3GPP_rep_crit(const xmlChar *value)
 {
     int res = CPD_ERROR;

     if (!xmlStrcmp(value, BAD_CAST"no_rep")) {
         res = REP_CRIT_NONE;
     }
     else if (!xmlStrcmp(value, BAD_CAST"event_rep_crit")) {
         res = REP_CRIT_EVENT;
     }
     else if (!xmlStrcmp(value, BAD_CAST"period_rep_crit")) {
         res = REP_CRIT_PERIOD;
     }

     return res;
 }

 /*
  * 1. convert an xml string to XML_PARSER_METHOD_TYPE_*
  */
GPP_METHOD_TYPE_E xmlStringTo3GPP_method_type(const xmlChar *value)
 {
     int result = GPP_METHOD_TYPE_NONE;

     if ((!xmlStrcmp(value, BAD_CAST"ms_assisted")) ||
         (!xmlStrcmp(value, BAD_CAST"ms_assisted_pref")) ||
         (!xmlStrcmp(value, BAD_CAST"ue_assisted")) ||
         (!xmlStrcmp(value, BAD_CAST"ue_assisted_pref")))
     {
         result = GPP_METHOD_TYPE_MS_ASSISTED;
     }
     else if ((!xmlStrcmp(value, BAD_CAST"ms_based")) ||
              (!xmlStrcmp(value, BAD_CAST"ms_based_pref")) ||
              (!xmlStrcmp(value, BAD_CAST"ue_based")) ||
              (!xmlStrcmp(value, BAD_CAST"ue_based_pref")))
     {
         result = GPP_METHOD_TYPE_MS_BASED;
     }
     else if (!xmlStrcmp(value, BAD_CAST"ms_assisted_no_accuracy")) {
         result = GPP_METHOD_TYPE_MS_ASSISTED_NO_ACCURACY;
     }

     return result;
 }


 /*
  * 1. convert an xml string to SAT_INFO_DOPL1_UNCERT_*
  * 2. Release the provided xml string
  */
int xmlStringToErrorReason(xmlChar *value)
 {
     int res = CPD_ERROR;

     if (!xmlStrcmp(value, BAD_CAST"undefined_error")) {
         res = POS_ERROR_UNDEFINED;
     }
     else if (!xmlStrcmp(value, BAD_CAST"not_enough_gps_satellites")) {
         res = POS_ERROR_NOT_ENOUGH_GPS_SATELLITES;
     }
     else if (!xmlStrcmp(value, BAD_CAST"gps_assist_data_missing")) {
         res = POS_ERROR_NOT_GPS_ASSISTANCE_DATA_MISSING;
     }

     xmlFree(value);
     return res;
 }

 /*
  * 1. convert an xml string to MULTIPATH_*
  * 2. Release the provided xml string
  */
int xmlStringToMultiPath(xmlChar *value)
 {
     int res = CPD_ERROR;

     if (!xmlStrcmp(value, BAD_CAST"not_measured")) {
         res = MULTIPATH_NOT_MEASURED;
     }
     else if (!xmlStrcmp(value, BAD_CAST"low")) {
         res = MULTIPATH_LOW;
     }
     else if (!xmlStrcmp(value, BAD_CAST"medium")) {
         res = MULTIPATH_MEDIUM;
     }
     else if (!xmlStrcmp(value, BAD_CAST"high")) {
         res = MULTIPATH_HIGH;
     }

     xmlFree(value);
     return res;
 }

