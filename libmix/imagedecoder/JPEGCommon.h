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

#ifndef JPEGCOMMON_H
#define JPEGCOMMON_H

#include <va/va.h>
#include <va/va_dec_jpeg.h>
#include <sys/types.h>
#include <string.h>
#include <utils/Vector.h>

using namespace android;

#define JPEG_MAX_COMPONENTS 4
#define JPEG_MAX_QUANT_TABLES 4

#define SURF_TILING_NONE    0
#define SURF_TILING_X       1
#define SURF_TILING_Y       2

extern uint32_t aligned_width(uint32_t width, int tiling);
extern uint32_t aligned_height(uint32_t height, int tiling);

struct RenderTarget {
    enum bufType{
        KERNEL_DRM,
        ANDROID_GRALLOC,
        INTERNAL_BUF,
        USER_PTR,
    };

    int width;
    int height;
    int stride;
    bufType type;
    int format;
    int pixel_format;
    int handle;
    VARectangle rect;
};

struct JpegInfo
{
    // in: use either buf+bufsize or inputs
    union {
        struct {
            uint8_t *buf;
            uint32_t bufsize;
        };
        android::Vector<uint8_t> *inputs;
    };
    bool use_vector_input;
    bool need_header_only;
    // internal use
    uint32_t component_order;
    uint32_t dqt_ind;
    uint32_t dht_ind;
    uint32_t scan_ind;
    bool frame_marker_found;
    bool soi_parsed;
    bool sof_parsed;
    bool dqt_parsed;
    bool dht_parsed;
    bool sos_parsed;
    bool dri_parsed;
    bool jfif_parsed;
    bool adobe_parsed;
    uint8_t adobe_transform;
    // out
    uint32_t image_width;
    uint32_t image_height;
    uint32_t image_color_fourcc;
    VAPictureParameterBufferJPEGBaseline picture_param_buf;
    VASliceParameterBufferJPEGBaseline slice_param_buf[JPEG_MAX_COMPONENTS];
    VAIQMatrixBufferJPEGBaseline qmatrix_buf;
    VAHuffmanTableBufferJPEGBaseline hufman_table_buf;
    uint32_t dht_byte_offset[4];
    uint32_t dqt_byte_offset[4];
    uint32_t huffman_tables_num;
    uint32_t quant_tables_num;
    uint32_t soi_offset;
    uint32_t eoi_offset;
    uint32_t scan_ctrl_count;
};

enum JpegDecodeStatus
{
    JD_SUCCESS                          =   0,
    JD_UNINITIALIZED                    =   1,
    JD_ALREADY_INITIALIZED              =   2,
    JD_RENDER_TARGET_TYPE_UNSUPPORTED   =   3,
    JD_INPUT_FORMAT_UNSUPPORTED         =   4,
    JD_OUTPUT_FORMAT_UNSUPPORTED        =   5,
    JD_INVALID_RENDER_TARGET            =   6,
    JD_RENDER_TARGET_NOT_INITIALIZED    =   7,
    JD_CODEC_UNSUPPORTED                =   8,
    JD_INITIALIZATION_ERROR             =   9,
    JD_RESOURCE_FAILURE                 =  10,
    JD_DECODE_FAILURE                   =  11,
    JD_BLIT_FAILURE                     =  12,
    JD_ERROR_BITSTREAM                  =  13,
    JD_RENDER_TARGET_BUSY               =  14,
    JD_IMAGE_TOO_SMALL                  =  15,
    JD_IMAGE_TOO_BIG                    =  16,
    JD_INSUFFICIENT_BYTE                =  17,
    JD_UNIMPLEMENTED                    =  18,
    JD_COLORSPACE_UNSUPPORTED			=  19,
    JD_COMPONENT_NUM_UNSUPPORTED		=  20,
    JD_HUFFTABLE_NUM_UNSUPPORTED		=  21,
    JD_QUANTTABLE_NUM_UNSUPPORTED		=  22,
};

inline char * fourcc2str(uint32_t fourcc, char * str = NULL)
{
    static char tmp[5];
    if (str == NULL) {
        str = tmp;
        memset(str, 0, sizeof str);
    }
    str[0] = fourcc & 0xff;
    str[1] = (fourcc >> 8 )& 0xff;
    str[2] = (fourcc >> 16) & 0xff;
    str[3] = (fourcc >> 24)& 0xff;
    str[4] = '\0';
    return str;
}

inline int fourcc2VaFormat(uint32_t fourcc)
{
    switch(fourcc) {
    case VA_FOURCC_422H:
    case VA_FOURCC_422V:
    case VA_FOURCC_YUY2:
    case VA_FOURCC_UYVY:
        return VA_RT_FORMAT_YUV422;
    case VA_FOURCC_IMC3:
    case VA_FOURCC_YV12:
    case VA_FOURCC_NV12:
        return VA_RT_FORMAT_YUV420;
    case VA_FOURCC_444P:
        return VA_RT_FORMAT_YUV444;
    case VA_FOURCC_411P:
    case VA_FOURCC_411R:
        return VA_RT_FORMAT_YUV411;
    case VA_FOURCC('4','0','0','P'):
        return VA_RT_FORMAT_YUV400;
    case VA_FOURCC_BGRA:
    case VA_FOURCC_ARGB:
    case VA_FOURCC_RGBA:
        return VA_RT_FORMAT_RGB32;
    default:
        // Add if needed
        return -1;
    }
}

inline uint32_t sampFactor2Fourcc(int h1, int h2, int h3, int v1, int v2, int v3)
{
    if (h1 == 2 && h2 == 1 && h3 == 1 &&
            v1 == 2 && v2 == 1 && v3 == 1) {
        return VA_FOURCC_IMC3;
    }
    else if (h1 == 2 && h2 == 1 && h3 == 1 &&
            v1 == 1 && v2 == 1 && v3 == 1) {
        return VA_FOURCC_422H;
    }
    else if (h1 == 1 && h2 == 1 && h3 == 1 &&
            v1 == 1 && v2 == 1 && v3 == 1) {
        return VA_FOURCC_444P;
    }
    else if (h1 == 4 && h2 == 1 && h3 == 1 &&
            v1 == 1 && v2 == 1 && v3 == 1) {
        return VA_FOURCC_411P;
    }
    else if (h1 == 1 && h2 == 1 && h3 == 1 &&
            v1 == 4 && v2 == 1 && v3 == 1) {
        return VA_FOURCC_411R;
    }
    else if (h1 == 1 && h2 == 1 && h3 == 1 &&
            v1 == 2 && v2 == 1 && v3 == 1) {
        return VA_FOURCC_422V;
    }
    else if (h1 == 2 && h2 == 1 && h3 == 1 &&
            v1 == 2 && v2 == 2 && v3 == 2) {
        return VA_FOURCC_422H;
    }
    else if (h2 == 2 && h2 == 2 && h3 == 2 &&
            v1 == 2 && v2 == 1 && v3 == 1) {
        return VA_FOURCC_422V;
    }
    else
    {
        return VA_FOURCC('4','0','0','P');
    }
}

inline int fourcc2LumaBitsPerPixel(uint32_t fourcc)
{
    switch(fourcc) {
    case VA_FOURCC_422H:
    case VA_FOURCC_422V:
    case VA_FOURCC_IMC3:
    case VA_FOURCC_YV12:
    case VA_FOURCC_NV12:
    case VA_FOURCC_444P:
    case VA_FOURCC_411P:
    case VA_FOURCC_411R:
    case VA_FOURCC('4','0','0','P'):
        return 1;
    case VA_FOURCC_YUY2:
    case VA_FOURCC_UYVY:
        return 2;
    case VA_FOURCC_BGRA:
    case VA_FOURCC_ARGB:
    case VA_FOURCC_RGBA:
        return 4;
    default:
        // Add if needed
        return 1;
    }
}

// Platform dependent
extern int fourcc2PixelFormat(uint32_t fourcc);
extern uint32_t pixelFormat2Fourcc(int pixel_format);

#endif
