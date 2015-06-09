/*
**
** Copyright 2008, The Android Open Source Project
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

#ifndef __ANRDROID_HARDWARE_CAMERA_H__
#define __ANRDROID_HARDWARE_CAMERA_H__

#include <utils/Log.h>

#include "jni.h"
#include "JNIHelp.h"
#include "android_runtime/AndroidRuntime.h"

#include <android_runtime/android_graphics_SurfaceTexture.h>
#include <android_runtime/android_view_Surface.h>

#include <cutils/properties.h>
#include <utils/Vector.h>

#include <gui/GLConsumer.h>
#include <gui/Surface.h>
#include <camera/Camera.h>
#include <binder/IMemory.h>

using namespace android;

// WARNING! Adding another member variable or other modifications to this class
// will break ABI compatibility!

// provides persistent context for calls from native code to Java
class JNICameraContext: public CameraListener
{
public:
    JNICameraContext(JNIEnv* env, jobject weak_this, jclass clazz, const sp<Camera>& camera);
    ~JNICameraContext() { release(); }
    virtual void notify(int32_t msgType, int32_t ext1, int32_t ext2);
    virtual void postData(int32_t msgType, const sp<IMemory>& dataPtr,
                          camera_frame_metadata_t *metadata);
    virtual void postDataTimestamp(nsecs_t timestamp, int32_t msgType, const sp<IMemory>& dataPtr);
    void postMetadata(JNIEnv *env, int32_t msgType, camera_frame_metadata_t *metadata);
    void addCallbackBuffer(JNIEnv *env, jbyteArray cbb, int msgType);
    void setCallbackMode(JNIEnv *env, bool installed, bool manualMode);
    sp<Camera> getCamera() { Mutex::Autolock _l(mLock); return mCamera; }
    bool isRawImageCallbackBufferAvailable() const;
    void release();

private:
    void copyAndPost(JNIEnv* env, const sp<IMemory>& dataPtr, int msgType);
    void clearCallbackBuffers_l(JNIEnv *env, Vector<jbyteArray> *buffers);
    void clearCallbackBuffers_l(JNIEnv *env);
    jbyteArray getCallbackBuffer(JNIEnv *env, Vector<jbyteArray> *buffers, size_t bufferSize);

    jobject     mCameraJObjectWeak;     // weak reference to java object
    jclass      mCameraJClass;          // strong reference to java class
    sp<Camera>  mCamera;                // strong reference to native object
    jclass      mFaceClass;  // strong reference to Face class
    jclass      mRectClass;  // strong reference to Rect class
    Mutex       mLock;

    /*
     * Global reference application-managed raw image buffer queue.
     *
     * Manual-only mode is supported for raw image callbacks, which is
     * set whenever method addCallbackBuffer() with msgType =
     * CAMERA_MSG_RAW_IMAGE is called; otherwise, null is returned
     * with raw image callbacks.
     */
    Vector<jbyteArray> mRawImageCallbackBuffers;

    /*
     * Application-managed preview buffer queue and the flags
     * associated with the usage of the preview buffer callback.
     */
    Vector<jbyteArray> mCallbackBuffers; // Global reference application managed byte[]
    bool mManualBufferMode;              // Whether to use application managed buffers.
    bool mManualCameraCallbackSet;       // Whether the callback has been set, used to
                                         // reduce unnecessary calls to set the callback.
};

#endif //__ANRDROID_HARDWARE_CAMERA_H__
