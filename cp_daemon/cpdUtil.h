/*
 *  hardware/Intel/cp_daemon/cpdUtil.h
 *
 * General utilities for CPDD - header file for cpdUtil.c
 *
 * Martin Junkar 09/18/2011
 *
 *
 */

#ifndef _CPDUTIL_H_
#define _CPDUTIL_H_
void initTime(void);
unsigned int get_msec_time_now(void);
unsigned int getMsecTime(void);
unsigned int getMsecDt(unsigned int t);
int getTimeString(char *, int );
int cpdMoveBufferLeft(char *pB, int *pIndex, int left);
int readUserChoice(void);
int cpdFindString(char *pB, int len, char *findMe, int lenStr);
int cpdParseConfigFile(char *config_file, char *filePath);
#endif   /* _CPDUTIL_H_ */

