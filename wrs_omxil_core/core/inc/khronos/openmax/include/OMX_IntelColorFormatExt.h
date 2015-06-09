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

#ifndef OMX_IntelColorFormatExt_h
#define OMX_IntelColorFormatExt_h

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <OMX_IVCommon.h>

/**
 * Enumeration defining intel possible uncompressed image/video formats
 */
typedef enum OMX_INTEL_COLOR_FORMATTYPE {
    OMX_INTEL_COLOR_FormatYUV420PackedSemiPlanar = 0x7FA00E00,
    OMX_INTEL_COLOR_FormatYUV420PackedSemiPlanar_Tiled = 0x7FA00F00,
    OMX_INTEL_COLOR_FormatHalYV12 = 0x32315659
} OMX_INTEL_COLOR_FORMATTYPE;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* OMX_IndexExt_h */
/* File EOF */
