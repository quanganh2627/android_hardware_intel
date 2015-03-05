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
#ifndef __INTEL_BUFFER_MANAGER_H__
#define __INTEL_BUFFER_MANAGER_H__

#include <ui/GraphicBufferMapper.h>
#include <hardware/hardware.h>
#include <system/graphics.h>
#include <IntelWsbm.h>
#include <OMX_IVCommon.h>
#include <hal_public.h>
#include <pvr2d.h>
#include <pthread.h>
#include <services.h>

class IntelDisplayBuffer
{
protected:
    void *mBufferObject;
    uint32_t mHandle;
    uint32_t mSize;
    void *mVirtAddr;
    uint32_t mGttOffsetInPage;
    uint32_t mStride;
public:
     IntelDisplayBuffer()
         : mBufferObject(0), mHandle(0),
           mSize(0), mVirtAddr(0), mGttOffsetInPage(0) {}
     IntelDisplayBuffer(void *buf, void *addr,
                        uint32_t gtt, uint32_t size, uint32_t handle = 0)
         : mBufferObject(buf),
           mHandle(handle),
           mSize(size),
           mVirtAddr(addr),
           mGttOffsetInPage(gtt) {}
     ~IntelDisplayBuffer() {}
     void* getBufferObject() const { return mBufferObject; }
     void* getCpuAddr() const { return mVirtAddr; }
     uint32_t getGttOffsetInPage() const { return mGttOffsetInPage; }
     uint32_t getSize() const { return mSize; }
     uint32_t getHandle() const { return mHandle; }
     void setStride(int stride) { mStride = stride; }
     uint32_t getStride() const { return mStride; }
};

// pixel format supported by HWC
// TODO: share the extended pixel format with gralloc HAL
enum {
    HAL_PIXEL_FORMAT_INTEL_HWC_NV12_VED = OMX_INTEL_COLOR_FormatYUV420PackedSemiPlanar,
    HAL_PIXEL_FORMAT_INTEL_HWC_NV12 = HAL_PIXEL_FORMAT_NV12,
    HAL_PIXEL_FORMAT_INTEL_HWC_YUY2 = HAL_PIXEL_FORMAT_YUY2,
    HAL_PIXEL_FORMAT_INTEL_HWC_UYVY = HAL_PIXEL_FORMAT_UYVY,
    HAL_PIXEL_FORMAT_INTEL_HWC_I420 = HAL_PIXEL_FORMAT_I420,
    HAL_PIXEL_FORMAT_INTEL_HWC_NV12_TILE = OMX_INTEL_COLOR_FormatYUV420PackedSemiPlanar_Tiled,
};

class IntelDisplayDataBuffer : public IntelDisplayBuffer
{
public:
    enum {
        BUFFER_CHANGE = 0x00000001UL,
        SIZE_CHANGE   = 0x00000002UL,
    };
    uint32_t mBobDeinterlace;
private:
    uint32_t mFormat;
    uint32_t mWidth;
    uint32_t mHeight;
    uint32_t mYStride;
    uint32_t mUVStride;
    uint32_t mRawStride;
    uint32_t mSrcX;
    uint32_t mSrcY;
    uint32_t mSrcWidth;
    uint32_t mSrcHeight;
    uint32_t mUpdateFlags;
    IntelDisplayBuffer *mBuffer;
public:
    IntelDisplayDataBuffer();
    IntelDisplayDataBuffer(uint32_t format, uint32_t w, uint32_t h);
    ~IntelDisplayDataBuffer() {}
    void setFormat(int format) { mFormat = format; }
    void setBuffer(IntelDisplayBuffer *buffer);
    IntelDisplayBuffer* getBuffer() { return mBuffer; }
    void setWidth(uint32_t w);
    void setHeight(uint32_t h);
    void setStride(uint32_t stride);
    void setStride(uint32_t yStride, uint32_t uvStride);
    void setCrop(int x, int y, int w, int h);
    void setDeinterlaceType(uint32_t bob_deinterlace);
    void inline appendFlags(uint32_t flags) { mUpdateFlags |= flags; }
    void inline removeFlags(uint32_t flags) { mUpdateFlags &= ~flags; }
    void inline clearFlags() { mUpdateFlags = 0; }
    bool inline isFlags(uint32_t flags) {
        return (mUpdateFlags & flags) ? true : false;
    }
    uint32_t inline getFormat() const { return mFormat; }
    uint32_t inline getWidth() const { return mWidth; }
    uint32_t inline getHeight() const { return mHeight; }
    uint32_t inline getStride() const { return mRawStride; }
    uint32_t inline getYStride() const { return mYStride; }
    uint32_t inline getUVStride() const { return mUVStride; }
    uint32_t inline getSrcX() const { return mSrcX; }
    uint32_t inline getSrcY() const { return mSrcY; }
    uint32_t inline getSrcWidth() const { return mSrcWidth; }
    uint32_t inline getSrcHeight() const { return mSrcHeight; }
};

class IntelBufferManager
{
public:
    enum {
        TTM_BUFFER = 1,
        PVR_BUFFER,
        BCD_BUFFER,
        GRALLOC_BUFFER,
    };
protected:
    int mDrmFd;
    bool mInitialized;
public:
    virtual bool initialize() { return true; }
    virtual IntelDisplayBuffer* get(int size, int gttAlignment) { return 0; }
    virtual void put(IntelDisplayBuffer *buf) {}
    virtual IntelDisplayBuffer* map(uint32_t handle) { return 0; }
    virtual IntelDisplayBuffer* map(uint32_t device, uint32_t handle) {
        return 0;
    }
    virtual void unmap(IntelDisplayBuffer *buffer) {}
    virtual IntelDisplayBuffer* wrap(void *virt, int size) {
        return 0;
    }
    virtual void unwrap(IntelDisplayBuffer *buffer) {}
    virtual void waitIdle(uint32_t khandle){}
    virtual bool alloc(uint32_t size, uint32_t* um_handle, uint32_t* km_handle) {
        return false;
    }
    virtual bool dealloc(uint32_t um_handle) { return false; }
    virtual bool updateCursorReg(int count, IntelDisplayBuffer *cursorDataBuffer,
                        int x, int y, int w, int h, bool isEnable) {return false;}
    virtual IntelDisplayBuffer* curAlloc(int w, int h) {return 0;}
    virtual void curFree(IntelDisplayBuffer *buffer) {}
    bool initCheck() const { return mInitialized; }
    int getDrmFd() const { return mDrmFd; }
    IntelBufferManager(int fd)
        : mDrmFd(fd), mInitialized(false) {
    }
    virtual ~IntelBufferManager() {};
};

class IntelTTMBufferManager : public IntelBufferManager
{
private:
    IntelWsbm *mWsbm;
    uint32_t mVideoBridgeIoctl;
private:
    bool getVideoBridgeIoctl();
public:
    IntelTTMBufferManager(int fd)
        : IntelBufferManager(fd), mWsbm(0), mVideoBridgeIoctl(0) {}
    ~IntelTTMBufferManager();
    bool initialize();
    IntelDisplayBuffer* map(uint32_t handle);
    IntelDisplayBuffer* get(int size, int gttAlignment);
    void put(IntelDisplayBuffer *buf);
    void unmap(IntelDisplayBuffer *buffer);
};

enum {
    OUTPUT_FORCE_NULL = 0,
    OUTPUT_FORCE_GPU,
    OUTPUT_FORCE_OVERLAY,
};

typedef struct {
    // transform made by clients (clients to hwc)
    int client_transform;
    int metadata_transform;
    int rotated_width;
    int rotated_height;
    int surface_protected;
    int force_output_method;
    uint32_t rotated_buffer_handle;
    uint32_t renderStatus;
    unsigned int used_by_widi;
    int bob_deinterlace;
    int tiling;
    uint32_t width;
    uint32_t height;
    uint32_t luma_stride;
    uint32_t chroma_u_stride;
    uint32_t chroma_v_stride;
    uint32_t format;
    uint32_t khandle;
    int64_t  timestamp;

    uint32_t rotate_luma_stride;
    uint32_t rotate_chroma_u_stride;
    uint32_t rotate_chroma_v_stride;

    nsecs_t hwc_timestamp;
    uint32_t layer_transform;

    void *native_window;
} intel_gralloc_payload_t;

class IntelPVRBufferManager : public IntelBufferManager {
private:
    PVR2DCONTEXTHANDLE mPVR2DHandle;
private:
    bool pvr2DInit();
    void pvr2DDestroy();
    bool gttMap(PVR2DMEMINFO *buf, int *offset,
                uint32_t virt, uint32_t size,  uint32_t gttAlign);
    bool gttUnmap(PVR2DMEMINFO *buf);
    /*add for Cursor debug*/
    void fillPixelColor(uint8_t* buf, int x, int y, int stride, int PIXEL_SIZE, int color);
    void fill4Block(uint8_t* buf, uint8_t a, uint8_t b, uint8_t c, uint8_t d,
                    int w, int h, int stride, int offset);
    void drawSingleDigital(uint8_t* buf, int w, int h, int stride, int offset);
public:
    IntelPVRBufferManager(int fd)
        : IntelBufferManager(fd), mPVR2DHandle(0) {}
    ~IntelPVRBufferManager();
    bool initialize();
    IntelDisplayBuffer* wrap(void *virt, int size);
    void unwrap(IntelDisplayBuffer *buffer);
    IntelDisplayBuffer* map(uint32_t handle);
    void unmap(IntelDisplayBuffer *buffer);
    bool updateCursorReg(int count, IntelDisplayBuffer *cursorDataBuffer,
                         int x, int y, int w, int h, bool isEnable);
    IntelDisplayBuffer* curAlloc(int w, int h);
    void curFree(IntelDisplayBuffer *buffer);
};

// NOTE: the number of max device devices should be aligned with kernel driver
#define INTEL_BCD_DEVICE_NUM_MAX    9
#define INTEL_BCD_BUFFER_NUM_MAX    20

class IntelBCDBufferManager : public IntelBufferManager {
private:
    // FIXME: remove wsbm later
    IntelWsbm *mWsbm;
private:
    bool gttMap(uint32_t devId, uint32_t bufferId,
                uint32_t gttAlign, int *offset);
    bool gttUnmap(uint32_t devId, uint32_t bufferId);
    bool getBCDInfo(uint32_t devId, uint32_t *count, uint32_t *stride);
public:
    IntelBCDBufferManager(int fd);
    ~IntelBCDBufferManager();
    bool initialize();
    IntelDisplayBuffer* map(uint32_t device, uint32_t handle);
    void unmap(IntelDisplayBuffer *buffer);
    IntelDisplayBuffer** map(uint32_t device, uint32_t *count);
    void unmap(IntelDisplayBuffer **buffer, uint32_t count);
    IntelDisplayBuffer* get(int size, int alignment);
    void put(IntelDisplayBuffer* buf);
};

class IntelGraphicBufferManager : public IntelBufferManager {
private:
    PVRSRV_CONNECTION *mPVRSrvConnection;
    PVRSRV_DEV_DATA mDevData;
    IMG_HANDLE mDevMemContext;
    IMG_HANDLE mGeneralHeap;

    // wsbm
    IntelWsbm *mWsbm;
private:
    bool gttMap(PVRSRV_CLIENT_MEM_INFO *memInfo,
                uint32_t gttAlign, int *offset);
    bool gttUnmap(PVRSRV_CLIENT_MEM_INFO *memInfo);
public:
    IntelGraphicBufferManager(int fd)
        : IntelBufferManager(fd) {}
    ~IntelGraphicBufferManager();
    bool initialize();
    IntelDisplayBuffer* map(uint32_t handle);
    void unmap(IntelDisplayBuffer *buffer);
    IntelDisplayBuffer* get(int size, int gttAlignment);
    void put(IntelDisplayBuffer *buf);
    IntelDisplayBuffer* wrap(void *virt, int size);
    void unwrap(IntelDisplayBuffer *buffer);
    void waitIdle(uint32_t khandle);
    bool alloc(uint32_t size, uint32_t* um_handle, uint32_t* km_handle);
    bool dealloc(uint32_t um_handle);
};

class IntelPayloadBuffer
{
private:
    IntelBufferManager* mBufferManager;
    IntelDisplayBuffer* mBuffer;
    void* mPayload;
public:
    IntelPayloadBuffer(IntelBufferManager* bufferManager, int bufferFd);
    ~IntelPayloadBuffer();
    void* getCpuAddr(){return mPayload;}
};

#endif /*__INTEL_PVR_BUFFER_MANAGER_H__*/
