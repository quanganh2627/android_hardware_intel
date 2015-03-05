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
#ifndef __INTEL_HWCOMPOSER_LAYER_H__
#define __INTEL_HWCOMPOSER_LAYER_H__

#define HWC_REMOVE_DEPRECATED_VERSIONS 1

#include <string.h>
#include <hardware/hwcomposer.h>
#include <IntelDisplayPlaneManager.h>

class IntelHWComposerLayer {
public:
    enum {
        LAYER_TYPE_INVALID,
        LAYER_TYPE_RGB,
        LAYER_TYPE_YUV,
    };

private:
    hwc_layer_1_t *mHWCLayer;
    IntelDisplayPlane *mPlane;
    int mFlags;
    bool mForceOverlay;
    bool mNeedClearup;

    // layer info
    int mLayerType;
    int mFormat;
    bool mIsProtected;
public:
    IntelHWComposerLayer();
    IntelHWComposerLayer(hwc_layer_1_t *layer,
                         IntelDisplayPlane *plane,
                         int flags);
    ~IntelHWComposerLayer();

friend class IntelHWComposerLayerList;
};

class IntelHWComposerLayerList {
private:
    IntelHWComposerLayer *mLayerList;
    IntelDisplayPlaneManager *mPlaneManager;
    int mNumLayers;
    int mNumRGBLayers;
    int mNumYUVLayers;
    int mAttachedSpritePlanes;
    int mAttachedOverlayPlanes;
    int mNumAttachedPlanes;
    bool mInitialized;
public:
    IntelHWComposerLayerList(IntelDisplayPlaneManager *pm);
    ~IntelHWComposerLayerList();
    bool initCheck() const { return mInitialized; }
    void updateLayerList(hwc_display_contents_1_t *layerList);
    bool invalidatePlanes();
    void attachPlane(int index, IntelDisplayPlane *plane, int flags);
    void detachPlane(int index, IntelDisplayPlane *plane);
    IntelDisplayPlane* getPlane(int index);
    void setFlags(int index, int flags);
    int getFlags(int index);
    void setForceOverlay(int index, bool isForceOverlay);
    bool getForceOverlay(int index);
    void setNeedClearup(int index, bool needClearup);
    bool getNeedClearup(int index);
    int getLayerType(int index) const;
    int getLayerFormat(int index) const;
    bool isProtectedLayer(int index) const;
    int getLayersCount() const { return mNumLayers; }
    int getRGBLayerCount() const;
    int getYUVLayerCount() const;
    int getAttachedPlanesCount() const { return mNumAttachedPlanes; }
    int getAttachedSpriteCount() const { return mAttachedSpritePlanes; }
    int getAttachedOverlayCount() const { return mAttachedOverlayPlanes; }
};

#endif /*__INTEL_HWCOMPOSER_LAYER_H__*/
