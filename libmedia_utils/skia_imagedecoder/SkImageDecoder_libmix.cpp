/*
* Copyright (c) 2014 Intel Corporation.  All rights reserved.
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
/*
 * Copyright 2007 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

//#define LOG_NDEBUG 0

#include "SkImageDecoder_libmix.h"
#include <va/va.h>
#include <va/va_android.h>
#include "va/va_dec_jpeg.h"
#include "JPEGDecoder.h"
#include <hardware/gralloc.h>
#include <utils/Timers.h>

// this enables our rgb->yuv code, which is faster than libjpeg on ARM
#define WE_CONVERT_TO_YUV

// If ANDROID_RGB is defined by in the jpeg headers it indicates that jpeg offers
// support for two additional formats (1) JCS_RGBA_8888 and (2) JCS_RGB_565.

#if defined(SK_DEBUG)
#define DEFAULT_FOR_SUPPRESS_JPEG_IMAGE_DECODER_WARNINGS false
#define DEFAULT_FOR_SUPPRESS_JPEG_IMAGE_DECODER_ERRORS false
#else  // !defined(SK_DEBUG)
#define DEFAULT_FOR_SUPPRESS_JPEG_IMAGE_DECODER_WARNINGS true
#define DEFAULT_FOR_SUPPRESS_JPEG_IMAGE_DECODER_ERRORS true
#endif  // defined(SK_DEBUG)
SK_CONF_DECLARE(bool, c_suppressJPEGImageDecoderWarnings,
                "images.jpeg.suppressDecoderWarnings",
                DEFAULT_FOR_SUPPRESS_JPEG_IMAGE_DECODER_WARNINGS,
                "Suppress most JPG warnings when calling decode functions.");
SK_CONF_DECLARE(bool, c_suppressJPEGImageDecoderErrors,
                "images.jpeg.suppressDecoderErrors",
                DEFAULT_FOR_SUPPRESS_JPEG_IMAGE_DECODER_ERRORS,
                "Suppress most JPG error messages when decode "
                "function fails.");

enum IMGDEC_INIT_STATUS {
    IMGDEC_INIT_STATUS_NOT_STARTED = 0,
    IMGDEC_INIT_STATUS_IN_PROGRESS,
    IMGDEC_INIT_STATUS_FAILED,
    IMGDEC_INIT_STATUS_SUCCEEDED,
};

static IMGDEC_INIT_STATUS global_init_status = IMGDEC_INIT_STATUS_NOT_STARTED;

// No need for global_uninitialize.
// when the process is killed, the global VA resource is automatically released.
static Mutex jd_lock;
static VADisplay global_vadisplay = 0;
static void *global_initialize(void * args)
{
    Mutex::Autolock autoLock(jd_lock);
    if (global_vadisplay == 0) {
        uint32_t dpy;
        int va_major_version, va_minor_version;
        VAStatus st;
        JpegDecodeStatus dst;
        global_init_status = IMGDEC_INIT_STATUS_IN_PROGRESS;
        global_vadisplay = vaGetDisplay(&dpy);
        if (global_vadisplay == NULL)
            goto cleanup;
        st = vaInitialize(global_vadisplay, &va_major_version, &va_minor_version);
        if (st != VA_STATUS_SUCCESS)
            goto cleanup;
        dst = JpegDecoder::preInit(global_vadisplay);
        if (dst == JD_SUCCESS) {
            global_init_status = IMGDEC_INIT_STATUS_SUCCEEDED;
            ALOGI("Successfully initialized VA global resource");
        }
        else {
            goto cleanup;
        }
    } else {
        // TODO: validate the resource
        ALOGI("Already initialized VA global resource");
    }
    return NULL;
cleanup:
    if (global_vadisplay != 0)
        vaTerminate(global_vadisplay);
    global_init_status = IMGDEC_INIT_STATUS_FAILED;
    ALOGE("Failed initializing VA global resource");
    return NULL;
}

static void init_render_target(RenderTarget &target, int width, int height, uint32_t fourcc)
{
    target.type = RenderTarget::INTERNAL_BUF;
    target.handle = generateHandle();
    target.width = aligned_width(width, SURF_TILING_Y);
    target.height = aligned_width(height, SURF_TILING_Y);
    target.pixel_format = fourcc;
    target.rect.x = target.rect.y = 0;
    target.rect.width = target.width;
    target.rect.height = target.height;
}

//#define DUMP_RGBA
//#define DUMP_DECODE
#define LOG_DECODE_TIME
#define MIN_PIXEL (1000 * 1000)
#define MAX_PIXEL (6000 * 6000)
bool SkJPEGMixImageDecoder::onDecode(SkStream* stream, SkBitmap* bm, Mode mode) {
#define BUFSIZE 1024
    int i;
    bool ret;
    JpegInfo jinfo;
    android::Vector<uint8_t> inputvec;
    static char tmpbuf[BUFSIZE];
    JpegDecodeStatus st;
    RenderTarget decbuf, blitbuf;
    uint32_t outw, outh, aligned_outw, aligned_outh;
    BlitEvent blit_event;
    uint8_t *rgba_out = NULL;
    SkStream *newstream = NULL;
    size_t bytes;
    INT32 bpr;
    uint8_t *rowptr = NULL;
    RenderTarget *targets = NULL;
    int sampleSize;
#if defined(DUMP_RGBA) || defined(DUMP_DECODE)
    char fn[128];
    FILE *fdump = NULL;
    JpegDecoder::MapHandle maphandle;
    uint8_t *mapaddr = NULL;
    uint32_t mapoffsets[4], mappitches[4];
#endif
    nsecs_t starttime = systemTime(SYSTEM_TIME_MONOTONIC);
    nsecs_t fallbacktime, endtime;

    memset(&jinfo, 0, sizeof(jinfo));
    jinfo.use_vector_input = true;
    jinfo.inputs = &inputvec;
    jinfo.need_header_only = true;
#ifdef TIME_DECODE
    SkAutoTime atm("JPEG Decode");
#endif
    sampleSize = this->getSampleSize();

    if (sampleSize != 1 &&
        sampleSize != 2 &&
        sampleSize != 4 &&
        sampleSize != 8) {
        ALOGV("%s sampleSize %d not supported, fallback", __func__, sampleSize);
        goto fallback;
    }

    if (mode == kDecodeBounds_Mode) {
        ALOGV("%s decodeMode %d not supported, fallback", __func__, mode);
        goto fallback;
    }

    mDecoder = new JpegDecoder(global_vadisplay,
        VA_INVALID_ID, VA_INVALID_ID, true);
    if (!mDecoder) {
        ALOGE("%s failed to new JpegDecoder", __func__);
        goto fallback;
    }

    jinfo.inputs->clear();

    do {
        bytes = stream->read(tmpbuf, BUFSIZE);
        if (bytes == 0) {
            ALOGV("%s unexpected EOF, fallback", __func__);
            goto fallback;
        }
        jinfo.inputs->appendArray((const uint8_t*)tmpbuf, bytes);
        st = mDecoder->parse(jinfo);
        if (this->shouldCancelDecode()) {
            ALOGV("%s decoding canceled", __func__);
            goto return_false;
        }
    } while (st == JD_INSUFFICIENT_BYTE);

    switch (st) {
    case JD_SUCCESS:
        break;
    default:
        ALOGD("%s JPEG parsing failed: %d, fallback", __func__, st);
        goto fallback;
    }

    // fall back to SW for small pictures to decode faster
    if (jinfo.image_height * jinfo.image_width < MIN_PIXEL) {
        ALOGV("%s JPEG resolution %ux%u too small, fallback",
            __func__, jinfo.image_width, jinfo.image_height);
        goto fallback;
    }

    // fall back to SW for big pictures to reduce memory footprint
    if (jinfo.image_height * jinfo.image_width > MAX_PIXEL) {
        ALOGV("%s JPEG resolution %ux%u too big, fallback",
            __func__, jinfo.image_width, jinfo.image_height);
        goto fallback;
    }

    outw = jinfo.image_width / sampleSize;
    outh = jinfo.image_height / sampleSize;
    aligned_outw = aligned_width(outw, SURF_TILING_Y);
    aligned_outh = aligned_width(outh, SURF_TILING_Y);
    bm->setInfo(SkImageInfo::Make(outw, outh,
                                  kN32_SkColorType, kOpaque_SkAlphaType));
    do {
        bytes = stream->read(tmpbuf, BUFSIZE);
        jinfo.inputs->appendArray((const uint8_t*)tmpbuf, bytes);
        if (this->shouldCancelDecode()) {
            ALOGV("%s decoding canceled", __func__);
            goto return_false;
        }
    } while (bytes > 0 && !stream->isAtEnd());

    ALOGV("%s Got all data from JPEG file: %d bytes", __func__,
        jinfo.inputs->size());

    init_render_target(decbuf, jinfo.image_width, jinfo.image_height,
        jinfo.image_color_fourcc);

    if (this->shouldCancelDecode()) {
        ALOGV("%s decoding canceled", __func__);
        goto return_false;
    }

    targets = &decbuf;
    st = mDecoder->init(jinfo.image_width, jinfo.image_height, &targets, 1);
    if (st != JD_SUCCESS) {
        ALOGE("%s libmix init returns %d, fallback", __func__, st);
        goto fallback;
    }

    jinfo.need_header_only = false;
    st = mDecoder->parse(jinfo);
    if (st != JD_SUCCESS) {
        ALOGE("%s libmix full parse returns %d, fallback", __func__, st);
        goto fallback;
    }
    st = mDecoder->decode(jinfo, decbuf);
    if (st != JD_SUCCESS) {
        ALOGE("%s libmix decode returns %d, fallback", __func__, st);
        goto fallback;
    }

    ALOGV("%s successfully decoded JPEG", __func__);

    rgba_out = (uint8_t*)memalign(0x1000,
            aligned_outw * aligned_outh * 4);

    if (!rgba_out) {
        ALOGE("%s failed to allocate RGBA buf, fallback", __func__);
        goto fallback;
    }

    st = mDecoder->blitToLinearRgba(decbuf, rgba_out,
        jinfo.image_width, jinfo.image_height,
        blit_event, sampleSize);

    if (st != JD_SUCCESS) {
        ALOGE("%s blit returns %d, fallback", __func__, st);
        goto fallback;
    }

    ALOGV("%s successfully blitted JPEG to RGBA8888", __func__);

#ifdef DUMP_DECODE
    maphandle = mDecoder->mapData(decbuf, (void**)&mapaddr, mapoffsets, mappitches);
    sprintf(fn, "/sdcard/%ux%u.%s", mappitches[0], jinfo.image_height,
        fourcc2str(jinfo.image_color_fourcc));
    fdump = fopen(fn, "wb+");
    if (fdump) {
        fwrite(mapaddr + mapoffsets[0], mappitches[0], jinfo.image_height, fdump);
        fwrite(mapaddr + mapoffsets[1], mappitches[1], jinfo.image_height, fdump);
        fwrite(mapaddr + mapoffsets[2], mappitches[2], jinfo.image_height, fdump);
        fclose(fdump);
    }
    else abort();
    mDecoder->unmapData(decbuf, maphandle);
#endif

#ifdef DUMP_DECODE
    sprintf(fn, "/sdcard/%ux%u.rgba", aligned_outw, aligned_outh);
    fdump = fopen(fn, "wb+");
    if (fdump) {
        fwrite(rgba_out, aligned_outw * aligned_outh * 4, 1, fdump);
        fclose(fdump);
    }
    else abort();
#endif

    if (this->shouldCancelDecode()) {
        ALOGV("%s decoding canceled", __func__);
        goto return_false;
    }

    if (!this->allocPixelRef(bm, NULL)) {
        ALOGE("%s failed to allocPixelRef", __func__);
        goto return_false;
    }

    //SkAutoLockPixels alp(*bm);
    bpr =  bm->rowBytes();
    rowptr = (uint8_t*)bm->getPixels();

    for (i = 0; i < outh; ++i) {
        memcpy(rowptr, rgba_out + i * aligned_outw * 4,
            bpr);
        rowptr += bpr;
        if (this->shouldCancelDecode()) {
            ALOGV("%s decoding canceled", __func__);
            goto return_false;
        }
    }
    mDecoder->deinit();
    delete mDecoder;
    mDecoder = NULL;
    free(rgba_out);
    endtime = systemTime(SYSTEM_TIME_MONOTONIC);
#ifdef LOG_DECODE_TIME
    ALOGD("mixImageDecoder took %.2fms to decode %ux%u (%dx downscale) "
        "with HW+GPU", (endtime - starttime)/1000000.0,
        jinfo.image_width, jinfo.image_height, sampleSize);
#endif
    return true;

fallback:
    fallbacktime = systemTime(SYSTEM_TIME_MONOTONIC);
    if (rgba_out)
        free(rgba_out);
    if (mDecoder) {
        mDecoder->deinit();
        delete mDecoder;
    }
    mDecoder = NULL;

    if (!stream->rewind()) {
        ALOGV("%s failed to rewind stream for falling back, use work around", __func__);
        // drain the stream and create a new stream
        do {
            bytes = stream->read(tmpbuf, BUFSIZE);
            jinfo.inputs->appendArray((const uint8_t*)tmpbuf, bytes);
            if (this->shouldCancelDecode()) {
                ALOGV("%s decoding canceled", __func__);
                goto return_false;
            }
        } while (bytes > 0 && !stream->isAtEnd());

        // FIXME: remove this dirty work around...
        newstream = new SkMemoryStream(jinfo.inputs->array(), jinfo.inputs->size(), false);
        ret = SkJPEGTurboImageDecoder::onDecode(newstream, bm, mode);
        delete newstream;
    }
    else ret = SkJPEGTurboImageDecoder::onDecode(stream, bm, mode);

    endtime = systemTime(SYSTEM_TIME_MONOTONIC);
    if (!ret)
        ALOGE("%s failed to decode with fallback", __func__);

    return ret;

return_false:
    if (rgba_out)
        free(rgba_out);
    if (mDecoder) {
        mDecoder->deinit();
        delete mDecoder;
    }
    mDecoder = NULL;
    endtime = systemTime(SYSTEM_TIME_MONOTONIC);
    return false;
}

bool SkJPEGMixImageDecoder::onBuildTileIndex(SkStreamRewindable* stream, int *width, int *height) {
    // TODO: implement libmix acceleration here
    ALOGV("Fall back to turboDecoder onBuildTileIndex from mixDecoder");
    return SkJPEGTurboImageDecoder::onBuildTileIndex(stream, width, height);
}

bool SkJPEGMixImageDecoder::onDecodeSubset(SkBitmap* bm, const SkIRect& region) {
    // TODO: implement libmix acceleration here
    ALOGV("Fall back to turboDecoder onDecodeSubset from mixDecoder");
    return SkJPEGTurboImageDecoder::onDecodeSubset(bm, region);
}

bool SkJPEGMixImageEncoder::onEncode(SkWStream* stream, const SkBitmap& bm, int quality) {
    bool ret;
    nsecs_t starttime = systemTime(SYSTEM_TIME_MONOTONIC);
    nsecs_t fallbacktime, endtime;

    // TODO: implement libmix acceleration here
fallback:
    fallbacktime = systemTime(SYSTEM_TIME_MONOTONIC);

    ALOGV("Fall back to turboEncoder onEncode from mixEncoder");
    ret = SkJPEGTurboImageEncoder::onEncode(stream, bm, quality);
    endtime = systemTime(SYSTEM_TIME_MONOTONIC);
    if (ret) {
#ifdef LOG_DECODE_TIME
        ALOGD("mixImageEncoder took %.2fms + %.2fms to encode %dx%d "
            "with fallback to libjpeg-turbo",
            (fallbacktime - starttime)/1000000.0,
            (endtime - fallbacktime)/1000000.0,
            bm.width(), bm.height());
#endif
    }
    return ret;
}


///////////////////////////////////////////////////////////////////////////////
DEFINE_DECODER_CREATOR(JPEGMixImageDecoder);
DEFINE_ENCODER_CREATOR(JPEGMixImageEncoder);
///////////////////////////////////////////////////////////////////////////////

static bool is_jpeg(SkStreamRewindable* stream) {
    static const unsigned char gHeader[] = { 0xFF, 0xD8, 0xFF };
    static const size_t HEADER_SIZE = sizeof(gHeader);

    char buffer[HEADER_SIZE];
    size_t len = stream->read(buffer, HEADER_SIZE);

    if (len != HEADER_SIZE) {
        return false;   // can't read enough
    }
    if (memcmp(buffer, gHeader, HEADER_SIZE)) {
        return false;
    }

    return true;
}

static inline bool check_libmix_global_init() {
    static pthread_t init_thread;
    int err;

#ifndef BAYTRAIL
    return false;
#endif

    static char property[PROPERTY_VALUE_MAX];
    if (property_get("skia.libmix.disabled", property, NULL) > 0) {
        if (strcmp(property, "1") == 0) {
            return false;
        }
    }

    switch (global_init_status) {
    case IMGDEC_INIT_STATUS_NOT_STARTED:
        err = pthread_create(&init_thread, NULL, global_initialize, NULL);
        if (err) {
            global_init_status = IMGDEC_INIT_STATUS_FAILED;
            ALOGE("%s Failed to start global init thread: %d (%s)",
                __FUNCTION__, err, strerror(err));
        }
        ALOGV("%s started global init thread", __func__);
        return false;
    case IMGDEC_INIT_STATUS_IN_PROGRESS:
        return false;
    case IMGDEC_INIT_STATUS_FAILED:
        ALOGV("%s global init failed", __func__);
        return false;
    case IMGDEC_INIT_STATUS_SUCCEEDED:
        return true;
    default:
        ALOGW("%s unknown global init status %d",
            __func__, global_init_status);
        return false;
    }
    return false;
}

static SkImageDecoder* sk_libmix_dfactory(SkStreamRewindable* stream) {
    if (check_libmix_global_init() && is_jpeg(stream))
        return SkNEW(SkJPEGMixImageDecoder);
    return NULL;
}

static SkImageDecoder::Format get_format_jpeg(SkStreamRewindable* stream) {
    if (check_libmix_global_init() && is_jpeg(stream))
        return SkImageDecoder::kJPEG_Format;
    return SkImageDecoder::kUnknown_Format;
}

static SkImageEncoder* sk_libmix_efactory(SkImageEncoder::Type t) {
    if (check_libmix_global_init() && (SkImageEncoder::kJPEG_Type == t))
        return SkNEW(SkJPEGMixImageEncoder);
    return NULL;
}

static SkImageDecoder_DecodeReg gDReg(sk_libmix_dfactory);
static SkImageDecoder_FormatReg gFormatReg(get_format_jpeg);
static SkImageEncoder_EncodeReg gEReg(sk_libmix_efactory);
