/*
 * Copyright (c) 2007-2013 Intel Corporation. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#define LOG_TAG "Camera_VAConvertor"
#include <intel_bufmgr.h>
#include <drm_fourcc.h>
#include "LogHelper.h"
#include "VAConvertor.h"

#define CHECK_VASTATUS(str) \
    do { \
        if (vaStatus != VA_STATUS_SUCCESS) { \
            LOGE("%s failed :%s\n", str, vaErrorStr(vaStatus)); \
            return UNKNOWN_ERROR;}   \
    }while(0)

namespace android {

/*
 * This structs is copy from graphic area.
 * It's only to get buffer name from buffer handle.
 * Will be remove when buffer handle can be use directly in surface creation
 */

VAConvertor::VAConvertor(bool cache):
    mInitialized(false),
    mIsCacheRenderBuf(cache),
    mVA(NULL),
    mVPP(NULL),
    mIIDKey(0),
    mOIDKey(0)
{
    LOG1("@%s", __FUNCTION__);
    mIBuffers.clear();
    mOBuffers.clear();

    if (init()) {
        LOGE("Fail to initialize VAConvertor");
    }
}

VAConvertor::~VAConvertor()
{
    LOG1("@%s", __FUNCTION__);
    deInit();
}

status_t VAConvertor::init()
{
    VAStatus vaStatus;
    LOG1("@%s", __FUNCTION__);

    mVA = new VideoVPPBase(mIsCacheRenderBuf);
    if (!mVA) {
        LOGE("Fail to construct VideoVPPBase");
        return NO_MEMORY;
    }

    vaStatus = mVA->start();
    CHECK_VASTATUS("start");

    mVPP = VPParameters::create(mVA);
    if (mVPP == NULL) {
        LOGE("Fail to create VPParameters");
        return UNKNOWN_ERROR;
    }
    LOG1("@%s end", __FUNCTION__);
    return OK;
}

void VAConvertor::clearBuffers(KeyedVector<BufferID , RenderTarget *> &buffers)
{
    for (unsigned int i = 0 ; i < buffers.size() ; i++) {
        RenderTarget *rt = buffers.valueAt(i);
        if (rt)
            delete rt;

        buffers.removeItemsAt(i);
    }
}

status_t VAConvertor::deInit()
{
    LOG1("@%s", __FUNCTION__);

    if (mVPP) {
        delete mVPP;
        mVPP = NULL;
    }

    if (mVA) {
        mVA->stop();
        delete mVA;
        mVA = NULL;
    }

    if (!mIBuffers.isEmpty()) {
        LOGW("Input buffer is not clear before destory");
        clearBuffers(mIBuffers);
    }

    if (!mOBuffers.isEmpty()) {
        LOGW("Output buffer is not clear before destory");
        clearBuffers(mOBuffers);
    }

    mIIDKey = 0;
    mOIDKey = 0;
    return OK;
}

status_t VAConvertor::stop()
{
    LOG1("@%s", __FUNCTION__);

    if (mVA) {
        mVA->stop();
    }
    return OK;
}

int VAConvertor::processFrame(int inputBufferId, int outputBufferId)
{
    VAStatus vaStatus;

    RenderTarget *in  = mIBuffers.valueFor(inputBufferId);
    RenderTarget *out = mOBuffers.valueFor(outputBufferId);

    if (in == NULL || out == NULL) {
        LOGE("Find error render target");
        return -1;
    }
    vaStatus = mVA->perform(*in, *out, mVPP, false);
    CHECK_VASTATUS("perform");

    return vaStatus;
}
status_t VAConvertor::mapV4L2FmtToVAFmt(int &vaRTFormat, int &vaFourcc, int graphicFormat)
{
    switch (graphicFormat) {
         case V4L2_PIX_FMT_NV12:
            vaRTFormat = VA_RT_FORMAT_YUV420;
            vaFourcc   = VA_FOURCC_NV12;
            break;
         case V4L2_PIX_FMT_NV21:
            vaRTFormat = VA_RT_FORMAT_YUV420;
            vaFourcc   = 0;
            break;
         case V4L2_PIX_FMT_YUYV:
            vaRTFormat = VA_RT_FORMAT_YUV422;
            vaFourcc   = VA_FOURCC_YUY2;
            break;
         case V4L2_PIX_FMT_YUV422P:
            vaRTFormat = VA_RT_FORMAT_YUV422;
            vaFourcc = VA_FOURCC_422H;
            break;
         case V4L2_PIX_FMT_YVU420:
            vaRTFormat = VA_RT_FORMAT_YUV420;
            vaFourcc = VA_FOURCC_YV12;
            break;
         case V4L2_PIX_FMT_YUV420:
            vaRTFormat = VA_RT_FORMAT_YUV420;
            vaFourcc = VA_FOURCC_YV12;
            break;
         default:
            LOGW("Graphic format:%x is not supported", graphicFormat);
            return BAD_VALUE;
    }

    return OK;
}


status_t VAConvertor::mapGraphicFmtToVAFmt(int &vaRTFormat, int &vaFourcc, int graphicFormat)
{
    switch (graphicFormat) {
        case HAL_PIXEL_FORMAT_NV12_TILED_INTEL:
            vaRTFormat = VA_RT_FORMAT_YUV420;
            vaFourcc   = VA_FOURCC_NV12;
            break;
        case HAL_PIXEL_FORMAT_YCbCr_422_I:
            vaRTFormat = VA_RT_FORMAT_YUV422;
            vaFourcc   = VA_FOURCC_YUY2;
            break;
        case HAL_PIXEL_FORMAT_YCbCr_422_H_INTEL:
            vaRTFormat = VA_RT_FORMAT_YUV422;
            vaFourcc = VA_FOURCC_422H;
            break;
        case HAL_PIXEL_FORMAT_YV12:
             vaRTFormat = VA_RT_FORMAT_YUV420;
             vaFourcc = VA_FOURCC_YV12;
             break;
        default:
            LOGW("Graphic format:%x is not supported", graphicFormat);
            return BAD_VALUE;
    }

    return OK;
}
int VAConvertor::addInputBuffer(buffer_handle_t pBufHandle, int width, int height, int format)
{
    RenderTarget *rt;
    struct mfx_gralloc_drm_handle_t *pGrallocHandle;

    LOG1("@%s %dx%d format:%x current count:%d", __FUNCTION__, width, height, format, mIIDKey);

    rt = new RenderTarget();
    if (rt == NULL) {
        LOGE("Fail to allocate RenderTarget");
        return -1;
    }

    pGrallocHandle = (struct mfx_gralloc_drm_handle_t *) pBufHandle;
    rt->width    = width;
    rt->height   = height;
    rt->stride   = pGrallocHandle->pitch;
    rt->type     = RenderTarget::KERNEL_DRM;
    rt->handle   = pGrallocHandle->name;
    rt->rect.x   = rt->rect.y = 0;
    rt->rect.width   = rt->width;
    rt->rect.height  = height;
    mapV4L2FmtToVAFmt(rt->format,rt->pixel_format, format);

    //add to vector
    mIBuffers.add(++mIIDKey, rt);
    return mIIDKey;
}
int VAConvertor::addOutputBuffer(buffer_handle_t pBufHandle, int width, int height, int format)
{
    RenderTarget *rt;
    struct mfx_gralloc_drm_handle_t *pGrallocHandle;

    LOG1("@%s %dx%d format:%x current count:%d", __FUNCTION__, width, height,format, mOIDKey);

    rt = new RenderTarget();
    if (rt == NULL) {
        LOGE("Fail to allocate RenderTarget");
        return -1;
    }

    //FIXME, will be removed when va driver support buffer handle directly.
    pGrallocHandle = (struct mfx_gralloc_drm_handle_t *) pBufHandle;
    rt->width    = width;
    rt->height   = height;
    rt->stride   = pGrallocHandle->pitch;
    rt->type     = RenderTarget::KERNEL_DRM;
    rt->handle   = pGrallocHandle->name;
    rt->rect.x   = rt->rect.y = 0;
    rt->rect.width   = rt->width;
    rt->rect.height  = height;
    mapV4L2FmtToVAFmt(rt->format, rt->pixel_format, format);

    //add to vector
    mOBuffers.add(++mOIDKey, rt);
    return mOIDKey;
}

void VAConvertor::removeInputBuffer(int bufferId)
{
    RenderTarget *rt = mIBuffers.valueFor(bufferId);
    if (rt) {
        delete rt;
        rt = NULL;
    }

    mIBuffers.removeItem(bufferId);
}
void VAConvertor::removeOutputBuffer(int bufferId)
{
    RenderTarget *rt = mOBuffers.valueFor(bufferId);
    if (rt) {
        delete rt;
        rt = NULL;
    }

    mOBuffers.removeItem(bufferId);
}

status_t VAConvertor::VPPColorConverter(buffer_handle_t input_handle,buffer_handle_t output_handle,int mInWidth,int mInHeight,int mInputFormat,int mOutWidth,int mOutHeight,int mOutputFormat)
{
    status_t status = NO_ERROR;
    int inIDkey = 0;
    int OutIDKey = 0;
    if(input_handle == 0L || output_handle== 0)
    {
       LOGE("input handle =%p, output handle =%p",input_handle,output_handle);
       return -1;//to be changed
     }
     inIDkey= addInputBuffer(input_handle,mInWidth,mInHeight,mInputFormat);
     OutIDKey = addOutputBuffer(output_handle,mOutWidth,mOutHeight,mOutputFormat);
     processFrame(inIDkey,OutIDKey);
     removeInputBuffer(inIDkey);
     removeOutputBuffer(OutIDKey);
     return status;
}

status_t VAConvertor::VPPBitBlit(RenderTarget *in, RenderTarget *out)
{
    VAStatus vaStatus;
    int inHalformat = 0;
    int outHalformat = 0;
    LOG1("@%s", __FUNCTION__);
    inHalformat = in->pixel_format;
    mapGraphicFmtToVAFmt(in->format,in->pixel_format,inHalformat);
    outHalformat = out->pixel_format;
    mapGraphicFmtToVAFmt(out->format,out->pixel_format,outHalformat);
    vaStatus = mVA->perform(*in, *out, mVPP, false);
    in->pixel_format = inHalformat;
    out->pixel_format = outHalformat;
    return vaStatus;
}

/*
create a RenderTarget for a buffer handle base it's related information
rt: the new RenderTarget
*/
status_t VAConvertor::ConfigBuffer(RenderTarget *rt, buffer_handle_t pBufHandle, int width, int height, int format)
{
    struct mfx_gralloc_drm_handle_t *pGrallocHandle = (struct mfx_gralloc_drm_handle_t *) pBufHandle;

    LOG1("@%s %dx%d format:%x current count:%d", __FUNCTION__, width, height, format, mIIDKey);

    if (rt == NULL) {
        LOGE("Fail to allocate RenderTarget");
        return -1;
    }

    rt->width    = width;
    rt->height   = height;
    rt->stride   = pGrallocHandle->pitch;
    rt->type     = RenderTarget::KERNEL_DRM;
    rt->handle   = pGrallocHandle->name;
    rt->rect.x   = rt->rect.y = 0;
    rt->rect.width   = rt->width;
    rt->rect.height  = height;
    rt->pixel_format =  format;

    return NO_ERROR;
}

}; // namespace android

