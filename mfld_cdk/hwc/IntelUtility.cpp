/*
 * Copyright (c) 2013, Intel Corporation. All rights reserved.
 *
 * Redistribution.
 * Redistribution and use in binary form, without modification, are
 * permitted provided that the following conditions are met:
 *  * Redistributions must reproduce the above copyright notice and
 * the following disclaimer in the documentation and/or other materials
 * provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its
 * suppliers may be used to endorse or promote products derived from
 * this software without specific  prior written permission.
 *  * No reverse engineering, decompilation, or disassembly of this
 * software is permitted.
 *
 * Limited patent license.
 * Intel Corporation grants a world-wide, royalty-free, non-exclusive
 * license under patents it now or hereafter owns or controls to make,
 * have made, use, import, offer to sell and sell ("Utilize") this
 * software, but solely to the extent that any such patent is necessary
 * to Utilize the software alone, or in combination with an operating
 * system licensed under an approved Open Source license as listed by
 * the Open Source Initiative at http://opensource.org/licenses.
 * The patent license shall not apply to any other combinations which
 * include this software. No hardware per se is licensed hereunder.
 *
 * DISCLAIMER.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors:
 *    Lingyun Zhu <lingyun.zhu@intel.com>
 *
 */

#include <utils/Log.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <IntelHWComposer.h>
#include <IntelBufferManager.h>
#include <pvrversion.h>
#include <cutils/properties.h>
#include <PixelFormat.h>

#include <IntelUtility.h>

using namespace::android;

IntelUtility::IntelUtility() : mGrallocModule(0), mLayerLists(0), mIndex(0)
{
    sprintf(mDefaultDumpPath, "/data/app/hwcDumpLayers");
}

IntelUtility::IntelUtility(int num, struct hwc_display_contents_1** list)
{
    hw_module_t const* module;
    if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module) != 0) {
        LOGD("IntelUtility hw_get_module failed.");
        return;
    }
    mGrallocModule = (gralloc_module_t*)module;
    mLayerLists = list;
    mNumDisplay = num;
    mIndex = 0;
    sprintf(mDefaultDumpPath, "/data/app/hwcDumpLayers");
}

IntelUtility::~IntelUtility()
{
}

void IntelUtility::setLayerList(struct hwc_display_contents_1** list)
{
    mLayerLists = list;
}

bool IntelUtility::needDump()
{
    int dump = 0;
    char val[PROPERTY_VALUE_MAX];

    if(property_get("debug.hwc.dumplayers", val, NULL) > 0) {
        dump = atoi(val);
    }

//    LOGD("IntelUtility::needDump %d, val = %s", dump, val);
    return dump ? true : false;
}

void IntelUtility::dumpLayers(char* path)
{
    if (!needDump())
    {
        return;
    }

    int i = 0;
    int j = 0;
    int err = 0;
    char* vaddr;
    hwc_layer_1_t* layer = 0;
    int num = 0;
    int width = 0;
    int height = 0;
    int stride = 0;
    int format = 0;
    char *filePath = path;

    if (path == NULL) {
        path = mDefaultDumpPath;
    }

    if(access(path, F_OK) == -1) {
    // File not exist;
        if(mkdir(path, 0777) == -1) {
            LOGD("IntelUtility::dumpLayers failed to create dir: %s. Please mkdir manually.", path);
            return;
        }
    }

    if(access(path, W_OK|X_OK) == -1) {
        LOGD("IntelUtility::dumpLayers failed to Write/Execute dir: %s", path);
        return;
    }

    struct timeval t;
    gettimeofday(&t, NULL);
//    LOGD("IntelUtility::dumpLayers mNumDisplay = %d", mNumDisplay);
    for (j = 0; j < mNumDisplay; j++) {
//        LOGD("IntelUtility::dumpLayers mLayerLists[%d] = %p", j, mLayerLists[j]);
        if (!mLayerLists[j]) {
            continue;
        }

        layer = mLayerLists[j]->hwLayers;
        num = mLayerLists[j]->numHwLayers;

        for (i = 0; i < num; i++) {
            hwc_layer_1_t* l = &(layer[i]);
            IMG_native_handle_t* grallocHandle = (IMG_native_handle_t*)l->handle;

            if(grallocHandle == NULL) {
                continue;
            }

            // lock buffer
            mGrallocModule->lock((gralloc_module_t*)mGrallocModule, l->handle, GRALLOC_USAGE_SW_READ_OFTEN, 0, 0, 1, 1, (void**)&vaddr);
            if (err != 0) {
                LOGD("IntelUtility::dumpLayers: gralloc_module_lock failed. (errno = %d)", err);
                return;
            }
            // save to bmp
            width = grallocHandle->iWidth;
            height = grallocHandle->iHeight;
            stride = grallocHandle->iStride; // TODO
            format = grallocHandle->iFormat;

            char fileName[128] = {0};

            if (i == (num-1)) {
                // FrameBuffer Target
                sprintf(fileName, "%s/Time%ld.%06ld__dpy%d_FBTarget_num%d_w%d_s%d_h%d_f%d.bmp", path, t.tv_sec, t.tv_usec, j, num, width, stride, height, format);
            } else {
                // Normal Layer
                sprintf(fileName, "%s/Time%ld.%06ld__dpy%d_Layer%02d__num%d_w%d_s%d_h%d_f%d.bmp", path, t.tv_sec, t.tv_usec, j, i, num, width, stride, height, format);
            }

            //TODO: Before DDK1.10, the framebuffer is swapchain, and there is no stride in the allocated buffer, so use width to dump.
            //      After DDK1.10, the framebuffer is allocated as nomal gralloc buffer, so need use stride to dump.
            // DDK1.9
            if (PVRVERSION_MAJ == 1 && PVRVERSION_MIN == 9) {
                if (i == num-1) {
                    // FramebufferTarget
                    generateBitmap(width, height, format, vaddr, fileName);
                } else {
                    // Normal Layer
                    generateBitmap(stride, height, format, vaddr, fileName);
                }
            }
            else if (PVRVERSION_MAJ == 1 && PVRVERSION_MIN >= 10) {
            // DDK after 1.9
                generateBitmap(stride, height, format, vaddr, fileName);
            }
            // unlock buffer
            mGrallocModule->unlock((gralloc_module_t*)mGrallocModule, l->handle);
        }
    }

}

int IntelUtility::generateBitmap(int width, int height, int format, char const * data, char const * bitmapName){
    int fd = 0;
    int header_size = 0;
    int data_size = 0;
    int file_size = 0;
    int bitcount = 0;   // bit number per pixel

    BITMAPFILEHEADER  bmfh;
    BITMAPINFOHEADER  bmih;

    memset(&bmfh, 0, sizeof(BITMAPFILEHEADER));
    memset(&bmih, 0, sizeof(BITMAPINFOHEADER));

    if ( width <= 0 || height <= 0 || data == NULL || bitmapName == NULL ) {
        return -EINVAL;
    }

    if ( format != PIXEL_FORMAT_RGBA_8888 &&
        format != PIXEL_FORMAT_RGBX_8888 &&
        format != PIXEL_FORMAT_RGB_888 &&
        format != PIXEL_FORMAT_BGRA_8888 ) {
        LOGD("Layer::generateBitmap() not support format.");
        return 0;
    }

    if ( format == PIXEL_FORMAT_RGB_888 ) {
        bitcount = 24;
    }
    else {
        bitcount = 32;
    }

    header_size = 54;
    data_size = width * height * bitcount / 8;   // Bytes
    file_size = header_size + data_size;

    bmfh.bfType = 0x4D42;     // 'B''M' ("!! W !!")
    bmfh.bfSize = file_size;
    bmfh.bfReserved1 = bmfh.bfReserved2 = 0;
    bmfh.bfOffBits = header_size;

    bmih.biSize = 40;    // default
    bmih.biWidth = width;
    bmih.biHeight = height;
    bmih.biPlanes = 1;
    bmih.biBitCount = bitcount;
    bmih.biCompression = 0;

    bmih.biSizeImage = data_size;
    bmih.biXPixPerMeter = 0;
    bmih.biYPixPerMeter = 0;
    bmih.biClrUsed = 0;
    bmih.biClrImportant = 0;

    fd = open(bitmapName, O_WRONLY|O_CREAT|O_TRUNC, 0755);
    if(fd < 0) {
        LOGD("ERROR: generateBitmap: failed to open %s (%d)", bitmapName, errno);
        return -errno;
    }

    write(fd, &bmfh.bfType, 2);
    write(fd, &bmfh.bfSize, 4);
    write(fd, &bmfh.bfReserved1, 2);
    write(fd, &bmfh.bfReserved2, 2);
    write(fd, &bmfh.bfOffBits, 4);

    write(fd, &bmih, sizeof(bmih));
    write(fd, data, data_size);

    close(fd);

    return 0;
}
