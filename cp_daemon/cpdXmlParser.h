/*
 * hardware/Intel/cp_daemon/cpdXmlParser.h
 *
 * Header file for cpdXmlParser.c.
 *
 * Martin Junkar 09/18/2011
 *
 */

#ifndef _CPDXMLPARSER_H_
#define _CPDXMLPARSER_H_
#include "cpd.h"
/* DEBUG: */
void cpdLogRequestParametersInXmlParser_t(pCPD_CONTEXT );
void cpdCreatePositionResponse_t(pCPD_CONTEXT );
/* END DEBUG: */
int cpdXmlParse(pCPD_CONTEXT , char *, int );
#endif  /* _CPDXMLPARSER_H_ */

