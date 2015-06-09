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

#ifndef JDLIBVA_H
#define JDLIBVA_H

#include <pthread.h>
#include <stdio.h>
#include "jpeglib.h"

#define Display unsigned int
#define BOOL int

#define JPEG_MAX_COMPONENTS 4
#define JPEG_MAX_QUANT_TABLES 4

typedef struct {
    const uint8_t* bitstream_buf;
    uint32_t image_width;
    uint32_t image_height;

    boolean hw_state_ready;
    boolean hw_caps_ready;
    boolean hw_path;
    boolean initialized;
    boolean resource_allocated;
    boolean require_image;

    uint32_t file_size;
    uint32_t rotation;
    int      tile_mode;
    huffman_index *huff_index;

    uint32_t cap_available;
    uint32_t cap_enabled;

    unsigned long priv;
} jd_libva_struct;

typedef enum {
    DECODE_NOT_STARTED = -7,
    DECODE_INVALID_DATA = -6,
    DECODE_DRIVER_FAIL = -5,
    DECODE_PARSER_FAIL = -4,
    DECODE_PARSER_INSUFFICIENT_BYTES = -3,
    DECODE_MEMORY_FAIL = -2,
    DECODE_FAIL = -1,
    DECODE_SUCCESS = 0,

} IMAGE_DECODE_STATUS;

#define JPEG_CAPABILITY_DECODE     0x0
#define JPEG_CAPABILITY_UPSAMPLE   0x1
#define JPEG_CAPABILITY_DOWNSCALE  0x2

/*********************** for libjpeg ****************************/
typedef int32_t Decode_Status;
extern jd_libva_struct jd_libva;
#ifdef __cplusplus
extern "C" {
#endif
Decode_Status jdva_initialize (jd_libva_struct * jd_libva_ptr);
void jdva_deinitialize (jd_libva_struct * jd_libva_ptr);
Decode_Status jdva_fill_input(j_decompress_ptr cinfo, jd_libva_struct * jd_libva_ptr);
void jdva_drain_input(j_decompress_ptr cinfo, jd_libva_struct * jd_libva_ptr);
Decode_Status jdva_decode (j_decompress_ptr cinfo, jd_libva_struct * jd_libva_ptr);
Decode_Status jdva_read_scanlines (j_decompress_ptr cinfo, jd_libva_struct * jd_libva_ptr, char ** scanlines, unsigned int* row_ctr, unsigned int max_lines);
Decode_Status jdva_init_read_tile_scanline(j_decompress_ptr cinfo, jd_libva_struct * jd_libva_ptr, int *x, int *y, int *w, int *h);
Decode_Status jdva_read_tile_scanline (j_decompress_ptr cinfo, jd_libva_struct * jd_libva_ptr, char ** scanlines, unsigned int* row_ctr);
Decode_Status jdva_create_resource (jd_libva_struct * jd_libva_ptr);
Decode_Status jdva_release_resource (jd_libva_struct * jd_libva_ptr);
Decode_Status jdva_parse_bitstream(j_decompress_ptr cinfo, jd_libva_struct * jd_libva_ptr);
void jdva_return_filled_bytes(j_decompress_ptr cinfo, jd_libva_struct * jd_libva_ptr);

#ifdef __cplusplus
}
#endif


#endif

