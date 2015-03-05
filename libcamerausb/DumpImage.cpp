/*
 * Copyright (C) 2011 The Android Open Source Project
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
#define LOG_TAG "Camera_ImageDump"

//#include <camera/CameraParameters.h>
//#include <linux/videodev2.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "DumpImage.h"
#include "LogHelper.h"
#include "VAConvertor.h"

namespace android {


void write_image(const void *data, const int size, int width, int height,const char *name,int format)
{
    char filename[80];
    static unsigned int count = 0;
    unsigned int i;
    size_t bytes;
    FILE *fp;

    snprintf(filename, sizeof(filename), "/data/nv12/dump_%d_%d_%03u_%s", width,
             height, count, name);

    fp = fopen (filename, "w+");
    if (fp == NULL) {
        LOGE ("open file %s failed %s", filename, strerror (errno));
        return ;
    }
    if(format == HAL_PIXEL_FORMAT_YCbCr_422_H_INTEL)
    {
        //Y
        fwrite (data,width*height, 1, fp);
        // U
        for (int i = 0; i < height; ++i) {
           fwrite(data + width*height, 1, width/2, fp);
        }
        // V
        for (int i = 0; i < height; ++i) {
            fwrite(data+width*height*2, 1, width/2, fp);
        }
    }
    else if ((bytes = fwrite (data, size, 1, fp)) < (size_t)size)
        LOGW ("Write less raw bytes to %s: %d, %d", filename, size, bytes);

    count++;

    fclose (fp);
}

}

