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

#ifndef ANDROID_LIBCAMERA_PREVIEW_THREAD_H
#define ANDROID_LIBCAMERA_PREVIEW_THREAD_H

#include <utils/threads.h>
#include <utils/Vector.h>
#include <camera.h>
#include <camera/CameraParameters.h>
#include "MessageQueue.h"
#include "AtomCommon.h"
#include "IAtomIspObserver.h"

namespace android {

class DebugFrameRate;
class Callbacks;
class CallbacksThread;

/**
 * Maximum capacity of the vector where we store the Gfx Preview Buffers
 * This define does not control the actual number of buffers used, just
 * the maximum allowed.
 */
#define MAX_NUMBER_PREVIEW_GFX_BUFFERS      10

/**
 * \def GFX_BUFFERS_DURING_SHARED_TEXTURE_USE
 * Number of shared gfx buffers to use when 0-copy mode available
 */
#define GFX_BUFFERS_DURING_SHARED_TEXTURE_USE 6

/**
 * \def GFX_BUFFERS_DURING_OVERLAY_USE
 * Number of Gfx Buffers dequeued from window  when we render via overlay
 * In this case AtomISP is allocating its own buffers to feed the ISP  preview
 * in PreviewThread we do a rotation or memcopy from that set to the Gfx buffers
 * (the ones dequeued from the window)
 */
#define GFX_BUFFERS_DURING_OVERLAY_USE 4

/**
 * \def GFX_BUFFERS_FOR_RESERVED_USE
 * Number of Gfx Buffers to allocate and dequeue from window for rendering
 * postview frames serialized with shared-mode preview frames.
 */
#define GFX_BUFFERS_FOR_RESERVED_USE 1

/**
 * \def GFX_DEQUEUE_RETRY_COUNT
 * Number of retries when error or reserved buffer is received from native
 * window.
 */
#define GFX_DEQUEUE_RETRY_COUNT 3


/**
 * \class ICallbackPreview
 *
 *  Interface implemented by classes that want to receive preview frames after
 *  PreviewThread has finish with them
 */
class ICallbackPreview {
public:
    /**
     * \enum CallbackType
     * Types of callbacks the PreviewThread accepts
     */
    enum CallbackType {
        INPUT,          /*!< Callback is triggered before being processed by PreviewThread for every frame */
        INPUT_ONCE,     /*!< Callback is triggered only once before frame has been processed by PreviewThread (i.e. rendered) */
        OUTPUT,         /*!< Callback is triggered after PreviewThread has render the frame for every frame */
        OUTPUT_ONCE,    /*!< Callback is triggered once after PreviewThread has render the frame */
        OUTPUT_WITH_DATA/*!< Callback is triggered after PreviewThread has render the frame for every frame.
                             The AtomBuffer associated with this frame is also sent. This is how we pass the
                             preview frame to the PostProcThread */
    };

    ICallbackPreview() {}
    virtual ~ICallbackPreview() {}
    virtual void previewBufferCallback(AtomBuffer *memory, CallbackType t) = 0;
    virtual int getCameraID() = 0;
};

/**
 * class PreviewThread
 *
 * This class is in charge of configuring the preview window send by the client
 * and render the preview buffers sent by CtrlThread
 */
class PreviewThread : public Thread, public IAtomIspObserver {

// constructor destructor
public:
    PreviewThread(sp<CallbacksThread> callbacksThread, Callbacks* callbacks);
    virtual ~PreviewThread();

// prevent copy constructor and assignment operator
private:
    PreviewThread(const PreviewThread& other);
    PreviewThread& operator=(const PreviewThread& other);

// Thread overrides
public:
    status_t requestExitAndWait();

// IAtomIspObserver overrides
public:
    virtual bool atomIspNotify(IAtomIspObserver::Message *msg, const ObserverState state);

// public methods
public:
    enum PreviewState {
        STATE_STOPPED,
        STATE_NO_WINDOW,
        STATE_CONFIGURED,
        STATE_ENABLED,
        STATE_ENABLED_HIDDEN,
    };

    PreviewState getPreviewState() const;
    unsigned int getFramesDone() const { return mFramesDone; };
    status_t setPreviewState(PreviewState state);
    status_t hidePreview(struct timeval &after_frame);
    status_t setCallback(ICallbackPreview *cb, ICallbackPreview::CallbackType t);
    void getDefaultParameters(CameraParameters *params);
    bool isWindowConfigured();
    status_t preview(AtomBuffer *buff);
    status_t postview(AtomBuffer *buff, bool hidePreview = false, bool synchronous = false);
    status_t setPreviewWindow(struct preview_stream_ops *window);
    status_t setPreviewConfig(int preview_width, int preview_height,
                              int preview_cb_format, bool shared_mode = true, int buffer_count = -1);
    status_t fetchPreviewBuffers(Vector<AtomBuffer> &pvBufs);
    status_t fetchPreviewBufferGeometry(int *w, int *h, int *bpl);
    status_t returnPreviewBuffers();
    status_t flushBuffers();
    status_t enableOverlay(bool set = true, int rotation = 90);

// private types
private:

    // thread message id's
    enum MessageId {

        MESSAGE_ID_EXIT = 0,            // call requestExitAndWait
        MESSAGE_ID_PREVIEW,
        MESSAGE_ID_POSTVIEW,
        MESSAGE_ID_SET_PREVIEW_WINDOW,
        MESSAGE_ID_SET_PREVIEW_CONFIG,
        MESSAGE_ID_FETCH_PREVIEW_BUFS,
        MESSAGE_ID_RETURN_PREVIEW_BUFS,
        MESSAGE_ID_FLUSH,
        MESSAGE_ID_WINDOW_QUERY,
        MESSAGE_ID_SET_CALLBACK,
        MESSAGE_ID_FETCH_BUF_GEOMETRY,

        // max number of messages
        MESSAGE_ID_MAX
    };

    //
    // message data structures
    //

    struct MessagePreview {
        AtomBuffer buff;
        bool hide;
        bool synchronous;
    };

    struct MessageSetPreviewWindow {
        struct preview_stream_ops *window;
        bool synchronous;
    };

    struct MessageSetCallback {
        ICallbackPreview *icallback;
        ICallbackPreview::CallbackType type;
    };

    struct MessageSetPreviewConfig {
        int width;
        int height;
        int cb_format;
        int bufferCount;
        bool sharedMode;
    };

    // union of all message data
    union MessageData {

        // MESSAGE_ID_PREVIEW
        MessagePreview preview;

        // MESSAGE_ID_SET_PREVIEW_WINDOW
        MessageSetPreviewWindow setPreviewWindow;

        // MESSAGE_ID_SET_PREVIEW_CONFIG
        MessageSetPreviewConfig setPreviewConfig;

        // MESSAGE_ID_SET_CALLBACK
        MessageSetCallback setCallback;

    };

    // message id and message data
    struct Message {
        MessageId id;
        MessageData data;
    };

    enum GfxBufferOwner {
        OWNER_PREVIEWTHREAD,
        OWNER_CLIENT,
        OWNER_WINDOW
    };

    struct GfxAtomBuffer {
        GfxBufferOwner owner;
        AtomBuffer  buffer;
    };

protected:
    status_t setState(PreviewState state);
    void inputBufferCallback();
    bool outputBufferCallback(AtomBuffer *buff);

// private methods
private:
    // thread message execution functions
    status_t handleMessageExit();
    status_t handleMessageFlush();
    status_t handleMessageIsWindowConfigured();
    status_t handleMessageSetCallback(MessageSetCallback *msg);
    status_t handleSetPreviewWindow(MessageSetPreviewWindow *msg);
    status_t handleSetPreviewConfig(MessageSetPreviewConfig *msg);
    status_t handlePreview(MessagePreview *msg);
    status_t handleFetchPreviewBuffers(void);
    status_t handleReturnPreviewBuffers(void);
    status_t handlePostview(MessagePreview *msg);
    status_t handleMessageFetchBufferGeometry(void);

    // main message function
    status_t waitForAndExecuteMessage();

    // inherited from Thread
    virtual bool threadLoop();

    // Miscellaneous helper methods
    void freeLocalPreviewBuf(void);
    void allocateLocalPreviewBuf(void);
    bool checkSkipFrame(int frameNum);
    void frameDone(AtomBuffer &buff);
    status_t allocateGfxPreviewBuffers(int numberOfBuffers);
    status_t freeGfxPreviewBuffers();
    int getGfxBufferBytesPerLine();
    void padPreviewBuffer(GfxAtomBuffer* &gfx, MessagePreview* &msg);
    GfxAtomBuffer* dequeueFromWindow();
    void copyPreviewBuffer(AtomBuffer* src, AtomBuffer* dst);
    void getEffectiveDimensions(int *w, int *h);

    GfxAtomBuffer* lookForGfxBufferHandle(buffer_handle_t *);
    GfxAtomBuffer* lookForAtomBuffer(AtomBuffer *buffer);
    status_t fetchReservedBuffers(int numOfReservedBuffers);
    GfxAtomBuffer* pickReservedBuffer();



// private data
private:
    MessageQueue<Message, MessageId> mMessageQueue;
    bool mThreadRunning;
    PreviewState mState;
    mutable Mutex mStateMutex;
    typedef key_value_pair_t<ICallbackPreview::CallbackType, ICallbackPreview*> callback_pair_t;
    typedef Vector<callback_pair_t> CallbackVector;
    CallbackVector mInputBufferCb;
    CallbackVector mOutputBufferCb;
    nsecs_t         mLastFrameTs;
    unsigned int    mFramesDone;
    sp<CallbacksThread> mCallbacksThread;

    preview_stream_ops_t *mPreviewWindow;   /*!< struct passed from Service to control the native window */
    AtomBuffer          mPreviewBuf;        /*!< Local preview buffer to give to the user */
    Callbacks           *mCallbacks;
    int                 mMinUndequeued;     /*!< Minimum number frames
                                                 to keep in window */
    Vector<GfxAtomBuffer> mPreviewBuffers;    /*!< Vector with the buffers retrieved from window */
    Vector<GfxAtomBuffer> mReservedBuffers;   /*!< Vector to hold buffers reserved for postview */
    int                 mBuffersInWindow;   /*!< Number of buffers currently in the preview window */
    size_t              mNumOfPreviewBuffers;
    bool                mFetchDone;
    sp<DebugFrameRate>  mDebugFPS;          /*!< reference to the object that keeps
                                                 track of the fps */
    int mPreviewWidth;
    int mPreviewHeight;
    int mPreviewBpl;
    int mPreviewFourcc; /*!< Native preview stream pixel format (PlatformData::getPreviewFormat()) */
    int mPreviewCbFormat; /*!< Preview callback pixel format (CameraParameters::KEY_PREVIEW_FORMAT) */
    int mGfxBpl;        /*!< Gfx buffer bpl, due to hardware limitation Gfx
                          and ISP buffer bpl alignment may be mismatched. */

    bool mOverlayEnabled; /*!< */
    bool mSharedMode; /*!< true if gfx buffers are shared with AtomISP for 0-copy */
    int mRotation;   /*!< Relative rotation of the camera scan order to
                          the display attached to overlay plane */
}; // class PreviewThread

}; // namespace android

#endif // ANDROID_LIBCAMERA_PREVIEW_THREAD_H
