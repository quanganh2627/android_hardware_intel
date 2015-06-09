/*
 * Copyright © 2013 Intel Corporation
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
 *    Brian Rogers <brian.e.rogers@intel.com>
 *
 */

#ifndef __WIDI_DISPLAY_DEVICE_H__
#define __WIDI_DISPLAY_DEVICE_H__

#include <utils/KeyedVector.h>
#include <utils/List.h>
#include <utils/Mutex.h>
#include <utils/RefBase.h>

#include "IntelDisplayDevice.h"
#include "IFrameServer.h"

using namespace android;

class WidiDisplayDevice : public IntelDisplayDevice, public BnFrameServer {
protected:
    struct CachedBuffer : public android::RefBase {
        CachedBuffer(IntelBufferManager *gbm, IntelDisplayBuffer* buffer);
        ~CachedBuffer();
        IntelBufferManager* grallocBufferManager;
        IntelDisplayBuffer* displayBuffer;
    };
    struct HeldCscBuffer : public android::RefBase {
        HeldCscBuffer(const android::sp<WidiDisplayDevice>& wdd, const sp<GraphicBuffer>& gb);
        virtual ~HeldCscBuffer();
        android::sp<WidiDisplayDevice> wdd;
        android::sp<GraphicBuffer> buffer;
    };
    struct HeldDecoderBuffer : public android::RefBase {
        HeldDecoderBuffer(const android::sp<CachedBuffer>& cachedBuffer);
        virtual ~HeldDecoderBuffer();
        android::sp<CachedBuffer> cachedBuffer;
    };
    struct Configuration {
        android::sp<IFrameTypeChangeListener> typeChangeListener;
        android::sp<IFrameListener> frameListener;
        FrameProcessingPolicy policy;
        bool extendedModeEnabled;
        bool forceNotify;
    };
    android::Mutex mConfigLock;
    Configuration mCurrentConfig;
    Configuration mNextConfig;
    size_t mLayerToSend;

    WidiExtendedModeInfo *mExtendedModeInfo;
    uint32_t mExtLastKhandle;
    int64_t mExtLastTimestamp;

    int64_t mRenderTimestamp;

    // colorspace conversion
    android::Mutex mCscLock;
    IMG_gralloc_module_public_t* mGrallocModule;
    alloc_device_t* mGrallocDevice;
    android::List< android::sp<GraphicBuffer> > mAvailableCscBuffers;
    int mCscBuffersToCreate;
    uint32_t mCscWidth;
    uint32_t mCscHeight;

    FrameInfo mLastInputFrameInfo;
    FrameInfo mLastOutputFrameInfo;

    android::KeyedVector<uint32_t, android::sp<CachedBuffer> > mMappedBufferCache;
    android::Mutex mHeldBuffersLock;
    android::KeyedVector<uint32_t, android::sp<android::RefBase> > mHeldBuffers;

private:
    android::sp<CachedBuffer> getMappedBuffer(uint32_t handle);
    void sendToWidi(const hwc_layer_1_t& layer);

public:
    WidiDisplayDevice(IntelBufferManager *bm,
                      IntelBufferManager *gm,
                      IntelDisplayPlaneManager *pm,
                      IntelHWComposerDrm *drm,
                      WidiExtendedModeInfo *extinfo,
                      uint32_t index);

    ~WidiDisplayDevice();

    // IFrameServer methods
    virtual android::status_t start(android::sp<IFrameTypeChangeListener> frameTypeChangeListener, bool disableExtVideoMode);
    virtual android::status_t stop(bool isConnected);
    virtual android::status_t notifyBufferReturned(int index);
    virtual android::status_t setResolution(const FrameProcessingPolicy& policy, android::sp<IFrameListener> listener);

    virtual bool prepare(hwc_display_contents_1_t *list);
    virtual bool commit(hwc_display_contents_1_t *list,
                        buffer_handle_t *bh, int &numBuffers);
    virtual bool dump(char *buff, int buff_len, int *cur_len);

    virtual void onHotplugEvent(bool hpd);

    virtual bool getDisplayConfig(uint32_t* configs, size_t* numConfigs);
    virtual bool getDisplayAttributes(uint32_t config,
            const uint32_t* attributes, int32_t* values);

};

#endif /*__WIDI_DISPLAY_DEVICE_H__*/
