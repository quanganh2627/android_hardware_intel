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

#include "va/va.h"
#include "va/va_vpp.h"
#include "va/va_drmcommon.h"
#include "va/va_tpi.h"
#include "JPEGDecoder.h"
#include "ImageDecoderTrace.h"
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include "JPEGCommon_Gen.h"

uint32_t aligned_height(uint32_t height, int tiling)
{
    switch(tiling) {
    // Y-tile (128 x 32): NV12, 411P, IMC3, 422H, 422V, 444P
    case SURF_TILING_Y:
        return (height + (32-1)) & ~(32-1);
    // X-tile (512 x 8):
    case SURF_TILING_X:
        return (height + (8-1)) & ~(8-1);
    // Linear: other
    default:
        return height;
    }
}
uint32_t aligned_width(uint32_t width, int tiling)
{
    switch(tiling) {
    // Y-tile (128 x 32): NV12, 411P, IMC3, 422H, 422V, 444P
    case SURF_TILING_Y:
        return (width + (128-1)) & ~(128-1);
    // X-tile (512 x 8):
    case SURF_TILING_X:
        return (width + (512-1)) & ~(512-1);
    // Linear: other
    default:
        return width;
    }
}

int fourcc2PixelFormat(uint32_t fourcc)
{
    switch(fourcc) {
    case VA_FOURCC_YV12:
        return HAL_PIXEL_FORMAT_YV12;
    case VA_FOURCC_422H:
        return HAL_PIXEL_FORMAT_YCbCr_422_H_INTEL;
    case VA_FOURCC_YUY2:
        return HAL_PIXEL_FORMAT_YCbCr_422_I;
    case VA_FOURCC_NV12:
        return HAL_PIXEL_FORMAT_NV12_TILED_INTEL;
    case VA_FOURCC_RGBA:
        return HAL_PIXEL_FORMAT_RGBA_8888;
    default:
        return -1;
    }
}
uint32_t pixelFormat2Fourcc(int pixel_format)
{
    switch(pixel_format) {
    case HAL_PIXEL_FORMAT_YV12:
        return VA_FOURCC_YV12;
    case HAL_PIXEL_FORMAT_YCbCr_422_H_INTEL:
        return VA_FOURCC_422H;
    case HAL_PIXEL_FORMAT_YCbCr_422_I:
        return VA_FOURCC_YUY2;
    case HAL_PIXEL_FORMAT_NV12_TILED_INTEL:
        return VA_FOURCC_NV12;
    case HAL_PIXEL_FORMAT_RGBA_8888:
        return VA_FOURCC_RGBA;
    default:
        return 0;
    }
}

#define JD_CHECK(err, label) \
        if (err) { \
            ETRACE("%s::%d: failed: %d", __PRETTY_FUNCTION__, __LINE__, err); \
            goto label; \
        }

#define JD_CHECK_RET(err, label, retcode) \
        if (err) { \
            status = retcode; \
            ETRACE("%s::%d: failed: %d", __PRETTY_FUNCTION__, __LINE__, err); \
            goto label; \
        }

bool JpegDecoder::jpegColorFormatSupported(JpegInfo &jpginfo) const
{
    return (jpginfo.image_color_fourcc == VA_FOURCC_IMC3) ||
        (jpginfo.image_color_fourcc == VA_FOURCC_422H) ||
        (jpginfo.image_color_fourcc == VA_FOURCC_422V) ||
        (jpginfo.image_color_fourcc == VA_FOURCC_411P) ||
        //(jpginfo.image_color_fourcc == VA_FOURCC('4','0','0','P')) ||
        (jpginfo.image_color_fourcc == VA_FOURCC_444P);
}

JpegDecodeStatus JpegDecoder::createSurfaceDrm(int width, int height, uint32_t fourcc, unsigned long boname, int stride, VASurfaceID *surf_id)
{
    VAStatus st;
    VASurfaceAttrib                 attrib_list;
    VASurfaceAttribExternalBuffers  vaSurfaceExternBuf;
    memset(&vaSurfaceExternBuf, 0, sizeof (VASurfaceAttribExternalBuffers));
    vaSurfaceExternBuf.pixel_format = fourcc;
    vaSurfaceExternBuf.width        = width;
    vaSurfaceExternBuf.height       = height;
    vaSurfaceExternBuf.pitches[0]   = stride;
    vaSurfaceExternBuf.buffers      = &boname;
    vaSurfaceExternBuf.num_buffers  = 1;
    vaSurfaceExternBuf.flags        = VA_SURFACE_ATTRIB_MEM_TYPE_KERNEL_DRM;
    attrib_list.type          = VASurfaceAttribExternalBufferDescriptor;
    attrib_list.flags         = VA_SURFACE_ATTRIB_SETTABLE;
    attrib_list.value.type    = VAGenericValueTypePointer;
    attrib_list.value.value.p = (void *)&vaSurfaceExternBuf;

    VTRACE("%s, vaformat=0x%x, width=%d, height=%d, attrib=", __FUNCTION__, fourcc2VaFormat(fourcc),
        width, height);
    VTRACE("            ext.pixel_format=0x%x", vaSurfaceExternBuf.pixel_format);
    VTRACE("            ext.width=%u", vaSurfaceExternBuf.width);
    VTRACE("            ext.height=%u", vaSurfaceExternBuf.height);
    VTRACE("            ext.data_size=%u", vaSurfaceExternBuf.data_size);
    VTRACE("            ext.num_planes=%u", vaSurfaceExternBuf.num_planes);
    VTRACE("            ext.pitches=%u,%u,%u,%u", vaSurfaceExternBuf.pitches[0],vaSurfaceExternBuf.pitches[1],vaSurfaceExternBuf.pitches[2],vaSurfaceExternBuf.pitches[3]);
    VTRACE("            ext.offsets=%u,%u,%u,%u", vaSurfaceExternBuf.offsets[0],vaSurfaceExternBuf.offsets[1],vaSurfaceExternBuf.offsets[2],vaSurfaceExternBuf.offsets[3]);
    VTRACE("            ext.buffers[0]=%lu", vaSurfaceExternBuf.buffers[0]);
    VTRACE("            ext.num_buffers=%u", vaSurfaceExternBuf.num_buffers);
    VTRACE("            ext.flags=%u", vaSurfaceExternBuf.flags);
    VTRACE("            attrib_list.type=%u", attrib_list.type);
    VTRACE("            attrib_list.flags=%u", attrib_list.flags);
    VTRACE("            attrib_list.type=%u", attrib_list.value.type);

    st = vaCreateSurfaces(mDisplay,
            fourcc2VaFormat(fourcc),
            width,
            height,
            surf_id,
            1,
            &attrib_list,
            1);
    VTRACE("%s createSurface DRM for vaformat %u, fourcc %s", __FUNCTION__, fourcc2VaFormat(fourcc), fourcc2str(fourcc));
    if (st != VA_STATUS_SUCCESS) {
        ETRACE("%s: vaCreateSurfaces returns %d", __PRETTY_FUNCTION__, st);
        return JD_RESOURCE_FAILURE;
    }
    return JD_SUCCESS;
}

JpegDecodeStatus JpegDecoder::createSurfaceGralloc(int width, int height, uint32_t fourcc, buffer_handle_t handle, int stride, VASurfaceID *surf_id)
{
    VAStatus st;
    VASurfaceAttrib                 attrib_list;
    VASurfaceAttribExternalBuffers  vaSurfaceExternBuf;
    memset(&vaSurfaceExternBuf, 0, sizeof (VASurfaceAttribExternalBuffers));
    vaSurfaceExternBuf.pixel_format = fourcc;
    vaSurfaceExternBuf.width        = width;
    vaSurfaceExternBuf.height       = height;
    vaSurfaceExternBuf.pitches[0]   = stride;
    vaSurfaceExternBuf.buffers      = (unsigned long*)&handle;
    vaSurfaceExternBuf.num_buffers  = 1;
    vaSurfaceExternBuf.flags        = VA_SURFACE_ATTRIB_MEM_TYPE_ANDROID_GRALLOC;
    attrib_list.type          = VASurfaceAttribExternalBufferDescriptor;
    attrib_list.flags         = VA_SURFACE_ATTRIB_SETTABLE;
    attrib_list.value.type    = VAGenericValueTypePointer;
    attrib_list.value.value.p = (void *)&vaSurfaceExternBuf;
    VTRACE("%s, vaformat=0x%x, width=%d, height=%d, attrib=", __FUNCTION__, fourcc2VaFormat(fourcc),
        width, height);
    VTRACE("            ext.pixel_format=0x%x", vaSurfaceExternBuf.pixel_format);
    VTRACE("            ext.width=%u", vaSurfaceExternBuf.width);
    VTRACE("            ext.height=%u", vaSurfaceExternBuf.height);
    VTRACE("            ext.data_size=%u", vaSurfaceExternBuf.data_size);
    VTRACE("            ext.num_planes=%u", vaSurfaceExternBuf.num_planes);
    VTRACE("            ext.pitches=%u,%u,%u,%u", vaSurfaceExternBuf.pitches[0],vaSurfaceExternBuf.pitches[1],vaSurfaceExternBuf.pitches[2],vaSurfaceExternBuf.pitches[3]);
    VTRACE("            ext.offsets=%u,%u,%u,%u", vaSurfaceExternBuf.offsets[0],vaSurfaceExternBuf.offsets[1],vaSurfaceExternBuf.offsets[2],vaSurfaceExternBuf.offsets[3]);
    VTRACE("            ext.buffers[0]=%lu", vaSurfaceExternBuf.buffers[0]);
    VTRACE("            ext.num_buffers=%u", vaSurfaceExternBuf.num_buffers);
    VTRACE("            ext.flags=%u", vaSurfaceExternBuf.flags);
    VTRACE("            attrib_list.type=%u", attrib_list.type);
    VTRACE("            attrib_list.flags=%u", attrib_list.flags);
    VTRACE("            attrib_list.type=%u", attrib_list.value.type);

    st = vaCreateSurfaces(mDisplay,
            fourcc2VaFormat(fourcc),
            width,
            height,
            surf_id,
            1,
            &attrib_list,
            1);
    VTRACE("%s createSurface GRALLOC for vaformat %u, fourcc %s", __FUNCTION__, fourcc2VaFormat(fourcc), fourcc2str(fourcc));
    if (st != VA_STATUS_SUCCESS) {
        ETRACE("%s: vaCreateSurfaces returns %d", __PRETTY_FUNCTION__, st);
        return JD_RESOURCE_FAILURE;
     }
    return JD_SUCCESS;
}


JpegDecodeStatus JpegDecoder::createSurfaceUserptr(int width, int height, uint32_t fourcc, uint8_t* ptr, VASurfaceID *surf_id)
{
    VAStatus st;
    VASurfaceAttrib                 attrib_list;
    VASurfaceAttribExternalBuffers  vaSurfaceExternBuf;
    memset(&vaSurfaceExternBuf, 0, sizeof (VASurfaceAttribExternalBuffers));
    vaSurfaceExternBuf.pixel_format = fourcc;
    vaSurfaceExternBuf.width        = width;
    vaSurfaceExternBuf.height       = height;
    vaSurfaceExternBuf.pitches[0]   = width;
    vaSurfaceExternBuf.offsets[0]   = 0;
    switch (fourcc) {
    case VA_FOURCC_NV12:
        vaSurfaceExternBuf.pitches[1]   = width;
        vaSurfaceExternBuf.pitches[2]   = 0;
        vaSurfaceExternBuf.pitches[3]   = 0;
        vaSurfaceExternBuf.offsets[1]   = width * height;
        vaSurfaceExternBuf.offsets[2]   = 0;
        vaSurfaceExternBuf.offsets[3]   = 0;
        break;
    case VA_FOURCC_YUY2:
    case VA_FOURCC_UYVY:
        vaSurfaceExternBuf.pitches[0]   = width * 2;
        vaSurfaceExternBuf.pitches[1]   = 0;
        vaSurfaceExternBuf.pitches[2]   = 0;
        vaSurfaceExternBuf.pitches[3]   = 0;
        vaSurfaceExternBuf.offsets[1]   = 0;
        vaSurfaceExternBuf.offsets[2]   = 0;
        vaSurfaceExternBuf.offsets[3]   = 0;
        break;
    case VA_FOURCC_YV12:
        vaSurfaceExternBuf.pitches[1]   = width / 2;
        vaSurfaceExternBuf.pitches[2]   = width / 2;
        vaSurfaceExternBuf.pitches[3]   = 0;
        vaSurfaceExternBuf.offsets[1]   = width * height;
        vaSurfaceExternBuf.offsets[2]   = width * height * 5 / 4;
        vaSurfaceExternBuf.offsets[3]   = 0;
        break;
    case VA_FOURCC_RGBA:
        vaSurfaceExternBuf.pitches[0]   = width * 4;
        vaSurfaceExternBuf.pitches[1]   = 0;
        vaSurfaceExternBuf.pitches[2]   = 0;
        vaSurfaceExternBuf.pitches[3]   = 0;
        vaSurfaceExternBuf.offsets[1]   = 0;
        vaSurfaceExternBuf.offsets[2]   = 0;
        vaSurfaceExternBuf.offsets[3]   = 0;
        break;
    case VA_FOURCC_411P:
        vaSurfaceExternBuf.pitches[1]   = width;
        vaSurfaceExternBuf.pitches[2]   = width;
        vaSurfaceExternBuf.pitches[3]   = 0;
        vaSurfaceExternBuf.offsets[1]   = width * height;
        vaSurfaceExternBuf.offsets[2]   = width * height * 2;
        vaSurfaceExternBuf.offsets[3]   = 0;
        break;
    case VA_FOURCC_411R:
        vaSurfaceExternBuf.pitches[1]   = width;
        vaSurfaceExternBuf.pitches[2]   = width;
        vaSurfaceExternBuf.pitches[3]   = 0;
        vaSurfaceExternBuf.offsets[1]   = width * height;
        vaSurfaceExternBuf.offsets[2]   = width * height * 5 / 4;
        vaSurfaceExternBuf.offsets[3]   = 0;
        break;
    case VA_FOURCC_IMC3:
        vaSurfaceExternBuf.pitches[1]   = width;
        vaSurfaceExternBuf.pitches[2]   = width;
        vaSurfaceExternBuf.pitches[3]   = 0;
        vaSurfaceExternBuf.offsets[1]   = width * height;
        vaSurfaceExternBuf.offsets[2]   = width * height * 3 / 2;
        vaSurfaceExternBuf.offsets[3]   = 0;
        break;
    case VA_FOURCC_422H:
        vaSurfaceExternBuf.pitches[1]   = width;
        vaSurfaceExternBuf.pitches[2]   = width;
        vaSurfaceExternBuf.pitches[3]   = 0;
        vaSurfaceExternBuf.offsets[1]   = width * height;
        vaSurfaceExternBuf.offsets[2]   = width * height;
        vaSurfaceExternBuf.offsets[3]   = 0;
        break;
    case VA_FOURCC_422V:
        vaSurfaceExternBuf.pitches[1]   = width;
        vaSurfaceExternBuf.pitches[2]   = width;
        vaSurfaceExternBuf.pitches[3]   = 0;
        vaSurfaceExternBuf.offsets[1]   = width * height;
        vaSurfaceExternBuf.offsets[2]   = width * height * 3 / 2;
        vaSurfaceExternBuf.offsets[3]   = 0;
        break;
    case VA_FOURCC_444P:
        vaSurfaceExternBuf.pitches[1]   = width;
        vaSurfaceExternBuf.pitches[2]   = width;
        vaSurfaceExternBuf.pitches[3]   = 0;
        vaSurfaceExternBuf.offsets[1]   = width * height;
        vaSurfaceExternBuf.offsets[2]   = width * height * 2;
        vaSurfaceExternBuf.offsets[3]   = 0;
        break;
    case VA_FOURCC('4','0','0','P'):
    default:
        vaSurfaceExternBuf.pitches[1]   = 0;
        vaSurfaceExternBuf.pitches[2]   = 0;
        vaSurfaceExternBuf.pitches[3]   = 0;
        vaSurfaceExternBuf.offsets[1]   = 0;
        vaSurfaceExternBuf.offsets[2]   = 0;
        vaSurfaceExternBuf.offsets[3]   = 0;
        break;
    }
    vaSurfaceExternBuf.buffers      = (unsigned long*)ptr;
    vaSurfaceExternBuf.num_buffers  = 1;
    vaSurfaceExternBuf.flags        = VA_SURFACE_ATTRIB_MEM_TYPE_ANDROID_GRALLOC;
    attrib_list.type          = VASurfaceAttribMemoryType;
    attrib_list.flags         = VA_SURFACE_ATTRIB_SETTABLE;
    attrib_list.value.type    = VAGenericValueTypeInteger;
    attrib_list.value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_USER_PTR;

    VTRACE("%s, vaformat=0x%x, width=%d, height=%d, attrib=", __FUNCTION__, fourcc2VaFormat(fourcc),
        width, height);
    VTRACE("            ext.pixel_format=0x%x", vaSurfaceExternBuf.pixel_format);
    VTRACE("            ext.width=%u", vaSurfaceExternBuf.width);
    VTRACE("            ext.height=%u", vaSurfaceExternBuf.height);
    VTRACE("            ext.data_size=%u", vaSurfaceExternBuf.data_size);
    VTRACE("            ext.num_planes=%u", vaSurfaceExternBuf.num_planes);
    VTRACE("            ext.pitches=%u,%u,%u,%u", vaSurfaceExternBuf.pitches[0],vaSurfaceExternBuf.pitches[1],vaSurfaceExternBuf.pitches[2],vaSurfaceExternBuf.pitches[3]);
    VTRACE("            ext.offsets=%u,%u,%u,%u", vaSurfaceExternBuf.offsets[0],vaSurfaceExternBuf.offsets[1],vaSurfaceExternBuf.offsets[2],vaSurfaceExternBuf.offsets[3]);
    VTRACE("            ext.buffers[0]=%lu", vaSurfaceExternBuf.buffers[0]);
    VTRACE("            ext.num_buffers=%u", vaSurfaceExternBuf.num_buffers);
    VTRACE("            ext.flags=%u", vaSurfaceExternBuf.flags);
    VTRACE("            attrib_list.type=%u", attrib_list.type);
    VTRACE("            attrib_list.flags=%u", attrib_list.flags);
    VTRACE("            attrib_list.type=%u", attrib_list.value.type);

    st = vaCreateSurfaces(mDisplay,
            fourcc2VaFormat(fourcc),
            width,
            height,
            surf_id,
            1,
            &attrib_list,
            1);
    VTRACE("%s createSurface GRALLOC for vaformat %u, fourcc %s", __FUNCTION__, fourcc2VaFormat(fourcc), fourcc2str(fourcc));
    if (st != VA_STATUS_SUCCESS) {
        ETRACE("%s: vaCreateSurfaces returns %d", __PRETTY_FUNCTION__, st);
        return JD_RESOURCE_FAILURE;
    }
    return JD_SUCCESS;

}



