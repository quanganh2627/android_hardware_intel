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
#ifndef HWCLAYERLIST_H_
#define HWCLAYERLIST_H_

#include <Dump.h>
#include <hardware/hwcomposer.h>
#include <utils/SortedVector.h>

#include <IDisplayPlane.h>
#include <DisplayPlaneManager.h>

namespace android {
namespace intel {

class HwcLayer {
public:
    enum {
        LAYER_FB = 0,
        LAYER_SPRITE,
        LAYER_OVERLAY,
        LAYER_PRIMARY,
    };
public:
    HwcLayer(int index, hwc_layer_1_t *layer);
    ~HwcLayer();

    // plane operations
    bool attachPlane(IDisplayPlane *plane);
    IDisplayPlane* detachPlane();

    void setType(uint32_t type);
    uint32_t getType() const;

    int getIndex() const;
    hwc_layer_1_t* getLayer() const;
    IDisplayPlane* getPlane() const;

    bool update(hwc_layer_1_t *layer, int disp);
private:
    int mIndex;
    hwc_layer_1_t *mLayer;
    IDisplayPlane *mPlane;
    uint32_t mType;
};

class HwcLayerList {
public:
    HwcLayerList(hwc_display_contents_1_t *list, DisplayPlaneManager& dpm,
                  IDisplayPlane* primary, int disp);
    virtual ~HwcLayerList();

    class HwcLayerVector : public SortedVector<HwcLayer*> {
    public:
        HwcLayerVector();
        virtual int do_compare(const void* lhs, const void* rhs) const;
    };

    virtual bool update(hwc_display_contents_1_t *list);
    virtual IDisplayPlane* getPlane(uint32_t index) const;

    // dump interface
    virtual void dump(Dump& d);
protected:
    virtual void setZOrder(bool& primaryAvailable);
    virtual void revisit();
    virtual bool check(IDisplayPlane& plane, hwc_layer_1_t& layer);
    virtual void analyzeFrom(uint32_t index);
    virtual void analyze();
private:
    hwc_display_contents_1_t *mList;
    HwcLayerVector mLayers;
    HwcLayerVector mOverlayLayers;
    HwcLayerVector mFBLayers;
    uint32_t mLayerCount;
    bool mForcePrimaryFlip;
    ZOrderConfig mZOrderConfig;
    // need a display plane manager to get display plane info;
    DisplayPlaneManager& mDisplayPlaneManager;
    // primary plane of attached display device
    IDisplayPlane* mPrimaryPlane;
    HwcLayer *mFramebufferTarget;
    int mDisplayIndex;
};

} // namespace intel
} // namespace android


#endif /* HWCLAYERLIST_H_ */
