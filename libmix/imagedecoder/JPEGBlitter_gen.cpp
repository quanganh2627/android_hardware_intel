/*
* Copyright (c) 2009-2011 Intel Corporation.  All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

//#define LOG_NDEBUG 0
#include "JPEGBlitter.h"
#include "JPEGCommon_Gen.h"
#include "JPEGDecoder.h"
#include <utils/Timers.h>
#include <va/va.h>
#include <va/va_tpi.h>
#include "ImageDecoderTrace.h"
#include <cm/cm_rt.h>

#define BLIT_METHOD_CM 1 // 0 for VA+GpuCopy method, 1 for pure CM method

#define CM_KERNEL_FUNC_NAME yuv_tiled_to_rgba_linear

#define JD_CHECK(err, label) \
        if (err) { \
            ETRACE("%s::%d: failed: %d", __FUNCTION__, __LINE__, err); \
            goto label; \
        }

#define JD_CHECK_RET(err, label, retcode) \
        if (err) { \
            status = retcode; \
            ETRACE("%s::%d: failed: %d", __FUNCTION__, __LINE__, err); \
            goto label; \
        }

#define JD_CM_CHECK_RET(err, label, retcode) \
        if (err) { \
            status = retcode; \
            ETRACE("CM %s::%d: failed: 0x%08x", __FUNCTION__, __LINE__, err); \
            goto label; \
        }

VAProcColorStandardType fourcc2ColorStandard(uint32_t fourcc)
{
    switch(fourcc) {
    case VA_FOURCC_NV12:
    case VA_FOURCC_YUY2:
    case VA_FOURCC_UYVY:
    case VA_FOURCC('4','0','0','P'):
    case VA_FOURCC_411P:
    case VA_FOURCC_411R:
    case VA_FOURCC_IMC3:
    case VA_FOURCC_422H:
    case VA_FOURCC_422V:
    case VA_FOURCC_444P:
    case VA_FOURCC_YV12:
        return VAProcColorStandardBT601;
    default:
        return VAProcColorStandardNone;
    }
}

static JpegDecodeStatus vaVppBlit(VADisplay display, VAContextID context,
                 VASurfaceID in_surf, VARectangle *in_rect, uint32_t in_fourcc,
                 VASurfaceID out_surf, VARectangle *out_rect, uint32_t out_fourcc)
{
    VAProcPipelineCaps vpp_pipeline_cap ;
    VABufferID vpp_pipeline_buf = VA_INVALID_ID;
    VAProcPipelineParameterBuffer vpp_param;
    VAStatus vpp_status;
    JpegDecodeStatus status = JD_SUCCESS;
    char str[10];
    nsecs_t t1, t2;

    memset(&vpp_param, 0, sizeof(VAProcPipelineParameterBuffer));
    t1 = systemTime();
    vpp_param.surface                 = in_surf;
    vpp_param.output_region           = out_rect;
    vpp_param.surface_region          = in_rect;
    vpp_param.surface_color_standard  = fourcc2ColorStandard(in_fourcc);
    vpp_param.output_background_color = 0;
    vpp_param.output_color_standard   = fourcc2ColorStandard(out_fourcc);
    vpp_param.filter_flags            = VA_FRAME_PICTURE;
    vpp_param.filters                 = NULL;
    vpp_param.num_filters             = 0;
    vpp_param.forward_references      = 0;
    vpp_param.num_forward_references  = 0;
    vpp_param.backward_references     = 0;
    vpp_param.num_backward_references = 0;
    vpp_param.blend_state             = NULL;
    vpp_param.rotation_state          = VA_ROTATION_NONE;
    vpp_status = vaCreateBuffer(display,
                                context,
                                VAProcPipelineParameterBufferType,
                                sizeof(VAProcPipelineParameterBuffer),
                                1,
                                &vpp_param,
                                &vpp_pipeline_buf);
    JD_CHECK_RET(vpp_status, cleanup, JD_RESOURCE_FAILURE);

    vpp_status = vaBeginPicture(display,
                                context,
                                out_surf);
    JD_CHECK_RET(vpp_status, cleanup, JD_BLIT_FAILURE);

    //Render the picture
    vpp_status = vaRenderPicture(display,
                                 context,
                                 &vpp_pipeline_buf,
                                 1);
    JD_CHECK_RET(vpp_status, cleanup, JD_BLIT_FAILURE);

    vpp_status = vaEndPicture(display, context);
    JD_CHECK_RET(vpp_status, cleanup, JD_BLIT_FAILURE);

    vaDestroyBuffer(display, vpp_pipeline_buf);
    JD_CHECK_RET(vpp_status, cleanup, JD_BLIT_FAILURE);
    t2 = systemTime();
    VTRACE("Finished HW CSC %s(%d,%d,%u,%u)=>%s(%d,%d,%u,%u) for %f ms",
        fourcc2str(in_fourcc, str),
        in_rect->x, in_rect->y, in_rect->width, in_rect->height,
        fourcc2str(out_fourcc, str + 5),
        out_rect->x, out_rect->y, out_rect->width, out_rect->height,
        ns2us(t2 - t1)/1000.0);

    return JD_SUCCESS;
cleanup:
    if (vpp_pipeline_buf != VA_INVALID_ID)
        vaDestroyBuffer(display, vpp_pipeline_buf);
    return status;
}

static JpegDecodeStatus vaBlit(VADisplay display, VAContextID context,
                 VASurfaceID in_surf, VARectangle *in_rect, uint32_t in_fourcc,
                 VASurfaceID out_surf, VARectangle *out_rect, uint32_t out_fourcc)
{
    char fourccstr[10];
    ALOGD("%s, in %s, out %s", __FUNCTION__, fourcc2str(in_fourcc, fourccstr), fourcc2str(out_fourcc, fourccstr + 5));
    if (((in_fourcc == VA_FOURCC_422H) ||
        (in_fourcc == VA_FOURCC_444P) ||
        (in_fourcc == VA_FOURCC_IMC3) ||
        (in_fourcc == VA_FOURCC_411P) ||
        (in_fourcc == VA_FOURCC_422V) ||
        (in_fourcc == VA_FOURCC_NV12) ||
        (in_fourcc == VA_FOURCC_YUY2) ||
        (in_fourcc == VA_FOURCC_UYVY) ||
        (in_fourcc == VA_FOURCC_YV12) ||
        (in_fourcc == VA_FOURCC_BGRA) ||
        (in_fourcc == VA_FOURCC_RGBA))
        &&
        ((out_fourcc == VA_FOURCC_422H) ||
        (out_fourcc == VA_FOURCC_444P) ||
        (out_fourcc == VA_FOURCC_IMC3) ||
        (out_fourcc == VA_FOURCC_411P) ||
        (out_fourcc == VA_FOURCC_422V) ||
        (out_fourcc == VA_FOURCC_NV12) ||
        (out_fourcc == VA_FOURCC_YV12) ||
        (out_fourcc == VA_FOURCC_YUY2) ||
        (out_fourcc == VA_FOURCC_UYVY) ||
        (out_fourcc == VA_FOURCC_BGRA) ||
        (out_fourcc == VA_FOURCC_RGBA))) {
        return vaVppBlit(display, context, in_surf, in_rect, in_fourcc,
               out_surf, out_rect, out_fourcc);
    }
    else {
        return JD_INPUT_FORMAT_UNSUPPORTED;
    }
}

static CmDevice *pDev = NULL;
static CmProgram *pProgram = NULL;
static CmKernel *pKernel = NULL;
static Mutex cmLock;
JpegDecodeStatus JpegBlitter::preInit(VADisplay display)
{
    nsecs_t t1, t2;
    t1 = t2 = systemTime();
#if BLIT_METHOD_CM
#define ISA_FILE "/system/etc/libmix_imagedecoder_genx.isa"
    if (!pDev || !pProgram) {
        Mutex::Autolock autoCmLock(cmLock);
        if (!pDev || !pProgram) {
            ITRACE("%s CM is not initialized yet, pre-init it", __FUNCTION__);
            UINT ver;
            INT result;
            FILE* pIsaFile = NULL;
            int codeSize;
            BYTE* pIsaBytes = NULL;
            result = CreateCmDevice(pDev, ver, display);
            if (result != CM_SUCCESS) {
                ETRACE("%s CreateCmDevice failed: %d", __FUNCTION__, result);
                return JD_INITIALIZATION_ERROR;
            }

            pIsaFile = fopen(ISA_FILE, "rb");
            if (pIsaFile==NULL) {
                ETRACE("%s fopen failed", __FUNCTION__);
                DestroyCmDevice(pDev);
                return JD_INITIALIZATION_ERROR;
            }
            fseek (pIsaFile, 0, SEEK_END);
            codeSize = ftell (pIsaFile);
            rewind(pIsaFile);
            if (codeSize==0) {
                ETRACE("%s codesize failed", __FUNCTION__);
                DestroyCmDevice(pDev);
                fclose(pIsaFile);
                return JD_INITIALIZATION_ERROR;
            }
            pIsaBytes = (BYTE*) malloc(codeSize);
            if (pIsaBytes==NULL) {
                ETRACE("%s malloc failed", __FUNCTION__);
                DestroyCmDevice(pDev);
                fclose(pIsaFile);
                return JD_INITIALIZATION_ERROR;
            }
            if (fread(pIsaBytes, 1, codeSize, pIsaFile) != (size_t)codeSize) {
                ETRACE("%s fread failed", __FUNCTION__);
                free(pIsaBytes);
                DestroyCmDevice(pDev);
                fclose(pIsaFile);
                return JD_INITIALIZATION_ERROR;
            }
            fclose(pIsaFile);
            pIsaFile = NULL;

            result = pDev->LoadProgram(pIsaBytes, codeSize, pProgram);
            if (result != CM_SUCCESS) {
                ETRACE("%s LoadProgram failed: %d", __FUNCTION__, result);
                free(pIsaBytes);
                DestroyCmDevice(pDev);
                return JD_INITIALIZATION_ERROR;
            }
            free(pIsaBytes);
            pIsaBytes = NULL;

            t2 = systemTime();
            VTRACE("%s CM pre-init succeded, took %.2f ms", __FUNCTION__, (t2-t1)/1000000.0);
        }
    }
    return JD_SUCCESS;
#else
    if (!pDev) {
        ITRACE("%s CM is not initialized yet, pre-init it", __FUNCTION__);
        UINT ver;
        INT result;
        result = CreateCmDevice(pDev, ver, display);
        if (result != CM_SUCCESS || !pDev) {
            ETRACE("%s CreateCmDevice returns %d", __FUNCTION__, result);
            return JD_INITIALIZATION_ERROR;
        }
        t2 = systemTime();
        VTRACE("%s CM pre-init succeded, took %.2f ms", __FUNCTION__, (t2-t1)/1000000.0);
    }
    return JD_SUCCESS;
#endif
}
void JpegBlitter::init(JpegDecoder &dec)
{
    if (!mInitialized) {
        Mutex::Autolock autoLock(mLock);
        if (!mInitialized) {
            mDecoder = &dec;
            mInitialized = true;
        }
    }
}

void JpegBlitter::deinit()
{
    if (mInitialized) {
        Mutex::Autolock autoLock(mLock);
        if (mInitialized) {
#if BLIT_METHOD_CM
            // do nothing since DRM releases everything when FD is closed
#endif
            mInitialized = false;
        }
    }
    mPrivate = NULL;
}

JpegDecodeStatus JpegBlitter::blit(RenderTarget &src, RenderTarget &dst, int scale_factor)
{
    if (!mInitialized) {
        ETRACE("%s not initialized", __FUNCTION__);
        return JD_UNINITIALIZED;
    }
    if (mDecoder == NULL)
        return JD_UNINITIALIZED;
    JpegDecodeStatus st;
    uint32_t src_fourcc, dst_fourcc;
    char tmp[10];
    src_fourcc = src.pixel_format;
    dst_fourcc = dst.pixel_format;
    VASurfaceID src_surf = mDecoder->getSurfaceID(src);
    if (src_surf == VA_INVALID_ID) {
        ETRACE("%s invalid src %s target", __FUNCTION__, fourcc2str(src_fourcc));
        return JD_INVALID_RENDER_TARGET;
    }
    VASurfaceID dst_surf = mDecoder->getSurfaceID(dst);
    if (dst_surf == VA_INVALID_ID) {
        WTRACE("%s foreign dst target for JpegDecoder, create surface for it, not guaranteed to free it!!!", __FUNCTION__);
        st = mDecoder->createSurfaceFromRenderTarget(dst, &dst_surf);
        if (st != JD_SUCCESS || dst_surf == VA_INVALID_ID) {
            ETRACE("%s failed to create surface for dst target", __FUNCTION__);
            return JD_RESOURCE_FAILURE;
        }
    }

    VTRACE("%s blitting from %s to %s", __FUNCTION__, fourcc2str(src_fourcc, tmp), fourcc2str(dst_fourcc, tmp + 5));
    st = vaBlit(mDecoder->mDisplay, mContextId, src_surf, &src.rect, src_fourcc,
                dst_surf, &dst.rect, dst_fourcc);

    return st;
}

static JpegDecodeStatus blitToLinearRgba_va_gpucopy(JpegDecoder *decoder,
        VADisplay dp, VAContextID ctx, RenderTarget &src,
        uint8_t *sysmem, uint32_t width, uint32_t height, int scale_factor)
{
    CmQueue *pQueue = NULL;
    CmSurface2D *pSurf= NULL;
    CmEvent *pEvent = NULL;
    INT result;
    UINT ver;
    RenderTarget target;
    VASurfaceID surf;
    nsecs_t t1, t2, t3, t4;
    target.type = RenderTarget::INTERNAL_BUF;
    target.pixel_format = VA_FOURCC_RGBA;
    target.handle = generateHandle();
    target.width = aligned_width(width, SURF_TILING_Y);
    target.height = aligned_height(height, SURF_TILING_Y);
    target.stride = aligned_width(width, SURF_TILING_Y);
    target.rect.x = target.rect.y = 0;
    target.rect.width = width;
    target.rect.height = height;
    VASurfaceID src_surf = decoder->getSurfaceID(src);
    if (src_surf == VA_INVALID_ID) {
        ETRACE("%s invalid src %s target", __FUNCTION__, fourcc2str(src.pixel_format));
        return JD_INVALID_RENDER_TARGET;
    }
    JpegDecodeStatus st = decoder->createSurfaceFromRenderTarget(target, &surf);
    if (st != JD_SUCCESS || surf == VA_INVALID_ID) {
        ETRACE("%s failed to create surface for RGBA linear target", __FUNCTION__);
        return JD_RESOURCE_FAILURE;
    }
    st = vaBlit(dp, ctx, src_surf, &src.rect, src.pixel_format,
                surf, &target.rect, target.pixel_format);

    if (st) {
        ETRACE("%s: failed to blit to RGBA via VAAPI", __FUNCTION__);
        decoder->destroySurface(target);
        return JD_BLIT_FAILURE;
    }

    t1 = systemTime();
    result = pDev->CreateSurface2D(surf, pSurf);
    if (result != CM_SUCCESS || !pSurf) {
        ETRACE("%s CmCreateSurface2D returns %d", __FUNCTION__, result);
        decoder->destroySurface(target);
        return JD_BLIT_FAILURE;
    }
    result = pDev->CreateQueue( pQueue);
    if (result != CM_SUCCESS || !pQueue) {
        ETRACE("%s CmCreateQueue returns %d", __FUNCTION__, result);
        pDev->DestroySurface(pSurf);
        decoder->destroySurface(target);
        return JD_BLIT_FAILURE;
    }
    t2 = systemTime();
    result = pQueue->EnqueueCopyGPUToCPU(pSurf, sysmem, pEvent);
    if (result != CM_SUCCESS) {
        ETRACE("%s CmEnqueueCopyGPUToCPU returns %d", __FUNCTION__, result);
        pDev->DestroySurface(pSurf);
        decoder->destroySurface(target);
        return JD_BLIT_FAILURE;
    }
    DWORD dwTimeOutMs = 0;
    result = pEvent->WaitForTaskFinished(dwTimeOutMs);
    if (result != CM_SUCCESS) {
        ETRACE("%s WaitForTaskFinished returns %d", __FUNCTION__, result);
        pDev->DestroySurface(pSurf);
        decoder->destroySurface(target);
        return JD_BLIT_FAILURE;
    }
    t3 = systemTime();
    result = pDev->DestroySurface(pSurf);
    if (result != CM_SUCCESS) {
        WTRACE("%s CmDestroySurface returns %d", __FUNCTION__, result);
    }
    t4 = systemTime();
    st = decoder->destroySurface(target);
    if (st) {
        WTRACE("%s: failed to destroy VA surface", __FUNCTION__);
    }
    ITRACE("%s: cm GpuCopy took %.2f+%.2f+%.2f ms", __FUNCTION__,
        (t2 - t1)/1000000.0,
        (t3 - t2)/1000000.0,
        (t4 - t3)/1000000.0);
    return st;
}

JpegDecodeStatus JpegBlitter::getRgbaTile(RenderTarget &src,
                                     uint8_t *sysmem,
                                     int left, int top, int width, int height, int scale_factor)
{
    const int CM_GPU_TASK_WIDTH = 8;
    const int CM_GPU_TASK_HEIGHT = 8;
    VASurfaceID srcVaId;

    srcVaId = mDecoder->getSurfaceID(src);
    JpegDecodeStatus status = JD_SUCCESS;
    uint32_t aligned_w = width;
    uint32_t aligned_h = height;

    CmThreadSpace *pThreadSpace = NULL;
    CmTask *pKernelArray = NULL;
    CmQueue *pQueue = NULL;
    CmSurface2D *pInSurf= NULL;
    SurfaceIndex *pInSurfId = NULL;
    CmBufferUP *pOutBuf = NULL;
    SurfaceIndex *pOutBufId = NULL;
    CmEvent *pEvent = NULL;
    UINT ver;
    int threadswidth, threadsheight;
    INT result;
    DWORD dwTimeOutMs = -1;
    uint32_t cm_in_fourcc;
    threadswidth = aligned_w/CM_GPU_TASK_WIDTH;
    threadsheight = aligned_h/CM_GPU_TASK_HEIGHT;
    nsecs_t t1, t2, t3, t4, t5, t6, t7;
    Mutex::Autolock autoLock(cmLock);
    t1 = t2 = t3 = t4 = t5 = t6 = t7 = systemTime();

    if (!pDev || !pProgram) {
        ETRACE("%s CM not initialized", __FUNCTION__);
        return JD_UNINITIALIZED;
    }

    t2 = systemTime();
    // create thread space
    result = pDev->CreateKernel(pProgram, CM_KERNEL_FUNCTION(yuv_tiled_to_rgba_tile), pKernel);
    JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);

    result = pDev->CreateSurface2D(srcVaId, pInSurf);
    JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);
    result = pInSurf->GetIndex(pInSurfId);
    JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);
    // create bufferUp from dst ptr
    result = pDev->CreateBufferUP(aligned_w * aligned_h * 4, sysmem, pOutBuf);
    JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);
    result = pOutBuf->GetIndex(pOutBufId);
    JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);

    result = pDev->CreateQueue( pQueue);
    JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);
    result = pDev->CreateThreadSpace(threadswidth, threadsheight, pThreadSpace);
    JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);
    result = pKernel->SetThreadCount( threadswidth* threadsheight );
    JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);
    // enqueue csc
    pKernel->SetKernelArg(0,sizeof(SurfaceIndex),pInSurfId);
    pKernel->SetKernelArg(1,sizeof(SurfaceIndex),pOutBufId);
    pKernel->SetKernelArg(2,sizeof(int),&left);
    pKernel->SetKernelArg(3,sizeof(int),&top);
    pKernel->SetKernelArg(4,sizeof(int),&aligned_w);
    pKernel->SetKernelArg(5,sizeof(int),&aligned_h);

    cm_in_fourcc = src.pixel_format;

    pKernel->SetKernelArg(6,sizeof(uint32_t),&cm_in_fourcc);
    result = pDev->CreateTask(pKernelArray);
    JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);
    result = pKernelArray->AddKernel (pKernel);
    JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);
    result = pQueue->Enqueue(pKernelArray, pEvent, pThreadSpace);
    JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);
    // wait kernel finish
    t3 = systemTime();
    result = pEvent->WaitForTaskFinished(dwTimeOutMs);
    JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);
    //event = NULL;//(BlitEvent)pEvent;
    t4 = systemTime();

cleanup:
    // destroy thread space/house cleaning
    if (pOutBuf) pDev->DestroyBufferUP(pOutBuf);
    t5 = systemTime();
    if (pInSurf) pDev->DestroySurface(pInSurf);
    t6 = systemTime();
    if (pKernelArray) pDev->DestroyTask(pKernelArray);
    if (pThreadSpace) pDev->DestroyThreadSpace(pThreadSpace);
    if (pKernel) pDev->DestroyKernel(pKernel);
    t7 = systemTime();

    VTRACE("%s blit with CM %ux%u took %.2f + %.2f + %.2f + %.2f + %.2f + %.2f ms", __FUNCTION__,
        width, height,
        (t2 - t1)/1000000.0,
        (t3 - t2)/1000000.0,
        (t4 - t3)/1000000.0,
        (t5 - t4)/1000000.0,
        (t6 - t5)/1000000.0,
        (t7 - t6)/1000000.0);
    return status;
}


static JpegDecodeStatus blitToLinearRgba_cm(JpegDecoder *decoder,
        VADisplay dp, VAContextID ctx, RenderTarget &src, uint8_t *sysmem, uint32_t width, uint32_t height,
        BlitEvent &event, int scale_factor)
{
    const int CM_GPU_TASK_WIDTH = 32;
    const int CM_GPU_TASK_HEIGHT = 8;
    VASurfaceID srcVaId;
    Mutex::Autolock autoLock(cmLock);

    srcVaId = decoder->getSurfaceID(src);
    JpegDecodeStatus status = JD_SUCCESS;
    uint32_t aligned_in_w = aligned_width(width, SURF_TILING_Y);
    uint32_t aligned_in_h = aligned_height(height, SURF_TILING_Y);
    uint32_t aligned_out_w = aligned_width(width/scale_factor, SURF_TILING_Y);
    uint32_t aligned_out_h = aligned_height(height/scale_factor, SURF_TILING_Y);

    CmThreadSpace *pThreadSpace = NULL;
    CmTask *pKernelArray = NULL;
    CmQueue *pQueue = NULL;
    CmSurface2D *pInSurf= NULL;
    SurfaceIndex *pInSurfId = NULL;
    CmBufferUP *pOutBuf = NULL;
    SurfaceIndex *pOutBufId = NULL;
    CmEvent *pEvent = NULL;
    UINT ver;
    int threadswidth, threadsheight;
    INT result;
    DWORD dwTimeOutMs = -1;
    uint32_t cm_in_fourcc;
    threadswidth = aligned_in_w/CM_GPU_TASK_WIDTH;
    threadsheight = aligned_in_h/CM_GPU_TASK_HEIGHT;
    nsecs_t t1, t2, t3, t4, t5, t6, t7, t8, t9, t10;
    t1 = t2 = t3 = t4 = t5 = t6 = t7 = t8 = t9 = t10 = systemTime();

    if (!pDev || !pProgram) {
        ETRACE("%s CM not initialized", __FUNCTION__);
        return JD_UNINITIALIZED;
    }

    // create thread space
    result = pDev->CreateKernel(pProgram, CM_KERNEL_FUNCTION(CM_KERNEL_FUNC_NAME), pKernel);
    JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);

    t2 = systemTime();
    result = pDev->CreateSurface2D(srcVaId, pInSurf);
    JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);

    result = pInSurf->GetIndex(pInSurfId);
    JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);
    // create bufferUp from dst ptr
    t3 = systemTime();
    result = pDev->CreateBufferUP(aligned_out_w * aligned_out_h * 4, sysmem, pOutBuf);
    JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);
    result = pOutBuf->GetIndex(pOutBufId);
    JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);
    t4 = systemTime();
    result = pDev->CreateQueue( pQueue);
    JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);
    result = pDev->CreateThreadSpace(threadswidth, threadsheight, pThreadSpace);
    JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);
    result = pKernel->SetThreadCount( threadswidth* threadsheight );
    JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);
    // enqueue csc
    pKernel->SetKernelArg(0,sizeof(SurfaceIndex),pInSurfId);
    pKernel->SetKernelArg(1,sizeof(SurfaceIndex),pOutBufId);
    pKernel->SetKernelArg(2,sizeof(int),&aligned_out_w);
    cm_in_fourcc = src.pixel_format;
    pKernel->SetKernelArg(3,sizeof(uint32_t),&cm_in_fourcc);
    pKernel->SetKernelArg(4,sizeof(int), &scale_factor);
    result = pDev->CreateTask(pKernelArray);
    JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);
    result = pKernelArray->AddKernel (pKernel);
    JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);
    result = pQueue->Enqueue(pKernelArray, pEvent, pThreadSpace);
    JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);
    // wait kernel finish
    t5 = systemTime();
    result = pEvent->WaitForTaskFinished(dwTimeOutMs);
    JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);
    event = NULL;//(BlitEvent)pEvent;
    t6 = systemTime();

cleanup:
    // destroy thread space/house cleaning
    if (pOutBuf) pDev->DestroyBufferUP(pOutBuf);
    t7 = systemTime();
    if (pInSurf) pDev->DestroySurface(pInSurf);
    t8 = systemTime();
    if (pKernelArray) pDev->DestroyTask(pKernelArray);
    if (pThreadSpace) pDev->DestroyThreadSpace(pThreadSpace);
    if (pKernel) pDev->DestroyKernel(pKernel);
    t9 = systemTime();

    VTRACE("%s blit with CM %ux%u(%dx) took %.2f + %.2f + %.2f + %.2f + %.2f + %.2f + %.2f + %.2f ms", __FUNCTION__,
        width, height, scale_factor,
        (t2 - t1)/1000000.0,
        (t3 - t2)/1000000.0,
        (t4 - t3)/1000000.0,
        (t5 - t4)/1000000.0,
        (t6 - t5)/1000000.0,
        (t7 - t6)/1000000.0,
        (t8 - t7)/1000000.0,
        (t9 - t8)/1000000.0);
    return status;
}

JpegDecodeStatus JpegBlitter::blitToLinearRgba(RenderTarget &src,
                                               uint8_t *sysmem,
                                               uint32_t width, uint32_t height,
                                               BlitEvent &event, int scale_factor)
{
    Mutex::Autolock autoLock(mDecoder->mLock);
#if BLIT_METHOD_CM
    return blitToLinearRgba_cm(mDecoder, mDecoder->mDisplay, mContextId, src, sysmem, width, height, event, scale_factor);//, (CmSurface2D*&)mPrivate);
#else
    return blitToLinearRgba_va_gpucopy(mDecoder, mDecoder->mDisplay, mContextId, src, sysmem, width, height, scale_factor);
#endif
}

JpegDecodeStatus JpegBlitter::blitToCameraSurfaces(RenderTarget &src,
                                                   buffer_handle_t dst_nv12,
                                                   buffer_handle_t dst_yuy2,
                                                   uint8_t *dst_nv21,
                                                   uint8_t *dst_yv12,
                                                   uint32_t width, uint32_t height,
                                                   BlitEvent &event)
{
#define CM_GPU_TASK_WIDTH 32
#define CM_GPU_TASK_HEIGHT 8
    VASurfaceID srcVaId, nv12_surf_id, yuy2_surf_id;
    srcVaId = nv12_surf_id = yuy2_surf_id = VA_INVALID_ID;
    srcVaId = mDecoder->getSurfaceID(src);
    JpegDecodeStatus status = JD_SUCCESS;
    uint32_t aligned_w = aligned_width(width, SURF_TILING_Y);
    uint32_t aligned_h = aligned_height(height, SURF_TILING_Y);

    CmThreadSpace *pThreadSpace = NULL;
    CmTask *pKernelArray = NULL;
    CmQueue *pQueue = NULL;
    CmSurface2D *pInSurf= NULL;
    SurfaceIndex *pInSurfId = NULL;
    CmSurface2D *pOutNV12Surf= NULL;
    SurfaceIndex *pOutNV12SurfId = NULL;
    CmSurface2D *pOutYUY2Surf= NULL;
    SurfaceIndex *pOutYUY2SurfId = NULL;
    CmBufferUP *pOutNV21Surf = NULL;
    SurfaceIndex *pOutNV21SurfId = NULL;
    CmBufferUP *pOutYV12Surf = NULL;
    SurfaceIndex *pOutYV12SurfId = NULL;
    uint8_t do_nv21, do_yv12;
    do_nv21 = do_yv12 = 0;
    CmEvent *pEvent = NULL;
    RenderTarget nv12_target, yuy2_target;
    UINT ver;
    int threadswidth, threadsheight;
    INT result;
    DWORD dwTimeOutMs = -1;
    uint32_t cm_in_fourcc;
    threadswidth = aligned_w/CM_GPU_TASK_WIDTH;
    threadsheight = aligned_h/CM_GPU_TASK_HEIGHT;
    nsecs_t t1, t2, t3, t4, t5;
    t1 = t2 = t3 = t4 = t5 = systemTime();
    Mutex::Autolock autoLock(cmLock);

    if (!pDev || !pProgram) {
        ETRACE("%s CM not initialized", __FUNCTION__);
        return JD_UNINITIALIZED;
    }
    t2 = systemTime();
    // create thread space
    result = pDev->CreateKernel(pProgram, CM_KERNEL_FUNCTION(yuv422h_tiled_to_camera_surfaces), pKernel);
    JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);

    // src surface
    result = pDev->CreateSurface2D(srcVaId, pInSurf);
    JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);
    result = pInSurf->GetIndex(pInSurfId);
    JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);
    // dst nv12 + yuy2
    nv12_target.handle = (int)dst_nv12;
    nv12_target.type = RenderTarget::ANDROID_GRALLOC;
    nv12_target.height = aligned_height(height, SURF_TILING_Y);
    nv12_target.width = aligned_width(width, SURF_TILING_Y);
    nv12_target.pixel_format = VA_FOURCC_NV12;
    nv12_target.stride = nv12_target.width;
    nv12_target.rect.x = nv12_target.rect.y = 0;
    nv12_target.rect.width = nv12_target.width;
    nv12_target.rect.height = nv12_target.height;
    mDecoder->createSurfaceFromRenderTarget(nv12_target, &nv12_surf_id);
    yuy2_target.handle = (int)dst_yuy2;
    yuy2_target.type = RenderTarget::ANDROID_GRALLOC;
    yuy2_target.height = aligned_height(height, SURF_TILING_Y);
    yuy2_target.width = aligned_width(width, SURF_TILING_Y);
    yuy2_target.pixel_format = VA_FOURCC_YUY2;
    yuy2_target.stride = yuy2_target.width * 2;
    yuy2_target.rect.x = yuy2_target.rect.y = 0;
    yuy2_target.rect.width = yuy2_target.width;
    yuy2_target.rect.height = yuy2_target.height;
    mDecoder->createSurfaceFromRenderTarget(yuy2_target, &yuy2_surf_id);
    result = pDev->CreateSurface2D(nv12_surf_id, pOutNV12Surf);
    JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);
    result = pOutNV12Surf->GetIndex(pOutNV12SurfId);
    JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);
    result = pDev->CreateSurface2D(yuy2_surf_id, pOutYUY2Surf);
    JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);
    result = pOutYUY2Surf->GetIndex(pOutYUY2SurfId);
    JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);
    // dst nv21
    if (dst_nv21) {
        result = pDev->CreateBufferUP(aligned_w * aligned_h * 3 / 2, dst_nv21, pOutNV21Surf);
        JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);
        result = pOutNV21Surf->GetIndex(pOutNV21SurfId);
        JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);
        do_nv21 = 1;
    }
    else {
        pOutNV21SurfId = pInSurfId;
        do_nv21 = 0;
    }
    // dst yv12
    if (dst_yv12) {
        result = pDev->CreateBufferUP(aligned_w * aligned_h * 3 / 2, dst_yv12, pOutYV12Surf);
        JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);
        result = pOutYV12Surf->GetIndex(pOutYV12SurfId);
        JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);
        do_yv12 = 1;
    }
    else {
        pOutYV12SurfId = pInSurfId;
        do_yv12 = 0;
    }
    result = pDev->CreateQueue( pQueue);
    JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);
    result = pDev->CreateThreadSpace(threadswidth, threadsheight, pThreadSpace);
    JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);
    result = pKernel->SetThreadCount( threadswidth* threadsheight );
    JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);
    // enqueue csc
    pKernel->SetKernelArg(0,sizeof(SurfaceIndex),pInSurfId);
    pKernel->SetKernelArg(1,sizeof(SurfaceIndex),pOutNV12SurfId);
    pKernel->SetKernelArg(2,sizeof(SurfaceIndex),pOutYUY2SurfId);
    pKernel->SetKernelArg(3,sizeof(SurfaceIndex),pOutNV21SurfId);
    pKernel->SetKernelArg(4,sizeof(SurfaceIndex),pOutYV12SurfId);
    pKernel->SetKernelArg(5,sizeof(int),&aligned_h);
    pKernel->SetKernelArg(6,sizeof(int),&aligned_w);
    pKernel->SetKernelArg(7,sizeof(uint8_t),&do_nv21);
    pKernel->SetKernelArg(8,sizeof(uint8_t),&do_yv12);
    result = pDev->CreateTask(pKernelArray);
    JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);
    result = pKernelArray->AddKernel (pKernel);
    JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);
    result = pQueue->Enqueue(pKernelArray, pEvent, pThreadSpace);
    JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);
    // wait kernel finish
    t3 = systemTime();
    //result = pEvent->WaitForTaskFinished(dwTimeOutMs);
    //JD_CM_CHECK_RET(result, cleanup, JD_BLIT_FAILURE);
    event = (BlitEvent)pEvent;
    t4 = systemTime();

cleanup:
    // destroy thread space/house cleaning
    if (pOutYV12Surf) pDev->DestroyBufferUP(pOutYV12Surf);
    if (pOutNV21Surf) pDev->DestroyBufferUP(pOutNV21Surf);
    if (pOutYUY2Surf) pDev->DestroySurface(pOutYUY2Surf);
    if (pOutNV12Surf) pDev->DestroySurface(pOutNV12Surf);
    if (pInSurf) pDev->DestroySurface(pInSurf);
    if (nv12_surf_id != VA_INVALID_ID) mDecoder->destroySurface(nv12_target);
    if (yuy2_surf_id != VA_INVALID_ID) mDecoder->destroySurface(yuy2_target);
    if (pKernelArray) pDev->DestroyTask(pKernelArray);
    if (pThreadSpace) pDev->DestroyThreadSpace(pThreadSpace);
    if (pKernel) pDev->DestroyKernel(pKernel);
    t5 = systemTime();
    VTRACE("%s blit with CM took %.2f + %.2f + %.2f + %.2f ms", __FUNCTION__,
        (t2 - t1)/1000000.0,
        (t3 - t2)/1000000.0,
        (t4 - t3)/1000000.0,
        (t5 - t4)/1000000.0);
    return status;
}

void JpegBlitter::syncBlit(BlitEvent &event)
{
    nsecs_t now = systemTime();
    DWORD dwTimeOutMs = -1;
    CmEvent *pEvent = (CmEvent*)event;
    UINT64 executionTime;
    if (event == NULL)
        return;
    INT result = pEvent->WaitForTaskFinished(dwTimeOutMs);
    if (result != CM_SUCCESS) {
        ETRACE("%s: Failed to sync blit event", __FUNCTION__);
    }
    else {
        event = NULL;
        VTRACE("%s: syncBlit took %.2f ms", __FUNCTION__, (systemTime()-now)/1000000.0);
    }
}

