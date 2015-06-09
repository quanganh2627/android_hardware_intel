/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ANDROID_LIBCAMERA_ONLINEBRACKET_H
#define ANDROID_LIBCAMERA_ONLINEBRACKET_H

#include <utils/threads.h>
#include <system/camera.h>
#include <utils/List.h>
#include <UniquePtr.h>
#include "I3AControls.h"
#include "ICameraHwControls.h"
#include "BracketManager.h"
#include "MessageQueue.h"

namespace android {

#define MAX_RETRY_COUNT 1

class AtomISP;

class OnlineBracket :
    public BracketImpl,
    public Thread {

// constructor destructor
public:
    OnlineBracket(AtomISP *atomISP, I3AControls *aaaControls, BracketManager *manager, int cameraId);
    virtual ~OnlineBracket();

// prevent copy constructor and assignment operator
private:
    OnlineBracket(const OnlineBracket& other);
    OnlineBracket& operator=(const OnlineBracket& other);

// Thread overrides
public:
    status_t requestExitAndWait();

// public methods inherited from BracketImpl
public:
    virtual status_t init(int length, int skip);
    virtual status_t startBracketing(int *expIdFrom = NULL);
    virtual status_t stopBracketing();
    virtual status_t getSnapshot(AtomBuffer &snapshotBuf, AtomBuffer &postviewBuf);
    virtual status_t putSnapshot(AtomBuffer &snapshotBuf, AtomBuffer &postviewBuf);

// inherited from Thread
private:
    virtual bool threadLoop();

// private types
private:
    // thread message id's
    enum MessageId {

        MESSAGE_ID_EXIT = 0,            // call requestExitAndWait
        MESSAGE_ID_START_BRACKETING,
        MESSAGE_ID_STOP_BRACKETING,
        MESSAGE_ID_GET_SNAPSHOT,
        MESSAGE_ID_PUT_SNAPSHOT,

        // max number of messages
        MESSAGE_ID_MAX
    };

    struct MessageCapture {
        AtomBuffer snapshotBuf;
        AtomBuffer postviewBuf;
    };

    // union of all message data
    union MessageData {
        // MESSAGE_ID_PUT_SNAPSHOT
        MessageCapture capture;
    };

    // message structure
    struct Message {
        MessageId id;
        MessageData data;
    };

    // thread states
    enum State {
        STATE_STOPPED,
        STATE_BRACKETING,
        STATE_CAPTURE
    };

// private methods
private:
    status_t applyBracketing();
    status_t applyBracketingParams();
    status_t skipFrames(int numFrames, int doBracket = 0);
    int getNumLostFrames(int frameSequenceNbr);
    void getRecoveryParams(int &skipNum, int &bracketNum);

    // main message function and message handlers
    status_t waitForAndExecuteMessage();
    status_t handleMessageStartBracketing();
    status_t handleMessageStopBracketing();
    status_t handleMessageGetSnapshot();
    status_t handleMessagePutSnapshot(MessageCapture capture);
    status_t handleExit();

// private data
private:
    BracketManager *mManager;
    I3AControls *m3AControls;
    IHWIspControl *mIspCI;
    AtomISP *mISP;

    State mState;
    int  mFpsAdaptSkip;
    int  mBurstLength;
    int  mBurstCaptureNum;
    int  mSnapshotReqNum;
    int  mBracketNum;
    int  mLastFrameSequenceNbr;
    BracketingType* mBracketing;
    List<SensorAeConfig>* mBracketingParams;
    MessageQueue<Message, MessageId> mMessageQueue;
    bool mThreadRunning;
    UniquePtr<AtomBuffer[]> mSnapshotBufs;
    UniquePtr<AtomBuffer[]> mPostviewBufs;
    int mCameraId;

}; // class OnlineBracket

} // namespace android

#endif // ANDROID_LIBCAMERA_ONLINEBRACKET_H
