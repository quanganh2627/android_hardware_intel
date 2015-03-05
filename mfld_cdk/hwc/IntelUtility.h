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

#ifndef __INTEL_HWCOMPOSER_UTILITY_H__
#define __INTEL_HWCOMPOSER_UTILITY_H__

#define HWC_DEBUG_DUMP_LAYERS

typedef struct tagBITMAPFILEHEADER { /* bmfh */
    short   bfType;
    int     bfSize;
    short   bfReserved1;
    short   bfReserved2;
    int     bfOffBits;
} BITMAPFILEHEADER;

typedef struct tagBITMAPINFOHEADER { /* bmih */
    int     biSize;
    int     biWidth;
    int     biHeight;

    int     biPlanes:16;
    int     biBitCount:16;

    int     biCompression;
    int     biSizeImage;
    int     biXPixPerMeter;
    int     biYPixPerMeter;
    int     biClrUsed;
    int     biClrImportant;
} BITMAPINFOHEADER;


class IntelUtility
{
public:
    IntelUtility();
    IntelUtility(int, struct hwc_display_contents_1**);
    ~IntelUtility();

    void setLayerList(struct hwc_display_contents_1**);
    void dumpLayers(char*);
    int generateBitmap(int width, int height, int format, char const * data, char const * bitmapName);
    bool needDump(void);
private:
    gralloc_module_t*    mGrallocModule;
    struct hwc_display_contents_1**  mLayerLists;
    int mNumDisplay;
    int mIndex;
    char mDefaultDumpPath[128];
};

#endif
