/*
 * Copyright (c) 2010 The Khronos Group Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

/** OMX_VideoExt.h - OpenMax IL version 1.1.2
 * The OMX_VideoExt header file contains extensions to the
 * definitions used by both the application and the component to
 * access video items.
 */

#ifndef OMX_VideoExt_h
#define OMX_VideoExt_h

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Each OMX header shall include all required header files to allow the
 * header to compile without errors.  The includes below are required
 * for this header file to compile successfully
 */
#include <OMX_Core.h>

/** NALU Formats */
typedef enum OMX_NALUFORMATSTYPE {
    OMX_NaluFormatStartCodes = 1,
    OMX_NaluFormatOneNaluPerBuffer = 2,
    OMX_NaluFormatOneByteInterleaveLength = 4,
    OMX_NaluFormatTwoByteInterleaveLength = 8,
    OMX_NaluFormatFourByteInterleaveLength = 16,
    OMX_NaluFormatZeroByteInterleaveLength = 32,
    OMX_NaluFormatStartCodesSeparateFirstHeader = 64,
    OMX_NaluFormatCodingMax = 0x7FFFFFFF
} OMX_NALUFORMATSTYPE;


/** NAL Stream Format */
typedef struct OMX_NALSTREAMFORMATTYPE{
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_NALUFORMATSTYPE eNaluFormat;
} OMX_NALSTREAMFORMATTYPE;

typedef struct OMX_VIDEO_PARAM_BYTESTREAMTYPE {
     OMX_U32 nSize;                 // Size of the structure
     OMX_VERSIONTYPE nVersion;      // OMX specification version
     OMX_U32 nPortIndex;            // Port that this structure applies to
     OMX_BOOL bBytestream;          // Enable/disable bytestream support
} OMX_VIDEO_PARAM_BYTESTREAMTYPE;

typedef struct OMX_VIDEO_CONFIG_INTEL_BITRATETYPE {
     OMX_U32 nSize;
     OMX_VERSIONTYPE nVersion;
     OMX_U32 nPortIndex;
     OMX_U32 nMaxEncodeBitrate;    // Maximum bitrate
     OMX_U32 nTargetPercentage;    // Target bitrate as percentage of maximum bitrate; e.g. 95 is 95%
     OMX_U32 nWindowSize;          // Window size in milliseconds allowed for bitrate to reach target
     OMX_U32 nInitialQP;           // Initial QP for I frames
     OMX_U32 nMinQP;
} OMX_VIDEO_CONFIG_INTEL_BITRATETYPE;

typedef enum OMX_VIDEO_INTEL_CONTROLRATETYPE {
    OMX_Video_Intel_ControlRateDisable,
    OMX_Video_Intel_ControlRateVariable,
    OMX_Video_Intel_ControlRateConstant,
    OMX_Video_Intel_ControlRateVideoConferencingMode,
    OMX_Video_Intel_ControlRateMax = 0x7FFFFFFF
} OMX_VIDEO_INTEL_CONTROLRATETYPE;

typedef struct OMX_VIDEO_PARAM_INTEL_BITRATETYPE {
     OMX_U32 nSize;
     OMX_VERSIONTYPE nVersion;
     OMX_U32 nPortIndex;
     OMX_VIDEO_INTEL_CONTROLRATETYPE eControlRate;
     OMX_U32 nTargetBitrate;
} OMX_VIDEO_PARAM_INTEL_BITRATETYPE;

typedef struct OMX_VIDEO_PARAM_INTEL_AVC_DECODE_SETTINGS {
     OMX_U32 nSize;                       // Size of the structure
     OMX_VERSIONTYPE nVersion;            // OMX specification version
     OMX_U32 nPortIndex;                  // Port that this structure applies to
     OMX_U32 nMaxNumberOfReferenceFrame;  // Maximum number of reference frames
     OMX_U32 nMaxWidth;                   // Maximum width of video
     OMX_U32 nMaxHeight;                  // Maximum height of video
} OMX_VIDEO_PARAM_INTEL_AVC_DECODE_SETTINGS;


typedef struct OMX_VIDEO_CONFIG_INTEL_SLICE_NUMBERS {
     OMX_U32 nSize;                       // Size of the structure
     OMX_VERSIONTYPE nVersion;            // OMX specification version
     OMX_U32 nPortIndex;                  // Port that this structure applies to
     OMX_U32 nISliceNumber;               // I frame slice number
     OMX_U32 nPSliceNumber;               // P frame slice number
} OMX_VIDEO_CONFIG_INTEL_SLICE_NUMBERS;


typedef struct OMX_VIDEO_CONFIG_INTEL_AIR {
     OMX_U32 nSize;                       // Size of the structure
     OMX_VERSIONTYPE nVersion;            // OMX specification version
     OMX_U32 nPortIndex;                  // Port that this structure applies to
     OMX_BOOL bAirEnable;                 // Enable AIR
     OMX_BOOL bAirAuto;                   // AIR auto
     OMX_U32 nAirMBs;                     // Number of AIR MBs
     OMX_U32 nAirThreshold;               // AIR Threshold

} OMX_VIDEO_CONFIG_INTEL_AIR;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* OMX_VideoExt_h */
/* File EOF */
