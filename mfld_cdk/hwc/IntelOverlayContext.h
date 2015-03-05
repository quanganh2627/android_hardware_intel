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
#ifndef __INTEL_OVERLAY_CONTEXT_H__
#define __INTEL_OVERLAY_CONTEXT_H__

#include <IntelOverlayHW.h>
#include <pthread.h>

extern "C" {
#include "xf86drm.h"
#include "xf86drmMode.h"
}

typedef enum {
    PIPE_MIPI0 = 0,
    PIPE_MIPI1,
    PIPE_HDMI,
} intel_display_pipe_t;

typedef enum {
    OUTPUT_MIPI0 = 0,
    OUTPUT_MIPI1,
    OUTPUT_HDMI,
    OUTPUT_MAX,
} intel_drm_output_t;

typedef enum {
	OVERLAY_MIPI0 = 0,
	OVERLAY_CLONE_MIPI0,
	OVERLAY_CLONE_MIPI1,
	OVERLAY_CLONE_DUAL,
	OVERLAY_EXTEND,
	OVERLAY_UNKNOWN,
} intel_overlay_mode_t;

typedef struct {
    drmModeConnection connections[OUTPUT_MAX];
    drmModeModeInfo modes[OUTPUT_MAX];
    bool mode_valid[OUTPUT_MAX];
    intel_overlay_mode_t display_mode;
} intel_drm_output_state_t;

typedef struct {
    int x;
    int y;
    int w;
    int h;
} intel_overlay_position_t;

typedef enum {
    OVERLAY_INIT = 0,
    OVERLAY_RESET,
    OVERLAY_DISABLED,
    OVERLAY_ENABLED,
} intel_overlay_state_t;

typedef enum {
    OVERLAY_ROTATE_0 = 0,
    OVERLAY_ROTATE_90,
    OVERLAY_ROTATE_180,
    OVERLAY_ROTATE_270,
} intel_overlay_rotation_t;

typedef enum {
    OVERLAY_ORIENTATION_PORTRAINT = 1,
    OVERLAY_ORIENTATION_LANDSCAPE,
} intel_overlay_orientation_t;

class IntelOverlayContext
{
public:
    enum {
        FLASH_NEEDED = 0x00000001UL,
        WAIT_VBLANK  = 0x00000002UL,
        UPDATE_COEF  = 0x00000004UL,
    };
private:
    int mHandle;
    intel_overlay_context_t *mContext;
    intel_overlay_back_buffer_t *mOverlayBackBuffer;
    IntelOverlayBuffer *mBackBuffer;
    int mSize;
    int mDrmFd;
    IntelBufferManager *mBufferManager;
protected:
    bool backBufferInit();
    bool bufferOffsetSetup(IntelDisplayDataBuffer& buf);
    uint32_t calculateSWidthSW(uint32_t offset, uint32_t width);
    bool coordinateSetup(IntelDisplayDataBuffer& buf);
    bool setCoeffRegs(double *coeff, int mantSize, coeffPtr pCoeff, int pos);
    void updateCoeff(int taps, double fCutoff, bool isHoriz, bool isY,
                     coeffPtr pCoeff);
    bool scalingSetup(IntelDisplayDataBuffer& buffer);
    intel_overlay_state_t getOverlayState() const;
    void setOverlayState(intel_overlay_state_t state);
    void checkPosition(int& x, int& y, int& w, int& h, IntelDisplayDataBuffer& buffer);

    void lock();
    void unlock();
public:
    IntelOverlayContext(int drmFd, IntelBufferManager *bufferManager = NULL)
        :mHandle(0), mContext(0),
         mOverlayBackBuffer(0),
         mBackBuffer(0),
         mSize(0), mDrmFd(drmFd),
         mBufferManager(bufferManager) {}
    ~IntelOverlayContext();

    // ashmen operations
    bool create();
    bool open(int handle, int size);
    bool destroy();
    void clean();
    int getHandle() const { return mHandle; }
    int getSize() const { return mSize; }

    // operations to context
    void setBackBufferGttOffset(const uint32_t gttOffset);
    uint32_t getGttOffsetInPage();
    void setOutputConnection(const int output, drmModeConnection connection);
    drmModeConnection getOutputConnection(const int output);
    void setOutputMode(const int output, drmModeModeInfoPtr mode, int valid);
    void setDisplayMode(intel_overlay_mode_t displayMode);
    intel_overlay_mode_t getDisplayMode();
    intel_overlay_orientation_t getOrientation();
    intel_overlay_back_buffer_t* getBackBuffer() { return mOverlayBackBuffer; }

    // interfaces for data device
    bool setDataBuffer(IntelDisplayDataBuffer& dataBuffer);

    // interfaces for control device
    void setRotation(int rotation);
    void setPosition(int x, int y, int w, int h);

    // interfaces for both data & control devices
    bool flush(uint32_t flags);
    bool enable();
    bool disable();
    bool reset();
    void setPipe(intel_display_pipe_t pipe);
    void setPipeByMode(intel_overlay_mode_t displayMode);
};

#endif /*__INTEL_OVERLAY_CONTEXT_H__ */
