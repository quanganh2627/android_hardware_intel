#ifndef STATEMACHINE_H
#define STATEMACHINE_H
#include <stdint.h>
#include <linux/uinput.h>
#include "unipoint.h"


#define MASK_RIGHT_GESTURE	    0x1
#define MASK_LEFT_GESTURE	    0x2
#define MASK_DOWN_GESTURE	    0x4
#define MASK_UP_GESTURE		    0x8
#define MASK_TAP_GESTURE		    0x10
#define MASK_TAPHOLD_GESTURE	    0x20
#define MASK_POINTER_GESTURE     0x40
#define MASK_VOLUME_UP           0x80
#define MASK_VOLUME_DOWN         0x100
#define MASK_DOUBLE_TAP		0x200

typedef enum E_MODE{
	MODE_NONE = 0,
	MODE_NORMAL = 1,
	MODE_VOLUME = 2
} MODE; 


//SwtichToNew Mode
void SwtichToNewMode(MODE);


//GetCurrent Mode 
MODE GetCurrentMode();


//Process inside statemachine 
int StateMachine_process(int uinput_fd, gesture_output gest_out,centroid_output cent_out);



#endif /* STATEMACHINE_H */

