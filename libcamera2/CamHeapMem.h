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

#ifndef CAMHEAPMEM_H_
#define CAMHEAPMEM_H_

#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include "LogHelper.h"

namespace android {

/**
 * Have to copy this private class here because we want to access the MemoryBase object
 * which is hiding in the handle of camera_memory_t
 * It's for camera recording to share MemoryHeap buffer and zero-copy preview
 * callbacks.
 */
class CameraHeapMemory : public RefBase {
public:
    CameraHeapMemory(int fd, size_t buf_size, uint_t num_buffers) :
        mBufSize(buf_size),
        mNumBufs(num_buffers) {}

    CameraHeapMemory(size_t buf_size, uint_t num_buffers) :
        mBufSize(buf_size),
        mNumBufs(num_buffers) {}

    void commonInitialization() {
        handle.data = mHeap->base();
        handle.size = mBufSize * mNumBufs;
        handle.handle = this;

        mBuffers = new sp<MemoryBase>[mNumBufs];
        for (uint_t i = 0; i < mNumBufs; i++)
            mBuffers[i] = new MemoryBase(mHeap,
                                         i * mBufSize,
                                         mBufSize);
    }

    virtual ~CameraHeapMemory() { LOG1("@%s", __FUNCTION__); }

    size_t mBufSize;
    uint_t mNumBufs;
    sp<MemoryHeapBase> mHeap;
    sp<MemoryBase> *mBuffers;
    camera_memory_t handle;
};

/**
 * Class for overriding the offset and size of camera heap memory allocations
 * during callbacks. Give the original MemoryBase from CameraHeapMemory mBuffers
 * to the constructor of this class with the new offset and desired size.
 */
class CameraMemoryBase : public MemoryBase
{
public:
    CameraMemoryBase(const sp<MemoryBase> &base, ssize_t offset, size_t size)
        : MemoryBase(base->getMemory(NULL, NULL), offset, size), mSize(size), mOffset(offset), mBase(base) {}
    virtual ~CameraMemoryBase() {  LOG1("@%s", __FUNCTION__); }
    virtual sp<IMemoryHeap> getMemory(ssize_t* offset, size_t* size) const {
        if (offset) *offset = mOffset;
        if (size)   *size = mSize;
        return mBase->getMemory(NULL, NULL);
    }
    sp<MemoryBase> getMemoryBase() { return mBase; }

protected:
    size_t getSize() const { return mSize; }
    ssize_t getOffset() const { return mOffset; }

private:
    size_t          mSize;
    ssize_t         mOffset;
    sp<MemoryBase>  mBase;
};

} // namespace android

#endif /* CAMHEAPMEM_H_ */
