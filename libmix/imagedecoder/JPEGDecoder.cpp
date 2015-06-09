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

//#define LOG_NDEBUG 0
#include <va/va.h>
#include <va/va_tpi.h>
#include "JPEGDecoder.h"
#include "JPEGParser.h"
#include "JPEGBlitter.h"
#include "ImageDecoderTrace.h"

#define JPEG_MAX_SETS_HUFFMAN_TABLES 2

#define TABLE_CLASS_DC  0
#define TABLE_CLASS_AC  1
#define TABLE_CLASS_NUM 2

// for config
#if 0
#define HW_DECODE_MIN_WIDTH  1000 // for JPEG smaller than this, use SW decode
#define HW_DECODE_MIN_HEIGHT 1000 // for JPEG smaller than this, use SW decode

#define HW_DECODE_MAX_WIDTH  6000 // for JPEG bigger than this, use SW decode
#define HW_DECODE_MAX_HEIGHT 4000 // for JPEG bigger than this, use SW decode
#endif

typedef uint32_t Display;

#define JD_CHECK(err, label) \
        if (err) { \
            ETRACE("%s::%d: failed: %d", __PRETTY_FUNCTION__, __LINE__, err); \
            goto label; \
        }

#define JD_CHECK_RET(err, label, retcode) \
        if (err) { \
            status = retcode; \
            ETRACE("%s::%d: failed: %d", __PRETTY_FUNCTION__, __LINE__, err); \
            goto label; \
        }

static int handlectr = 0;
int generateHandle()
{
    return handlectr++;
}

JpegDecodeStatus JpegDecoder::preInit(VADisplay display)
{
    return JpegBlitter::preInit(display);
}

JpegDecoder::JpegDecoder(VADisplay display, VAConfigID vpCfgId, VAContextID vpCtxId, bool use_blitter)
    :mInitialized(false),
    mDisplay(display),
    mConfigId(VA_INVALID_ID),
    mContextId(VA_INVALID_ID),
    mParser(NULL),
    mBlitter(NULL),
    mDispCreated(false)
{
    mParser = new CJPEGParse;
    mBsParser = new JpegBitstreamParser;
    if (!display) {
        if ((vpCfgId != VA_INVALID_ID) ||
            (vpCtxId != VA_INVALID_ID) ||
            (use_blitter == true)) {
            ETRACE("%s: invalid init argument", __FUNCTION__);
            return;
        }
        Display dpy;
        int va_major_version, va_minor_version;
        mDisplay = vaGetDisplay(&dpy);
        vaInitialize(mDisplay, &va_major_version, &va_minor_version);
        mDispCreated = true;
    }
    if (use_blitter) {
        if ((display == NULL)) {
            ETRACE("%s: invalid init argument", __FUNCTION__);
            return;
        }
        mBlitter = new JpegBlitter(display, vpCfgId,vpCtxId);
    }
    VTRACE("%s CTOR succeded", __FUNCTION__);
}
JpegDecoder::~JpegDecoder()
{
    if (mInitialized) {
        WTRACE("Freeing JpegDecoder: not destroyed yet. Force destroy resource");
        deinit();
    }
    if (mBlitter)
        mBlitter->deinit();
    delete mBlitter;
    if (mDispCreated)
        vaTerminate(mDisplay);
    delete mParser;
    delete mBsParser;
    VTRACE("%s DTOR succeded", __FUNCTION__);
}

JpegDecoder::MapHandle JpegDecoder::mapData(RenderTarget &target, void ** data, uint32_t * offsets, uint32_t * pitches)
{
    VAImage *img = NULL;
    VASurfaceID surf_id = getSurfaceID(target);
    if (surf_id != VA_INVALID_ID) {
        img = new VAImage();
        if (img == NULL) {
            ETRACE("%s: create VAImage fail", __FUNCTION__);
            return 0;
        }
        VAStatus st;
        st = vaDeriveImage(mDisplay, surf_id, img);
        if (st != VA_STATUS_SUCCESS) {
            delete img;
            img = NULL;
            ETRACE("%s: vaDeriveImage fail %d", __FUNCTION__, st);
            return 0;
        }
        st = vaMapBuffer(mDisplay, img->buf, data);
        if (st != VA_STATUS_SUCCESS) {
            vaDestroyImage(mDisplay, img->image_id);
            delete img;
            img = NULL;
            ETRACE("%s: vaMapBuffer fail %d", __FUNCTION__, st);
            return 0;
        }
        offsets[0] = img->offsets[0];
        offsets[1] = img->offsets[1];
        offsets[2] = img->offsets[2];
        pitches[0] = img->pitches[0];
        pitches[1] = img->pitches[1];
        pitches[2] = img->pitches[2];
        VTRACE("%s: successfully mapped RenderTarget %p, handle %d, data=%p, offsets=[%u,%u,%u], pitches=[%u,%u,%u], size=%u, %ux%u, to handle.img %p",
            __FUNCTION__, &target, target.handle, *data, offsets[0], offsets[1], offsets[2],
            pitches[0], pitches[1], pitches[2], img->data_size,
            img->width, img->height, img);

        return (unsigned long)img;
    }
    ETRACE("%s: get Surface ID fail", __FUNCTION__);
    return 0;
}

void JpegDecoder::unmapData(RenderTarget &target, JpegDecoder::MapHandle maphandle)
{
    if (maphandle != 0) {
        vaUnmapBuffer(mDisplay, ((VAImage*)maphandle)->buf);
        vaDestroyImage(mDisplay, ((VAImage*)maphandle)->image_id);
        VTRACE("%s deleting VAImage %p", __FUNCTION__, ((VAImage*)maphandle));
        delete ((VAImage*)maphandle);
    }
}

JpegDecodeStatus JpegDecoder::init(int w, int h, RenderTarget **targets, int num)
{
    if (mInitialized) {
        VTRACE("%s already initialized", __FUNCTION__);
        return JD_ALREADY_INITIALIZED;
    }
    Mutex::Autolock autoLock(mLock);
    if (!mInitialized) {
        // TODO: read min wxh and max wxh from sys config
        // VA setup
        nsecs_t now = systemTime();
        mGrallocSurfaceMap.clear();
        mDrmSurfaceMap.clear();
        mNormalSurfaceMap.clear();
        mUserptrSurfaceMap.clear();
        VAStatus st;
        VASurfaceID surfid;
        for (int i = 0; i < num; ++i) {
            JpegDecodeStatus st = createSurfaceFromRenderTarget(*targets[i], &surfid);
            if (st != JD_SUCCESS || surfid == VA_INVALID_ID) {
                ETRACE("%s failed to create surface from RenderTarget handle 0x%x",
                    __FUNCTION__, targets[i]->handle);
                return JD_RESOURCE_FAILURE;
            }
            VTRACE("%s successfully created surface %u for renderTarget %p, handle %d",
                __FUNCTION__, surfid, targets[i], targets[i]->handle);
        }
        VAConfigAttrib attrib;

        attrib.type = VAConfigAttribRTFormat;
        st = vaGetConfigAttributes(mDisplay, VAProfileJPEGBaseline, VAEntrypointVLD, &attrib, 1);
        if (st != VA_STATUS_SUCCESS) {
            ETRACE("vaGetConfigAttributes failed. va_status = 0x%x", st);
            return JD_INITIALIZATION_ERROR;
        }
        st = vaCreateConfig(mDisplay, VAProfileJPEGBaseline, VAEntrypointVLD, &attrib, 1, &mConfigId);
        if (st != VA_STATUS_SUCCESS) {
            ETRACE("vaCreateConfig failed. va_status = 0x%x", st);
            return JD_INITIALIZATION_ERROR;
        }
        mContextId = VA_INVALID_ID;
        size_t gmsize = mGrallocSurfaceMap.size();
        size_t dmsize = mDrmSurfaceMap.size();
        size_t nmsize = mNormalSurfaceMap.size();
        size_t umsize = mUserptrSurfaceMap.size();
        VASurfaceID *surfaces = new VASurfaceID[gmsize + dmsize + nmsize + umsize];
        for (size_t i = 0; i < gmsize + dmsize + nmsize + umsize; ++i) {
            if (i < gmsize)
                surfaces[i] = mGrallocSurfaceMap.valueAt(i);
            else if (i < gmsize + dmsize)
                surfaces[i] = mDrmSurfaceMap.valueAt(i - gmsize);
            else if (i < gmsize + dmsize + nmsize)
                surfaces[i] = mNormalSurfaceMap.valueAt(i - gmsize - dmsize);
            else
                surfaces[i] = mUserptrSurfaceMap.valueAt(i - gmsize - dmsize - nmsize);
        }
        st = vaCreateContext(mDisplay, mConfigId,
            w, h,
            0,
            surfaces, gmsize + dmsize + nmsize,
            &mContextId);
        delete[] surfaces;
        if (st != VA_STATUS_SUCCESS) {
            ETRACE("vaCreateContext failed. va_status = 0x%x", st);
            return JD_INITIALIZATION_ERROR;
        }

        VTRACE("JpegDecoder::init took %.2f ms", (systemTime() - now)/1000000.0);
        mInitialized = true;
    }
    return JD_SUCCESS;
}

JpegDecodeStatus JpegDecoder::blit(RenderTarget &src, RenderTarget &dst, int scale_factor)
{
    if (mBlitter) {
        mBlitter->init(*this);
        return mBlitter->blit(src, dst, scale_factor);
    }
    else
        return JD_BLIT_FAILURE;
}

JpegDecodeStatus JpegDecoder::getRgbaTile(RenderTarget &src,
                                     uint8_t *sysmem,
                                     int left, int top, int width, int height, int scale_factor)
{
    if (mBlitter) {
        nsecs_t now = systemTime();
        mBlitter->init(*this);
        nsecs_t t1 = systemTime();
        JpegDecodeStatus st = mBlitter->getRgbaTile(src, sysmem, left, top, width, height, scale_factor);
        VTRACE("Decoder::%s took %.2f + %.2f ms", __FUNCTION__,
            (t1-now)/1000000.0, (systemTime()-t1)/1000000.0);
        return st;
    }
    else
        return JD_BLIT_FAILURE;

}

JpegDecodeStatus JpegDecoder::blitToLinearRgba(RenderTarget &src, uint8_t *sysmem, uint32_t width, uint32_t height, BlitEvent &event, int scale_factor)
{
    if (mBlitter) {
        nsecs_t now = systemTime();
        mBlitter->init(*this);
        nsecs_t t1 = systemTime();
        JpegDecodeStatus st = mBlitter->blitToLinearRgba(src, sysmem, width, height, event, scale_factor);
        VTRACE("Decoder::%s took %.2f + %.2f ms", __FUNCTION__,
            (t1-now)/1000000.0, (systemTime()-t1)/1000000.0);
        return st;
    }
    else
        return JD_BLIT_FAILURE;
}

JpegDecodeStatus JpegDecoder::blitToCameraSurfaces(RenderTarget &src,
                                                   buffer_handle_t dst_nv12,
                                                   buffer_handle_t dst_yuy2,
                                                   uint8_t *dst_nv21,
                                                   uint8_t *dst_yv12,
                                                   uint32_t width, uint32_t height, BlitEvent &event)
{
    if (mBlitter) {
        nsecs_t now = systemTime();
        mBlitter->init(*this);
        nsecs_t t1 = systemTime();
        JpegDecodeStatus st = mBlitter->blitToCameraSurfaces(src, dst_nv12, dst_yuy2, dst_nv21, dst_yv12, width, height, event);
        VTRACE("Decoder::%s took %.2f + %.2f ms", __FUNCTION__,
            (t1-now)/1000000.0, (systemTime()-t1)/1000000.0);
        return st;
    }
    else
        return JD_BLIT_FAILURE;
}

void JpegDecoder::syncBlit(BlitEvent &event)
{
    if (!mBlitter) {
        ETRACE("%s blitter not initialized", __FUNCTION__);
        return;
    }
    mBlitter->syncBlit(event);
}

JpegDecodeStatus JpegDecoder::parseHeader(JpegInfo &jpginfo)
{
#define ROLLBACK_IF_FAIL(stmt) \
    do { \
        if (!stmt) { \
            VTRACE("%s::%d, parser failed at offset %u, remaining bytes %u, total bytes %zu", \
                __FUNCTION__, __LINE__, mBsParser->getByteOffset(), mBsParser->getRemainingBytes(), \
                jpginfo.bufsize); \
            goto rollback; \
        } \
    } while(0);

    if (jpginfo.use_vector_input) {
        mBsParser->set(jpginfo.inputs);
    }
    else {
        mBsParser->set(jpginfo.buf, jpginfo.bufsize);
    }

    uint8_t marker;
    uint32_t rollbackoff;
    rollbackoff = mBsParser->getByteOffset();
    ROLLBACK_IF_FAIL(mBsParser->tryGetNextMarker(&marker));

    while (marker != CODE_EOI &&( !mBsParser->endOfBuffer())) {
        switch (marker) {
            case CODE_SOI: {
                VTRACE("%s SOI at 0x%08x", __FUNCTION__, mBsParser->getByteOffset());
                jpginfo.soi_offset = mBsParser->getByteOffset() - 2;
                jpginfo.soi_parsed = true;
                break;
            }
            // If the marker is an APP marker skip over the data
            case CODE_APP0:
                {
                    // parse JFIF
                    uint32_t len;
                    uint8_t data[5];
                    memset(data, 0, sizeof data);
                    ROLLBACK_IF_FAIL(mBsParser->tryReadBytes(&len, 2));
                    ROLLBACK_IF_FAIL(mBsParser->getRemainingBytes() >= len);
                    mBsParser->tryReadNextByte(&data[0]);
                    mBsParser->tryReadNextByte(&data[1]);
                    mBsParser->tryReadNextByte(&data[2]);
                    mBsParser->tryReadNextByte(&data[3]);
                    mBsParser->tryReadNextByte(&data[4]);
                    if (data[0] == 0x4A &&
                        data[1] == 0x46 &&
                        data[2] == 0x49 &&
                        data[3] == 0x46 &&
                        data[4] == 0) {
                        jpginfo.jfif_parsed = true;
                        VTRACE("%s parsed JFIF marker at 0x%08x", __FUNCTION__, mBsParser->getByteOffset());
                    }
                }
                break;
            case CODE_APP1:
            case CODE_APP2:
            case CODE_APP3:
            case CODE_APP4:
            case CODE_APP5:
            case CODE_APP6:
            case CODE_APP7:
            case CODE_APP8:
            case CODE_APP9:
            case CODE_APP10:
            case CODE_APP11:
            case CODE_APP12:
            case CODE_APP13:
                VTRACE("%s APP %x at 0x%08x", __FUNCTION__, marker, mBsParser->getByteOffset());
                uint32_t bytes_to_burn;
                ROLLBACK_IF_FAIL(mBsParser->tryReadBytes(&bytes_to_burn, 2));
                bytes_to_burn -= 2;
                ROLLBACK_IF_FAIL(mBsParser->tryBurnBytes(bytes_to_burn));
                break;
            case CODE_APP14:
                {
                    // parse ADOBE
                    uint32_t len;
                    uint8_t data[5];
                    memset(data, 0, sizeof data);
                    ROLLBACK_IF_FAIL(mBsParser->tryReadBytes(&len, 2));
                    ROLLBACK_IF_FAIL(mBsParser->getRemainingBytes() >= 12);
                    mBsParser->tryReadNextByte(&data[0]);
                    mBsParser->tryReadNextByte(&data[1]);
                    mBsParser->tryReadNextByte(&data[2]);
                    mBsParser->tryReadNextByte(&data[3]);
                    mBsParser->tryReadNextByte(&data[4]);
                    if (data[0] == 0x41 &&
                        data[1] == 0x64 &&
                        data[2] == 0x6F &&
                        data[3] == 0x62 &&
                        data[4] == 0x65) {
                        jpginfo.adobe_parsed = true;
                        mBsParser->tryBurnBytes(6);
                        mBsParser->tryReadNextByte(&jpginfo.adobe_transform);
                        VTRACE("%s parsed adobe marker at 0x%08x, transform = %u", __FUNCTION__, mBsParser->getByteOffset() - 7, jpginfo.adobe_transform);
                    }
                }
                break;
            case CODE_APP15: {
                VTRACE("%s APP %x at 0x%08x", __FUNCTION__, marker, mBsParser->getByteOffset());
                uint32_t bytes_to_burn;
                ROLLBACK_IF_FAIL(mBsParser->tryReadBytes(&bytes_to_burn, 2));
                bytes_to_burn -= 2;
                ROLLBACK_IF_FAIL(mBsParser->tryBurnBytes(bytes_to_burn));
                break;
            }
            // Store offset to DQT data to avoid parsing bitstream in user mode
            case CODE_DQT: {
                VTRACE("%s DQT at 0x%08x", __FUNCTION__, mBsParser->getByteOffset());
                //if (jpginfo.dqt_ind < 4) {
                jpginfo.dqt_byte_offset[jpginfo.dqt_ind] = mBsParser->getByteOffset() - jpginfo.soi_offset;
                jpginfo.dqt_ind++;
                uint32_t bytes_to_burn;
                ROLLBACK_IF_FAIL(mBsParser->tryReadBytes(&bytes_to_burn, 2));
                bytes_to_burn -= 2;
                ROLLBACK_IF_FAIL(mBsParser->tryBurnBytes(bytes_to_burn));
                //} else {
                //    ETRACE("ERROR: Decoder does not support more than 4 Quant Tables\n");
                //    return JD_CODEC_UNSUPPORTED;
                //}
                jpginfo.dqt_parsed = true;
                break;
            }
            // Throw exception for all SOF marker other than SOF0
            case CODE_SOF1:
            case CODE_SOF2:
            case CODE_SOF3:
            case CODE_SOF5:
            case CODE_SOF6:
            case CODE_SOF7:
            case CODE_SOF8:
            case CODE_SOF9:
            case CODE_SOF10:
            case CODE_SOF11:
            case CODE_SOF13:
            case CODE_SOF14:
            case CODE_SOF15: {
                ETRACE("ERROR: unsupport SOF\n");
                break;
            }
            case CODE_SOF_BASELINE: {
                VTRACE("%s SOF_BASELINE at 0x%08x", __FUNCTION__, mBsParser->getByteOffset());
                ROLLBACK_IF_FAIL((mBsParser->getRemainingBytes() >= 10));
                jpginfo.frame_marker_found = true;
                bool r;
                ROLLBACK_IF_FAIL(mBsParser->tryBurnBytes(2)); // Throw away frame header length 
                uint8_t sample_precision;
                ROLLBACK_IF_FAIL(mBsParser->tryReadNextByte(&sample_precision));
                if (sample_precision != 8) {
                    ETRACE("sample_precision is not supported\n");
                    return JD_INPUT_FORMAT_UNSUPPORTED;
                }
                // Extract pic width and height
                uint32_t w, h;
                ROLLBACK_IF_FAIL(mBsParser->tryReadBytes(&h, 2));
                ROLLBACK_IF_FAIL(mBsParser->tryReadBytes(&w, 2));
                ROLLBACK_IF_FAIL(mBsParser->tryReadNextByte(&jpginfo.picture_param_buf.num_components));
                jpginfo.picture_param_buf.picture_width = w;
                jpginfo.picture_param_buf.picture_height = h;
                VTRACE("%s pic wxh=%ux%u, %u components", __FUNCTION__, 
                    jpginfo.picture_param_buf.picture_width,
                    jpginfo.picture_param_buf.picture_height,
                    jpginfo.picture_param_buf.num_components);

                ROLLBACK_IF_FAIL((mBsParser->getRemainingBytes() >= jpginfo.picture_param_buf.num_components * 3));

                if (jpginfo.picture_param_buf.num_components > JPEG_MAX_COMPONENTS) {
                    ETRACE("ERROR: reached max components\n");
                    return JD_ERROR_BITSTREAM;
                }
#if 0
                if (jpginfo.picture_param_buf.picture_height < HW_DECODE_MIN_HEIGHT
                    || jpginfo.picture_param_buf.picture_width < HW_DECODE_MIN_WIDTH) {
                    VTRACE("PERFORMANCE: %ux%u JPEG will decode faster with SW\n",
                        jpginfo.picture_param_buf.picture_width,
                        jpginfo.picture_param_buf.picture_height);
                    return JD_IMAGE_TOO_SMALL;
                }
                if (jpginfo.picture_param_buf.picture_height > HW_DECODE_MAX_HEIGHT
                    || jpginfo.picture_param_buf.picture_width > HW_DECODE_MAX_WIDTH) {
                    VTRACE("PERFORMANCE: %ux%u HW JPEG will consume too much memory, use SW\n",
                        jpginfo.picture_param_buf.picture_width,
                        jpginfo.picture_param_buf.picture_height);
                    return JD_IMAGE_TOO_BIG;
                }
#endif
                uint8_t comp_ind = 0;
                for (comp_ind = 0; comp_ind < jpginfo.picture_param_buf.num_components; comp_ind++) {
                    ROLLBACK_IF_FAIL(mBsParser->tryReadNextByte(&jpginfo.picture_param_buf.components[comp_ind].component_id));
                    uint8_t hv_sampling;
                    ROLLBACK_IF_FAIL(mBsParser->tryReadNextByte(&hv_sampling));
                    jpginfo.picture_param_buf.components[comp_ind].h_sampling_factor = hv_sampling >> 4;
                    jpginfo.picture_param_buf.components[comp_ind].v_sampling_factor = hv_sampling & 0xf;
                    ROLLBACK_IF_FAIL(mBsParser->tryReadNextByte(&jpginfo.picture_param_buf.components[comp_ind].quantiser_table_selector));
                }
                jpginfo.image_width = jpginfo.picture_param_buf.picture_width;
                jpginfo.image_height = jpginfo.picture_param_buf.picture_height;
                jpginfo.image_color_fourcc = sampFactor2Fourcc(jpginfo.picture_param_buf.components[0].h_sampling_factor,
                    jpginfo.picture_param_buf.components[1].h_sampling_factor,
                    jpginfo.picture_param_buf.components[2].h_sampling_factor,
                    jpginfo.picture_param_buf.components[0].v_sampling_factor,
                    jpginfo.picture_param_buf.components[1].v_sampling_factor,
                    jpginfo.picture_param_buf.components[2].v_sampling_factor);

                VTRACE("%s jpg %ux%u, fourcc=%s",
                    __FUNCTION__, jpginfo.image_width, jpginfo.image_height, fourcc2str(jpginfo.image_color_fourcc));

                if (!jpegColorFormatSupported(jpginfo)) {
                    ETRACE("%s color format not supported", fourcc2str(jpginfo.image_color_fourcc));
                    return JD_COLORSPACE_UNSUPPORTED;
                }
                jpginfo.sof_parsed = true;
                break;
            }
            case CODE_DHT: {
                VTRACE("%s DHT at 0x%08x", __FUNCTION__, mBsParser->getByteOffset());
                //if (jpginfo.dht_ind < 4) {
                jpginfo.dht_byte_offset[jpginfo.dht_ind] = mBsParser->getByteOffset() - jpginfo.soi_offset;
                jpginfo.dht_ind++;
                uint32_t bytes_to_burn;
                if (!mBsParser->tryReadBytes(&bytes_to_burn, 2)) {
                    VTRACE("%s failed to read 2 bytes from 0x%08x, remaining 0x%08x, total 0x%08x",
                        __FUNCTION__, mBsParser->getByteOffset(),
                        mBsParser->getRemainingBytes(), jpginfo.bufsize);
                    jpginfo.dht_ind--;
                    goto rollback;
                }
                bytes_to_burn -= 2;
                if (!mBsParser->tryBurnBytes(bytes_to_burn)) {
                    VTRACE("%s failed to burn %x bytes from 0x%08x, remaining 0x%08x, total 0x%08x",
                        __FUNCTION__, bytes_to_burn, mBsParser->getByteOffset(),
                        mBsParser->getRemainingBytes(), jpginfo.bufsize);
                    jpginfo.dht_ind--;
                    goto rollback;
                }
                //} else {
                //    ETRACE("ERROR: Decoder does not support more than 4 Huff Tables\n");
                //    return JD_ERROR_BITSTREAM;
                //}
                jpginfo.dht_parsed = true;
                break;
            }
            // Parse component information in SOS marker
            case CODE_SOS: {
                VTRACE("%s SOS at 0x%08x", __FUNCTION__, mBsParser->getByteOffset());
                ROLLBACK_IF_FAIL(mBsParser->tryBurnBytes(2));
                uint8_t component_in_scan;
                ROLLBACK_IF_FAIL(mBsParser->tryReadNextByte(&component_in_scan));
                uint8_t comp_ind = 0;
                ROLLBACK_IF_FAIL((mBsParser->getRemainingBytes() >= (uint32_t)(2 * component_in_scan + 3)));
                for (comp_ind = 0; comp_ind < component_in_scan; comp_ind++) {
                    uint8_t comp_id = 0;
                    mBsParser->tryReadNextByte(&comp_id);
                    uint8_t comp_data_ind;
                    for (comp_data_ind = 0; comp_data_ind < jpginfo.picture_param_buf.num_components; comp_data_ind++) {
                        if (comp_id == jpginfo.picture_param_buf.components[comp_data_ind].component_id) {
                            jpginfo.slice_param_buf[jpginfo.scan_ind].components[comp_ind].component_selector = comp_data_ind + 1;
                            break;
                        }
                    }
                    uint8_t huffman_tables;
                    ROLLBACK_IF_FAIL(mBsParser->tryReadNextByte(&huffman_tables));
                    jpginfo.slice_param_buf[jpginfo.scan_ind].components[comp_ind].dc_table_selector = huffman_tables >> 4;
                    jpginfo.slice_param_buf[jpginfo.scan_ind].components[comp_ind].ac_table_selector = huffman_tables & 0xf;
                }
                uint8_t curr_byte;
                ROLLBACK_IF_FAIL(mBsParser->tryReadNextByte(&curr_byte)); // Ss
                if (curr_byte != 0) {
                    ETRACE("ERROR: curr_byte 0x%08x (position 0x%08x) != 0\n", curr_byte, mBsParser->getByteOffset());
                    return JD_ERROR_BITSTREAM;
                }
                ROLLBACK_IF_FAIL(mBsParser->tryReadNextByte(&curr_byte));  // Se
                if (curr_byte != 0x3f) {
                    ETRACE("ERROR: curr_byte 0x%08x (position 0x%08x) != 0x3f\n", curr_byte, mBsParser->getByteOffset());
                    return JD_ERROR_BITSTREAM;
                }
                ROLLBACK_IF_FAIL(mBsParser->tryReadNextByte(&curr_byte));  // Ah, Al
                if (curr_byte != 0) {
                    ETRACE("ERROR: curr_byte 0x%08x (position 0x%08x) != 0\n", curr_byte, mBsParser->getByteOffset());
                    return JD_ERROR_BITSTREAM;
                }
                // Set slice control variables needed
                jpginfo.slice_param_buf[jpginfo.scan_ind].slice_data_offset = mBsParser->getByteOffset() - jpginfo.soi_offset;
                jpginfo.slice_param_buf[jpginfo.scan_ind].num_components = component_in_scan;
                jpginfo.sos_parsed = true;
                if (jpginfo.scan_ind) {
                    /* If there is more than one scan, the slice for all but the final scan should only run up to the beginning of the next scan */
                    jpginfo.slice_param_buf[jpginfo.scan_ind - 1].slice_data_size =
                        (jpginfo.slice_param_buf[jpginfo.scan_ind].slice_data_offset - jpginfo.slice_param_buf[jpginfo.scan_ind - 1].slice_data_offset );;
                    }
                    jpginfo.scan_ind++;
                    jpginfo.scan_ctrl_count++;   // gsDXVA2Globals.uiScanCtrlCount
                    break;
                }
            case CODE_DRI: {
                rollbackoff = mBsParser->getByteOffset() - 2;
                VTRACE("%s DRI at 0x%08x", __FUNCTION__, mBsParser->getByteOffset());
                uint32_t size;
                ROLLBACK_IF_FAIL(mBsParser->tryReadBytes(&size, 2));
                uint32_t ri;
                ROLLBACK_IF_FAIL(mBsParser->tryReadBytes(&ri, 2));
                jpginfo.slice_param_buf[jpginfo.scan_ind].restart_interval = ri;
                ROLLBACK_IF_FAIL(mBsParser->tryBurnBytes(size - 4));
                jpginfo.dri_parsed = true;
                break;
            }
            default:
                break;
        }
        if (jpginfo.need_header_only &&
            jpginfo.soi_parsed && jpginfo.sos_parsed &&
            jpginfo.sof_parsed && jpginfo.dqt_parsed &&
            jpginfo.dht_parsed) {
            switch (jpginfo.picture_param_buf.num_components) {
            case 1:
                jpginfo.image_color_fourcc = VA_FOURCC('4', '0', '0', 'P');
                break;
            case 3:
                if (jpginfo.jfif_parsed) {
                }
                else if (jpginfo.adobe_parsed) {
                    if (jpginfo.adobe_transform == 0) {
                        // it should have been parsed as 444P, set correct color-space
                        jpginfo.image_color_fourcc = VA_FOURCC_RGBP;
                    }
                }
                else {
                    if (jpginfo.picture_param_buf.components[0].component_id == 82 &&
                        jpginfo.picture_param_buf.components[0].component_id == 71 &&
                        jpginfo.picture_param_buf.components[0].component_id == 66) {
                        // it should have been parsed as 444P, set correct color-space
                        jpginfo.image_color_fourcc = VA_FOURCC_RGBP;
                    }
                }
                break;
            default:
                ALOGE("%s: Unsupported component number", __FUNCTION__);
                return JD_COMPONENT_NUM_UNSUPPORTED;
            }
            if (jpginfo.image_color_fourcc == VA_FOURCC_RGBP) {
                ALOGE("%s: Unsupported jpeg_color_space: %s", __FUNCTION__, fourcc2str(jpginfo.image_color_fourcc));
                return JD_COLORSPACE_UNSUPPORTED;
            }
            VTRACE("%s: for header_only, we've got all what we need. return now", __FUNCTION__);
            return JD_SUCCESS;
        }
        else {
            VTRACE("%s: soi %d, sos %d, sof %d, dqt %d, dht %d, dri %d, remaining %u", __FUNCTION__,
                jpginfo.soi_parsed,
                jpginfo.sos_parsed,
                jpginfo.sof_parsed,
                jpginfo.dqt_parsed,
                jpginfo.dht_parsed,
                jpginfo.dri_parsed,
                mBsParser->getRemainingBytes());
        }
        rollbackoff = mBsParser->getByteOffset();
        if (!mBsParser->tryGetNextMarker(&marker)) {
            VTRACE("%s: can't get next marker, offset 0x%08x, need_header_only=%d",
                __FUNCTION__,
                mBsParser->getByteOffset(),
                jpginfo.need_header_only);
            if (jpginfo.need_header_only) {
                mBsParser->trySetByteOffset(rollbackoff);
                return JD_INSUFFICIENT_BYTE;
            }
            else {
                return JD_SUCCESS;
            }
        }
        else if (marker == 0) {
            VTRACE("%s: got non-marker %x at offset 0x%08x", __FUNCTION__, marker, mBsParser->getByteOffset());
            return JD_SUCCESS;
        }

        // If the EOI code is found, store the byte offset before the parsing finishes
        if( marker == CODE_EOI ) {
            jpginfo.eoi_offset = mBsParser->getByteOffset();
            VTRACE("%s: got EOI at 0x%08x, stop parsing now", __FUNCTION__, jpginfo.eoi_offset);
            return JD_SUCCESS;
        }
    }
    return JD_SUCCESS;
rollback:
    mBsParser->trySetByteOffset(rollbackoff);
    return JD_INSUFFICIENT_BYTE;
}

JpegDecodeStatus JpegDecoder::parse(JpegInfo &jpginfo)
{
    JpegDecodeStatus st = parseHeader(jpginfo);
    if (st) {
        if (st != JD_INSUFFICIENT_BYTE)
            ETRACE("%s header parsing failure: %d", __FUNCTION__, st);
        return st;
    }
    if (jpginfo.need_header_only)
        return JD_SUCCESS;

    if (!jpginfo.soi_parsed ||
        !jpginfo.sos_parsed ||
        !jpginfo.sof_parsed ||
        !jpginfo.dqt_parsed ||
        !jpginfo.dht_parsed) {
        ETRACE("%s Didn't get all soi/sos/sof/dqt/dht", __FUNCTION__);
        return JD_UNINITIALIZED;
    }
    jpginfo.quant_tables_num = jpginfo.dqt_ind;
    jpginfo.huffman_tables_num = jpginfo.dht_ind;

    /* The slice for the last scan should run up to the end of the picture */
    if (jpginfo.eoi_offset) {
        jpginfo.slice_param_buf[jpginfo.scan_ind - 1].slice_data_size = (jpginfo.eoi_offset - jpginfo.slice_param_buf[jpginfo.scan_ind - 1].slice_data_offset);
    }
    else {
        jpginfo.slice_param_buf[jpginfo.scan_ind - 1].slice_data_size = (jpginfo.bufsize - jpginfo.slice_param_buf[jpginfo.scan_ind - 1].slice_data_offset);
    }
    // throw AppException if SOF0 isn't found
    if (!jpginfo.frame_marker_found) {
        ETRACE("EEORR: Reached end of bitstream while trying to parse headers\n");
        return JD_ERROR_BITSTREAM;
    }

    JpegDecodeStatus status = parseTableData(jpginfo);
    if (status != JD_SUCCESS) {
        ETRACE("ERROR: Parsing table data returns %d", status);
        return status;
    }

    jpginfo.image_width = jpginfo.picture_param_buf.picture_width;
    jpginfo.image_height = jpginfo.picture_param_buf.picture_height;
    jpginfo.image_color_fourcc = sampFactor2Fourcc(jpginfo.picture_param_buf.components[0].h_sampling_factor,
        jpginfo.picture_param_buf.components[1].h_sampling_factor,
        jpginfo.picture_param_buf.components[2].h_sampling_factor,
        jpginfo.picture_param_buf.components[0].v_sampling_factor,
        jpginfo.picture_param_buf.components[1].v_sampling_factor,
        jpginfo.picture_param_buf.components[2].v_sampling_factor);

    VTRACE("%s jpg %ux%u, fourcc=%s",
        __FUNCTION__, jpginfo.image_width, jpginfo.image_height, fourcc2str(jpginfo.image_color_fourcc));

    if (!jpegColorFormatSupported(jpginfo)) {
        ETRACE("%s color format not supported", fourcc2str(jpginfo.image_color_fourcc));
        return JD_INPUT_FORMAT_UNSUPPORTED;
    }
    return JD_SUCCESS;
}

JpegDecodeStatus JpegDecoder::createSurfaceFromRenderTarget(const RenderTarget &target, VASurfaceID *surfid)
{
    switch (target.type) {
    case RenderTarget::KERNEL_DRM:
        {
            JpegDecodeStatus st = createSurfaceDrm(target.width,
                target.height,
                target.pixel_format,
                (unsigned long)target.handle,
                target.stride,
                surfid);
            if (st != JD_SUCCESS)
                return st;
            mDrmSurfaceMap.add((unsigned long)target.handle, *surfid);
            VTRACE("%s added surface %u (Drm handle %d) to DrmSurfaceMap",
                __PRETTY_FUNCTION__, *surfid, target.handle);
        }
        break;
    case RenderTarget::ANDROID_GRALLOC:
        {
            JpegDecodeStatus st = createSurfaceGralloc(target.width,
                target.height,
                target.pixel_format,
		(buffer_handle_t)(unsigned long)target.handle,
                target.stride,
                surfid);
            if (st != JD_SUCCESS)
                return st;
            mGrallocSurfaceMap.add((buffer_handle_t)(unsigned long)target.handle, *surfid);
            VTRACE("%s added surface %u (Gralloc handle %d) to DrmSurfaceMap",
                __PRETTY_FUNCTION__, *surfid, target.handle);
        }
        break;
    case RenderTarget::INTERNAL_BUF:
        {
            JpegDecodeStatus st = createSurfaceInternal(target.width,
                target.height,
                target.pixel_format,
                target.handle,
                surfid);
            if (st != JD_SUCCESS)
                return st;
            mNormalSurfaceMap.add(target.handle, *surfid);
            VTRACE("%s added surface %u (internal buffer id %d) to SurfaceList",
                __PRETTY_FUNCTION__, *surfid, target.handle);
        }
        break;
    case RenderTarget::USER_PTR:
        {
            JpegDecodeStatus st = createSurfaceUserptr(target.width,
                target.height,
                target.pixel_format,
		(uint8_t*)(unsigned long)target.handle,
                surfid);
            if (st != JD_SUCCESS)
                return st;
            mUserptrSurfaceMap.add(target.handle, *surfid);
            VTRACE("%s added surface %u (internal buffer id %d) to SurfaceList",
                __PRETTY_FUNCTION__, *surfid, target.handle);
        }
        break;
    default:
        return JD_RENDER_TARGET_TYPE_UNSUPPORTED;
    }
    return JD_SUCCESS;
}

JpegDecodeStatus JpegDecoder::createSurfaceInternal(int width, int height, uint32_t fourcc, int handle, VASurfaceID *surf_id)
{
    VAStatus va_status;
    VASurfaceAttrib attrib;
    attrib.type = VASurfaceAttribPixelFormat;
    attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;
    attrib.value.type = VAGenericValueTypeInteger;
    uint32_t vaformat = fourcc2VaFormat(fourcc);
    attrib.value.value.i = fourcc;
    VTRACE("enter %s, wxh=%dx%d, fourcc %s", __FUNCTION__, width, height, fourcc2str(fourcc));
    va_status = vaCreateSurfaces(mDisplay,
                                vaformat,
                                width,
                                height,
                                surf_id,
                                1,
                                &attrib,
                                1);
    if (va_status != VA_STATUS_SUCCESS) {
        ETRACE("%s: createSurface (format %u, fourcc %s) returns %d", __PRETTY_FUNCTION__, vaformat, fourcc2str(fourcc), va_status);
        return JD_RESOURCE_FAILURE;
    }
    return JD_SUCCESS;
}

JpegDecodeStatus JpegDecoder::destroySurface(RenderTarget &target)
{
    Mutex::Autolock autoLock(mLock);
    VASurfaceID surf = getSurfaceID(target);
    if (surf == VA_INVALID_ID) {
        ETRACE("%s: failed to destroy surface type %d, handle %d", __FUNCTION__, target.type, target.handle);
        return JD_INVALID_RENDER_TARGET;
    }
    switch(target.type) {
    case RenderTarget::KERNEL_DRM:
        mDrmSurfaceMap.removeItem((unsigned long)target.handle);
        break;
    case RenderTarget::ANDROID_GRALLOC:
        mGrallocSurfaceMap.removeItem((buffer_handle_t)(unsigned long)target.handle);
        break;
    case RenderTarget::INTERNAL_BUF:
        mNormalSurfaceMap.removeItem(target.handle);
        break;
    case RenderTarget::USER_PTR:
        mUserptrSurfaceMap.removeItem(target.handle);
        break;
    default:
        break;
    }
    VTRACE("%s: succeeded destroying surface type %d, handle %d", __FUNCTION__, target.type, target.handle);
    return JD_SUCCESS;
}

JpegDecodeStatus JpegDecoder::destroySurface(VASurfaceID surf)
{
    return JD_UNIMPLEMENTED;
}

VASurfaceID JpegDecoder::getSurfaceID(const RenderTarget &target) const
{
    int index;
    switch (target.type) {
    case RenderTarget::KERNEL_DRM:
        index = mDrmSurfaceMap.indexOfKey((unsigned long)target.handle);
        if (index < 0)
            return VA_INVALID_ID;
        else
            return mDrmSurfaceMap.valueAt(index);
    case RenderTarget::ANDROID_GRALLOC:
        index = mGrallocSurfaceMap.indexOfKey((buffer_handle_t)(unsigned long)target.handle);
        if (index < 0)
            return VA_INVALID_ID;
        else
            return mGrallocSurfaceMap.valueAt(index);
    case RenderTarget::INTERNAL_BUF:
        index = mNormalSurfaceMap.indexOfKey(target.handle);
        if (index < 0)
            return VA_INVALID_ID;
        else
            return mNormalSurfaceMap.valueAt(index);
    case RenderTarget::USER_PTR:
        index = mUserptrSurfaceMap.indexOfKey(target.handle);
        if (index < 0)
            return VA_INVALID_ID;
        else
            return mUserptrSurfaceMap.valueAt(index);
    default:
        ETRACE("%s invalid surface type", __FUNCTION__);
        return VA_INVALID_ID;
    }
    return VA_INVALID_ID;
}

JpegDecodeStatus JpegDecoder::sync(RenderTarget &target)
{
    VASurfaceID surf_id = getSurfaceID(target);
    if (surf_id == VA_INVALID_ID)
        return JD_INVALID_RENDER_TARGET;
    vaSyncSurface(mDisplay, surf_id);
    return JD_SUCCESS;
}
bool JpegDecoder::busy(RenderTarget &target) const
{
    VASurfaceStatus surf_st;
    VASurfaceID surf_id = getSurfaceID(target);
    if (surf_id == VA_INVALID_ID)
        return false;
    VAStatus st = vaQuerySurfaceStatus(mDisplay, surf_id, &surf_st);
    if (st != VA_STATUS_SUCCESS)
        return false;
    return surf_st != VASurfaceReady;
}


JpegDecodeStatus JpegDecoder::decode(JpegInfo &jpginfo, const RenderTarget &target)
{
    VAStatus va_status = VA_STATUS_SUCCESS;
    VASurfaceStatus surf_status;
    VABufferID desc_buf[5];
    uint32_t bitstream_buffer_size = 0;
    uint8_t* bufaddr = NULL;
    uint32_t scan_idx = 0;
    uint32_t buf_idx = 0;
    uint32_t chopping = VA_SLICE_DATA_FLAG_ALL;
    uint32_t bytes_remaining;
    VASurfaceID surf_id = getSurfaceID(target);
    nsecs_t now = systemTime();
    if (surf_id == VA_INVALID_ID) {
        ETRACE("%s render_target %p, handle %d is not initailized by JpegDecoder", __FUNCTION__, &target, target.handle);
        return JD_RENDER_TARGET_NOT_INITIALIZED;
    }
    va_status = vaQuerySurfaceStatus(mDisplay, surf_id, &surf_status);
    if (surf_status != VASurfaceReady) {
        ETRACE("%s render_target %p, handle %d is still busy", __FUNCTION__, &target, target.handle);
        return JD_RENDER_TARGET_BUSY;
    }

    if (jpginfo.use_vector_input) {
        bitstream_buffer_size = jpginfo.inputs->size();
        bufaddr = const_cast<uint8_t*>(jpginfo.inputs->array());
    }
    else {
        bitstream_buffer_size = jpginfo.bufsize;
        bufaddr = jpginfo.buf;
    }

    if (jpginfo.eoi_offset)
        bytes_remaining = jpginfo.eoi_offset - jpginfo.soi_offset;
    else
        bytes_remaining = bitstream_buffer_size - jpginfo.soi_offset;

    uint32_t src_offset = jpginfo.soi_offset;
    uint32_t cpy_row;

    Vector<VABufferID> buf_list;
    va_status = vaBeginPicture(mDisplay, mContextId, surf_id);
    if (va_status != VA_STATUS_SUCCESS) {
        ETRACE("vaBeginPicture failed. va_status = 0x%x", va_status);
        return JD_DECODE_FAILURE;
    }
    VTRACE("%s begin decode render target %p, handle %d", __FUNCTION__, &target, target.handle);
    va_status = vaCreateBuffer(mDisplay, mContextId, VAPictureParameterBufferType, sizeof(VAPictureParameterBufferJPEGBaseline), 1, &jpginfo.picture_param_buf, &desc_buf[buf_idx]);
    if (va_status != VA_STATUS_SUCCESS) {
        ETRACE("vaCreateBuffer VAPictureParameterBufferType failed. va_status = 0x%x", va_status);
        return JD_RESOURCE_FAILURE;
    }
    VTRACE("%s successfully created PicParamBuf, id=%u", __FUNCTION__, desc_buf[buf_idx]);
    buf_list.add(desc_buf[buf_idx++]);

    va_status = vaCreateBuffer(mDisplay, mContextId, VAIQMatrixBufferType, sizeof(VAIQMatrixBufferJPEGBaseline), 1, &jpginfo.qmatrix_buf, &desc_buf[buf_idx]);
    if (va_status != VA_STATUS_SUCCESS) {
        ETRACE("vaCreateBuffer VAIQMatrixBufferType failed. va_status = 0x%x", va_status);
        return JD_RESOURCE_FAILURE;
    }
    VTRACE("%s successfully created IQMatrixBuf, id=%u", __FUNCTION__, desc_buf[buf_idx]);
    buf_list.add(desc_buf[buf_idx++]);

    va_status = vaCreateBuffer(mDisplay, mContextId, VAHuffmanTableBufferType, sizeof(VAHuffmanTableBufferJPEGBaseline), 1, &jpginfo.hufman_table_buf, &desc_buf[buf_idx]);
    if (va_status != VA_STATUS_SUCCESS) {
        ETRACE("vaCreateBuffer VAHuffmanTableBufferType failed. va_status = 0x%x", va_status);
        return JD_RESOURCE_FAILURE;
    }
    VTRACE("%s successfully created HuffmanTableBuf, id=%u", __FUNCTION__, desc_buf[buf_idx]);
    buf_list.add(desc_buf[buf_idx++]);

    do {
        /* Get Bitstream Buffer */
        uint32_t bytes = ( bytes_remaining < bitstream_buffer_size ) ? bytes_remaining : bitstream_buffer_size;
        bytes_remaining -= bytes;
        /* Get Slice Control Buffer */
        VASliceParameterBufferJPEGBaseline dest_scan_ctrl[JPEG_MAX_COMPONENTS];
        uint32_t src_idx = 0;
        uint32_t dest_idx = 0;
        memset(dest_scan_ctrl, 0, sizeof(dest_scan_ctrl));
        for (src_idx = scan_idx; src_idx < jpginfo.scan_ctrl_count ; src_idx++) {
            if (jpginfo.slice_param_buf[ src_idx ].slice_data_offset) {
                /* new scan, reset state machine */
                chopping = VA_SLICE_DATA_FLAG_ALL;
                VTRACE("Scan:%i FileOffset:%x Bytes:%x \n", src_idx,
                    jpginfo.slice_param_buf[ src_idx ].slice_data_offset,
                    jpginfo.slice_param_buf[ src_idx ].slice_data_size );
                /* does the slice end in the buffer */
                if (jpginfo.slice_param_buf[ src_idx ].slice_data_offset + jpginfo.slice_param_buf[ src_idx ].slice_data_size > bytes + src_offset) {
                    chopping = VA_SLICE_DATA_FLAG_BEGIN;
                }
            } else {
                if (jpginfo.slice_param_buf[ src_idx ].slice_data_size > bytes) {
                    chopping = VA_SLICE_DATA_FLAG_MIDDLE;
                } else {
                    if ((chopping == VA_SLICE_DATA_FLAG_BEGIN) || (chopping == VA_SLICE_DATA_FLAG_MIDDLE)) {
                        chopping = VA_SLICE_DATA_FLAG_END;
                    }
                }
            }
            dest_scan_ctrl[dest_idx].slice_data_flag = chopping;

            if ((chopping == VA_SLICE_DATA_FLAG_ALL) || (chopping == VA_SLICE_DATA_FLAG_BEGIN))
                dest_scan_ctrl[dest_idx].slice_data_offset = jpginfo.slice_param_buf[ src_idx ].slice_data_offset;
            else
                dest_scan_ctrl[dest_idx].slice_data_offset = 0;

            const int32_t bytes_in_seg = bytes - dest_scan_ctrl[dest_idx].slice_data_offset;
            const uint32_t scan_data = ((uint32_t)bytes_in_seg < jpginfo.slice_param_buf[src_idx].slice_data_size) ? bytes_in_seg : jpginfo.slice_param_buf[src_idx].slice_data_size ;
            jpginfo.slice_param_buf[src_idx].slice_data_offset = 0;
            jpginfo.slice_param_buf[src_idx].slice_data_size -= scan_data;
            dest_scan_ctrl[dest_idx].slice_data_size = scan_data;
            dest_scan_ctrl[dest_idx].num_components = jpginfo.slice_param_buf[src_idx].num_components;
            dest_scan_ctrl[dest_idx].restart_interval = jpginfo.slice_param_buf[src_idx].restart_interval;
            memcpy(&dest_scan_ctrl[dest_idx].components, & jpginfo.slice_param_buf[ src_idx ].components,
                sizeof(jpginfo.slice_param_buf[ src_idx ].components) );
            dest_idx++;
            if ((chopping == VA_SLICE_DATA_FLAG_ALL) || (chopping == VA_SLICE_DATA_FLAG_END)) { /* all good good */
            } else {
                break;
            }
        }
        scan_idx = src_idx;
        /* Get Slice Control Buffer */
        va_status = vaCreateBuffer(mDisplay, mContextId, VASliceParameterBufferType, sizeof(VASliceParameterBufferJPEGBaseline) * dest_idx, 1, dest_scan_ctrl, &desc_buf[buf_idx]);
        if (va_status != VA_STATUS_SUCCESS) {
            ETRACE("vaCreateBuffer VASliceParameterBufferType failed. va_status = 0x%x, dest_idx=%d, buf_idx=%d", va_status, dest_idx, buf_idx);
            return JD_RESOURCE_FAILURE;
        }
        VTRACE("vaCreateBuffer VASliceParameterBufferType succeeded. va_status = 0x%x, dest_idx=%d, buf_idx=%d", va_status, dest_idx, buf_idx);
        buf_list.add(desc_buf[buf_idx++]);
        va_status = vaCreateBuffer(mDisplay, mContextId, VASliceDataBufferType, bytes, 1, bufaddr + src_offset, &desc_buf[buf_idx]);
        if (va_status != VA_STATUS_SUCCESS) {
            ETRACE("vaCreateBuffer VASliceDataBufferType (%u bytes) failed. va_status = 0x%x", bytes, va_status);
            return JD_RESOURCE_FAILURE;
        }
        VTRACE("%s successfully created SliceDataBuf, id=%u", __FUNCTION__, desc_buf[buf_idx]);
        buf_list.add(desc_buf[buf_idx++]);
        va_status = vaRenderPicture( mDisplay, mContextId, desc_buf, buf_idx);
        if (va_status != VA_STATUS_SUCCESS) {
            ETRACE("vaRenderPicture failed. va_status = 0x%x", va_status);
            return JD_DECODE_FAILURE;
        }
        buf_idx = 0;

        src_offset += bytes;
    } while (bytes_remaining);

    va_status = vaEndPicture(mDisplay, mContextId);
    VTRACE("vaEndPicture returns %d", va_status);

    while(buf_list.size() > 0) {
        va_status = vaDestroyBuffer(mDisplay, buf_list.top());
        VTRACE("vaDestroyBuffer %u returns %d", buf_list.top(), va_status);
        buf_list.pop();
    }
    if (va_status != VA_STATUS_SUCCESS) {
        ETRACE("vaEndPicture failed. va_status = 0x%x", va_status);
        return JD_DECODE_FAILURE;
    }

    VTRACE("%s successfully ended picture, rendertarget %p, handle %d", __FUNCTION__, &target, target.handle);
    VTRACE("JpegDecoder decode took %.2f ms", (systemTime() - now)/1000000.0);
    return JD_SUCCESS;
}
void JpegDecoder::deinit()
{
    if (mInitialized) {
        Mutex::Autolock autoLock(mLock);
        if (mInitialized) {
            vaDestroyContext(mDisplay, mContextId);
            vaDestroyConfig(mDisplay, mConfigId);
            mInitialized = false;
            size_t gralloc_size = mGrallocSurfaceMap.size();
            size_t drm_size = mDrmSurfaceMap.size();
            size_t internal_surf_size = mNormalSurfaceMap.size();
            size_t up_surf_size = mUserptrSurfaceMap.size();
            for (size_t i = 0; i < gralloc_size; ++i) {
                VASurfaceID surf_id = mGrallocSurfaceMap.valueAt(i);
                vaDestroySurfaces(mDisplay, &surf_id, 1);
            }
            for (size_t i = 0; i < drm_size; ++i) {
                VASurfaceID surf_id = mDrmSurfaceMap.valueAt(i);
                vaDestroySurfaces(mDisplay, &surf_id, 1);
            }
            for (size_t i = 0; i < internal_surf_size; ++i) {
                VASurfaceID surf_id = mNormalSurfaceMap.valueAt(i);
                vaDestroySurfaces(mDisplay, &surf_id, 1);
            }
            for (size_t i = 0; i < up_surf_size; ++i) {
                VASurfaceID surf_id = mUserptrSurfaceMap.valueAt(i);
                vaDestroySurfaces(mDisplay, &surf_id, 1);
            }
            mGrallocSurfaceMap.clear();
            mDrmSurfaceMap.clear();
            mNormalSurfaceMap.clear();
            mUserptrSurfaceMap.clear();
        }
    }
}

JpegDecodeStatus JpegDecoder::parseTableData(JpegInfo &jpginfo) {
#define REPORT_BS_ERR_IF_FAIL(stmt) \
            do { \
                if (!stmt) { \
                    ETRACE("%s::%d, bitstream error at offset %u, remaining bytes %u, total bytes %zu", \
                        __FUNCTION__, __LINE__, mBsParser->getByteOffset(), mBsParser->getRemainingBytes(), \
                        bufsize); \
                    return JD_ERROR_BITSTREAM; \
                } \
            } while(0);

    memset(&jpginfo.qmatrix_buf, 0, sizeof(jpginfo.qmatrix_buf));
    uint32_t dqt_ind = 0;
    uint32_t bufsize;

    if (jpginfo.use_vector_input)
        bufsize = jpginfo.inputs->size();
    else
        bufsize = jpginfo.bufsize;

    for (dqt_ind = 0; dqt_ind < jpginfo.quant_tables_num; dqt_ind++) {
        REPORT_BS_ERR_IF_FAIL(mBsParser->trySetByteOffset(jpginfo.dqt_byte_offset[dqt_ind]));
        uint32_t table_bytes;
        REPORT_BS_ERR_IF_FAIL(mBsParser->tryReadBytes(&table_bytes, 2 ));
        table_bytes -= 2;
        do {
            uint8_t table_info;
            REPORT_BS_ERR_IF_FAIL(mBsParser->tryReadNextByte(&table_info));
            table_bytes--;
            uint32_t table_length = table_bytes > 64 ? 64 : table_bytes;
            uint32_t table_precision = table_info >> 4;
            REPORT_BS_ERR_IF_FAIL ((table_precision == 0));
            uint32_t table_id = table_info & 0xf;

            jpginfo.qmatrix_buf.load_quantiser_table[table_id] = 1;

            if (table_id < JPEG_MAX_QUANT_TABLES) {
                // Pull Quant table data from bitstream
                uint32_t byte_ind;
                for (byte_ind = 0; byte_ind < table_length; byte_ind++) {
                    REPORT_BS_ERR_IF_FAIL(mBsParser->tryReadNextByte(&jpginfo.qmatrix_buf.quantiser_table[table_id][byte_ind]));
                }
            } else {
                ETRACE("%s DQT table ID is not supported", __FUNCTION__);
                REPORT_BS_ERR_IF_FAIL(mBsParser->tryBurnBytes(table_length));
            }
            table_bytes -= table_length;
        } while (table_bytes);
    }

    // Parse Huffman tables
    memset(&jpginfo.hufman_table_buf, 0, sizeof(jpginfo.hufman_table_buf));
    uint32_t dht_ind = 0;
    for (dht_ind = 0; dht_ind < jpginfo.huffman_tables_num; dht_ind++) {
        REPORT_BS_ERR_IF_FAIL(mBsParser->trySetByteOffset(jpginfo.dht_byte_offset[dht_ind]));
        uint32_t table_bytes;
        REPORT_BS_ERR_IF_FAIL(mBsParser->tryReadBytes( &table_bytes, 2 ));
        table_bytes -= 2;
        do {
            uint8_t table_info;
            REPORT_BS_ERR_IF_FAIL(mBsParser->tryReadNextByte(&table_info));
            table_bytes--;
            uint32_t table_class = table_info >> 4; // Identifies whether the table is for AC or DC
            uint32_t table_id = table_info & 0xf;
            jpginfo.hufman_table_buf.load_huffman_table[table_id] = 1;

            if ((table_class < TABLE_CLASS_NUM) && (table_id < JPEG_MAX_SETS_HUFFMAN_TABLES)) {
                if (table_class == 0) {
                    //const uint8_t* bits = mBsParser->getCurrentIndex();
                    // Find out the number of entries in the table
                    uint32_t table_entries = 0;
                    uint32_t bit_ind;
                    for (bit_ind = 0; bit_ind < 16; bit_ind++) {
                        jpginfo.hufman_table_buf.huffman_table[table_id].num_dc_codes[bit_ind] = mBsParser->itemAt(mBsParser->getByteOffset() + bit_ind);
                        table_entries += jpginfo.hufman_table_buf.huffman_table[table_id].num_dc_codes[bit_ind];
                    }

                    // Create table of code values
                    REPORT_BS_ERR_IF_FAIL(mBsParser->tryBurnBytes(16));
                    table_bytes -= 16;
                    uint32_t tbl_ind;
                    for (tbl_ind = 0; tbl_ind < table_entries; tbl_ind++) {
                        REPORT_BS_ERR_IF_FAIL(mBsParser->tryReadNextByte(&jpginfo.hufman_table_buf.huffman_table[table_id].dc_values[tbl_ind]));
                        table_bytes--;
                    }

                } else { // for AC class
                    //const uint8_t* bits = mBsParser->getCurrentIndex();
                    // Find out the number of entries in the table
                    uint32_t table_entries = 0;
                    uint32_t bit_ind = 0;
                    for (bit_ind = 0; bit_ind < 16; bit_ind++) {
                        jpginfo.hufman_table_buf.huffman_table[table_id].num_ac_codes[bit_ind] = mBsParser->itemAt(mBsParser->getByteOffset() + bit_ind);//bits[bit_ind];
                        table_entries += jpginfo.hufman_table_buf.huffman_table[table_id].num_ac_codes[bit_ind];
                    }

                    // Create table of code values
                    REPORT_BS_ERR_IF_FAIL(mBsParser->tryBurnBytes(16));
                    table_bytes -= 16;
                    uint32_t tbl_ind = 0;
                    for (tbl_ind = 0; tbl_ind < table_entries; tbl_ind++) {
                        REPORT_BS_ERR_IF_FAIL(mBsParser->tryReadNextByte(&jpginfo.hufman_table_buf.huffman_table[table_id].ac_values[tbl_ind]));
                        table_bytes--;
                    }
                }//end of else
            } else {
                // Find out the number of entries in the table
                ETRACE("%s DHT table ID is not supported", __FUNCTION__);
                uint32_t table_entries = 0;
                uint32_t bit_ind = 0;
                for(bit_ind = 0; bit_ind < 16; bit_ind++) {
                    uint8_t tmp;
                    REPORT_BS_ERR_IF_FAIL(mBsParser->tryReadNextByte(&tmp));
                    table_entries += tmp;
                    table_bytes--;
                }
                REPORT_BS_ERR_IF_FAIL(mBsParser->tryBurnBytes(table_entries));
                table_bytes -= table_entries;
            }

        } while (table_bytes);
    }

    return JD_SUCCESS;
}

