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
#ifndef __INTEL_DISPLAY_PLANE_MANAGER_H__
#define __INTEL_DISPLAY_PLANE_MANAGER_H__

#include <cutils/log.h>
#include <cutils/atomic.h>
#include <utils/KeyedVector.h>
#include <hardware/hardware.h>
#include <hardware/gralloc.h>
#include <hal_public.h>
#include <IntelHWComposerDrm.h>
#include <IntelHWComposerDump.h>
#include <IntelBufferManager.h>
#include <IntelOverlayHW.h>
#include <IntelHWComposerCfg.h>

#include <linux/psb_drm.h>

typedef struct intel_sprite_context sprite_plane_context_t;
typedef struct intel_overlay_context overlay_plane_context_t;
typedef struct intel_sprite_context intel_sprite_context_t;
typedef struct mdfld_plane_contexts mdfld_plane_contexts_t;

class IntelDisplayPlaneContext {
public:
    IntelDisplayPlaneContext() {}
    virtual ~IntelDisplayPlaneContext() {}
};

typedef struct intel_display_plane_position {
    int left;
    int top;
    int right;
    int bottom;
} intel_display_plane_position_t;

typedef enum {
    PIPE_MIPI0 = 0,
    PIPE_MIPI1,
    PIPE_HDMI,
} intel_display_pipe_t;

class IntelDisplayPlane : public IntelHWComposerDump {
public:
    enum {
            DISPLAY_PLANE_SPRITE = 1,
            DISPLAY_PLANE_PRIMARY,
            DISPLAY_PLANE_OVERLAY,
            DISPLAY_PLANE_RGB_OVERLAY,
    };

        // flush flags
        enum {
            FLASH_NEEDED     = 0x00000001UL,
            WAIT_VBLANK      = 0x00000002UL,
            UPDATE_COEF      = 0x00000004UL,
            UPDATE_CONTROL   = 0x00000008UL,
            UPDATE_SURFACE   = 0x00000010UL,
            BOB_DEINTERLACE  = 0x00000020UL,
            DELAY_DISABLE    = 0x00000040UL,
            WMS_NEEDED       = 0x00000080UL,
            FLASH_GAMMA      = 0x00000100UL,
            HDMI_HP          = 0x00000200UL,
        };
protected:
    int mDrmFd;
    int mType;
    int mIndex;
    IntelBufferManager *mBufferManager;
    IntelDisplayPlaneContext *mContext;

    // plane parameters
    IntelDisplayBuffer *mDataBuffer;
    uint32_t mDataBufferHandle;
    intel_display_plane_position_t mPosition;
    bool mForceBottom;

    bool mInitialized;
public:
    IntelDisplayPlane(int fd, int type,
                      int index, IntelBufferManager *bufferManager)
        : mDrmFd(fd), mType(type), mIndex(index),
          mBufferManager(bufferManager),
          mContext(0), mDataBuffer(0), mDataBufferHandle(0),
          mForceBottom(false),
          mInitialized(false) {
	    memset(&mPosition, 0, sizeof(intel_display_plane_position_t));
    }
    virtual ~IntelDisplayPlane() {}
    virtual bool initCheck() const { return mInitialized; }
    virtual int getPlaneType() const { return mType; }
    virtual IntelDisplayPlaneContext* getContext() { return mContext; }
    virtual void setPosition(int left, int top, int right, int bottom) {
        mPosition.left = left;
        mPosition.top = top;
        mPosition.right = right;
        mPosition.bottom = bottom;
    }
    virtual bool setDataBuffer(uint32_t handle, uint32_t flags, IMG_native_handle_t* nHandle) { return true; }
    virtual bool setDataBuffer(IntelDisplayBuffer& buffer) {
        mDataBuffer = &buffer;
        return true;
    }
    virtual IntelDisplayBuffer* getDataBuffer() {
        return mDataBuffer;
    }
    virtual uint32_t getDataBufferHandle() {
        return mDataBufferHandle;
    }
    virtual bool invalidateDataBuffer() { return true; }
    virtual bool flip(void *context, uint32_t flags) { return true; }
    virtual void waitForFlipCompletion() {}
    virtual bool enable() { return true; }
    virtual bool disable() { return true; }
    virtual bool reset() { return true; }
    virtual void setPipe(intel_display_pipe_t pipe) {}
    virtual void setPipeByMode(intel_overlay_mode_t displayMode) {}
    virtual void forceBottom(bool bottom) {}

    // DRM mode change handler
    virtual uint32_t onDrmModeChange() { return 0; }

    friend class IntelDisplayPlaneManager;
};

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

typedef struct {
    // back buffer info
    uint32_t back_buffer_handle;
    uint32_t gtt_offset_in_page;

    // pipe configuration
    uint32_t pipe;

    // dest info
    intel_overlay_position_t position;
    intel_overlay_rotation_t rotation;
    intel_overlay_orientation_t orientation;
    bool is_rotated;
    bool position_changed;
    bool is_interlaced;

    // power info
    intel_overlay_state_t state;

    // ashmem related
    pthread_mutex_t lock;
    pthread_mutexattr_t attr;
    volatile int32_t refCount;
} intel_overlay_context_t;

class IntelOverlayContext : public IntelDisplayPlaneContext
{
protected:
    int mHandle;
    intel_overlay_context_t *mContext;
    intel_overlay_back_buffer_t *mOverlayBackBuffer;
    struct {
         uint32_t OSTART_0Y;
         uint32_t OSTART_1Y;
         uint32_t OSTART_0U;
         uint32_t OSTART_0V;
         uint32_t OSTART_1U;
         uint32_t OSTART_1V;
    } mOverlaySurface;
    IntelDisplayBuffer *mBackBuffer;
    int mSize;
    int mDrmFd;
    IntelBufferManager *mBufferManager;
    int yStride;
    int uvStride;
    bool mOnTop;

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

    intel_overlay_mode_t drmModeChanged(IntelOverlayContext& context);

    void lock();
    void unlock();
public:
    IntelOverlayContext(int drmFd, IntelBufferManager *bufferManager = NULL)
        :mHandle(0),
         mContext(0),
         mOverlayBackBuffer(0),
         mBackBuffer(0),
         mSize(0), mDrmFd(drmFd),
         mBufferManager(bufferManager) {}
    IntelOverlayContext()
        :mHandle(0),
         mContext(0),
         mOverlayBackBuffer(0),
         mBackBuffer(0),
         mSize(0),
         mOnTop(false) {}

    ~IntelOverlayContext();

    // ashmen operations
    bool create();
    bool open(int handle, int size);
    bool destroy();
    void clean();
    int getSize() const { return mSize; }

    // operations to context
    void setBackBufferGttOffset(const uint32_t gttOffset);
    uint32_t getGttOffsetInPage();

    intel_overlay_orientation_t getOrientation();
    intel_overlay_back_buffer_t* getBackBuffer() { return mOverlayBackBuffer; }
    bool updateBackBuffer2Kernel(int index);

    // interfaces for data device
    bool setDataBuffer(IntelDisplayDataBuffer& dataBuffer);

    // interfaces for control device
    void setRotation(int rotation);
    void setPosition(int x, int y, int w, int h);

    // interfaces for both data & control devices
    bool flush(uint32_t flags);
    bool enable();
    bool disable(uint32_t flags);
    bool reset();
    void setPipe(intel_display_pipe_t pipe);
    void setPipeByMode(intel_overlay_mode_t displayMode);
    uint32_t getPipe();
    void forceBottom(bool bottom);
    bool waitForFlip();

    // DRM mode change handle
    intel_overlay_mode_t onDrmModeChange();

    // Set overlay on top if no rgb layers in the list
    bool setOverlayOnTop(bool isOnTop);
    bool sameFlipSurface();
};

class IntelOverlayContextMfld : public IntelOverlayContext
{
public:
    IntelOverlayContextMfld(int drmFd,
            IntelBufferManager *bufferManager = NULL)
        : IntelOverlayContext(drmFd, bufferManager) {}
    bool flush_frame_or_top_field(uint32_t flags);
    bool flush_bottom_field(uint32_t flags);
};

class IntelOverlayPlane : public IntelDisplayPlane {
private:
    enum {
        //BZ 33017. Don't hold too many buffers in GTT for advoiding reach GTT max size(128M).
        //The final fix should be that video drvier don't need manage GTT anymore and hwc
        //responsible for map/unmap all display buffers in GTT.
        OVERLAY_DATA_BUFFER_NUM_MAX = 4,
    };
    // overlay mapped data buffers
    struct {
        uint32_t handle;
        unsigned long long ui64Stamp;
        IntelDisplayBuffer *buffer;
        uint32_t bufferType;
        int grallocBuffFd;
    } mDataBuffers[OVERLAY_DATA_BUFFER_NUM_MAX];
    int mNextBuffer;

public:
    IntelOverlayPlane(int fd, int index, IntelBufferManager *bufferManager);
    virtual ~IntelOverlayPlane();
    virtual void setPosition(int left, int top, int right, int bottom);
    virtual bool setDataBuffer(uint32_t handle, uint32_t flags, IMG_native_handle_t* nHandle);
    virtual bool setDataBuffer(IntelDisplayBuffer& buffer);

    virtual bool invalidateDataBuffer();
    virtual bool flip(void *context, uint32_t flags);
    virtual void waitForFlipCompletion();
    virtual bool reset();
    virtual bool disable();
    virtual void setPipe(intel_display_pipe_t pipe);
    virtual void setPipeByMode(intel_overlay_mode_t displayMode);
    virtual void forceBottom(bool bottom);
    virtual uint32_t onDrmModeChange();
    virtual bool setOverlayOnTop(bool isOnTop);
};

class IntelRGBOverlayPlane : public IntelOverlayPlane {
public:
	virtual uint32_t convert(uint32_t handle, int w, int h, int x, int y);
	virtual bool invalidateDataBuffer();
public:
	IntelRGBOverlayPlane(int fd, int index,
                            IntelBufferManager *bufferManager);
	virtual ~IntelRGBOverlayPlane();
private:
    class PixelFormatConverter {
    public:
        PixelFormatConverter();
        ~PixelFormatConverter();
        bool initialize();
        uint32_t convertBuffer(uint32_t handle, int w, int h, int x, int y);
        void reset();
    private:
        IMG_gralloc_module_public_t *mGrallocModule;
        alloc_device_t *mAllocDev;
        android::KeyedVector<uint64_t, uint32_t> mBufferMapping;
        uint32_t mCurrentBuffer;
    };

    PixelFormatConverter *mPixelFormatConverter;
};

// sprite plane formats
enum {
    INTEL_SPRITE_PIXEL_FORMAT_BGRX565  = 0x14000000UL,
    INTEL_SPRITE_PIXEL_FORMAT_BGRX8888 = 0x18000000UL,
    INTEL_SPRITE_PIXEL_FORMAT_BGRA8888 = 0x1c000000UL,
    INTEL_SPRITE_PIXEL_FORMAT_RGBX8888 = 0x38000000UL,
    INTEL_SPRITE_PIXEL_FORMAT_RGBA8888 = 0x3c000000UL,
};

// plane z order configuration bits
enum {
    INTEL_SPRITE_FORCE_BOTTOM = 0x00000004UL,
    INTEL_OVERLAY_FORCE_BOTTOM = 0x00008000UL,
};

class IntelSpriteContext : public IntelDisplayPlaneContext {
private:
    intel_sprite_context_t mContext;
public:
    IntelSpriteContext() {
        memset(&mContext, 0, sizeof(mContext));
    }
    ~IntelSpriteContext();
    intel_sprite_context_t* getContext() { return &mContext; }
};

class IntelSpritePlane : public IntelDisplayPlane {
public:
    IntelSpritePlane(int fd, int index, IntelBufferManager *bufferManager);
    virtual ~IntelSpritePlane();
    virtual void setPosition(int left, int top, int right, int bottom);
    virtual bool setDataBuffer(IntelDisplayBuffer& buffer);
    virtual bool invalidateDataBuffer();
    virtual bool flip(void *context, uint32_t flags);
    virtual bool reset();
    virtual bool disable();
};

class MedfieldSpritePlane : public IntelSpritePlane {
private:
    enum {
        SPRITE_DATA_BUFFER_NUM_MAX = 3,
    };

    struct {
        unsigned long long ui64Stamp;
        uint32_t handle;
        IntelDisplayBuffer *buffer;
    } mDataBuffers[SPRITE_DATA_BUFFER_NUM_MAX];
    int mNextBuffer;
    int mPrevHitSlot;
protected:
    virtual bool checkPosition(int& left, int& top, int& right, int& bottom);
public:
    MedfieldSpritePlane(int fd, int index, IntelBufferManager *bufferManager);
    ~MedfieldSpritePlane();
    virtual void setPosition(int left, int top, int right, int bottom);
    virtual bool setDataBuffer(IntelDisplayBuffer& buffer);
    virtual bool setDataBuffer(uint32_t handle, uint32_t flags, IMG_native_handle_t* nHandle);
    virtual bool invalidateDataBuffer();
    virtual bool flip(void *context, uint32_t flags);
    virtual bool reset();
    virtual bool disable();
    virtual void forceBottom(bool bottom);
};

class IntelDisplayPlaneManager : public IntelHWComposerDump {
public:
    enum {
        ZORDER_POaOc = 0,
        ZORDER_POcOa,
        ZORDER_OaOcP,
        ZORDER_OcOaP,
    };
private:
    int mSpritePlaneCount;
    int mPrimaryPlaneCount;
    int mOverlayPlaneCount;
    int mTotalPlaneCount;
    // maximum plane count for each pipe
    int mMaxPlaneCount;
    IntelDisplayPlane **mSpritePlanes;
    IntelDisplayPlane **mPrimaryPlanes;
    IntelDisplayPlane **mOverlayPlanes;
    IntelDisplayPlane **mRGBOverlayPlanes;

    // Bitmap of free planes. Bit0 - plane A, bit 1 - plane B, etc.
    uint32_t mFreeSpritePlanes;
    uint32_t mFreePrimaryPlanes;
    uint32_t mFreeOverlayPlanes;
    uint32_t mReclaimedSpritePlanes;
    uint32_t mReclaimedPrimaryPlanes;
    uint32_t mReclaimedOverlayPlanes;

    int mDrmFd;
    IntelBufferManager *mBufferManager;
    IntelBufferManager *mGrallocBufferManager;

    // raw data structure of display planes context
    // the length of the plane context should be different for different
    // platforms, but they follow the same structure as below
    // uint32_t active_sprites | active_primaries
    // uint32_t active_overlays
    // An array of primary context (same as sprite context)
    // An array of sprite context
    // An array of overlay_context
    void *mPlaneContexts;
    int mContextLength;

    int *mZOrderConfigs;

    bool mInitialized;
private:
    int getPlane(uint32_t& mask);
    int getPlane(uint32_t& mask, int index);
    void putPlane(int index, uint32_t& mask);
public:
    IntelDisplayPlaneManager(int fd,
                             IntelBufferManager *bm,
                             IntelBufferManager *gm);
    virtual ~IntelDisplayPlaneManager();
    virtual bool initCheck() { return mInitialized; }
    virtual void detect();

    // plane allocation & free
    IntelDisplayPlane* getSpritePlane();
    IntelDisplayPlane* getPrimaryPlane(int pipe);
    IntelDisplayPlane* getOverlayPlane();
    IntelDisplayPlane* getRGBOverlayPlane();

    bool hasFreeSprites();
    bool hasFreeOverlays();
    bool hasReclaimedOverlays();
    bool hasFreeRGBOverlays();
    bool primaryAvailable(int index);

    void reclaimPlane(IntelDisplayPlane *plane);
    void disableReclaimedPlanes(int type);
    void *getPlaneContexts() const;
    void resetPlaneContexts();
    int getContextLength() const;
    int setZOrderConfig(int config, int pipe);
    int getZOrderConfig(int pipe);
    // dump plane info
    bool dump(char *buff, int buff_len, int *cur_len);
};

#endif /*__INTEL_DISPLAY_PLANE_MANAGER_H__*/
