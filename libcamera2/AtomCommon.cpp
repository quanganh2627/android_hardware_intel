/*
 * Copyright (C) 2012 The Android Open Source Project
 * Copyright (c) 2012-2014 Intel Corporation
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

#include "AtomCommon.h"
#include "PlatformData.h"
#include <ia_coordinate.h>
#ifndef GRAPHIC_IS_GEN
#include <hal_public.h>
#else
#include <ufo/graphics.h>
#include "VAScaler.h"
#endif

#include <dlfcn.h>
#include <sys/types.h>
#include <pthread.h>

#define MAX_BACKTRACE_DEPTH 15

struct stack_crawl_state_t {
    size_t count;
    intptr_t* addrs;
};

namespace android {

/*
 * supported V4L2 fmts and resolutions
 */
const struct AtomFormatBridge sV4L2PixelFormatBridge[] = {
    {
        .pixelformat = V4L2_PIX_FMT_YUV420,
        .depth = 12,
        .planar = true,
        .bayer = false
    }, {
        .pixelformat = V4L2_PIX_FMT_YVU420,
        .depth = 12,
        .planar = true,
        .bayer = false
    }, {
        .pixelformat = V4L2_PIX_FMT_YUV422P,
        .depth = 16,
        .planar = true,
        .bayer = false
    }, {
        .pixelformat = V4L2_PIX_FMT_YUV444,
        .depth = 24,
        .planar = false,
        .bayer = false
    }, {
        .pixelformat = V4L2_PIX_FMT_NV12,
        .depth = 12,
        .planar = true,
        .bayer = false
    }, {
        .pixelformat = V4L2_PIX_FMT_NV21,
        .depth = 12,
        .planar = true,
        .bayer = false
    }, {
        .pixelformat = V4L2_PIX_FMT_NV16,
        .depth = 16,
        .planar = true,
        .bayer = false
    }, {
        .pixelformat = V4L2_PIX_FMT_YUYV,
        .depth = 16,
        .planar = false,
        .bayer = false
    }, {
        .pixelformat = V4L2_PIX_FMT_UYVY,
        .depth = 16,
        .planar = false,
        .bayer = false
    }, { /* This one is for parallel sensors! DO NOT USE! */
        .pixelformat = V4L2_PIX_FMT_UYVY,
        .depth = 16,
        .planar = false,
        .bayer = false
    }, {
        .pixelformat = V4L2_PIX_FMT_SBGGR16,
        .depth = 16,
        .planar = false,
        .bayer = true
    }, {
        .pixelformat = V4L2_PIX_FMT_SBGGR8,
        .depth = 8,
        .planar = false,
        .bayer = true
    }, {
        .pixelformat = V4L2_PIX_FMT_SGBRG8,
        .depth = 8,
        .planar = false,
        .bayer = true
    }, {
        .pixelformat = V4L2_PIX_FMT_SGRBG8,
        .depth = 8,
        .planar = false,
        .bayer = true
    }, {
        .pixelformat = V4L2_PIX_FMT_SRGGB8,
        .depth = 8,
        .planar = false,
        .bayer = true
    }, {
        .pixelformat = V4L2_PIX_FMT_SBGGR10,
        .depth = 16,
        .planar = false,
        .bayer = true
    }, {
        .pixelformat = V4L2_PIX_FMT_SGBRG10,
        .depth = 16,
        .planar = false,
        .bayer = true
    }, {
        .pixelformat = V4L2_PIX_FMT_SGRBG10,
        .depth = 16,
        .planar = false,
        .bayer = true
    }, {
        .pixelformat = V4L2_PIX_FMT_SRGGB10,
        .depth = 16,
        .planar = false,
        .bayer = true
    }, {
        .pixelformat = V4L2_PIX_FMT_SBGGR12,
        .depth = 16,
        .planar = false,
        .bayer = true
    }, {
        .pixelformat = V4L2_PIX_FMT_SGBRG12,
        .depth = 16,
        .planar = false,
        .bayer = true
    }, {
        .pixelformat = V4L2_PIX_FMT_SGRBG12,
        .depth = 16,
        .planar = false,
        .bayer = true
    }, {
        .pixelformat = V4L2_PIX_FMT_SRGGB12,
        .depth = 16,
        .planar = false,
        .bayer = true
    }, {
        .pixelformat = V4L2_PIX_FMT_RGB32,
        .depth = 32,
        .planar = false,
        .bayer = true
    }, {
        .pixelformat = V4L2_PIX_FMT_RGB565,
        .depth = 16,
        .planar = false,
        .bayer = true
    }, {
        .pixelformat = V4L2_PIX_FMT_JPEG,
        .depth = 8,
        .planar = false,
        .bayer = false
    },
};

/**
 * returns sAtomFormatBrige* for given V4L2 pixelformat (fourcc)
 */
const struct AtomFormatBridge* getAtomFormatBridge(unsigned int fourcc)
{
    unsigned int i;
    for (i = 0;i < sizeof(sV4L2PixelFormatBridge)/sizeof(AtomFormatBridge);i++) {
        if (fourcc == sV4L2PixelFormatBridge[i].pixelformat)
            return &sV4L2PixelFormatBridge[i];
    }

    ALOGE("Unknown pixel format being used : %s, aborting!", v4l2Fmt2Str(fourcc));
    abort();
    return NULL;
}

timeval AtomBufferFactory_AtomBufDefTS = {0, 0}; // default timestamp (see AtomCommon.h)
AtomBuffer AtomBufferFactory::createAtomBuffer(AtomBufferType type,
                                               int fourcc,
                                               int width,
                                               int height,
                                               int bpl,
                                               int size,
                                               IBufferOwner *owner,
                                               camera_memory_t *buff,
                                               camera_memory_t *metadata_buff,
                                               int id,
                                               int frameCounter,
                                               int ispPrivate,
                                               bool shared,
                                               struct timeval capture_timestamp,
                                               void *dataPtr,
                                               GFXBufferInfo *gfxInfo) {
    AtomBuffer buf;
    buf.fourcc = fourcc;
    buf.type = type;
    buf.width = width;
    buf.height = height;
    buf.bpl = bpl;
    buf.size = size;
    buf.owner = owner;
    buf.buff = buff;
    buf.metadata_buff = metadata_buff;
    buf.id = id;
    buf.expId = EXP_ID_INVALID;
    buf.frameCounter = frameCounter;
    buf.ispPrivate = ispPrivate;
    buf.status = FRAME_STATUS_NA;
    buf.shared = shared;
    buf.capture_timestamp = capture_timestamp;
    buf.dataPtr = dataPtr;
    buf.frameSequenceNbr = 0;
    if (dataPtr == NULL && buff != NULL)
        buf.dataPtr = buff->data;
    if (gfxInfo) {
        buf.gfxInfo = *gfxInfo;
    } else {
        buf.gfxInfo.gfxBuffer = NULL;
        buf.gfxInfo.gfxBufferHandle = NULL;
        buf.gfxInfo.scalerId = -1;
        buf.gfxInfo.locked = false;
    }
    buf.gfxInfo_rec.gfxBuffer = NULL;
    buf.gfxInfo_rec.gfxBufferHandle = NULL;
    buf.gfxInfo_rec.scalerId = -1;
    buf.gfxInfo_rec.locked = false;
    buf.auxBuf = NULL;
    buf.returnAfterCB = false;
    buf.sensorFrameId = -1;

    return buf;
}

bool isParameterSet(const char *param, const CameraParameters &params)
{
    if (param == NULL) {
        ALOGW("param string is NULL in isParameterSet");
        return false;
    }

    const char* strParam = params.get(param);
    int len = strlen(CameraParameters::TRUE);
    if (strParam != NULL && strncmp(strParam, CameraParameters::TRUE, len) == 0) {
        return true;
    }
    return false;
}

bool isBayerFormat(int fourcc)
{
    const AtomFormatBridge* afb = getAtomFormatBridge(fourcc);
    return afb->bayer;
}

/**
 * TODO: This needs to be removed once CSS API changes to support different bpl's
 *
 * In case of Raw capture the requirement changes
 *
 * \param format [in] V4L2 pixel format of the image
 * \param width [in] width in pixels
 *
 * \return bpl following the Display subsystem requirement
 **/
int SGXandDisplayBpl(int fourcc, int width)
{
    /**
     * Raw format has special aligning requirements
     */
    if (isBayerFormat(fourcc))
        width = ALIGN128(width);

    return pixelsToBytes(fourcc, width);
}

/**
 * Converts AF windows based on provided conversion information
 * \param focusWindows [in,out] windows to convert
 * \param winCount number of windows in focusWindows
 * \param convWindow [in] User-defined conversion window that will be used for conversion scaling (optional)
 */
void convertAfWindows(CameraWindow* focusWindows, size_t winCount, const AAAWindowInfo *convWindow)
{
    LOG1("@%s", __FUNCTION__);
    if (winCount > 0) {

        for (size_t i = 0; i < winCount; i++) {
            // Camera KEY_FOCUS_AREAS Coordinates range from -1000 to 1000. Let's convert..
            if (convWindow == NULL) {
                convertFromAndroidToIaCoordinates(focusWindows[i], focusWindows[i]);
            } else {
                convertFromAndroidCoordinates(focusWindows[i], focusWindows[i], *convWindow);
            }
            LOG1("Converted AF window %d: (%d,%d,%d,%d)",
                    i,
                    focusWindows[i].x_left,
                    focusWindows[i].y_top,
                    focusWindows[i].x_right,
                    focusWindows[i].y_bottom);
        }
    }
}

void convertFromAndroidToIaCoordinates(const CameraWindow &srcWindow, CameraWindow &toWindow)
{
    const ia_coordinate_system androidCoord = {-1000, -1000, 1000, 1000};
    const ia_coordinate_system iaCoord = {IA_COORDINATE_TOP, IA_COORDINATE_LEFT, IA_COORDINATE_BOTTOM, IA_COORDINATE_RIGHT};
    ia_coordinate topleft = {0, 0};
    ia_coordinate bottomright = {0, 0};

    topleft.x = srcWindow.x_left;
    topleft.y = srcWindow.y_top;
    bottomright.x = srcWindow.x_right;
    bottomright.y = srcWindow.y_bottom;

    topleft = ia_coordinate_convert(&androidCoord,
                                    &iaCoord, topleft);
    bottomright = ia_coordinate_convert(&androidCoord,
                                        &iaCoord, bottomright);

    toWindow.x_left = topleft.x;
    toWindow.y_top = topleft.y;
    toWindow.x_right = bottomright.x;
    toWindow.y_bottom = bottomright.y;
}

/**
 * Mirror the buffer contents by flipping the data horizontally or
 * vertically based on the camera sensor orientation and device
 * orientation.
 */
void mirrorBuffer(AtomBuffer *buffer, int currentOrientation, int cameraOrientation)
{
    LOG1("@%s", __FUNCTION__);

    int rotation = (cameraOrientation - currentOrientation + 360) % 360;
    if (rotation == 90 || rotation == 270) {
        flipBufferH(buffer);
    } else {
        flipBufferV(buffer);
    }
}

void flipBufferV(AtomBuffer *buffer) {
    LOG1("@%s", __FUNCTION__);
    int width, height, bpl;
    unsigned char *data = NULL;

    void *ptr = NULL;
    if (buffer->shared)
        ptr = (void *) *((char **)buffer->dataPtr);
    else
        ptr = buffer->dataPtr;

    data = (unsigned char *) ptr;
    width = buffer->width;
    height = buffer->height;
    bpl = buffer->bpl;
    int w = width / 2;
    unsigned char temp = 0;

    // Y
    for (int j=0; j < height; j++) {
        for (int i=0; i < w; i++) {
            temp = data[i];
            data[i] = data[width-i-1];
            data[width-i-1] = temp;
        }
        data = data + bpl;
    }

    int h = height / 2;
    unsigned char tempu = 0;
    unsigned char tempv = 0;

    // U+V
    for (int j=0; j < h; j++) {
        for (int i=0; i < w; i+=2) {
            tempu = data[i];
            tempv = data[i+1];
            data[i] = data[width-i-2];
            data[i+1] = data[width-i-1];
            data[width-i-2] = tempu;
            data[width-i-1] = tempv;
        }
        data = data + bpl;
    }
}

void flipBufferH(AtomBuffer *buffer) {
    LOG1("@%s", __FUNCTION__);
    int width, height, bpl;
    unsigned char *data = NULL;

    void *ptr = NULL;
    if (buffer->shared)
        ptr = (void *) *((char **)buffer->dataPtr);
    else
        ptr = buffer->dataPtr;

    data = (unsigned char *) ptr;
    width = buffer->width;
    height = buffer->height;
    bpl = buffer->bpl;
    int h = height / 2;
    unsigned char temp = 0;

    // Y
    for (int j=0; j < width; j++) {
        for (int i=0; i < h; i++) {
            temp = data[i*bpl + j];
            data[i*bpl + j] = data[(height-1-i)*bpl + j];
            data[(height-1-i)*bpl + j] = temp;
        }
    }

    // U+V
    data = data + bpl * height;
    h = height / 4;
    int heightUV = height / 2;

    for (int j=0; j < width; j++) {
        for (int i=0; i < h; i++) {
            temp = data[i*bpl + j];
            data[i*bpl + j] = data[(heightUV-1-i)*bpl + j];
            data[(heightUV-1-i)*bpl + j] = temp;
        }
    }
}

int getGFXHALPixelFormatFromV4L2Format(int previewFormat)
{
    LOG1("@%s", __FUNCTION__);
    int halPixelFormat = BAD_VALUE;

    switch(previewFormat) {
    case V4L2_PIX_FMT_NV12:
        halPixelFormat = HAL_PIXEL_FORMAT_NV12;
        break;
    case V4L2_PIX_FMT_NV21:
        halPixelFormat = CAM_HAL_PIXEL_FORMAT_NV21;
        break;
    case V4L2_PIX_FMT_YVU420:
        halPixelFormat = HAL_PIXEL_FORMAT_YV12;
        break;
    case V4L2_PIX_FMT_SGRBG8:
    case V4L2_PIX_FMT_SGBRG10:
    case V4L2_PIX_FMT_SBGGR10:
    case V4L2_PIX_FMT_SRGGB10:
        halPixelFormat = HAL_PIXEL_FORMAT_RGBA_8888;
        break;
    case V4L2_PIX_FMT_YUYV:
        halPixelFormat = HAL_PIXEL_FORMAT_YCbCr_422_I;
        break;
    default: break;
    }

    if (halPixelFormat == BAD_VALUE) {
        ALOGE("@%s Unknown / unsupported preview pixel format: fatal error, aborting",
                __FUNCTION__);
        abort();
    }

    return halPixelFormat;
}

/************************************************************************
 * DEBUGGING UTILITIES
 *
 */


int get_backtrace(intptr_t* addrs, size_t max_entries) {
    stack_crawl_state_t state;
    state.count = max_entries;
    state.addrs = addrs;

    size_t i, s;
    pthread_attr_t thread_attr;
    unsigned sb, st;
    size_t stacksize;
    pthread_t thread = pthread_self();
    unsigned *_ebp, *base_ebp;
    unsigned *caller;

    pthread_attr_init(&thread_attr);
    s = pthread_getattr_np(thread, &thread_attr);
    if (s) goto out;
    s = pthread_attr_getstack(&thread_attr, (void **)(&sb), &stacksize);
    if (s) goto out;
    st = sb + stacksize;

    asm ("movl %%ebp, %0"
            : "=r" (_ebp)
    );

    if (_ebp >= (unsigned *)(st - 4) || _ebp < (unsigned *)sb)
            goto out;
    base_ebp = _ebp;
    caller = (unsigned *) *(_ebp + 1);

    for (i = 0; i < max_entries; i++) {
        addrs[i] = (intptr_t) caller;
        state.count--;
        _ebp = (unsigned *) *_ebp;
        if (_ebp >= (unsigned *)(st - 4) || _ebp < base_ebp) break;
        caller = (unsigned *) *(_ebp + 1);
    }

out:
    pthread_attr_destroy(&thread_attr);

    return max_entries - state.count;
}

/**
 * Helper method to trace via log error the callstack in a particular point
 * this is only a RD feature.
 */
void trace_callstack () {
    intptr_t bt[MAX_BACKTRACE_DEPTH];
    Dl_info info;
    void* offset = 0;
    const char* symbol = NULL;
    const char* fname = NULL;

    int depth = get_backtrace(bt, MAX_BACKTRACE_DEPTH);

    for (int i = 0; i < depth; i++) {
        if (dladdr((void*)bt[i], &info)) {
               offset = info.dli_saddr;
               symbol = info.dli_sname;
               fname = info.dli_fname;
               ALOGE("Camera_BT:%s:%s:+%p",fname,symbol,offset);
        } else {
            ALOGE("Camera_BT symbol not found in address %x",bt[i]);
        }

    }

}

/**
 * RD Helper method to inject an YUV file to an Atombuffer
 * This function can be used during investigations anywhere in the HAL
 * codebase
 */
void inject(AtomBuffer *b, const char* name)
{
#ifdef ENABLE_FILE_INJECTION
    int bytes = 0;

    ALOGE("Injecting yuv file %s resolution (%dx%d) format %s",name,b->width, b->height,v4l2Fmt2Str(b->fourcc));

    FILE *fd = fopen(name, "rb+");

    if(fd == NULL) {
        ALOGE("%s: could not open inject file ", __FUNCTION__);
        return;
    }

    bytes = fread(b->dataPtr, 1, b->size, fd);
    if (bytes != b->size) {
        ALOGE("ERROR INJECTING %s read %d size %d",name, bytes, b->size);
    }

    fclose(fd);
#endif
}

}
