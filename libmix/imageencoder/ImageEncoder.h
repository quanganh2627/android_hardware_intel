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

#ifndef __LIBMIX_INTEL_IMAGE_ENCODER_H__
#define __LIBMIX_INTEL_IMAGE_ENCODER_H__

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <va/va.h>
#include <va/va_android.h>
#include <va/va_tpi.h>
#include <va/va_enc_jpeg.h>

#define INTEL_IMAGE_ENCODER_DEFAULT_QUALITY 90
#define INTEL_IMAGE_ENCODER_MAX_QUALITY 100
#define INTEL_IMAGE_ENCODER_MIN_QUALITY 1
#define INTEL_IMAGE_ENCODER_MAX_BUFFERS 64
#define INTEL_IMAGE_ENCODER_REQUIRED_STRIDE 64
#ifndef VA_FOURCC_YV16
#define VA_FOURCC_YV16 0x36315659
#endif
#define SURFACE_TYPE_USER_PTR 0x00000004
#define SURFACE_TYPE_GRALLOC 0x00100000

#define LOGE ALOGE
#define LOGV ALOGV

typedef enum {
    VIDENC_UNKNOWN = -1,
    VIDENC_MRST = 0,
    VIDENC_LEXT = 1,
    VIDENC_MFLD = 2,
    VIDENC_MRFL = 3,
    VIDENC_BYT = 10,
    VIDENC_CHT = 11,
}VideoEncodeHW;

class IntelImageEncoder {
public:
	IntelImageEncoder(void);
	~IntelImageEncoder(void) {};

	/*
	 * initializeEncoder initializes Intel JPEG HW encoder,
	 * only needs to be called once at booting unless errors happen.
	 * Return zero for success and non-zero for failure.
	 */
	int initializeEncoder(void);

	/*
	 * createSourceSurface creates a surface for encoding.
	 * based on the input buffer taking source image data.
	 * This can be called after the encoder is initialized.
	 * Parameters:
	 * source_type: the type of source buffer:
	 *              SURFACE_TYPE_USER_PTR for malloced buffer;
	 *              SURFACE_TYPE_GRALLOC for gralloced buffer.
	 * source_buffer: the input source buffer address, which
	 *                must be aligned to page-size
	 * width: the width of source image data
	 * height: the height of source image data
	 * stride: the stride of source image data
	 * fourcc: the fourcc value of source, VA_RT_FORMAT_YUV420
	 *         and VA_RT_FORMAT_YUV422 are supported.
	 * image_seqp: the pointer to a returned image sequential
	 *             number to identity an image and its surface.
	 * Return zero for success and non-zero for failure.
	 */
	int createSourceSurface(int source_type, void *source_buffer,
				unsigned int width,unsigned int height,
				unsigned int stride, unsigned int fourcc,
				int *image_seqp);

	/*
	 * createContext creates the encoding context,
	 * based on the parameters of a existing surface.
	 * This can be called after the encoder is initialized
	 * and at least one surface is created.
	 * Parameters:
	 * first_image_seq: the sequential number of image/surface
	 *                  to be based to create context,
	 *                  the created context will be set to its
	 *                  parameters: width, height, stride & fourcc.
	 * max_coded_sizep: the returned pointer to the max coded buffer.
	 *                  size for references, given the current context.
	 * Return zero for success and non-zero for failure.
	 */
	int createContext(int first_image_seq, unsigned int *max_coded_sizep);
	int createContext(unsigned int *max_coded_sizep)
	{
		return this->createContext(0, max_coded_sizep);
	}

	/*
	 * setQuality changes the current quality value,
	 * whose default value is 90.
	 * This can be called after the encoder is initialized.
	 * Parameters:
	 * new_quality: the new quality value to be set.
	 * Return zero for success and non-zero for failure.
	 */
	int setQuality(unsigned int new_quality);

	/*
	 * encode triggers an image/surface's JPEG encoding.
	 * This can be called after the encoder is initialize
	 * and context is created.
	 * Parameters:
	 * image_seq: the sequential number of an image/surface
	 *            to be encoded. This has not to be the same
	 *            as the one used to create context, but it
	 *            has to be of the same parameters.
	 *            If a surface with different paras needs to
	 *            be encoded, another context based on its
	 *            parameters is needed.
	 * new_quality: the new quality value to be set.
	 * Return zero for success and non-zero for failure.
	 */
	int encode(int image_seq, unsigned int new_quality);
	int encode(int image_seq)
	{
		return this->encode(image_seq, quality);
	}
	int encode(void)
	{
		return this->encode(0, quality);
	}

	/*
	 * getCodedSize waits for the current encoding task's completion
	 * and returns the exact coded size.
	 * Only one encoding task can be triggered under an instance
	 * of IntelImageEncoder at any minute.
	 * This has not be called right after encode is called,
	 * instead, this can be called any minutes after the encoding
	 * is triggered to support both synced/asynced encoding usage.
	 * Parameters:
	 * coded_data_sizep: the returned pointer to the actual size
	 *                   value of coded JPEG.
	 * Return zero for success and non-zero for failure.
	 */
	int getCodedSize(unsigned int *coded_data_sizep);

	/*
	 * getCoded copies coded data out for users.
	 * This should be called after getCodedSize.
	 * Parameters:
	 * user_coded_buf: the input buffer to take coded data.
	 *                 After getCoded is returned with no errors,
	 *                 this buffer will have the coded JPEG in it.
	 * user_coded_buf_size: the size of input buffer.
	 *                      If too small, an error'll be returned.
	 * Return zero for success and non-zero for failure.
	 */
	int getCoded(void *user_coded_buf,
			unsigned int user_coded_buf_size);

	int destroySourceSurface(int image_seq);
	int destroyContext(void);
	int deinitializeEncoder(void);

private:
	typedef enum {
		LIBVA_UNINITIALIZED = 0,
		LIBVA_INITIALIZED,
		LIBVA_CONTEXT_CREATED,
		LIBVA_ENCODING,
		LIBVA_PENDING_GET_CODED,
	}IntelImageEncoderStatus;

	/* Valid since LIBVA_UNINITIALIZED */
	IntelImageEncoderStatus encoder_status;
	unsigned int quality;

	/* Valid Since LIBVA_INITIALIZED */
	VADisplay va_dpy;
	VAConfigAttrib va_configattrib;
	VAConfigAttrib va_configattrib_ext;

	/* Valid if a surface is created */
	unsigned int images_count;
	VASurfaceID va_surfaceid[INTEL_IMAGE_ENCODER_MAX_BUFFERS];
	unsigned int surface_width[INTEL_IMAGE_ENCODER_MAX_BUFFERS];
	unsigned int surface_height[INTEL_IMAGE_ENCODER_MAX_BUFFERS];
	unsigned int surface_fourcc[INTEL_IMAGE_ENCODER_MAX_BUFFERS];

	/* Valid since LIBVA_CONTEXT_CREATED */
	VAConfigID va_configid;
	VAContextID va_contextid;
	unsigned int context_width;
	unsigned int context_height;
	unsigned int context_fourcc;
	VABufferID va_codedbufferid;
	unsigned int coded_buf_size;

	/* Valid since LIBVA_ENCODING */
	int reserved_image_seq;

	/* Valid since LIBVA_PENDING_GET_CODED */
	VACodedBufferSegment *va_codedbuffersegment;
	unsigned int coded_data_size;

	VideoEncodeHW mHwPlatform;

};

#endif /* __LIBMIX_INTEL_IMAGE_ENCODER_H__ */
