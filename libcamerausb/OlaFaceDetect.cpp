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

#include "OlaFaceDetect.h"
#include <stdlib.h>
#include <system/camera.h>
#include "IFaceDetectionListener.h"
#include "CameraCommon.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "OlaFaceDetect"
//#define LOG_NDEBUG 0
#include <utils/Log.h>

namespace android {

OlaFaceDetect::OlaFaceDetect(IFaceDetectionListener *pListener) :
            IFaceDetector(pListener),
            mMessageQueue("OlaFaceDetector"),
            mFaceDetectionStruct(0),
            mbRunning(false)
{
}

OlaFaceDetect::~OlaFaceDetect()
{
    ALOGV("%s: Destroy the OlaFaceDetec\n", __func__);

    mbRunning = false;
    if (mFaceDetectionStruct)
        CameraFaceDetection_Destroy(&mFaceDetectionStruct);
    mFaceDetectionStruct = 0;

    ALOGV("%s: Destroy the OlaFaceDetec DONE.\n", __func__);
}

void OlaFaceDetect::start()
{
    ALOGV("%s: START Face Detection mFaceDetectionStruct 0x%p\n", __func__, mFaceDetectionStruct);
    int ret = 0;

    // Since clients can stop the thread asynchronously with stop(wait=false)
    // there is a chance that the thread didn't wake up to process MESSAGE_ID_EXIT.
    // In that case, let's just remove the MESSAGE_ID_EXIT message from the queue
    // and let it keep running.
    mMessageQueue.remove(MESSAGE_ID_EXIT);
    if (!mFaceDetectionStruct) {
        ret = CameraFaceDetection_Create(&mFaceDetectionStruct);
        if (!ret) {
            mbRunning = true;
            run();
        }
        ALOGV("%s: Ola Face Detection struct Created. Ret: %d struct: 0x%p", __func__,ret, mFaceDetectionStruct);
    }else{
        mbRunning = true;
        run();//restart thread
    }
}

void OlaFaceDetect::stop(bool wait)
{
    ALOGV("%s: STOP Face DEtection mFaceDetectionStruct 0x%p\n", __func__, mFaceDetectionStruct);
    Message msg;
    msg.id = MESSAGE_ID_EXIT;
    mMessageQueue.remove(MESSAGE_ID_FRAME); // flush all buffers
    mMessageQueue.send( &msg );
    if (wait) {
        requestExitAndWait();
    }else
        requestExit();
}

status_t OlaFaceDetect::handleExit()
{
    ALOGV("%s: Stop Face Detection\n", __func__);
    int ret = 0;
    mbRunning = false;
    return NO_ERROR;
}

int OlaFaceDetect::sendFrame(CameraBuffer *img, int width, int height)
{
    ALOGV("%s: sendFrame, data =%p, width=%d height=%d\n", __func__, img->getData(), width, height);
    Message msg;
    msg.id = MESSAGE_ID_FRAME;
    msg.data.frame.img = img;
    msg.data.frame.height = height;
    msg.data.frame.width = width;
    if (mMessageQueue.send(&msg) == NO_ERROR) {
        if (img != 0)
            img->incrementProcessor();
        return 0;
    }
    else
        return -1;
}

bool OlaFaceDetect::threadLoop()
{
    status_t status = NO_ERROR;
    Message msg;
    while(mbRunning)
    {
        ALOGV("getting message....");
        mMessageQueue.receive(&msg);
        ALOGV("operation message ID = %d", msg.id);
        switch (msg.id)
        {
        case MESSAGE_ID_FRAME:
            status = handleFrame(msg.data.frame);
            break;
        case MESSAGE_ID_EXIT:
            status = handleExit();
            break;
        default:
            status = INVALID_OPERATION;
            break;
        }
        if (status != NO_ERROR) {
            ALOGE("operation failed, status = %d", status);
        }
    }
    return false;
}
status_t OlaFaceDetect::handleFrame(MessageFrame frame)
{
    ALOGV("%s: Face detection executing\n", __func__);
    if (mFaceDetectionStruct == 0) return INVALID_OPERATION;

    ALOGV("%s: data =%p, width=%d height=%d\n", __func__, frame.img->getData(), frame.width, frame.height);
    int faces = CameraFaceDetection_FindFace(mFaceDetectionStruct,
            (unsigned char*) (frame.img->getData()),
            frame.width, frame.height);
    ALOGV("%s CameraFaceDetection_FindFace faces %d, %d\n", __func__, faces, mFaceDetectionStruct->numDetected);

    camera_frame_metadata_t face_metadata;
    face_metadata.number_of_faces = mFaceDetectionStruct->numDetected;
    face_metadata.faces = (camera_face_t *)mFaceDetectionStruct->detectedFaces;
    for (int i=0; i<face_metadata.number_of_faces;i++) {
        camera_face_t& face =face_metadata.faces[i];
        ALOGV("face id=%d, score =%d", face.id, face.score);
        ALOGV("rect = (%d, %d, %d, %d)",face.rect[0],face.rect[1],
                face.rect[2],face.rect[3]);
        ALOGV("mouth: (%d, %d)",face.mouth[0], face.mouth[1]);
        ALOGV("left eye: (%d, %d)", face.left_eye[0], face.left_eye[1]);
        ALOGV("right eye: (%d, %d)", face.right_eye[0], face.right_eye[1]);
    }
    //blocking call
    ALOGV("%s calling listener", __func__);
    mpListener->facesDetected(face_metadata, frame.img);
    frame.img>decrementReader();
    ALOGV("%s returned from listener", __func__);

    return NO_ERROR;
}

}
