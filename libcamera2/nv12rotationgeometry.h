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
 *
 * nv12rotationgeometry.h
 * This file contains the list of NV12 image resolution and stride sets
 * that are supported for optimized rotation.
 */
#if defined(NV12_ROTATION_STRIDES)
// NV_ROTATION_STRIDES defines the stride and macroblock size combinations that
// are to be used:
// - RSTRIDE: reading stride; scanline width (including any padding) in pixels
// - WSTRIDE: writing stride; same as above, but for the rotated image
// - MACROBLOCK: side length of the macroblock square used to rotate images
//               in pixels, must be a multiple of 16
//                   (RSTRIDE, WSTRIDE, MACROBLOCK)
NV12_ROTATION_STRIDES( 512,    512,     32        )
NV12_ROTATION_STRIDES( 640,    512,     32        )
NV12_ROTATION_STRIDES(1024,    576,     32        )
NV12_ROTATION_STRIDES( 832,    640,     32        )
NV12_ROTATION_STRIDES( 768,    512,     32        )
NV12_ROTATION_STRIDES( 960,    768,     32        )
NV12_ROTATION_STRIDES( 800,    640,     32        )
NV12_ROTATION_STRIDES( 1280,   768,     32        )
NV12_ROTATION_STRIDES( 1920,  1088,     32        )
#undef NV12_ROTATION_STRIDES
#endif

#if defined(NV12_ROTATION_GEOMETRY)
// NV12_ROTATION_GEOMETRY defines the following attributes of an NV12 image:
// - COLUMNNS: width in pixels
// - ROWS: height pixels
// - RSTRIDE: reading stride; scanline width (including any padding) in pixels
// - WSTRIDE: writing stride; same as above, but for the rotated image
// - MACROBLOCK: side length of the macroblock square used to rotate images
//               in pixels, must be a multiple of 16
// Each NV12_ROTATION_GEOMETRY entry causes optimized rotation code to be
// generated for the defined geometry
//
//                    (COLUMNS, ROWS, RSTRIDE, WSTRIDE, MACROBLOCK)
NV12_ROTATION_GEOMETRY( 352,    288,   512,      512,     32       )
NV12_ROTATION_GEOMETRY( 720,    480,   768,      512,     32       )
NV12_ROTATION_GEOMETRY( 640,    480,   640,      512,     32       )
NV12_ROTATION_GEOMETRY( 640,    360,   640,      512,     32       )
NV12_ROTATION_GEOMETRY( 800,    600,   832,      640,     32       )
NV12_ROTATION_GEOMETRY( 800,    600,   800,      640,     32       )
NV12_ROTATION_GEOMETRY( 960,    720,   960,      768,     32       )
NV12_ROTATION_GEOMETRY(1024,    576,  1024,      576,     32       )
NV12_ROTATION_GEOMETRY(1280,    720,  1280,      768,     32       )
NV12_ROTATION_GEOMETRY(1920,   1080,  1920,     1088,     32       )
#undef NV12_ROTATION_GEOMETRY
#endif
