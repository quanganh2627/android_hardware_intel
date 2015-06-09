/*
 * Copyright (c) 2014 Intel Corporation.
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

#ifndef ATOMEXTISP3A_H
#define ATOMEXTISP3A_H

#include "AtomSoc3A.h"

namespace android {

class AtomExtIsp3A : public AtomSoc3A
{

// Public member functions:
public:
    AtomExtIsp3A(int cameraId, HWControlGroup &hwcg);
    virtual ~AtomExtIsp3A();

    // Overrides AtomSoc3A
    virtual status_t setAfEnabled(bool en);
    virtual status_t setAfMode(AfMode mode);
    virtual AfMode getAfMode();

    virtual status_t setAfWindows(CameraWindow *windows, size_t numWindows, const AAAWindowInfo *convWindow = NULL);

    virtual status_t startStillAf();
    virtual status_t stopStillAf();

    virtual void setFaceDetection(bool enabled);
    // -- end AtomSoc3A overrides

    virtual status_t setAeFlashMode(FlashMode mode);
    virtual FlashMode getAeFlashMode();

// prevent copy constructor and assignment operator
private:
    AtomExtIsp3A(const AtomExtIsp3A& other);
    AtomExtIsp3A& operator=(const AtomExtIsp3A& other);

// Private data:
private:
    int mCameraId;
    IHWIspControl *mISP;
    IHWSensorControl *mSensorCI;
    IHWFlashControl *mFlashCI;
    IHWLensControl *mLensCI;
    AeMode mPublicAeMode;
    int mDrvAfMode;
    int mDrvFlashMode;
    bool mFaceDetectionActive;
};

} // namespace android

#endif // ATOMEXTISP3A_H
