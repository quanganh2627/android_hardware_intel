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
#ifndef IMAGESCALER_H_
#define IMAGESCALER_H_

namespace android {

class AtomBuffer;

class ImageScaler {
public:
    static void downScaleImage(AtomBuffer *src, AtomBuffer *dst,
            int src_skip_lines_top = 0, int src_skip_lines_bottom = 0);
    static void downScaleImage(void *src, void *dest,
            int dest_w, int dest_h, int dest_bpl,
            int src_w, int src_h, int src_bpl,
            int fourcc, int src_skip_lines_top = 0,
            int src_skip_lines_bottom = 0);

    static void cropNV12orNV21Image(const AtomBuffer *src, AtomBuffer *dst,
                                    int leftCrop, int rightCrop, int topCrop, int bottomCrop);
    static void centerCropNV12orNV21Image(const AtomBuffer *src, AtomBuffer *dst);

protected:
    static void downScaleYUY2Image(unsigned char *dest, const unsigned char *src,
        const int dest_w, const int dest_h, const int src_w, const int src_h);

    static void downScaleAndCropNv12Image(
        unsigned char *dest, const unsigned char *src,
        const int dest_w, const int dest_h, const int dest_bpl,
        const int src_w, const int src_h, const int src_bpl,
        const int src_skip_lines_top = 0,
        const int src_skip_lines_bottom = 0);

    static void trimNv12Image(
        unsigned char *dest, const unsigned char *src,
        const int dest_w, const int dest_h, const int dest_bpl,
        const int src_w, const int src_h, const int src_bpl,
        const int src_skip_lines_top = 0,
        const int src_skip_lines_bottom = 0);

    static void downScaleAndCropNv12ImageQvga(
        unsigned char *dest, const unsigned char *src,
        const int dest_bpl, const int src_bpl);

    static void downScaleAndCropNv12ImageQcif(
        unsigned char *dest, const unsigned char *src,
        const int dest_bpl, const int src_bpl);

    static void downScaleNv12ImageFrom800x600ToQvga(
        unsigned char *dest, const unsigned char *src,
        const int dest_bpl, const int src_bpl);

};

};

#endif
