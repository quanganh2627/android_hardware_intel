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
 *
 */

#ifndef CAMERABUFFERALLOCATOR_H_
#define CAMERABUFFERALLOCATOR_H_
extern "C"
{
#include <intel_bufmgr.h>
}
#include "Callbacks.h"

namespace android {

class CameraBuffer;

/**
 * ICameraBufferAllocator allocates/deallocates, maps/unmaps buffers to be used by camera driver
 * ideally all allocators can map buffers to user pointers so
 * camera HAL only uses V4L2_MEMORY_USERPTR for all kinds of buffers
 * However, this can easily extend to support V4L2_MEMORY_DMABUF, V4L2_MEMORY_MMAP,
 * or other future possibilities.
 *
 * Another responsibility of allocators is to convert buffers to meta data buffers
 * that can be shared with video encoder if store_meta_data_in_buffers mode
 * is enabled.
 *
 * Buffer synchronization for sharing is out of scope of this class currently.
 *
 */
class ICameraBufferAllocator
{
public:
    ICameraBufferAllocator(){};
    virtual ~ICameraBufferAllocator(){};

    /**
     * Allocate memory for a buffer.
     *
     * @param buf Buffer for which memory is to be allocated
     * @param size number of bytes to be allocated
     * @param w with of the frame
     * @param h height of the frame
     * @param format fourcc value for color format
     * @return size of allocated, -1 for any error
     */
    virtual int allocateMemory(CameraBuffer* buf,
            unsigned int size, Callbacks* callbacks, int w = 0, int h =0, int format = 0) = 0;


protected:
    virtual void* map(CameraBuffer* buf) = 0;

    //by default does nothing for unmap to avoid unnecessary impl for
    // user space allocators.
    virtual void unmap(CameraBuffer* buf)
    {
    };
    /**
     * Release memory for a buffer
     * This should only be called from CameraBuffer.
     * @param buf buffer to be released
     * @return 0 if success.
     */
    virtual int releaseMemory(CameraBuffer* buf) = 0;
    /**
     * Covert the buffer to meta data stream.
     * The data is stored in buf->mCameraMem->data and ready
     * to be send to consumers.
     *
     * @param buf buffer to be converted
     * @return size of meta data stream
     */
    virtual int toMetaDataStream(CameraBuffer* buf) = 0;

    virtual bool bufferOwnsThisData (const CameraBuffer* buf, void* data) = 0;

    friend class CameraBuffer;
};

/**
 * An allocator that allocates raw camera_memory_t objects.
 *
 * It uses camera HAL callbacks to allocate IMemory objects from heap.
 *
 */
class CameraMemoryAllocator: public ICameraBufferAllocator
{
public:
    static ICameraBufferAllocator* instance();

    virtual int allocateMemory(CameraBuffer* buf,
            unsigned int size, Callbacks* callbacks, int w = 0, int h =0, int format = 0);
    virtual ~CameraMemoryAllocator();

private:

    CameraMemoryAllocator();
    virtual int releaseMemory(CameraBuffer* buf);
    virtual void* map(CameraBuffer* buf);
    virtual int toMetaDataStream(CameraBuffer* buf);
    bool bufferOwnsThisData (const CameraBuffer* buf, void* data);
};

class GEMFlinkAllocator: public ICameraBufferAllocator
{

public:
    static ICameraBufferAllocator* instance();

    virtual int allocateMemory(CameraBuffer* buf,
            unsigned int size, Callbacks* callbacks, int w = 0, int h =0, int format = 0);

    virtual ~GEMFlinkAllocator();

private:
    GEMFlinkAllocator();

    virtual void* map(CameraBuffer* buf);
    virtual void unmap(CameraBuffer* buf);
    virtual int releaseMemory(CameraBuffer* buf);
    virtual int toMetaDataStream(CameraBuffer* buf);
    bool bufferOwnsThisData (const CameraBuffer* buf, void* data);
    drm_intel_bufmgr *mDRMBufMgr;
};

};//namespace
#endif /* CAMERABUFFERALLOCATOR_H_ */
