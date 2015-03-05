/** \file event_dev.h
 *  \brief Gets raw Unipoint capacitive values from an evdev node.
 *  \author Praem J. Phulwani
 *  \date 7/30/2010
 */
#ifndef EVENT_DEV_H_
#define EVENT_DEV_H_

#include <stdio.h>
#include <stdint.h>

#ifndef UNIPOINT_DEBUG
#define UNIPOINT_DEBUG 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

int event_dev_open(FILE **stream, const char * path);

void event_dev_close(FILE *stream);

/*! \brief Reads twelve capcitive values from a Unipoint evdev node.
 */
int event_dev_get(FILE *stream, unsigned int * restrict capvals_out);

#ifdef __cplusplus
}
#endif

#endif/*EVENT_DEV_H_*/
