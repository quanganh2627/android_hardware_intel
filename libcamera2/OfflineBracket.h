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

#ifndef ANDROID_LIBCAMERA_OFFLINEBRACKET_H
#define ANDROID_LIBCAMERA_OFFLINEBRACKET_H

#include <system/camera.h>
#include "I3AControls.h"
#include "ICameraHwControls.h"
#include "BracketManager.h"

namespace android {

class AtomISP;

class OfflineBracket : public BracketImpl {

// constructor destructor
public:
    OfflineBracket(AtomISP *atomISP, I3AControls *aaaControls, BracketManager *manager);
    virtual ~OfflineBracket();

// prevent copy constructor and assignment operator
private:
    OfflineBracket(const OfflineBracket& other);
    OfflineBracket& operator=(const OfflineBracket& other);

// public methods inherited from BracketImpl
public:
    virtual status_t init(int length, int skip);
    virtual status_t startBracketing(int *expIdFrom = NULL);
    virtual status_t stopBracketing();
    virtual status_t getSnapshot(AtomBuffer &snapshotBuf, AtomBuffer &postviewBuf);
    virtual status_t putSnapshot(AtomBuffer &snapshotBuf, AtomBuffer &postviewBuf);

// private types
private:
    enum State {
        STATE_STOPPED,
        STATE_BRACKETING,
    };

// private data
private:
    BracketManager *mManager;
    I3AControls *m3AControls;
    AtomISP *mISP;

    State mState;
    BracketingType *mBracketing;
    List<SensorAeConfig> *mBracketingParams;
    int  mBurstLength;
    int  mFpsAdaptSkip;
    unsigned int mExpectedExpId;

}; // class OfflineBracket

} // namespace android

#endif // ANDROID_LIBCAMERA_OFFLINEBRACKET_H
