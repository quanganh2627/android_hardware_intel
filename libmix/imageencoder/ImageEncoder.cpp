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

/*#define LOG_NDEBUG 0*/
#define LOG_TAG "IntelImageEncoder"

#include <cutils/log.h>
#include "ImageEncoder.h"
#include "JpegTbls.h"

IntelImageEncoder::IntelImageEncoder(void)
{
	/* Initialize variables */
	encoder_status = LIBVA_UNINITIALIZED;
	quality = INTEL_IMAGE_ENCODER_DEFAULT_QUALITY;

	va_dpy = NULL;
	memset((void *)&va_configattrib, 0, sizeof(va_configattrib));

	images_count = 0;
	memset((void *)va_surfaceid, 0x0, sizeof(va_surfaceid));
	memset((void *)surface_width, 0x0, sizeof(surface_width));
	memset((void *)surface_height, 0x0, sizeof(surface_height));
	memset((void *)surface_fourcc, 0x0, sizeof(surface_fourcc));

	va_configid = 0;
	va_contextid = 0;
	context_width = 0;
	context_height = 0;
	context_fourcc = 0;
	va_codedbufferid = 0;
	coded_buf_size = 0;

	reserved_image_seq = -1;

	va_codedbuffersegment = NULL;
	coded_data_size = 0;

	LOGV("IntelImageEncoder: done\n");
}

int IntelImageEncoder::initializeEncoder(void)
{
	int i =0;
	VAStatus va_status;
	int display_num = 0;
	int major_version = -1, minor_version = -1;
	const char *driver_version = NULL;
	VAEntrypoint va_entrypoints[5];
	int va_entrypoints_count = 0;

	if (encoder_status != LIBVA_UNINITIALIZED) {
		LOGE("initializeEncoder: already initialized!\n");
		return VA_STATUS_ERROR_OPERATION_FAILED;
	}

	/* Get display */
	va_dpy = vaGetDisplay(&display_num);
	if (NULL == va_dpy) {
		LOGE("initializeEncoder: vaGetDisplay failed!\n");
		return VA_STATUS_ERROR_OPERATION_FAILED;
	}

	/* Initialize */
	va_status = vaInitialize(va_dpy, &major_version, &minor_version);
	if (va_status != VA_STATUS_SUCCESS) {
		LOGE("initializeEncoder: vaInitialize failed (%d)!\n", va_status);
		return va_status;
	}
	LOGV("initializeEncoder: LibVA version: %d.%d\n", major_version, minor_version);

	/* Query driver version */
	driver_version = vaQueryVendorString(va_dpy);
	if (NULL == driver_version) {
		LOGE("initializeEncoder: vaQueryVendorString failed!\n");
		vaTerminate(va_dpy);
		va_dpy = NULL;
		return VA_STATUS_ERROR_OPERATION_FAILED;
	}
	LOGV("initializeEncoder: Driver version: %s\n", driver_version);
    if (driver_version) {
        if (strstr(driver_version, "MRFL"))
            mHwPlatform = VIDENC_MRFL;
        else if (strstr(driver_version, "MFLD"))
            mHwPlatform = VIDENC_MFLD;
        else if (strstr(driver_version, "MRST"))
            mHwPlatform = VIDENC_MRST;
        else if (strstr(driver_version, "LEXINGTON"))
            mHwPlatform = VIDENC_LEXT;
        else
            mHwPlatform = VIDENC_BYT;
    }

	/* Query JPEG baseline encoding's entrypoint */
	va_status = vaQueryConfigEntrypoints(va_dpy, VAProfileJPEGBaseline, va_entrypoints,
										&va_entrypoints_count);
	if (va_status != VA_STATUS_SUCCESS) {
		LOGE("initializeEncoder: vaQueryConfigEntrypoints failed (%d)!\n", va_status);
		vaTerminate(va_dpy);
		va_dpy = NULL;
		return va_status;
	}

	for (i=0; i < va_entrypoints_count; ++i) {
		if (VAEntrypointEncPicture == va_entrypoints[i])
			break;
	}
	if (i == va_entrypoints_count) {
		LOGE("initializeEncoder: no JPEG Baseline encoding entrypoint was found!\n");
		vaTerminate(va_dpy);
		va_dpy = NULL;
		return VA_STATUS_ERROR_UNIMPLEMENTED;
	}

	/* Get supported configuration attributes */
	va_configattrib.type = VAConfigAttribRTFormat;
	va_status = vaGetConfigAttributes(va_dpy, VAProfileJPEGBaseline, VAEntrypointEncPicture,
									&va_configattrib, 1);
	if (va_status != VA_STATUS_SUCCESS) {
		LOGE("initializeEncoder: vaGetConfigAttributes failed (%d)!\n", va_status);
		vaTerminate(va_dpy);
		va_dpy = NULL;
		memset((void *)&va_configattrib, 0x0, sizeof(va_configattrib));
		return va_status;
	}

	if(mHwPlatform == VIDENC_BYT) {
		va_configattrib_ext.type = VAConfigAttribEncJPEG;
		va_status = vaGetConfigAttributes(va_dpy, VAProfileJPEGBaseline, VAEntrypointEncPicture,
				&va_configattrib_ext, 1);
		if (va_status != VA_STATUS_SUCCESS) {
			LOGE("initializeEncoder: vaGetConfigAttributes failed (%d)!\n", va_status);
			vaTerminate(va_dpy);
			va_dpy = NULL;
			memset((void *)&va_configattrib_ext, 0x0, sizeof(va_configattrib_ext));
			return va_status;
		}
	}

	encoder_status = LIBVA_INITIALIZED;
	LOGV("initializeEncoder: done\n");
	return VA_STATUS_SUCCESS;
}

int IntelImageEncoder::createSourceSurface(int source_type, void *source_buffer,
						unsigned int width,unsigned int height,
						unsigned int stride, unsigned int fourcc,
						int *image_seqp)
{
	int i =0;
	VAStatus va_status;
	VASurfaceAttribExternalBuffers va_surfacebuf;
	VASurfaceAttrib va_surfaceattrib[2];
	unsigned long ptr = 0;

	if (LIBVA_UNINITIALIZED == encoder_status) {
		LOGE("createSourceSurface: uninitialized, not ready to create surface!\n");
		return VA_STATUS_ERROR_OPERATION_FAILED;
	}

	if (images_count > INTEL_IMAGE_ENCODER_MAX_BUFFERS) {
		LOGE("createSourceSurface: the max supported count was already reached!\n");
		return VA_STATUS_ERROR_OPERATION_FAILED;
	}

	if ((source_type != SURFACE_TYPE_USER_PTR) &&
	(source_type != SURFACE_TYPE_GRALLOC)) {
		LOGE("createSourceSurface: buffer type 0x%x was not supported!\n", source_type);
		return VA_STATUS_ERROR_INVALID_PARAMETER;
	}

	if (NULL == source_buffer) {
		LOGE("createSourceSurface: the input buffer address can't be null!\n");
		return VA_STATUS_ERROR_INVALID_PARAMETER;
	}

	if ((source_type == SURFACE_TYPE_USER_PTR) &&
	((unsigned long)source_buffer & 0xFFF)) {
		LOGE("createSourceSurface: the user input buffer wasn't aligned to 4096!\n");
		return VA_STATUS_ERROR_INVALID_PARAMETER;
	}

	if (stride % INTEL_IMAGE_ENCODER_REQUIRED_STRIDE) {
		LOGE("createSourceSurface: the stride value %d is not alligned to %d!\n",
			stride, INTEL_IMAGE_ENCODER_REQUIRED_STRIDE);
		return VA_STATUS_ERROR_INVALID_PARAMETER;
	}

	if ((width % 2) || (height % 2)) {
		LOGE("createSourceSurface: only even dimensions were supportd!\n");
		return VA_STATUS_ERROR_RESOLUTION_NOT_SUPPORTED;
	}

	if (((fourcc != VA_RT_FORMAT_YUV420) && (fourcc != VA_RT_FORMAT_YUV422)) ||
		!(fourcc & va_configattrib.value)) {
		/* Currently supported image formats:
		 * #define VA_RT_FORMAT_YUV420	0x00000001
		 * #define VA_RT_FORMAT_YUV422	0x00000002
		 */
		LOGE("createSourceSurface: the image format %d was not supported!\n", fourcc);
		return VA_STATUS_ERROR_INVALID_IMAGE_FORMAT;
	}

	/* Find the first available image sequential number */
	for (i=0; i<INTEL_IMAGE_ENCODER_MAX_BUFFERS; ++i) {
		if (0 == va_surfaceid[i]) { /* Empty */
			break;
		}
	}
	if(INTEL_IMAGE_ENCODER_MAX_BUFFERS == i) {
		LOGE("createSourceSurface: failed because the max surface count was reached!\n");
		return VA_STATUS_ERROR_ALLOCATION_FAILED;
	}

	/* Allocate a source surface */
	if (VA_RT_FORMAT_YUV420 == fourcc)
		va_surfacebuf.pixel_format = VA_FOURCC_NV12;
	else if (VA_RT_FORMAT_YUV422 == fourcc)
		va_surfacebuf.pixel_format = VA_FOURCC_YV16;
	va_surfacebuf.width = width;
	va_surfacebuf.height = height;
	va_surfacebuf.data_size = stride * height * 1.5;
	va_surfacebuf.num_buffers = 1;
	va_surfacebuf.num_planes = 3;
	va_surfacebuf.pitches[0] = stride;
	va_surfacebuf.pitches[1] = stride;
	va_surfacebuf.pitches[2] = stride;
	va_surfacebuf.pitches[3] = 0;
	va_surfacebuf.offsets[0] = 0;
	va_surfacebuf.offsets[1] = stride * height;
	va_surfacebuf.offsets[2] = va_surfacebuf.offsets[1];
	va_surfacebuf.offsets[3] = 0;
	va_surfacebuf.buffers = (unsigned long *)&ptr;
	*(va_surfacebuf.buffers) = (unsigned long)source_buffer;
	va_surfacebuf.flags = VA_SURFACE_ATTRIB_MEM_TYPE_ANDROID_GRALLOC;
	va_surfacebuf.private_data = NULL;
	va_surfaceattrib[0].type = (VASurfaceAttribType)VASurfaceAttribMemoryType;
	va_surfaceattrib[0].flags = VA_SURFACE_ATTRIB_SETTABLE;
	va_surfaceattrib[0].value.type = VAGenericValueTypeInteger;
	va_surfaceattrib[0].value.value.i = source_type;
	va_surfaceattrib[1].type = (VASurfaceAttribType)VASurfaceAttribExternalBufferDescriptor;
	va_surfaceattrib[1].flags = VA_SURFACE_ATTRIB_SETTABLE;
	va_surfaceattrib[1].value.type = VAGenericValueTypePointer;
	va_surfaceattrib[1].value.value.p = (void *)&va_surfacebuf;

	va_status = vaCreateSurfaces(va_dpy, fourcc, width, height,
					&va_surfaceid[i], 1, va_surfaceattrib, 2);
	if (va_status != VA_STATUS_SUCCESS) {
		LOGE("createSourceSurface: vaCreateSurfaces failed (%d)!\n", va_status);
		va_surfaceid[i] = 0;
		return va_status;
	}

	surface_width[i] = width;
	surface_height[i] = height;
	surface_fourcc[i] = fourcc;

	*image_seqp = i;
	++images_count;

	return VA_STATUS_SUCCESS;
}

int IntelImageEncoder::createContext(int first_image_seq, unsigned int *max_coded_sizep)
{
	VAStatus va_status;
	VAConfigAttrib va_cur_configattrib[2];
	int config_attrib_num = 0;

	*max_coded_sizep = 0;

	if (LIBVA_UNINITIALIZED == encoder_status) {
		LOGE("createContext: uninitialized, not ready to create context!\n");
		return VA_STATUS_ERROR_OPERATION_FAILED;
	} else if (encoder_status >= LIBVA_CONTEXT_CREATED) {
		LOGE("createContext: there already is an active context!\n");
		return VA_STATUS_ERROR_OPERATION_FAILED;
	}

	if ((first_image_seq < 0) ||
		(first_image_seq >= INTEL_IMAGE_ENCODER_MAX_BUFFERS) ||
		(VA_INVALID_SURFACE == va_surfaceid[first_image_seq])) {
		LOGE("createContext: invalid image sequential number!\n");
		return VA_STATUS_ERROR_INVALID_PARAMETER;
	}

	context_width = surface_width[first_image_seq];
	context_height = surface_height[first_image_seq];
	context_fourcc = surface_fourcc[first_image_seq];

	/* Create a config */
	va_cur_configattrib[0].type = VAConfigAttribRTFormat;
	va_cur_configattrib[0].value = context_fourcc;
	config_attrib_num++;
	if(mHwPlatform == VIDENC_BYT) {
		VAConfigAttribValEncJPEG attrib_jpeg_enc;
		attrib_jpeg_enc.value = va_configattrib_ext.value;
		attrib_jpeg_enc.bits.arithmatic_coding_mode = 0;
		attrib_jpeg_enc.bits.progressive_dct_mode = 0;
		attrib_jpeg_enc.bits.non_interleaved_mode = 1;
		attrib_jpeg_enc.bits.differential_mode = 0;
		va_cur_configattrib[1].type = VAConfigAttribEncJPEG;
		va_cur_configattrib[1].value = attrib_jpeg_enc.value;
		config_attrib_num++;
	}
	va_status = vaCreateConfig(va_dpy, VAProfileJPEGBaseline, VAEntrypointEncPicture,
								va_cur_configattrib, config_attrib_num, &va_configid);
	if (va_status != VA_STATUS_SUCCESS) {
		LOGE("createContext: vaCreateConfig failed (%d)!\n", va_status);
		va_configid = 0;
		return va_status;
	}

	/* Create a context */
	va_status = vaCreateContext(va_dpy, va_configid, context_width, context_height,
					VA_PROGRESSIVE, &va_surfaceid[first_image_seq], 1, &va_contextid);
	if (va_status != VA_STATUS_SUCCESS) {
		LOGE("createContext: vaCreateContext failed (%d)!\n", va_status);
		va_contextid = 0;
		vaDestroyConfig(va_dpy, va_configid);
		va_configid = 0;
		return va_status;
	}


	/* Create a coded buffer */
	coded_buf_size = (((context_width+15)/16)*((context_height+15)/16)*320) + 640;
	coded_buf_size = (coded_buf_size+0xf) & ~0xf;

	va_status = vaCreateBuffer(va_dpy, va_contextid, VAEncCodedBufferType, coded_buf_size,
					1, NULL, &va_codedbufferid);
	if (va_status != VA_STATUS_SUCCESS) {
		LOGE("createContext: vaCreateBuffer VAEncCodedBufferType failed (%d)!\n", va_status);
		vaDestroyContext(va_dpy, va_contextid);
		va_contextid = 0;
		vaDestroyConfig(va_dpy, va_configid);
		va_configid = 0;
		va_codedbufferid = 0;
		return va_status;
	}

	*max_coded_sizep = coded_buf_size;

	encoder_status = LIBVA_CONTEXT_CREATED;
	LOGV("createContext: done\n");
	return VA_STATUS_SUCCESS;
}

int IntelImageEncoder::setQuality(unsigned int new_quality)
{
	if (quality == new_quality) {
		return VA_STATUS_SUCCESS;
	}

	if (LIBVA_ENCODING == encoder_status) {
		LOGE("setQuality: can't update quality while encoding!\n");
		return VA_STATUS_ERROR_OPERATION_FAILED;
	}

	if ((new_quality > INTEL_IMAGE_ENCODER_MAX_QUALITY) ||
		(new_quality < INTEL_IMAGE_ENCODER_MIN_QUALITY)) {
		LOGE("setQuality: invalid new quality value, not updated!\n");
		return VA_STATUS_ERROR_INVALID_PARAMETER;
	}

	quality = new_quality;

	LOGV("setQuality: quality was updated to %d\n", quality);
	return VA_STATUS_SUCCESS;
}

int IntelImageEncoder::encode(int image_seq, unsigned int new_quality)
{
	VAStatus va_status;
	VAEncPictureParameterBufferJPEG va_picparabuffer;
	VABufferID va_picparabufferid = 0;

	if (encoder_status < LIBVA_CONTEXT_CREATED) {
		LOGE("encode: no context created to perform encoding!\n");
		return VA_STATUS_ERROR_OPERATION_FAILED;
	} else if (encoder_status > LIBVA_CONTEXT_CREATED) {
		LOGE("encode: there already is an active encoding task!\n");
		return VA_STATUS_ERROR_OPERATION_FAILED;
	}

	if ((image_seq < 0) ||
		(image_seq >= INTEL_IMAGE_ENCODER_MAX_BUFFERS) ||
		(VA_INVALID_SURFACE == va_surfaceid[image_seq])) {
		LOGE("encode: invalid image sequential number!\n");
		return VA_STATUS_ERROR_INVALID_PARAMETER;
	} else if ((context_width != surface_width[image_seq]) ||
				(context_height != surface_height[image_seq]) ||
				(context_fourcc != surface_fourcc[image_seq])) {
		LOGE("encode: the input image didn't fit in the current context!\n");
		return VA_STATUS_ERROR_INVALID_PARAMETER;
	}

	/* Update quality */
	if (setQuality(new_quality) != VA_STATUS_SUCCESS) {
		LOGE("encode: the input quality value was invalid, encoding aborted!\n");
		return VA_STATUS_ERROR_INVALID_PARAMETER;
	}

	/* Reset coded */
	va_codedbuffersegment = NULL;
	coded_data_size = 0;

	/* Begin picture */
	va_status = vaBeginPicture(va_dpy, va_contextid, va_surfaceid[image_seq]);
	if (va_status != VA_STATUS_SUCCESS) {
		LOGE("encode: vaBeginPicture failed (%d)!\n", va_status);
		return va_status;
	}

	/* Create a picture-parameter buffer */
	va_picparabuffer.picture_width  = context_width;
	va_picparabuffer.picture_height = context_height;
	va_picparabuffer.reconstructed_picture= 0;
	va_picparabuffer.coded_buf = va_codedbufferid;
	va_picparabuffer.pic_flags.bits.profile = 0; /* Baseline */
	va_picparabuffer.pic_flags.bits.progressive = 0; /* Sequential */
	va_picparabuffer.pic_flags.bits.huffman = 1; /* Huffman */
	va_picparabuffer.pic_flags.bits.interleaved = 0; /* Non-interleaved */
	va_picparabuffer.pic_flags.bits.differential = 0; /* Non-differential */
	va_picparabuffer.sample_bit_depth = 8; /* 8-bits */
	va_picparabuffer.num_components = 3; /* 3-components */
	va_picparabuffer.quality = quality; /* JPEG ENC quality */
	if(mHwPlatform == VIDENC_BYT) {
		va_picparabuffer.component_id[0] = 0;
		va_picparabuffer.quantiser_table_selector[0] = 0;
		va_picparabuffer.component_id[1] = 1;
		va_picparabuffer.quantiser_table_selector[0] = 1;
		va_picparabuffer.component_id[2] = 2;
		va_picparabuffer.quantiser_table_selector[0] = 1;
	}
	va_status = vaCreateBuffer(va_dpy, va_contextid, VAEncPictureParameterBufferType,
					sizeof(va_picparabuffer), 1, &va_picparabuffer,&va_picparabufferid);
	if (va_status != VA_STATUS_SUCCESS) {
		LOGE("encode: vaCreateBuffer VAEncPictureParameterBufferType failed (%d)!\n", va_status);
		return va_status;
	}

	/* Render picture */
	va_status = vaRenderPicture(va_dpy, va_contextid, &va_picparabufferid, 1);
	if (va_status != VA_STATUS_SUCCESS) {
		LOGE("encode: vaRenderPicture failed (%d)!\n", va_status);
		vaDestroyBuffer(va_dpy, va_picparabufferid);
		return va_status;
	}

	/* Destroy the used picture-parameter buffer */
	vaDestroyBuffer(va_dpy, va_picparabufferid);
	if(mHwPlatform == VIDENC_BYT) {
		VAEncSliceParameterBufferJPEG va_ScanParams;
		VABufferID va_ScanParamsBufId;
		VAHuffmanTableBufferJPEGBaseline va_HufTbl;
		VABufferID va_HufTblBufId;
		VAQMatrixBufferJPEG va_QMatrix;
		VABufferID va_QMatrixBufId;

		build_default_enc_quantiser_tables(&va_QMatrix);
		va_status = vaCreateBuffer(va_dpy, va_contextid, VAQMatrixBufferType,
				sizeof(VAQMatrixBufferJPEG), 1, &va_QMatrix, &va_QMatrixBufId);
		if (va_status != VA_STATUS_SUCCESS) {
			LOGE("encode: vaCreateBuffer VAQMatrixBufferJPEG failed (%d)!\n", va_status);
			return va_status;
		}
		build_default_enc_huffman_tables(&va_HufTbl);
		va_status = vaCreateBuffer(va_dpy, va_contextid, VAHuffmanTableBufferType,
				sizeof(VAHuffmanTableBufferJPEGBaseline), 1, &va_HufTbl, &va_HufTblBufId);
		if (va_status != VA_STATUS_SUCCESS) {
			LOGE("encode: vaCreateBuffer VAHuffmanTableBufferJPEGBaseline failed (%d)!\n", va_status);
			return va_status;
		}
		va_ScanParams.restart_interval = 0;
		va_ScanParams.num_components = 3;
		va_ScanParams.components[0].component_selector = 0;
		va_ScanParams.components[0].dc_table_selector = 0;
		va_ScanParams.components[0].ac_table_selector = 0;
		va_ScanParams.components[1].component_selector = 1;
		va_ScanParams.components[1].dc_table_selector = 1;
		va_ScanParams.components[1].ac_table_selector = 1;
		va_ScanParams.components[2].component_selector = 2;
		va_ScanParams.components[2].dc_table_selector = 1;
		va_ScanParams.components[2].ac_table_selector = 1;
		va_status = vaCreateBuffer(va_dpy, va_contextid, VAEncSliceParameterBufferType,
				sizeof(va_ScanParams), 1, &va_ScanParams, &va_ScanParamsBufId);
		if (va_status != VA_STATUS_SUCCESS) {
			LOGE("encode: vaCreateBuffer VAEncSliceParameterBufferType failed (%d)!\n", va_status);
			return va_status;
		}
		va_status = vaRenderPicture(va_dpy, va_contextid, &va_QMatrixBufId, 1);
		if (va_status != VA_STATUS_SUCCESS) {
			LOGE("encode: vaRenderPicture failed (%d)!\n", va_status);
			vaDestroyBuffer(va_dpy, va_QMatrixBufId);
			return va_status;
		}
		va_status = vaRenderPicture(va_dpy, va_contextid, &va_HufTblBufId, 1);
		if (va_status != VA_STATUS_SUCCESS) {
			LOGE("encode: vaRenderPicture failed (%d)!\n", va_status);
			vaDestroyBuffer(va_dpy, va_HufTblBufId);
			return va_status;
		}
		va_status = vaRenderPicture(va_dpy, va_contextid, &va_ScanParamsBufId, 1);
		if (va_status != VA_STATUS_SUCCESS) {
			LOGE("encode: vaRenderPicture failed (%d)!\n", va_status);
			vaDestroyBuffer(va_dpy, va_ScanParamsBufId);
			return va_status;
		}
		vaDestroyBuffer(va_dpy, va_QMatrixBufId);
		vaDestroyBuffer(va_dpy, va_HufTblBufId);
		vaDestroyBuffer(va_dpy, va_ScanParamsBufId);
	}

	/* End picture */
	va_status = vaEndPicture(va_dpy, va_contextid);
	if (va_status != VA_STATUS_SUCCESS) {
		LOGE("encode: vaEndPicture failed (%d)!\n", va_status);
		vaDestroyBuffer(va_dpy, va_picparabufferid);
		return va_status;
	}

	reserved_image_seq = image_seq;
	encoder_status = LIBVA_ENCODING;
	LOGV("encode: done\n");
	return VA_STATUS_SUCCESS;
}

int IntelImageEncoder::getCodedSize(unsigned int *coded_data_sizep)
{
	VAStatus va_status;
	VACodedBufferSegment *coded_segment = NULL;

	if (encoder_status != LIBVA_ENCODING) {
		LOGE("getCodedSize: no encoding active to get coded data!\n");
		return VA_STATUS_ERROR_OPERATION_FAILED;
	}

	if (NULL == coded_data_sizep) {
		LOGE("getCodedSize: invalid NULL pointer as input paramter!\n");
		return VA_STATUS_ERROR_INVALID_PARAMETER;
	}

	if (VA_INVALID_SURFACE == va_surfaceid[reserved_image_seq]) {
		LOGE("getCodedSize: invalid image, probably already destroyed!\n");
		return VA_STATUS_ERROR_OPERATION_FAILED;
	}

	/* Sync surface */
	va_status = vaSyncSurface(va_dpy, va_surfaceid[reserved_image_seq]);
	if (va_status != VA_STATUS_SUCCESS) {
		LOGE("getCodedSize: vaSyncSurface failed (%d)!\n", va_status);
		reserved_image_seq = -1;
		encoder_status = LIBVA_CONTEXT_CREATED;
		return va_status;
	}

	/* Map the coded buffer */
	va_status = vaMapBuffer(va_dpy, va_codedbufferid, (void **)&va_codedbuffersegment);
	if (va_status != VA_STATUS_SUCCESS) {
		LOGE("getCoded: vaMapBuffer failed (%d)!\n", va_status);
		reserved_image_seq = -1;
		encoder_status = LIBVA_CONTEXT_CREATED;
		return va_status;
	}

	/* Initialize coded data size */
	*coded_data_sizep = 0;
	coded_segment = va_codedbuffersegment;

	/* Get the total size of coded data */
	while (coded_segment != NULL) {
		*coded_data_sizep += coded_segment->size;
		coded_segment = (VACodedBufferSegment *)coded_segment->next;
	}
	coded_data_size = *coded_data_sizep;

	reserved_image_seq = -1;
	encoder_status = LIBVA_PENDING_GET_CODED;

	LOGV("getCodedSize: done\n");
	return va_status;
}

int IntelImageEncoder::getCoded(void *user_coded_buf,
				unsigned int user_coded_buf_size)
{
	VAStatus va_status;
	unsigned int copied_size = 0;

	if (encoder_status != LIBVA_PENDING_GET_CODED) {
		LOGE("getCoded: no ready coded data!\n");
		return VA_STATUS_ERROR_OPERATION_FAILED;
	}

	if (NULL == user_coded_buf) {
		LOGE("getCoded: invalid NULL pointer as input paramter!\n");
		return VA_STATUS_ERROR_INVALID_PARAMETER;
	}

	if (user_coded_buf_size < coded_data_size) {
		LOGE("getCoded: coded buffer was smaller than coded data size!\n");
		return VA_STATUS_ERROR_OPERATION_FAILED;
	}

	/* Get the total size of coded data */
	while (va_codedbuffersegment != NULL) {
		memcpy((void *)((unsigned long)user_coded_buf+copied_size),
				va_codedbuffersegment->buf,
				va_codedbuffersegment->size);
		copied_size += va_codedbuffersegment->size;
		va_codedbuffersegment = (VACodedBufferSegment *)va_codedbuffersegment->next;
	}

	/* Reset coded records */
	va_codedbuffersegment = NULL;
	coded_data_size = 0;

	reserved_image_seq = -1;
	encoder_status = LIBVA_CONTEXT_CREATED;

	va_status = vaUnmapBuffer(va_dpy, va_codedbufferid);
	if (va_status != VA_STATUS_SUCCESS) {
		LOGE("getCoded: vaUnmapBuffer failed (%d)!\n", va_status);
		return va_status;
	}

	LOGV("getCoded: done\n");
	return va_status;
}

int IntelImageEncoder::destroySourceSurface(int image_seq)
{
	VAStatus va_status;

	if ((image_seq < 0) || (images_count < 1) || (0 == va_surfaceid[image_seq])) {
		LOGE("destroySourceSurface: invalid image sequential number!\n");
		return VA_STATUS_ERROR_INVALID_PARAMETER;
	} else if (image_seq == reserved_image_seq) {
		LOGE("destroySourceSurface: Image %d was under encoding and can't be destroyed!\n",
			image_seq);
		return VA_STATUS_ERROR_OPERATION_FAILED;
	}

	if (LIBVA_UNINITIALIZED == encoder_status) {
		LOGE("destroySourceSurface: uninitialized, not ready to destroy surface!\n");
		return VA_STATUS_ERROR_OPERATION_FAILED;
	}

	/* Destroy a source surface */
	va_status = vaDestroySurfaces(va_dpy, &va_surfaceid[image_seq], 1);
	if (va_status != VA_STATUS_SUCCESS) {
		LOGE("destroySourceSurface: vaDestroySurfaces failed (%d)!\n", va_status);
	}

	va_surfaceid[image_seq] = 0;
	surface_width[image_seq] = 0;
	surface_height[image_seq] = 0;
	surface_fourcc[image_seq] = 0;

	--images_count;

	return va_status;
}

int IntelImageEncoder::destroyContext(void)
{
	VAStatus va_status = VA_STATUS_SUCCESS;
	VAStatus va_final_status = VA_STATUS_SUCCESS;

	if (0 == va_contextid) {
		LOGE("destroyContext: no context to destroy!\n");
		return VA_STATUS_ERROR_OPERATION_FAILED;
	}

	if (LIBVA_ENCODING == encoder_status) {
		LOGE("destroyContext: encoding was ongoing, can't destroy context!\n");
		return VA_STATUS_ERROR_OPERATION_FAILED;
	}

	if (LIBVA_PENDING_GET_CODED == encoder_status) {
		va_codedbuffersegment = NULL;
		coded_data_size = 0;

		va_status = vaUnmapBuffer(va_dpy, va_codedbufferid);
		if (va_status != VA_STATUS_SUCCESS) {
			LOGE("destroyContext: vaUnmapBuffer failed (%d)!\n", va_status);
		}
		va_final_status |= va_status;
	}

	/* Destroy the coded buffer */
	va_status = vaDestroyBuffer(va_dpy, va_codedbufferid);
	if (va_status != VA_STATUS_SUCCESS) {
		LOGE("destroyContext: vaDestroyBuffer VAEncCodedBufferType failed (%d)!\n", va_status);
	}
	va_final_status |= va_status;
	va_codedbufferid = 0;
	coded_buf_size = 0;

	/* Destroy context */
	va_status = vaDestroyContext(va_dpy, va_contextid);
	if (va_status != VA_STATUS_SUCCESS) {
		LOGE("destroyContext: vaDestroyContext failed (%d)!\n", va_status);
	}
	va_final_status |= va_status;
	va_contextid = 0;
	context_width = 0;
	context_height = 0;
	context_fourcc = 0;

	/* Destroy config */
	va_status = vaDestroyConfig(va_dpy, va_configid);
	if (va_status != VA_STATUS_SUCCESS) {
		LOGE("destroyContext: vaDestroyConfig failed (%d)!\n", va_status);
	}
	va_final_status |= va_status;
	va_configid = 0;

	encoder_status = LIBVA_INITIALIZED;

	LOGV("destroyContext: done\n");
	return va_final_status;
}

int IntelImageEncoder::deinitializeEncoder(void)
{
	int i;
	VAStatus va_status;

	if (NULL == va_dpy) {
		LOGE("deinitializeEncoder: no LibVA display to deinitialized!\n");
		return VA_STATUS_ERROR_OPERATION_FAILED;
	}

	if (LIBVA_ENCODING == encoder_status) {
		LOGE("deinitializeEncoder: encoding was ongoing, can't deinitialize LibVA!\n");
		return VA_STATUS_ERROR_OPERATION_FAILED;
	} else if ((LIBVA_CONTEXT_CREATED == encoder_status) ||
		(LIBVA_PENDING_GET_CODED == encoder_status)) {
		/* Destroy context if it exists */
		destroyContext();
	}

	if (images_count > 0) {
		for (i=0; i<INTEL_IMAGE_ENCODER_MAX_BUFFERS; ++i) {
			if (va_surfaceid[i]) {
				/* Destroy any surface if it exists */
				destroySourceSurface(i);
			}
		}
	}

	va_status = vaTerminate(va_dpy);
	if (va_status != VA_STATUS_SUCCESS) {
		LOGE("deinitializeEncoder: vaTerminate failed (%d)!\n", va_status);
	}

	memset((void *)&va_configattrib, 0, sizeof(va_configattrib));
	va_dpy = NULL;
	encoder_status = LIBVA_UNINITIALIZED;

	LOGV("deinitializeEncoder: done\n");
	return va_status;
}
