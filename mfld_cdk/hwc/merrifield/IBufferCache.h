/*
 * Copyright © 2012 Intel Corporation
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Jackie Li <yaodong.li@intel.com>
 *
 */
#ifndef IBUFFERCACHE_H_
#define IBUFFERCACHE_H_

namespace android {
namespace intel {

class IBufferCache {
public:
    IBufferCache() {}
    virtual ~IBufferCache() {}
    // add a new mapper into buffer cache
    virtual bool addMapper(uint64_t handle, IBufferMapper* mapper) = 0;
    // remove a mapper from buffer cache
    virtual void removeMapper(IBufferMapper* mapper) = 0;
    // get a buffer mapper
    virtual IBufferMapper* getMapper(uint64_t handle) = 0;
    // clear cache
    virtual void clear() = 0;
};

// Generic buffer cache
class BufferCache : public IBufferCache {
public:
    BufferCache(int size);
    virtual ~BufferCache();
    // add a new mapper into buffer cache
    virtual bool addMapper(uint64_t handle, IBufferMapper* mapper);
    // remove a mapper from buffer cache
    virtual void removeMapper(IBufferMapper* mapper);
    // get a buffer mapper
    virtual IBufferMapper* getMapper(uint64_t handle);

    virtual void clear();
private:
    KeyedVector<uint64_t, IBufferMapper*> mBufferPool;
};

}
}


#endif /* IBUFFERCACHE_H_ */
