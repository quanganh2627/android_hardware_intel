/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ANDROID_LIBCAMERA_CAMERA_DRIVER
#define ANDROID_LIBCAMERA_CAMERA_DRIVER

#include <set>
#include <utils/Timers.h>
#include <utils/Errors.h>
#include <utils/Vector.h>
#include <utils/Errors.h>
#include <utils/threads.h>
#include <camera/CameraParameters.h>
#include <JPEGDecoder.h>
#include "CameraCommon.h"

namespace android {

class Callbacks;

class CameraDriver {

// public types
public:

// constructor/destructor
public:
    CameraDriver(int cameraId);
    ~CameraDriver();

// public types
    enum Mode {
        MODE_NONE,
        MODE_PREVIEW,
        MODE_CAPTURE,
        MODE_VIDEO,
    };

    enum Effect {
        EFFECT_NONE,
        EFFECT_MONO,
        EFFECT_NEGATIVE,
        EFFECT_SOLARIZE,
        EFFECT_SEPIA,
        EFFECT_POSTERIZE,
        EFFECT_WHITEBOARD,
        EFFECT_BLACKBOARD,
        EFFECT_AQUA,
    };

    enum FlashMode {
        FLASH_MODE_OFF,
        FLASH_MODE_AUTO,
        FLASH_MODE_ON,
        FLASH_MODE_RED_EYE,
        FLASH_MODE_TORCH,
    };

    enum SceneMode {
        SCENE_MODE_AUTO,
        SCENE_MODE_ACTION,
        SCENE_MODE_PORTRAIT,
        SCENE_MODE_LANDSCAPE,
        SCENE_MODE_NIGHT,
        SCENE_MODE_NIGHT_PORTRAIT,
        SCENE_MODE_THEATRE,
        SCENE_MODE_BEACH,
        SCENE_MODE_SNOW,
        SCENE_MODE_SUNSET,
        SCENE_MODE_STEADYPHOTO,
        SCENE_MODE_FIREWORKS,
        SCENE_MODE_SPORTS,
        SCENE_MODE_PARTY,
        SCENE_MODE_CANDLELIGHT,
        SCENE_MODE_BARCODE,
    };

    enum FocusMode {
        FOCUS_DISTANCE_INFINITY,
        FOCUS_MODE_AUTO,
        FOCUS_MODE_INFINITY,
        FOCUS_MODE_MACRO,
        FOCUS_MODE_FIXED,
        FOCUS_MODE_EDOF,
        FOCUS_MODE_CONTINUOUS_VIDEO,
        FOCUS_MODE_CONTINUOUS_PICTURE,
    };

    enum WhiteBalanceMode {
        WHITE_BALANCE_AUTO,
        WHITE_BALANCE_INCANDESCENT,
        WHITE_BALANCE_FLUORESCENT,
        WHITE_BALANCE_WARM_FLUORESCENT,
        WHITE_BALANCE_DAYLIGHT,
        WHITE_BALANCE_CLOUDY_DAYLIGHT,
        WHITE_BALANCE_TWILIGHT,
        WHITE_BALANCE_SHADE,
    };

    enum PowerLineFrequency {
        DISABLE,
        FREQUENCY_50HZ,
        FREQUENCY_60HZ,
    };

    enum GraType {
        YUV422H_FOR_JPEG,
        NV12_FOR_VIDEO,
    };

// public methods
public:

    void getDefaultParameters(CameraParameters *params);

    status_t start(Mode mode,RenderTarget **all_targets,int targetBufNum);
    status_t stop();

    inline int getNumBuffers() { return NUM_DEFAULT_BUFFERS; }

    status_t getPreviewFrame(CameraBuffer **driverbuff,CameraBuffer *yuvbuff);
    status_t putPreviewFrame(CameraBuffer *buff);

    status_t getRecordingFrame(CameraBuffer **driverbuff, CameraBuffer *yuvbuff, nsecs_t *timestamp);
    status_t putRecordingFrame(CameraBuffer *buff);

    status_t setSnapshotBuffers(void *buffs, int numBuffs);
    status_t getSnapshot(CameraBuffer **driverbuff, CameraBuffer *yuvbuff);
    status_t putSnapshot(CameraBuffer *buff);
    status_t putThumbnail(CameraBuffer *buff);
    CameraBuffer* findBuffer(void* findMe) const;

    bool dataAvailable();
    bool isBufferValid(const CameraBuffer * buffer) const;

    status_t setPreviewFrameSize(int width, int height);
    status_t setPostviewFrameSize(int width, int height);
    status_t setSnapshotFrameSize(int width, int height);
    status_t setVideoFrameSize(int width, int height);
    void setBufferAllocator(ICameraBufferAllocator* alloc);


    // The camera sensor YUV format
    inline int getFormat() { return mFormat; }

    void getVideoSize(int *width, int *height);

    void getZoomRatios(Mode mode, CameraParameters *params);
    void computeZoomRatios(char *zoom_ratio, int max_count);
    void getFocusDistances(CameraParameters *params);
    status_t setZoom(int zoom);

    // EXIF params
    status_t getFNumber(unsigned int *fNumber); // format is: (numerator << 16) | denominator
    status_t getExposureInfo(CamExifExposureProgramType *exposureProgram,
                             CamExifExposureModeType *exposureMode,
                             int *exposureTime,
                             float *exposureBias,
                             int *aperture);
    status_t getBrightness(float *brightness);
    status_t getIsoSpeed(int *isoSpeed);
    status_t getMeteringMode(CamExifMeteringModeType *meteringMode);
    status_t getAWBMode(CamExifWhiteBalanceType *wbMode);
    status_t getSceneMode(CamExifSceneCaptureType *sceneMode);

    // camera hardware information
    static int getNumberOfCameras();
    static status_t getCameraInfo(int cameraId, camera_info *cameraInfo);

    status_t autoFocus();
    status_t cancelAutoFocus();

    status_t setEffect(Effect effect);
    status_t setFlashMode(FlashMode flashMode);
    status_t setSceneMode(SceneMode sceneMode);
    status_t setFocusMode(FocusMode focusModei,
                          CameraWindow *windows = 0,    // optional
                          int numWindows = 0);          // optional
    status_t setExposureModeBrightness(float expNorm, int expBias);
    status_t setWhiteBalanceMode(WhiteBalanceMode wbMode);
    status_t setAeLock(bool lock);
    status_t setAwbLock(bool lock);
    status_t setMeteringAreas(CameraWindow *windows, int numWindows);
    status_t setWbAttribute(void);
    void setCallbacks(sp<Callbacks> &callbacks) { mCallbacks = callbacks; }

// private types
private:

    static const int MAX_CAMERAS         = 8;
    static const int NUM_DEFAULT_BUFFERS = 6;
    struct FrameInfo {
        void setMax(int w, int h) { maxWidth = w; maxHeight = h; }

        int width;      // Frame width
        int height;     // Frame height
        int padding;    // Frame padded width
        int maxWidth;   // Frame maximum width
        int maxHeight;  // Frame maximum height
        int size;       // Frame size in bytes
    };

    struct Config {
        FrameInfo preview;    // preview
        FrameInfo recording;  // recording
        FrameInfo snapshot;   // snapshot
        FrameInfo postview;   // postview (thumbnail for capture)
        int num_snapshot;     // number of snapshots to take
        int zoom;             // zoom value
    };

    struct CameraSensor {
        char *devName;              // device node's name, e.g. /dev/video0
        struct camera_info info;    // camera info defined by Android
        int fd;                     // the file descriptor of device at run time

        /* more fields will be added when we find more 'per camera' data*/
    };


    struct DriverBuffer {
        CameraBuffer camBuff;
        struct v4l2_buffer vBuff; /** this will have user pointer
                                   * or handle specific to camBuff.MemoryType
                                   * current plan:
                                   * user pointer will be used for FLINK, USER,
                                   * GRALLOC, DRM_PRIME_FD
                                   * How to handle ION? preferrable also
                                   * map to user pointer
                                   */
    };

    struct DriverBufferPool {
        int numBuffers;
        int numBuffersQueued;
        CameraBuffer *thumbnail;
        DriverBuffer *bufs;
    };

    struct DriverSupportedControls {
        bool zoomAbsolute;
        bool focusAuto;
        bool focusAbsolute;
        bool tiltAbsolute;
        bool panAbsolute;
        bool exposureAutoPriority;
        bool exposureAbsolute;
        bool exposureAuto;
        bool backlightCompensation;
        bool sharpness;
        bool whiteBalanceTemperature;
        bool powerLineFrequency;
        bool gain;
        bool whiteBalanceTemperatureAuto;
        bool saturation;
        bool contrast;
        bool brightness;
        bool hue;
    };

// private methods
private:

    status_t startPreview(RenderTarget **all_targets,int targetBufNum);
    status_t stopPreview();
    status_t startRecording(RenderTarget **all_targets,int targetBufNum);
    status_t stopRecording();
    status_t startCapture(RenderTarget **all_targets,int targetBufNum);
    status_t stopCapture();

    static int enumerateCameras();
    static void cleanupCameras();
    const char* getMaxSnapShotResolution();

    // Open, Close, Configure methods
    int openDevice();
    void closeDevice();
    int configureDevice(Mode deviceMode, int w, int h, int numBuffers,RenderTarget **all_targets,int targetBufNum);
    int deconfigureDevice();
    int startDevice();
    void stopDevice();

    // Buffer methods
    status_t allocateBuffer(int fd, int index, int w, int h, int format);
    status_t allocateBuffers(int numBuffers, int w, int h, int format);
    status_t freeBuffer(int index);
    status_t freeBuffers();
    status_t queueBuffer(CameraBuffer *buff, bool init = false);
    status_t dequeueBuffer(CameraBuffer **driverbuff,CameraBuffer *yuvbuff, nsecs_t *timestamp = 0, bool forJpeg = 0);

    status_t v4l2_capture_open(const char *devName);
    status_t v4l2_capture_close(int fd);
    status_t v4l2_capture_querycap(int fd, struct v4l2_capability *cap);
    status_t v4l2_capture_queryctrl(int fd, int attribute_num);
    status_t querySupportedControls();
    status_t getZoomMaxMinValues();
    status_t getBrightnessMaxMinValues();
    void detectDeviceResolutions();
    int set_capture_mode(Mode deviceMode);
    int v4l2_capture_try_format(int fd, int *w, int *h);
    int v4l2_capture_g_framerate(int fd, float * framerate, int width, int height);
    int v4l2_capture_s_format(int fd, int w, int h);
    int set_attribute (int fd, int attribute_num,
                               const int value, const char *name);
    int set_zoom (int fd, int zoom);
    status_t setFrameInfo(FrameInfo *fi, int width, int height);
    status_t setPowerLineFrequency(PowerLineFrequency frequency);

    // private members
private:

    static int numCameras;
    static Mutex mCameraSensorLock;                             // lock to access mCameraSensor
    static struct CameraSensor *mCameraSensor[MAX_CAMERAS];     // all camera sensors in CameraDriver Class.

    Mode mMode;
    sp<Callbacks> mCallbacks;

    Config mConfig;

    struct DriverBufferPool mBufferPool;

    int mSessionId; // uniquely identify each session

    int mCameraId;

    int mFormat;

    struct DriverSupportedControls mSupportedControls;

    int mZoomMax;

    int mZoomMin;

    int mBrightMax;

    int mBrightMin;

    ICameraBufferAllocator* mBufAlloc;

    String8 mPicSizes;
    String8 mBestPicSize;
    String8 mVidSizes;
    String8 mBestVidSize;

    JpegDecoder *mJpegDecoder;
    std::set<String8> mJpegModes;

    WhiteBalanceMode mWBMode;
    int mExpBias;
}; // class CameraDriver

}; // namespace android

#endif // ANDROID_LIBCAMERA_CAMERA_DRIVER
