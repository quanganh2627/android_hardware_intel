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

#include "JPEGCommon_Img.h"
#include "JPEGDecoder.h"

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
    case VA_FOURCC_YUY2:
        return HAL_PIXEL_FORMAT_YCbCr_422_I;
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
    case HAL_PIXEL_FORMAT_YCbCr_422_I:
        return VA_FOURCC_YUY2;
    case HAL_PIXEL_FORMAT_RGBA_8888:
        return VA_FOURCC_RGBA;
    default:
        return 0;
    }
}


bool JpegDecoder::jpegColorFormatSupported(JpegInfo &jpginfo) const
{
    return (jpginfo.image_color_fourcc == VA_FOURCC_IMC3) ||
        (jpginfo.image_color_fourcc == VA_FOURCC_422H) ||
        (jpginfo.image_color_fourcc == VA_FOURCC_444P);
}

JpegDecodeStatus JpegDecoder::createSurfaceDrm(int width, int height, uint32_t fourcc, unsigned long boname, int stride, VASurfaceID *surf_id)
{
    return JD_RENDER_TARGET_TYPE_UNSUPPORTED;
}

JpegDecodeStatus JpegDecoder::createSurfaceGralloc(int width, int height, uint32_t fourcc, buffer_handle_t handle, int stride, VASurfaceID *surf_id)
{
    VAStatus st;
    VASurfaceAttributeTPI attrib_tpi;
    uint32_t va_format = VA_RT_FORMAT_YUV444;
    attrib_tpi.count = 1;
    attrib_tpi.luma_stride = stride;
    attrib_tpi.pixel_format = VA_FOURCC_YV32;
    attrib_tpi.width = width;
    attrib_tpi.height = height;
    attrib_tpi.type = VAExternalMemoryAndroidGrallocBuffer;
    attrib_tpi.buffers = (unsigned long *)&handle;

    st = vaCreateSurfacesWithAttribute(
        mDisplay,
        width,
        height,
        va_format,
        1,
        surf_id,
        &attrib_tpi);
    if (st != VA_STATUS_SUCCESS)
        return JD_RESOURCE_FAILURE;
    return JD_SUCCESS;
}

JpegDecodeStatus JpegDecoder::createSurfaceUserptr(int width, int height, uint32_t fourcc, uint8_t* ptr, VASurfaceID *surf_id)
{
    return JD_INVALID_RENDER_TARGET;
}

