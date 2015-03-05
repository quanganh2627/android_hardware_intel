/*
 * hardware/Intel/cp_daemon/cpdXmlUtils.h
 *
 * Miscellaneous XML parsing utilities - header file for cpdXmlUtils.c.
 *
 * Martin Junkar 09/18/2011
 *
 */

#ifndef _CPDXMLUTILS_H_
#define _CPDXMLUTILS_H_

char *xmlBoolToString(int value);
int xmlStringToBool(xmlChar *value);
int xmlStringToInt(xmlChar *value);
long xmlStringToLong(xmlChar *value);

int xmlNodeGetInt(xmlNode *pNode, int *pValue);
int xmlNodeGetLong(xmlNode *pNode, long *pValue);
int xmlNodeGetStr(xmlNode *pNode, char *pValue, int);

int xmlStringTo3GPP_meas_interval(xmlChar *value);
int xmlStringTo3GPP_rep_amount(xmlChar *value);
int xmlStringTo3GPP_rep_interval_long(xmlChar *value);
int xmlStringTo3GPP_mult_sets(xmlChar *value);
int xmlStringTo3GPP_msg_status(xmlChar *value);
int xmlStringTo3GPP_rrlp_method(xmlChar *value);
int xmlStringTo3GPP_tr_pos_chg(xmlChar *value);
int xmlStringTo3GPP_tr_SFN_SFN_chg(xmlChar *value);
int xmlStringTo3GPP_tr_SFN_GPS_TOW(xmlChar *value);

int xmlStringTo3GPP_shape_type(const xmlChar *value);
int xmlStringTo3GPP_nav_elem_sat_status(xmlChar *value);
int xmlStringTo3GPP_rep_quant_rrc_method(xmlChar *value);
int xmlStringTo3GPP_dopl1_uncert(xmlChar *value);
int xmlStringTo3GPP_rep_crit(const xmlChar *value);
int xmlStringTo3GPP_method_type(const xmlChar *value);
int xmlStringToErrorReason(xmlChar *value);
int xmlStringToMultiPath(xmlChar *value);
#endif

