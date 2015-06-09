/*
**
** Copyright 2012-2014, Intel Corporation
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

//#define LOG_NDEBUG 0
#define LOG_TAG "IntelCamera-JNI"

#include <camera/Camera.h>
#include <jni.h>
#include <JNIHelp.h>
#include <utils/Log.h>
#include <android_runtime/AndroidRuntime.h>
#include "android_hardware_Camera.h"

#include "intel_camera_extensions.h"

#include "libacc.h"

using namespace android;

class IntelCameraListener: public CameraListener
{
public:
    IntelCameraListener(JNICameraContext* aRealListener, jobject weak_this, jclass clazz);
    ~IntelCameraListener() { release();}
    void notify(int32_t msgType, int32_t ext1, int32_t ext2);
    void postData(int32_t msgType, const sp<IMemory>& dataPtr,
                  camera_frame_metadata_t *metadata) ;
    void postDataTimestamp(nsecs_t timestamp, int32_t msgType, const sp<IMemory>& dataPtr);
    sp<Camera> getCamera() { return mRealListener->getCamera();}
    void release();

private:
    JNICameraContext* mRealListener;
    jobject mCameraJObjectWeak;
    jclass mPanoramaMetadataClass;  // strong reference to PanoramaMetadata class
    jclass mPanoramaSnapshotClass;  // strong reference to PanoramaSnapshot class
    jclass mUllSnapshotClass;  // strong reference to UllSnapshot class
    jclass mSceneDetectionMetadataClass; // strong reference to SceneDetectionMetadata class
    jclass mCameraJClass;

};

struct fields_t {
    jfieldID intel_listener;
    jmethodID post_event;
    jmethodID panorama_metadata_constructor;
    jfieldID panorama_metadata_h_displacement;
    jfieldID panorama_metadata_v_displacement;
    jfieldID panorama_metadata_direction;
    jfieldID panorama_metadata_motion_blur;
    jfieldID panorama_metadata_finalization_started;
    jmethodID panorama_snapshot_constructor;
    jfieldID panorama_snapshot_metadata;
    jfieldID panorama_snapshot_snapshot;
    // Ultra-low light
    jmethodID ull_snapshot_constructor;
    jfieldID ull_id;
    jfieldID ull_snapshot_snapshot;
    // Scene Detection
    jmethodID scene_detection_metadata_constructor;
    jfieldID scene_detection_metadata_hdr;
    jfieldID scene_detection_metadata_scene;
};

static fields_t fields;

static CameraAcc* acc;
CameraAcc* get_acc()
{
    return acc;
}

extern sp<Camera> get_native_camera(JNIEnv *env, jobject thiz, struct JNICameraContext** context);

static void com_intel_camera_extensions_IntelCamera_native_setup(JNIEnv *env, jobject thiz,
        jobject weak_this, jobject cameraDevice)
{
    ALOGV("native setup");
    JNICameraContext* listener;
    sp<Camera> camera = get_native_camera(env, cameraDevice, &listener);
    if (camera == 0) return;

    jclass clazz = env->GetObjectClass(thiz);
    sp<IntelCameraListener> l = new IntelCameraListener(listener, weak_this, clazz);
    l->incStrong(thiz);
    camera->setListener(l);
    env->SetIntField(thiz, fields.intel_listener, (int)l.get());

    acc = new CameraAcc(camera);
}

static void com_intel_camera_extensions_IntelCamera_native_release(JNIEnv *env, jobject thiz)
{
    ALOGV("native_release");
    IntelCameraListener* intel_listener = reinterpret_cast<IntelCameraListener*>(env->GetIntField(thiz, fields.intel_listener));
    // Make sure we do not attempt to callback on a deleted Java object.
    env->SetIntField(thiz, fields.intel_listener, 0);
    if (intel_listener != NULL)
        // remove context to prevent further Java access
        intel_listener->decStrong(thiz);
}

static bool com_intel_camera_extensions_IntelCamera_enableIntelCamera(JNIEnv *env, jobject thiz, jobject cameraDevice)
{
    ALOGV("enableIntelCamera");
    IntelCameraListener* intel_listener = reinterpret_cast<IntelCameraListener*>(env->GetIntField(thiz, fields.intel_listener));
    sp<Camera> camera = intel_listener->getCamera();
    if (camera == NULL) {
        ALOGE("get camera handle failed");
        return false;
    }

    return camera->sendCommand(CAMERA_CMD_ENABLE_INTEL_PARAMETERS, 0, 0);
}

static void com_intel_camera_extensions_IntelCamera_startSceneDetection(JNIEnv *env, jobject thiz)
{
    ALOGV("startSceneDetection");
    IntelCameraListener* intel_listener = reinterpret_cast<IntelCameraListener*>(env->GetIntField(thiz, fields.intel_listener));
    sp<Camera> camera = intel_listener->getCamera();
    if (camera == NULL) {
        ALOGE("get camera handle failed");
        return;
    }

    camera->sendCommand(CAMERA_CMD_START_SCENE_DETECTION, 0, 0);
}

static void com_intel_camera_extensions_IntelCamera_stopSceneDetection(JNIEnv *env, jobject thiz)
{
    ALOGV("stopSceneDetection");
    IntelCameraListener* intel_listener = reinterpret_cast<IntelCameraListener*>(env->GetIntField(thiz, fields.intel_listener));
    sp<Camera> camera = intel_listener->getCamera();
    if (camera == NULL) {
        ALOGE("get camera handle failed");
        return;
    }

    camera->sendCommand(CAMERA_CMD_STOP_SCENE_DETECTION, 0, 0);
}

static void com_intel_camera_extensions_IntelCamera_startPanorama(JNIEnv *env, jobject thiz)
{
    ALOGV("startPanorama");
    IntelCameraListener* intel_listener = reinterpret_cast<IntelCameraListener*>(env->GetIntField(thiz, fields.intel_listener));
    sp<Camera> camera = intel_listener->getCamera();
    if (camera == NULL)
        return;

    if (camera->sendCommand(CAMERA_CMD_START_PANORAMA, 0, 0) != NO_ERROR) {
        jniThrowRuntimeException(env, "start panorama failed");
    }
}

static void com_intel_camera_extensions_IntelCamera_stopPanorama(JNIEnv *env, jobject thiz)
{
    ALOGV("stopPanorama");
    IntelCameraListener* intel_listener = reinterpret_cast<IntelCameraListener*>(env->GetIntField(thiz, fields.intel_listener));
    sp<Camera> camera = intel_listener->getCamera();
    if (camera == NULL)
        return;

    if (camera->sendCommand(CAMERA_CMD_STOP_PANORAMA, 0, 0) != NO_ERROR) {
        jniThrowRuntimeException(env, "stop panorama failed");
    }
}

static void com_intel_camera_extensions_IntelCamera_startSmileShutter(JNIEnv *env, jobject thiz)
{
    ALOGV("startSmileShutter");
    IntelCameraListener* intel_listener = reinterpret_cast<IntelCameraListener*>(env->GetIntField(thiz, fields.intel_listener));
    sp<Camera> camera = intel_listener->getCamera();

    if (camera == NULL) {
        ALOGE("get camera handle failed");
        return;
    }

    camera->sendCommand(CAMERA_CMD_START_SMILE_SHUTTER, 0, 0);
}


static void com_intel_camera_extensions_IntelCamera_stopSmileShutter(JNIEnv *env, jobject thiz)
{
    ALOGV("stopSmileShutter");
    IntelCameraListener* intel_listener = reinterpret_cast<IntelCameraListener*>(env->GetIntField(thiz, fields.intel_listener));
    sp<Camera> camera = intel_listener->getCamera();

    if (camera == NULL) {
        ALOGE("get camera handle failed");
        return;
    }

    camera->sendCommand(CAMERA_CMD_STOP_SMILE_SHUTTER, 0, 0);
}

static void com_intel_camera_extensions_IntelCamera_startBlinkShutter(JNIEnv *env, jobject thiz)
{
    ALOGV("startBlinkShutter");
    IntelCameraListener* intel_listener = reinterpret_cast<IntelCameraListener*>(env->GetIntField(thiz, fields.intel_listener));
    sp<Camera> camera = intel_listener->getCamera();

    if (camera == NULL) {
        ALOGE("get camera handle failed");
        return;
    }

    camera->sendCommand(CAMERA_CMD_START_BLINK_SHUTTER, 0, 0);
}


static void com_intel_camera_extensions_IntelCamera_stopBlinkShutter(JNIEnv *env, jobject thiz)
{
    ALOGV("stopBlinkShutter");
    IntelCameraListener* intel_listener = reinterpret_cast<IntelCameraListener*>(env->GetIntField(thiz, fields.intel_listener));
    sp<Camera> camera = intel_listener->getCamera();

    if (camera == NULL) {
        ALOGE("get camera handle failed");
        return;
    }

    camera->sendCommand(CAMERA_CMD_STOP_BLINK_SHUTTER, 0, 0);
}

static void com_intel_camera_extensions_IntelCamera_cancelSmartShutterPicture(JNIEnv *env, jobject thiz)
{
    ALOGV("cancelSmartShutterPicture");
    IntelCameraListener* intel_listener = reinterpret_cast<IntelCameraListener*>(env->GetIntField(thiz, fields.intel_listener));
    sp<Camera> camera = intel_listener->getCamera();

    if (camera == NULL) {
        ALOGE("get camera handle failed");
        return;
    }

    camera->sendCommand(CAMERA_CMD_CANCEL_SMART_SHUTTER_PICTURE, 0, 0);
}

static void com_intel_camera_extensions_IntelCamera_forceSmartShutterPicture(JNIEnv *env, jobject thiz)
{
    ALOGV("forceSmartShutterPicture");
    IntelCameraListener* intel_listener = reinterpret_cast<IntelCameraListener*>(env->GetIntField(thiz, fields.intel_listener));
    sp<Camera> camera = intel_listener->getCamera();

    if (camera == NULL) {
        ALOGE("get camera handle failed");
        return;
    }

    camera->sendCommand(CAMERA_CMD_FORCE_SMART_SHUTTER_PICTURE, 0, 0);
}

static void com_intel_camera_extensions_IntelCamera_startFaceRecognition(JNIEnv *env, jobject thiz)
{
    ALOGV("startFaceRecognition");
    IntelCameraListener* intel_listener = reinterpret_cast<IntelCameraListener*>(env->GetIntField(thiz, fields.intel_listener));
    sp<Camera> camera = intel_listener->getCamera();

    if (camera == NULL) {
        ALOGE("get camera handle failed");
        return;
    }

    camera->sendCommand(CAMERA_CMD_START_FACE_RECOGNITION, 0, 0);
}


static void com_intel_camera_extensions_IntelCamera_stopFaceRecognition(JNIEnv *env, jobject thiz)
{
    ALOGV("stopFaceRecognition");
    IntelCameraListener* intel_listener = reinterpret_cast<IntelCameraListener*>(env->GetIntField(thiz, fields.intel_listener));
    sp<Camera> camera = intel_listener->getCamera();

    if (camera == NULL) {
        ALOGE("get camera handle failed");
        return;
    }

    camera->sendCommand(CAMERA_CMD_STOP_FACE_RECOGNITION, 0, 0);
}

static void com_intel_camera_extensions_IntelCamera_startContinuousShooting(JNIEnv *env, jobject thiz)
{
    ALOGV("startContinuousShooting");
    IntelCameraListener* intel_listener = reinterpret_cast<IntelCameraListener*>(env->GetIntField(thiz, fields.intel_listener));
    sp<Camera> camera = intel_listener->getCamera();

    if (camera == NULL) {
        ALOGE("get camera handle failed");
        return;
    }

    camera->sendCommand(CAMERA_CMD_START_CONTINUOUS_SHOOTING, 0, 0);
}

static void com_intel_camera_extensions_IntelCamera_stopContinuousShooting(JNIEnv *env, jobject thiz)
{
    ALOGV("stopContinuousShooting");
    IntelCameraListener* intel_listener = reinterpret_cast<IntelCameraListener*>(env->GetIntField(thiz, fields.intel_listener));
    sp<Camera> camera = intel_listener->getCamera();

    if (camera == NULL) {
        ALOGE("get camera handle failed");
        return;
    }

    camera->sendCommand(CAMERA_CMD_STOP_CONTINUOUS_SHOOTING, 0, 0);
}

static void com_intel_camera_extensions_IntelCamera_pausePreviewFrameUpdate(JNIEnv *env, jobject thiz)
{
    ALOGV("pausePreviewFrameUpdate");
    IntelCameraListener* intel_listener = reinterpret_cast<IntelCameraListener*>(env->GetIntField(thiz, fields.intel_listener));
    sp<Camera> camera = intel_listener->getCamera();

    if (camera == NULL) {
        ALOGE("get camera handle failed");
        return;
    }

    camera->sendCommand(CAMERA_CMD_PAUSE_PREVIEW_FRAME_UPDATE, 0, 0);
}

static void com_intel_camera_extensions_IntelCamera_resumePreviewFrameUpdate(JNIEnv *env, jobject thiz)
{
    ALOGV("resumePreviewFrameUpdate");
    IntelCameraListener* intel_listener = reinterpret_cast<IntelCameraListener*>(env->GetIntField(thiz, fields.intel_listener));
    sp<Camera> camera = intel_listener->getCamera();

    if (camera == NULL) {
        ALOGE("get camera handle failed");
        return;
    }

    camera->sendCommand(CAMERA_CMD_RESUME_PREVIEW_FRAME_UPDATE, 0, 0);
}

static void com_intel_camera_extensions_IntelCamera_setPreviewFrameCaptureId(JNIEnv *env, jobject thiz, jint id)
{
    ALOGV("setPreviewFrameCaptureId");
    IntelCameraListener* intel_listener = reinterpret_cast<IntelCameraListener*>(env->GetIntField(thiz, fields.intel_listener));
    sp<Camera> camera = intel_listener->getCamera();

    if (camera == NULL) {
        ALOGE("get camera handle failed");
        return;
    }

    camera->sendCommand(CAMERA_CMD_SET_PREVIEW_FRAME_CAPTURE_ID, id, 0);
}

IntelCameraListener::IntelCameraListener(JNICameraContext* aRealListener, jobject weak_this, jclass clazz)
{
    ALOGV("new IntelCameraListener");
    JNIEnv *env = AndroidRuntime::getJNIEnv();
    if (env == NULL) {
        mRealListener = NULL;
        mCameraJClass = NULL;
        mPanoramaMetadataClass = NULL;
        mPanoramaSnapshotClass = NULL;
        mUllSnapshotClass = NULL;
        mCameraJObjectWeak = NULL;
        mSceneDetectionMetadataClass = NULL;

        ALOGE("getJNIEnv error, IntelCameraListener construction failed");
    } else {
        mRealListener = aRealListener;

        mCameraJClass = (jclass)env->NewGlobalRef(clazz);

        clazz = env->FindClass("com/intel/camera/extensions/IntelCamera$PanoramaMetadata");
        mPanoramaMetadataClass = (jclass) env->NewGlobalRef(clazz);

        clazz = env->FindClass("com/intel/camera/extensions/IntelCamera$PanoramaSnapshot");
        mPanoramaSnapshotClass = (jclass) env->NewGlobalRef(clazz);

        clazz = env->FindClass("com/intel/camera/extensions/IntelCamera$UllSnapshot");
        mUllSnapshotClass = (jclass) env->NewGlobalRef(clazz);

        clazz = env->FindClass("com/intel/camera/extensions/IntelCamera$SceneDetectionMetadata");
        mSceneDetectionMetadataClass = (jclass) env->NewGlobalRef(clazz);

        mCameraJObjectWeak = env->NewGlobalRef(weak_this);
    }
}

void IntelCameraListener::release()
{
    ALOGV("release IntelCameraListener");
    JNIEnv *env = AndroidRuntime::getJNIEnv();
    if(env == NULL)
        return;

    if (mCameraJClass != NULL) {
        env->DeleteGlobalRef(mCameraJClass);
        mCameraJClass = NULL;
    }

    if (mPanoramaMetadataClass != NULL) {
        env->DeleteGlobalRef(mPanoramaMetadataClass);
        mPanoramaMetadataClass = NULL;
    }

    if (mPanoramaSnapshotClass != NULL) {
        env->DeleteGlobalRef(mPanoramaSnapshotClass);
        mPanoramaSnapshotClass = NULL;
    }

    if (mUllSnapshotClass != NULL) {
        env->DeleteGlobalRef(mUllSnapshotClass);
        mUllSnapshotClass = NULL;
    }

    if (mSceneDetectionMetadataClass != NULL) {
        env->DeleteGlobalRef(mSceneDetectionMetadataClass);
        mSceneDetectionMetadataClass = NULL;
    }

    if (mCameraJObjectWeak != NULL) {
        env->DeleteGlobalRef(mCameraJObjectWeak);
        mCameraJObjectWeak = NULL;
    }

    mRealListener = NULL;
}

void IntelCameraListener::notify(int32_t msgType, int32_t ext1, int32_t ext2)
{
    ALOGV("intel notification, msgType:0%d", msgType);
    JNIEnv *env = AndroidRuntime::getJNIEnv();

    switch (msgType) {
    case CAMERA_MSG_ULL_TRIGGERED:
    case CAMERA_MSG_LOW_BATTERY:
    case CAMERA_MSG_FRAME_ID:
        if (env != NULL)
            env->CallStaticVoidMethod(mCameraJClass, fields.post_event,
                                      mCameraJObjectWeak, msgType, ext1, ext2, NULL);
        break;
    case CAMERA_MSG_ACC_POINTER:
        acc->notifyPointer(ext1, ext2);
        break;
    case CAMERA_MSG_ACC_FINISHED:
        acc->notifyFinished();
        break;
    default:
        if (mRealListener != NULL)
            mRealListener->notify(msgType, ext1, ext2);
        break;
    }
}

void IntelCameraListener::postData(int32_t msgType, const sp<IMemory>& dataPtr,
                           camera_frame_metadata_t *metadata)
{
    ssize_t offset(0);
    size_t size(0);

    if (dataPtr == NULL) {
       ALOGE("postData dataPtr is null");
        return;
    }

    sp<IMemoryHeap> heap = dataPtr->getMemory(&offset, &size);

    if (heap == NULL) {
        ALOGE("potsData data heap is null");
        return;
    }

    uint8_t *heapBase = (uint8_t*)heap->base();
    JNIEnv *env = NULL;

    if (heapBase != NULL &&
        (msgType == CAMERA_MSG_PANORAMA_METADATA || msgType == CAMERA_MSG_PANORAMA_SNAPSHOT)) {
        // Panorama-related message handlings:
        const camera_panorama_metadata* pMetadata = reinterpret_cast<const camera_panorama_metadata*>(heapBase + offset);
        const jbyte* pPic = NULL;
        env = AndroidRuntime::getJNIEnv();
        if (env == NULL)
            return;
        switch (msgType) {
        case CAMERA_MSG_PANORAMA_METADATA:
            if (pMetadata == NULL)
                ALOGE("metadata was null");
            else {
                jobject metadata = env->NewObject(mPanoramaMetadataClass, fields.panorama_metadata_constructor);
                if (metadata != NULL) {
                    env->SetIntField(metadata, fields.panorama_metadata_direction, pMetadata->direction);
                    env->SetIntField(metadata, fields.panorama_metadata_h_displacement, pMetadata->horizontal_displacement);
                    env->SetIntField(metadata, fields.panorama_metadata_v_displacement, pMetadata->vertical_displacement);
                    env->SetBooleanField(metadata, fields.panorama_metadata_motion_blur, pMetadata->motion_blur);
                    env->SetBooleanField(metadata, fields.panorama_metadata_finalization_started, pMetadata->finalization_started);
                    env->CallStaticVoidMethod(mCameraJClass, fields.post_event,
                                              mCameraJObjectWeak, msgType, 0, 0, metadata);
                    env->DeleteLocalRef(metadata);
                } else {
                    ALOGE("Couldn't allocate metadata object");
                    env->ExceptionClear();
                }
            }
            break;
        case CAMERA_MSG_PANORAMA_SNAPSHOT:
            pPic = reinterpret_cast<jbyte *>(heapBase + offset + sizeof(camera_panorama_metadata));
            if (pMetadata == NULL || pPic == NULL)
                ALOGE("Null snaphost data. pMetadata %d pPic %d", (int) pMetadata, (int) pPic);
            else {
                size_t arraySize = size - sizeof(camera_panorama_metadata);
                // construct metadata object
                jobject metadata = env->NewObject(mPanoramaMetadataClass, fields.panorama_metadata_constructor);
                jbyteArray array = env->NewByteArray(arraySize);
                jobject panoramaSnapshot = env->NewObject(mPanoramaSnapshotClass, fields.panorama_snapshot_constructor);

                if (metadata == NULL || panoramaSnapshot == NULL || array == NULL) {
                    ALOGE("Couldn't allocate panorama snapshot objects");
                    if (metadata)
                        env->DeleteLocalRef(metadata);
                    if (array)
                        env->DeleteLocalRef(array);
                    if (panoramaSnapshot)
                        env->DeleteLocalRef(panoramaSnapshot);

                    env->ExceptionClear();
                    break;
                }
                env->SetIntField(metadata, fields.panorama_metadata_direction, pMetadata->direction);
                env->SetIntField(metadata, fields.panorama_metadata_h_displacement, pMetadata->horizontal_displacement);
                env->SetIntField(metadata, fields.panorama_metadata_v_displacement, pMetadata->vertical_displacement);
                env->SetBooleanField(metadata, fields.panorama_metadata_motion_blur, pMetadata->motion_blur);
                env->SetBooleanField(metadata, fields.panorama_metadata_finalization_started, pMetadata->finalization_started);

                // set image data
                env->SetByteArrayRegion(array, 0, arraySize, pPic);
                // set callback object fields
                env->SetObjectField(panoramaSnapshot, fields.panorama_snapshot_metadata, metadata);
                env->SetObjectField(panoramaSnapshot, fields.panorama_snapshot_snapshot, array);

                // finally, we are done constructing, so call the java class
                env->CallStaticVoidMethod(mCameraJClass, fields.post_event,
                                          mCameraJObjectWeak, msgType, 0, 0, panoramaSnapshot);
                env->DeleteLocalRef(metadata);
                env->DeleteLocalRef(array);
                env->DeleteLocalRef(panoramaSnapshot);
            }
            break;
        default:
            ALOGE("Unknown data callback message.");
            break;
        }
    } else if (heapBase != NULL && msgType == CAMERA_MSG_ULL_SNAPSHOT) {
        // ULL data call back message:
        env = AndroidRuntime::getJNIEnv();
        if (env == NULL)
            return;

        // Take the ULL metadata from the start of the buffer:
        const camera_ull_metadata *ullMetadata = reinterpret_cast<const camera_ull_metadata*>(heapBase + offset);
        // ... and the pic data is after the meta in the buffer:
        const jbyte *ullPic = reinterpret_cast<jbyte*>(heapBase + offset + sizeof(camera_ull_metadata));

        if (ullPic == NULL) {
            ALOGE("Null ULL snapshot data. pPic %d", (int)ullPic);
        } else {
            size_t arraySize = size - sizeof(camera_ull_metadata);
            jbyteArray array = env->NewByteArray(arraySize);
            jobject ullSnapshot = env->NewObject(mUllSnapshotClass, fields.ull_snapshot_constructor);

            if (ullSnapshot == NULL || array == NULL) {
                ALOGE("Couldn't allocate ULL snapshot object and/or metadata ullSnapshot (%p), array (%p)", ullSnapshot, array);
                if (array)
                    env->DeleteLocalRef(array);
                if (ullSnapshot)
                    env->DeleteLocalRef(ullSnapshot);

                env->ExceptionClear();
            } else {
                // set image data
                env->SetByteArrayRegion(array, 0, arraySize, ullPic);
                // set callback object fields
                env->SetIntField(ullSnapshot, fields.ull_id, ullMetadata->id);
                env->SetObjectField(ullSnapshot, fields.ull_snapshot_snapshot, array);

                // done constructing, so call the java class
                env->CallStaticVoidMethod(mCameraJClass, fields.post_event,
                                          mCameraJObjectWeak, msgType, 0, 0, ullSnapshot);

                env->DeleteLocalRef(array);
                env->DeleteLocalRef(ullSnapshot);
            }
        }
    } else if (heapBase != NULL && msgType == CAMERA_MSG_SCENE_DETECT) {
        env = AndroidRuntime::getJNIEnv();
        if (env == NULL)
            return;

        const camera_scene_detection_metadata* pMetadatax = reinterpret_cast<const camera_scene_detection_metadata*>(heapBase + offset);
        if (pMetadatax == NULL)
            ALOGE("scene detection metadata was null");
        else {
            jobject metadata = env->NewObject(mSceneDetectionMetadataClass, fields.scene_detection_metadata_constructor);

            if (metadata == NULL) {
                ALOGE("NULL metadata for scene detection");
                env->ExceptionClear();
            } else {
                jstring scenestring =  env->NewStringUTF(pMetadatax->scene);
                env->SetObjectField(metadata, fields.scene_detection_metadata_scene, scenestring);
                env->SetBooleanField(metadata, fields.scene_detection_metadata_hdr, pMetadatax->hdr);
                env->CallStaticVoidMethod(mCameraJClass, fields.post_event, mCameraJObjectWeak, msgType, 0, 0, metadata);

                env->DeleteLocalRef(metadata);
                env->DeleteLocalRef(scenestring);
            }
        }
    } else if (heapBase != NULL && msgType == CAMERA_MSG_ACC_ARGUMENT_BUFFER) {
        acc->postArgumentBuffer(heap, heapBase, size, offset);
    } else if (heapBase != NULL && msgType == CAMERA_MSG_ACC_PREVIEW_BUFFER) {
        acc->postPreviewBuffer(heap, heapBase, size, offset);
    } else if (heapBase != NULL && msgType == CAMERA_MSG_ACC_METADATA_BUFFER) {
        acc->postMetadataBuffer(heap, heapBase, size, offset);
    } else if (mRealListener != NULL) {
        // Pass through all other messages that were not handled above
        mRealListener->postData(msgType, dataPtr, metadata);
    }
}

void IntelCameraListener::postDataTimestamp(nsecs_t timestamp, int32_t msgType, const sp<IMemory>& dataPtr)
{
    if (mRealListener != NULL)
        mRealListener->postDataTimestamp(timestamp,  msgType, dataPtr);
}

static JNINativeMethod camMethods[] = {
    { "native_setup",
      "(Ljava/lang/Object;Landroid/hardware/Camera;)V",
      (void*)com_intel_camera_extensions_IntelCamera_native_setup },
    { "native_release",
      "()V",
      (void*)com_intel_camera_extensions_IntelCamera_native_release },
    { "native_enableIntelCamera",
      "()Z",
      (void *)com_intel_camera_extensions_IntelCamera_enableIntelCamera},
    { "native_startSceneDetection",
      "()V",
      (void *)com_intel_camera_extensions_IntelCamera_startSceneDetection },
    { "native_stopSceneDetection",
      "()V",
      (void *)com_intel_camera_extensions_IntelCamera_stopSceneDetection },
    { "native_startPanorama",
      "()V",
      (void *)com_intel_camera_extensions_IntelCamera_startPanorama },
    { "native_stopPanorama",
      "()V",
      (void *)com_intel_camera_extensions_IntelCamera_stopPanorama },
    { "native_startSmileShutter",
      "()V",
      (void *)com_intel_camera_extensions_IntelCamera_startSmileShutter },
    { "native_stopSmileShutter",
      "()V",
      (void *)com_intel_camera_extensions_IntelCamera_stopSmileShutter },
    { "native_startBlinkShutter",
      "()V",
      (void *)com_intel_camera_extensions_IntelCamera_startBlinkShutter },
    { "native_stopBlinkShutter",
      "()V",
      (void *)com_intel_camera_extensions_IntelCamera_stopBlinkShutter },
    { "native_cancelSmartShutterPicture",
      "()V",
      (void *)com_intel_camera_extensions_IntelCamera_cancelSmartShutterPicture },
    { "native_forceSmartShutterPicture",
      "()V",
      (void *)com_intel_camera_extensions_IntelCamera_forceSmartShutterPicture },
    { "native_startFaceRecognition",
      "()V",
      (void *)com_intel_camera_extensions_IntelCamera_startFaceRecognition },
    { "native_stopFaceRecognition",
      "()V",
      (void *)com_intel_camera_extensions_IntelCamera_stopFaceRecognition },
    { "native_startContinuousShooting",
      "()V",
      (void *)com_intel_camera_extensions_IntelCamera_startContinuousShooting },
    { "native_stopContinuousShooting",
      "()V",
      (void *)com_intel_camera_extensions_IntelCamera_stopContinuousShooting },
    { "native_pausePreviewFrameUpdate",
      "()V",
      (void *)com_intel_camera_extensions_IntelCamera_pausePreviewFrameUpdate },
    { "native_resumePreviewFrameUpdate",
      "()V",
      (void *)com_intel_camera_extensions_IntelCamera_resumePreviewFrameUpdate },
    { "native_setPreviewFrameCaptureId",
      "(I)V",
      (void *)com_intel_camera_extensions_IntelCamera_setPreviewFrameCaptureId },
};

int register_com_intel_camera_extensions_IntelCamera(JNIEnv *env)
{
    ALOGV("regist intel camera");
    jclass clazz = env->FindClass("com/intel/camera/extensions/IntelCamera");
    jfieldID field = env->GetFieldID(clazz, "mNativeContext", "I");
    if (field != NULL) fields.intel_listener = field;
    fields.post_event = env->GetStaticMethodID(clazz, "postEventFromNative",
                                        "(Ljava/lang/Object;IIILjava/lang/Object;)V");

    clazz = env->FindClass("com/intel/camera/extensions/IntelCamera$PanoramaMetadata");
    fields.panorama_metadata_direction = env->GetFieldID(clazz, "direction", "I");
    fields.panorama_metadata_h_displacement = env->GetFieldID(clazz, "horizontalDisplacement", "I");
    fields.panorama_metadata_v_displacement = env->GetFieldID(clazz, "verticalDisplacement", "I");
    fields.panorama_metadata_motion_blur = env->GetFieldID(clazz, "motionBlur", "Z");
    fields.panorama_metadata_finalization_started = env->GetFieldID(clazz, "finalizationStarted", "Z");
    fields.panorama_metadata_constructor = env->GetMethodID(clazz, "<init>", "()V");
    if (fields.panorama_metadata_constructor == NULL) {
        ALOGE("Can't find com/intel/camera/extensions/IntelCamera$PanoramaMetadata.PanoramaMetadata()");
        return -1;
    }

    clazz = env->FindClass("com/intel/camera/extensions/IntelCamera$PanoramaSnapshot");
    fields.panorama_snapshot_metadata = env->GetFieldID(clazz, "metadataDuringSnap", "Lcom/intel/camera/extensions/IntelCamera$PanoramaMetadata;");
    fields.panorama_snapshot_snapshot = env->GetFieldID(clazz, "snapshot", "[B");
    fields.panorama_snapshot_constructor = env->GetMethodID(clazz, "<init>", "()V");
    if (fields.panorama_snapshot_constructor == NULL) {
        ALOGE("Can't find com/intel/camera/extensions/IntelCamera$PanoramaSnapshot.PanoramaSnapshot()");
        return -1;
    }

    clazz = env->FindClass("com/intel/camera/extensions/IntelCamera$UllSnapshot");
    fields.ull_id = env->GetFieldID(clazz, "id", "I");
    fields.ull_snapshot_snapshot = env->GetFieldID(clazz, "snapshot", "[B");
    fields.ull_snapshot_constructor = env->GetMethodID(clazz, "<init>", "()V");
    if (fields.ull_snapshot_constructor == NULL) {
        ALOGE("Can't find com/intel/camera/extensions/IntelCamera$UllSnapshot.UllSnapshot()");
        return -1;
    }

    clazz = env->FindClass("com/intel/camera/extensions/IntelCamera$SceneDetectionMetadata");
    fields.scene_detection_metadata_scene = env->GetFieldID(clazz, "sceneDetected", "Ljava/lang/String;");
    fields.scene_detection_metadata_hdr = env->GetFieldID(clazz, "hdr", "Z");
    fields.scene_detection_metadata_constructor = env->GetMethodID(clazz, "<init>", "()V");
    if (fields.scene_detection_metadata_constructor == NULL) {
        ALOGE("Can't find com/intel/camera/extensions/IntelCamera$SceneDetectionMetadata.SceneDetectionMetadata()");
        return -1;
    }

    return AndroidRuntime::registerNativeMethods(env, "com/intel/camera/extensions/IntelCamera",
                                                 camMethods, NELEM(camMethods));
}

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    JNIEnv* env = NULL;
    jint result = -1;

    if (vm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
        ALOGE("ERROR: GetEnv failed\n");
        goto fail;
    }
    assert(env != NULL);

    if (register_com_intel_camera_extensions_IntelCamera(env) < 0) {
        ALOGE("ERROR: native registration failed\n");
        goto fail;
    }
    result = JNI_VERSION_1_4;

fail:
    return result;
}


