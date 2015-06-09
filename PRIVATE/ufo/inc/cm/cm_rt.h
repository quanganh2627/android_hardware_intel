/**             
***
*** Copyright  (C) 1985-2012 Intel Corporation. All rights reserved.
***
*** The information and source code contained herein is the exclusive
*** property of Intel Corporation. and may not be disclosed, examined
*** or reproduced in whole or in part without explicit written authorization
*** from the company.
***
*** ----------------------------------------------------------------------------
**/ 
#pragma once

#if (!defined(__GNUC__))
#define __GNUC__
#endif

//Using CM_DX9 by default
#if (!defined(__GNUC__)) && (!defined(CM_DX11))
#ifndef CM_DX9
#define CM_DX9
#endif
#endif

#ifdef __cplusplus
#   define EXTERN_C     extern "C"
#else
#   define EXTERN_C
#endif

#include "cm_common.h"

#ifdef __GNUC__
#include "cm_rt_linux.h"
#else
#include "cm_rt_win.h"  
#endif


#define CM_KERNEL_FUNCTION(...) CM_KERNEL_FUNCTION2(__VA_ARGS__)

#define CM_RT_API 

#ifndef CM_1_0
#define CM_1_0          100
#endif 
#ifndef CM_2_0
#define CM_2_0          200
#endif
#ifndef CM_2_1
#define CM_2_1          201
#endif
#ifndef CM_2_2
#define CM_2_2          202
#endif
#ifndef CM_2_3
#define CM_2_3          203
#endif
#ifndef CM_2_4
#define CM_2_4          204
#endif
#ifndef CM_3_0
#define CM_3_0          300
#endif

//Define MDF version for MDF 3.0
#ifndef __INTEL_MDF 
#define __INTEL_MDF     CM_3_0
#endif

#define CM_SUCCESS                                  0
#define CM_FAILURE                                  -1
#define CM_NOT_IMPLEMENTED                          -2
#define CM_SURFACE_ALLOCATION_FAILURE               -3
#define CM_OUT_OF_HOST_MEMORY                       -4
#define CM_SURFACE_FORMAT_NOT_SUPPORTED             -5
#define CM_EXCEED_SURFACE_AMOUNT                    -6
#define CM_EXCEED_KERNEL_ARG_AMOUNT                 -7
#define CM_EXCEED_KERNEL_ARG_SIZE_IN_BYTE           -8
#define CM_INVALID_ARG_INDEX                        -9
#define CM_INVALID_ARG_VALUE                        -10
#define CM_INVALID_ARG_SIZE                         -11
#define CM_INVALID_THREAD_INDEX                     -12
#define CM_INVALID_WIDTH                            -13
#define CM_INVALID_HEIGHT                           -14
#define CM_INVALID_DEPTH                            -15
#define CM_INVALID_COMMON_ISA                       -16
#define CM_D3DDEVICEMGR_OPEN_HANDLE_FAIL            -17 // IDirect3DDeviceManager9::OpenDeviceHandle fail
#define CM_D3DDEVICEMGR_DXVA2_E_NEW_VIDEO_DEVICE    -18 // IDirect3DDeviceManager9::LockDevice return DXVA2_E_VIDEO_DEVICE_LOCKED
#define CM_D3DDEVICEMGR_LOCK_DEVICE_FAIL            -19 // IDirect3DDeviceManager9::LockDevice return other fail than DXVA2_E_VIDEO_DEVICE_LOCKED
#define CM_EXCEED_SAMPLER_AMOUNT                    -20
#define CM_EXCEED_MAX_KERNEL_PER_ENQUEUE            -21
#define CM_EXCEED_MAX_KERNEL_SIZE_IN_BYTE           -22
#define CM_EXCEED_MAX_THREAD_AMOUNT_PER_ENQUEUE     -23
#define CM_EXCEED_VME_STATE_G6_AMOUNT               -24
#define CM_INVALID_THREAD_SPACE                     -25 
#define CM_EXCEED_MAX_TIMEOUT                       -26
#define CM_JITDLL_LOAD_FAILURE                      -27
#define CM_JIT_COMPILE_FAILURE                      -28
#define CM_JIT_COMPILESIM_FAILURE                   -29
#define CM_INVALID_THREAD_GROUP_SPACE               -30
#define CM_THREAD_ARG_NOT_ALLOWED                   -31
#define CM_INVALID_GLOBAL_BUFFER_INDEX              -32
#define CM_INVALID_BUFFER_HANDLER                   -33
#define CM_EXCEED_MAX_SLM_SIZE                      -34
#define CM_JITDLL_OLDER_THAN_ISA                    -35
#define CM_INVALID_HARDWARE_THREAD_NUMBER           -36
#define CM_GTPIN_INVOKE_FAILURE                     -37
#define CM_INVALIDE_L3_CONFIGURATION                -38
#define CM_INVALID_D3D11_TEXTURE2D_USAGE            -39
#define CM_INTEL_GFX_NOTFOUND                       -40
#define CM_GPUCOPY_INVALID_SYSMEM                   -41
#define CM_GPUCOPY_INVALID_WIDTH                    -42
#define CM_GPUCOPY_INVALID_STRIDE                   -43
#define CM_EVENT_DRIVEN_FAILURE                     -44
#define CM_LOCK_SURFACE_FAIL                        -45 // Lock surface failed
#define CM_INVALID_GENX_BINARY                      -46
#define CM_FEATURE_NOT_SUPPORTED_IN_DRIVER          -47 // driver out-of-sync
#define CM_QUERY_DLL_VERSION_FAILURE                -48 //Fail in getting DLL file version
#define CM_KERNELPAYLOAD_PERTHREADARG_MUTEX_FAIL    -49
#define CM_KERNELPAYLOAD_PERKERNELARG_MUTEX_FAIL    -50
#define CM_KERNELPAYLOAD_SETTING_FAILURE            -51
#define CM_KERNELPAYLOAD_SURFACE_INVALID_BTINDEX    -52 
#define CM_NOT_SET_KERNEL_ARGUMENT                  -53
#define CM_GPUCOPY_INVALID_SURFACES                 -54
#define CM_GPUCOPY_INVALID_SIZE                     -55
#define CM_GPUCOPY_OUT_OF_RESOURCE                  -56
#define CM_DEVICE_INVALID_D3DDEVICE                 -57
#define CM_SURFACE_DELAY_DESTROY                    -58
#define	CM_INVALID_VEBOX_STATE                      -59
#define CM_INVALID_VEBOX_SURFACE                    -60
#define CM_FEATURE_NOT_SUPPORTED_BY_HARDWARE        -61
#define CM_RESOURCE_USAGE_NOT_SUPPORT_READWRITE     -62
#define CM_MULTIPLE_MIPLEVELS_NOT_SUPPORTED         -63
#define CM_INVALID_UMD_CONTEXT                      -64
#define CM_INVALID_LIBVA_SURFACE                    -65
#define CM_INVALID_LIBVA_INITIALIZE                 -66
#define CM_SURFACE_CACHED                           -74
#define CM_SURFACE_IN_USE                           -75
#define CM_INVALIDE_KERNEL_SPILL_CODE               -76

#define CM_MIN_SURF_WIDTH   1
#define CM_MIN_SURF_HEIGHT  1
#define CM_MIN_SURF_DEPTH   2

#define CM_MAX_1D_SURF_WIDTH 0X8000000 // 2^27

#define CM_MAX_GPUCOPY_SURFACE_WIDTH_IN_BYTE        65408
#define CM_MAX_GPUCOPY_SURFACE_HEIGHT  				4088

//IVB+
#define CM_MAX_2D_SURF_WIDTH_IVB_PLUS   16384
#define CM_MAX_2D_SURF_HEIGHT_IVB_PLUS  16384

#define CM_MAX_2D_SURF_WIDTH    CM_MAX_2D_SURF_WIDTH_IVB_PLUS
#define CM_MAX_2D_SURF_HEIGHT   CM_MAX_2D_SURF_HEIGHT_IVB_PLUS

#define CM_MAX_3D_SURF_WIDTH    2048
#define CM_MAX_3D_SURF_HEIGHT   2048
#define CM_MAX_3D_SURF_DEPTH    2048

#define CM_MAX_OPTION_SIZE_IN_BYTE          512
#define CM_MAX_KERNEL_NAME_SIZE_IN_BYTE     256
#define CM_MAX_ISA_FILE_NAME_SIZE_IN_BYTE   256

#define CM_MAX_THREADSPACE_WIDTH        511
#define CM_MAX_THREADSPACE_HEIGHT       511

#define IVB_MAX_SLM_SIZE_PER_GROUP   16 // 64KB PER Group on Gen7

#define COMPILER_RESERVED_SURFACE_NUM 5

//Time in seconds before kernel should timeout
#define CM_MAX_TIMEOUT 2
//Time in milliseconds before kernel should timeout
#define CM_MAX_TIMEOUT_MS CM_MAX_TIMEOUT*1000
#define CM_NO_EVENT  ((CmEvent *)(-1))	//NO Event

#define CM_DEVICE_CREATE_OPTION_DEFAULT                     0
#define CM_DEVICE_CREATE_OPTION_SCRATCH_SPACE_DISABLE       1
#define CM_DEVICE_CREATE_OPTION_SURFACE_REUSE_ENABLE        128

typedef enum _CM_STATUS
{
    CM_STATUS_QUEUED         = 0,
    CM_STATUS_FLUSHED        = 1,
    CM_STATUS_FINISHED       = 2,
    CM_STATUS_STARTED        = 3
} CM_STATUS;

typedef struct _CM_SAMPLER_STATE
{
    CM_TEXTURE_FILTER_TYPE minFilterType;
    CM_TEXTURE_FILTER_TYPE magFilterType;   
    CM_TEXTURE_ADDRESS_TYPE addressU;   
    CM_TEXTURE_ADDRESS_TYPE addressV;   
    CM_TEXTURE_ADDRESS_TYPE addressW; 
} CM_SAMPLER_STATE;

typedef enum _GPU_PLATFORM{
    PLATFORM_INTEL_UNKNOWN     = 0,
    PLATFORM_INTEL_SNB         = 1,   //Sandy Bridge
    PLATFORM_INTEL_IVB         = 2,   //Ivy Bridge
    PLATFORM_INTEL_HSW         = 3,   //Haswell
    PLATFORM_INTEL_BDW         = 4,   //Broadwell
    PLATFORM_INTEL_VLV         = 5    //ValleyView
} GPU_PLATFORM;

typedef enum _GPU_GT_PLATFORM{
    PLATFORM_INTEL_GT_UNKNOWN  = 0,
    PLATFORM_INTEL_GT1         = 1,
    PLATFORM_INTEL_GT2         = 2,
    PLATFORM_INTEL_GT3         = 3,
    PLATFORM_INTEL_GT4         = 4,
    PLATFORM_INTEL_GTVLV       = 5,
    PLATFORM_INTEL_GTVLVPLUS   = 6,
    PLATFORM_INTEL_GT1_5       = 7
    
} GPU_GT_PLATFORM;

typedef enum _CM_DEVICE_CAP_NAME
{
    CAP_KERNEL_COUNT_PER_TASK,
    CAP_KERNEL_BINARY_SIZE,
    CAP_SAMPLER_COUNT ,
    CAP_SAMPLER_COUNT_PER_KERNEL,
    CAP_BUFFER_COUNT ,
    CAP_SURFACE2D_COUNT,
    CAP_SURFACE3D_COUNT,
    CAP_SURFACE_COUNT_PER_KERNEL,
    CAP_ARG_COUNT_PER_KERNEL,
    CAP_ARG_SIZE_PER_KERNEL ,
    CAP_USER_DEFINED_THREAD_COUNT_PER_TASK,
    CAP_HW_THREAD_COUNT,
    CAP_SURFACE2D_FORMAT_COUNT,
    CAP_SURFACE2D_FORMATS,
    CAP_SURFACE3D_FORMAT_COUNT,
    CAP_SURFACE3D_FORMATS,
    CAP_VME_STATE_G6_COUNT,
    CAP_GPU_PLATFORM,
    CAP_GT_PLATFORM,
    CAP_MIN_FREQUENCY,
    CAP_MAX_FREQUENCY,
    CAP_L3_CONFIG,
    CAP_GPU_CURRENT_FREQUENCY
} CM_DEVICE_CAP_NAME;

typedef enum _CM_FASTCOPY_OPTION
{
    CM_FASTCOPY_OPTION_NONBLOCKING  = 0x00,
    CM_FASTCOPY_OPTION_BLOCKING     = 0x01
} CM_FASTCOPY_OPTION;

// CM RT DLL File Version
typedef struct _CM_DLL_FILE_VERSION
{
    WORD    wMANVERSION;
    WORD    wMANREVISION;
    WORD    wSUBREVISION;
    WORD    wBUILD_NUMBER; 
    //Version constructed as : "wMANVERSION.wMANREVISION.wSUBREVISION.wBUILD_NUMBER"
} CM_DLL_FILE_VERSION, *PCM_DLL_FILE_VERSION;

#define NUM_SEARCH_PATH_STATES_G6       14
#define NUM_MBMODE_SETS_G6              4

typedef struct _VME_SEARCH_PATH_LUT_STATE_G6
{
    // DWORD 0
    union
    {
        struct
        {
            DWORD   SearchPathLocation_X_0  : 4;
            DWORD   SearchPathLocation_Y_0  : 4;
            DWORD   SearchPathLocation_X_1  : 4;
            DWORD   SearchPathLocation_Y_1  : 4;
            DWORD   SearchPathLocation_X_2  : 4;
            DWORD   SearchPathLocation_Y_2  : 4;
            DWORD   SearchPathLocation_X_3  : 4;
            DWORD   SearchPathLocation_Y_3  : 4;
        };
        struct
        {
            DWORD   Value;
        };
    };
} VME_SEARCH_PATH_LUT_STATE_G6, *PVME_SEARCH_PATH_LUT_STATE_G6;

typedef struct _VME_RD_LUT_SET_G6
{
    // DWORD 0
    union
    {
        struct
        {
            DWORD   LUT_MbMode_0    : 8;
            DWORD   LUT_MbMode_1    : 8;
            DWORD   LUT_MbMode_2    : 8;
            DWORD   LUT_MbMode_3    : 8;
        };
        struct
        {
            DWORD   Value;
        };
    } DW0;

    // DWORD 1
    union
    {
        struct
        {
            DWORD   LUT_MbMode_4    : 8;
            DWORD   LUT_MbMode_5    : 8;
            DWORD   LUT_MbMode_6    : 8;
            DWORD   LUT_MbMode_7    : 8;
        };
        struct
        {
            DWORD   Value;
        };
    } DW1;

    // DWORD 2
    union
    {
        struct 
        {
            DWORD   LUT_MV_0        : 8;
            DWORD   LUT_MV_1        : 8;
            DWORD   LUT_MV_2        : 8;
            DWORD   LUT_MV_3        : 8;
        };
        struct
        {
            DWORD   Value;
        };
    } DW2;

    // DWORD 3
    union
    {
        struct
        {
            DWORD   LUT_MV_4        : 8;
            DWORD   LUT_MV_5        : 8;
            DWORD   LUT_MV_6        : 8;
            DWORD   LUT_MV_7        : 8;
        };
        struct
        {
            DWORD   Value;
        };
    } DW3;
} VME_RD_LUT_SET_G6, *PVME_RD_LUT_SET_G6;


typedef struct _VME_STATE_G6
{
    // DWORD 0 - DWORD 13
    VME_SEARCH_PATH_LUT_STATE_G6    SearchPath[NUM_SEARCH_PATH_STATES_G6];

    // DWORD 14
    union
    {
        struct
        {
            DWORD   LUT_MbMode_8_0  : 8;
            DWORD   LUT_MbMode_9_0  : 8;
            DWORD   LUT_MbMode_8_1  : 8;
            DWORD   LUT_MbMode_9_1  : 8;
        };
        struct
        {
            DWORD   Value;
        };
    } DW14;

    // DWORD 15
    union
    {
        struct
        {
            DWORD   LUT_MbMode_8_2  : 8;
            DWORD   LUT_MbMode_9_2  : 8;
            DWORD   LUT_MbMode_8_3  : 8;
            DWORD   LUT_MbMode_9_3  : 8;
        };
        struct
        {
            DWORD   Value;
        };
    } DW15;

    // DWORD 16 - DWORD 31
    VME_RD_LUT_SET_G6   RdLutSet[NUM_MBMODE_SETS_G6];
} VME_STATE_G6, *PVME_STATE_G6;

#define CM_MAX_DEPENDENCY_COUNT         8

typedef enum _CM_DEPENDENCY_PATTERN
{
    CM_NONE_DEPENDENCY          = 0,    //All threads run parallel, scanline dispatch
    CM_WAVEFRONT                = 1,
    CM_WAVEFRONT26              = 2,
    CM_VERTICAL_DEPENDENCY      = 3,
    CM_HORIZONTAL_DEPENDENCY    = 4,
    CM_WAVEFRONT26Z             = 5
} CM_DEPENDENCY_PATTERN;

typedef struct _CM_DEPENDENCY
{
    UINT    count;
    INT     deltaX[CM_MAX_DEPENDENCY_COUNT];
    INT     deltaY[CM_MAX_DEPENDENCY_COUNT];
}CM_DEPENDENCY;

/**************** L3/Cache ***************/
typedef enum _MEMORY_OBJECT_CONTROL{
    // SNB
    MEMORY_OBJECT_CONTROL_USE_GTT_ENTRY,
    MEMORY_OBJECT_CONTROL_NEITHER_LLC_NOR_MLC,
    MEMORY_OBJECT_CONTROL_LLC_NOT_MLC,
    MEMORY_OBJECT_CONTROL_LLC_AND_MLC,

    // IVB
    MEMORY_OBJECT_CONTROL_FROM_GTT_ENTRY = MEMORY_OBJECT_CONTROL_USE_GTT_ENTRY,                                 // Caching dependent on pte
    MEMORY_OBJECT_CONTROL_L3,                                             // Cached in L3$
    MEMORY_OBJECT_CONTROL_LLC,                                            // Cached in LLC 
    MEMORY_OBJECT_CONTROL_LLC_L3,                                         // Cached in LLC & L3$

    // HSW
#ifdef CMRT_SIM
    MEMORY_OBJECT_CONTROL_USE_PTE, // Caching dependent on pte
#else
    MEMORY_OBJECT_CONTROL_USE_PTE = MEMORY_OBJECT_CONTROL_FROM_GTT_ENTRY, // Caching dependent on pte
#endif
    MEMORY_OBJECT_CONTROL_UC,                                             // Uncached
    MEMORY_OBJECT_CONTROL_LLC_ELLC,
    MEMORY_OBJECT_CONTROL_ELLC,
    MEMORY_OBJECT_CONTROL_L3_USE_PTE,
    MEMORY_OBJECT_CONTROL_L3_UC,
    MEMORY_OBJECT_CONTROL_L3_LLC_ELLC,
    MEMORY_OBJECT_CONTROL_L3_ELLC,

    MEMORY_OBJECT_CONTROL_UNKNOWN = 0xff
} MEMORY_OBJECT_CONTROL;

typedef enum _MEMORY_TYPE {
    CM_USE_PTE,
    CM_UN_CACHEABLE,
    CM_WRITE_THROUGH,
    CM_WRITE_BACK
} MEMORY_TYPE;

typedef enum _CM_BOUNDARY_PIXEL_MODE
{
    CM_BOUNDARY_NORMAL  = 0x0,
    CM_BOUNDARY_PROGRESSIVE_FRAME = 0x2,
    CM_BOUNARY_INTERLACED_FRAME = 0x3
}CM_BOUNDARY_PIXEL_MODE;

#define BDW_L3_CONFIG_NUM 8
#define HSW_L3_CONFIG_NUM 12
#define IVB_2_L3_CONFIG_NUM 12
#define IVB_1_L3_CONFIG_NUM 12

typedef struct _L3_CONFIG_REGISTER_VALUES{
    UINT SQCREG1_VALUE;
    UINT CNTLREG2_VALUE;
    UINT CNTLREG3_VALUE;
} L3_CONFIG_REGISTER_VALUES;

typedef enum _L3_SUGGEST_CONFIG
{
    IVB_L3_PLANE_DEFAULT,
    IVB_L3_PLANE_1,
    IVB_L3_PLANE_2,
    IVB_L3_PLANE_3,
    IVB_L3_PLANE_4,
    IVB_L3_PLANE_5,
    IVB_L3_PLANE_6,
    IVB_L3_PLANE_7,
    IVB_L3_PLANE_8,
    IVB_L3_PLANE_9,
    IVB_L3_PLANE_10,
    IVB_L3_PLANE_11,
    
    HSW_L3_PLANE_DEFAULT = IVB_L3_PLANE_DEFAULT,
    HSW_L3_PLANE_1,
    HSW_L3_PLANE_2,
    HSW_L3_PLANE_3,
    HSW_L3_PLANE_4,
    HSW_L3_PLANE_5,
    HSW_L3_PLANE_6,
    HSW_L3_PLANE_7,
    HSW_L3_PLANE_8,
    HSW_L3_PLANE_9,
    HSW_L3_PLANE_10,
    HSW_L3_PLANE_11,

    IVB_SLM_PLANE_DEFAULT = IVB_L3_PLANE_9,
    HSW_SLM_PLANE_DEFAULT = HSW_L3_PLANE_9
} L3_SUGGEST_CONFIG;

static const L3_CONFIG_REGISTER_VALUES IVB_L3_PLANE[IVB_1_L3_CONFIG_NUM]  = 
{                                                                    // SLM    URB   Rest DC     RO     I/S    C    T      Sum
    {0x01730000, 0x00080040, 0x00000000},     // {0,    256,    0,     0,     256,    0,     0,    0,     512},
    {0x00730000, 0x02040040, 0x00000000},     //{0,    256,    0,   128,     128,    0,     0,    0,     512},
    {0x00730000, 0x00800040, 0x00080410},     //{0,    256,    0,    32,       0,   64,    32,  128,     512},
    {0x00730000, 0x01000038, 0x00080410},     //{0,    224,    0,    64,       0,   64,    32,  128,     512},
    {0x00730000, 0x02000038, 0x00040410},     //{0,    224,    0,   128,       0,   64,    32,   64,     512},
    {0x00730000, 0x01000038, 0x00040420},     //{0,    224,    0,    64,       0,  128,    32,   64,     512},
    {0x01730000, 0x00000038, 0x00080420},     //{0,    224,    0,     0,       0,  128,    32,  128,     512},
    {0x01730000, 0x00000040, 0x00080020},     //{0,    256,    0,     0,       0,  128,     0,  128,     512},
    {0x00730000, 0x020400a1, 0x00000000},      //{128,    128,    0,   128,     128,    0,     0,    0,     512},
    {0x00730000, 0x010000a1, 0x00040810},      // {128,    128,    0,    64,       0,   64,    64,   64,     512},
    {0x00730000, 0x008000a1, 0x00080410},      //{128,    128,    0,    32,       0,   64,    32,  128,     512},
    {0x00730000, 0x008000a1, 0x00040420}      //{128,    128,    0,    32,       0,  128,   32,    64,     512}
};

static const L3_CONFIG_REGISTER_VALUES HSW_L3_PLANE[HSW_L3_CONFIG_NUM]  = 
{                                                                    // SLM    URB   Rest DC     RO     I/S    C    T      Sum
    {0x01610000, 0x00080040, 0x00000000},     // {0,    256,    0,     0,     256,    0,     0,    0,     512},
    {0x00610000, 0x02040040, 0x00000000},     //{0,    256,    0,   128,     128,    0,     0,    0,     512},
    {0x00610000, 0x00800040, 0x00080410},     //{0,    256,    0,    32,       0,   64,    32,  128,     512},
    {0x00610000, 0x01000038, 0x00080410},     //{0,    224,    0,    64,       0,   64,    32,  128,     512},
    {0x00610000, 0x02000038, 0x00040410},     //{0,    224,    0,   128,       0,   64,    32,   64,     512},
    {0x00610000, 0x01000038, 0x00040420},     //{0,    224,    0,    64,       0,  128,    32,   64,     512},
    {0x01610000, 0x00000038, 0x00080420},     //{0,    224,    0,     0,       0,  128,    32,  128,     512},
    {0x01610000, 0x00000040, 0x00080020},     //{0,    256,    0,     0,       0,  128,     0,  128,     512},
    {0x00610000, 0x020400a1, 0x00000000},      //{128,    128,    0,   128,     128,    0,     0,    0,     512},
    {0x00610000, 0x010000a1, 0x00040810},      // {128,    128,    0,    64,       0,   64,    64,   64,     512},
    {0x00610000, 0x008000a1, 0x00080410},      //{128,    128,    0,    32,       0,   64,    32,  128,     512},
    {0x00610000, 0x008000a1, 0x00040420}      //{128,    128,    0,    32,       0,  128,   32,    64,     512}
};

static const L3_CONFIG_REGISTER_VALUES BDW_L3_PLANE[BDW_L3_CONFIG_NUM]  = 
{                                                                    // SLM    URB   Rest DC     RO     I/S    C    T      Sum
    {0x01610000, 0x00080040, 0x00000000},     //{0,    48,    48,    0,    0,    0,    0,    0,    96},
    {0x00610000, 0x02040040, 0x00000000},     //{0,    48,    0,    16,    32,    0,    0,    0,    96},
    {0x00610000, 0x02040040, 0x00000000},     //{0,    32,    0,    16,    48,    0,    0,    0,    96},
    {0x00610000, 0x02040040, 0x00000000},     //{0,    32,    0,    0,    64,    0,    0,    0,    96},
    {0x00610000, 0x02040040, 0x00000000},     //{0,    32,    64,    0,    0,    0,    0,    0,    96},
    {0x00610000, 0x02040040, 0x00000000},     //{32,    16,    48,    0,    0,    0,    0,    0,    96},
    {0x00610000, 0x02040040, 0x00000000},     //{32,    16,    0,    16,    32,    0,    0,    0,    96},
    {0x00610000, 0x02040040, 0x00000000}      //{32,    16,    0,    32,    16,    0,    0,    0,    96}
};

/***********START SAMPLER8X8******************/
//Sampler8x8 data structures

typedef enum _CM_SAMPLER8x8_SURFACE_
{
    CM_AVS_SURFACE = 0,
    CM_VA_SURFACE = 1
}CM_SAMPLER8x8_SURFACE;

typedef enum _CM_VA_PLUS_SURFACE_
{
    CM_VA_PLUS_OTHER = 0,
    CM_VA_PLUS_CORRECLATION_SEARCH_SURFACE = 1,
    CM_VA_PLUS_LBP_CORRELATION_SURFACE = 2
}CM_VA_PLUS_SURFACE;

typedef enum _CM_SURFACE_ADDRESS_CONTROL_MODE_
{
    CM_SURFACE_CLAMP = 0,
    CM_SURFACE_MIRROR = 1
}CM_SURFACE_ADDRESS_CONTROL_MODE;

typedef enum _CM_MESSAGE_SEQUENCE_
{
    CM_MS_1x1   = 0,
    CM_MS_16x1  = 1,
    CM_MS_16x4  = 2,
    CM_MS_32x1  = 3,
    CM_MS_32x4  = 4,
    CM_MS_64x1  = 5,
    CM_MS_64x4  = 6
}CM_MESSAGE_SEQUENCE;

typedef enum _CM_MIN_MAX_FILTER_CONTROL_
{
    CM_MIN_FILTER   = 0,
    CM_MAX_FILTER   = 1,
    CM_BOTH_FILTER  = 3
}CM_MIN_MAX_FILTER_CONTROL;

typedef enum _CM_VA_FUNCTION_
{
    CM_VA_MINMAXFILTER  = 0,
    CM_VA_DILATE        = 1,
    CM_VA_ERODE         = 2
} CM_VA_FUNCTION;

//GT-PIN
typedef struct _CM_SURFACE_DETAILS{
    UINT        width; 
    UINT        height; 
    UINT        depth; 
    CM_SURFACE_FORMAT   format; 
    UINT        planeIndex;
    UINT        pitch; 
    UINT        slicePitch;
    UINT        SurfaceBaseAddress;
    UINT8       TiledSurface;
    UINT8       TileWalk;
    UINT        XOffset;
    UINT        YOffset; 
    
}CM_SURFACE_DETAILS;


/*
 *  AVS SAMPLER8x8 STATE
 */
typedef struct _CM_AVS_COEFF_TABLE{
    float   FilterCoeff_0_0;
    float   FilterCoeff_0_1;
    float   FilterCoeff_0_2;
    float   FilterCoeff_0_3;
    float   FilterCoeff_0_4;
    float   FilterCoeff_0_5;
    float   FilterCoeff_0_6;
    float   FilterCoeff_0_7;
}CM_AVS_COEFF_TABLE;

typedef struct _CM_AVS_INTERNEL_COEFF_TABLE{
    BYTE   FilterCoeff_0_0;
    BYTE   FilterCoeff_0_1;
    BYTE   FilterCoeff_0_2;
    BYTE   FilterCoeff_0_3;
    BYTE   FilterCoeff_0_4;
    BYTE   FilterCoeff_0_5;
    BYTE   FilterCoeff_0_6;
    BYTE   FilterCoeff_0_7;
}CM_AVS_INTERNEL_COEFF_TABLE;

#define CM_NUM_COEFF_ROWS 17
typedef struct _CM_AVS_NONPIPLINED_STATE{
    bool BypassXAF;
    bool BypassYAF;
    BYTE DefaultSharpLvl;
    BYTE maxDerivative4Pixels;
    BYTE maxDerivative8Pixels;
    BYTE transitionArea4Pixels;
    BYTE transitionArea8Pixels;    
    CM_AVS_COEFF_TABLE Tbl0X[CM_NUM_COEFF_ROWS];
    CM_AVS_COEFF_TABLE Tbl0Y[CM_NUM_COEFF_ROWS];
    CM_AVS_COEFF_TABLE Tbl1X[CM_NUM_COEFF_ROWS];
    CM_AVS_COEFF_TABLE Tbl1Y[CM_NUM_COEFF_ROWS];
}CM_AVS_NONPIPLINED_STATE;

typedef struct _CM_AVS_INTERNEL_NONPIPLINED_STATE{
    bool BypassXAF;
    bool BypassYAF;
    BYTE DefaultSharpLvl;
    BYTE maxDerivative4Pixels;
    BYTE maxDerivative8Pixels;
    BYTE transitionArea4Pixels;
    BYTE transitionArea8Pixels;    
    CM_AVS_INTERNEL_COEFF_TABLE Tbl0X[CM_NUM_COEFF_ROWS];
    CM_AVS_INTERNEL_COEFF_TABLE Tbl0Y[CM_NUM_COEFF_ROWS];
    CM_AVS_INTERNEL_COEFF_TABLE Tbl1X[CM_NUM_COEFF_ROWS];
    CM_AVS_INTERNEL_COEFF_TABLE Tbl1Y[CM_NUM_COEFF_ROWS];
}CM_AVS_INTERNEL_NONPIPLINED_STATE;

typedef struct _CM_AVS_STATE_MSG{
    bool AVSTYPE; //true nearest, false adaptive    
    bool EightTapAFEnable; //HSW+
    bool BypassIEF; //ignored for BWL, moved to sampler8x8 payload.
    bool ShuffleOutputWriteback; //SKL mode only to be set when AVS msg sequence is 4x4 or 8x4
    unsigned short GainFactor;
    unsigned char GlobalNoiseEstm;
    unsigned char StrongEdgeThr;
    unsigned char WeakEdgeThr;
    unsigned char StrongEdgeWght;
    unsigned char RegularWght;
    unsigned char NonEdgeWght;
    //For Non-piplined states
    unsigned short stateID;
    CM_AVS_NONPIPLINED_STATE * AvsState;
} CM_AVS_STATE_MSG;

/*
 *  CONVOLVE STATE DATA STRUCTURES
 */

typedef struct _CM_CONVOLVE_COEFF_TABLE{
    float   FilterCoeff_0_0;
    float   FilterCoeff_0_1;
    float   FilterCoeff_0_2;
    float   FilterCoeff_0_3;
    float   FilterCoeff_0_4;
    float   FilterCoeff_0_5;
    float   FilterCoeff_0_6;
    float   FilterCoeff_0_7;
    float   FilterCoeff_0_8;
    float   FilterCoeff_0_9;
    float   FilterCoeff_0_10;
    float   FilterCoeff_0_11;
    float   FilterCoeff_0_12;
    float   FilterCoeff_0_13;
    float   FilterCoeff_0_14;
    float   FilterCoeff_0_15;
    float   FilterCoeff_0_16;    
    float   FilterCoeff_0_17;
    float   FilterCoeff_0_18;
    float   FilterCoeff_0_19;
    float   FilterCoeff_0_20;
    float   FilterCoeff_0_21;
    float   FilterCoeff_0_22;
    float   FilterCoeff_0_23;
    float   FilterCoeff_0_24;
    float   FilterCoeff_0_25;
    float   FilterCoeff_0_26;
    float   FilterCoeff_0_27;
    float   FilterCoeff_0_28;
    float   FilterCoeff_0_29;
    float   FilterCoeff_0_30;
    float   FilterCoeff_0_31;
}CM_CONVOLVE_COEFF_TABLE;

#define CM_NUM_CONVOLVE_ROWS 16
#define CM_NUM_CONVOLVE_ROWS_SKL 32
typedef struct _CM_CONVOLVE_STATE_MSG{
        bool CoeffSize; //true 16-bit, false 8-bit
        byte SclDwnValue; //Scale down value
        byte Width; //Kernel Width
        byte Height; //Kernel Height   
        //SKL mode
        bool isVertical32Mode;
        bool isHorizontal32Mode;
    CM_CONVOLVE_COEFF_TABLE Table[CM_NUM_CONVOLVE_ROWS_SKL];
} CM_CONVOLVE_STATE_MSG;

/*
 *   MISC SAMPLER8x8 State
 */
typedef struct _CM_MISC_STATE {
    //DWORD 0
    union{
        struct{
            DWORD   Row0      : 16;
            DWORD   Reserved  : 8;
            DWORD   Width     : 4;
            DWORD   Height    : 4;
        };
        struct{
            DWORD value;
        };
    } DW0;

    //DWORD 1
    union{
        struct{
            DWORD   Row1      : 16;
            DWORD   Row2      : 16;
        };
        struct{
            DWORD value;
        };
    } DW1;

    //DWORD 2
    union{
        struct{
            DWORD   Row3      : 16;
            DWORD   Row4      : 16;
        };
        struct{
            DWORD value;
        };
    } DW2;

    //DWORD 3
    union{
        struct{
            DWORD   Row5      : 16;
            DWORD   Row6      : 16;
        };
        struct{
            DWORD value;
        };
    } DW3;

    //DWORD 4
    union{
        struct{
            DWORD   Row7      : 16;
            DWORD   Row8      : 16;
        };
        struct{
            DWORD value;
        };
    } DW4;

    //DWORD 5
    union{
        struct{
            DWORD   Row9      : 16;
            DWORD   Row10      : 16;
        };
        struct{
            DWORD value;
        };
    } DW5;

    //DWORD 6
    union{
        struct{
            DWORD   Row11      : 16;
            DWORD   Row12      : 16;
        };
        struct{
            DWORD value;
        };
    } DW6;

    //DWORD 7
    union{
        struct{
            DWORD   Row13      : 16;
            DWORD   Row14      : 16;
        };
        struct{
            DWORD value;
        };
    } DW7;
} CM_MISC_STATE;
 
typedef struct _CM_MISC_STATE_MSG{
    //DWORD 0
    union{
        struct{
            DWORD   Row0      : 16;
            DWORD   Reserved  : 8;
            DWORD   Width     : 4;
            DWORD   Height    : 4;
        };
        struct{
            DWORD value;
        };
    }DW0;

    //DWORD 1
    union{
        struct{
            DWORD   Row1      : 16;
            DWORD   Row2      : 16;
        };
        struct{
            DWORD value;
        };
    }DW1;

    //DWORD 2
    union{
        struct{
            DWORD   Row3      : 16;
            DWORD   Row4      : 16;
        };
        struct{
            DWORD value;
        };
    }DW2;

    //DWORD 3
    union{
        struct{
            DWORD   Row5      : 16;
            DWORD   Row6      : 16;
        };
        struct{
            DWORD value;
        };
    }DW3;

    //DWORD 4
    union{
        struct{
            DWORD   Row7      : 16;
            DWORD   Row8      : 16;
        };
        struct{
            DWORD value;
        };
    }DW4;

    //DWORD 5
    union{
        struct{
            DWORD   Row9      : 16;
            DWORD   Row10      : 16;
        };
        struct{
            DWORD value;
        };
    }DW5;

    //DWORD 6
    union{
        struct{
            DWORD   Row11      : 16;
            DWORD   Row12      : 16;
        };
        struct{
            DWORD value;
        };
    }DW6;

    //DWORD 7
    union{
        struct{
            DWORD   Row13      : 16;
            DWORD   Row14      : 16;
        };
        struct{
            DWORD value;
        };
    }DW7;
} CM_MISC_STATE_MSG;

typedef enum _CM_SAMPLER_STATE_TYPE_
{
    CM_SAMPLER8X8_AVS   = 0,
    CM_SAMPLER8X8_CONV  = 1,
    CM_SAMPLER8X8_MISC  = 3,
    CM_SAMPLER8X8_NONE
}CM_SAMPLER_STATE_TYPE;

typedef struct _CM_SAMPLER_8X8_DESCR{
    CM_SAMPLER_STATE_TYPE stateType;
    union
    {
        CM_AVS_STATE_MSG * avs;
        CM_CONVOLVE_STATE_MSG * conv;
        CM_MISC_STATE_MSG * misc; //ERODE/DILATE/MINMAX
    };
} CM_SAMPLER_8X8_DESCR;

typedef  struct _CM_HAL_AVS_PARAM {
    CM_AVS_STATE_MSG avs;
    CM_AVS_INTERNEL_NONPIPLINED_STATE avs_nonpipelined;
} CM_HAL_AVS_PARAM;

typedef struct _CM_SAMPLER_8X8_STATE {
    CM_SAMPLER_STATE_TYPE stateType;
    union {
        CM_HAL_AVS_PARAM           avs_state;
        CM_CONVOLVE_STATE_MSG convolve_state;
        CM_MISC_STATE                  misc_state;
    };
} CM_SAMPLER_8X8_STATE;

typedef struct _CM_HAL_SAMPLER_8X8_STATE_PARAM{
    CM_SAMPLER_8X8_STATE    sampler8x8State;
    DWORD                              dwHandle;                                       // [out] Handle 
} CM_HAL_SAMPLER_8X8_STATE_PARAM;

//GT-PINS urfaceDetails
typedef struct _CM_HAL_SURFACE_DETAILS{
    CM_SAMPLER_8X8_STATE    sampler8x8State;
    DWORD                   dwHandle;                                       // [out] Handle 
} CM_HAL_SURFACE_DETAILS;

//!
//! CM Sampler8X8 
//!
class CmSampler8x8
{
public:
    CM_RT_API virtual INT GetIndex( SamplerIndex* & pIndex ) = 0 ;

};
/***********END SAMPLER8X8********************/
class CmEvent
{
public:
    CM_RT_API virtual INT GetStatus( CM_STATUS& status) = 0 ;
    CM_RT_API virtual INT GetExecutionTime(UINT64& time) = 0;
    CM_RT_API virtual INT WaitForTaskFinished(DWORD dwTimeOutMs = CM_MAX_TIMEOUT_MS) = 0 ;
};

class CmThreadSpace;

class CmKernel
{
public:       
    CM_RT_API virtual INT SetThreadCount(UINT count ) = 0;
    CM_RT_API virtual INT SetKernelArg(UINT index, size_t size, const void * pValue ) = 0;

    CM_RT_API virtual INT SetThreadArg(UINT threadId, UINT index, size_t size, const void * pValue ) = 0;
    CM_RT_API virtual INT SetStaticBuffer(UINT index, const void * pValue ) = 0;

    CM_RT_API virtual INT SetKernelPayloadData(size_t size, const void *pValue) = 0;
    CM_RT_API virtual INT SetKernelPayloadSurface(UINT surfaceCount, SurfaceIndex** pSurfaces) = 0;
    CM_RT_API virtual INT SetSurfaceBTI(SurfaceIndex* pSurface, UINT BTIndex) = 0;    

    CM_RT_API virtual INT AssociateThreadSpace(CmThreadSpace* & pTS) = 0;
};

class CmTask
{
public:       
    CM_RT_API virtual INT AddKernel(CmKernel *pKernel) = 0;
    CM_RT_API virtual INT Reset(void) = 0;
    CM_RT_API virtual INT AddSync(void) = 0;
}; 

class CmProgram;
class SurfaceIndex;
class SamplerIndex;

class CmBuffer
{
public:
    CM_RT_API virtual INT GetIndex( SurfaceIndex*& pIndex ) = 0; 
    CM_RT_API virtual INT ReadSurface( unsigned char* pSysMem, CmEvent* pEvent, UINT64 sysMemSize = 0xFFFFFFFFFFFFFFFFULL ) = 0;
    CM_RT_API virtual INT WriteSurface( const unsigned char* pSysMem, CmEvent* pEvent, UINT64 sysMemSize = 0xFFFFFFFFFFFFFFFFULL ) = 0;
    CM_RT_API virtual INT InitSurface(const DWORD initValue, CmEvent* pEvent) = 0;
    CM_RT_API virtual INT SetMemoryObjectControl(MEMORY_OBJECT_CONTROL mem_ctrl, MEMORY_TYPE mem_type, UINT  age) = 0;
};

class CmBufferUP
{
public:
    CM_RT_API virtual INT GetIndex( SurfaceIndex*& pIndex ) = 0; 
    CM_RT_API virtual INT SetMemoryObjectControl(MEMORY_OBJECT_CONTROL mem_ctrl, MEMORY_TYPE mem_type, UINT  age) = 0;
};

class CmSurface2D
{
public:    
    CM_RT_API virtual INT GetIndex( SurfaceIndex*& pIndex ) = 0; 

#ifdef CM_DX9
    CM_RT_API virtual INT GetD3DSurface( IDirect3DSurface9*& pD3DSurface) = 0;
#elif defined(CM_DX11)
    CM_RT_API virtual INT GetD3DSurface( ID3D11Texture2D*& pD3D11Texture2D) = 0;
#endif

    CM_RT_API virtual INT ReadSurface( unsigned char* pSysMem, CmEvent* pEvent, UINT64 sysMemSize = 0xFFFFFFFFFFFFFFFFULL ) = 0;
    CM_RT_API virtual INT WriteSurface( const unsigned char* pSysMem, CmEvent* pEvent, UINT64 sysMemSize = 0xFFFFFFFFFFFFFFFFULL ) = 0;
    CM_RT_API virtual INT ReadSurfaceStride( unsigned char* pSysMem, CmEvent* pEvent, const UINT stride, UINT64 sysMemSize = 0xFFFFFFFFFFFFFFFFULL ) = 0;
    CM_RT_API virtual INT WriteSurfaceStride( const unsigned char* pSysMem, CmEvent* pEvent, const UINT stride, UINT64 sysMemSize = 0xFFFFFFFFFFFFFFFFULL ) = 0;
    CM_RT_API virtual INT InitSurface(const DWORD initValue, CmEvent* pEvent) = 0;
#ifdef CM_DX11
    CM_RT_API virtual INT QuerySubresourceIndex(UINT& FirstArraySlice, UINT& FirstMipSlice) = 0;
#endif
    CM_RT_API virtual INT SetMemoryObjectControl(MEMORY_OBJECT_CONTROL mem_ctrl, MEMORY_TYPE mem_type, UINT  age) = 0;
    CM_RT_API virtual INT SetSurfaceState(UINT iWidth, UINT iHeight, CM_SURFACE_FORMAT Format, CM_BOUNDARY_PIXEL_MODE boundaryMode) = 0;
};

class CmSurface2DUP  
{
public:    
    CM_RT_API virtual INT GetIndex( SurfaceIndex*& pIndex ) = 0; 
    CM_RT_API virtual INT SetMemoryObjectControl(MEMORY_OBJECT_CONTROL mem_ctrl, MEMORY_TYPE mem_type, UINT  age) = 0;
};

class CmSurface3D  
{
public:    
    CM_RT_API virtual INT GetIndex( SurfaceIndex*& pIndex ) = 0; 
    CM_RT_API virtual INT ReadSurface( unsigned char* pSysMem, CmEvent* pEvent, UINT64 sysMemSize = 0xFFFFFFFFFFFFFFFFULL ) = 0;
    CM_RT_API virtual INT WriteSurface( const unsigned char* pSysMem, CmEvent* pEvent, UINT64 sysMemSize = 0xFFFFFFFFFFFFFFFFULL ) = 0;
    CM_RT_API virtual INT InitSurface(const DWORD initValue, CmEvent* pEvent) = 0;
    CM_RT_API virtual INT SetMemoryObjectControl(MEMORY_OBJECT_CONTROL mem_ctrl, MEMORY_TYPE mem_type, UINT  age) = 0;
};

class CmSampler
{
public:
    CM_RT_API virtual INT GetIndex( SamplerIndex* & pIndex ) = 0 ;
};

class CmThreadSpace
{
public:
    CM_RT_API virtual INT AssociateThread( UINT x, UINT y, CmKernel* pKernel , UINT threadId ) = 0;
    CM_RT_API virtual INT SelectThreadDependencyPattern ( CM_DEPENDENCY_PATTERN pattern ) = 0;
    CM_RT_API virtual INT AssociateThreadWithMask( UINT x, UINT y, CmKernel* pKernel, UINT threadId, BYTE nDependencyMask ) = 0;
    CM_RT_API virtual INT SetThreadSpaceColorCount( UINT colorCount ) = 0;
};

class CmThreadGroupSpace;

class CmQueue
{
public:    
    CM_RT_API virtual INT Enqueue( CmTask* pTask, CmEvent* & pEvent, const CmThreadSpace* pTS = NULL ) = 0;
    CM_RT_API virtual INT DestroyEvent( CmEvent* & pEvent ) = 0; 
    CM_RT_API virtual INT EnqueueWithGroup( CmTask* pTask, CmEvent* & pEvent, const CmThreadGroupSpace* pTGS = NULL )=0;
    CM_RT_API virtual INT EnqueueCopyCPUToGPU( CmSurface2D* pSurface, const unsigned char* pSysMem, CmEvent* & pEvent ) = 0; 
    CM_RT_API virtual INT EnqueueCopyGPUToCPU( CmSurface2D* pSurface, unsigned char* pSysMem, CmEvent* & pEvent ) = 0;
    CM_RT_API virtual INT EnqueueCopyCPUToGPUStride( CmSurface2D* pSurface, const unsigned char* pSysMem, const UINT stride, CmEvent* & pEvent ) = 0; 
    CM_RT_API virtual INT EnqueueCopyGPUToCPUStride( CmSurface2D* pSurface, unsigned char* pSysMem, const UINT stride, CmEvent* & pEvent ) = 0;
    CM_RT_API virtual INT EnqueueInitSurface2D( CmSurface2D* pSurface, const DWORD initValue, CmEvent* &pEvent ) = 0;
    CM_RT_API virtual INT EnqueueCopyGPUToGPU( CmSurface2D* pOutputSurface, CmSurface2D* pInputSurface, CmEvent* & pEvent ) = 0;
    CM_RT_API virtual INT EnqueueCopyCPUToCPU( unsigned char* pDstSysMem, unsigned char* pSrcSysMem, UINT size, CmEvent* & pEvent ) = 0;
    CM_RT_API virtual INT EnqueueCopyCPUToGPUFullStride( CmSurface2D* pSurface, const unsigned char* pSysMem, const UINT widthStride, const UINT heightStride, const UINT option, CmEvent* & pEvent ) = 0;
    CM_RT_API virtual INT EnqueueCopyGPUToCPUFullStride( CmSurface2D* pSurface, unsigned char* pSysMem, const UINT widthStride, const UINT heightStride, const UINT option, CmEvent* & pEvent ) = 0;
    CM_RT_API virtual INT EnqueueWithHints( CmTask* pTask, CmEvent* & pEvent, UINT hints = 0) = 0;
};

class CmVmeState
{
public:
    CM_RT_API virtual INT GetIndex( VmeIndex* & pIndex ) = 0 ;
};

class CmDevice
{
public:

#ifdef CM_DX9
    CM_RT_API virtual INT GetD3DDeviceManager( IDirect3DDeviceManager9* & pDeviceManager )= 0;
#endif

    CM_RT_API virtual INT CreateBuffer(UINT size, CmBuffer* & pSurface )=0;
    CM_RT_API virtual INT CreateSurface2D(UINT width, UINT height, CM_SURFACE_FORMAT format, CmSurface2D* & pSurface ) = 0;
    CM_RT_API virtual INT CreateSurface3D(UINT width, UINT height, UINT depth, CM_SURFACE_FORMAT format, CmSurface3D* & pSurface ) = 0;

#ifdef CM_DX9
    CM_RT_API virtual INT CreateSurface2D( IDirect3DSurface9* pD3DSurf, CmSurface2D* & pSurface ) = 0;
    CM_RT_API virtual INT CreateSurface2D( IDirect3DSurface9** pD3DSurf, const UINT surfaceCount, CmSurface2D**  pSpurface )= 0;
#elif defined(CM_DX11)
    CM_RT_API virtual INT CreateSurface2D( ID3D11Texture2D* pD3DSurf, CmSurface2D* & pSurface ) = 0;
    CM_RT_API virtual INT CreateSurface2D( ID3D11Texture2D** pD3DSurf, const UINT surfaceCount, CmSurface2D**  pSpurface )= 0;
#elif defined (__GNUC__)
    CM_RT_API virtual INT CreateSurface2D( VASurfaceID  iVASurface, CmSurface2D* & pSurface ) = 0;
    CM_RT_API virtual INT CreateSurface2D( VASurfaceID* iVASurface, const UINT surfaceCount, CmSurface2D** pSurface ) = 0;
#endif
    
    CM_RT_API virtual INT DestroySurface( CmBuffer* & pSurface) = 0;
    CM_RT_API virtual INT DestroySurface( CmSurface2D* & pSurface) = 0;
    CM_RT_API virtual INT DestroySurface( CmSurface3D* & pSurface) = 0;
  
    CM_RT_API virtual INT CreateQueue( CmQueue* & pQueue) = 0; 
    CM_RT_API virtual INT LoadProgram( void* pCommonISACode, const UINT size, CmProgram*& pProgram, const char* options = NULL ) = 0;
    CM_RT_API virtual INT CreateKernel( CmProgram* pProgram, const char* kernelName, CmKernel* & pKernel, const char* options = NULL) = 0;
    CM_RT_API virtual INT CreateKernel( CmProgram* pProgram, const char* kernelName, const void * fncPnt, CmKernel* & pKernel, const char* options = NULL) = 0;
    CM_RT_API virtual INT CreateSampler( const CM_SAMPLER_STATE & sampleState, CmSampler* & pSampler ) = 0;

    CM_RT_API virtual INT DestroyKernel( CmKernel*& pKernel) = 0;
    CM_RT_API virtual INT DestroySampler( CmSampler*& pSampler ) = 0;
    CM_RT_API virtual INT DestroyProgram( CmProgram*& pProgram ) = 0;
    CM_RT_API virtual INT DestroyThreadSpace( CmThreadSpace* & pTS ) = 0; 

    CM_RT_API virtual INT CreateTask(CmTask *& pTask)=0;
    CM_RT_API virtual INT DestroyTask(CmTask*& pTask)=0;

    CM_RT_API virtual INT GetCaps(CM_DEVICE_CAP_NAME capName, size_t& capValueSize, void* pCapValue ) = 0;

    CM_RT_API virtual INT CreateVmeStateG6( const VME_STATE_G6 & vmeState, CmVmeState* & pVmeState ) = 0;
    CM_RT_API virtual INT DestroyVmeStateG6( CmVmeState*& pVmeState ) = 0;

    CM_RT_API virtual INT CreateVmeSurfaceG6( CmSurface2D* pCurSurface, CmSurface2D* pForwardSurface, CmSurface2D* pBackwardSurface, SurfaceIndex* & pVmeIndex ) = 0;
    CM_RT_API virtual INT DestroyVmeSurfaceG6( SurfaceIndex* & pVmeIndex ) = 0;

    CM_RT_API virtual INT CreateThreadSpace( UINT width, UINT height, CmThreadSpace* & pTS ) = 0;

    CM_RT_API virtual INT CreateBufferUP(UINT size, void* pSystMem, CmBufferUP* & pSurface )=0;
    CM_RT_API virtual INT DestroyBufferUP( CmBufferUP* & pSurface) = 0;

    CM_RT_API virtual INT GetSurface2DInfo( UINT width, UINT height, CM_SURFACE_FORMAT format, UINT & pitch, UINT & physicalSize)= 0;
    CM_RT_API virtual INT CreateSurface2DUP( UINT width, UINT height, CM_SURFACE_FORMAT format, void* pSysMem, CmSurface2DUP* & pSurface )= 0;
    CM_RT_API virtual INT DestroySurface2DUP( CmSurface2DUP* & pSurface) = 0;

    CM_RT_API virtual INT CreateVmeSurfaceG7_5 ( CmSurface2D* pCurSurface, CmSurface2D** pForwardSurface, CmSurface2D** pBackwardSurface, const UINT surfaceCountForward, const UINT surfaceCountBackward, SurfaceIndex* & pVmeIndex )=0;
    CM_RT_API virtual INT DestroyVmeSurfaceG7_5( SurfaceIndex* & pVmeIndex ) = 0;
    CM_RT_API virtual INT CreateSampler8x8(const CM_SAMPLER_8X8_DESCR  & smplDescr, CmSampler8x8*& psmplrState)=0;
    CM_RT_API virtual INT DestroySampler8x8( CmSampler8x8*& pSampler )=0;
    CM_RT_API virtual INT CreateSampler8x8Surface(CmSurface2D* p2DSurface, SurfaceIndex* & pDIIndex, CM_SAMPLER8x8_SURFACE surf_type = CM_VA_SURFACE, CM_SURFACE_ADDRESS_CONTROL_MODE = CM_SURFACE_CLAMP )=0;
    CM_RT_API virtual INT DestroySampler8x8Surface(SurfaceIndex* & pDIIndex)=0;

    CM_RT_API virtual INT CreateThreadGroupSpace( UINT thrdSpaceWidth, UINT thrdSpaceHeight, UINT grpSpaceWidth, UINT grpSpaceHeight, CmThreadGroupSpace*& pTGS ) = 0;
    CM_RT_API virtual INT DestroyThreadGroupSpace(CmThreadGroupSpace*& pTGS) = 0;
    CM_RT_API virtual INT SetL3Config( L3_CONFIG_REGISTER_VALUES *l3_c) = 0;
    CM_RT_API virtual INT SetSuggestedL3Config( L3_SUGGEST_CONFIG l3_s_c) = 0;

    CM_RT_API virtual INT SetCaps(CM_DEVICE_CAP_NAME capName, size_t capValueSize, void* pCapValue) = 0;

    CM_RT_API virtual INT CreateGroupedVAPlusSurface(CmSurface2D* p2DSurface1, CmSurface2D* p2DSurface2, SurfaceIndex* & pDIIndex, CM_SURFACE_ADDRESS_CONTROL_MODE = CM_SURFACE_CLAMP)=0;
    CM_RT_API virtual INT DestroyGroupedVAPlusSurface(SurfaceIndex* & pDIIndex)=0;

#ifdef CM_DX11
    CM_RT_API virtual INT GetD3D11Device(ID3D11Device* &pD3D11Device) = 0;
#endif

    CM_RT_API virtual INT CreateSamplerSurface2D(CmSurface2D* p2DSurface, SurfaceIndex* & pSamplerSurfaceIndex) = 0;
    CM_RT_API virtual INT CreateSamplerSurface3D(CmSurface3D* p3DSurface, SurfaceIndex* & pSamplerSurfaceIndex) = 0;
    CM_RT_API virtual INT DestroySamplerSurface(SurfaceIndex* & pSamplerSurfaceIndex) = 0;
    CM_RT_API virtual INT GetRTDllVersion(CM_DLL_FILE_VERSION* pFileVersion) = 0;
    CM_RT_API virtual INT GetJITDllVersion(CM_DLL_FILE_VERSION* pFileVersion) = 0;

    CM_RT_API virtual INT InitPrintBuffer(size_t printbufsize = 1048576) = 0; 
    CM_RT_API virtual INT FlushPrintBuffer() = 0;

#ifdef CM_DX11
    CM_RT_API virtual INT CreateSurface2DSubresource( ID3D11Texture2D* pD3D11Texture2D, UINT subresourceCount, CmSurface2D** ppSurfaces, UINT& createdSurfaceCount, UINT option = 0 ) = 0;
    CM_RT_API virtual INT CreateSurface2DbySubresourceIndex( ID3D11Texture2D* pD3D11Texture2D, UINT FirstArraySlice, UINT FirstMipSlice, CmSurface2D* &pSurface) = 0;
#endif
};

typedef void (*IMG_WALKER_FUNTYPE)(void* img, void* arg);

EXTERN_C CM_RT_API void  CMRT_SetHwDebugStatus(bool bInStatus);
EXTERN_C CM_RT_API bool CMRT_GetHwDebugStatus();
EXTERN_C CM_RT_API INT CMRT_GetSurfaceDetails(CmEvent* pEvent, UINT kernIndex, UINT surfBTI, CM_SURFACE_DETAILS& outDetails);
EXTERN_C CM_RT_API void CMRT_PrepareGTPinBuffers(void* ptr0, int size0InBytes, void* ptr1, int size1InBytes, void* ptr2, int size2InBytes);
EXTERN_C CM_RT_API void CMRT_SetGTPinArguments(char* commandLine, void* gtpinInvokeStruct);
EXTERN_C CM_RT_API void CMRT_EnableGTPinMarkers(void);

EXTERN_C CM_RT_API INT DestroyCmDevice(CmDevice* &pD);

#define CM_CALLBACK __cdecl
typedef void (CM_CALLBACK *callback_function)(CmEvent*, void *);

EXTERN_C CM_RT_API UINT CMRT_GetKernelCount(CmEvent *pEvent);
EXTERN_C CM_RT_API INT CMRT_GetKernelName(CmEvent *pEvent, UINT index, char** KernelName);
EXTERN_C CM_RT_API INT CMRT_GetKernelThreadSpace(CmEvent *pEvent, UINT index, UINT* localWidth, UINT* localHeight, UINT* globalWidth, UINT* globalHeight);
EXTERN_C CM_RT_API INT CMRT_GetSubmitTime(CmEvent *pEvent, LARGE_INTEGER* time);
EXTERN_C CM_RT_API INT CMRT_GetHWStartTime(CmEvent *pEvent, LARGE_INTEGER* time);
EXTERN_C CM_RT_API INT CMRT_GetHWEndTime(CmEvent *pEvent, LARGE_INTEGER* time);
EXTERN_C CM_RT_API INT CMRT_GetCompleteTime(CmEvent *pEvent, LARGE_INTEGER* time);
EXTERN_C CM_RT_API INT CMRT_SetEventCallback(CmEvent* pEvent, callback_function function, void* user_data);
EXTERN_C CM_RT_API INT CMRT_Enqueue(CmQueue* pQueue, CmTask* pTask, CmEvent** pEvent, const CmThreadSpace* pTS = NULL);

#ifdef CM_DX9
    EXTERN_C CM_RT_API INT CreateCmDevice(CmDevice* &pD, UINT& version, IDirect3DDeviceManager9* pD3DDeviceMgr = NULL );
    EXTERN_C CM_RT_API INT CreateCmDeviceEx(CmDevice* &pD, UINT & version, IDirect3DDeviceManager9* pD3DDeviceMgr, UINT  DevCreateOption = CM_DEVICE_CREATE_OPTION_DEFAULT);
    EXTERN_C CM_RT_API INT CreateCmDeviceEmu(CmDevice* &pDevice, UINT& version, IDirect3DDeviceManager9* pD3DDeviceMgr = NULL );
    EXTERN_C CM_RT_API INT DestroyCmDeviceEmu(CmDevice* &pDevice, IDirect3DDeviceManager9* pD3DDeviceMgr = NULL );
    EXTERN_C CM_RT_API INT CreateCmDeviceSim(CmDevice* &pDevice, UINT& version, IDirect3DDeviceManager9* pD3DDeviceMgr = NULL );
    EXTERN_C CM_RT_API INT DestroyCmDeviceSim(CmDevice* &pDevice, IDirect3DDeviceManager9* pD3DDeviceMgr = NULL );
#elif defined(CM_DX11)
    EXTERN_C CM_RT_API INT CreateCmDevice(CmDevice* &pD, UINT& version, ID3D11Device* pD3D11Device = NULL);
    EXTERN_C CM_RT_API INT CreateCmDeviceEx(CmDevice* &pD, UINT & version, ID3D11Device* pD3D11Device, UINT  DevCreateOption = CM_DEVICE_CREATE_OPTION_DEFAULT);
    EXTERN_C CM_RT_API INT CreateCmDeviceEmu(CmDevice* &pDevice, UINT& version, ID3D11Device* pD3DDeviceMgr = NULL );
    EXTERN_C CM_RT_API INT DestroyCmDeviceEmu(CmDevice* &pDevice, ID3D11Device* pD3DDeviceMgr = NULL );
    EXTERN_C CM_RT_API INT CreateCmDeviceSim(CmDevice* &pDevice, UINT& version, ID3D11Device* pD3DDeviceMgr = NULL );
    EXTERN_C CM_RT_API INT DestroyCmDeviceSim(CmDevice* &pDevice, ID3D11Device* pD3DDeviceMgr = NULL );
#elif defined(__GNUC__)
    EXTERN_C CM_RT_API INT CreateCmDevice(CmDevice* &pD, UINT& version, VADisplay va_dpy = NULL);
    EXTERN_C CM_RT_API INT CreateCmDeviceEx(CmDevice* &pD, UINT & version, VADisplay va_dpy, UINT  DevCreateOption = CM_DEVICE_CREATE_OPTION_DEFAULT);
    EXTERN_C CM_RT_API INT CreateCmDeviceEmu(CmDevice* &pDevice, UINT& version, VADisplay va_dpy = NULL);
    EXTERN_C CM_RT_API INT DestroyCmDeviceEmu(CmDevice* &pDevice);
    EXTERN_C CM_RT_API INT CreateCmDeviceSim(CmDevice* &pDevice, UINT& version);
    EXTERN_C CM_RT_API INT DestroyCmDeviceSim(CmDevice* &pDevice);
#endif

#ifdef CMRT_EMU
    #define CreateCmDevice CreateCmDeviceEmu
    #define DestroyCmDevice DestroyCmDeviceEmu
#elif CMRT_SIM
    #define CreateCmDevice CreateCmDeviceSim
    #define DestroyCmDevice DestroyCmDeviceSim
#endif
