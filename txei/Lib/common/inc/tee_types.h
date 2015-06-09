/*************************************************************************
 ** Copyright (C) 2012 Intel Corporation. All rights reserved.          **
 **                                                                     **
 ** Permission is hereby granted, free of charge, to any person         **
 ** obtaining a copy of this software and associated documentation      **
 ** files (the "Software"), to deal in the Software without             **
 ** restriction, including without limitation the rights to use, copy,  **
 ** modify, merge, publish, distribute, sublicense, and/or sell copies  **
 ** of the Software, and to permit persons to whom the Software is      **
 ** furnished to do so, subject to the following conditions:            **
 **                                                                     **
 ** The above copyright notice and this permission notice shall be      **
 ** included in all copies or substantial portions of the Software.     **
 ** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,     **
 ** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF  **
 ** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND               **
 ** NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS **
 ** BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN  **
 ** ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN   **
 ** CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE    **
 ** SOFTWARE.                                                           **
 *************************************************************************/

/**
 * @file drm_types.h
 * @brief Header file for common DRM types
 *
 * @History:
 *-----------------------------------------------------------------------------
 * Date>>...>...Author>.>...Description
 *-----------------------------------------------------------------------------
 * 08/23/12>>...ppesara>>...Original port from Medfield implementation
 */

#ifndef __ACD_TYPES_H__
#define __ACD_TYPES_H__

#include <byteswap.h>
#ifdef MSVS
#include "dx_pal_types.h"
#endif /* MSVS */



/*!
 * Defines
 */
#define MAX_DATA_BUF_PARAMS		2
#define FROM_HOST_PARAM_INDEX		0
#define TO_HOST_PARAM_INDEX		0

typedef uint32_t DRM_RESULT;

typedef enum {
	DATA_UNKNOWN = 0,
	DATA_IN,
	DATA_OUT,
	DATA_INOUT
} data_type_t; 

struct dma_object {
	uint32_t handle;
	uint32_t size;
	data_type_t type;
};

struct data_buffer {
	void *buffer;
	uint32_t size;
	data_type_t type;
};

typedef enum {
	DRM_SCHEME_Netflix,
	DRM_SCHEME_Widevine,
	DRM_SCHEME_WidevineHLS,
} drm_scheme_t;

typedef enum {
	DRMS_UNINITIALIZED,
	DRMS_ACTIVE,
} drm_state_t;

typedef enum {
	S_UNINITIALIZED,
	S_PLAY,
	S_PAUSE,
} session_state_t;

#ifndef NULL
#define NULL 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#define INIT_DATA_BUFFER(data_t, addr, s, t)		\
		do {					\
			data_t.buffer = (void *)addr;	\
			data_t.size = s;		\
			data_t.type = t;		\
		} while(0)

#define INIT_DMA_OBJECT(dma_obj, h, s, t)	\
		do {				\
			dma_obj.handle = h;	\
			dma_obj.size = s;	\
			dma_obj.type = t;	\
		} while(0)

#define INIT_DMA_BUF(data_buf_t, buf_addr, buf_size, buf_type)	\
		do {						\
			data_buf_t.buffer = buf_addr;		\
			data_buf_t.size = buf_size;		\
			data_buf_t.type = buf_type;		\
		} while(0)

#define INIT_FROM_HOST_PARAM_BUF(data_buf_t, buf_addr, buf_size) \
		INIT_DMA_BUF(data_buf_t, buf_addr, buf_size, DATA_IN)

#define INIT_TO_HOST_PARAM_BUF(data_buf_t, buf_addr, buf_size) \
		INIT_DMA_BUF(data_buf_t, buf_addr, buf_size, DATA_OUT)

#endif /* end __DRM_TYPES_H__ */

