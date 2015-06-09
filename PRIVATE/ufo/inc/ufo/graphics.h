/*
 * INTEL CONFIDENTIAL
 *
 * Copyright 2013-2014 Intel Corporation All Rights Reserved.
 *
 * The source code contained or described herein and all documents related to the
 * source code ("Material") are owned by Intel Corporation or its suppliers or
 * licensors. Title to the Material remains with Intel Corporation or its suppliers
 * and licensors. The Material contains trade secrets and proprietary and confidential
 * information of Intel or its suppliers and licensors. The Material is protected by
 * worldwide copyright and trade secret laws and treaty provisions. No part of the
 * Material may be used, copied, reproduced, modified, published, uploaded, posted,
 * transmitted, distributed, or disclosed in any way without Intels prior express
 * written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel
 * or otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 *
 */

/**
 * This <ufo/graphics.h> file contains UFO specific pixel formats.
 *
 * \remark This file is Android specific
 * \remark Do not put any internal definitions here!
 */

#ifndef INTEL_UFO_GRAPHICS_H
#define INTEL_UFO_GRAPHICS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * UFO specific pixel format definitions
 *
 * Range 0x100 - 0x1FF is reserved for pixel formats specific to the HAL implementation
 *
 * \see #include <system/graphics.h>
 */
enum {

#if UFO_GRALLOC_ENABLE_NV12_GENERIC_FORMAT
    /**
     * NV12 format:
     *
     * YUV 4:2:0 image with a plane of 8 bit Y samples followed by an interleaved
     * U/V plane containing 8 bit 2x2 subsampled colour difference samples.
     *
     * Microsoft defines this format as follows:
     *
     * A format in which all Y samples are found first in memory as an array
     * of unsigned char with an even number of lines (possibly with a larger
     * stride for memory alignment). This is followed immediately by an array
     * of unsigned char containing interleaved Cb and Cr samples. If these
     * samples are addressed as a little-endian WORD type, Cb would be in the
     * least significant bits and Cr would be in the most significant bits
     * with the same total stride as the Y samples.
     *
     * NV12 is the preferred 4:2:0 pixel format. 
     *
     * Layout information:
     * - Y plane with even height and width
     * - U/V are interlaved and 1/2 width and 1/2 height
     *
     *       ____________w___________ .....
     *      |Y0|Y1                   |    :
     *      |                        |    :
     *      h                        h    :
     *      |                        |    :
     *      |                        |    :
     *      |________________________|....:    
     *      |U|V|U|V                 |    :
     *     h/2                      h/2   :
     *      |____________w___________|....:
     *
     */
    HAL_PIXEL_FORMAT_NV12_GENERIC_INTEL = 0x1FF,
    HAL_PIXEL_FORMAT_NV12_GENERIC_FOURCC = 0x3231564E,
#endif

    /**
     * Intel NV12 format Y tiled.
     *
     * \see HAL_PIXEL_FORMAT_NV12_GENERIC_FOURCC
     *
     * Additional layout information:
     * - stride: aligned to 128 bytes
     * - height: aligned to 64 lines
     * - tiling: always Y tiled
     *
     *       ____________w___________ ____
     *      |Y0|Y1                   |    |
     *      |                        |    |
     *      h                        h    h'= align(h,64)
     *      |                        |    |
     *      |                        |    |
     *      |____________w___________|    |
     *      :                             |
     *      :________________________ ____|     
     *      |U|V|U|V                 |    :
     *     h/2                      h/2   :
     *      |____________w___________|    h"= h'/2
     *      :.............................:
     *
     *      stride = align(w,128)
     *
     */
    HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL = 0x100,

    /**
     * Intel NV12 format linear.
     *
     * \see HAL_PIXEL_FORMAT_NV12_GENERIC_FOURCC
     *
     * Additional layout information:
     * - stride: aligned to 512, 1024, 1280, 2048, 4096
     * - height: aligned to 32 lines
     * - tiling: always linear
     *
     *       ____________w___________ ____
     *      |Y0|Y1                   |    |
     *      |                        |    |
     *      h                        h    h'= align(h,32)
     *      |                        |    |
     *      |                        |    |
     *      |____________w___________|    |
     *      :                             |
     *      :________________________ ____|     
     *      |U|V|U|V                 |    :
     *     h/2                      h/2   :
     *      |____________w___________|    h"= h'/2
     *      :.............................:
     *
     *      stride = align(w,512..1024..1280..2048..4096)
     *
     */
    HAL_PIXEL_FORMAT_NV12_LINEAR_INTEL = 0x101,

    /**
     * Layout information:
     * - U/V are 1/2 width and full height
     * - stride: aligned to 128
     * - height: aligned to 64
     * - tiling: always Y tiled
     *
     *       ____________w___________ ____
     *      |Y                       |    |
     *      |                        |    |
     *      h                        h    h' = align(h,64)
     *      |____________w___________|    |
     *      :                             |
     *      :_____________________________|     
     *      |V           |                :
     *      |            |                :
     *      h            h                h' = align(h,64)
     *      |_____w/2____|                :
     *      :____________ ................:
     *      |U           |                :
     *      |            |                :
     *      h            h                h' = align(h,64)
     *      |_____w/2____|                :
     *      :.............................:
     *
     *      stride = align(w,128)
     *
     */
    HAL_PIXEL_FORMAT_YCrCb_422_H_INTEL = 0x102, // YV16

    /**
     * Intel NV12 format packed linear.
     *
     * \see HAL_PIXEL_FORMAT_NV12_GENERIC_FOURCC
     *
     * Additional layout information:
     * - stride: same as width
     * - height: no alignment
     * - tiling: always linear
     *
     *       ____________w___________ 
     *      |Y0|Y1                   |
     *      |                        |
     *      h                        h
     *      |                        |
     *      |                        |
     *      |____________w___________|
     *      |U|V|U|V                 |
     *     h/2                      h/2
     *      |____________w___________|
     *
     */
    HAL_PIXEL_FORMAT_NV12_LINEAR_PACKED_INTEL = 0x103,

    /**
     * Three planes, 8 bit Y plane followed by 8 bit 2x1 subsampled U and V planes.
     * Similar to IMC3 but U/V are full height.
     * The width must be even.
     * There are no specific restrictions on pitch, height and alignment.
     * It can be linear or tiled if required.
     * Horizontal stride is aligned to 128.
     * Vertical stride is aligned to 64.
     *
     *       ________________________ .....
     *      |Y0|Y1                   |    :
     *      |                        |    :
     *      h                        h    h'= align(h,64)
     *      |____________w___________|    :
     *      :____________ ________________:
     *      |U0|U1       |                :
     *      |            |                :
     *     h|            |                h'= align(h,64)
     *      |_____w/2____|                :
     *      :____________ ................:
     *      |V0|V1       |                :
     *      |            |                :
     *     h|            |                h'= align(h,64)
     *      |______w/2___|                :
     *      :.............................:
     *
     *      stride = align(w,128)
     */
    HAL_PIXEL_FORMAT_YCbCr_422_H_INTEL = 0x104, // YU16
    
    /**
     * Intel NV12 format X tiled.
     * This is VXD specific format.
     *
     * \see HAL_PIXEL_FORMAT_NV12_GENERIC_FOURCC
     *
     * Additional layout information:
     * - stride: aligned to 512, 1024, 2048, 4096
     * - height: aligned to 32 lines
     * - tiling: always X tiled
     *
     *       ____________w___________ ____
     *      |Y0|Y1                   |    |
     *      |                        |    |
     *      h                        h    h'= align(h,32)
     *      |                        |    |
     *      |                        |    |
     *      |____________w___________|    |
     *      :                             |
     *      :________________________ ____|     
     *      |U|V|U|V                 |    :
     *     h/2                      h/2   :
     *      |____________w___________|    h"= h'/2
     *      :.............................:
     *
     *      stride = align(w,512..1024..2048..4096)
     *
     */
    HAL_PIXEL_FORMAT_NV12_X_TILED_INTEL = 0x105,

    /**
     * Legacy RGB formats.
     * These formats were removed from Android framework code,
     * but they are still required by EGL functionality (EGL image).
     * Re-introduced here to support conformance EGL tests.
     */
    HAL_PIXEL_FORMAT_RGBA_5551_INTEL = 0x106,
    HAL_PIXEL_FORMAT_RGBA_4444_INTEL = 0x107,

    /**
     * Only single 8 bit Y plane. 
     * Horizontal stride is aligned to 128.
     * Vertical stride is aligned to 64.
     * Tiling is Y-tiled.
     *       ________________________ .....
     *      |Y0|Y1|                  |    :
     *      |                        |    :
     *      h                        h    h'= align(h,64)
     *      |____________w___________|    :
     *      :                             :
     *      :............stride...........:
     *
     *      stride = align(w,128)
     */
    HAL_PIXEL_FORMAT_GENERIC_8BIT_INTEL = 0x108,

    /**
     * Three planes, 8 bit Y plane followed by U, V plane with 1/4 width and full height.
     * The U and V planes have the same stride as the Y plane.
     * An width is multiple of 4 pixels.
     * Horizontal stride is aligned to 128.
     * Vertical stride is aligned to 64.
     *       ________________________ .....
     *      |Y0|Y1                   |    :
     *      |                        |    :
     *      h                        |    h'= align(h,64)
     *      |____________w___________|    :
     *      :_______ .....................:
     *      |U|U    |                     :    
     *      |       |                     :
     *      h       |                     h'= align(h,64)
     *      |__w/4__|                     :
     *      :_______ .....................:
     *      |V|V    |                     :
     *      |       |                     :
     *      h       |                     h'= align(h,64)
     *      |__w/4__|                     :
     *      :............stride...........:
     *
     *      stride = align(w,128)
     */
    HAL_PIXEL_FORMAT_YCbCr_411_INTEL    = 0x109,

    /**
     * Three planes, 8 bit Y plane followed by U, V plane with 1/2 width and 1/2 height.
     * The U and V planes have the same stride as the Y plane.
     * Width and height must be even.
     * Horizontal stride is aligned to 128.
     * Vertical stride is aligned to 64.
     *       ________________________ .....
     *      |Y0|Y1                   |    :
     *      |                        |    :
     *      h                        h    h'= align(h,64)
     *      |____________w___________|    :
     *      :                             :
     *      :____________ ................:
     *      |U0|U1       |                :
     *   h/2|_______w/2__|                h"= h'/2
     *      :____________ ................:
     *      |V0|V1       |                :
     *   h/2|_______w/2__|                h"= h'/2
     *      :.................stride......:
     *
     *      stride = align(w,128)
     */
    HAL_PIXEL_FORMAT_YCbCr_420_H_INTEL  = 0x10A,

    /**
     * Three planes, 8 bit Y plane followed by U, V plane with full width and 1/2 height.
     * Horizontal stride is aligned to 128.
     * Vertical stride is aligned to 64.
     *       ________________________ .....
     *      |Y0|Y1                   |    :
     *      |                        |    :
     *      h                        h    h'= align(h,64)
     *      |____________w___________|    :
     *      :                             :
     *      :________________________ ....:
     *      |U0|U1                   |    :
     *   h/2|____________w___________|    h"= h'/2
     *      :________________________ ....:
     *      |V0|V1                   |    :
     *   h/2|____________w___________|    h"= h'/2
     *      :.............................:
     *
     *      stride = align(w,128)
     */
    HAL_PIXEL_FORMAT_YCbCr_422_V_INTEL  = 0x10B,

    /**
     * Three planes, 8 bit Y plane followed by U, V plane with full width and full height.
     * Horizontal stride is aligned to 128.
     * Vertical stride is aligned to 64.
     *       ________________________ .....
     *      |Y0|Y1                   |    :
     *      |                        |    :
     *      h                        h    h'= align(h,64)
     *      |____________w___________|    :
     *      :________________________ ....:
     *      |U0|U1                   |    :
     *      |                        |    :
     *      h                        h    h'= align(h,64)
     *      |____________w___________|    :
     *      :________________________ ....:
     *      |V0|V1                   |    :
     *      |                        |    :
     *      h                        h    h'= align(h,64)
     *      |____________w___________|    :
     *      :.............................:
     *
     *      stride = align(w,128)
     */   
    HAL_PIXEL_FORMAT_YCbCr_444_INTEL    = 0x10C,

    /**
     * R/G/B components in separate planes
     * - all planes have full width and full height
     * - horizontal stride aligned to 128
     * - vertical stride same as height
     *
     *       ________________________ .....
     *      |R                       |    :
     *      |                        |    :
     *      h                        h    h'= align(h,64)
     *      |____________w___________|    :
     *      :________________________ ....:
     *      |G                       |    :
     *      |                        |    :
     *      h                        h    h'= align(h,64)
     *      |____________w___________|    :
     *      :________________________ ....:
     *      |B                       |    :
     *      |                        |    :
     *      h                        h    h'= align(h,64)
     *      |____________w___________|    :
     *      :.............................:
     *
     *      stride = align(w,128)
     */   
    HAL_PIXEL_FORMAT_RGBP_INTEL         = 0x10D,

    /**
     * B/G/R components in separate planes
     * - all planes have full width and full height
     * - horizontal stride aligned to 128
     * - vertical stride same as height
     *
     * \see HAL_PIXEL_FORMAT_RGBP_INTEL
     *
     *       ____________w___________ .....
     *      |B                       |    :
     *      |                        |    :
     *      h                        h    h'= align(h,64)
     *      |____________w___________|    :
     *      :________________________ ....:
     *      |G                       |    :
     *      |                        |    :
     *      h                        h    h'= align(h,64)
     *      |____________w___________|    :
     *      :________________________ ....:
     *      |R                       |    :
     *      |                        |    :
     *      h                        h    h'= align(h,64)
     *      |____________w___________|    :
     *      :.............................:
     *
     *      stride = align(w,128)
     */   
    HAL_PIXEL_FORMAT_BGRP_INTEL         = 0x10E,

    /**
     * Intel NV12 format for camera.
     *
     * \see HAL_PIXEL_FORMAT_NV12_GENERIC_FOURCC
     *
     * Additional layout information:
     * - height: must be even
     * - stride: aligned to 64
     * - vstride: same as height
     * - tiling: always linear
     *
     *       ________________________ .....
     *      |Y0|Y1                   |    :
     *      |                        |    :
     *      h                        h    h
     *      |                        |    :
     *      |                        |    :
     *      |____________w___________|....:
     *      |U|V|U|V                 |    :
     *     h/2                      h/2  h/2
     *      |____________w___________|....:
     *
     *      stride = align(w,64)
     *      vstride = h
     *
     */
    HAL_PIXEL_FORMAT_NV12_LINEAR_CAMERA_INTEL = 0x10F,

    /**
     * Intel P010 format.
     *
     * Used for 10bit usage for HEVC/VP9 decoding and video processing.
     *
     * Layout information:
     * - each pixel being represented by 16 bits (2 bytes)
     * - Y plane with even height and width
     * - hstride multiple of 64 pixels (128 bytes)
     * - hstride is specified in pixels, not in bytes
     * - vstride is aligned to 64 lines
     * - U/V are interleaved and 1/2 width and 1/2 height
     * - memory is Y tiled
     *
     *       ____________w___________ ____
     *      |Y0|Y1                   |    |
     *      |                        |    |
     *      h                        h    h'= align(h,64)
     *      |                        |    |
     *      |                        |    |
     *      |____________w___________|    |
     *      :                             |
     *      :________________________ ____|     
     *      |U|V|U|V                 |    :
     *     h/2                      h/2   :
     *      |____________w___________|    h"= h'/2
     *      :.............................:
     *
     *      pitch (in bytes) = align(w*2,128)
     *      size (in bytes) = pitch*3/2
     */
    HAL_PIXEL_FORMAT_P010_INTEL = 0x110,

    /**
     * \deprecated alias name
     * \see HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL
     */
    HAL_PIXEL_FORMAT_NV12_TILED_INTEL   = HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL,
    HAL_PIXEL_FORMAT_NV12_INTEL         = HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL,
    HAL_PIXEL_FORMAT_INTEL_NV12         = HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL,

    /**
     * \note THIS WILL BE GOING AWAY!
     *
     * \deprecated value out of range of reserved pixel formats
     * \see #include <openmax/OMX_IVCommon.h>
     * \see OMX_INTEL_COLOR_FormatYUV420PackedSemiPlanar
     * \see HAL_PIXEL_FORMAT_NV12_LINEAR_INTEL
     */
    HAL_PIXEL_FORMAT_YUV420PackedSemiPlanar_INTEL = 0x7FA00E00,

    /**
     * \note THIS WILL BE GOING AWAY!
     *
     * \deprecated value out of range of reserved pixel formats
     * \see #include <openmax/OMX_IVCommon.h>
     * \see OMX_INTEL_COLOR_FormatYUV420PackedSemiPlanar
     * \see HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL
     */
    HAL_PIXEL_FORMAT_YUV420PackedSemiPlanar_Tiled_INTEL = 0x7FA00F00,
};

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* INTEL_UFO_GRAPHICS_H */
