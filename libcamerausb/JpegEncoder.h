/*
 * Copyright Samsung Electronics Co.,LTD.
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * JPEG DRIVER MODULE (JpegEncoder.h)
 * Author  : ge.lee       -- initial version
 * Date    : 03 June 2010
 * Purpose : This file implements the JPEG encoder APIs as needed by Camera HAL
 */
#ifndef __JPG_API_H__
#define __JPG_API_H__

#include <stdint.h>
#include <sys/ioctl.h>

#include "Exif.h"

namespace android {

#define MAX_JPG_WIDTH                   4352
#define MAX_JPG_HEIGHT                  3264
#define MAX_JPG_RESOLUTION              (MAX_JPG_WIDTH * MAX_JPG_HEIGHT)

#define MAX_JPG_THUMBNAIL_WIDTH         640
#define MAX_JPG_THUMBNAIL_HEIGHT        480
#define MAX_JPG_THUMBNAIL_RESOLUTION    (MAX_JPG_THUMBNAIL_WIDTH *  \
                                            MAX_JPG_THUMBNAIL_HEIGHT)

#define MAX_RGB_WIDTH                   800
#define MAX_RGB_HEIGHT                  480
#define MAX_RGB_RESOLUTION              (MAX_RGB_WIDTH * MAX_RGB_HEIGHT)

/*******************************************************************************/
/* define JPG & image memory */
/* memory area is 4k(PAGE_SIZE) aligned because of VirtualCopyEx() */
#define JPG_STREAM_BUF_SIZE     \
        (MAX_JPG_RESOLUTION / PAGE_SIZE + 1) * PAGE_SIZE
#define JPG_STREAM_THUMB_BUF_SIZE   \
        (MAX_JPG_THUMBNAIL_RESOLUTION / PAGE_SIZE + 1) * PAGE_SIZE
#define JPG_FRAME_BUF_SIZE  \
        ((MAX_JPG_RESOLUTION * 3) / PAGE_SIZE + 1) * PAGE_SIZE
#define JPG_FRAME_THUMB_BUF_SIZE    \
        ((MAX_JPG_THUMBNAIL_RESOLUTION * 3) / PAGE_SIZE + 1) * PAGE_SIZE
#define JPG_RGB_BUF_SIZE    \
        ((MAX_RGB_RESOLUTION * 4) / PAGE_SIZE + 1) * PAGE_SIZE

#define JPG_TOTAL_BUF_SIZE  (JPG_STREAM_BUF_SIZE + \
                             JPG_STREAM_THUMB_BUF_SIZE + \
                             JPG_FRAME_BUF_SIZE + \
                             JPG_FRAME_THUMB_BUF_SIZE + \
                             JPG_RGB_BUF_SIZE)

#define JPG_MAIN_START      0x00
#define JPG_THUMB_START     JPG_STREAM_BUF_SIZE
#define IMG_MAIN_START      (JPG_STREAM_BUF_SIZE + JPG_STREAM_THUMB_BUF_SIZE)
#define IMG_THUMB_START     (IMG_MAIN_START + JPG_FRAME_BUF_SIZE)
/*******************************************************************************/

typedef enum {
    JPG_FAIL,
    JPG_SUCCESS,
    OK_HD_PARSING,
    ERR_HD_PARSING,
    OK_ENC_OR_DEC,
    ERR_ENC_OR_DEC,
    ERR_UNKNOWN
} jpg_return_status;

class JpegEncoder {
public:
    JpegEncoder();
    virtual ~JpegEncoder();

	// MAP: Added to set thumbnail from external data
	jpg_return_status setThumbData(const unsigned char *thumbBuf, unsigned int thumbSize);

	jpg_return_status makeExif(unsigned char *exifOut,
                               exif_attribute_t *exifIn,
                               unsigned int *size,
                               bool useMainbufForThumb = false);

private:

    inline void writeExifIfd(unsigned char **pCur,
                                 unsigned short tag,
                                 unsigned short type,
                                 unsigned int count,
                                 uint32_t value);
    inline void writeExifIfd(unsigned char **pCur,
                                 unsigned short tag,
                                 unsigned short type,
                                 unsigned int count,
                                 unsigned char *pValue);
    inline void writeExifIfd(unsigned char **pCur,
                                 unsigned short tag,
                                 unsigned short type,
                                 unsigned int count,
                                 rational_t *pValue,
                                 unsigned int *offset,
                                 unsigned char *start);
    inline void writeExifIfd(unsigned char **pCur,
                                 unsigned short tag,
                                 unsigned short type,
                                 unsigned int count,
                                 unsigned char *pValue,
                                 unsigned int *offset,
                                 unsigned char *start);
    bool available;

	unsigned char * m_thumbBuf; // MAP: Added to set thumbnail from external data
	unsigned int m_thumbSize; // MAP: Added to set thumbnail from external data
};
};
#endif /* __JPG_API_H__ */
