/*===================== begin_copyright_notice ==================================

  INTEL CONFIDENTIAL
  Copyright 2014
  Intel Corporation All Rights Reserved.

  The source code contained or described herein and all documents related to the
  source code ("Material") are owned by Intel Corporation or its suppliers or
  licensors. Title to the Material remains with Intel Corporation or its suppliers
  and licensors. The Material contains trade secrets and proprietary and confidential
  information of Intel or its suppliers and licensors. The Material is protected by
  worldwide copyright and trade secret laws and treaty provisions. No part of the
  Material may be used, copied, reproduced, modified, published, uploaded, posted,
  transmitted, distributed, or disclosed in any way without Intels prior express
  written permission.

  No license under any patent, copyright, trade secret or other intellectual
  property right is granted to or conferred upon you by disclosure or delivery
  of the Materials, either expressly, by implication, inducement, estoppel
  or otherwise. Any license under such intellectual property rights must be
  express and approved by Intel in writing.

  File Name: iVP.h

Abstract:
encapsulate the VP interfaces and make the low level API transparent.

Environment:
Android

Notes:


======================= end_copyright_notice ==================================*/


#ifndef __IVP_INTEL_H__
#define __IVP_INTEL_H__

__BEGIN_DECLS

#include <system/window.h>
#include <stdbool.h>

#define MAX_SUB_PICTURE_LAYER           16      // Maximum subpictures supported for composition
#define CODECHAL_ENCRYPTION_UNSUPPORTED 0      // Codec encryption unsupported flag is 0.
#define CODECHAL_ENCRYPTION_SUPPORTED   1      // Codec encryption supported flag is 1.

#define CODECHAL_ENCRYPTION_NONE    0          // None encryption in codec flag is 0.
#define CODECHAL_ENCRYPTION_CASCADE 1          // Cascade encryption in codec flag is 1.
#define CODECHAL_ENCRYPTION_PAVP    2          // PAVP encryption in codec flag is 2.

/*
  Advance filter bit masks
  Use these masks to set filter bits in iVP_layer_t.VPfilters
  Ex. to enable DN filter.
    iVP_layer_t.VPfilters       |= FILTER_DENOISE;      // turn on the filter
    iVP_layer_t.fDenoiseFactor   = 1.0;                 // set DN parameter
 */
#define FILTER_DENOISE      0x0001             // Denoise filter bit mask is 1
#define FILTER_DEINTERLACE  0x0002             // Deinterlace filter bit masks is 2.
#define FILTER_SHARPNESS    0x0004             // Sharpness filter bit masks is 4.
#define FILTER_COLORBALANCE 0x0008             // Colorbalance filter bit masks is 8.

#define FILTER_COLORBALANCE_PARAM_SIZE      4  // Colorbalance filter parameters size is 4.

#define IVP_SUPPORTS_LEVEL_EXPANSION        1  // flag to indicate whether level expansion is supported

// width and height could be any value, it is just used by VAAPI.
#define IVP_DEFAULT_WIDTH    1280
#define IVP_DEFAULT_HEIGHT   720

// the flag is defined to just sync with mainline
#define IVP_DEFAULT_CAPABLILITY 0   // All supported VEBOX feature

// Available encryption types
typedef enum
{
    PAVP_ENCRYPTION_NONE            = 1,       // none encryption
    PAVP_ENCRYPTION_AES128_CTR      = 2,       // AES128_CTR algorithm
    PAVP_ENCRYPTION_AES128_CBC      = 4,       // AES128_CBC algorithm
    PAVP_ENCRYPTION_AES128_ECB      = 8,       // AES128_ECB algorithm
    PAVP_ENCRYPTION_DECE_AES128_CTR = 16       // DECE_AES128_CTR algorithm
} PAVP_ENCRYPTION_TYPE;

// Available counter types
typedef enum
{
    PAVP_COUNTER_TYPE_A = 1,                  // 32-bit counter, 96 zeroes (CTG, ELK, ILK, SNB)
    PAVP_COUNTER_TYPE_B = 2,                  // 64-bit counter, 32-bit Nonce, 32-bit zero (SNB)
    PAVP_COUNTER_TYPE_C = 4                   // Standard AES counter
} PAVP_COUNTER_TYPE;

//Status Code for iVP
typedef enum _IVP_STATUS
{
    IVP_STATUS_ERROR   = -1,                  // error result
    IVP_STATUS_SUCCESS = 0x00000000,          // successful result
    IVP_ERROR_OUT_OF_MEMORY,                  // out of memory
    IVP_ERROR_INVALID_CONTEXT,                // invalid context content
    IVP_ERROR_INVALID_OPERATION,              // invalid operation
    IVP_ERROR_INVALID_PARAMETER,              // invalid parameters
}iVP_status;

//Avaiable iVP buffer types
typedef enum _IVP_BUFFER_TYPE
{
    IVP_INVALID_HANDLE,                       // invalid buffer handle
    IVP_GRALLOC_HANDLE,                       // gralloc buffer handle
    IVP_DRM_FLINK,                            // drm buffer flink
}iVP_buffer_type;

//iVP rectangle definition
typedef struct _IVP_RECT
{
    int left;                                 // X value of upper-left point
    int top;                                  // Y value of upper-left point
    int width;                                // width of the rectangle
    int height;                               // height of the rectangle
} iVP_rect_t;

//Avaiable rotation types
typedef enum
{
    IVP_ROTATE_NONE = 0,                      // VA_ROTATION_NONE which is defined in va.h
    IVP_ROTATE_90   = 1,                      // VA_ROTATION_90 which is defined in va.h
    IVP_ROTATE_180  = 2,                      // VA_ROTATION_180 which is defined in va.h
    IVP_ROTATE_270  = 3,                      // VA_ROTATION_270 which is defined in va.h
} iVP_rotation_t;

//Avaiable flip types
typedef enum
{
    IVP_FLIP_NONE = 0,                        // VA_MIRROR_NONE which is defined in va.h
    IVP_FLIP_H    = 1,                        // VA_MIRROR_HORIZONTAL which is defined in va.h
    IVP_FLIP_V    = 2,                        // VA_MIRROR_VERTICAL which is defined in va.h
} iVP_flip_t;

//Avaiable filter types
typedef enum
{
    IVP_FILTER_HQ   = 0,                      // high-quality filter (AVS scaling)
    IVP_FILTER_FAST = 1,                      // fast filter (bilinear scaling)
} iVP_filter_t;

// Avaiable blending types
typedef enum
{
    /// No blending
    IVP_BLEND_NONE = 0,

    /// There is blending, the source pixel is assumed to be
    /// alpha-premultiplied and the effective alpha is "Sa Pa" (per-pixel
    /// alpha times the per-plane alpha). The implemented equation is:
    ///
    /// Drgba' = Srgba Pa + Drgba (1 - Sa Pa)
    ///
    /// Where
    ///     Drgba' is the result of the blending,
    ///     Srgba is the premultiplied source colour,
    ///     Pa is a per-plane defined alpha (the "alpha" field in iVP_layer_t),
    ///     Drgba is the framebuffer content prior to the blending,
    ///     Sa is the alpha component from the Srgba vector.
    ///
    /// Note that Pa gets out of the equation when its value is 1.0.
    IVP_ALPHA_SOURCE_PREMULTIPLIED_TIMES_PLANE,

    /// There is blending, the source pixel is assumed to be premultiplied
    /// and the effective alpha is "Sa" (per-pixel alpha). The implemented
    /// equation is:
    ///
    /// Drgba' = Srgba + Drgba (1 - Sa)
    ///
    /// Where
    ///     Drgba' is the result of the blending,
    ///     Srgba is the premultiplied source colour,
    ///     Drgba is the framebuffer content prior to the blending,
    ///     Sa is the alpha component from the Srgba vector.
    IVP_ALPHA_SOURCE_PREMULTIPLIED,

    /// There is blending, the source pixel is assumed to be
    /// non alpha-premultiplied and the effective alpha is "Pa" (per-plane
    /// alpha). The implemented equation is:
    ///
    /// Drgba' = Srgba Pa + Drgba (1 - Pa)
    ///
    /// Where
    ///     Drgba' is the result of the blending,
    ///     Srgba is the non-premultiplied source colour,
    ///     Pa is a per-plane defined alpha (the "alpha" field in iVP_layer_t),
    ///     Drgba is the framebuffer content prior to the blending.
    ///
    /// Note that the per-pixel alpha is ignored, up to the point that
    /// the source alpha is not even implicitly present premultipliying
    /// the source colour.
    IVP_ALPHA_SOURCE_CONSTANT,

} iVP_blend_t;

// Avaiable deinterlace type
typedef enum
{
    IVP_DEINTERLACE_BOB = 0,                   // BOB DI
    IVP_DEINTERLACE_ADI = 1,                   // ADI
} iVP_deinterlace_t;

// Avaiable sample types
typedef enum
{
    IVP_SAMPLETYPE_PROGRESSIVE = 0,           // progressive sample
    IVP_SAMPLETYPE_TOPFIELD,                  // top filed sample
    IVP_SAMPLETYPE_BOTTOMFIELD                // bottom filed sample
} iVP_sample_type_t;

// Available color range types
typedef enum
{
    IVP_COLOR_RANGE_NONE = 0,                 // default value
    IVP_COLOR_RANGE_PARTIAL,                  // all set partial range
    IVP_COLOR_RANGE_FULL                      // all set full range
} iVP_color_range_t;

// layer information pass to iVP
typedef struct _IVP_LAYER_T
{
    union
    {
        buffer_handle_t     gralloc_handle;            // buffer is allocated from gralloc
        int                 gem_handle;                // buffer is allocated from drm directly
    };

    iVP_buffer_type         bufferType;                // input buffer type
    iVP_rect_t              *srcRect;                  // source rectangle
    iVP_rect_t              *destRect;                 // dest rectangle
    iVP_rotation_t          rotation;                  // rotation info
    iVP_flip_t              flip;                      // flip info
    iVP_filter_t            filter;                    // filtering quality
    iVP_color_range_t       colorRange;                // color range
    iVP_blend_t             blend;                     // blending mode
    union {
        float               alpha;                     // Plane alpha
        float               blend_global_alpha;        // Deprecated name for the alpha field
    };
    float                   blend_min_luma;            // Minimum luma value
    float                   blend_max_luma;            // Maximum luma value

    unsigned int            app_ID;                    // app ID for PAVP
    bool                    encrypt;                   // specify whether the surface encrypted or not

    iVP_sample_type_t       sample_type;               // BOB DI

    long long int           VPfilters;                 // VP filter

    float                   fDenoiseFactor;            // DN VP filter parameter

    iVP_deinterlace_t       iDeinterlaceMode;          // DI VP filter parameter

    float                   fSharpnessFactor;          // sharpness VP filter parameter

    float                   fColorBalanceHue;          // ColorBalance Hue value
    float                   fColorBalanceSaturation;   // ColorBalance saturation value
    float                   fColorBalanceBrightness;   // ColorBalance brightness value
    float                   fColorBalanceContrast;     // ColorBalance Contrast value

}iVP_layer_t;

typedef unsigned int iVPCtxID;                        //Context of the iVP


//*****************************************************************************
//
// iVP_create_context
//  @param: ctx[OUT]  iVP Context ID
//          width     should be any non-zero value
//          height    should be any non-zero value
//
//*****************************************************************************
iVP_status iVP_create_context(iVPCtxID *ctx, unsigned int width, unsigned int height, unsigned int vpCapabilityFlag = 0);

//*****************************************************************************
//
//  @Function iVP_exec
//                  Supports CSC/Scaling/Composition/AlphaBlending/Sharpnes/procamp
//  @param:
//       ctx:       iVP Context ID
//       primary:   primary surface for VPP
//       subSurfs:  sub surfaces for composition [optional for CSC/Scaling]
//       numOfSubs: number of sub surfaces
//       outSurf:   output buffer for VP
//       syncFlag:  determine whether need to do vaSyncSurface
//*****************************************************************************
iVP_status iVP_exec(iVPCtxID     *ctx,
        iVP_layer_t        *primarySurf,
        iVP_layer_t        *subSurfs,
        unsigned int       numOfSubs,
        iVP_layer_t        *outSurf,
        bool               syncFlag
        );
//*****************************************************************************
//
//  @Function iVP_exec_multiOut
//                  Support multiple output, but just support widi dual output now
//  @param:
//       ctx:       iVP Context ID
//       primary:   primary surface for VPP
//       subSurfs:  sub surfaces for composition [optional for CSC/Scaling]
//       numOfSubs: number of sub surfaces
//       outSurfs:  output buffer for VP
//       numOfOuts: number of out surfaces
//       syncFlag:  determine whether need to do vaSyncSurface
//*****************************************************************************
iVP_status iVP_exec_multiOut(iVPCtxID *ctx,
        iVP_layer_t       *primarySurf,
        iVP_layer_t        *subSurfs,
        unsigned int       numOfSubs,
        iVP_layer_t        *outSurfs,
        unsigned int       numOfOuts,
        bool               syncFlag
        );

//*****************************************************************************
//
// iVP_destroy_context
//  @param:
//       ctx  iVP Context ID
//
//
//*****************************************************************************
iVP_status iVP_destroy_context(iVPCtxID *ctx);

__END_DECLS
#endif /* __IVP_INTEL_VAAPI_H__ */

