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

#include "JPEGDecoder.h"
#include "JPEGBlitter.h"
#include "JPEGCommon_Gen.h"
#include <utils/threads.h>
#include <utils/Timers.h>
#include <stdio.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <hardware/gralloc.h>

static char jpgfile[100];

RenderTarget& init_render_target_drm(RenderTarget &target, int width, int height, uint32_t fourcc, buffer_handle_t *handle)
{
    hw_module_t const* module = NULL;
    alloc_device_t *allocdev = NULL;
    struct gralloc_module_t *gralloc_module = NULL;
    int stride, bpp, err;
    bpp = fourcc2LumaBitsPerPixel(fourcc);
    err = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module);
    if (err || !module) {
        printf("%s failed to get gralloc module\n", __PRETTY_FUNCTION__);
        assert(false);
    }
    gralloc_module = (struct gralloc_module_t *)module;
    err = gralloc_open(module, &allocdev);
    if (err || !allocdev) {
        printf("%s failed to open alloc device\n", __PRETTY_FUNCTION__);
        assert(false);
    }
    err = allocdev->alloc(allocdev,
            width,
            height,
            fourcc2PixelFormat(fourcc),
            GRALLOC_USAGE_HW_RENDER,
            handle,
            &stride);
    if (err) {
        gralloc_close(allocdev);
        printf("%s failed to allocate surface %d, %dx%d, pixelformat %x\n", __PRETTY_FUNCTION__, err,
            width, height, fourcc2PixelFormat(fourcc));
        assert(false);
    }
    unsigned long boname;
    err = gralloc_module->perform(gralloc_module,
        INTEL_UFO_GRALLOC_MODULE_PERFORM_GET_BO_NAME,
        *handle,
        &boname);
    assert(!err);
    target.type = RenderTarget::KERNEL_DRM;
    target.handle = (int)boname;
    switch(fourcc) {
    case VA_FOURCC_NV12:
    case VA_FOURCC_422H:
    case VA_FOURCC_422V:
    case VA_FOURCC_IMC3:
    case VA_FOURCC_444P:
    case VA_FOURCC_411P:
    case VA_FOURCC('4','0','0','P'):
        target.width = aligned_width(width, SURF_TILING_Y);
        target.height = aligned_height(height, SURF_TILING_Y);
        break;
    default:
        target.width = aligned_width(width, SURF_TILING_NONE);
        target.height = aligned_height(height, SURF_TILING_NONE);
        break;
    }
    target.format = fourcc2VaFormat(fourcc);
    target.pixel_format = fourcc;
    target.rect.x = target.rect.y = 0;
    target.rect.width = width;
    target.rect.height = height;
    target.stride = stride * bpp;
    gralloc_close(allocdev);
    return target;
}

RenderTarget& init_render_target_gralloc(RenderTarget &target, int width, int height, uint32_t fourcc)
{
    hw_module_t const* module = NULL;
    alloc_device_t *allocdev = NULL;
    struct gralloc_module_t *gralloc_module = NULL;
    buffer_handle_t handle;
    int stride, bpp, err;
    bpp = fourcc2LumaBitsPerPixel(fourcc);
    err = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module);
    if (err || !module) {
        printf("%s failed to get gralloc module\n", __PRETTY_FUNCTION__);
        assert(false);
    }
    gralloc_module = (struct gralloc_module_t *)module;
    err = gralloc_open(module, &allocdev);
    if (err || !allocdev) {
        printf("%s failed to open alloc device\n", __PRETTY_FUNCTION__);
        assert(false);
    }
    err = allocdev->alloc(allocdev,
            width,
            height,
            fourcc2PixelFormat(fourcc),
            GRALLOC_USAGE_HW_RENDER,
            &handle,
            &stride);
    if (err) {
        gralloc_close(allocdev);
        printf("%s failed to allocate surface %d, %dx%d, pixelformat %x\n", __PRETTY_FUNCTION__, err,
            width, height, fourcc2PixelFormat(fourcc));
        assert(false);
    }
    target.type = RenderTarget::ANDROID_GRALLOC;
    target.handle = (int)handle;
    switch(fourcc) {
    case VA_FOURCC_NV12:
    case VA_FOURCC_YUY2:
    case VA_FOURCC_UYVY:
    case VA_FOURCC_422H:
    case VA_FOURCC_422V:
    case VA_FOURCC_IMC3:
    case VA_FOURCC_444P:
    case VA_FOURCC_411P:
    case VA_FOURCC('4','0','0','P'):
        target.width = aligned_width(width, SURF_TILING_Y);
        target.height = aligned_height(height, SURF_TILING_Y);
        break;
    default:
        target.width = aligned_width(width, SURF_TILING_NONE);
        target.height = aligned_height(height, SURF_TILING_NONE);
        break;
    }

    target.format = fourcc2VaFormat(fourcc);
    target.pixel_format = fourcc;
    target.rect.x = target.rect.y = 0;
    target.rect.width = width;
    target.rect.height = height;
    target.stride = stride * bpp;
    gralloc_close(allocdev);
    return target;
}

RenderTarget& init_render_target_userptr(RenderTarget &target, int width, int height, uint32_t fourcc)
{
    hw_module_t const* module = NULL;
    alloc_device_t *allocdev = NULL;
    static int surf_hnd = 0;
    int stride, bpp, err;
    void * userptr = NULL;
    size_t mallocsize;
    bpp = fourcc2LumaBitsPerPixel(fourcc);
    target.type = RenderTarget::USER_PTR;

    // all linear, no alignment
    switch(fourcc) {
    case VA_FOURCC_NV12:
        mallocsize = width * height * 3 / 2;
        break;
    case VA_FOURCC_YUY2:
    case VA_FOURCC_UYVY:
        mallocsize = width * height * 2;
        break;
    case VA_FOURCC_422H:
        mallocsize = width * height * 3;
        break;
    case VA_FOURCC_422V:
        mallocsize = width * height * 2;
        break;
    case VA_FOURCC_IMC3:
        mallocsize = width * height * 2;
        break;
    case VA_FOURCC_444P:
        mallocsize = width * height * 3;
        break;
    case VA_FOURCC_411P:
        mallocsize = width * height * 3;
        break;
    case VA_FOURCC_411R:
        mallocsize = width * height * 3 / 2;
        break;
    case VA_FOURCC('4','0','0','P'):
        mallocsize = width * height;
        break;
    case VA_FOURCC_RGBA:
    case VA_FOURCC_BGRA:
        mallocsize = width * height * 4;
        break;
    default:
        mallocsize = width * height * 3;
        break;
    }
    userptr = memalign(0x1000, mallocsize);
    target.width = width;
    target.height = height;
    target.pixel_format = fourcc;
    target.rect.x = target.rect.y = 0;
    target.rect.width = target.width;
    target.rect.height = target.height;
    target.handle = (int)userptr;
    //target.stride = stride * bpp;
    return target;
}

RenderTarget& init_render_target(RenderTarget &target, int width, int height, uint32_t fourcc)
{
    hw_module_t const* module = NULL;
    alloc_device_t *allocdev = NULL;
    static int surf_hnd = 0;
    int stride, bpp, err;
    bpp = fourcc2LumaBitsPerPixel(fourcc);
    target.type = RenderTarget::INTERNAL_BUF;
    target.handle = generateHandle();
    switch(fourcc) {
    case VA_FOURCC_NV12:
    case VA_FOURCC_YUY2:
    case VA_FOURCC_UYVY:
    case VA_FOURCC_422H:
    case VA_FOURCC_422V:
    case VA_FOURCC_IMC3:
    case VA_FOURCC_444P:
    case VA_FOURCC_411P:
    case VA_FOURCC('4','0','0','P'):
        target.width = aligned_width(width, SURF_TILING_Y);
        target.height = aligned_height(height, SURF_TILING_Y);
        break;
    default:
        target.width = aligned_width(width, SURF_TILING_NONE);
        target.height = aligned_height(height, SURF_TILING_NONE);
        break;
    }
    target.pixel_format = fourcc;
    target.rect.x = target.rect.y = 0;
    target.rect.width = target.width;
    target.rect.height = target.height;
    //target.stride = stride * bpp;
    return target;
}

void deinit_render_target(RenderTarget &target, buffer_handle_t *handle = NULL)
{
    hw_module_t const* module = NULL;
    alloc_device_t *allocdev = NULL;
    struct gralloc_module_t *gralloc_module = NULL;
    int err = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module);
    if (err || !module) {
        printf("%s failed to get gralloc module\n", __PRETTY_FUNCTION__);
        return;
    }
    gralloc_module = (struct gralloc_module_t *)module;
    err = gralloc_open(module, &allocdev);
    if (err || !allocdev) {
        printf("%s failed to get gralloc module\n", __PRETTY_FUNCTION__);
        return;
    }
    if (handle && target.type == RenderTarget::KERNEL_DRM)
        allocdev->free(allocdev, *handle);
    else if (target.type == RenderTarget::ANDROID_GRALLOC)
        allocdev->free(allocdev, (buffer_handle_t)target.handle);
    else if (target.type == RenderTarget::USER_PTR)
        free((void*)target.handle);
    gralloc_close(allocdev);
}

void decode_blit_functionality_test(RenderTarget::bufType type, uint32_t format, int scale_factor)
{
    JpegDecodeStatus st;
    VAStatus vast;
    JpegInfo jpginfo;
    hw_module_t const* module = NULL;
    alloc_device_t *allocdev = NULL;
    struct gralloc_module_t *gralloc_module = NULL;
    VAStatus vret;
    char decdumpfile[100];
    char origdecdumpfile[100];
    char nv12dumpfile[100];
    char nv21dumpfile[100];
    char yuy2dumpfile[100];
    char yv12dumpfile[100];
    char rgbadumpfile[100];
    FILE* fpdump = NULL;
    memset(&jpginfo, 0, sizeof(JpegInfo));
    memset(decdumpfile, 0, sizeof(decdumpfile));
    memset(origdecdumpfile, 0, sizeof(origdecdumpfile));
    memset(nv12dumpfile, 0, sizeof(nv12dumpfile));
    memset(nv21dumpfile, 0, sizeof(nv21dumpfile));
    memset(yuy2dumpfile, 0, sizeof(yuy2dumpfile));
    memset(yv12dumpfile, 0, sizeof(yv12dumpfile));
    memset(rgbadumpfile, 0, sizeof(rgbadumpfile));
    VADisplay display = NULL;
    VAConfigID vpCfgId = VA_INVALID_ID;
    VAContextID vpCtxId = VA_INVALID_ID;
    typedef uint32_t Display;
    Display dpy;
    int va_major_version, va_minor_version;
    VAConfigAttrib  vpp_attrib;
    display = vaGetDisplay(&dpy);
    vast = vaInitialize(display, &va_major_version, &va_minor_version);
    assert(vast == VA_STATUS_SUCCESS);
    vpp_attrib.type  = VAConfigAttribRTFormat;
    vpp_attrib.value = VA_RT_FORMAT_YUV420;
    vast = vaCreateConfig(display, VAProfileNone,
                                VAEntrypointVideoProc,
                                &vpp_attrib,
                                1, &vpCfgId);
    assert(vast == VA_STATUS_SUCCESS);
    vast = vaCreateContext(display, vpCfgId, 1920, 1080, 0, NULL, 0, &vpCtxId);
    assert(vast == VA_STATUS_SUCCESS);
    JpegDecoder decoder(display, vpCfgId, vpCtxId, true);
    decoder.preInit(display);
    RenderTarget dec_target;
    buffer_handle_t dec_handle, nv12_handle, yuy2_handle;
    uint8_t *nv12_mem, *yuy2_mem, *nv21_mem, *yv12_mem, *rgba_mem;
    int stride;
    FILE* fp = fopen(jpgfile, "rb");
    assert(fp);
    fseek(fp, 0, SEEK_END);
    jpginfo.bufsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    jpginfo.buf = new uint8_t[jpginfo.bufsize];
    fread(jpginfo.buf, 1, jpginfo.bufsize, fp);
    fclose(fp);

    printf("finished loading src file: size %u\n", jpginfo.bufsize);
    jpginfo.need_header_only = false;
    jpginfo.use_vector_input = false;
    st = decoder.parse(jpginfo);
    assert(st == JD_SUCCESS);
    printf("parse succeeded: %ux%u\n", jpginfo.image_width, jpginfo.image_height);

    if (format == 0)
        format = jpginfo.image_color_fourcc;

    char buftypename[100];
    switch(type) {
    case RenderTarget::KERNEL_DRM:
        sprintf(buftypename, "DRM");
        init_render_target_drm(dec_target, jpginfo.image_width, jpginfo.image_height, format, &dec_handle);
        break;
    case RenderTarget::ANDROID_GRALLOC:
        sprintf(buftypename, "GRALLOC");
        init_render_target_gralloc(dec_target, jpginfo.image_width, jpginfo.image_height, format);
        break;
    case RenderTarget::INTERNAL_BUF:
        sprintf(buftypename, "DRIVER");
        init_render_target(dec_target, jpginfo.image_width, jpginfo.image_height, format);
        break;
    default:
        assert(0);
        break;
    }

    uint32_t aligned_w = aligned_width(jpginfo.image_width, SURF_TILING_Y);
    uint32_t aligned_h = aligned_height(jpginfo.image_height, SURF_TILING_Y);
    uint32_t aligned_scaled_w = aligned_width(jpginfo.image_width / scale_factor, SURF_TILING_Y);
    uint32_t aligned_scaled_h = aligned_width(jpginfo.image_height / scale_factor, SURF_TILING_Y);
    int err;
    err = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module);
    if (err || !module) {
        printf("%s failed to get gralloc module\n", __PRETTY_FUNCTION__);
        assert(false);
    }
    gralloc_module = (struct gralloc_module_t *)module;
    err = gralloc_open(module, &allocdev);
    if (err || !allocdev) {
        printf("%s failed to open alloc device\n", __PRETTY_FUNCTION__);
        assert(false);
    }
    err = allocdev->alloc(allocdev,
            aligned_w,
            aligned_h,
            fourcc2PixelFormat(VA_FOURCC_NV12),
            GRALLOC_USAGE_HW_RENDER,
            &nv12_handle,
            &stride);
    if (err) {
        printf("%s failed to allocate surface %d, %dx%d, pixelformat %x\n", __PRETTY_FUNCTION__, err,
            aligned_w, aligned_h, fourcc2PixelFormat(VA_FOURCC_NV12));
        assert(false);
    }
    err = allocdev->alloc(allocdev,
            aligned_w,
            aligned_h,
            fourcc2PixelFormat(VA_FOURCC_YUY2),
            GRALLOC_USAGE_HW_RENDER,
            &yuy2_handle,
            &stride);
    if (err) {
        printf("%s failed to allocate surface %d, %dx%d, pixelformat %x\n", __PRETTY_FUNCTION__, err,
            aligned_w, aligned_h, fourcc2PixelFormat(VA_FOURCC_YUY2));
        assert(false);
    }

    nv21_mem = (uint8_t*)memalign(0x1000, aligned_w * aligned_h * 3 / 2);
    yv12_mem = (uint8_t*)memalign(0x1000, aligned_w * aligned_h * 3 / 2);
    rgba_mem = (uint8_t*)memalign(0x1000, aligned_scaled_w * aligned_scaled_h * 4);
    assert(nv21_mem && yv12_mem && rgba_mem);

    sprintf(decdumpfile, "/sdcard/jpeg_%s_dec_%dx%d.%s", buftypename, jpginfo.image_width, jpginfo.image_height, fourcc2str(format));
    sprintf(origdecdumpfile, "/sdcard/jpeg_%s_dec_orig_%dx%d.yuv", buftypename, aligned_w, aligned_h);
    sprintf(nv12dumpfile, "/sdcard/jpeg_%s_out_%dx%d.nv12", buftypename, aligned_w, aligned_h);
    sprintf(nv21dumpfile, "/sdcard/jpeg_%s_out_%dx%d.nv21", buftypename, aligned_w, aligned_h);
    sprintf(yuy2dumpfile, "/sdcard/jpeg_%s_out_%dx%d.yuy2", buftypename, aligned_w, aligned_h);
    sprintf(yv12dumpfile, "/sdcard/jpeg_%s_out_%dx%d.yv12", buftypename, aligned_w, aligned_h);
    sprintf(rgbadumpfile, "/sdcard/jpeg_%s_out_%dx%d.rgba", buftypename, aligned_scaled_w, aligned_scaled_h);

    RenderTarget* targetlist[1] = {&dec_target};
    st = decoder.init(jpginfo.image_width, jpginfo.image_height, targetlist, 1);
    assert(st == JD_SUCCESS);

    st = decoder.decode(jpginfo, dec_target);
    printf("decode returns %d\n", st);
    assert(st == JD_SUCCESS);

    uint8_t *data;
    uint32_t offsets[3];
    uint32_t pitches[3];

    JpegDecoder::MapHandle maphandle = decoder.mapData(dec_target, (void**) &data, offsets, pitches);
    assert (maphandle);
    fpdump = fopen(decdumpfile, "wb");
    assert(fpdump);
    int hs, vs, nv12, yuy2, uyvy;
    hs = vs = nv12 = yuy2 = uyvy = 0;
    switch(format) {
    case VA_FOURCC_NV12:
        nv12 = 1;
        break;
    case VA_FOURCC_YUY2:
        yuy2 = 1;
        break;
    case VA_FOURCC_UYVY:
        uyvy = 1;
        break;
    case VA_FOURCC('4','0','0','P'):
        hs = vs = 0;
        break;
    case VA_FOURCC_411P:
        hs = 4;
        vs = 1;
        break;
    case VA_FOURCC_411R:
        hs = 1;
        vs = 4;
        break;
    case VA_FOURCC_IMC3:
        hs = 2;
        vs = 2;
        break;
    case VA_FOURCC_422H:
        hs = 2;
        vs = 1;
        break;
    case VA_FOURCC_422V:
        hs = 1;
        vs = 2;
        break;
    case VA_FOURCC_444P:
        hs = vs = 1;
        break;
    default:
        printf("Invalid format %x\n", format);
        assert(false);
        break;
    }
    if (nv12) {
        for (uint32_t i = 0; i < jpginfo.image_height; ++i) {
            fwrite(data + offsets[0] + i * pitches[0], 1, jpginfo.image_width, fpdump);
        }
        for (uint32_t i = 0; i < jpginfo.image_height/2; ++i) {
            fwrite(data + offsets[1] + i * pitches[1], 1, jpginfo.image_width, fpdump);
        }
    }
    else if (yuy2 || uyvy) {
        for (uint32_t i = 0; i < jpginfo.image_height; ++i) {
            fwrite(data + offsets[0] + i * pitches[0], 2, jpginfo.image_width, fpdump);
        }
    }
    else { // yuv planar
        // Y
        for (uint32_t i = 0; i < jpginfo.image_height; ++i) {
            fwrite(data + offsets[0] + i * pitches[0], 1, jpginfo.image_width, fpdump);
        }
        if (hs != 0 && vs != 0) {
            // U
            for (uint32_t i = 0; i < jpginfo.image_height / vs; ++i) {
                fwrite(data + offsets[1] + i * pitches[1], 1, jpginfo.image_width/hs, fpdump);
            }
            // V
            for (uint32_t i = 0; i < jpginfo.image_height / vs; ++i) {
                fwrite(data + offsets[2] + i * pitches[2], 1, jpginfo.image_width/hs, fpdump);
            }
        }
    }
    fclose(fpdump);
    printf("Dumped decoded YUV to %s\n", decdumpfile);
    decoder.unmapData(dec_target, maphandle);

    BlitEvent ev;

    st = decoder.blitToLinearRgba(dec_target, rgba_mem, aligned_w, aligned_h, ev, scale_factor);
    assert(st == JD_SUCCESS);

    decoder.syncBlit(ev);
    fpdump = fopen(rgbadumpfile, "wb");
    assert(fpdump);
    fwrite(rgba_mem, 4, aligned_scaled_w * aligned_scaled_h, fpdump);
    fclose(fpdump);
    printf("Dumped RGBA into %s\n", rgbadumpfile);

    // test blit_to_camera_surfaces
    if (format == VA_FOURCC_422H) {

        RenderTarget nv12_dst, yuy2_dst;
        nsecs_t t1, t2;
        init_render_target_gralloc(nv12_dst, aligned_w, aligned_h, VA_FOURCC_NV12);
        init_render_target_gralloc(yuy2_dst, aligned_w, aligned_h, VA_FOURCC_YUY2);
        t1 = systemTime();
        st = decoder.blit(dec_target, nv12_dst, 1);
        st = decoder.blit(dec_target, yuy2_dst, 1);
        t2 = systemTime();
        printf("422H->NV12+YUY2 VA took %.2f ms\n", (t2-t1)/1000000.0);
        deinit_render_target(nv12_dst);
        deinit_render_target(yuy2_dst);
        t1 = systemTime();
        st = decoder.blitToCameraSurfaces(dec_target, nv12_handle, yuy2_handle,
                                          NULL, NULL,
                                          aligned_w, aligned_h,
                                          ev);
        t2 = systemTime();
        decoder.syncBlit(ev);
        printf("422H->NV12+YUY2 CM took %.2f ms\n", (t2-t1)/1000000.0);
        t1 = systemTime();
        st = decoder.blitToCameraSurfaces(dec_target, nv12_handle, yuy2_handle,
                                          nv21_mem, yv12_mem,
                                          aligned_w, aligned_h,
                                          ev);
        t2 = systemTime();
        decoder.syncBlit(ev);
        printf("422H->NV12+YUY2+NV21+YV12 CM took %.2f ms\n", (t2-t1)/1000000.0);
        assert(st == JD_SUCCESS);
        fpdump = fopen(nv21dumpfile, "wb");
        assert(fpdump);
        fwrite(nv21_mem, 1, aligned_w * aligned_h* 3 /2, fpdump);
        fclose(fpdump);
        printf("Dumped NV21 into %s\n", nv21dumpfile);
        fpdump = fopen(yv12dumpfile, "wb");
        assert(fpdump);
        fwrite(yv12_mem, 1, aligned_w * aligned_h * 3 / 2, fpdump);
        fclose(fpdump);
        printf("Dumped YV12 into %s\n", yv12dumpfile);
        gralloc_module->lock(gralloc_module, nv12_handle, GRALLOC_USAGE_SW_READ_OFTEN, 0, 0, aligned_w, aligned_h, (void**)&nv12_mem);
        fpdump = fopen(nv12dumpfile, "wb");
        assert(fpdump);
        fwrite(nv12_mem, 1, aligned_w * aligned_h * 3 / 2, fpdump);
        fclose(fpdump);
        gralloc_module->unlock(gralloc_module, nv12_handle);
        printf("Dumped NV12 into %s\n", nv12dumpfile);
        gralloc_module->lock(gralloc_module, yuy2_handle, GRALLOC_USAGE_SW_READ_OFTEN, 0, 0, aligned_w, aligned_h, (void**)&yuy2_mem);
        fpdump = fopen(yuy2dumpfile, "wb");
        assert(fpdump);
        fwrite(yuy2_mem, 2, aligned_w * aligned_h, fpdump);
        fclose(fpdump);
        gralloc_module->unlock(gralloc_module, yuy2_handle);
        printf("Dumped YUY2 into %s\n", yuy2dumpfile);
    }

    decoder.deinit();

    allocdev->free(allocdev, nv12_handle);
    allocdev->free(allocdev, yuy2_handle);
    free(nv21_mem);
    free(yv12_mem);
    free(rgba_mem);

    switch(type) {
    case RenderTarget::KERNEL_DRM:
        deinit_render_target(dec_target, &dec_handle);
        break;
    default:
        deinit_render_target(dec_target);
        break;
    }
    delete[] jpginfo.buf;
    gralloc_close(allocdev);
    vaDestroyContext(display, vpCtxId);
    vaDestroyConfig(display, vpCfgId);
    vaTerminate(display);
}

int main(int argc, char ** argv)
{
    int res, scale;
    uint32_t format = 0;
    scale = 1;
    memset(jpgfile, 0, sizeof(jpgfile));
    while ((res = getopt(argc, argv, "i:f:s:")) >= 0) {
        switch (res) {
            case 'i':
                {
                    strcpy(jpgfile, optarg);
                    break;
                }
            case 's':
                {
                    scale = atoi(optarg);
                    break;
                }
            case 'f':
                {
                    if (strcmp(optarg, "NV12") == 0) {
                        format = VA_FOURCC_NV12;
                    }
                    else if (strcmp(optarg, "YUY2") == 0) {
                        format = VA_FOURCC_YUY2;
                    }
                    else if (strcmp(optarg, "UYVY") == 0) {
                        format = VA_FOURCC_UYVY;
                    }
                    else {
                        format = 0;
                        printf("INVALID output decode format, using YUV planar\n");
                    }
                    break;
                }
            default:
                printf("usage: testjpegdec -i <filename> [-w <width> -h <height>]\n");
                exit(-1);
        }
    }
    if (strcmp(jpgfile, "") == 0) {
        printf("usage: testjpegdec -i <filename> [-f <decode output format FOURCC>] [-s <scaling_factor>]\n");
        printf("       available output FOURCC: NV12, YUY2, UYVY, 0. 0 by default (YUV planar)\n");
        printf("       available scaling_factor: 1, 2, 4, 8. 1 by default (no down-scale)\n");
        exit(-1);
    }
    printf("----- DRM surface type test -----\n");
    //decode_blit_functionality_test(RenderTarget::KERNEL_DRM, 0);
    printf("----- GRALLOC surface type test -----\n");
    //decode_blit_functionality_test(RenderTarget::ANDROID_GRALLOC, 0);
    printf("----- Normal surface type test, scale %d-----\n", scale);
    decode_blit_functionality_test(RenderTarget::INTERNAL_BUF, format, scale);
    printf("----- Userptr surface type test -----\n");
    //decode_blit_functionality_test(RenderTarget::USER_PTR, format);
    return 0;
}
