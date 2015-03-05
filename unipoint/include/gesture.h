/** \file gesture.h
 *  \brief A thread in which to perform gesture recognition asynchronously.
 *  \author Venkatesh Srinivas
 *  \date 7/30/2010
 */
#ifndef _GESTURE_H
#define _GESTURE_H

typedef enum gesture_output_type {
    NONE_GESTURE    = 0,
    RIGHT_GESTURE   = 1,
    LEFT_GESTURE    = 2,
    DOWN_GESTURE    = 3,
    UP_GESTURE      = 4,
    TAP_GESTURE     = 5
} gesture_output_type;
#define GESTURE_COUNT 6

#endif
