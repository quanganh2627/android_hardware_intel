/*
 * Copyright (C) 2012 The Android Open Source Project
 * Copyright (c) 2012 Intel Corporation
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

#ifndef ANDROID_LIBCAMERA_PANORAMA_H
#define ANDROID_LIBCAMERA_PANORAMA_H

#include <utils/threads.h>
#include <system/camera.h>
#include <camera/CameraParameters.h>
#include "MessageQueue.h"
#include "AtomCommon.h"
#include "Callbacks.h"
#include "CallbacksThread.h"
#include "I3AControls.h"
#ifdef __cplusplus
extern "C" {
#endif
#include "ia_panorama.h"
#ifdef __cplusplus
}
#endif

namespace android {

#define PANORAMA_MAX_BLURVALUE 12
// PREV_WIDTH & HEIGHT must be from the list CameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES
#define PANORAMA_DEF_PREV_WIDTH 160
#define PANORAMA_DEF_PREV_HEIGHT 120
#define PANORAMA_MAX_LIVE_PREV_WIDTH 1024
#define PANORAMA_MAX_LIVE_PREV_HEIGHT 576

class ICallbackPanorama {
public:
    ICallbackPanorama() {}
    virtual ~ICallbackPanorama() {}
    virtual void panoramaCaptureTrigger(void) = 0;
    virtual void panoramaFinalized(AtomBuffer *img, AtomBuffer *pvImg) = 0;
};

// public types
enum PanoramaState {
    PANORAMA_STOPPED = 0,
    PANORAMA_STARTED,
    PANORAMA_WAITING_FOR_SNAPSHOT,
    PANORAMA_DETECTING_OVERLAP
};

class PanoramaThread : public Thread, public IBufferOwner {
// constructor/destructor
public:
#ifdef ENABLE_INTEL_EXTRAS
    PanoramaThread(ICallbackPanorama *panoramaCallback, I3AControls *aaaControls,
                   sp<CallbacksThread> callbacksThread, Callbacks *callbacks, int cameraId);
    ~PanoramaThread();

    void getDefaultParameters(CameraParameters *intel_params, int cameraId);
    void startPanorama(void);
    void stopPanorama(bool synchronous = false);
    void startPanoramaCapture(void);
    void stopPanoramaCapture(void);
    void panoramaStitch(AtomBuffer *img, AtomBuffer *pv);
    void returnBuffer(AtomBuffer *buff);
    void setThumbnailSize(int width, int height);

    status_t reInit();
    bool detectOverlap(ia_frame *frame);
    status_t stitch(AtomBuffer *img, AtomBuffer *pv);
    status_t cancelStitch();
    void finalize(void);
    void sendFrame(AtomBuffer &buf);
    PanoramaState getState(void);
    void flush(void);

// Thread overrides
public:
    status_t requestExitAndWait();
// private types
private:
    class PanoramaStitchThread : public Thread {
    public:
        PanoramaStitchThread(Callbacks *callbacks);
        ~PanoramaStitchThread() {};
        status_t requestExitAndWait();
        status_t flush(); // processes stitches in queue
        status_t cancel(ia_panorama_state* mContext); // drops stitches in queue without processing, cancels last stitch
        status_t stitch(ia_panorama_state* mContext, AtomBuffer frame, int stitchId, int cameraId);

    // prevent copy constructor and assignment operator
    private:
        PanoramaStitchThread(const PanoramaStitchThread& other);
        PanoramaStitchThread& operator=(const PanoramaStitchThread& other);

    private:
        virtual bool threadLoop();
        status_t waitForAndExecuteMessage();

        enum MessageId {
            MESSAGE_ID_EXIT = 0, // call requestExitAndWait
            MESSAGE_ID_STITCH,
            MESSAGE_ID_FLUSH,
            MESSAGE_ID_MAX
        };

        struct MessageStitch {
            int stitchId;
            AtomBuffer img;
            ia_panorama_state* mContext;
        };

        // union of all message data
        union MessageData {
            // MESSAGE_ID_STITCH
            MessageStitch stitch;
        };

        // message id and message data
        struct Message {
            MessageId id;
            MessageData data;
        };

        status_t handleMessageStitch(MessageStitch &stitch);
        status_t handleExit();
        status_t handleFlush();
        MessageQueue<Message, MessageId> mMessageQueue;
        bool mThreadRunning;
        Callbacks *mCallbacks;
    };

    // thread message id's
    enum MessageId {

        MESSAGE_ID_EXIT = 0,            // call requestExitAndWait
        MESSAGE_ID_STITCH,
        MESSAGE_ID_FRAME,
        MESSAGE_ID_START_PANORAMA,
        MESSAGE_ID_STOP_PANORAMA,
        MESSAGE_ID_START_PANORAMA_CAPTURE,
        MESSAGE_ID_STOP_PANORAMA_CAPTURE,
        MESSAGE_ID_FINALIZE,
        MESSAGE_ID_THUMBNAILSIZE,
        MESSAGE_ID_FLUSH,

        // max number of messages
        MESSAGE_ID_MAX
    };

    //
    // message data structures
    //
    struct MessageStitch {
        int stitchId;
        AtomBuffer img;
        AtomBuffer pv;
    };

    struct MessageStopPanorama {
        bool synchronous;
    };

    struct MessageFrame {
        ia_frame frame;
    };

    struct MessageThumbnailSize {
        int width;
        int height;
    };

    // union of all message data
    union MessageData {
        // MESSAGE_ID_FRAME
        MessageStitch stitch;
        MessageStopPanorama stop;
        MessageFrame frame;
        MessageThumbnailSize thumbnail;
    };

    // message id and message data
    struct Message {
        MessageId id;
        MessageData data;
    };

// inherited from Thread
private:
    virtual bool threadLoop();

// private methods
private:
    status_t handleExit(void);
    status_t handleStitch(const MessageStitch &frame);
    status_t handleFrame(MessageFrame &frame);
    status_t handleMessageStartPanorama(void);
    status_t handleMessageStopPanorama(const MessageStopPanorama &stop);
    status_t handleMessageStartPanoramaCapture(void);
    status_t handleMessageStopPanoramaCapture(void);
    status_t handleMessageFinalize(void);
    status_t handleMessageThumbnailSize(const MessageThumbnailSize &size);
    status_t handleMessageFlush();

    // main message function
    status_t waitForAndExecuteMessage();

    bool isBlurred(int width, int dx, int dy) const;
    void fullHeightSrcForThumb(AtomBuffer &img, int &srcWidth, int &srcHeight, int &startPixel);
    void fullWidthSrcForThumb(AtomBuffer &img, int &srcWidth, int &srcHeight, int &skipLinesTop, int &skipLinesBottom);

private:
    ICallbackPanorama *mPanoramaCallback;
    ia_panorama_state* mContext; // mStitchLock is used to protect this
    MessageQueue<Message, MessageId> mMessageQueue;
    camera_panorama_metadata_t mCurrentMetadata;
    // counter for the entire panorama snapshots (to limit maximum nr. of snapshots)
    int mPanoramaTotalCount;
    int mPanoramaMaxSnapshotCount;
    bool mThreadRunning;
    bool mPanoramaWaitingForImage;
    sp<CallbacksThread> mCallbacksThread;
    Callbacks *mCallbacks;
    PanoramaState mState;
    int mPreviewWidth;
    int mPreviewHeight;
    int mThumbnailWidth;
    int mThumbnailHeight;
    sp<PanoramaStitchThread> mPanoramaStitchThread; // mStitchLock is used to protect this
    bool mStopInProgress; // mStitchLock is used to protect this
    Mutex mStitchLock; //! Protects mStopInProgress, mPanoramaStitchThread and mContext queried by different threads
#else
    // function stubs for building without Intel extra features
public:
    PanoramaThread(ICallbackPanorama *panoramaCallback, I3AControls *aaaControls,
                   sp<CallbacksThread> callbacksThread, Callbacks *callbacks, int cameraId): m3AControls(aaaControls) {}
    ~PanoramaThread() {}

    // getDefaultParameters() defined in PanoramaThread.cpp:
    void getDefaultParameters(CameraParameters *intel_params, int cameraId) {}
    void startPanorama() {}
    void stopPanorama(bool synchronous = false) {}
    void startPanoramaCapture() {}
    void stopPanoramaCapture() {}
    void panoramaStitch(AtomBuffer *img, AtomBuffer *pv) {}
    void returnBuffer(AtomBuffer *buff) {}
    void setThumbnailSize(int width, int height) {}

    status_t reInit() { return NO_ERROR; }
    bool detectOverlap(ia_frame *frame) { return false; }
    status_t stitch(AtomBuffer *img, AtomBuffer *pv) { return NO_ERROR; }
    status_t cancelStitch() { return NO_ERROR; }
    void finalize() {}
    void sendFrame(AtomBuffer &buf) {}
    PanoramaState getState() { return PANORAMA_STOPPED; }
    void flush(void) {}

// Thread overrides
public:
    status_t requestExitAndWait() { return UNKNOWN_ERROR; }
private:
    // inherited from Thread
    virtual bool threadLoop() { return false; }

#endif // ENABLE_INTEL_EXTRAS

// prevent copy constructor and assignment operator
private:
    PanoramaThread(const PanoramaThread& other);
    PanoramaThread& operator=(const PanoramaThread& other);

    int mCameraId;
    I3AControls* m3AControls;
}; // class Panorama

}; // namespace android

#endif // ANDROID_LIBCAMERA_PANORAMA_H

