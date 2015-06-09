/*
 * Copyright (c) 2008-2012, Intel Corporation. All rights reserved.
 *
 * Redistribution.
 * Redistribution and use in binary form, without modification, are
 * permitted provided that the following conditions are met:
 *  * Redistributions must reproduce the above copyright notice and
 * the following disclaimer in the documentation and/or other materials
 * provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its
 * suppliers may be used to endorse or promote products derived from
 * this software without specific  prior written permission.
 *  * No reverse engineering, decompilation, or disassembly of this
 * software is permitted.
 *
 * Limited patent license.
 * Intel Corporation grants a world-wide, royalty-free, non-exclusive
 * license under patents it now or hereafter owns or controls to make,
 * have made, use, import, offer to sell and sell ("Utilize") this
 * software, but solely to the extent that any such patent is necessary
 * to Utilize the software alone, or in combination with an operating
 * system licensed under an approved Open Source license as listed by
 * the Open Source Initiative at http://opensource.org/licenses.
 * The patent license shall not apply to any other combinations which
 * include this software. No hardware per se is licensed hereunder.
 *
 * DISCLAIMER.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors:
 *    Jackie Li <yaodong.li@intel.com>
 *
 */
#include <IntelDisplayPlaneManager.h>
#include <IntelOverlayUtil.h>

IntelSpriteContext::~IntelSpriteContext()
{

}

IntelSpritePlane::IntelSpritePlane(int fd, int index, IntelBufferManager *bm)
    : IntelDisplayPlane(fd, IntelDisplayPlane::DISPLAY_PLANE_SPRITE, index, bm)
{
    bool ret;
    ALOGD_IF(ALLOW_SPRITE_PRINT, "%s\n", __func__);

    // create data buffer
    IntelDisplayBuffer *dataBuffer = new IntelDisplayDataBuffer(0, 0, 0);
    if (!dataBuffer) {
        ALOGE("%s: Failed to create sprite data buffer\n", __func__);
        return;
    }

    // create sprite context
    IntelSpriteContext *spriteContext = new IntelSpriteContext();
    if (!spriteContext) {
        ALOGE("%s: Failed to create sprite context\n", __func__);
        goto sprite_create_err;
    }

    // initialized successfully
    mDataBuffer = dataBuffer;
    mContext = spriteContext;
    mInitialized = true;
    return;
sprite_create_err:
    delete dataBuffer;
}

IntelSpritePlane::~IntelSpritePlane()
{
    if (mContext) {
	// flush context
        //flip(IntelDisplayPlane::FLASH_NEEDED);

        // disable sprite
        disable();

        // delete sprite context;
        delete mContext;

        // destroy overlay data buffer;
        delete mDataBuffer;

        mContext = 0;
        mInitialized = false;
    }
}

void IntelSpritePlane::setPosition(int left, int top, int right, int bottom)
{
    if (initCheck()) {
        // TODO: check position

	int x = left;
        int y = top;
        int w = right - left;
        int h = bottom - top;

        IntelSpriteContext *spriteContext =
            reinterpret_cast<IntelSpriteContext*>(mContext);
        intel_sprite_context_t *context = spriteContext->getContext();

        // update dst position
        context->pos = (y & 0xfff) << 16 | (x & 0xfff);
        context->size = ((h - 1) & 0xfff) << 16 | ((w - 1) & 0xfff);
    }
}

bool IntelSpritePlane::setDataBuffer(IntelDisplayBuffer& buffer)
{
    if (initCheck()) {
        IntelDisplayBuffer *bufferPtr = &buffer;
        IntelDisplayDataBuffer *spriteDataBuffer =
	            reinterpret_cast<IntelDisplayDataBuffer*>(bufferPtr);
        IntelSpriteContext *spriteContext =
            reinterpret_cast<IntelSpriteContext*>(mContext);
        intel_sprite_context_t *context = spriteContext->getContext();

        uint32_t format = spriteDataBuffer->getFormat();
        uint32_t spriteFormat;
        int bpp;

        switch (format) {
        case HAL_PIXEL_FORMAT_RGBA_8888:
            spriteFormat = INTEL_SPRITE_PIXEL_FORMAT_RGBA8888;
            bpp = 4;
            break;
        case HAL_PIXEL_FORMAT_RGBX_8888:
            spriteFormat = INTEL_SPRITE_PIXEL_FORMAT_RGBX8888;
            bpp = 4;
            break;
        case HAL_PIXEL_FORMAT_BGRA_8888:
            spriteFormat = INTEL_SPRITE_PIXEL_FORMAT_BGRA8888;
            bpp = 4;
            break;
        default:
            ALOGE("%s: unsupported format 0x%x\n", __func__, format);
            return false;
        }

        // set offset;
        int srcX = spriteDataBuffer->getSrcX();
        int srcY = spriteDataBuffer->getSrcY();
        int srcWidth = spriteDataBuffer->getSrcWidth();
        int srcHeight = spriteDataBuffer->getSrcHeight();
        uint32_t stride = align_to(bpp * srcWidth, 64);
        uint32_t linoff = srcY * stride + srcX * bpp;

        // gtt
        uint32_t gttOffsetInPage = spriteDataBuffer->getGttOffsetInPage();

        // update context
        context->cntr = spriteFormat | 0x80000000;
        context->linoff = linoff;
        context->stride = stride;
        context->surf = gttOffsetInPage << 12;
        context->update_mask = SPRITE_UPDATE_ALL;

        return true;
    }
    ALOGE("%s: sprite plane was not initialized\n", __func__);
    return false;
}

bool IntelSpritePlane::flip(void *context, uint32_t flags)
{
    return true;
}

bool IntelSpritePlane::reset()
{
    return true;
}

bool IntelSpritePlane::disable()
{
    return true;
}

bool IntelSpritePlane::invalidateDataBuffer()
{
    ALOGD_IF(ALLOW_SPRITE_PRINT, "%s\n", __func__);
    if (initCheck()) {
	 mBufferManager->unmap(mDataBuffer);
	 delete mDataBuffer;
	 mDataBuffer = new IntelDisplayDataBuffer(0, 0, 0);
	 return true;
    }

    return false;
}
