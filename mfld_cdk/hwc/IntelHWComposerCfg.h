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
 *    Lei Zhang <lei.zhang@intel.com>
 *
 */
#ifndef __INTEL_HWCOMPOSER_CFG_CPP__
#define __INTEL_HWCOMPOSER_CFG_CPP__

#define HWC_CFG_PATH "/etc/hwc.cfg"
#define CFG_STRING_LEN 1024

typedef struct {
    unsigned char enable;
    unsigned int log_level;
    unsigned int bypasspost;
} hwc_cfg;

enum hwc_log_level {
    NO_DEBUG = 0x00,
    HWC_DEBUG = 0x01,
    PLANE_DEBUG = 0x02,
    SPRITE_DEBUG = 0x04,
    OVERLAY_DEBUG = 0x08,
    WIDI_DEBUG = 0x10,
    LAYER_DEBUG = 0x20,
    MONITOR_DEBUG = 0x40,
    BUFFER_DEBUG = 0x80,
};

#define ALLOW_PRINT(cfg, level) ((cfg & level) == level) ? true : false

#define ALLOW_NO_PRINT         ALLOW_PRINT(cfg.log_level, NO_DEBUG)
#define ALLOW_HWC_PRINT        ALLOW_PRINT(cfg.log_level, HWC_DEBUG)
#define ALLOW_PLANE_PRINT      ALLOW_PRINT(cfg.log_level, PLANE_DEBUG)
#define ALLOW_SPRITE_PRINT     ALLOW_PRINT(cfg.log_level, SPRITE_DEBUG)
#define ALLOW_OVERLAY_PRINT    ALLOW_PRINT(cfg.log_level, OVERLAY_DEBUG)
#define ALLOW_WIDI_PRINT       ALLOW_PRINT(cfg.log_level, WIDI_DEBUG)
#define ALLOW_LAYER_PRINT      ALLOW_PRINT(cfg.log_level, LAYER_DEBUG)
#define ALLOW_MONITOR_PRINT    ALLOW_PRINT(cfg.log_level, MONITOR_DEBUG)
#define ALLOW_BUFFER_PRINT     ALLOW_PRINT(cfg.log_level, BUFFER_DEBUG)

extern hwc_cfg cfg;

#endif /*__INTEL_HWCOMPOSER_CFG_CPP__*/
