/*
 * Copyright (c) 2008-2012, Intel Corporation. All rights reserved.
 *
 * Redistribution.
 * Redistribution and use in binary form, without modification, are
 * permitted provided that the following conditions are met:
 *  * Redistributions must reproduce the above copyright notice and
 * the following disclaimer in the documentation and/or other materials
 * provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its
 * suppliers may be used to endorse or promote products derived from
 * this software without specific  prior written permission.
 *  * No reverse engineering, decompilation, or disassembly of this
 * software is permitted.
 *
 * Limited patent license.
 * Intel Corporation grants a world-wide, royalty-free, non-exclusive
 * license under patents it now or hereafter owns or controls to make,
 * have made, use, import, offer to sell and sell ("Utilize") this
 * software, but solely to the extent that any such patent is necessary
 * to Utilize the software alone, or in combination with an operating
 * system licensed under an approved Open Source license as listed by
 * the Open Source Initiative at http://opensource.org/licenses.
 * The patent license shall not apply to any other combinations which
 * include this software. No hardware per se is licensed hereunder.
 *
 * DISCLAIMER.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors:
 *    Jackie Li <yaodong.li@intel.com>
 *
 */
#ifndef __INTEL_OVERLAY_HW_H__
#define __INTEL_OVERLAY_HW_H__

#include <IntelBufferManager.h>

/*only one overlay data buffer for testing*/
#define PVR_OVERLAY_BUFFER_NUM          1
#define INTEL_OVERLAY_MAX_WIDTH         2048
#define INTEL_OVERLAY_MAX_HEIGHT        2048
#define INTEL_OVERLAY_MIN_STRIDE        512
#define INTEL_OVERLAY_MAX_STRIDE_PACKED (8 * 1024)
#define INTEL_OVERLAY_MAX_STRIDE_LINEAR (4 * 1024)
#define PVR_OVERLAY_MAX_SCALING_RATIO   7

/* Polyphase filter coefficients */
#define N_HORIZ_Y_TAPS                  5
#define N_VERT_Y_TAPS                   3
#define N_HORIZ_UV_TAPS                 3
#define N_VERT_UV_TAPS                  3
#define N_PHASES                        17
#define MAX_TAPS                        5

/* Filter cutoff frequency limits. */
#define MIN_CUTOFF_FREQ                 1.0
#define MAX_CUTOFF_FREQ                 3.0

/*Overlay init micros*/
#define OVERLAY_INIT_CONTRAST           0x4b
#define OVERLAY_INIT_BRIGHTNESS         -19
#define OVERLAY_INIT_SATURATION         0x92
#define OVERLAY_INIT_GAMMA0             0x080808
#define OVERLAY_INIT_GAMMA1             0x101010
#define OVERLAY_INIT_GAMMA2             0x202020
#define OVERLAY_INIT_GAMMA3             0x404040
#define OVERLAY_INIT_GAMMA4             0x808080
#define OVERLAY_INIT_GAMMA5             0xc0c0c0
#define OVERLAY_INIT_COLORKEY           0
#define OVERLAY_INIT_COLORKEYMASK       ((0x0 << 31) | (0X0 << 30))
#define OVERLAY_INIT_CONFIG             ((0x1 << 18) | (0x1 << 3))

/*overlay register values*/
#define OVERLAY_FORMAT_MASK             (0xf << 10)
#define OVERLAY_FORMAT_PACKED_YUV422    (0x8 << 10)
#define OVERLAY_FORMAT_PLANAR_NV12_1    (0x7 << 10)
#define OVERLAY_FORMAT_PLANAR_NV12_2    (0xb << 10)
#define OVERLAY_FORMAT_PLANAR_YUV420    (0xc << 10)
#define OVERLAY_FORMAT_PLANAR_YUV422    (0xd << 10)
#define OVERLAY_FORMAT_PLANAR_YUV41X    (0xe << 10)

#define OVERLAY_PACKED_ORDER_YUY2       (0x0 << 14)
#define OVERLAY_PACKED_ORDER_YVYU       (0x1 << 14)
#define OVERLAY_PACKED_ORDER_UYVY       (0x2 << 14)
#define OVERLAY_PACKED_ORDER_VYUY       (0x3 << 14)
#define OVERLAY_PACKED_ORDER_MASK       (0x3 << 14)

#define OVERLAY_MEMORY_LAYOUT_TILED     (0x1 << 19)
#define OVERLAY_MEMORY_LAYOUT_LINEAR    (0x0 << 19)

#define OVERLAY_MIRRORING_NORMAL        (0x0 << 17)
#define OVERLAY_MIRRORING_HORIZONTAL    (0x1 << 17)
#define OVERLAY_MIRRORING_VERTIACAL     (0x2 << 17)
#define OVERLAY_MIRRORING_BOTH          (0x3 << 17)

#define BUF_TYPE                (0x1<<5)
#define BUF_TYPE_FRAME          (0x0<<5)
#define BUF_TYPE_FIELD          (0x1<<5)
#define TEST_MODE               (0x1<<4)
#define BUFFER_SELECT           (0x3<<2)
#define BUFFER0                 (0x0<<2)
#define BUFFER1                 (0x1<<2)
#define FIELD_SELECT            (0x1<<1)
#define FIELD0                  (0x0<<1)
#define FIELD1                  (0x1<<1)
#define OVERLAY_ENABLE          0x1


/* Overlay contorl registers*/
typedef struct pvr_overlay_control_block_t {
    uint32_t OBUF_0Y;
    uint32_t OBUF_1Y;
    uint32_t OBUF_0U;
    uint32_t OBUF_0V;
    uint32_t OBUF_1U;
    uint32_t OBUF_1V;
    uint32_t OSTRIDE;
    uint32_t YRGB_VPH;
    uint32_t UV_VPH;
    uint32_t HORZ_PH;
    uint32_t INIT_PHS;
    uint32_t DWINPOS;
    uint32_t DWINSZ;
    uint32_t SWIDTH;
    uint32_t SWIDTHSW;
    uint32_t SHEIGHT;
    uint32_t YRGBSCALE;
    uint32_t UVSCALE;
    uint32_t OCLRC0;
    uint32_t OCLRC1;
    uint32_t DCLRKV;
    uint32_t DCLRKM;
    uint32_t SCHRKVH;
    uint32_t SCHRKVL;
    uint32_t SCHRKEN;
    uint32_t OCONFIG;
    uint32_t OCMD;
    uint32_t RESERVED1;
    uint32_t OSTART_0Y;
    uint32_t OSTART_1Y;
    uint32_t OSTART_0U;
    uint32_t OSTART_0V;
    uint32_t OSTART_1U;
    uint32_t OSTART_1V;
    uint32_t OTILEOFF_0Y;
    uint32_t OTILEOFF_1Y;
    uint32_t OTILEOFF_0U;
    uint32_t OTILEOFF_0V;
    uint32_t OTILEOFF_1U;
    uint32_t OTILEOFF_1V;
    uint32_t FASTHSCALE;
    uint32_t UVSCALEV;

    uint32_t RESERVEDC[(0x200 - 0xA8) / 4];
    uint16_t Y_VCOEFS[N_VERT_Y_TAPS * N_PHASES];
    uint16_t RESERVEDD[0x100 / 2 - N_VERT_Y_TAPS * N_PHASES];
    uint16_t Y_HCOEFS[N_HORIZ_Y_TAPS * N_PHASES];
    uint16_t RESERVEDE[0x200 / 2 - N_HORIZ_Y_TAPS * N_PHASES];
    uint16_t UV_VCOEFS[N_VERT_UV_TAPS * N_PHASES];
    uint16_t RESERVEDF[0x100 / 2 - N_VERT_UV_TAPS * N_PHASES];
    uint16_t UV_HCOEFS[N_HORIZ_UV_TAPS * N_PHASES];
    uint16_t RESERVEDG[0x100 / 2 - N_HORIZ_UV_TAPS * N_PHASES];
} intel_overlay_back_buffer_t;

typedef struct {
    uint8_t sign;
    uint16_t mantissa;
    uint8_t exponent;
} coeffRec, *coeffPtr;

#endif /*__INTEL_OVERLAY_HW_H__*/
