#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <poll.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <linux/input.h>
#include <event_handler.h>
#include <event_dev.h>
#include <macros.h>
#include <uinput.h>
#include <gesture.h>


#include "statemachine.h"
#include "event_handler.h"
#define SM_MAX_GESTURES 20

typedef struct s_gesture_out_time {
    gesture_output cent;
    clock_t fireTime;
} Gesture_Out_Time;



static int gHistoryIndex = 0;
static Gesture_Out_Time gHistory[SM_MAX_GESTURES];
static time_t   timep;
extern int statechanged;



#ifndef UNIPOINT_DEBUG
#define UNIPOINT_DEBUG 1
#endif

#define DBUG_OUT(...) \
	if (UNIPOINT_DEBUG) \
		fprintf(stderr, __VA_ARGS__)




static long counter;



extern void event_send_gesture(
    const int gesture_mask,
    centroid_output cent_out,
    const int uinput_fd);



static MODE gCurMode = MODE_NORMAL;
static bool bexceeded = false; // indicate whether the history box index 0 is really 0.


// insert raw input to history for current touch engagement
void gestureHistoryPush(const gesture_output cent)
{



    if(cent.type!=NONE_GESTURE) {

        counter++;

        if(gHistoryIndex == SM_MAX_GESTURES) {
            gHistoryIndex = 0;
            bexceeded = true;

        }

        gHistory[gHistoryIndex].cent= cent; // does this copy
        gHistory[gHistoryIndex].fireTime = clock();
        gHistoryIndex++;

    }
}


void SwtichToNewMode(MODE newmode)
{
    //Thread mutex usage here ?

    gCurMode = newmode;



}


MODE GetCurrentMode()
{
    return gCurMode;

}




//Check whether this is double tap
int IsDoubleTap()
{

    int curentindex = (gHistoryIndex - 1 + SM_MAX_GESTURES) % SM_MAX_GESTURES;
    int preindex = 0;
    int prepreindex = 0;
    int ret = -1;


    DBUG_OUT(" Current Counter = %d ,gHistoryIndex = %d\n",counter,gHistoryIndex);



    if(!bexceeded & gHistoryIndex < 2)
    {


        DBUG_OUT("%s,!bexceeded & gHistoryIndex < 3, return\n ",ctime(&timep));

        //No gestures to determin
        return ;
    }

    preindex = (curentindex - 1 + SM_MAX_GESTURES) % SM_MAX_GESTURES;

    if(gHistory[preindex].cent.type == TAP_GESTURE && gHistory[curentindex].cent.type == TAP_GESTURE )
    {
        int diff = gHistory[curentindex].fireTime - gHistory[preindex].fireTime;

        if(diff < 0.7*CLOCKS_PER_SEC) {

            DBUG_OUT("Double Tap Detected\n");
            return 1;

        }
    }

    return 0;

}



//This function is used to check and do the  actual mode change
void checkAndSwitchMode()
{
    int curentindex = (gHistoryIndex - 1 + SM_MAX_GESTURES) % SM_MAX_GESTURES;
    int preindex = 0;
    int prepreindex = 0;
    int ret = -1;


    DBUG_OUT( " Current Counter = %d ,gHistoryIndex = %d\n",counter,gHistoryIndex);

    if(gCurMode == MODE_NONE)
    {
        DBUG_OUT("Unipoint is disabled, no mode switch allowed from user ");
        return;
    }

    if(!bexceeded & gHistoryIndex < 3)
    {


        DBUG_OUT("%s,!bexceeded & gHistoryIndex < 3, return\n ",ctime(&timep));

        //No gestures to determin
        return ;
    }

    preindex = (curentindex - 1 + SM_MAX_GESTURES) % SM_MAX_GESTURES;;
    prepreindex = (preindex -1 + SM_MAX_GESTURES) % SM_MAX_GESTURES;

    if(gHistory[prepreindex].cent.type == TAP_GESTURE && gHistory[preindex].cent.type == TAP_GESTURE )
    {

        if((gHistory[curentindex].fireTime - gHistory[prepreindex].fireTime )<0.5*CLOCKS_PER_SEC) {
            if(gCurMode == MODE_NORMAL)
            {
                gCurMode = MODE_VOLUME;

            } else if(gCurMode == MODE_VOLUME)
            {
                gCurMode = MODE_NORMAL;

            }


            //find mode switch triggle ,  clear the history box.
            gHistoryIndex = 0;
            bexceeded = false;

            DBUG_OUT("%s, MODE_NORMAL, MODE CHANGED SUCCESSFUlly, new MODE= %d\n",ctime(&timep),gCurMode);

            statechanged = 1;

            //Invoke Socket Thread 's callback function to send notification to Client.
            ret = CallBack_NotifyClientOnStateChange();
            if(ret!=0)
            {
                DBUG_OUT( " Notify Client about state change failed \n");
                statechanged = 0;
            }

        }
    }

}



int StateMachine_process(int uinput_fd, gesture_output gest_out,centroid_output cent_out )
{
    int err = 0;

    gestureHistoryPush(gest_out);
    checkAndSwitchMode();

    time(&timep);

    switch(gCurMode)
    {
    case MODE_NONE:
    {
        DBUG_OUT("MODE_NONE, Unipoint is disabled already ");

        break;
    }
    case MODE_NORMAL:
    {

        switch(gest_out.type)
        {

        case NONE_GESTURE :
            DBUG_OUT("%s, MODE_NORMAL, Daemon Received NONE_GESTURE event \n",ctime(&timep));
            break;
        case RIGHT_GESTURE :
            DBUG_OUT( "%s, MODE_NORMAL, Daemon Received RIGHT_GESTURE event \n",ctime(&timep));
            event_send_gesture(MASK_RIGHT_GESTURE,cent_out,uinput_fd);
            break;
        case LEFT_GESTURE :
            DBUG_OUT("%s, MODE_NORMAL, Daemon Received LEFT_GESTURE event \n",ctime(&timep));
            event_send_gesture(MASK_LEFT_GESTURE,cent_out,uinput_fd);
            break;
        case DOWN_GESTURE :
            DBUG_OUT("%s, MODE_NORMAL, Daemon Received DOWN_GESTURE event \n",ctime(&timep));
            event_send_gesture(MASK_DOWN_GESTURE,cent_out,uinput_fd);
            break;
        case UP_GESTURE :
            DBUG_OUT("%s, MODE_NORMAL, Daemon Received UP_GESTURE event \n",ctime(&timep));
            event_send_gesture(MASK_UP_GESTURE,cent_out,uinput_fd);
            break;
        case TAP_GESTURE :
            DBUG_OUT( "%s, MODE_NORMAL, Daemon Received TAP_GESTURE event \n",ctime(&timep));
            event_send_gesture(MASK_TAP_GESTURE,cent_out,uinput_fd);

            break;
        case TAPHOLD_GESTURE :
            DBUG_OUT("%s, MODE_NORMAL, Daemon Received TAPHOLD_GESTURE event \n",ctime(&timep));
            event_send_gesture(MASK_TAPHOLD_GESTURE,cent_out,uinput_fd);
            break;
        case POINTER_GESTURE :
            DBUG_OUT("%s, MODE_NORMAL, Daemon Received POINTER_GESTURE event \n",ctime(&timep));
            event_send_gesture(MASK_POINTER_GESTURE,cent_out,uinput_fd);
            break;
        case TIMEDOUT_GESTURE :
            DBUG_OUT("%s, MODE_NORMAL, Daemon Received TIMEDOUT_GESTURE event \n",ctime(&timep));
            event_send_gesture(MASK_POINTER_GESTURE,cent_out,uinput_fd);
            break;
        default:

            DBUG_OUT("%s, MODE_NORMAL, Daemon Received Unrecognized	event \n",ctime(&timep));
            break;

        }
        break;
    }

    //If current mode is MODE_VOLUME
    case MODE_VOLUME:
    {

        switch(gest_out.type)
        {

        case NONE_GESTURE :
            DBUG_OUT("%s, MODE_VOLUME, Daemon Received NONE_GESTURE event \n",ctime(&timep));
            break;
        case RIGHT_GESTURE :
            DBUG_OUT("%s, MODE_VOLUME,Daemon Received RIGHT_GESTURE event \n",ctime(&timep));
            //	event_send_gesture(MASK_RIGHT_GESTURE,cent_out,uinput_fd);
            break;
        case LEFT_GESTURE :
            DBUG_OUT("%s, MODE_VOLUME,Daemon Received LEFT_GESTURE event \n",ctime(&timep));
            //	event_send_gesture(MASK_LEFT_GESTURE,cent_out,uinput_fd);
            break;
        case DOWN_GESTURE :
            DBUG_OUT("%s, MODE_VOLUME,Daemon Received VOLUME_DOWN event \n",ctime(&timep));
            event_send_gesture(MASK_VOLUME_DOWN,cent_out,uinput_fd);
            break;
        case UP_GESTURE :
            DBUG_OUT( "%s, MODE_VOLUME,Daemon Received VOLUME_UP event \n",ctime(&timep));
            event_send_gesture(MASK_VOLUME_UP,cent_out,uinput_fd);
            break;
        case TAP_GESTURE :
            DBUG_OUT("%s, MODE_VOLUME,Daemon Received TAP_GESTURE event \n",ctime(&timep));
            //	event_send_gesture(MASK_TAP_GESTURE,cent_out,uinput_fd);

            break;
        case TAPHOLD_GESTURE :
            DBUG_OUT("%s, MODE_VOLUME,Daemon Received TAPHOLD_GESTURE event \n",ctime(&timep));
            //	event_send_gesture(MASK_TAPHOLD_GESTURE,cent_out,uinput_fd);
            break;
        case POINTER_GESTURE :
            DBUG_OUT("%s, MODE_VOLUME,Daemon Received POINTER_GESTURE event \n",ctime(&timep));
            //	event_send_gesture(MASK_POINTER_GESTURE,cent_out,uinput_fd);
            break;
        case TIMEDOUT_GESTURE :
            DBUG_OUT("%s, MODE_VOLUME,Daemon Received TIMEDOUT_GESTURE event \n",ctime(&timep));
            //	event_send_gesture(MASK_POINTER_GESTURE,cent_out,uinput_fd);
            break;
        default:

            DBUG_OUT("%s, MODE_VOLUME,Daemon Received Unrecognized	event \n",ctime(&timep));
            break;

        }

        break;
    }
    default:
    {
        DBUG_OUT("%s,Uncrecognized MODE \n",ctime(&timep));
        break;
    }
    }


    return err;
}


