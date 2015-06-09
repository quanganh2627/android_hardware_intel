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

#ifndef ANDROID_LIBCAMERA_PICTURE_THREAD_H
#define ANDROID_LIBCAMERA_PICTURE_THREAD_H

#include <utils/threads.h>
#include <camera/CameraParameters.h>
#include "MessageQueue.h"
#include "CameraCommon.h"
#include "JpegCompressor.h"
#include "JpegEncoder.h" // for EXIF
#include "VAConvertor.h"

namespace android {

class Callbacks;

class PictureThread : public Thread {

// constructor destructor
public:
    PictureThread();
    virtual ~PictureThread();

// Thread overrides
public:
    status_t requestExitAndWait();

// public types
public:

    struct Image {
        int format;
        int quality;
        int width;
        int height;
    };

    struct Config {
        Image picture;
        Image thumbnail;
        exif_attribute_t exif;
        bool jpegfromdriver;
    };

// public methods
public:

    status_t encode(CameraBuffer *snaphotBuf,CameraBuffer *interBuf=NULL,CameraBuffer *postviewBuf = NULL);
    void getDefaultParameters(CameraParameters *params);
    void setConfig(Config *config);
    status_t flushBuffers();

    void setCallbacks(sp<Callbacks> &callbacks) { mCallbacks = callbacks; }

// private types
private:

    // thread message id's
    enum MessageId {

        MESSAGE_ID_EXIT = 0,            // call requestExitAndWait
        MESSAGE_ID_ENCODE,
        MESSAGE_ID_FLUSH,

        // max number of messages
        MESSAGE_ID_MAX
    };

    //
    // message data structures
    //

    struct MessageEncode {
        CameraBuffer *snaphotBuf;
        CameraBuffer *interBuf;
        CameraBuffer *postviewBuf;
    };

    // union of all message data
    union MessageData {

        // MESSAGE_ID_ENCODE
        MessageEncode encode;
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
    status_t handleMessageFlush();

    // main message function
    status_t waitForAndExecuteMessage();

    status_t encodeToJpeg(void *mainBuf, int mainSize, void *thumbBuf, CameraBuffer *destBuf,int picture_stride,int thumbnail_stride,int alignPicHeight,int alignThumHeight);

// inherited from Thread
private:
    virtual bool threadLoop();

// private data
private:

    JpegEncoder encoder; // for EXIF
    MessageQueue<Message, MessageId> mMessageQueue;
    bool mThreadRunning;
    sp<Callbacks> mCallbacks;
    JpegCompressor compressor;
    JpegCompressor::InputBuffer mEncoderInBuf;
    JpegCompressor::OutputBuffer mEncoderOutBuf;
    unsigned char* mOutData; //temporary buffer to hold output data
    int mMaxOutDataSize;
    unsigned char* mExifBuf;//temporary buffer to hold exif data
    Config mConfig;
    VAConvertor *mVaConvertor;
    int mInputFormat;

// public data
public:

}; // class PictureThread

}; // namespace android

#endif // ANDROID_LIBCAMERA_PICTURE_THREAD_H
