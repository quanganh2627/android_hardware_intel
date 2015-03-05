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

#ifndef IFACEDETECTOR_H_
#define IFACEDETECTOR_H_

namespace android {
class CameraBuffer;
class IFaceDetectionListener;
class IFaceDetector
{
public:
    IFaceDetector(IFaceDetectionListener *pListener) : mpListener(pListener) {};
    virtual ~IFaceDetector() {};
    virtual int getMaxFacesDetectable() = 0;
    virtual void start(void) = 0;
    /**
    * This stops face detector. Face detector may still process buffer data passed
    * in previously via sendFame. Wait should be used with care because it may cause deadlock.
    * Wait is needed only if client needs confirmation FD done with the buffer received.
    */
    virtual void stop(bool wait = false) = 0;
    /**
     * Face detector will process the buffer as soon as possible and callback listener
     * Client can safely de-allocate the buffer after callback received, or -1 is receied
     * as return value of this method.
     * However, before callback received, the face detector may still use the buffer.
     * returns -1 if the buffer is not accepted.
     * otherwise return 0.
     */
    virtual int sendFrame(CameraBuffer *img, int width, int height) = 0;
protected:
    IFaceDetectionListener *mpListener;
};

}

#endif /* IFACEDETECTOR_H_ */
