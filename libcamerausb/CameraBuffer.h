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

#ifndef CAMERABUFFER_H_
#define CAMERABUFFER_H_
#include <hardware/camera.h>
#include <VideoVPPBase.h>
#include "CameraCommon.h"

namespace android
{

enum BufferType {
    BUFFER_TYPE_PREVIEW =0,
    BUFFER_TYPE_VIDEO,
    BUFFER_TYPE_SNAPSHOT,
    BUFFER_TYPE_THUMBNAIL,
    BUFFER_TYPE_INTERMEDIATE, //used for intermediate conversion,
                             // no need to return to driver
    BUFFER_TYPE_JPEGDEC,
    BUFFER_TYPE_VIDEOENCODER,
    BUFFER_TYPE_PREVIEWCALLBACK,
    BUFFER_TYPE_CAP,

    BUFFER_TYPE_MAX
};

class CameraBuffer;

class IBufferOwner
{
public:
    virtual void returnBuffer(CameraBuffer* buff1) = 0;
    virtual ~IBufferOwner(){};
};

class CameraDriver;
class ControlThread;
class ICameraBufferAllocator;
class GEMFlinkAllocator;
class CameraMemoryAllocator;

class CameraBuffer {

public:

    CameraBuffer();
    ~CameraBuffer();

    int getID() const;

    /**
     *  returns user space pointer to raw data.
     *  It'll automatically map buffer handle to user space if needed.
     */
    void* getData();

   /**
     *  returns size of raw data.
     */
   int getDataSize();

    /**
     * release memory allocated for this buffer.
     */
    void releaseMemory();

    /**
     *  generates a camera_memory_t object that is ready for sending
     *  to downstream (encode, preview callbacks, etc.) processing
     * @return object populated with proper video data or meta data.
     */
    camera_memory_t* getCameraMem();

    /*!< Pointer to the memory allocated by callback, used to store metadata info for recording */
    camera_memory_t *metadata_buff;

    /**
     * Processors of a buffer should  decrement reader count
     * when the buffer is no longer in use.
     *
     * Buffer will be automatically returned to driver if
     * processor count goes to zero.
     *
     * Note, as a general rule in any ref count scheme, processors
     * should always increment count before decrement. In other words,
     * a processor has to hold a reference before releasing.
     *
     * Also a processor should always hold a reference before passing the object
     * to another processor.
     */
    void decrementProcessor();

    /**
     * Processors of a buffer should increment reader count
     * as soon as it holds a reference before accessing data in the buffer.
     */
    void incrementProcessor();
    void LockGrallocData(void** addr,int* size);
    void UnLockGrallocData();
    buffer_handle_t GetGrabuffHandle();

    RenderTarget* GetRenderTargetHandle();
    int GetGraStride();
    int GetType();

private:
    //not allowed to pass buffer by value.
    //made copy constructor and assignment operator private.
    CameraBuffer(const CameraBuffer& other) ;
    void operator=(const CameraBuffer& other);

    bool hasData(void* data) const;
    void setOwner(IBufferOwner* o);
    void returnToOwner();

    camera_memory_t *mCamMem;
    int mID;                    // id for debugging data flow path
    int mDriverPrivate;         // Private to the CameraDriver class.
                                // No other classes should touch this
    IBufferOwner* mOwner;       // owner who is responsible to enqueue back
                                // to CameraDriver
    volatile int32_t mProcessorCount; //processors that currently use the buffer
    BufferType mType;
    int mFormat;                // Color format in fourcc format, which is
                                // the same as V4L2_PIX_FMT_* macros
    int mSize;                  // memory block size in bytes
    uint32_t mWidth;             //picture width
    uint32_t mHeight;            //picture height
    void* mData;                 // user pointer to real frame data
    ICameraBufferAllocator* mAlloc;
    void* mAllocPrivate;     // Allocator specific handle,
                             // gralloc handle, gem bo, etc
                             //
    //next for gralloc usage
    buffer_handle_t mGrhandle;
    struct gralloc_module_t *mGralloc_module;
    int mGraBuffSize;
    int mStride;
    RenderTarget *mDecTargetBuf;

    // Theses are special friends that need and are
    // allowed to access my private members.
    friend class CameraDriver;
    friend class ControlThread;
    friend class ICameraBufferAllocator;
    friend class GEMFlinkAllocator;
    friend class CameraMemoryAllocator;
    friend class CamGraphicBufferAllocator;
};

}//namespace

#endif /* CAMERABUFFER_H_ */
