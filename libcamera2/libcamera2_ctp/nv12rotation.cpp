/*
 * Copyright (c) 2012 Intel Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
// TODO:
// 1. add macroblock support for UV
// 2. test optimal reading direction for top & right when (ROWS|COLUMNS)%MACROBLOCK
#include "nv12rotation.h"
#include "AtomCommon.h"
#include <cstdlib>

#define STRIZE2(s) #s
#define STRIZE(s) STRIZE2(s)


bool genericRotateBy90(const int   width,
                        const int   height,
                        const int   rstride,
                        const int   wstride,
                        const char* sptr,
                        char*       dptr);

#define NV12_ROTATION_STRIDES(RSTRIDE, WSTRIDE, MACROBLOCK) \
static void rotateXx4x##RSTRIDE##x##WSTRIDE##Y \
    (const unsigned char* source, unsigned char* target, int columns) \
    __attribute__ ((noinline)); \
static void rotateXx4x##RSTRIDE##x##WSTRIDE##Y \
    (const unsigned char* source, unsigned char* target, int columns) \
{ \
    const unsigned char* terminator = source + columns; \
 \
    asm /*volatile*/ ( "\
                    mov %0, %%esi;            \
                    mov %1, %%edi;            \
                    mov %2, %%ecx;            \
                    .align 16;                \
                    1:                        \
                    movdqu                    (%%esi), %%xmm0; \
                    movdqu "STRIZE(  RSTRIDE)"(%%esi), %%xmm1; \
                    movdqu "STRIZE(2*RSTRIDE)"(%%esi), %%xmm2; \
                    movdqu "STRIZE(3*RSTRIDE)"(%%esi), %%xmm3; \
                                              \
                    prefetchnta                     64(%%esi);  \
                    prefetchnta "STRIZE(  RSTRIDE+64)"(%%esi);  \
                    prefetchnta "STRIZE(2*RSTRIDE+64)"(%%esi);  \
                    prefetchnta "STRIZE(3*RSTRIDE+64)"(%%esi);  \
                                              \
                    add $16, %%esi;           \
                                              \
                                              \
                    movdqa %%xmm1, %%xmm4;    \
                    movdqa %%xmm3, %%xmm5;    \
                    punpcklbw %%xmm0, %%xmm4; \
                    punpcklbw %%xmm2, %%xmm5; \
                                              \
                    movdqa %%xmm1, %%xmm6;    \
                    movdqa %%xmm3, %%xmm7;    \
                    punpckhbw %%xmm0, %%xmm6; \
                    punpckhbw %%xmm2, %%xmm7; \
                                              \
                    movdqa %%xmm5, %%xmm0;    \
                    punpckhwd %%xmm4, %%xmm5; \
                    punpcklwd %%xmm4, %%xmm0; \
                                              \
                    movdqa %%xmm7, %%xmm2;    \
                    punpckhwd %%xmm6, %%xmm7; \
                    punpcklwd %%xmm6, %%xmm2; \
                                              \
                                              \
                    movd %%xmm0,                     (%%edi); \
                    movd %%xmm5, "STRIZE( 4*WSTRIDE)"(%%edi); \
                    movd %%xmm2, "STRIZE( 8*WSTRIDE)"(%%edi); \
                    movd %%xmm7, "STRIZE(12*WSTRIDE)"(%%edi); \
                    psrldq $4, %%xmm0;        \
                    psrldq $4, %%xmm5;        \
                    psrldq $4, %%xmm2;        \
                    psrldq $4, %%xmm7;        \
                    movd %%xmm0, "STRIZE(   WSTRIDE)"(%%edi); \
                    movd %%xmm5, "STRIZE( 5*WSTRIDE)"(%%edi); \
                    movd %%xmm2, "STRIZE( 9*WSTRIDE)"(%%edi); \
                    movd %%xmm7, "STRIZE(13*WSTRIDE)"(%%edi); \
                    psrldq $4, %%xmm0;        \
                    psrldq $4, %%xmm5;        \
                    psrldq $4, %%xmm2;        \
                    psrldq $4, %%xmm7;        \
                    movd %%xmm0, "STRIZE( 2*WSTRIDE)"(%%edi); \
                    movd %%xmm5, "STRIZE( 6*WSTRIDE)"(%%edi); \
                    movd %%xmm2, "STRIZE(10*WSTRIDE)"(%%edi); \
                    movd %%xmm7, "STRIZE(14*WSTRIDE)"(%%edi); \
                    psrldq $4, %%xmm0;        \
                    psrldq $4, %%xmm5;        \
                    psrldq $4, %%xmm2;        \
                    psrldq $4, %%xmm7;        \
                    movd %%xmm0, "STRIZE( 3*WSTRIDE)"(%%edi); \
                    movd %%xmm5, "STRIZE( 7*WSTRIDE)"(%%edi); \
                    movd %%xmm2, "STRIZE(11*WSTRIDE)"(%%edi); \
                    movd %%xmm7, "STRIZE(15*WSTRIDE)"(%%edi); \
                                              \
                                              \
                    cmp %%esi, %%ecx;         \
                    lea "STRIZE(16*WSTRIDE)"(%%edi), %%edi;   \
                                              \
                    jne 1b;                   \
                    " \
                 : \
                 : "g"(source), "g"(target), "m"(terminator) \
                 : "%ecx", "%esi", "%edi", "memory" \
                 ); \
}
#include "nv12rotationgeometry.h"


#define NV12_ROTATION_STRIDES(RSTRIDE, WSTRIDE, MACROBLOCK) \
static void rotateXx4x##RSTRIDE##x##WSTRIDE##UV \
    (const unsigned char* source, unsigned char* target) \
    __attribute__ ((noinline)); \
static void rotateXx4x##RSTRIDE##x##WSTRIDE##UV \
    (const unsigned char* source, unsigned char* target, size_t columns) \
{ \
    const unsigned char* terminator = source + columns; \
 \
    asm /*volatile*/ ( "\
                    mov %0, %%esi;            \
                    mov %1, %%edi;            \
                    mov %2, %%ecx;            \
                                              \
                    .align 16;                \
                    1:                        \
                    movdqu    (%%esi), %%xmm0; \
                    movdqu " STRIZE(RSTRIDE) "(%%esi), %%xmm1; \
                                              \
                    prefetchnta 64(%%esi);   \
                    prefetchnta "STRIZE(RSTRIDE+64)"(%%esi);   \
                                              \
                    add $16, %%esi;           \
                                              \
                    movdqa %%xmm0, %%xmm3;    \
                    movdqa %%xmm1, %%xmm2;    \
                                              \
                    punpcklwd %%xmm0, %%xmm1; \
                    punpckhwd %%xmm3, %%xmm2; \
                    movd %%xmm1,                    (%%edi); \
                    movd %%xmm2, "STRIZE(4*WSTRIDE)"(%%edi); \
                                              \
                    psrldq $4, %%xmm1;        \
                    psrldq $4, %%xmm2;        \
                    movd %%xmm1, "STRIZE(  WSTRIDE)"(%%edi); \
                    movd %%xmm2, "STRIZE(5*WSTRIDE)"(%%edi); \
                                              \
                    psrldq $4, %%xmm1;        \
                    psrldq $4, %%xmm2;        \
                    movd %%xmm1, "STRIZE(2*WSTRIDE)"(%%edi); \
                    movd %%xmm2, "STRIZE(6*WSTRIDE)"(%%edi); \
                                              \
                    psrldq $4, %%xmm1;        \
                    psrldq $4, %%xmm2;        \
                    movd %%xmm1, "STRIZE(3*WSTRIDE)"(%%edi); \
                    movd %%xmm2, "STRIZE(7*WSTRIDE)"(%%edi); \
                                              \
                    cmp %%esi, %%ecx;         \
                    lea "STRIZE(8*WSTRIDE)"(%%edi), %%edi;   \
                                              \
                    jne 1b;                   \
                    " \
                 : \
                 : "g"(source), "g"(target), "m"(terminator) \
                 : "%ecx", "%esi", "%edi", "memory" \
                 ); \
}
#include "nv12rotationgeometry.h"


#define NV12_ROTATION_STRIDES(RSTRIDE, WSTRIDE, MACROBLOCK) \
static void rotate##MACROBLOCK##x4x##RSTRIDE##x##WSTRIDE##Y \
    (const unsigned char* source, unsigned char* target) \
    __attribute__ ((noinline)); \
static void rotate##MACROBLOCK##x4x##RSTRIDE##x##WSTRIDE##Y \
    (const unsigned char* source, unsigned char* target) \
{ \
    const unsigned char* terminator = source + MACROBLOCK; \
 \
    asm /*volatile*/ ( "\
                    mov %0, %%esi;            \
                    mov %1, %%edi;            \
                    mov %2, %%ecx;            \
                    .align 16;                \
                    1:                        \
                    movdqu                    (%%esi), %%xmm0; \
                    movdqu "STRIZE(  RSTRIDE)"(%%esi), %%xmm1; \
                    movdqu "STRIZE(2*RSTRIDE)"(%%esi), %%xmm2; \
                    movdqu "STRIZE(3*RSTRIDE)"(%%esi), %%xmm3; \
                                              \
                    prefetchnta                     64(%%esi);  \
                    prefetchnta "STRIZE(  RSTRIDE+64)"(%%esi);  \
                    prefetchnta "STRIZE(2*RSTRIDE+64)"(%%esi);  \
                    prefetchnta "STRIZE(3*RSTRIDE+64)"(%%esi);  \
                                              \
                    add $16, %%esi;           \
                                              \
                                              \
                    movdqa %%xmm1, %%xmm4;    \
                    movdqa %%xmm3, %%xmm5;    \
                    punpcklbw %%xmm0, %%xmm4; \
                    punpcklbw %%xmm2, %%xmm5; \
                                              \
                    movdqa %%xmm1, %%xmm6;    \
                    movdqa %%xmm3, %%xmm7;    \
                    punpckhbw %%xmm0, %%xmm6; \
                    punpckhbw %%xmm2, %%xmm7; \
                                              \
                    movdqa %%xmm5, %%xmm0;    \
                    punpckhwd %%xmm4, %%xmm5; \
                    punpcklwd %%xmm4, %%xmm0; \
                                              \
                    movdqa %%xmm7, %%xmm2;    \
                    punpckhwd %%xmm6, %%xmm7; \
                    punpcklwd %%xmm6, %%xmm2; \
                                              \
                                              \
                    movd %%xmm0,                     (%%edi); \
                    movd %%xmm5, "STRIZE( 4*WSTRIDE)"(%%edi); \
                    movd %%xmm2, "STRIZE( 8*WSTRIDE)"(%%edi); \
                    movd %%xmm7, "STRIZE(12*WSTRIDE)"(%%edi); \
                    psrldq $4, %%xmm0;        \
                    psrldq $4, %%xmm5;        \
                    psrldq $4, %%xmm2;        \
                    psrldq $4, %%xmm7;        \
                    movd %%xmm0, "STRIZE(   WSTRIDE)"(%%edi); \
                    movd %%xmm5, "STRIZE( 5*WSTRIDE)"(%%edi); \
                    movd %%xmm2, "STRIZE( 9*WSTRIDE)"(%%edi); \
                    movd %%xmm7, "STRIZE(13*WSTRIDE)"(%%edi); \
                    psrldq $4, %%xmm0;        \
                    psrldq $4, %%xmm5;        \
                    psrldq $4, %%xmm2;        \
                    psrldq $4, %%xmm7;        \
                    movd %%xmm0, "STRIZE( 2*WSTRIDE)"(%%edi); \
                    movd %%xmm5, "STRIZE( 6*WSTRIDE)"(%%edi); \
                    movd %%xmm2, "STRIZE(10*WSTRIDE)"(%%edi); \
                    movd %%xmm7, "STRIZE(14*WSTRIDE)"(%%edi); \
                    psrldq $4, %%xmm0;        \
                    psrldq $4, %%xmm5;        \
                    psrldq $4, %%xmm2;        \
                    psrldq $4, %%xmm7;        \
                    movd %%xmm0, "STRIZE( 3*WSTRIDE)"(%%edi); \
                    movd %%xmm5, "STRIZE( 7*WSTRIDE)"(%%edi); \
                    movd %%xmm2, "STRIZE(11*WSTRIDE)"(%%edi); \
                    movd %%xmm7, "STRIZE(15*WSTRIDE)"(%%edi); \
                                              \
                                              \
                    cmp %%esi, %%ecx;         \
                    lea "STRIZE(16*WSTRIDE)"(%%edi), %%edi;   \
                                              \
                    jne 1b;                   \
                    "\
                 :\
                 : "g"(source), "g"(target), "m"(terminator)\
                 : "%ecx", "%esi", "%edi", "memory"\
                 );\
}
#include "nv12rotationgeometry.h"


#define NV12_ROTATION_STRIDES(RSTRIDE, WSTRIDE, MACROBLOCK) \
static void rotate##MACROBLOCK##x##MACROBLOCK##x##RSTRIDE##x##WSTRIDE##Y \
    (const unsigned char* source, unsigned char* target) \
{ \
    const unsigned char* s = source + (MACROBLOCK - 4) * RSTRIDE; \
    unsigned char*       t = target; \
 \
    for (int slices = MACROBLOCK / 4; slices > 0; --slices) { \
        rotate##MACROBLOCK##x4x##RSTRIDE##x##WSTRIDE##Y(s, t); \
        s -= 4 * RSTRIDE; \
        t += 4; \
    } \
}
#include "nv12rotationgeometry.h"


#define NV12_ROTATION_GEOMETRY(COLUMNS, ROWS, RSTRIDE, WSTRIDE, MACROBLOCK) \
static void rotate##COLUMNS##x##ROWS##x##RSTRIDE##x##WSTRIDE##x##MACROBLOCK \
    (const unsigned char* source, unsigned char* target) \
{ \
    const unsigned char* s; \
    unsigned char*       t; \
 \
    for (int row = 0; row < ROWS/MACROBLOCK; ++row) { \
        s = source + ROWS * RSTRIDE - ((row + 1) * MACROBLOCK * RSTRIDE); \
        t = target + row * MACROBLOCK; \
        for (int col = 0; col < COLUMNS/MACROBLOCK; ++col) { \
            rotate##MACROBLOCK##x##MACROBLOCK##x##RSTRIDE##x##WSTRIDE##Y(s, t); \
            s += MACROBLOCK; \
            t += MACROBLOCK * WSTRIDE; \
        } \
    } \
 \
    if (ROWS % MACROBLOCK) { \
        s = source; \
        t = target + ROWS - 4; \
        \
        for (int row = 0; row < (ROWS % MACROBLOCK) / 4; ++row) { \
            rotateXx4x##RSTRIDE##x##WSTRIDE##Y(s, t, COLUMNS); \
            s += 4 * RSTRIDE; \
            t -= 4; \
        } \
    } \
 \
    if (COLUMNS % MACROBLOCK) { \
        s = source \
          + (ROWS % MACROBLOCK) * RSTRIDE + (COLUMNS - COLUMNS % MACROBLOCK); \
        t = target \
          + (COLUMNS - COLUMNS % MACROBLOCK) * WSTRIDE \
          + (ROWS - ROWS % MACROBLOCK - 4); \
        \
        for (int row = 0; row < (ROWS - ROWS % MACROBLOCK) / 4; ++row) { \
            rotateXx4x##RSTRIDE##x##WSTRIDE##Y(s, t, COLUMNS % MACROBLOCK); \
            s += 4 * RSTRIDE; \
            t -= 4; \
        } \
    } \
 \
    s = source + RSTRIDE * ROWS; \
    t = target + COLUMNS * WSTRIDE + (ROWS - 4); \
    for (int rows = ROWS / 4; rows > 0; --rows) { \
        rotateXx4x##RSTRIDE##x##WSTRIDE##UV(s, t, COLUMNS); \
        s += 4 * RSTRIDE / 2; \
        t -= 4; \
    } \
}
#include "nv12rotationgeometry.h"


bool nv12rotateBy90(const int   width,
                    const int   height,
                    const int   rstride,
                    const int   wstride,
                    const char* sptr,
                    char*       dptr)
{
    bool rotated;

#define NV12_ROTATION_GEOMETRY(COLUMNS, ROWS, RSTRIDE, WSTRIDE, MACROBLOCK) \
    if (width == COLUMNS && height == ROWS && rstride == RSTRIDE && wstride == WSTRIDE) { \
        rotate##COLUMNS##x##ROWS##x##RSTRIDE##x##WSTRIDE##x##MACROBLOCK \
            ((const unsigned char*)sptr, (unsigned char*)dptr); \
        rotated = true; \
    } else
#include "nv12rotationgeometry.h"
    {
         rotated = genericRotateBy90(width, height, rstride, wstride, sptr, dptr);
    }

    return rotated;
}

/**
 * Fall back rotation algorithm in case the resolution requested is not
 * optimized
 **/
bool genericRotateBy90(const int   width,
                        const int   height,
                        const int   rstride,
                        const int   wstride,
                        const char* sptr,
                        char*       dptr)
{
    int i,j;
    char* a = (char*) sptr;
    char* b = dptr;

    ALOGW("Unoptimized CPU rotation! "
         "Disable overlay or optimize for this resolution(%dx%d)",width, height);

    // Luma rotation
    for (i = 0; i < width; i++) {
        for (j = height-1; j >= 0 ; j--) {
            b[height - j -1] = a[i + j*rstride];
        }
        b += wstride;
    }

    a += rstride*height;

    //Chroma rotation
    for (i = 0; i < width; i+=2) {
        for (j = height; j > 0 ; j-=2) {
            b[height -j] = a[i + (j/2-1)*rstride]; //U
            b[height -j +1] = a[i+1 + (j/2 -1)*rstride]; //V


        }
        b = b + wstride;
    }
    return true;
}
