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
#include <MrflPrimaryPlane.h>
#include <MrflGrallocBuffer.h>
#include <MrflGrallocBufferMapper.h>

namespace android {
namespace intel {

static Log& log = Log::getInstance();

MrflPrimaryPlane::MrflPrimaryPlane(int index, int pipe)
    : SpritePlane(index, pipe),
      mForceBottom(true)
{
    log.v("MrflPrimaryPlane");
    SpritePlane::mType = PLANE_PRIMARY;
    // reset context
    memset(&mContext, 0, sizeof(mContext));
}

MrflPrimaryPlane::~MrflPrimaryPlane()
{
    log.v("~MrflPrimaryPlane");
}

void MrflPrimaryPlane::setFramebufferTarget(IDataBuffer& buf)
{
    log.v("MrflPrimaryPlane::setFramebufferTarget");

    // don't need to map data buffer for primary plane
    mContext.type = DC_PRIMARY_PLANE;
    mContext.ctx.prim_ctx.update_mask = SPRITE_UPDATE_ALL;
    mContext.ctx.prim_ctx.index = mIndex;
    mContext.ctx.prim_ctx.pipe = mPipe;
    mContext.ctx.prim_ctx.linoff = 0;
    mContext.ctx.prim_ctx.stride = align_to((4 * align_to(mPosition.w, 32)), 64);
    mContext.ctx.prim_ctx.pos = 0;
    mContext.ctx.prim_ctx.size =
        ((mPosition.h - 1) & 0xfff) << 16 | ((mPosition.w - 1) & 0xfff);
    mContext.ctx.prim_ctx.surf = 0;

    mContext.ctx.prim_ctx.cntr = PLANE_PIXEL_FORMAT_BGRA8888;
    mContext.ctx.prim_ctx.cntr |= 0x80000000;
    if (mForceBottom)
        mContext.ctx.prim_ctx.cntr  |= 0x00000004;
}

bool MrflPrimaryPlane::isValidBuffer(uint32_t handle)
{
    log.v("MrflPrimaryPlane::isValidBuffer: handle = 0x%x", handle);
    MrflGrallocBuffer buff(handle);
    uint32_t format = buff.getFormat();

    // check buffer format
    switch (format) {
    case IDataBuffer::FORMAT_BGRA8888:
    case IDataBuffer::FORMAT_BGRX8888:
    case IDataBuffer::FORMAT_RGB565:
        return true;
    default:
        log.v("MrflPrimaryPlane::isValidBuffer: unsupported format 0x%x",
              format);
    }

    return false;
}

void MrflPrimaryPlane::checkPosition(int& x, int& y, int& w, int& h)
{

}

bool MrflPrimaryPlane::setDataBuffer(IBufferMapper& mapper)
{
    int bpp;
    int srcX, srcY;
    int dstX, dstY, dstW, dstH;
    uint32_t spriteFormat;
    uint32_t stride;
    uint32_t linoff;

    log.v("MrflPrimaryPlane::setDataBuffer");

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
        log.e("MrflPrimaryPlane::setDataBuffer: unsupported format 0x%x",
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
        log.e("MrflPrimaryPlane::setDataBuffer: invalid source crop");
        return false;
    }

    // update context
    mContext.type = DC_SPRITE_PLANE;
    mContext.ctx.sp_ctx.index = 0;
    mContext.ctx.sp_ctx.pipe = 0;
    mContext.ctx.sp_ctx.cntr = spriteFormat | 0x80000000;
    mContext.ctx.sp_ctx.linoff = linoff;
    mContext.ctx.sp_ctx.stride = stride;
    mContext.ctx.sp_ctx.surf = mapper.getGttOffsetInPage(0) << 12;
    mContext.ctx.sp_ctx.pos = (dstY & 0xfff) << 16 | (dstX & 0xfff);
    mContext.ctx.sp_ctx.size =
        ((dstH - 1) & 0xfff) << 16 | ((dstW - 1) & 0xfff);
    mContext.ctx.sp_ctx.update_mask = SPRITE_UPDATE_ALL;
    if (mForceBottom)
        mContext.ctx.sp_ctx.cntr  |= 0x00000004;

    log.v("MrflPrimaryPlane::setDataBuffer: cntr = 0x%x, linoff = 0x%x, stride = 0x%x,"
          "surf = 0x%x, pos = 0x%x, size = 0x%x\n",
          mContext.ctx.sp_ctx.cntr,
          mContext.ctx.sp_ctx.linoff,
          mContext.ctx.sp_ctx.stride,
          mContext.ctx.sp_ctx.surf,
          mContext.ctx.sp_ctx.pos,
          mContext.ctx.sp_ctx.size);
    return true;
}

bool MrflPrimaryPlane::setDataBuffer(uint32_t handle)
{
    MrflGrallocBuffer tmpBuf(handle);
    MrflGrallocBuffer *buf;
    IBufferMapper *mapper;
    uint32_t usage;
    bool ret;

    log.v("MrflPrimaryPlane::setDataBuffer: handle = %d");

    usage = tmpBuf.getUsage();
    if (!handle || (GRALLOC_USAGE_HW_FB & usage)) {
        setFramebufferTarget(tmpBuf);
        return true;
    }

    // primary is used as a sprite plane
    log.v("MrflPrimaryPlane::setDataBuffer: usage = 0x%x", usage);

    if (!initCheck()) {
        log.e("MrflPrimaryPlane::setDataBuffer: plane hasn't been initialized");
        return false;
    }

    if (!handle) {
        log.e("MrflPrimaryPlane::setDataBuffer: invalid buffer handle");
        return false;
    }

    buf = new MrflGrallocBuffer(handle);
    if (!buf) {
        log.e("MrflPrimaryPlane::setDataBuffer: failed to allocate buffer");
        return false;
    }

    // update buffer's source crop
    buf->setCrop(mSrcCrop.x, mSrcCrop.y, mSrcCrop.w, mSrcCrop.h);

    // map buffer if it's not in cache
    mapper = mGrallocBufferCache->getMapper(buf->getStamp());
    if (!mapper) {
        log.v("MrflPrimaryPlane::setDataBuffer: new buffer, will add it");
        mapper = new MrflGrallocBufferMapper(*mGrallocModule, *buf);
        if (!mapper) {
            log.e("MrflPrimaryPlane::setDataBuffer: failed to allocate mapper");
            goto mapper_err;
        }
        // map gralloc buffer
        ret = mapper->map();
        if (!ret) {
            log.e("MrflPrimaryPlane::setDataBuffer: failed to map");
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

bool MrflPrimaryPlane::assignToPipe(uint32_t pipe)
{
    log.v("MrflPrimaryPlane::assignToPipe:Unsupported for a primary plane");
    return true;
}

void MrflPrimaryPlane::setZOrderConfig(ZOrderConfig& config)
{
    log.v("MrflPrimaryPlane::setZOrderConfig");

    // for primary plane of external display, force it to be bottom
    if (mIndex == Drm::OUTPUT_HDMI) {
        mForceBottom = true;
        return;
    }

    if (config.overlayCount)
        mForceBottom = false;
    else
        mForceBottom = true;
}

bool MrflPrimaryPlane::reset()
{
    log.v("reset");
    return true;
}

bool MrflPrimaryPlane::flip()
{
    log.v("flip");
    return true;
}

bool MrflPrimaryPlane::enable()
{
    log.v("enable");
    return true;
}

bool MrflPrimaryPlane::disable()
{
    log.v("disable");
    return true;
}

void* MrflPrimaryPlane::getContext() const
{
    log.v("getContext");
    return (void *)&mContext;
}

} // namespace intel
} // namespace android
