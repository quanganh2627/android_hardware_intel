/*
 * Copyright (C) 2011 The Android Open Source Project
 * Copyright (c) 2012-2014 Intel Corporation
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

#ifndef ANDROID_LIBCAMERA_PICTURE_THREAD_H
#define ANDROID_LIBCAMERA_PICTURE_THREAD_H

#include <utils/threads.h>
#include <camera.h>
#include <camera/CameraParameters.h>
#include "MessageQueue.h"
#include "AtomCommon.h"
#include "EXIFMaker.h"
#include "JpegHwEncoder.h"
#include "ScalerService.h"
#include "IAtomIspObserver.h"

namespace android {

class Callbacks;
class CallbacksThread;
class ICallbackPicture;

class PictureThread : public Thread
                     ,public IAtomIspObserver {

// constructor destructor
public:
    PictureThread(I3AControls *aaaControls, sp<ScalerService> scaler,
            sp<CallbacksThread> callbacksThread, Callbacks *callbacks,
            ICallbackPicture *pictureDone,
            int cameraId);
    virtual ~PictureThread();

// prevent copy constructor and assignment operator
private:
    PictureThread(const PictureThread& other);
    PictureThread& operator=(const PictureThread& other);

// Thread overrides
public:
    status_t requestExitAndWait();

// public types
    class MetaData {
    public:
      bool flashFired;                       /*!< whether flash was fired */
      SensorAeConfig *aeConfig;              /*!< defined in I3AControls.h */
      atomisp_makernote_info *atomispMkNote; /*!< kernel provided metadata, defined linux/atomisp.h */
      ia_binary_data *ia3AMkNote;            /*!< defined in ia_mkn_types.h */
      bool saveMirrored;                     /*!< whether to do mirroring */
      int cameraOrientation;                 /*!< camera sensor orientation */
      int currentOrientation;                /*!< Current orientation of the device */
      ia_face_state faceState;

      void free(I3AControls* aaaControls);
    };

// IAtomIspObserver overrides
public:
    virtual bool atomIspNotify(IAtomIspObserver::Message *msg, const ObserverState state);

// public methods
public:

    status_t encode(MetaData &metaData, AtomBuffer *snapshotBuf, AtomBuffer *postviewBuf = NULL, bool dataHasBeenFlushed = true);

    void getDefaultParameters(CameraParameters *params, int cameraId);
    status_t initialize(const CameraParameters &params, int zoomRatio);
    status_t allocSnapshotBuffers(const AtomBuffer& formatDescriptor,
                                int sharedBuffersNum,
                                Vector<AtomBuffer> *bufs,
                                bool registerToScaler);

    status_t allocPostviewBuffers(const AtomBuffer& formatDescriptor,
                                  int sharedBuffersNum,
                                  Vector<AtomBuffer> *bufs,
                                  bool registerToScaler);

    void setMakerNote(atomisp_makernote_info makerNote);

    status_t wait(); // wait to finish queued messages (sync)
    status_t flushBuffers();

    // Exif
    void setExifMaker(const String8& data);
    void setExifModel(const String8& data);
    void setExifSoftware(const String8& data);

// private types
private:

    // thread message id's
    enum MessageId {

        MESSAGE_ID_EXIT = 0,            // call requestExitAndWait
        MESSAGE_ID_ENCODE,
        MESSAGE_ID_ALLOC_BUFS,
        MESSAGE_ID_ALLOC_POSTVIEW_BUFS,
        MESSAGE_ID_WAIT,
        MESSAGE_ID_FLUSH,
        MESSAGE_ID_INITIALIZE,
        MESSAGE_ID_SET_MAKERNOTE,
        MESSAGE_ID_CAPTURE,

        // max number of messages
        MESSAGE_ID_MAX
    };

    //
    // message data structures
    //

    struct MessageAllocBufs {
        AtomBuffer formatDesc;      /*!< format descriptor the buffers to allocate: width, height, fourcc */
        int numBufs;                /*!< amount of buffers to allocate */
        Vector<AtomBuffer> *bufs;   /*!< Vector where to store the return buffers */
        bool registerToScaler;      /*!< whether to register buffers to scaler */
    };

    struct MessageCapture {
        AtomBuffer captureBuf; /*!< can be metadata or jpeg */
    };

    struct MessageEncode {
        AtomBuffer snapshotBuf;
        AtomBuffer postviewBuf;
        MetaData metaData;
        bool dataHasBeenFlushed;
    };

    struct MessageSetMakernote {
        atomisp_makernote_info makerNote;
    };

    struct MessageParam {
        const CameraParameters *params;
        int zoomRatio;
    };

    // union of all message data
    union MessageData {

        // MESSAGE_ID_ENCODE
        MessageEncode encode;
        MessageAllocBufs alloc;
        MessageParam param;
        MessageCapture capture;
        MessageSetMakernote maker;
    };

    // message id and message data
    struct Message {
        MessageId id;
        MessageData data;
    };

// private methods
private:

    // thread message execution functions
    status_t handleMessageExit();
    status_t handleMessageEncode(MessageEncode *encode);
    status_t handleMessageAllocBufs(MessageAllocBufs *msg);
    status_t handleMessageAllocPostviewBufs(MessageAllocBufs *msg);
    status_t handleMessageWait();
    status_t handleMessageFlush();
    status_t handleMessageInitialize(MessageParam *msg);
    status_t handleMessageCapture(MessageCapture *msg);
    status_t handleMessageSetMakernote(MessageSetMakernote *msg);

    // main message function
    status_t waitForAndExecuteMessage();

    void setupExifWithMetaData(const MetaData &metaData);
    status_t encodeToJpeg(AtomBuffer *mainBuf, AtomBuffer *thumbBuf, AtomBuffer *destBuf, bool dataHasBeenFlushed);

    status_t allocateInputBuffers(AtomBuffer& formatDescriptor, int numBufs, bool registerToScaler);
    void     freeInputBuffers();

    status_t allocatePostviewBuffers(const AtomBuffer& formatDescriptor, int numBufs, bool registerToScaler);
    void freePostviewBuffers();

    status_t allocateExifBuffer();

    void unregisterFromGpuScalerAndFree(AtomBuffer bufferArray[], int numBuffs);

    int      encodeExifAndThumbnail(AtomBuffer *thumbnail, unsigned char* exifDst);
    status_t startHwEncoding(AtomBuffer *mainBuf);
    status_t completeHwEncode(AtomBuffer *mainBuf, AtomBuffer *destBuf);
    void     encodeExif(AtomBuffer *thumBuf);
    status_t doSwEncode(AtomBuffer *mainBuf, AtomBuffer* destBuf);
    status_t scaleMainPic(AtomBuffer *mainBuf);

    uint32_t getJpegDataSize(const void* framePtr) const;
    void setupExifWithNv12Meta(AtomBuffer *mainBuf);
    status_t assembleJpeg(AtomBuffer *mainBuf, AtomBuffer *mainBuf2);

// inherited from Thread
private:
    virtual bool threadLoop();

// private data
private:

    MessageQueue<Message, MessageId> mMessageQueue;
    bool        mThreadRunning;
    Callbacks       *mCallbacks;
    sp<CallbacksThread> mCallbacksThread;
    JpegHwEncoder   *mHwCompressor;
    EXIFMaker       *mExifMaker;
    AtomBuffer      mExifBuf;
    AtomBuffer      mOutBuf;
    AtomBuffer      mThumbBuf;
    AtomBuffer      mScaledPic; /*!< Temporary local buffer where we scale the main
                                     picture (snapshot) in case is of a different
                                     resolution than the image requested by the client */
    AtomBuffer      mFirstPartBuf;

    List<AtomBuffer> mCapturePostViewBufList;
    Mutex            mCapturePostViewBufListLock; // protect mCapturePostViewBufList

    /*
     * The resolution below is set up during initialize in case the receiving buffer
     * is of a different resolution so we know we have to scale
     */
    int mPictWidth;     /*!< Width of the main snapshot to encode */
    int mPictHeight;    /*!< Height of the main snapshot to encode */
    int mPictureQuality;
    int mThumbnailQuality;

    /* Input buffers */
    AtomBuffer *mInputBufferArray;
    char       **mInputBuffDataArray;   /*!< Convenience variable. TODO remove and use mInputBufferArray */
    int        mInputBuffers;

    /* Postview buffers */
    AtomBuffer *mPostviewBufferArray;
    int mPostviewBuffers;

    sp<ScalerService> mScaler;

    int mMaxOutJpegBufSize; /*!< the max JPEG Buffer Size. This is initialized to
                                 the size of the input YUV buffer*/

    // Exif data
    String8 mExifMakerName;
    String8 mExifModelName;
    String8 mExifSoftwareName;

    // 3A controls
    I3AControls* m3AControls;
    // for flushing buffers
    ICallbackPicture *mPictureDoneCallback;
    int mCameraId;

    atomisp_makernote_info mMakerInfo;
}; // class PictureThread

}; // namespace android

#endif // ANDROID_LIBCAMERA_PICTURE_THREAD_H
