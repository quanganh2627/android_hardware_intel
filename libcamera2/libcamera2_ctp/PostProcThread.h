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

#ifndef ANDROID_LIBCAMERA_POSTPROC_THREAD_H
#define ANDROID_LIBCAMERA_POSTPROC_THREAD_H

#include <utils/threads.h>
#include <time.h>
#include <camera/CameraParameters.h>
#include "IntelParameters.h"
#include "FaceDetector.h"
#include "MessageQueue.h"
#include "IFaceDetector.h"
#include "PanoramaThread.h"
#include "PreviewThread.h" // ICallbackPreview
#include "SensorThread.h"

namespace android {

class Callbacks;

class ICallbackPostProc {
public:
    ICallbackPostProc() {}
    virtual ~ICallbackPostProc() {}
    virtual void facesDetected(const ia_face_state *faceState) = 0;
    virtual void postProcCaptureTrigger() = 0;
};


class PostProcThread : public IFaceDetector,
                       public Thread,
                       public ICallbackPreview,
                       public IOrientationListener
{

// constructor/destructor
public:
    PostProcThread(ICallbackPostProc *postProcDone, PanoramaThread *panoramaThread, I3AControls *aaaControls,
                   sp<CallbacksThread> callbacksThread, Callbacks *callbacks, int cameraId);
    virtual ~PostProcThread();
    status_t init(void* isp);

// Common methods
    void getDefaultParameters(CameraParameters *params, CameraParameters *intel_parameters, int cameraId);
    void flushFrames();
    status_t setZoom(int zoomRatio);
// Thread overrides
public:
    status_t requestExitAndWait();

// ICallbackPreview overrides
public:
    virtual void previewBufferCallback(AtomBuffer *buff, ICallbackPreview::CallbackType t);

public:
    SmartShutterMode mode;
    int level;

// IFaceDetector overrides
public:
    virtual int getMaxFacesDetectable(){
        return MAX_FACES_DETECTABLE;
    };
    virtual void startFaceDetection();
    virtual void stopFaceDetection(bool wait=false);
    virtual int sendFrame(AtomBuffer *img);
    virtual void startSmartShutter(SmartShutterMode mode, int level);
    virtual void stopSmartShutter(SmartShutterMode mode);
    virtual bool isSmartRunning();
    virtual bool isSmileRunning();
    virtual int getSmileThreshold();
    virtual bool isBlinkRunning();
    virtual int getBlinkThreshold();
    virtual void captureOnTrigger();
    virtual void forceSmartCaptureTrigger();
    virtual bool isSmartCaptureTriggered();
    virtual void resetSmartCaptureTrigger();
    virtual void stopCaptureOnTrigger();
    virtual void startFaceRecognition();
    virtual void stopFaceRecognition();
    virtual void loadIspExtensions(bool videoMode);
    virtual void unloadIspExtensions();
    bool isFaceRecognitionRunning();
    int getCameraID();

// IOrientationListener
public:
    void orientationChanged(int orientation);

// private types
private:

    //smart shutter parameters structure
    struct SmartShutterParams {
        bool smartRunning;
        bool smileRunning;
        bool blinkRunning;
        bool captureOnTrigger;  // true when capture on event is necessary (smile or blink)
        bool captureTriggered;  // true when capture on event has happened already
        bool captureForced;
        int smileThreshold;
        int blinkThreshold;
    };

    // thread message id's
    enum MessageId {

        MESSAGE_ID_EXIT = 0,            // call requestExitAndWait
        MESSAGE_ID_FRAME,
        MESSAGE_ID_START_FACE_DETECTION,
        MESSAGE_ID_STOP_FACE_DETECTION,
        MESSAGE_ID_START_SMART_SHUTTER,
        MESSAGE_ID_STOP_SMART_SHUTTER,
        MESSAGE_ID_CAPTURE_ON_TRIGGER,
        MESSAGE_ID_STOP_CAPTURE_ON_TRIGGER,
        MESSAGE_ID_IS_SMILE_RUNNING,
        MESSAGE_ID_IS_SMART_CAPTURE_TRIGGERED,
        MESSAGE_ID_RESET_SMART_CAPTURE_TRIGGER,
        MESSAGE_ID_FORCE_SMART_CAPTURE_TRIGGER,
        MESSAGE_ID_GET_SMILE_THRESHOLD,
        MESSAGE_ID_IS_BLINK_RUNNING,
        MESSAGE_ID_GET_BLINK_THRESHOLD,
        MESSAGE_ID_START_FACE_RECOGNITION,
        MESSAGE_ID_STOP_FACE_RECOGNITION,
        MESSAGE_ID_IS_FACE_RECOGNITION_RUNNING,
        MESSAGE_ID_LOAD_ISP_EXTENSIONS,
        MESSAGE_ID_UNLOAD_ISP_EXTENSIONS,
        MESSAGE_ID_SET_ZOOM,
        MESSAGE_ID_SET_ROTATION,

        // max number of messages
        MESSAGE_ID_MAX
    };

    //
    // message data structures
    //
    struct MessageFrame {
        AtomBuffer img;
    };

    struct MessageConfig {
        int value;
    };

    struct MessageSmartShutter {
        int mode;
        int level;
    };

    struct MessageLoadIspExtensions {
        bool videoMode;
    };

    // union of all message data
    union MessageData {
        // MESSAGE_ID_FRAME
        MessageFrame frame;
        // MESSAGE_START_SMART_SHUTTER
        // MESSAGE_STOP_SMART_SHUTTER
        MessageSmartShutter smartShutterParam;
        // MESSAGE_ID_LOAD_ISP_EXTENSIONS
        MessageLoadIspExtensions loadIspExtensions;
        // MESSAGE_ID_SET_ZOOM
        // MESSAGE_ID_SET_ROTATION
        MessageConfig config;
    };

    // message id and message data
    struct Message {
        MessageId id;
        MessageData data;
    };

// prevent copy constructor and assignment operator
private:
    PostProcThread(const PostProcThread& other);
    PostProcThread& operator=(const PostProcThread& other);

// inherited from Thread
private:
    virtual bool threadLoop();

// private methods
private:
    status_t handleFrame(MessageFrame frame);
    status_t handleExit();
    status_t handleMessageStartFaceDetection();
    status_t handleMessageStopFaceDetection();
    status_t handleMessageStartSmartShutter(MessageSmartShutter params);
    status_t handleMessageStopSmartShutter(MessageSmartShutter params);
    status_t handleMessageCaptureOnTrigger();
    status_t handleMessageStopCaptureOnTrigger();
    status_t handleMessageIsSmileRunning();
    status_t handleMessageGetSmileThreshold();
    status_t handleMessageIsBlinkRunning();
    status_t handleMessageIsSmartCaptureTriggered();
    status_t handleMessageResetSmartCaptureTrigger();
    status_t handleMessageForceSmartCaptureTrigger();
    status_t handleMessageStartSmartShutter();
    status_t handleMessageStopSmartShutter();
    status_t handleMessageGetBlinkThreshold();
    status_t handleMessageStartFaceRecognition();
    status_t handleMessageStopFaceRecognition();
    status_t handleMessageIsFaceRecognitionRunning();
    status_t handleMessageLoadIspExtensions(const MessageLoadIspExtensions&);
    status_t handleMessageUnloadIspExtensions();
    status_t handleMessageSetZoom(MessageConfig &msg);
    status_t handleMessageSetRotation(MessageConfig &msg);

    // main message function
    status_t waitForAndExecuteMessage();

// private data
private:
    FaceDetector* mFaceDetector;
    PanoramaThread *mPanoramaThread;
    MessageQueue<Message, MessageId> mMessageQueue;
    int mLastReportedNumberOfFaces;
    Callbacks *mCallbacks;
    ICallbackPostProc* mPostProcDoneCallback;
    I3AControls *m3AControls;
    bool mThreadRunning;
    bool mFaceDetectionRunning;
    bool mFaceRecognitionRunning;
    SmartShutterParams mSmartShutter;
    void *mIspHandle;
    int mZoomRatio;
    int mRotation;
    int mCameraOrientation;
    bool mIsBackCamera;
    int mCameraId;
}; // class PostProcThread

}; // namespace android

#endif // ANDROID_LIBCAMERA_POSTPROC_THREAD_H
