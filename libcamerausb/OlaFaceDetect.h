/*
**
** Copyright 2012, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#ifndef OLAFACEDETECT_H_
#define OLAFACEDETECT_H_

#include <utils/threads.h>
#include "CameraFaceDetection.h"
#include "IFaceDetector.h"
#include "MessageQueue.h"
#include "CameraCommon.h"

namespace android {
class FaceDetectorFactory;

class OlaFaceDetect : public IFaceDetector,
                      public Thread
{

private:
    static const int sMaxDetectable = MAX_DETECTABLE;
    OlaFaceDetect(IFaceDetectionListener *pAListener) ;
    virtual ~OlaFaceDetect();

public:
    virtual int getMaxFacesDetectable(){
        return sMaxDetectable;
    };
    virtual void start();
    virtual void stop(bool wait=false);
    virtual int sendFrame(CameraBuffer *img, int width, int height);

private:

    // thread message id's
    enum MessageId {

        MESSAGE_ID_EXIT = 0,            // call requestExitAndWait
        MESSAGE_ID_FRAME,

        // max number of messages
        MESSAGE_ID_MAX
    };

    //
    // message data structures
    //
    struct MessageFrame {
        CameraBuffer* img;
        int width;
        int height;
    };

    // union of all message data
    union MessageData {
        // MESSAGE_ID_FRAME
        MessageFrame frame;

    };

    // message id and message data
    struct Message {
        MessageId id;
        MessageData data;
    };
    friend class FaceDetectorFactory;
// inherited from Thread
private:
    virtual bool threadLoop();
    status_t handleFrame(MessageFrame frame);
    status_t handleExit();

// private data
private:

    MessageQueue<Message, MessageId> mMessageQueue;
    CameraFaceDetection * mFaceDetectionStruct;
    volatile bool mbRunning;
};

}

#endif /* OLAFACEDETECT_H_ */
