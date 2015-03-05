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

#ifndef ANDROID_LIBCAMERA_CONTROL_THREAD_H
#define ANDROID_LIBCAMERA_CONTROL_THREAD_H

#include <utils/threads.h>
#include <utils/Vector.h>
#include <hardware/camera.h>
#include <camera/CameraParameters.h>
#include "MessageQueue.h"
#include "PreviewThread.h"
#include "PictureThread.h"
#include "VideoThread.h"
#include "CallbacksThread.h"
#include "PipeThread.h"
#include "CameraCommon.h"
#include "IFaceDetectionListener.h"
#include "GraphicBufferAllocator.h"
namespace android {

class Callbacks;
class CameraDriver;
class BufferShareRegistry;
class IFaceDetector;
class CamGraphicBufferAllocator;

//
// ControlThread implements most of the operations defined
// by camera_device_ops_t. Refer to hardware/camera.h
// for documentation on each operation.
//
class ControlThread :
    public Thread,
    public IBufferOwner {

// constructor destructor
public:
    ControlThread(int cameraId);
    virtual ~ControlThread();

// Thread overrides
public:
    status_t requestExitAndWait();

// public methods
public:

    status_t setPreviewWindow(struct preview_stream_ops *window);

    // message callbacks
    void setCallbacks(camera_notify_callback notify_cb,
                      camera_data_callback data_cb,
                      camera_data_timestamp_callback data_cb_timestamp,
                      camera_request_memory get_memory,
                      void* user);
    void enableMsgType(int32_t msg_type);
    void disableMsgType(int32_t msg_type);
    bool msgTypeEnabled(int32_t msg_type);

    // synchronous (blocking) state machine methods
    status_t startPreview();
    status_t stopPreview();
    status_t startRecording();
    status_t stopRecording();

    int sendCommand( int32_t cmd, int32_t arg1, int32_t arg2);

    // return true if preview or recording is enabled
    bool previewEnabled();
    bool recordingEnabled();
    int storeMetaDataInVideoBuffers(int enable);

    // parameter APIs
    status_t setParameters(const char *params);
    char *getParameters();
    void putParameters(char *params);

    // snapshot (asynchronous)
    status_t takePicture();
    status_t cancelPicture();

    // autofocus commands (asynchronous)
    status_t autoFocus();
    status_t cancelAutoFocus();

    // return recording frame to driver (asynchronous)
    status_t releaseRecordingFrame(void *buff);

    // TODO: need methods to configure control thread
    // TODO: decide if configuration method should send a message

// callback methods
private:
    virtual void autoFocusDone();
    virtual void returnBuffer(CameraBuffer *buff);

// private types
private:

    // thread message id's
    enum MessageId {

        MESSAGE_ID_EXIT = 0,            // call requestExitAndWait
        MESSAGE_ID_START_PREVIEW,
        MESSAGE_ID_STOP_PREVIEW,
        MESSAGE_ID_START_RECORDING,
        MESSAGE_ID_STOP_RECORDING,
        MESSAGE_ID_TAKE_PICTURE,
        MESSAGE_ID_CANCEL_PICTURE,
        MESSAGE_ID_AUTO_FOCUS,
        MESSAGE_ID_CANCEL_AUTO_FOCUS,
        MESSAGE_ID_RELEASE_RECORDING_FRAME,
        MESSAGE_ID_RETURN_BUFFER,
        MESSAGE_ID_SET_PARAMETERS,
        MESSAGE_ID_GET_PARAMETERS,
        MESSAGE_ID_AUTO_FOCUS_DONE,
        MESSAGE_ID_COMMAND,
        MESSAGE_ID_FACES_DETECTED,
        MESSAGE_ID_STORE_META_DATA,

        // max number of messages
        MESSAGE_ID_MAX
    };

    //
    // message data structures
    //

    struct MessageReleaseRecordingFrame {
        void *buff;
    };

    struct MessageReturnBuffer {
        CameraBuffer* buff;
    };

    struct MessageSetParameters {
        char* params;
    };

    struct MessageGetParameters {
        char** params;
    };

    struct MessageCommand{
        int32_t cmd_id;
        int32_t arg1;
        int32_t arg2;
    };

    struct MessageFacesDetected {
        camera_frame_metadata_t* meta;
        CameraBuffer* buf;
    };

    struct MessageStoreMetaData {
        bool enable;
    };

    // union of all message data
    union MessageData {

        // MESSAGE_ID_RELEASE_RECORDING_FRAME
        MessageReleaseRecordingFrame releaseRecordingFrame;

        // MESSAGE_ID_RETURN_BUFFER
        MessageReturnBuffer returnBuffer;

        // MESSAGE_ID_SET_PARAMETERS
        MessageSetParameters setParameters;

        // MESSAGE_ID_GET_PARAMETERS
        MessageGetParameters getParameters;

        // MESSAGE_ID_COMMAND
        MessageCommand command;
        //MESSAGE_ID_FACES_DETECTED
        MessageFacesDetected FacesDetected;

        MessageStoreMetaData storeMetaData;
    };

    // message id and message data
    struct Message {
        MessageId id;
        MessageData data;
    };

    // thread states
    enum State {
        STATE_STOPPED,
        STATE_PREVIEW_STILL,
        STATE_PREVIEW_VIDEO,
        STATE_RECORDING,
        STATE_CAPTURE,
    };
    enum GraType {
        YUV422H_FOR_JPEG,
        NV12_FOR_VIDEO,
    };

// private methods
private:

    void initDefaultParams();

    // state machine helper functions
    status_t restartPreview(bool videoMode);
    status_t startPreviewCore(bool videoMode);
    status_t stopPreviewCore();

    status_t returnPreviewBuffer(CameraBuffer *buff);
    status_t returnVideoBuffer(CameraBuffer *buff);
    status_t returnSnapshotBuffer(CameraBuffer *buff);
    status_t returnThumbnailBuffer(CameraBuffer *buff);
    status_t returnConversionBuffer(CameraBuffer *buff);
    status_t returnGrallocBuffer(CameraBuffer *buff);
    status_t returnJpegdecBuffer(CameraBuffer *buff);
    status_t returnVPPNV12Buffer(CameraBuffer *buff);

    status_t returnCaptureBuffer(CameraBuffer *buff);

    // thread message execution functions
    status_t handleMessageExit();
    status_t handleMessageStartPreview();
    status_t handleMessageStopPreview();
    status_t handleMessageStartRecording();
    status_t handleMessageStopRecording();
    status_t handleMessageTakePicture();
    status_t handleMessageCancelPicture();
    status_t handleMessageAutoFocus();
    status_t handleMessageCancelAutoFocus();
    status_t handleMessageReleaseRecordingFrame(MessageReleaseRecordingFrame *msg);
    status_t handleMessageReturnBuffer(MessageReturnBuffer *msg);
    status_t handleMessageSetParameters(MessageSetParameters *msg);
    status_t handleMessageGetParameters(MessageGetParameters *msg);
    status_t handleMessageAutoFocusDone();
    status_t handleMessageCommand(MessageCommand* msg);
    status_t startFaceDetection();
    status_t stopFaceDetection(bool wait=false);
    status_t handleMessageFacesDetected(MessageFacesDetected* msg);
    status_t handleMessageStoreMetaData(MessageStoreMetaData* msg);

    // main message function
    status_t waitForAndExecuteMessage();

    CameraBuffer* findConversionBuffer(void *findMe);
    CameraBuffer* findGraBuffer(void *findMe);

    // dequeue buffers from driver and deliver them
    status_t dequeuePreview();
    status_t dequeueRecording();

    // parameters handling functions
    bool isParameterSet(const char* param);
    bool isThumbSupported(State state);

    status_t gatherExifInfo(const CameraParameters *params, bool flash, exif_attribute_t *exif);

    // These are parameters that can be set while the driver is running (most params can be
    // set while the driver is stopped as well).
    status_t processDynamicParameters(const CameraParameters *oldParams,
            CameraParameters *newParams);
    status_t processParamFlash(const CameraParameters *oldParams,
                CameraParameters *newParams);
    status_t processParamAELock(const CameraParameters *oldParams,
                CameraParameters *newParams);
    status_t processParamAWBLock(const CameraParameters *oldParams,
            CameraParameters *newParams);
    status_t processParamEffect(const CameraParameters *oldParams,
            CameraParameters *newParams);
    status_t processParamSceneMode(const CameraParameters *oldParams,
            CameraParameters *newParams);
    status_t processParamFocusMode(const CameraParameters *oldParams,
            CameraParameters *newParams);
    status_t processParamExpoComp(const CameraParameters *oldParams,
            CameraParameters *newParams);
    status_t processParamWhiteBalance(const CameraParameters *oldParams,
            CameraParameters *newParams);
    status_t processParamSetMeteringAreas(const CameraParameters * oldParams,
            CameraParameters * newParams);

    bool verifyCameraWindow(const CameraWindow &win);
    void preSetCameraWindows(CameraWindow* focusWindows, size_t winCount);


    // These are params that can only be set while the driver is stopped. If the parameters
    // changed while the driver is running, the driver will need to be stopped, reconfigured, and
    // restarted. Static parameters will most likely affect buffer size and/or format so buffers
    // must be deallocated and reallocated accordingly.
    status_t processStaticParameters(const CameraParameters *oldParams,
            CameraParameters *newParams);
    status_t validateParameters(const CameraParameters *params);

    status_t stopCapture();

    CameraBuffer* getFreeBuffer(){
        if(mFreeBuffers.size() == 0)
            return 0;
        CameraBuffer* ret = mFreeBuffers.editTop();
        mFreeBuffers.pop();
        return ret;
    }
    CameraBuffer* getFreeGraBuffer(GraType type){
        if(type == YUV422H_FOR_JPEG)
        {
             if(mFreeJpegBuffers.size() == 0)
                  return 0;
             CameraBuffer* ret = mFreeJpegBuffers.editTop();
             mFreeJpegBuffers.pop();
             return ret;
        }
        else if(type == NV12_FOR_VIDEO)
        {
            if(mFreeVPPOutBuffers.size() == 0)
               return 0;
            CameraBuffer* ret = mFreeVPPOutBuffers.editTop();
            mFreeVPPOutBuffers.pop();
               return ret;
        }
        else
        {
            return 0;
        }

   }

#ifdef ENABLE_INTEL_METABUFFER
            void initMetaDataBuf(IntelMetadataBuffer* metaDatabuf);
#endif
      status_t allocateGraMetaDataBuffers();
      void freeGraMetaDataBuffers();

// inherited from Thread
private:
    virtual bool threadLoop();

// private data
private:

    CameraDriver *mDriver;
    sp<PreviewThread> mPreviewThread;
    sp<PictureThread> mPictureThread;
    sp<VideoThread> mVideoThread;
    sp<PipeThread> mPipeThread;

    MessageQueue<Message, MessageId> mMessageQueue;
    State mState;
    bool mThreadRunning;
    sp<Callbacks> mCallbacks;
    sp<CallbacksThread> mCallbacksThread;

    CameraBuffer *mConversionBuffers;
    int mNumBuffers;
    Vector<CameraBuffer *> mFreeBuffers;

    CameraParameters mParameters;
    IFaceDetector* m_pFaceDetector;
    bool mFaceDetectionActive;
    bool mAutoFocusActive;
    bool mThumbSupported;

    CameraBuffer* mLastRecordJpegBuff;//to save jpeg buffer for snpashot during video
    CameraBuffer* mLastRecordingBuff;  // to save yuv buffer after jpegdec for snapshot during video
    int mCameraFormat;
    bool mStoreMetaDataInVideoBuffers;

    mutable Mutex mStateLock;

    //add for Graphic buffer allocate and free
    CamGraphicBufferAllocator * mGraphicBufAlloc;
    RenderTarget **all_targets;
    CameraBuffer *mJpegdecBufferPool;
    int mNumJpegdecBuffers;
    Vector<CameraBuffer *> mFreeJpegBuffers;

    CameraBuffer *mCallbackMidBuff;

    CameraBuffer *mVPPOutBufferPool;
    int mNumVPPOutBuffers;
    Vector<CameraBuffer *> mFreeVPPOutBuffers;

    int mDecoderedFormat;
    int mRecordformat;
    int mJpegEncoderFormat;

    CameraBuffer *yuvBuffer;
    CameraBuffer *postviewBuffer;
    CameraBuffer *interBuff;
    int driverWidth; //the actual width from camera module
    int driverHeight;//the actual height from camera module
    bool mJpegFromDriver;  //whether get jpeg file from driver for jpeg encoder
    bool mRestartdevice;  //whether need to restart the device when picture size changed

}; // class ControlThread

}; // namespace android

#endif // ANDROID_LIBCAMERA_CONTROL_THREAD_H
