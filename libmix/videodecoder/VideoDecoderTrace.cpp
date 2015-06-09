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


#include "VideoDecoderTrace.h"

#ifdef ENABLE_VIDEO_DECODER_TRACE

void TraceVideoDecoder(const char* cat, const char* fun, int line, const char* format, ...)
{
    if (NULL == cat || NULL == fun || NULL == format)
        return;

    printf("%s %s(#%d): ", cat, fun, line);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

#ifdef DUMP_INPUT_BUFFER
static void mem_put_le32(char *mem, unsigned int val) {
    mem[0] = val;
    mem[1] = val >> 8;
    mem[2] = val >> 16;
    mem[3] = val >> 24;
}

void DumpInputBuffer(VideoDecodeBuffer *buffer, const char * codec) {
    FILE *fp_out;
    char filename[20] = "/data/input.";
    strcat(filename, codec);
    char header[4];
    if((fp_out = fopen(filename,"a+")) == NULL) {
        ETRACE("Fail to open outputfile: %s  continue to decode...", filename);
    } else {
        if (strncmp(codec,"vp8", 3) == 0) {
           // vp8 need add frame size(4 bytes) at the begin of each frame
           // vega can idetify this kind of vp8 es format
            mem_put_le32(header, buffer->size);
            fwrite(header, 1, 4, fp_out);
        }

        /* write frame data */
        fwrite(buffer->data, 1, buffer->size, fp_out);
        fclose(fp_out);
    }
}
#endif

#endif

