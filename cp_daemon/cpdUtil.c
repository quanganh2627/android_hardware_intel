/*
 *  hardware/Intel/cp_daemon/cpdUtil.c
 *
 * General utilities for CPDD.
 *
 * Martin Junkar 09/18/2011
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <utime.h>
#include <sys/time.h>
#include <time.h>

static struct timeval start_time;

void initTime(void)
{
    gettimeofday(&start_time, NULL);
}

unsigned int get_msec_time_now(void)
{
    struct timeval t;
    unsigned int tMsec;
    gettimeofday(&t, NULL);
    tMsec = (t.tv_sec) * 1000 + (t.tv_usec)/1000;
    return tMsec;
}

unsigned int getMsecTime(void)
{
    struct timeval t;
    unsigned int tMsec;
    gettimeofday(&t, NULL);
    tMsec = (t.tv_sec - start_time.tv_sec) * 1000 + (t.tv_usec - start_time.tv_usec)/1000;
    return tMsec;
}

int getTimeString(char *pS, int len)
{
    struct tm local;
    struct timespec ts;

    pS[0] = 0;
    if ( clock_gettime( CLOCK_REALTIME, &ts ) == 0 ) {
        localtime_r(&ts.tv_sec, &local);
        snprintf(pS, len, "<%02d:%02d:%02d:%02d.%06u>:",
            local.tm_mday,
            local.tm_hour, local.tm_min, local.tm_sec, (unsigned int) (ts.tv_nsec/1000UL));
        return strlen(pS);
    }
    return 0;
}


unsigned int getMsecDt(unsigned int t)
{
    unsigned int tNow = getMsecTime();
    if (t > tNow){
        return t - tNow;
    }
    return tNow - t;
}

int cpdMoveBufferLeft(char *pB, int *pIndex, int left)
{
    if (left < 0) {
        return *pIndex;
    }

    if (left > *pIndex) {
        left = *pIndex;
    }
    memmove(pB, (pB + left), *pIndex);
    *pIndex = *pIndex - left;

    return *pIndex;
}

/*
 * used in debug mode, reading console input
 */
int readUserChoice(void)
{
    int choice;
    char response[10];

    do {
        fgets(response, 9, stdin);
    }while ((sscanf(response, "%d", &choice) != 1) );
    return choice;
}

/*
 * find a string of bytes in a buffer of sub-bffer.
 * similar to strstr() but it handles special cases used in CPD.
 */
int cpdFindString(char *pB, int len, char *findMe, int lenStr)
{
    int result = -1;
    char *pS = NULL;
    char *pC = NULL;
    char *pN = NULL;
    int i = 0;

    if (len <= 0) {
        return result;
    }
    if (pB == NULL) {
        return result;
    }
    pN = pB;
    while ((result < 0) && (i < len)) {
        while (i < len) {
            if (pN[i] == findMe[0]) {
                pC = &(pN[i]);
                break;
            }
            i++;
        }

        if (pC != NULL) {
            pS = strstr(pC, findMe);
            if (pS != NULL){
                result = (int) (pS - pB);
                if (result > len) {
                    result = -1;
                }
                else {
                    break;
                }
            }
        }
        else {
            break;
        }
        i++;
    }
    return result;
}

/*
 * Create & start a thread, initialize variables and wait fdor thread to confirm it is running.
 */
int cpdCreateThread()
{
    int result = 0;
/*  result = pthread_create(&(pCpd->modemInfo.modemReadThread), NULL, cpdModemReadThreadLoop, (void *) pCpd);
    usleep(1000);
    if ((result == 0) && (pCpd->modemInfo.modemReadThreadState == THREAD_STATE_RUNNING)) {
        result = 1;
    }
*/
    return result;
}


/*
 * Read gps.conf file and check if logging is enabled.
 */
int cpdParseConfigFile(char *config_file, char* filePath)
{
    FILE *fp = NULL;
    FILE *fp_test = NULL;
    char whitespace[] = "= \t\r\n";
    char line[300];
    char *config_param = NULL;
    char *config_value = NULL;

    int logging_on = -1;
    int parsed_test_file = 0;

    if ((fp = fopen(config_file, "r")) == NULL)
    {
        return(logging_on);
    }
    logging_on = 0;
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        if (line[0] == '#')
        {
         // Skip lines starting with #
            continue;
        }

        config_param = strtok(line, whitespace);
        if (config_param == NULL)
        {
               // Skip bad lines
               continue;
        }

        config_value = strtok(NULL, whitespace);
        if (config_value == NULL)
        {
            continue;
        }

        if (strcmp(config_param, "LOGGING_ON") == 0)
        {
            logging_on = atoi(config_value);
        }
        else if (strcmp(config_param, "CPD_LOG") == 0)
        {
            strcpy(filePath, config_value);
        }
        else if (strcmp(config_param, "TEST_PATH") == 0)
        {
            if (!parsed_test_file)
            {
                fp_test = fopen(config_value, "r");

                if (fp_test == NULL)
                {
                    // Could not open test config file
                    //Continue using current config file
                    if (logging_on < 1) {
                        logging_on = -1;
                    }
                }
                else
                {
                    fclose(fp);
                    fp = fp_test;
                }
                parsed_test_file = 1;
            }
        }
        else
        {
            //"UNKNOWN Config Param
            continue;
        }

    }
    fclose(fp);
    return logging_on;
}

