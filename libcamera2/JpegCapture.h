/*
 * Copyright (c) 2014 Intel Corporation
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

#ifndef ANDROID_LIBCAMERA_JPEG_CAPTURE_H
#define ANDROID_LIBCAMERA_JPEG_CAPTURE_H

#include <stdint.h>
#include <endian.h>
#include <system/camera.h>

#define V4L2_PIX_FMT_CONTINUOUS_JPEG V4L2_PIX_FMT_JPEG

namespace android {

const int NUM_OF_JPEG_CAPTURE_SNAPSHOT_BUF = 6;
const int YUV_MAX_WIDTH = 4128;
const int YUV_MAX_HEIGHT = 3096;

// Frame size and place
const size_t JPEG_INFO_START = 2048;
const size_t JPEG_INFO_SIZE = 2048;
const size_t NV12_META_START = JPEG_INFO_START + JPEG_INFO_SIZE;
const size_t NV12_META_SIZE = 4096;
const size_t JPEG_META_START = NV12_META_START + NV12_META_SIZE;
const size_t JPEG_META_SIZE = 4096;
const size_t JPEG_DATA_START = JPEG_META_START + JPEG_META_SIZE;
const size_t JPEG_DATA_SIZE = 0x800000;
const size_t JPEG_FRAME_SIZE = JPEG_DATA_START + JPEG_DATA_SIZE;

// HDR frame size and place
const size_t HDR_INFO_START = 2048;
const size_t HDR_INFO_SIZE = 2048;
const size_t YUV_META_START = HDR_INFO_START + HDR_INFO_SIZE;
const size_t YUV_META_SIZE = 4096;
const size_t YUV422_DATA_START = YUV_META_START + YUV_META_SIZE;
const size_t YUV422_DATA_SIZE = YUV_MAX_WIDTH * YUV_MAX_HEIGHT * 2;
const size_t HDR_FRAME_SIZE = 25624576;

// JPEG INFO addresses
const size_t JPEG_INFO_START_MARKER_ADDR = 0x0;
const size_t JPEG_INFO_MODE_ADDR = 0xF;
const size_t JPEG_INFO_COUNT_ADDR = 0x10;
const size_t JPEG_INFO_SIZE_ADDR = 0x13;
const size_t JPEG_INFO_YUV_FRAME_ID_ADDR = 0x17;
const size_t JPEG_INFO_THUMBNAIL_FRAME_ID_ADDR = 0x1B;
const size_t JPEG_INFO_END_MARKER_ADDR = 0x1F;
const size_t JPEG_INFO_Q_VALUE_ADDR = 0x30;
const size_t JPEG_INFO_JPEG_SIZE_Q_VALUE_ADDR = 0x32;

// HDR INFO addresses
const size_t HDR_INFO_START_MARKER_ADDR = 0x0;
const size_t HDR_INFO_MODE_ADDR = 0x0F;
const size_t HDR_INFO_COUNT_ADDR = 0x10;

// HDR INFO DATA
const char HDR_INFO_MODE_HDR = 0x10;
const char HDR_INFO_MODE_LLS = 0x20;

// JPEG INFO data
const char JPEG_INFO_START_MARKER[] =  "JPEG INFO-START";
const char JPEG_INFO_END_MARKER[] = "JPEG INFO-END";
const char HDR_INFO_START_MARKER[] = "HDR INFO-START";

enum JpegFrameType {
    JPEG_FRAME_TYPE_META = 0x00,
    JPEG_FRAME_TYPE_FULL = 0x01,
    JPEG_FRAME_TYPE_SPLIT = 0x02
};

// NV12 META addresses
const size_t NV12_META_START_MARKER_ADDR = 0x0;
const size_t NV12_META_FRAME_COUNT_ADDR = 0xE;
const size_t NV12_META_ISO_ADDR = 0x1C;
const size_t NV12_META_EXPOSURE_BIAS_ADDR = 0X20;
const size_t NV12_META_TV_ADDR = 0x24;
const size_t NV12_META_BV_ADDR = 0x28;
const size_t NV12_META_EXPOSURE_TIME_DENOMINATOR_ADDR = 0X2C;
const size_t NV12_META_FLASH_ADDR = 0x3C;
const size_t NV12_META_AV_ADDR = 0x228;
const size_t NV12_META_AF_STATE_ADDR = 0x846;

enum AfState {
    // AF states
    AF_INVALID = 0x0010,
    AF_FOCUSING = 0x0020,
    AF_SUCCESS = 0x0030,
    AF_FAIL = 0x0040,
    // CAF states
    CAF_RESTART_CHECK = 0x0001,
    CAF_FOCUSING = 0x0002,
    CAF_SUCCESS = 0x0003,
    CAF_FAIL = 0x0004
};

const size_t NV12_META_END_MARKER_ADDR = 0xFF4;
const size_t NV12_META_TOP_OFFSET_ADDR  = 0x13E;
const size_t NV12_META_LEFT_OFFSET_ADDR = 0x13C;
const size_t NV12_META_FD_ONOFF_ADDR = 0x70;
const size_t NV12_META_FD_COUNT_ADDR = 0x72;
const size_t NV12_META_FIRST_FACE_ADDR = 0x74;
const size_t NV12_META_NEED_LLS_ADDR = 0x2B8;

const uint16_t NV12_META_MAX_FACE_COUNT = 16;

// NV12 META data
const char NV12_META_START_MARKER[] =  "METADATA-START";
const char NV12_META_END_MARKER[] = "METADATA-END";

// Generic ON/OFF bits
const int EXTISP_FEAT_ON = 1;
const int EXTISP_FEAT_OFF = 0;

// JPEG META addresses
const size_t JPEG_META_FRAME_COUNT_ADDR = 0x13;

// make sure these don't collide with intel extensions
enum {
    CAMERA_CMD_EXTISP_HDR        = 1014,
    CAMERA_CMD_EXTISP_LLS        = 1264,
    CAMERA_CMD_FRONT_SS          = 0x4F1,  /* TODO, SS did not confirmed the name and value of Smart Stabilization. Please change them when SS give the final confirmation. */
    CAMERA_CMD_EXTISP_KIDS_MODE  = 0x4F2,
    CAMERA_CMD_AUTO_LOW_LIGHT    = 1351,
    CAMERA_CMD_BURST_START       = 1571,
    CAMERA_CMD_BURST_STOP        = 1572,
    CAMERA_CMD_BURST_DELETE      = 1573
};

struct extended_frame_metadata_t {
    int32_t number_of_faces;
    camera_face_t *faces;
    int32_t needLLS;
};

static uint32_t getU32fromFrame(uint8_t* framePtr, size_t addr) {

    uint32_t result = *((uint32_t*)(framePtr + addr));

    result = be32toh(result);

    return result;
}

static uint16_t getU16fromFrame(uint8_t* framePtr, size_t addr) {

    uint16_t result = *((uint16_t*)(framePtr + addr));

    result = be16toh(result);

    return result;
}

} // namespace

#endif // ANDROID_LIBCAMERA_JPEG_CAPTURE_H
