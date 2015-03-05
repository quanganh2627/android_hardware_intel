// DLG TODO Add UMG's official CopyRight Head

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <math.h>

#include <macros.h>
#include <unipoint.h>

// ballistics global support
// DLG TODO make structure in unipoint.h
extern point gpointRawLast;
extern point gvelocityRawLast;
extern ballistics gballistic;

extern F11_centroid_registers gcentroidHistory[SWIPE_MAX_DURATION_FRAMES];
extern centroid_scratchpad gcentroidStratchPad[SWIPE_MAX_DURATION_FRAMES];
extern unsigned int gcentroidHistoryIndex;

extern int gprocessMode; 
extern int grunningFrameCount;

int
initialize(void)
{
	int error = 0;

	//zero out location parameters (But need to redo ever time new finger contacts sensor)
	gprocessMode = PROC_IDLE;
	grunningFrameCount = 0;

	//set ballistics characteristics 
	// DLG TODO move to unipoint.h all subject to change
	gballistic.gain.X = 4000.0F;// gain on delta signal 
	gballistic.gain.Y = 5000.0F;// gain on delta signal 
	gballistic.precision.X = 0.2;// accuracy of delta signal
	gballistic.precision.Y = 0.2;// accuracy of delta signal
	gballistic.vmax.X = 0.5;//50;// largest delta before cap
	gballistic.vmax.Y = 0.5;//50;// largest delta before cap
	gballistic.lower.X = F11_XMIN;
	gballistic.lower.Y = F11_XMIN; // both same slope
	gballistic.upper.X = 280.0F;//F11_XMAX/2;
	gballistic.upper.Y = 280.0F;//F11_XMAX/2; // both same slope

	gcentroidHistoryIndex = 0;

	return error;
} // end init

int release(void) {
	int error = 0;

	//DLG TODO place free here if we ever malloc

	return error;
} // end release 