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
#include <Log.h>
#include <Drm.h>
#include <MrflGrallocBufferMapper.h>

namespace android {
namespace intel {

static Log& log = Log::getInstance();
MrflGrallocBufferMapper::MrflGrallocBufferMapper(IMG_gralloc_module_public_t& module,
                                                    MrflGrallocBuffer& buffer)
    : mRefCount(0),
      mIMGGrallocModule(module),
      mBuffer(buffer),
      mBufferObject(0)
{
    log.v("MrflGrallocBufferMapper::MrflGrallocBufferMapper");

    for (int i = 0; i < SUB_BUFFER_MAX; i++) {
        mGttOffsetInPage[i] = 0;
        mCpuAddress[i] = 0;
        mSize[i] = 0;
    }
}

MrflGrallocBufferMapper::~MrflGrallocBufferMapper()
{
    log.v("MrflGrallocBufferMapper::~MrflGrallocBufferMapper");
    // delete data buffer
    delete &mBuffer;
}

bool MrflGrallocBufferMapper::gttMap(void *vaddr,
                                      uint32_t size,
                                      uint32_t gttAlign,
                                      int *offset)
{
    struct psb_gtt_mapping_arg arg;
    Drm& drm(Drm::getInstance());
    bool ret;

    log.v("MrflGrallocBufferMapper::gttMap: virt 0x%x, size %d\n", vaddr, size);

    if (!vaddr || !size || !offset) {
        log.v("MrflGrallocBufferMapper::gttMap: invalid parameters.");
        return false;
    }

    arg.type = PSB_GTT_MAP_TYPE_VIRTUAL;
    arg.page_align = gttAlign;
    arg.vaddr = (uint32_t)vaddr;
    arg.size = size;

    ret = drm.writeReadIoctl(DRM_PSB_GTT_MAP, &arg, sizeof(arg));
    if (ret == false) {
        log.e("MrflGrallocBufferMapper::gttMap: gtt mapping failed");
        return false;
    }

    log.v("MrflGrallocBufferMapper::gttMap: offset %d", arg.offset_pages);
    *offset =  arg.offset_pages;
    return true;
}

bool MrflGrallocBufferMapper::gttUnmap(void *vaddr)
{
    struct psb_gtt_mapping_arg arg;
    Drm& drm(Drm::getInstance());
    bool ret;

    log.v("MrflGrallocBufferMapper::gttUnmap: virt 0x%x", vaddr);

    if(!vaddr) {
        log.e("MrflGrallocBufferMapper::gttUnmap: invalid parameter");
        return false;
    }

    arg.type = PSB_GTT_MAP_TYPE_VIRTUAL;
    arg.vaddr = (uint32_t)vaddr;

    ret = drm.writeIoctl(DRM_PSB_GTT_UNMAP, &arg, sizeof(arg));
    if(ret == false) {
        log.e("%MrflGrallocBufferMapper::gttUnmap: gtt unmapping failed");
        return false;
    }

    return true;
}

bool MrflGrallocBufferMapper::map()
{
    void *vaddr = 0;
    uint32_t size = 0;
    int gttOffsetInPage = 0;
    bool ret;
    int err;
    int i;

    log.v("MrflGrallocBufferMapper::map");

    // get virtual address
    for (i = 0; i < SUB_BUFFER_MAX; i++) {
        err = mIMGGrallocModule.getCpuAddress(&mIMGGrallocModule,
                                              mBuffer.getStamp(),
                                              i,
                                              &vaddr,
                                              &size);
        if (err) {
            log.e("MrflGrallocBufferMapper::map: failed to map. err = %d",
                  err);
            goto map_err;
        }

        // map to gtt
        ret = gttMap(vaddr, size, 0, &gttOffsetInPage);
        if (!ret)
            log.v("MrflGrallocBufferMapper::map: failed to map %d into gtt", i);

        mCpuAddress[i] = vaddr;
        mSize[i] = size;
        mGttOffsetInPage[i] = gttOffsetInPage;
    }

    return true;
gtt_err:
    for (i = 0; i < SUB_BUFFER_MAX; i++) {
        if (mCpuAddress[i])
            gttUnmap(mCpuAddress[i]);
    }
map_err:
    mIMGGrallocModule.putCpuAddress(&mIMGGrallocModule,
                                    mBuffer.getStamp());
    return false;
}

bool MrflGrallocBufferMapper::unmap()
{
    int i;

    log.v("MrflGrallocBufferMapper::unmap");

    for (i = 0; i < SUB_BUFFER_MAX; i++) {
        if (mCpuAddress[i])
            gttUnmap(mCpuAddress[i]);

        mGttOffsetInPage[i] = 0;
        mCpuAddress[i] = 0;
        mSize[i] = 0;
    }

    mIMGGrallocModule.putCpuAddress(&mIMGGrallocModule,
                                    mBuffer.getStamp());
    return true;
}

int MrflGrallocBufferMapper::incRef()
{
    mRefCount++;

    return mRefCount;
}

int MrflGrallocBufferMapper::decRef()
{
    mRefCount--;
    return mRefCount;
}

uint32_t MrflGrallocBufferMapper::getGttOffsetInPage(int subIndex) const
{
    if (subIndex >= 0 && subIndex < SUB_BUFFER_MAX)
        return mGttOffsetInPage[subIndex];
    return 0;
}

void* MrflGrallocBufferMapper::getCpuAddress(int subIndex) const
{
    if (subIndex >=0 && subIndex < SUB_BUFFER_MAX)
        return mCpuAddress[subIndex];
    return 0;
}

uint32_t MrflGrallocBufferMapper::getSize(int subIndex) const
{
    if (subIndex >= 0 && subIndex < SUB_BUFFER_MAX)
        return mSize[subIndex];
    return 0;
}

stride_t& MrflGrallocBufferMapper::getStride () const
{
    return mBuffer.getStride();
}

uint32_t MrflGrallocBufferMapper::getWidth() const
{
    return mBuffer.getWidth();
}

uint32_t MrflGrallocBufferMapper::getHeight() const
{
    return mBuffer.getHeight();
}

crop_t& MrflGrallocBufferMapper::getCrop() const
{
    return mBuffer.getCrop();
}

uint32_t MrflGrallocBufferMapper::getFormat() const
{
    return mBuffer.getFormat();
}

} // namespace intel
} // namespace android
