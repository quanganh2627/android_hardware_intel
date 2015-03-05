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

#ifndef FACEDETECTORFACTORY_H_
#define FACEDETECTORFACTORY_H_
#include <utils/StrongPointer.h>
#include "IFaceDetector.h"
#include "IFaceDetectionListener.h"

namespace android
{
class FaceDetectorFactory {
public:
    /**
     * create a detector instance with given listener.
     * caller is responsible to delete the detector when it is done.
     */
    static IFaceDetector* createDetector(IFaceDetectionListener* listener)
    {
        ALOGE("%s: not implemented !!", __FUNCTION__);
        return 0;
    }

    static bool destroyDetector(IFaceDetector* d)
    {
        ALOGE("%s: not implemented !!", __FUNCTION__);
        return false;
    }
private:

    FaceDetectorFactory(){};
    virtual ~FaceDetectorFactory(){};
};

}
#endif /* FACEDETECTORFACTORY_H_ */
