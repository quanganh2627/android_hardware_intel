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

#ifndef ANDROID_LIBCAMERA_FACEDETECTOR_H
#define ANDROID_LIBCAMERA_FACEDETECTOR_H

#include <utils/threads.h>
#include <system/camera.h>
#include "MessageQueue.h"
#include "ia_face.h"

namespace android {

/**
 * the maximum number of faces detectable at the same time
 */
#define MAX_FACES_DETECTABLE 32
#define SMILE_THRESHOLD_MAX 100
#define BLINK_THRESHOLD_MAX 100
#define SMILE_THRESHOLD 48
#define BLINK_THRESHOLD 50

static const int EYE_M_THRESHOLD=307;

// Smart Shutter Parameters
enum SmartShutterMode {
    SMILE_MODE = 0,
    BLINK_MODE
};

enum SmileState {
    NO_SMILE = 0,
    SMILE,
    START_OF_SMILE
};

#define PERSONDB_FILENAME   ".PersonDB.db"
#define PERSONDB_DEFAULT_PATH   "/sdcard/DCIM"

class FaceDetector {

// constructor/destructor
public:
#ifdef ENABLE_INTEL_EXTRAS
    FaceDetector();
    ~FaceDetector();

    void setAcc(void* acc);
    int getFaces(camera_face_t *faces, int width, int height);
    void getFaceState(ia_face_state *faceStateOut, int width, int height,
                      int zoomRatio);
    int faceDetect(ia_frame *frame);
    void eyeDetect(ia_frame *frame);
    void skinTypeDetect(ia_frame *frame);
    void setSmileThreshold(int threshold);
    bool smileDetect(ia_frame *frame);
    void setBlinkThreshold(int threshold);
    bool blinkDetect(ia_frame *frame);
    status_t startFaceRecognition();
    status_t stopFaceRecognition();
    status_t clearFacesDetected();
    status_t reset();
    void faceRecognize(ia_frame *frame);
    int faceDatabaseRegister(const void* feature, int personId, int featureId,
                             int timeStamp, int condition, int checksum, int version);

    // Use FaceDBLoaderThread to load database and register to face lib.
    class FaceDBLoaderThread : public Thread {
    public:
        FaceDBLoaderThread(FaceDetector* faceDetector);
        ~FaceDBLoaderThread();

        status_t requestExitAndWait();

    private:
        virtual bool threadLoop();

        status_t loadFaceDb();

    // private data
    private:
        FaceDetector* mFaceDetector;
    };

private:
    bool isEyeMotionless(ia_coordinate leftEye, ia_coordinate rightEye, int index, int trackingID);

// private data
private:
    ia_face_state* mContext;
    int mSmileThreshold;
    int mBlinkThreshold;
    bool mFaceRecognitionRunning;
    bool mFaceDBLoaded;
    ia_acceleration mAccApi;
    ia_coordinate mPrevLeftEyeCoordinate[MAX_FACES_DETECTABLE];
    ia_coordinate mPrevRightEyeCoordinate[MAX_FACES_DETECTABLE];
    int mFaceTrackingId[MAX_FACES_DETECTABLE];    // unique face tracking ID (bigger than 0)

    mutable Mutex mLock;
    sp<FaceDBLoaderThread> mFaceDBLoaderThread;
// function stubs for building without Intel extra features
#else
    FaceDetector() {}
    ~FaceDetector() {}

    void setAcc(void* acc) {}
    int getFaces(camera_face_t *faces, int width, int height) { return 0; }
    void getFaceState(ia_face_state *faceStateOut, int width, int height,
                      int zoomRatio) {
        assert(faceStateOut != NULL);
        faceStateOut->num_faces = 0;
    }
    int faceDetect(ia_frame *frame) { return 0; }
    void eyeDetect(ia_frame *frame) {}
    void skinTypeDetect(ia_frame *frame){}
    void setSmileThreshold(int threshold) {}
    bool smileDetect(ia_frame *frame) { return false; }
    bool blinkDetect(ia_frame *frame) { return true; }
    void setBlinkThreshold(int threshold) {}
    status_t startFaceRecognition() { return UNKNOWN_ERROR; }
    status_t stopFaceRecognition() { return UNKNOWN_ERROR; }
    status_t reset() { return UNKNOWN_ERROR; }
    void faceRecognize(ia_frame *frame) {}

// Thread overrides
public:
    status_t requestExitAndWait() { return UNKNOWN_ERROR; }

// inherited from Thread
private:
    virtual bool threadLoop() { return false; }
    bool isEyeMotionless(ia_coordinate leftEye, ia_coordinate rightEye, int index, int trackingID) { return false; }
#endif

// prevent copy constructor and assignment operator
private:
    FaceDetector(const FaceDetector& other);
    FaceDetector& operator=(const FaceDetector& other);
}; // class FaceDetector

}; // namespace android

#endif // ANDROID_LIBCAMERA_FACEDETECTOR_H
