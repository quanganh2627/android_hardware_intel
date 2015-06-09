/*
 * Copyright (c) 2014 Intel Corporation. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Kun Wang <kun.k.wang@intel.com>
 *
 */

#include "vsp_VPP.h"
#include "vsp_compose.h"
#include "psb_buffer.h"
#include "psb_surface.h"
#include "vsp_cmdbuf.h"
#include "psb_drv_debug.h"


#define INIT_DRIVER_DATA    psb_driver_data_p driver_data = (psb_driver_data_p) ctx->pDriverData;
#define INIT_CONTEXT_VPP    context_VPP_p ctx = (context_VPP_p) obj_context->format_data;
#define CONFIG(id)  ((object_config_p) object_heap_lookup( &driver_data->config_heap, id ))
#define CONTEXT(id) ((object_context_p) object_heap_lookup( &driver_data->context_heap, id ))
#define BUFFER(id)  ((object_buffer_p) object_heap_lookup( &driver_data->buffer_heap, id ))

#define SURFACE(id)    ((object_surface_p) object_heap_lookup( &ctx->obj_context->driver_data->surface_heap, id ))

#define ALIGN_TO_128(value) ((value + 128 - 1) & ~(128 - 1))
#define ALIGN_TO_16(value) ((value + 16 - 1) & ~(16 - 1))

VAStatus vsp_compose_process_pipeline_param(context_VPP_p ctx, object_context_p __maybe_unused obj_context, object_buffer_p obj_buffer)
{

	VAStatus vaStatus = VA_STATUS_SUCCESS;
	vsp_cmdbuf_p cmdbuf = ctx->obj_context->vsp_cmdbuf;
	VAProcPipelineParameterBuffer *pipeline_param = (VAProcPipelineParameterBuffer *)obj_buffer->buffer_data;
	struct VssWiDi_ComposeSequenceParameterBuffer *cell_compose_param = NULL;
	object_surface_p yuv_surface = NULL;
	object_surface_p rgb_surface = NULL;
	object_surface_p output_surface = NULL;
	int yuv_width = 0, yuv_height = 0, yuv_stride = 0;
	int rgb_width = 0, rgb_height = 0, rgb_stride = 0;
	int out_width = 0, out_height = 0, out_stride = 0;
	int out_buf_width = 0, out_buf_height = 0;
	int op_x = 0, op_y = 0, op_w = 0, op_h = 0;
	int ip_x = 0, ip_y = 0, ip_w = 0, ip_h = 0;
	int no_rgb = 0, no_yuv = 0;

	cell_compose_param = (struct VssWiDi_ComposeSequenceParameterBuffer *)cmdbuf->compose_param_p;

	/* init tile set */
	cell_compose_param->Is_input_tiled = 0;
	cell_compose_param->Is_output_tiled = 0;

	/* The END command */
	if (pipeline_param->pipeline_flags & VA_PIPELINE_FLAG_END) {
		vsp_cmdbuf_compose_end(cmdbuf);
		/* Destory the VSP context */
		vsp_cmdbuf_vpp_context(cmdbuf, VssGenDestroyContext, CONTEXT_COMPOSE_ID, 0);
		goto out;
	}

	if (pipeline_param->num_additional_outputs <= 0 || !pipeline_param->additional_outputs) {
		no_rgb = 1;
	} else {
		/* The RGB surface will be the first element */
		rgb_surface = SURFACE(pipeline_param->additional_outputs[0]);
		if (rgb_surface == NULL)
			no_rgb = 1;
	}

	/* Init the VSP context */
	if (ctx->obj_context->frame_count == 0)
		vsp_cmdbuf_vpp_context(cmdbuf, VssGenInitializeContext, CONTEXT_COMPOSE_ID, VSP_APP_ID_WIDI_ENC);

	yuv_surface = SURFACE(pipeline_param->surface);
	if (yuv_surface == NULL)
		no_yuv = 1;

	output_surface = ctx->obj_context->current_render_target;
	if (!no_yuv) {
		yuv_width = ALIGN_TO_16(yuv_surface->width);
		yuv_height = yuv_surface->height;
		yuv_stride = yuv_surface->psb_surface->stride;

#ifdef PSBVIDEO_VPP_TILING
		if (GET_SURFACE_INFO_tiling(yuv_surface->psb_surface))
			cell_compose_param->Is_input_tiled = 1;
#endif
	}

#ifdef PSBVIDEO_VPP_TILING
		if (GET_SURFACE_INFO_tiling(output_surface->psb_surface))
			cell_compose_param->Is_output_tiled = 1;
#endif

	out_width = output_surface->width;
	out_buf_width = ALIGN_TO_16(output_surface->width);
	out_height = output_surface->height_origin;
	out_buf_height = output_surface->height;
	out_stride = output_surface->psb_surface->stride;

	if (!no_rgb) {
		rgb_width = ALIGN_TO_16(rgb_surface->width);
		rgb_height = ALIGN_TO_16(rgb_surface->height);
		rgb_stride = rgb_surface->psb_surface->stride * 4;
		/* FIXME: should make format an enum */
		if (rgb_surface->pixel_format == VA_FOURCC_BGRA)
			cell_compose_param->RGBA_Format = 2;
		else if (rgb_surface->pixel_format == VA_FOURCC_RGBA)
			cell_compose_param->RGBA_Format = 1;
		else
			cell_compose_param->RGBA_Format = 2;
	}

	ip_w = yuv_width;
	ip_h = yuv_height;
	if (pipeline_param->surface_region) {
		ip_x = pipeline_param->surface_region->x;
		ip_y = pipeline_param->surface_region->y;
		ip_w = pipeline_param->surface_region->width & (~1);
		ip_h = pipeline_param->surface_region->height & (~1);
	}

	/* scale op rect */
	op_w = out_width;
	op_h = out_height;
	if (pipeline_param->output_region) {
		op_x = pipeline_param->output_region->x & (~1);
		op_y = pipeline_param->output_region->y & (~1);
		op_w = pipeline_param->output_region->width & (~1);
		op_h = pipeline_param->output_region->height & (~1);
	}

	/* RGB related */
	cell_compose_param->RGB_Width = rgb_width;
	cell_compose_param->RGB_Height = rgb_height;
	cell_compose_param->RGBA_IN_Stride = rgb_stride;

	if (!no_rgb)
		vsp_cmdbuf_reloc_pic_param(
			&(cell_compose_param->RGBA_Buffer),
			0,
			&(rgb_surface->psb_surface->buf),
			cmdbuf->param_mem_loc,
			cell_compose_param);
	else
		cell_compose_param->RGBA_Buffer = 0;

	/* Input YUV Video related */
	cell_compose_param->Video_IN_Y_stride = yuv_stride;
	if (!no_yuv) {
		vsp_cmdbuf_reloc_pic_param(
			&(cell_compose_param->Video_IN_Y_Buffer),
			0,
			&(yuv_surface->psb_surface->buf),
			cmdbuf->param_mem_loc,
			cell_compose_param);
		vsp_cmdbuf_reloc_pic_param(
			&(cell_compose_param->Video_IN_UV_Buffer),
			yuv_height * cell_compose_param->Video_IN_Y_stride,
			&(yuv_surface->psb_surface->buf),
			cmdbuf->param_mem_loc,
			cell_compose_param);
	} else {
		cell_compose_param->Video_IN_Y_Buffer = 0;
		cell_compose_param->Video_IN_UV_Buffer = 0;
	}

	/* Output Video related */
	cell_compose_param->Video_OUT_xsize = out_width;
	cell_compose_param->Video_OUT_ysize = out_height;
	cell_compose_param->Video_OUT_Y_stride = out_stride;
	vsp_cmdbuf_reloc_pic_param(
				&(cell_compose_param->Video_OUT_Y_Buffer),
				0,
				&(output_surface->psb_surface->buf),
				cmdbuf->param_mem_loc,
				cell_compose_param);

	cell_compose_param->Video_OUT_UV_Buffer =
        cell_compose_param->Video_OUT_Y_Buffer +
        out_buf_height * cell_compose_param->Video_OUT_Y_stride;

	/* Blending related params */
	/* check which plane is bigger */
	if (rgb_width * rgb_height > ip_h * ip_w)
		cell_compose_param->Is_video_the_back_ground = 0;
	else
		cell_compose_param->Is_video_the_back_ground = 1;

	cell_compose_param->ROI_scaling_ip_width = ip_w;
	cell_compose_param->ROI_scaling_ip_height = ip_h;
	cell_compose_param->ROI_scaling_ip_x = ip_x;
	cell_compose_param->ROI_scaling_ip_y = ip_y;

	cell_compose_param->ROI_scaling_op_width = op_w;
	cell_compose_param->ROI_scaling_op_height = op_h;
	cell_compose_param->ROI_scaling_op_x = op_x;
	cell_compose_param->ROI_scaling_op_y = op_y;

	if (!no_yuv) {
		cell_compose_param->YUVscalefactor_dx = (unsigned int)(1024 / (((float)op_w) / ip_w) + 0.5);
		cell_compose_param->YUVscalefactor_dy = (unsigned int)(1024 / (((float)op_h) / ip_h) + 0.5);
	}

	/* FIXME: found out how to set by pass from uplayer */
	cell_compose_param->bypass_mode = 0;

	vsp_cmdbuf_insert_command(cmdbuf,
				CONTEXT_COMPOSE_ID,
				&cmdbuf->param_mem,
				VssWiDi_ComposeFrameCommand,
				ctx->compose_param_offset,
				sizeof(struct VssWiDi_ComposeSequenceParameterBuffer));

	/* Insert Fence Command */
	vsp_cmdbuf_fence_compose_param(cmdbuf, wsbmKBufHandle(wsbmKBuf(cmdbuf->param_mem.drm_buf)));

out:
	free(pipeline_param);
	obj_buffer->buffer_data = NULL;
	obj_buffer->size = 0;

	return vaStatus;
}
