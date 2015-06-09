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

#ifndef ANDROID_LIBCAMERA_BRACKETMANAGER_H
#define ANDROID_LIBCAMERA_BRACKETMANAGER_H

#include <utils/threads.h>
#include <system/camera.h>
#include <utils/List.h>
#include <UniquePtr.h>
#include "I3AControls.h"
#include "ICameraHwControls.h"
#include "MessageQueue.h"

namespace android {

#define MAX_RETRY_COUNT 1

enum BracketingMode {
    BRACKET_NONE = 0,
    BRACKET_EXPOSURE,
    BRACKET_FOCUS,
};

struct BracketingType {
    BracketingMode mode;
    float minValue;
    float maxValue;
    float currentValue;
    float step;
    UniquePtr<float[]> values;
};

class BracketManager : public Thread {

// constructor destructor
public:
    BracketManager(HWControlGroup &hwcg, I3AControls *aaaControls);
    virtual ~BracketManager();

// prevent copy constructor and assignment operator
private:
    BracketManager(const BracketManager& other);
    BracketManager& operator=(const BracketManager& other);

// Thread overrides
public:
    status_t requestExitAndWait();

// public methods
public:
    status_t initBracketing(int length, int skip, float *bracketValues = NULL);
    void setBracketMode(BracketingMode mode);
    BracketingMode getBracketMode();
    void getNextAeConfig(SensorAeConfig *aeConfig);
    status_t startBracketing();
    status_t stopBracketing();
    // wrapper for AtomISP getSnapShot() and putSnapshot()
    status_t getSnapshot(AtomBuffer &snapshotBuf, AtomBuffer &postviewBuf);
    status_t putSnapshot(AtomBuffer &snapshotBuf, AtomBuffer &postviewBuf);

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
    I3AControls *m3AControls;
    IHWIspControl *mISP;
    IHWSensorControl *mSensorCI;
    int  mFpsAdaptSkip;
    int  mBurstLength;
    int  mBurstCaptureNum;
    int  mSnapshotReqNum;
    int  mBracketNum;
    int  mLastFrameSequenceNbr;
    BracketingType mBracketing;
    List<SensorAeConfig> mBracketingParams;
    State mState;
    MessageQueue<Message, MessageId> mMessageQueue;
    bool mThreadRunning;
    UniquePtr<AtomBuffer[]> mSnapshotBufs;
    UniquePtr<AtomBuffer[]> mPostviewBufs;

}; // class BracketManager

} // namespace android

#endif // ANDROID_LIBCAMERA_BRACKETMANAGER_H
