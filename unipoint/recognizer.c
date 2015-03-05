// DLG TODO Add UMG's official CopyRight Head

#include <math.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unipoint.h>

// sample history raw and scratch pad
extern F11_centroid_registers gcentroidHistory[SWIPE_MAX_DURATION_FRAMES];
extern centroid_scratchpad gcentroidScratchPad[SWIPE_MAX_DURATION_FRAMES];
extern float gangleHistogramRaw[ANGLE_HISTOGRAM_BIN_COUNT];
extern float gangleHistogramNorm[ANGLE_HISTOGRAM_BIN_COUNT];
extern unsigned int gcentroidHistoryIndex;
extern gesture_features ggestureFeatures;

// DEV ONLY turn this on for maturing features with manual custom classification
#undef CUSTOM_CLASSIFICATION_ON

// --- external functions for development/debug/QA support ---

// provide raw input history for current touch engagement
int 
centroidHistoryPull(F11_centroid_registers *cent, const unsigned int index)
{
	assert(index < gcentroidHistoryIndex);
	assert( gcentroidHistoryIndex > 0);

	*cent = gcentroidHistory[(gcentroidHistoryIndex - 1) - index];
	return 0;
}

// remove all history of current touch engagement
void
centroidHistoryClear()
{
	gcentroidHistoryIndex = 0;
}

// provide side of current engagement history
int
centroidHistoryCount(int * count)
{
	*count = gcentroidHistoryIndex;
	return 0;
}

// provide centroid features of currrent touch engagement
int 
centroidScratchPull(centroid_scratchpad * scratch, const unsigned int index)
{
	assert(index < gcentroidHistoryIndex);
	assert( gcentroidHistoryIndex > 0);

	*scratch = gcentroidScratchPad[(gcentroidHistoryIndex - 1) - index];
	return 0;
}

// provide gestures feature created from centroid history
int
gestureFeaPull(gesture_features * fea)
{
	*fea = ggestureFeatures;
	return 0;
}

//provide slope histogram per frame
int
angleHistogramRawPull(float * hist, const unsigned int index)
{
	assert(index < ANGLE_HISTOGRAM_BIN_COUNT);
	assert( index >= 0);

	*hist = gangleHistogramRaw[index];
	return 0;
}

//provide slope histogram per frame
int
angleHistogramNormPull(float * hist, const unsigned int index)
{
	assert(index < ANGLE_HISTOGRAM_BIN_COUNT);
	assert( index >= 0);

	*hist = gangleHistogramNorm[index];
	return 0;
}

// --- internal functions ---

// insert raw input to history for current touch engagement
void
centroidHistoryPush(const F11_centroid_registers cent)
{
	assert( gcentroidHistoryIndex < SWIPE_MAX_DURATION_FRAMES);

	if (gcentroidHistoryIndex < SWIPE_MAX_DURATION_FRAMES) {
		gcentroidHistory[gcentroidHistoryIndex] = cent; // does this copy 
		gcentroidHistoryIndex++;
	}
}


// classify the current touch history 
int classifyGesture() {

	int returnGesture = NONE_GESTURE;

	// assume globals are set
	assert( gcentroidHistoryIndex > 0);

	float TapClosureYThresh = 50.0; //(float)gcentroidHistory[0].Ymax * 0.1;
	float TapClosureXThresh = 50.0; //(float)gcentroidHistory[0].Xmax * 0.1;

	float NoTrendThreshold = 0.5;
	float UDTrendThreshold = 0.25;
	float UpTrendThreshold = 0.25;
	float DownTrendThreshold = 0.25;

	ggestureFeatures.ClassificationPath = -1;

#ifdef CUSTOM_CLASSIFICATION_ON

	if(ggestureFeatures.LRVelTrig > 20.0 && ggestureFeatures.LRMaxTrend > 100.0)
	{ ggestureFeatures.ClassificationPath = 1; returnGesture = RIGHT_GESTURE; goto returnGestureLabel;}
	if(ggestureFeatures.LRVelTrig > 20.0 && ggestureFeatures.LRMaxTrend > 70.0 && ggestureFeatures.LRMaxEX > 290)
	{ ggestureFeatures.ClassificationPath = 2; returnGesture = RIGHT_GESTURE; goto returnGestureLabel;}
	if(ggestureFeatures.LRVelTrig < -20.0 && ggestureFeatures.LRMaxTrend < -100.0)
	{ ggestureFeatures.ClassificationPath = 3; returnGesture = LEFT_GESTURE; goto returnGestureLabel;}
	if(ggestureFeatures.LRVelTrig < -20.0 && ggestureFeatures.LRMaxTrend < -70.0 && ggestureFeatures.LRMaxEX < 30)
	{ ggestureFeatures.ClassificationPath = 4; returnGesture = LEFT_GESTURE; goto returnGestureLabel;}
	if(ggestureFeatures.LRMaxTrend > 160.0)
	{ ggestureFeatures.ClassificationPath = 5; returnGesture = RIGHT_GESTURE; goto returnGestureLabel;}
	if(ggestureFeatures.LRMaxTrend < -160.0) 
	{ ggestureFeatures.ClassificationPath = 6; returnGesture = LEFT_GESTURE; goto returnGestureLabel;}
	if(ggestureFeatures.LRVelTrig > 24.0)
	{ ggestureFeatures.ClassificationPath = 7; returnGesture = RIGHT_GESTURE; goto returnGestureLabel;}
	if(ggestureFeatures.LRVelTrig < -24.0) 
	{ ggestureFeatures.ClassificationPath = 8; returnGesture = LEFT_GESTURE; goto returnGestureLabel;}

	if((ggestureFeatures.LRMaxTrend / ggestureFeatures.LRindexE) > 25.0) 
	{ ggestureFeatures.ClassificationPath = 9; returnGesture = RIGHT_GESTURE; goto returnGestureLabel;}
	if((ggestureFeatures.LRMaxTrend / ggestureFeatures.LRindexE) < -25.0)
	{ ggestureFeatures.ClassificationPath = 10; returnGesture = LEFT_GESTURE; goto returnGestureLabel;}


	////LR simple curver for now
	//if(ggestureFeatures.LRVelTrig > 30.0 && ggestureFeatures.LRRailHug < 25.0) 
	//	returnGesture = RIGHT_GESTURE; goto returnGestureLabel;
	//if(ggestureFeatures.LRVelTrig < -30.0 && ggestureFeatures.LRRailHug < 25.0)
	//	returnGesture = LEFT_GESTURE; goto returnGestureLabel;

	//if(ggestureFeatures.LRVelTrig > 20.0 && ggestureFeatures.LRRailHug < 15.0) 
	//	returnGesture = RIGHT_GESTURE; goto returnGestureLabel;
	//if(ggestureFeatures.LRVelTrig < -20.0 && ggestureFeatures.LRRailHug < 15.0)
	//	returnGesture = LEFT_GESTURE; goto returnGestureLabel;

	//if(ggestureFeatures.LRVelTrig > 15.0 && ggestureFeatures.LRRailHug < 10.0) 
	//		returnGesture = RIGHT_GESTURE; goto returnGestureLabel;
	//if(ggestureFeatures.LRVelTrig < -15.0 && ggestureFeatures.LRRailHug < 10.0) 
	//		returnGesture = LEFT_GESTURE; goto returnGestureLabel;
	//
	////brutt velocity
	//	//LR simple curver for now
	//if(ggestureFeatures.LRVelTrig > 20.0 && ggestureFeatures.YClosure < 50.0) 
	//	returnGesture = RIGHT_GESTURE; goto returnGestureLabel;
	//if(ggestureFeatures.LRVelTrig < -20.0 && ggestureFeatures.YClosure < 50.0)
	//	returnGesture = LEFT_GESTURE; goto returnGestureLabel;
#endif

	// consider with Histogram features
	// if both trend entire time in one direction use directly
	float Xtrend = 0.0;
	float Ytrend = 0.0;
	int startX = gcentroidHistory[2].X;
	int endX   = gcentroidHistory[gcentroidHistoryIndex-2].X;
	int startY = gcentroidHistory[2].Y;
	int endY   = gcentroidHistory[gcentroidHistoryIndex-2].Y;
	Xtrend = (float)(endX - startX);
	Ytrend = (float)(endY - startY);

    double distMeas = sqrt((double)Xtrend*(double)Xtrend + (double)Ytrend*(double)Ytrend);
                


#ifdef CUSTOM_CLASSIFICATION_ON

	// is there a dominate trend?
	// these are the strong trends even if fast movement. we see it becaues no onset off set
	if(ggestureFeatures.CoTrendUp/(float)ggestureFeatures.frameCount > 0.75) // dlg to do make constants
	{ ggestureFeatures.ClassificationPath = 100; returnGesture = UP_GESTURE; goto returnGestureLabel; }

	if(ggestureFeatures.CoTrendDown/(float)ggestureFeatures.frameCount > 0.75) 
	{ ggestureFeatures.ClassificationPath = 101; returnGesture = DOWN_GESTURE; goto returnGestureLabel; }

	// these are the weaker trends with onset off set effects
	if(   ((ggestureFeatures.CoTrendUp + ggestureFeatures.CoTrendDown)/(float)ggestureFeatures.frameCount > UDTrendThreshold) 
			&& (ggestureFeatures.frameCount >= 14)
			 )
	{
		if (ggestureFeatures.CoTrendUp > ggestureFeatures.CoTrendDown
			&& (ggestureFeatures.CoTrendUp/(float)ggestureFeatures.frameCount) > UpTrendThreshold
			&& (ggestureFeatures.CoTrendUp > (float)ggestureFeatures.CoTrendNone))
		{ ggestureFeatures.ClassificationPath = 102; returnGesture = UP_GESTURE; goto returnGestureLabel; }
		
		if(ggestureFeatures.CoTrendDown > ggestureFeatures.CoTrendUp
			&& (ggestureFeatures.CoTrendDown/(float)ggestureFeatures.frameCount) > DownTrendThreshold
			&& (ggestureFeatures.CoTrendDown > (float)ggestureFeatures.CoTrendNone))
		{ ggestureFeatures.ClassificationPath = 103; returnGesture = DOWN_GESTURE; goto returnGestureLabel; }
	}

	// if we've gotten this far we have no dominent feature to interpret 
	// 

	// biggest problem with up/down trend now is they show up in tap
	// to look for tap here
	// we define tap as contact/lift if short
	// and contact/stationary/lift if long
	// with intensity stable in middle (use variance)
	// and in many cases the contact/lift move in oposite directions even if not same magnitude

	// crudely look at up/none/down but could traverse cotrend slots for up % then none % then down %
	// would like one in front none in middle other in back to 70% of peak
	if(ggestureFeatures.CoTrendNone > ggestureFeatures.CoTrendDown
		&& ggestureFeatures.CoTrendNone > ggestureFeatures.CoTrendUp
		&& ggestureFeatures.CoTrendNone/(float)ggestureFeatures.frameCount > 0.50
		) 
	{ ggestureFeatures.ClassificationPath = 104; returnGesture = TAP_GESTURE; goto returnGestureLabel; } 

	if(ggestureFeatures.CoTrendNone/(float)ggestureFeatures.frameCount > 0.75)
	{ ggestureFeatures.ClassificationPath = 105; returnGesture = TAP_GESTURE; goto returnGestureLabel; } 

	// if nothing dominated and it's short then it's a tap
	if(ggestureFeatures.frameCount < 14)
		{ ggestureFeatures.ClassificationPath = 106; returnGesture = TAP_GESTURE; goto returnGestureLabel; } 
	else if(ggestureFeatures.CoTrendDown > ggestureFeatures.CoTrendUp) 
		{ ggestureFeatures.ClassificationPath = 107; returnGesture = DOWN_GESTURE; goto returnGestureLabel; } 
	else  
		{ ggestureFeatures.ClassificationPath = 108; returnGesture = UP_GESTURE; goto returnGestureLabel; } 

#endif

	//classify off the histogram of angles and magnitude.
	
	// find histogram peak
	float angleMagMax = -1.0;
	int   angleMagMaxIndex = -1;
	for(int i = 0;i<ANGLE_HISTOGRAM_BIN_COUNT;i++) {
		if(gangleHistogramRaw[i] > angleMagMax) {
			angleMagMax = gangleHistogramRaw[i];
			angleMagMaxIndex = i;
		}
	}

	int HIST_PEAK_SMOOTHER = 2;
	float UPANGLETOLERANCE = 60.0; // angle off 90 for high confidence up
	float DOWNANGLETOLERANCE = 60.0; // angle off 90 for high confidence up
	float LEFTANGLETOLERANCE = 30.0; // angle off 90 for high confidence up
	float RIGHTANGLETOLERANCE = 30.0; // angle off 90 for high confidence up

	int j = 0; // peak
	float weightedPeak = 0.0; 
	for(int i=angleMagMaxIndex-HIST_PEAK_SMOOTHER;i<=angleMagMaxIndex+HIST_PEAK_SMOOTHER;i++)
	{
	// sum peak and two neighbors
		j = (i<0)?(ANGLE_HISTOGRAM_BIN_COUNT+i):i;
		j = (i>(ANGLE_HISTOGRAM_BIN_COUNT-1))?i-ANGLE_HISTOGRAM_BIN_COUNT:j;
		weightedPeak += gangleHistogramRaw[j];
	}
	

	float angleThis = angleMagMaxIndex * (float)ANGLE_HISTOGRAM_BIN_SIZE + (float)ANGLE_HISTOGRAM_BIN_SIZE/2;
	ggestureFeatures.weightedPeakAngle = angleThis;
	ggestureFeatures.weightedPeak = weightedPeak;

#ifdef CUSTOM_CLASSIFICATION_ON

	if(weightedPeak > 200.0) { // units of Xmax Ymax on FW API (could be devided by number of hits to these bins or maybe just distance if fine
		// use this directional index directly
		if(abs(angleThis - 0)  <= RIGHTANGLETOLERANCE) { ggestureFeatures.ClassificationPath = 12; returnGesture = RIGHT_GESTURE; goto returnGestureLabel; }	
		if(abs(angleThis - 90) <= UPANGLETOLERANCE)    { ggestureFeatures.ClassificationPath = 13; returnGesture = UP_GESTURE; goto returnGestureLabel; }
		if(abs(angleThis - 180) <= LEFTANGLETOLERANCE) { ggestureFeatures.ClassificationPath = 14; returnGesture = LEFT_GESTURE; goto returnGestureLabel; }
		if(abs(angleThis - 270) <= DOWNANGLETOLERANCE) { ggestureFeatures.ClassificationPath = 15; returnGesture = DOWN_GESTURE; goto returnGestureLabel; }
		if(abs(angleThis - 360) <= RIGHTANGLETOLERANCE){ ggestureFeatures.ClassificationPath = 16; returnGesture = RIGHT_GESTURE; goto returnGestureLabel;} 
	} 
	
	if(weightedPeak < 10.0) { // too weak to call it
		ggestureFeatures.ClassificationPath = 17;returnGesture = TAP_GESTURE; goto returnGestureLabel;
	}

#endif

	// if we haven't returned yet then we're in the gray area
	// check for tap influence at 180 degrees from peak;
	int jj = 0; // peak
	float tapPeak = 0.0;
	int tapInfluenceIndex = angleMagMaxIndex + ANGLE_HISTOGRAM_BIN_COUNT / 2; // 180 away
	int HIST_TAP_SMOOTHER = 2;
	for(int i=tapInfluenceIndex-HIST_TAP_SMOOTHER;i<=tapInfluenceIndex+HIST_TAP_SMOOTHER;i++)
	{
	// sum peak and two neighbors
		jj = (i<0)?(ANGLE_HISTOGRAM_BIN_COUNT+i):i;
		jj = (i>(ANGLE_HISTOGRAM_BIN_COUNT-1))?i-ANGLE_HISTOGRAM_BIN_COUNT:jj;
		tapPeak += gangleHistogramRaw[jj];
	}

	ggestureFeatures.tapPeak = tapPeak;
	ggestureFeatures.angleMagMax = angleMagMax;

#ifdef CUSTOM_CLASSIFICATION_ON

	if(tapPeak > angleMagMax * 0.50
		&& gcentroidHistoryIndex < 10) 
	{ggestureFeatures.ClassificationPath = 18; returnGesture = TAP_GESTURE; goto returnGestureLabel;}

#endif

	//now we're in the noise
	//ask about peakness with respect to entire graph = use normalized
	//find peak of normalized (it's at same peak unnormalized
	int left = ((angleMagMaxIndex-1)<0)?ANGLE_HISTOGRAM_BIN_COUNT-1:angleMagMaxIndex-1;
	int right = ((angleMagMaxIndex+1)>ANGLE_HISTOGRAM_BIN_COUNT-1)?0:angleMagMaxIndex+1;
	float peakedness = 0.0;
	if ( left < ANGLE_HISTOGRAM_BIN_COUNT
			 && left >= 0
			 && angleMagMaxIndex < ANGLE_HISTOGRAM_BIN_COUNT
			 && angleMagMaxIndex >= 0
			 && right < ANGLE_HISTOGRAM_BIN_COUNT
			 && right >= 0)
	{
	peakedness = gangleHistogramNorm[left] + gangleHistogramNorm[angleMagMaxIndex] + gangleHistogramNorm[right];
	}

	angleThis = angleMagMaxIndex * (float)ANGLE_HISTOGRAM_BIN_SIZE + (float)ANGLE_HISTOGRAM_BIN_SIZE/2;

	ggestureFeatures.peakedness = peakedness;
	ggestureFeatures.peakednessAngle = angleThis;

#ifdef CUSTOM_CLASSIFICATION_ON

	float UPSHIFTTOLERANCE = 20.0;
	if(peakedness > 0.7) {
		//we have weak but peaked
		//if(abs(angleThis - 0)  <= (RIGHTANGLETOLERANCE-UPSHIFTTOLERANCE)) { ggestureFeatures.ClassificationPath = 19;returnGesture = RIGHT_GESTURE; goto returnGestureLabel; }	
		if(abs(angleThis - 90) <= (UPANGLETOLERANCE+UPSHIFTTOLERANCE)) { ggestureFeatures.ClassificationPath = 20;returnGesture = UP_GESTURE; goto returnGestureLabel; }
		//if(abs(angleThis - 180) <= (LEFTANGLETOLERANCE-UPSHIFTTOLERANCE)) { ggestureFeatures.ClassificationPath = 21;returnGesture = LEFT_GESTURE; goto returnGestureLabel; }
		if(abs(angleThis - 270) <= (DOWNANGLETOLERANCE+UPSHIFTTOLERANCE)) { ggestureFeatures.ClassificationPath = 22;returnGesture = DOWN_GESTURE; goto returnGestureLabel; }
		//if(abs(angleThis - 360) <= (RIGHTANGLETOLERANCE-UPSHIFTTOLERANCE)) { ggestureFeatures.ClassificationPath = 23;returnGesture = RIGHT_GESTURE; goto returnGestureLabel;} 
	}

	////closure for tap
	//if(    (ggestureFeatures.XClosure < TapClosureXThresh)
	//	&& (ggestureFeatures.YClosure < TapClosureYThresh) ){
	//			ggestureFeatures.ClassificationPath = 24;returnGesture = TAP_GESTURE; goto returnGestureLabel;
	//	}

#endif

	// still in noise. not peaked, not closed, so blend histogram into answer
	float dullPeak = 0.0;
	int dullIndex = -1;

	for(int i=0;i<ANGLE_HISTOGRAM_BIN_COUNT;i++) {
		int left = ((i-1)<0)?ANGLE_HISTOGRAM_BIN_COUNT-1:i-1;
		int right = ((i+1)>ANGLE_HISTOGRAM_BIN_COUNT-1)?0:i+1;
		peakedness = gangleHistogramNorm[left] + gangleHistogramNorm[i] + gangleHistogramNorm[right];
		if(peakedness >= dullPeak) {
			dullPeak = peakedness;
			dullIndex = i;
		}
	}
		
	//we have weak but peaked
	angleThis = dullIndex * ANGLE_HISTOGRAM_BIN_SIZE + ANGLE_HISTOGRAM_BIN_SIZE/2;

	ggestureFeatures.dullPeak =  dullPeak;
	ggestureFeatures.dullPeakAngle = angleThis;

#ifdef CUSTOM_CLASSIFICATION_ON

	if(abs(angleThis - 0)  <= (RIGHTANGLETOLERANCE-UPSHIFTTOLERANCE)) { ggestureFeatures.ClassificationPath = 25; returnGesture = RIGHT_GESTURE; goto returnGestureLabel; }	
	if(abs(angleThis - 90) <= (UPANGLETOLERANCE+UPSHIFTTOLERANCE)) { ggestureFeatures.ClassificationPath = 26;returnGesture = UP_GESTURE; goto returnGestureLabel; }
	if(abs(angleThis - 180) <= (LEFTANGLETOLERANCE-UPSHIFTTOLERANCE)) { ggestureFeatures.ClassificationPath = 27;returnGesture = LEFT_GESTURE; goto returnGestureLabel; }
	if(abs(angleThis - 270) <= (DOWNANGLETOLERANCE+UPSHIFTTOLERANCE)) { ggestureFeatures.ClassificationPath = 28;returnGesture = DOWN_GESTURE; goto returnGestureLabel; }
	if(abs(angleThis - 360) <= (RIGHTANGLETOLERANCE-UPSHIFTTOLERANCE)) { ggestureFeatures.ClassificationPath = 29;returnGesture = RIGHT_GESTURE; goto returnGestureLabel;} 

	ggestureFeatures.ClassificationPath = 500;
	returnGesture = POINTER_GESTURE;
	goto returnGestureLabel;
	
#else

	returnGesture = PROCESSING_COMPLETE;

#endif

	// we only get to this point if the CUSTOM CLASSIFIER IS OFF
#define J48CLASS 
	// C 0.25 M 5 (parsed features down to observable in the tree
	// only for files used in training with jm/ss/vk/dg - many holes
	// DV1 + plastic cap + 735617 
#ifdef J48CLASS
	if( ggestureFeatures.LRVelTrig <= -22 )
	{
		if( ggestureFeatures.weightedPeakAngle <= 250 )
		{
			if( ggestureFeatures.YClosure <= 430.75 )
			{
				returnGesture = LEFT_GESTURE;
			}
			if( ggestureFeatures.YClosure > 430.75 )
			{
				returnGesture = DOWN_GESTURE;
			}
		}
		if( ggestureFeatures.weightedPeakAngle > 250 )
		{
			returnGesture = DOWN_GESTURE;
		}
	}
	if( ggestureFeatures.LRVelTrig > -22 )
	{
		if( ggestureFeatures.LRMaxEX <= 264 )
		{
			if( ggestureFeatures.CoTrendDownDist <= -123.859314 )
			{
				if( ggestureFeatures.LRVelTrig <= 38.666668 )
				{
					if( ggestureFeatures.downLocationNorm <= 0.75 )
					{
						returnGesture = DOWN_GESTURE;
					}
					if( ggestureFeatures.downLocationNorm > 0.75 )
					{
						returnGesture = UP_GESTURE;
					}
				}
				if( ggestureFeatures.LRVelTrig > 38.666668 )
				{
					returnGesture = RIGHT_GESTURE;
				}
			}
			if( ggestureFeatures.CoTrendDownDist > -123.859314 )
			{
				if( ggestureFeatures.CoTrendUpDist <= 60.125 )
				{
					if( ggestureFeatures.LRVelTrig <= 32.666668 )
					{
						if( ggestureFeatures.frameCount <= 8 )
						{
							if( ggestureFeatures.angleMagMax <= 39.266991 )
							{
								returnGesture = TAP_GESTURE;
							}
							if( ggestureFeatures.angleMagMax > 39.266991 )
							{
								returnGesture = LEFT_GESTURE;
							}
						}
						if( ggestureFeatures.frameCount > 8 )
						{
							if( ggestureFeatures.XClosure <= 46.5 )
							{
								returnGesture = TAP_GESTURE;
							}
							if( ggestureFeatures.XClosure > 46.5 )
							{
								if( ggestureFeatures.downSlotCode <= 0 )
								{
									returnGesture = UP_GESTURE;
								}
								if( ggestureFeatures.downSlotCode > 0 )
								{
									returnGesture = DOWN_GESTURE;
								}
							}
						}
					}
					if( ggestureFeatures.LRVelTrig > 32.666668 )
					{
						returnGesture = RIGHT_GESTURE;
					}
				}
				if( ggestureFeatures.CoTrendUpDist > 60.125 )
				{
					if( ggestureFeatures.YClosure <= 68.75 )
					{
						if( ggestureFeatures.tapPeak <= 0 )
						{
							returnGesture = UP_GESTURE;
						}
						if( ggestureFeatures.tapPeak > 0 )
						{
							returnGesture = TAP_GESTURE;
						}
					}
					if( ggestureFeatures.YClosure > 68.75 )
					{
						returnGesture = UP_GESTURE;
					}
				}
			}
		}
		if( ggestureFeatures.LRMaxEX > 264 )
		{
			if( ggestureFeatures.LRMaxTrend <= 117 )
			{
				if( ggestureFeatures.CoTrendDown <= 2 )
				{
					returnGesture = RIGHT_GESTURE;
				}
				if( ggestureFeatures.CoTrendDown > 2 )
				{
					returnGesture = DOWN_GESTURE;
				}
			}
			if( ggestureFeatures.LRMaxTrend > 117 )
			{
				if( ggestureFeatures.LRMaxEX <= 273 )
				{
					if( ggestureFeatures.CoTrendDown <= 6 )
					{
						returnGesture = RIGHT_GESTURE;
					}
					if( ggestureFeatures.CoTrendDown > 6 )
					{
						returnGesture = DOWN_GESTURE;
					}
				}
				if( ggestureFeatures.LRMaxEX > 273 )
				{
					returnGesture = RIGHT_GESTURE;
				}
			}
		}
	}

#endif

returnGestureLabel:
	ggestureFeatures.Classification = returnGesture;
	return returnGesture;

} // end classify gesture

// process raw data and story in centroid filtered history
void centroidFilterHistory() { 

	int smoothingKernel = 3;

	//Do nothing on over or under flow
	if(!(gcentroidHistoryIndex < SWIPE_MAX_DURATION_FRAMES))
		return;
	if(!(gcentroidHistoryIndex > 0))
		return;

	int Ymax = gcentroidHistory[0].Ymax;
	int Xmax = gcentroidHistory[0].Xmax;
	
	float Xalpha = 0.0; //DLG TODO make single poles global constants in unipoint header
	float Yalpha = 0.5;
	float Wxalpha = 0.5;
	float Wyalpha = 0.5;

	float smoothXlast = gcentroidHistory[0].X;
	float smoothYlast = gcentroidHistory[0].Y;
	float smoothWxlast = gcentroidHistory[0].Wx;
	float smoothWylast = gcentroidHistory[0].Wy;

	//run single pole IIR for fast high frequency noise removal 
	for(int i = 0;i<gcentroidHistoryIndex;i++){
		smoothXlast = smoothXlast * (Xalpha) + gcentroidHistory[i].X * (1.0 - Xalpha);
		smoothYlast = smoothYlast * (Yalpha) + gcentroidHistory[i].Y * (1.0 - Yalpha);
		smoothWxlast = smoothWxlast * (Wxalpha) + gcentroidHistory[i].Wx * (1.0 - Wxalpha);
		smoothWylast = smoothWylast * (Wyalpha) + gcentroidHistory[i].Wy * (1.0 - Wyalpha);

		gcentroidScratchPad[i].Xs = smoothXlast;
		gcentroidScratchPad[i].Ys = smoothYlast;
		gcentroidScratchPad[i].Wxs = smoothWxlast;
		gcentroidScratchPad[i].Wys = smoothWylast;

	} // end for each value entry

	//draw other values from smoothed values
	//DLG TODO ask SYNA what the proper unit (and mm) relationship is between Y and Wy
	for(int i = 0;i<gcentroidHistoryIndex;i++) {
			gcentroidScratchPad[i].X = gcentroidScratchPad[i].Xs; //DLG TODO this removes raw from scratch (fix in final version)
			gcentroidScratchPad[i].Y = gcentroidScratchPad[i].Ys;
			gcentroidScratchPad[i].Yts = gcentroidScratchPad[i].Ys + (gcentroidScratchPad[i].Wys/(double)0xF)*(gcentroidHistory[i].Ymax/2.0);
	} // end for each value entry

	gcentroidScratchPad[0].dX = 0.0;
	gcentroidScratchPad[0].dY = 0.0;

	// derive raw velocities
	for(int i = 1;i<gcentroidHistoryIndex;i++) {
			gcentroidScratchPad[i].dX = gcentroidScratchPad[i].X - gcentroidScratchPad[i-1].X;
			gcentroidScratchPad[i].dY = gcentroidScratchPad[i].Y - gcentroidScratchPad[i-1].Y;
			gcentroidScratchPad[i].dYts = gcentroidScratchPad[i].Yts - gcentroidScratchPad[i-1].Yts;
	} // end for each value entry
	
	// smooth velocities especially for edge analysis
	float dXEdgeAlpha = 0.95; //0.7; for edge
	float dYAlpha     = 0.95;
	float velocityLRLast = gcentroidScratchPad[1].dX;
	float velocityUDLast = gcentroidScratchPad[1].dY;

	for(int i = 1; i<gcentroidHistoryIndex;i++) {
		velocityLRLast = velocityLRLast * (dXEdgeAlpha) + gcentroidScratchPad[i].dX * (1.0 - dXEdgeAlpha);
		gcentroidScratchPad[i].dXs = velocityLRLast;

		velocityUDLast = velocityUDLast * (dYAlpha) + gcentroidScratchPad[i].dY * (1.0 - dYAlpha);
		gcentroidScratchPad[i].dYs = velocityUDLast;

	} // end instantanious edge velocity


	for(int i = 0; i<gcentroidHistoryIndex;i++) {
		float thetaMag = 0.0;
		float thetaRadians = 0.0;

		float dXThis = gcentroidScratchPad[i].dX;
		float dYThis = gcentroidScratchPad[i].dY;

		if(dXThis == 0.0 && dYThis == 0.0 ) {    //atan2(0,0) -> 0 
			thetaRadians = 0.0; // no magnitude
		}
		else if(dXThis == 0.0 && dYThis > 0.0) { //atan2(+#,0) -> 90.0;
			thetaRadians = 1.5707963267949;
		}
		else if(dXThis == 0.0 && dYThis < 0.0) { //atan2(-#,0) -> -90.0;
			thetaRadians = -1.5707963267949;
		}
		else { 	                                 //atan2(#,#)
			thetaRadians = atan2(dYThis,dXThis);
		}

		if(dXThis == 0.0 && dYThis == 0.0)
			thetaMag = 0.0;
		else 
			thetaMag = sqrt(dXThis * dXThis + dYThis*dYThis);

		// unwrap degrees but if small noise angle avoid unwrapping in wrong direction
		float thetaDegrees = thetaRadians * (180.0/(3.1416));
		if(thetaDegrees < 0.0) 
			thetaDegrees += 360;

		if(thetaMag > 5.0) {
			//thetaNorm += thetaMag;
			//thetaNormCount++;
			gcentroidScratchPad[i].dXsdYs = thetaMag ;
			gcentroidScratchPad[i].T = thetaDegrees;// * thetaMag;
		} else {
			gcentroidScratchPad[i].dXsdYs = 0.0;
			gcentroidScratchPad[i].T = 0.0;
		}

	} // end slopes and differences


	// run through slope signs for co-traveling slopes. 
	// see which domonates gesture
	int upCoTrend = 0;
	float upCoTrendDist = 0.0;
	int downCoTrend = 0;
	float downCoTrendDist = 0.0;
	int slopeCoTrend = 10;
	int noneCoTrend = 0;
	float noneCoTrendDist = 0.0;
	int oposCoTrend = 0;

	gcentroidScratchPad[0].CoTrendUp = 0;
	gcentroidScratchPad[0].CoTrendDown = 0;
	gcentroidScratchPad[0].CoTrendNone = 0;
	gcentroidScratchPad[0].CoTrendOppos = 0;


	for(int i = 1;i<gcentroidHistoryIndex;i++){

		gcentroidScratchPad[i].CoTrendUp = gcentroidScratchPad[i-1].CoTrendUp;
		gcentroidScratchPad[i].CoTrendDown = gcentroidScratchPad[i-1].CoTrendDown;
		gcentroidScratchPad[i].CoTrendNone = gcentroidScratchPad[i-1].CoTrendNone;
		gcentroidScratchPad[i].CoTrendOppos = gcentroidScratchPad[i-1].CoTrendOppos;

		//if both are moving in same direction
		//DLG TODO which is better using a delta TipY of dY which is delta smoothed Y
		// this could be better if we used a peak window (like LR trigger) rather than lumping entire trend together
		float dYTip = gcentroidScratchPad[i].dYts;
		if((gcentroidScratchPad[i].dY * dYTip) > 0)
		{
			if(dYTip > slopeCoTrend) { 
				upCoTrend++;
				upCoTrendDist += gcentroidScratchPad[i].dY;
				gcentroidScratchPad[i].CoTrendUp = upCoTrend;
			} else if(dYTip < -slopeCoTrend) {
				downCoTrend++;
				downCoTrendDist += gcentroidScratchPad[i].dY;
				gcentroidScratchPad[i].CoTrendDown = downCoTrend;
			} else {
				noneCoTrend++;
				noneCoTrendDist += gcentroidScratchPad[i].dY;
				gcentroidScratchPad[i].CoTrendNone = noneCoTrend;
			}
		} else {
			oposCoTrend++;
			gcentroidScratchPad[i].CoTrendOppos = oposCoTrend;
		}
	} // end for each scratch pad
	
	// not look for trend relatinship to one another
	int nextSlot = 1;
	int upSlotCode = 0;
	int noneSlotCode = 0;
	int downSlotCode = 0;
	int up50Index = -1;
	int none50Index = -1;
	int down50Index = -1;
	//int up50Tally = 0;
	//int none50Tally = 0;
	//int down50Tally = 0;
	int up50Mark = gcentroidScratchPad[gcentroidHistoryIndex-1].CoTrendUp/2;
	int none50Mark = gcentroidScratchPad[gcentroidHistoryIndex-1].CoTrendNone/2;
	int down50Mark = gcentroidScratchPad[gcentroidHistoryIndex-1].CoTrendDown/2;
	float up50LocationNorm = 0.0;
	float none50LocationNorm = 0.0;
	float down50LocationNorm = 0.0;

	for(int i=1;i<gcentroidHistoryIndex;i++) {
		if(gcentroidScratchPad[i].CoTrendUp >= up50Mark && up50Index < 0 && up50Mark > 1) { up50Index = i; up50LocationNorm = ((float)i)/(float)(gcentroidHistoryIndex); upSlotCode = nextSlot++; }
		if(gcentroidScratchPad[i].CoTrendNone >= none50Mark && none50Index < 0 && none50Mark > 1) { none50Index = i; none50LocationNorm = ((float)i)/(float)(gcentroidHistoryIndex); noneSlotCode = nextSlot++; }
		if(gcentroidScratchPad[i].CoTrendDown >= down50Mark && down50Index < 0 && down50Mark > 1) { down50Index = i; down50LocationNorm = ((float)i)/(float)(gcentroidHistoryIndex); downSlotCode = nextSlot++; }
	} 

	if(up50LocationNorm < 0.05)
		up50LocationNorm = 0.5;
	if(up50LocationNorm > 0.94)
		up50LocationNorm = 0.94;

	if(none50LocationNorm < 0.05)
		none50LocationNorm = 0.05;
	if(none50LocationNorm > 0.94)
		none50LocationNorm = 0.94;

	if(down50LocationNorm < 0.05)
		down50LocationNorm = 0.05;
	if(down50LocationNorm > 0.94)
		down50LocationNorm = 0.94;

	// hash code for sequence
	int sequenceHash = upSlotCode * 100 + noneSlotCode * 10 + downSlotCode;
	//TODO the location shouldn't be a percentage? might be better with raw time because as interaction gets shorter the percentages will move to 0.5 norm location
	float locationHash = (int)((up50LocationNorm * 10)+0.5)*100 + (int)((none50LocationNorm * 10)+0.5)*10 + (int)((down50LocationNorm * 10)+0.5);

	ggestureFeatures.sequenceHash = sequenceHash;
	ggestureFeatures.locationHash = locationHash;
	ggestureFeatures.upSlotCode = upSlotCode;
	ggestureFeatures.downSlotCode = downSlotCode;
	ggestureFeatures.noneSlotCode = noneSlotCode;
	ggestureFeatures.upLocationNorm = up50LocationNorm;
	ggestureFeatures.downLocationNorm = down50LocationNorm;
	ggestureFeatures.noneLocationNorm = none50LocationNorm;

#undef ANGLE_DURING_COTREND
#ifdef ANGLE_DURING_COTREND // this did not work well in many cases
	
	// now look solely at movement during trends
	// keep separate for now
	float AngleDuringUp = 0.0;
	float AngleDuringDown = 0.0;
	float AngleDuringNone = 0.0;
	float NormDuringUp = 0.0;
	float NormDuringDown = 0.0;
	float NormDuringNone = 0.0;

	for(int i=1;i<gcentroidHistoryIndex;i++) {
		//see if this trend moved
		if(gcentroidScratchPad[i].CoTrendDown != gcentroidScratchPad[i-1].CoTrendDown) { 
			AngleDuringDown += gcentroidScratchPad[i].dX; //
			NormDuringDown  += gcentroidScratchPad[i].dY; 
		} 
		//see if this trend moved
		if(gcentroidScratchPad[i].CoTrendNone != gcentroidScratchPad[i-1].CoTrendNone) { 
			AngleDuringNone += gcentroidScratchPad[i].dX; //
			NormDuringNone  += gcentroidScratchPad[i].dY; 
		} 
		//see if this trend moved
		if(gcentroidScratchPad[i].CoTrendUp != gcentroidScratchPad[i-1].CoTrendUp) { 
			AngleDuringUp += gcentroidScratchPad[i].dX; //
			NormDuringUp  += gcentroidScratchPad[i].dY; 
		} 
	}
	
	gcentroidScratchPad[0].T = 0.0;
	gcentroidScratchPad[1].T = 0.0;
	gcentroidScratchPad[2].T = 0.0;

	if(NormDuringUp != 0.0) 
	gcentroidScratchPad[0].T = atan2(NormDuringUp, AngleDuringUp) * (180.0/3.1416);
	
	if(NormDuringNone != 0.0)
	gcentroidScratchPad[1].T = atan2(NormDuringNone, AngleDuringNone) * (180.0/3.1416);
	
	if(NormDuringDown != 0.0)
	gcentroidScratchPad[2].T = atan2(NormDuringDown, AngleDuringDown) * (180.0/3.1416);
	

	if(NormDuringUp == 0.0 && AngleDuringUp == 0.0)
		gcentroidScratchPad[3].T = 0.0;
	else
		gcentroidScratchPad[3].T = sqrt(NormDuringUp*NormDuringUp + AngleDuringUp*AngleDuringUp);
	
	if(NormDuringNone == 0.0 && AngleDuringNone == 0.0)
		gcentroidScratchPad[4].T = 0.0;
	else
		gcentroidScratchPad[4].T = sqrt(NormDuringNone*NormDuringNone + AngleDuringNone*AngleDuringNone);
	
	if(NormDuringDown == 0.0 && AngleDuringDown == 0.0)
		gcentroidScratchPad[5].T = 0.0;
	else
		gcentroidScratchPad[5].T = sqrt(NormDuringDown*NormDuringDown + AngleDuringDown*AngleDuringDown);
#endif // dlg removing angle and mag 

	// promote then ending trends to feature status
	ggestureFeatures.CoTrendUp = gcentroidScratchPad[gcentroidHistoryIndex-1].CoTrendUp;
	ggestureFeatures.CoTrendDown = gcentroidScratchPad[gcentroidHistoryIndex-1].CoTrendDown;
	ggestureFeatures.CoTrendNone = gcentroidScratchPad[gcentroidHistoryIndex-1].CoTrendNone;
	ggestureFeatures.CoTrendOppos =  gcentroidScratchPad[gcentroidHistoryIndex-1].CoTrendOppos;
	ggestureFeatures.CoTrendUpDist = upCoTrendDist;
	ggestureFeatures.CoTrendDownDist = downCoTrendDist;
	ggestureFeatures.CoTrendNoneDist = noneCoTrendDist;
	float valueTemp = upCoTrendDist/upCoTrend;
	float valueTemp2 = downCoTrendDist/downCoTrend;
	
	if(abs(valueTemp) > abs(valueTemp2))
		ggestureFeatures.UDVelTrend = valueTemp; // DLG or should we save both in separate features
	else if(abs(valueTemp) < abs(valueTemp2))
		ggestureFeatures.UDVelTrend = valueTemp2;
	else 
		ggestureFeatures.UDVelTrend = 0.0;

#undef SEGMENT_MINMAX // this is initial version and did not work well 
#ifdef SEGMENT_MINMAX

	//now create dominate trends in Y
	float uptrend = 0.0;
	float downtrend = 0.0;
	float notrend = 0.0;
	float counttrend = 0.0;
	float startY = gcentroidScratchPad[0].Yts;
	int   startIndexY = 0;
	float endY = gcentroidScratchPad[gcentroidHistoryIndex-1].Yts;
	int   endIndexY = gcentroidHistoryIndex-1;
	float maxY = -1.0;
	int   maxIndexY = 0;
	float minY = Ymax +1;
	int   minIndexY = 0;

	//find max/min and end/start points
	for(int i=0;i<gcentroidHistoryIndex;i++) {
		
		if(maxY < gcentroidScratchPad[i].Yts) {
			maxY = gcentroidScratchPad[i].Yts;
			maxIndexY = i;
		} 
		if(minY > gcentroidScratchPad[i].Yts) {
			minY = gcentroidScratchPad[i].Yts;
			minIndexY = i;
		} 
	}

	//find trends between peaks
	//gesture_direction_trend trend1, trend2, trend3, trend4;
	int	segment0 = startIndexY;
	int segment1 = (minIndexY < maxIndexY) ? minIndexY : maxIndexY;
	int	segment2 = (minIndexY >= maxIndexY)? minIndexY : maxIndexY;
	int	segment3 = endIndexY;
	int segCurrent  = 0;
	float segTrend01 = 0.0;
	float segTrend12 = 0.0;
	float segTrend23 = 0.0;

	for(int i=0;i<gcentroidHistoryIndex;i++) {
		
			//set segment to trend based on where i is w.r.t. segment indices
			if (i <= segment1 && segment1 != segment0)
				segCurrent = 1;
			else if((i <= segment2) && (segment2 != segment1))
				segCurrent = 2;
			else if((i <= segment3) && (segment3 != segment2))
				segCurrent = 3;

		switch(segCurrent) {
			case 1:
				segTrend01 += gcentroidScratchPad[i].dYts;
				break;
			case 2:
				segTrend12 += gcentroidScratchPad[i].dYts;
				break;
			case 3: 
				segTrend23 += gcentroidScratchPad[i].dYts;
				break;
			default:
				break;
		}
	}

	// populate features struct
	ggestureFeatures.UDSegSY        = gcentroidScratchPad[segment0].Yts;
	ggestureFeatures.UDSeg1Y        = gcentroidScratchPad[segment1].Yts;
	ggestureFeatures.UDSeg2Y        = gcentroidScratchPad[segment2].Yts;
	ggestureFeatures.UDSegEY        = gcentroidScratchPad[segment3].Yts;
	ggestureFeatures.UDtrendS1      = segTrend01;
	ggestureFeatures.UDtrend12      = segTrend12;
	ggestureFeatures.UDtrend2E      = segTrend23;
	ggestureFeatures.UDindexS       = segment0;
	ggestureFeatures.UDindex1       = segment1;
	ggestureFeatures.UDindex2       = segment2;
	ggestureFeatures.UDindexE       = segment3;
#endif


	ggestureFeatures.frameCount     = gcentroidHistoryIndex;

#ifdef SEGMENT_MINMAX
	// find endpoints of exit segment
	// and measure movement against edge of sensor
	// find endpoints of start of segment
	// and measure movement againts 
	//now create dominate trends in X

	float lefttrend = 0.0;
	float righttrend = 0.0;
	float notlrtrend = 0.0;
	float threshlrtrend = Xmax/4;
	float startX = gcentroidScratchPad[0].X;
	int   startIndexX = 0;
	float endX = gcentroidScratchPad[gcentroidHistoryIndex-1].X;
	int   endIndexX = gcentroidHistoryIndex-1;
	float maxX = -1.0;
	int   maxIndexX = 0;
	float minX = Xmax + 1;
	int   minIndexX = 0;

	//find max/min and end/start points
	for(int i=0;i<gcentroidHistoryIndex;i++) {
		
		if(maxX < gcentroidScratchPad[i].X) {
			maxX = gcentroidScratchPad[i].X;
			maxIndexX = i;
		} 
		if(minX > gcentroidScratchPad[i].X) {
			minX = gcentroidScratchPad[i].X;
			minIndexX = i;
		} 
	}

	//find trends between peaks
	//gesture_direction_trend trend1, trend2, trend3, trend4;
	segment0 = startIndexX;
	segment1 = (minIndexX <=  maxIndexX) ? minIndexX : maxIndexX;
	segment2 = (minIndexX <= maxIndexX)? maxIndexX : minIndexX;
	segment3 = endIndexX;
	segCurrent  = 0;
	segTrend01 = 0.0;
	segTrend12 = 0.0;
	segTrend23 = 0.0;

	for(int i=0;i<gcentroidHistoryIndex;i++) {
		
			//set you segment to trend
			if (i <= segment1 && segment1 != segment0)
				segCurrent = 1;
			else if((i <= segment2) && (segment2 != segment1))
				segCurrent = 2;
			else if((i <= segment3) && (segment3 != segment2))
				segCurrent = 3;

		switch(segCurrent) {
			case 1:
				segTrend01 += gcentroidScratchPad[i].dX;
				break;
			case 2:
				segTrend12 += gcentroidScratchPad[i].dX;
				break;
			case 3: 
				segTrend23 += gcentroidScratchPad[i].dX;
				break;
			default:
				break;
		}
	}

	// promote to features.
	ggestureFeatures.LRSegSX        = gcentroidScratchPad[segment0].X;
	ggestureFeatures.LRSeg1X        = gcentroidScratchPad[segment1].X;
	ggestureFeatures.LRSeg2X        = gcentroidScratchPad[segment2].X;
	ggestureFeatures.LRSegEX        = gcentroidScratchPad[segment3].X;
	ggestureFeatures.LRtrendS1      = segTrend01;
	ggestureFeatures.LRtrend12      = segTrend12;
	ggestureFeatures.LRtrend2E      = segTrend23;
	ggestureFeatures.LRindexS       = segment0;
	ggestureFeatures.LRindex1       = segment1;
	ggestureFeatures.LRindex2       = segment2;
	ggestureFeatures.LRindexE       = segment3;

	// find longest trend.
	float segXTrendMax = segTrend01;
	int segXStart = segment0;
	int segXEnd = segment1;

	if(fabs(segXTrendMax) < fabs(segTrend12)) {
		segXTrendMax = segTrend12;
		segXStart = segment1;
		segXEnd   = segment2;
	}
	if(fabs(segXTrendMax) < fabs(segTrend23)) {
		segXTrendMax = segTrend23;
		segXStart = segment2;
		segXEnd   = segment3;
	}
#endif

	//find closure for tap
	int endIndex = (int)gcentroidHistoryIndex/2;
	endIndex = (endIndex > 4)?4:endIndex;
	float AvgXStart = 0.0;
	float AvgXEnd = 0.0;
	float AvgYStart = 0.0;
	float AvgYEnd = 0.0;

	for(int i = 0;i<endIndex;i++) {
		AvgXStart += gcentroidHistory[i].X;
		AvgXEnd += gcentroidHistory[gcentroidHistoryIndex-1-i].X;
		AvgYStart += gcentroidHistory[i].Y;
		AvgYEnd += gcentroidHistory[gcentroidHistoryIndex-1-i].Y;
	}

	float XClosure = fabs(((double)(AvgXStart-AvgXEnd))/(double)endIndex);
	float YClosure = fabs(((double)(AvgYStart-AvgYEnd))/(double)endIndex);
	
	ggestureFeatures.XClosure = XClosure;
	ggestureFeatures.YClosure = YClosure;


	//clear out old hi jacked value
	for(int i=0;i<gcentroidHistoryIndex;i++) { gcentroidScratchPad[i].dXts = 0.0;}

	// sliding LR trigger window to replace segmental method based on rail.
	int TRIGGER_WINDOW_SIZE = 4; // dlg to be #defined based on framerate and LR flick speads
	int idxWindow = -1;
	bool trgWindow = false;
	float maxWindow = 0.0;
	float LRWindowThresh = (Xmax/15.0);
	
	//dlg sliding window assumes 3 points in scratch
	assert(gcentroidHistoryIndex >= 3);

	int TriggerWindowSize = (TRIGGER_WINDOW_SIZE<gcentroidHistoryIndex)?TRIGGER_WINDOW_SIZE-1:gcentroidHistoryIndex-1; // take smaller of SIZE of number of points this pass
	for(int i=2;i<gcentroidHistoryIndex-(TriggerWindowSize-1);i++) {
		float thisWindow = 0.0; // computer this window 
		for(int j = 0;j<TriggerWindowSize;j++) {
			thisWindow +=gcentroidScratchPad[j+i].dX;
		}
		if(abs(thisWindow) > abs(maxWindow)) {
			maxWindow = thisWindow;
			idxWindow = i+(int)(TriggerWindowSize/2);
		}
		gcentroidScratchPad[i].dXts = thisWindow; //hijack this entry

	} //end sliding trigger window

	// design idea is to collect everything in single direction. L or R
	// consider speed, distance traveled, place in gesture
	float maxLeft = 0.0;
	int   idxLeft = 0;
	int   cntLeft = 0;

	float maxRight = 0.0;
	int   idxRight = 0;
	int   cntRight = 0;

	float runDistance = 0.0;
	int   runDirection = 0;
	int   runTime = 0;
	
	
	for(int i = 1;i<gcentroidHistoryIndex;i++) {
			float thisValue = gcentroidScratchPad[i].dX;
		
			if((thisValue > 0.0) && runDirection > 0) { //same sign and L2R
				runDistance += thisValue;
				runTime++;
			} else if((thisValue < 0.0) && runDirection < 0 ) { // same sign and R2L
				runDistance += thisValue; 
				runTime++;
			} else if((thisValue < 0.0) && runDirection > 0) { // switching from R2L to L2R 
				// store this L2R run and prime other direction
				if(runDistance >= maxRight) {
					maxRight = runDistance;
					idxRight = i-1;
					cntRight = runTime;
				} 
				runDistance = thisValue;
				runDirection = -1;
				runTime = 1;
			} else if((thisValue > 0.0) && runDirection < 0) {
				// store this R2L run and prime other direction
				if(runDistance <= maxLeft) {
					maxLeft = runDistance;
					idxLeft = i-1;
					cntLeft = runTime;
				}
				runDistance = thisValue;
				runDirection = 1;
				runTime = 1;
			} else if(thisValue == 0) {
				if(runDirection == 0) {
					//do nothing we haven't seen a direction yet
				} else {
					runTime++;
				} 
			} // end of the integrators 

			//start the first run only after we see a direction
			if(runDirection == 0 && thisValue != 0 ) {
				runDirection = (thisValue>0)?1:-1;
				runTime = 1;
				runDistance = thisValue;
			}

	} // end collect single direction

	//collect end segments after loop ends
	if(runDistance >= maxRight) {
		maxRight = runDistance;
		idxRight = gcentroidHistoryIndex-1;
		cntRight = runTime;
	} 

	if(runDistance <= maxLeft) {
		maxLeft = runDistance;
		idxLeft = gcentroidHistoryIndex-1;
		cntLeft = runTime;
	}

	// at this point we have the maximum left and right trends in the flick
	// we can compute triggers from
	// max distance close to full pad Xmax and runDistance/runTime

#undef LR_VELOCITY_TRIGGERS
	// this worked well for LR triggers and LR + down on edge of sensor 
	// but turned off to look for more eligent way to achieve same thing with vel window
#ifdef LR_VELOCITY_TRIGGERS 
	//trigger on LR edge velocity
	//DLG TODO this could be in classifier
	float velocityTriggered = 0.0;
	bool startTriggered = false;
	float EdgeTrendThreshold = Xmax/6; //must move enough of sensor
	float EdgeTrendThresholdwRailLower = Xmax/6;
	float EdgeVelocityThreshold = 15; // ticks per frame with 354 full scale 9mm
	float EdgeBruttVelocityThreshold = 30;
	float EdgeBruttTrendThreshold = Xmax/2;


	// is this left to right movement 
	if (    (segXTrendMax > EdgeTrendThreshold) // enough peak to peak movement left to right
	    &&  (gcentroidScratchPad[segXEnd].X   > (Xmax - Xmax/10)) // and ending on edge
		&&  (gcentroidScratchPad[segXStart].X < (Xmax - Xmax/4)) )	// and starting off edge
	{ 

		//if velocity triggered as we neared the edge
		for(int i = segXStart;i<=segXEnd;i++) {
			if(gcentroidScratchPad[i].X > (Xmax - Xmax/5)) {
				startTriggered = true;
			}
			if(startTriggered) { // if we started off edge
				if(gcentroidScratchPad[i].dXs > EdgeVelocityThreshold) {
					velocityTriggered = gcentroidScratchPad[i].dXs;
					break;
				} 
			}  
		} // end trigger velocity
	}

	// is this left to right and rail movement
	double tailTally = 0;
	double railDist   = 0; 

	if (    (segXTrendMax > EdgeTrendThresholdwRailLower) // enough peak to peak movement left to right
	    &&  (gcentroidScratchPad[segXEnd].X   > (Xmax - Xmax/5)) //  and ending on edge
		&&  (gcentroidScratchPad[segXStart].X < (Xmax - Xmax/4)) ) // and starting off edge a little bit 
	{ 

		//if velocity triggered as we neared the edge
		for(int i = segXEnd;i<gcentroidHistoryIndex;i++) {
				railDist += Xmax - gcentroidScratchPad[i].X;
				tailTally++;
			}
	} // end if on edge
	

	// is this right to left movement 
	//DLG TODO this could be in classifier
	if (    (segXTrendMax < -EdgeBruttTrendThreshold)) // enough peak to peak movement left to right
	{ 
		//if velocity triggered as we neared the edge
		for(int i = segXStart;i<=segXEnd;i++) {
				if(gcentroidScratchPad[i].dXs < -EdgeBruttVelocityThreshold) {
					velocityTriggered = gcentroidScratchPad[i].dXs;
					break;
				} 
				if( (gcentroidScratchPad[i].dXs < -EdgeVelocityThreshold)
					&& (YClosure < 50.0) ){
						velocityTriggered = gcentroidScratchPad[i].dXs;
					break;
				}
			  
		} // end trigger velocity
	}

	// is this right to left movement 
	//DLG TODO this could be in classifier
	if (    (segXTrendMax < -EdgeTrendThreshold) // enough peak to peak movement left to right
	    &&  (gcentroidScratchPad[segXEnd].X   < (Xmax/10)) // and ending on edge
		&&  (gcentroidScratchPad[segXStart].X > (Xmax/4)) )	// and starting off edge
	{ 

		//if velocity triggered as we neared the edge
		for(int i = segXStart;i<=segXEnd;i++) {
			if(gcentroidScratchPad[i].X < (Xmax/5)) {
				startTriggered = true;
			}
			if(startTriggered) { // if we started off edge
				if(gcentroidScratchPad[i].dXs < -EdgeVelocityThreshold) {
					velocityTriggered = gcentroidScratchPad[i].dXs;
					break;
				} 
			}  
		} // end trigger velocity
	}

	// is this left to right and rail movement
	if (    (segXTrendMax < -EdgeTrendThresholdwRailLower) // enough peak to peak movement left to right
	    &&  (gcentroidScratchPad[segXEnd].X   < (Xmax/5)) //  and ending on edge
		&&  (gcentroidScratchPad[segXStart].X > (Xmax/4)) ) // and starting off edge a little bit 
	{ 

		tailTally = 0;
		railDist   = 0; 

		//if velocity triggered as we neared the edge
		for(int i = segXEnd;i<gcentroidHistoryIndex;i++) {
				railDist += gcentroidScratchPad[i].X;
				tailTally++;
			}

	} // end if on edge
	
	if(tailTally == 0.0 ) {
		ggestureFeatures.LRRailHug = Xmax/5;
	} else { 
		ggestureFeatures.LRRailHug = railDist/tailTally;
	}

	// populate features struct 
	ggestureFeatures.LRMaxSegSX	= gcentroidScratchPad[segXStart].X;
	//ggestureFeatures.LRMaxEX    = gcentroidScratchPad[segXEnd].X;
	//ggestureFeatures.LRMaxTrend = segXTrendMax;
#endif

	ggestureFeatures.LRVelTrig  = maxWindow/TriggerWindowSize; // first version was velocityTriggered;

	if(abs(maxRight) > abs(maxLeft)) {
		ggestureFeatures.LRMaxTrend = maxRight;
		ggestureFeatures.LRMaxEX    = gcentroidScratchPad[idxRight].X;
		ggestureFeatures.LRindexE   = cntRight;
	}else{
		ggestureFeatures.LRMaxTrend = maxLeft;
		ggestureFeatures.LRMaxEX    = gcentroidScratchPad[idxLeft].X;
		ggestureFeatures.LRindexE   = cntLeft;
	}


	// clear last histogram
	for(int i = 0;i<ANGLE_HISTOGRAM_BIN_COUNT;i++) {
		gangleHistogramRaw[i] = 0.0;
		gangleHistogramNorm[i] = 0.0;
	} // end clear

	// run histrogram of angles influenced by magnitude

	float angleHistogramTotalMag = 0.0;
	int	  angleHistogramEntries = 0.0;

	// turn frame to frame angles into a histrogram
	for(int i = 1;i<gcentroidHistoryIndex;i++) {
		//make index by dividing by bin size
		int angleIndex = (int)(gcentroidScratchPad[i].T/ANGLE_HISTOGRAM_BIN_SIZE);
		gangleHistogramRaw[angleIndex] += gcentroidScratchPad[i].dXsdYs;
	} // end for each angle
	

	// testing of trend filtered histogram
	for(int i = 1;i<gcentroidHistoryIndex;i++) {
		//make index by dividing by bin size
		if(   gcentroidScratchPad[i].CoTrendDown > gcentroidScratchPad[i-1].CoTrendDown
			||gcentroidScratchPad[i].CoTrendUp > gcentroidScratchPad[i-1].CoTrendUp
			||gcentroidScratchPad[i].CoTrendNone > gcentroidScratchPad[i-1].CoTrendNone
			) {
			int angleIndex = (int)(gcentroidScratchPad[i].T/ANGLE_HISTOGRAM_BIN_SIZE);
			gangleHistogramNorm[angleIndex] += gcentroidScratchPad[i].dXsdYs;
			angleHistogramTotalMag += gcentroidScratchPad[i].dXsdYs;
			angleHistogramEntries++;
		}
	} // end for each angle


	for(int i = 0;i<ANGLE_HISTOGRAM_BIN_COUNT;i++) {
		if(angleHistogramTotalMag != 0.0) 
			gangleHistogramNorm[i] = gangleHistogramNorm[i]/angleHistogramTotalMag;
		else 
			gangleHistogramNorm[i] = 0.0;

	} // end normalize

	return;

} // end centroid filter history
