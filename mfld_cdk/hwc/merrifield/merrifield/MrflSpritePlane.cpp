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
#include <MrflSpritePlane.h>
#include <MrflGrallocBuffer.h>
#include <MrflGrallocBufferMapper.h>

namespace android {
namespace intel {

static Log& log = Log::getInstance();

MrflSpritePlane::MrflSpritePlane(int index, int pipe)
    : SpritePlane(3, pipe)
{
    log.v("MrflSpritePlane");
    // reset context
    memset(&mContext, 0, sizeof(mContext));
}

MrflSpritePlane::~MrflSpritePlane()
{
    log.v("~MrflSpritePlane");
}

bool MrflSpritePlane::isValidBuffer(uint32_t handle)
{
    MrflGrallocBuffer buff(handle);
    uint32_t format = buff.getFormat();

    log.v("MrflSpritePlane::isValidTransform: trans = 0x%x");

    if (!initCheck()) {
        log.e("MrflSpritePlane::isValidTransform: plane hasn't been initialized");
        return false;
    }

    // check buffer format
    switch (format) {
    case IDataBuffer::FORMAT_BGRA8888:
    case IDataBuffer::FORMAT_BGRX8888:
    case IDataBuffer::FORMAT_RGB565:
        return true;
    default:
        log.v("MrflSpritePlane::isValidBuffer: unsupported format 0x%x",
              format);
    }

    return false;
}


void MrflSpritePlane::checkPosition(int& x, int& y, int& w, int& h)
{

}

bool MrflSpritePlane::setDataBuffer(IBufferMapper& mapper)
{
    int bpp;
    int srcX, srcY;
    int dstX, dstY, dstW, dstH;
    uint32_t spriteFormat;
    uint32_t stride;
    uint32_t linoff;

    log.v("MrflSpritePlane::setDataBuffer");

    // setup plane position
    dstX = mPosition.x;
    dstY = mPosition.y;
    dstW = mPosition.w;
    dstH = mPosition.h;

    checkPosition(dstX, dstY, dstW, dstH);

    // setup plane format
    switch (mapper.getFormat()) {
    case IDataBuffer::FORMAT_RGBA8888:
        spriteFormat = PALEN_PIXEL_FORMAT_RGBA8888;
        bpp = 4;
        break;
    case IDataBuffer::FORMAT_RGBX8888:
        spriteFormat = PLANE_PIXEL_FORMAT_BGRX8888;
        bpp = 4;
        break;
    case IDataBuffer::FORMAT_BGRX8888:
        spriteFormat = PLANE_PIXEL_FORMAT_BGRX8888;
        bpp = 4;
        break;
    case IDataBuffer::FORMAT_BGRA8888:
        spriteFormat = PLANE_PIXEL_FORMAT_BGRA8888;
        bpp = 4;
        break;
    case IDataBuffer::FORMAT_RGB565:
        spriteFormat = PLANE_FORMAT_BGRX565;
        bpp = 2;
        break;
    default:
        log.e("MrflSpritePlane::setDataBuffer: unsupported format 0x%x",
              mapper.getFormat());
        return false;
    }

    // setup stride and source buffer crop
    srcX = mapper.getCrop().x;
    srcY = mapper.getCrop().y;
    stride = mapper.getStride().rgb.stride;
    linoff = srcY * stride + srcX * bpp;

    // unlikely happen, but still we need make sure linoff is valid
    if (linoff > (stride * mapper.getHeight())) {
        log.e("MrflSpritePlane::setDataBuffer: invalid source crop");
        return false;
    }

    // update context
    mContext.type = DC_SPRITE_PLANE;
    mContext.ctx.sp_ctx.index = mIndex;
    mContext.ctx.sp_ctx.pipe = mPipe;
    mContext.ctx.sp_ctx.cntr = spriteFormat | 0x80000000;
    mContext.ctx.sp_ctx.linoff = linoff;
    mContext.ctx.sp_ctx.stride = stride;
    mContext.ctx.sp_ctx.surf = mapper.getGttOffsetInPage(0) << 12;
    mContext.ctx.sp_ctx.pos = (dstY & 0xfff) << 16 | (dstX & 0xfff);
    mContext.ctx.sp_ctx.size =
        ((dstH - 1) & 0xfff) << 16 | ((dstW - 1) & 0xfff);
    mContext.ctx.sp_ctx.update_mask = SPRITE_UPDATE_ALL;

    log.v("MrflSpritePlane::setDataBuffer: cntr = 0x%x, linoff = 0x%x, stride = 0x%x,"
          "surf = 0x%x, pos = 0x%x, size = 0x%x\n",
          mContext.ctx.sp_ctx.cntr,
          mContext.ctx.sp_ctx.linoff,
          mContext.ctx.sp_ctx.stride,
          mContext.ctx.sp_ctx.surf,
          mContext.ctx.sp_ctx.pos,
          mContext.ctx.sp_ctx.size);
    return true;
}

bool MrflSpritePlane::setDataBuffer(uint32_t handle)
{
    MrflGrallocBuffer *buf;
    IBufferMapper *mapper;
    bool ret;

    log.v("MrflSpritePlane::setDataBuffer: handle = %d");

    if (!initCheck()) {
        log.e("MrflSpritePlane::setDataBuffer: plane hasn't been initialized");
        return false;
    }

    if (!handle) {
        log.e("MrflSpritePlane::setDataBuffer: invalid buffer handle");
        return false;
    }

    buf = new MrflGrallocBuffer(handle);
    if (!buf) {
        log.e("MrflSpritePlane::setDataBuffer: failed to allocate buffer");
        return false;
    }

    // update buffer's source crop
    buf->setCrop(mSrcCrop.x, mSrcCrop.y, mSrcCrop.w, mSrcCrop.h);

    // map buffer if it's not in cache
    mapper = mGrallocBufferCache->getMapper(buf->getStamp());
    if (!mapper) {
        log.v("MrflSpritePlane::setDataBuffer: new buffer, will add it");
        mapper = new MrflGrallocBufferMapper(*mGrallocModule, *buf);
        if (!mapper) {
            log.e("MrflSpritePlane::setDataBuffer: failed to allocate mapper");
            goto mapper_err;
        }
        // map gralloc buffer
        ret = mapper->map();
        if (!ret) {
            log.e("MrflSpritePlane::setDataBuffer: failed to map");
            goto map_err;
        }

        // add mapper
        mGrallocBufferCache->addMapper(buf->getStamp(), mapper);
    }

    return setDataBuffer(*mapper);
map_err:
    delete mapper;
    return false;
mapper_err:
    delete buf;
    return false;
}

bool MrflSpritePlane::flip()
{
    log.v("MrflSpritePlane::flip");
    return true;
}

void* MrflSpritePlane::getContext() const
{
    log.v("MrflSpritePlane::getContext");
    return (void *)&mContext;
}

} // namespace intel
} // namespace android
