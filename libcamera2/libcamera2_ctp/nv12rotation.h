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
#ifndef NV12ROTATION_H
#define NV12ROTATION_H

// nv12rotateBy90() is an optimized 90 degree rotation for a select
// set of NV12 images. Since it can only rotate some image geometries,
// the return value indicates whether the rotation was done or not.
// Width, height, rstride and wstride parameters are in pixels.
bool nv12rotateBy90(const int   width,   // width of the source image
                    const int   height,  // height of the source image
                    const int   rstride, // scanline stride of the source image
                    const int   wstride, // scanline stride of the target image
                    const char* sptr,    // source image
                    char*       dptr);   // target image

#endif
