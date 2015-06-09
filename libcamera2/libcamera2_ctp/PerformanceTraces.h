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

#ifndef ANDROID_LIBCAMERA_PERFORMANCE_TRACES
#define ANDROID_LIBCAMERA_PERFORMANCE_TRACES

namespace android {

/**
 * \class PerformanceTraces
 *
 * Interface for managing R&D traces used for performance
 * analysis and testing.
 *
 * This interface is designed to minimize call overhead and it can
 * be disabled altogether in product builds. Calling the functions
 * from different threads is safe (no crashes), but may lead to
 * at least transient incorrect results, so the output values need
 * to be postprocessed for analysis.
 *
 * This code should be disabled in product builds.
 */
namespace PerformanceTraces {

  // this is a bit ugly but this is a compact way to define a no-op
  // implementation in case LIBCAMERA_RD_FEATURES is not set
#undef STUB_BODY
#ifdef LIBCAMERA_RD_FEATURES
#define STUB_BODY ;
#else
#define STUB_BODY {}
#endif

  class Launch2Preview {
  public:
    static void enable(bool set) STUB_BODY
    static void start(void) STUB_BODY
    static void stop(int mFrameNum) STUB_BODY
  };

  class Launch2FocusLock {
  public:
    static void enable(bool set) STUB_BODY
    static void start(void) STUB_BODY
    static void stop(void) STUB_BODY
  };

  class FaceLock {
  public:
    static void enable(bool set) STUB_BODY
    static void start(int frameNum) STUB_BODY
    static void getCurFrameNum(const int mFrameNum) STUB_BODY
    static void stop(int mFaceNum = 0) STUB_BODY
  };


  class Shot2Shot {
  public:
    static void enable(bool set) STUB_BODY
    static void start(void) STUB_BODY
    static void takePictureCalled(void) STUB_BODY
    static void stop(void) STUB_BODY
  };

  class ShutterLag {
  public:
    static void enable(bool set) STUB_BODY
    static void takePictureCalled(void) STUB_BODY
    static void snapshotTaken(struct timeval *ts) STUB_BODY
  };

  class AAAProfiler {
  public:
    static void enable(bool set) STUB_BODY
    static void start(void) STUB_BODY
    static void stop(void) STUB_BODY
  };

  class SwitchCameras {
  public:
    static void enable(bool set) STUB_BODY
    static void start(int cameraid) STUB_BODY
    static void getOriginalMode(bool videomode) STUB_BODY
    static void called(bool videomode) STUB_BODY
    static void stop(void) STUB_BODY
  };

/**
 * PnP use following to breakdown all current PI/KPIs(L2P, S2S, HDRS2P, FaceLock,
 * FocusLock, Back/FrontCameraSwitch, StillVideoModeSwitch)
 */
  class PnPBreakdown {
  public:
    static void start(void) STUB_BODY
    static void enable(bool set) STUB_BODY
    static void step(const char *func, const char* note = 0, const int mFrameNum = -1) STUB_BODY
    static void stop(void) STUB_BODY
  };

  class HDRShot2Preview {
  public:
    static void start(void) STUB_BODY
    static void enable(bool set) STUB_BODY
    static void HDRCalled(void) STUB_BODY
    static void stop(void) STUB_BODY
  };

  /**
   * Helper function to disable all the performance traces
   */
  void reset(void);

 /**
   * Helper macro to call PerformanceTraces::Breakdown::step() with
   * the proper function name, and pass additional arguments.
   *
   * @param note textual description of the trace point
   */
  #define PERFORMANCE_TRACES_BREAKDOWN_STEP(note) \
    PerformanceTraces::PnPBreakdown::step(__FUNCTION__, note)


 /**
   * Helper macro to call PerformanceTraces::Breakdown::step() with
   * the proper function name, and pass additional arguments.
   *
   * @param note textual description of the trace point
   * @param frameCounter frame id this trace relates to
   *
   * See also PERFORMANCE_TRACES_BREAKDOWN_STEP_NOPARAM()
   */
  #define PERFORMANCE_TRACES_BREAKDOWN_STEP_PARAM(note, mFrameNum) \
    PerformanceTraces::PnPBreakdown::step(__FUNCTION__, note, mFrameNum)

  #define PERFORMANCE_TRACES_BREAKDOWN_STEP_NOPARAM() \
    PerformanceTraces::PnPBreakdown::step(__FUNCTION__)

  /**
   * Helper macro to call when a take picture message
   * is actually handled.
   */
  #define PERFORMANCE_TRACES_SHOT2SHOT_TAKE_PICTURE_HANDLE() \
      do { \
          PerformanceTraces::Shot2Shot::takePictureCalled(); \
          PerformanceTraces::PnPBreakdown::step(__FUNCTION__); \
      } while(0)

  /**
   * Helper macro to call when a take picture message
   * is actually handled.
   */
  #define PERFORMANCE_TRACES_HDR_SHOT2PREVIEW_CALLED() \
      do { \
          PerformanceTraces::HDRShot2Preview::HDRCalled(); \
          PerformanceTraces::PnPBreakdown::step(__FUNCTION__); \
      } while(0)


 /**
   * Helper macro to call when takePicture HAL method is called.
   * This step is used in multiple metrics.
   */
  #define PERFORMANCE_TRACES_TAKE_PICTURE_QUEUE() \
      do { \
          PerformanceTraces::PnPBreakdown::step(__FUNCTION__);  \
          PerformanceTraces::ShutterLag::takePictureCalled(); \
      } while(0)

  /**
   * Helper macro to call when preview frame has been sent
   * to display subsystem. This step is used in multiple metrics.
   *
   * @param x preview frame counter
   */
  #define PERFORMANCE_TRACES_PREVIEW_SHOWN(x) \
      do { \
          PerformanceTraces::Launch2Preview::stop(x); \
          PerformanceTraces::HDRShot2Preview::stop(); \
          PerformanceTraces::SwitchCameras::stop(); \
          PerformanceTraces::FaceLock::start(x); \
      } while(0)

  /**
   * Helper macro to call PerformanceTraces::Shot2Shot::takePictureCalled() with
   * the proper function name.
   */
  #define PERFORMANCE_TRACES_LAUNCH_START() \
      do { \
          PerformanceTraces::PnPBreakdown::start(); \
          PerformanceTraces::Launch2FocusLock::start(); \
          PerformanceTraces::Launch2Preview::start(); \
      } while(0)

}; // ns PerformanceTraces
}; // ns android

#endif // ANDROID_LIBCAMERA_PERFORMANCE_TRACES
