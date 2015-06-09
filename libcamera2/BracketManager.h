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

#include <system/camera.h>
#include <utils/List.h>
#include <UniquePtr.h>
#include "I3AControls.h"
#include "ICameraHwControls.h"

namespace android {

enum BracketingMode {
    BRACKET_NONE = 0,
    BRACKET_EXPOSURE,
    BRACKET_FOCUS,
};

enum BracketImplMethod {
    IMPL_OFFLINE = 0,
    IMPL_ONLINE,
};

struct BracketingType {
    BracketingMode mode;
    float minValue;
    float maxValue;
    float currentValue;
    float step;
    UniquePtr<float[]> values;
};

class AtomISP;

class BracketImpl {
// constructor destructor
public:
    virtual ~BracketImpl() {};

// public methods
public:
    virtual status_t init(int length, int skip) = 0;
    virtual status_t startBracketing(int *expIdFrom = NULL) = 0;
    virtual status_t stopBracketing() = 0;
    virtual status_t getSnapshot(AtomBuffer &snapshotBuf, AtomBuffer &postviewBuf) = 0;
    virtual status_t putSnapshot(AtomBuffer &snapshotBuf, AtomBuffer &postviewBuf) = 0;
}; // class BracketImpl

class BracketManager {

// constructor destructor
public:
    BracketManager(AtomISP *atomISP, I3AControls *aaaControls, int cameraId);
    virtual ~BracketManager();

// prevent copy constructor and assignment operator
private:
    BracketManager(const BracketManager& other);
    BracketManager& operator=(const BracketManager& other);

// public methods
public:
    status_t initBracketing(int length, int skip, BracketImplMethod method, float *bracketValues = NULL);
    void setBracketMode(BracketingMode mode);
    BracketingMode getBracketMode();
    void getNextAeConfig(SensorAeConfig *aeConfig);
    status_t startBracketing(int *expIdFrom = NULL);
    status_t stopBracketing();
    // wrapper for AtomISP getSnapShot() and putSnapshot()
    status_t getSnapshot(AtomBuffer &snapshotBuf, AtomBuffer &postviewBuf);
    status_t putSnapshot(AtomBuffer &snapshotBuf, AtomBuffer &postviewBuf);

// private types
private:
    // thread states
    enum State {
        STATE_STOPPED,
        STATE_BRACKETING,
    };

    friend class OnlineBracket;
    friend class OfflineBracket;
// private methods
private:
    status_t createImpl(BracketImplMethod method);
    void destroyImpl();

// private data
private:
    I3AControls *m3AControls;
    AtomISP *mISP;

    State mState;
    BracketingType mBracketing;
    BracketImplMethod mImplMethod;
    BracketImpl *mBracketImpl;
    List<SensorAeConfig> mBracketingParams;
    int  mBurstLength;
    int mCameraId;
}; // class BracketManager

} // namespace android

#endif // ANDROID_LIBCAMERA_BRACKETMANAGER_H
