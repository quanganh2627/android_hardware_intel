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
#include <MrflOverlayPlane.h>
#include <MrflGrallocBuffer.h>
#include <MrflGrallocBufferMapper.h>
#include <TTMBuffer.h>
#include <TTMBufferMapper.h>

namespace android {
namespace intel {

static Log& log = Log::getInstance();

MrflOverlayPlane::MrflOverlayPlane(int index, int pipe)
    : OverlayPlane(index, pipe)
{
    log.v("MrflOverlayPlane");
    memset(&mContext, 0, sizeof(mContext));
}

MrflOverlayPlane::~MrflOverlayPlane()
{
    log.v("~MrflOverlayPlane");
}

bool MrflOverlayPlane::isValidBuffer(uint32_t handle)
{
    log.v("MrflOverlayPlane::isValidBuffer: handle = 0x%x", handle);
    MrflGrallocBuffer buff(handle);
    uint32_t format = buff.getFormat();

    if (!initCheck()) {
        log.e("MrflOverlayPlane::setDataBuffer: overlay wasn't initialized");
        return false;
    }

    // check buffer format
    switch (format) {
    case IDataBuffer::FORMAT_YV12:
    case IDataBuffer::FORMAT_I420:
    case IDataBuffer::FORMAT_NV12_VED:
    case IDataBuffer::FORMAT_YUY2:
    case IDataBuffer::FORMAT_UYVY:
        return true;
    default:
        log.v("MrflOverlayPlane::isValidBuffer: unsupported format 0x%x",
              format);
    }

    return false;
}

IBufferMapper* MrflOverlayPlane::getGrallocMapper(uint32_t handle)
{
    MrflGrallocBuffer *buf;
    IBufferMapper *mapper;
    bool ret;

    buf = new MrflGrallocBuffer(handle);
    if (!buf) {
        log.e("MrflOverlayPlane::setDataBuffer: failed to allocate buffer");
        return 0;
    }

    // map buffer if it's not in cache
    mapper = mGrallocBufferCache->getMapper(buf->getStamp());
    if (!mapper) {
        log.v("MrflOverlayPlane::getGrallocMapper: new buffer, will add it");
        // update buffer's source crop
        buf->setCrop(mSrcCrop.x, mSrcCrop.y, mSrcCrop.w, mSrcCrop.h);

        mapper = new MrflGrallocBufferMapper(*mGrallocModule, *buf);
        if (!mapper) {
            log.e("MrflOverlayPlane::getGrallocMapper: failed to allocate mapper");
            goto mapper_err;
        }
        // map gralloc buffer
        ret = mapper->map();
        if (!ret) {
            log.e("MrflOverlayPlane::getGrallocMapper: failed to map");
            goto map_err;
        }

        // add mapper
        mGrallocBufferCache->addMapper(buf->getStamp(), mapper);
    }

    // TODO: delete gralloc buffer;
    log.v("MrflOverlayPlane::getGrallocMapper: got gralloc mapper");
    return mapper;
map_err:
    delete mapper;
    return 0;
mapper_err:
    delete buf;
    return 0;
}

IBufferMapper* MrflOverlayPlane::getTTMMapper(IBufferMapper& grallocMapper)
{
    struct PayloadBuffer *payload;
    uint32_t khandle;
    uint32_t w, h;
    uint32_t yStride, uvStride;
    stride_t stride;
    int srcX, srcY, srcW, srcH;
    int tmp;

    TTMBuffer *buf;
    TTMBufferMapper *mapper;
    bool ret;

    payload = (struct PayloadBuffer *)grallocMapper.getCpuAddress(SUB_BUFFER1);
    if (!payload) {
        log.e("MrflOverlayPlane::getTTMMapper: invalid payload buffer");
        return 0;
    }

    // init ttm buffer
    khandle = payload->rotated_buffer_handle;
    mapper = reinterpret_cast<TTMBufferMapper *>(mTTMBufferCache->getMapper(khandle));
    if (!mapper) {
        log.v("MrflOverlayPlane::getTTMMapper: new buffer, will add it");
        buf = new TTMBuffer(khandle);
        if (!buf) {
            log.e("MrflOverlayPlane::getTTMMapper: failed to create buffer");
            return 0;
        }

        w = payload->rotated_width;
        h = payload->rotated_height;
        srcX = grallocMapper.getCrop().x;
        srcY = grallocMapper.getCrop().y;
        srcW = grallocMapper.getCrop().w;
        srcH = grallocMapper.getCrop().h;

        if (mTransform == PLANE_TRANSFORM_90 || mTransform == PLANE_TRANSFORM_270) {
            tmp = srcH;
            srcH = srcW;
            srcW = tmp;

            tmp = srcX;
            srcX = srcY;
            srcY = tmp;
        }

        // skip pading bytes in rotate buffer
        switch(mTransform) {
        case PLANE_TRANSFORM_90:
            srcX += ((srcW + 0xf) & ~0xf) - srcW;
            break;
        case PLANE_TRANSFORM_180:
            srcX += ((srcW + 0xf) & ~0xf) - srcW;
            srcY += ((srcH + 0xf) & ~0xf) - srcH;
            break;
        case PLANE_TRANSFORM_270:
            srcY += ((srcH + 0xf) & ~0xf) - srcH;
            break;
        default:
            break;
        }

        // calculate stride
        switch (grallocMapper.getFormat()) {
        case IDataBuffer::FORMAT_YV12:
        case IDataBuffer::FORMAT_I420:
            yStride = align_to(align_to(w, 32), 64);
            uvStride = align_to(yStride >> 1, 64);
            stride.yuv.yStride = yStride;
            stride.yuv.uvStride = uvStride;
            break;
        case IDataBuffer::FORMAT_NV12_VED:
            yStride = align_to(align_to(w, 32), 64);
            uvStride = yStride;
            stride.yuv.yStride = yStride;
            stride.yuv.uvStride = uvStride;
            break;
        case IDataBuffer::FORMAT_YUY2:
        case IDataBuffer::FORMAT_UYVY:
            yStride = align_to((align_to(w, 32) << 1), 64);
            uvStride = 0;
            stride.yuv.yStride = yStride;
            stride.yuv.uvStride = uvStride;
            break;
        }

        // update buffer
        buf->setStride(stride);
        buf->setWidth(w);
        buf->setHeight(h);
        buf->setCrop(srcX, srcY, srcW, srcH);
        buf->setFormat(grallocMapper.getFormat());

        // create buffer mapper
        mapper = new TTMBufferMapper(*mWsbm, *buf);
        if (!mapper) {
            log.e("MrflOverlayPlane::getTTMMapper: failed to allocate mapper");
            goto mapper_err;
        }
        // map gralloc buffer
        ret = mapper->map();
        if (!ret) {
            log.e("MrflOverlayPlane::getTTMMapper: failed to map");
            goto map_err;
        }

        // add mapper
        mTTMBufferCache->addMapper(khandle, mapper);
    }

    // sync rotated data buffer.
    // Well, I have to, video driver DOESN'T support sync this buffer
    ret = mapper->waitIdle();
    if (ret == false)
        log.w("MrflOverlayPlane::getTTMMapper: failed to wait idle");

    log.v("MrflOverlayPlane::getTTMMapper: got ttm mapper");

    return mapper;
map_err:
    delete mapper;
    return 0;
mapper_err:
    delete buf;
    return 0;
}

bool MrflOverlayPlane::rotatedBufferReady(IBufferMapper& mapper)
{
    struct PayloadBuffer *payload;
    uint32_t format;

    // only NV12_VED has rotated buffer
    format = mapper.getFormat();
    if (format != IDataBuffer::FORMAT_NV12_VED)
        return false;

    payload = (struct PayloadBuffer *)mapper.getCpuAddress(SUB_BUFFER1);
    // check payload
    if (!payload) {
        log.e("MrflOverlayPlane::rotatedBufferReady: no payload found");
        return false;
    }

    if (payload->force_output_method == FORCE_OUTPUT_GPU)
        return false;

    if (payload->client_transform != mTransform) {
        log.w("MrflOverlayPlane::rotatedBufferReady: client is not ready");
        return false;
    }

    return true;
}

bool MrflOverlayPlane::setDataBuffer(uint32_t handle)
{
    IBufferMapper *mapper;
    bool ret;

    log.v("MrflOverlayPlane::setDataBuffer: handle = %d");

    if (!initCheck()) {
        log.e("MrflOverlayPlane::setDataBuffer: overlay wasn't initialized");
        return false;
    }

    if (!handle) {
        log.e("MrflOverlayPlane::setDataBuffer: invalid buffer handle");
        return false;
    }

    // get gralloc mapper
    mapper = getGrallocMapper(handle);
    if (!mapper) {
        log.e("MrflOverlayPlane::setDataBuffer: failed to get gralloc mapper");
        return false;
    }

    // check transform when overlay is attached to primary device
    if (mTransform && !mPipe) {
        if (!rotatedBufferReady(*mapper)) {
            log.w("MrflOverlayPlane::setDataBuffer: rotated buffer is not ready");
            return false;
        }

        // get rotated data buffer mapper
        mapper = getTTMMapper(*mapper);
        if (!mapper) {
            log.e("MrflOverlayPlane::setDataBuffer: failed to get rotated buffer");
            return false;
        }
    }

    return OverlayPlane::setDataBuffer(*mapper);
}

bool MrflOverlayPlane::flip()
{
    log.v("MrflOverlayPlane:flip");

    if (!initCheck()) {
        log.e("MrflOverlayPlane::setDataBuffer: overlay wasn't initialized");
        return false;
    }

    mContext.type = DC_OVERLAY_PLANE;
    mContext.ctx.ov_ctx.ovadd = 0x0;
    mContext.ctx.ov_ctx.ovadd = (mBackBuffer->gttOffsetInPage << 12);
    mContext.ctx.ov_ctx.index = mIndex;
    mContext.ctx.ov_ctx.pipe = mPipe;
    mContext.ctx.ov_ctx.ovadd |= 0x1;

    log.v("MrflOverlayPlane::flip: ovadd = 0x%x, index = %d, pipe = %d",
          mContext.ctx.ov_ctx.ovadd,
          mIndex,
          mPipe);

    return true;
}

void* MrflOverlayPlane::getContext() const
{
    log.v("MrflOverlayPlane::getContext");
    return (void *)&mContext;
}

} // namespace intel
} // namespace android
