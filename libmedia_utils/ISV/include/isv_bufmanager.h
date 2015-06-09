/*
 * Copyright (C) 2012 Intel Corporation.  All rights reserved.
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
 *
 */

#ifndef __ISV_BUFMANAGER_H
#define __ISV_BUFMANAGER_H

#include <utils/RefBase.h>
#include <utils/Mutex.h>
#include <utils/Errors.h>
#include "isv_worker.h"

using namespace android;

#define ISV_BUFFER_MANAGER_DEBUG 0

class ISVWorker;

class ISVBuffer
{
public:
    typedef enum {
        ISV_BUFFER_GRALLOC,
        ISV_BUFFER_METADATA,
    } ISV_BUFFERTYPE;
private:
    //FIX ME: copy from ufo gralloc.h
    typedef struct _ufo_buffer_details_t
    {
        int width;       // \see alloc_device_t::alloc
        int height;      // \see alloc_device_t::alloc
        int format;      // \see alloc_device_t::alloc
        int usage;       // \see alloc_device_t::alloc
        int name;        // flink
        uint32_t fb;     // framebuffer id
        int drmformat;   // drm format
        int pitch;       // buffer pitch (in bytes)
        int size;        // buffer size (in bytes)
        int allocWidth;  // allocated buffer width in pixels.
        int allocHeight; // allocated buffer height in lines.
        int allocOffsetX;// horizontal pixel offset to content origin within allocated buffer.
        int allocOffsetY;// vertical line offset to content origin within allocated buffer.
    } ufo_buffer_details_t;

    enum
    {
        INTEL_UFO_GRALLOC_MODULE_PERFORM_GET_BO_INFO = 6 // (buffer_handle_t, buffer_info_t*)
    };

public:
    ISVBuffer(sp<ISVWorker> worker,
            unsigned long buffer, unsigned long grallocHandle,
            uint32_t width, uint32_t height,
            uint32_t stride, uint32_t colorFormat,
            ISV_BUFFERTYPE type)
        :mWorker(worker),
        mBuffer(buffer),
        mGrallocHandle(grallocHandle),
        mWidth(width),
        mHeight(height),
        mStride(stride),
        mColorFormat(colorFormat),
        mType(type),
        mSurface(-1) {}

    ISVBuffer(sp<ISVWorker> worker,
            unsigned long buffer,
            ISV_BUFFERTYPE type)
        :mWorker(worker),
        mBuffer(buffer),
        mGrallocHandle(0),
        mWidth(0),
        mHeight(0),
        mStride(0),
        mColorFormat(0),
        mType(type),
        mSurface(-1) {}

    ~ISVBuffer();

    // init buffer info
    status_t initBufferInfo();

    // get va surface
    int32_t getSurface() { return mSurface; }
    // get buffer handle
    unsigned long getHandle() { return mBuffer; }

private:
    sp<ISVWorker> mWorker;
    unsigned long mBuffer;
    unsigned long mGrallocHandle;
    uint32_t mWidth;
    uint32_t mHeight;
    uint32_t mStride;
    uint32_t mColorFormat;
    ISV_BUFFERTYPE mType;
    int32_t mSurface;
};

class ISVBufferManager: public RefBase
{
public:
    ISVBufferManager()
        :mWorker(NULL),
        mMetaDataMode(false) {}

    ~ISVBufferManager() {}
    // set mBuffers size
    status_t setBufferCount(int32_t size);

    // register/unregister ISVBuffers to mBuffers
    status_t useBuffer(const sp<ANativeWindowBuffer> nativeBuffer);
    status_t useBuffer(unsigned long handle);
    status_t freeBuffer(unsigned long handle);

    // Map to ISVBuffer
    ISVBuffer* mapBuffer(unsigned long handle);
    // set isv worker
    void setWorker(sp<ISVWorker> worker) { mWorker = worker; }
    void setMetaDataMode(bool metaDataMode) { mMetaDataMode = metaDataMode; }
private:
    typedef enum {
        GRALLOC_BUFFER_MODE = 0,
        META_DATA_MODE = 1,
    } ISV_WORK_MODE;

    sp<ISVWorker> mWorker;
    bool mMetaDataMode;
    // VPP buffer queue
    Vector<ISVBuffer*> mBuffers;
    Mutex mBufferLock; // to protect access to mBuffers
};


#endif //#define __ISV_BUFMANAGER_H
