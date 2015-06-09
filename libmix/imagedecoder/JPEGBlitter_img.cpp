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

#include "JPEGBlitter.h"
JpegDecodeStatus JpegBlitter::preInit(VADisplay display)
{
    return JD_UNIMPLEMENTED;
}
JpegDecodeStatus JpegBlitter::blit(RenderTarget &src, RenderTarget &dst, int scale_factor)
{
    return JD_OUTPUT_FORMAT_UNSUPPORTED;
}

JpegDecodeStatus JpegBlitter::blitToLinearRgba(RenderTarget &src, uint8_t *sysmem, uint32_t width, uint32_t height, BlitEvent &event, int scale_factor)
{
    return JD_OUTPUT_FORMAT_UNSUPPORTED;
}
JpegDecodeStatus JpegBlitter::getRgbaTile(RenderTarget &src,
                                     uint8_t *sysmem,
                                     int left, int top, int width, int height, int scale_factor)
{
    return JD_OUTPUT_FORMAT_UNSUPPORTED;
}
void JpegBlitter::init(JpegDecoder& /*dec*/)
{
    // Do nothing
}
void JpegBlitter::deinit()
{
    // Do nothing
}
void JpegBlitter::syncBlit(BlitEvent &event)
{
    // Do nothing
}
JpegDecodeStatus JpegBlitter::blitToCameraSurfaces(RenderTarget &src,
                                                   buffer_handle_t dst_nv12,
                                                   buffer_handle_t dst_yuy2,
                                                   uint8_t *dst_nv21,
                                                   uint8_t *dst_yv12,
                                                   uint32_t width, uint32_t height,
                                                   BlitEvent &event)
{
    return JD_OUTPUT_FORMAT_UNSUPPORTED;
}
