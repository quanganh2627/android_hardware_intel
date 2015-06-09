/*
 * Copyright © 2012 Intel Corporation
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Jackie Li <yaodong.li@intel.com>
 *
 */
#ifndef __INTEL_DISPLAY_DEVICE_CPP__
#define __INTEL_DISPLAY_DEVICE_CPP__

#include <EGL/egl.h>

#include <IntelHWComposerDrm.h>
#include <IntelBufferManager.h>
#include <IntelHWComposerLayer.h>
#include <IntelHWComposerDump.h>
#include "RotationBufferProvider.h"

class IntelDisplayConfig {
private:
    uint32_t mRefreshRate;
    uint32_t mWidth;
    uint32_t mHeight;
    uint32_t mDpiX;
    uint32_t mDpiY;
    uint32_t mFormat;
    uint32_t mFlag;   //Interlaced;
public:
    uint32_t getRefreshRate() { return mRefreshRate; }
    uint32_t getWidth() { return mWidth; }
    uint32_t getHeight() { return mHeight; }
    uint32_t getDpiX() { return mDpiX; }
    uint32_t getDpiY() { return mDpiY; }
    uint32_t getFormat() { return mFormat; }
    uint32_t getFlag() { return mFlag; }
};


class IntelDisplayVsync {
//vsync is also need to be handled by device;
//FIXME: IMPLEMENT IT LATER
};

struct WidiExtendedModeInfo {
    IMG_native_handle_t* widiExtHandle;
    bool videoSentToWidi;
};

class IntelDisplayDevice : public IntelHWComposerDump {
private:
    IntelWsbm *mWsbm;
protected:
    //IntelDisplayConfig mConfig;
    IntelDisplayPlaneManager *mPlaneManager;
    IntelHWComposerDrm *mDrm;
    IntelBufferManager *mBufferManager;
    IntelBufferManager *mGrallocBufferManager;
    IntelHWComposerLayerList *mLayerList;
    RotationBufferProvider *mRotationBufProvider;
    uint32_t mDisplayIndex;
    bool mForceSwapBuffer;
    bool mHotplugEvent;
    bool mIsConnected;
    bool mInitialized;
    bool mIsScreenshotActive;
    bool mIsBlank;
    bool mVideoSeekingActive;
        // detect fb layers state to bypass fb composition.
    int mYUVOverlay;

    enum {
        NUM_FB_BUFFERS = 3,
    };
    enum {
        LAYER_SAME_RGB_BUFFER_SKIP_RELEASEFENCEFD = -2,
    };
    struct fb_buffer{
        unsigned long long ui64Stamp;
        IntelDisplayBuffer *buffer;
    } mFBBuffers[NUM_FB_BUFFERS];
    int mNextBuffer;

protected:
    virtual bool isHWCUsage(int usage);
    virtual bool isHWCFormat(int format);
    virtual bool isHWCTransform(uint32_t transform);
    virtual bool isHWCBlending(uint32_t blending);
    virtual bool isHWCLayer(hwc_layer_1_t *layer);
    virtual bool areLayersIntersecting(hwc_layer_1_t *top, hwc_layer_1_t* bottom);
    virtual bool isLayerSandwiched(int index, hwc_display_contents_1_t *list);

    virtual bool isOverlayLayer(hwc_display_contents_1_t *list,
                        int index,
                        hwc_layer_1_t *layer,
                        int& flags);
    virtual bool isRGBOverlayLayer(hwc_display_contents_1_t *list,
                        int index,
                        hwc_layer_1_t *layer,
                        int& flags);
    virtual bool isSpriteLayer(hwc_display_contents_1_t *list,
                       int index,
                       hwc_layer_1_t *layer,
                       int& flags);
    virtual bool isPrimaryLayer(hwc_display_contents_1_t *list,
                       int index,
                       hwc_layer_1_t *layer,
                       int& flags);
    virtual bool overlayPrepare(int index, hwc_layer_1_t *layer, int flags){return false;} ;
    virtual bool rgbOverlayPrepare(int index, hwc_layer_1_t *layer, int flags);
    virtual bool spritePrepare(int index, hwc_layer_1_t *layer, int flags);
    virtual bool primaryPrepare(int index, hwc_layer_1_t *layer, int flags);
    virtual bool flipFramebufferContexts(void *contexts, hwc_layer_1_t *layer);

    virtual void dumpLayerList(hwc_display_contents_1_t *list);
    virtual bool isScreenshotActive(hwc_display_contents_1_t *list);
    virtual bool isVideoPutInWindow(int output, hwc_layer_1_t *layer);
    virtual int  getMetaDataTransform(hwc_layer_1_t *layer,
            uint32_t &transform);
    //*not virtual as not intend for override.Just for implemetation inheritence*/
    bool updateLayersData(hwc_display_contents_1_t *list);
    void revisitLayerList(hwc_display_contents_1_t *list,
                                              bool isGeometryChanged);
private:
    bool initializeRotationBufProvider();
    void destroyRotationBufProvider();
    void updateZorderConfig();
    bool isBobDeinterlace(hwc_layer_1_t *layer);
    bool useOverlayRotation(hwc_layer_1_t *layer, int index, uint32_t& handle,
                           int& w, int& h,
                           int& srcX, int& srcY, int& srcW, int& srcH, uint32_t& transform);

public:
    virtual bool initCheck() { return mInitialized; }
    virtual bool prepare(hwc_display_contents_1_t *hdc) {return true;}
    virtual bool commit(hwc_display_contents_1_t *hdc, buffer_handle_t *bh,
                        int* acqureFenceFd, int** releaseFenceFd,
                        int &numBuffers) { return true; }
    virtual bool release();
    virtual bool dump(char *buff, int buff_len, int *cur_len);
    virtual bool blank(int blank);
    virtual void onHotplugEvent(bool hpd);

    virtual bool getDisplayConfig(uint32_t* configs, size_t* numConfigs);
    virtual bool getDisplayAttributes(uint32_t config,
            const uint32_t* attributes, int32_t* values);

    IntelDisplayDevice(IntelDisplayPlaneManager *pm,
                       IntelHWComposerDrm *drm,
                       IntelBufferManager *bm,
                       IntelBufferManager *gm,
                       uint32_t index);

    virtual ~IntelDisplayDevice();
};

class IntelMIPIDisplayDevice : public IntelDisplayDevice {
protected:
    WidiExtendedModeInfo *mExtendedModeInfo;
    bool mVideoSentToWidi;


    buffer_handle_t mLastHandles[5];
    bool mSkipComposition;
    bool mHasGlesComposition;
    bool mHasSkipLayer;
    void handleSmartComposition(hwc_display_contents_1_t *list);

    buffer_handle_t mPrevFlipHandles[10];

protected:
    bool isForceOverlay(hwc_layer_1_t *layer);
    void updateZorderConfig();
    bool shouldHide(hwc_layer_1_t *layer);

protected:
    virtual bool isOverlayLayer(hwc_display_contents_1_t *list,
                        int index,
                        hwc_layer_1_t *layer,
                        int& flags);
    virtual bool isRGBOverlayLayer(hwc_display_contents_1_t *list,
                                   unsigned int index,
                                   hwc_layer_1_t *layer,
                                   int& flags);
    virtual bool isSpriteLayer(hwc_display_contents_1_t *list,
                       int index,
                       hwc_layer_1_t *layer,
                       int& flags);
    virtual bool isPrimaryLayer(hwc_display_contents_1_t *list,
                       int index,
                       hwc_layer_1_t *layer,
                       int& flags);

    virtual void onGeometryChanged(hwc_display_contents_1_t *list);
    virtual bool overlayPrepare(int index, hwc_layer_1_t *layer, int flags);
public:
    IntelMIPIDisplayDevice(IntelBufferManager *bm,
                           IntelBufferManager *gm,
                           IntelDisplayPlaneManager *pm,
                           IntelHWComposerDrm *drm,
                           WidiExtendedModeInfo *extinfo,
                           uint32_t index);
    ~IntelMIPIDisplayDevice();
    virtual bool prepare(hwc_display_contents_1_t *hdc);
    virtual bool commit(hwc_display_contents_1_t *hdc, buffer_handle_t *bh,
                        int* acqureFenceFd, int** releaseFenceFd, int &numBuffers);
    virtual bool dump(char *buff, int buff_len, int *cur_len);

    virtual bool getDisplayConfig(uint32_t* configs, size_t* numConfigs);
    virtual bool getDisplayAttributes(uint32_t config,
            const uint32_t* attributes, int32_t* values);
};

class IntelHDMIDisplayDevice : public IntelDisplayDevice {
protected:
    virtual void onGeometryChanged(hwc_display_contents_1_t *list);
    virtual bool overlayPrepare(int index, hwc_layer_1_t *layer, int flags);
private:
    bool flipOverlayerPlane(void* context,hwc_display_contents_1_t *list,
                                    buffer_handle_t *bh,
                                    int &numBuffers,int*acqureFenceFd,int**releaseFenceFd);
    bool flipFrameBufferTarget(void* context,hwc_display_contents_1_t *list,
                                    buffer_handle_t *bh,
                                    int &numBuffers,int*acqureFenceFd,int**releaseFenceFd);
    bool needFlipOverlay(hwc_display_contents_1_t *list);
    bool determineGraphicVisibilityInExtendMode(hwc_display_contents_1_t *list,int index);
    void enableHDMIGraphicPlane(bool enable);
public:
    IntelHDMIDisplayDevice(IntelBufferManager *bm,
                       IntelBufferManager *gm,
                       IntelDisplayPlaneManager *pm,
                       IntelHWComposerDrm *drm,
                       uint32_t index);

    ~IntelHDMIDisplayDevice();
    virtual bool prepare(hwc_display_contents_1_t *hdc);
    virtual bool commit(hwc_display_contents_1_t *hdc, buffer_handle_t *bh,
                        int* acqureFenceFd, int** releaseFenceFd, int &numBuffers);
    virtual bool dump(char *buff, int buff_len, int *cur_len);

    virtual void onHotplugEvent(bool hpd);
    virtual bool getDisplayConfig(uint32_t* configs, size_t* numConfigs);
    virtual bool getDisplayAttributes(uint32_t config,
            const uint32_t* attributes, int32_t* values);
private:
    bool mGraphicPlaneVisible;
};
#endif /*__INTEL__DISPLAY_DEVICE__*/
