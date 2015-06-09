/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <utils/Timers.h>

#define LOG_TAG "Atom_PerformanceTraces"
#include "LogHelper.h"

#include <time.h>
#include "PerformanceTraces.h"

namespace android {
namespace PerformanceTraces {

/**
 * \class PerformanceTimer
 *
 * Private class for managing R&D traces used for performance
 * analysis and testing.
 *
 * This code should be disabled in product builds.
 */
class PerformanceTimer {

public:
    nsecs_t mStartAt;
    nsecs_t mLastRead;
    bool mFilled;            //!< timestamp has been taken
    bool mRequested;         //!< trace is requested/enabled

    PerformanceTimer(void) :
        mStartAt(0),
        mLastRead(0),
        mFilled(false),
        mRequested(false) {
    }

    bool isRunning(void) { return mFilled && mRequested; }

    bool isRequested(void) { return mRequested; }

    int64_t timeUs(void) {
        uint64_t now = systemTime();
        mLastRead = now;
        return (now - mStartAt) / 1000;
    }

    int64_t lastTimeUs(void) {
        uint64_t now = systemTime();
        return (now - mLastRead) / 1000;
    }

   /**
     * Enforce a standard format on timestamp traces parsed
     * by offline PnP tools.
     *
     * \see system/core/include/android/log.h
     */
    void formattedTrace(const char* p, const char *f) {
        ALOGD("%s:%s, Time: %lld us, Diff: %lld us",
             p, f, timeUs(), mFilled ? lastTimeUs() : -1);
    }

    void start(void) {
        mStartAt = mLastRead = systemTime();
        mFilled = true;
    }

    void stop(void) { mFilled = false; }

};

// To allow disabling all tracing infrastructure for non-R&D builds,
// wrap everything in LIBCAMERA_RD_FEATURES (see Android.mk).
// -----------------------------------------------------------------

#ifdef LIBCAMERA_RD_FEATURES

static PerformanceTimer gLaunch2Preview;
static PerformanceTimer gLaunch2FocusLock;
static PerformanceTimer gFaceLock;
static PerformanceTimer gShot2Shot;
static PerformanceTimer gShutterLag;
static PerformanceTimer gSwitchCameras;
static PerformanceTimer gAAAProfiler;
static PerformanceTimer gPnPBreakdown;
static PerformanceTimer gHDRShot2Preview;

static int gFaceLockFrame = -1;
static bool gHDRCalled = false;
static bool gSwitchCamerasCalled = false;
static bool gSwitchCamerasOriginalVideoMode = false;
static bool gSwitchCamerasVideoMode = false;
static int gSwitchCamerasOriginalCameraId = 0;
/**
 * Reset the flags that enable the different performance traces
 * This is needed during HAL open so that we can turn off the performance
 * traces from the system property
 */
void reset(void)
{
    gSwitchCamerasCalled = false;
    gSwitchCamerasVideoMode = false;
    gHDRCalled =false;
    gLaunch2Preview.mRequested = false;
    gShot2Shot.mRequested = false;
    gAAAProfiler.mRequested = false;
    gShutterLag.mRequested = false;
    gSwitchCameras.mRequested = false;
    gLaunch2FocusLock.mRequested = false;

}
/**
 * Controls trace state
 */
void Launch2Preview::enable(bool set)
{
    gLaunch2Preview.mRequested = set;
}

/**
 * Starts the launch2preview trace.
 */
void Launch2Preview::start(void)
{
    if (gLaunch2Preview.isRequested()) {
        if (gPnPBreakdown.isRunning())
            PnPBreakdown::step("Launch2Preview::start");
        gLaunch2Preview.start();
    }
}

/**
 * Stops the launch2preview trace and prints out results.
 */
void Launch2Preview::stop(int mFrameNum)
{
    if (gLaunch2Preview.isRunning()) {
        if (gPnPBreakdown.isRunning())
            PnPBreakdown::step("Launch2Preview::stop");
        if (mFrameNum == 1) {
            ALOGD("LAUNCH time to the 1st preview frame show:\t%lld ms\n",
                 gLaunch2Preview.timeUs() / 1000);
        } else {
            ALOGD("LAUNCH: skip %d frame, time to the 1st preview frame show:\t%lld ms\n",
                 (mFrameNum - 1), gLaunch2Preview.timeUs() / 1000);
        }

        gLaunch2Preview.stop();
    }
}

/**
 * Controls trace state
 */
void Launch2FocusLock::enable(bool set)
{
    gLaunch2FocusLock.mRequested = set;
}

/**
 * Starts the launch2FocusLock trace.
 */
void Launch2FocusLock::start(void)
{
    if (gLaunch2FocusLock.isRequested()) {
        gLaunch2FocusLock.formattedTrace("Launch2FocusLock", __FUNCTION__);
        gLaunch2FocusLock.start();
    }
}

/**
 * Stops the launch2FocusLock trace and prints out results.
 */
void Launch2FocusLock::stop(void)
{
    if (gLaunch2FocusLock.isRunning()) {
        if (gPnPBreakdown.isRunning())
            PnPBreakdown::step("Launch2FocusLock::stop");
        ALOGD("LAUNCH time calculated from create instance to lock the focus frame:\t%lld ms\n",
             gLaunch2FocusLock.timeUs() / 1000);
        gLaunch2FocusLock.stop();
    }
}

/**
 * Controls trace state
 */
void FaceLock::enable(bool set)
{
    gFaceLock.mRequested = set;
}

/**
 * Starts the FaceLock trace.
 */
void FaceLock::start(int frameNum)
{
    if (gFaceLock.isRequested() && !gFaceLock.isRunning()) {
        gFaceLock.formattedTrace("FaceLock", __FUNCTION__);
        gFaceLockFrame = frameNum;
        gFaceLock.start();
    }
}

/**
 * get current preview frame num
 */
void FaceLock::getCurFrameNum(const int mFrameNum)
{
    if (gFaceLock.isRunning()) {
        gFaceLockFrame = mFrameNum - gFaceLockFrame;
    }
}

/**
 * Stops the FaceLock trace and prints out results.
 */
void FaceLock::stop(int mFaceNum)
{
    if (gFaceLock.isRunning()) {
        ALOGD("FaceLock face num: %d , Need frame: %d , From preview frame got to face lock successfully:\t%lld ms\n",
             mFaceNum, gFaceLockFrame, gFaceLock.timeUs() / 1000);
        gFaceLock.mRequested = false;
        gFaceLock.stop();
    }
}

/**
 * Controls trace state
 */
void ShutterLag::enable(bool set)
{
    gShutterLag.mRequested = set;
}

/**
 * Starts the ShutterLag trace.
 */
void ShutterLag::takePictureCalled(void)
{
    if (gShutterLag.isRequested())
        gShutterLag.start();
}

/**
 * Prints ShutterLag trace results.
 */
void ShutterLag::snapshotTaken(struct timeval *ts)
{
    if (gShutterLag.isRunning()) {
        ALOGD("ShutterLag from takePicture() to shot taken:\t%lldms\n",
             (((nsecs_t(ts->tv_sec)*1000000LL
             +  nsecs_t(ts->tv_usec))
             - gShutterLag.mStartAt/1000)/1000));
    }
}

/**
 * Controls trace state
 */
void Shot2Shot::enable(bool set)
{
    gShot2Shot.mRequested = set;
}

/**
 * Starts shot2shot trace
 */
void Shot2Shot::start(void)
{
    if (gShot2Shot.isRequested()) {
        gShot2Shot.start();
        if (gPnPBreakdown.isRunning())
            PnPBreakdown::step("Shot2Shot::start");
    }
}

/**
 * Marks that take picture call has been issued.
 *
 * This is needed to reliably detect start and end of shot2shot
 * sequences.
 */
void Shot2Shot::takePictureCalled(void)
{
    if (gShot2Shot.isRunning() == true)
        stop();

    start();
}

void Shot2Shot::stop(void)
{
    if (gShot2Shot.isRunning()) {
        if (gPnPBreakdown.isRunning())
            PnPBreakdown::step("Shot2Shot::stop");
            ALOGD("shot2shot latency: %lld us.", gShot2Shot.timeUs());
        gShot2Shot.stop();
    }
}

/**
 * Controls trace state
 */

void AAAProfiler::enable(bool set)
{
    gAAAProfiler.mRequested = set;
}

/**
 * Starts the AAAprofiler trace.
 */
void AAAProfiler::start(void)
{
    if (gAAAProfiler.isRequested()) {
        gAAAProfiler.formattedTrace("gAAAProfiler", __FUNCTION__);
        gAAAProfiler.start();
    }
}

/**
 * Stops the AAAprofiler trace and prints out results.
 */
void AAAProfiler::stop(void)
{
    if (gAAAProfiler.isRunning()) {
        ALOGD("3A profiling time::\t%lldms\n",
             gAAAProfiler.timeUs() / 1000);
        gAAAProfiler.stop();
    }
}

/**
 * Controls trace state
 */
void SwitchCameras::enable(bool set)
{
    gSwitchCameras.mRequested = set;
}

/**
 * Starts the SwitchCameras trace.
 */
void SwitchCameras::start(int cameraid)
{
    if (gSwitchCameras.isRequested()) {
        if (gPnPBreakdown.isRunning())
            PnPBreakdown::step("Switch::start");
        gSwitchCamerasCalled = false;
        gSwitchCamerasOriginalVideoMode = false;
        gSwitchCamerasVideoMode = false;
        gSwitchCamerasOriginalCameraId = cameraid;
        gSwitchCameras.start();
    }
}

/**
 * Get the original mode
 */
void SwitchCameras::getOriginalMode(bool videomode)
{
    if (gSwitchCameras.isRequested())
        gSwitchCamerasOriginalVideoMode = videomode;
}

/**
 * This function will be called at the time of start preview.
 */
void SwitchCameras::called(bool videomode)
{
    if (gSwitchCameras.isRequested()) {
        gSwitchCamerasCalled = true;
        gSwitchCamerasVideoMode = videomode;
    }
}

/**
 * Stops the SwitchCameras trace and prints out results.
 */
void SwitchCameras::stop(void)
{
    if (gSwitchCameras.isRunning() && gSwitchCamerasCalled == true) {
        if (gPnPBreakdown.isRunning())
            PnPBreakdown::step("Switch::stop");
        if (gSwitchCamerasOriginalVideoMode == gSwitchCamerasVideoMode) {
            ALOGD("Using %s mode, Switch from %s camera to %s camera, SWITCH time::\t%lldms\n",
                    (gSwitchCamerasVideoMode ? "video" : "camera"),
                    ((gSwitchCamerasOriginalCameraId == 0) ? "back" : "front"),
                    ((gSwitchCamerasOriginalCameraId == 1) ? "back" : "front"),
                    gSwitchCameras.timeUs() / 1000);
        } else {
            ALOGD("Using %s camera, Switch from %s mode to %s mode, SWITCH time::\t%lldms\n",
                    ((gSwitchCamerasOriginalCameraId == 0) ? "back" : "front"),
                    (gSwitchCamerasOriginalVideoMode ? "video" : "camera"),
                    (gSwitchCamerasVideoMode ? "video" : "camera"),
                    gSwitchCameras.timeUs() / 1000);
        }
        gSwitchCamerasCalled = false;
        gSwitchCameras.stop();
    }
}

/**
 * Enable more detailed breakdown analysis that shows  how long
 * intermediate steps consumed
 */
void PnPBreakdown::enable(bool set)
{
    gPnPBreakdown.mRequested = set;
}

/**
 * Start the log breakdown performance tracer.
 */
void PnPBreakdown::start(void)
{
    if (gPnPBreakdown.isRequested()) {
        gPnPBreakdown.formattedTrace("PnPBreakdown", __FUNCTION__);
        gPnPBreakdown.start();
    }
}

/**
 * Mark an intermediate step in breakdown tracer.
 *
 * @arg func, the function name which called it.
 * @arg not, a string printed with the breakdown trace
 * @arg mFrameNum, the num of the frame got from ISP.
 */
void PnPBreakdown::step(const char *func, const char* note, const int mFrameNum)
{
    if (gPnPBreakdown.isRunning()) {
        if (!note)
            note = "";
        if (mFrameNum < 0)
            ALOGD("PnPBreakdown-step %s:%s, Time: %lld us, Diff: %lld us",
                 func, note, gPnPBreakdown.timeUs(), gPnPBreakdown.lastTimeUs());
        else
            ALOGD("PnPBreakdown-step %s:%s[%d], Time: %lld us, Diff: %lld us",
                 func, note, mFrameNum, gPnPBreakdown.timeUs(), gPnPBreakdown.lastTimeUs());
   }
}

/**
 * Stop the performance tracer.
 */
void PnPBreakdown::stop(void)
{
    if (gPnPBreakdown.isRunning()) {
        gPnPBreakdown.formattedTrace("PnPBreakdown", __FUNCTION__);
        gPnPBreakdown.stop();
    }
}

/**
 * Controls trace state
 */
void HDRShot2Preview::enable(bool set)
{
    gHDRShot2Preview.mRequested = set;
}

/**
 * Starts HDR Shot2Preview trace
 */
void HDRShot2Preview::start(void)
{
    if (gHDRShot2Preview.isRequested() && !gHDRShot2Preview.isRunning()) {
        gHDRShot2Preview.start();
    }
}

/**
 * Marks that HDR call has been issued.
 *
 * This is needed to reliably detect start and end of HDR shot2preview
 * sequences.
 */
void HDRShot2Preview::HDRCalled(void)
{
    if (gHDRShot2Preview.isRunning()) {
        gHDRCalled = true;
    }
}

void HDRShot2Preview::stop(void)
{
    if (gHDRShot2Preview.isRunning() && gHDRCalled) {
        gHDRCalled = false;
        if (gPnPBreakdown.isRunning())
            PnPBreakdown::step("HDRShot2Preview::stop");
        ALOGD("hdr shot2preview latency: %lld us", gHDRShot2Preview.timeUs());
        gHDRShot2Preview.stop();
    }
}

#else // LIBCAMERA_RD_FEATURES
void reset(void) {}

#endif // LIBCAMERA_RD_FEATURES

} // namespace PerformanceTraces
} // namespace android
