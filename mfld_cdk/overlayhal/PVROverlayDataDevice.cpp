/*
 * Copyright (C) 2008 The Android Open Source Project
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
#include <PVROverlayDataDevice.h>
#include <OverlayHALUtils.h>

#include <fcntl.h>
#include <errno.h>

#include <cutils/log.h>
#include <cutils/atomic.h>
#include <math.h>

PVROverlayDataDevice::PVROverlayDataDevice()
{
    LOGV("%s: creating data device...\n", __func__);
    pthread_mutex_init(&mLock, NULL);
    pthread_cond_init(&mHasBuffer, NULL);

    /*Allocate overlay control buffer*/
    memset(&mControlBlkBuffer, 0, sizeof(mControlBlkBuffer));
    mControlBlkBuffer.size = sizeof(struct pvr_overlay_control_block_t);
    /*need align to 64k in TT memory, which is 16 pages*/
    mControlBlkBuffer.gttAlign = 16;
    bool ret = PVROverlayHAL::Instance().allocateOverlayBuffer(&mControlBlkBuffer);
    if(ret == false) {
        LOGE("%s: allocate overlay buffer failed\n", __func__);
    }

    LOGV("%s: allocated control block\n", __func__);

    /*clean up external buffer*/
    memset(&mExternalBuffer, 0, sizeof(mExternalBuffer));

    mControlBlock = (struct pvr_overlay_control_block_t *)mControlBlkBuffer.overlayCPUAddress;

    LOGV("%s: finish successfully. mControlBlock %p\n",
        __func__, mControlBlock);
}

PVROverlayDataDevice::~PVROverlayDataDevice()
{
    LOGV("%s: destroying data device\n", __func__);

    this->lock();

    /*reset overlay*/
    resetOverlay();

    /*remove overlay control buffer and data buffers*/
    bool ret = PVROverlayHAL::Instance().destroyOverlayBuffer(&mControlBlkBuffer);
    if(ret == false)
        LOGE("%s: cannot destroy overlay control buffer %p\n",
            __func__, &mControlBlkBuffer);

    for(int i=0; i<PVR_OVERLAY_BUFFER_NUM; i++) {
        bool ret = PVROverlayHAL::Instance().destroyOverlayBuffer(&mDataBuffers[i]);
        if(ret == false)
            LOGE("%s: cannot destroy overlay data buffer %d\n",
                __func__, i);
    }

    /*
     * close shared context
     * NOTE: comment this out for super source usage.
     * overlay usage of super source was not compatible with Android overlay
     * architecture.
     * FIXME: uncomment it.
     */
    //PVROverlayHAL::Instance().destroySharedContext(false);

    this->unlock();

    /*destroy condition and mutex*/
    pthread_cond_destroy(&mHasBuffer);
    pthread_mutex_destroy(&mLock);

    LOGV("%s: data device destroyed\n", __func__);
}

bool PVROverlayDataDevice::initialize(struct pvr_overlay_handle_t * overlay)
{
    int i = 0;
    int j = 0;
    bool ret = false;
    struct pvr_overlay_buffer_t * pvrOverlayBuffer = NULL;
    int fd;
    int size;

    LOGV("PVROverlayDataDevice::%s: overlay handle %p\n",
        __func__, overlay);

    if(!overlay) {
        LOGE("%s: overlay handle is NULL\n", __func__);
        return false;
    }

    this->lock();

    fd = overlay->sharedFd;
    size = overlay->sharedSize;

    /*open shared context*/
    ret = PVROverlayHAL::Instance().openSharedContext(fd, size);
    if (ret == false) {
        LOGE("%s: cannot open shared context\n", __func__);
        this->unlock();
        return false;
    }

    /*do drm detection again*/
    ret =  PVROverlayHAL::Instance().detectDrmModeInfo();
    if (ret == false) {
        LOGE("%s: failed to detect DRM info, use mipi0 by default\n",
            __func__);
    }

    /*allocate data buffers*/
    for(i=0; i<PVR_OVERLAY_BUFFER_NUM; i++) {
        pvrOverlayBuffer = &mDataBuffers[i];
        pvrOverlayBuffer->width = overlay->width;
        pvrOverlayBuffer->height = overlay->height;
        pvrOverlayBuffer->yStride = overlay->yStride;
        pvrOverlayBuffer->uvStride = overlay->uvStride;
        pvrOverlayBuffer->format = overlay->format;
        pvrOverlayBuffer->align = 0;
        pvrOverlayBuffer->size = overlay->size;
        pvrOverlayBuffer->dataSize = overlay->dataSize;
        pvrOverlayBuffer->vsyncState = PVR_OVERLAY_VSYNC_INIT;
        pvrOverlayBuffer->gttOffset = 0;
        pvrOverlayBuffer->overlayBuffer = NULL;
        pvrOverlayBuffer->overlayIndex = overlay->overlayIndex;
        pvrOverlayBuffer->gttAlign = 0;

        /*request Overlay HAL to allocate overlay buffer. (lock free)*/
        ret = PVROverlayHAL::Instance().allocateOverlayBuffer(pvrOverlayBuffer);
        if(ret == false) {
            LOGE("%s: allocate data buffer %d failed\n", __func__, i);
            goto mem_err;
        }
    }

    /*init external buffer*/
    mExternalBuffer.width = overlay->width;
    mExternalBuffer.height = overlay->height;
    mExternalBuffer.yStride = overlay->yStride;
    mExternalBuffer.uvStride = overlay->uvStride;
    mExternalBuffer.format = overlay->format;
    mExternalBuffer.align = 0;
    mExternalBuffer.size = overlay->size;
    mExternalBuffer.dataSize = overlay->dataSize;
    mExternalBuffer.vsyncState = PVR_OVERLAY_VSYNC_INIT;
    mExternalBuffer.gttOffset = 0;
    mExternalBuffer.overlayBuffer = NULL;
    mExternalBuffer.overlayIndex = overlay->overlayIndex;

    /*use overlay HAL data buffer by default*/
    mUsingExternalBuffer = false;

    mOverlayIndex = pvrOverlayBuffer->overlayIndex;

    /*update free buffer count*/
    mNumFreeBuffers = PVR_OVERLAY_BUFFER_NUM;

    /*reset overlay*/
    resetOverlay();

    /*setup the dest window*/
    this->dstX = 0;
    this->dstY = 0;
    this->dstWidth = overlay->width;
    this->dstHeight = overlay->height;

    this->unlock();

    LOGV("PVROverlayDataDevice::%s: initialized successfully(%s).\n",
        __func__, pvrOverlayBuffer->overlayIndex ? "C" : "A");

    return true;
mem_err:
    /*destroy buffers which have been allocated*/
    for(j=i-1; j>=0; j--) {
        pvrOverlayBuffer = &mDataBuffers[j];

        bool ret = PVROverlayHAL::Instance().destroyOverlayBuffer(pvrOverlayBuffer);
        if(ret == false) {
            LOGE("%s: destroy data buffer %d failed\n", __func__, j);
        }
    }
drm_err:
    /*TODO: destroy shared context*/

    this->unlock();
    return false;
}

/*NOTE: don't need lock/unlock in a data device*/
void PVROverlayDataDevice::lock()
{

}

void PVROverlayDataDevice::unlock()
{

}

bool PVROverlayDataDevice::post(struct pvr_overlay_buffer_t * buffer)
{
    if(!buffer) {
        LOGE("%s: invalid buffer\n", __func__);
        return false;
    }

    LOGV("%s: pBase %p\n", __func__, buffer->overlayCPUAddress);

    uint32_t format = buffer->format;

    this->lock();

    /**
     * Check whether we need do pipe switching
     * FIXME: only do this when HDMI hotplug is detected.
     */
    PVROverlayHAL::Instance().drmModeChanged(&mControlBlkBuffer,
                                            mControlBlock);

    formatOverlayBuffer(buffer);
    bufferOffsetSetup(buffer);
    coordinateSetup(buffer);
    scalingSetup(buffer->width, buffer->height, dstWidth, dstHeight);

    mControlBlock->OSTRIDE = ((buffer->yStride) & (~0x3f)) |
                (((buffer->uvStride) & (~0x3f)) << 16);
    mControlBlock->OCMD = 0x1;
    mControlBlock->OCMD &= ~OVERLAY_PACKED_ORDER_MASK;
    mControlBlock->OCMD &= ~OVERLAY_FORMAT_MASK;
    mControlBlock->OCMD &= ~((0x1 << 1) | (0x3 << 2) | (0x1 << 5));
    mControlBlock->OCMD &= ~(0x3 << 17);
    mControlBlock->OCMD &= ~(0x1 << 19);
    mControlBlock->OCMD |= ((0x1 << 0) | (0x0 << 1)) ;

    switch (format) {
    case OVERLAY_FORMAT_YCbYCr_420_I:       /*I420*/
    //case OVERLAY_FORMAT_CbYCrY_420_I:     /*YV12*/
        mControlBlock->OCMD |= OVERLAY_FORMAT_PLANAR_YUV420;
        break;
    case OVERLAY_FORMAT_YCbCr_420_SP:       /*NV12*/
        mControlBlock->OCMD |= OVERLAY_FORMAT_PLANAR_NV12_2;
        break;
        case OVERLAY_FORMAT_YCbYCr_422_I:   /*YUY2*/
        mControlBlock->OCMD |= OVERLAY_FORMAT_PACKED_YUV422;
        mControlBlock->OCMD |= OVERLAY_PACKED_ORDER_YUY2;
        break;
        case OVERLAY_FORMAT_CbYCrY_422_I:   /*UYVY*/
        mControlBlock->OCMD |= OVERLAY_FORMAT_PACKED_YUV422;
        mControlBlock->OCMD |= OVERLAY_PACKED_ORDER_UYVY;
            break;
    default:
        LOGE("%s: unsupported format %d\n", __func__, format);
    }


    this->unlock();

    LOGV("%s: posting buffer %p\n", __func__, buffer);

    bool ret = PVROverlayHAL::Instance().updateOverlay(&mControlBlkBuffer,
                            mControlBlock,
                            true);
    if(ret == false) {
        LOGE("%s: post overlay failed\n", __func__);
        return false;
    }

    LOGV("%s: overlay posted successfully\n", __func__);

    return true;
}

bool PVROverlayDataDevice::postExternal(uint32_t device, uint32_t handle)
{
    LOGV("%s: external buffer handle 0x%x\n", __func__, handle);

    /*wrap external buffer*/
    bool ret = PVROverlayHAL::Instance().wrapExternalTTMBuffer(&mExternalBuffer,
                        device, handle);
    if (ret == false) {
        LOGE("%s: wrap extern buffer failed\n", __func__);
        return ret;
    }

    struct pvr_overlay_buffer_t * buffer = &mExternalBuffer;
    uint32_t format = buffer->format;

    this->lock();

    /**
     * Check whether we need do pipe switching
     * FIXME: only do this when HDMI hotplug is detected.
     */
    PVROverlayHAL::Instance().drmModeChanged(&mControlBlkBuffer,
                                            mControlBlock);

    /*mask data device to use external data buffer*/
    mUsingExternalBuffer = true;

    bufferOffsetSetup(buffer);
    coordinateSetup(buffer);
    scalingSetup(buffer->width, buffer->height, dstWidth, dstHeight);

    mControlBlock->OSTRIDE = ((buffer->yStride) & (~0x3f)) |
                    (((buffer->uvStride) & (~0x3f)) << 16);

    mControlBlock->OCMD = 0x1;
    //mControlBlock->OCMD &= ~OVERLAY_PACKED_ORDER_MASK;
    //mControlBlock->OCMD &= ~OVERLAY_FORMAT_MASK;
    //mControlBlock->OCMD &= ~((0x1 << 1) | (0x3 << 2) | (0x1 << 5));
    //mControlBlock->OCMD &= ~(0x3 << 17);
    //mControlBlock->OCMD &= ~(0x1 << 19);
    //mControlBlock->OCMD |= ((0x1 << 0) | (0x0 << 1));

    switch (format) {
    case OVERLAY_FORMAT_YCbYCr_420_I:           /*I420*/
        //case OVERLAY_FORMAT_CbYCrY_420_I:     /*YV12*/
        mControlBlock->OCMD |= OVERLAY_FORMAT_PLANAR_YUV420;
        break;
    case OVERLAY_FORMAT_YCbCr_420_SP:           /*NV12*/
        mControlBlock->OCMD |= OVERLAY_FORMAT_PLANAR_NV12_2;
        break;
        case OVERLAY_FORMAT_YCbYCr_422_I:       /*YUY2*/
        mControlBlock->OCMD |= OVERLAY_FORMAT_PACKED_YUV422;
        mControlBlock->OCMD |= OVERLAY_PACKED_ORDER_YUY2;
        break;
        case OVERLAY_FORMAT_CbYCrY_422_I:       /*UYVY*/
        mControlBlock->OCMD |= OVERLAY_FORMAT_PACKED_YUV422;
        mControlBlock->OCMD |= OVERLAY_PACKED_ORDER_UYVY;
        break;
    default:
        LOGE("%s: unsupported format %d\n", __func__, format);
    }

    this->unlock();

    LOGV("%s: posting buffer %p\n", __func__, buffer);

    ret = PVROverlayHAL::Instance().updateOverlay(&mControlBlkBuffer,
                        mControlBlock, true);
    if(ret == false) {
        LOGE("%s: post overlay failed\n", __func__);
        return false;
    }

    LOGV("%s: overlay posted successfully\n", __func__);

    return true;
}

struct pvr_overlay_buffer_t * PVROverlayDataDevice::getBuffer()
{
    LOGV("%s: getting buffer...\n", __func__);

    pthread_mutex_lock(&mLock);

    if(mNumFreeBuffers <= 0) {
        LOGV("%s: no free buffer. waiting...\n", __func__);
        pthread_cond_wait(&mHasBuffer, &mLock);
        LOGV("%s: free buffer avaliable.\n", __func__);
    }

    mNumFreeBuffers--;

    /*TODO: return the first free buffer*/

    pthread_mutex_unlock(&mLock);

    LOGV("%s: free buffer %p avaliable.\n", __func__, &mDataBuffers[0]);

    return &mDataBuffers[0];
}

bool PVROverlayDataDevice::putBuffer(struct pvr_overlay_buffer_t * buffer)
{
    LOGV("%s: putting buffer ...\n", __func__);

    pthread_mutex_lock(&mLock);

    mNumFreeBuffers++;

    /*TODO: put the buffer to the right place*/

    pthread_cond_signal(&mHasBuffer);

    pthread_mutex_unlock(&mLock);

    LOGV("%s: put buffer %p successfully.\n", __func__, buffer);

    return true;
}

void PVROverlayDataDevice::signal()
{
    LOGV("%s: signaling...\n", __func__);
}

void PVROverlayDataDevice::waitBuffer()
{
    LOGV("%s: waiting buffer...\n", __func__);
}

bool PVROverlayDataDevice::controlBlockInit()
{
    if(!mControlBlock) {
        LOGE("%s: Control Block is NULL\n", __func__);
        return false;
    }

    LOGV("%s: control block init...\n", __func__);

    memset(mControlBlock, 0, sizeof(struct pvr_overlay_control_block_t));

    /*reset overlay*/
    mControlBlock->OCLRC0 = (OVERLAY_INIT_CONTRAST << 18) |
                (OVERLAY_INIT_BRIGHTNESS & 0xff);
    mControlBlock->OCLRC1 = OVERLAY_INIT_SATURATION;
    mControlBlock->DCLRKV = OVERLAY_INIT_COLORKEY;
    mControlBlock->DCLRKM = OVERLAY_INIT_COLORKEYMASK;
    mControlBlock->OCONFIG = 0;
    mControlBlock->OCONFIG |= (0x1 << 3);
    mControlBlock->OCONFIG |= (0x1 << 27);
    mControlBlock->SCHRKEN &= ~(0x7 << 24);
    mControlBlock->SCHRKEN |= 0xff;

    LOGV("%s: successfully.\n", __func__);
    return true;
}

void PVROverlayDataDevice::bufferOffsetSetup(struct pvr_overlay_buffer_t * buf)
{
    LOGV("%s: setting up buffer offset...\n", __func__);
    if(!buf) {
        LOGE("%s: invalid buf\n", __func__);
        return;
    }
    uint32_t format = buf->format;
    uint32_t gttOffsetInBytes = buf->gttOffset << 12;

    switch(format) {
    case OVERLAY_FORMAT_YCbYCr_420_I:    /*I420*/
        mControlBlock->OBUF_0Y = gttOffsetInBytes;
        mControlBlock->OBUF_0U = gttOffsetInBytes +
                     align_to((buf->yStride * buf->height), 4096);
        mControlBlock->OBUF_0V = mControlBlock->OBUF_0U +
                     align_to((buf->uvStride * (buf->height / 2)), 4096);

        break;
#if 0
    case OVERLAY_FORMAT_CbYCrY_420_I:    /*YV12*/
        mControlBlock->OBUF_0Y = gttOffsetInBytes;
        mControlBlock->OBUF_0V = gttOffsetInBytes +
                     align_to((buf->yStride * buf->height), 4096);
        mControlBlock->OBUF_0U = mControlBlock->OBUF_0U +
                     align_to((buf->uvStride * (buf->height / 2)), 4096);
        break;
#endif
    /**
     * NOTE: this is the decoded video format, align the height to 32B
     * as it's defined by video driver
     */
    case OVERLAY_FORMAT_YCbCr_420_SP:    /*NV12*/
        mControlBlock->OBUF_0Y = gttOffsetInBytes;
        mControlBlock->OBUF_0U =
            gttOffsetInBytes + buf->yStride * align_to(buf->height, 32);
        mControlBlock->OBUF_0V = 0;
        break;
    case OVERLAY_FORMAT_YCbYCr_422_I:    /*YUY2*/
    case OVERLAY_FORMAT_CbYCrY_422_I:    /*UYVY*/
        mControlBlock->OBUF_0Y = gttOffsetInBytes;
        mControlBlock->OBUF_0U = 0;
        mControlBlock->OBUF_0V = 0;
        break;
    default:
        LOGE("%s: unsupported format %d\n", __func__, format);
    }

    mControlBlock->OBUF_1Y = mControlBlock->OBUF_0Y;
    mControlBlock->OBUF_1U = mControlBlock->OBUF_0U;
    mControlBlock->OBUF_1V = mControlBlock->OBUF_0V;

    LOGV("%s: done. offset (%d, %d, %d)\n", __func__,
                      mControlBlock->OBUF_0Y,
                      mControlBlock->OBUF_0U,
                      mControlBlock->OBUF_0V);
}

uint32_t PVROverlayDataDevice::calculateSWidthSW(uint32_t offset, uint32_t width)
{
    LOGV("%s: calculating SWidthSW...offset %d, width %d\n",
        __func__,
        offset,
        width);

    uint32_t swidth = ((offset + width + 0x3F) >> 6) - (offset >> 6);

    swidth <<= 1;
    swidth -= 1;

    return swidth;
}

void PVROverlayDataDevice::coordinateSetup(struct pvr_overlay_buffer_t * buf)
{
    uint32_t swidthy = 0;
    uint32_t swidthuv = 0;

    if(!buf) {
        LOGE("%s: invalid overlay buffer\n", __func__);
        return;
    }

    LOGV("%s: setting up coordinates...\n", __func__);

    uint32_t format = buf->format;
    uint32_t width = buf->width;
    uint32_t height = buf->height;
    uint32_t offsety = mControlBlock->OBUF_0Y;
    uint32_t offsetu = mControlBlock->OBUF_0U;

    switch (format) {
    case OVERLAY_FORMAT_YCbYCr_420_I:       /*I420*/
    //case OVERLAY_FORMAT_CbYCrY_420_I:     /*YV12*/
    case OVERLAY_FORMAT_YCbCr_420_SP:       /*NV12*/
        break;
    case OVERLAY_FORMAT_YCbYCr_422_I:   /*YUY2*/
    case OVERLAY_FORMAT_CbYCrY_422_I:   /*UYVY*/
        width <<= 1;
        break;
    default:
        LOGE("%s: unsupported format %d\n", __func__, format);
        while(1);
    }

    if (width == 0 || height == 0) {
        LOGE("%s: invalid src dim\n", __func__);
        while(1);
    }

    mControlBlock->SWIDTH = width | ((width / 2) << 16);
    swidthy = calculateSWidthSW(offsety, width);
    swidthuv = calculateSWidthSW(offsetu, width / 2);
    mControlBlock->SWIDTHSW = (swidthy << 2) | (swidthuv << 18);
    mControlBlock->SHEIGHT = height | ((height / 2) << 16);

    /*get dst position from HAL*/
    PVROverlayHAL::Instance().getPosition(mOverlayIndex,
                        &dstX, &dstY, &dstWidth, &dstHeight);

    LOGV("pos (%d, %d), size (%dx%d)\n", dstX, dstY, dstWidth, dstHeight);

    mControlBlock->DWINPOS = (dstY << 16) | dstX;
    mControlBlock->DWINSZ = (dstHeight << 16) | dstWidth;

    LOGV("%s: finished\n", __func__);
}

bool PVROverlayDataDevice::setCoeffRegs(double *coeff, int mantSize, coeffPtr pCoeff, int pos)
{
    int maxVal, icoeff, res;
    int sign;
    double c;

    sign = 0;
    maxVal = 1 << mantSize;
    c = *coeff;
    if (c < 0.0) {
        sign = 1;
        c = -c;
    }

    res = 12 - mantSize;
    if ((icoeff = (int)(c * 4 * maxVal + 0.5)) < maxVal) {
        pCoeff[pos].exponent = 3;
        pCoeff[pos].mantissa = icoeff << res;
        *coeff = (double)icoeff / (double)(4 * maxVal);
    } else if ((icoeff = (int)(c * 2 * maxVal + 0.5)) < maxVal) {
        pCoeff[pos].exponent = 2;
        pCoeff[pos].mantissa = icoeff << res;
        *coeff = (double)icoeff / (double)(2 * maxVal);
    } else if ((icoeff = (int)(c * maxVal + 0.5)) < maxVal) {
        pCoeff[pos].exponent = 1;
        pCoeff[pos].mantissa = icoeff << res;
        *coeff = (double)icoeff / (double)(maxVal);
    } else if ((icoeff = (int)(c * maxVal * 0.5 + 0.5)) < maxVal) {
        pCoeff[pos].exponent = 0;
        pCoeff[pos].mantissa = icoeff << res;
        *coeff = (double)icoeff / (double)(maxVal / 2);
    } else {
        /* Coeff out of range */
        return false;
    }

    pCoeff[pos].sign = sign;
    if (sign)
        *coeff = -(*coeff);
    return true;
}

void PVROverlayDataDevice::updateCoeff(int taps,
                    double fCutoff,
                    bool isHoriz,
                    bool isY,
                    coeffPtr pCoeff)
{
    int i, j, j1, num, pos, mantSize;
    double pi = 3.1415926535, val, sinc, window, sum;
    double rawCoeff[MAX_TAPS * 32], coeffs[N_PHASES][MAX_TAPS];
    double diff;
    int tapAdjust[MAX_TAPS], tap2Fix;
    bool isVertAndUV;

    if (isHoriz)
        mantSize = 7;
    else
        mantSize = 6;

    isVertAndUV = !isHoriz && !isY;
    num = taps * 16;
    for (i = 0; i < num  * 2; i++) {
        val = (1.0 / fCutoff) * taps * pi * (i - num) / (2 * num);
        if (val == 0.0)
            sinc = 1.0;
        else
            sinc = sin(val) / val;

        /* Hamming window */
        window = (0.5 - 0.5 * cos(i * pi / num));
        rawCoeff[i] = sinc * window;
    }

    for (i = 0; i < N_PHASES; i++) {
        /* Normalise the coefficients. */
        sum = 0.0;
        for (j = 0; j < taps; j++) {
            pos = i + j * 32;
            sum += rawCoeff[pos];
        }
        for (j = 0; j < taps; j++) {
            pos = i + j * 32;
            coeffs[i][j] = rawCoeff[pos] / sum;
        }

        /* Set the register values. */
        for (j = 0; j < taps; j++) {
            pos = j + i * taps;
            if ((j == (taps - 1) / 2) && !isVertAndUV)
                setCoeffRegs(&coeffs[i][j], mantSize + 2, pCoeff, pos);
            else
                setCoeffRegs(&coeffs[i][j], mantSize, pCoeff, pos);
        }

        tapAdjust[0] = (taps - 1) / 2;
        for (j = 1, j1 = 1; j <= tapAdjust[0]; j++, j1++) {
            tapAdjust[j1] = tapAdjust[0] - j;
            tapAdjust[++j1] = tapAdjust[0] + j;
        }

        /* Adjust the coefficients. */
        sum = 0.0;
        for (j = 0; j < taps; j++)
            sum += coeffs[i][j];
        if (sum != 1.0) {
            for (j1 = 0; j1 < taps; j1++) {
                tap2Fix = tapAdjust[j1];
                diff = 1.0 - sum;
                coeffs[i][tap2Fix] += diff;
                pos = tap2Fix + i * taps;
                if ((tap2Fix == (taps - 1) / 2) && !isVertAndUV)
                    setCoeffRegs(&coeffs[i][tap2Fix], mantSize + 2, pCoeff, pos);
                else
                    setCoeffRegs(&coeffs[i][tap2Fix], mantSize, pCoeff, pos);

                sum = 0.0;
                for (j = 0; j < taps; j++)
                    sum += coeffs[i][j];
                if (sum == 1.0)
                    break;
            }
        }
    }
}

void PVROverlayDataDevice::scalingSetup(uint32_t srcWidth, uint32_t srcHeight,
                    uint32_t dstWidth, uint32_t dstHeight)
{
    int xscaleInt, xscaleFract, yscaleInt, yscaleFract;
    int xscaleIntUV, xscaleFractUV;
    int yscaleIntUV, yscaleFractUV;
    /* UV is half the size of Y -- YUV420 */
    int uvratio = 2;
    uint32_t newval;
    coeffRec xcoeffY[N_HORIZ_Y_TAPS * N_PHASES];
    coeffRec xcoeffUV[N_HORIZ_UV_TAPS * N_PHASES];
    int i, j, pos;
    bool scaleChanged = false;

    /*
     * Y down-scale factor as a multiple of 4096.
     */
    if (srcWidth == dstWidth && srcHeight == dstHeight) {
        xscaleFract = (1 << 12);
        yscaleFract = (1 << 12);
    } else {
        xscaleFract = ((srcWidth - 1) << 12) / dstWidth;
        yscaleFract = ((srcHeight - 1) << 12) / dstHeight;
    }
    /* Calculate the UV scaling factor. */
    xscaleFractUV = xscaleFract / uvratio;
    yscaleFractUV = yscaleFract / uvratio;

    /*
     * To keep the relative Y and UV ratios exact, round the Y scales
     * to a multiple of the Y/UV ratio.
     */
    xscaleFract = xscaleFractUV * uvratio;
    yscaleFract = yscaleFractUV * uvratio;

    /* Integer (un-multiplied) values. */
    xscaleInt = xscaleFract >> 12;
    yscaleInt = yscaleFract >> 12;

    xscaleIntUV = xscaleFractUV >> 12;
    yscaleIntUV = yscaleFractUV >> 12;

    /* shouldn't get here */
    if (xscaleInt > 7) {
        LOGE("%s: xscaleInt > 7\n", __func__);
        return;
    }

    /* shouldn't get here */
    if (xscaleIntUV > 7) {
        LOGE("%s: xscaleIntUV > 7\n", __func__);
        return;
    }

    newval = (xscaleInt << 15) |
    ((xscaleFract & 0xFFF) << 3) | ((yscaleFract & 0xFFF) << 20);
    if (newval != mControlBlock->YRGBSCALE) {
        scaleChanged = true;
        mControlBlock->YRGBSCALE = newval;
    }

    newval = (xscaleIntUV << 15) | ((xscaleFractUV & 0xFFF) << 3) |
    ((yscaleFractUV & 0xFFF) << 20);
    if (newval != mControlBlock->UVSCALE) {
        scaleChanged = true;
        mControlBlock->UVSCALE = newval;
    }

    newval = yscaleInt << 16 | yscaleIntUV;
    if (newval != mControlBlock->UVSCALEV) {
        scaleChanged = true;
        mControlBlock->UVSCALEV = newval;
    }

    /* Recalculate coefficients if the scaling changed. */
    /*
     * Only Horizontal coefficients so far.
     */
    if (scaleChanged) {
        double fCutoffY;
        double fCutoffUV;

        fCutoffY = xscaleFract / 4096.0;
        fCutoffUV = xscaleFractUV / 4096.0;

        /* Limit to between 1.0 and 3.0. */
        if (fCutoffY < MIN_CUTOFF_FREQ)
            fCutoffY = MIN_CUTOFF_FREQ;
        if (fCutoffY > MAX_CUTOFF_FREQ)
            fCutoffY = MAX_CUTOFF_FREQ;
        if (fCutoffUV < MIN_CUTOFF_FREQ)
            fCutoffUV = MIN_CUTOFF_FREQ;
        if (fCutoffUV > MAX_CUTOFF_FREQ)
            fCutoffUV = MAX_CUTOFF_FREQ;

        updateCoeff(N_HORIZ_Y_TAPS, fCutoffY, true, true, xcoeffY);
        updateCoeff(N_HORIZ_UV_TAPS, fCutoffUV, true, false, xcoeffUV);

        for (i = 0; i < N_PHASES; i++) {
            for (j = 0; j < N_HORIZ_Y_TAPS; j++) {
                pos = i * N_HORIZ_Y_TAPS + j;
                mControlBlock->Y_HCOEFS[pos] =
                        (xcoeffY[pos].sign << 15 |
                          xcoeffY[pos].exponent << 12 |
                          xcoeffY[pos].mantissa);
            }
        }
        for (i = 0; i < N_PHASES; i++) {
            for (j = 0; j < N_HORIZ_UV_TAPS; j++) {
                pos = i * N_HORIZ_UV_TAPS + j;
                mControlBlock->UV_HCOEFS[pos] =
                         (xcoeffUV[pos].sign << 15 |
                          xcoeffUV[pos].exponent << 12 |
                          xcoeffUV[pos].mantissa);
            }
        }
    }
}

/**
 * NOTE: this function is only called for the standard YUV data with
 * NO padding bytes)
 */
bool PVROverlayDataDevice::formatOverlayBuffer(struct pvr_overlay_buffer_t * buffer)
{
    unsigned char * yBuffer = NULL;
    unsigned char * uBuffer = NULL;
    unsigned char * vBuffer = NULL;
    uint32_t yDelta = 0;
    uint32_t uvDelta = 0;
    uint32_t i = 0;

    LOGV("%s: formating overlay buffer...\n", __func__);

    if(!buffer) {
        LOGE("%s: invalid overlay buffer\n", __func__);
        return false;
    }

    if(!(buffer->dataBuffer) || !(buffer->overlayBuffer)) {
        LOGE("%s: overlay buffer format error\n", __func__);
        return false;
    }

    unsigned char * data = (unsigned char *)buffer->dataCPUAddress;
    unsigned char * overlayData = (unsigned char *)buffer->overlayCPUAddress;

    if(!data || !overlayData) {
        LOGE("%s: map buffer error\n", __func__);
        return false;
    }

    uint32_t format = buffer->format;
    uint32_t width = buffer->width;
    uint32_t height = buffer->height;
    uint32_t yStride = buffer->yStride;
    uint32_t uvStride = buffer->uvStride;

    switch(format) {
        case OVERLAY_FORMAT_YCbYCr_420_I:       /*I420*/
        //case OVERLAY_FORMAT_CbYCrY_420_I:     /*YV12*/
            yDelta = yStride - width;
            uvDelta = uvStride - (width >> 1);
            yBuffer = overlayData;
            uBuffer = overlayData + align_to(yStride * height, 4096);
            vBuffer = uBuffer + align_to(uvStride * (height >> 1), 4096);

            /*format yBuffer*/
            for(i=0; i<height; i++) {
                memcpy(yBuffer, data + (width * i), width);
                memset((yBuffer + width), 0x0, yDelta);
                yBuffer += yStride;
            }

            /*format uBuffer*/
            for(i=0; i<(height >> 1); i++) {
                memcpy(uBuffer,
                    data + (width * height) + ((width >> 1) * i),
                    (width >> 1));

                memset((uBuffer + (width >> 1)), 0x0, uvDelta);
                uBuffer += uvStride;
            }

            /*format vBuffer*/
            for(i=0; i<(height >> 1); i++) {
                memcpy(vBuffer,
                    data + (width * height) + (width * height >> 2) + ((width >> 1) * i),
                    (width >> 1));

                memset((vBuffer + (width >> 1)), 0x0, uvDelta);
                vBuffer += uvStride;
            }
            break;
        case OVERLAY_FORMAT_YCbCr_420_SP:       /*NV12*/
            yDelta = yStride - width;
            uvDelta = uvStride - width;

            yBuffer = overlayData;
            uBuffer = overlayData + align_to(yStride * height, 4096);

            /*format yBuffer*/
            for(i=0; i<height; i++) {
                memcpy(yBuffer, data + (width * i), width);
                memset((yBuffer + width), 0x0, yDelta);
                yBuffer += yStride;
            }

            /*format uvBuffer*/
            for(i=0; i<(height >> 1); i++) {
                memcpy(uBuffer,
                    data + (width * height) + (width * i),
                    width);

                memset((uBuffer + width), 0x0, uvDelta);
                uBuffer += uvStride;
            }
            break;
        case OVERLAY_FORMAT_YCbYCr_422_I:       /*YUY2*/
        case OVERLAY_FORMAT_CbYCrY_422_I:       /*UYVY*/
            yDelta = yStride - (width << 1);
            yBuffer = overlayData;

            for(i=0; i<height; i++) {
                memcpy(yBuffer,
                    data + (width << 1) * i,
                    (width << 1));
                memset((yBuffer + (width << 1)), 0x0, yDelta);
                yBuffer += yStride;
            }
            break;
        default:
            LOGE("%s: unknown format %d\n", __func__, format);
            return false;
    }

    LOGV("%s: overlay buffer formated successfully\n", __func__);
    return true;
}

bool PVROverlayDataDevice::setCrop(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    LOGV("%s: %d %d %d %d\n", __func__, x, y, w, h);

    /*TODO: implement this later*/
    return false;
}

bool PVROverlayDataDevice::getCrop(uint32_t *x,
                uint32_t *y,
                uint32_t *w,
                uint32_t *h)
{
    LOGV("%s: getting Crop...\n", __func__);
    if(!x || !y || !w || !h) {
        LOGE("%s: Invalid parameter\n", __func__);
        return false;
    }
    /*TODO: implement this later*/
    return false;
}

void PVROverlayDataDevice::setOverlayPipe(int pipe)
{
    /*call down to HAL setOverlayPipe*/
    PVROverlayHAL::Instance().setOverlayPipe(mOverlayIndex, pipe);
}

void PVROverlayDataDevice::setOverlayUsage(int usage)
{
    /*call down to HAL setOverlayUsage*/
    PVROverlayHAL::Instance().setOverlayUsage(mOverlayIndex, usage);
}

/*NOTE: only impact stride of external buffer*/
void PVROverlayDataDevice::setStride(int stride)
{
    this->lock();

    uint32_t format = mExternalBuffer.format;

    LOGV("%s: y stride %d, fomrat 0x%x\n", __func__, stride, format);

    uint32_t yStride;
    uint32_t uvStride;

    /* check the input params, reject if not supported or invalid */
    yStride = stride;

    switch (format) {
    case OVERLAY_FORMAT_YCbYCr_420_I:       /*I420*/
        uvStride = yStride >> 1;
        break;
    case OVERLAY_FORMAT_YCbCr_420_SP:       /*NV12*/
        uvStride = yStride;
        break;
    case OVERLAY_FORMAT_YCbYCr_422_I:       /*YUY2*/
        uvStride = 0;
        break;
    default:
        LOGE("%s: unsupported format %d\n", __func__, format);
        this->unlock();
        return;
    }

    mExternalBuffer.yStride = yStride;
    mExternalBuffer.uvStride = uvStride;

    this->unlock();
}

/**
 * reset overlay HW
 */
void PVROverlayDataDevice::resetOverlay()
{
    LOGE("%s: resetting overlay...\n", __func__);

    controlBlockInit();
    PVROverlayHAL::Instance().updateOverlay(&mControlBlkBuffer,
                        mControlBlock, false);
}

void PVROverlayDataDevice::setDrmModeChanged(bool changed)
{
    PVROverlayHAL::Instance().setDrmModeChanged(changed);
}
